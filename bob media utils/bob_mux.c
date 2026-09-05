#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../common/cutil.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct {
    char *image;
    double start;
    double end;
} Row;

typedef struct {
    Row *items;
    size_t count;
    size_t cap;
} Rows;

static char *shell_quote(const char *s) {
    size_t extra = 3;
    for (const char *p = s; *p; p++) {
        extra += (*p == '\'') ? 4 : 1;
    }
    char *out = cu_xmalloc(extra + 1);
    char *w = out;
    *w++ = '\'';
    for (const char *p = s; *p; p++) {
        if (*p == '\'') {
            memcpy(w, "'\\''", 4);
            w += 4;
        } else {
            *w++ = *p;
        }
    }
    *w++ = '\'';
    *w = '\0';
    return out;
}

static bool extract_json_string(const char *line, const char *key, char **out) {
    char pattern[128];
    const char *p;
    const char *start;
    char *w;

    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    p = strstr(line, pattern);
    if (!p) {
        return false;
    }
    start = p + strlen(pattern);
    w = cu_xmalloc(strlen(start) + 1);
    *out = w;
    for (p = start; *p; p++) {
        if (*p == '\\' && p[1]) {
            p++;
            *w++ = *p;
        } else if (*p == '"') {
            *w = '\0';
            return true;
        } else {
            *w++ = *p;
        }
    }
    free(*out);
    *out = NULL;
    return false;
}

static bool extract_json_double(const char *line, const char *key, double *out) {
    char pattern[128];
    const char *p;
    char *end = NULL;

    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    p = strstr(line, pattern);
    if (!p) {
        return false;
    }
    p += strlen(pattern);
    errno = 0;
    *out = strtod(p, &end);
    return errno == 0 && end && end != p;
}

static void rows_add(Rows *rows, char *image, double start, double end) {
    if (rows->count == rows->cap) {
        rows->cap = rows->cap ? rows->cap * 2 : 128;
        rows->items = cu_xrealloc(rows->items, rows->cap * sizeof(*rows->items));
    }
    rows->items[rows->count].image = image;
    rows->items[rows->count].start = start;
    rows->items[rows->count].end = end;
    rows->count++;
}

static Rows load_rows(const char *path) {
    char *data = cu_read_text_file(path);
    char *save = NULL;
    char *line = strtok_r(data, "\n", &save);
    Rows rows = {0};

    while (line) {
        char *image = NULL;
        double start = 0.0;
        double end = 0.0;

        if (line[0] &&
            extract_json_string(line, "image_filename", &image) &&
            extract_json_double(line, "start_seconds", &start) &&
            extract_json_double(line, "end_seconds", &end)) {
            rows_add(&rows, image, start, end);
        } else if (image) {
            free(image);
        }
        line = strtok_r(NULL, "\n", &save);
    }
    free(data);
    if (rows.count == 0) {
        cu_die("no timing rows loaded");
    }
    return rows;
}

static void write_concat(const char *path, const char *images_dir, const Rows *rows) {
    FILE *out = fopen(path, "wb");
    if (!out) {
        cu_die_errno("open concat failed");
    }
    fputs("ffconcat version 1.0\n", out);
    for (size_t i = 0; i < rows->count; i++) {
        char full[PATH_MAX];
        double duration = rows->items[i].end - rows->items[i].start;
        snprintf(full, sizeof(full), "%s/%s", images_dir, rows->items[i].image);
        FILE *check = fopen(full, "rb");
        if (!check) {
            fprintf(stderr, "bob_mux: missing image %s\n", full);
            exit(1);
        }
        fclose(check);
        fprintf(out, "file '%s'\n", full);
        fprintf(out, "duration %.6f\n", duration > 0.033334 ? duration : 0.033334);
    }
    fprintf(out, "file '%s/%s'\n", images_dir, rows->items[rows->count - 1].image);
    fclose(out);
}

static void run_checked(const char *command) {
    int status = system(command);
    if (status != 0) {
        fprintf(stderr, "bob_mux: command failed:\n%s\n", command);
        exit(1);
    }
}

int main(int argc, char **argv) {
    Rows rows;
    char command[8192];

    if (argc != 7) {
        fprintf(stderr, "usage: %s GROUPS.jsonl IMAGES_DIR AUDIO.wav OUTPUT.mp4 CONCAT.ffconcat FPS\n", argv[0]);
        return 2;
    }
    cu_set_program_name("bob_mux");

    rows = load_rows(argv[1]);
    write_concat(argv[5], argv[2], &rows);

    char *q_concat = shell_quote(argv[5]);
    char *q_audio = shell_quote(argv[3]);
    char *q_output = shell_quote(argv[4]);
    snprintf(command, sizeof(command),
             "ffmpeg -hide_banner -y -f concat -safe 0 -i %s -i %s "
             "-vf fps=%s,scale=1280:720:flags=lanczos,format=yuv420p "
             "-c:v libx264 -preset medium -crf 18 -c:a aac -b:a 160k -shortest %s",
             q_concat, q_audio, argv[6], q_output);
    run_checked(command);

    free(q_concat);
    free(q_audio);
    free(q_output);
    printf("%s\n", argv[4]);
    return 0;
}
