# Native AA rendering utility

`aa_render_c` replaces the custom Python orchestration used by the earlier AA
renders. It is written in C17 and compiled with `gcc -O3`.

The utility handles:

- MP3 inventory, duration probing, and durable source copying
- native `whisper.cpp` transcription orchestration
- speech-only SRT and timed-script generation
- one-frame-per-10-seconds photorealistic render-plan generation
- accepted PNG normalization and permanent progress/checksum registration
- the deterministic final-minute card
- H.264/AAC MP4 assembly through `ffmpeg`
- `ffprobe`, full-decode, and checksum verification

It intentionally does not replace the image-generation model: doing so would
reduce photorealistic quality. Image generation remains the built-in high-quality
service, while every local pipeline step is native.

Build and smoke-test:

```sh
make -C media aa test
```

All episode state and large outputs are written beneath:

```text
/Volumes/500GB HDD/AA renders
```

The three current recordings require 2,004 total 1920x1080 frames:

- `aa_080517`: 610
- `aa_081917`: 701
- `aa_082617`: 693

Build commands above run from the repository root. The executable is
`media/bin/aa_render_c`. Building requires OpenSSL. JSON parsing and writing are implemented locally in
`support/json_compat.[ch]`, without a JSON library dependency.
Set `AA_INVENTORY_DIR` on the make command to test inventory against recordings;
the default smoke test uses an empty temporary directory.
