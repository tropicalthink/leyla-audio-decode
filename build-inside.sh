#!/usr/bin/env bash
# Runs inside the pinned emscripten image (see build.sh): fetches the pinned
# FFmpeg tag, configures only the DTS and TrueHD/MLP decoders, and links
# glue.c into one single-file ES module with the WASM inlined, the shape
# @audio/decode-eac3 ships in.
set -euo pipefail

FFMPEG_TAG="$1"
OUT="$2"
WORK=/tmp/leyla-wasm-audio
mkdir -p "$WORK"
cd "$WORK"
git -c advice.detachedHead=false clone --quiet --depth 1 --branch "$FFMPEG_TAG" \
    https://git.ffmpeg.org/ffmpeg.git ffmpeg
cd ffmpeg
# No --enable-gpl and no --enable-nonfree: the result stays LGPL-2.1+.
emconfigure ./configure \
    --target-os=none --arch=x86_32 --enable-cross-compile \
    --cc=emcc --cxx=em++ --ar=emar --ranlib=emranlib --nm=emnm \
    --disable-asm --disable-x86asm --disable-inline-asm \
    --disable-runtime-cpudetect --disable-autodetect --disable-pthreads \
    --disable-w32threads --disable-os2threads --disable-network \
    --disable-programs --disable-doc --disable-debug --disable-stripping \
    --disable-everything \
    --disable-avdevice --disable-avformat --disable-avfilter \
    --disable-swscale --disable-swresample \
    --enable-decoder=dca,truehd,mlp --enable-parser=dca,mlp \
    --extra-cflags="-Oz -ffile-prefix-map=$WORK=." \
    >/dev/null
emmake make -j"${JOBS:-8}" >/dev/null

cd "$WORK"
emcc /build/glue.c -Iffmpeg ffmpeg/libavcodec/libavcodec.a ffmpeg/libavutil/libavutil.a \
    -Oz -ffile-prefix-map="$WORK"=. -ffile-prefix-map=/build=. \
    -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createLeylaAudio \
    -sSINGLE_FILE=1 -sALLOW_MEMORY_GROWTH=1 -sFILESYSTEM=0 \
    -sENVIRONMENT=web,worker,node \
    -sEXPORTED_FUNCTIONS=_malloc,_free,_leyla_audio_create,_leyla_audio_decode,_leyla_audio_output,_leyla_audio_stride,_leyla_audio_channels,_leyla_audio_sample_rate,_leyla_audio_channel,_leyla_audio_bits,_leyla_audio_flush,_leyla_audio_destroy \
    -sEXPORTED_RUNTIME_METHODS=HEAPU8,HEAPF32 \
    -o "/out/$OUT"
