#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include "json_compat.h"
#include <math.h>
#include <openssl/evp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define PROJECT "/Users/barbalet/next/github/next/fieldofchaos-chechnya/youtube video/aa"
#define OUTPUT "/Volumes/500GB HDD/AA renders"
#define INTERVAL 10.0
#define WIDTH 1920
#define HEIGHT 1080
#define CARD_SECONDS 60.0

static const char *shots[] = {
    "wide documentary establishing shot that clearly shows the passage's place and activity",
    "candid medium shot centered on the people or action described by the passage",
    "intimate close documentary detail of the most meaningful object, face, or gesture",
    "natural over-the-shoulder viewpoint that places the viewer inside the remembered event",
    "cinematic side-angle scene with layered foreground and background storytelling",
    "quiet environmental portrait or still life that literally evokes the spoken memory",
    "slightly elevated observational angle with realistic everyday detail",
    "low eye-level documentary frame with strong depth and a grounded sense of place"
};

typedef struct {
    double start;
    double end;
    char *text;
} Cue;

typedef struct {
    Cue *items;
    size_t count;
    size_t capacity;
} CueList;

static void die(const char *message) {
    fprintf(stderr, "aa_render: %s\n", message);
    exit(EXIT_FAILURE);
}

static void die_errno(const char *message) {
    fprintf(stderr, "aa_render: %s: %s\n", message, strerror(errno));
    exit(EXIT_FAILURE);
}

static bool exists(const char *path) {
    struct stat st;
    return lstat(path, &st) == 0;
}

static bool regular_file(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static void mkdir_p(const char *path) {
    char copy[PATH_MAX];
    if (snprintf(copy, sizeof(copy), "%s", path) >= (int)sizeof(copy)) die("path too long");
    size_t len = strlen(copy);
    if (!len) return;
    if (copy[len - 1] == '/') copy[len - 1] = '\0';
    for (char *p = copy + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(copy, 0755) != 0 && errno != EEXIST) die_errno("mkdir");
            *p = '/';
        }
    }
    if (mkdir(copy, 0755) != 0 && errno != EEXIST) die_errno("mkdir");
}

static void parent_dir(const char *path, char *out, size_t out_size) {
    if (snprintf(out, out_size, "%s", path) >= (int)out_size) die("path too long");
    char *slash = strrchr(out, '/');
    if (!slash) {
        snprintf(out, out_size, ".");
    } else if (slash == out) {
        slash[1] = '\0';
    } else {
        *slash = '\0';
    }
}

static void path_join(char *out, size_t out_size, const char *a, const char *b) {
    if (snprintf(out, out_size, "%s/%s", a, b) >= (int)out_size) die("path too long");
}

static void stem_from_path(const char *path, char *stem, size_t stem_size) {
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    if (snprintf(stem, stem_size, "%s", base) >= (int)stem_size) die("stem too long");
    char *dot = strrchr(stem, '.');
    if (dot) *dot = '\0';
    for (char *p = stem; *p; ++p) {
        if (!( (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
               (*p >= '0' && *p <= '9') || *p == '_' || *p == '-' )) {
            die("unsafe recording stem");
        }
    }
}

static int runv(char *const argv[]) {
    pid_t pid = fork();
    if (pid < 0) die_errno("fork");
    if (pid == 0) {
        execvp(argv[0], argv);
        fprintf(stderr, "aa_render: exec %s: %s\n", argv[0], strerror(errno));
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) die_errno("waitpid");
    if (!WIFEXITED(status)) return 128;
    return WEXITSTATUS(status);
}

static char *capturev(char *const argv[]) {
    int fds[2];
    if (pipe(fds) != 0) die_errno("pipe");
    pid_t pid = fork();
    if (pid < 0) die_errno("fork");
    if (pid == 0) {
        close(fds[0]);
        if (dup2(fds[1], STDOUT_FILENO) < 0) _exit(126);
        close(fds[1]);
        execvp(argv[0], argv);
        _exit(127);
    }
    close(fds[1]);
    size_t cap = 4096, len = 0;
    char *buffer = malloc(cap);
    if (!buffer) die("out of memory");
    for (;;) {
        if (len + 2048 + 1 > cap) {
            cap *= 2;
            char *grown = realloc(buffer, cap);
            if (!grown) die("out of memory");
            buffer = grown;
        }
        ssize_t got = read(fds[0], buffer + len, cap - len - 1);
        if (got < 0 && errno == EINTR) continue;
        if (got < 0) die_errno("read pipe");
        if (got == 0) break;
        len += (size_t)got;
    }
    close(fds[0]);
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) die_errno("waitpid");
    buffer[len] = '\0';
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        free(buffer);
        die("child command failed");
    }
    return buffer;
}

static void copy_file(const char *source, const char *destination) {
    int in = open(source, O_RDONLY);
    if (in < 0) die_errno("open source");
    char parent[PATH_MAX];
    parent_dir(destination, parent, sizeof(parent));
    mkdir_p(parent);
    char partial[PATH_MAX];
    if (snprintf(partial, sizeof(partial), "%s.partial", destination) >= (int)sizeof(partial)) die("path too long");
    int out = open(partial, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out < 0) die_errno("open destination");
    char buffer[1 << 20];
    for (;;) {
        ssize_t count = read(in, buffer, sizeof(buffer));
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) die_errno("read source");
        if (!count) break;
        ssize_t offset = 0;
        while (offset < count) {
            ssize_t written = write(out, buffer + offset, (size_t)(count - offset));
            if (written < 0 && errno == EINTR) continue;
            if (written < 0) die_errno("write destination");
            offset += written;
        }
    }
    if (fsync(out) != 0) die_errno("fsync destination");
    close(in);
    close(out);
    if (rename(partial, destination) != 0) die_errno("rename destination");
}

