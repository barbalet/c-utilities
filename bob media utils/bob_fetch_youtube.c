#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common/cutil.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

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

static void clean_url(char *url) {
    size_t len = strlen(url);

    while (len > 0 && isspace((unsigned char)url[len - 1])) {
        url[--len] = '\0';
    }
    while (len > 0 && (url[len - 1] == ',' || url[len - 1] == ')' || url[len - 1] == ']')) {
        url[--len] = '\0';
    }
}

static void run_checked(const char *command) {
    int status = system(command);
    if (status != 0) {
        fprintf(stderr, "bob_fetch_youtube: command failed:\n%s\n", command);
        exit(1);
    }
}

static void first_existing_media(char *out, size_t out_size, const char *dir) {
    const char *exts[] = { "mp4", "m4a", "webm", "opus", "wav" };

    for (size_t i = 0; i < sizeof(exts) / sizeof(exts[0]); i++) {
        snprintf(out, out_size, "%s/source.%s", dir, exts[i]);
        if (access(out, R_OK) == 0) {
            return;
        }
    }

    cu_die("downloaded source media was not found");
}

int main(int argc, char **argv) {
    const char *ytdlp = "/private/tmp/bobmottram-venv/bin/yt-dlp";
    const char *ffmpeg = "ffmpeg";
    char url[2048];
    char audio_dir[PATH_MAX];
    char transcript_dir[PATH_MAX];
    char command[8192];
    char source_media[PATH_MAX];
    char wav_path[PATH_MAX];

    if (argc < 4 || argc > 5) {
        fprintf(stderr, "usage: %s URL AUDIO_DIR TRANSCRIPT_DIR [YT_DLP]\n", argv[0]);
        return 2;
    }
    cu_set_program_name("bob_fetch_youtube");

    if (argc == 5) {
        ytdlp = argv[4];
    }

    snprintf(url, sizeof(url), "%s", argv[1]);
    clean_url(url);
    snprintf(audio_dir, sizeof(audio_dir), "%s", argv[2]);
    snprintf(transcript_dir, sizeof(transcript_dir), "%s", argv[3]);
    cu_mkdir_p(audio_dir);
    cu_mkdir_p(transcript_dir);

    char *q_ytdlp = shell_quote(ytdlp);
    char *q_url = shell_quote(url);
    char *q_audio_template = NULL;
    char *q_transcript_template = NULL;
    char audio_template[PATH_MAX];
    char transcript_template[PATH_MAX];

    snprintf(audio_template, sizeof(audio_template), "%s/source.%%(ext)s", audio_dir);
    snprintf(transcript_template, sizeof(transcript_template), "%s/source", transcript_dir);
    q_audio_template = shell_quote(audio_template);
    q_transcript_template = shell_quote(transcript_template);

    snprintf(command, sizeof(command),
             "%s --impersonate chrome --remote-components ejs:github "
             "--js-runtimes node:/opt/homebrew/bin/node "
             "--extractor-args 'youtube:player_client=android' "
             "-f '18/140/251/bestaudio/best' -o %s %s",
             q_ytdlp, q_audio_template, q_url);
    run_checked(command);

    snprintf(command, sizeof(command),
             "%s --impersonate chrome --remote-components ejs:github "
             "--js-runtimes node:/opt/homebrew/bin/node "
             "--extractor-args 'youtube:player_client=android' "
             "--write-subs --write-auto-subs "
             "--sub-langs 'en.*' --sub-format vtt --skip-download -o %s %s",
             q_ytdlp, q_transcript_template, q_url);
    run_checked(command);

    first_existing_media(source_media, sizeof(source_media), audio_dir);
    snprintf(wav_path, sizeof(wav_path), "%s/source.wav", audio_dir);
    char *q_source = shell_quote(source_media);
    char *q_wav = shell_quote(wav_path);
    snprintf(command, sizeof(command),
             "%s -hide_banner -y -i %s -vn -ac 1 -ar 24000 -c:a pcm_s16le %s",
             ffmpeg, q_source, q_wav);
    run_checked(command);

    free(q_ytdlp);
    free(q_url);
    free(q_audio_template);
    free(q_transcript_template);
    free(q_source);
    free(q_wav);

    printf("%s\n", wav_path);
    return 0;
}
