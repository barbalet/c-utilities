#include "cutil.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static const char *program_name = "cutil";

void cu_set_program_name(const char *name) {
    if (name && *name) {
        program_name = name;
    }
}

void cu_die(const char *message) {
    fprintf(stderr, "%s: %s\n", program_name, message);
    exit(1);
}

void cu_die_errno(const char *message) {
    fprintf(stderr, "%s: %s: %s\n", program_name, message, strerror(errno));
    exit(1);
}

void *cu_xmalloc(size_t size) {
    void *ptr = malloc(size ? size : 1);
    if (!ptr) {
        cu_die("out of memory");
    }
    return ptr;
}

void *cu_xrealloc(void *ptr, size_t size) {
    void *next = realloc(ptr, size ? size : 1);
    if (!next) {
        cu_die("out of memory");
    }
    return next;
}

char *cu_xstrndup(const char *s, size_t len) {
    char *copy = cu_xmalloc(len + 1);
    memcpy(copy, s, len);
    copy[len] = '\0';
    return copy;
}

char *cu_xstrdup(const char *s) {
    return cu_xstrndup(s, strlen(s));
}

bool cu_file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

bool cu_dir_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

bool cu_ends_with(const char *s, const char *suffix) {
    size_t slen = strlen(s);
    size_t suffix_len = strlen(suffix);
    return slen >= suffix_len && strcmp(s + slen - suffix_len, suffix) == 0;
}

void cu_mkdir_p(const char *path) {
    char tmp[PATH_MAX];
    size_t len = strlen(path);

    if (len == 0 || len >= sizeof(tmp)) {
        cu_die("bad directory path");
    }

    memcpy(tmp, path, len + 1);
    if (len > 1 && tmp[len - 1] == '/') {
        tmp[--len] = '\0';
    }

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                cu_die_errno("mkdir failed");
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        cu_die_errno("mkdir failed");
    }
}

CuBuffer cu_read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    CuBuffer b = {0};
    size_t cap = 0;

    if (!f) {
        cu_die_errno(path);
    }
    for (;;) {
        size_t got;
        if (b.len + 4097 > cap) {
            cap = cap ? cap * 2 : 8192;
            b.data = cu_xrealloc(b.data, cap);
        }
        got = fread(b.data + b.len, 1, 4096, f);
        b.len += got;
        if (got < 4096) {
            if (ferror(f)) {
                fclose(f);
                free(b.data);
                cu_die_errno("read failed");
            }
            break;
        }
    }
    fclose(f);
    if (!b.data) {
        b.data = cu_xmalloc(1);
    }
    b.data[b.len] = '\0';
    return b;
}

char *cu_read_text_file(const char *path) {
    CuBuffer b = cu_read_file(path);
    return b.data;
}

void cu_write_all(FILE *f, const void *data, size_t len) {
    if (len && fwrite(data, 1, len, f) != len) {
        cu_die_errno("write failed");
    }
}

void cu_write_file_bytes(const char *path, const unsigned char *data, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        cu_die_errno(path);
    }
    cu_write_all(f, data, len);
    if (fclose(f) != 0) {
        cu_die_errno(path);
    }
}

char *cu_json_escape_alloc(const char *s) {
    size_t len = 0;
    char *out;
    char *p;

    for (const unsigned char *c = (const unsigned char *)s; *c; c++) {
        switch (*c) {
        case '\\':
        case '"':
        case '\b':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
            len += 2;
            break;
        default:
            len += *c < 0x20 ? 6 : 1;
            break;
        }
    }

    out = cu_xmalloc(len + 1);
    p = out;
    for (const unsigned char *c = (const unsigned char *)s; *c; c++) {
        switch (*c) {
        case '\\': *p++ = '\\'; *p++ = '\\'; break;
        case '"': *p++ = '\\'; *p++ = '"'; break;
        case '\b': *p++ = '\\'; *p++ = 'b'; break;
        case '\f': *p++ = '\\'; *p++ = 'f'; break;
        case '\n': *p++ = '\\'; *p++ = 'n'; break;
        case '\r': *p++ = '\\'; *p++ = 'r'; break;
        case '\t': *p++ = '\\'; *p++ = 't'; break;
        default:
            if (*c < 0x20) {
                snprintf(p, 7, "\\u%04x", *c);
                p += 6;
            } else {
                *p++ = (char)*c;
            }
            break;
        }
    }
    *p = '\0';
    return out;
}

void cu_json_escape_write(FILE *out, const char *s) {
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '\\': fputs("\\\\", out); break;
        case '"': fputs("\\\"", out); break;
        case '\b': fputs("\\b", out); break;
        case '\f': fputs("\\f", out); break;
        case '\n': fputs("\\n", out); break;
        case '\r': fputs("\\r", out); break;
        case '\t': fputs("\\t", out); break;
        default:
            if (*p < 0x20) {
                fprintf(out, "\\u%04x", *p);
            } else {
                fputc(*p, out);
            }
            break;
        }
    }
}

void cu_trim_inplace(char *s) {
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

void cu_strip_newline(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[--len] = '\0';
    }
}
