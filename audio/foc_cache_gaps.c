#include "foc_common.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static void wav_stem(const char *wav_path, char *stem, size_t cap) {
    const char *base = strrchr(wav_path, '/');
    size_t len;
    base = base ? base + 1 : wav_path;
    snprintf(stem, cap, "%s", base);
    len = strlen(stem);
    if (len > 4 && strcmp(stem + len - 4, ".wav") == 0) stem[len - 4] = '\0';
}

static int find_latest_chunk_json(const char *cache_dir, const char *stem, int chunk_index, char *out, size_t cap) {
    char chunk_dir[1024];
    char prefix[512];
    char best[1024] = {0};
    DIR *dir;
    struct dirent *entry;
    time_t best_time = 0;
    int found = 0;

    snprintf(chunk_dir, sizeof(chunk_dir), "%s/chunks", cache_dir);
    snprintf(prefix, sizeof(prefix), "%s-chunk-%02d-", stem, chunk_index);
    dir = opendir(chunk_dir);
    if (!dir) return 0;
    while ((entry = readdir(dir)) != NULL) {
        char full[1024];
        struct stat st;
        if (strncmp(entry->d_name, prefix, strlen(prefix)) != 0) continue;
        if (!foc_has_suffix(entry->d_name, ".json")) continue;
        snprintf(full, sizeof(full), "%s/%s", chunk_dir, entry->d_name);
        if (stat(full, &st) != 0) continue;
        if (!found || st.st_mtime >= best_time) {
            found = 1;
            best_time = st.st_mtime;
            snprintf(best, sizeof(best), "%s", full);
        }
    }
    closedir(dir);
    if (!found) return 0;
    snprintf(out, cap, "%s", best);
    return 1;
}

static int cache_text_status(const char *cache_dir, const FocSegment *segment, const char *segment_json, const char *segment_wav) {
    size_t meta_size = 0;
    char *meta = foc_read_file(segment_json, &meta_size);
    long max_chars;
    long chunk_count;
    FocChunkList chunks = {0};
    char stem[512];
    size_t i;
    int ok = 1;

    if (!meta) return -1;
    max_chars = foc_json_get_long_slice(meta, meta_size, "max_chars_per_call", 260);
    chunk_count = foc_json_get_long_slice(meta, meta_size, "chunk_count", -1);
    free(meta);
    if (max_chars <= 0) max_chars = 260;
    if (chunk_count <= 0) return -1;
    if (foc_split_text(segment->spoken_text, (size_t)max_chars, &chunks) != 0) return -1;
    if ((long)chunks.count != chunk_count) ok = 0;
    wav_stem(segment_wav, stem, sizeof(stem));
    for (i = 0; ok && i < chunks.count; i++) {
        char chunk_json[1024];
        size_t chunk_size = 0;
        char *chunk_meta;
        char *chunk_text;
        if (!find_latest_chunk_json(cache_dir, stem, (int)i + 1, chunk_json, sizeof(chunk_json))) {
            ok = -1;
            break;
        }
        chunk_meta = foc_read_file(chunk_json, &chunk_size);
        if (!chunk_meta) {
            ok = -1;
            break;
        }
        chunk_text = foc_json_get_string_slice(chunk_meta, chunk_size, "text");
        if (!chunk_text || strcmp(chunk_text, chunks.items[i]) != 0) ok = 0;
        free(chunk_text);
        free(chunk_meta);
    }
    foc_free_chunks(&chunks);
    return ok;
}

int main(int argc, char **argv) {
    const char *script_path = argc > 1 ? argv[1] : "text/foc_script_txt/foc_script.txt";
    const char *cache_dir = argc > 2 ? argv[2] : ".cache/chatterbox/foc_script_segments";
    int show_all = argc > 3 && strcmp(argv[3], "--all") == 0;
    FocScript script = {0};
    size_t i;
    size_t prefix_found = 0, usable_cached = 0, verified_cached = 0, unverified_cached = 0, missing = 0, bad = 0, stale = 0;
    size_t contiguous_cached = 0;
    size_t chunks = 0;
    int first_missing = 0;
    int contiguous = 1;
    int sample_rate = 0;
    uint64_t speech_frames = 0;

    if (foc_load_script(script_path, &script, 0) != 0) return 2;

    if (show_all) puts("segment,source_line,speaker,status,duration_seconds,chunk_count,wav_path");
    for (i = 0; i < script.count; i++) {
        char json[1024], wav[1024];
        FocWavInfo info;
        int chunk_count = 0;
        double duration = 0.0;
        const char *status = "cached";

        if (!foc_find_latest_cache(cache_dir, &script.items[i], json, sizeof(json), wav, sizeof(wav))) {
            status = "missing";
            missing++;
            contiguous = 0;
            if (!first_missing) first_missing = script.items[i].index;
            if (show_all) printf("%d,%d,\"%s\",%s,0.000,0,\n",
                                 script.items[i].index, script.items[i].source_line, script.items[i].speaker, status);
            continue;
        }
        prefix_found++;
        if (foc_wav_info(wav, &info) != 0 || info.bits_per_sample != 16) {
            status = "bad_wav";
            bad++;
            contiguous = 0;
            if (!first_missing) first_missing = script.items[i].index;
            if (show_all) printf("%d,%d,\"%s\",%s,0.000,0,\"%s\"\n",
                                 script.items[i].index, script.items[i].source_line, script.items[i].speaker, status, wav);
            continue;
        }
        {
            int text_status = cache_text_status(cache_dir, &script.items[i], json, wav);
            if (text_status == 0) {
            status = "stale_text";
            stale++;
            contiguous = 0;
            if (!first_missing) first_missing = script.items[i].index;
            if (show_all) printf("%d,%d,\"%s\",%s,0.000,0,\"%s\"\n",
                                 script.items[i].index, script.items[i].source_line, script.items[i].speaker, status, wav);
            continue;
            }
            if (text_status < 0) {
                status = "unverified_cached";
                unverified_cached++;
            } else {
                verified_cached++;
            }
        }
        if (!sample_rate) sample_rate = info.sample_rate;
        duration = info.sample_rate ? (double)info.frames / (double)info.sample_rate : 0.0;
        chunk_count = foc_json_chunk_count(json);
        if (chunk_count > 0) chunks += (size_t)chunk_count;
        speech_frames += info.frames;
        usable_cached++;
        if (contiguous) contiguous_cached++;
        if (show_all) printf("%d,%d,\"%s\",%s,%.3f,%d,\"%s\"\n",
                             script.items[i].index, script.items[i].source_line, script.items[i].speaker,
                             status, duration, chunk_count, wav);
    }

    printf("script=%s\ncache_dir=%s\nsegments=%zu\nprefix_found=%zu\nusable_cached=%zu\nverified_cached=%zu\nunverified_cached=%zu\nstale_text=%zu\ncontiguous_usable_from_start=%zu\nmissing=%zu\nbad=%zu\nfirst_missing_or_stale_segment=%d\ncached_tts_chunks=%zu\nsample_rate=%d\ncached_speech_seconds=%.3f\n",
           script_path,
           cache_dir,
           script.count,
           prefix_found,
           usable_cached,
           verified_cached,
           unverified_cached,
           stale,
           contiguous_cached,
           missing,
           bad,
           first_missing,
           chunks,
           sample_rate,
           sample_rate ? (double)speech_frames / (double)sample_rate : 0.0);

    foc_free_script(&script);
    return (missing || bad || stale) ? 1 : 0;
}
