#!/usr/bin/env bash
# Reproducible build of dist/leyla-audio-decode.js (FFmpeg's dca and
# truehd/mlp decoders as WebAssembly). Needs Docker and network access to
# git.ffmpeg.org. See README.md.
set -euo pipefail

# Pinned inputs: change them only together with README.md's recorded hash.
EMSDK_IMAGE="emscripten/emsdk:4.0.15"
FFMPEG_TAG="n7.1.5"
OUT="leyla-audio-decode.js"

here="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$here/dist"
out_dir="$(mktemp -d)"
trap 'rm -rf "$out_dir"' EXIT

docker run --rm -e SOURCE_DATE_EPOCH=0 -e JOBS="${JOBS:-8}" \
    -v "$here":/build:ro -v "$out_dir":/out \
    -u "$(id -u):$(id -g)" -e HOME=/tmp -e EM_CACHE=/tmp/emcache \
    "$EMSDK_IMAGE" bash /build/build-inside.sh "$FFMPEG_TAG" "$OUT"

cp "$out_dir/$OUT" "$here/dist/$OUT"
(cd "$here" && sha256sum "dist/$OUT")
printf 'gzip -9: %s bytes\n' "$(gzip -9 -c "$here/dist/$OUT" | wc -c)"
