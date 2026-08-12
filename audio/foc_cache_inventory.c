#define _DARWIN_C_SOURCE
#include "foc_common.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct {
    uint64_t dirs;
    uint64_t files;
    uint64_t wav_files;
    uint64_t json_files;
    uint64_t other_files;
    uint64_t segment_wav;
    uint64_t segment_json;
    uint64_t unique_segment_wav;
    uint64_t unique_segment_json;
    uint64_t chunk_wav;
    uint64_t chunk_json;
    uint64_t bytes;
    uint64_t wav_bytes;
    time_t newest_mtime;
    char newest_path[PATH_MAX];
    unsigned char seen_segment_wav[10000];
    unsigned char seen_segment_json[10000];
} Inventory;

static int segment_id_from_name(const char *name) {
    int i;
    int value = 0;
    for (i = 0; i < 4; i++) {
        if (name[i] < '0' || name[i] > '9') return 0;
        value = value * 10 + (name[i] - '0');
    }
    return name[4] == '-' ? value : 0;
}

static int scan_dir(const char *path, int in_chunks, Inventory *inv) {
    DIR *dir = opendir(path);
    struct dirent *entry;
    if (!dir) {
        fprintf(stderr, "cannot open %s: %s\n", path, strerror(errno));
        return -1;
    }
    inv->dirs++;
    while ((entry = readdir(dir)) != NULL) {
        char full[PATH_MAX];
        struct stat st;
        int child_in_chunks = in_chunks || strcmp(entry->d_name, "chunks") == 0;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (snprintf(full, sizeof(full), "%s/%s", path, entry->d_name) >= (int)sizeof(full)) {
            fprintf(stderr, "path too long under %s\n", path);
            closedir(dir);
            return -1;
        }
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            if (scan_dir(full, child_in_chunks, inv) != 0) {
                closedir(dir);
                return -1;
            }
            continue;
        }
        if (!S_ISREG(st.st_mode)) continue;
        inv->files++;
        inv->bytes += (uint64_t)st.st_size;
        if (st.st_mtime >= inv->newest_mtime) {
            inv->newest_mtime = st.st_mtime;
            snprintf(inv->newest_path, sizeof(inv->newest_path), "%s", full);
        }
        if (foc_has_suffix(entry->d_name, ".wav")) {
            inv->wav_files++;
            inv->wav_bytes += (uint64_t)st.st_size;
            if (in_chunks) inv->chunk_wav++;
            else {
                int segment_id = segment_id_from_name(entry->d_name);
                inv->segment_wav++;
                if (segment_id > 0 && segment_id < 10000 && !inv->seen_segment_wav[segment_id]) {
                    inv->seen_segment_wav[segment_id] = 1;
                    inv->unique_segment_wav++;
                }
            }
        } else if (foc_has_suffix(entry->d_name, ".json")) {
            inv->json_files++;
            if (in_chunks) inv->chunk_json++;
            else {
                int segment_id = segment_id_from_name(entry->d_name);
                inv->segment_json++;
                if (segment_id > 0 && segment_id < 10000 && !inv->seen_segment_json[segment_id]) {
                    inv->seen_segment_json[segment_id] = 1;
                    inv->unique_segment_json++;
                }
            }
        } else {
            inv->other_files++;
        }
    }
    closedir(dir);
    return 0;
}

int main(int argc, char **argv) {
    const char *cache_dir = argc > 1 ? argv[1] : ".cache/chatterbox/foc_script_segments";
    uint64_t expected_segments = argc > 2 ? strtoull(argv[2], NULL, 10) : 0u;
    const char *script_path = argc > 3 ? argv[3] : NULL;
    Inventory inv;
    memset(&inv, 0, sizeof(inv));
    if (scan_dir(cache_dir, 0, &inv) != 0) return 2;
    printf("cache_dir=%s\n", cache_dir);
    printf("dirs=%llu\nfiles=%llu\nbytes=%llu\nwav_files=%llu\nwav_bytes=%llu\njson_files=%llu\nother_files=%llu\n",
           (unsigned long long)inv.dirs,
           (unsigned long long)inv.files,
           (unsigned long long)inv.bytes,
           (unsigned long long)inv.wav_files,
           (unsigned long long)inv.wav_bytes,
           (unsigned long long)inv.json_files,
           (unsigned long long)inv.other_files);
    printf("segment_wav=%llu\nsegment_json=%llu\nunique_segment_wav=%llu\nunique_segment_json=%llu\nduplicate_segment_wav=%llu\nduplicate_segment_json=%llu\nchunk_wav=%llu\nchunk_json=%llu\n",
           (unsigned long long)inv.segment_wav,
           (unsigned long long)inv.segment_json,
           (unsigned long long)inv.unique_segment_wav,
           (unsigned long long)inv.unique_segment_json,
           (unsigned long long)(inv.segment_wav - inv.unique_segment_wav),
           (unsigned long long)(inv.segment_json - inv.unique_segment_json),
           (unsigned long long)inv.chunk_wav,
           (unsigned long long)inv.chunk_json);
    if (expected_segments) {
        uint64_t remaining = inv.unique_segment_wav >= expected_segments ? 0u : expected_segments - inv.unique_segment_wav;
        printf("expected_segments=%llu\nremaining_segment_wav=%llu\n",
               (unsigned long long)expected_segments,
               (unsigned long long)remaining);
    }
    printf("newest_mtime_epoch=%lld\nnewest_path=%s\n",
           (long long)inv.newest_mtime,
           inv.newest_path[0] ? inv.newest_path : "");
    if (script_path && script_path[0]) {
        FocScript script = {0};
        size_t i;
        uint64_t script_cached = 0;
        uint64_t script_bad = 0;
        if (foc_load_script(script_path, &script, 0) != 0) return 2;
        for (i = 0; i < script.count; i++) {
            char json_path[PATH_MAX];
            char wav_path[PATH_MAX];
            FocWavInfo info;
            if (!foc_find_latest_cache(cache_dir, &script.items[i], json_path, sizeof(json_path), wav_path, sizeof(wav_path))) {
                continue;
            }
            if (foc_wav_info(wav_path, &info) != 0) {
                script_bad++;
                continue;
            }
            script_cached++;
        }
        printf("script=%s\nscript_segments=%zu\nscript_prefix_cached=%llu\nscript_prefix_missing=%llu\nscript_prefix_bad=%llu\n",
               script_path,
               script.count,
               (unsigned long long)script_cached,
               (unsigned long long)(script.count > script_cached ? script.count - script_cached : 0),
               (unsigned long long)script_bad);
        foc_free_script(&script);
    }
    return 0;
}
