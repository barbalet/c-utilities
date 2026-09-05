# C Utilities

Native C tools for audio, transcripts, PNG planning, and media rendering.

## Layout

The repository has three top-level code directories:

- `media/`: all workflow executables, one Makefile, and workflow documentation.
- `support/`: shared local helpers (`cutil.[ch]` and `foc_common.[ch]`).
- `third_party/`: the separate Apache-2.0 EleisonScel library snapshot.

The previous `audio/`, `bob media utils/`, `png render utils/`, and
`aa_render_c/` sources are consolidated directly into `media/`. The previous
`common/` helpers and audio's `foc_common.[ch]` now live in `support/`.
Executable names and command arguments are preserved; all executables now live
in `media/bin/`.

## Build

From the repository root:

```sh
make -C media          # All 26 tools, including the AA renderer
make -C media core     # 25 audio, Bob, and PNG tools
make -C media audio    # Field of Chaos audio tools
make -C media bob      # Bob media tools
make -C media png      # foc_prepare and foc_imagegen
make -C media aa       # AA renderer
```

The core tools require a C compiler and standard/POSIX development headers.
The AA renderer additionally requires OpenSSL. Its JSON implementation is
self-contained in `support/json_compat.c` and `support/json_compat.h`; no JSON
library or pkg-config installation is required.
Its Makefile defaults `OPENSSL_PREFIX` to `/opt/homebrew/opt/openssl@3`;
override it for another installation.

```sh
make -C media test     # JSON tests and AA inventory smoke test
make -C media test-json # Standalone JSON implementation tests
make -C media test AA_INVENTORY_DIR=/path/to/recordings
make -C media clean    # Recursively remove compiled code throughout the repository
```

`foc-status` and `foc-finalize` remain available through `make -C media`.
Their `FOC_*` path settings are interpreted relative to `media/` when using
`-C media`; override them to point to your workflow data.

## Clean compiled files

From the repository root, run:

```sh
./clean.py --dry-run
./clean.py
```

The Python 3 script recursively removes `.o`/`.obj` files and recognizes compiled
executables, shared libraries, static archives, and WebAssembly by their file
headers. It preserves source, scripts, media assets, and version-control metadata;
it does not follow symbolic links. Its default root is the script's directory,
so it also works when invoked from another working directory. `make -C media clean`
runs the same script. Use `--root /path/to/tree` to choose another tree explicitly.

## Workflow documentation

- [Audio and TTS cache tools](media/AUDIO.md)
- [Bob media preparation](media/BOB.md)
- [PNG planning and image generation](media/PNG.md)
- [AA rendering](media/AA_RENDER_C.md)
- [Shared helpers](support/README.md)
- [Third-party provenance and license](third_party/eleisonscel-c-utilities/README.md)

The original audio license is preserved in `media/AUDIO_LICENSE`; it also
covers the relocated `support/foc_common.[ch]` files. The imported third-party
library retains its own license and internal layout.

Generated executables and object files under `media/bin/` are ignored by Git.
The PNG source includes expanded-frame materialization (`fill-expanded-copies`)
and the `foc_imagegen` utility. Bob sources came from the local Jackson snapshot.

## Import provenance

The latest public GitHub version checked before importing the EleisonScel
snapshot was:

- Repository: `barbalet/c-utilities`
- URL: <https://github.com/barbalet/c-utilities>
- Branch: `main`
- Commit: `d759e338aa3193862f068777d538d709e3fea14b`
- Commit date: 2026-09-04 20:46:50 UTC
- Commit message: `Remove committed executables`

The recent commit history shows the current direction of the repo:

- `d759e33`: remove committed executables
- `427a45c`: organize C utility snapshots
- `fcaeff4`: add the native AA rendering method
- `6c28e92`: update utilities
- `d9a2cad`: update README content
- `6e82593`: update the audio side
- `aebf8b7`: optimize the audio C path
- `41cf089`: update audio rendering
- `addcf9c`: add PNG rendering work
- `33c3c3b`: add new utilities
- `597ba6c`: update new utilities
- `d15cb61`: update utilities from the use path
- `6f91282`: initial commit

These workflow snapshots were subsequently consolidated into the layout above.

## Local `c-utilities` Directories Found

The scoped search found these local directories named `c-utilities`:

- `/Users/barbalet/next/github/next/c-utilities`
- `/Users/barbalet/next/github/next/azaz/trainingdata/movetoazaz/tools/c-utilities`
- `/Users/barbalet/next/github/next/leicester_ymca/jackson/c-utilities`
- `/Users/barbalet/Documents/ChatGPT/jackson/c-utilities`
- `/Users/barbalet/Documents/ChatGPT/jackson/quidjibo/tools/c-utilities`
- `/Users/barbalet/Documents/ChatGPT/jackson/quidjibo/movetoazaz/tools/c-utilities`

The import target GitHub checkout was `/Users/barbalet/next/github/next/c-utilities`.
It was at the GitHub commit recorded above before the library import.
