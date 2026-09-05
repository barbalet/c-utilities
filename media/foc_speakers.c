#include "foc_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *name; size_t lines, words, chars; } SpeakerCount;

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "text/foc_script_txt/foc_script.txt";
    FocScript script = {0};
    SpeakerCount *counts = NULL;
    size_t count = 0, cap = 0, i, j;
    if (foc_load_script(path, &script, 0) != 0) return 2;
    for (i = 0; i < script.count; i++) {
        FocSegment *seg = &script.items[i];
        for (j = 0; j < count; j++) if (strcmp(counts[j].name, seg->speaker) == 0) break;
        if (j == count) {
            if (count == cap) {
                cap = cap ? cap * 2 : 32;
                counts = (SpeakerCount *)realloc(counts, cap * sizeof(*counts));
                if (!counts) return 3;
            }
            counts[j].name = seg->speaker;
            counts[j].lines = counts[j].words = counts[j].chars = 0;
            count++;
        }
        counts[j].lines++;
        counts[j].words += seg->words;
        counts[j].chars += seg->chars;
    }
    printf("unique_speakers=%zu\n", count);
    for (i = 0; i < count; i++) {
        printf("%02zu\t%s\tlines=%zu\twords=%zu\tchars=%zu\n", i + 1, counts[i].name, counts[i].lines, counts[i].words, counts[i].chars);
    }
    free(counts);
    foc_free_script(&script);
    return 0;
}
