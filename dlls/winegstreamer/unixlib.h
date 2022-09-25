/*
 * winegstreamer Unix library interface
 *
 * Copyright 2020-2021 Zebediah Figura for CodeWeavers
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

#ifndef __WINE_WINEGSTREAMER_UNIXLIB_H
#define __WINE_WINEGSTREAMER_UNIXLIB_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include "windef.h"
#include "winternl.h"
#include "wtypes.h"
#include "mmreg.h"

#include "wine/unixlib.h"

struct wg_format
{
    enum wg_major_type
    {
        WG_MAJOR_TYPE_UNKNOWN = 0,
        WG_MAJOR_TYPE_AUDIO,
        WG_MAJOR_TYPE_AUDIO_MPEG1,
        WG_MAJOR_TYPE_AUDIO_MPEG4,
        WG_MAJOR_TYPE_AUDIO_WMA,
        WG_MAJOR_TYPE_VIDEO,
        WG_MAJOR_TYPE_VIDEO_CINEPAK,
        WG_MAJOR_TYPE_VIDEO_H264,
        WG_MAJOR_TYPE_VIDEO_WMV,
        WG_MAJOR_TYPE_VIDEO_INDEO,
    } major_type;

    union
    {
        struct
        {
            enum wg_audio_format
            {
                WG_AUDIO_FORMAT_UNKNOWN,

                WG_AUDIO_FORMAT_U8,
                WG_AUDIO_FORMAT_S16LE,
                WG_AUDIO_FORMAT_S24LE,
                WG_AUDIO_FORMAT_S32LE,
                WG_AUDIO_FORMAT_F32LE,
                WG_AUDIO_FORMAT_F64LE,
            } format;

            uint32_t channels;
            uint32_t channel_mask; /* In WinMM format. */
            uint32_t rate;
        } audio;
        struct
        {
            uint32_t layer;
            uint32_t rate;
            uint32_t channels;
        } audio_mpeg1;
        struct
        {
            uint32_t payload_type;
            uint32_t codec_data_len;
            unsigned char codec_data[64];
        } audio_mpeg4;
        struct
        {
            uint32_t version;
            uint32_t bitrate;
            uint32_t rate;
            uint32_t depth;
            uint32_t channels;
            uint32_t block_align;
            uint32_t codec_data_len;
            unsigned char codec_data[64];
        } audio_wma;

        struct
        {
            enum wg_video_format
            {
                WG_VIDEO_FORMAT_UNKNOWN,

                WG_VIDEO_FORMAT_BGRA,
                WG_VIDEO_FORMAT_BGRx,
                WG_VIDEO_FORMAT_BGR,
                WG_VIDEO_FORMAT_RGB15,
                WG_VIDEO_FORMAT_RGB16,

                WG_VIDEO_FORMAT_AYUV,
                WG_VIDEO_FORMAT_I420,
                WG_VIDEO_FORMAT_NV12,
                WG_VIDEO_FORMAT_UYVY,
                WG_VIDEO_FORMAT_YUY2,
                WG_VIDEO_FORMAT_YV12,
                WG_VIDEO_FORMAT_YVYU,
            } format;
            /* Positive height indicates top-down video; negative height
             * indicates bottom-up video. */
            int32_t width, height;
            uint32_t fps_n, fps_d;
            RECT padding;
        } video;
        struct
        {
            uint32_t width;
            uint32_t height;
            uint32_t fps_n;
            uint32_t fps_d;
        } video_cinepak;
        struct
        {
            int32_t width, height;
            uint32_t fps_n, fps_d;
            uint32_t profile;
            uint32_t level;
        } video_h264;
        struct
        {
            enum wg_wmv_video_format
            {
                WG_WMV_VIDEO_FORMAT_UNKNOWN,
                WG_WMV_VIDEO_FORMAT_WMV1,
                WG_WMV_VIDEO_FORMAT_WMV2,
                WG_WMV_VIDEO_FORMAT_WMV3,
                WG_WMV_VIDEO_FORMAT_WMVA,
                WG_WMV_VIDEO_FORMAT_WVC1,
            } format;
            int32_t width, height;
            uint32_t fps_n, fps_d;
        } video_wmv;
        struct
        {
            int32_t width, height;
            uint32_t fps_n, fps_d;
            uint32_t version;
        } video_indeo;
    } u;
};

enum wg_sample_flag
{
    WG_SAMPLE_FLAG_INCOMPLETE = 1,
    WG_SAMPLE_FLAG_HAS_PTS = 2,
    WG_SAMPLE_FLAG_HAS_DURATION = 4,
    WG_SAMPLE_FLAG_SYNC_POINT = 8,
    WG_SAMPLE_FLAG_DISCONTINUITY = 0x10,
    WG_SAMPLE_FLAG_PREROLL = 0x20,
};

struct wg_sample
{
    /* pts and duration are in 100-nanosecond units. */
    UINT64 pts;
    UINT64 duration;
    LONG refcount; /* unix refcount */
    UINT32 flags;
    UINT32 max_size;
    UINT32 size;
    BYTE *data;
};

