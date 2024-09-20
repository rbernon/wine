/*
 * Copyright 2024 Rémi Bernon for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#if 0
#pragma makedep unix
#endif

#include "config.h"

#ifdef HAVE_FFMPEG

#include "unix_private.h"
#include "unix_decoder.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmo);

static inline BOOL frame_check_alignment( int size, const AVFrame *frame )
{
    int i, align = av_cpu_max_align();

    if (size % align) return FALSE;
    for (i = 0; i < ARRAY_SIZE(frame->data); i++) if ((UINT_PTR)frame->data[i] % align) return FALSE;
    for (i = 0; i < ARRAY_SIZE(frame->linesize); i++) if (frame->linesize[i] % align) return FALSE;

    return TRUE;
}

static INT64 get_user_time( INT64 time, AVRational time_base )
{
    static const AVRational USER_TIME_BASE_Q = {1, 10000000};
    return av_rescale_q_rnd( time, time_base, USER_TIME_BASE_Q, AV_ROUND_PASS_MINMAX );
}

static void buffer_free( void *opaque, uint8_t *data )
{
}

static BOOL audio_frame_wrap_sample( AVFrame *frame, const WAVEFORMATEXTENSIBLE *info, const struct sample *sample )
{
    enum AVSampleFormat sample_fmt = sample_format_from_wave_format( &info->Format );
    void *data = (void *)(UINT_PTR)sample->data;
    int size;

    TRACE( "frame %p, format %#x, info %p, sample %p\n", frame, sample_fmt, info, sample );

    size = av_samples_fill_arrays( frame->data, frame->linesize, data, info->Format.nChannels,
                                   sample->size / info->Format.nBlockAlign, sample_fmt, 1 );
    if (size > sample->size || !frame_check_alignment( size, frame ))
    {
        TRACE("frame %dx%d %#x doesn't match alignment %zu\n", frame->width, frame->height, sample_fmt, av_cpu_max_align());
        memset( frame->data, 0, sizeof(frame->data) );
        memset( frame->linesize, 0, sizeof(frame->linesize) );
        return FALSE;
    }

    frame->format = sample_fmt;
    frame->sample_rate = info->Format.nSamplesPerSec;
    frame->nb_samples = sample->size / info->Format.nBlockAlign;
#if LIBAVUTIL_VERSION_MAJOR >= 58
    if (info->dwChannelMask) av_channel_layout_from_mask( &frame->ch_layout, info->dwChannelMask );
    else av_channel_layout_default( &frame->ch_layout, info->Format.nChannels );
#else
    frame->channels = info->Format.nChannels;
    frame->channel_layout = info->dwChannelMask;
#endif
    frame->buf[0] = av_buffer_create( data, sample->size, buffer_free, NULL, 0 );
    frame->extended_data = frame->data;
    frame->opaque = (void *)sample;

    frame->pts = av_rescale( sample->pts, AV_TIME_BASE, 10000000 );
#if LIBAVUTIL_VERSION_MAJOR >= 58
    frame->duration = av_rescale( sample->duration, AV_TIME_BASE, 10000000 );
#endif

    return TRUE;
}

static void audio_frame_init_with_format( AVFrame *frame, const WAVEFORMATEXTENSIBLE *info, UINT size )
{
    if (frame->buf[0]) return;

    frame->format = sample_format_from_wave_format( &info->Format );
    frame->sample_rate = info->Format.nSamplesPerSec;
    frame->nb_samples = size / info->Format.nBlockAlign;
#if LIBAVUTIL_VERSION_MAJOR >= 58
    if (info->dwChannelMask) av_channel_layout_from_mask( &frame->ch_layout, info->dwChannelMask );
    else av_channel_layout_default( &frame->ch_layout, info->Format.nChannels );
#else
    frame->channels = info->Format.nChannels;
    frame->channel_layout = info->Format.nChannels;
#endif
    frame->extended_data = frame->data;
    av_frame_get_buffer( frame, 0 );
}

static int audio_frame_copy_from_sample( AVFrame *frame, const WAVEFORMATEXTENSIBLE *info, const struct sample *sample )
{
    enum AVSampleFormat sample_fmt = sample_format_from_wave_format( &info->Format );
    const void *data = (void *)(UINT_PTR)sample->data;
    UINT8 *input_planes[4] = {0}, *output_planes[4] = {0};
    int ret, input_strides[4] = {0}, output_strides[4] = {0};

    ERR( "frame %p, format %#x, info %p, sample %p\n", frame, sample_fmt, info, sample );

    memcpy( output_planes, frame->data, sizeof(output_planes) );
    memcpy( output_strides, frame->linesize, sizeof(output_strides) );
    ret = av_samples_fill_arrays( input_planes, input_strides, data, info->Format.nChannels,
                                  sample->size / info->Format.nBlockAlign, sample_fmt, 1 );

    av_samples_copy( output_planes, input_planes, 0, 0, frame->nb_samples, info->Format.nChannels, sample_fmt );
    return ret;
}

static int audio_frame_copy_to_sample( AVFrame *frame, const WAVEFORMATEXTENSIBLE *info, struct sample *sample )
{
    enum AVSampleFormat sample_fmt = sample_format_from_wave_format( &info->Format );
    const void *data = (void *)(UINT_PTR)sample->data;
    UINT8 *input_planes[4] = {0}, *output_planes[4] = {0};
    int ret, input_strides[4] = {0}, output_strides[4] = {0};

    ERR( "frame %p, format %#x, info %p, sample %p\n", frame, sample_fmt, info, sample );

    memcpy( input_planes, frame->data, sizeof(input_planes) );
    memcpy( input_strides, frame->linesize, sizeof(input_strides) );
    ret = av_samples_fill_arrays( output_planes, output_strides, data, info->Format.nChannels,
                                  frame->nb_samples, sample_fmt, 1 );

    av_samples_copy( output_planes, input_planes, 0, 0, frame->nb_samples, info->Format.nChannels, sample_fmt );
    return ret;
}

static void wave_format_extensible_copy( WAVEFORMATEXTENSIBLE *dst, const WAVEFORMATEX *src )
{
    if (src->wFormatTag != WAVE_FORMAT_EXTENSIBLE) dst->Format = *src;
    else *dst = *(WAVEFORMATEXTENSIBLE *)src;
    if (!dst->Format.nBlockAlign) dst->Format.nBlockAlign = dst->Format.nChannels * dst->Format.wBitsPerSample / 8;
}


struct audio_converter
{
    struct transform transform;
    struct SwrContext *resampler;
    AVRational time_base;

    WAVEFORMATEXTENSIBLE input_format;
    AVCodecParameters *input_par;
    AVFrame input_frame;

    WAVEFORMATEXTENSIBLE output_format;
    AVCodecParameters *output_par;
    AVFrame output_frame;

    AVFrame cached_frame;
};

static void audio_converter_init( struct audio_converter *converter )
{
    AVCodecParameters *par;
    int ret;

    converter->resampler = swr_alloc();
    converter->time_base.num = 1;
    converter->time_base.den = 1;

    par = converter->input_par;
#if LIBAVUTIL_VERSION_MAJOR >= 58
    av_opt_set_chlayout( converter->resampler, "in_chlayout", &par->ch_layout, 0 );
#else
    av_opt_set_int( converter->resampler, "in_channel_count", par->channels, 0 );
    av_opt_set_channel_layout( converter->resampler, "in_channel_layout", par->channel_layout, 0 );
#endif
    av_opt_set_int( converter->resampler, "in_sample_rate", par->sample_rate, 0 );
    av_opt_set_sample_fmt( converter->resampler, "in_sample_fmt", par->format, 0 );
    converter->time_base.den *= par->sample_rate;

    par = converter->output_par;
#if LIBAVUTIL_VERSION_MAJOR >= 58
    av_opt_set_chlayout( converter->resampler, "out_chlayout", &par->ch_layout, 0 );
#else
    av_opt_set_int( converter->resampler, "out_channel_count", par->channels, 0 );
    av_opt_set_channel_layout( converter->resampler, "out_channel_layout", par->channel_layout, 0 );
#endif
    av_opt_set_int( converter->resampler, "out_sample_rate", par->sample_rate, 0 );
    av_opt_set_sample_fmt( converter->resampler, "out_sample_fmt", par->format, 0 );
    converter->time_base.den *= par->sample_rate;

    ret = swr_init( converter->resampler );
    ERR( "ret %d %s\n", ret, av_err2str( ret ) );
}

static void audio_converter_cleanup( struct audio_converter *converter )
{
    swr_free( &converter->resampler );
    av_frame_unref( &converter->input_frame );
    av_frame_unref( &converter->output_frame );
    av_frame_unref( &converter->cached_frame );
}

static NTSTATUS audio_converter_destroy( struct transform *iface )
{
    struct audio_converter *converter = CONTAINING_RECORD( iface, struct audio_converter, transform );

    FIXME( "stub!\n" );

    audio_converter_cleanup( converter );

    avcodec_parameters_free( &converter->output_par );
    avcodec_parameters_free( &converter->input_par );
    free( converter );

    return STATUS_SUCCESS;
}

static int audio_converter_process_input_frame( struct audio_converter *converter, AVFrame *input_frame, struct sample *output_sample )
{
    static const AVRational USER_TIME_BASE_Q = {1, 10000000};
    int ret, max_samples = output_sample->size / converter->output_format.Format.nBlockAlign;
    AVFrame output_frame = {0};

    TRACE( "input samples %u, pts %jd, output max samples %u\n",
           input_frame->nb_samples, input_frame->pts, max_samples );

    if (!audio_frame_wrap_sample( &output_frame, &converter->output_format, output_sample ))
    {
        audio_frame_init_with_format( &converter->output_frame, &converter->output_format,
                                      output_sample->size );
        av_frame_move_ref( &output_frame, &converter->output_frame );
    }

    output_frame.pts = swr_next_pts( converter->resampler,
                                     av_rescale_q_rnd( input_frame->pts, USER_TIME_BASE_Q,
                                                       converter->time_base, AV_ROUND_PASS_MINMAX ) );
    ret = swr_convert( converter->resampler, output_frame.data, max_samples,
                       (const uint8_t **)input_frame->data, input_frame->nb_samples );
    if (ret < 0) ERR( "error ret %d (%s)\n", -ret, av_err2str( ret ) );
    else output_frame.nb_samples = ret;

    output_sample->pts = get_user_time( output_frame.pts, converter->time_base );
    output_sample->flags |= SAMPLE_FLAG_SYNC_POINT;
    output_sample->duration = av_rescale(output_frame.nb_samples, 10000000, converter->output_format.Format.nSamplesPerSec);

    if (output_frame.opaque)
    {
        enum AVSampleFormat sample_fmt = sample_format_from_wave_format( &converter->output_format.Format );
        if (ret > 0)
            ret = av_samples_get_buffer_size( NULL, converter->output_format.Format.nChannels,
                                              output_frame.nb_samples, sample_fmt, 1 );
        av_frame_unref( &output_frame );
    }
    else
    {
        if (ret > 0)
            ret = audio_frame_copy_to_sample( &output_frame, &converter->output_format, output_sample );
        av_frame_move_ref( &converter->output_frame, &output_frame );
    }

    if (swr_get_out_samples( converter->resampler, 0 ))
        output_sample->flags |= SAMPLE_FLAG_INCOMPLETE;

    output_sample->size = ret >= 0 ? ret : 0;
    if (ret < 0) return STATUS_UNSUCCESSFUL;
    if (!ret) return STATUS_PENDING;

    TRACE( "returning output sample size %#jx, pts %jd, duration %jd\n", output_sample->size,
           output_sample->pts, output_sample->duration );
    return STATUS_SUCCESS;
}

static NTSTATUS audio_converter_process_input( struct transform *iface, const struct sample *sample )
{
    struct audio_converter *converter = CONTAINING_RECORD( iface, struct audio_converter, transform );

    if (!audio_frame_wrap_sample( &converter->input_frame, &converter->input_format, sample ))
    {
        av_frame_move_ref( &converter->input_frame, &converter->cached_frame );
        audio_frame_init_with_format( &converter->input_frame, &converter->input_format, sample->size );
        audio_frame_copy_from_sample( &converter->input_frame, &converter->input_format, sample );
    }

    converter->input_frame.pts = sample->pts;
#if LIBAVUTIL_VERSION_MAJOR >= 58
    converter->input_frame.duration = sample->duration;
#endif

    return STATUS_SUCCESS;
}

static NTSTATUS audio_converter_process_output( struct transform *iface, struct sample *sample )
{
    struct audio_converter *converter = CONTAINING_RECORD( iface, struct audio_converter, transform );
    NTSTATUS status;

    status = audio_converter_process_input_frame( converter, &converter->input_frame, sample );

    if (converter->input_frame.opaque) av_frame_unref( &converter->input_frame );
    else av_frame_move_ref( &converter->cached_frame, &converter->input_frame );

    return status;
}

static const struct transform_ops audio_converter_ops =
{
    .destroy = audio_converter_destroy,
    .process_input = audio_converter_process_input,
    .process_output = audio_converter_process_output,
};

static NTSTATUS audio_converter_create( const struct media_type *input_type, const struct media_type *output_type, struct transform **out )
{
    struct audio_converter *converter;
    AVRational sar, fps;

    if (!(converter = calloc( 1, sizeof(*converter) ))) return STATUS_NO_MEMORY;
    converter->transform.ops = &audio_converter_ops;

    converter->input_par = avcodec_parameters_alloc();
    codec_params_from_media_type( converter->input_par, &sar, &fps, input_type );
    wave_format_extensible_copy( &converter->input_format, input_type->audio );

    converter->output_par = avcodec_parameters_alloc();
    codec_params_from_media_type( converter->output_par, &sar, &fps, output_type );
    wave_format_extensible_copy( &converter->output_format, output_type->audio );

    audio_converter_init( converter );

    *out = &converter->transform;

    return STATUS_SUCCESS;
}

static void audio_converter_init_decoder( struct audio_converter *converter, AVCodecContext *input,
                                          const struct media_type *output_type )
{
    AVRational sar, fps;

    converter->input_par = avcodec_parameters_alloc();
    avcodec_parameters_from_context( converter->input_par, input );

    converter->output_par = avcodec_parameters_alloc();
    codec_params_from_media_type( converter->output_par, &sar, &fps, output_type );
    wave_format_extensible_copy( &converter->output_format, output_type->audio );

    audio_converter_init( converter );
}


struct audio_decoder
{
    struct transform transform;
    struct audio_converter converter;
    struct decoder *decoder;
};

static NTSTATUS audio_decoder_destroy( struct transform *iface )
{
    struct audio_decoder *audio_decoder = CONTAINING_RECORD( iface, struct audio_decoder, transform );

    TRACE( "audio_decoder %p\n", audio_decoder );

    audio_converter_cleanup( &audio_decoder->converter );
    avcodec_parameters_free( &audio_decoder->converter.output_par );
    avcodec_parameters_free( &audio_decoder->converter.input_par );

    decoder_destroy( audio_decoder->decoder );
    free( audio_decoder );

    return STATUS_SUCCESS;
}

static NTSTATUS audio_decoder_process_input( struct transform *iface, const struct sample *sample )
{
    struct audio_decoder *audio_decoder = CONTAINING_RECORD( iface, struct audio_decoder, transform );
    TRACE( "audio_decoder %p, sample %p, size %#jx, pts %jd, duration %jd\n", audio_decoder, sample,
           sample->size, sample->pts, sample->duration );
    return decoder_process_input( audio_decoder->decoder, sample );
}

static NTSTATUS audio_decoder_process_output( struct transform *iface, struct sample *sample )
{
    struct audio_decoder *audio_decoder = CONTAINING_RECORD( iface, struct audio_decoder, transform );
    AVFrame decoded_frame = {0};
    NTSTATUS status;

    TRACE( "audio_decoder %p, sample %p, max_size %#jx\n", audio_decoder, sample, sample->size );

    if ((status = decoder_process_output( audio_decoder->decoder, &decoded_frame )) && status != STATUS_PENDING)
    {
        sample->size = 0;
        return status;
    }

    TRACE( "received input frame samples %u, pts %jd\n", decoded_frame.nb_samples, decoded_frame.pts );
    status = audio_converter_process_input_frame( &audio_decoder->converter, &decoded_frame, sample );
    av_frame_unref( &decoded_frame );

    return status;
}

static NTSTATUS audio_decoder_drain( struct transform *iface, BOOL discard )
{
    struct audio_decoder *audio_decoder = CONTAINING_RECORD( iface, struct audio_decoder, transform );
    NTSTATUS status;

    status = decoder_drain( audio_decoder->decoder, discard );
    if (discard)
    {
        audio_converter_cleanup( &audio_decoder->converter );
        audio_converter_init( &audio_decoder->converter );
    }

    return status;
}

static const struct transform_ops audio_decoder_ops =
{
    .destroy = audio_decoder_destroy,
    .process_input = audio_decoder_process_input,
    .process_output = audio_decoder_process_output,
    .drain = audio_decoder_drain,
};

static NTSTATUS audio_decoder_create( const struct media_type *input_type, const struct media_type *output_type, struct transform **out )
{
    struct audio_decoder *audio_decoder;

    if (!(audio_decoder = calloc( 1, sizeof(*audio_decoder) ))) return STATUS_NO_MEMORY;
    audio_decoder->transform.ops = &audio_decoder_ops;
    decoder_create( input_type, &audio_decoder->decoder );

    audio_converter_init_decoder( &audio_decoder->converter, audio_decoder->decoder->context, output_type );

    TRACE( "audio_decoder %p\n", audio_decoder );
    *out = &audio_decoder->transform;

    return STATUS_SUCCESS;
}


struct audio_encoder
{
    struct encoder encoder;
};

static NTSTATUS audio_encoder_create( const struct media_type *input_type, const struct media_type *output_type, struct transform **out )
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS audio_encoder_destroy( struct transform_destroy_params *params )
{
    return STATUS_SUCCESS;
}

NTSTATUS audio_encoder_process_input( struct transform_process_input_params *params )
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS audio_encoder_process_output( struct transform_process_output_params *params )
{
    return STATUS_NOT_IMPLEMENTED;
}


NTSTATUS audio_transform_create( struct transform_create_params *params )
{
    enum AVSampleFormat input_format, output_format;
    struct transform *transform;
    NTSTATUS status;

    input_format = sample_format_from_wave_format( params->input_type.audio );
    output_format = sample_format_from_wave_format( params->output_type.audio );
    if (input_format == AV_SAMPLE_FMT_NONE && output_format == AV_SAMPLE_FMT_NONE)
    {
        FIXME( "Unsupported media types!\n" );
        return STATUS_INVALID_PARAMETER;
    }

    if (input_format == AV_SAMPLE_FMT_NONE)
        status = audio_decoder_create( &params->input_type, &params->output_type, &transform );
    else if (output_format == AV_SAMPLE_FMT_NONE)
        status = audio_encoder_create( &params->input_type, &params->output_type, &transform );
    else
        status = audio_converter_create( &params->input_type, &params->output_type, &transform );

    if (!status) params->transform.handle = (UINT_PTR)transform;
    return status;
}

NTSTATUS audio_transform_check( struct transform_check_params *params )
{
    enum AVSampleFormat input_format, output_format;

    input_format = sample_format_from_subtype( &params->input, 32 );
    output_format = sample_format_from_subtype( &params->output, 32 );
    if (input_format == AV_SAMPLE_FMT_NONE && output_format == AV_SAMPLE_FMT_NONE)
    {
        FIXME( "Unsupported media types!\n" );
        return STATUS_INVALID_PARAMETER;
    }

    if (input_format == AV_SAMPLE_FMT_NONE)
    {
        enum AVCodecID codec_id = codec_id_from_audio_subtype( &params->input );
        if (codec_id == AV_CODEC_ID_NONE || !avcodec_find_decoder( codec_id ))
        {
            ERR( "Unsupported %s decoder.\n", avcodec_get_name( codec_id ) );
            return STATUS_NOT_SUPPORTED;
        }
    }
    if (output_format == AV_SAMPLE_FMT_NONE)
    {
        enum AVCodecID codec_id = codec_id_from_audio_subtype( &params->output );
        if (codec_id == AV_CODEC_ID_NONE || !avcodec_find_encoder( codec_id ))
        {
            ERR( "Unsupported %s encoder.\n", avcodec_get_name( codec_id ) );
            return STATUS_NOT_SUPPORTED;
        }
    }

    return STATUS_SUCCESS;
}

#endif /* HAVE_FFMPEG */
