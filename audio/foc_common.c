#define _DARWIN_C_SOURCE
#include "foc_common.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static char *xstrndup(const char *s, size_t n) {
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

char *foc_read_file(const char *path, size_t *size_out) {
    FILE *f = fopen(path, "rb");
    char *data;
    long n;
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    n = ftell(f);
    if (n < 0) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    data = (char *)malloc((size_t)n + 1);
    if (!data) { fclose(f); return NULL; }
    if (fread(data, 1, (size_t)n, f) != (size_t)n) { free(data); fclose(f); return NULL; }
    fclose(f);
    data[n] = '\0';
    if (size_out) *size_out = (size_t)n;
    return data;
}

static size_t count_words(const char *s) {
    size_t words = 0;
    int in_word = 0;
    while (*s) {
        if (isspace((unsigned char)*s)) {
            in_word = 0;
        } else if (!in_word) {
            words++;
            in_word = 1;
        }
        s++;
    }
    return words;
}

static void collapse_space(char *s) {
    char *r = s;
    char *w = s;
    int in_space = 1;
    while (*r) {
        if (isspace((unsigned char)*r)) {
            if (!in_space) *w++ = ' ';
            in_space = 1;
        } else {
            *w++ = *r;
            in_space = 0;
        }
        r++;
    }
    if (w > s && w[-1] == ' ') w--;
    *w = '\0';
}

int foc_load_script(const char *path, FocScript *script, int verbose_malformed) {
    size_t size = 0;
    char *data = foc_read_file(path, &size);
    char *p;
    int source_line = 1;
    size_t cap = 64;
    if (!data) {
        fprintf(stderr, "cannot read %s: %s\n", path, strerror(errno));
        return -1;
    }
    script->items = (FocSegment *)calloc(cap, sizeof(FocSegment));
    script->count = 0;
    if (!script->items) { free(data); return -1; }
    p = data;
    while (*p) {
        char *line_start = p;
        char *line_end;
        char *delim;
        while (*p && *p != '\n') p++;
        line_end = p;
        if (*p == '\n') p++;
        if (line_end > line_start && line_end[-1] == '\r') line_end--;
        delim = NULL;
        if (line_end > line_start) {
            char saved = *line_end;
            *line_end = '\0';
            delim = strstr(line_start, "  :  ");
            if (delim) {
                FocSegment *seg;
                char *body;
                if (script->count == cap) {
                    FocSegment *grown;
                    cap *= 2;
                    grown = (FocSegment *)realloc(script->items, cap * sizeof(FocSegment));
                    if (!grown) { *line_end = saved; free(data); return -1; }
                    memset(grown + script->count, 0, (cap - script->count) * sizeof(FocSegment));
                    script->items = grown;
                }
                seg = &script->items[script->count];
                seg->index = (int)script->count + 1;
                seg->source_line = source_line;
                seg->speaker = xstrndup(line_start, (size_t)(delim - line_start));
                seg->source_text = xstrndup(line_start, strlen(line_start));
                body = xstrndup(delim + 5, strlen(delim + 5));
                if (!seg->speaker || !seg->source_text || !body) {
                    *line_end = saved;
                    free(body);
                    free(data);
                    return -1;
                }
                collapse_space(body);
                seg->spoken_text = body;
                seg->chars = strlen(body);
                seg->words = count_words(body);
                script->count++;
            } else if (verbose_malformed) {
                fprintf(stderr, "%s:%d: malformed/non-speaker line: %s\n", path, source_line, line_start);
            }
            *line_end = saved;
        }
        source_line++;
    }
    free(data);
    return 0;
}

void foc_free_script(FocScript *script) {
    size_t i;
    if (!script) return;
    for (i = 0; i < script->count; i++) {
        free(script->items[i].speaker);
        free(script->items[i].source_text);
        free(script->items[i].spoken_text);
    }
    free(script->items);
    script->items = NULL;
    script->count = 0;
}

static char *trim_copy(const char *start, const char *end) {
    while (start < end && isspace((unsigned char)*start)) start++;
    while (end > start && isspace((unsigned char)end[-1])) end--;
    return xstrndup(start, (size_t)(end - start));
}

