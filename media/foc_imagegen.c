#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "cutil.h"

static void usage(FILE *out) {
    fprintf(out,
            "Usage: foc_imagegen --prompt TEXT|--prompt-file PATH --out PATH [options]\n"
            "\n"
            "Options:\n"
            "  --model NAME          Default: gpt-image-2\n"
            "  --size WIDTHxHEIGHT   Default: 2048x1152\n"
            "  --quality VALUE       Default: high\n"
            "  --output-format FMT   Default: png\n"
            "  --force               Overwrite existing output\n"
            "  --dry-run             Write request JSON to stdout, do not call API\n"
            "  -h, --help            Show this help\n"
            "\n"
            "Requires OPENAI_API_KEY and curl. Writes native API PNG bytes; it does not resample.\n");
}

static char *make_request_json(const char *model, const char *prompt, const char *size,
                               const char *quality, const char *format) {
    char *m = cu_json_escape_alloc(model);
    char *p = cu_json_escape_alloc(prompt);
    char *s = cu_json_escape_alloc(size);
    char *q = cu_json_escape_alloc(quality);
    char *f = cu_json_escape_alloc(format);
    int needed = snprintf(NULL, 0,
                          "{\"model\":\"%s\",\"prompt\":\"%s\",\"size\":\"%s\","
                          "\"quality\":\"%s\",\"output_format\":\"%s\",\"n\":1}\n",
                          m, p, s, q, f);
    char *json;

    if (needed < 0) {
        cu_die("failed to build request JSON");
    }
    json = cu_xmalloc((size_t)needed + 1);
    snprintf(json, (size_t)needed + 1,
             "{\"model\":\"%s\",\"prompt\":\"%s\",\"size\":\"%s\","
             "\"quality\":\"%s\",\"output_format\":\"%s\",\"n\":1}\n",
             m, p, s, q, f);
    free(m);
    free(p);
    free(s);
    free(q);
    free(f);
    return json;
}

static char *make_temp_path(const char *suffix) {
    const char *tmp = getenv("TMPDIR");
    char templ[4096];
    int fd;

    if (!tmp || !*tmp) {
        tmp = "/tmp";
    }
    if (snprintf(templ, sizeof(templ), "%s/foc_imagegen_XXXXXX", tmp) >= (int)sizeof(templ)) {
        cu_die("temporary path too long");
    }

    char *path = cu_xstrdup(templ);
    fd = mkstemp(path);
    if (fd < 0) {
        cu_die_errno("mkstemp failed");
    }
    close(fd);

    if (suffix && *suffix) {
        int needed = snprintf(NULL, 0, "%s%s", path, suffix);
        char *with_suffix;
        unlink(path);
        if (needed < 0) {
            cu_die("failed to build temporary path");
        }
        with_suffix = cu_xmalloc((size_t)needed + 1);
        snprintf(with_suffix, (size_t)needed + 1, "%s%s", path, suffix);
        free(path);
        path = with_suffix;
        fd = open(path, O_CREAT | O_EXCL | O_WRONLY, 0600);
        if (fd < 0) {
            cu_die_errno("temporary create failed");
        }
        close(fd);
    }
    return path;
}

static void write_text_file_0600(const char *path, const char *text) {
    int fd = open(path, O_WRONLY | O_TRUNC);
    FILE *f;

    if (fd < 0) {
        cu_die_errno(path);
    }
    if (fchmod(fd, 0600) != 0) {
        close(fd);
        cu_die_errno("fchmod failed");
    }
    f = fdopen(fd, "wb");
    if (!f) {
        close(fd);
        cu_die_errno("fdopen failed");
    }
    cu_write_all(f, text, strlen(text));
    if (fclose(f) != 0) {
        cu_die_errno(path);
    }
}

