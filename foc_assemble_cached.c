#include "foc_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    FocSegment *seg;
    char wav[1024];
    FocWavInfo info;
    uint64_t start, speech_end, end;
} Item;

int main(int argc, char **argv) {
    const char *script_path = argc > 1 ? argv[1] : "text/foc_script_txt/foc_script.txt";
    const char *cache_dir = argc > 2 ? argv[2] : ".cache/chatterbox/foc_script_segments";
    const char *out_aiff = argc > 3 ? argv[3] : "text/foc_script_txt/foc_script.aiff";
    const char *out_json = argc > 4 ? argv[4] : "text/foc_script_txt/foc_script.json";
    double max_seconds = argc > 5 ? atof(argv[5]) : 600.0;
    double silence_seconds = argc > 6 ? atof(argv[6]) : 0.18;
    FocScript script = {0};
    Item *items = NULL;
    size_t count = 0, i;
    int sample_rate = 0, channels = 0;
    uint64_t total_frames = 0, silence_frames = 0;
    FILE *aiff, *json;
    int complete = 0;

    if (foc_load_script(script_path, &script, 0) != 0) return 2;
    items = (Item *)calloc(script.count, sizeof(*items));
    if (!items) return 3;

    for (i = 0; i < script.count; i++) {
        char meta[1024];
        double seconds_now;
        if (!foc_find_latest_cache(cache_dir, &script.items[i], meta, sizeof(meta), items[count].wav, sizeof(items[count].wav))) break;
        if (foc_wav_info(items[count].wav, &items[count].info) != 0) break;
        if (items[count].info.bits_per_sample != 16) break;
        if (!sample_rate) {
            sample_rate = items[count].info.sample_rate;
            channels = items[count].info.channels;
            silence_frames = (uint64_t)(silence_seconds * (double)sample_rate + 0.5);
        }
        if (items[count].info.sample_rate != sample_rate || items[count].info.channels != channels) break;
        if (count) total_frames += silence_frames;
        items[count].seg = &script.items[i];
        items[count].start = total_frames;
        total_frames += items[count].info.frames;
        items[count].speech_end = total_frames;
        items[count].end = total_frames;
        if (count + 1 < script.count) items[count].end += silence_frames;
        count++;
        seconds_now = (double)total_frames / (double)sample_rate;
        if (max_seconds > 0.0 && seconds_now >= max_seconds) { complete = 1; break; }
    }
    if (!count || !sample_rate) {
        fprintf(stderr, "no contiguous cached WAV segments found\n");
        free(items);
        foc_free_script(&script);
        return 1;
    }

    aiff = fopen(out_aiff, "wb+");
    if (!aiff) { perror(out_aiff); return 2; }
    if (foc_write_aiff_header(aiff, sample_rate, total_frames, channels) != 0) return 2;
    for (i = 0; i < count; i++) {
        if (foc_copy_wav_pcm16_as_aiff(aiff, items[i].wav, &items[i].info) != 0) {
            fprintf(stderr, "failed copying %s\n", items[i].wav);
            return 2;
        }
        if (i + 1 < count) foc_write_aiff_silence(aiff, silence_frames, channels);
    }
    foc_patch_aiff_header(aiff, sample_rate, total_frames, channels);
    fclose(aiff);

    json = fopen(out_json, "wb");
    if (!json) { perror(out_json); return 2; }
    fprintf(json, "{\n  \"project\":\"foc_script\",\n  \"source_text_file\":");
    foc_json_escape(json, script_path);
    fprintf(json, ",\n  \"audio_file\":");
    foc_json_escape(json, out_aiff);
    fprintf(json, ",\n  \"sample_rate_hz\":%d,\n  \"segment_count\":%zu,\n  \"duration_seconds\":%.6f,\n  \"requested_max_duration_seconds\":%.6f,\n  \"duration_cap_reached\":%s,\n  \"incomplete_reason\":",
            sample_rate, count, (double)total_frames / (double)sample_rate, max_seconds, complete ? "true" : "false");
    if (complete) foc_json_escape(json, "");
    else foc_json_escape(json, "not enough contiguous cached WAV segments; render more TTS chunks first");
    fprintf(json, ",\n  \"segments\":[\n");
    for (i = 0; i < count; i++) {
        FocSegment *s = items[i].seg;
        fprintf(json, "    {\"segment_index\":%d,\"source_line\":%d,\"speaker\":", s->index, s->source_line);
        foc_json_escape(json, s->speaker);
        fprintf(json, ",\"spoken_text\":");
        foc_json_escape(json, s->spoken_text);
        fprintf(json, ",\"start_seconds\":%.6f,\"speech_end_seconds\":%.6f,\"end_seconds\":%.6f,\"segment_cache_audio\":",
                (double)items[i].start / sample_rate, (double)items[i].speech_end / sample_rate, (double)items[i].end / sample_rate);
        foc_json_escape(json, items[i].wav);
        fprintf(json, "}%s\n", i + 1 == count ? "" : ",");
    }
    fprintf(json, "  ]\n}\n");
    fclose(json);

    printf("wrote %s\nwrote %s\nsegments=%zu\nduration_seconds=%.3f\ncomplete=%s\n",
           out_aiff, out_json, count, (double)total_frames / (double)sample_rate, complete ? "true" : "false");
    free(items);
    foc_free_script(&script);
    return complete ? 0 : 4;
}