static int chunk_push(FocChunkList *chunks, const char *start, const char *end) {
    char **grown;
    char *copy = trim_copy(start, end);
    if (!copy) return -1;
    if (!*copy) { free(copy); return 0; }
    grown = (char **)realloc(chunks->items, (chunks->count + 1) * sizeof(char *));
    if (!grown) { free(copy); return -1; }
    chunks->items = grown;
    chunks->items[chunks->count++] = copy;
    return 0;
}

static int split_words_to_chunks(const char *piece, size_t max_chars, FocChunkList *chunks) {
    const char *p = piece;
    char *current = (char *)calloc(max_chars + 2, 1);
    size_t current_len = 0;
    if (!current) return -1;
    while (*p) {
        const char *word_start;
        const char *word_end;
        size_t word_len;
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        word_start = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        word_end = p;
        word_len = (size_t)(word_end - word_start);
        if (word_len > max_chars) {
            if (current_len && chunk_push(chunks, current, current + current_len) != 0) { free(current); return -1; }
            current_len = 0;
            if (chunk_push(chunks, word_start, word_end) != 0) { free(current); return -1; }
            continue;
        }
        if (current_len && current_len + 1 + word_len > max_chars) {
            if (chunk_push(chunks, current, current + current_len) != 0) { free(current); return -1; }
            current_len = 0;
        }
        if (current_len) current[current_len++] = ' ';
        memcpy(current + current_len, word_start, word_len);
        current_len += word_len;
        current[current_len] = '\0';
    }
    if (current_len && chunk_push(chunks, current, current + current_len) != 0) { free(current); return -1; }
    free(current);
    return 0;
}

static int add_piece_with_limit(const char *start, const char *end, size_t max_chars, FocChunkList *chunks, char **current, size_t *current_len) {
    char *piece = trim_copy(start, end);
    size_t piece_len;
    if (!piece) return -1;
    piece_len = strlen(piece);
    if (!piece_len) { free(piece); return 0; }
    if (piece_len > max_chars) {
        if (*current_len && chunk_push(chunks, *current, *current + *current_len) != 0) { free(piece); return -1; }
        *current_len = 0;
        if (split_words_to_chunks(piece, max_chars, chunks) != 0) { free(piece); return -1; }
        free(piece);
        return 0;
    }
    if (*current_len && *current_len + 1 + piece_len > max_chars) {
        if (chunk_push(chunks, *current, *current + *current_len) != 0) { free(piece); return -1; }
        *current_len = 0;
    }
    if (*current_len) (*current)[(*current_len)++] = ' ';
    memcpy(*current + *current_len, piece, piece_len);
    *current_len += piece_len;
    (*current)[*current_len] = '\0';
    free(piece);
    return 0;
}

int foc_split_text(const char *text, size_t max_chars, FocChunkList *chunks) {
    const char *sentence_start = text;
    const char *p = text;
    char *current;
    size_t current_len = 0;
    chunks->items = NULL;
    chunks->count = 0;
    if (max_chars == 0 || strlen(text) <= max_chars) return chunk_push(chunks, text, text + strlen(text));
    current = (char *)calloc(max_chars + 2, 1);
    if (!current) return -1;
    while (1) {
        int at_end = *p == '\0';
        int sentence_break = !at_end && strchr(".!?", *p) && (p[1] == '\0' || isspace((unsigned char)p[1]));
        if (at_end || sentence_break) {
            const char *sentence_end = at_end ? p : p + 1;
            if ((size_t)(sentence_end - sentence_start) > max_chars) {
                const char *clause_start = sentence_start;
                const char *q = sentence_start;
                while (q <= sentence_end) {
                    int clause_end = q == sentence_end || (strchr(",;:", *q) && (q[1] == '\0' || isspace((unsigned char)q[1])));
                    if (clause_end) {
                        const char *end = q == sentence_end ? q : q + 1;
                        if (add_piece_with_limit(clause_start, end, max_chars, chunks, &current, &current_len) != 0) { free(current); return -1; }
                        clause_start = q + 1;
                    }
                    if (q == sentence_end) break;
                    q++;
                }
            } else {
                if (add_piece_with_limit(sentence_start, sentence_end, max_chars, chunks, &current, &current_len) != 0) { free(current); return -1; }
            }
            if (at_end) break;
            p++;
            while (*p && isspace((unsigned char)*p)) p++;
            sentence_start = p;
            continue;
        }
        p++;
    }
    if (current_len && chunk_push(chunks, current, current + current_len) != 0) { free(current); return -1; }
    free(current);
    return chunks->count ? 0 : chunk_push(chunks, text, text + strlen(text));
}