static int run_curl(const char *config_path) {
    pid_t pid = fork();
    int status;

    if (pid < 0) {
        cu_die_errno("fork failed");
    }
    if (pid == 0) {
        execlp("curl", "curl", "--config", config_path, (char *)NULL);
        _exit(127);
    }
    for (;;) {
        if (waitpid(pid, &status, 0) >= 0) {
            break;
        }
        if (errno != EINTR) {
            cu_die_errno("waitpid failed");
        }
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        fprintf(stderr, "foc_imagegen: curl terminated by signal %d\n", WTERMSIG(status));
    }
    return 1;
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static char *extract_json_string(const char *json, const char *key) {
    char pattern[128];
    const char *p;
    char *out;
    size_t len = 0;
    size_t cap = 4096;

    if (snprintf(pattern, sizeof(pattern), "\"%s\"", key) >= (int)sizeof(pattern)) {
        cu_die("JSON key too long");
    }
    p = strstr(json, pattern);
    if (!p) {
        return NULL;
    }
    p += strlen(pattern);
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p++ != ':') return NULL;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p++ != '"') return NULL;

    out = cu_xmalloc(cap);
    while (*p) {
        unsigned char c = (unsigned char)*p++;
        if (c == '"') {
            out[len] = '\0';
            return out;
        }
        if (c == '\\') {
            c = (unsigned char)*p++;
            switch (c) {
            case '"': break;
            case '\\': break;
            case '/': break;
            case 'b': c = '\b'; break;
            case 'f': c = '\f'; break;
            case 'n': c = '\n'; break;
            case 'r': c = '\r'; break;
            case 't': c = '\t'; break;
            case 'u': {
                int v0 = hex_value(p[0]);
                int v1 = hex_value(p[1]);
                int v2 = hex_value(p[2]);
                int v3 = hex_value(p[3]);
                if (v0 < 0 || v1 < 0 || v2 < 0 || v3 < 0) {
                    free(out);
                    return NULL;
                }
                int code = (v0 << 12) | (v1 << 8) | (v2 << 4) | v3;
                p += 4;
                c = code < 128 ? (unsigned char)code : '?';
                break;
            }
            default:
                free(out);
                return NULL;
            }
        }
        if (len + 2 > cap) {
            cap *= 2;
            out = cu_xrealloc(out, cap);
        }
        out[len++] = (char)c;
    }
    free(out);
    return NULL;
}

static int b64_value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static unsigned char *base64_decode(const char *s, size_t *out_len) {
    size_t slen = strlen(s);
    size_t cap = (slen / 4 + 1) * 3;
    unsigned char *out = cu_xmalloc(cap);
    size_t len = 0;
    int vals[4];
    int n = 0;

    for (size_t i = 0; i < slen; i++) {
        char c = s[i];
        int v;

        if (isspace((unsigned char)c)) {
            continue;
        }
        if (c == '=') {
            vals[n++] = -2;
        } else {
            v = b64_value(c);
            if (v < 0) {
                free(out);
                cu_die("invalid base64 in API response");
            }
            vals[n++] = v;
        }

        if (n == 4) {
            if (vals[0] < 0 || vals[1] < 0) {
                free(out);
                cu_die("invalid base64 padding");
            }
            out[len++] = (unsigned char)((vals[0] << 2) | (vals[1] >> 4));
            if (vals[2] != -2) {
                if (vals[2] < 0) {
                    free(out);
                    cu_die("invalid base64 padding");
                }
                out[len++] = (unsigned char)(((vals[1] & 15) << 4) | (vals[2] >> 2));
                if (vals[3] != -2) {
                    if (vals[3] < 0) {
                        free(out);
                        cu_die("invalid base64 padding");
                    }
                    out[len++] = (unsigned char)(((vals[2] & 3) << 6) | vals[3]);
                }
            }
            n = 0;
        }
    }
    if (n != 0) {
        free(out);
        cu_die("truncated base64 in API response");
    }
    *out_len = len;
    return out;
}

static void validate_png_header(const unsigned char *data, size_t len) {
    static const unsigned char sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (len < 24 || memcmp(data, sig, sizeof(sig)) != 0) {
        cu_die("API response did not decode to a PNG");
    }
}

static void print_png_dimensions(const unsigned char *data, size_t len) {
    unsigned width;
    unsigned height;

    if (len < 24) {
        return;
    }
    width = ((unsigned)data[16] << 24) | ((unsigned)data[17] << 16) |
            ((unsigned)data[18] << 8) | (unsigned)data[19];
    height = ((unsigned)data[20] << 24) | ((unsigned)data[21] << 16) |
             ((unsigned)data[22] << 8) | (unsigned)data[23];
    fprintf(stderr, "foc_imagegen: wrote native PNG %ux%u\n", width, height);
}

static void cleanup_temp_paths(const char *request_path, const char *response_path,
                               const char *config_path) {
    if (request_path) {
        unlink(request_path);
    }
    if (response_path) {
        unlink(response_path);
    }
    if (config_path) {
        unlink(config_path);
    }
}