static void write_atomic(const char *path, const char *payload, size_t length) {
    char parent[PATH_MAX], partial[PATH_MAX];
    parent_dir(path, parent, sizeof(parent));
    mkdir_p(parent);
    if (snprintf(partial, sizeof(partial), "%s.partial", path) >= (int)sizeof(partial)) die("path too long");
    FILE *file = fopen(partial, "wb");
    if (!file) die_errno("open partial file");
    if (fwrite(payload, 1, length, file) != length) die_errno("write partial file");
    if (fflush(file) != 0) die_errno("flush partial file");
    if (fsync(fileno(file)) != 0) die_errno("fsync partial file");
    if (fclose(file) != 0) die_errno("close partial file");
    if (rename(partial, path) != 0) die_errno("rename partial file");
}

static void write_json_atomic(const char *path, struct json_object *value) {
    const char *text = json_object_to_json_string_ext(value, JSON_C_TO_STRING_PRETTY);
    size_t len = strlen(text);
    char *payload = malloc(len + 2);
    if (!payload) die("out of memory");
    memcpy(payload, text, len);
    payload[len++] = '\n';
    payload[len] = '\0';
    write_atomic(path, payload, len);
    free(payload);
}

static struct json_object *load_json(const char *path) {
    struct json_object *value = json_object_from_file(path);
    if (!value) {
        fprintf(stderr, "aa_render: cannot parse JSON: %s\n", path);
        exit(EXIT_FAILURE);
    }
    return value;
}

static double probe_duration(const char *path) {
    char *argv[] = {
        "ffprobe", "-v", "error", "-show_entries", "format=duration",
        "-of", "default=nw=1:nk=1", (char *)path, NULL
    };
    char *output = capturev(argv);
    double duration = strtod(output, NULL);
    free(output);
    if (!(duration > 0.0)) die("invalid audio duration");
    return duration;
}

static void sha256_file(const char *path, char result[65]) {
    FILE *file = fopen(path, "rb");
    if (!file) die_errno("open for sha256");
    EVP_MD_CTX *context = EVP_MD_CTX_new();
    if (!context) die("cannot allocate SHA-256 context");
    if (EVP_DigestInit_ex(context, EVP_sha256(), NULL) != 1) die("cannot initialize SHA-256");
    unsigned char buffer[1 << 20];
    size_t count;
    while ((count = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (EVP_DigestUpdate(context, buffer, count) != 1) die("cannot update SHA-256");
    }
    if (ferror(file)) die_errno("read for sha256");
    fclose(file);
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_length = 0;
    if (EVP_DigestFinal_ex(context, digest, &digest_length) != 1 || digest_length != 32) {
        EVP_MD_CTX_free(context);
        die("cannot finalize SHA-256");
    }
    EVP_MD_CTX_free(context);
    for (unsigned int i = 0; i < digest_length; ++i) sprintf(result + i * 2, "%02x", digest[i]);
    result[64] = '\0';
}

static void make_paths(const char *stem, char state[PATH_MAX], char frames[PATH_MAX], char source[PATH_MAX]) {
    snprintf(state, PATH_MAX, "%s/render_state/%s_movie", OUTPUT, stem);
    snprintf(frames, PATH_MAX, "%s/%s_frames", OUTPUT, stem);
    snprintf(source, PATH_MAX, "%s/source_audio/%s.mp3", OUTPUT, stem);
}

static void log_event(const char *stem, const char *format, ...) {
    char state[PATH_MAX], frames[PATH_MAX], source[PATH_MAX], path[PATH_MAX];
    make_paths(stem, state, frames, source);
    mkdir_p(state);
    snprintf(path, sizeof(path), "%s/native_pipeline.log", state);
    FILE *file = fopen(path, "ab");
    if (!file) die_errno("open native pipeline log");
    time_t now = time(NULL);
    struct tm utc;
    gmtime_r(&now, &utc);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &utc);
    fprintf(file, "%s ", timestamp);
    va_list args;
    va_start(args, format);
    vfprintf(file, format, args);
    va_end(args);
    fputc('\n', file);
    fflush(file);
    fsync(fileno(file));
    fclose(file);
}

static void ensure_link(const char *target, const char *link_path) {
    struct stat st;
    if (lstat(link_path, &st) == 0) return;
    char parent[PATH_MAX];
    parent_dir(link_path, parent, sizeof(parent));
    mkdir_p(parent);
    if (symlink(target, link_path) != 0) die_errno("symlink");
}

static char *read_all(const char *path, size_t *length) {
    FILE *file = fopen(path, "rb");
    if (!file) die_errno("open input");
    if (fseek(file, 0, SEEK_END) != 0) die_errno("seek input");
    long size = ftell(file);
    if (size < 0) die_errno("tell input");
    rewind(file);
    char *data = malloc((size_t)size + 1);
    if (!data) die("out of memory");
    size_t got = fread(data, 1, (size_t)size, file);
    if (got != (size_t)size) die_errno("read input");
    fclose(file);
    data[got] = '\0';
    if (length) *length = got;
    return data;
}

static double parse_srt_time(const char *text) {
    int h = 0, m = 0, s = 0, ms = 0;
    if (sscanf(text, "%d:%d:%d,%d", &h, &m, &s, &ms) != 4 &&
        sscanf(text, "%d:%d:%d.%d", &h, &m, &s, &ms) != 4) die("invalid SRT timestamp");
    return h * 3600.0 + m * 60.0 + s + ms / 1000.0;
}

static void format_time(double value, char output[32], char separator) {
    long long total = llround(fmax(0.0, value) * 1000.0);
    long long h = total / 3600000; total %= 3600000;
    long long m = total / 60000; total %= 60000;
    long long s = total / 1000; long long ms = total % 1000;
    snprintf(output, 32, "%02lld:%02lld:%02lld%c%03lld", h, m, s, separator, ms);
}

