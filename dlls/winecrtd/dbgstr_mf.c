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

const char *debugstr_mf_guid(const GUID *guid)
{
    static struct guid_def guid_defs[] =
    {
#define X(g) { &(g), #g }
        X(EVRConfig_AllowBatching),
        X(EVRConfig_AllowDropToBob),
        X(EVRConfig_AllowDropToHalfInterlace),
        X(EVRConfig_AllowDropToThrottle),
        X(EVRConfig_AllowScaling),
        X(EVRConfig_ForceBatching),
        X(EVRConfig_ForceBob),
        X(EVRConfig_ForceHalfInterlace),
        X(EVRConfig_ForceScaling),
        X(EVRConfig_ForceThrottle),
        X(MF_ACTIVATE_CUSTOM_VIDEO_MIXER_ACTIVATE),
        X(MF_ACTIVATE_CUSTOM_VIDEO_MIXER_CLSID),
        X(MF_ACTIVATE_CUSTOM_VIDEO_MIXER_FLAGS),
        X(MF_ACTIVATE_CUSTOM_VIDEO_PRESENTER_ACTIVATE),
        X(MF_ACTIVATE_CUSTOM_VIDEO_PRESENTER_CLSID),
        X(MF_ACTIVATE_CUSTOM_VIDEO_PRESENTER_FLAGS),
        X(MF_ACTIVATE_MFT_LOCKED),
        X(MF_ACTIVATE_VIDEO_WINDOW),
        X(MF_AUDIO_RENDERER_ATTRIBUTE_ENDPOINT_ID),
        X(MF_AUDIO_RENDERER_ATTRIBUTE_ENDPOINT_ROLE),
        X(MF_AUDIO_RENDERER_ATTRIBUTE_FLAGS),
        X(MF_AUDIO_RENDERER_ATTRIBUTE_SESSION_ID),
        X(MF_AUDIO_RENDERER_ATTRIBUTE_STREAM_CATEGORY),
        X(MF_BYTESTREAM_CONTENT_TYPE),
        X(MF_BYTESTREAM_DLNA_PROFILE_ID),
        X(MF_BYTESTREAM_DURATION),
        X(MF_BYTESTREAM_IFO_FILE_URI),
        X(MF_BYTESTREAM_LAST_MODIFIED_TIME),
        X(MF_BYTESTREAM_ORIGIN_NAME),
        X(MF_D3D12_SYNCHRONIZATION_OBJECT),
        X(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME),
        X(MF_DEVSOURCE_ATTRIBUTE_MEDIA_TYPE),
        X(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE),
        X(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_ENDPOINT_ID),
        X(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID),
        X(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_ROLE),
        X(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_SYMBOLIC_LINK),
        X(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_CATEGORY),
        X(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID),
        X(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_HW_SOURCE),
        X(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_MAX_BUFFERS),
        X(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_PROVIDER_DEVICE_ID),
        X(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK),
        X(MF_DISABLE_FRAME_CORRUPTION_INFO),
        X(MF_EVENT_DO_THINNING),
        X(MF_EVENT_MFT_CONTEXT),
        X(MF_EVENT_MFT_INPUT_STREAM_ID),
        X(MF_EVENT_OUTPUT_NODE),
        X(MF_EVENT_PRESENTATION_TIME_OFFSET),
        X(MF_EVENT_SCRUBSAMPLE_TIME),
        X(MF_EVENT_SESSIONCAPS),
        X(MF_EVENT_SESSIONCAPS_DELTA),
        X(MF_EVENT_SOURCE_ACTUAL_START),
        X(MF_EVENT_SOURCE_CHARACTERISTICS),
        X(MF_EVENT_SOURCE_CHARACTERISTICS_OLD),
        X(MF_EVENT_SOURCE_FAKE_START),
        X(MF_EVENT_SOURCE_PROJECTSTART),
        X(MF_EVENT_SOURCE_TOPOLOGY_CANCELED),
        X(MF_EVENT_START_PRESENTATION_TIME),
        X(MF_EVENT_START_PRESENTATION_TIME_AT_OUTPUT),
        X(MF_EVENT_STREAM_METADATA_CONTENT_KEYIDS),
        X(MF_EVENT_STREAM_METADATA_KEYDATA),
        X(MF_EVENT_STREAM_METADATA_SYSTEMID),
        X(MF_EVENT_TOPOLOGY_STATUS),
        X(MF_LOW_LATENCY),
        X(MF_MEDIA_ENGINE_AUDIO_CATEGORY),
        X(MF_MEDIA_ENGINE_AUDIO_ENDPOINT_ROLE),
        X(MF_MEDIA_ENGINE_BROWSER_COMPATIBILITY_MODE),
        X(MF_MEDIA_ENGINE_BROWSER_COMPATIBILITY_MODE_IE10),
        X(MF_MEDIA_ENGINE_BROWSER_COMPATIBILITY_MODE_IE11),
        X(MF_MEDIA_ENGINE_BROWSER_COMPATIBILITY_MODE_IE9),
        X(MF_MEDIA_ENGINE_BROWSER_COMPATIBILITY_MODE_IE_EDGE),
        X(MF_MEDIA_ENGINE_CALLBACK),
        X(MF_MEDIA_ENGINE_COMPATIBILITY_MODE),
        X(MF_MEDIA_ENGINE_COMPATIBILITY_MODE_WIN10),
        X(MF_MEDIA_ENGINE_COMPATIBILITY_MODE_WWA_EDGE),
        X(MF_MEDIA_ENGINE_CONTENT_PROTECTION_FLAGS),
        X(MF_MEDIA_ENGINE_CONTENT_PROTECTION_MANAGER),
        X(MF_MEDIA_ENGINE_CONTINUE_ON_CODEC_ERROR),
        X(MF_MEDIA_ENGINE_COREWINDOW),
        X(MF_MEDIA_ENGINE_DXGI_MANAGER),
        X(MF_MEDIA_ENGINE_EME_CALLBACK),
        X(MF_MEDIA_ENGINE_EXTENSION),
        X(MF_MEDIA_ENGINE_MEDIA_PLAYER_MODE),
        X(MF_MEDIA_ENGINE_NEEDKEY_CALLBACK),
        X(MF_MEDIA_ENGINE_OPM_HWND),
        X(MF_MEDIA_ENGINE_PLAYBACK_HWND),
        X(MF_MEDIA_ENGINE_PLAYBACK_VISUAL),
        X(MF_MEDIA_ENGINE_SOURCE_RESOLVER_CONFIG_STORE),
        X(MF_MEDIA_ENGINE_STREAM_CONTAINS_ALPHA_CHANNEL),
        X(MF_MEDIA_ENGINE_SYNCHRONOUS_CLOSE),
        X(MF_MEDIA_ENGINE_TELEMETRY_APPLICATION_ID),
        X(MF_MEDIA_ENGINE_TRACK_ID),
        X(MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT),
        X(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION),
        X(MF_MT_AAC_PAYLOAD_TYPE),
        X(MF_MT_ALL_SAMPLES_INDEPENDENT),
        X(MF_MT_ALPHA_MODE),
        X(MF_MT_AM_FORMAT_TYPE),
        X(MF_MT_ARBITRARY_FORMAT),
        X(MF_MT_ARBITRARY_HEADER),
        X(MF_MT_AUDIO_AVG_BYTES_PER_SECOND),
        X(MF_MT_AUDIO_BITS_PER_SAMPLE),
        X(MF_MT_AUDIO_BLOCK_ALIGNMENT),
        X(MF_MT_AUDIO_CHANNEL_MASK),
        X(MF_MT_AUDIO_FLAC_MAX_BLOCK_SIZE),
        X(MF_MT_AUDIO_FLOAT_SAMPLES_PER_SECOND),
        X(MF_MT_AUDIO_FOLDDOWN_MATRIX),
        X(MF_MT_AUDIO_NUM_CHANNELS),
        X(MF_MT_AUDIO_PREFER_WAVEFORMATEX),
        X(MF_MT_AUDIO_SAMPLES_PER_BLOCK),
        X(MF_MT_AUDIO_SAMPLES_PER_SECOND),
        X(MF_MT_AUDIO_VALID_BITS_PER_SAMPLE),
        X(MF_MT_AUDIO_WMADRC_AVGREF),
        X(MF_MT_AUDIO_WMADRC_AVGTARGET),
        X(MF_MT_AUDIO_WMADRC_PEAKREF),
        X(MF_MT_AUDIO_WMADRC_PEAKTARGET),
        X(MF_MT_AVG_BIT_ERROR_RATE),
        X(MF_MT_AVG_BITRATE),
        X(MF_MT_COMPRESSED),
        X(MF_MT_CUSTOM_VIDEO_PRIMARIES),
        X(MF_MT_D3D12_CPU_READBACK),
        X(MF_MT_D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER),
        X(MF_MT_D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
        X(MF_MT_D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
        X(MF_MT_D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS),
        X(MF_MT_D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
        X(MF_MT_D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE),
        X(MF_MT_D3D12_TEXTURE_LAYOUT),
        X(MF_MT_D3D_RESOURCE_VERSION),
        X(MF_MT_DECODER_MAX_DPB_COUNT),
        X(MF_MT_DECODER_USE_MAX_RESOLUTION),
        X(MF_MT_DEFAULT_STRIDE),
        X(MF_MT_DEPTH_MEASUREMENT),
        X(MF_MT_DEPTH_VALUE_UNIT),
        X(MF_MT_DRM_FLAGS),
        X(MF_MT_DV_AAUX_CTRL_PACK_0),
        X(MF_MT_DV_AAUX_CTRL_PACK_1),
        X(MF_MT_DV_AAUX_SRC_PACK_0),
        X(MF_MT_DV_AAUX_SRC_PACK_1),
        X(MF_MT_DV_VAUX_CTRL_PACK),
        X(MF_MT_DV_VAUX_SRC_PACK),
        X(MF_MT_FIXED_SIZE_SAMPLES),
        X(MF_MT_FORWARD_CUSTOM_NALU),
        X(MF_MT_FORWARD_CUSTOM_SEI),
        X(MF_MT_FRAME_RATE),
        X(MF_MT_FRAME_RATE_RANGE_MAX),
        X(MF_MT_FRAME_RATE_RANGE_MIN),
        X(MF_MT_FRAME_SIZE),
        X(MF_MT_GEOMETRIC_APERTURE),
        X(MF_MT_H264_CAPABILITIES),
        X(MF_MT_H264_LAYOUT_PER_STREAM),
        X(MF_MT_H264_MAX_CODEC_CONFIG_DELAY),
        X(MF_MT_H264_MAX_MB_PER_SEC),
        X(MF_MT_H264_RATE_CONTROL_MODES),
        X(MF_MT_H264_RESOLUTION_SCALING),
        X(MF_MT_H264_SIMULCAST_SUPPORT),
        X(MF_MT_H264_SUPPORTED_RATE_CONTROL_MODES),
        X(MF_MT_H264_SUPPORTED_SLICE_MODES),
        X(MF_MT_H264_SUPPORTED_SYNC_FRAME_TYPES),
        X(MF_MT_H264_SUPPORTED_USAGES),
        X(MF_MT_H264_SVC_CAPABILITIES),
        X(MF_MT_H264_USAGE),
        X(MF_MT_IMAGE_LOSS_TOLERANT),
        X(MF_MT_IN_BAND_PARAMETER_SET),
        X(MF_MT_INTERLACE_MODE),
        X(MF_MT_MAJOR_TYPE),
        X(MF_MT_MAX_FRAME_AVERAGE_LUMINANCE_LEVEL),
        X(MF_MT_MAX_KEYFRAME_SPACING),
        X(MF_MT_MAX_LUMINANCE_LEVEL),
        X(MF_MT_MIN_MASTERING_LUMINANCE),
        X(MF_MT_MINIMUM_DISPLAY_APERTURE),
        X(MF_MT_MPEG2_CONTENT_PACKET),
        X(MF_MT_MPEG2_FLAGS),
        X(MF_MT_MPEG2_HDCP),
        X(MF_MT_MPEG2_ONE_FRAME_PER_PACKET),
        X(MF_MT_MPEG2_STANDARD),
        X(MF_MT_MPEG2_TIMECODE),
        X(MF_MT_MPEG4_CURRENT_SAMPLE_ENTRY),
        X(MF_MT_MPEG4_SAMPLE_DESCRIPTION),
        X(MF_MT_MPEG4_TRACK_TYPE),
        X(MF_MT_MPEG_SEQUENCE_HEADER),
        X(MF_MT_MPEG_START_TIME_CODE),
        X(MF_MT_ORIGINAL_4CC),
        X(MF_MT_ORIGINAL_WAVE_FORMAT_TAG),
        X(MF_MT_OUTPUT_BUFFER_NUM),
        X(MF_MT_PAD_CONTROL_FLAGS),
        X(MF_MT_PALETTE),
        X(MF_MT_PAN_SCAN_APERTURE),
        X(MF_MT_PAN_SCAN_ENABLED),
        X(MF_MT_PIXEL_ASPECT_RATIO),
        X(MF_MT_REALTIME_CONTENT),
        X(MF_MT_SAMPLE_SIZE),
        X(MF_MT_SECURE),
        X(MF_MT_SOURCE_CONTENT_HINT),
        X(MF_MT_SPATIAL_AUDIO_DATA_PRESENT),
        X(MF_MT_SPATIAL_AUDIO_MAX_DYNAMIC_OBJECTS),
        X(MF_MT_SPATIAL_AUDIO_MAX_METADATA_ITEMS),
        X(MF_MT_SPATIAL_AUDIO_MIN_METADATA_ITEM_OFFSET_SPACING),
        X(MF_MT_SPATIAL_AUDIO_OBJECT_METADATA_FORMAT_ID),
        X(MF_MT_SPATIAL_AUDIO_OBJECT_METADATA_LENGTH),
        X(MF_MT_SUBTYPE),
        X(MF_MT_TIMESTAMP_CAN_BE_DTS),
        X(MF_MT_TRANSFER_FUNCTION),
        X(MF_MT_USER_DATA),
        X(MF_MT_VIDEO_3D),
        X(MF_MT_VIDEO_3D_FIRST_IS_LEFT),
        X(MF_MT_VIDEO_3D_FORMAT),
        X(MF_MT_VIDEO_3D_LEFT_IS_BASE),
        X(MF_MT_VIDEO_3D_NUM_VIEWS),
        X(MF_MT_VIDEO_CHROMA_SITING),
        X(MF_MT_VIDEO_H264_NO_FMOASO),
        X(MF_MT_VIDEO_LEVEL),
        X(MF_MT_VIDEO_LIGHTING),
        X(MF_MT_VIDEO_NO_FRAME_ORDERING),
        X(MF_MT_VIDEO_NOMINAL_RANGE),
        X(MF_MT_VIDEO_PRIMARIES),
        X(MF_MT_VIDEO_PROFILE),
        X(MF_MT_VIDEO_RENDERER_EXTENSION_PROFILE),
        X(MF_MT_VIDEO_ROTATION),
        X(MF_MT_WRAPPED_TYPE),
        X(MF_MT_YUV_MATRIX),
        X(MF_PD_ADAPTIVE_STREAMING),
        X(MF_PD_APP_CONTEXT),
        X(MF_PD_AUDIO_ENCODING_BITRATE),
        X(MF_PD_AUDIO_ISVARIABLEBITRATE),
        X(MF_PD_DURATION),
        X(MF_PD_LAST_MODIFIED_TIME),
        X(MF_PD_MIME_TYPE),
        X(MF_PD_PLAYBACK_BOUNDARY_TIME),
        X(MF_PD_PLAYBACK_ELEMENT_ID),
        X(MF_PD_PMPHOST_CONTEXT),
        X(MF_PD_PREFERRED_LANGUAGE),
        X(MF_PD_SAMI_STYLELIST),
        X(MF_PD_TOTAL_FILE_SIZE),
        X(MF_PD_VIDEO_ENCODING_BITRATE),
        X(MF_PMP_SERVER_CONTEXT),
        X(MF_READWRITE_D3D_OPTIONAL),
        X(MF_READWRITE_DISABLE_CONVERTERS),
        X(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS),
        X(MF_READWRITE_MMCSS_CLASS),
        X(MF_READWRITE_MMCSS_CLASS_AUDIO),
        X(MF_READWRITE_MMCSS_PRIORITY),
        X(MF_READWRITE_MMCSS_PRIORITY_AUDIO),
        X(MF_SA_BUFFERS_PER_SAMPLE),
        X(MF_SA_D3D11_ALLOW_DYNAMIC_YUV_TEXTURE),
        X(MF_SA_D3D11_AWARE),
        X(MF_SA_D3D11_BINDFLAGS),
        X(MF_SA_D3D11_HW_PROTECTED),
        X(MF_SA_D3D11_SHARED),
        X(MF_SA_D3D11_SHARED_WITHOUT_MUTEX),
        X(MF_SA_D3D11_USAGE),
        X(MF_SA_D3D12_CLEAR_VALUE),
        X(MF_SA_D3D12_HEAP_FLAGS),
        X(MF_SA_D3D12_HEAP_TYPE),
        X(MF_SA_D3D_AWARE),
        X(MF_SA_MINIMUM_OUTPUT_SAMPLE_COUNT),
        X(MF_SA_MINIMUM_OUTPUT_SAMPLE_COUNT_PROGRESSIVE),
        X(MF_SA_REQUIRED_SAMPLE_COUNT),
        X(MF_SA_REQUIRED_SAMPLE_COUNT_PROGRESSIVE),
        X(MF_SAMPLEGRABBERSINK_IGNORE_CLOCK),
        X(MF_SAMPLEGRABBERSINK_SAMPLE_TIME_OFFSET),
        X(MF_SD_LANGUAGE),
        X(MF_SD_MUTUALLY_EXCLUSIVE),
        X(MF_SD_PROTECTED),
        X(MF_SD_SAMI_LANGUAGE),
        X(MF_SD_STREAM_NAME),
        X(MF_SESSION_APPROX_EVENT_OCCURRENCE_TIME),
        X(MF_SESSION_CONTENT_PROTECTION_MANAGER),
        X(MF_SESSION_GLOBAL_TIME),
        X(MF_SESSION_QUALITY_MANAGER),
        X(MF_SESSION_REMOTE_SOURCE_MODE),
        X(MF_SESSION_SERVER_CONTEXT),
        X(MF_SESSION_TOPOLOADER),
        X(MF_SINK_WRITER_ASYNC_CALLBACK),
        X(MF_SINK_WRITER_DISABLE_THROTTLING),
        X(MF_SINK_WRITER_ENCODER_CONFIG),
        X(MF_SOURCE_READER_ASYNC_CALLBACK),
        X(MF_SOURCE_READER_D3D11_BIND_FLAGS),
        X(MF_SOURCE_READER_DISABLE_CAMERA_PLUGINS),
        X(MF_SOURCE_READER_DISABLE_DXVA),
        X(MF_SOURCE_READER_DISCONNECT_MEDIASOURCE_ON_SHUTDOWN),
        X(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING),
        X(MF_SOURCE_READER_ENABLE_TRANSCODE_ONLY_TRANSFORMS),
        X(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING),
        X(MF_SOURCE_READER_MEDIASOURCE_CHARACTERISTICS),
        X(MF_SOURCE_READER_MEDIASOURCE_CONFIG),
        X(MF_TOPOLOGY_DXVA_MODE),
        X(MF_TOPOLOGY_DYNAMIC_CHANGE_NOT_ALLOWED),
        X(MF_TOPOLOGY_ENABLE_XVP_FOR_PLAYBACK),
        X(MF_TOPOLOGY_ENUMERATE_SOURCE_TYPES),
        X(MF_TOPOLOGY_HARDWARE_MODE),
        X(MF_TOPOLOGY_NO_MARKIN_MARKOUT),
        X(MF_TOPOLOGY_PLAYBACK_FRAMERATE),
        X(MF_TOPOLOGY_PLAYBACK_MAX_DIMS),
        X(MF_TOPOLOGY_PROJECTSTART),
        X(MF_TOPOLOGY_PROJECTSTOP),
        X(MF_TOPOLOGY_RESOLUTION_STATUS),
        X(MF_TOPOLOGY_START_TIME_ON_PRESENTATION_SWITCH),
        X(MF_TOPOLOGY_STATIC_PLAYBACK_OPTIMIZATIONS),
        X(MF_TOPONODE_CONNECT_METHOD),
        X(MF_TOPONODE_D3DAWARE),
        X(MF_TOPONODE_DECODER),
        X(MF_TOPONODE_DECRYPTOR),
        X(MF_TOPONODE_DISABLE_PREROLL),
        X(MF_TOPONODE_DISCARDABLE),
        X(MF_TOPONODE_DRAIN),
        X(MF_TOPONODE_ERROR_MAJORTYPE),
        X(MF_TOPONODE_ERROR_SUBTYPE),
        X(MF_TOPONODE_ERRORCODE),
        X(MF_TOPONODE_FLUSH),
        X(MF_TOPONODE_LOCKED),
        X(MF_TOPONODE_MARKIN_HERE),
        X(MF_TOPONODE_MARKOUT_HERE),
        X(MF_TOPONODE_MEDIASTART),
        X(MF_TOPONODE_MEDIASTOP),
        X(MF_TOPONODE_NOSHUTDOWN_ON_REMOVE),
        X(MF_TOPONODE_PRESENTATION_DESCRIPTOR),
        X(MF_TOPONODE_PRIMARYOUTPUT),
        X(MF_TOPONODE_RATELESS),
        X(MF_TOPONODE_SEQUENCE_ELEMENTID),
        X(MF_TOPONODE_SOURCE),
        X(MF_TOPONODE_STREAM_DESCRIPTOR),
        X(MF_TOPONODE_STREAMID),
        X(MF_TOPONODE_TRANSFORM_OBJECTID),
        X(MF_TOPONODE_WORKQUEUE_ID),
        X(MF_TOPONODE_WORKQUEUE_ITEM_PRIORITY),
        X(MF_TOPONODE_WORKQUEUE_MMCSS_CLASS),
        X(MF_TOPONODE_WORKQUEUE_MMCSS_PRIORITY),
        X(MF_TOPONODE_WORKQUEUE_MMCSS_TASKID),
        X(MF_TRANSCODE_ADJUST_PROFILE),
        X(MF_TRANSCODE_CONTAINERTYPE),
        X(MF_TRANSCODE_DONOT_INSERT_ENCODER),
        X(MF_TRANSCODE_ENCODINGPROFILE),
        X(MF_TRANSCODE_QUALITYVSSPEED),
        X(MF_TRANSCODE_SKIP_METADATA_TRANSFER),
        X(MF_TRANSCODE_TOPOLOGYMODE),
        X(MF_TRANSFORM_ASYNC),
        X(MF_TRANSFORM_ASYNC_UNLOCK),
        X(MF_TRANSFORM_CATEGORY_Attribute),
        X(MF_TRANSFORM_FLAGS_Attribute),
        X(MF_VIDEO_MAX_MB_PER_SEC),
        X(MF_VIDEO_PROCESSOR_ALGORITHM),
        X(MF_XVP_CALLER_ALLOCATES_OUTPUT),
        X(MF_XVP_DISABLE_FRC),
        X(MF_XVP_SAMPLE_LOCK_TIMEOUT),
        X(MFAudioFormat_AAC),
        X(MFAudioFormat_AAC_HDCP),
        X(MFAudioFormat_ADTS),
        X(MFAudioFormat_ADTS_HDCP),
        X(MFAudioFormat_ALAC),
        X(MFAudioFormat_AMR_NB),
        X(MFAudioFormat_AMR_WB),
        X(MFAudioFormat_AMR_WP),
        X(MFAudioFormat_Base_HDCP),
        X(MFAudioFormat_Dolby_AC3),
        X(MFAudioFormat_Dolby_AC3_HDCP),
        X(MFAudioFormat_Dolby_AC3_SPDIF),
        X(MFAudioFormat_Dolby_DDPlus),
        X(MFAudioFormat_DRM),
        X(MFAudioFormat_DTS),
        X(MFAudioFormat_FLAC),
        X(MFAudioFormat_Float),
        X(MFAudioFormat_Float_SpatialObjects),
        X(MFAudioFormat_LPCM),
        X(MFAudioFormat_MP3),
        X(MFAudioFormat_MPEG),
        X(MFAudioFormat_MSP1),
        X(MFAudioFormat_Opus),
        X(MFAudioFormat_PCM),
        X(MFAudioFormat_PCM_HDCP),
        X(MFAudioFormat_Vorbis),
        X(MFAudioFormat_WMASPDIF),
        X(MFAudioFormat_WMAudio_Lossless),
        X(MFAudioFormat_WMAudioV8),
        X(MFAudioFormat_WMAudioV9),
        X(MFMediaType_Audio),
        X(MFMediaType_Binary),
        X(MFMediaType_Default),
        X(MFMediaType_FileTransfer),
        X(MFMediaType_HTML),
        X(MFMediaType_Image),
        X(MFMediaType_MultiplexedFrames),
        X(MFMediaType_Perception),
        X(MFMediaType_Protected),
        X(MFMediaType_SAMI),
        X(MFMediaType_Script),
        X(MFMediaType_Stream),
        X(MFMediaType_Subtitle),
        X(MFMediaType_Video),
        X(MFSampleExtension_3DVideo),
        X(MFSampleExtension_3DVideo_SampleFormat),
        X(MFSampleExtension_DecodeTimestamp),
        X(MFSampleExtension_ForwardedDecodeUnits),
        X(MFSampleExtension_ForwardedDecodeUnitType),
        X(MFSampleExtension_TargetGlobalLuminance),
        X(MFSampleExtension_Timestamp),
        X(MFSampleExtension_Token),
        X(MFT_CATEGORY_AUDIO_DECODER),
        X(MFT_CATEGORY_AUDIO_EFFECT),
        X(MFT_CATEGORY_AUDIO_ENCODER),
        X(MFT_CATEGORY_DEMULTIPLEXER),
        X(MFT_CATEGORY_MULTIPLEXER),
        X(MFT_CATEGORY_OTHER),
        X(MFT_CATEGORY_VIDEO_DECODER),
        X(MFT_CATEGORY_VIDEO_EFFECT),
        X(MFT_CATEGORY_VIDEO_ENCODER),
        X(MFT_CATEGORY_VIDEO_PROCESSOR),
        X(MFT_CODEC_MERIT_Attribute),
        X(MFT_CONNECTED_STREAM_ATTRIBUTE),
        X(MFT_CONNECTED_TO_HW_STREAM),
        X(MFT_DECODER_EXPOSE_OUTPUT_TYPES_IN_NATIVE_ORDER),
        X(MFT_DECODER_FINAL_VIDEO_RESOLUTION_HINT),
        X(MFT_ENCODER_SUPPORTS_CONFIG_EVENT),
        X(MFT_ENUM_ADAPTER_LUID),
        X(MFT_ENUM_HARDWARE_URL_Attribute),
        X(MFT_ENUM_HARDWARE_VENDOR_ID_Attribute),
        X(MFT_ENUM_TRANSCODE_ONLY_ATTRIBUTE),
        X(MFT_FIELDOFUSE_UNLOCK_Attribute),
        X(MFT_FRIENDLY_NAME_Attribute),
        X(MFT_INPUT_TYPES_Attributes),
        X(MFT_OUTPUT_TYPES_Attributes),
        X(MFT_PREFERRED_ENCODER_PROFILE),
        X(MFT_PREFERRED_OUTPUTTYPE_Attribute),
        X(MFT_PROCESS_LOCAL_Attribute),
        X(MFT_PROCESS_LOCAL_Attribute),
        X(MFT_REMUX_MARK_I_PICTURE_AS_CLEAN_POINT),
        X(MFT_REMUX_MARK_I_PICTURE_AS_CLEAN_POINT),
        X(MFT_SUPPORT_3DVIDEO),
        X(MFT_SUPPORT_3DVIDEO),
        X(MFT_SUPPORT_DYNAMIC_FORMAT_CHANGE),
        X(MFT_TRANSFORM_CLSID_Attribute),
        X(MFTranscodeContainerType_3GP),
        X(MFTranscodeContainerType_AC3),
        X(MFTranscodeContainerType_ADTS),
        X(MFTranscodeContainerType_AMR),
        X(MFTranscodeContainerType_ASF),
        X(MFTranscodeContainerType_AVI),
        X(MFTranscodeContainerType_FLAC),
        X(MFTranscodeContainerType_FMPEG4),
        X(MFTranscodeContainerType_MP3),
        X(MFTranscodeContainerType_MPEG2),
        X(MFTranscodeContainerType_MPEG4),
        X(MFTranscodeContainerType_WAVE),
        X(MFVideoFormat_420O),
        X(MFVideoFormat_A16B16G16R16F),
        X(MFVideoFormat_A2R10G10B10),
        X(MFVideoFormat_AI44),
        X(MFVideoFormat_ARGB32),
        X(MFVideoFormat_AV1),
        X(MFVideoFormat_AYUV),
        X(MFVideoFormat_Base_HDCP),
        X(MFVideoFormat_D16),
        X(MFVideoFormat_DV25),
        X(MFVideoFormat_DV50),
        X(MFVideoFormat_DVC),
        X(MFVideoFormat_DVH1),
        X(MFVideoFormat_DVHD),
        X(MFVideoFormat_DVSD),
        X(MFVideoFormat_DVSL),
        X(MFVideoFormat_H263),
        X(MFVideoFormat_H264),
        X(MFVideoFormat_H264_ES),
        X(MFVideoFormat_H264_HDCP),
        X(MFVideoFormat_H265),
        X(MFVideoFormat_HEVC),
        X(MFVideoFormat_HEVC_ES),
        X(MFVideoFormat_HEVC_HDCP),
        X(MFVideoFormat_I420),
        X(MFVideoFormat_IYUV),
        X(MFVideoFormat_L16),
        X(MFVideoFormat_L8),
        X(MFVideoFormat_M4S2),
        X(MFVideoFormat_MJPG),
        X(MFVideoFormat_MP43),
        X(MFVideoFormat_MP4S),
        X(MFVideoFormat_MP4V),
        X(MFVideoFormat_MPEG2),
        X(MFVideoFormat_MPG1),
        X(MFVideoFormat_MSS1),
        X(MFVideoFormat_MSS2),
        X(MFVideoFormat_NV11),
        X(MFVideoFormat_NV12),
        X(MFVideoFormat_ORAW),
        X(MFVideoFormat_P010),
        X(MFVideoFormat_P016),
        X(MFVideoFormat_P210),
        X(MFVideoFormat_P216),
        X(MFVideoFormat_RGB24),
        X(MFVideoFormat_RGB32),
        X(MFVideoFormat_RGB555),
        X(MFVideoFormat_RGB565),
        X(MFVideoFormat_RGB8),
        X(MFVideoFormat_UYVY),
        X(MFVideoFormat_v210),
        X(MFVideoFormat_v216),
        X(MFVideoFormat_v410),
        X(MFVideoFormat_VP10),
        X(MFVideoFormat_VP80),
        X(MFVideoFormat_VP90),
        X(MFVideoFormat_WMV1),
        X(MFVideoFormat_WMV2),
        X(MFVideoFormat_WMV3),
        X(MFVideoFormat_WVC1),
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
        X(VIDEO_ZOOM_RECT),
#undef X
    };
    struct guid_def *ret = NULL, tmp = {.guid = guid};
    static BOOL sorted;

    if (!guid) return "(null)";
    if (!sorted)
    {
        static SRWLOCK lock;
        RtlAcquireSRWLockExclusive(&lock);

        qsort(guid_defs, ARRAY_SIZE(guid_defs), sizeof(struct guid_def), guid_def_cmp);
        sorted = TRUE;

        RtlReleaseSRWLockExclusive(&lock);
    }

    ret = bsearch(&tmp, guid_defs, ARRAY_SIZE(guid_defs), sizeof(*guid_defs), guid_def_cmp);
    return ret ? wine_dbg_sprintf("%s", ret->name) : wine_dbgstr_guid(guid);
}

const char *debugstr_mf_media_event_type(DWORD event)
{
    switch (event)
    {
#define X(e) case e: return #e
        X(MEUnknown);
        X(MEError);
        X(MEExtendedType);
        X(MENonFatalError);
        X(MESessionUnknown);
        X(MESessionTopologySet);
        X(MESessionTopologiesCleared);
        X(MESessionStarted);
        X(MESessionPaused);
        X(MESessionStopped);
        X(MESessionClosed);
        X(MESessionEnded);
        X(MESessionRateChanged);
        X(MESessionScrubSampleComplete);
        X(MESessionCapabilitiesChanged);
        X(MESessionTopologyStatus);
        X(MESessionNotifyPresentationTime);
        X(MENewPresentation);
        X(MELicenseAcquisitionStart);
        X(MELicenseAcquisitionCompleted);
        X(MEIndividualizationStart);
        X(MEIndividualizationCompleted);
        X(MEEnablerProgress);
        X(MEEnablerCompleted);
        X(MEPolicyError);
        X(MEPolicyReport);
        X(MEBufferingStarted);
        X(MEBufferingStopped);
        X(MEConnectStart);
        X(MEConnectEnd);
        X(MEReconnectStart);
        X(MEReconnectEnd);
        X(MERendererEvent);
        X(MESessionStreamSinkFormatChanged);
        X(MESourceUnknown);
        X(MESourceStarted);
        X(MEStreamStarted);
        X(MESourceSeeked);
        X(MEStreamSeeked);
        X(MENewStream);
        X(MEUpdatedStream);
        X(MESourceStopped);
        X(MEStreamStopped);
        X(MESourcePaused);
        X(MEStreamPaused);
        X(MEEndOfPresentation);
        X(MEEndOfStream);
        X(MEMediaSample);
        X(MEStreamTick);
        X(MEStreamThinMode);
        X(MEStreamFormatChanged);
        X(MESourceRateChanged);
        X(MEEndOfPresentationSegment);
        X(MESourceCharacteristicsChanged);
        X(MESourceRateChangeRequested);
        X(MESourceMetadataChanged);
        X(MESequencerSourceTopologyUpdated);
        X(MESinkUnknown);
        X(MEStreamSinkStarted);
        X(MEStreamSinkStopped);
        X(MEStreamSinkPaused);
        X(MEStreamSinkRateChanged);
        X(MEStreamSinkRequestSample);
        X(MEStreamSinkMarker);
        X(MEStreamSinkPrerolled);
        X(MEStreamSinkScrubSampleComplete);
        X(MEStreamSinkFormatChanged);
        X(MEStreamSinkDeviceChanged);
        X(MEQualityNotify);
        X(MESinkInvalidated);
        X(MEAudioSessionNameChanged);
        X(MEAudioSessionVolumeChanged);
        X(MEAudioSessionDeviceRemoved);
        X(MEAudioSessionServerShutdown);
        X(MEAudioSessionGroupingParamChanged);
        X(MEAudioSessionIconChanged);
        X(MEAudioSessionFormatChanged);
        X(MEAudioSessionDisconnected);
        X(MEAudioSessionExclusiveModeOverride);
        X(MECaptureAudioSessionVolumeChanged);
        X(MECaptureAudioSessionDeviceRemoved);
        X(MECaptureAudioSessionFormatChanged);
        X(MECaptureAudioSessionDisconnected);
        X(MECaptureAudioSessionExclusiveModeOverride);
        X(MECaptureAudioSessionServerShutdown);
        X(METrustUnknown);
        X(MEPolicyChanged);
        X(MEContentProtectionMessage);
        X(MEPolicySet);
        X(MEWMDRMLicenseBackupCompleted);
        X(MEWMDRMLicenseBackupProgress);
        X(MEWMDRMLicenseRestoreCompleted);
        X(MEWMDRMLicenseRestoreProgress);
        X(MEWMDRMLicenseAcquisitionCompleted);
        X(MEWMDRMIndividualizationCompleted);
        X(MEWMDRMIndividualizationProgress);
        X(MEWMDRMProximityCompleted);
        X(MEWMDRMLicenseStoreCleaned);
        X(MEWMDRMRevocationDownloadCompleted);
        X(METransformUnknown);
        X(METransformNeedInput);
        X(METransformHaveOutput);
        X(METransformDrainComplete);
        X(METransformMarker);
        X(METransformInputStreamStateChanged);
        X(MEByteStreamCharacteristicsChanged);
        X(MEVideoCaptureDeviceRemoved);
        X(MEVideoCaptureDevicePreempted);
        X(MEStreamSinkFormatInvalidated);
        X(MEEncodingParameters);
        X(MEContentProtectionMetadata);
        X(MEDeviceThermalStateChanged);
#undef X
    }

    return wine_dbg_sprintf("%u", event);
}
