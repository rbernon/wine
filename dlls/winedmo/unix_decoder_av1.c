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

#if defined(HAVE_FFMPEG) && defined(HAVE_DAV1D)

#include "unix_private.h"
#include "unix_decoder.h"

#include <dav1d/dav1d.h>

#include "wine/debug.h"
#include "wine/list.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmo);

struct av1_decoder
{
    struct decoder iface;
    Dav1dContext *context;
};

static struct av1_decoder *av1_decoder_from_decoder( struct decoder *iface )
{
    return CONTAINING_RECORD( iface, struct av1_decoder, iface );
}

int av1_decoder_destroy( struct decoder *iface )
{
    struct av1_decoder *decoder = av1_decoder_from_decoder( iface );

    TRACE( "decoder %p\n", decoder );

    dav1d_close( &decoder->context );
    free( decoder );
    return 0;
}

static void av1_packet_unref( const uint8_t *buf, void *cookie )
{
    AVPacket *packet = cookie;
    av_packet_free( &packet );
}

int av1_decoder_process_input( struct decoder *iface, AVPacket *packet )
{
    struct av1_decoder *decoder = av1_decoder_from_decoder( iface );
    AVPacket *tmp = av_packet_alloc();
    Dav1dData data = {0};
    int ret;

    TRACE( "decoder %p, packet %p, size %#x\n", decoder, packet, packet ? packet->size : 0 );

    av_packet_move_ref( tmp, packet );
    dav1d_data_wrap( &data, tmp->data, tmp->size, av1_packet_unref, tmp );
    if ((ret = dav1d_send_data( decoder->context, &data )))
    {
        av_packet_move_ref( packet, tmp );
        av_packet_free( &tmp );
    }

    if (ret == DAV1D_ERR(EAGAIN)) return AVERROR(EAGAIN);
    if (ret < 0) return ret;
    return 0;
}

static void av1_buffer_free( void *opaque, uint8_t *data )
{
    Dav1dPicture *picture = opaque;
    dav1d_picture_unref( picture );
    free( picture );
}

int av1_decoder_process_output( struct decoder *iface, AVFrame *frame )
{
    struct av1_decoder *decoder = av1_decoder_from_decoder( iface );
    Dav1dPicture *picture;
    size_t size;
    int ret;

    TRACE( "decoder %p, frame %p\n", decoder, frame );

    if (!(picture = calloc( 1, sizeof(*picture) ))) return AVERROR(ENOMEM);

    if ((ret = dav1d_get_picture( decoder->context, picture )))
    {
        free( picture );
        if (ret == DAV1D_ERR(EAGAIN)) return AVERROR(EAGAIN);
        return AVERROR(EINVAL);
    }

    switch (picture->p.layout)
    {
    case DAV1D_PIXEL_LAYOUT_I400: frame->format = AV_PIX_FMT_YUV420P; break;
    case DAV1D_PIXEL_LAYOUT_I420: frame->format = AV_PIX_FMT_YUV420P; break;
    case DAV1D_PIXEL_LAYOUT_I422: frame->format = AV_PIX_FMT_YUV422P; break;
    case DAV1D_PIXEL_LAYOUT_I444: frame->format = AV_PIX_FMT_YUV444P; break;
    }

    frame->width = picture->p.w;
    frame->height = picture->p.h;
    frame->data[0] = picture->data[0];
    frame->data[1] = picture->data[1];
    frame->data[2] = picture->data[1];
    frame->linesize[0] = picture->stride[0];
    frame->linesize[1] = picture->stride[1];
    frame->linesize[2] = picture->stride[1];

    size = (picture->stride[0] + picture->stride[1] + picture->stride[1]) * frame->height;
    frame->buf[0] = av_buffer_create( frame->data[0], size, av1_buffer_free, picture, 0 );
    frame->extended_data = frame->data;

    return 0;
}

int av1_decoder_flush( struct decoder *iface )
{
    struct av1_decoder *decoder = av1_decoder_from_decoder( iface );
    TRACE( "decoder %p\n", decoder );
    dav1d_flush( decoder->context );
    return 0;
}

static const struct decoder_ops av1_decoder_ops =
{
    av1_decoder_destroy,
    av1_decoder_process_input,
    av1_decoder_process_output,
    av1_decoder_flush,
};

NTSTATUS av1_decoder_create( const struct media_type *input_type, const AVCodecParameters *par,
                             AVRational sar, AVRational fps, struct decoder **out )
{
    struct av1_decoder *decoder;
    Dav1dSettings settings;
    NTSTATUS status = 0;

    if (!(decoder = calloc( 1, sizeof(*decoder) ))) return STATUS_NO_MEMORY;
    decoder->iface.ops = &av1_decoder_ops;

    dav1d_default_settings(&settings);
    dav1d_open(&decoder->context, &settings);

    status = decoder_context_create( NULL, par, sar, fps, &decoder->iface.context );
    if (status) free( decoder );
    else *out = &decoder->iface;

    return status;
}

#endif /* defined(HAVE_FFMPEG) && defined(HAVE_DAV1D) */