void foc_free_chunks(FocChunkList *chunks) {
    size_t i;
    for (i = 0; i < chunks->count; i++) free(chunks->items[i]);
    free(chunks->items);
    chunks->items = NULL;
    chunks->count = 0;
}

char *foc_slug(const char *text, size_t max_len) {
    char *out = (char *)malloc(max_len + 1);
    size_t w = 0;
    int dash = 1;
    if (!out) return NULL;
    while (*text && w < max_len) {
        unsigned char c = (unsigned char)*text++;
        if (isalnum(c)) {
            out[w++] = (char)tolower(c);
            dash = 0;
        } else if (!dash && w < max_len) {
            out[w++] = '-';
            dash = 1;
        }
    }
    while (w > 0 && out[w - 1] == '-') w--;
    if (w == 0) {
        const char *fallback = "line";
        while (*fallback && w < max_len) out[w++] = *fallback++;
    }
    out[w] = '\0';
    return out;
}

void foc_json_escape(FILE *out, const char *text) {
    fputc('"', out);
    while (*text) {
        unsigned char c = (unsigned char)*text++;
        switch (c) {
            case '\\': fputs("\\\\", out); break;
            case '"': fputs("\\\"", out); break;
            case '\n': fputs("\\n", out); break;
            case '\r': fputs("\\r", out); break;
            case '\t': fputs("\\t", out); break;
            default:
                if (c < 32) fprintf(out, "\\u%04x", c);
                else fputc(c, out);
        }
    }
    fputc('"', out);
}

char *foc_json_unescape(const char *start, const char *end) {
    char *out = (char *)malloc((size_t)(end - start) + 1);
    char *w = out;
    const char *p = start;
    if (!out) return NULL;
    while (p < end) {
        if (*p == '\\' && p + 1 < end) {
            p++;
            switch (*p) {
                case '\\': *w++ = '\\'; break;
                case '"': *w++ = '"'; break;
                case '/': *w++ = '/'; break;
                case 'b': *w++ = '\b'; break;
                case 'f': *w++ = '\f'; break;
                case 'n': *w++ = '\n'; break;
                case 'r': *w++ = '\r'; break;
                case 't': *w++ = '\t'; break;
                default: *w++ = *p; break;
            }
            p++;
        } else {
            *w++ = *p++;
        }
    }
    *w = '\0';
    return out;
}

static const char *json_find_key(const char *object, size_t len, const char *key) {
    char needle[192];
    const char *p = object;
    const char *end = object + len;
    if (snprintf(needle, sizeof(needle), "\"%s\"", key) >= (int)sizeof(needle)) return NULL;
    while (p < end) {
        const char *hit = strstr(p, needle);
        if (!hit || hit >= end) return NULL;
        p = hit + strlen(needle);
        while (p < end && isspace((unsigned char)*p)) p++;
        if (p < end && *p == ':') return p + 1;
    }
    return NULL;
}

char *foc_json_get_string_slice(const char *object, size_t len, const char *key) {
    const char *p = json_find_key(object, len, key);
    const char *end = object + len;
    const char *start;
    if (!p) return NULL;
    while (p < end && isspace((unsigned char)*p)) p++;
    if (p >= end || *p != '"') return NULL;
    start = ++p;
    while (p < end) {
        if (*p == '"' && (p == start || p[-1] != '\\')) return foc_json_unescape(start, p);
        p++;
    }
    return NULL;
}

