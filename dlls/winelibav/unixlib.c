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

#include <stddef.h>
#include <stdarg.h>
#include <assert.h>

#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "ntuser.h"

#include "initguid.h"
#include "d3d9.h"
#include "mfapi.h"
#include "uuids.h"

#include "wine/debug.h"
#include "unixlib.h"

WINE_DEFAULT_DEBUG_CHANNEL( libav );

#ifdef SONAME_LIBAVFORMAT

#define MAKE_FUNCPTR( f ) static typeof(f) *p_##f

#ifdef SONAME_LIBAVUTIL
#include <libavutil/avutil.h>
#include <libavutil/dict.h>
MAKE_FUNCPTR( av_log_set_callback );
MAKE_FUNCPTR( av_dict_get );
#endif /* SONAME_LIBAVUTIL */

#include <libavformat/avformat.h>
MAKE_FUNCPTR( avformat_version );
MAKE_FUNCPTR( avio_alloc_context );
MAKE_FUNCPTR( avio_context_free );
MAKE_FUNCPTR( avformat_alloc_context );
MAKE_FUNCPTR( avformat_free_context );
MAKE_FUNCPTR( avformat_open_input );
MAKE_FUNCPTR( avformat_find_stream_info );
MAKE_FUNCPTR( avformat_close_input );
MAKE_FUNCPTR( av_read_frame );
MAKE_FUNCPTR( av_seek_frame );
MAKE_FUNCPTR( av_packet_alloc );
MAKE_FUNCPTR( av_packet_unref );

static void *avformat_handle;
static void *avutil_handle;
static const GUID GUID_NULL;

static inline const char *debugstr_ratio( const MFRatio *ratio )
{
    return wine_dbg_sprintf( "%d:%d", (int)ratio->Numerator, (int)ratio->Denominator );
}

static inline const char *debugstr_area( const MFVideoArea *area )
{
    return wine_dbg_sprintf( "(%d,%d)-(%d,%d)", (int)area->OffsetX.value, (int)area->OffsetY.value,
                             (int)area->Area.cx, (int)area->Area.cy );
}

DEFINE_MEDIATYPE_GUID( MFVideoFormat_CVID, MAKEFOURCC( 'c', 'v', 'i', 'd' ) );
DEFINE_MEDIATYPE_GUID( MFVideoFormat_ABGR32, D3DFMT_A8B8G8R8 );

static inline void hexdump( const char *msg, const void *data, UINT size )
{
    const unsigned char *ptr = (void *)data, *end = ptr + size;
    TRACE( "%s: %p-%p\n", msg, data, end );
    for (int i = 0, j; ptr + i < end;)
    {
        char buffer[256], *buf = buffer;
        buf += sprintf( buf, "%08x ", i );
        for (j = 0; j < 8 && ptr + i + j < end; ++j) buf += sprintf( buf, " %02x", ptr[i + j] );
        for (; j < 8 && ptr + i + j >= end; ++j) buf += sprintf( buf, "   " );
        buf += sprintf( buf, " " );
        for (j = 8; j < 16 && ptr + i + j < end; ++j) buf += sprintf( buf, " %02x", ptr[i + j] );
        for (; j < 16 && ptr + i + j >= end; ++j) buf += sprintf( buf, "   " );
        buf += sprintf( buf, "  |" );
        for (j = 0; j < 16 && ptr + i < end; ++j, ++i)
            buf += sprintf( buf, "%c", ptr[i] >= ' ' && ptr[i] <= '~' ? ptr[i] : '.' );
        buf += sprintf( buf, "|" );
        TRACE( "%s\n", buffer );
    }
}

static WORD wave_format_tag_from_codec_id( enum AVCodecID id )
{
    switch (id)
    {
    case AV_CODEC_ID_PCM_U8: return WAVE_FORMAT_PCM;
    case AV_CODEC_ID_PCM_S16LE: return WAVE_FORMAT_PCM;
    case AV_CODEC_ID_PCM_S32LE: return WAVE_FORMAT_PCM;
    case AV_CODEC_ID_PCM_S24LE: return WAVE_FORMAT_PCM;
    case AV_CODEC_ID_PCM_F32LE: return WAVE_FORMAT_IEEE_FLOAT;
    case AV_CODEC_ID_PCM_F64LE: return WAVE_FORMAT_IEEE_FLOAT;
    default: return WAVE_FORMAT_EXTENSIBLE;
    }
}

static WORD depth_from_codec_id( enum AVCodecID id )
{
    switch (id)
    {
    case AV_CODEC_ID_PCM_U8: return 8;
    case AV_CODEC_ID_PCM_S16LE: return 16;
    case AV_CODEC_ID_PCM_S32LE: return 32;
    case AV_CODEC_ID_PCM_S24LE: return 24;
    case AV_CODEC_ID_PCM_F32LE: return 32;
    case AV_CODEC_ID_PCM_F64LE: return 64;
    default: return 0;
    }
}

static GUID subtype_from_codec_id( enum AVCodecID id )
{
    switch (id)
    {
    case AV_CODEC_ID_PCM_U8: return MFAudioFormat_PCM;
    case AV_CODEC_ID_PCM_S16LE: return MFAudioFormat_PCM;
    case AV_CODEC_ID_PCM_S32LE: return MFAudioFormat_PCM;
    case AV_CODEC_ID_PCM_S24LE: return MFAudioFormat_PCM;
    case AV_CODEC_ID_PCM_F32LE: return MFAudioFormat_Float;
    case AV_CODEC_ID_PCM_F64LE: return MFAudioFormat_Float;
    default: return GUID_NULL;
    }
}

