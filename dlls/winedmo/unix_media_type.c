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

#include "initguid.h"
#include "unix_private.h"

#include "d3d9.h"
#include "mfapi.h"
#include "uuids.h"

#include "wine/debug.h"

#ifdef HAVE_FFMPEG

WINE_DEFAULT_DEBUG_CHANNEL(dmo);

DEFINE_MEDIATYPE_GUID( MFVideoFormat_ABGR32, D3DFMT_A8B8G8R8 );

static inline const char *debugstr_ratio( const MFRatio *ratio )
{
    return wine_dbg_sprintf( "%d:%d", (int)ratio->Numerator, (int)ratio->Denominator );
}

static inline const char *debugstr_area( const MFVideoArea *area )
{
    return wine_dbg_sprintf( "(%d,%d)-(%d,%d)", area->OffsetX.value, area->OffsetY.value,
                             (int)area->Area.cx, (int)area->Area.cy );
}

#define TRACE_HEXDUMP( data, size )                                                               \
    if (__WINE_IS_DEBUG_ON(_TRACE, __wine_dbch___default))                                        \
    do {                                                                                          \
        const unsigned char *__ptr, *__end, *__tmp;                                               \
        for (__ptr = (void *)(data), __end = __ptr + (size); __ptr < __end; __ptr += 16)          \
        {                                                                                         \
            char __buf[256], *__lim = __buf + sizeof(__buf), *__out = __buf;                      \
            __out += snprintf( __out, __lim - __out, "%08zx ", (char *)__ptr - (char *)data );    \
            for (__tmp = __ptr; __tmp < __end && __tmp < __ptr + 16; ++__tmp)                     \
                __out += snprintf( __out, __lim - __out, " %02x", *__tmp );                       \
            memset( __out, ' ', (__ptr + 16 - __tmp) * 3 + 2 );                                   \
            __out += (__ptr + 16 - __tmp) * 3 + 2;                                                \
            for (__tmp = __ptr; __tmp < __end && __tmp < __ptr + 16; ++__tmp)                     \
                *__out++ = *__tmp >= ' ' && *__tmp <= '~' ? *__tmp : '.';                         \
            *__out++ = 0;                                                                         \
            TRACE( "%s\n", __buf );                                                               \
        }                                                                                         \
    } while (0)

static NTSTATUS codec_params_add_extra_data( AVCodecParameters *params, const void *extra_data, UINT extra_data_size )
{
    if (!extra_data_size) return STATUS_SUCCESS;
    if (!(params->extradata = av_mallocz(extra_data_size + AV_INPUT_BUFFER_PADDING_SIZE))) return STATUS_NO_MEMORY;
    memcpy( params->extradata, extra_data, extra_data_size );
    params->extradata_size = extra_data_size;
    return STATUS_SUCCESS;
}

enum AVCodecID codec_id_from_audio_subtype( const GUID *subtype )
{
    const struct AVCodecTag *table[] = {avformat_get_riff_audio_tags(), avformat_get_mov_audio_tags(), 0};
    enum AVCodecID codec_id = av_codec_get_id( table, subtype->Data1 );
    if (codec_id != AV_CODEC_ID_NONE) return codec_id;
    if (IsEqualGUID( subtype, &MFAudioFormat_AAC )) return AV_CODEC_ID_AAC;
    FIXME( "Unsupported subtype %s (%s)\n", debugstr_guid( subtype ), debugstr_fourcc( subtype->Data1 ) );
    return codec_id;
}

static enum AVCodecID codec_id_from_wave_format_tag( UINT tag, UINT depth )
{
    switch (tag)
    {
    case WAVE_FORMAT_PCM:
        if (depth == 32) return AV_CODEC_ID_PCM_S32LE;
        if (depth == 24) return AV_CODEC_ID_PCM_S24LE;
        if (depth == 16) return AV_CODEC_ID_PCM_S16LE;
        if (depth == 8) return AV_CODEC_ID_PCM_U8;
        break;
    case WAVE_FORMAT_IEEE_FLOAT:
        if (depth == 64) return AV_CODEC_ID_PCM_F64LE;
        if (depth == 32) return AV_CODEC_ID_PCM_F32LE;
        break;
    }

