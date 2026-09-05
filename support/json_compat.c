#include "json_compat.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

enum json_kind { OBJECT, ARRAY, STRING, NUMBER, BOOLEAN };
struct member { char *key; size_t key_length; struct json_object *value; };
struct json_object {
    enum json_kind kind;
    size_t references;
    double number;
    char *string;
    size_t string_length;
    char *number_text;
    char *serialized;
    struct member *members;
    size_t count, capacity;
};

static void json_die(const char *message) {
    fprintf(stderr, "json_compat: %s\n", message);
    exit(EXIT_FAILURE);
}
static void *xmalloc(size_t size) {
    void *result = malloc(size ? size : 1);
    if (!result) json_die("out of memory");
    return result;
}
static void *xrealloc(void *pointer, size_t size) {
    void *result = realloc(pointer, size ? size : 1);
    if (!result) json_die("out of memory");
    return result;
}
static char *copy_bytes(const char *text, size_t length) {
    if (length == SIZE_MAX) json_die("string too large");
    char *result = xmalloc(length + 1);
    memcpy(result, text, length);
    result[length] = '\0';
    return result;
}

/* Allocation failures are fatal; malformed input is reported with EINVAL. */
static struct json_object *new_value(enum json_kind kind) {
    struct json_object *object = xmalloc(sizeof(*object));
    memset(object, 0, sizeof(*object));
    object->kind = kind;
    object->references = 1;
    return object;
}

struct json_object *json_object_new_object(void) { return new_value(OBJECT); }
struct json_object *json_object_new_array(void) { return new_value(ARRAY); }
struct json_object *json_object_new_double(double value) {
    struct json_object *object = new_value(NUMBER);
    object->number = value;
    return object;
}
struct json_object *json_object_new_int(int value) { return json_object_new_double(value); }
struct json_object *json_object_new_string(const char *value) {
    if (!value) return NULL;
    struct json_object *object = new_value(STRING);
    object->string_length = strlen(value);
    object->string = copy_bytes(value, object->string_length);
    return object;
}
struct json_object *json_object_get(struct json_object *object) {
    if (object) {
        if (object->references == SIZE_MAX) json_die("JSON reference count overflow");
        object->references++;
    }
    return object;
}
int json_object_put(struct json_object *object) {
    if (!object || --object->references) return 0;
    for (size_t i = 0; i < object->count; i++) {
        free(object->members[i].key);
        json_object_put(object->members[i].value);
    }
    free(object->members);
    free(object->string);
    free(object->number_text);
    free(object->serialized);
    free(object);
    return 1;
}

static void append(struct json_object *object, const char *key, size_t length, struct json_object *value) {
    if (object->count == object->capacity) {
        if (object->capacity > SIZE_MAX / 2 / sizeof(struct member))
            json_die("JSON collection too large");
        object->capacity = object->capacity ? object->capacity * 2 : 8;
        object->members = xrealloc(object->members, object->capacity * sizeof(struct member));
    }
    object->members[object->count++] = (struct member){key ? copy_bytes(key, length) : NULL, length, value};
}
static int object_add(struct json_object *object, const char *key, size_t length, struct json_object *value) {
    if (!object || object->kind != OBJECT || !key || object == value) return -1;
    for (size_t i = 0; i < object->count; i++) {
        if (object->members[i].key_length == length && !memcmp(object->members[i].key, key, length)) {
            json_object_put(object->members[i].value);
            object->members[i].value = value;
            return 0;
        }
    }
    append(object, key, length, value);
    return 0;
}
int json_object_object_add(struct json_object *object, const char *key, struct json_object *value) {
    return key ? object_add(object, key, strlen(key), value) : -1;
}
struct json_object *json_object_object_get(const struct json_object *object, const char *key) {
    if (!object || object->kind != OBJECT || !key) return NULL;
    for (size_t i = 0; i < object->count; i++)
        if (object->members[i].key_length == strlen(key) &&
            !memcmp(object->members[i].key, key, strlen(key))) return object->members[i].value;
    return NULL;
}
int json_object_array_add(struct json_object *array, struct json_object *value) {
    if (!array || array->kind != ARRAY || array == value) return -1;
    append(array, NULL, 0, value);
    return 0;
}
struct json_object *json_object_array_get_idx(const struct json_object *array, size_t index) {
    return array && array->kind == ARRAY && index < array->count ? array->members[index].value : NULL;
}
size_t json_object_array_length(const struct json_object *array) {
    return array && array->kind == ARRAY ? array->count : 0;
}
double json_object_get_double(const struct json_object *object) {
    if (!object) return 0;
    if (object->kind == NUMBER || object->kind == BOOLEAN) return object->number;
    if (object->kind == STRING) return strtod(object->string, NULL);
    return 0;
}
int json_object_get_int(const struct json_object *object) {
    double value = json_object_get_double(object);
    if (isnan(value)) return 0;
    if (value >= INT_MAX) return INT_MAX;
    if (value <= INT_MIN) return INT_MIN;
    return (int)value;
}


