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

static void get_video_content_size( const MFVideoInfo *info, int *width, int *height )
{
    if (info->MinimumDisplayAperture.OffsetX.value || info->MinimumDisplayAperture.OffsetY.value ||
        info->MinimumDisplayAperture.Area.cx || info->MinimumDisplayAperture.Area.cy)
    {
        *width = info->MinimumDisplayAperture.OffsetX.value + info->MinimumDisplayAperture.Area.cx;
        *height = info->MinimumDisplayAperture.OffsetY.value + info->MinimumDisplayAperture.Area.cy;
    }
    else
    {
        *width = info->dwWidth;
        *height = info->dwHeight;
    }
}

static void buffer_free( void *opaque, uint8_t *data )
{
}

static BOOL video_frame_wrap_sample( AVFrame *frame, const MFVIDEOFORMAT *info, const struct sample *sample )
{
    enum AVPixelFormat pix_fmt = pixel_format_from_video_format( info );
    void *data = (void *)(UINT_PTR)sample->data;
    int size;

    size = av_image_fill_arrays( frame->data, frame->linesize, data, pix_fmt,
                                 info->videoInfo.dwWidth, info->videoInfo.dwHeight, 1 );

    if (sample->stride && sample->stride != frame->linesize[0])
    {
        int linesize[4] = {sample->stride};

        if (frame->linesize[1]) linesize[1] = sample->stride * frame->linesize[1] / frame->linesize[0];
        if (frame->linesize[2]) linesize[2] = sample->stride * frame->linesize[2] / frame->linesize[0];
        if (frame->linesize[3]) linesize[3] = sample->stride * frame->linesize[3] / frame->linesize[0];

        if (frame->data[1]) frame->data[1] = frame->data[0] + (frame->data[1] - frame->data[0]) * linesize[0] / frame->linesize[0];
        if (frame->data[2]) frame->data[2] = frame->data[1] + (frame->data[2] - frame->data[1]) * linesize[1] / frame->linesize[1];
        if (frame->data[3]) frame->data[3] = frame->data[2] + (frame->data[3] - frame->data[2]) * linesize[2] / frame->linesize[2];

        memcpy( frame->linesize, linesize, sizeof(linesize) );
    }

    if (size > sample->size || !frame_check_alignment( size, frame ))
    {
        WARN( "frame %dx%d %#x doesn't match alignment %zu\n", frame->width, frame->height, pix_fmt,
              av_cpu_max_align() );
        memset( frame->data, 0, sizeof(frame->data) );
        memset( frame->linesize, 0, sizeof(frame->linesize) );
        return FALSE;
    }

    if (info->videoInfo.VideoFlags & MFVideoFlag_BottomUpLinearRep)
    {
        frame->data[0] = frame->data[0] + frame->linesize[0] * (info->videoInfo.dwHeight - 1);
        frame->linesize[0] = -frame->linesize[0];
    }

    frame->format = pix_fmt;
    get_video_content_size( &info->videoInfo, &frame->width, &frame->height );
    frame->buf[0] = av_buffer_create( data, sample->size, buffer_free, NULL, 0 );
    frame->extended_data = frame->data;
    frame->opaque = (void *)sample;

    return TRUE;
}

static void video_frame_init_with_format( AVFrame *frame, const MFVIDEOFORMAT *info )
{
    if (frame->buf[0]) return;

    frame->format = pixel_format_from_video_format( info );
    frame->width = info->videoInfo.dwWidth;
    frame->height = info->videoInfo.dwHeight;
    frame->extended_data = frame->data;
    av_frame_get_buffer( frame, 0 ); /* allocate the buffer with the padded width/height */

    get_video_content_size( &info->videoInfo, &frame->width, &frame->height );
}

