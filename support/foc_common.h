#ifndef FOC_COMMON_H
#define FOC_COMMON_H

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

typedef struct {
    int index;
    int source_line;
    char *speaker;
    char *source_text;
    char *spoken_text;
    size_t chars;
    size_t words;
} FocSegment;

typedef struct {
    FocSegment *items;
    size_t count;
} FocScript;

typedef struct {
    char **items;
    size_t count;
} FocChunkList;

typedef struct {
    int sample_rate;
    int channels;
    int bits_per_sample;
    uint32_t data_offset;
    uint32_t data_bytes;
    uint64_t frames;
} FocWavInfo;

typedef int (*FocJsonObjectCallback)(const char *object, size_t len, void *ctx);

int foc_load_script(const char *path, FocScript *script, int verbose_malformed);
void foc_free_script(FocScript *script);
int foc_split_text(const char *text, size_t max_chars, FocChunkList *chunks);
void foc_free_chunks(FocChunkList *chunks);
char *foc_slug(const char *text, size_t max_len);
void foc_json_escape(FILE *out, const char *text);
char *foc_json_unescape(const char *start, const char *end);
int foc_json_each_array_object(const char *json_path, const char *array_key, FocJsonObjectCallback cb, void *ctx);
char *foc_json_get_string_slice(const char *object, size_t len, const char *key);
double foc_json_get_number_slice(const char *object, size_t len, const char *key, double default_value);
long foc_json_get_long_slice(const char *object, size_t len, const char *key, long default_value);
int foc_find_latest_cache(const char *cache_dir, const FocSegment *segment, char *json_path, size_t json_cap, char *wav_path, size_t wav_cap);
double foc_json_duration_seconds(const char *path);
int foc_json_chunk_count(const char *path);
int foc_wav_info(const char *path, FocWavInfo *info);
int foc_copy_wav_pcm16_as_aiff(FILE *out, const char *path, const FocWavInfo *info);
int foc_write_aiff_header(FILE *out, int sample_rate, uint64_t frames, int channels);
int foc_patch_aiff_header(FILE *out, int sample_rate, uint64_t frames, int channels);
int foc_write_aiff_silence(FILE *out, uint64_t frames, int channels);
uint64_t foc_file_size(const char *path);
int foc_has_suffix(const char *s, const char *suffix);
char *foc_read_file(const char *path, size_t *size_out);

#endif
