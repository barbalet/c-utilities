#include "foc_common.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    const char *script_path = argc > 1 ? argv[1] : "text/foc_script_txt/foc_script.txt";
    const char *cache_dir = argc > 2 ? argv[2] : ".cache/chatterbox/foc_script_segments";
    double max_seconds = argc > 3 ? atof(argv[3]) : 600.0;
    const double silence = argc > 4 ? atof(argv[4]) : 0.18;
    FocScript script = {0};
    double total = 0.0;
    size_t i, contiguous = 0;
    if (foc_load_script(script_path, &script, 0) != 0) return 2;
    for (i = 0; i < script.count; i++) {
        char json[1024], wav[1024];
        double dur;
        if (!foc_find_latest_cache(cache_dir, &script.items[i], json, sizeof(json), wav, sizeof(wav))) break;
        dur = foc_json_duration_seconds(json);
        if (dur < 0.0) break;
        if (i) total += silence;
        total += dur;
        contiguous++;
        if (total >= max_seconds) {
            printf("status=duration_cap_available\n");
            printf("boundary_segment=%zu\nsource_line=%d\nspeaker=%s\nseconds=%.3f\n", i + 1, script.items[i].source_line, script.items[i].speaker, total);
            foc_free_script(&script);
            return 0;
        }
    }
    printf("status=more_tts_needed\n");
    printf("contiguous_rendered=%zu\nseconds=%.3f\n", contiguous, total);
    if (contiguous < script.count) {
        FocSegment *next = &script.items[contiguous];
        printf("next_segment=%zu\nsource_line=%d\nspeaker=%s\nchars=%zu\nwords=%zu\n", contiguous + 1, next->source_line, next->speaker, next->chars, next->words);
    }
    printf("target_seconds=%.3f\nremaining_seconds=%.3f\n", max_seconds, max_seconds - total);
    foc_free_script(&script);
    return 0;
}