static int video_frame_copy_from_sample( AVFrame *frame, const MFVIDEOFORMAT *info, const struct sample *sample )
{
    enum AVPixelFormat pix_fmt = pixel_format_from_video_format( info );
    const void *data = (void *)(UINT_PTR)sample->data;
    UINT8 *input_planes[4] = {0}, *output_planes[4] = {0};
    int ret, input_strides[4] = {0}, output_strides[4] = {0};

    memcpy( output_planes, frame->data, sizeof(output_planes) );
    memcpy( output_strides, frame->linesize, sizeof(output_strides) );
    ret = av_image_fill_arrays( input_planes, input_strides, data, pix_fmt, info->videoInfo.dwWidth,
                                info->videoInfo.dwHeight, 1 );

    if (info->videoInfo.VideoFlags & MFVideoFlag_BottomUpLinearRep)
    {
        input_planes[0] = input_planes[0] + input_strides[0] * (info->videoInfo.dwHeight - 1);
        input_strides[0] = -input_strides[0];
    }

    av_image_copy( output_planes, output_strides, (const UINT8 **)input_planes, input_strides,
                   pix_fmt, info->videoInfo.dwWidth, info->videoInfo.dwHeight );
    return ret;
}

static int video_frame_copy_to_sample( AVFrame *frame, const MFVIDEOFORMAT *info, struct sample *sample )
{
    enum AVPixelFormat pix_fmt = pixel_format_from_video_format( info );
    const void *data = (void *)(UINT_PTR)sample->data;
    UINT8 *input_planes[4] = {0}, *output_planes[4] = {0};
    int ret, input_strides[4] = {0}, output_strides[4] = {0};

    memcpy( input_planes, frame->data, sizeof(input_planes) );
    memcpy( input_strides, frame->linesize, sizeof(input_strides) );
    ret = av_image_fill_arrays( output_planes, output_strides, data, pix_fmt,
                                info->videoInfo.dwWidth, info->videoInfo.dwHeight, 1 );

    if (info->videoInfo.VideoFlags & MFVideoFlag_BottomUpLinearRep)
    {
        input_planes[0] = input_planes[0] + input_strides[0] * (info->videoInfo.dwHeight - 1);
        input_strides[0] = -input_strides[0];
    }

    av_image_copy( output_planes, output_strides, (const UINT8 **)input_planes, input_strides,
                   pix_fmt, info->videoInfo.dwWidth, info->videoInfo.dwHeight );
    return ret;
}

struct video_converter
{
    struct transform transform;
    struct SwsContext *scaler;

    MFVIDEOFORMAT input_format;
    AVFrame input_frame;

    MFVIDEOFORMAT output_format;
    AVFrame output_frame;

    AVFrame cached_frame;
};

static NTSTATUS video_converter_init( struct video_converter *converter, int input_width,
                                      int input_height, enum AVPixelFormat input_fmt, int output_width,
                                      int output_height, enum AVPixelFormat output_fmt )
{
    int ret;

    if (!(converter->scaler = sws_alloc_context())) return STATUS_NO_MEMORY;
    av_opt_set( converter->scaler, "sws_flags", "neighbor", 0 );
    av_opt_set_int( converter->scaler, "threads", 16, 0 );

    av_opt_set_int( converter->scaler, "srcw", input_width, 0 );
    av_opt_set_int( converter->scaler, "srch", input_height, 0 );
    av_opt_set_pixel_fmt( converter->scaler, "src_format", input_fmt, 0 );

    av_opt_set_int( converter->scaler, "dstw", output_width, 0 );
    av_opt_set_int( converter->scaler, "dsth", output_height, 0 );
    av_opt_set_pixel_fmt( converter->scaler, "dst_format", output_fmt, 0 );

    if ((ret = sws_init_context( converter->scaler, NULL, NULL )) < 0)
    {
        ERR( "Failed to allocate scaler context\n" );
        return STATUS_NO_MEMORY;
    }

    return STATUS_SUCCESS;
}

static void video_converter_cleanup( struct video_converter *converter )
{
    if (converter->scaler) sws_freeContext( converter->scaler );
    converter->scaler = NULL;
    av_frame_unref( &converter->input_frame );
    av_frame_unref( &converter->output_frame );
    av_frame_unref( &converter->cached_frame );
}

static NTSTATUS video_converter_destroy( struct transform *iface )
{
    struct video_converter *converter = CONTAINING_RECORD( iface, struct video_converter, transform );

    TRACE( "\n" );

    video_converter_cleanup( converter );
    free( converter );

    return STATUS_SUCCESS;
}

