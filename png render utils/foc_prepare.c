#include <ctype.h>
#include <errno.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    char *name;
    char *tail;
} ExistingEntry;

typedef struct {
    ExistingEntry *items;
    size_t count;
    size_t cap;
} ExistingList;

typedef struct {
    char **items;
    size_t count;
    size_t cap;
} NameList;

static const char *known_mentions[] = {
    "Tom Barbalay",
    "Margaret Barbalay",
    "The Professor",
    "The Mystic Seven",
    "Jack Barbalay",
    "Paul Keating",
    "John Jones",
    "Cosmic Storm",
    "Suzanna",
    "Henry",
    "Kurt",
    "Kevin",
    "Judges",
    "Bruce",
    "Amy",
    "Sarah",
    "John Draper",
    "23103",
    "23152",
    "23459",
};

static void die(const char *message) {
    fprintf(stderr, "foc_prepare: %s\n", message);
    exit(1);
}

static void *xrealloc(void *ptr, size_t size) {
    void *next = realloc(ptr, size);
    if (!next) {
        die("out of memory");
    }
    return next;
}

static char *xstrdup(const char *s) {
    char *copy = strdup(s);
    if (!copy) {
        die("out of memory");
    }
    return copy;
}

static void trim_inplace(char *s) {
    size_t len;
    char *start = s;

    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }

    len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
}

static void strip_newline(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[--len] = '\0';
    }
}

static bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static bool dir_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static bool ends_with(const char *s, const char *suffix) {
    size_t slen = strlen(s);
    size_t suffix_len = strlen(suffix);
    return slen >= suffix_len && strcmp(s + slen - suffix_len, suffix) == 0;
}

static int parse_positive_int(const char *s, const char *label) {
    char *end = NULL;
    long value;

    errno = 0;
    value = strtol(s, &end, 10);
    if (errno != 0 || !end || *end != '\0' || value <= 0 || value > 1000000) {
        fprintf(stderr, "foc_prepare: invalid %s: %s\n", label, s);
        exit(1);
    }
    return (int)value;
}

static double parse_positive_double(const char *s, const char *label) {
    char *end = NULL;
    double value;

    errno = 0;
    value = strtod(s, &end);
    if (errno != 0 || !end || *end != '\0' || value <= 0.0) {
        fprintf(stderr, "foc_prepare: invalid %s: %s\n", label, s);
        exit(1);
    }
    return value;
}

static bool name_boundary(char c) {
    return c == '\0' || (!isalnum((unsigned char)c) && c != '_');
}

static bool contains_name(const char *text, const char *name) {
    const size_t nlen = strlen(name);
    const char *hit = text;

    while ((hit = strstr(hit, name)) != NULL) {
        char before = hit == text ? '\0' : hit[-1];
        char after = hit[nlen];
        if (name_boundary(before) && name_boundary(after)) {
            return true;
        }
        hit++;
    }
    return false;
}

static void namelist_add(NameList *list, const char *raw_name) {
    char *name = xstrdup(raw_name);
    trim_inplace(name);
    if (name[0] == '\0') {
        free(name);
        return;
    }

    for (size_t i = 0; i < list->count; i++) {
        if (strcmp(list->items[i], name) == 0) {
            free(name);
            return;
        }
    }

    if (list->count == list->cap) {
        list->cap = list->cap ? list->cap * 2 : 32;
        list->items = xrealloc(list->items, list->cap * sizeof(*list->items));
    }
    list->items[list->count++] = name;
}

static void namelist_free(NameList *list) {
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i]);
    }
    free(list->items);
}

static const char *find_speaker_sep(const char *line) {
    const char *sep = strstr(line, "  :  ");
    if (sep) {
        return sep;
    }
    return strstr(line, " : ");
}

static size_t speaker_sep_len(const char *sep) {
    return strncmp(sep, "  :  ", 5) == 0 ? 5 : 3;
}

static char *slice(const char *start, size_t len) {
    char *out = malloc(len + 1);
    if (!out) {
        die("out of memory");
    }
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

static char *speaker_from_line(const char *line) {
    const char *sep = find_speaker_sep(line);
    char *speaker;

    if (!sep) {
        return xstrdup("");
    }
    speaker = slice(line, (size_t)(sep - line));
    trim_inplace(speaker);
    return speaker;
}

static char *text_from_line(const char *line) {
    const char *sep = find_speaker_sep(line);
    if (!sep) {
        return xstrdup(line);
    }
    return xstrdup(sep + speaker_sep_len(sep));
}

static void existing_add(ExistingList *list, const char *name, const char *tail) {
    if (list->count == list->cap) {
        list->cap = list->cap ? list->cap * 2 : 32;
        list->items = xrealloc(list->items, list->cap * sizeof(*list->items));
    }
    list->items[list->count].name = xstrdup(name);
    list->items[list->count].tail = xstrdup(tail);
    list->count++;
}

static void read_existing_characters(ExistingList *existing, const char *path) {
    FILE *f;
    char *line = NULL;
    size_t cap = 0;

    if (!file_exists(path)) {
        return;
    }

    f = fopen(path, "r");
    if (!f) {
        return;
    }

    while (getline(&line, &cap, f) != -1) {
        char *comma;
        char *name;
        strip_newline(line);
        trim_inplace(line);
        if (line[0] == '\0') {
            continue;
        }
        comma = strchr(line, ',');
        if (comma) {
            *comma = '\0';
            name = line;
            trim_inplace(name);
            existing_add(existing, name, comma + 1);
        } else {
            existing_add(existing, line, "");
        }
    }

    free(line);
    fclose(f);
}

static const char *existing_tail(const ExistingList *existing, const char *name) {
    for (size_t i = 0; i < existing->count; i++) {
        if (strcmp(existing->items[i].name, name) == 0) {
            return existing->items[i].tail;
        }
    }
    return NULL;
}

static bool namelist_contains(const NameList *list, const char *name) {
    for (size_t i = 0; i < list->count; i++) {
        if (strcmp(list->items[i], name) == 0) {
            return true;
        }
    }
    return false;
}

static void existing_free(ExistingList *existing) {
    for (size_t i = 0; i < existing->count; i++) {
        free(existing->items[i].name);
        free(existing->items[i].tail);
    }
    free(existing->items);
}

static void collect_characters(const char *script_path, NameList *names) {
    FILE *f = fopen(script_path, "r");
    char *line = NULL;
    size_t cap = 0;

    if (!f) {
        fprintf(stderr, "foc_prepare: cannot open %s: %s\n", script_path, strerror(errno));
        exit(1);
    }

    while (getline(&line, &cap, f) != -1) {
        char *speaker;
        strip_newline(line);
        if (line[0] == '\0') {
            continue;
        }

        speaker = speaker_from_line(line);
        if (speaker[0] != '\0') {
            namelist_add(names, speaker);
        }
        free(speaker);

        for (size_t i = 0; i < sizeof(known_mentions) / sizeof(known_mentions[0]); i++) {
            if (contains_name(line, known_mentions[i])) {
                namelist_add(names, known_mentions[i]);
            }
        }
    }

    free(line);
    fclose(f);
}

static int write_characters(const char *script_path, const char *out_path) {
    NameList names = {0};
    ExistingList existing = {0};
    FILE *out;

    collect_characters(script_path, &names);
    read_existing_characters(&existing, out_path);

    out = fopen(out_path, "w");
    if (!out) {
        fprintf(stderr, "foc_prepare: cannot write %s: %s\n", out_path, strerror(errno));
        existing_free(&existing);
        namelist_free(&names);
        return 1;
    }

    for (size_t i = 0; i < names.count; i++) {
        const char *tail = existing_tail(&existing, names.items[i]);
        if (tail && tail[0] != '\0') {
            fprintf(out, "%s,%s\n", names.items[i], tail);
        } else {
            fprintf(out, "%s\n", names.items[i]);
        }
    }

    for (size_t i = 0; i < existing.count; i++) {
        if (!namelist_contains(&names, existing.items[i].name)) {
            if (existing.items[i].tail[0] != '\0') {
                fprintf(out, "%s,%s\n", existing.items[i].name, existing.items[i].tail);
            } else {
                fprintf(out, "%s\n", existing.items[i].name);
            }
        }
    }

    fclose(out);
    existing_free(&existing);
    namelist_free(&names);
    return 0;
}

static void json_string(FILE *out, const char *s) {
    fputc('"', out);
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        switch (c) {
            case '\\':
                fputs("\\\\", out);
                break;
            case '"':
                fputs("\\\"", out);
                break;
            case '\b':
                fputs("\\b", out);
                break;
            case '\f':
                fputs("\\f", out);
                break;
            case '\n':
                fputs("\\n", out);
                break;
            case '\r':
                fputs("\\r", out);
                break;
            case '\t':
                fputs("\\t", out);
                break;
            default:
                if (c < 0x20) {
                    fprintf(out, "\\u%04x", c);
                } else {
                    fputc(c, out);
                }
        }
    }
    fputc('"', out);
}

