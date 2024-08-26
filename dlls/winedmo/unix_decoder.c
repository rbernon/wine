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

static int ffmpeg_decoder_destroy( struct decoder *decoder )
{
    free( decoder );
    return 0;
}

static int ffmpeg_decoder_process_input( struct decoder *decoder, AVPacket *packet )
{
    return avcodec_send_packet( decoder->context, packet );
}

static int ffmpeg_decoder_process_output( struct decoder *decoder, AVFrame *frame )
{
    return avcodec_receive_frame( decoder->context, frame );
}

static int ffmpeg_decoder_flush( struct decoder *decoder )
{
    avcodec_flush_buffers( decoder->context );
    return 0;
}

static const struct decoder_ops ffmpeg_decoder_ops =
{
    ffmpeg_decoder_destroy,
    ffmpeg_decoder_process_input,
    ffmpeg_decoder_process_output,
    ffmpeg_decoder_flush,
};

static int context_create_from_parameters( const AVCodecParameters *par, AVRational sar, AVRational fps,
                                           AVCodecContext **context )
{
    static const AVRational USER_TIME_BASE_Q = {1, 10000000};
    const AVCodec *codec;
    int ret;

    if (!(codec = avcodec_find_decoder( par->codec_id )))
    {
        ERR( "Failed to find decoder\n" );
        return -1;
    }
    if (!(*context = avcodec_alloc_context3( codec )))
    {
        ERR( "Failed to allocate decoder context\n" );
        return -1;
    }
    if ((ret = avcodec_parameters_to_context( *context, par )) < 0)
    {
        ERR( "Failed to initialize decoder context, ret %d\n", ret );
        avcodec_free_context( context );
        return -1;
    }

    (*context)->sample_aspect_ratio = sar;
    (*context)->framerate = fps;
    (*context)->flags = AV_CODEC_FLAG_OUTPUT_CORRUPT;
    (*context)->pkt_timebase = USER_TIME_BASE_Q;
    (*context)->thread_type = FF_THREAD_FRAME;
    (*context)->thread_count = 4;

    if ((ret = avcodec_open2( *context, codec, NULL )) < 0)
    {
        ERR( "Failed to open decoder context, ret %d\n", ret );
        avcodec_free_context( context );
    }

    return 0;
}

static int ffmpeg_decoder_create( const AVCodecParameters *par, AVRational sar, AVRational fps, struct decoder **out )
{
    struct decoder *decoder;
    int ret;

    if (!(decoder = calloc( 1, sizeof(*decoder) ))) return -1;
    decoder->ops = &ffmpeg_decoder_ops;

    if ((ret = context_create_from_parameters( par, sar, fps, &decoder->context ))) free( decoder );
    else *out = decoder;

    return ret;
}

NTSTATUS decoder_create( const struct media_type *input_type, struct decoder **out )
{
    AVCodecParameters *par;
    AVRational sar, fps;
    NTSTATUS status;

    if (!(par = avcodec_parameters_alloc())) return STATUS_NO_MEMORY;
    if (!(status = codec_params_from_media_type( par, &sar, &fps, input_type )))
    {
        if (!ffmpeg_decoder_create( par, sar, fps, out )) status = STATUS_SUCCESS;
        else if (!vt_decoder_create( par, sar, fps, out )) status = STATUS_SUCCESS;
        else status = STATUS_UNSUCCESSFUL;
    }

    avcodec_parameters_free( &par );
    return status;
}

NTSTATUS decoder_destroy( struct decoder *decoder )
{
    avcodec_free_context( &decoder->context );
    decoder->ops->destroy( decoder );
    return STATUS_SUCCESS;
}

static NTSTATUS decoder_decode_packet( struct decoder *decoder, AVPacket *packet )
{
    int ret;

    TRACE( "decoder %p, packet %p\n", decoder, packet );

    ret = decoder->ops->process_input( decoder, packet );
    if (ret < 0) TRACE( "avcodec_send_packet returned %d (%s)\n", ret, av_err2str( ret ) );
    else TRACE( "avcodec_send_packet returned %d\n", ret );

    if (decoder->flushing) decoder->ops->flush( decoder );
    if (!packet) decoder->draining = decoder->flushing = 0;

    if (ret >= 0) return STATUS_SUCCESS;
    if (ret == AVERROR( EAGAIN ) || ret == AVERROR_EOF) return STATUS_PENDING;

    WARN( "error ret %d (%s)\n", -ret, av_err2str( ret ) );
    return ret == AVERROR_INVALIDDATA ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

static NTSTATUS decoder_decode_input_packet( struct decoder *decoder )
{
    UINT status;

    if (!decoder->input_packet) status = STATUS_SUCCESS;
    else
    {
        TRACE( "decoder %p, decoding %p %#x\n", decoder, decoder->input_packet->data,
               decoder->input_packet->size );
        status = decoder_decode_packet( decoder, decoder->input_packet );
        if (status != STATUS_PENDING) av_packet_free( &decoder->input_packet );
    }

    if (!status && decoder->draining) status = decoder_decode_packet( decoder, NULL );
    TRACE( "status %#x\n", status );
    return status;
}

NTSTATUS decoder_process_input( struct decoder *decoder, const struct sample *sample )
{
    UINT status;

    TRACE( "decoder %p size %jd\n", decoder, sample->size );

    /* decode any pending parsed packet, return if busy */
    if ((status = decoder_decode_input_packet( decoder ))) return status;

    decoder->input_packet = packet_from_sample( sample );
    return decoder_decode_input_packet( decoder );
}

NTSTATUS decoder_process_output( struct decoder *decoder, AVFrame *output_frame )
{
    int ret;

    if ((ret = decoder->ops->process_output( decoder, output_frame )) < 0)
    {
        WARN( "error ret %d (%s)\n", -ret, av_err2str( ret ) );
        if (ret == AVERROR_EOF) decoder->ops->flush( decoder );
        if (ret == AVERROR( EAGAIN ) || ret == AVERROR_EOF) return STATUS_PENDING;
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

NTSTATUS decoder_drain( struct decoder *decoder, BOOL discard )
{
    decoder->draining = 1;
    if ((decoder->flushing = discard)) decoder->ops->flush( decoder );
    while (!decoder_decode_input_packet( decoder ) && decoder->draining) /* nothing */;
    return STATUS_SUCCESS;
}

#endif /* HAVE_FFMPEG */