static void cue_push(CueList *list, double start, double end, const char *text) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity ? list->capacity * 2 : 256;
        list->items = realloc(list->items, list->capacity * sizeof(Cue));
        if (!list->items) die("out of memory");
    }
    list->items[list->count].start = start;
    list->items[list->count].end = end;
    list->items[list->count].text = strdup(text);
    if (!list->items[list->count].text) die("out of memory");
    list->count++;
}

static void cue_free(CueList *list) {
    for (size_t i = 0; i < list->count; ++i) free(list->items[i].text);
    free(list->items);
    memset(list, 0, sizeof(*list));
}

static char *trim(char *text) {
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') ++text;
    char *end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) --end;
    *end = '\0';
    return text;
}

static CueList parse_srt(const char *path) {
    size_t size = 0;
    char *data = read_all(path, &size);
    size_t clean = 0;
    for (size_t i = 0; i < size; ++i) {
        if (data[i] != '\r') data[clean++] = data[i];
    }
    data[clean] = '\0';
    CueList cues = {0};
    char *cursor = data;
    while (*cursor) {
        while (*cursor == '\n') ++cursor;
        if (!*cursor) break;
        char *block_end = strstr(cursor, "\n\n");
        if (block_end) *block_end = '\0';
        char *save = NULL;
        char *line = strtok_r(cursor, "\n", &save);
        char *time_line = NULL;
        while (line) {
            line = trim(line);
            if (strstr(line, "-->")) { time_line = line; break; }
            line = strtok_r(NULL, "\n", &save);
        }
        if (!time_line) {
            cursor = block_end ? block_end + 2 : cursor + strlen(cursor);
            continue;
        }
        char start_text[32] = {0}, end_text[32] = {0};
        if (sscanf(time_line, "%31s --> %31s", start_text, end_text) != 2) {
            cursor = block_end ? block_end + 2 : cursor + strlen(cursor);
            continue;
        }
        char combined[16384] = {0};
        size_t used = 0;
        while ((line = strtok_r(NULL, "\n", &save)) != NULL) {
            line = trim(line);
            if (!*line) continue;
            size_t n = strlen(line);
            if (used && used + 1 < sizeof(combined)) combined[used++] = ' ';
            if (used + n >= sizeof(combined)) n = sizeof(combined) - used - 1;
            memcpy(combined + used, line, n);
            used += n;
            combined[used] = '\0';
        }
        if (used) cue_push(&cues, parse_srt_time(start_text), parse_srt_time(end_text), combined);
        cursor = block_end ? block_end + 2 : cursor + strlen(cursor);
    }
    free(data);
    if (!cues.count) die("SRT contains no cues");
    return cues;
}

static void make_script(const char *stem, const char *srt_path) {
    CueList cues = parse_srt(srt_path);
    char state[PATH_MAX], frames[PATH_MAX], source[PATH_MAX], script[PATH_MAX], link[PATH_MAX];
    make_paths(stem, state, frames, source);
    snprintf(script, sizeof(script), "%s/%s_script.txt", state, stem);
    char partial[PATH_MAX];
    snprintf(partial, sizeof(partial), "%s.partial", script);
    FILE *file = fopen(partial, "wb");
    if (!file) die_errno("open script");
    for (size_t i = 0; i < cues.count; ++i) {
        char start[32], end[32];
        format_time(cues.items[i].start, start, '.');
        format_time(cues.items[i].end, end, '.');
        fprintf(file, "[%s - %s] SPEECH: %s\n", start, end, cues.items[i].text);
    }
    fclose(file);
    if (rename(partial, script) != 0) die_errno("rename script");
    snprintf(link, sizeof(link), "%s/%s_script.txt", PROJECT, stem);
    ensure_link(script, link);
    cue_free(&cues);
}

static void command_inventory(const char *directory) {
    DIR *dir = opendir(directory);
    if (!dir) die_errno("open inventory directory");
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len < 8 || strncmp(entry->d_name, "aa_", 3) || strcmp(entry->d_name + len - 4, ".mp3")) continue;
        char path[PATH_MAX], stem[256];
        path_join(path, sizeof(path), directory, entry->d_name);
        stem_from_path(path, stem, sizeof(stem));
        double duration = probe_duration(path);
        printf("%s\tduration=%.3f\tframes=%d\n", stem, duration, (int)ceil(duration / INTERVAL));
    }
    closedir(dir);
}