static void frame_path(char *buf, size_t buflen, const char *frames_dir, int index) {
    size_t len = strlen(frames_dir);
    const char *slash = len > 0 && frames_dir[len - 1] == '/' ? "" : "/";
    snprintf(buf, buflen, "%s%sframe_%03d_source_line_%03d.png", frames_dir, slash, index, index);
}

static void character_path(char *buf, size_t buflen, const char *characters_dir, const char *name) {
    size_t len = strlen(characters_dir);
    const char *slash = len > 0 && characters_dir[len - 1] == '/' ? "" : "/";
    snprintf(buf, buflen, "%s%s%s.png", characters_dir, slash, name);
}

static int count_lines(const char *script_path) {
    FILE *f = fopen(script_path, "r");
    char *line = NULL;
    size_t cap = 0;
    int count = 0;

    if (!f) {
        fprintf(stderr, "foc_prepare: cannot open %s: %s\n", script_path, strerror(errno));
        exit(1);
    }

    while (getline(&line, &cap, f) != -1) {
        strip_newline(line);
        if (line[0] != '\0') {
            count++;
        }
    }
    free(line);
    fclose(f);
    return count;
}

static int write_manifest(const char *script_path, const char *frames_dir, const char *out_path) {
    FILE *script = fopen(script_path, "r");
    FILE *out;
    char *line = NULL;
    size_t cap = 0;
    int frame_count = count_lines(script_path);
    int generated_count = 0;
    int index = 0;

    if (!script) {
        fprintf(stderr, "foc_prepare: cannot open %s: %s\n", script_path, strerror(errno));
        return 1;
    }

    for (int i = 1; i <= frame_count; i++) {
        char path[4096];
        frame_path(path, sizeof(path), frames_dir, i);
        if (file_exists(path)) {
            generated_count++;
        }
    }

    out = fopen(out_path, "w");
    if (!out) {
        fprintf(stderr, "foc_prepare: cannot write %s: %s\n", out_path, strerror(errno));
        fclose(script);
        return 1;
    }

    fprintf(out, "{\n");
    fprintf(out, "  \"source_script\": ");
    json_string(out, script_path);
    fprintf(out, ",\n  \"frames_dir\": ");
    json_string(out, frames_dir);
    fprintf(out, ",\n  \"frame_count\": %d,\n", frame_count);
    fprintf(out, "  \"generated_count\": %d,\n", generated_count);
    fprintf(out, "  \"status\": \"%s\",\n", generated_count == frame_count ? "complete" : "in_progress");
    fprintf(out, "  \"frames\": [\n");

    while (getline(&line, &cap, script) != -1) {
        char path[4096];
        char *speaker;
        char *text;
        bool generated;

        strip_newline(line);
        if (line[0] == '\0') {
            continue;
        }

        index++;
        frame_path(path, sizeof(path), frames_dir, index);
        generated = file_exists(path);
        speaker = speaker_from_line(line);
        text = text_from_line(line);

        fprintf(out, "    {\n");
        fprintf(out, "      \"frame_index\": %d,\n", index);
        fprintf(out, "      \"line_index\": %d,\n", index);
        fprintf(out, "      \"speaker\": ");
        json_string(out, speaker);
        fprintf(out, ",\n      \"line\": ");
        json_string(out, line);
        fprintf(out, ",\n      \"text\": ");
        json_string(out, text);
        fprintf(out, ",\n      \"frame\": ");
        json_string(out, path);
        fprintf(out, ",\n      \"status\": \"%s\"\n", generated ? "generated" : "pending");
        fprintf(out, "    }%s\n", index == frame_count ? "" : ",");

        free(speaker);
        free(text);
    }

    fprintf(out, "  ]\n");
    fprintf(out, "}\n");

    free(line);
    fclose(script);
    fclose(out);
    return 0;
}

