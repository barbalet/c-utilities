#include "json_compat.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct json_object *parse(const char *text) {
    struct json_object *object = json_tokener_parse(text);
    assert(errno == 0);
    return object;
}
static void invalid(const char *text) {
    struct json_object *object = json_tokener_parse(text);
    assert(!object && errno == EINVAL);
}
static void ownership(void) {
    struct json_object *plan = parse("{\"start\":1.25,\"details\":{\"status\":\"pending\"}}");
    struct json_object *progress = json_object_new_object();
    struct json_object *shared = json_object_object_get(plan, "details");
    assert(json_object_object_add(progress, "details", json_object_get(shared)) == 0);
    assert(json_object_object_add(shared, "status", json_object_new_string("complete")) == 0);
    assert(json_object_object_get(progress, "details") == shared);
    json_object_put(plan); /* The retained child must survive its original parent. */
    assert(!strcmp(json_object_get_string(json_object_object_get(shared, "status")), "complete"));
    struct json_object *saved = json_object_get(shared);
    assert(json_object_object_add(progress, "details", json_object_new_int(7)) == 0);
    assert(!strcmp(json_object_get_string(json_object_object_get(saved, "status")), "complete"));
    json_object_put(saved);
    json_object_put(progress);

    struct json_object *array = json_object_new_array();
    struct json_object *text = json_object_new_string("retained");
    assert(json_object_array_add(array, text) == 0);
    assert(json_object_array_add(array, json_object_get(text)) == 0);
    assert(json_object_array_add(array, NULL) == 0);
    assert(json_object_array_length(array) == 3);
    assert(json_object_array_get_idx(array, 0) == json_object_array_get_idx(array, 1));
    assert(!json_object_array_get_idx(array, (size_t)-1));
    assert(json_object_array_add(array, array) == -1);
    json_object_put(array);
}
static void strings_and_numbers(void) {
    const char *input = "{\"text\":\"quote: \\\" slash: \\\\ \\/ \\b\\f\\n\\r\\t \\u00e9 \\ud83d\\ude00\","
                        "\"nul\":\"a\\u0000b\",\"a\\u0000b\":1,\"a\":2,"
                        "\"number\":-12.5e+2,\"big\":18446744073709551615,\"bool\":true,\"null\":null}";
    struct json_object *object = parse(input);
    assert(json_object_get_double(json_object_object_get(object, "number")) == -1250);
    assert(json_object_get_int(json_object_object_get(object, "bool")) == 1);
    assert(json_object_get_int(json_object_object_get(object, "a")) == 2);
    const char *text = json_object_to_json_string_ext(object, JSON_C_TO_STRING_PRETTY);
    assert(strstr(text, "a\\u0000b") && strstr(text, "18446744073709551615"));
    struct json_object *copy = parse(text);
    assert(!strcmp(json_object_get_string(json_object_object_get(object, "text")),
                   json_object_get_string(json_object_object_get(copy, "text"))));
    json_object_put(copy);
    json_object_put(object);
    object = parse("{\"x\":1,\"x\":2,\"X\":3}");
    assert(!strcmp(json_object_to_json_string_ext(object, 0), "{\"x\":2,\"X\":3}"));
    json_object_put(object);
    object = json_object_new_double(INFINITY);
    assert(json_object_get_int(object) == INT_MAX);
    assert(!strcmp(json_object_to_json_string_ext(object, 0), "null"));
    json_object_put(object);
    object = json_object_new_double(NAN);
    assert(json_object_get_int(object) == 0);
    json_object_put(object);
    assert(!parse("null"));
    assert(!strcmp(json_object_to_json_string_ext(NULL, 0), "null"));

    const char *locales[] = {"de_DE.UTF-8", "fr_FR.UTF-8", "C"};
    for (size_t i = 0; i < sizeof(locales)/sizeof(locales[0]); i++) {
        if (!setlocale(LC_NUMERIC, locales[i])) continue;
        object = parse("1.25");
        assert(json_object_get_double(object) == 1.25);
        json_object_put(object);
        object = json_object_new_double(1.25);
        assert(!strcmp(json_object_to_json_string_ext(object, 0), "1.25"));
        json_object_put(object);
    }
    setlocale(LC_NUMERIC, "C");
}
static void malformed(void) {
    const char *bad[] = {"", " ", "{", "[", "[1,]", "{\"x\":1,}", "{x:1}",
        "{\"x\" 1}", "[1 2]", "01", "-01", "+1", "1.", ".1", "1e", "1e+", "--1",
        "true false", "nullx", "NaN", "Infinity", "1e9999", "/*x*/{}", "\v{}",
        "\"unterminated", "\"\\x00\"", "\"\\u123\"", "\"\\ud800\"", "\"\\udc00\"",
        "\"\\ud800\\u1234\"", "\"\n\"", "\"\300\200\"", "\"\355\240\200\"",
        "\"\364\220\200\200\"", "\"\342\202\""};
    for (size_t i = 0; i < sizeof(bad)/sizeof(bad[0]); i++) invalid(bad[i]);
    char deep[600];
    memset(deep, '[', 200); memset(deep + 200, ']', 200); deep[400] = 0;
    invalid(deep);
    memset(deep, '[', 128); memset(deep + 128, ']', 128); deep[256] = 0;
    struct json_object *object = parse(deep);
    json_object_put(object);
}
int main(int argc, char **argv) {
    /* File mode also serves differential checks against another JSON parser. */
    if (argc == 2) {
        struct json_object *object = json_object_from_file(argv[1]);
        if (errno) return 2;
        puts(json_object_to_json_string_ext(object, JSON_C_TO_STRING_PLAIN));
        json_object_put(object);
        return 0;
    }
    ownership();
    strings_and_numbers();
    malformed();
    puts("json_compat: ownership, Unicode, numbers, locale, and malformed-input tests passed");
    return 0;
}