static void init_wave_format_ex( const AVStream *stream, WORD format_tag, WAVEFORMATEX *format, UINT32 format_size )
{
    const AVCodecParameters *codec = stream->codecpar;

    memset( format, 0, format_size );
    format->cbSize = format_size - sizeof(*format);
    format->wFormatTag = format_tag;
    format->nChannels = codec->ch_layout.nb_channels;
    format->nSamplesPerSec = codec->sample_rate;

    if (!(format->wBitsPerSample = depth_from_codec_id( codec->codec_id )) &&
        !(format->wBitsPerSample = codec->bits_per_coded_sample))
        format->wBitsPerSample = codec->bits_per_raw_sample;

    if (!(format->nBlockAlign = codec->block_align))
        format->nBlockAlign = format->wBitsPerSample * format->nChannels / 8;

    if (!(format->nAvgBytesPerSec = codec->bit_rate / 8))
        format->nAvgBytesPerSec = format->nSamplesPerSec * format->nBlockAlign;
}

static NTSTATUS stream_type_wave_format_extensible( const AVStream *stream, const GUID *subtype, UINT64 channel_mask,
                                                    WAVEFORMATEXTENSIBLE *format, UINT32 *format_size )
{
    const AVCodecParameters *codec = stream->codecpar;
    UINT32 capacity = *format_size;

    *format_size = sizeof(*format) + codec->extradata_size;
    if (*format_size > capacity) return STATUS_BUFFER_TOO_SMALL;

    init_wave_format_ex( stream, WAVE_FORMAT_EXTENSIBLE, &format->Format, *format_size );
    if (codec->extradata_size && codec->extradata) memcpy( format + 1, codec->extradata, codec->extradata_size );
    format->Samples.wValidBitsPerSample = 0;
    format->dwChannelMask = channel_mask;
    format->SubFormat = *subtype;

    TRACE( "tag %#x, %u channels, sample rate %u, %u bytes/sec, alignment %u, %u bits/sample, "
           "%u valid bps, channel mask %#x, subtype %s.\n",
           format->Format.wFormatTag, format->Format.nChannels, (int)format->Format.nSamplesPerSec,
           (int)format->Format.nAvgBytesPerSec, format->Format.nBlockAlign,
           format->Format.wBitsPerSample, format->Samples.wValidBitsPerSample,
           (int)format->dwChannelMask, debugstr_guid( &format->SubFormat ) );
    if (format->Format.cbSize && TRACE_ON(libav))
    {
        UINT extra_size = sizeof(WAVEFORMATEX) + format->Format.cbSize - sizeof(WAVEFORMATEXTENSIBLE);
        hexdump( "  Extra", format + 1, extra_size );
    }

    return STATUS_SUCCESS;
}

static NTSTATUS stream_type_wave_format_ex( const AVStream *stream, WORD format_tag, UINT32 wave_format_size,
                                            WAVEFORMATEX *format, UINT32 *format_size )
{
    const AVCodecParameters *codec = stream->codecpar;
    UINT32 capacity = *format_size;

    *format_size = max( wave_format_size, sizeof(*format) + codec->extradata_size );
    if (*format_size > capacity) return STATUS_BUFFER_TOO_SMALL;

    init_wave_format_ex( stream, format_tag, format, *format_size );
    if (codec->extradata_size && codec->extradata) memcpy( format + 1, codec->extradata, codec->extradata_size );

    TRACE( "tag %#x, %u channels, sample rate %u, %u bytes/sec, alignment %u, %u bits/sample.\n",
           format->wFormatTag, format->nChannels, (int)format->nSamplesPerSec,
           (int)format->nAvgBytesPerSec, format->nBlockAlign, format->wBitsPerSample );
    if (format->cbSize && TRACE_ON(libav)) hexdump( "  Extra", format + 1, format->cbSize );

    return STATUS_SUCCESS;
}

static NTSTATUS stream_type_wave_format( const AVStream *stream, void *format, UINT32 *format_size )
{
    const AVCodecParameters *codec = stream->codecpar;
    WORD format_tag = WAVE_FORMAT_EXTENSIBLE;
    UINT64 channel_mask;
    int channels;

    format_tag = wave_format_tag_from_codec_id( codec->codec_id );

    if (!(channels = codec->ch_layout.nb_channels)) channels = 1;
    if (codec->ch_layout.order != AV_CHANNEL_ORDER_NATIVE || !(channel_mask = codec->ch_layout.u.mask))
        channel_mask = 0;

    if (format_tag == WAVE_FORMAT_EXTENSIBLE || channel_mask != 0)
    {
        GUID subtype = subtype_from_codec_id( codec->codec_id );
        return stream_type_wave_format_extensible( stream, &subtype, channel_mask, format, format_size );
    }

    return stream_type_wave_format_ex( stream, format_tag, sizeof(WAVEFORMATEX), format, format_size );
}