static int read_png_dimensions(const char *path, int *width, int *height) {
    static const unsigned char sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    unsigned char header[24];
    FILE *f = fopen(path, "rb");
    size_t nread;

    if (!f) {
        return 0;
    }
    nread = fread(header, 1, sizeof(header), f);
    fclose(f);

    if (nread != sizeof(header) || memcmp(header, sig, sizeof(sig)) != 0 ||
        memcmp(header + 12, "IHDR", 4) != 0) {
        return -1;
    }

    *width = ((int)header[16] << 24) | ((int)header[17] << 16) |
             ((int)header[18] << 8) | (int)header[19];
    *height = ((int)header[20] << 24) | ((int)header[21] << 16) |
              ((int)header[22] << 8) | (int)header[23];
    return 1;
}

static int print_status(const char *script_path, const char *frames_dir) {
    int frame_count = count_lines(script_path);
    int generated = 0;
    int first_missing = 0;
    bool in_range = false;
    int range_start = 0;

    printf("source_script=%s\n", script_path);
    printf("frames_dir=%s\n", frames_dir);
    printf("frame_count=%d\n", frame_count);

    for (int i = 1; i <= frame_count; i++) {
        char path[4096];
        bool exists;

        frame_path(path, sizeof(path), frames_dir, i);
        exists = file_exists(path);
        if (exists) {
            generated++;
            if (in_range) {
                printf("missing_range=%03d-%03d\n", range_start, i - 1);
                in_range = false;
            }
        } else {
            if (first_missing == 0) {
                first_missing = i;
            }
            if (!in_range) {
                range_start = i;
                in_range = true;
            }
        }
    }
    if (in_range) {
        printf("missing_range=%03d-%03d\n", range_start, frame_count);
    }

    printf("generated_count=%d\n", generated);
    printf("missing_count=%d\n", frame_count - generated);
    if (first_missing) {
        char path[4096];
        frame_path(path, sizeof(path), frames_dir, first_missing);
        printf("next_missing=%03d\n", first_missing);
        printf("next_missing_frame=%s\n", path);
    } else {
        printf("next_missing=complete\n");
    }
    return first_missing ? 1 : 0;
}

static int print_next_missing(const char *script_path, const char *frames_dir) {
    FILE *script = fopen(script_path, "r");
    char *line = NULL;
    size_t cap = 0;
    int index = 0;

    if (!script) {
        fprintf(stderr, "foc_prepare: cannot open %s: %s\n", script_path, strerror(errno));
        return 1;
    }

    while (getline(&line, &cap, script) != -1) {
        char path[4096];
        char *speaker;
        char *text;

        strip_newline(line);
        if (line[0] == '\0') {
            continue;
        }
        index++;
        frame_path(path, sizeof(path), frames_dir, index);
        if (file_exists(path)) {
            continue;
        }

        speaker = speaker_from_line(line);
        text = text_from_line(line);
        printf("frame_index=%03d\n", index);
        printf("line_index=%03d\n", index);
        printf("frame=%s\n", path);
        printf("speaker=%s\n", speaker);
        printf("text=%s\n", text);
        free(speaker);
        free(text);
        free(line);
        fclose(script);
        return 0;
    }

    printf("complete\n");
    free(line);
    fclose(script);
    return 0;
}

static int audit_png_dimensions(const char *frames_dir, int frame_count, int expected_width, int expected_height) {
    int present = 0;
    int missing = 0;
    int invalid = 0;
    int wrong = 0;

    for (int i = 1; i <= frame_count; i++) {
        char path[4096];
        int width = 0;
        int height = 0;
        int result;

        frame_path(path, sizeof(path), frames_dir, i);
        result = read_png_dimensions(path, &width, &height);
        if (result == 0) {
            printf("missing\t%03d\t%s\n", i, path);
            missing++;
            continue;
        }
        if (result < 0) {
            printf("invalid_png\t%03d\t%s\n", i, path);
            invalid++;
            continue;
        }

        present++;
        if (width != expected_width || height != expected_height) {
            printf("wrong_dimensions\t%03d\t%dx%d\t%s\n", i, width, height, path);
            wrong++;
        }
    }

    printf("present_count=%d\n", present);
    printf("missing_count=%d\n", missing);
    printf("invalid_png_count=%d\n", invalid);
    printf("wrong_dimension_count=%d\n", wrong);
    return (missing || invalid || wrong) ? 1 : 0;
}

static void ffconcat_string(FILE *out, const char *s) {
    fputc('\'', out);
    for (; *s; s++) {
        if (*s == '\'' || *s == '\\') {
            fputc('\\', out);
        }
        fputc(*s, out);
    }
    fputc('\'', out);
}

static int write_concat(const char *frames_dir, int frame_count, double duration, const char *out_path) {
    FILE *out = fopen(out_path, "w");

    if (!out) {
        fprintf(stderr, "foc_prepare: cannot write %s: %s\n", out_path, strerror(errno));
        return 1;
    }

    fputs("ffconcat version 1.0\n", out);
    for (int i = 1; i <= frame_count; i++) {
        char path[4096];
        frame_path(path, sizeof(path), frames_dir, i);
        fputs("file ", out);
        ffconcat_string(out, path);
        fputc('\n', out);
        fprintf(out, "duration %.6f\n", duration);
    }
    if (frame_count > 0) {
        char path[4096];
        frame_path(path, sizeof(path), frames_dir, frame_count);
        fputs("file ", out);
        ffconcat_string(out, path);
        fputc('\n', out);
    }

    fclose(out);
    return 0;
}

static void write_character_refs(FILE *out, const char *line, const char *speaker, const char *characters_dir) {
    bool first = true;
    char path[4096];

    fputc('[', out);
    if (speaker[0] != '\0') {
        character_path(path, sizeof(path), characters_dir, speaker);
        if (file_exists(path)) {
            json_string(out, path);
            first = false;
        }
    }

    for (size_t i = 0; i < sizeof(known_mentions) / sizeof(known_mentions[0]); i++) {
        if (strcmp(known_mentions[i], speaker) == 0 || !contains_name(line, known_mentions[i])) {
            continue;
        }
        character_path(path, sizeof(path), characters_dir, known_mentions[i]);
        if (!file_exists(path)) {
            continue;
        }
        if (!first) {
            fputs(", ", out);
        }
        json_string(out, path);
        first = false;
    }
    fputc(']', out);
}

