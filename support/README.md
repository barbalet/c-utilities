# Shared support code

- `cutil.c` / `cutil.h`: allocation, files, directories, strings, JSON escaping,
  and fatal errors, used by the Bob and PNG tools.
- `foc_common.c` / `foc_common.h`: script parsing, chunks, manifests, audio
  formats, and cache helpers, used by the Field of Chaos audio tools.

The media Makefile builds these modules once and links them into their consumers.
The audio helper module retains the license in `../media/AUDIO_LICENSE`.

## Self-contained JSON support

`json_compat.c` / `json_compat.h` implement the 18 json-c-style functions used by
`aa_render_c`, using standard C only. Compile the source into a program and
include the header; neither json-c nor cJSON is required.

The module provides objects, arrays, strings, numbers, booleans and null,
reference-counted ownership, file parsing, and compact or indented output.
Parsing validates JSON syntax and UTF-8, decodes Unicode surrogate pairs, and
preserves embedded NULs and parsed number spelling when serializing. Duplicate
object keys replace earlier values. Shared child references survive release of
their original parent.

This is the renderer's API subset, not the full json-c API/ABI. Parsing rejects
comments, trailing data, invalid Unicode and numbers outside the finite double
range. Nesting is limited to 128 levels; cyclic object graphs are unsupported.
Numeric getters use double precision and the integer getter clamps to `int`.
Constructed nonfinite numbers serialize as null. Formatting may differ from
json-c while retaining JSON values. Allocation failure exits with a diagnostic.
The header documents ownership and parse-error behavior.

Run `make -C media test-json` from the repository root for regression tests.
