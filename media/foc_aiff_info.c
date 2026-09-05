#include "foc_common.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static uint16_t be16(const unsigned char *p) { return ((uint16_t)p[0] << 8) | (uint16_t)p[1]; }
static uint32_t be32(const unsigned char *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3]; }

static int read_aiff(const char *path) {
    FILE *f = fopen(path, "rb");
    unsigned char h[12];
    int channels = 0, bits = 0, sample_rate = 0;
    uint32_t frames = 0;
    if (!f) { perror(path); return 1; }
    if (fread(h, 1, 12, f) != 12 || memcmp(h, "FORM", 4) || memcmp(h + 8, "AIFF", 4)) {
        fclose(f);
        fprintf(stderr, "%s: not AIFF\n", path);
        return 1;
    }
    while (1) {
        unsigned char ch[8], comm[18];
        uint32_t n;
        long pos;
        if (fread(ch, 1, 8, f) != 8) break;
        n = be32(ch + 4);
        pos = ftell(f);
        if (!memcmp(ch, "COMM", 4) && n >= 18) {
            if (fread(comm, 1, 18, f) != 18) break;
            channels = be16(comm);
            frames = be32(comm + 2);
            bits = be16(comm + 6);
            sample_rate = 0;
        }
        fseek(f, pos + n + (n & 1), SEEK_SET);
    }
    fclose(f);
    if (!channels || !bits) {
        fprintf(stderr, "%s: no COMM chunk\n", path);
        return 1;
    }
    printf("%s\tchannels=%d\tbits=%d\tframes=%u\tduration_at_24000=%.3f\tfile_bytes=%llu\n",
           path, channels, bits, frames, (double)frames / 24000.0, (unsigned long long)foc_file_size(path));
    (void)sample_rate;
    return 0;
}

int main(int argc, char **argv) {
    int i, rc = 0;
    if (argc < 2) {
        fprintf(stderr, "usage: %s file.aiff [file.aiff...]\n", argv[0]);
        return 2;
    }
    for (i = 1; i < argc; i++) if (read_aiff(argv[i]) != 0) rc = 1;
    return rc;
}