static int write_prompt_plan(const char *script_path, const char *characters_dir,
                             const char *frames_dir, const char *out_path) {
    FILE *script = fopen(script_path, "r");
    FILE *out;
    char *line = NULL;
    size_t cap = 0;
    int index = 0;

    if (!script) {
        fprintf(stderr, "foc_prepare: cannot open %s: %s\n", script_path, strerror(errno));
        return 1;
    }
    if (!dir_exists(characters_dir)) {
        fprintf(stderr, "foc_prepare: cannot open character directory %s\n", characters_dir);
        fclose(script);
        return 1;
    }

    out = fopen(out_path, "w");
    if (!out) {
        fprintf(stderr, "foc_prepare: cannot write %s: %s\n", out_path, strerror(errno));
        fclose(script);
        return 1;
    }

    while (getline(&line, &cap, script) != -1) {
        char frame[4096];
        char prev[4096];
        char *speaker;
        char *text;

        strip_newline(line);
        if (line[0] == '\0') {
            continue;
        }

        index++;
        frame_path(frame, sizeof(frame), frames_dir, index);
        if (index > 1) {
            frame_path(prev, sizeof(prev), frames_dir, index - 1);
        } else {
            prev[0] = '\0';
        }
        speaker = speaker_from_line(line);
        text = text_from_line(line);

        fputs("{\"frame_index\":", out);
        fprintf(out, "%d", index);
        fputs(",\"line_index\":", out);
        fprintf(out, "%d", index);
        fputs(",\"frame\":", out);
        json_string(out, frame);
        fputs(",\"previous_frame\":", out);
        json_string(out, prev);
        fputs(",\"speaker\":", out);
        json_string(out, speaker);
        fputs(",\"text\":", out);
        json_string(out, text);
        fputs(",\"character_refs\":", out);
        write_character_refs(out, line, speaker, characters_dir);
        fputs("}\n", out);

        free(speaker);
        free(text);
    }

    free(line);
    fclose(script);
    fclose(out);
    return 0;
}

static int verify_characters(const char *characters_path, const char *characters_dir) {
    FILE *f = fopen(characters_path, "r");
    DIR *dir;
    NameList expected = {0};
    NameList actual = {0};
    char *line = NULL;
    size_t cap = 0;
    int missing = 0;
    int extra = 0;

    if (!f) {
        fprintf(stderr, "foc_prepare: cannot open %s: %s\n", characters_path, strerror(errno));
        return 1;
    }

    while (getline(&line, &cap, f) != -1) {
        char *comma;
        strip_newline(line);
        trim_inplace(line);
        if (line[0] == '\0') {
            continue;
        }
        comma = strchr(line, ',');
        if (comma) {
            *comma = '\0';
            trim_inplace(line);
        }
        namelist_add(&expected, line);
    }
    free(line);
    fclose(f);

    dir = opendir(characters_dir);
    if (!dir) {
        fprintf(stderr, "foc_prepare: cannot open %s: %s\n", characters_dir, strerror(errno));
        namelist_free(&expected);
        return 1;
    }

    for (;;) {
        struct dirent *ent = readdir(dir);
        char *name;
        size_t len;

        if (!ent) {
            break;
        }
        if (!ends_with(ent->d_name, ".png")) {
            continue;
        }
        len = strlen(ent->d_name) - 4;
        name = slice(ent->d_name, len);
        namelist_add(&actual, name);
        free(name);
    }
    closedir(dir);

    for (size_t i = 0; i < expected.count; i++) {
        char path[4096];
        character_path(path, sizeof(path), characters_dir, expected.items[i]);
        if (!file_exists(path)) {
            printf("missing\t%s.png\n", expected.items[i]);
            missing++;
        }
    }
    for (size_t i = 0; i < actual.count; i++) {
        if (!namelist_contains(&expected, actual.items[i])) {
            printf("extra\t%s.png\n", actual.items[i]);
            extra++;
        }
    }

    printf("expected_count=%zu\n", expected.count);
    printf("actual_count=%zu\n", actual.count);
    printf("missing_count=%d\n", missing);
    printf("extra_count=%d\n", extra);

    namelist_free(&expected);
    namelist_free(&actual);
    return (missing || extra) ? 1 : 0;
}

typedef struct {
    char *object_json;
    int segment_index;
    int source_line;
    double start_seconds;
    double end_seconds;
    double segment_duration_seconds;
    char *speaker;
    char *source_text;
    char *spoken_text;
    int additional_frame_count;
    int planned_frame_count;
    int expanded_frame_start_index;
    int expanded_frame_end_index;
} SegmentInfo;

typedef struct {
    SegmentInfo *items;
    size_t count;
    size_t cap;
    char *prefix_before_segments;
    char *suffix_from_segments_close;
} SegmentInfoList;

static char *read_text_file(const char *path) {
    FILE *f = fopen(path, "rb");
    long size;
    char *buf;

    if (!f) {
        fprintf(stderr, "foc_prepare: cannot open %s: %s\n", path, strerror(errno));
        exit(1);
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        die("cannot seek input file");
    }
    size = ftell(f);
    if (size < 0) {
        fclose(f);
        die("cannot measure input file");
    }
    rewind(f);
    buf = malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        die("out of memory");
    }
    if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
        free(buf);
        fclose(f);
        die("cannot read input file");
    }
    buf[size] = '\0';
    fclose(f);
    return buf;
}

static const char *skip_ws(const char *p) {
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    return p;
}

static char *join_path(const char *dir, const char *name) {
    size_t dlen = strlen(dir);
    size_t nlen = strlen(name);
    bool slash = dlen > 0 && dir[dlen - 1] == '/';
    char *out = malloc(dlen + (slash ? 0 : 1) + nlen + 1);

    if (!out) {
        die("out of memory");
    }
    memcpy(out, dir, dlen);
    if (!slash) {
        out[dlen++] = '/';
    }
    memcpy(out + dlen, name, nlen);
    out[dlen + nlen] = '\0';
    return out;
}

static char *keyframe_source_name(int source_line) {
    char name[128];
    snprintf(name, sizeof(name), "frame_%03d_source_line_%03d.png", source_line, source_line);
    return xstrdup(name);
}

static char *expanded_keyframe_name(int global_index, int source_line) {
    char name[160];
    snprintf(name, sizeof(name), "frame_%06d_source_line_%03d_part_01_keyframe.png",
             global_index, source_line);
    return xstrdup(name);
}

static char *expanded_additional_name(int global_index, int source_line, int part) {
    char name[160];
    snprintf(name, sizeof(name), "frame_%06d_source_line_%03d_part_%02d.png",
             global_index, source_line, part);
    return xstrdup(name);
}

static char *json_key_pattern(const char *key) {
    size_t len = strlen(key);
    char *pattern = malloc(len + 3);

    if (!pattern) {
        die("out of memory");
    }
    pattern[0] = '"';
    memcpy(pattern + 1, key, len);
    pattern[len + 1] = '"';
    pattern[len + 2] = '\0';
    return pattern;
}

