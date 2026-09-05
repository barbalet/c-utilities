# foc c-utilities

Small GCC-buildable helpers for the `Field of Chaos` rendering workflow. The goal is to keep repeated script, manifest, frame, and PNG checks in fast local C rather than short Python scripts.

## Build

```sh
make -C media png
```

This builds two binaries:

```sh
media/bin/foc_prepare
media/bin/foc_imagegen
```

## Commands

Generate or preserve the character list:

```sh
media/bin/foc_prepare characters ../foc_script.txt ../foc_characters.txt
```

Create or refresh the frame manifest:

```sh
media/bin/foc_prepare manifest ../foc_script.txt "../foc_script PNGs" "../foc_script PNGs/frame_manifest.json"
```

Show render progress and missing frame ranges:

```sh
media/bin/foc_prepare status ../foc_script.txt "../foc_script PNGs"
```

Print the next missing frame, speaker, and text:

```sh
media/bin/foc_prepare next ../foc_script.txt "../foc_script PNGs"
```

Audit saved PNG dimensions without calling Python or `sips` in a loop:

```sh
media/bin/foc_prepare audit-png "../foc_script PNGs" 436 1920 1080
```

Write an ffmpeg concat demuxer list with fixed frame duration:

```sh
media/bin/foc_prepare concat "../foc_script PNGs" 436 2.0 "../foc_script PNGs/frames_fixed_2s.ffconcat"
```

Create a JSONL render plan with frame paths, speaker text, previous-frame path, and matching character reference PNGs:

```sh
media/bin/foc_prepare prompt-plan ../foc_script.txt ../foc_characters "../foc_script PNGs" "../foc_script PNGs/prompt_plan.jsonl"
```

Verify that every `Name` in `foc_characters.txt` has a matching `Name.png`, and report extra PNGs:

```sh
media/bin/foc_prepare verify-characters ../foc_characters.txt ../foc_characters
```

Create the expanded multi-frame render plan from the base audio timing JSON. Run this from the `jackson` folder so the generated JSON contains project-relative paths:

```sh
c-utilities/media/bin/foc_prepare expand-json \
  foc_script.keyframes_only.json \
  "foc_script PNGs" \
  foc_characters \
  "foc_script expanded PNGs" \
  foc_script.json
```

Copy the existing one-PNG-per-line keyframes into their expanded global frame-number positions:

```sh
c-utilities/media/bin/foc_prepare renumber-keyframes \
  foc_script.keyframes_only.json \
  "foc_script PNGs" \
  "foc_script expanded PNGs"
```

Materialize every missing expanded PNG as a real file by copying that line's keyframe. Existing expanded files are skipped, so AI-rendered replacements are preserved:

```sh
c-utilities/media/bin/foc_prepare fill-expanded-copies \
  foc_script.keyframes_only.json \
  "foc_script PNGs" \
  "foc_script expanded PNGs"
```

## Notes

- Frame filenames use `frame_%03d_source_line_%03d.png`.
- Expanded keyframe filenames use `frame_%06d_source_line_%03d_part_01_keyframe.png`.
- Expanded pending frame filenames use `frame_%06d_source_line_%03d_part_%02d.png`.
- `expand-json` assigns 6 to 20 additional frames per script line using `max(ceil(segment_duration_seconds / 7), ceil(word_count / 45))`, clamped to that range.
- `fill-expanded-copies` is a fast placeholder/materialization pass. It creates every missing expanded PNG by copying the source line keyframe, but it does not overwrite already-rendered expanded PNGs.
- Character filenames use the exact name before the comma in `foc_characters.txt`, plus `.png`.
- PNG dimension auditing reads the PNG IHDR header only; it does not decode image pixels.
- `foc_prepare` prepares and verifies the render workflow. `foc_imagegen` calls the image-generation API through curl.