    FIXME( "Not implemented\n" );
    return AV_CODEC_ID_NONE;
}

static enum AVSampleFormat sample_format_from_wave_format_tag( UINT tag, UINT depth )
{
    switch (tag)
    {
    case WAVE_FORMAT_PCM:
        if (depth == 32) return AV_SAMPLE_FMT_S32;
        if (depth == 16) return AV_SAMPLE_FMT_S16;
        if (depth == 8) return AV_SAMPLE_FMT_U8;
        break;
    case WAVE_FORMAT_IEEE_FLOAT:
        if (depth == 64) return AV_SAMPLE_FMT_DBL;
        if (depth == 32) return AV_SAMPLE_FMT_FLT;
        break;
    }

    FIXME( "Not implemented\n" );
    return AV_SAMPLE_FMT_NONE;
}

enum AVSampleFormat sample_format_from_subtype( const GUID *subtype, UINT depth )
{
    return sample_format_from_wave_format_tag( subtype->Data1, depth );
}

enum AVSampleFormat sample_format_from_wave_format( const WAVEFORMATEX *format )
{
    switch (format->wFormatTag)
    {
    case WAVE_FORMAT_PCM:
    case WAVE_FORMAT_IEEE_FLOAT:
        return sample_format_from_wave_format_tag( format->wFormatTag, format->wBitsPerSample );
    case WAVE_FORMAT_EXTENSIBLE:
    {
        WAVEFORMATEXTENSIBLE *extensible = CONTAINING_RECORD( format, WAVEFORMATEXTENSIBLE, Format );
        return sample_format_from_wave_format_tag( extensible->SubFormat.Data1, format->wBitsPerSample );
    }
    }

    FIXME( "Not implemented\n" );
    return AV_SAMPLE_FMT_NONE;
}

