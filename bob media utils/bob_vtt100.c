#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "../common/cutil.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct {
    double start;
    double end;
    char *text;
} Cue;

typedef struct {
    double start;
    double end;
    char *text;
} Word;

typedef struct {
    Cue *items;
    size_t count;
    size_t cap;
} CueList;

typedef struct {
    Word *items;
    size_t count;
    size_t cap;
} WordList;

static void cue_add(CueList *list, double start, double end, char *text) {
    if (list->count == list->cap) {
        list->cap = list->cap ? list->cap * 2 : 256;
        list->items = cu_xrealloc(list->items, list->cap * sizeof(*list->items));
    }
    list->items[list->count].start = start;
    list->items[list->count].end = end;
    list->items[list->count].text = text;
    list->count++;
}

static void word_add(WordList *list, double start, double end, const char *text, size_t len) {
    if (len == 0) {
        return;
    }
    if (list->count > 0 && strcmp(list->items[list->count - 1].text, text) == 0) {
        return;
    }
    if (list->count == list->cap) {
        list->cap = list->cap ? list->cap * 2 : 1024;
        list->items = cu_xrealloc(list->items, list->cap * sizeof(*list->items));
    }
    list->items[list->count].start = start;
    list->items[list->count].end = end;
    list->items[list->count].text = cu_xstrndup(text, len);
    list->count++;
}

static void trim_right(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r' || isspace((unsigned char)s[len - 1]))) {
        s[--len] = '\0';
    }
}

static double parse_time(const char *s) {
    int a = 0, b = 0, c = 0, ms = 0;

    if (sscanf(s, "%d:%d:%d.%d", &a, &b, &c, &ms) == 4) {
        return a * 3600.0 + b * 60.0 + c + ms / 1000.0;
    }
    if (sscanf(s, "%d:%d.%d", &b, &c, &ms) == 3) {
        return b * 60.0 + c + ms / 1000.0;
    }
    return -1.0;
}

static bool parse_timing_line(const char *line, double *start, double *end) {
    const char *arrow = strstr(line, "-->");
    char left[64];
    char right[64];
    size_t llen;
    const char *rstart;
    size_t rlen = 0;

    if (!arrow) {
        return false;
    }
    llen = (size_t)(arrow - line);
    while (llen > 0 && isspace((unsigned char)line[llen - 1])) {
        llen--;
    }
    if (llen >= sizeof(left)) {
        return false;
    }
    memcpy(left, line, llen);
    left[llen] = '\0';

    rstart = arrow + 3;
    while (*rstart && isspace((unsigned char)*rstart)) {
        rstart++;
    }
    while (rstart[rlen] && !isspace((unsigned char)rstart[rlen])) {
        rlen++;
    }
    if (rlen == 0 || rlen >= sizeof(right)) {
        return false;
    }
    memcpy(right, rstart, rlen);
    right[rlen] = '\0';

    *start = parse_time(left);
    *end = parse_time(right);
    return *start >= 0.0 && *end >= *start;
}

static void append_text(char **buf, size_t *len, size_t *cap, const char *text) {
    size_t add = strlen(text);
    if (add == 0) {
        return;
    }
    if (*len > 0 && (*buf)[*len - 1] != '\n') {
        if (*len + 2 > *cap) {
            *cap = (*cap ? *cap * 2 : 256);
            *buf = cu_xrealloc(*buf, *cap);
        }
        (*buf)[(*len)++] = '\n';
        (*buf)[*len] = '\0';
    }
    while (*len + add + 1 > *cap) {
        *cap = (*cap ? *cap * 2 : 256);
        *buf = cu_xrealloc(*buf, *cap);
    }
    memcpy(*buf + *len, text, add + 1);
    *len += add;
}

static char *clean_vtt_text(const char *raw) {
    char *out = cu_xmalloc(strlen(raw) + 1);
    bool in_tag = false;
    bool last_space = true;
    char *w = out;

    for (const char *p = raw; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '<') {
            in_tag = true;
            continue;
        }
        if (c == '>') {
            in_tag = false;
            continue;
        }
        if (in_tag) {
            continue;
        }
        if (c == '&') {
            if (strncmp(p, "&amp;", 5) == 0) {
                *w++ = '&';
                p += 4;
                last_space = false;
                continue;
            }
            if (strncmp(p, "&quot;", 6) == 0) {
                *w++ = '"';
                p += 5;
                last_space = false;
                continue;
            }
            if (strncmp(p, "&#39;", 5) == 0 || strncmp(p, "&apos;", 6) == 0) {
                *w++ = '\'';
                p += (p[1] == '#') ? 4 : 5;
                last_space = false;
                continue;
            }
        }
        if (isspace(c)) {
            if (!last_space) {
                *w++ = ' ';
                last_space = true;
            }
        } else {
            *w++ = (char)c;
            last_space = false;
        }
    }
    if (w > out && w[-1] == ' ') {
        w--;
    }
    *w = '\0';
    return out;
}

