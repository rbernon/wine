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

#include <stddef.h>
#include <stdarg.h>

#define COBJMACROS
#include "windef.h"
#include "winbase.h"

#include "mfapi.h"
#include "mferror.h"
#include "mfidl.h"
#include "mfobjects.h"
#include "mftransform.h"
#include "rpcproxy.h"
#include "wmcodecdsp.h"

#include "wine/debug.h"
#include "wine/winedmo.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmo);

struct video_encoder
{
    IMFTransform IMFTransform_iface;
    LONG refcount;

    const GUID *const *input_types;
    UINT input_type_count;
    const GUID *const *output_types;
    UINT output_type_count;

    IMFMediaType *input_type;
    MFT_INPUT_STREAM_INFO input_info;
    IMFMediaType *output_type;
    MFT_OUTPUT_STREAM_INFO output_info;

    IMFAttributes *attributes;

    struct winedmo_transform winedmo_transform;
};

static inline struct video_encoder *impl_from_IMFTransform(IMFTransform *iface)
{
    return CONTAINING_RECORD(iface, struct video_encoder, IMFTransform_iface);
}

static HRESULT video_encoder_create_input_type(struct video_encoder *encoder,
        const GUID *subtype, IMFMediaType **out)
{
    IMFVideoMediaType *input_type;
    UINT64 ratio;
    UINT32 value;
    HRESULT hr;

    if (FAILED(hr = MFCreateVideoMediaTypeFromSubtype(subtype, &input_type)))
        return hr;

    if (FAILED(hr = IMFMediaType_GetUINT64(encoder->output_type, &MF_MT_FRAME_SIZE, &ratio))
            || FAILED(hr = IMFVideoMediaType_SetUINT64(input_type, &MF_MT_FRAME_SIZE, ratio)))
        goto done;

    if (FAILED(hr = IMFMediaType_GetUINT64(encoder->output_type, &MF_MT_FRAME_RATE, &ratio))
            || FAILED(hr = IMFVideoMediaType_SetUINT64(input_type, &MF_MT_FRAME_RATE, ratio)))
        goto done;

    if (FAILED(hr = IMFMediaType_GetUINT32(encoder->output_type, &MF_MT_INTERLACE_MODE, &value))
            || FAILED(hr = IMFVideoMediaType_SetUINT32(input_type, &MF_MT_INTERLACE_MODE, value)))
        goto done;

    if (FAILED(IMFMediaType_GetUINT32(encoder->output_type, &MF_MT_VIDEO_NOMINAL_RANGE, &value)))
        value = MFNominalRange_Wide;
    if (FAILED(hr = IMFVideoMediaType_SetUINT32(input_type, &MF_MT_VIDEO_NOMINAL_RANGE, value)))
        goto done;

    if (FAILED(IMFMediaType_GetUINT64(encoder->output_type, &MF_MT_PIXEL_ASPECT_RATIO, &ratio)))
        ratio = (UINT64)1 << 32 | 1;
    if (FAILED(hr = IMFVideoMediaType_SetUINT64(input_type, &MF_MT_PIXEL_ASPECT_RATIO, ratio)))
        goto done;

    IMFMediaType_AddRef((*out = (IMFMediaType *)input_type));

done:
    IMFVideoMediaType_Release(input_type);
    return hr;
}

static HRESULT video_encoder_try_create_transform(struct video_encoder *encoder)
{
    union winedmo_format *input_format, *output_format;
    UINT format_size;
    NTSTATUS status;
    HRESULT hr;

    winedmo_transform_destroy(&encoder->winedmo_transform);

    if (FAILED(hr = MFCreateMFVideoFormatFromMFMediaType(encoder->input_type, (MFVIDEOFORMAT **)&input_format, &format_size)))
        return hr;
    if (FAILED(hr = MFCreateMFVideoFormatFromMFMediaType(encoder->output_type, (MFVIDEOFORMAT **)&output_format, &format_size)))
    {
        CoTaskMemFree(input_format);
        return hr;
    }

    if ((status = winedmo_transform_create(MFMediaType_Video, input_format, output_format, &encoder->winedmo_transform)))
    {
        ERR("FAILED %#lx\n", status);
        hr = HRESULT_FROM_NT(status);
    }
    CoTaskMemFree(output_format);
    CoTaskMemFree(input_format);
    return hr;
}

