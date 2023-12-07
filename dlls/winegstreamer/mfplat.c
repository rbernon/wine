/*
 * Copyright 2019 Nikolay Sivov for CodeWeavers
 * Copyright 2020 Zebediah Figura for CodeWeavers
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

#include "gst_private.h"

#include "ks.h"
#include "ksmedia.h"
#include "wmcodecdsp.h"
#include "d3d9types.h"
#include "mfapi.h"
#include "mmreg.h"

#include "wine/debug.h"
#include "wine/list.h"

#include "initguid.h"

WINE_DEFAULT_DEBUG_CHANNEL(mfplat);

DEFINE_GUID(DMOVideoFormat_RGB32,D3DFMT_X8R8G8B8,0x524f,0x11ce,0x9f,0x53,0x00,0x20,0xaf,0x0b,0xa7,0x70);
DEFINE_GUID(DMOVideoFormat_RGB24,D3DFMT_R8G8B8,0x524f,0x11ce,0x9f,0x53,0x00,0x20,0xaf,0x0b,0xa7,0x70);
DEFINE_GUID(DMOVideoFormat_RGB565,D3DFMT_R5G6B5,0x524f,0x11ce,0x9f,0x53,0x00,0x20,0xaf,0x0b,0xa7,0x70);
DEFINE_GUID(DMOVideoFormat_RGB555,D3DFMT_X1R5G5B5,0x524f,0x11ce,0x9f,0x53,0x00,0x20,0xaf,0x0b,0xa7,0x70);
DEFINE_GUID(DMOVideoFormat_RGB8,D3DFMT_P8,0x524f,0x11ce,0x9f,0x53,0x00,0x20,0xaf,0x0b,0xa7,0x70);
DEFINE_MEDIATYPE_GUID(MFAudioFormat_RAW_AAC,WAVE_FORMAT_RAW_AAC1);
DEFINE_MEDIATYPE_GUID(MFVideoFormat_VC1S,MAKEFOURCC('V','C','1','S'));
DEFINE_MEDIATYPE_GUID(MFVideoFormat_IV50,MAKEFOURCC('I','V','5','0'));
DEFINE_MEDIATYPE_GUID(MFVideoFormat_ABGR32,D3DFMT_A8B8G8R8);
DEFINE_MEDIATYPE_GUID(MFAudioFormat_GStreamer,MAKEFOURCC('G','S','T','a'));
DEFINE_MEDIATYPE_GUID(MFVideoFormat_GStreamer,MAKEFOURCC('G','S','T','v'));
DEFINE_GUID(MEDIASUBTYPE_WMV_Unknown, 0x7ce12ca9,0xbfbf,0x43d9,0x9d,0x00,0x82,0xb8,0xed,0x54,0x31,0x6b);

struct class_factory
{
    IClassFactory IClassFactory_iface;
    LONG refcount;
    HRESULT (*create_instance)(REFIID riid, void **obj);
};

static struct class_factory *impl_from_IClassFactory(IClassFactory *iface)
{
    return CONTAINING_RECORD(iface, struct class_factory, IClassFactory_iface);
}

static HRESULT WINAPI class_factory_QueryInterface(IClassFactory *iface, REFIID riid, void **obj)
{
    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), obj);

    if (IsEqualGUID(riid, &IID_IClassFactory) ||
            IsEqualGUID(riid, &IID_IUnknown))
    {
        *obj = iface;
        IClassFactory_AddRef(iface);
        return S_OK;
    }

    WARN("%s is not supported.\n", debugstr_guid(riid));
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI class_factory_AddRef(IClassFactory *iface)
{
    struct class_factory *factory = impl_from_IClassFactory(iface);
    return InterlockedIncrement(&factory->refcount);
}

static ULONG WINAPI class_factory_Release(IClassFactory *iface)
{
    struct class_factory *factory = impl_from_IClassFactory(iface);
    ULONG refcount = InterlockedDecrement(&factory->refcount);

    if (!refcount)
        free(factory);

    return refcount;
}

static HRESULT WINAPI class_factory_CreateInstance(IClassFactory *iface, IUnknown *outer, REFIID riid, void **obj)
{
    struct class_factory *factory = impl_from_IClassFactory(iface);

    TRACE("%p, %p, %s, %p.\n", iface, outer, debugstr_guid(riid), obj);

    if (outer)
    {
        *obj = NULL;
        return CLASS_E_NOAGGREGATION;
    }

    return factory->create_instance(riid, obj);
}

static HRESULT WINAPI class_factory_LockServer(IClassFactory *iface, BOOL dolock)
{
    TRACE("%p, %d.\n", iface, dolock);
    return S_OK;
}

static const IClassFactoryVtbl class_factory_vtbl =
{
    class_factory_QueryInterface,
    class_factory_AddRef,
    class_factory_Release,
    class_factory_CreateInstance,
    class_factory_LockServer,
};

static const GUID CLSID_GStreamerAudioDecoder = {0x480b1517, 0xc8e9, 0x4eae, {0xb0, 0x06, 0xe6, 0x30, 0x07, 0x18, 0xd8, 0x5d}};
static const GUID CLSID_GStreamerVideoDecoder = {0x480b1518, 0xc8e9, 0x4eae, {0xb0, 0x06, 0xe6, 0x30, 0x07, 0x18, 0xd8, 0x5d}};
static const GUID CLSID_GStreamerByteStreamHandler = {0x317df618, 0x5e5a, 0x468a, {0x9f, 0x15, 0xd8, 0x27, 0xa9, 0xa0, 0x81, 0x62}};

static const GUID CLSID_GStreamerSchemePlugin = {0x587eeb6a,0x7336,0x4ebd,{0xa4,0xf2,0x91,0xc9,0x48,0xde,0x62,0x2c}};

static const struct class_object
{
    const GUID *clsid;
    HRESULT (*create_instance)(REFIID riid, void **obj);
}
class_objects[] =
{
    { &CLSID_GStreamerAudioDecoder, &audio_decoder_create },
    { &CLSID_GStreamerVideoDecoder, &video_decoder_create },
    { &CLSID_VideoProcessorMFT, &video_processor_create },
    { &CLSID_GStreamerByteStreamHandler, &gstreamer_byte_stream_handler_create },
    { &CLSID_MSAACDecMFT, &aac_decoder_create },
    { &CLSID_MSH264DecoderMFT, &h264_decoder_create },
    { &CLSID_MSH264EncoderMFT, &h264_encoder_create },
    { &CLSID_GStreamerSchemePlugin, &gstreamer_scheme_handler_create },
};

HRESULT mfplat_get_class_object(REFCLSID rclsid, REFIID riid, void **obj)
{
    struct class_factory *factory;
    unsigned int i;
    HRESULT hr;

    for (i = 0; i < ARRAY_SIZE(class_objects); ++i)
    {
        if (IsEqualGUID(class_objects[i].clsid, rclsid))
        {
            if (!(factory = malloc(sizeof(*factory))))
                return E_OUTOFMEMORY;

            factory->IClassFactory_iface.lpVtbl = &class_factory_vtbl;
            factory->refcount = 1;
            factory->create_instance = class_objects[i].create_instance;

            hr = IClassFactory_QueryInterface(&factory->IClassFactory_iface, riid, obj);
            IClassFactory_Release(&factory->IClassFactory_iface);
            return hr;
        }
    }

    return CLASS_E_CLASSNOTAVAILABLE;
}

HRESULT mfplat_DllRegisterServer(void)
{
    MFT_REGISTER_TYPE_INFO resampler_types[] =
    {
        {MFMediaType_Audio, MFAudioFormat_PCM},
        {MFMediaType_Audio, MFAudioFormat_Float},
    };

    MFT_REGISTER_TYPE_INFO aac_decoder_input_types[] =
    {
        {MFMediaType_Audio, MFAudioFormat_AAC},
        {MFMediaType_Audio, MFAudioFormat_RAW_AAC},
        {MFMediaType_Audio, MFAudioFormat_ADTS},
    };
    MFT_REGISTER_TYPE_INFO aac_decoder_output_types[] =
    {
        {MFMediaType_Audio, MFAudioFormat_Float},
        {MFMediaType_Audio, MFAudioFormat_PCM},
    };

    MFT_REGISTER_TYPE_INFO wma_decoder_input_types[] =
    {
        {MFMediaType_Audio, MEDIASUBTYPE_MSAUDIO1},
        {MFMediaType_Audio, MFAudioFormat_WMAudioV8},
        {MFMediaType_Audio, MFAudioFormat_WMAudioV9},
        {MFMediaType_Audio, MFAudioFormat_WMAudio_Lossless},
    };
    MFT_REGISTER_TYPE_INFO wma_decoder_output_types[] =
    {
        {MFMediaType_Audio, MFAudioFormat_PCM},
        {MFMediaType_Audio, MFAudioFormat_Float},
    };

    MFT_REGISTER_TYPE_INFO h264_decoder_input_types[] =
    {
        {MFMediaType_Video, MFVideoFormat_H264},
        {MFMediaType_Video, MFVideoFormat_H264_ES},
    };
    MFT_REGISTER_TYPE_INFO h264_decoder_output_types[] =
    {
        {MFMediaType_Video, MFVideoFormat_NV12},
        {MFMediaType_Video, MFVideoFormat_YV12},
        {MFMediaType_Video, MFVideoFormat_IYUV},
        {MFMediaType_Video, MFVideoFormat_I420},
        {MFMediaType_Video, MFVideoFormat_YUY2},
    };

    MFT_REGISTER_TYPE_INFO h264_encoder_input_types[] =
    {
        {MFMediaType_Video, MFVideoFormat_IYUV},
        {MFMediaType_Video, MFVideoFormat_YV12},
        {MFMediaType_Video, MFVideoFormat_NV12},
        {MFMediaType_Video, MFVideoFormat_YUY2},
    };
    MFT_REGISTER_TYPE_INFO h264_encoder_output_types[] =
    {
        {MFMediaType_Video, MFVideoFormat_H264},
    };

    MFT_REGISTER_TYPE_INFO video_processor_input_types[] =
    {
        {MFMediaType_Video, MFVideoFormat_IYUV},
        {MFMediaType_Video, MFVideoFormat_YV12},
        {MFMediaType_Video, MFVideoFormat_NV12},
        {MFMediaType_Video, MFVideoFormat_YUY2},
        {MFMediaType_Video, MFVideoFormat_ARGB32},
        {MFMediaType_Video, MFVideoFormat_RGB32},
        {MFMediaType_Video, MFVideoFormat_NV11},
        {MFMediaType_Video, MFVideoFormat_AYUV},
        {MFMediaType_Video, MFVideoFormat_UYVY},
        {MFMediaType_Video, MEDIASUBTYPE_P208},
        {MFMediaType_Video, MFVideoFormat_RGB24},
        {MFMediaType_Video, MFVideoFormat_RGB555},
        {MFMediaType_Video, MFVideoFormat_RGB565},
        {MFMediaType_Video, MFVideoFormat_RGB8},
        {MFMediaType_Video, MFVideoFormat_I420},
        {MFMediaType_Video, MFVideoFormat_Y216},
        {MFMediaType_Video, MFVideoFormat_v410},
        {MFMediaType_Video, MFVideoFormat_Y41P},
        {MFMediaType_Video, MFVideoFormat_Y41T},
        {MFMediaType_Video, MFVideoFormat_Y42T},
        {MFMediaType_Video, MFVideoFormat_YVYU},
        {MFMediaType_Video, MFVideoFormat_420O},
    };
    MFT_REGISTER_TYPE_INFO video_processor_output_types[] =
    {
        {MFMediaType_Video, MFVideoFormat_IYUV},
        {MFMediaType_Video, MFVideoFormat_YV12},
        {MFMediaType_Video, MFVideoFormat_NV12},
        {MFMediaType_Video, MFVideoFormat_YUY2},
        {MFMediaType_Video, MFVideoFormat_ARGB32},
        {MFMediaType_Video, MFVideoFormat_RGB32},
        {MFMediaType_Video, MFVideoFormat_NV11},
        {MFMediaType_Video, MFVideoFormat_AYUV},
        {MFMediaType_Video, MFVideoFormat_UYVY},
        {MFMediaType_Video, MEDIASUBTYPE_P208},
        {MFMediaType_Video, MFVideoFormat_RGB24},
        {MFMediaType_Video, MFVideoFormat_RGB555},
        {MFMediaType_Video, MFVideoFormat_RGB565},
        {MFMediaType_Video, MFVideoFormat_RGB8},
        {MFMediaType_Video, MFVideoFormat_I420},
        {MFMediaType_Video, MFVideoFormat_Y216},
        {MFMediaType_Video, MFVideoFormat_v410},
        {MFMediaType_Video, MFVideoFormat_Y41P},
        {MFMediaType_Video, MFVideoFormat_Y41T},
        {MFMediaType_Video, MFVideoFormat_Y42T},
        {MFMediaType_Video, MFVideoFormat_YVYU},
    };

    MFT_REGISTER_TYPE_INFO wmv_decoder_input_types[] =
    {
        {MFMediaType_Video, MFVideoFormat_WMV1},
        {MFMediaType_Video, MFVideoFormat_WMV2},
        {MFMediaType_Video, MFVideoFormat_WMV3},
        {MFMediaType_Video, MEDIASUBTYPE_WMVP},
        {MFMediaType_Video, MEDIASUBTYPE_WVP2},
        {MFMediaType_Video, MEDIASUBTYPE_WMVR},
        {MFMediaType_Video, MEDIASUBTYPE_WMVA},
        {MFMediaType_Video, MFVideoFormat_WVC1},
        {MFMediaType_Video, MFVideoFormat_VC1S},
    };
    MFT_REGISTER_TYPE_INFO wmv_decoder_output_types[] =
    {
        {MFMediaType_Video, MFVideoFormat_YV12},
        {MFMediaType_Video, MFVideoFormat_YUY2},
        {MFMediaType_Video, MFVideoFormat_UYVY},
        {MFMediaType_Video, MFVideoFormat_YVYU},
        {MFMediaType_Video, MFVideoFormat_NV11},
        {MFMediaType_Video, MFVideoFormat_NV12},
        {MFMediaType_Video, DMOVideoFormat_RGB32},
        {MFMediaType_Video, DMOVideoFormat_RGB24},
        {MFMediaType_Video, DMOVideoFormat_RGB565},
        {MFMediaType_Video, DMOVideoFormat_RGB555},
        {MFMediaType_Video, DMOVideoFormat_RGB8},
    };

    MFT_REGISTER_TYPE_INFO color_convert_input_types[] =
    {
        {MFMediaType_Video, MFVideoFormat_YV12},
        {MFMediaType_Video, MFVideoFormat_YUY2},
        {MFMediaType_Video, MFVideoFormat_UYVY},
        {MFMediaType_Video, MFVideoFormat_AYUV},
        {MFMediaType_Video, MFVideoFormat_NV12},
        {MFMediaType_Video, DMOVideoFormat_RGB32},
        {MFMediaType_Video, DMOVideoFormat_RGB565},
        {MFMediaType_Video, MFVideoFormat_I420},
        {MFMediaType_Video, MFVideoFormat_IYUV},
        {MFMediaType_Video, MFVideoFormat_YVYU},
        {MFMediaType_Video, DMOVideoFormat_RGB24},
        {MFMediaType_Video, DMOVideoFormat_RGB555},
        {MFMediaType_Video, DMOVideoFormat_RGB8},
        {MFMediaType_Video, MEDIASUBTYPE_V216},
        {MFMediaType_Video, MEDIASUBTYPE_V410},
        {MFMediaType_Video, MFVideoFormat_NV11},
        {MFMediaType_Video, MFVideoFormat_Y41P},
        {MFMediaType_Video, MFVideoFormat_Y41T},
        {MFMediaType_Video, MFVideoFormat_Y42T},
        {MFMediaType_Video, MFVideoFormat_YVU9},
    };
    MFT_REGISTER_TYPE_INFO color_convert_output_types[] =
    {
        {MFMediaType_Video, MFVideoFormat_YV12},
        {MFMediaType_Video, MFVideoFormat_YUY2},
        {MFMediaType_Video, MFVideoFormat_UYVY},
        {MFMediaType_Video, MFVideoFormat_AYUV},
        {MFMediaType_Video, MFVideoFormat_NV12},
        {MFMediaType_Video, DMOVideoFormat_RGB32},
        {MFMediaType_Video, DMOVideoFormat_RGB565},
        {MFMediaType_Video, MFVideoFormat_I420},
        {MFMediaType_Video, MFVideoFormat_IYUV},
        {MFMediaType_Video, MFVideoFormat_YVYU},
        {MFMediaType_Video, DMOVideoFormat_RGB24},
        {MFMediaType_Video, DMOVideoFormat_RGB555},
        {MFMediaType_Video, DMOVideoFormat_RGB8},
        {MFMediaType_Video, MEDIASUBTYPE_V216},
        {MFMediaType_Video, MEDIASUBTYPE_V410},
        {MFMediaType_Video, MFVideoFormat_NV11},
    };

    MFT_REGISTER_TYPE_INFO audio_decoder_input_types[] =
    {
        {MFMediaType_Audio, MFAudioFormat_GStreamer},
    };
    MFT_REGISTER_TYPE_INFO audio_decoder_output_types[] =
    {
        {MFMediaType_Audio, MFAudioFormat_Float},
        {MFMediaType_Audio, MFAudioFormat_PCM},
    };

    MFT_REGISTER_TYPE_INFO video_decoder_input_types[] =
    {
        {MFMediaType_Video, MFVideoFormat_GStreamer},
        {MFMediaType_Video, MFVideoFormat_IV50},
    };
    MFT_REGISTER_TYPE_INFO video_decoder_output_types[] =
    {
        {MFMediaType_Video, MFVideoFormat_YV12},
        {MFMediaType_Video, MFVideoFormat_YUY2},
        {MFMediaType_Video, MFVideoFormat_NV11},
        {MFMediaType_Video, MFVideoFormat_NV12},
        {MFMediaType_Video, MFVideoFormat_RGB32},
        {MFMediaType_Video, MFVideoFormat_RGB24},
        {MFMediaType_Video, MFVideoFormat_RGB565},
        {MFMediaType_Video, MFVideoFormat_RGB555},
        {MFMediaType_Video, MFVideoFormat_RGB8},
    };

    struct mft
    {
        GUID clsid;
        GUID category;
        WCHAR name[MAX_PATH];
        UINT32 flags;
        UINT32 input_types_count;
        MFT_REGISTER_TYPE_INFO *input_types;
        UINT32 output_types_count;
        MFT_REGISTER_TYPE_INFO *output_types;
    }
    mfts[] =
    {
        {
            CLSID_MSAACDecMFT,
            MFT_CATEGORY_AUDIO_DECODER,
            L"Microsoft AAC Audio Decoder MFT",
            MFT_ENUM_FLAG_SYNCMFT,
            ARRAY_SIZE(aac_decoder_input_types),
            aac_decoder_input_types,
            ARRAY_SIZE(aac_decoder_output_types),
            aac_decoder_output_types,
        },
        {
            CLSID_WMADecMediaObject,
            MFT_CATEGORY_AUDIO_DECODER,
            L"WMAudio Decoder MFT",
            MFT_ENUM_FLAG_SYNCMFT,
            ARRAY_SIZE(wma_decoder_input_types),
            wma_decoder_input_types,
            ARRAY_SIZE(wma_decoder_output_types),
            wma_decoder_output_types,
        },
        {
            CLSID_MSH264DecoderMFT,
            MFT_CATEGORY_VIDEO_DECODER,
            L"Microsoft H264 Video Decoder MFT",
            MFT_ENUM_FLAG_SYNCMFT,
            ARRAY_SIZE(h264_decoder_input_types),
            h264_decoder_input_types,
            ARRAY_SIZE(h264_decoder_output_types),
            h264_decoder_output_types,
        },
        {
            CLSID_MSH264EncoderMFT,
            MFT_CATEGORY_VIDEO_ENCODER,
            L"H264 Encoder MFT",
            MFT_ENUM_FLAG_SYNCMFT,
            ARRAY_SIZE(h264_encoder_input_types),
            h264_encoder_input_types,
            ARRAY_SIZE(h264_encoder_output_types),
            h264_encoder_output_types,
        },
        {
            CLSID_WMVDecoderMFT,
            MFT_CATEGORY_VIDEO_DECODER,
            L"WMVideo Decoder MFT",
            MFT_ENUM_FLAG_SYNCMFT,
            ARRAY_SIZE(wmv_decoder_input_types),
            wmv_decoder_input_types,
            ARRAY_SIZE(wmv_decoder_output_types),
            wmv_decoder_output_types,
        },
        {
            CLSID_VideoProcessorMFT,
            MFT_CATEGORY_VIDEO_PROCESSOR,
            L"Microsoft Video Processor MFT",
            MFT_ENUM_FLAG_SYNCMFT,
            ARRAY_SIZE(video_processor_input_types),
            video_processor_input_types,
            ARRAY_SIZE(video_processor_output_types),
            video_processor_output_types,
        },
        {
            CLSID_CResamplerMediaObject,
            MFT_CATEGORY_AUDIO_EFFECT,
            L"Resampler MFT",
            MFT_ENUM_FLAG_SYNCMFT,
            ARRAY_SIZE(resampler_types),
            resampler_types,
            ARRAY_SIZE(resampler_types),
            resampler_types,
        },
        {
            CLSID_CFrameRateConvertDmo,
            MFT_CATEGORY_VIDEO_EFFECT,
            L"Frame Rate Converter",
            MFT_ENUM_FLAG_SYNCMFT,
            /* FIXME: check the actual media types */
            ARRAY_SIZE(color_convert_input_types),
            color_convert_input_types,
            ARRAY_SIZE(color_convert_output_types),
            color_convert_output_types,
        },
        {
            CLSID_CResizerDMO,
            MFT_CATEGORY_VIDEO_EFFECT,
            L"Resizer MFT",
            MFT_ENUM_FLAG_SYNCMFT,
            /* FIXME: check the actual media types */
            ARRAY_SIZE(color_convert_input_types),
            color_convert_input_types,
            ARRAY_SIZE(color_convert_output_types),
            color_convert_output_types,
        },
        {
            CLSID_CColorControlDmo,
            MFT_CATEGORY_VIDEO_EFFECT,
            L"Color Control",
            MFT_ENUM_FLAG_SYNCMFT,
            /* FIXME: check the actual media types */
            ARRAY_SIZE(color_convert_input_types),
            color_convert_input_types,
            ARRAY_SIZE(color_convert_output_types),
            color_convert_output_types,
        },
        {
            CLSID_CColorConvertDMO,
            MFT_CATEGORY_VIDEO_EFFECT,
            L"Color Converter MFT",
            MFT_ENUM_FLAG_SYNCMFT,
            ARRAY_SIZE(color_convert_input_types),
            color_convert_input_types,
            ARRAY_SIZE(color_convert_output_types),
            color_convert_output_types,
        },
        {
            CLSID_GStreamerAudioDecoder,
            MFT_CATEGORY_AUDIO_DECODER,
            L"Wine Audio Decoder MFT",
            MFT_ENUM_FLAG_SYNCMFT,
            ARRAY_SIZE(audio_decoder_input_types),
            audio_decoder_input_types,
            ARRAY_SIZE(audio_decoder_output_types),
            audio_decoder_output_types,
        },
        {
            CLSID_GStreamerVideoDecoder,
            MFT_CATEGORY_VIDEO_DECODER,
            L"Wine Video Decoder MFT",
            MFT_ENUM_FLAG_SYNCMFT,
            ARRAY_SIZE(video_decoder_input_types),
            video_decoder_input_types,
            ARRAY_SIZE(video_decoder_output_types),
            video_decoder_output_types,
        },
    };

    unsigned int i;
    HRESULT hr;

    for (i = 0; i < ARRAY_SIZE(mfts); i++)
    {
        hr = MFTRegister(mfts[i].clsid, mfts[i].category, mfts[i].name, mfts[i].flags, mfts[i].input_types_count,
                    mfts[i].input_types, mfts[i].output_types_count, mfts[i].output_types, NULL);

        if (FAILED(hr))
        {
            FIXME("Failed to register MFT, hr %#lx.\n", hr);
            return hr;
        }
    }

    return S_OK;
}