static NTSTATUS video_converter_process_output_frame( struct video_converter *converter, AVFrame *output_frame )
{
    int ret = 0;

#if LIBSWSCALE_VERSION_MAJOR >= 6
    ret = sws_scale_frame( converter->scaler, output_frame, &converter->input_frame );
#else
    ret = sws_scale( converter->scaler, (const UINT8 **)converter->input_frame.data,
                     converter->input_frame.linesize, 0, converter->input_frame.height,
                     output_frame->data, output_frame->linesize );
#endif
    if (ret < 0) ERR( "error ret %d (%s)\n", -ret, av_err2str( ret ) );

    if (converter->input_frame.opaque) av_frame_unref( &converter->input_frame );
    else av_frame_move_ref( &converter->cached_frame, &converter->input_frame );

    if (!ret) return STATUS_PENDING;
    if (ret < 0) return STATUS_UNSUCCESSFUL;
    return STATUS_SUCCESS;
}

static NTSTATUS video_converter_process_input_frame( struct video_converter *converter,
                                                     AVFrame *input_frame, struct sample *output_sample )
{
    AVFrame output_frame = {0};
    int ret = 0;

    if (!video_frame_wrap_sample( &output_frame, &converter->output_format, output_sample ))
    {
        video_frame_init_with_format( &converter->output_frame, &converter->output_format );
        av_frame_move_ref( &output_frame, &converter->output_frame );
    }

#if LIBSWSCALE_VERSION_MAJOR >= 6
    ret = sws_scale_frame( converter->scaler, &output_frame, input_frame );
#else
    ret = sws_scale( converter->scaler, (const UINT8 **)input_frame->data, input_frame->linesize, 0,
                     input_frame->height, output_frame.data, output_frame.linesize );
#endif
    if (ret < 0) ERR( "error ret %d (%s)\n", -ret, av_err2str( ret ) );

    output_sample->pts = input_frame->pts;
#if LIBAVUTIL_VERSION_MAJOR >= 58
    output_sample->duration = input_frame->duration;
#endif
#ifdef AV_FRAME_FLAG_KEY
    if (input_frame->flags & AV_FRAME_FLAG_KEY) output_sample->flags |= SAMPLE_FLAG_SYNC_POINT;
#else
    if (input_frame->key_frame) output_sample->flags |= SAMPLE_FLAG_SYNC_POINT;
#endif

    if (output_frame.opaque)
    {
        enum AVPixelFormat pix_fmt = pixel_format_from_video_format( &converter->output_format );
        if (ret >= 0)
            ret = av_image_get_buffer_size( pix_fmt, converter->output_format.videoInfo.dwWidth,
                                            converter->output_format.videoInfo.dwHeight, 1 );
        av_frame_unref( &output_frame );
    }
    else
    {
        if (ret >= 0)
            ret = video_frame_copy_to_sample( &output_frame, &converter->output_format, output_sample );
        av_frame_move_ref( &converter->output_frame, &output_frame );
    }

    output_sample->size = ret >= 0 ? ret : 0;
    TRACE( "sample size %#x pts %u\n", (UINT)output_sample->size, (int)output_sample->pts );
    if (!ret) return STATUS_PENDING;
    if (ret < 0) return STATUS_UNSUCCESSFUL;
    return STATUS_SUCCESS;
}

static NTSTATUS video_converter_process_input( struct transform *iface, const struct sample *sample )
{
    struct video_converter *converter = CONTAINING_RECORD( iface, struct video_converter, transform );

    if (!video_frame_wrap_sample( &converter->input_frame, &converter->input_format, sample ))
    {
        av_frame_move_ref( &converter->input_frame, &converter->cached_frame );
        video_frame_init_with_format( &converter->input_frame, &converter->input_format );
        video_frame_copy_from_sample( &converter->input_frame, &converter->input_format, sample );
    }

    converter->input_frame.pts = sample->pts;
#if LIBAVUTIL_VERSION_MAJOR >= 58
    converter->input_frame.duration = sample->duration;
#endif

    return STATUS_SUCCESS;
}

static NTSTATUS video_converter_process_output( struct transform *iface, struct sample *sample )
{
    struct video_converter *converter = CONTAINING_RECORD( iface, struct video_converter, transform );
    NTSTATUS status;

    status = video_converter_process_input_frame( converter, &converter->input_frame, sample );

    if (converter->input_frame.opaque) av_frame_unref( &converter->input_frame );
    else av_frame_move_ref( &converter->cached_frame, &converter->input_frame );

    return status;
}

static const struct transform_ops video_converter_ops =
{
    .destroy = video_converter_destroy,
    .process_input = video_converter_process_input,
    .process_output = video_converter_process_output,
};

