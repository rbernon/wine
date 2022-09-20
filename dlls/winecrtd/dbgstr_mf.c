/*
 * Copyright 2022 Rémi Bernon for CodeWeavers
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

#include <stdarg.h>
#include <stddef.h>

#define WINBASEAPI
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "winuser.h"
#include "wingdi.h"

#define COM_NO_WINDOWS_H
#define RPC_NO_WINDOWS_H
#include "objbase.h"

#undef DEFINE_GUID
#undef EXTERN_GUID

#define EXTERN_GUID DEFINE_GUID
#define GUID_DEFINED

#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    static const GUID name = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }

#include "mfapi.h"
#include "mfd3d12.h"
#include "mfidl.h"
#include "mfmediaengine.h"
#include "mfobjects.h"
#include "mfplay.h"
#include "mfreadwrite.h"
#include "mftransform.h"
#include "uuids.h"

#include "wine/debug.h"

struct guid_def
{
    const GUID *guid;
    const char *name;
};

static int guid_def_cmp(const void *a, const void *b)
{
    const struct guid_def *a_def = a, *b_def = b;
    return memcmp(a_def->guid, b_def->guid, sizeof(GUID));
}

struct sort_guid_defs_params
{
    struct guid_def *array;
    UINT count;
};

static BOOL WINAPI sort_guid_defs(INIT_ONCE *once, void *param, void **context)
{
    struct sort_guid_defs_params *params = param;
    qsort(params->array, params->count, sizeof(struct guid_def), guid_def_cmp);
    return TRUE;
}

const char *debugstr_mf_guid(const GUID *guid)
{
    static struct guid_def guid_defs[] =
    {
#define X(g) { &(g), #g }
        X(MFAudioFormat_ADTS),
        X(MFAudioFormat_PCM),
        X(MFAudioFormat_PCM_HDCP),
        X(MFAudioFormat_Float),
        X(MFAudioFormat_DTS),
        X(MFAudioFormat_DRM),
        X(MFAudioFormat_MSP1),
        X(MFAudioFormat_Vorbis),
        X(MFAudioFormat_AAC),
        X(MFVideoFormat_RGB24),
        X(MFVideoFormat_ARGB32),
        X(MFVideoFormat_RGB32),
        X(MFVideoFormat_RGB565),
        X(MFVideoFormat_RGB555),
        X(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID),
        X(MFT_CATEGORY_MULTIPLEXER),
        X(MFVideoFormat_A2R10G10B10),
        X(MFT_CATEGORY_VIDEO_EFFECT),
        X(MFMediaType_Script),
        X(MFMediaType_Image),
        X(MFMediaType_HTML),
        X(MFMediaType_Binary),
        X(MFVideoFormat_MPEG2),
        X(MFMediaType_FileTransfer),
        X(MFVideoFormat_RGB8),
        X(MFAudioFormat_Dolby_AC3),
        X(MFVideoFormat_L8),
        X(MFAudioFormat_LPCM),
        X(MFVideoFormat_420O),
        X(MFVideoFormat_AI44),
        X(MFVideoFormat_AV1),
        X(MFVideoFormat_AYUV),
        X(MFVideoFormat_H263),
        X(MFVideoFormat_H264),
        X(MFVideoFormat_H265),
        X(MFVideoFormat_HEVC),
        X(MFVideoFormat_HEVC_ES),
        X(MFT_CATEGORY_AUDIO_EFFECT),
        X(MFVideoFormat_I420),
        X(MFVideoFormat_IYUV),
        X(MFT_CATEGORY_VIDEO_DECODER),
        X(MFVideoFormat_M4S2),
        X(MFVideoFormat_MJPG),
        X(MFVideoFormat_MP43),
        X(MFVideoFormat_MP4S),
        X(MFVideoFormat_MP4V),
        X(MFVideoFormat_MPG1),
        X(MFVideoFormat_MSS1),
        X(MFVideoFormat_MSS2),
        X(MFVideoFormat_NV11),
        X(MFVideoFormat_NV12),
        X(MFVideoFormat_ORAW),
        X(MFAudioFormat_Opus),
        X(MFAudioFormat_MPEG),
        X(MFVideoFormat_D16),
        X(MFVideoFormat_P010),
        X(MFVideoFormat_P016),
        X(MFVideoFormat_P210),
        X(MFVideoFormat_P216),
        X(MFVideoFormat_L16),
        X(MFAudioFormat_MP3),
        X(MFVideoFormat_UYVY),
        X(MFVideoFormat_VP10),
        X(MFVideoFormat_VP80),
        X(MFVideoFormat_VP90),
        X(MFVideoFormat_WMV1),
        X(MFVideoFormat_WMV2),
        X(MFVideoFormat_WMV3),
        X(MFVideoFormat_WVC1),
        X(MFT_CATEGORY_OTHER),
        X(MFVideoFormat_Y210),
        X(MFVideoFormat_Y216),
        X(MFVideoFormat_Y410),
        X(MFVideoFormat_Y416),
        X(MFVideoFormat_Y41P),
        X(MFVideoFormat_Y41T),
        X(MFVideoFormat_Y42T),
        X(MFVideoFormat_YUY2),
        X(MFVideoFormat_YV12),
        X(MFVideoFormat_YVU9),
        X(MFVideoFormat_YVYU),
        X(MFAudioFormat_WMAudioV8),
        X(MFAudioFormat_ALAC),
        X(MFAudioFormat_AMR_NB),
        X(MFMediaType_Audio),
        X(MFAudioFormat_WMAudioV9),
        X(MFAudioFormat_AMR_WB),
        X(MFAudioFormat_WMAudio_Lossless),
        X(MFAudioFormat_AMR_WP),
        X(MFAudioFormat_WMASPDIF),
        X(MFVideoFormat_DV25),
        X(MFVideoFormat_DV50),
        X(MFVideoFormat_DVC),
        X(MFVideoFormat_DVH1),
        X(MFVideoFormat_DVHD),
        X(MFVideoFormat_DVSD),
        X(MFVideoFormat_DVSL),
        X(MFVideoFormat_A16B16G16R16F),
        X(MFVideoFormat_v210),
        X(MFVideoFormat_v216),
        X(MFVideoFormat_v410),
        X(MFMediaType_Video),
        X(MFAudioFormat_AAC_HDCP),
        X(MFT_CATEGORY_DEMULTIPLEXER),
        X(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID),
        X(MFT_CATEGORY_VIDEO_ENCODER),
        X(MFAudioFormat_Dolby_AC3_HDCP),
        X(MFMediaType_Subtitle),
        X(MFMediaType_Stream),
        X(MFAudioFormat_Dolby_AC3_SPDIF),
        X(MFAudioFormat_Float_SpatialObjects),
        X(MFMediaType_SAMI),
        X(MFAudioFormat_ADTS_HDCP),
        X(MFAudioFormat_FLAC),
        X(MFAudioFormat_Dolby_DDPlus),
        X(MFMediaType_MultiplexedFrames),
        X(MFT_CATEGORY_AUDIO_DECODER),
        X(MFAudioFormat_Base_HDCP),
        X(MFT_CATEGORY_AUDIO_ENCODER),
        X(MFVideoFormat_Base_HDCP),
        X(MFVideoFormat_H264_HDCP),
        X(MFVideoFormat_HEVC_HDCP),
        X(MFMediaType_Default),
        X(MFMediaType_Protected),
        X(MFVideoFormat_H264_ES),
        X(MFMediaType_Perception),
        X(MFT_CATEGORY_VIDEO_PROCESSOR),
#undef X
    };
    static INIT_ONCE once = INIT_ONCE_STATIC_INIT;

    struct sort_guid_defs_params params = {.array = guid_defs, .count = ARRAY_SIZE(guid_defs)};
    struct guid_def *ret = NULL, tmp = {.guid = guid};

    if (!guid) return "(null)";
    InitOnceExecuteOnce( &once, sort_guid_defs, &params, NULL );

    ret = bsearch(&tmp, guid_defs, ARRAY_SIZE(guid_defs), sizeof(*guid_defs), guid_def_cmp);
    return ret ? wine_dbg_sprintf("%s", ret->name) : wine_dbgstr_guid(guid);
}