static NTSTATUS stream_type_heaac_wave_format( const AVStream *stream, HEAACWAVEFORMAT *format, UINT32 *format_size )
{
    const AVCodecParameters *codec = stream->codecpar;
    UINT32 capacity = *format_size;

    *format_size = max( sizeof(*format), sizeof(format->wfInfo) + codec->extradata_size );
    if (*format_size > capacity) return STATUS_BUFFER_TOO_SMALL;

    init_wave_format_ex( stream, WAVE_FORMAT_MPEG_HEAAC, &format->wfInfo.wfx, *format_size );
    if (codec->extradata_size && codec->extradata) memcpy( format->pbAudioSpecificConfig, codec->extradata, codec->extradata_size );
    format->wfInfo.wPayloadType = 0;
    format->wfInfo.wAudioProfileLevelIndication = 0;
    format->wfInfo.wStructType = 0;

    TRACE( "tag %#x, %u channels, sample rate %u, %u bytes/sec, alignment %u, %u bits/sample, payload %#x, level %#x, struct %#x.\n",
           format->wfInfo.wfx.wFormatTag, format->wfInfo.wfx.nChannels, (int)format->wfInfo.wfx.nSamplesPerSec,
           (int)format->wfInfo.wfx.nAvgBytesPerSec, format->wfInfo.wfx.nBlockAlign, format->wfInfo.wfx.wBitsPerSample,
           format->wfInfo.wPayloadType, format->wfInfo.wAudioProfileLevelIndication, format->wfInfo.wStructType );
    if (format->wfInfo.wfx.cbSize && TRACE_ON(libav))
    {
        UINT extra_size = sizeof(WAVEFORMATEX) + format->wfInfo.wfx.cbSize - sizeof(format->wfInfo);
        hexdump( "  Extra", format->pbAudioSpecificConfig, extra_size );
    }

    return STATUS_SUCCESS;
}

static GUID subtype_from_video_format( enum AVPixelFormat fmt )
{
    switch (fmt)
    {
    case AV_PIX_FMT_YUV420P: return MFVideoFormat_I420;
    case AV_PIX_FMT_YUVJ420P: return MFVideoFormat_I420;
    case AV_PIX_FMT_YUYV422: return MFVideoFormat_YUY2;
    case AV_PIX_FMT_UYVY422: return MFVideoFormat_UYVY;
    case AV_PIX_FMT_BGR0: return MFVideoFormat_RGB32;
    case AV_PIX_FMT_RGBA: return MFVideoFormat_ABGR32;
    case AV_PIX_FMT_BGRA: return MFVideoFormat_ARGB32;
    case AV_PIX_FMT_BGR24: return MFVideoFormat_RGB24;
    case AV_PIX_FMT_NV12: return MFVideoFormat_NV12;
    case AV_PIX_FMT_NV21: return MFVideoFormat_NV21;
    case AV_PIX_FMT_RGB565: return MFVideoFormat_RGB565;
    case AV_PIX_FMT_RGB555: return MFVideoFormat_RGB555;
    default: return GUID_NULL;
    }
}

static void init_video_format( const AVStream *stream, const GUID *subtype, MFVIDEOFORMAT *format,
                               UINT32 format_size, UINT32 video_plane_align )
{
    const AVCodecParameters *codec = stream->codecpar;

    memset( format, 0, format_size );
    format->dwSize = format_size;

    if (subtype) format->guidFormat = *subtype;
    else if (codec->format != AV_PIX_FMT_NONE) format->guidFormat = subtype_from_video_format( codec->format );
    else if (codec->codec_tag)
    {
        format->guidFormat = MFVideoFormat_Base;
        format->guidFormat.Data1 = codec->codec_tag;
    }

    if (codec->width)
        format->videoInfo.dwWidth = (codec->width + video_plane_align) & ~video_plane_align;
    if (codec->height)
        format->videoInfo.dwHeight = (codec->height + video_plane_align) & ~video_plane_align;
    if (format->videoInfo.dwWidth != codec->width || format->videoInfo.dwHeight != codec->height)
    {
        format->videoInfo.MinimumDisplayAperture.Area.cx = codec->width;
        format->videoInfo.MinimumDisplayAperture.Area.cy = codec->height;
    }
    format->videoInfo.GeometricAperture = format->videoInfo.MinimumDisplayAperture;
    format->videoInfo.PanScanAperture = format->videoInfo.MinimumDisplayAperture;

    if (codec->sample_aspect_ratio.num && codec->sample_aspect_ratio.den)
    {
        format->videoInfo.PixelAspectRatio.Numerator = codec->sample_aspect_ratio.num;
        format->videoInfo.PixelAspectRatio.Denominator = codec->sample_aspect_ratio.den;
    }
    else if (stream->sample_aspect_ratio.num && stream->sample_aspect_ratio.den)
    {
        format->videoInfo.PixelAspectRatio.Numerator = stream->sample_aspect_ratio.num;
        format->videoInfo.PixelAspectRatio.Denominator = stream->sample_aspect_ratio.den;
    }
    else
    {
        format->videoInfo.PixelAspectRatio.Numerator = 1;
        format->videoInfo.PixelAspectRatio.Denominator = 1;
    }

    if (codec->framerate.num && codec->framerate.den)
    {
        format->videoInfo.FramesPerSecond.Numerator = codec->framerate.num;
        format->videoInfo.FramesPerSecond.Denominator = codec->framerate.den;
    }
    else if (stream->avg_frame_rate.num && stream->avg_frame_rate.den)
    {
        format->videoInfo.FramesPerSecond.Numerator = stream->avg_frame_rate.num;
        format->videoInfo.FramesPerSecond.Denominator = stream->avg_frame_rate.den;
    }
}

