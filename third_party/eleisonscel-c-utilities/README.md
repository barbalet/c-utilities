# EleisonScel C Utilities Snapshot

This directory contains selected source, header, documentation, and license
material from the EleisonScel `c-utilities` repository.

Upstream reference:

- Repository: `EleisonScel/c-utilities`
- URL: <https://github.com/EleisonScel/c-utilities>
- Snapshot commit: `bc753f45db1c04d765cd73bb5ff5bf4133c1c274`
- Snapshot date: 2026-08-29 22:26:29 UTC
- Snapshot commit message: `Delete safe_cast.h`
- License: Apache-2.0

The snapshot is kept separate from the local workflow-specific utility folders
so it can be reviewed, updated, or removed without obscuring the active
Barbalet `c-utilities` git history.

Included elements:

- `include/c-utilities/`: public headers
- `src/`: implementation files
- `docs/README.upstream.md`: upstream README material
- `LICENSE`: upstream Apache-2.0 license

Generated files, object files, Finder metadata, and executable binaries are not
part of this third-party snapshot.

Most modules are plain C99. The OpenGL helpers depend on GLEW/OpenGL headers and
libraries supplied by the consuming project.