static const char *json_field_value(const char *object, const char *key) {
    char *pattern = json_key_pattern(key);
    const char *hit = strstr(object, pattern);
    const char *colon;

    free(pattern);
    if (!hit) {
        return NULL;
    }
    colon = strchr(hit, ':');
    if (!colon) {
        return NULL;
    }
    return skip_ws(colon + 1);
}

static char *json_extract_string(const char *object, const char *key) {
    const char *p = json_field_value(object, key);
    char *out;
    size_t cap = 128;
    size_t len = 0;

    if (!p || *p != '"') {
        return xstrdup("");
    }
    p++;
    out = malloc(cap);
    if (!out) {
        die("out of memory");
    }

    while (*p && *p != '"') {
        char c = *p++;
        if (c == '\\') {
            c = *p++;
            switch (c) {
                case '"': c = '"'; break;
                case '\\': c = '\\'; break;
                case '/': c = '/'; break;
                case 'b': c = '\b'; break;
                case 'f': c = '\f'; break;
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                case 'u':
                    if (isxdigit((unsigned char)p[0]) && isxdigit((unsigned char)p[1]) &&
                        isxdigit((unsigned char)p[2]) && isxdigit((unsigned char)p[3])) {
                        p += 4;
                    }
                    c = '?';
                    break;
                default:
                    break;
            }
        }
        if (len + 2 > cap) {
            cap *= 2;
            out = xrealloc(out, cap);
        }
        out[len++] = c;
    }
    out[len] = '\0';
    return out;
}

static int json_extract_int(const char *object, const char *key, int fallback) {
    const char *p = json_field_value(object, key);
    char *end = NULL;
    long value;

    if (!p) {
        return fallback;
    }
    errno = 0;
    value = strtol(p, &end, 10);
    if (errno != 0 || end == p) {
        return fallback;
    }
    return (int)value;
}

static double json_extract_double(const char *object, const char *key, double fallback) {
    const char *p = json_field_value(object, key);
    char *end = NULL;
    double value;

    if (!p) {
        return fallback;
    }
    errno = 0;
    value = strtod(p, &end);
    if (errno != 0 || end == p) {
        return fallback;
    }
    return value;
}

static int word_count(const char *text) {
    int count = 0;
    bool in_word = false;

    for (; *text; text++) {
        if (isspace((unsigned char)*text)) {
            in_word = false;
        } else if (!in_word) {
            count++;
            in_word = true;
        }
    }
    return count;
}

static int ceil_div_duration(double duration, double divisor) {
    int whole = (int)(duration / divisor);
    double exact = (double)whole * divisor;

    if (duration > exact + 0.000001) {
        whole++;
    }
    return whole;
}

static int expanded_additional_count(double duration, const char *spoken_text) {
    int by_duration = ceil_div_duration(duration, 7.0);
    int words = word_count(spoken_text);
    int by_words = (words + 44) / 45;
    int value = by_duration > by_words ? by_duration : by_words;

    if (value < 6) {
        value = 6;
    }
    if (value > 20) {
        value = 20;
    }
    return value;
}

static void segment_info_free(SegmentInfo *segment) {
    free(segment->object_json);
    free(segment->speaker);
    free(segment->source_text);
    free(segment->spoken_text);
}

static void segment_list_add(SegmentInfoList *list, const char *start, size_t len, int fallback_index) {
    SegmentInfo *segment;

    if (list->count == list->cap) {
        list->cap = list->cap ? list->cap * 2 : 64;
        list->items = xrealloc(list->items, list->cap * sizeof(*list->items));
    }
    segment = &list->items[list->count++];
    memset(segment, 0, sizeof(*segment));
    segment->object_json = slice(start, len);
    segment->segment_index = json_extract_int(segment->object_json, "segment_index", fallback_index);
    segment->source_line = json_extract_int(segment->object_json, "source_line", segment->segment_index);
    segment->start_seconds = json_extract_double(segment->object_json, "start_seconds", 0.0);
    segment->end_seconds = json_extract_double(segment->object_json, "end_seconds", 0.0);
    segment->segment_duration_seconds =
        json_extract_double(segment->object_json, "segment_duration_seconds",
                            segment->end_seconds - segment->start_seconds);
    segment->speaker = json_extract_string(segment->object_json, "speaker");
    segment->source_text = json_extract_string(segment->object_json, "source_text");
    segment->spoken_text = json_extract_string(segment->object_json, "spoken_text");
    segment->additional_frame_count =
        expanded_additional_count(segment->segment_duration_seconds, segment->spoken_text);
    segment->planned_frame_count = segment->additional_frame_count + 1;
}

static void segment_list_free(SegmentInfoList *list) {
    for (size_t i = 0; i < list->count; i++) {
        segment_info_free(&list->items[i]);
    }
    free(list->items);
    free(list->prefix_before_segments);
    free(list->suffix_from_segments_close);
}

static int parse_segments_json(const char *json_path, SegmentInfoList *list) {
    char *json = read_text_file(json_path);
    char *segments_key = strstr(json, "\"segments\"");
    char *array_start;
    char *p;
    int depth = 0;
    bool in_string = false;
    bool escaped = false;
    int object_depth = 0;
    char *object_start = NULL;
    int index = 0;

    if (!segments_key) {
        fprintf(stderr, "foc_prepare: no segments array in %s\n", json_path);
        free(json);
        return 1;
    }
    array_start = strchr(segments_key, '[');
    if (!array_start) {
        fprintf(stderr, "foc_prepare: malformed segments array in %s\n", json_path);
        free(json);
        return 1;
    }

    list->prefix_before_segments = slice(json, (size_t)(segments_key - json));
    p = array_start + 1;

    for (; *p; p++) {
        char c = *p;
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                in_string = false;
            }
            continue;
        }

        if (c == '"') {
            in_string = true;
            continue;
        }
        if (c == '{') {
            if (object_depth == 0) {
                object_start = p;
            }
            object_depth++;
            continue;
        }
        if (c == '}') {
            object_depth--;
            if (object_depth == 0 && object_start) {
                index++;
                segment_list_add(list, object_start, (size_t)(p - object_start + 1), index);
                object_start = NULL;
            }
            continue;
        }
        if (c == '[') {
            depth++;
            continue;
        }
        if (c == ']') {
            if (depth == 0 && object_depth == 0) {
                list->suffix_from_segments_close = xstrdup(p);
                free(json);
                return 0;
            }
            depth--;
        }
    }

    fprintf(stderr, "foc_prepare: unterminated segments array in %s\n", json_path);
    free(json);
    return 1;
}