static NTSTATUS stream_type_video_format( const AVStream *stream, const GUID *subtype, MFVIDEOFORMAT *format,
                                          UINT32 *format_size, UINT32 video_plane_align )
{
    const AVCodecParameters *codec = stream->codecpar;
    UINT32 capacity = *format_size;

    *format_size = sizeof(*format) + codec->extradata_size;
    if (*format_size > capacity) return STATUS_BUFFER_TOO_SMALL;

    init_video_format( stream, subtype, format, *format_size, video_plane_align );
    if (codec->extradata_size && codec->extradata) memcpy( format + 1, codec->extradata, codec->extradata_size );

    TRACE( "subtype %s %ux%u, FPS %s, aperture %s, PAR %s, videoFlags %#x.\n",
           debugstr_guid( &format->guidFormat ), (int)format->videoInfo.dwWidth,
           (int)format->videoInfo.dwHeight, debugstr_ratio( &format->videoInfo.FramesPerSecond ),
           debugstr_area( &format->videoInfo.MinimumDisplayAperture ),
           debugstr_ratio( &format->videoInfo.PixelAspectRatio ), (int)format->videoInfo.VideoFlags );
    if (format->dwSize > sizeof(*format) && TRACE_ON(libav)) hexdump( "  Extra", format + 1, format->dwSize - sizeof(*format) );

    return STATUS_SUCCESS;
}

static NTSTATUS stream_type_mpeg_video_format( const AVStream *stream, struct mpeg_video_format *format,
                                               UINT32 *format_size, UINT32 video_plane_align )
{
    const AVCodecParameters *codec = stream->codecpar;
    UINT32 capacity = *format_size;

    *format_size = sizeof(*format) + codec->extradata_size;
    if (*format_size > capacity) return STATUS_BUFFER_TOO_SMALL;

    init_video_format( stream, &MEDIASUBTYPE_MPEG1Payload, &format->hdr, *format_size, video_plane_align );
    if (codec->extradata_size && codec->extradata)
    {
        memcpy( format->sequence_header, codec->extradata, codec->extradata_size );
        format->sequence_header_count = codec->extradata_size;
    }

    TRACE( "subtype %s %ux%u, FPS %s, aperture %s, PAR %s, videoFlags %#x, start_time_code %u, profile %u, level %u, flags %#x.\n",
           debugstr_guid( &format->hdr.guidFormat ), (int)format->hdr.videoInfo.dwWidth,
           (int)format->hdr.videoInfo.dwHeight, debugstr_ratio( &format->hdr.videoInfo.FramesPerSecond ),
           debugstr_area( &format->hdr.videoInfo.MinimumDisplayAperture ),
           debugstr_ratio( &format->hdr.videoInfo.PixelAspectRatio ), (int)format->hdr.videoInfo.VideoFlags,
           format->start_time_code, format->profile, format->level, format->flags );
    if (format->sequence_header_count && TRACE_ON(libav)) hexdump( "  Extra", format->sequence_header, format->sequence_header_count );

    return STATUS_SUCCESS;
}

static void log_callback( void *ctx, int level, const char *fmt, va_list va_args )
{
    enum __wine_debug_class dbcl;
    if (level <= AV_LOG_ERROR) dbcl = __WINE_DBCL_ERR;
    if (level <= AV_LOG_WARNING) dbcl = __WINE_DBCL_WARN;
    if (level <= AV_LOG_TRACE) dbcl = __WINE_DBCL_TRACE;
    wine_dbg_vlog( dbcl, __wine_dbch___default, __FILE__, __LINE__, __func__,
                   __WINE_DBG_RETADDR, fmt, va_args );
}