static void command_init(const char *audio_path) {
    if (!regular_file(audio_path)) die("audio source does not exist");
    if (!exists(OUTPUT)) die("external output drive is unavailable");
    char stem[256], state[PATH_MAX], frames[PATH_MAX], source[PATH_MAX];
    stem_from_path(audio_path, stem, sizeof(stem));
    make_paths(stem, state, frames, source);
    mkdir_p(state); mkdir_p(frames);
    char source_parent[PATH_MAX]; parent_dir(source, source_parent, sizeof(source_parent)); mkdir_p(source_parent);
    if (!regular_file(source)) copy_file(audio_path, source);
    double duration = probe_duration(source);
    int count = (int)ceil(duration / INTERVAL);
    double card_start = fmax(0.0, duration - CARD_SECONDS);

    struct json_object *root = json_object_new_object();
    json_object_object_add(root, "schema_version", json_object_new_int(1));
    json_object_object_add(root, "stem", json_object_new_string(stem));
    json_object_object_add(root, "source_audio", json_object_new_string(source));
    json_object_object_add(root, "audio_duration_seconds", json_object_new_double(duration));
    json_object_object_add(root, "frame_interval_seconds", json_object_new_double(INTERVAL));
    json_object_object_add(root, "frame_count", json_object_new_int(count));
    json_object_object_add(root, "final_card_start_seconds", json_object_new_double(card_start));
    json_object_object_add(root, "status", json_object_new_string("needs_native_transcript"));
    char state_path[PATH_MAX]; snprintf(state_path, sizeof(state_path), "%s/state.json", state);
    write_json_atomic(state_path, root);
    json_object_put(root);

    char project_audio[PATH_MAX], project_frames[PATH_MAX], project_state[PATH_MAX];
    snprintf(project_audio, sizeof(project_audio), "%s/%s.mp3", PROJECT, stem);
    snprintf(project_frames, sizeof(project_frames), "%s/%s_frames", PROJECT, stem);
    snprintf(project_state, sizeof(project_state), "%s/render_work/%s_movie", PROJECT, stem);
    ensure_link(source, project_audio);
    ensure_link(frames, project_frames);
    ensure_link(state, project_state);

    char readme[PATH_MAX]; snprintf(readme, sizeof(readme), "%s/README.md", state);
    char body[8192];
    int n = snprintf(body, sizeof(body),
        "# `%s.mp4` native AA render recovery\n\n"
        "This episode uses the compiled `aa_render_c` utility. All audio, transcripts, plans, frames, logs, subtitles, partial outputs and final outputs are stored under `%s`.\n\n"
        "- Source: `%s`\n- Duration: %.3f seconds\n- Cadence: one 1920x1080 photorealistic transcript-driven frame every 10 seconds\n"
        "- Required frames: %d\n- Final minute: deterministic requested text card\n- Narrative frames: no readable text, logos or watermarks\n\n"
        "Resume with `aa_render_c status %s`; completed frames are never regenerated.\n",
        stem, OUTPUT, source, duration, count, stem);
    if (n < 0 || n >= (int)sizeof(body)) die("README too long");
    write_atomic(readme, body, (size_t)n);
    log_event(stem, "initialized duration=%.3f frames=%d source=%s", duration, count, source);
    printf("initialized %s duration=%.3f frames=%d state=%s\n", stem, duration, count, state);
}

static void command_transcribe(const char *stem, const char *whisper_cli, const char *model) {
    char state[PATH_MAX], frames[PATH_MAX], source[PATH_MAX];
    make_paths(stem, state, frames, source);
    if (!regular_file(source)) die("native source audio is missing; run init first");
    char wav[PATH_MAX], prefix[PATH_MAX], srt[PATH_MAX], public_srt[PATH_MAX];
    snprintf(wav, sizeof(wav), "%s/transcript_input.wav", state);
    snprintf(prefix, sizeof(prefix), "%s/transcript", state);
    snprintf(srt, sizeof(srt), "%s.srt", prefix);
    snprintf(public_srt, sizeof(public_srt), "%s/%s.srt", OUTPUT, stem);
    log_event(stem, "transcription started engine=%s model=%s", whisper_cli, model);
    if (!regular_file(wav)) {
        char *argv[] = {"ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", source,
                        "-vn", "-ac", "1", "-ar", "16000", "-c:a", "pcm_s16le", wav, NULL};
        if (runv(argv)) die("ffmpeg audio conversion failed");
    }
    char *argv[] = {(char *)whisper_cli, "-m", (char *)model, "-f", wav, "-l", "en", "-t", "8",
                    "-osrt", "-otxt", "-oj", "-of", prefix, "-pp", NULL};
    if (runv(argv)) die("native whisper transcription failed");
    if (!regular_file(srt)) die("native whisper did not create an SRT");
    copy_file(srt, public_srt);
    make_script(stem, public_srt);
    char project_srt[PATH_MAX]; snprintf(project_srt, sizeof(project_srt), "%s/%s.srt", PROJECT, stem);
    ensure_link(public_srt, project_srt);
    log_event(stem, "transcription complete srt=%s", public_srt);
    printf("transcribed %s srt=%s\n", stem, public_srt);
}

static char *combined_for_interval(const CueList *cues, double start, double end) {
    size_t cap = 4096, len = 0;
    char *text = calloc(1, cap);
    if (!text) die("out of memory");
    for (size_t i = 0; i < cues->count; ++i) {
        if (cues->items[i].end <= start || cues->items[i].start >= end) continue;
        size_t n = strlen(cues->items[i].text);
        if (len + n + 2 > cap) {
            while (len + n + 2 > cap) cap *= 2;
            text = realloc(text, cap);
            if (!text) die("out of memory");
        }
        if (len) text[len++] = ' ';
        memcpy(text + len, cues->items[i].text, n);
        len += n;
        text[len] = '\0';
    }
    return text;
}