static CueList parse_vtt(const char *path) {
    char *data = cu_read_text_file(path);
    char *cursor = data;
    char *line = strsep(&cursor, "\n");
    CueList cues = {0};
    double cue_start = -1.0;
    double cue_end = -1.0;
    char *text = NULL;
    size_t text_len = 0;
    size_t text_cap = 0;

    while (line) {
        trim_right(line);
        if (parse_timing_line(line, &cue_start, &cue_end)) {
            free(text);
            text = NULL;
            text_len = text_cap = 0;
            line = strsep(&cursor, "\n");
            while (line) {
                trim_right(line);
                if (line[0] == '\0' && text_len == 0) {
                    line = strsep(&cursor, "\n");
                    continue;
                }
                if (line[0] == '\0') {
                    break;
                }
                append_text(&text, &text_len, &text_cap, line);
                line = strsep(&cursor, "\n");
            }
            if (text && text[0]) {
                char *raw = cu_xstrndup(text, strlen(text));
                cu_trim_inplace(raw);
                if (raw[0]) {
                    cue_add(&cues, cue_start, cue_end, raw);
                } else {
                    free(raw);
                }
            }
            free(text);
            text = NULL;
            text_len = text_cap = 0;
        }
        line = strsep(&cursor, "\n");
    }
    free(data);
    return cues;
}

static bool word_char(unsigned char c) {
    return isalnum(c) || c == '\'' || c == '-';
}

static bool is_vtt_time_tag(const char *p) {
    return p[0] == '<' &&
           isdigit((unsigned char)p[1]) &&
           isdigit((unsigned char)p[2]) &&
           p[3] == ':';
}

static size_t count_words(const char *text) {
    size_t count = 0;
    bool in_word = false;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if (word_char(*p)) {
            if (!in_word) {
                count++;
                in_word = true;
            }
        } else {
            in_word = false;
        }
    }
    return count;
}

