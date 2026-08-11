#include "foc_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char **items;
    size_t count;
} PathList;

static char *json_unescape_simple(const char *start, const char *end) {
    size_t cap = (size_t)(end - start) + 1;
    char *out = (char *)malloc(cap);
    char *w = out;
    const char *p = start;
    if (!out) return NULL;
    while (p < end) {
        if (*p == '\\' && p + 1 < end) {
            p++;
            switch (*p) {
                case '\\': *w++ = '\\'; break;
                case '"': *w++ = '"'; break;
                case '/': *w++ = '/'; break;
                case 'n': *w++ = '\n'; break;
                case 'r': *w++ = '\r'; break;
                case 't': *w++ = '\t'; break;
                default: *w++ = *p; break;
            }
            p++;
        } else {
            *w++ = *p++;
        }
    }
    *w = '\0';
    return out;
}

static int load_manifest_cache_paths(const char *manifest_path, PathList *paths) {
    size_t size = 0;
    char *data = foc_read_file(manifest_path, &size);
    const char *needle = "\"segment_cache_audio\"";
    char *p;
    (void)size;
    paths->items = NULL;
    paths->count = 0;
    if (!data) {
        perror(manifest_path);
        return -1;
    }
    p = data;
    while ((p = strstr(p, needle)) != NULL) {
        char *colon = strchr(p, ':');
        char *quote;
        char *end;
        char **grown;
        if (!colon) break;
        quote = strchr(colon, '"');
        if (!quote) break;
        quote++;
        end = quote;
        while (*end) {
            if (*end == '"' && (end == quote || end[-1] != '\\')) break;
            end++;
        }
        if (!*end) break;
        grown = (char **)realloc(paths->items, (paths->count + 1) * sizeof(char *));
        if (!grown) { free(data); return -1; }
        paths->items = grown;
        paths->items[paths->count] = json_unescape_simple(quote, end);
        if (!paths->items[paths->count]) { free(data); return -1; }
        paths->count++;
        p = end + 1;
    }
    free(data);
    return 0;
}

static void free_paths(PathList *paths) {
    size_t i;
    for (i = 0; i < paths->count; i++) free(paths->items[i]);
    free(paths->items);
    paths->items = NULL;
    paths->count = 0;
}

typedef struct {
    FocSegment *seg;
    const char *wav;
    FocWavInfo info;
    uint64_t start, speech_end, end;
} Item;

