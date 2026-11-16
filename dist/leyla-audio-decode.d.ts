// Types for the Emscripten module `build.sh` produces.
// The exported C functions are `glue.c`'s.

export interface LeylaAudioModule {
  HEAPU8: Uint8Array
  HEAPF32: Float32Array
  _malloc(size: number): number
  _free(pointer: number): void
  _leyla_audio_create(codec: number, coreOnly: number): number
  _leyla_audio_decode(handle: number, data: number, size: number): number
  _leyla_audio_output(handle: number): number
  _leyla_audio_stride(handle: number): number
  _leyla_audio_channels(handle: number): number
  _leyla_audio_sample_rate(handle: number): number
  _leyla_audio_channel(handle: number, index: number): number
  _leyla_audio_bits(handle: number): number
  _leyla_audio_flush(handle: number): void
  _leyla_audio_destroy(handle: number): void
}

export default function createLeylaAudio(): Promise<LeylaAudioModule>
