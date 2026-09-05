# c-utilities
A collection of C utilities: aligned memory allocation with multiple backends, error handling, dynamic arrays, clamp functions, OpenGL helpers and more.

---

## Modules

### aligned_memory
Portable C library for aligned memory allocation.  
Supports Windows, POSIX, C11 and fallback using standard malloc.
> `am_aligned_realloc(pointer, 0)` is **not** equivalent to `am_aligned_free(pointer)` - it returns `NULL` without releasing the memory block.  
> In C11 the size passed to `am_aligned_malloc` must be a multiple of alignment. The **library automatically rounds up** the size when using the C11 backend.  
> In C23 this requirement is removed. To avoid unnecessary overhead, the library does **not** round the size when compiled for C23 support. If your code relies on rounding, ensure the size is a multiple of alignment explicitly.  

### assert_m
Flexible assert macros:
- `assert_m(condition, message)` - assertion with a string message.  
- `assert_mf(condition, format, ...)` - assertion with format (requires C99 or GCC/clang extensions).
- `assert_check_m(condition, message)` - evaluates `condition` like an if check; in debug builds fails on false with an assertion message.   
- `assert_check_mf(condition, format, ...)` - same as `assert_check_m` with format (requires C99 or GCC/clang extensions).   
- `static_assert_m(condition, message)` - compile-time checks (requires C11 with a string literal or C89 but text will not be accurate).   
> `assert_m` and `assert_mf` are disabled when `NDEBUG` is defined.   
> `assert_check_m` and `assert_check_mf` are **not disabled** by `NDEBUG`; they still return condition's truth.   
> `static_assert_m` is **not** affected by `NDEBUG`.   

### clamp_values
Inline clamping functions.  
Supported types: int64_t, uint64_t, size_t, float, double, long double.  
For narrower integers (for example `int32_t`) you can safely cast to the corresponding supported type; otherwise an implicit conversion will occur, which isn't recommended.  

### cleanup_register
Register a cleanup function with optional argument to be called at program exit via `atexit`.  
- Supports **one** registered function with a single void * argument. Subsequent calls with a new function pointer are ignored.
- The **argument pointer** can be updated at any time by calling function without a function pointer.
- At least one of the pointers must be non-`NULL` (enforced in debug builds).
- Requires C99.

### exit_print
Print an error message to stderr and exit with `EXIT_FAILURE`.  
- `ep_exit_print(format_pointer, ...)` - prints a formatted message and exits.  
- `ep_exit_print_free(free_flag, format_pointer, ...)` - prints formatted message, frees `format_pointer` if `free_flag` is true and `format_pointer` is not `NULL` and then exit.  
`format_pointer` must be non-`NULL` (enforced in debug builds).  

### dynamic_array
Dynamic array utilities with automatic resizing and memory management.
- `da_dynamic_array_ensure_capacity` - ensures the array has at least `needed` capacity, growing by a factor of 1.5; if that allocation fails, it attempts to allocate exactly `needed` elements and if both allocations fail, returns `false`.
- `da_dynamic_array_ensure_capacity_list` - same as `da_dynamic_array_ensure_capacity` but operates on multiple dynamic arrays simultaneously (all resized to the same capacity) to simplify consistent resizing of related arrays. Requires an array of `DA_Dynamic_Array_List` structures, each specifying the data pointer and element size for the corresponding array.
- `da_dynamic_array_shrink` - shrinks capacity to fit `amount` elements (if `amount` smaller than `base_amount`, capacity is set to `base_amount`). If both `amount` and `base_amount` are `0`, the array is freed and capacity is zeroed.
- `da_dynamic_array_free` - frees the array and nullifies the pointer, size, and capacity.  
> All functions perform bounds checking and handle allocation failures gracefully.  
> Requires C99.

### write_out_error_message
Stack-like error message storage and retrieval.
- `woem_push` - formats and stores an error message;
- `woem_push_raw` - stores a pre-allocated error message;
- `woem_pop` - retrieves the most recent error message and removes it from storage; returns NULL if none exists and also returns flag indicating memory ownership. 
- `woem_shrink` - shrinks the dynamic storage to the exact number of messages (optimises memory usage).
- `woem_clear` - frees all stored messages and resets the storage.
> **Memory ownership**  
> - `woem_push` allocate memory internally.  
> - `woem_push_raw` takes ownership of the passed pointer.  
> - `woem_pop` returns a flag and a pointer, if the flag is `true` the caller is responsible for calling `free` on returned pointer.  
> - `woem_clear` frees all remaining messages.  