static NTSTATUS video_converter_create( const struct media_type *input_type, const struct media_type *output_type, struct transform **out )
{
    int input_width, input_height, output_width, output_height;
    struct video_converter *converter;

    TRACE( "\n" );

    if (!(converter = calloc( 1, sizeof(*converter) ))) return STATUS_NO_MEMORY;
    converter->transform.ops = &video_converter_ops;

    converter->input_format = *input_type->video;
    get_video_content_size( &converter->input_format.videoInfo, &input_width, &input_height );
    converter->output_format = *output_type->video;
    get_video_content_size( &converter->output_format.videoInfo, &output_width, &output_height );

    video_converter_init( converter, input_width, input_height,
                          pixel_format_from_video_format( &converter->input_format ), output_width,
                          output_height, pixel_format_from_video_format( &converter->output_format ) );

    TRACE( "converter %p converter->codec %p\n", converter, converter->scaler );
    *out = &converter->transform;

    return STATUS_SUCCESS;
}

static NTSTATUS video_converter_init_decoder( struct video_converter *converter, AVCodecContext *input, MFVIDEOFORMAT *output )
{
    int width = input->width, height = input->height, output_width, output_height;

    if (input->coded_width) width = input->coded_width;
    if (input->coded_height) height = input->coded_height;

    converter->output_format = *output;
    get_video_content_size( &converter->output_format.videoInfo, &output_width, &output_height );

    return video_converter_init( converter, width, height, input->pix_fmt, width, height,
                                 pixel_format_from_video_format( &converter->output_format ) );
}


struct video_decoder
{
    struct transform transform;
    struct video_converter converter;
    struct decoder *decoder;
    AVFrame last_frame;
    AVFrame next_frame;
};

static NTSTATUS video_decoder_destroy( struct transform *iface )
{
    struct video_decoder *video_decoder = CONTAINING_RECORD( iface, struct video_decoder, transform );

    TRACE( "video_decoder %p\n", video_decoder );

    av_frame_unref( &video_decoder->last_frame );
    av_frame_unref( &video_decoder->next_frame );
    video_converter_cleanup( &video_decoder->converter );
    decoder_destroy( video_decoder->decoder );
    free( video_decoder );

    return STATUS_SUCCESS;
}

NTSTATUS video_decoder_get_output_type( struct transform *iface, struct media_type *media_type )
{
    struct video_decoder *video_decoder = CONTAINING_RECORD( iface, struct video_decoder, transform );
    AVCodecContext *context = video_decoder->decoder->context;
    enum AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;
    AVCodecParameters *par;
    AVRational sar, fps;
    NTSTATUS status;
    UINT align = 0;

    TRACE( "video_decoder %p\n", video_decoder );

    if (context->sw_pix_fmt == AV_PIX_FMT_NONE) return STATUS_PENDING;

    pix_fmt = pixel_format_from_video_format( &video_decoder->converter.output_format );
    if (!video_decoder->converter.scaler)
        video_converter_init_decoder( &video_decoder->converter, context,
                                      &video_decoder->converter.output_format );

    par = avcodec_parameters_alloc();
    avcodec_parameters_from_context( par, context );
    par->codec_tag = 0;
    par->codec_id = AV_CODEC_ID_RAWVIDEO;
    par->format = pix_fmt == AV_PIX_FMT_NONE ? AV_PIX_FMT_NV12 : pix_fmt;
    sar = context->sample_aspect_ratio;
    fps = context->framerate;

    if (context->codec_id == AV_CODEC_ID_H264) align = 15;
    status = media_type_from_codec_params( par, &sar, &fps, align, media_type );
    avcodec_parameters_free( &par );
    return status;
}

NTSTATUS video_decoder_set_output_type( struct transform *iface, const struct media_type *media_type )
{
    struct video_decoder *video_decoder = CONTAINING_RECORD( iface, struct video_decoder, transform );
    int width = media_type->video->videoInfo.dwWidth, height = media_type->video->videoInfo.dwHeight;
    enum AVPixelFormat format = pixel_format_from_video_format( media_type->video );
    AVCodecContext *context = video_decoder->decoder->context;

    TRACE( "video_decoder %p width %u height %u format %#x\n", video_decoder, width, height, format );

    video_converter_cleanup( &video_decoder->converter );
    video_converter_init_decoder( &video_decoder->converter, context, media_type->video );

    av_frame_unref( &video_decoder->next_frame );
    av_frame_move_ref( &video_decoder->next_frame, &video_decoder->last_frame );

    return STATUS_SUCCESS;
}