static void compute_expanded_indices(SegmentInfoList *segments) {
    int global = 0;

    for (size_t i = 0; i < segments->count; i++) {
        SegmentInfo *segment = &segments->items[i];
        segment->expanded_frame_start_index = global + 1;
        global += segment->planned_frame_count;
        segment->expanded_frame_end_index = global;
    }
}

static int sum_additional_frames(const SegmentInfoList *segments) {
    int sum = 0;

    for (size_t i = 0; i < segments->count; i++) {
        sum += segments->items[i].additional_frame_count;
    }
    return sum;
}

static int sum_planned_frames(const SegmentInfoList *segments) {
    int sum = 0;

    for (size_t i = 0; i < segments->count; i++) {
        sum += segments->items[i].planned_frame_count;
    }
    return sum;
}

static void read_character_png_names(const char *characters_dir, NameList *names) {
    DIR *dir = opendir(characters_dir);

    if (!dir) {
        return;
    }
    for (;;) {
        struct dirent *ent = readdir(dir);
        char *name;
        size_t len;

        if (!ent) {
            break;
        }
        if (!ends_with(ent->d_name, ".png")) {
            continue;
        }
        len = strlen(ent->d_name) - 4;
        name = slice(ent->d_name, len);
        namelist_add(names, name);
        free(name);
    }
    closedir(dir);
}

static bool segment_mentions_character(const SegmentInfo *segment, const char *name) {
    return strcmp(segment->speaker, name) == 0 ||
           contains_name(segment->source_text, name) ||
           contains_name(segment->spoken_text, name);
}

static void write_relative_path(FILE *out, const char *dir, const char *name) {
    char *path = join_path(dir, name);
    json_string(out, path);
    free(path);
}

static void write_character_objects(FILE *out, const SegmentInfo *segment,
                                    const NameList *character_names,
                                    const char *characters_dir) {
    bool first = true;

    fputs("[", out);
    for (size_t i = 0; i < character_names->count; i++) {
        const char *name = character_names->items[i];
        char *png_name;
        char *path;

        if (!segment_mentions_character(segment, name)) {
            continue;
        }
        png_name = malloc(strlen(name) + 5);
        if (!png_name) {
            die("out of memory");
        }
        sprintf(png_name, "%s.png", name);
        path = join_path(characters_dir, png_name);
        free(png_name);
        if (!file_exists(path)) {
            free(path);
            continue;
        }
        if (!first) {
            fputs(", ", out);
        }
        fputs("{\"name\": ", out);
        json_string(out, name);
        fputs(", \"image\": ", out);
        json_string(out, path);
        fputs("}", out);
        first = false;
        free(path);
    }
    fputs("]", out);
}

static void write_render_frames(FILE *out, const SegmentInfo *segment,
                                const char *keyframes_dir, const char *expanded_dir) {
    double step = segment->segment_duration_seconds / (double)segment->planned_frame_count;

    fputs("[\n", out);
    for (int part = 1; part <= segment->planned_frame_count; part++) {
        int global = segment->expanded_frame_start_index + part - 1;
        double frame_start = segment->start_seconds + step * (double)(part - 1);
        double frame_end = segment->start_seconds + step * (double)part;

        fputs("        {\n", out);
        fprintf(out, "          \"global_frame_index\": %d,\n", global);
        fprintf(out, "          \"source_line\": %d,\n", segment->source_line);
        fprintf(out, "          \"segment_frame_index\": %d,\n", part);
        if (part == 1) {
            char *original = keyframe_source_name(segment->source_line);
            char *expanded = expanded_keyframe_name(global, segment->source_line);
            fputs("          \"frame_role\": \"existing_keyframe\",\n", out);
            fputs("          \"original_frame_filename\": ", out);
            write_relative_path(out, keyframes_dir, original);
            fputs(",\n          \"frame_filename\": ", out);
            write_relative_path(out, expanded_dir, expanded);
            fputs(",\n          \"status\": \"generated\",\n", out);
            free(original);
            free(expanded);
        } else {
            char *expanded = expanded_additional_name(global, segment->source_line, part);
            fputs("          \"frame_role\": \"additional_render\",\n", out);
            fputs("          \"frame_filename\": ", out);
            write_relative_path(out, expanded_dir, expanded);
            fputs(",\n          \"status\": \"pending\",\n", out);
            free(expanded);
        }
        fprintf(out, "          \"start_seconds\": %.6f,\n", frame_start);
        fprintf(out, "          \"end_seconds\": %.6f,\n", frame_end);
        fprintf(out, "          \"duration_seconds\": %.6f,\n", step);
        fprintf(out, "          \"position_in_segment\": %.6f\n",
                segment->planned_frame_count == 1 ? 0.0 :
                (double)(part - 1) / (double)(segment->planned_frame_count - 1));
        fprintf(out, "        }%s\n", part == segment->planned_frame_count ? "" : ",");
    }
    fputs("      ]", out);
}

static void write_expanded_references(FILE *out, const SegmentInfoList *segments,
                                      size_t index, const NameList *character_names,
                                      const char *keyframes_dir,
                                      const char *characters_dir,
                                      const char *expanded_dir) {
    const SegmentInfo *segment = &segments->items[index];
    char *current_original = keyframe_source_name(segment->source_line);
    char *current_expanded = expanded_keyframe_name(segment->expanded_frame_start_index,
                                                   segment->source_line);

    fputs("{\n", out);
    fputs("        \"previous_keyframe\": ", out);
    if (index == 0) {
        fputs("null", out);
    } else {
        const SegmentInfo *prev = &segments->items[index - 1];
        char *name = expanded_keyframe_name(prev->expanded_frame_start_index, prev->source_line);
        write_relative_path(out, expanded_dir, name);
        free(name);
    }
    fputs(",\n        \"current_keyframe\": ", out);
    write_relative_path(out, expanded_dir, current_expanded);
    fputs(",\n        \"next_keyframe\": ", out);
    if (index + 1 >= segments->count) {
        fputs("null", out);
    } else {
        const SegmentInfo *next = &segments->items[index + 1];
        char *name = expanded_keyframe_name(next->expanded_frame_start_index, next->source_line);
        write_relative_path(out, expanded_dir, name);
        free(name);
    }
    fputs(",\n        \"original_previous_keyframe\": ", out);
    if (index == 0) {
        fputs("null", out);
    } else {
        char *name = keyframe_source_name(segments->items[index - 1].source_line);
        write_relative_path(out, keyframes_dir, name);
        free(name);
    }
    fputs(",\n        \"original_current_keyframe\": ", out);
    write_relative_path(out, keyframes_dir, current_original);
    fputs(",\n        \"original_next_keyframe\": ", out);
    if (index + 1 >= segments->count) {
        fputs("null", out);
    } else {
        char *name = keyframe_source_name(segments->items[index + 1].source_line);
        write_relative_path(out, keyframes_dir, name);
        free(name);
    }
    fputs(",\n        \"characters\": ", out);
    write_character_objects(out, segment, character_names, characters_dir);
    fputs("\n      }", out);

    free(current_original);
    free(current_expanded);
}