double foc_json_get_number_slice(const char *object, size_t len, const char *key, double default_value) {
    const char *p = json_find_key(object, len, key);
    char *parse_end = NULL;
    double value;
    if (!p) return default_value;
    value = strtod(p, &parse_end);
    return parse_end == p ? default_value : value;
}

long foc_json_get_long_slice(const char *object, size_t len, const char *key, long default_value) {
    const char *p = json_find_key(object, len, key);
    char *parse_end = NULL;
    long value;
    if (!p) return default_value;
    value = strtol(p, &parse_end, 10);
    return parse_end == p ? default_value : value;
}

int foc_json_each_array_object(const char *json_path, const char *array_key, FocJsonObjectCallback cb, void *ctx) {
    size_t size = 0;
    char *data = foc_read_file(json_path, &size);
    char needle[192];
    char *p;
    char *end;
    int count = 0;
    (void)size;
    if (!data) {
        fprintf(stderr, "cannot read %s: %s\n", json_path, strerror(errno));
        return -1;
    }
    if (snprintf(needle, sizeof(needle), "\"%s\"", array_key) >= (int)sizeof(needle)) {
        free(data);
        return -1;
    }
    p = strstr(data, needle);
    if (!p) {
        free(data);
        return 0;
    }
    p = strchr(p, '[');
    if (!p) {
        free(data);
        return 0;
    }
    p++;
    end = data + strlen(data);
    while (p < end) {
        int in_string = 0;
        int escaped = 0;
        int depth = 0;
        char *object_start = NULL;
        while (p < end) {
            if (!in_string && *p == ']') {
                free(data);
                return count;
            }
            if (!in_string && *p == '{') {
                object_start = p;
                depth = 1;
                p++;
                break;
            }
            p++;
        }
        if (!object_start) break;
        while (p < end && depth > 0) {
            char c = *p;
            if (in_string) {
                if (escaped) escaped = 0;
                else if (c == '\\') escaped = 1;
                else if (c == '"') in_string = 0;
            } else {
                if (c == '"') in_string = 1;
                else if (c == '{') depth++;
                else if (c == '}') depth--;
            }
            p++;
        }
        if (depth != 0) break;
        if (cb(object_start, (size_t)(p - object_start), ctx) != 0) {
            free(data);
            return -1;
        }
        count++;
    }
    free(data);
    return count;
}

int foc_has_suffix(const char *s, const char *suffix) {
    size_t a = strlen(s), b = strlen(suffix);
    return a >= b && strcmp(s + a - b, suffix) == 0;
}

int foc_find_latest_cache(const char *cache_dir, const FocSegment *segment, char *json_path, size_t json_cap, char *wav_path, size_t wav_cap) {
    DIR *dir;
    struct dirent *entry;
    char *slug;
    char prefix[256];
    char best_json[1024] = {0};
    time_t best_time = 0;
    int found = 0;
    slug = foc_slug(segment->speaker, 32);
    if (!slug) return 0;
    snprintf(prefix, sizeof(prefix), "%04d-%04d-%s-", segment->index, segment->source_line, slug);
    free(slug);
    dir = opendir(cache_dir);
    if (!dir) return 0;
    while ((entry = readdir(dir)) != NULL) {
        char full[1024];
        struct stat st;
        if (strncmp(entry->d_name, prefix, strlen(prefix)) != 0) continue;
        if (!foc_has_suffix(entry->d_name, ".json")) continue;
        snprintf(full, sizeof(full), "%s/%s", cache_dir, entry->d_name);
        if (stat(full, &st) != 0) continue;
        if (!found || st.st_mtime >= best_time) {
            found = 1;
            best_time = st.st_mtime;
            snprintf(best_json, sizeof(best_json), "%s", full);
        }
    }
    closedir(dir);
    if (!found) return 0;
    snprintf(json_path, json_cap, "%s", best_json);
    snprintf(wav_path, wav_cap, "%s", best_json);
    if (strlen(wav_path) > 5) strcpy(wav_path + strlen(wav_path) - 5, ".wav");
    return 1;
}

