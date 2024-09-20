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

#define DEFINE_FUNCPTR
#include "unix_private.h"

#ifdef HAVE_FFMPEG

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmo);

NTSTATUS encoder_init( struct encoder *encoder, const struct media_type *input_type )
{
    AVCodecParameters *par;
    const AVCodec *codec;
    AVRational sar, fps;
    int ret;

    par = avcodec_parameters_alloc();
    codec_params_from_media_type( par, &sar, &fps, input_type );

    if (!(codec = avcodec_find_encoder( par->codec_id )))
    {
        ERR( "Failed to find encoder\n" );
        avcodec_parameters_free( &par );
        return STATUS_NOT_IMPLEMENTED;
    }
    if (!(encoder->codec = avcodec_alloc_context3( codec )))
    {
        ERR( "Failed to allocate encoder context\n" );
        avcodec_parameters_free( &par );
        return STATUS_NO_MEMORY;
    }

    if ((ret = avcodec_parameters_to_context( encoder->codec, par )) < 0)
    {
        ERR( "Failed to initialize encoder context, ret %d\n", ret );
        avcodec_parameters_free( &par );
        return STATUS_UNSUCCESSFUL;
    }
    avcodec_parameters_free( &par );

    encoder->codec->sample_aspect_ratio = sar;
    encoder->codec->framerate = fps;
    encoder->codec->pkt_timebase = AV_TIME_BASE_Q;
    if ((ret = avcodec_open2( encoder->codec, codec, NULL )) < 0)
    {
        ERR( "Failed to open encoder context, ret %d\n", ret );
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

NTSTATUS encoder_cleanup( struct encoder *encoder )
{
    avcodec_free_context( &encoder->codec );
    return STATUS_SUCCESS;
}

#endif /* HAVE_FFMPEG */