### gl_utils
OpenGL error checking.  
- `gl_errors_clean` - clear all pending OpenGL errors.  
- `gl_errors_check` - check for errors, returns `true` if none, otherwise stores errors via `woem_push`.  
- `gl_get_error_string` - returns a string description for a given OpenGL error code.  
> Requires OpenGL and GLEW.  

### gl_wrappers
Safe wrappers for all OpenGL functions that may set an error state; zero overhead in release builds.
- `glCreateShader_wrapped`
- `glShaderSource_wrapped`
- `glCompileShader_wrapped`
- `glCreateProgram_wrapped`
- `glAttachShader_wrapped`
- `glLinkProgram_wrapped`
- `glDeleteShader_wrapped`
- `glDeleteProgram_wrapped`
> Functions return `bool`. In release builds they always `true`; in debug they validate arguments and return result of OpenGL error checking.  
> Requires OpenGL and GLEW.  

### create_shader_program
Create OpenGL shader program from vertex and fragment sources.
- `csp_create_shader_program` - create a shader program from a single fragment and single vertex shader source code string.
- `csp_create_shader_program_many_sources` - creates a program from one or more vertex and fragment source strings.

### lcg_random
Fast and lightweight pseudo-random number generator with a minimal overhead, based on linear congruential algorithm and designed for unbiased statistical distribution.  
> **Deterministic by default** - seeds are fixed, change it via lcg_set_random64 and lcg_set_random32.  
> **Thread-unsafe** - has global seed states which are shared.  
> **Bounded random numbers** - function calls lcg_rand32_max(max) and lcg_rand64_max(max) returns values in range of [0; max - 1].  
> Requires C99.  

### safe_shift
Portable checked bitwise shifts with well-defined wrapping   
- `sa_safe_shl_<type>` - signed left shifts do a bitwise truncation.   
- `sa_safe_shr_<type>` - signed right shifts are sign-extending.   
> Return `true` if shift may not be safely performed (or if pointer is `NULL`), `false` on success.   
> Unsigned shifts are zero-fill. No undefined and implementation-defined behavior for signed types.   

### safe_\<operation>
Portable overflow-checked integer arithmetic operations without undefined or implementation-defined behavior. Includes addition, subtraction, multiplication, division, mathematical modulus, negation, absolute value, exponentiation.
- `sa_ovf_add_<type>`, `sa_ovf_sub_<type>`, `sa_ovf_mul_<type>` - compute on no overflow.   
- `sa_ovf_div_<type>` - checks for division by zero and potentional overflow.   
- `sa_math_mod_<type>` - compute mathematical module (negative result is impossible unlike built-in `%` operator).   
- `sa_ovf_neg_<type>` - negate a number.   
- `sa_ovf_abs_<type>` - take absolute value out.   
- `sa_ovf_pow_<type>` - fast exponentiation by squaring.   
> Return `true` if overflow occurred, `false` on success.   
> Use __builtin_<operation>_overflow or <stdckdint.h> if available otherwise portable manual check.   

### handle file
Portable safe file handling operations.   
- `hf_file_read` - reads a file into a heap-allocated buffer.   
- `hf_file_write` - writes data to a file replacing original.   
- `hf_file_append` - append data to the end of a file.   
- `hf_file_copy` - copies a file from source to destination.   
- `hf_file_rename_move` - renames or moves a file (with an additional cross-device copy fallback on POSIX).   
- `hf_file_delete` - deletes a file.   
- `hf_file_size_get` - retrieves the size of a file.   
- `hf_file_chunk_read` - read a chunk of data from a specific position.   
- `hf_file_chunk_write` - writes a chunk of data at a specific position.   
- `hf_file_is_exists` - check if a file exists.   
> Returns `0` on success, `-1` if file closure failed and `>0` for other errors.   
> Support Windows, POSIX and standard backends. On windows safely handles UTF-8 paths, absolute path resulotion, UNC paths and long paths exceeding the standard (`MAX_PATH`) maximal length.    
> Requires C99   

---

**License**: Apache 2.0