static void command_plan(const char *stem) {
    char state[PATH_MAX], frames[PATH_MAX], source[PATH_MAX], state_path[PATH_MAX], srt[PATH_MAX];
    make_paths(stem, state, frames, source);
    snprintf(state_path, sizeof(state_path), "%s/state.json", state);
    snprintf(srt, sizeof(srt), "%s/%s.srt", OUTPUT, stem);
    struct json_object *state_json = load_json(state_path);
    double duration = json_object_get_double(json_object_object_get(state_json, "audio_duration_seconds"));
    int frame_count = json_object_get_int(json_object_object_get(state_json, "frame_count"));
    double card_start = json_object_get_double(json_object_object_get(state_json, "final_card_start_seconds"));
    make_script(stem, srt);
    CueList cues = parse_srt(srt);

    struct json_object *root = json_object_new_object();
    json_object_object_add(root, "schema_version", json_object_new_int(2));
    json_object_object_add(root, "native_utility", json_object_new_string("aa_render_c"));
    json_object_object_add(root, "source_audio", json_object_new_string(source));
    json_object_object_add(root, "subtitle_file", json_object_new_string(srt));
    json_object_object_add(root, "visual_direction", json_object_new_string("literal photorealistic transcript-driven documentary scenes, never a desk-only sequence"));
    json_object_object_add(root, "visual_style", json_object_new_string("strict photorealistic natural-light documentary photography"));
    json_object_object_add(root, "frame_interval_seconds", json_object_new_double(INTERVAL));
    json_object_object_add(root, "frame_width", json_object_new_int(WIDTH));
    json_object_object_add(root, "frame_height", json_object_new_int(HEIGHT));
    json_object_object_add(root, "audio_duration_seconds", json_object_new_double(duration));
    json_object_object_add(root, "frame_count", json_object_new_int(frame_count));
    struct json_object *card = json_object_new_object();
    json_object_object_add(card, "start_seconds", json_object_new_double(card_start));
    json_object_object_add(card, "end_seconds", json_object_new_double(duration));
    struct json_object *lines = json_object_new_array();
    const char *card_lines[] = {"From the computers that produced.", "AA.", "AA in AI.", "Tell your friends!", "barbalet@gmail.com"};
    for (size_t i = 0; i < sizeof(card_lines)/sizeof(card_lines[0]); ++i) json_object_array_add(lines, json_object_new_string(card_lines[i]));
    json_object_object_add(card, "lines", lines);
    json_object_object_add(root, "final_minute_card", card);
    struct json_object *array = json_object_new_array();
    for (int index = 0; index < frame_count; ++index) {
        double start = index * INTERVAL;
        double end = fmin(duration, start + INTERVAL);
        char filename[256]; snprintf(filename, sizeof(filename), "%s_frame_%04d.png", stem, index);
        char *excerpt = combined_for_interval(&cues, start, end);
        struct json_object *row = json_object_new_object();
        json_object_object_add(row, "index", json_object_new_int(index));
        json_object_object_add(row, "start_seconds", json_object_new_double(start));
        json_object_object_add(row, "end_seconds", json_object_new_double(end));
        json_object_object_add(row, "duration_seconds", json_object_new_double(end - start));
        json_object_object_add(row, "frame_file", json_object_new_string(filename));
        json_object_object_add(row, "kind", json_object_new_string(start >= card_start ? "end_card" : "narrative"));
        json_object_object_add(row, "shot", json_object_new_string(shots[index % 8]));
        json_object_object_add(row, "script_excerpt", json_object_new_string(excerpt));
        json_object_object_add(row, "safe_visual_context", json_object_new_string(excerpt));
        json_object_array_add(array, row);
        free(excerpt);
    }
    json_object_object_add(root, "frames", array);
    char plan_path[PATH_MAX]; snprintf(plan_path, sizeof(plan_path), "%s/render_plan.json", state);
    write_json_atomic(plan_path, root);
    json_object_put(root);

    char progress_path[PATH_MAX]; snprintf(progress_path, sizeof(progress_path), "%s/progress.json", state);
    if (!regular_file(progress_path)) {
        struct json_object *progress = json_object_new_object();
        json_object_object_add(progress, "schema_version", json_object_new_int(2));
        json_object_object_add(progress, "native_utility", json_object_new_string("aa_render_c"));
        json_object_object_add(progress, "required_frame_count", json_object_new_int(frame_count));
        json_object_object_add(progress, "completed_frame_count", json_object_new_int(0));
        json_object_object_add(progress, "next_frame_index", json_object_new_int(0));
        json_object_object_add(progress, "status", json_object_new_string("ready_to_render"));
        json_object_object_add(progress, "completed_frames", json_object_new_array());
        write_json_atomic(progress_path, progress);
        json_object_put(progress);
    }
    char prompts_path[PATH_MAX]; snprintf(prompts_path, sizeof(prompts_path), "%s/image_prompts.jsonl", state);
    FILE *prompts = fopen(prompts_path, "wb");
    if (!prompts) die_errno("open prompts");
    struct json_object *plan = load_json(plan_path);
    struct json_object *plan_frames = json_object_object_get(plan, "frames");
    for (int index = 0; index < frame_count; ++index) {
        struct json_object *row = json_object_array_get_idx(plan_frames, (size_t)index);
        const char *kind = json_object_get_string(json_object_object_get(row, "kind"));
        if (!strcmp(kind, "end_card")) continue;
        const char *shot = json_object_get_string(json_object_object_get(row, "shot"));
        const char *excerpt = json_object_get_string(json_object_object_get(row, "safe_visual_context"));
        char prompt[32768];
        snprintf(prompt, sizeof(prompt),
            "Use case: photorealistic-natural\nAsset type: 1920x1080 AA podcast-film frame %04d\nPrimary request: Create a literal visual scene from this spoken passage: %s\nComposition/framing: %s, 16:9 horizontal\nStyle/medium: rigorous photorealistic cinematic documentary photography, natural skin, hands, materials and period detail\nConstraints: no readable or pseudo-readable text, no captions, logos, trademarks, labels, interfaces or watermarks; not a desk-only scene; show the passage itself visually.",
            index, excerpt, shot);
        struct json_object *job = json_object_new_object();
        json_object_object_add(job, "index", json_object_new_int(index));
        json_object_object_add(job, "prompt", json_object_new_string(prompt));
        fprintf(prompts, "%s\n", json_object_to_json_string_ext(job, JSON_C_TO_STRING_PLAIN));
        json_object_put(job);
    }
    fclose(prompts);
    json_object_put(plan);
    json_object_put(state_json);
    cue_free(&cues);
    log_event(stem, "render plan complete frames=%d plan=%s", frame_count, plan_path);
    printf("planned %s frames=%d plan=%s prompts=%s\n", stem, frame_count, plan_path, prompts_path);
}

static struct json_object *plan_for(const char *stem, char path[PATH_MAX]) {
    char state[PATH_MAX], frames[PATH_MAX], source[PATH_MAX];
    make_paths(stem, state, frames, source);
    snprintf(path, PATH_MAX, "%s/render_plan.json", state);
    return load_json(path);
}

