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
#include "mfapi.h"

#include "wine/unixlib.h"

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
    DWORD size;
    INT64 dts;
    INT64 pts;
    INT64 duration;
    BYTE *data;
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
    } u;
};

struct demuxer_stream_type_params
{
    demuxer_t demuxer;
    UINT32 stream;
    struct media_type media_type;
};

enum unix_funcs
{
    unix_process_attach,
    unix_demuxer_create,
    unix_demuxer_destroy,
    unix_demuxer_read,
    unix_demuxer_seek,
    unix_demuxer_stream_lang,
    unix_demuxer_stream_name,
    unix_demuxer_stream_type,
    unix_funcs_count,
};

#define UNIX_CALL( func, params ) WINE_UNIX_CALL( unix_##func, params )

#endif /* __WINE_WINELIBAV_UNIXLIB_H */