static int write_expanded_json(const char *input_json, const char *keyframes_dir,
                               const char *characters_dir, const char *expanded_dir,
                               const char *out_json) {
    SegmentInfoList segments = {0};
    NameList character_names = {0};
    FILE *out;
    int additional_total;
    int planned_total;

    if (parse_segments_json(input_json, &segments) != 0) {
        segment_list_free(&segments);
        return 1;
    }
    compute_expanded_indices(&segments);
    additional_total = sum_additional_frames(&segments);
    planned_total = sum_planned_frames(&segments);
    read_character_png_names(characters_dir, &character_names);

    out = fopen(out_json, "w");
    if (!out) {
        fprintf(stderr, "foc_prepare: cannot write %s: %s\n", out_json, strerror(errno));
        namelist_free(&character_names);
        segment_list_free(&segments);
        return 1;
    }

    fputs(segments.prefix_before_segments, out);
    fprintf(out, "\"expanded_frame_count\": %d,\n", planned_total);
    fprintf(out, "  \"additional_frame_count\": %d,\n", additional_total);
    fputs("  \"render_expansion\": {\n", out);
    fputs("    \"version\": 1,\n", out);
    fputs("    \"purpose\": \"Plan 6 to 20 additional generated PNG frames for each foc_script.txt line while preserving the original audio segment timing.\",\n", out);
    fputs("    \"frame_size\": {\"width\": 1920, \"height\": 1080},\n", out);
    fprintf(out, "    \"source_segment_count\": %zu,\n", segments.count);
    fprintf(out, "    \"existing_keyframe_count\": %zu,\n", segments.count);
    fputs("    \"min_additional_frames_per_segment\": 6,\n", out);
    fputs("    \"max_additional_frames_per_segment\": 20,\n", out);
    fprintf(out, "    \"additional_frame_count\": %d,\n", additional_total);
    fprintf(out, "    \"planned_frame_count_including_keyframes\": %d,\n", planned_total);
    fprintf(out, "    \"original_keyframes_dir\": ");
    json_string(out, keyframes_dir);
    fputs(",\n    \"expanded_frames_dir\": ", out);
    json_string(out, expanded_dir);
    fputs(",\n    \"renumbered_keyframe_count\": ", out);
    fprintf(out, "%zu,\n", segments.count);
    fputs("    \"renumbered_keyframe_filename_pattern\": \"foc_script expanded PNGs/frame_%06d_source_line_%03d_part_01_keyframe.png\",\n", out);
    fputs("    \"additional_frame_filename_pattern\": \"foc_script expanded PNGs/frame_%06d_source_line_%03d_part_%02d.png\",\n", out);
    fputs("    \"count_selection\": \"additional_frame_count = clamp(max(ceil(segment_duration_seconds / 7), ceil(word_count / 45)), 6, 20)\",\n", out);
    fputs("    \"timing\": \"Each segment duration is divided evenly across the existing keyframe plus its additional planned frames.\",\n", out);
    fputs("    \"renumbering_note\": \"Existing keyframe PNGs should be copied into the expanded PNG folder at their global planned frame indices. The six-digit frame number is the new timeline location; source_line preserves the original script/keyframe number; missing numbered PNGs are pending additional renders.\",\n", out);
    fputs("    \"continuity_guidance\": \"Use previous/current/next keyframes and listed character PNGs as references; keep camera, props, and characters coherent within each source line and across adjacent lines.\"\n", out);
    fputs("  },\n  \"segments\": [\n", out);

    for (size_t i = 0; i < segments.count; i++) {
        SegmentInfo *segment = &segments.items[i];
        char *original_name = keyframe_source_name(segment->source_line);
        char *expanded_name = expanded_keyframe_name(segment->expanded_frame_start_index,
                                                     segment->source_line);
        char *close = strrchr(segment->object_json, '}');
        size_t prefix_len = close ? (size_t)(close - segment->object_json) : strlen(segment->object_json);

        fwrite(segment->object_json, 1, prefix_len, out);
        fputs(",\n      \"original_keyframe_filename\": ", out);
        write_relative_path(out, keyframes_dir, original_name);
        fputs(",\n      \"keyframe_filename\": ", out);
        write_relative_path(out, expanded_dir, expanded_name);
        fputs(",\n      \"keyframe_status\": \"generated\",\n", out);
        fprintf(out, "      \"additional_frame_count\": %d,\n", segment->additional_frame_count);
        fprintf(out, "      \"planned_frame_count\": %d,\n", segment->planned_frame_count);
        fprintf(out, "      \"expanded_frame_start_index\": %d,\n", segment->expanded_frame_start_index);
        fprintf(out, "      \"expanded_frame_end_index\": %d,\n", segment->expanded_frame_end_index);
        fputs("      \"render_references\": ", out);
        write_expanded_references(out, &segments, i, &character_names,
                                  keyframes_dir, characters_dir, expanded_dir);
        fputs(",\n      \"render_frames\": ", out);
        write_render_frames(out, segment, keyframes_dir, expanded_dir);
        fprintf(out, "\n    }%s\n", i + 1 == segments.count ? "" : ",");

        free(original_name);
        free(expanded_name);
    }

    fputs(segments.suffix_from_segments_close, out);
    fclose(out);

    printf("segments=%zu\n", segments.count);
    printf("additional_frame_count=%d\n", additional_total);
    printf("planned_frame_count_including_keyframes=%d\n", planned_total);
    printf("expanded_json=%s\n", out_json);

    namelist_free(&character_names);
    segment_list_free(&segments);
    return 0;
}

static int ensure_directory(const char *path) {
    if (dir_exists(path)) {
        return 0;
    }
    if (mkdir(path, 0755) == 0) {
        return 0;
    }
    if (errno == EEXIST && dir_exists(path)) {
        return 0;
    }
    fprintf(stderr, "foc_prepare: cannot create directory %s: %s\n", path, strerror(errno));
    return 1;
}