static HRESULT WINAPI transform_QueryInterface(IMFTransform *iface, REFIID iid, void **out)
{
    struct video_encoder *encoder = impl_from_IMFTransform(iface);

    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IMFTransform) || IsEqualGUID(iid, &IID_IUnknown))
        *out = &encoder->IMFTransform_iface;
    else
    {
        *out = NULL;
        WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static ULONG WINAPI transform_AddRef(IMFTransform *iface)
{
    struct video_encoder *encoder = impl_from_IMFTransform(iface);
    ULONG refcount = InterlockedIncrement(&encoder->refcount);

    TRACE("iface %p increasing refcount to %lu.\n", encoder, refcount);

    return refcount;
}

static ULONG WINAPI transform_Release(IMFTransform *iface)
{
    struct video_encoder *encoder = impl_from_IMFTransform(iface);
    ULONG refcount = InterlockedDecrement(&encoder->refcount);

    TRACE("iface %p decreasing refcount to %lu.\n", encoder, refcount);

    if (!refcount)
    {
        if (encoder->input_type)
            IMFMediaType_Release(encoder->input_type);
        if (encoder->output_type)
            IMFMediaType_Release(encoder->output_type);
        IMFAttributes_Release(encoder->attributes);
        winedmo_transform_destroy(&encoder->winedmo_transform);
        free(encoder);
    }

    return refcount;
}

static HRESULT WINAPI transform_GetStreamLimits(IMFTransform *iface, DWORD *input_minimum,
        DWORD *input_maximum, DWORD *output_minimum, DWORD *output_maximum)
{
    TRACE("iface %p, input_minimum %p, input_maximum %p, output_minimum %p, output_maximum %p.\n",
            iface, input_minimum, input_maximum, output_minimum, output_maximum);
    *input_minimum = *input_maximum = *output_minimum = *output_maximum = 1;
    return S_OK;
}

static HRESULT WINAPI transform_GetStreamCount(IMFTransform *iface, DWORD *inputs, DWORD *outputs)
{
    TRACE("iface %p, inputs %p, outputs %p.\n", iface, inputs, outputs);
    *inputs = *outputs = 1;
    return S_OK;
}

static HRESULT WINAPI transform_GetStreamIDs(IMFTransform *iface, DWORD input_size, DWORD *inputs,
        DWORD output_size, DWORD *outputs)
{
    FIXME("iface %p, input_size %lu, inputs %p, output_size %lu, outputs %p.\n", iface,
            input_size, inputs, output_size, outputs);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_GetInputStreamInfo(IMFTransform *iface, DWORD id, MFT_INPUT_STREAM_INFO *info)
{
    struct video_encoder *encoder = impl_from_IMFTransform(iface);

    TRACE("iface %p, id %#lx, info %p.\n", iface, id, info);

    *info = encoder->input_info;
    return S_OK;
}

static HRESULT WINAPI transform_GetOutputStreamInfo(IMFTransform *iface, DWORD id, MFT_OUTPUT_STREAM_INFO *info)
{
    struct video_encoder *encoder = impl_from_IMFTransform(iface);

    TRACE("iface %p, id %#lx, info %p.\n", iface, id, info);

    *info = encoder->output_info;
    return S_OK;
}

static HRESULT WINAPI transform_GetAttributes(IMFTransform *iface, IMFAttributes **attributes)
{
    struct video_encoder *encoder = impl_from_IMFTransform(iface);

    TRACE("iface %p, attributes %p.\n", iface, attributes);

    if (!attributes)
        return E_POINTER;

    IMFAttributes_AddRef((*attributes = encoder->attributes));
    return S_OK;
}

static HRESULT WINAPI transform_GetInputStreamAttributes(IMFTransform *iface, DWORD id, IMFAttributes **attributes)
{
    FIXME("iface %p, id %#lx, attributes %p.\n", iface, id, attributes);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_GetOutputStreamAttributes(IMFTransform *iface, DWORD id, IMFAttributes **attributes)
{
    FIXME("iface %p, id %#lx, attributes %p.\n", iface, id, attributes);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_DeleteInputStream(IMFTransform *iface, DWORD id)
{
    FIXME("iface %p, id %#lx.\n", iface, id);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_AddInputStreams(IMFTransform *iface, DWORD streams, DWORD *ids)
{
    FIXME("iface %p, streams %lu, ids %p.\n", iface, streams, ids);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_GetInputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    struct video_encoder *encoder = impl_from_IMFTransform(iface);

    TRACE("iface %p, id %#lx, index %#lx, type %p.\n", iface, id, index, type);

    *type = NULL;

    if (!encoder->output_type)
        return MF_E_TRANSFORM_TYPE_NOT_SET;
    if (index >= encoder->input_type_count)
        return MF_E_NO_MORE_TYPES;

    return video_encoder_create_input_type(encoder, encoder->input_types[index], type);
}

static HRESULT WINAPI transform_GetOutputAvailableType(IMFTransform *iface, DWORD id,
        DWORD index, IMFMediaType **type)
{
    struct video_encoder *encoder = impl_from_IMFTransform(iface);

    TRACE("iface %p, id %#lx, index %#lx, type %p.\n", iface, id, index, type);

    *type = NULL;
    if (index >= encoder->output_type_count)
        return MF_E_NO_MORE_TYPES;
    return MFCreateVideoMediaTypeFromSubtype(encoder->output_types[index], (IMFVideoMediaType **)type);
}

static HRESULT WINAPI transform_SetInputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    struct video_encoder *encoder = impl_from_IMFTransform(iface);
    IMFMediaType *good_input_type;
    GUID major, subtype;
    UINT64 ratio;
    BOOL result;
    HRESULT hr;
    ULONG i;

    TRACE("iface %p, id %#lx, type %p, flags %#lx.\n", iface, id, type, flags);

    if (!type)
    {
        if (encoder->input_type)
        {
            IMFMediaType_Release(encoder->input_type);
            encoder->input_type = NULL;
        }
        winedmo_transform_destroy(&encoder->winedmo_transform);
        return S_OK;
    }

    if (!encoder->output_type)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (FAILED(IMFMediaType_GetGUID(type, &MF_MT_MAJOR_TYPE, &major)) ||
            FAILED(IMFMediaType_GetGUID(type, &MF_MT_SUBTYPE, &subtype)))
        return E_INVALIDARG;

    if (!IsEqualGUID(&major, &MFMediaType_Video))
        return MF_E_INVALIDMEDIATYPE;

    for (i = 0; i < encoder->input_type_count; ++i)
        if (IsEqualGUID(&subtype, encoder->input_types[i]))
            break;
    if (i == encoder->input_type_count)
        return MF_E_INVALIDMEDIATYPE;

    if (FAILED(IMFMediaType_GetUINT64(type, &MF_MT_FRAME_SIZE, &ratio))
            || FAILED(IMFMediaType_GetUINT64(type, &MF_MT_FRAME_RATE, &ratio)))
        return MF_E_INVALIDMEDIATYPE;

    if (FAILED(hr = video_encoder_create_input_type(encoder, &subtype, &good_input_type)))
        return hr;
    hr = IMFMediaType_Compare(good_input_type, (IMFAttributes *)type,
            MF_ATTRIBUTES_MATCH_INTERSECTION, &result);
    IMFMediaType_Release(good_input_type);
    if (FAILED(hr) || !result)
        return MF_E_INVALIDMEDIATYPE;

    if (flags & MFT_SET_TYPE_TEST_ONLY)
        return S_OK;

    if (encoder->input_type)
        IMFMediaType_Release(encoder->input_type);
    IMFMediaType_AddRef((encoder->input_type = type));

    if (FAILED(hr = video_encoder_try_create_transform(encoder)))
    {
        IMFMediaType_Release(encoder->input_type);
        encoder->input_type = NULL;
    }

    return hr;
}

static HRESULT WINAPI transform_SetOutputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    struct video_encoder *encoder = impl_from_IMFTransform(iface);
    UINT32 uint32_value;
    UINT64 uint64_value;
    GUID major, subtype;
    ULONG i;

    TRACE("iface %p, id %#lx, type %p, flags %#lx.\n", iface, id, type, flags);

    if (!type)
    {
        if (encoder->input_type)
        {
            IMFMediaType_Release(encoder->input_type);
            encoder->input_type = NULL;
        }
        if (encoder->output_type)
        {
            IMFMediaType_Release(encoder->output_type);
            encoder->output_type = NULL;
            memset(&encoder->output_info, 0, sizeof(encoder->output_info));
        }
        winedmo_transform_destroy(&encoder->winedmo_transform);

        return S_OK;
    }

    if (FAILED(IMFMediaType_GetGUID(type, &MF_MT_MAJOR_TYPE, &major))
            || FAILED(IMFMediaType_GetGUID(type, &MF_MT_SUBTYPE, &subtype)))
        return E_INVALIDARG;

    if (!IsEqualGUID(&major, &MFMediaType_Video))
        return MF_E_INVALIDMEDIATYPE;

    for (i = 0; i < encoder->output_type_count; ++i)
        if (IsEqualGUID(&subtype, encoder->output_types[i]))
            break;
    if (i == encoder->output_type_count)
        return MF_E_INVALIDMEDIATYPE;

    if (FAILED(IMFMediaType_GetUINT64(type, &MF_MT_FRAME_SIZE, &uint64_value)))
        return MF_E_INVALIDMEDIATYPE;

    if (flags & MFT_SET_TYPE_TEST_ONLY)
        return S_OK;

    if (FAILED(IMFMediaType_GetUINT64(type, &MF_MT_FRAME_RATE, &uint64_value))
            || FAILED(IMFMediaType_GetUINT32(type, &MF_MT_AVG_BITRATE, &uint32_value))
            || FAILED(IMFMediaType_GetUINT32(type, &MF_MT_INTERLACE_MODE, &uint32_value)))
        return MF_E_INVALIDMEDIATYPE;

    if (encoder->input_type)
    {
        IMFMediaType_Release(encoder->input_type);
        encoder->input_type = NULL;
    }

    if (encoder->output_type)
        IMFMediaType_Release(encoder->output_type);
    IMFMediaType_AddRef((encoder->output_type = type));

    /* FIXME: Add MF_MT_MPEG_SEQUENCE_HEADER attribute. */

    /* FIXME: Hardcode a size that native uses for 1920 * 1080.
     * And hope it's large enough to make things work for now.
     * The right way is to calculate it based on frame width and height. */
    encoder->output_info.cbSize = 0x3bc400;

    winedmo_transform_destroy(&encoder->winedmo_transform);

    return S_OK;
}

static HRESULT WINAPI transform_GetInputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **type)
{
    struct video_encoder *encoder = impl_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, id %#lx, type %p\n", iface, id, type);

    if (!encoder->input_type)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (FAILED(hr = MFCreateMediaType(type)))
        return hr;

    return IMFMediaType_CopyAllItems(encoder->input_type, (IMFAttributes *)*type);

}

static HRESULT WINAPI transform_GetOutputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **type)
{
    struct video_encoder *encoder = impl_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, id %#lx, type %p\n", iface, id, type);

    if (!encoder->output_type)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (FAILED(hr = MFCreateMediaType(type)))
        return hr;

    return IMFMediaType_CopyAllItems(encoder->output_type, (IMFAttributes *)*type);
}

static HRESULT WINAPI transform_GetInputStatus(IMFTransform *iface, DWORD id, DWORD *flags)
{
    FIXME("iface %p, id %#lx, flags %p.\n", iface, id, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_GetOutputStatus(IMFTransform *iface, DWORD *flags)
{
    FIXME("iface %p, flags %p stub!\n", iface, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_SetOutputBounds(IMFTransform *iface, LONGLONG lower, LONGLONG upper)
{
    FIXME("iface %p, lower %I64d, upper %I64d.\n", iface, lower, upper);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_ProcessEvent(IMFTransform *iface, DWORD id, IMFMediaEvent *event)
{
    FIXME("iface %p, id %#lx, event %p stub!\n", iface, id, event);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_ProcessMessage(IMFTransform *iface, MFT_MESSAGE_TYPE message, ULONG_PTR param)
{
    struct video_encoder *encoder = impl_from_IMFTransform(iface);

    TRACE("iface %p, message %#x, param %Ix.\n", iface, message, param);

    switch (message)
    {
        case MFT_MESSAGE_COMMAND_DRAIN:
            if (winedmo_transform_drain(encoder->winedmo_transform, FALSE)) return E_FAIL;
            return S_OK;

        case MFT_MESSAGE_COMMAND_FLUSH:
            if (winedmo_transform_drain(encoder->winedmo_transform, TRUE)) return E_FAIL;
            return S_OK;

        default:
            FIXME("Ignoring message %#x.\n", message);
            return S_OK;
    }
}

static HRESULT WINAPI transform_ProcessInput(IMFTransform *iface, DWORD id, IMFSample *sample, DWORD flags)
{
    struct video_encoder *encoder = impl_from_IMFTransform(iface);
    DMO_OUTPUT_DATA_BUFFER input = {0};
    IMFMediaBuffer *buffer;
    NTSTATUS status;
    HRESULT hr;

    TRACE("iface %p, id %#lx, sample %p, flags %#lx.\n", iface, id, sample, flags);

    if (!encoder->winedmo_transform.handle)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (FAILED(hr = IMFSample_ConvertToContiguousBuffer(sample, &buffer)))
        return hr;
    if (SUCCEEDED(hr = MFCreateLegacyMediaBufferOnMFMediaBuffer(sample, buffer, 0, &input.pBuffer)))
    {
        if ((status = winedmo_transform_process_input(encoder->winedmo_transform, &input)))
        {
            if (status == STATUS_PENDING) hr = MF_E_NOTACCEPTING;
            else hr = HRESULT_FROM_NT(status);
        }
        IMediaBuffer_Release(input.pBuffer);
    }
    IMFMediaBuffer_Release(buffer);

    return hr;
}

static HRESULT WINAPI transform_ProcessOutput(IMFTransform *iface, DWORD flags, DWORD count,
        MFT_OUTPUT_DATA_BUFFER *samples, DWORD *status)
{
    FIXME("iface %p, flags %#lx, count %lu, samples %p, status %p.\n", iface, flags, count, samples, status);
    return E_NOTIMPL;
}

static const IMFTransformVtbl transform_vtbl =
{
    transform_QueryInterface,
    transform_AddRef,
    transform_Release,
    transform_GetStreamLimits,
    transform_GetStreamCount,
    transform_GetStreamIDs,
    transform_GetInputStreamInfo,
    transform_GetOutputStreamInfo,
    transform_GetAttributes,
    transform_GetInputStreamAttributes,
    transform_GetOutputStreamAttributes,
    transform_DeleteInputStream,
    transform_AddInputStreams,
    transform_GetInputAvailableType,
    transform_GetOutputAvailableType,
    transform_SetInputType,
    transform_SetOutputType,
    transform_GetInputCurrentType,
    transform_GetOutputCurrentType,
    transform_GetInputStatus,
    transform_GetOutputStatus,
    transform_SetOutputBounds,
    transform_ProcessEvent,
    transform_ProcessMessage,
    transform_ProcessInput,
    transform_ProcessOutput,
};

static HRESULT video_encoder_create(const GUID *const *input_types, UINT input_type_count,
        const GUID *const *output_types, UINT output_type_count, struct video_encoder **out)
{
    struct video_encoder *encoder;
    HRESULT hr;

    if (!(encoder = calloc(1, sizeof(*encoder))))
        return E_OUTOFMEMORY;

    encoder->IMFTransform_iface.lpVtbl = &transform_vtbl;
    encoder->refcount = 1;

    encoder->input_types = input_types;
    encoder->input_type_count = input_type_count;
    encoder->output_types = output_types;
    encoder->output_type_count = output_type_count;

    if (FAILED(hr = MFCreateAttributes(&encoder->attributes, 16)))
        goto failed;
    if (FAILED(hr = IMFAttributes_SetUINT32(encoder->attributes, &MFT_ENCODER_SUPPORTS_CONFIG_EVENT, TRUE)))
        goto failed;

    *out = encoder;
    TRACE("Created video encoder %p\n", encoder);
    return S_OK;

failed:
    if (encoder->attributes)
        IMFAttributes_Release(encoder->attributes);
    free(encoder);
    return hr;
}

static const GUID *const h264_encoder_input_types[] =
{
    &MFVideoFormat_IYUV,
    &MFVideoFormat_YV12,
    &MFVideoFormat_NV12,
    &MFVideoFormat_YUY2,
};

static const GUID *const h264_encoder_output_types[] =
{
    &MFVideoFormat_H264,
};

static HRESULT WINAPI h264_encoder_factory_CreateInstance(IClassFactory *iface, IUnknown *outer,
        REFIID riid, void **out)
{
    struct video_encoder *encoder;
    NTSTATUS status;
    HRESULT hr;

    TRACE("%p, %s, %p.\n", outer, debugstr_guid(riid), out);

    if ((status = winedmo_transform_check(MFMediaType_Video, MFVideoFormat_NV12, MFVideoFormat_H264)))
    {
        WARN("Unsupported winedmo transform, status %#lx.\n", status);
        return E_NOTIMPL;
    }

    *out = NULL;
    if (outer)
        return CLASS_E_NOAGGREGATION;
    if (FAILED(hr = video_encoder_create(h264_encoder_input_types, ARRAY_SIZE(h264_encoder_input_types),
            h264_encoder_output_types, ARRAY_SIZE(h264_encoder_output_types), &encoder)))
        return hr;
    *out = (IUnknown *)&encoder->IMFTransform_iface;
    TRACE("Created h264 encoder transform %p.\n", &encoder->IMFTransform_iface);

    hr = IUnknown_QueryInterface(&encoder->IMFTransform_iface, riid, out);
    IUnknown_Release(&encoder->IMFTransform_iface);
    return hr;
}

static HRESULT WINAPI class_factory_QueryInterface(IClassFactory *iface, REFIID riid, void **out)
{
    *out = IsEqualGUID(riid, &IID_IClassFactory) || IsEqualGUID(riid, &IID_IUnknown) ? iface : NULL;
    return *out ? S_OK : E_NOINTERFACE;
}
static ULONG WINAPI class_factory_AddRef(IClassFactory *iface)
{
    return 2;
}
static ULONG WINAPI class_factory_Release(IClassFactory *iface)
{
    return 1;
}
static HRESULT WINAPI class_factory_LockServer(IClassFactory *iface, BOOL dolock)
{
    return S_OK;
}

static const IClassFactoryVtbl h264_encoder_factory_vtbl =
{
    class_factory_QueryInterface,
    class_factory_AddRef,
    class_factory_Release,
    h264_encoder_factory_CreateInstance,
    class_factory_LockServer,
};

static IClassFactory h264_encoder_factory = {&h264_encoder_factory_vtbl};

/***********************************************************************
 *              DllGetClassObject (mfh264enc.@)
 */
HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void **out)
{
    if (IsEqualGUID(clsid, &CLSID_MSH264EncoderMFT))
        return IClassFactory_QueryInterface(&h264_encoder_factory, riid, out);

    *out = NULL;
    FIXME("Unknown clsid %s.\n", debugstr_guid(clsid));
    return CLASS_E_CLASSNOTAVAILABLE;
}

/***********************************************************************
 *              DllRegisterServer (mfh264enc.@)
 */
HRESULT WINAPI DllRegisterServer(void)
{
    MFT_REGISTER_TYPE_INFO h264_encoder_mft_inputs[] =
    {
        {MFMediaType_Video, MFVideoFormat_IYUV},
        {MFMediaType_Video, MFVideoFormat_YV12},
        {MFMediaType_Video, MFVideoFormat_NV12},
        {MFMediaType_Video, MFVideoFormat_YUY2},
    };
    MFT_REGISTER_TYPE_INFO h264_encoder_mft_outputs[] =
    {
        {MFMediaType_Video, MFVideoFormat_H264},
    };
    HRESULT hr;

    TRACE("\n");

    if (FAILED(hr = __wine_register_resources()))
        return hr;
    if (FAILED(hr = MFTRegister(CLSID_MSH264EncoderMFT, MFT_CATEGORY_VIDEO_ENCODER,
            (WCHAR *)L"H264 Encoder MFT", MFT_ENUM_FLAG_SYNCMFT,
            ARRAY_SIZE(h264_encoder_mft_inputs), h264_encoder_mft_inputs,
            ARRAY_SIZE(h264_encoder_mft_outputs), h264_encoder_mft_outputs, NULL)))
        return hr;

    return S_OK;
}

/***********************************************************************
 *              DllUnregisterServer (mfh264enc.@)
 */
HRESULT WINAPI DllUnregisterServer(void)
{
    HRESULT hr;

    TRACE("\n");

    if (FAILED(hr = __wine_unregister_resources()))
        return hr;
    if (FAILED(hr = MFTUnregister(CLSID_MSH264EncoderMFT)))
        return hr;

    return S_OK;
}
