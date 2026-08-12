#include "foc_common.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *defaults[] = {
    "brother", "brothers", "rap", "rapping", "rapper", "phat", "ain't",
    "mawler", "mawlers", "jazz", "am-do", "motherfucker", "black cancers",
    "one two three", "pumping it", "sound of rap", "kill all the motherfuckers",
    NULL
};

static char *lower_copy(const char *s) {
    size_t n = strlen(s), i;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)s[i]);
    out[n] = '\0';
    return out;
}

static int is_boundary_char(char c) {
    return !isalnum((unsigned char)c);
}

static int contains_term(const char *line, const char *term) {
    const int phrase = strchr(term, ' ') != NULL;
    const size_t n = strlen(term);
    const char *p;
    if (phrase) return strstr(line, term) != NULL;
    p = line;
    while ((p = strstr(p, term)) != NULL) {
        const char left = p == line ? '\0' : p[-1];
        const char right = p[n];
        if ((p == line || is_boundary_char(left)) && (right == '\0' || is_boundary_char(right))) return 1;
        p += n ? n : 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "text/foc_script_txt/foc_script.txt";
    FocScript script = {0};
    int hits = 0;
    size_t i;
    if (foc_load_script(path, &script, 0) != 0) return 2;
    for (i = 0; i < script.count; i++) {
        char *line = lower_copy(script.items[i].source_text);
        int t;
        if (!line) return 3;
        if (argc > 2) {
            int a;
            for (a = 2; a < argc; a++) {
                char *term = lower_copy(argv[a]);
                if (term && contains_term(line, term)) {
                    printf("%s:%d:%s:%s\n", path, script.items[i].source_line, argv[a], script.items[i].source_text);
                    hits++;
                }
                free(term);
            }
        } else {
            for (t = 0; defaults[t]; t++) {
                if (contains_term(line, defaults[t])) {
                    printf("%s:%d:%s:%s\n", path, script.items[i].source_line, defaults[t], script.items[i].source_text);
                    hits++;
                }
            }
        }
        free(line);
    }
    foc_free_script(&script);
    printf("hits=%d\n", hits);
    return hits ? 1 : 0;
}
