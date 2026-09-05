#include "foc_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    FILE *out;
    int count;
    int width;
    int height;
} QueueCtx;

static int write_queue_item(const char *object, size_t len, void *raw) {
    QueueCtx *ctx = (QueueCtx *)raw;
    long segment_index = foc_json_get_long_slice(object, len, "segment_index", ctx->count + 1);
    long source_line = foc_json_get_long_slice(object, len, "source_line", 0);
    double start = foc_json_get_number_slice(object, len, "start_seconds", 0.0);
    double end = foc_json_get_number_slice(object, len, "end_seconds", foc_json_get_number_slice(object, len, "speech_end_seconds", start));
    char *speaker = foc_json_get_string_slice(object, len, "speaker");
    char *spoken_text = foc_json_get_string_slice(object, len, "spoken_text");
    char *image_filename = foc_json_get_string_slice(object, len, "image_filename");
    char generated_name[128];

    if (!speaker) speaker = foc_json_unescape("", "");
    if (!spoken_text) spoken_text = foc_json_get_string_slice(object, len, "source_text");
    if (!spoken_text) spoken_text = foc_json_unescape("", "");
    if (!image_filename) {
        char *slug = foc_slug(spoken_text, 72);
        snprintf(generated_name, sizeof(generated_name), "%04ld-%s.png", segment_index, slug ? slug : "line");
        free(slug);
        image_filename = foc_json_unescape(generated_name, generated_name + strlen(generated_name));
    }

    fprintf(ctx->out, "{\"segment_index\":%ld,\"source_line\":%ld,\"speaker\":", segment_index, source_line);
    foc_json_escape(ctx->out, speaker);
    fputs(",\"spoken_text\":", ctx->out);
    foc_json_escape(ctx->out, spoken_text);
    fputs(",\"image_filename\":", ctx->out);
    foc_json_escape(ctx->out, image_filename);
    fprintf(ctx->out, ",\"start_seconds\":%.6f,\"end_seconds\":%.6f,\"width\":%d,\"height\":%d}\n",
            start, end, ctx->width, ctx->height);

    free(speaker);
    free(spoken_text);
    free(image_filename);
    ctx->count++;
    return 0;
}

int main(int argc, char **argv) {
    const char *manifest_path = argc > 1 ? argv[1] : "text/foc_script_txt/foc_script.json";
    const char *out_path = argc > 2 ? argv[2] : "text/foc_script_txt/foc_script_png_queue.jsonl";
    QueueCtx ctx = {0};
    int result;

    ctx.width = argc > 3 ? atoi(argv[3]) : 1080;
    ctx.height = argc > 4 ? atoi(argv[4]) : 1920;
    if (ctx.width <= 0) ctx.width = 1080;
    if (ctx.height <= 0) ctx.height = 1920;

    ctx.out = fopen(out_path, "wb");
    if (!ctx.out) {
        perror(out_path);
        return 2;
    }
    result = foc_json_each_array_object(manifest_path, "segments", write_queue_item, &ctx);
    fclose(ctx.out);
    if (result < 0) return 2;
    printf("wrote %s\nframes=%d\nsize=%dx%d\n", out_path, ctx.count, ctx.width, ctx.height);
    return ctx.count ? 0 : 1;
}
