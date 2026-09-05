#ifndef JSON_COMPAT_H
#define JSON_COMPAT_H

#include <stddef.h>

/* The json-c API subset used by aa_render_c, implemented using only standard C.
 * This is source compatibility, not a replacement for the full json-c ABI.
 * Constructors/parse return one owned reference. Lookups borrow references.
 * Successful add operations transfer one reference; get retains, put releases.
 * NULL represents JSON null. Cycles are not supported. Numeric getters use double; parsed number text is preserved.
 * Parsing is strict JSON with UTF-8 validation and a 128-level nesting limit.
 * Parse failure sets errno=EINVAL; valid JSON null returns NULL with errno=0.
 * Allocation failure is fatal. Nonfinite constructed numbers serialize as null.
 * Returned strings belong to the object. Serialized strings remain valid until
 * that same object is serialized again or destroyed. Not thread-safe.
 */
struct json_object;
#define JSON_C_TO_STRING_PLAIN 0
#define JSON_C_TO_STRING_PRETTY 2

struct json_object *json_object_new_object(void);
struct json_object *json_object_new_array(void);
struct json_object *json_object_new_int(int value);
struct json_object *json_object_new_double(double value);
struct json_object *json_object_new_string(const char *value);
struct json_object *json_object_get(struct json_object *object);
int json_object_put(struct json_object *object);
int json_object_object_add(struct json_object *object, const char *key, struct json_object *value);
struct json_object *json_object_object_get(const struct json_object *object, const char *key);
int json_object_array_add(struct json_object *array, struct json_object *value);
struct json_object *json_object_array_get_idx(const struct json_object *array, size_t index);
size_t json_object_array_length(const struct json_object *array);
int json_object_get_int(const struct json_object *object);
double json_object_get_double(const struct json_object *object);
const char *json_object_get_string(struct json_object *object);
const char *json_object_to_json_string_ext(struct json_object *object, int flags);
struct json_object *json_tokener_parse(const char *text);
struct json_object *json_object_from_file(const char *path);

#endif