struct buffer { char *data; size_t length, capacity; };
static void write_bytes(struct buffer *out, const char *text, size_t length) {
    if (length > SIZE_MAX - out->length - 1) json_die("JSON output too large");
    size_t needed = out->length + length + 1;
    if (needed > out->capacity) {
        size_t cap = out->capacity ? out->capacity : 64;
        while (cap < needed) {
            if (cap > SIZE_MAX / 2) { cap = needed; break; }
            cap *= 2;
        }
        out->data = xrealloc(out->data, cap);
        out->capacity = cap;
    }
    memcpy(out->data + out->length, text, length);
    out->length += length;
    out->data[out->length] = '\0';
}
static void write_char(struct buffer *out, char value) { write_bytes(out, &value, 1); }

/* Validate a UTF-8 sequence, excluding overlong encodings and surrogates. */
static size_t utf8_length(const unsigned char *text, size_t remaining) {
    if (!remaining) return 0;
    unsigned first = text[0], code;
    size_t length;
    if (first < 0x80) return 1;
    if (first >= 0xc2 && first <= 0xdf) { length = 2; code = first & 31; }
    else if (first >= 0xe0 && first <= 0xef) { length = 3; code = first & 15; }
    else if (first >= 0xf0 && first <= 0xf4) { length = 4; code = first & 7; }
    else return 0;
    if (remaining < length) return 0;
    for (size_t i = 1; i < length; i++) {
        if ((text[i] & 0xc0) != 0x80) return 0;
        code = (code << 6) | (text[i] & 63);
    }
    if ((length == 3 && code < 0x800) || (length == 4 && code < 0x10000) ||
        code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) return 0;
    return length;
}
static void write_string(struct buffer *out, const char *text, size_t length) {
    static const char hex[] = "0123456789abcdef";
    write_char(out, '"');
    for (size_t i = 0; i < length; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c == '"' || c == '\\') { write_char(out, '\\'); write_char(out, (char)c); }
        else if (c < 32) {
            char escape[] = {'\\', 'u', '0', '0', hex[c >> 4], hex[c & 15]};
            write_bytes(out, escape, sizeof(escape));
        } else if (c >= 128) {
            size_t count = utf8_length((const unsigned char *)text + i, length - i);
            if (!count) json_die("invalid UTF-8 string");
            write_bytes(out, text + i, count);
            i += count - 1;
        } else write_char(out, (char)c);
    }
    write_char(out, '"');
}
static void indent(struct buffer *out, unsigned depth) {
    write_char(out, '\n');
    for (unsigned i = 0; i < depth; i++) write_bytes(out, "  ", 2);
}
static void serialize(struct buffer *out, const struct json_object *object, int pretty, unsigned depth) {
    if (depth > 128) json_die("JSON nesting exceeds 128 levels");
    if (!object) { write_bytes(out, "null", 4); return; }
    switch (object->kind) {
    case STRING: write_string(out, object->string, object->string_length); break;
    case BOOLEAN: write_bytes(out, object->number ? "true" : "false", object->number ? 4 : 5); break;
    case NUMBER:
        if (object->number_text) write_bytes(out, object->number_text, strlen(object->number_text));
        else if (!isfinite(object->number)) write_bytes(out, "null", 4);
        else {
            char number[128];
            int count = snprintf(number, sizeof(number), "%.17g", object->number);
            if (count < 0 || (size_t)count >= sizeof(number)) json_die("number formatting failed");
            /* JSON always uses '.', even when the application changes locale. */
            const char *decimal = localeconv()->decimal_point;
            char *point = *decimal ? strstr(number, decimal) : NULL;
            if (point && strcmp(decimal, ".")) {
                write_bytes(out, number, (size_t)(point - number));
                write_char(out, '.');
                point += strlen(decimal);
                write_bytes(out, point, strlen(point));
            } else write_bytes(out, number, (size_t)count);
        }
        break;
    case OBJECT: case ARRAY:
        write_char(out, object->kind == OBJECT ? '{' : '[');
        for (size_t i = 0; i < object->count; i++) {
            if (i) write_char(out, ',');
            if (pretty) indent(out, depth + 1);
            if (object->kind == OBJECT) {
                write_string(out, object->members[i].key, object->members[i].key_length);
                write_bytes(out, pretty ? ": " : ":", pretty ? 2 : 1);
            }
            serialize(out, object->members[i].value, pretty, depth + 1);
        }
        if (pretty && object->count) indent(out, depth);
        write_char(out, object->kind == OBJECT ? '}' : ']');
        break;
    }
}
const char *json_object_to_json_string_ext(struct json_object *object, int flags) {
    if (!object) return "null";
    struct buffer out = {0};
    serialize(&out, object, flags & JSON_C_TO_STRING_PRETTY, 0);
    free(object->serialized);
    object->serialized = out.data;
    return out.data;
}
const char *json_object_get_string(struct json_object *object) {
    if (!object) return NULL;
    return object->kind == STRING ? object->string : json_object_to_json_string_ext(object, 0);
}