static void command_register(const char *stem, int index, const char *image, bool keep_source) {
    if (!regular_file(image)) die("generated image does not exist");
    char plan_path[PATH_MAX];
    struct json_object *plan = plan_for(stem, plan_path);
    struct json_object *rows = json_object_object_get(plan, "frames");
    int frame_count = json_object_get_int(json_object_object_get(plan, "frame_count"));
    if (index < 0 || index >= frame_count) die("frame index out of range");
    struct json_object *row = json_object_array_get_idx(rows, (size_t)index);
    const char *file_name = json_object_get_string(json_object_object_get(row, "frame_file"));
    const char *kind = json_object_get_string(json_object_object_get(row, "kind"));
    char state[PATH_MAX], frames[PATH_MAX], source[PATH_MAX], final[PATH_MAX], partial[PATH_MAX];
    make_paths(stem, state, frames, source);
    path_join(final, sizeof(final), frames, file_name);
    if (regular_file(final)) die("refusing to overwrite completed frame");
    snprintf(partial, sizeof(partial), "%s.partial.png", final);
    char source_hash[65]; sha256_file(image, source_hash);
    char filter[256]; snprintf(filter, sizeof(filter), "scale=%d:%d:force_original_aspect_ratio=increase,crop=%d:%d,format=rgb24", WIDTH, HEIGHT, WIDTH, HEIGHT);
    char *argv[] = {"ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", (char *)image,
                    "-vf", filter, "-frames:v", "1", "-compression_level", "9", partial, NULL};
    if (runv(argv)) die("frame normalization failed");
    if (rename(partial, final) != 0) die_errno("install normalized frame");
    if (!keep_source && strcmp(kind, "end_card")) unlink(image);
    char frame_hash[65]; sha256_file(final, frame_hash);

    char progress_path[PATH_MAX]; snprintf(progress_path, sizeof(progress_path), "%s/progress.json", state);
    struct json_object *progress = load_json(progress_path);
    struct json_object *completed = json_object_object_get(progress, "completed_frames");
    struct json_object *entry = json_object_new_object();
    json_object_object_add(entry, "index", json_object_new_int(index));
    json_object_object_add(entry, "kind", json_object_new_string(kind));
    json_object_object_add(entry, "source_generated_sha256", json_object_new_string(source_hash));
    json_object_object_add(entry, "frame_file", json_object_new_string(final));
    json_object_object_add(entry, "frame_sha256", json_object_new_string(frame_hash));
    json_object_object_add(entry, "start_seconds", json_object_get(json_object_object_get(row, "start_seconds")));
    json_object_object_add(entry, "end_seconds", json_object_get(json_object_object_get(row, "end_seconds")));
    json_object_object_add(entry, "script_excerpt", json_object_get(json_object_object_get(row, "script_excerpt")));
    json_object_object_add(entry, "visual_review", json_object_new_string(!strcmp(kind, "end_card") ? "deterministic final-minute end card accepted" : "strictly photorealistic, transcript-driven, text-free scene accepted"));
    json_object_array_add(completed, entry);
    int completed_count = (int)json_object_array_length(completed);
    int next = 0;
    for (; next < frame_count; ++next) {
        struct json_object *next_row = json_object_array_get_idx(rows, (size_t)next);
        const char *next_name = json_object_get_string(json_object_object_get(next_row, "frame_file"));
        char next_path[PATH_MAX]; path_join(next_path, sizeof(next_path), frames, next_name);
        if (!regular_file(next_path)) break;
    }
    json_object_object_add(progress, "completed_frame_count", json_object_new_int(completed_count));
    json_object_object_add(progress, "next_frame_index", json_object_new_int(next));
    json_object_object_add(progress, "status", json_object_new_string(completed_count == frame_count ? "complete" : "rendering"));
    write_json_atomic(progress_path, progress);

    char checksums[PATH_MAX]; snprintf(checksums, sizeof(checksums), "%s/frames.sha256", state);
    FILE *sum = fopen(checksums, "ab");
    if (!sum) die_errno("open frame checksums");
    fprintf(sum, "%s  %s\n", frame_hash, final);
    fclose(sum);
    json_object_put(progress);
    json_object_put(plan);
    log_event(stem, "registered frame=%04d completed=%d/%d next=%d", index, completed_count, frame_count, next);
    printf("registered %s %04d completed=%d/%d next=%d\n", stem, index, completed_count, frame_count, next);
}

static void command_end_card(const char *stem) {
    char state[PATH_MAX], frames[PATH_MAX], source[PATH_MAX], card[PATH_MAX], textfile[PATH_MAX];
    make_paths(stem, state, frames, source);
    snprintf(card, sizeof(card), "%s/%s_end_card.png", state, stem);
    snprintf(textfile, sizeof(textfile), "%s/end_card.txt", state);
    const char *text = "From the computers that produced.\nAA.\nAA in AI.\nTell your friends!\nbarbalet@gmail.com\n";
    write_atomic(textfile, text, strlen(text));
    char filter[PATH_MAX * 2];
    snprintf(filter, sizeof(filter), "drawtext=fontfile=/System/Library/Fonts/Supplemental/Arial.ttf:textfile='%s':fontcolor=0xf5f6f8:fontsize=72:line_spacing=30:x=(w-text_w)/2:y=(h-text_h)/2", textfile);
    char *argv[] = {"ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi", "-i",
                    "color=c=0x070a0f:s=1920x1080", "-vf", filter, "-frames:v", "1", card, NULL};
    if (runv(argv)) die("end-card generation failed");
    char plan_path[PATH_MAX]; struct json_object *plan = plan_for(stem, plan_path);
    struct json_object *rows = json_object_object_get(plan, "frames");
    int count = json_object_get_int(json_object_object_get(plan, "frame_count"));
    for (int index = 0; index < count; ++index) {
        struct json_object *row = json_object_array_get_idx(rows, (size_t)index);
        const char *kind = json_object_get_string(json_object_object_get(row, "kind"));
        if (strcmp(kind, "end_card")) continue;
        const char *name = json_object_get_string(json_object_object_get(row, "frame_file"));
        char final[PATH_MAX]; path_join(final, sizeof(final), frames, name);
        if (!regular_file(final)) command_register(stem, index, card, true);
    }
    json_object_put(plan);
    log_event(stem, "deterministic final-minute card frames registered");
    printf("end card complete %s\n", card);
}

