# C Utilities

Native C utility programs for Codex-adjacent media, render, and Field of Chaos
workflows.

The repository is organized by workflow domain:

- `audio/`: Field of Chaos text and audio pipeline helpers
- `png render utils/`: Field of Chaos PNG render planning and verification
- `aa_render_c/`: native AA rendering orchestration
- `bob media utils/`: Bob media preparation utilities copied from the local
  Jackson snapshot
- `third_party/eleisonscel-c-utilities/`: Apache-2.0 EleisonScel C utility
  library snapshot, kept separate from the workflow-specific utilities

## Latest GitHub State

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

That history is why this checkout keeps the existing workflow-oriented layout
instead of flattening every utility into one source directory.

## Local `c-utilities` Directories Found

The scoped search found these local directories named `c-utilities`:

- `/Users/barbalet/next/github/next/c-utilities`
- `/Users/barbalet/next/github/next/azaz/trainingdata/movetoazaz/tools/c-utilities`
- `/Users/barbalet/next/github/next/leicester_ymca/jackson/c-utilities`
- `/Users/barbalet/Documents/ChatGPT/jackson/c-utilities`
- `/Users/barbalet/Documents/ChatGPT/jackson/quidjibo/tools/c-utilities`
- `/Users/barbalet/Documents/ChatGPT/jackson/quidjibo/movetoazaz/tools/c-utilities`

The target GitHub checkout is `/Users/barbalet/next/github/next/c-utilities`.
It was already at the latest GitHub commit above before the current import.

## What Was Added

The PNG render utility source was refreshed from the fuller local
`leicester_ymca/jackson` copy, which includes expanded-frame materialization via
`fill-expanded-copies`.

The PNG render folder also now includes the `foc_imagegen` source from the local
`azaz/trainingdata/movetoazaz` copy. Its Makefile builds both PNG render tools:

```sh
cd "png render utils"
make
```

The Bob media utilities from the local Jackson snapshot were added under:

```text
bob media utils/
```

The EleisonScel library snapshot was added under:

```text
third_party/eleisonscel-c-utilities/
```

It contains the upstream `include/c-utilities/` headers, `src/` sources,
Apache-2.0 license, and upstream README material. It is kept as a third-party
library snapshot because it is a general reusable C support library, while the
rest of this repository is organized around local media and render workflows.

## Build

Build each workflow independently:

```sh
make -C audio
make -C "png render utils"
make -C "bob media utils"
make -C aa_render_c -f Makefile.aa_render_c
```

The AA renderer Makefile also provides its own smoke-test target:

```sh
make -C aa_render_c -f Makefile.aa_render_c clean all test
```

## Size And Generated Files

The repo is intentionally kept small.

- No copied file is larger than 30 MB.
- No copied directory is larger than 30 MB.
- Object files are ignored with `*.o`.
- Generated executable binaries are ignored by name.
- Existing tracked object artifacts and executable binaries were removed from
  this checkout.

The committed tree should contain source, documentation, and build recipes only.
Run the Makefiles locally whenever the utilities need to be rebuilt.