static void cues_to_words(const CueList *cues, WordList *words) {
    for (size_t i = 0; i < cues->count; i++) {
        const Cue *cue = &cues->items[i];
        const char *first_tag = strstr(cue->text, "<00:");

        if (first_tag || strstr(cue->text, "<0:")) {
            const char *cursor = cue->text;

            if (first_tag && first_tag != cue->text) {
                const char *prefix_start = cue->text;
                for (const char *scan = cue->text; scan < first_tag; scan++) {
                    if (*scan == '\n') {
                        prefix_start = scan + 1;
                    }
                }
                char *prefix = cu_xstrndup(prefix_start, (size_t)(first_tag - prefix_start));
                char *clean = clean_vtt_text(prefix);
                size_t nwords = count_words(clean);
                const char *p = clean;
                size_t word_index = 0;
                double first_time = cue->start;
                char time_buf[64];
                const char *tag_end = strchr(first_tag, '>');
                if (tag_end && (size_t)(tag_end - first_tag - 1) < sizeof(time_buf)) {
                    memcpy(time_buf, first_tag + 1, (size_t)(tag_end - first_tag - 1));
                    time_buf[tag_end - first_tag - 1] = '\0';
                    first_time = parse_time(time_buf);
                    if (first_time < cue->start) {
                        first_time = cue->start;
                    }
                }
                while (*p) {
                    while (*p && !word_char((unsigned char)*p)) {
                        p++;
                    }
                    if (!*p) {
                        break;
                    }
                    const char *start = p;
                    while (*p && word_char((unsigned char)*p)) {
                        p++;
                    }
                    double a = cue->start + (first_time - cue->start) * ((double)word_index / (double)nwords);
                    double b = cue->start + (first_time - cue->start) * ((double)(word_index + 1) / (double)nwords);
                    word_add(words, a, b, start, (size_t)(p - start));
                    word_index++;
                }
                free(prefix);
                free(clean);
            }

            while ((cursor = strchr(cursor, '<')) != NULL) {
                if (!is_vtt_time_tag(cursor)) {
                    cursor++;
                    continue;
                }
                const char *tag_end = strchr(cursor, '>');
                char time_buf[64];
                double start_time;
                const char *chunk_start;
                const char *next_tag;
                const char *chunk_end;
                double end_time;
                char *chunk;
                char *clean;
                size_t nwords;
                size_t word_index = 0;
                const char *p;

                if (!tag_end || (size_t)(tag_end - cursor - 1) >= sizeof(time_buf)) {
                    break;
                }
                memcpy(time_buf, cursor + 1, (size_t)(tag_end - cursor - 1));
                time_buf[tag_end - cursor - 1] = '\0';
                start_time = parse_time(time_buf);
                if (start_time < 0.0) {
                    cursor = tag_end + 1;
                    continue;
                }

                chunk_start = tag_end + 1;
                next_tag = chunk_start;
                while ((next_tag = strchr(next_tag, '<')) != NULL && !is_vtt_time_tag(next_tag)) {
                    next_tag++;
                }
                chunk_end = next_tag ? next_tag : cue->text + strlen(cue->text);
                end_time = cue->end;
                if (next_tag) {
                    const char *next_end = strchr(next_tag, '>');
                    if (next_end && (size_t)(next_end - next_tag - 1) < sizeof(time_buf)) {
                        memcpy(time_buf, next_tag + 1, (size_t)(next_end - next_tag - 1));
                        time_buf[next_end - next_tag - 1] = '\0';
                        double parsed = parse_time(time_buf);
                        if (parsed > start_time) {
                            end_time = parsed;
                        }
                    }
                }

                chunk = cu_xstrndup(chunk_start, (size_t)(chunk_end - chunk_start));
                clean = clean_vtt_text(chunk);
                cu_trim_inplace(clean);
                nwords = count_words(clean);
                p = clean;
                while (*p && nwords > 0) {
                    while (*p && !word_char((unsigned char)*p)) {
                        p++;
                    }
                    if (!*p) {
                        break;
                    }
                    const char *start = p;
                    while (*p && word_char((unsigned char)*p)) {
                        p++;
                    }
                    double a = start_time + (end_time - start_time) * ((double)word_index / (double)nwords);
                    double b = start_time + (end_time - start_time) * ((double)(word_index + 1) / (double)nwords);
                    word_add(words, a, b, start, (size_t)(p - start));
                    word_index++;
                }
                free(chunk);
                free(clean);
                cursor = next_tag ? next_tag : chunk_end;
            }
            continue;
        }

        if ((cue->end - cue->start) <= 0.05) {
            continue;
        }

        char *clean = clean_vtt_text(cue->text);
        size_t nwords = count_words(clean);
        size_t word_index = 0;
        const char *p = clean;

        while (*p) {
            while (*p && !word_char((unsigned char)*p)) {
                p++;
            }
            if (!*p) {
                break;
            }
            const char *start = p;
            while (*p && word_char((unsigned char)*p)) {
                p++;
            }
            double a = cue->start + (cue->end - cue->start) * ((double)word_index / (double)nwords);
            double b = cue->start + (cue->end - cue->start) * ((double)(word_index + 1) / (double)nwords);
            word_add(words, a, b, start, (size_t)(p - start));
            word_index++;
        }
        free(clean);
    }
}

static void slugify(const char *text, char *out, size_t out_size) {
    size_t w = 0;
    bool dash = false;

    for (const unsigned char *p = (const unsigned char *)text; *p && w + 1 < out_size; p++) {
        if (isalnum(*p)) {
            out[w++] = (char)tolower(*p);
            dash = false;
        } else if (!dash && w > 0) {
            out[w++] = '-';
            dash = true;
        }
        if (w >= 72) {
            break;
        }
    }
    while (w > 0 && out[w - 1] == '-') {
        w--;
    }
    if (w == 0 && out_size > 1) {
        out[w++] = 'x';
    }
    out[w] = '\0';
}

static void srt_time(FILE *out, double seconds) {
    long millis = (long)(seconds * 1000.0 + 0.5);
    long h = millis / 3600000;
    millis %= 3600000;
    long m = millis / 60000;
    millis %= 60000;
    long s = millis / 1000;
    millis %= 1000;
    fprintf(out, "%02ld:%02ld:%02ld,%03ld", h, m, s, millis);
}

static void write_word_jsonl(const char *path, const WordList *words) {
    FILE *out = fopen(path, "wb");
    if (!out) {
        cu_die_errno("open words jsonl failed");
    }
    for (size_t i = 0; i < words->count; i++) {
        fprintf(out, "{\"word_index\":%zu,\"start_seconds\":%.6f,\"end_seconds\":%.6f,\"word\":\"",
                i + 1, words->items[i].start, words->items[i].end);
        cu_json_escape_write(out, words->items[i].text);
        fputs("\"}\n", out);
    }
    fclose(out);
}