static void command_assemble(const char *stem) {
    char plan_path[PATH_MAX]; struct json_object *plan = plan_for(stem, plan_path);
    double duration = json_object_get_double(json_object_object_get(plan, "audio_duration_seconds"));
    struct json_object *card = json_object_object_get(plan, "final_minute_card");
    double card_start = json_object_get_double(json_object_object_get(card, "start_seconds"));
    int count = json_object_get_int(json_object_object_get(plan, "frame_count"));
    struct json_object *rows = json_object_object_get(plan, "frames");
    char state[PATH_MAX], frames[PATH_MAX], source[PATH_MAX], concat[PATH_MAX], output[PATH_MAX], partial[PATH_MAX];
    make_paths(stem, state, frames, source);
    snprintf(concat, sizeof(concat), "%s/%s_frames.ffconcat", state, stem);
    snprintf(output, sizeof(output), "%s/%s.mp4", OUTPUT, stem);
    snprintf(partial, sizeof(partial), "%s/%s.partial.mp4", OUTPUT, stem);
    double extension = 0.0;
    for (int i = 0; i < count; ++i) {
        struct json_object *row = json_object_array_get_idx(rows, (size_t)i);
        double start = json_object_get_double(json_object_object_get(row, "start_seconds"));
        double end = json_object_get_double(json_object_object_get(row, "end_seconds"));
        if (start < card_start && card_start < end) extension = end - card_start;
    }
    FILE *file = fopen(concat, "wb");
    if (!file) die_errno("open concat");
    fprintf(file, "ffconcat version 1.0\n");
    char last[PATH_MAX] = {0};
    for (int i = 0; i < count; ++i) {
        struct json_object *row = json_object_array_get_idx(rows, (size_t)i);
        const char *name = json_object_get_string(json_object_object_get(row, "frame_file"));
        char path[PATH_MAX]; path_join(path, sizeof(path), frames, name);
        if (!regular_file(path)) { fclose(file); die("all frames must exist before assembly"); }
        double start = json_object_get_double(json_object_object_get(row, "start_seconds"));
        double end = json_object_get_double(json_object_object_get(row, "end_seconds"));
        double frame_duration = end - start;
        if (start < card_start && card_start < end) frame_duration = card_start - start;
        if (i == count - 1) frame_duration += extension;
        fprintf(file, "file '%s'\nduration %.6f\n", path, frame_duration);
        snprintf(last, sizeof(last), "%s", path);
    }
    fprintf(file, "file '%s'\n", last);
    fclose(file);
    char duration_text[64]; snprintf(duration_text, sizeof(duration_text), "%.6f", duration);
    char *argv[] = {"ffmpeg", "-hide_banner", "-y", "-f", "concat", "-safe", "0", "-i", concat,
                    "-i", source, "-map", "0:v:0", "-map", "1:a:0", "-vf", "fps=30,format=yuv420p",
                    "-c:v", "libx264", "-preset", "medium", "-crf", "22", "-pix_fmt", "yuv420p",
                    "-movflags", "+faststart", "-c:a", "aac", "-b:a", "192k", "-t", duration_text, partial, NULL};
    if (runv(argv)) die("MP4 assembly failed");
    if (rename(partial, output) != 0) die_errno("install MP4");
    json_object_put(plan);
    log_event(stem, "MP4 assembled output=%s duration=%.3f", output, duration);
    printf("assembled %s\n", output);
}

static void append_checksum(FILE *file, const char *path) {
    if (!regular_file(path)) return;
    char hash[65]; sha256_file(path, hash);
    fprintf(file, "%s  %s\n", hash, path);
}