int main(int argc, char **argv) {
    const char *model = "gpt-image-2";
    const char *prompt = NULL;
    const char *prompt_file = NULL;
    const char *out_path = NULL;
    const char *size = "2048x1152";
    const char *quality = "high";
    const char *format = "png";
    bool force = false;
    bool dry_run = false;
    char *prompt_owned = NULL;

    cu_set_program_name("foc_imagegen");

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            usage(stdout);
            return 0;
        } else if (strcmp(arg, "--model") == 0 && i + 1 < argc) {
            model = argv[++i];
        } else if (strcmp(arg, "--prompt") == 0 && i + 1 < argc) {
            prompt = argv[++i];
        } else if (strcmp(arg, "--prompt-file") == 0 && i + 1 < argc) {
            prompt_file = argv[++i];
        } else if (strcmp(arg, "--out") == 0 && i + 1 < argc) {
            out_path = argv[++i];
        } else if (strcmp(arg, "--size") == 0 && i + 1 < argc) {
            size = argv[++i];
        } else if (strcmp(arg, "--quality") == 0 && i + 1 < argc) {
            quality = argv[++i];
        } else if (strcmp(arg, "--output-format") == 0 && i + 1 < argc) {
            format = argv[++i];
        } else if (strcmp(arg, "--force") == 0) {
            force = true;
        } else if (strcmp(arg, "--dry-run") == 0) {
            dry_run = true;
        } else {
            fprintf(stderr, "foc_imagegen: unknown or incomplete argument: %s\n", arg);
            usage(stderr);
            return 2;
        }
    }

    if (prompt && prompt_file) {
        cu_die("use --prompt or --prompt-file, not both");
    }
    if (prompt_file) {
        CuBuffer b = cu_read_file(prompt_file);
        prompt_owned = b.data;
        prompt = prompt_owned;
    }
    if (!prompt || !*prompt) {
        cu_die("missing --prompt or --prompt-file");
    }
    if (!out_path || !*out_path) {
        cu_die("missing --out");
    }
    if (!force && cu_file_exists(out_path)) {
        cu_die("output exists; pass --force to overwrite");
    }

    char *request_json = make_request_json(model, prompt, size, quality, format);
    if (dry_run) {
        fputs(request_json, stdout);
        free(request_json);
        free(prompt_owned);
        return 0;
    }

    const char *api_key = getenv("OPENAI_API_KEY");
    if (!api_key || !*api_key) {
        cu_die("OPENAI_API_KEY is not set");
    }

    char *request_path = make_temp_path(".json");
    char *response_path = make_temp_path(".json");
    char *config_path = make_temp_path(".curl");
    char *escaped_request_path = cu_json_escape_alloc(request_path);
    char *escaped_response_path = cu_json_escape_alloc(response_path);
    char *escaped_api_key = cu_json_escape_alloc(api_key);
    int needed = snprintf(NULL, 0,
                          "url = \"https://api.openai.com/v1/images/generations\"\n"
                          "request = \"POST\"\n"
                          "header = \"Authorization: Bearer %s\"\n"
                          "header = \"Content-Type: application/json\"\n"
                          "data-binary = \"@%s\"\n"
                          "output = \"%s\"\n"
                          "silent\n"
                          "show-error\n"
                          "fail-with-body\n",
                          escaped_api_key, escaped_request_path, escaped_response_path);
    if (needed < 0) {
        cu_die("failed to build curl config");
    }
    char *config = cu_xmalloc((size_t)needed + 1);
    snprintf(config, (size_t)needed + 1,
             "url = \"https://api.openai.com/v1/images/generations\"\n"
             "request = \"POST\"\n"
             "header = \"Authorization: Bearer %s\"\n"
             "header = \"Content-Type: application/json\"\n"
             "data-binary = \"@%s\"\n"
             "output = \"%s\"\n"
             "silent\n"
             "show-error\n"
             "fail-with-body\n",
             escaped_api_key, escaped_request_path, escaped_response_path);

    write_text_file_0600(request_path, request_json);
    write_text_file_0600(config_path, config);
    fprintf(stderr, "foc_imagegen: calling Image API for native %s %s\n", size, format);

    int curl_code = run_curl(config_path);
    CuBuffer response = cu_read_file(response_path);
    if (curl_code != 0) {
        fprintf(stderr, "foc_imagegen: curl failed with exit code %d\n", curl_code);
        if (response.len) {
            fprintf(stderr, "%s\n", response.data);
        }
        cleanup_temp_paths(request_path, response_path, config_path);
        return 1;
    }

    char *b64 = extract_json_string(response.data, "b64_json");
    if (!b64) {
        fprintf(stderr, "foc_imagegen: no b64_json in API response\n");
        if (response.len) {
            fprintf(stderr, "%s\n", response.data);
        }
        cleanup_temp_paths(request_path, response_path, config_path);
        return 1;
    }
    size_t png_len = 0;
    unsigned char *png = base64_decode(b64, &png_len);
    validate_png_header(png, png_len);
    cu_write_file_bytes(out_path, png, png_len);
    print_png_dimensions(png, png_len);

    cleanup_temp_paths(request_path, response_path, config_path);
    free(request_path);
    free(response_path);
    free(config_path);
    free(escaped_request_path);
    free(escaped_response_path);
    free(escaped_api_key);
    free(config);
    free(request_json);
    free(response.data);
    free(b64);
    free(png);
    free(prompt_owned);
    return 0;
}
