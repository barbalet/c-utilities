#include "foc_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *unescape_json_string(const char *start, const char *end) {
    char *out = (char *)malloc((size_t)(end - start) + 1);
    char *w = out;
    const char *p = start;
    if (!out) return NULL;
    while (p < end) {
        if (*p == '\\' && p + 1 < end) {
            p++;
            if (*p == 'n') *w++ = '\n';
            else if (*p == 'r') *w++ = '\r';
            else if (*p == 't') *w++ = '\t';
            else *w++ = *p;
            p++;
        } else {
            *w++ = *p++;
        }
    }
    *w = '\0';
    return out;
}

static int each_cache_path(const char *manifest, int (*cb)(const char *, void *), void *ctx) {
    size_t size = 0;
    char *data = foc_read_file(manifest, &size);
    const char *needle = "\"segment_cache_audio\"";
    char *p;
    int count = 0;
    (void)size;
    if (!data) { perror(manifest); return -1; }
    p = data;
    while ((p = strstr(p, needle)) != NULL) {
        char *colon = strchr(p, ':');
        char *quote;
        char *end;
        char *path;
        if (!colon) break;
        quote = strchr(colon, '"');
        if (!quote) break;
        quote++;
        end = quote;
        while (*end) {
            if (*end == '"' && (end == quote || end[-1] != '\\')) break;
            end++;
        }
        if (!*end) break;
        path = unescape_json_string(quote, end);
        if (!path) { free(data); return -1; }
        if (cb(path, ctx) != 0) { free(path); free(data); return -1; }
        free(path);
        count++;
        p = end + 1;
    }
    free(data);
    return count;
}

typedef struct {
    int expected_rate;
    int expected_channels;
    int bad;
    uint64_t frames;
} CheckCtx;

static int check_path(const char *path, void *raw) {
    CheckCtx *ctx = (CheckCtx *)raw;
    FocWavInfo info;
    if (foc_wav_info(path, &info) != 0) {
        fprintf(stderr, "missing_or_bad_wav=%s\n", path);
        ctx->bad++;
        return 0;
    }
    if (!ctx->expected_rate) {
        ctx->expected_rate = info.sample_rate;
        ctx->expected_channels = info.channels;
    }
    if (info.sample_rate != ctx->expected_rate || info.channels != ctx->expected_channels || info.bits_per_sample != 16) {
        fprintf(stderr, "format_mismatch=%s\n", path);
        ctx->bad++;
    }
    ctx->frames += info.frames;
    return 0;
}

int main(int argc, char **argv) {
    const char *manifest = argc > 1 ? argv[1] : "text/foc_script_txt/foc_script.python-preview.json";
    CheckCtx ctx = {0};
    int count = each_cache_path(manifest, check_path, &ctx);
    if (count < 0) return 2;
    printf("manifest=%s\ncache_paths=%d\nbad_paths=%d\nsample_rate=%d\nchannels=%d\nspeech_duration_seconds=%.3f\n",
           manifest, count, ctx.bad, ctx.expected_rate, ctx.expected_channels,
           ctx.expected_rate ? (double)ctx.frames / (double)ctx.expected_rate : 0.0);
    return ctx.bad ? 1 : 0;
}
