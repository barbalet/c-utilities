#include "foc_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *script_path = argc > 1 ? argv[1] : "text/foc_script_txt/foc_script.txt";
    const char *out_path = argc > 2 ? argv[2] : "-";
    size_t max_chars = argc > 3 ? (size_t)strtoul(argv[3], NULL, 10) : 260;
    FocScript script = {0};
    FILE *out = stdout;
    size_t i, j;
    if (foc_load_script(script_path, &script, 0) != 0) return 2;
    if (out_path[0] != '-' || out_path[1] != '\0') {
        out = fopen(out_path, "wb");
        if (!out) { perror(out_path); foc_free_script(&script); return 2; }
    }
    for (i = 0; i < script.count; i++) {
        FocChunkList chunks = {0};
        if (foc_split_text(script.items[i].spoken_text, max_chars, &chunks) != 0) return 3;
        for (j = 0; j < chunks.count; j++) {
            fprintf(out, "{\"segment_index\":%d,\"source_line\":%d,\"speaker\":",
                    script.items[i].index, script.items[i].source_line);
            foc_json_escape(out, script.items[i].speaker);
            fprintf(out, ",\"chunk_index\":%zu,\"chunk_count\":%zu,\"text\":", j + 1, chunks.count);
            foc_json_escape(out, chunks.items[j]);
            fprintf(out, ",\"chars\":%zu}\n", strlen(chunks.items[j]));
        }
        foc_free_chunks(&chunks);
    }
    if (out != stdout) fclose(out);
    foc_free_script(&script);
    return 0;
}