static int copy_file_binary(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    FILE *out;
    unsigned char buf[65536];

    if (!in) {
        fprintf(stderr, "foc_prepare: cannot open %s: %s\n", src, strerror(errno));
        return 1;
    }
    out = fopen(dst, "wb");
    if (!out) {
        fprintf(stderr, "foc_prepare: cannot write %s: %s\n", dst, strerror(errno));
        fclose(in);
        return 1;
    }
    for (;;) {
        size_t nread = fread(buf, 1, sizeof(buf), in);
        if (nread > 0 && fwrite(buf, 1, nread, out) != nread) {
            fprintf(stderr, "foc_prepare: write failed for %s\n", dst);
            fclose(in);
            fclose(out);
            return 1;
        }
        if (nread < sizeof(buf)) {
            if (ferror(in)) {
                fprintf(stderr, "foc_prepare: read failed for %s\n", src);
                fclose(in);
                fclose(out);
                return 1;
            }
            break;
        }
    }
    if (fclose(in) != 0 || fclose(out) != 0) {
        fprintf(stderr, "foc_prepare: close failed while copying %s\n", src);
        return 1;
    }
    return 0;
}

static int renumber_keyframes(const char *input_json, const char *keyframes_dir,
                              const char *expanded_dir) {
    SegmentInfoList segments = {0};
    int copied = 0;
    int missing = 0;

    if (parse_segments_json(input_json, &segments) != 0) {
        segment_list_free(&segments);
        return 1;
    }
    compute_expanded_indices(&segments);
    if (ensure_directory(expanded_dir) != 0) {
        segment_list_free(&segments);
        return 1;
    }

    for (size_t i = 0; i < segments.count; i++) {
        SegmentInfo *segment = &segments.items[i];
        char *src_name = keyframe_source_name(segment->source_line);
        char *dst_name = expanded_keyframe_name(segment->expanded_frame_start_index,
                                                segment->source_line);
        char *src = join_path(keyframes_dir, src_name);
        char *dst = join_path(expanded_dir, dst_name);

        if (!file_exists(src)) {
            printf("missing_keyframe\t%d\t%s\n", segment->source_line, src);
            missing++;
        } else if (copy_file_binary(src, dst) == 0) {
            copied++;
        } else {
            missing++;
        }

        free(src_name);
        free(dst_name);
        free(src);
        free(dst);
    }

    printf("segments=%zu\n", segments.count);
    printf("copied_keyframes=%d\n", copied);
    printf("missing_keyframes=%d\n", missing);
    printf("expanded_dir=%s\n", expanded_dir);

    segment_list_free(&segments);
    return missing ? 1 : 0;
}

static void usage(FILE *out) {
    fputs("usage:\n", out);
    fputs("  foc_prepare characters <foc_script.txt> <foc_characters.txt>\n", out);
    fputs("  foc_prepare manifest <foc_script.txt> <frames_dir> <frame_manifest.json>\n", out);
    fputs("  foc_prepare status <foc_script.txt> <frames_dir>\n", out);
    fputs("  foc_prepare next <foc_script.txt> <frames_dir>\n", out);
    fputs("  foc_prepare audit-png <frames_dir> <frame_count> <width> <height>\n", out);
    fputs("  foc_prepare concat <frames_dir> <frame_count> <duration_seconds> <out.ffconcat>\n", out);
    fputs("  foc_prepare prompt-plan <foc_script.txt> <foc_characters_dir> <frames_dir> <out.jsonl>\n", out);
    fputs("  foc_prepare verify-characters <foc_characters.txt> <foc_characters_dir>\n", out);
    fputs("  foc_prepare expand-json <base_foc_script.json> <keyframes_dir> <foc_characters_dir> <expanded_frames_dir> <out_foc_script.json>\n", out);
    fputs("  foc_prepare renumber-keyframes <base_foc_script.json> <keyframes_dir> <expanded_frames_dir>\n", out);
}

int main(int argc, char **argv) {
    if (argc < 2 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        usage(stdout);
        return argc < 2 ? 1 : 0;
    }

    if (strcmp(argv[1], "characters") == 0) {
        if (argc != 4) {
            usage(stderr);
            return 1;
        }
        return write_characters(argv[2], argv[3]);
    }

    if (strcmp(argv[1], "manifest") == 0) {
        if (argc != 5) {
            usage(stderr);
            return 1;
        }
        return write_manifest(argv[2], argv[3], argv[4]);
    }

    if (strcmp(argv[1], "status") == 0) {
        if (argc != 4) {
            usage(stderr);
            return 1;
        }
        return print_status(argv[2], argv[3]);
    }

    if (strcmp(argv[1], "next") == 0) {
        if (argc != 4) {
            usage(stderr);
            return 1;
        }
        return print_next_missing(argv[2], argv[3]);
    }

    if (strcmp(argv[1], "audit-png") == 0) {
        if (argc != 6) {
            usage(stderr);
            return 1;
        }
        return audit_png_dimensions(argv[2], parse_positive_int(argv[3], "frame_count"),
                                    parse_positive_int(argv[4], "width"),
                                    parse_positive_int(argv[5], "height"));
    }

    if (strcmp(argv[1], "concat") == 0) {
        if (argc != 6) {
            usage(stderr);
            return 1;
        }
        return write_concat(argv[2], parse_positive_int(argv[3], "frame_count"),
                            parse_positive_double(argv[4], "duration_seconds"), argv[5]);
    }

    if (strcmp(argv[1], "prompt-plan") == 0) {
        if (argc != 6) {
            usage(stderr);
            return 1;
        }
        return write_prompt_plan(argv[2], argv[3], argv[4], argv[5]);
    }

    if (strcmp(argv[1], "verify-characters") == 0) {
        if (argc != 4) {
            usage(stderr);
            return 1;
        }
        return verify_characters(argv[2], argv[3]);
    }

    if (strcmp(argv[1], "expand-json") == 0) {
        if (argc != 7) {
            usage(stderr);
            return 1;
        }
        return write_expanded_json(argv[2], argv[3], argv[4], argv[5], argv[6]);
    }

    if (strcmp(argv[1], "renumber-keyframes") == 0) {
        if (argc != 5) {
            usage(stderr);
            return 1;
        }
        return renumber_keyframes(argv[2], argv[3], argv[4]);
    }

    usage(stderr);
    return 1;
}