static void write_plain_text(const char *path, const WordList *words) {
    FILE *out = fopen(path, "wb");
    if (!out) {
        cu_die_errno("open text failed");
    }
    for (size_t i = 0; i < words->count; i++) {
        if (i) {
            fputc(' ', out);
        }
        fputs(words->items[i].text, out);
    }
    fputc('\n', out);
    fclose(out);
}

static void write_groups(const char *jsonl_path, const char *srt_path, const char *prompts_dir, const WordList *words) {
    FILE *jsonl = fopen(jsonl_path, "wb");
    FILE *srt = fopen(srt_path, "wb");
    size_t segment = 0;

    if (!jsonl || !srt) {
        cu_die_errno("open group outputs failed");
    }
    cu_mkdir_p(prompts_dir);
    DIR *dir = opendir(prompts_dir);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            size_t len = strlen(entry->d_name);
            if (len > 4 && strcmp(entry->d_name + len - 4, ".txt") == 0) {
                char path[PATH_MAX];
                snprintf(path, sizeof(path), "%s/%s", prompts_dir, entry->d_name);
                unlink(path);
            }
        }
        closedir(dir);
    }

    for (size_t i = 0; i < words->count; i += 100) {
        size_t end = i + 100;
        char text[8192];
        char slug[96];
        char image[160];
        char prompt_path[PATH_MAX];
        size_t pos = 0;

        if (end > words->count) {
            end = words->count;
        }
        segment++;
        for (size_t j = i; j < end; j++) {
            int written = snprintf(text + pos, sizeof(text) - pos, "%s%s", j == i ? "" : " ", words->items[j].text);
            if (written < 0 || (size_t)written >= sizeof(text) - pos) {
                break;
            }
            pos += (size_t)written;
        }
        slugify(text, slug, sizeof(slug));
        snprintf(image, sizeof(image), "%04zu-%s.png", segment, slug);

        fprintf(jsonl, "{\"segment_id\":\"%04zu\",\"segment_index\":%zu,\"start_seconds\":%.6f,\"end_seconds\":%.6f,\"word_start\":%zu,\"word_end\":%zu,\"image_filename\":\"%s\",\"text\":\"",
                segment, segment, words->items[i].start, words->items[end - 1].end, i + 1, end, image);
        cu_json_escape_write(jsonl, text);
        fputs("\"}\n", jsonl);

        fprintf(srt, "%zu\n", segment);
        srt_time(srt, words->items[i].start);
        fputs(" --> ", srt);
        srt_time(srt, words->items[end - 1].end);
        fprintf(srt, "\n%s\n\n", text);

        snprintf(prompt_path, sizeof(prompt_path), "%s/%04zu.txt", prompts_dir, segment);
        FILE *prompt = fopen(prompt_path, "wb");
        if (!prompt) {
            cu_die_errno("open prompt failed");
        }
        fprintf(prompt,
                "Use case: historical-scene\n"
                "Asset type: 1280x720 video still for bobmottram.mp4\n"
                "Primary request: Create a color graphite line-art rendered image in the same visual family as the Field of Chaos PNG frames. Interpret this transcript passage as a restrained documentary/story illustration.\n"
                "Scene/backdrop: derive the setting from the passage; if unclear, use a quiet interview/documentary environment with period-neutral details.\n"
                "Subject: people and objects implied by the passage, with coherent identities across the sequence where possible.\n"
                "Style/medium: color graphite line art, textured paper, dark precise linework, muted realistic color washes, not cartoonish.\n"
                "Composition/framing: cinematic 16:9, clear central subject, no text labels, no subtitles in the image.\n"
                "Constraints: no invented readable text, no watermark, no logo, no extra celebrity likeness unless explicitly named in the passage.\n"
                "Transcript passage: \"");
        cu_json_escape_write(prompt, text);
        fputs("\"\n", prompt);
        fclose(prompt);
    }

    fclose(jsonl);
    fclose(srt);
}

int main(int argc, char **argv) {
    CueList cues;
    WordList words = {0};

    cu_set_program_name("bob_vtt100");

    if (argc != 7) {
        fprintf(stderr, "usage: %s INPUT.vtt OUT.txt WORDS.jsonl GROUPS.jsonl GROUPS.srt PROMPTS_DIR\n", argv[0]);
        return 2;
    }

    cues = parse_vtt(argv[1]);
    if (cues.count == 0) {
        cu_die("no cues found in VTT");
    }
    cues_to_words(&cues, &words);
    if (words.count == 0) {
        cu_die("no words found in VTT");
    }
    write_plain_text(argv[2], &words);
    write_word_jsonl(argv[3], &words);
    write_groups(argv[4], argv[5], argv[6], &words);

    printf("cues=%zu words=%zu groups=%zu\n", cues.count, words.count, (words.count + 99) / 100);
    return 0;
}
