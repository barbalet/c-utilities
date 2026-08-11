#include "foc_common.h"
#include <stdio.h>

int main(int argc, char **argv) {
    int i;
    if (argc < 2) {
        fprintf(stderr, "usage: %s file.wav [file.wav...]\n", argv[0]);
        return 2;
    }
    for (i = 1; i < argc; i++) {
        FocWavInfo info;
        if (foc_wav_info(argv[i], &info) != 0) {
            fprintf(stderr, "%s: not a supported PCM WAV\n", argv[i]);
            return 1;
        }
        printf("%s\tsample_rate=%d\tchannels=%d\tbits=%d\tframes=%llu\tduration=%.3f\n",
               argv[i], info.sample_rate, info.channels, info.bits_per_sample,
               (unsigned long long)info.frames, (double)info.frames / (double)info.sample_rate);
    }
    return 0;
}