static void command_verify(const char *stem) {
    char state[PATH_MAX], frames[PATH_MAX], source[PATH_MAX], output[PATH_MAX], probe[PATH_MAX], verification[PATH_MAX];
    make_paths(stem, state, frames, source);
    snprintf(output, sizeof(output), "%s/%s.mp4", OUTPUT, stem);
    snprintf(probe, sizeof(probe), "%s/%s_mp4_ffprobe.json", state, stem);
    snprintf(verification, sizeof(verification), "%s/verification", state);
    mkdir_p(verification);
    char *probe_argv[] = {"ffprobe", "-v", "error", "-show_format", "-show_streams", "-of", "json", output, NULL};
    char *probe_text = capturev(probe_argv);
    write_atomic(probe, probe_text, strlen(probe_text));
    struct json_object *payload = json_tokener_parse(probe_text);
    free(probe_text);
    if (!payload) die("invalid ffprobe JSON");
    struct json_object *format = json_object_object_get(payload, "format");
    double actual = strtod(json_object_get_string(json_object_object_get(format, "duration")), NULL);
    double expected = probe_duration(source);
    if (fabs(actual - expected) > 0.25) die("MP4 duration mismatch");
    bool valid_video = false, valid_audio = false;
    struct json_object *streams = json_object_object_get(payload, "streams");
    for (size_t i = 0; i < json_object_array_length(streams); ++i) {
        struct json_object *stream = json_object_array_get_idx(streams, i);
        const char *type = json_object_get_string(json_object_object_get(stream, "codec_type"));
        const char *codec = json_object_get_string(json_object_object_get(stream, "codec_name"));
        if (!strcmp(type, "video")) {
            int w = json_object_get_int(json_object_object_get(stream, "width"));
            int h = json_object_get_int(json_object_object_get(stream, "height"));
            valid_video = !strcmp(codec, "h264") && w == WIDTH && h == HEIGHT;
        } else if (!strcmp(type, "audio")) valid_audio = !strcmp(codec, "aac");
    }
    json_object_put(payload);
    if (!valid_video || !valid_audio) die("MP4 codec or dimensions are invalid");
    char *decode_argv[] = {"ffmpeg", "-hide_banner", "-loglevel", "error", "-i", output, "-f", "null", "-", NULL};
    if (runv(decode_argv)) die("full MP4 decode failed");
    double snapshot_times[] = {fmax(0.0, expected - CARD_SECONDS - 0.25), fmax(0.0, expected - CARD_SECONDS + 0.25), fmax(0.0, expected - 1.0)};
    const char *snapshot_names[] = {"pre_card.png", "card_start.png", "near_end.png"};
    char snapshots[3][PATH_MAX];
    for (size_t i = 0; i < 3; ++i) {
        snprintf(snapshots[i], sizeof(snapshots[i]), "%s/%s", verification, snapshot_names[i]);
        char timestamp[64];
        snprintf(timestamp, sizeof(timestamp), "%.3f", snapshot_times[i]);
        char *snapshot_argv[] = {"ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-ss", timestamp,
                                 "-i", output, "-frames:v", "1", "-vf", "scale=1920:1080", snapshots[i], NULL};
        if (runv(snapshot_argv)) die("MP4 verification snapshot extraction failed");
    }
    char sums[PATH_MAX], plan[PATH_MAX], progress[PATH_MAX], srt[PATH_MAX], frame_sums[PATH_MAX], readme[PATH_MAX];
    snprintf(sums, sizeof(sums), "%s/final_outputs.sha256", state);
    snprintf(plan, sizeof(plan), "%s/render_plan.json", state);
    snprintf(progress, sizeof(progress), "%s/progress.json", state);
    snprintf(srt, sizeof(srt), "%s/%s.srt", OUTPUT, stem);
    snprintf(frame_sums, sizeof(frame_sums), "%s/frames.sha256", state);
    snprintf(readme, sizeof(readme), "%s/README.md", state);
    FILE *file = fopen(sums, "wb");
    if (!file) die_errno("open final checksums");
    append_checksum(file, source); append_checksum(file, output); append_checksum(file, srt);
    append_checksum(file, plan); append_checksum(file, progress); append_checksum(file, probe);
    append_checksum(file, frame_sums); append_checksum(file, readme);
    for (size_t i = 0; i < 3; ++i) append_checksum(file, snapshots[i]);
    fclose(file);
    log_event(stem, "verification complete duration=%.3f video=h264-%dx%d audio=aac", actual, WIDTH, HEIGHT);
    printf("verified %s duration=%.3f video=h264-%dx%d audio=aac checksums=%s\n", output, actual, WIDTH, HEIGHT, sums);
}

static void command_status(const char *stem) {
    char state[PATH_MAX], frames[PATH_MAX], source[PATH_MAX], state_path[PATH_MAX], progress_path[PATH_MAX], output[PATH_MAX];
    make_paths(stem, state, frames, source);
    snprintf(state_path, sizeof(state_path), "%s/state.json", state);
    snprintf(progress_path, sizeof(progress_path), "%s/progress.json", state);
    snprintf(output, sizeof(output), "%s/%s.mp4", OUTPUT, stem);
    struct json_object *state_json = load_json(state_path);
    int required = json_object_get_int(json_object_object_get(state_json, "frame_count"));
    int complete = 0, next = 0;
    const char *status = "needs_transcript";
    if (regular_file(progress_path)) {
        struct json_object *progress = load_json(progress_path);
        complete = json_object_get_int(json_object_object_get(progress, "completed_frame_count"));
        next = json_object_get_int(json_object_object_get(progress, "next_frame_index"));
        status = json_object_get_string(json_object_object_get(progress, "status"));
        printf("%s status=%s frames=%d/%d next=%d mp4=%s\n", stem, status, complete, required, next, regular_file(output) ? "yes" : "no");
        json_object_put(progress);
    } else {
        printf("%s status=%s frames=0/%d next=0 mp4=%s\n", stem, status, required, regular_file(output) ? "yes" : "no");
    }
    json_object_put(state_json);
}

static void usage(void) {
    fprintf(stderr,
        "Usage:\n"
        "  aa_render_c inventory DIRECTORY\n"
        "  aa_render_c init AUDIO.mp3\n"
        "  aa_render_c transcribe STEM WHISPER_CLI MODEL\n"
        "  aa_render_c plan STEM\n"
        "  aa_render_c register STEM INDEX IMAGE\n"
        "  aa_render_c end-card STEM\n"
        "  aa_render_c assemble STEM\n"
        "  aa_render_c verify STEM\n"
        "  aa_render_c status STEM\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    if (argc < 2) usage();
    if (!strcmp(argv[1], "inventory") && argc == 3) command_inventory(argv[2]);
    else if (!strcmp(argv[1], "init") && argc == 3) command_init(argv[2]);
    else if (!strcmp(argv[1], "transcribe") && argc == 5) command_transcribe(argv[2], argv[3], argv[4]);
    else if (!strcmp(argv[1], "plan") && argc == 3) command_plan(argv[2]);
    else if (!strcmp(argv[1], "register") && argc == 5) command_register(argv[2], atoi(argv[3]), argv[4], false);
    else if (!strcmp(argv[1], "end-card") && argc == 3) command_end_card(argv[2]);
    else if (!strcmp(argv[1], "assemble") && argc == 3) command_assemble(argv[2]);
    else if (!strcmp(argv[1], "verify") && argc == 3) command_verify(argv[2]);
    else if (!strcmp(argv[1], "status") && argc == 3) command_status(argv[2]);
    else usage();
    return EXIT_SUCCESS;
}
