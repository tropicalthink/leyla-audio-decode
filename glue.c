/*
 * Leyla's WebAssembly audio decoder: FFmpeg's DTS (dca) and TrueHD/MLP
 * decoders behind a handful of plain C calls, for
 * a web client's audio Worker. One MP4 sample goes in (a DTS frame, or
 * a TrueHD access unit); planar float PCM and the decoder's own channel
 * layout come out.
 *
 * The layout is the reason this module exists rather than a packaged
 * decoder: DTS and TrueHD streams cannot be labelled from their MP4 sample
 * entry, so the JavaScript side asks the decoder which channel each output
 * plane is (leyla_audio_channel), in the order FFmpeg emits them.
 *
 * LGPL-2.1-or-later, like the FFmpeg it links (built without --enable-gpl).
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "libavcodec/avcodec.h"
#include "libavutil/channel_layout.h"
#include "libavutil/samplefmt.h"

typedef struct LeylaAudio {
    AVCodecContext *ctx;
    AVPacket *pkt;
    AVFrame *frame;
    float *out;   /* planar: channel c starts at out + c * capacity */
    int capacity; /* samples per channel the buffer holds */
    int samples;  /* samples per channel of the last decode */
    int channels;
} LeylaAudio;

enum { LEYLA_DTS = 0, LEYLA_TRUEHD = 1 };

LeylaAudio *leyla_audio_create(int codec, int core_only)
{
    const AVCodec *decoder = avcodec_find_decoder(codec == LEYLA_TRUEHD ? AV_CODEC_ID_TRUEHD : AV_CODEC_ID_DTS);
    AVDictionary *options = NULL;
    LeylaAudio *d;
    if (!decoder)
        return NULL;
    d = calloc(1, sizeof(*d));
    if (!d)
        return NULL;
    d->ctx = avcodec_alloc_context3(decoder);
    d->pkt = av_packet_alloc();
    d->frame = av_frame_alloc();
    if (!d->ctx || !d->pkt || !d->frame)
        goto fail;
    /* DTS-HD MA's lossless extension (XLL) is optional: with core_only the
     * decoder ignores it and plays the lossy core every DTS-HD stream carries. */
    if (codec == LEYLA_DTS && core_only)
        av_dict_set(&options, "core_only", "1", 0);
    if (avcodec_open2(d->ctx, decoder, &options) < 0)
        goto fail;
    av_dict_free(&options);
    return d;
fail:
    av_dict_free(&options);
    avcodec_free_context(&d->ctx);
    av_packet_free(&d->pkt);
    av_frame_free(&d->frame);
    free(d);
    return NULL;
}

static float sample_at(const AVFrame *frame, int channel, int index, int channels)
{
    switch (frame->format) {
    case AV_SAMPLE_FMT_FLTP: return ((const float *)frame->extended_data[channel])[index];
    case AV_SAMPLE_FMT_FLT: return ((const float *)frame->extended_data[0])[index * channels + channel];
    case AV_SAMPLE_FMT_S16P: return ((const int16_t *)frame->extended_data[channel])[index] / 32768.0f;
    case AV_SAMPLE_FMT_S16: return ((const int16_t *)frame->extended_data[0])[index * channels + channel] / 32768.0f;
    case AV_SAMPLE_FMT_S32P: return ((const int32_t *)frame->extended_data[channel])[index] / 2147483648.0f;
    case AV_SAMPLE_FMT_S32: return ((const int32_t *)frame->extended_data[0])[index * channels + channel] / 2147483648.0f;
    default: return 0.0f;
    }
}

static int append_frame(LeylaAudio *d, const AVFrame *frame)
{
    int channels = frame->ch_layout.nb_channels;
    int needed = d->samples + frame->nb_samples;
    if (channels <= 0)
        return 0;
    if (d->samples > 0 && channels != d->channels)
        return AVERROR_INVALIDDATA; /* one sample never changes shape midway */
    if (needed > d->capacity || channels > d->channels) {
        int capacity = needed > d->capacity ? needed * 2 : d->capacity;
        float *grown = calloc((size_t)capacity * channels, sizeof(float));
        if (!grown)
            return AVERROR(ENOMEM);
        for (int c = 0; c < channels && c < d->channels && d->out; c++)
            memcpy(grown + (size_t)c * capacity, d->out + (size_t)c * d->capacity, sizeof(float) * d->samples);
        free(d->out);
        d->out = grown;
        d->capacity = capacity;
    }
    d->channels = channels;
    for (int c = 0; c < channels; c++) {
        float *plane = d->out + (size_t)c * d->capacity + d->samples;
        for (int i = 0; i < frame->nb_samples; i++)
            plane[i] = sample_at(frame, c, i, channels);
    }
    d->samples = needed;
    return 0;
}

/* Decodes one packet. Returns samples per channel (0 when the decoder has
 * nothing yet, e.g. TrueHD before its first major sync) or a negative
 * AVERROR. */
int leyla_audio_decode(LeylaAudio *d, const uint8_t *data, int size)
{
    int ret;
    d->samples = 0;
    ret = av_new_packet(d->pkt, size);
    if (ret < 0)
        return ret;
    memcpy(d->pkt->data, data, size);
    ret = avcodec_send_packet(d->ctx, d->pkt);
    av_packet_unref(d->pkt);
    if (ret < 0)
        return ret;
    for (;;) {
        ret = avcodec_receive_frame(d->ctx, d->frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        if (ret < 0)
            return ret;
        ret = append_frame(d, d->frame);
        av_frame_unref(d->frame);
        if (ret < 0)
            return ret;
    }
    return d->samples;
}

float *leyla_audio_output(LeylaAudio *d) { return d->out; }
int leyla_audio_stride(LeylaAudio *d) { return d->capacity; }
int leyla_audio_channels(LeylaAudio *d) { return d->channels; }
int leyla_audio_sample_rate(LeylaAudio *d) { return d->ctx->sample_rate; }

/* The AVChannel of output plane `index` (AV_CHAN_FRONT_LEFT = 0, ...), or
 * a negative value when the decoder does not know. */
int leyla_audio_channel(LeylaAudio *d, int index)
{
    return av_channel_layout_channel_from_index(&d->ctx->ch_layout, index);
}

/* Bits per raw sample the stream carries (16 or 24), for dither. */
int leyla_audio_bits(LeylaAudio *d) { return d->ctx->bits_per_raw_sample; }

void leyla_audio_flush(LeylaAudio *d) { avcodec_flush_buffers(d->ctx); }

void leyla_audio_destroy(LeylaAudio *d)
{
    if (!d)
        return;
    avcodec_free_context(&d->ctx);
    av_packet_free(&d->pkt);
    av_frame_free(&d->frame);
    free(d->out);
    free(d);
}
