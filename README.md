# leyla-audio-decode

An LGPL-2.1-or-later WebAssembly decoder for DTS, DTS-HD and TrueHD/MLP audio,
for the web client of [Leyla](https://leyla.media). It is FFmpeg's `dca`,
`truehd` and `mlp` decoders compiled with Emscripten, with `glue.c` as its
interface, as one ES module with the WASM inlined (the shape
`@audio/decode-eac3` ships in).

Why it exists:

- No package decodes TrueHD.
- `@audio/decode-dts` wraps libdca (GPL, core only).
- Neither reports the decoder's channel layout, and neither codec can be
  labelled from its MP4 sample entry. `glue.c` exposes it
  (`leyla_audio_channel`).

One MP4 sample goes in (a DTS frame or a TrueHD access unit); planar float PCM
and the decoder's own channel layout come out. `dist/leyla-audio-decode.d.ts`
has the types. DTS-HD MA's lossless layer is decoded unless the caller asks
for the core only.

## Pinned inputs

| Input | Value |
|---|---|
| FFmpeg | tag `n7.1.5`, <https://git.ffmpeg.org/ffmpeg.git> |
| Emscripten | Docker image `emscripten/emsdk:4.0.15` |
| Configure | `--disable-everything --enable-decoder=dca,truehd,mlp --enable-parser=dca,mlp`, no `--enable-gpl` / `--enable-nonfree` |

## Output

| | |
|---|---|
| File | `dist/leyla-audio-decode.js` |
| sha256 | `a75987cc0eea4fca0027d961a134b1e68bca19b47eb16d8c26c3a65cb94c68e8` |

## Rebuilding and replacing

```sh
./build.sh
```

It prints the new file's sha256 and gzip size. The module in `dist/` is the build Leyla ships. Rebuilding with the
pinned inputs gives the same bytes (`SOURCE_DATE_EPOCH=0`, path prefixes mapped); checked on 2026-11-17. To replace the module
in a running deployment, as the LGPL allows, build your own with this script
(or any FFmpeg build exposing `glue.c`'s functions) and substitute the chunk
that contains it.

## Licence

LGPL-2.1-or-later (`LICENSE`, `NOTICE`).
