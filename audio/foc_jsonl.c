#include "foc_common.h"
#include <stdio.h>

int main(int argc, char **argv) {
    const char *script_path = argc > 1 ? argv[1] : "text/foc_script_txt/foc_script.txt";
    const char *out_path = argc > 2 ? argv[2] : "-";
    FocScript script = {0};
    FILE *out = stdout;
    size_t i;
    if (foc_load_script(script_path, &script, 0) != 0) return 2;
    if (out_path[0] != '-' || out_path[1] != '\0') {
        out = fopen(out_path, "wb");
        if (!out) { perror(out_path); foc_free_script(&script); return 2; }
    }
    for (i = 0; i < script.count; i++) {
        FocSegment *s = &script.items[i];
        fprintf(out, "{\"segment_index\":%d,\"source_line\":%d,\"speaker\":", s->index, s->source_line);
        foc_json_escape(out, s->speaker);
        fputs(",\"spoken_text\":", out);
        foc_json_escape(out, s->spoken_text);
        fprintf(out, ",\"chars\":%zu,\"words\":%zu}\n", s->chars, s->words);
    }
    if (out != stdout) fclose(out);
    foc_free_script(&script);
    return 0;
}