static NTSTATUS codec_params_from_wave_format_ex( AVCodecParameters *params, const WAVEFORMATEX *format,
                                                  UINT32 format_size, const GUID *subtype, UINT64 channel_mask )
{
    if (format->wFormatTag == WAVE_FORMAT_PCM || format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
    {
        params->codec_id = codec_id_from_wave_format_tag( subtype->Data1, format->wBitsPerSample );
        params->format = sample_format_from_wave_format_tag( subtype->Data1, format->wBitsPerSample );
    }
    else
    {
        params->codec_id = codec_id_from_audio_subtype( subtype );
        params->format = AV_SAMPLE_FMT_NONE;
    }

    params->codec_type = AVMEDIA_TYPE_AUDIO;
    params->codec_tag = format->wFormatTag;
#if LIBAVUTIL_VERSION_MAJOR >= 58
    if (channel_mask) av_channel_layout_from_mask( &params->ch_layout, channel_mask );
    else av_channel_layout_default( &params->ch_layout, format->nChannels );
#else
    params->channels = format->nChannels;
    params->channel_layout = channel_mask;
#endif
    params->sample_rate = format->nSamplesPerSec;

    params->bits_per_raw_sample = format->wBitsPerSample;
    params->bits_per_coded_sample = format->wBitsPerSample;
    params->block_align = format->nBlockAlign;
    params->bit_rate = format->nAvgBytesPerSec * 8;

    return codec_params_add_extra_data( params, format + 1, format->cbSize );
}

static WAVEFORMATEX *strip_heaac_wave_format( const HEAACWAVEINFO *format, UINT *stripped_size )
{
    WAVEFORMATEX *stripped;
    UINT32 size;

    size = format->wfx.cbSize + sizeof(*stripped) - sizeof(*format);
    if (!(stripped = malloc( sizeof(*stripped) + size ))) return NULL;
    *stripped = format->wfx;
    stripped->cbSize = size;
    memcpy( stripped + 1, format + 1, size );
    *stripped_size = sizeof(*stripped) + size;

    return stripped;
}

static NTSTATUS codec_params_from_heaac_wave_format( AVCodecParameters *params, const HEAACWAVEINFO *format, UINT32 format_size,
                                                     const GUID *subtype, UINT channel_mask )
{
    WAVEFORMATEX *stripped;
    UINT stripped_size;
    NTSTATUS status;

    TRACE( "tag %#x, %u channels, sample rate %u, %u bytes/sec, alignment %u, %u bits/sample, payload %#x, level %#x, struct %#x.\n",
           format->wfx.wFormatTag, format->wfx.nChannels, (int)format->wfx.nSamplesPerSec,
           (int)format->wfx.nAvgBytesPerSec, format->wfx.nBlockAlign, format->wfx.wBitsPerSample,
           format->wPayloadType, format->wAudioProfileLevelIndication, format->wStructType );
    if (format->wfx.cbSize)
    {
        UINT extra_size = sizeof(WAVEFORMATEX) + format->wfx.cbSize - sizeof(*format);
        TRACE_HEXDUMP( format + 1, extra_size );
    }

    if (!(stripped = strip_heaac_wave_format( format, &stripped_size ))) return STATUS_NO_MEMORY;
    status = codec_params_from_wave_format_ex( params, stripped, stripped_size, subtype, channel_mask );
    free( stripped );

    return status;
}

static WAVEFORMATEX *strip_wave_format_extensible( const WAVEFORMATEXTENSIBLE *format, UINT *stripped_size )
{
    WAVEFORMATEX *stripped;
    UINT32 size;

    size = format->Format.cbSize + sizeof(*stripped) - sizeof(*format);
    if (!(stripped = malloc( sizeof(*stripped) + size ))) return NULL;
    *stripped = format->Format;
    stripped->cbSize = size;
    memcpy( stripped + 1, format + 1, size );
    *stripped_size = sizeof(*stripped) + size;

    return stripped;
}

static NTSTATUS codec_params_from_wave_format_extensible( AVCodecParameters *params, const WAVEFORMATEXTENSIBLE *format, UINT32 format_size )
{
    UINT stripped_size;
    NTSTATUS status;
    void *stripped;

    TRACE("tag %#x, %u channels, sample rate %u, %u bytes/sec, alignment %u, %u bits/sample, "
            "%u valid bps, channel mask %#x, subtype %s (%s).\n",
            format->Format.wFormatTag, format->Format.nChannels, (int)format->Format.nSamplesPerSec,
            (int)format->Format.nAvgBytesPerSec, format->Format.nBlockAlign, format->Format.wBitsPerSample,
            format->Samples.wValidBitsPerSample, (int)format->dwChannelMask, debugstr_guid(&format->SubFormat), debugstr_fourcc(format->SubFormat.Data1));
    if (format->Format.cbSize)
    {
        UINT extra_size = sizeof(WAVEFORMATEX) + format->Format.cbSize - sizeof(*format);
        TRACE_HEXDUMP( format + 1, extra_size );
    }

    if (!(stripped = strip_wave_format_extensible( format, &stripped_size ))) return STATUS_NO_MEMORY;
    if (IsEqualGUID( &format->SubFormat, &MFAudioFormat_AAC ))
        status = codec_params_from_heaac_wave_format( params, stripped, stripped_size, &format->SubFormat, format->dwChannelMask );
    else
        status = codec_params_from_wave_format_ex( params, stripped, stripped_size, &format->SubFormat, format->dwChannelMask );
    free( stripped );

    return status;
}

static NTSTATUS codec_params_from_audio_format( AVCodecParameters *params, const void *format, UINT32 format_size )
{
    const WAVEFORMATEX *wfx = format;
    GUID subtype = MFAudioFormat_Base;

    if (format_size < sizeof(*wfx) || sizeof(*wfx) + wfx->cbSize != format_size)
    {
        FIXME( "Unexpected audio format\n" );
        return STATUS_INVALID_PARAMETER;
    }
    subtype.Data1 = wfx->wFormatTag;

    if (wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
        return codec_params_from_wave_format_extensible( params, format, format_size );
    if (wfx->wFormatTag == WAVE_FORMAT_MPEG_HEAAC)
        return codec_params_from_heaac_wave_format( params, format, format_size, &subtype, 0 );

    TRACE( "tag %#x, %u channels, sample rate %u, %u bytes/sec, alignment %u, %u bits/sample.\n",
           wfx->wFormatTag, wfx->nChannels, (int)wfx->nSamplesPerSec, (int)wfx->nAvgBytesPerSec,
           wfx->nBlockAlign, wfx->wBitsPerSample );
    if (wfx->cbSize) TRACE_HEXDUMP( wfx + 1, wfx->cbSize );

    return codec_params_from_wave_format_ex( params, format, format_size, &subtype, 0 );
}

enum AVPixelFormat pixel_format_from_subtype( const GUID *subtype )
{
    if (IsEqualGUID( subtype, &MFVideoFormat_I420 )) return AV_PIX_FMT_YUV420P;
    if (IsEqualGUID( subtype, &MFVideoFormat_YV12 )) return AV_PIX_FMT_YUV420P;
    if (IsEqualGUID( subtype, &MFVideoFormat_IYUV )) return AV_PIX_FMT_YUV420P;
    if (IsEqualGUID( subtype, &MFVideoFormat_YUY2 )) return AV_PIX_FMT_YUYV422;
    if (IsEqualGUID( subtype, &MFVideoFormat_UYVY )) return AV_PIX_FMT_UYVY422;
    if (IsEqualGUID( subtype, &MFVideoFormat_YVYU )) return AV_PIX_FMT_YVYU422;
    if (IsEqualGUID( subtype, &MFVideoFormat_RGB32 )) return AV_PIX_FMT_BGR0;
    if (IsEqualGUID( subtype, &MFVideoFormat_ABGR32 )) return AV_PIX_FMT_RGBA;
    if (IsEqualGUID( subtype, &MFVideoFormat_ARGB32 )) return AV_PIX_FMT_BGRA;
    if (IsEqualGUID( subtype, &MFVideoFormat_RGB24 )) return AV_PIX_FMT_BGR24;
    if (IsEqualGUID( subtype, &MFVideoFormat_NV12 )) return AV_PIX_FMT_NV12;
    if (IsEqualGUID( subtype, &MFVideoFormat_NV21 )) return AV_PIX_FMT_NV21;
    if (IsEqualGUID( subtype, &MFVideoFormat_RGB565 )) return AV_PIX_FMT_RGB565;
    if (IsEqualGUID( subtype, &MFVideoFormat_RGB555 )) return AV_PIX_FMT_RGB555;
    if (IsEqualGUID( subtype, &MFVideoFormat_RGB8 )) return AV_PIX_FMT_RGB8;
    if (IsEqualGUID( subtype, &MFVideoFormat_NV11 ))
    {
        FIXME( "Unsupported NV11 format\n" );
        return AV_PIX_FMT_YUV411P;
    }
    return AV_PIX_FMT_NONE;
}

static inline BOOL is_mf_video_area_empty( const MFVideoArea *area )
{
    return !area->OffsetX.value && !area->OffsetY.value && !area->Area.cx && !area->Area.cy;
}

enum AVCodecID codec_id_from_video_subtype( const GUID *subtype )
{
    const struct AVCodecTag *table[] = {avformat_get_riff_video_tags(), avformat_get_mov_video_tags(), 0};
    enum AVCodecID codec_id = av_codec_get_id( table, subtype->Data1 );
    if (codec_id != AV_CODEC_ID_NONE) return codec_id;
    FIXME( "Unsupported subtype %s (%s)\n", debugstr_guid( subtype ), debugstr_fourcc( subtype->Data1 ) );
    return codec_id;
}

static NTSTATUS codec_params_from_video_format( AVCodecParameters *params, AVRational *sar, AVRational *fps,
                                                const MFVIDEOFORMAT *format, UINT32 format_size )
{
    TRACE( "subtype %s (%s) %ux%u, FPS %s, aperture %s, PAR %s, videoFlags %#x.\n",
            debugstr_guid(&format->guidFormat), debugstr_fourcc(format->guidFormat.Data1), (int)format->videoInfo.dwWidth, (int)format->videoInfo.dwHeight,
            debugstr_ratio(&format->videoInfo.FramesPerSecond), debugstr_area(&format->videoInfo.MinimumDisplayAperture),
            debugstr_ratio(&format->videoInfo.PixelAspectRatio), (int)format->videoInfo.VideoFlags );
    if (format->dwSize > sizeof(*format)) TRACE_HEXDUMP( format + 1, format->dwSize - sizeof(*format) );

    params->codec_type = AVMEDIA_TYPE_VIDEO;
    params->codec_tag = format->guidFormat.Data1;
    params->format = pixel_format_from_subtype( &format->guidFormat );
    if (params->format != AV_PIX_FMT_NONE) params->codec_id = AV_CODEC_ID_RAWVIDEO;
    else params->codec_id = codec_id_from_video_subtype( &format->guidFormat );

    params->width = format->videoInfo.dwWidth;
    params->height = format->videoInfo.dwHeight;

    if (format->videoInfo.PixelAspectRatio.Denominator)
    {
        sar->num = format->videoInfo.PixelAspectRatio.Numerator;
        sar->den = format->videoInfo.PixelAspectRatio.Denominator;
    }
    else
    {
        sar->num = 1;
        sar->den = 1;
    }
    if (format->videoInfo.FramesPerSecond.Denominator)
    {
        fps->num = format->videoInfo.FramesPerSecond.Numerator;
        fps->den = format->videoInfo.FramesPerSecond.Denominator;
    }

    if (!is_mf_video_area_empty( &format->videoInfo.MinimumDisplayAperture ))
    {
        params->width = format->videoInfo.MinimumDisplayAperture.Area.cx;
        params->height = format->videoInfo.MinimumDisplayAperture.Area.cy;
    }

    return codec_params_add_extra_data( params, format + 1, format_size - sizeof(*format) );
}

NTSTATUS codec_params_from_media_type( AVCodecParameters *params, AVRational *sar, AVRational *fps,
                                       const struct media_type *media_type )
{
    NTSTATUS status;

    if (IsEqualGUID( &media_type->major, &MFMediaType_Video ))
        status = codec_params_from_video_format( params, sar, fps, media_type->video, media_type->format_size );
    else if (IsEqualGUID( &media_type->major, &MFMediaType_Audio ))
        status = codec_params_from_audio_format( params, media_type->audio, media_type->format_size );
    else
    {
        FIXME( "not implemented\n" );
        status = STATUS_NOT_IMPLEMENTED;
    }

    TRACE( "codec type %#x, id %#x (%s), tag %#x (%s)\n", params->codec_type, params->codec_id,
           avcodec_get_name( params->codec_id ), params->codec_tag, debugstr_fourcc( params->codec_tag ) );
    if (params->extradata_size) TRACE_HEXDUMP( params->extradata, params->extradata_size );

    return status;
}

static UINT wave_format_tag_from_codec_id( enum AVCodecID id )
{
    const struct AVCodecTag *table[] = {avformat_get_riff_audio_tags(), avformat_get_mov_audio_tags(), 0};
    return av_codec_get_tag( table, id );
}

static void wave_format_ex_init( const AVCodecParameters *params, WAVEFORMATEX *format, UINT32 format_size, WORD format_tag )
{
    memset( format, 0, format_size );
    format->cbSize = format_size - sizeof(*format);
    format->wFormatTag = format_tag;
#if LIBAVUTIL_VERSION_MAJOR >= 58
    format->nChannels = params->ch_layout.nb_channels;
#else
    format->nChannels = params->channels;
#endif
    format->nSamplesPerSec = params->sample_rate;
    format->wBitsPerSample = av_get_bits_per_sample( params->codec_id );
    if (!format->wBitsPerSample) format->wBitsPerSample = params->bits_per_coded_sample;
    if (!(format->nBlockAlign = params->block_align)) format->nBlockAlign = format->wBitsPerSample * format->nChannels / 8;
    if (!(format->nAvgBytesPerSec = params->bit_rate / 8)) format->nAvgBytesPerSec = format->nSamplesPerSec * format->nBlockAlign;
}

static NTSTATUS wave_format_extensible_from_codec_params( const AVCodecParameters *params, WAVEFORMATEXTENSIBLE *format, UINT32 *format_size,
                                                          UINT wave_format_size, const GUID *subtype, UINT64 channel_mask )
{
    UINT32 capacity = *format_size;

    *format_size = max( wave_format_size, sizeof(*format) + params->extradata_size );
    if (*format_size > capacity) return STATUS_BUFFER_TOO_SMALL;

    wave_format_ex_init( params, &format->Format, *format_size, WAVE_FORMAT_EXTENSIBLE );
    if (params->extradata_size && params->extradata) memcpy( format + 1, params->extradata, params->extradata_size );
    format->Samples.wValidBitsPerSample = 0;
    format->dwChannelMask = channel_mask;
    format->SubFormat = *subtype;

    TRACE( "tag %#x, %u channels, sample rate %u, %u bytes/sec, alignment %u, %u bits/sample, %u valid bps,"
           " channel mask %#x, subtype %s (%s).\n", format->Format.wFormatTag, format->Format.nChannels,
           (int)format->Format.nSamplesPerSec, (int)format->Format.nAvgBytesPerSec, format->Format.nBlockAlign,
           format->Format.wBitsPerSample, format->Samples.wValidBitsPerSample, (int)format->dwChannelMask,
           debugstr_guid(&format->SubFormat), debugstr_fourcc(format->SubFormat.Data1) );
    if (format->Format.cbSize)
    {
        UINT extra_size = sizeof(WAVEFORMATEX) + format->Format.cbSize - sizeof(WAVEFORMATEXTENSIBLE);
        TRACE_HEXDUMP( format + 1, extra_size );
    }

    return STATUS_SUCCESS;
}

static NTSTATUS wave_format_ex_from_codec_params( const AVCodecParameters *params, WAVEFORMATEX *format, UINT32 *format_size,
                                                  UINT32 wave_format_size, WORD format_tag )
{
    UINT32 capacity = *format_size;

    *format_size = max( wave_format_size, sizeof(*format) + params->extradata_size );
    if (*format_size > capacity) return STATUS_BUFFER_TOO_SMALL;

    wave_format_ex_init( params, format, *format_size, format_tag );
    if (params->extradata_size && params->extradata) memcpy( format + 1, params->extradata, params->extradata_size );

    TRACE( "tag %#x, %u channels, sample rate %u, %u bytes/sec, alignment %u, %u bits/sample.\n",
           format->wFormatTag, format->nChannels, (int)format->nSamplesPerSec, (int)format->nAvgBytesPerSec,
           format->nBlockAlign, format->wBitsPerSample );
    if (format->cbSize) TRACE_HEXDUMP( format + 1, format->cbSize );

    return STATUS_SUCCESS;
}

static NTSTATUS heaac_wave_format_from_codec_params( const AVCodecParameters *params, HEAACWAVEINFO *format, UINT32 *format_size )
{
    UINT32 capacity = *format_size;

    *format_size = sizeof(*format) + params->extradata_size;
    if (*format_size > capacity) return STATUS_BUFFER_TOO_SMALL;

    wave_format_ex_init( params, &format->wfx, *format_size, WAVE_FORMAT_MPEG_HEAAC );
    if (params->extradata_size && params->extradata) memcpy( format + 1, params->extradata, params->extradata_size );
    format->wPayloadType = 0;
    format->wAudioProfileLevelIndication = 0;
    format->wStructType = 0;

    TRACE( "tag %#x, %u channels, sample rate %u, %u bytes/sec, alignment %u, %u bits/sample, payload %#x, "
           "level %#x, struct %#x.\n", format->wfx.wFormatTag, format->wfx.nChannels, (int)format->wfx.nSamplesPerSec,
           (int)format->wfx.nAvgBytesPerSec, format->wfx.nBlockAlign, format->wfx.wBitsPerSample, format->wPayloadType,
           format->wAudioProfileLevelIndication, format->wStructType );
    if (format->wfx.cbSize) TRACE_HEXDUMP( format + 1, sizeof(WAVEFORMATEX) + format->wfx.cbSize - sizeof(*format) );

    return STATUS_SUCCESS;
}

static NTSTATUS audio_format_from_codec_params( const AVCodecParameters *params, void *format, UINT32 *format_size )
{
    UINT format_tag, wave_format_size = sizeof(WAVEFORMATEX);
    UINT64 channel_mask;
    int channels;

    if (params->codec_id == AV_CODEC_ID_AAC) return heaac_wave_format_from_codec_params( params, format, format_size );
    if (params->codec_id == AV_CODEC_ID_MP1) wave_format_size = sizeof(MPEG1WAVEFORMAT);
    if (params->codec_id == AV_CODEC_ID_MP3) wave_format_size = sizeof(MPEGLAYER3WAVEFORMAT);
    if (params->codec_id == AV_CODEC_ID_WMAV1) wave_format_size = sizeof(MSAUDIO1WAVEFORMAT);
    if (params->codec_id == AV_CODEC_ID_WMAV2) wave_format_size = sizeof(WMAUDIO2WAVEFORMAT);
    if (params->codec_id == AV_CODEC_ID_WMAPRO) wave_format_size = sizeof(WMAUDIO3WAVEFORMAT);
    if (params->codec_id == AV_CODEC_ID_WMAVOICE) wave_format_size = sizeof(WMAUDIO3WAVEFORMAT);
    if (params->codec_id == AV_CODEC_ID_WMALOSSLESS) wave_format_size = sizeof(WMAUDIO3WAVEFORMAT);

#if LIBAVUTIL_VERSION_MAJOR >= 58
    if (!(channels = params->ch_layout.nb_channels)) channels = 1;
    if (params->ch_layout.order != AV_CHANNEL_ORDER_NATIVE) channel_mask = 0;
    else channel_mask = params->ch_layout.u.mask;
#else
    if (!(channels = params->channels)) channels = 1;
    channel_mask = params->channel_layout;
#endif

    format_tag = wave_format_tag_from_codec_id( params->codec_id );
    if (format_tag == WAVE_FORMAT_EXTENSIBLE || format_tag >> 16 || (channels > 2 && channel_mask != 0) ||
        params->codec_id == AV_CODEC_ID_VORBIS)
    {
        GUID subtype = MFAudioFormat_Base;

        if (params->codec_id == AV_CODEC_ID_VORBIS) subtype = MFAudioFormat_Vorbis;
        else subtype.Data1 = format_tag;

        wave_format_size += sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
        return wave_format_extensible_from_codec_params( params, format, format_size, wave_format_size,
                                                         &subtype, channel_mask );
    }

    return wave_format_ex_from_codec_params( params, format, format_size, wave_format_size, format_tag );
}

static GUID subtype_from_pixel_format( enum AVPixelFormat fmt )
{
    switch (fmt)
    {
    case AV_PIX_FMT_NONE: return MFVideoFormat_Base;
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
    case AV_PIX_FMT_RGB8: return MFVideoFormat_RGB8;
    default:
        FIXME( "Unsupported format %#x (%s)\n", fmt, av_get_pix_fmt_name( fmt ) );
        return MFVideoFormat_Base;
    }
}

enum AVPixelFormat pixel_format_from_video_format( const MFVIDEOFORMAT *format )
{
    enum AVPixelFormat pix_fmt = pixel_format_from_subtype( &format->guidFormat );
    if (pix_fmt != AV_PIX_FMT_NONE) return pix_fmt;
    FIXME( "Unsupported subtype %s (%s)\n", debugstr_guid( &format->guidFormat ),
           debugstr_fourcc( format->guidFormat.Data1 ) );
    return AV_PIX_FMT_NONE;
}

static UINT video_format_tag_from_codec_id( enum AVCodecID id )
{
    const struct AVCodecTag *table[] = {avformat_get_riff_video_tags(), avformat_get_mov_video_tags(), 0};
    return av_codec_get_tag( table, id );
}

static void mf_video_format_init( const AVCodecParameters *params, MFVIDEOFORMAT *format, UINT32 format_size,
                                  const AVRational *sar, const AVRational *fps, UINT32 align )
{
    memset( format, 0, format_size );
    format->dwSize = format_size;

    if (params->codec_id == AV_CODEC_ID_RAWVIDEO && params->format != AV_PIX_FMT_NONE)
        format->guidFormat = subtype_from_pixel_format( params->format );
    else
    {
        format->guidFormat = MFVideoFormat_Base;
        if (params->codec_id == AV_CODEC_ID_MPEG1VIDEO) format->guidFormat = MEDIASUBTYPE_MPEG1Payload;
        else if (params->codec_id == AV_CODEC_ID_H264) format->guidFormat.Data1 = MFVideoFormat_H264.Data1;
        else if (params->codec_id == AV_CODEC_ID_VP9) format->guidFormat.Data1 = MFVideoFormat_VP90.Data1;
        else if (params->codec_tag) format->guidFormat.Data1 = params->codec_tag;
        else format->guidFormat.Data1 = video_format_tag_from_codec_id( params->codec_id );
    }

    format->videoInfo.dwWidth = (params->width + align) & ~align;
    format->videoInfo.dwHeight = (params->height + align) & ~align;
    if (format->videoInfo.dwWidth != params->width || format->videoInfo.dwHeight != params->height)
    {
        format->videoInfo.MinimumDisplayAperture.Area.cx = params->width;
        format->videoInfo.MinimumDisplayAperture.Area.cy = params->height;
    }
    format->videoInfo.GeometricAperture = format->videoInfo.MinimumDisplayAperture;
    format->videoInfo.PanScanAperture = format->videoInfo.MinimumDisplayAperture;

    if (sar->num && sar->den)
    {
        format->videoInfo.PixelAspectRatio.Numerator = sar->num;
        format->videoInfo.PixelAspectRatio.Denominator = sar->den;
    }
    else
    {
        format->videoInfo.PixelAspectRatio.Numerator = 1;
        format->videoInfo.PixelAspectRatio.Denominator = 1;
    }

    if (fps->num && fps->den)
    {
        format->videoInfo.FramesPerSecond.Numerator = fps->num;
        format->videoInfo.FramesPerSecond.Denominator = fps->den;
    }
}

static NTSTATUS video_format_from_codec_params( const AVCodecParameters *params, MFVIDEOFORMAT *format, UINT32 *format_size,
                                                const AVRational *sar, const AVRational *fps, UINT32 align )
{
    UINT32 capacity = *format_size;

    *format_size = sizeof(*format) + params->extradata_size;
    if (*format_size > capacity) return STATUS_BUFFER_TOO_SMALL;

    mf_video_format_init( params, format, *format_size, sar, fps, align );
    if (params->extradata_size && params->extradata) memcpy( format + 1, params->extradata, params->extradata_size );

    TRACE( "subtype %s (%s) %ux%u, FPS %s, aperture %s, PAR %s, videoFlags %#x.\n", debugstr_guid(&format->guidFormat),
           debugstr_fourcc(format->guidFormat.Data1), (int)format->videoInfo.dwWidth, (int)format->videoInfo.dwHeight,
           debugstr_ratio(&format->videoInfo.FramesPerSecond), debugstr_area(&format->videoInfo.MinimumDisplayAperture),
           debugstr_ratio(&format->videoInfo.PixelAspectRatio), (int)format->videoInfo.VideoFlags );
    if (format->dwSize > sizeof(*format)) TRACE_HEXDUMP( format + 1, format->dwSize - sizeof(*format) );

    return STATUS_SUCCESS;
}

NTSTATUS media_type_from_codec_params( const AVCodecParameters *params, const AVRational *sar, const AVRational *fps,
                                       UINT32 align, struct media_type *media_type )
{
    TRACE( "codec type %#x, id %#x (%s), tag %#x (%s)\n", params->codec_type, params->codec_id, avcodec_get_name(params->codec_id),
           params->codec_tag, debugstr_fourcc(params->codec_tag) );
    if (params->extradata_size) TRACE_HEXDUMP( params->extradata, params->extradata_size );

    if (params->codec_type == AVMEDIA_TYPE_AUDIO)
    {
        media_type->major = MFMediaType_Audio;
        return audio_format_from_codec_params( params, media_type->audio, &media_type->format_size );
    }

    if (params->codec_type == AVMEDIA_TYPE_VIDEO)
    {
        media_type->major = MFMediaType_Video;
        return video_format_from_codec_params( params, media_type->video, &media_type->format_size, sar, fps, align );
    }

    FIXME( "Unknown type %#x\n", params->codec_type );
    return STATUS_NOT_IMPLEMENTED;
}

#endif /* HAVE_FFMPEG */
