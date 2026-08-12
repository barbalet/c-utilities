#include <ctype.h>
#include <errno.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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

    usage(stderr);
    return 1;
}
