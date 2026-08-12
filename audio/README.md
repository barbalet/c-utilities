# FOC C Utilities

Native C utilities for the Field of Chaos text/audio pipeline.

These tools replace Python for fast script/audio plumbing:

- `foc_validate`: validate `Speaker  :  text` script structure.
- `foc_speakers`: count speaker labels, words, and characters.
- `foc_termscan`: scan for blocked or suspicious terms.
- `foc_jsonl`: export script segments as JSONL.
- `foc_plan`: inspect contiguous cached TTS WAV segments and estimate what remains for a duration target.
- `foc_wav_info`: inspect PCM WAV cache files.
- `foc_aiff_info`: inspect generated AIFF files.
- `foc_assemble_cached`: assemble cached PCM WAV segments into an AIFF and timing JSON.
- `foc_assemble_manifest`: assemble exact validated cache paths from a renderer manifest into an AIFF and timing JSON.
- `foc_chunk_plan`: report Chatterbox-style chunk counts for long script lines.
- `foc_chunk_jsonl`: export a backend-neutral JSONL TTS chunk queue.
- `foc_manifest_check`: verify renderer manifest cache WAV paths and audio format.
- `foc_duration_report`: print cached segment durations and cumulative timing.

Build:

```sh
make -C c_utilities
```

Example:

```sh
c_utilities/bin/foc_plan text/foc_script_txt/foc_script.txt .cache/chatterbox/foc_script_segments 600 0.18
```

Important boundary: these tools do not perform neural TTS inference. Chatterbox
voice synthesis is still the slow model backend until replaced with a native
Core ML, Metal, ONNX, or other compiled inference path. These utilities keep the
script parsing, timing, cache inspection, and AIFF assembly backend-neutral.
