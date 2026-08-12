#include "foc_common.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    const char *script_path = argc > 1 ? argv[1] : "text/foc_script_txt/foc_script.txt";
    size_t max_chars = argc > 2 ? (size_t)strtoul(argv[2], NULL, 10) : 260;
    FocScript script = {0};
    size_t i, total_chunks = 0, max_chunk_count = 0, max_segment = 0;
    if (foc_load_script(script_path, &script, 0) != 0) return 2;
    for (i = 0; i < script.count; i++) {
        FocChunkList chunks = {0};
        if (foc_split_text(script.items[i].spoken_text, max_chars, &chunks) != 0) return 3;
        total_chunks += chunks.count;
        if (chunks.count > max_chunk_count) {
            max_chunk_count = chunks.count;
            max_segment = i + 1;
        }
        if (chunks.count > 1) {
            printf("%04zu source_line=%d speaker=\"%s\" chars=%zu words=%zu chunks=%zu\n",
                   i + 1, script.items[i].source_line, script.items[i].speaker,
                   script.items[i].chars, script.items[i].words, chunks.count);
        }
        foc_free_chunks(&chunks);
    }
    printf("segments=%zu\nmax_chars=%zu\ntotal_tts_chunks=%zu\nmax_chunk_segment=%zu\nmax_chunk_count=%zu\n",
           script.count, max_chars, total_chunks, max_segment, max_chunk_count);
    foc_free_script(&script);
    return 0;
}
