#ifndef CUTIL_H
#define CUTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef struct {
    char *data;
    size_t len;
} CuBuffer;

void cu_set_program_name(const char *name);
void cu_die(const char *message);
void cu_die_errno(const char *message);

void *cu_xmalloc(size_t size);
void *cu_xrealloc(void *ptr, size_t size);
char *cu_xstrdup(const char *s);
char *cu_xstrndup(const char *s, size_t len);

bool cu_file_exists(const char *path);
bool cu_dir_exists(const char *path);
bool cu_ends_with(const char *s, const char *suffix);
void cu_mkdir_p(const char *path);

CuBuffer cu_read_file(const char *path);
char *cu_read_text_file(const char *path);
void cu_write_all(FILE *f, const void *data, size_t len);
void cu_write_file_bytes(const char *path, const unsigned char *data, size_t len);

char *cu_json_escape_alloc(const char *s);
void cu_json_escape_write(FILE *out, const char *s);

void cu_trim_inplace(char *s);
void cu_strip_newline(char *s);

#endif