static NTSTATUS process_attach( void *params )
{
#ifdef SONAME_LIBAVUTIL
    if (!(avutil_handle = dlopen( SONAME_LIBAVUTIL, RTLD_NOW )))
    {
        ERR( "Failed to load %s\n", SONAME_LIBAVUTIL );
        goto failed;
    }

#define LOAD_FUNCPTR( f )                                                                          \
    if (!(p_##f = dlsym( avutil_handle, #f )))                                                     \
    {                                                                                              \
        ERR( "Failed to find " #f "\n" );                                                          \
        goto failed;                                                                               \
    }

    LOAD_FUNCPTR( av_log_set_callback );
    LOAD_FUNCPTR( av_dict_get );
#undef LOAD_FUNCPTR

    p_av_log_set_callback( log_callback );
    TRACE( "Loaded %s\n", SONAME_LIBAVUTIL );

#endif /* SONAME_LIBAVUTIL */

    if (!(avformat_handle = dlopen( SONAME_LIBAVFORMAT, RTLD_NOW )))
    {
        ERR( "Failed to load %s\n", SONAME_LIBAVFORMAT );
        goto failed;
    }

#define LOAD_FUNCPTR( f )                                                                          \
    if (!(p_##f = dlsym( avformat_handle, #f )))                                                   \
    {                                                                                              \
        ERR( "Failed to find " #f "\n" );                                                          \
        goto failed;                                                                               \
    }

    LOAD_FUNCPTR( avformat_version );
    LOAD_FUNCPTR( avio_alloc_context );
    LOAD_FUNCPTR( avio_context_free );
    LOAD_FUNCPTR( avformat_alloc_context );
    LOAD_FUNCPTR( avformat_free_context );
    LOAD_FUNCPTR( avformat_open_input );
    LOAD_FUNCPTR( avformat_find_stream_info );
    LOAD_FUNCPTR( avformat_close_input );
    LOAD_FUNCPTR( av_read_frame );
    LOAD_FUNCPTR( av_seek_frame );
    LOAD_FUNCPTR( av_packet_alloc );
    LOAD_FUNCPTR( av_packet_unref );
#undef LOAD_FUNCPTR

    TRACE( "Loaded %s\n", SONAME_LIBAVFORMAT );
    return STATUS_SUCCESS;

failed:
    if (avformat_handle)
    {
        dlclose( avformat_handle );
        avformat_handle = NULL;
    }
    if (avutil_handle)
    {
        dlclose( avutil_handle );
        avutil_handle = NULL;
    }
    return STATUS_NOT_FOUND;
}

static int call_io_read( void *opaque, uint8_t *buf, int size )
{
    struct call_io_read_params params = {.context = (UINT_PTR)opaque};
    struct io_context *io_ctx = opaque;
    ULONG ret_len, ret, total = 0;
    void *ret_ptr;

    TRACE( "opaque %p, buf %p, size %#x\n", opaque, buf, size );

    do
    {
        params.size = min( size, io_ctx->buffer_size );
        KeUserModeCallback( NtUserCallIOReadCallback, &params, sizeof(params), &ret_ptr, &ret_len );
        if (ret_len != sizeof(ULONG)) return AVERROR( EINVAL );

        if (!(ret = *(ULONG *)ret_ptr)) return total ? total : AVERROR_EOF;
        memcpy( buf, io_ctx->buffer, ret );
        io_ctx->position += ret;
        total += ret;

        if (ret < io_ctx->buffer_size) break;
        size -= io_ctx->buffer_size;
        buf += io_ctx->buffer_size;
    } while (size);

    return total;
}

#if FF_API_AVIO_WRITE_NONCONST
static int call_io_write( void *opaque, uint8_t *buf, int size )
#else
static int call_io_write( void *opaque, const uint8_t *buf, int size )
#endif
{
    struct call_io_read_params params = {.context = (UINT_PTR)opaque};
    struct io_context *io_ctx = opaque;
    ULONG ret_len, ret, total = 0;
    void *ret_ptr;

    TRACE( "opaque %p, buf %p, size %#x\n", opaque, buf, size );

    do
    {
        params.size = min( size, io_ctx->buffer_size );
        memcpy( io_ctx->buffer, buf, params.size );
        KeUserModeCallback( NtUserCallIOWriteCallback, &params, sizeof(params), &ret_ptr, &ret_len );
        if (ret_len != sizeof(ULONG)) return AVERROR( EINVAL );

        if (!(ret = *(ULONG *)ret_ptr)) return total ? total : AVERROR_EOF;
        io_ctx->position += ret;
        total += ret;

        if (ret < io_ctx->buffer_size) break;
        size -= io_ctx->buffer_size;
        buf += io_ctx->buffer_size;
    } while (size);

    return total;
}

static int64_t call_io_seek( void *opaque, int64_t offset, int whence )
{
    struct call_io_seek_params params = {.context = (UINT_PTR)opaque, .offset = offset};
    struct io_context *io_ctx = opaque;
    void *ret_ptr;
    ULONG ret_len;

    TRACE( "opaque %p, offset 0x%s, whence %#x\n", opaque, wine_dbgstr_longlong( offset ), whence );

    if (whence == AVSEEK_SIZE) return io_ctx->total_size;
    if (whence == SEEK_END) params.offset += io_ctx->total_size;
    if (whence == SEEK_CUR) params.offset += io_ctx->position;

    KeUserModeCallback( NtUserCallIOSeekCallback, &params, sizeof(params), &ret_ptr, &ret_len );
    if (ret_len != sizeof(UINT64)) return AVERROR( EINVAL );

    offset = *(UINT64 *)ret_ptr;
    io_ctx->position = offset;
    return offset;
}

static AVFormatContext *get_context( demuxer_t handle )
{
    return (AVFormatContext *)(UINT_PTR)handle;
}

static AVPacket *get_packet( packet_t handle )
{
    return (AVPacket *)(UINT_PTR)handle;
}

static INT64 demuxer_stream_time( const AVStream *stream, INT64 time )
{
    if (time == AV_NOPTS_VALUE) return INT64_MIN;
    if (!stream->time_base.num || !stream->time_base.den) return time;
    return time * stream->time_base.num * 10000000 / stream->time_base.den;
}

static INT64 get_context_duration( const AVFormatContext *ctx )
{
    INT64 i, max_duration = 0;

    if (ctx->duration != AV_NOPTS_VALUE) return ctx->duration * 10000000 / AV_TIME_BASE;

    for (i = 0; i < ctx->nb_streams; i++)
    {
        AVStream *stream = ctx->streams[i];
        INT64 duration = demuxer_stream_time( stream, stream->duration );
        if (duration == INT64_MIN || duration < max_duration) continue;
        max_duration = duration;
    }

    return max_duration;
}

static NTSTATUS demuxer_create( void *arg )
{
    struct demuxer_create_params *params = arg;
    AVFormatContext *ctx;
    int ret;

    TRACE( "io_ctx %p\n", params->io_ctx );

    if (!(ctx = p_avformat_alloc_context())) return STATUS_NO_MEMORY;
    if (!(ctx->pb = p_avio_alloc_context( NULL, 0, 0, params->io_ctx, call_io_read, call_io_write, call_io_seek )))
    {
        p_avformat_free_context( ctx );
        return STATUS_NO_MEMORY;
    }

    if ((ret = p_avformat_open_input( &ctx, NULL, NULL, NULL )) < 0)
    {
        ERR( "Failed to open input, ret %d.\n", ret );
        p_avformat_free_context( ctx );
        return STATUS_UNSUCCESSFUL;
    }

    if (!ctx->nb_streams && (ret = p_avformat_find_stream_info( ctx, NULL )) < 0)
    {
        ERR( "Failed to find stream info, ret %d.\n", ret );
        p_avformat_free_context( ctx );
        return STATUS_UNSUCCESSFUL;
    }

    params->demuxer = (UINT_PTR)ctx;
    params->duration = get_context_duration( ctx );
    params->stream_count = ctx->nb_streams;
    if (strstr( ctx->iformat->name, "mp4" )) strcpy( params->mime_type, "video/mp4" );
    else if (strstr( ctx->iformat->name, "avi" )) strcpy( params->mime_type, "video/avi" );
    else strcpy( params->mime_type, "video/x-raw" );

    return STATUS_SUCCESS;
}

static NTSTATUS demuxer_destroy( void *arg )
{
    struct demuxer_destroy_params *params = arg;
    AVFormatContext *ctx = get_context( params->demuxer );
    AVIOContext *io = ctx->pb;

    TRACE( "context %p\n", ctx );

    params->io_ctx = io->opaque;
    p_avformat_close_input( &ctx );
    p_avio_context_free( &io );

    return STATUS_SUCCESS;
}

static NTSTATUS demuxer_read( void *arg )
{
    struct demuxer_read_params *params = arg;
    AVFormatContext *ctx = get_context( params->demuxer );
    AVPacket *packet = get_packet( params->packet );
    UINT capacity = params->size;
    AVStream *stream;
    int ret;

    TRACE( "context %p, packet %p, capacity %#x\n", ctx, packet, capacity );

    if (!packet)
    {
        if (!(packet = p_av_packet_alloc())) return STATUS_NO_MEMORY;
        if ((ret = p_av_read_frame( ctx, packet )) < 0)
        {
            TRACE( "av_read_frame index %u returned %#x\n", packet->stream_index, -ret );
            p_av_packet_unref( packet );
            if (ret == AVERROR_EOF) return STATUS_END_OF_FILE;
            return STATUS_UNSUCCESSFUL;
        }

        params->packet = (UINT_PTR)packet;
        params->size = packet->size;
    }

    if ((capacity < packet->size)) return STATUS_BUFFER_TOO_SMALL;
    stream = ctx->streams[packet->stream_index];

    memcpy( params->data, packet->data, packet->size );
    params->stream = packet->stream_index;
    params->pts = demuxer_stream_time( stream, packet->pts );
    params->dts = demuxer_stream_time( stream, packet->dts );
    params->duration = demuxer_stream_time( stream, packet->duration );

    p_av_packet_unref( packet );
    return STATUS_SUCCESS;
}

static NTSTATUS demuxer_seek( void *arg )
{
    struct demuxer_seek_params *params = arg;
    AVFormatContext *ctx = get_context( params->demuxer );
    int64_t timestamp = params->timestamp * AV_TIME_BASE / 10000000;
    int ret;

    TRACE( "context %p, timestamp 0x%s\n", ctx, wine_dbgstr_longlong( params->timestamp ) );

    if ((ret = p_av_seek_frame( ctx, -1, timestamp, AVSEEK_FLAG_ANY )) < 0)
    {
        ERR( "Failed to seek context %p, ret %d\n", ctx, ret );
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

static NTSTATUS demuxer_stream_lang( void *arg )
{
    struct demuxer_stream_lang_params *params = arg;
    AVFormatContext *ctx = get_context( params->demuxer );
    AVStream *stream = ctx->streams[params->stream];
    AVDictionaryEntry *tag;

    TRACE( "context %p, stream %u\n", ctx, params->stream );

    if (!(tag = p_av_dict_get( stream->metadata, "language", NULL, AV_DICT_IGNORE_SUFFIX )))
        return STATUS_NOT_FOUND;

    lstrcpynA( params->buffer, tag->value, ARRAY_SIZE( params->buffer ) );
    return STATUS_SUCCESS;
}

static NTSTATUS demuxer_stream_name( void *arg )
{
    struct demuxer_stream_name_params *params = arg;
    AVFormatContext *ctx = get_context( params->demuxer );
    AVStream *stream = ctx->streams[params->stream];
    AVDictionaryEntry *tag;

    TRACE( "context %p, stream %u\n", ctx, params->stream );

    if (!(tag = p_av_dict_get( stream->metadata, "title", NULL, AV_DICT_IGNORE_SUFFIX )))
        return STATUS_NOT_FOUND;

    lstrcpynA( params->buffer, tag->value, ARRAY_SIZE( params->buffer ) );
    return STATUS_SUCCESS;
}

static NTSTATUS demuxer_stream_type( void *arg )
{
    struct demuxer_stream_type_params *params = arg;
    AVFormatContext *ctx = get_context( params->demuxer );
    AVStream *stream = ctx->streams[params->stream];
    const AVCodecParameters *codec = stream->codecpar;
    struct media_type *media_type = &params->media_type;

    TRACE( "context %p, stream %u, stream %p, index %u\n", ctx, params->stream, stream, stream->index );

    if (codec->codec_type == AVMEDIA_TYPE_AUDIO)
    {
        media_type->major = MFMediaType_Audio;

        TRACE( "codec type %#x, id %#x, tag %#x (%s)\n", codec->codec_type, codec->codec_id,
               codec->codec_tag, debugstr_an( (char *)&codec->codec_tag, 4 ) );

        if (codec->codec_id == AV_CODEC_ID_MP2)
            return stream_type_wave_format_ex( stream, WAVE_FORMAT_MPEG, sizeof(MPEG1WAVEFORMAT),
                                               media_type->u.audio, &media_type->format_size );
        if (codec->codec_id == AV_CODEC_ID_MP3)
            return stream_type_wave_format_ex( stream, WAVE_FORMAT_MPEGLAYER3, sizeof(MPEGLAYER3WAVEFORMAT),
                                               media_type->u.audio, &media_type->format_size );
        if (codec->codec_id == AV_CODEC_ID_WMAV1)
            return stream_type_wave_format_ex( stream, WAVE_FORMAT_MSAUDIO1, sizeof(MSAUDIO1WAVEFORMAT),
                                               media_type->u.audio, &media_type->format_size );
        if (codec->codec_id == AV_CODEC_ID_WMAV2)
            return stream_type_wave_format_ex( stream, WAVE_FORMAT_WMAUDIO2, sizeof(WMAUDIO2WAVEFORMAT),
                                               media_type->u.audio, &media_type->format_size );
        if (codec->codec_id == AV_CODEC_ID_WMAPRO)
            return stream_type_wave_format_ex( stream, WAVE_FORMAT_WMAUDIO3, sizeof(WMAUDIO3WAVEFORMAT),
                                               media_type->u.audio, &media_type->format_size );
        if (codec->codec_id == AV_CODEC_ID_WMALOSSLESS)
            return stream_type_wave_format_ex( stream, WAVE_FORMAT_WMAUDIO_LOSSLESS, sizeof(WMAUDIO3WAVEFORMAT),
                                               media_type->u.audio, &media_type->format_size );
        if (codec->codec_id == AV_CODEC_ID_WMAVOICE)
            return stream_type_wave_format_ex( stream, WAVE_FORMAT_WMAVOICE9, sizeof(WMAUDIO3WAVEFORMAT),
                                               media_type->u.audio, &media_type->format_size );
        if (codec->codec_id >= AV_CODEC_ID_PCM_S16LE && codec->codec_id <= AV_CODEC_ID_PCM_SGA)
            return stream_type_wave_format( stream, media_type->u.audio, &media_type->format_size );

        if (codec->codec_id == AV_CODEC_ID_AAC)
            return stream_type_heaac_wave_format( stream, media_type->u.format, &media_type->format_size );

        if (codec->codec_tag)
            return stream_type_wave_format_ex( stream, codec->codec_tag, sizeof(WAVEFORMATEX),
                                               media_type->u.audio, &media_type->format_size );

        FIXME( "Unknown audio codec id %#x, tag %#x (%s)\n", codec->codec_id, codec->codec_tag,
               debugstr_an( (char *)&codec->codec_tag, 4 ) );
        return stream_type_wave_format_ex( stream, codec->codec_tag, sizeof(WAVEFORMATEX),
                                           media_type->u.audio, &media_type->format_size );
    }

    if (codec->codec_type == AVMEDIA_TYPE_VIDEO)
    {
        GUID subtype = MFVideoFormat_Base;

        media_type->major = MFMediaType_Video;

        TRACE( "codec type %#x, id %#x, tag %#x (%s)\n", codec->codec_type, codec->codec_id,
               codec->codec_tag, debugstr_an( (char *)&codec->codec_tag, 4 ) );

        if (codec->codec_id == AV_CODEC_ID_CINEPAK)
            return stream_type_video_format( stream, &MFVideoFormat_CVID, media_type->u.video,
                                             &media_type->format_size, 0 );
        if (codec->codec_id == AV_CODEC_ID_H264)
            return stream_type_video_format( stream, &MFVideoFormat_H264, media_type->u.video,
                                             &media_type->format_size, 0 );

        if (codec->codec_id == AV_CODEC_ID_WMV1)
            return stream_type_video_format( stream, &MFVideoFormat_WMV1, media_type->u.video,
                                             &media_type->format_size, 0 );
        if (codec->codec_id == AV_CODEC_ID_WMV2)
            return stream_type_video_format( stream, &MFVideoFormat_WMV2, media_type->u.video,
                                             &media_type->format_size, 0 );
        if (codec->codec_id == AV_CODEC_ID_WMV3)
            return stream_type_video_format( stream, &MFVideoFormat_WMV3, media_type->u.video,
                                             &media_type->format_size, 0 );
        if (codec->codec_id == AV_CODEC_ID_VC1)
            return stream_type_video_format( stream, &MFVideoFormat_WVC1, media_type->u.video,
                                             &media_type->format_size, 0 );

        if (codec->codec_id == AV_CODEC_ID_MPEG1VIDEO)
            return stream_type_mpeg_video_format( stream, media_type->u.format, &media_type->format_size, 0 );
        if (codec->codec_id == AV_CODEC_ID_MPEG2VIDEO)
            return stream_type_mpeg_video_format( stream, media_type->u.format, &media_type->format_size, 0 );
        if (codec->codec_id == AV_CODEC_ID_MPEG4)
            return stream_type_video_format( stream, &MFVideoFormat_MP4V, media_type->u.video,
                                             &media_type->format_size, 0 );

        if (codec->codec_id == AV_CODEC_ID_RAWVIDEO)
            return stream_type_video_format( stream, NULL, media_type->u.video, &media_type->format_size, 0 );

        FIXME( "Unknown video codec id %#x, tag %#x (%s)\n", codec->codec_id, codec->codec_tag,
               debugstr_an( (char *)&codec->codec_tag, 4 ) );

        subtype.Data1 = codec->codec_tag;
        return stream_type_video_format( stream, &subtype, media_type->u.video, &media_type->format_size, 0 );
    }

    FIXME( "Unknown codec type %u id %#x fourcc %s\n", codec->codec_type, codec->codec_id,
           debugstr_an( (char *)&codec->codec_tag, 4 ) );
    return STATUS_NOT_IMPLEMENTED;
}

#else

#define DEFINE_ENTRY( f )                                                                          \
    static NTSTATUS f( void *params )                                                              \
    {                                                                                              \
        ERR( "libavformat support not compiled in.\n" );                                           \
        return STATUS_UNSUCCESSFUL;                                                                \
    }

DEFINE_ENTRY( process_attach )
DEFINE_ENTRY( demuxer_create )
DEFINE_ENTRY( demuxer_destroy )
DEFINE_ENTRY( demuxer_read )
DEFINE_ENTRY( demuxer_seek )
DEFINE_ENTRY( demuxer_stream_lang )
DEFINE_ENTRY( demuxer_stream_name )
DEFINE_ENTRY( demuxer_stream_type )
#undef DEFINE_ENTRY

#endif

const unixlib_entry_t __wine_unix_call_funcs[] =
{
#define X( name ) [unix_##name] = name
    X(process_attach),
    X(demuxer_create),
    X(demuxer_destroy),
    X(demuxer_read),
    X(demuxer_seek),
    X(demuxer_stream_lang),
    X(demuxer_stream_name),
    X(demuxer_stream_type),
};

C_ASSERT(ARRAY_SIZE(__wine_unix_call_funcs) == unix_funcs_count);

#ifdef _WIN64

typedef ULONG PTR32;

struct media_type32
{
    GUID major;
    UINT32 format_size;
    PTR32 format;
};

static NTSTATUS wow64_demuxer_create( void *arg )
{
    struct
    {
        PTR32 io_ctx;
        PTR32 url;
        UINT64 demuxer;
        char mime_type[256];
        UINT32 stream_count;
        INT64 duration;
    } *params32 = arg;
    struct demuxer_create_params params;
    NTSTATUS status;

    params.io_ctx = UintToPtr( params32->io_ctx );
    params.url = UintToPtr( params32->url );
    status = demuxer_create( &params );
    params32->demuxer = params.demuxer;
    params32->duration = params.duration;
    memcpy( params32->mime_type, params.mime_type, 256 );
    params32->stream_count = params.stream_count;

    return status;
}

static NTSTATUS wow64_demuxer_destroy( void *arg )
{
    struct
    {
        UINT64 demuxer;
        PTR32 io_ctx;
    } *params32 = arg;
    struct demuxer_create_params params;
    NTSTATUS status;

    params.demuxer = params32->demuxer;
    status = demuxer_destroy( &params );
    params32->io_ctx = PtrToUint( params.io_ctx );

    return status;
}

static NTSTATUS wow64_demuxer_read( void *arg )
{
    struct
    {
        UINT64 demuxer;
        UINT64 packet;
        UINT32 stream;
        UINT32 size;
        INT64 dts;
        INT64 pts;
        INT64 duration;
        PTR32 data;
    } *params32 = arg;
    struct demuxer_read_params params;
    NTSTATUS status;

    params.demuxer = params32->demuxer;
    params.packet = params32->packet;
    params.size = params32->size;
    params.data = UintToPtr( params32->data );

    status = demuxer_read( &params );
    params32->packet = params.packet;
    params32->size = params.size;
    params32->stream = params.stream;
    params32->dts = params.dts;
    params32->pts = params.pts;
    params32->duration = params.duration;

    return status;
}

static NTSTATUS wow64_demuxer_stream_type( void *arg )
{
    struct
    {
        UINT64 demuxer;
        UINT32 stream;
        struct media_type32 media_type;
    } *params32 = arg;
    struct demuxer_stream_type_params params =
    {
        .demuxer = params32->demuxer,
        .media_type =
        {
            .major = params32->media_type.major,
            .format_size = params32->media_type.format_size,
            .u.format = UintToPtr( params32->media_type.format ),
        },
    };
    NTSTATUS status;

    status = demuxer_stream_type( &params );
    params32->media_type.major = params.media_type.major;
    params32->media_type.format_size = params.media_type.format_size;
    return status;
}

const unixlib_entry_t __wine_unix_call_wow64_funcs[] =
{
#define X64( name ) [unix_##name] = wow64_##name
    X(process_attach),
    X64(demuxer_create),
    X64(demuxer_destroy),
    X64(demuxer_read),
    X(demuxer_seek),
    X(demuxer_stream_lang),
    X(demuxer_stream_name),
    X64(demuxer_stream_type),
};

C_ASSERT(ARRAY_SIZE(__wine_unix_call_wow64_funcs) == unix_funcs_count);

#endif /* _WIN64 */