static NTSTATUS video_decoder_process_input( struct transform *iface, const struct sample *sample )
{
    struct video_decoder *video_decoder = CONTAINING_RECORD( iface, struct video_decoder, transform );
    TRACE( "video_decoder %p, sample %p, size %#jx, pts %jd, duration %jd\n", video_decoder, sample,
           sample->size, sample->pts, sample->duration );
    return decoder_process_input( video_decoder->decoder, sample );
}

static NTSTATUS video_decoder_process_output( struct transform *iface, struct sample *sample )
{
    struct video_decoder *video_decoder = CONTAINING_RECORD( iface, struct video_decoder, transform );
    AVCodecContext *context = video_decoder->decoder->context;
    AVFrame decoded_frame = {0};
    NTSTATUS status;

    TRACE( "video_decoder %p, sample %p, max_size %#jx\n", video_decoder, sample, sample->size );

    if (video_decoder->next_frame.buf[0])
        av_frame_move_ref( &decoded_frame, &video_decoder->next_frame );
    else if ((status = decoder_process_output( video_decoder->decoder, &decoded_frame )))
    {
        sample->size = 0;
        return status;
    }

    av_frame_unref( &video_decoder->last_frame );
    av_frame_move_ref( &video_decoder->last_frame, &decoded_frame );

    if (context->pix_fmt != video_decoder->last_frame.format) video_converter_cleanup( &video_decoder->converter );
    context->pix_fmt = video_decoder->last_frame.format;
    if (!video_decoder->converter.scaler)
        video_converter_init_decoder( &video_decoder->converter, context,
                                      &video_decoder->converter.output_format );
    ERR( "%s\n", av_get_pix_fmt_name( context->pix_fmt ) );

    if (context->coded_width) video_decoder->last_frame.width = context->coded_width;
    if (context->coded_height) video_decoder->last_frame.height = context->coded_height;
    return video_converter_process_input_frame( &video_decoder->converter, &video_decoder->last_frame, sample );
}

static NTSTATUS video_decoder_drain( struct transform *iface, BOOL discard )
{
    struct video_decoder *video_decoder = CONTAINING_RECORD( iface, struct video_decoder, transform );
    NTSTATUS status;

    status = decoder_drain( video_decoder->decoder, discard );
    if (discard)
    {
        av_frame_unref( &video_decoder->last_frame );
        av_frame_unref( &video_decoder->next_frame );
    }

    return status;
}

static const struct transform_ops video_decoder_ops =
{
    .destroy = video_decoder_destroy,
    .set_output_type = video_decoder_set_output_type,
    .get_output_type = video_decoder_get_output_type,
    .process_input = video_decoder_process_input,
    .process_output = video_decoder_process_output,
    .drain = video_decoder_drain,
};

static NTSTATUS video_decoder_create( const struct media_type *input_type, const struct media_type *output_type, struct transform **out )
{
    struct video_decoder *video_decoder;

    if (!(video_decoder = calloc( 1, sizeof(*video_decoder) ))) return STATUS_NO_MEMORY;
    video_decoder->transform.ops = &video_decoder_ops;
    decoder_create( input_type, &video_decoder->decoder );

    video_decoder->converter.output_format = *output_type->video;

    TRACE( "video_decoder %p\n", video_decoder );
    *out = &video_decoder->transform;

    return STATUS_SUCCESS;
}


struct video_encoder
{
    struct transform transform;
    struct encoder encoder;
    struct video_converter converter;

    AVFrame last_frame;
    AVFrame next_frame;
};

NTSTATUS video_encoder_destroy( struct transform *iface )
{
    struct video_encoder *encoder = CONTAINING_RECORD( iface, struct video_encoder, transform );

    TRACE( "encoder %p encoder->codec %p\n", encoder, encoder->encoder.codec );

    av_frame_unref( &encoder->last_frame );
    av_frame_unref( &encoder->next_frame );
    video_converter_cleanup( &encoder->converter );
    encoder_cleanup( &encoder->encoder );
    free( encoder );

    return STATUS_SUCCESS;
}