struct parser { const char *cursor, *end; int failed; };
static void whitespace(struct parser *p) {
    while (p->cursor < p->end && (*p->cursor == ' ' || *p->cursor == '\t' ||
           *p->cursor == '\r' || *p->cursor == '\n')) p->cursor++;
}
static int take(struct parser *p, char value) {
    if (p->cursor < p->end && *p->cursor == value) { p->cursor++; return 1; }
    return 0;
}
static int hex4(struct parser *p, unsigned *value) {
    *value = 0;
    for (int i = 0; i < 4; i++) {
        if (p->cursor == p->end) return 0;
        unsigned char c = (unsigned char)*p->cursor++;
        unsigned digit;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else return 0;
        *value = (*value << 4) | digit;
    }
    return 1;
}
static void write_codepoint(struct buffer *out, unsigned code) {
    if (code < 0x80) write_char(out, (char)code);
    else if (code < 0x800) {
        write_char(out, (char)(0xc0 | (code >> 6)));
        write_char(out, (char)(0x80 | (code & 63)));
    } else if (code < 0x10000) {
        write_char(out, (char)(0xe0 | (code >> 12)));
        write_char(out, (char)(0x80 | ((code >> 6) & 63)));
        write_char(out, (char)(0x80 | (code & 63)));
    } else {
        write_char(out, (char)(0xf0 | (code >> 18)));
        write_char(out, (char)(0x80 | ((code >> 12) & 63)));
        write_char(out, (char)(0x80 | ((code >> 6) & 63)));
        write_char(out, (char)(0x80 | (code & 63)));
    }
}
static char *parse_string(struct parser *p, size_t *length) {
    struct buffer out = {0};
    if (!take(p, '"')) goto fail;
    while (p->cursor < p->end) {
        unsigned char c = (unsigned char)*p->cursor++;
        if (c == '"') {
            write_bytes(&out, "", 0);
            *length = out.length;
            return out.data;
        }
        if (c < 32) goto fail;
        if (c == '\\') {
            if (p->cursor == p->end) goto fail;
            c = (unsigned char)*p->cursor++;
            switch (c) {
            case '"': case '\\': case '/': write_char(&out, (char)c); break;
            case 'b': write_char(&out, '\b'); break;
            case 'f': write_char(&out, '\f'); break;
            case 'n': write_char(&out, '\n'); break;
            case 'r': write_char(&out, '\r'); break;
            case 't': write_char(&out, '\t'); break;
            case 'u': {
                unsigned code, low;
                if (!hex4(p, &code)) goto fail;
                if (code >= 0xd800 && code <= 0xdbff) {
                    if (!take(p, '\\') || !take(p, 'u') || !hex4(p, &low) ||
                        low < 0xdc00 || low > 0xdfff) goto fail;
                    code = 0x10000 + ((code - 0xd800) << 10) + (low - 0xdc00);
                } else if (code >= 0xdc00 && code <= 0xdfff) goto fail;
                write_codepoint(&out, code);
                break;
            }
            default: goto fail;
            }
        } else if (c >= 128) {
            const char *start = p->cursor - 1;
            size_t count = utf8_length((const unsigned char *)start, (size_t)(p->end - start));
            if (!count) goto fail;
            write_bytes(&out, start, count);
            p->cursor = start + count;
        } else write_char(&out, (char)c);
    }
fail:
    free(out.data);
    p->failed = 1;
    return NULL;
}
static int digit(const struct parser *p) {
    return p->cursor < p->end && *p->cursor >= '0' && *p->cursor <= '9';
}
static struct json_object *parse_number(struct parser *p) {
    const char *start = p->cursor;
    take(p, '-');
    if (!take(p, '0')) {
        if (!digit(p)) goto fail;
        while (digit(p)) p->cursor++;
    }
    if (take(p, '.')) {
        if (!digit(p)) goto fail;
        while (digit(p)) p->cursor++;
    }
    if (take(p, 'e') || take(p, 'E')) {
        if (!take(p, '+')) take(p, '-');
        if (!digit(p)) goto fail;
        while (digit(p)) p->cursor++;
    }
    char *token = copy_bytes(start, (size_t)(p->cursor - start));
    /* Convert using the current locale without changing global locale state. */
    struct buffer localized = {0};
    const char *point = strchr(token, '.');
    if (point) {
        write_bytes(&localized, token, (size_t)(point - token));
        const char *decimal = localeconv()->decimal_point;
        write_bytes(&localized, decimal, strlen(decimal));
        write_bytes(&localized, point + 1, strlen(point + 1));
    } else write_bytes(&localized, token, strlen(token));
    char *end;
    double number = strtod(localized.data, &end);
    int valid = *end == '\0' && isfinite(number);
    free(localized.data);
    if (!valid) { free(token); goto fail; }
    struct json_object *object = json_object_new_double(number);
    object->number_text = token; /* Preserve large integer and decimal spelling. */
    return object;
fail:
    p->failed = 1;
    return NULL;
}
static struct json_object *parse_value(struct parser *p, unsigned depth) {
    whitespace(p);
    if (depth > 128 || p->cursor == p->end) goto fail;
    char first = *p->cursor;
    if (first == '"') {
        struct json_object *object = new_value(STRING);
        object->string = parse_string(p, &object->string_length);
        if (p->failed) { json_object_put(object); return NULL; }
        return object;
    }
    if (first == '-' || digit(p)) return parse_number(p);
    if (first == '{' || first == '[') {
        p->cursor++;
        struct json_object *object = new_value(first == '{' ? OBJECT : ARRAY);
        char closing = first == '{' ? '}' : ']';
        whitespace(p);
        if (take(p, closing)) return object;
        for (;;) {
            char *key = NULL;
            size_t length = 0;
            if (first == '{') {
                whitespace(p);
                key = parse_string(p, &length);
                whitespace(p);
                if (p->failed || !take(p, ':')) {
                    free(key); json_object_put(object); goto fail;
                }
            }
            struct json_object *value = parse_value(p, depth + 1);
            if (p->failed) { free(key); json_object_put(object); return NULL; }
            if (first == '{') object_add(object, key, length, value);
            else json_object_array_add(object, value);
            free(key);
            whitespace(p);
            if (take(p, closing)) return object;
            if (!take(p, ',')) { json_object_put(object); goto fail; }
        }
    }
    const char *word = first == 't' ? "true" : first == 'f' ? "false" : first == 'n' ? "null" : NULL;
    if (word) {
        size_t length = strlen(word);
        if ((size_t)(p->end - p->cursor) < length || memcmp(p->cursor, word, length)) goto fail;
        p->cursor += length;
        if (first == 'n') return NULL;
        struct json_object *object = new_value(BOOLEAN);
        object->number = first == 't';
        return object;
    }
fail:
    p->failed = 1;
    return NULL;
}
static struct json_object *parse_document(const char *text, size_t length) {
    struct parser p = {text, text + length, 0};
    struct json_object *object = parse_value(&p, 0);
    whitespace(&p);
    if (p.failed || p.cursor != p.end) {
        json_object_put(object);
        errno = EINVAL;
        return NULL;
    }
    errno = 0; /* Distinguishes valid JSON null from parse failure. */
    return object;
}
struct json_object *json_tokener_parse(const char *text) {
    if (!text) { errno = EINVAL; return NULL; }
    return parse_document(text, strlen(text));
}
struct json_object *json_object_from_file(const char *path) {
    if (!path) { errno = EINVAL; return NULL; }
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    struct buffer input = {0};
    char block[4096];
    size_t count;
    while ((count = fread(block, 1, sizeof(block), file)) != 0) write_bytes(&input, block, count);
    if (ferror(file)) {
        int saved = errno ? errno : EIO;
        fclose(file); free(input.data); errno = saved;
        return NULL;
    }
    if (fclose(file)) { free(input.data); return NULL; }
    struct json_object *object = parse_document(input.data ? input.data : "", input.length);
    free(input.data);
    return object;
}
