#include "foc_common.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    const char *script_path = argc > 1 ? argv[1] : "text/foc_script_txt/foc_script.txt";
    size_t max_chunks_per_batch = argc > 2 ? (size_t)strtoul(argv[2], NULL, 10) : 60u;
    size_t max_chars = argc > 3 ? (size_t)strtoul(argv[3], NULL, 10) : 260u;
    FocScript script = {0};
    size_t i;
    size_t batch = 1;
    size_t batch_start = 1;
    size_t batch_segments = 0;
    size_t batch_chunks = 0;
    size_t total_chunks = 0;

    if (max_chunks_per_batch == 0) max_chunks_per_batch = 60u;
    if (max_chars == 0) max_chars = 260u;
    if (foc_load_script(script_path, &script, 0) != 0) return 2;

    puts("batch,start_segment,end_segment,start_source_line,end_source_line,segments,tts_chunks");
    for (i = 0; i < script.count; i++) {
        FocChunkList chunks = {0};
        size_t chunk_count;
        if (foc_split_text(script.items[i].spoken_text, max_chars, &chunks) != 0) {
            foc_free_script(&script);
            return 2;
        }
        chunk_count = chunks.count;
        foc_free_chunks(&chunks);

        if (batch_segments && batch_chunks + chunk_count > max_chunks_per_batch) {
            size_t end = i;
            printf("%zu,%zu,%zu,%d,%d,%zu,%zu\n",
                   batch,
                   batch_start,
                   end,
                   script.items[batch_start - 1].source_line,
                   script.items[end - 1].source_line,
                   batch_segments,
                   batch_chunks);
            batch++;
            batch_start = i + 1;
            batch_segments = 0;
            batch_chunks = 0;
        }

        batch_segments++;
        batch_chunks += chunk_count;
        total_chunks += chunk_count;
    }

    if (batch_segments) {
        printf("%zu,%zu,%zu,%d,%d,%zu,%zu\n",
               batch,
               batch_start,
               script.count,
               script.items[batch_start - 1].source_line,
               script.items[script.count - 1].source_line,
               batch_segments,
               batch_chunks);
    }
    fprintf(stderr, "segments=%zu\ntts_chunks=%zu\nbatches=%zu\nmax_chunks_per_batch=%zu\nmax_chars=%zu\n",
            script.count, total_chunks, batch_segments ? batch : 0, max_chunks_per_batch, max_chars);

    foc_free_script(&script);
    return 0;
}