struct wg_request
{
    enum wg_request_type
    {
        WG_REQUEST_TYPE_ALLOC = 1,
        WG_REQUEST_TYPE_INPUT = 2,
        WG_REQUEST_TYPE_OUTPUT = 4,
    } type;
    UINT32 stream;
    UINT64 token;

    union
    {
        struct
        {
            UINT32 size;
        } alloc;
        struct
        {
            UINT64 offset;
            UINT32 size;
        } input;
        struct
        {
            UINT32 size;
            BYTE *data;
        } output;
    } u;
};

enum wg_parser_type
{
    WG_PARSER_DECODEBIN,
    WG_PARSER_AVIDEMUX,
    WG_PARSER_MPEGAUDIOPARSE,
    WG_PARSER_WAVPARSE,
};

struct wg_parser_create_params
{
    struct wg_parser *parser;
    enum wg_parser_type type;
    bool unlimited_buffering;
    bool err_on;
    bool warn_on;
};

struct wg_parser_connect_params
{
    struct wg_parser *parser;
    UINT64 file_size;
};

struct wg_parser_wait_request_params
{
    struct wg_parser *parser;
    UINT32 type_mask;
    struct wg_request *request;
};

struct wg_parser_push_data_params
{
    struct wg_parser *parser;
    struct wg_sample *sample;
    UINT64 token;
};

struct wg_parser_get_stream_count_params
{
    struct wg_parser *parser;
    UINT32 count;
};

struct wg_parser_get_stream_duration_params
{
    struct wg_parser *parser;
    UINT32 stream;
    UINT64 duration;
};

struct wg_parser_get_stream_params
{
    struct wg_parser *parser;
    UINT32 index;
    struct wg_parser_stream *stream;
};

struct wg_parser_stream_get_preferred_format_params
{
    struct wg_parser_stream *stream;
    struct wg_format *format;
};

struct wg_parser_stream_get_codec_format_params
{
    struct wg_parser_stream *stream;
    struct wg_format *format;
};

struct wg_parser_stream_enable_params
{
    struct wg_parser_stream *stream;
    const struct wg_format *format;
};

struct wg_parser_wait_stream_request_params
{
    struct wg_parser *parser;
    UINT32 type_mask;
    struct wg_parser_stream *stream;
    struct wg_request *request;
};

struct wg_parser_read_data_params
{
    struct wg_parser *parser;
    struct wg_sample *sample;
    UINT64 token;
};

struct wg_parser_done_alloc_params
{
    struct wg_parser *parser;
    struct wg_sample *sample;
    UINT64 token;
};

struct wg_parser_stream_notify_qos_params
{
    struct wg_parser_stream *stream;
    bool underflow;
    DOUBLE proportion;
    INT64 diff;
    UINT64 timestamp;
};

enum wg_parser_tag
{
    WG_PARSER_TAG_LANGUAGE,
    WG_PARSER_TAG_NAME,
    WG_PARSER_TAG_COUNT
};

struct wg_parser_stream_get_tag_params
{
    struct wg_parser_stream *stream;
    enum wg_parser_tag tag;
    char *buffer;
    UINT32 *size;
};

struct wg_parser_stream_seek_params
{
    struct wg_parser_stream *stream;
    DOUBLE rate;
    UINT64 start_pos, stop_pos;
    DWORD start_flags, stop_flags;
};

struct wg_transform_create_params
{
    struct wg_transform *transform;
    const struct wg_format *input_format;
    const struct wg_format *output_format;
};

struct wg_transform_push_data_params
{
    struct wg_transform *transform;
    struct wg_sample *sample;
    HRESULT result;
};

struct wg_transform_read_data_params
{
    struct wg_transform *transform;
    struct wg_sample *sample;
    struct wg_format *format;
    HRESULT result;
};

struct wg_transform_set_output_format_params
{
    struct wg_transform *transform;
    const struct wg_format *format;
};

struct wg_transform_get_status_params
{
    struct wg_transform *transform;
    UINT32 accepts_input;
};

enum unix_funcs
{
    unix_wg_init_gstreamer,

    unix_wg_parser_create,
    unix_wg_parser_destroy,

    unix_wg_parser_connect,
    unix_wg_parser_disconnect,

    unix_wg_parser_wait_request,
    unix_wg_parser_wait_stream_request,
    unix_wg_parser_push_data,
    unix_wg_parser_read_data,
    unix_wg_parser_done_alloc,

    unix_wg_parser_get_stream_count,
    unix_wg_parser_get_stream_duration,
    unix_wg_parser_get_stream,

    unix_wg_parser_stream_get_preferred_format,
    unix_wg_parser_stream_get_codec_format,
    unix_wg_parser_stream_enable,
    unix_wg_parser_stream_disable,

    unix_wg_parser_stream_notify_qos,

    unix_wg_parser_stream_get_tag,
    unix_wg_parser_stream_seek,

    unix_wg_transform_create,
    unix_wg_transform_destroy,
    unix_wg_transform_set_output_format,

    unix_wg_transform_push_data,
    unix_wg_transform_read_data,
    unix_wg_transform_get_status,
};

#endif /* __WINE_WINEGSTREAMER_UNIXLIB_H */