double foc_json_duration_seconds(const char *path) {
    size_t size = 0;
    char *data = foc_read_file(path, &size);
    char *p;
    double value = -1.0;
    (void)size;
    if (!data) return -1.0;
    p = strstr(data, "\"duration_seconds\"");
    if (p && (p = strchr(p, ':')) != NULL) value = strtod(p + 1, NULL);
    free(data);
    return value;
}

int foc_json_chunk_count(const char *path) {
    size_t size = 0;
    char *data = foc_read_file(path, &size);
    char *p;
    long value = -1;
    (void)size;
    if (!data) return -1;
    p = strstr(data, "\"chunk_count\"");
    if (!p) p = strstr(data, "\"tts_chunk_count\"");
    if (p && (p = strchr(p, ':')) != NULL) value = strtol(p + 1, NULL, 10);
    free(data);
    return (int)value;
}

static uint16_t le16(const unsigned char *p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
static uint32_t le32(const unsigned char *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t be16(const unsigned char *p) { return ((uint16_t)p[0] << 8) | (uint16_t)p[1]; }
static uint32_t be32(const unsigned char *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3]; }

static void put_be16(FILE *out, uint16_t v) { fputc((v >> 8) & 255, out); fputc(v & 255, out); }
static void put_be32(FILE *out, uint32_t v) { fputc((v >> 24) & 255, out); fputc((v >> 16) & 255, out); fputc((v >> 8) & 255, out); fputc(v & 255, out); }

int foc_wav_info(const char *path, FocWavInfo *info) {
    FILE *f = fopen(path, "rb");
    unsigned char h[12];
    int have_fmt = 0, have_data = 0;
    memset(info, 0, sizeof(*info));
    if (!f) return -1;
    if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) != 0 || memcmp(h + 8, "WAVE", 4) != 0) {
        fclose(f);
        return -1;
    }
    while (!have_data) {
        unsigned char ch[8];
        uint32_t n;
        long payload;
        if (fread(ch, 1, 8, f) != 8) break;
        n = le32(ch + 4);
        payload = ftell(f);
        if (memcmp(ch, "fmt ", 4) == 0) {
            unsigned char fmt[32];
            if (n < 16 || fread(fmt, 1, n < sizeof(fmt) ? n : sizeof(fmt), f) < 16) { fclose(f); return -1; }
            info->channels = le16(fmt + 2);
            info->sample_rate = (int)le32(fmt + 4);
            info->bits_per_sample = le16(fmt + 14);
            have_fmt = 1;
        } else if (memcmp(ch, "data", 4) == 0) {
            info->data_offset = (uint32_t)ftell(f);
            info->data_bytes = n;
            have_data = 1;
        }
        if (!have_data) fseek(f, payload + n + (n & 1), SEEK_SET);
    }
    fclose(f);
    if (!have_fmt || !have_data || info->channels <= 0 || info->bits_per_sample <= 0) return -1;
    info->frames = info->data_bytes / (uint32_t)(info->channels * (info->bits_per_sample / 8));
    return 0;
}

static void write_extended(FILE *out, double num) {
    unsigned char bytes[10];
    int sign = 0, expon;
    double fmant, fs;
    uint32_t hi, lo;
    if (num < 0) { sign = 0x8000; num = -num; }
    if (num == 0) {
        expon = 0; hi = 0; lo = 0;
    } else {
        fmant = frexp(num, &expon);
        expon += 16382;
        fmant = ldexp(fmant, 32);
        fs = floor(fmant);
        hi = (uint32_t)fs;
        fmant = ldexp(fmant - fs, 32);
        fs = floor(fmant);
        lo = (uint32_t)fs;
        expon |= sign;
    }
    bytes[0] = (unsigned char)((expon >> 8) & 255);
    bytes[1] = (unsigned char)(expon & 255);
    bytes[2] = (unsigned char)((hi >> 24) & 255);
    bytes[3] = (unsigned char)((hi >> 16) & 255);
    bytes[4] = (unsigned char)((hi >> 8) & 255);
    bytes[5] = (unsigned char)(hi & 255);
    bytes[6] = (unsigned char)((lo >> 24) & 255);
    bytes[7] = (unsigned char)((lo >> 16) & 255);
    bytes[8] = (unsigned char)((lo >> 8) & 255);
    bytes[9] = (unsigned char)(lo & 255);
    fwrite(bytes, 1, 10, out);
}

