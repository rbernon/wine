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

#ifndef __WINE_WINELIBAV_UNIXLIB_H
#define __WINE_WINELIBAV_UNIXLIB_H

#include <stddef.h>
#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "ntuser.h"
#include "mfapi.h"

#include "wine/unixlib.h"

struct process_attach_params
{
    UINT64 read_callback;
    UINT64 write_callback;
    UINT64 seek_callback;
};

struct io_context
{
    UINT64 stream;
    UINT64 length;
    UINT64 position;
    UINT64 buffer_size;
    BYTE buffer[];
};

C_ASSERT( sizeof(struct io_context) == offsetof( struct io_context, buffer[0] ) );

struct read_params
{
    struct dispatch_callback_params dispatch;
    UINT64 io_ctx;
    INT32 size;
};

struct write_params
{
    struct dispatch_callback_params dispatch;
    UINT64 io_ctx;
    INT32 size;
};

struct seek_params
{
    struct dispatch_callback_params dispatch;
    UINT64 io_ctx;
    INT64 offset;
};

/* same as MPEG1VIDEOINFO / MPEG2VIDEOINFO but with MFVIDEOFORMAT */
struct mpeg_video_format
{
    MFVIDEOFORMAT hdr;
    UINT32 start_time_code;
    UINT32 profile;
    UINT32 level;
    UINT32 flags;
    UINT32 sequence_header_count;
    UINT32 __pad;
    BYTE sequence_header[];
};

C_ASSERT( sizeof(struct mpeg_video_format) == offsetof( struct mpeg_video_format, sequence_header[0] ) );

typedef UINT64 demuxer_t;
typedef UINT64 packet_t;
typedef UINT64 frame_t;
typedef UINT64 muxer_t;

struct sample
{
    /* timestamp and duration are in 100-nanosecond units. */
    UINT64 dts;
    UINT64 pts;
    UINT64 duration;
    UINT64 size;
    UINT64 data; /* pointer to user memory */
};

struct demuxer_create_params
{
    struct io_context *io_ctx;
    const char *url;

    demuxer_t demuxer;
    char mime_type[256];
    UINT32 stream_count;
    INT64 duration;
};

struct demuxer_destroy_params
{
    demuxer_t demuxer;
    struct io_context *io_ctx;
};

struct demuxer_read_params
{
    demuxer_t demuxer;
    packet_t packet;
    UINT32 stream;
    struct sample sample;
};

struct demuxer_seek_params
{
    demuxer_t demuxer;
    INT64 timestamp;
};

struct demuxer_stream_lang_params
{
    demuxer_t demuxer;
    UINT32 stream;
    char buffer[3];
};

struct demuxer_stream_name_params
{
    demuxer_t demuxer;
    UINT32 stream;
    char buffer[256];
};

struct media_type
{
    GUID major;
    UINT32 format_size;
    union
    {
        void *format;
        WAVEFORMATEX *audio;
        MFVIDEOFORMAT *video;
        UINT64 __pad;
    };
};

struct demuxer_stream_type_params
{
    demuxer_t demuxer;
    UINT32 stream;
    struct media_type media_type;
};

struct muxer_create_params
{
    struct io_context *io_ctx;
    muxer_t muxer;
};

struct muxer_destroy_params
{
    muxer_t muxer;
    struct io_context *io_ctx;
};

struct muxer_add_stream_params
{
    muxer_t muxer;
};

struct muxer_start_params
{
    muxer_t muxer;
};

struct muxer_write_params
{
    muxer_t muxer;
    UINT32 stream;
    DWORD size;
    INT64 dts;
    INT64 pts;
    INT64 duration;
    BYTE *data;
};

typedef UINT64 audio_converter_t;

struct audio_converter_create_params
{
    audio_converter_t converter;
    struct media_type input;
    struct media_type output;
};

struct audio_converter_destroy_params
{
    audio_converter_t converter;
};

struct audio_converter_process_params
{
    audio_converter_t converter;
    struct sample input;
    struct sample output;
};

typedef UINT64 video_converter_t;

struct video_converter_create_params
{
    video_converter_t converter;
    struct media_type input;
    struct media_type output;
};

struct video_converter_destroy_params
{
    video_converter_t converter;
};

struct video_converter_process_params
{
    video_converter_t converter;
    struct sample input;
    struct sample output;
};

enum unix_funcs
{
    unix_process_attach,

    unix_packet_wrap,
    unix_packet_unwrap,
    unix_frame_wrap,
    unix_frame_unwrap,

    unix_demuxer_create,
    unix_demuxer_destroy,
    unix_demuxer_read,
    unix_demuxer_seek,
    unix_demuxer_stream_lang,
    unix_demuxer_stream_name,
    unix_demuxer_stream_type,

    unix_muxer_create,
    unix_muxer_destroy,
    unix_muxer_add_stream,
    unix_muxer_start,
    unix_muxer_write,

    unix_encoder_create,
    unix_decoder_create,
    unix_decoder_get_output_type,
    unix_decoder_set_output_type,
    unix_resampler_create,

    unix_audio_converter_create,
    unix_audio_converter_destroy,
    unix_audio_converter_process,

    unix_video_converter_create,
    unix_video_converter_destroy,
    unix_video_converter_process,

    unix_funcs_count,
};

#define UNIX_CALL( func, params ) WINE_UNIX_CALL( unix_##func, params )

#endif /* __WINE_WINELIBAV_UNIXLIB_H */