int main(int argc, char **argv) {
    const char *script_path = argc > 1 ? argv[1] : "text/foc_script_txt/foc_script.txt";
    const char *manifest_path = argc > 2 ? argv[2] : "text/foc_script_txt/foc_script.python-preview.json";
    const char *out_aiff = argc > 3 ? argv[3] : "text/foc_script_txt/foc_script.aiff";
    const char *out_json = argc > 4 ? argv[4] : "text/foc_script_txt/foc_script.json";
    double silence_seconds = argc > 5 ? atof(argv[5]) : 0.18;
    FocScript script = {0};
    PathList paths = {0};
    Item *items = NULL;
    size_t i;
    int sample_rate = 0, channels = 0;
    uint64_t total_frames = 0, silence_frames = 0;
    FILE *aiff, *json;

    if (foc_load_script(script_path, &script, 0) != 0) return 2;
    if (load_manifest_cache_paths(manifest_path, &paths) != 0) return 2;
    if (paths.count != script.count) {
        fprintf(stderr, "manifest cache path count %zu does not match script segment count %zu\n", paths.count, script.count);
        free_paths(&paths);
        foc_free_script(&script);
        return 1;
    }
    items = (Item *)calloc(paths.count, sizeof(*items));
    if (!items) return 3;

    for (i = 0; i < paths.count; i++) {
        if (foc_wav_info(paths.items[i], &items[i].info) != 0) {
            fprintf(stderr, "bad WAV cache: %s\n", paths.items[i]);
            return 2;
        }
        if (items[i].info.bits_per_sample != 16) {
            fprintf(stderr, "unsupported WAV bit depth: %s\n", paths.items[i]);
            return 2;
        }
        if (!sample_rate) {
            sample_rate = items[i].info.sample_rate;
            channels = items[i].info.channels;
            silence_frames = (uint64_t)(silence_seconds * (double)sample_rate + 0.5);
        }
        if (items[i].info.sample_rate != sample_rate || items[i].info.channels != channels) {
            fprintf(stderr, "mixed WAV format at %s\n", paths.items[i]);
            return 2;
        }
        if (i) total_frames += silence_frames;
        items[i].seg = &script.items[i];
        items[i].wav = paths.items[i];
        items[i].start = total_frames;
        total_frames += items[i].info.frames;
        items[i].speech_end = total_frames;
        items[i].end = total_frames + (i + 1 < paths.count ? silence_frames : 0);
    }

    aiff = fopen(out_aiff, "wb+");
    if (!aiff) { perror(out_aiff); return 2; }
    if (foc_write_aiff_header(aiff, sample_rate, total_frames, channels) != 0) return 2;
    for (i = 0; i < paths.count; i++) {
        if (foc_copy_wav_pcm16_as_aiff(aiff, items[i].wav, &items[i].info) != 0) {
            fprintf(stderr, "failed copying %s\n", items[i].wav);
            return 2;
        }
        if (i + 1 < paths.count) foc_write_aiff_silence(aiff, silence_frames, channels);
    }
    foc_patch_aiff_header(aiff, sample_rate, total_frames, channels);
    fclose(aiff);

    json = fopen(out_json, "wb");
    if (!json) { perror(out_json); return 2; }
    fprintf(json, "{\n  \"project\":\"foc_script\",\n  \"source_text_file\":");
    foc_json_escape(json, script_path);
    fprintf(json, ",\n  \"renderer_manifest\":");
    foc_json_escape(json, manifest_path);
    fprintf(json, ",\n  \"audio_file\":");
    foc_json_escape(json, out_aiff);
    fprintf(json, ",\n  \"sample_rate_hz\":%d,\n  \"segment_count\":%zu,\n  \"duration_seconds\":%.6f,\n  \"silence_seconds_between_lines\":%.6f,\n  \"segments\":[\n",
            sample_rate, paths.count, (double)total_frames / (double)sample_rate, silence_seconds);
    for (i = 0; i < paths.count; i++) {
        FocSegment *s = items[i].seg;
        fprintf(json, "    {\"segment_index\":%d,\"source_line\":%d,\"speaker\":", s->index, s->source_line);
        foc_json_escape(json, s->speaker);
        fprintf(json, ",\"source_text\":");
        foc_json_escape(json, s->source_text);
        fprintf(json, ",\"spoken_text\":");
        foc_json_escape(json, s->spoken_text);
        fprintf(json, ",\"audio_file\":");
        foc_json_escape(json, out_aiff);
        fprintf(json, ",\"sample_rate_hz\":%d,\"start_sample\":%llu,\"speech_end_sample\":%llu,\"end_sample\":%llu,\"start_seconds\":%.6f,\"speech_end_seconds\":%.6f,\"end_seconds\":%.6f,\"speech_duration_seconds\":%.6f,\"segment_cache_audio\":",
                sample_rate,
                (unsigned long long)items[i].start,
                (unsigned long long)items[i].speech_end,
                (unsigned long long)items[i].end,
                (double)items[i].start / sample_rate,
                (double)items[i].speech_end / sample_rate,
                (double)items[i].end / sample_rate,
                (double)items[i].info.frames / sample_rate);
        foc_json_escape(json, items[i].wav);
        fprintf(json, "}%s\n", i + 1 == paths.count ? "" : ",");
    }
    fprintf(json, "  ]\n}\n");
    fclose(json);

    printf("wrote %s\nwrote %s\nsegments=%zu\nduration_seconds=%.3f\n",
           out_aiff, out_json, paths.count, (double)total_frames / (double)sample_rate);

    free(items);
    free_paths(&paths);
    foc_free_script(&script);
    return 0;
}