int foc_write_aiff_header(FILE *out, int sample_rate, uint64_t frames, int channels) {
    uint64_t data_bytes = frames * (uint64_t)channels * 2u;
    if (data_bytes > UINT32_MAX - 64) return -1;
    fwrite("FORM", 1, 4, out);
    put_be32(out, (uint32_t)(46 + data_bytes));
    fwrite("AIFF", 1, 4, out);
    fwrite("COMM", 1, 4, out);
    put_be32(out, 18);
    put_be16(out, (uint16_t)channels);
    put_be32(out, (uint32_t)frames);
    put_be16(out, 16);
    write_extended(out, (double)sample_rate);
    fwrite("SSND", 1, 4, out);
    put_be32(out, (uint32_t)(data_bytes + 8));
    put_be32(out, 0);
    put_be32(out, 0);
    return ferror(out) ? -1 : 0;
}

int foc_patch_aiff_header(FILE *out, int sample_rate, uint64_t frames, int channels) {
    if (fseek(out, 0, SEEK_SET) != 0) return -1;
    return foc_write_aiff_header(out, sample_rate, frames, channels);
}

int foc_copy_wav_pcm16_as_aiff(FILE *out, const char *path, const FocWavInfo *info) {
    FILE *in = fopen(path, "rb");
    unsigned char buf[8192];
    uint32_t remaining = info->data_bytes;
    if (!in) return -1;
    if (info->bits_per_sample != 16) { fclose(in); return -1; }
    if (fseek(in, (long)info->data_offset, SEEK_SET) != 0) { fclose(in); return -1; }
    while (remaining) {
        size_t want = remaining < sizeof(buf) ? remaining : sizeof(buf);
        size_t got = fread(buf, 1, want, in);
        size_t i;
        if (got == 0) { fclose(in); return -1; }
        for (i = 0; i + 1 < got; i += 2) {
            unsigned char a = buf[i];
            buf[i] = buf[i + 1];
            buf[i + 1] = a;
        }
        fwrite(buf, 1, got, out);
        remaining -= (uint32_t)got;
    }
    fclose(in);
    return ferror(out) ? -1 : 0;
}

int foc_write_aiff_silence(FILE *out, uint64_t frames, int channels) {
    unsigned char zeros[4096] = {0};
    uint64_t bytes = frames * (uint64_t)channels * 2u;
    while (bytes) {
        size_t n = bytes < sizeof(zeros) ? (size_t)bytes : sizeof(zeros);
        fwrite(zeros, 1, n, out);
        bytes -= n;
    }
    return ferror(out) ? -1 : 0;
}

uint64_t foc_file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (uint64_t)st.st_size;
}

int foc_aiff_info_file(const char *path, int *sample_rate, int *channels, int *bits, uint64_t *frames) {
    FILE *f = fopen(path, "rb");
    unsigned char h[12];
    if (!f) return -1;
    if (fread(h, 1, 12, f) != 12 || memcmp(h, "FORM", 4) != 0 || memcmp(h + 8, "AIFF", 4) != 0) {
        fclose(f);
        return -1;
    }
    while (1) {
        unsigned char ch[8], comm[18];
        uint32_t n;
        long pos;
        if (fread(ch, 1, 8, f) != 8) break;
        n = be32(ch + 4);
        pos = ftell(f);
        if (memcmp(ch, "COMM", 4) == 0 && n >= 18) {
            if (fread(comm, 1, 18, f) != 18) break;
            *channels = be16(comm);
            *frames = be32(comm + 2);
            *bits = be16(comm + 6);
            *sample_rate = 0;
            fclose(f);
            return 0;
        }
        fseek(f, pos + n + (n & 1), SEEK_SET);
    }
    fclose(f);
    return -1;
}
