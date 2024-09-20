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
#include "unix_private.h"

#ifdef HAVE_FFMPEG

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmo);

static AVFormatContext *get_muxer_context( struct winedmo_muxer muxer )
{
    return (AVFormatContext *)(UINT_PTR)muxer.handle;
}

NTSTATUS muxer_check( void *arg )
{
    struct demuxer_check_params *params = arg;
    const AVOutputFormat *format = av_guess_format( NULL, NULL, params->mime_type );
    return format ? STATUS_SUCCESS : STATUS_NOT_SUPPORTED;
}

NTSTATUS muxer_create( void *arg )
{
    struct muxer_create_params *params = arg;
    const AVOutputFormat *format;
    AVFormatContext *ctx;

    if (!(format = av_guess_format( NULL, params->url, params->mime_type )))
    {
        FIXME( "Output format not found for mime_type %s, url %s\n",
               debugstr_a( params->mime_type ), debugstr_a( params->url ) );
        return STATUS_NOT_IMPLEMENTED;
    }

    if (!(ctx = avformat_alloc_context())) return STATUS_NO_MEMORY;
    ctx->oformat = (AVOutputFormat *)format;

    if (!(ctx->pb = avio_alloc_context( NULL, 0, 0, params->context, unix_read_callback,
                                        unix_write_callback, unix_seek_callback )))
    {
        avformat_free_context( ctx );
        return STATUS_NO_MEMORY;
    }

    TRACE( "context %p\n", ctx );
    params->muxer.handle = (UINT_PTR)ctx;

    return STATUS_SUCCESS;
}

NTSTATUS muxer_destroy( void *arg )
{
    struct muxer_destroy_params *params = arg;
    AVFormatContext *ctx = get_muxer_context( params->muxer );
    AVIOContext *io = ctx->pb;

    TRACE( "context %p\n", ctx );

    params->context = io->opaque;
    if (ctx->nb_streams) av_write_trailer( ctx );
    avformat_free_context( ctx );

    return STATUS_SUCCESS;
}

NTSTATUS muxer_add_stream( void *arg )
{
    struct muxer_add_stream_params *params = arg;
    AVFormatContext *ctx = get_muxer_context( params->muxer );
    AVStream *stream;

    TRACE( "context %p\n", ctx );

    stream = avformat_new_stream( ctx, NULL );
    stream->id = params->stream_id;
    stream->codecpar = avcodec_parameters_alloc();
    codec_params_from_media_type( stream->codecpar, &stream->sample_aspect_ratio,
                                  &stream->avg_frame_rate, &params->media_type );
    stream->time_base = stream->avg_frame_rate;

    return STATUS_SUCCESS;
}

NTSTATUS muxer_start( void *arg )
{
    struct muxer_start_params *params = arg;
    AVFormatContext *ctx = get_muxer_context( params->muxer );
    int ret;

    TRACE( "context %p\n", ctx );

    ret = avformat_init_output( ctx, NULL );
    if (ret < 0) ERR( "error ret %d (%s)\n", -ret, av_err2str( ret ) );

    ret = avformat_write_header( ctx, NULL );
    if (ret < 0) ERR( "error ret %d (%s)\n", -ret, av_err2str( ret ) );

    return STATUS_SUCCESS;
}

NTSTATUS muxer_write( void *arg )
{
    struct muxer_write_params *params = arg;
    AVFormatContext *ctx = get_muxer_context( params->muxer );
    AVPacket *packet;

    TRACE( "context %p\n", ctx );

    packet = packet_from_sample( &params->sample );
    av_packet_rescale_ts( packet, AV_TIME_BASE_Q, ctx->streams[params->stream]->time_base );
    packet->stream_index = params->stream;

    av_interleaved_write_frame( ctx, packet );
    av_packet_free( &packet );

    return STATUS_SUCCESS;
}

#endif /* HAVE_FFMPEG */
