#include "foc_common.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    const char *script_path = argc > 1 ? argv[1] : "text/foc_script_txt/foc_script.txt";
    const char *cache_dir = argc > 2 ? argv[2] : ".cache/chatterbox/foc_script_segments";
    double silence = argc > 3 ? atof(argv[3]) : 0.18;
    FocScript script = {0};
    double total = 0.0;
    size_t i;
    if (foc_load_script(script_path, &script, 0) != 0) return 2;
    printf("segment,source_line,speaker,duration_seconds,cumulative_seconds,status\n");
    for (i = 0; i < script.count; i++) {
        char json[1024], wav[1024];
        double dur;
        if (!foc_find_latest_cache(cache_dir, &script.items[i], json, sizeof(json), wav, sizeof(wav))) {
            printf("%zu,%d,\"%s\",0.000,%.3f,missing\n", i + 1, script.items[i].source_line, script.items[i].speaker, total);
            break;
        }
        dur = foc_json_duration_seconds(json);
        if (dur < 0.0) dur = 0.0;
        if (i) total += silence;
        total += dur;
        printf("%zu,%d,\"%s\",%.3f,%.3f,cached\n", i + 1, script.items[i].source_line, script.items[i].speaker, dur, total);
    }
    foc_free_script(&script);
    return 0;
}
