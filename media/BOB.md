# Bob Media Utilities

Small native C helpers copied from the local Jackson `c-utilities` snapshot.
They sit beside the Field of Chaos audio, PNG, and AA-render utilities because
they cover a separate media-preparation workflow.

## Build

```sh
make -C media bob
```

This builds:

- `bob_fetch_youtube`
- `bob_mux`
- `bob_vtt100`

## Tools

`bob_fetch_youtube` prepares source material from YouTube URLs. It handles
directory creation, URL cleanup, and shell-safe command construction around the
external downloader workflow.

`bob_mux` reads image/timing rows and prepares muxing work for generated media
outputs.

`bob_vtt100` processes WebVTT-style timed text into the local Bob workflow.

Executables are built in `media/bin/`. Shared helpers live in `support/`.