NTSTATUS video_encoder_process_input( struct transform *iface, const struct sample *sample )
{
    struct video_encoder *encoder = CONTAINING_RECORD( iface, struct video_encoder, transform );
    int ret;

    video_frame_init_with_format( &encoder->converter.output_frame, &encoder->converter.output_format );
    av_frame_make_writable( &encoder->converter.output_frame );

    video_converter_process_input( &encoder->converter.transform, sample );
    video_converter_process_output_frame( &encoder->converter, &encoder->converter.output_frame );

    ret = avcodec_send_frame( encoder->encoder.codec, &encoder->converter.output_frame );
    if (ret < 0) ERR( "error ret %d (%s)\n", -ret, av_err2str( ret ) );

    return STATUS_SUCCESS;
}

NTSTATUS video_encoder_process_output( struct transform *iface, struct sample *sample )
{
    struct video_encoder *encoder = CONTAINING_RECORD( iface, struct video_encoder, transform );
    AVPacket packet = {0};
    int ret;

    ret = avcodec_receive_packet( encoder->encoder.codec, &packet );
    if (ret < 0) ERR( "error ret %d (%s)\n", -ret, av_err2str( ret ) );

    return STATUS_SUCCESS;
}

static const struct transform_ops video_encoder_ops =
{
    .destroy = video_encoder_destroy,
    .process_input = video_encoder_process_input,
    .process_output = video_encoder_process_output,
};

static NTSTATUS video_encoder_create( const struct media_type *input_type, const struct media_type *output_type, struct transform **out )
{
    struct video_encoder *encoder;

    TRACE( "\n" );

    if (!(encoder = calloc( 1, sizeof(*encoder) ))) return STATUS_NO_MEMORY;
    encoder->transform.ops = &video_encoder_ops;
    encoder_init( &encoder->encoder, input_type );

    encoder->converter.input_format = *input_type->video;

    TRACE( "encoder %p encoder->codec %p\n", encoder, encoder->encoder.codec );
    *out = &encoder->transform;

    return STATUS_SUCCESS;
}


NTSTATUS video_transform_create( struct transform_create_params *params )
{
    enum AVPixelFormat input_format, output_format;
    struct transform *transform;
    NTSTATUS status;

    input_format = pixel_format_from_video_format( params->input_type.video );
    output_format = pixel_format_from_video_format( params->output_type.video );
    if (input_format == AV_PIX_FMT_NONE && output_format == AV_PIX_FMT_NONE)
    {
        FIXME( "Unsupported media types!\n" );
        return STATUS_INVALID_PARAMETER;
    }

    if (input_format == AV_PIX_FMT_NONE)
        status = video_decoder_create( &params->input_type, &params->output_type, &transform );
    else if (output_format == AV_PIX_FMT_NONE)
        status = video_encoder_create( &params->input_type, &params->output_type, &transform );
    else status = video_converter_create( &params->input_type, &params->output_type, &transform );

    if (!status) params->transform.handle = (UINT_PTR)transform;
    return status;
}

NTSTATUS video_transform_check( struct transform_check_params *params )
{
    enum AVPixelFormat input_format, output_format;

    input_format = pixel_format_from_subtype( &params->input );
    output_format = pixel_format_from_subtype( &params->output );
    if (input_format == AV_PIX_FMT_NONE && output_format == AV_PIX_FMT_NONE)
    {
        FIXME( "Unsupported media types!\n" );
        return STATUS_INVALID_PARAMETER;
    }

    if (input_format == AV_PIX_FMT_NONE)
    {
        enum AVCodecID codec_id = codec_id_from_video_subtype( &params->input );
        if (codec_id == AV_CODEC_ID_NONE || !avcodec_find_decoder( codec_id ))
        {
            ERR( "Unsupported %s decoder.\n", avcodec_get_name( codec_id ) );
            return STATUS_NOT_SUPPORTED;
        }
    }
    if (output_format == AV_PIX_FMT_NONE)
    {
        enum AVCodecID codec_id = codec_id_from_video_subtype( &params->output );
        if (codec_id == AV_CODEC_ID_NONE || !avcodec_find_encoder( codec_id ))
        {
            ERR( "Unsupported %s encoder.\n", avcodec_get_name( codec_id ) );
            return STATUS_NOT_SUPPORTED;
        }
    }

    return STATUS_SUCCESS;
}

#endif /* HAVE_FFMPEG */
