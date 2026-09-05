#include "foc_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    FILE *out;
    int count;
    int include_trailing_silence;
} SrtCtx;

static void print_srt_time(FILE *out, double seconds) {
    int ms;
    int total;
    int h, m, s;
    if (seconds < 0.0) seconds = 0.0;
    ms = (int)(seconds * 1000.0 + 0.5);
    total = ms / 1000;
    ms %= 1000;
    h = total / 3600;
    m = (total / 60) % 60;
    s = total % 60;
    fprintf(out, "%02d:%02d:%02d,%03d", h, m, s, ms);
}

static void print_flat_text(FILE *out, const char *text) {
    int in_space = 0;
    while (*text) {
        unsigned char c = (unsigned char)*text++;
        if (c == '\r' || c == '\n' || c == '\t') c = ' ';
        if (c == ' ') {
            if (!in_space) fputc(' ', out);
            in_space = 1;
        } else {
            fputc(c, out);
            in_space = 0;
        }
    }
}

static int write_srt_segment(const char *object, size_t len, void *raw) {
    SrtCtx *ctx = (SrtCtx *)raw;
    double start = foc_json_get_number_slice(object, len, "start_seconds", 0.0);
    double end = ctx->include_trailing_silence
        ? foc_json_get_number_slice(object, len, "end_seconds", start)
        : foc_json_get_number_slice(object, len, "speech_end_seconds", foc_json_get_number_slice(object, len, "end_seconds", start));
    char *text = foc_json_get_string_slice(object, len, "spoken_text");
    if (!text) text = foc_json_get_string_slice(object, len, "source_text");
    if (!text) text = foc_json_unescape("", "");

    ctx->count++;
    fprintf(ctx->out, "%d\n", ctx->count);
    print_srt_time(ctx->out, start);
    fputs(" --> ", ctx->out);
    print_srt_time(ctx->out, end);
    fputc('\n', ctx->out);
    print_flat_text(ctx->out, text);
    fputs("\n\n", ctx->out);
    free(text);
    return 0;
}

int main(int argc, char **argv) {
    const char *manifest_path = argc > 1 ? argv[1] : "text/foc_script_txt/foc_script.json";
    const char *out_path = argc > 2 ? argv[2] : "text/foc_script_txt/foc_script.srt";
    const char *mode = argc > 3 ? argv[3] : "speech";
    SrtCtx ctx = {0};
    int result;

    ctx.include_trailing_silence = strcmp(mode, "line") == 0;
    ctx.out = fopen(out_path, "wb");
    if (!ctx.out) {
        perror(out_path);
        return 2;
    }
    result = foc_json_each_array_object(manifest_path, "segments", write_srt_segment, &ctx);
    fclose(ctx.out);
    if (result < 0) return 2;
    printf("wrote %s\nsubtitles=%d\nmode=%s\n", out_path, ctx.count, ctx.include_trailing_silence ? "line" : "speech");
    return ctx.count ? 0 : 1;
}
