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

#include "d3d9.h"
#include "dmoreg.h"
#include "dshow.h"
#include "dmort.h"
#include "mediaerr.h"
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

#include "initguid.h"

DEFINE_MEDIATYPE_GUID(MFVideoFormat_ABGR32, D3DFMT_A8B8G8R8);
DEFINE_GUID(DMOVideoFormat_RGB32,D3DFMT_X8R8G8B8,0x524f,0x11ce,0x9f,0x53,0x00,0x20,0xaf,0x0b,0xa7,0x70);
DEFINE_GUID(DMOVideoFormat_RGB24,D3DFMT_R8G8B8,0x524f,0x11ce,0x9f,0x53,0x00,0x20,0xaf,0x0b,0xa7,0x70);
DEFINE_GUID(DMOVideoFormat_RGB565,D3DFMT_R5G6B5,0x524f,0x11ce,0x9f,0x53,0x00,0x20,0xaf,0x0b,0xa7,0x70);
DEFINE_GUID(DMOVideoFormat_RGB555,D3DFMT_X1R5G5B5,0x524f,0x11ce,0x9f,0x53,0x00,0x20,0xaf,0x0b,0xa7,0x70);
DEFINE_GUID(DMOVideoFormat_RGB8,D3DFMT_P8,0x524f,0x11ce,0x9f,0x53,0x00,0x20,0xaf,0x0b,0xa7,0x70);

static HRESULT MF_RESULT_FROM_DMO(HRESULT hr)
{
    switch (hr)
    {
    case DMO_E_INVALIDSTREAMINDEX: return MF_E_INVALIDSTREAMNUMBER;
    case DMO_E_INVALIDTYPE: return MF_E_INVALIDMEDIATYPE;
    case DMO_E_TYPE_NOT_SET: return MF_E_TRANSFORM_TYPE_NOT_SET;
    case DMO_E_NOTACCEPTING: return MF_E_NOTACCEPTING;
    case DMO_E_TYPE_NOT_ACCEPTED: return MF_E_INVALIDMEDIATYPE;
    case DMO_E_NO_MORE_ITEMS: return MF_E_NO_MORE_TYPES;
    case S_FALSE: return MF_E_TRANSFORM_NEED_MORE_INPUT;
    }

    return hr;
}

static const GUID *const color_converter_inputs[] =
{
    &MEDIASUBTYPE_YV12,
    &MEDIASUBTYPE_YUY2,
    &MEDIASUBTYPE_UYVY,
    &MEDIASUBTYPE_AYUV,
    &MEDIASUBTYPE_NV12,
    &MFVideoFormat_RGB32,
    &MFVideoFormat_RGB565,
    &MEDIASUBTYPE_I420,
    &MEDIASUBTYPE_IYUV,
    &MEDIASUBTYPE_YVYU,
    &MFVideoFormat_RGB24,
    &MFVideoFormat_RGB555,
    &MEDIASUBTYPE_RGB8,
    &MEDIASUBTYPE_V216,
    &MEDIASUBTYPE_V410,
    &MEDIASUBTYPE_NV11,
    &MEDIASUBTYPE_Y41P,
    &MEDIASUBTYPE_Y41T,
    &MEDIASUBTYPE_Y42T,
    &MEDIASUBTYPE_YVU9,
};
static const GUID *const color_converter_outputs[] =
{
    &MEDIASUBTYPE_YV12,
    &MEDIASUBTYPE_YUY2,
    &MEDIASUBTYPE_UYVY,
    &MEDIASUBTYPE_AYUV,
    &MEDIASUBTYPE_NV12,
    &MFVideoFormat_RGB32,
    &MFVideoFormat_RGB565,
    &MEDIASUBTYPE_I420,
    &MEDIASUBTYPE_IYUV,
    &MEDIASUBTYPE_YVYU,
    &MFVideoFormat_RGB24,
    &MFVideoFormat_RGB555,
    &MEDIASUBTYPE_RGB8,
    &MEDIASUBTYPE_V216,
    &MEDIASUBTYPE_V410,
    &MEDIASUBTYPE_NV11,
};

static const GUID *get_mf_subtype(const GUID *subtype)
{
    if (IsEqualGUID(subtype, &MEDIASUBTYPE_RGB8))
        return &MFVideoFormat_RGB8;
    if (IsEqualGUID(subtype, &MEDIASUBTYPE_RGB555))
        return &MFVideoFormat_RGB555;
    if (IsEqualGUID(subtype, &MEDIASUBTYPE_RGB565))
        return &MFVideoFormat_RGB565;
    if (IsEqualGUID(subtype, &MEDIASUBTYPE_RGB24))
        return &MFVideoFormat_RGB24;
    if (IsEqualGUID(subtype, &MEDIASUBTYPE_RGB32))
        return &MFVideoFormat_RGB32;
    return subtype;
}

static const GUID *get_dmo_subtype(const GUID *subtype)
{
    if (IsEqualGUID(subtype, &MFVideoFormat_RGB8))
        return &MEDIASUBTYPE_RGB8;
    if (IsEqualGUID(subtype, &MFVideoFormat_RGB555))
        return &MEDIASUBTYPE_RGB555;
    if (IsEqualGUID(subtype, &MFVideoFormat_RGB565))
        return &MEDIASUBTYPE_RGB565;
    if (IsEqualGUID(subtype, &MFVideoFormat_RGB24))
        return &MEDIASUBTYPE_RGB24;
    if (IsEqualGUID(subtype, &MFVideoFormat_RGB32))
        return &MEDIASUBTYPE_RGB32;
    return subtype;
}

struct color_converter
{
    IUnknown IUnknown_iface;
    IMFTransform IMFTransform_iface;
    IMediaObject IMediaObject_iface;
    IPropertyBag IPropertyBag_iface;
    IPropertyStore IPropertyStore_iface;
    IUnknown *outer;
    LONG refcount;

    DMO_MEDIA_TYPE input_type;
    DMO_MEDIA_TYPE output_type;

    MFT_INPUT_STREAM_INFO input_info;
    MFT_OUTPUT_STREAM_INFO output_info;

    IUnknown *input_sample;
    struct winedmo_transform winedmo_transform;
};

static BOOL get_default_stride_sign(VIDEOINFOHEADER *vih)
{
    return vih->bmiHeader.biCompression == BI_RGB || vih->bmiHeader.biCompression == BI_BITFIELDS ? -1 : 1;
}

static void init_mf_video_format(MFVIDEOFORMAT *format, DMO_MEDIA_TYPE *mt)
{
    VIDEOINFOHEADER *vih = (VIDEOINFOHEADER *)mt->pbFormat;

    memset(format, 0, sizeof(*format));
    format->dwSize = sizeof(*format);
    format->guidFormat = *get_mf_subtype(&mt->subtype);
    format->videoInfo.dwWidth = vih->bmiHeader.biWidth;
    format->videoInfo.dwHeight = abs(vih->bmiHeader.biHeight);
    format->videoInfo.VideoFlags = get_default_stride_sign(vih) < 0
            && vih->bmiHeader.biHeight > 0 ? MFVideoFlag_BottomUpLinearRep : 0;
}

static HRESULT color_converter_create_transform(struct color_converter *converter)
{
    union winedmo_format input_format, output_format;
    NTSTATUS status;

    winedmo_transform_destroy(&converter->winedmo_transform);

    init_mf_video_format(&input_format.video, &converter->input_type);
    init_mf_video_format(&output_format.video, &converter->output_type);

    if ((status = winedmo_transform_create(MFMediaType_Video, &input_format,
            &output_format, &converter->winedmo_transform)))
    {
        WARN("Failed to create converter transform, status %#lx\n", status);
        return HRESULT_FROM_NT(status);
    }

    return S_OK;
}

static inline struct color_converter *color_converter_from_IUnknown(IUnknown *iface)
{
    return CONTAINING_RECORD(iface, struct color_converter, IUnknown_iface);
}

static HRESULT WINAPI color_converter_QueryInterface(IUnknown *iface, REFIID iid, void **out)
{
    struct color_converter *converter = color_converter_from_IUnknown(iface);

    TRACE("converter %p, iid %s, out %p.\n", converter, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown))
        *out = &converter->IUnknown_iface;
    else if (IsEqualGUID(iid, &IID_IMFTransform))
        *out = &converter->IMFTransform_iface;
    else if (IsEqualGUID(iid, &IID_IMediaObject))
        *out = &converter->IMediaObject_iface;
    else if (IsEqualIID(iid, &IID_IPropertyBag))
        *out = &converter->IPropertyBag_iface;
    else if (IsEqualIID(iid, &IID_IPropertyStore))
        *out = &converter->IPropertyStore_iface;
    else
    {
        *out = NULL;
        WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static ULONG WINAPI color_converter_AddRef(IUnknown *iface)
{
    struct color_converter *converter = color_converter_from_IUnknown(iface);
    ULONG refcount = InterlockedIncrement(&converter->refcount);
    TRACE("converter %p increasing refcount to %lu.\n", converter, refcount);
    return refcount;
}

static ULONG WINAPI color_converter_Release(IUnknown *iface)
{
    struct color_converter *converter = color_converter_from_IUnknown(iface);
    ULONG refcount = InterlockedDecrement(&converter->refcount);

    TRACE("converter %p decreasing refcount to %lu.\n", converter, refcount);

    if (!refcount)
    {
        winedmo_transform_destroy(&converter->winedmo_transform);
        MoFreeMediaType(&converter->input_type);
        MoFreeMediaType(&converter->output_type);
        free(converter);
    }

    return refcount;
}

static const IUnknownVtbl color_converter_vtbl =
{
    color_converter_QueryInterface,
    color_converter_AddRef,
    color_converter_Release,
};

static struct color_converter *color_converter_from_IMFTransform(IMFTransform *iface)
{
    return CONTAINING_RECORD(iface, struct color_converter, IMFTransform_iface);
}

static HRESULT WINAPI color_converter_IMFTransform_QueryInterface(IMFTransform *iface, REFIID iid, void **out)
{
    return IUnknown_QueryInterface(color_converter_from_IMFTransform(iface)->outer, iid, out);
}

static ULONG WINAPI color_converter_IMFTransform_AddRef(IMFTransform *iface)
{
    return IUnknown_AddRef(color_converter_from_IMFTransform(iface)->outer);
}

static ULONG WINAPI color_converter_IMFTransform_Release(IMFTransform *iface)
{
    return IUnknown_Release(color_converter_from_IMFTransform(iface)->outer);
}

static HRESULT WINAPI color_converter_IMFTransform_GetStreamLimits(IMFTransform *iface, DWORD *input_minimum,
        DWORD *input_maximum, DWORD *output_minimum, DWORD *output_maximum)
{
    TRACE("converter %p, input_minimum %p, input_maximum %p, output_minimum %p, output_maximum %p.\n",
            iface, input_minimum, input_maximum, output_minimum, output_maximum);
    *input_minimum = *input_maximum = *output_minimum = *output_maximum = 1;
    return S_OK;
}

static HRESULT WINAPI color_converter_IMFTransform_GetStreamCount(IMFTransform *iface, DWORD *inputs, DWORD *outputs)
{
    TRACE("converter %p, inputs %p, outputs %p.\n", color_converter_from_IMFTransform(iface), inputs, outputs);
    *inputs = *outputs = 1;
    return S_OK;
}

static HRESULT WINAPI color_converter_IMFTransform_GetStreamIDs(IMFTransform *iface, DWORD input_size, DWORD *inputs,
        DWORD output_size, DWORD *outputs)
{
    TRACE("converter %p, input_size %lu, inputs %p, output_size %lu, outputs %p.\n", color_converter_from_IMFTransform(iface),
            input_size, inputs, output_size, outputs);
    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_IMFTransform_GetInputStreamInfo(IMFTransform *iface, DWORD id, MFT_INPUT_STREAM_INFO *info)
{
    struct color_converter *converter = color_converter_from_IMFTransform(iface);

    TRACE("converter %p, id %#lx, info %p.\n", converter, id, info);

    if (IsEqualGUID(&converter->input_type.majortype, &GUID_NULL)
            || IsEqualGUID(&converter->output_type.majortype, &GUID_NULL))
    {
        memset(info, 0, sizeof(*info));
        return MF_E_TRANSFORM_TYPE_NOT_SET;
    }

    *info = converter->input_info;
    return S_OK;
}

static HRESULT WINAPI color_converter_IMFTransform_GetOutputStreamInfo(IMFTransform *iface, DWORD id, MFT_OUTPUT_STREAM_INFO *info)
{
    struct color_converter *converter = color_converter_from_IMFTransform(iface);

    TRACE("converter %p, id %#lx, info %p.\n", converter, id, info);

    if (IsEqualGUID(&converter->input_type.majortype, &GUID_NULL)
            || IsEqualGUID(&converter->output_type.majortype, &GUID_NULL))
    {
        memset(info, 0, sizeof(*info));
        return MF_E_TRANSFORM_TYPE_NOT_SET;
    }

    *info = converter->output_info;
    return S_OK;
}

static HRESULT WINAPI color_converter_IMFTransform_GetAttributes(IMFTransform *iface, IMFAttributes **attributes)
{
    TRACE("converter %p, attributes %p.\n", color_converter_from_IMFTransform(iface), attributes);
    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_IMFTransform_GetInputStreamAttributes(IMFTransform *iface, DWORD id, IMFAttributes **attributes)
{
    TRACE("converter %p, id %#lx, attributes %p.\n", color_converter_from_IMFTransform(iface), id, attributes);
    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_IMFTransform_GetOutputStreamAttributes(IMFTransform *iface, DWORD id, IMFAttributes **attributes)
{
    TRACE("converter %p, id %#lx, attributes %p.\n", color_converter_from_IMFTransform(iface), id, attributes);
    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_IMFTransform_DeleteInputStream(IMFTransform *iface, DWORD id)
{
    TRACE("converter %p, id %#lx.\n", color_converter_from_IMFTransform(iface), id);
    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_IMFTransform_AddInputStreams(IMFTransform *iface, DWORD streams, DWORD *ids)
{
    TRACE("converter %p, streams %lu, ids %p.\n", color_converter_from_IMFTransform(iface), streams, ids);
    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_IMFTransform_GetInputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    struct color_converter *converter = color_converter_from_IMFTransform(iface);
    DMO_MEDIA_TYPE mt = {0};
    HRESULT hr;

    TRACE("converter %p, id %#lx, index %#lx, type %p.\n", converter, id, index, type);

    if (FAILED(hr = MF_RESULT_FROM_DMO(IMediaObject_GetInputType(&converter->IMediaObject_iface, id, index, &mt))))
        return hr;
    if (SUCCEEDED(hr = MFCreateMediaTypeFromRepresentation(AM_MEDIA_TYPE_REPRESENTATION, &mt, type)))
        IMFMediaType_SetGUID(*type, &MF_MT_SUBTYPE, &mt.subtype);
    MoFreeMediaType(&mt);
    return hr;
}

static HRESULT WINAPI color_converter_IMFTransform_GetOutputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    struct color_converter *converter = color_converter_from_IMFTransform(iface);
    DMO_MEDIA_TYPE mt = {0};
    HRESULT hr;

    TRACE("converter %p, id %#lx, index %#lx, type %p.\n", converter, id, index, type);

    if (FAILED(hr = MF_RESULT_FROM_DMO(IMediaObject_GetOutputType(&converter->IMediaObject_iface, id, index, &mt))))
        return hr;
    if (SUCCEEDED(hr = MFCreateMediaTypeFromRepresentation(AM_MEDIA_TYPE_REPRESENTATION, &mt, type)))
        IMFMediaType_SetGUID(*type, &MF_MT_SUBTYPE, &mt.subtype);
    MoFreeMediaType(&mt);
    return hr;
}

static HRESULT WINAPI color_converter_IMFTransform_SetInputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    struct color_converter *converter = color_converter_from_IMFTransform(iface);
    DMO_MEDIA_TYPE mt = {0};
    HRESULT hr;

    TRACE("converter %p, id %#lx, type %p, flags %#lx.\n", converter, id, type, flags);

    if (type && FAILED(hr = MFInitAMMediaTypeFromMFMediaType(type, GUID_NULL, (AM_MEDIA_TYPE *)&mt)))
        return hr;
    if (type && FAILED(hr = IMFMediaType_GetItem(type, &MF_MT_DEFAULT_STRIDE, NULL)))
    {
        VIDEOINFOHEADER *vih = (VIDEOINFOHEADER *)mt.pbFormat;
        vih->bmiHeader.biHeight = get_default_stride_sign(vih) * abs(vih->bmiHeader.biHeight);
    }
    hr = MF_RESULT_FROM_DMO(IMediaObject_SetInputType(&converter->IMediaObject_iface, id, type ? &mt : NULL,
            flags & MFT_SET_TYPE_TEST_ONLY ? DMO_SET_TYPEF_TEST_ONLY : 0));
    MoFreeMediaType(&mt);
    return hr;
}

static HRESULT WINAPI color_converter_IMFTransform_SetOutputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    struct color_converter *converter = color_converter_from_IMFTransform(iface);
    DMO_MEDIA_TYPE mt = {0};
    HRESULT hr;

    TRACE("converter %p, id %#lx, type %p, flags %#lx.\n", converter, id, type, flags);

    if (type && FAILED(hr = MFInitAMMediaTypeFromMFMediaType(type, GUID_NULL, (AM_MEDIA_TYPE *)&mt)))
        return hr;
    if (type && FAILED(hr = IMFMediaType_GetItem(type, &MF_MT_DEFAULT_STRIDE, NULL)))
    {
        VIDEOINFOHEADER *vih = (VIDEOINFOHEADER *)mt.pbFormat;
        vih->bmiHeader.biHeight = get_default_stride_sign(vih) * abs(vih->bmiHeader.biHeight);
    }
    hr = MF_RESULT_FROM_DMO(IMediaObject_SetOutputType(&converter->IMediaObject_iface, id, type ? &mt : NULL,
            flags & MFT_SET_TYPE_TEST_ONLY ? DMO_SET_TYPEF_TEST_ONLY : 0));
    MoFreeMediaType(&mt);
    return hr;
}

static HRESULT WINAPI color_converter_IMFTransform_GetInputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **type)
{
    struct color_converter *converter = color_converter_from_IMFTransform(iface);

    TRACE("converter %p, id %#lx, type %p.\n", converter, id, type);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;
    if (IsEqualGUID(&converter->input_type.majortype, &GUID_NULL))
        return MF_E_TRANSFORM_TYPE_NOT_SET;
    return MFCreateMediaTypeFromRepresentation(AM_MEDIA_TYPE_REPRESENTATION, &converter->input_type, type);
}

static HRESULT WINAPI color_converter_IMFTransform_GetOutputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **type)
{
    struct color_converter *converter = color_converter_from_IMFTransform(iface);

    TRACE("converter %p, id %#lx, type %p.\n", converter, id, type);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;
    if (IsEqualGUID(&converter->output_type.majortype, &GUID_NULL))
        return MF_E_TRANSFORM_TYPE_NOT_SET;
    return MFCreateMediaTypeFromRepresentation(AM_MEDIA_TYPE_REPRESENTATION, &converter->output_type, type);
}

static HRESULT WINAPI color_converter_IMFTransform_GetInputStatus(IMFTransform *iface, DWORD id, DWORD *flags)
{
    FIXME("converter %p, id %#lx, flags %p stub!\n", color_converter_from_IMFTransform(iface), id, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_IMFTransform_GetOutputStatus(IMFTransform *iface, DWORD *flags)
{
    FIXME("converter %p, flags %p stub!\n", color_converter_from_IMFTransform(iface), flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_IMFTransform_SetOutputBounds(IMFTransform *iface, LONGLONG lower, LONGLONG upper)
{
    TRACE("converter %p, lower %I64d, upper %I64d.\n", color_converter_from_IMFTransform(iface), lower, upper);
    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_IMFTransform_ProcessEvent(IMFTransform *iface, DWORD id, IMFMediaEvent *event)
{
    FIXME("converter %p, id %#lx, event %p stub!\n", color_converter_from_IMFTransform(iface), id, event);
    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_IMFTransform_ProcessMessage(IMFTransform *iface, MFT_MESSAGE_TYPE message, ULONG_PTR param)
{
    FIXME("converter %p, message %#x, param %#Ix stub!\n", color_converter_from_IMFTransform(iface), message, param);
    return S_OK;
}

static HRESULT WINAPI color_converter_IMFTransform_ProcessInput(IMFTransform *iface, DWORD id, IMFSample *sample, DWORD flags)
{
    struct color_converter *converter = color_converter_from_IMFTransform(iface);
    IMediaBuffer *media_buffer;
    IMFMediaBuffer *buffer;
    HRESULT hr;

    TRACE("converter %p, id %#lx, sample %p, flags %#lx.\n", converter, id, sample, flags);

    if (FAILED(hr = IMFSample_ConvertToContiguousBuffer(sample, &buffer)))
        return hr;
    if (SUCCEEDED(hr = MFCreateLegacyMediaBufferOnMFMediaBuffer(sample, buffer, 0, &media_buffer)))
    {
        hr = MF_RESULT_FROM_DMO(IMediaObject_ProcessInput(&converter->IMediaObject_iface, id, media_buffer, 0, 0, 0));
        IMediaBuffer_Release(media_buffer);
    }
    IMFMediaBuffer_Release(buffer);

    return hr;
}

static HRESULT WINAPI color_converter_IMFTransform_ProcessOutput(IMFTransform *iface, DWORD flags, DWORD count,
        MFT_OUTPUT_DATA_BUFFER *output, DWORD *output_status)
{
    struct color_converter *converter = color_converter_from_IMFTransform(iface);
    DMO_OUTPUT_DATA_BUFFER dmo_output = {0};
    IMFMediaBuffer *buffer;
    DWORD dmo_status;
    HRESULT hr;

    TRACE("converter %p, flags %#lx, count %lu, output %p, output_status %p.\n", converter, flags, count, output, output_status);

    if (count != 1)
        return E_INVALIDARG;

    if (FAILED(hr = IMFSample_ConvertToContiguousBuffer(output->pSample, &buffer)))
        return hr;
    if (SUCCEEDED(hr = MFCreateLegacyMediaBufferOnMFMediaBuffer(output->pSample, buffer, 0, &dmo_output.pBuffer)))
    {
        hr = MF_RESULT_FROM_DMO(IMediaObject_ProcessOutput(&converter->IMediaObject_iface, flags, 1, &dmo_output, &dmo_status));
        IMediaBuffer_Release(dmo_output.pBuffer);
    }
    IMFMediaBuffer_Release(buffer);

    output->dwStatus = *output_status = 0;
    return hr;
}

static const IMFTransformVtbl color_converter_IMFTransform_vtbl =
{
    color_converter_IMFTransform_QueryInterface,
    color_converter_IMFTransform_AddRef,
    color_converter_IMFTransform_Release,
    color_converter_IMFTransform_GetStreamLimits,
    color_converter_IMFTransform_GetStreamCount,
    color_converter_IMFTransform_GetStreamIDs,
    color_converter_IMFTransform_GetInputStreamInfo,
    color_converter_IMFTransform_GetOutputStreamInfo,
    color_converter_IMFTransform_GetAttributes,
    color_converter_IMFTransform_GetInputStreamAttributes,
    color_converter_IMFTransform_GetOutputStreamAttributes,
    color_converter_IMFTransform_DeleteInputStream,
    color_converter_IMFTransform_AddInputStreams,
    color_converter_IMFTransform_GetInputAvailableType,
    color_converter_IMFTransform_GetOutputAvailableType,
    color_converter_IMFTransform_SetInputType,
    color_converter_IMFTransform_SetOutputType,
    color_converter_IMFTransform_GetInputCurrentType,
    color_converter_IMFTransform_GetOutputCurrentType,
    color_converter_IMFTransform_GetInputStatus,
    color_converter_IMFTransform_GetOutputStatus,
    color_converter_IMFTransform_SetOutputBounds,
    color_converter_IMFTransform_ProcessEvent,
    color_converter_IMFTransform_ProcessMessage,
    color_converter_IMFTransform_ProcessInput,
    color_converter_IMFTransform_ProcessOutput,
};

static inline struct color_converter *color_converter_from_IMediaObject(IMediaObject *iface)
{
    return CONTAINING_RECORD(iface, struct color_converter, IMediaObject_iface);
}

static HRESULT WINAPI color_converter_IMediaObject_QueryInterface(IMediaObject *iface, REFIID iid, void **obj)
{
    return IUnknown_QueryInterface(color_converter_from_IMediaObject(iface)->outer, iid, obj);
}

static ULONG WINAPI color_converter_IMediaObject_AddRef(IMediaObject *iface)
{
    return IUnknown_AddRef(color_converter_from_IMediaObject(iface)->outer);
}

static ULONG WINAPI color_converter_IMediaObject_Release(IMediaObject *iface)
{
    return IUnknown_Release(color_converter_from_IMediaObject(iface)->outer);
}

static HRESULT WINAPI color_converter_IMediaObject_GetStreamCount(IMediaObject *iface, DWORD *input, DWORD *output)
{
    TRACE("converter %p, input %p, output %p\n", color_converter_from_IMediaObject(iface), input, output);
    *input = *output = 1;
    return S_OK;
}

static HRESULT WINAPI color_converter_IMediaObject_GetInputStreamInfo(IMediaObject *iface, DWORD index, DWORD *flags)
{
    FIXME("converter %p, index %lu, flags %p stub!\n", color_converter_from_IMediaObject(iface), index, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_IMediaObject_GetOutputStreamInfo(IMediaObject *iface, DWORD index, DWORD *flags)
{
    FIXME("converter %p, index %lu, flags %p stub!\n", color_converter_from_IMediaObject(iface), index, flags);
    return E_NOTIMPL;
}

static HRESULT get_available_media_type(DWORD index, DMO_MEDIA_TYPE *type, const GUID *const *subtypes)
{
    VIDEOINFOHEADER *vih;
    const GUID *subtype;

    if (!(vih = CoTaskMemAlloc(sizeof(*vih))))
        return E_OUTOFMEMORY;
    subtype = subtypes[index];

    memset(vih, 0, sizeof(*vih));
    vih->bmiHeader.biSize = sizeof(vih->bmiHeader);

    memset(type, 0, sizeof(*type));
    type->majortype = MEDIATYPE_Video;
    type->formattype = FORMAT_VideoInfo;
    type->subtype = *subtype;
    type->pbFormat = (BYTE *)vih;
    type->cbFormat = sizeof(*vih);
    type->bFixedSizeSamples = TRUE;

    return S_OK;
}

static HRESULT WINAPI color_converter_IMediaObject_GetInputType(IMediaObject *iface, DWORD index, DWORD type_index,
        DMO_MEDIA_TYPE *type)
{
    TRACE("converter %p, index %lu, type_index %lu, type %p\n", color_converter_from_IMediaObject(iface), index, type_index, type);

    if (index > 0)
        return DMO_E_INVALIDSTREAMINDEX;
    if (type_index >= ARRAY_SIZE(color_converter_inputs))
        return DMO_E_NO_MORE_ITEMS;
    return type ? get_available_media_type(type_index, type, color_converter_inputs) : S_OK;
}

static HRESULT WINAPI color_converter_IMediaObject_GetOutputType(IMediaObject *iface, DWORD index, DWORD type_index,
        DMO_MEDIA_TYPE *type)
{
    TRACE("converter %p, index %lu, type_index %lu, type %p\n", color_converter_from_IMediaObject(iface), index, type_index, type);

    if (index > 0)
        return DMO_E_INVALIDSTREAMINDEX;
    if (type_index >= ARRAY_SIZE(color_converter_outputs))
        return DMO_E_NO_MORE_ITEMS;
    return type ? get_available_media_type(type_index, type, color_converter_outputs) : S_OK;
}

static HRESULT check_dmo_media_type(const DMO_MEDIA_TYPE *type, UINT *image_size,
        const GUID *const *formats, UINT format_count)
{
    VIDEOINFOHEADER *vih;
    ULONG i;

    if (!IsEqualGUID(&type->majortype, &MEDIATYPE_Video))
        return DMO_E_INVALIDTYPE;
    if (!IsEqualGUID(&type->formattype, &FORMAT_VideoInfo)
            && !IsEqualGUID(&type->formattype, &FORMAT_VideoInfo2))
        return DMO_E_INVALIDTYPE;
    if (type->cbFormat < sizeof(*vih))
        return DMO_E_INVALIDTYPE;

    for (i = 0; i < format_count; ++i)
        if (IsEqualGUID(get_dmo_subtype(&type->subtype), get_dmo_subtype(formats[i])))
            break;
    if (i == format_count)
        return DMO_E_INVALIDTYPE;

    vih = (VIDEOINFOHEADER *)type->pbFormat;
    if (!vih->bmiHeader.biWidth || !vih->bmiHeader.biHeight)
        return DMO_E_INVALIDTYPE;

    *image_size = vih->bmiHeader.biSizeImage;
    return S_OK;
}

static HRESULT WINAPI color_converter_IMediaObject_SetInputType(IMediaObject *iface, DWORD index,
        const DMO_MEDIA_TYPE *type, DWORD flags)
{
    struct color_converter *converter = color_converter_from_IMediaObject(iface);
    UINT32 image_size;
    HRESULT hr;

    TRACE("converter %p, index %#lx, type %p, flags %#lx.\n", converter, index, type, flags);

    if (index != 0)
        return DMO_E_INVALIDSTREAMINDEX;
    if (!type)
    {
        MoFreeMediaType(&converter->input_type);
        memset(&converter->input_type, 0, sizeof(converter->input_type));
        winedmo_transform_destroy(&converter->winedmo_transform);
        return S_OK;
    }

    if (FAILED(hr = check_dmo_media_type(type, &image_size, color_converter_inputs, ARRAY_SIZE(color_converter_inputs))))
        return hr;
    if (flags & DMO_SET_TYPEF_TEST_ONLY)
        return S_OK;

    MoFreeMediaType(&converter->input_type);
    memset(&converter->input_type, 0, sizeof(converter->input_type));

    if (FAILED(hr = MoCopyMediaType(&converter->input_type, type)))
        WARN("Failed to create input type from media type, hr %#lx\n", hr);
    else
        converter->input_info.cbSize = image_size;

    if (!IsEqualGUID(&converter->input_type.majortype, &GUID_NULL)
            && !IsEqualGUID(&converter->output_type.majortype, &GUID_NULL))
        hr = color_converter_create_transform(converter);

    if (FAILED(hr))
    {
        MoFreeMediaType(&converter->input_type);
        memset(&converter->input_type, 0, sizeof(converter->input_type));
    }

    return hr;
}

static HRESULT WINAPI color_converter_IMediaObject_SetOutputType(IMediaObject *iface, DWORD index,
        const DMO_MEDIA_TYPE *type, DWORD flags)
{
    struct color_converter *converter = color_converter_from_IMediaObject(iface);
    UINT32 image_size;
    HRESULT hr;

    TRACE("converter %p, index %#lx, type %p, flags %#lx.\n", converter, index, type, flags);

    if (index != 0)
        return DMO_E_INVALIDSTREAMINDEX;
    if (!type)
    {
        MoFreeMediaType(&converter->output_type);
        memset(&converter->output_type, 0, sizeof(converter->output_type));
        winedmo_transform_destroy(&converter->winedmo_transform);
        return S_OK;
    }

    if (FAILED(hr = check_dmo_media_type(type, &image_size, color_converter_outputs, ARRAY_SIZE(color_converter_outputs))))
        return hr;
    if (flags & DMO_SET_TYPEF_TEST_ONLY)
        return S_OK;

    MoFreeMediaType(&converter->output_type);
    memset(&converter->output_type, 0, sizeof(converter->output_type));

    if (FAILED(hr = MoCopyMediaType(&converter->output_type, type)))
        WARN("Failed to create output type from media type, hr %#lx\n", hr);
    else
        converter->output_info.cbSize = image_size;

    if (!IsEqualGUID(&converter->input_type.majortype, &GUID_NULL)
            && !IsEqualGUID(&converter->output_type.majortype, &GUID_NULL))
        hr = color_converter_create_transform(converter);

    if (FAILED(hr))
    {
        MoFreeMediaType(&converter->output_type);
        memset(&converter->output_type, 0, sizeof(converter->output_type));
    }

    return hr;
}

static HRESULT WINAPI color_converter_IMediaObject_GetInputCurrentType(IMediaObject *iface, DWORD index, DMO_MEDIA_TYPE *type)
{
    struct color_converter *converter = color_converter_from_IMediaObject(iface);

    TRACE("converter %p, index %#lx, type %p.\n", converter, index, type);

    if (index != 0)
        return DMO_E_INVALIDSTREAMINDEX;
    if (IsEqualGUID(&converter->output_type.majortype, &GUID_NULL))
        return DMO_E_TYPE_NOT_SET;
    return MoCopyMediaType(type, &converter->output_type);
}

static HRESULT WINAPI color_converter_IMediaObject_GetOutputCurrentType(IMediaObject *iface, DWORD index, DMO_MEDIA_TYPE *type)
{
    struct color_converter *converter = color_converter_from_IMediaObject(iface);

    TRACE("converter %p, index %#lx, type %p.\n", converter, index, type);

    if (index != 0)
        return DMO_E_INVALIDSTREAMINDEX;
    if (IsEqualGUID(&converter->output_type.majortype, &GUID_NULL))
        return DMO_E_TYPE_NOT_SET;
    return MoCopyMediaType(type, &converter->output_type);
}

static HRESULT WINAPI color_converter_IMediaObject_GetInputSizeInfo(IMediaObject *iface, DWORD index, DWORD *size,
        DWORD *lookahead, DWORD *alignment)
{
    FIXME("converter %p, index %lu, size %p, lookahead %p, alignment %p stub!\n", color_converter_from_IMediaObject(iface), index, size,
            lookahead, alignment);
    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_IMediaObject_GetOutputSizeInfo(IMediaObject *iface, DWORD index, DWORD *size, DWORD *alignment)
{
    FIXME("converter %p, index %lu, size %p, alignment %p stub!\n", color_converter_from_IMediaObject(iface), index, size, alignment);
    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_IMediaObject_GetInputMaxLatency(IMediaObject *iface, DWORD index, REFERENCE_TIME *latency)
{
    FIXME("converter %p, index %lu, latency %p stub!\n", color_converter_from_IMediaObject(iface), index, latency);
    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_IMediaObject_SetInputMaxLatency(IMediaObject *iface, DWORD index, REFERENCE_TIME latency)
{
    FIXME("converter %p, index %lu, latency %s stub!\n", color_converter_from_IMediaObject(iface), index, wine_dbgstr_longlong(latency));
    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_IMediaObject_Flush(IMediaObject *iface)
{
    FIXME("converter %p stub!\n", color_converter_from_IMediaObject(iface));
    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_IMediaObject_Discontinuity(IMediaObject *iface, DWORD index)
{
    FIXME("converter %p, index %lu stub!\n", color_converter_from_IMediaObject(iface), index);
    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_IMediaObject_AllocateStreamingResources(IMediaObject *iface)
{
    FIXME("converter %p stub!\n", color_converter_from_IMediaObject(iface));
    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_IMediaObject_FreeStreamingResources(IMediaObject *iface)
{
    FIXME("converter %p stub!\n", color_converter_from_IMediaObject(iface));
    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_IMediaObject_GetInputStatus(IMediaObject *iface, DWORD index, DWORD *flags)
{
    FIXME("converter %p, index %lu, flags %p stub!\n", color_converter_from_IMediaObject(iface), index, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_IMediaObject_ProcessInput(IMediaObject *iface, DWORD index,
        IMediaBuffer *buffer, DWORD flags, REFERENCE_TIME timestamp, REFERENCE_TIME timelength)
{
    DMO_OUTPUT_DATA_BUFFER input = {.pBuffer = buffer, .dwStatus = flags, .rtTimestamp = timestamp, .rtTimelength = timelength};
    struct color_converter *converter = color_converter_from_IMediaObject(iface);
    HRESULT hr = S_OK;
    NTSTATUS status;

    TRACE("converter %p, index %lu, buffer %p, flags %#lx, timestamp %I64d, timelength %I64d\n", converter,
            index, buffer, flags, timestamp, timelength);

    if (!converter->winedmo_transform.handle)
        return DMO_E_TYPE_NOT_SET;
    if (converter->input_sample)
        return DMO_E_NOTACCEPTING;

    if (!(status = winedmo_transform_process_input(converter->winedmo_transform, &input)))
    {
        converter->input_sample = (IUnknown *)buffer;
        IUnknown_AddRef(converter->input_sample);
    }
    else
    {
        if (status == STATUS_PENDING) hr = DMO_E_NOTACCEPTING;
        else hr = HRESULT_FROM_NT(status);
    }

    return hr;
}

static HRESULT WINAPI color_converter_IMediaObject_ProcessOutput(IMediaObject *iface, DWORD flags, DWORD count,
        DMO_OUTPUT_DATA_BUFFER *output, DWORD *output_status)
{
    struct color_converter *converter = color_converter_from_IMediaObject(iface);
    IUnknown *input_sample;
    HRESULT hr = S_OK;
    NTSTATUS status;

    TRACE("converter %p, flags %#lx, count %lu, output %p, output_status %p\n", converter,
            flags, count, output, output_status);

    if (!converter->winedmo_transform.handle)
        return DMO_E_TYPE_NOT_SET;
    if (count != 1)
        return E_INVALIDARG;
    if (flags)
        FIXME("Unimplemented flags %#lx\n", flags);

    if (!(input_sample = converter->input_sample))
        return S_FALSE;
    converter->input_sample = NULL;

    if ((status = winedmo_transform_process_output(converter->winedmo_transform, output)))
    {
        if (status == STATUS_PENDING) hr = S_FALSE;
        else hr = HRESULT_FROM_NT(status);
    }

    IUnknown_Release(input_sample);
    return hr;
}

static HRESULT WINAPI color_converter_IMediaObject_Lock(IMediaObject *iface, LONG lock)
{
    FIXME("converter %p, lock %ld stub!\n", color_converter_from_IMediaObject(iface), lock);
    return E_NOTIMPL;
}

static const IMediaObjectVtbl color_converter_IMediaObject_vtbl =
{
    color_converter_IMediaObject_QueryInterface,
    color_converter_IMediaObject_AddRef,
    color_converter_IMediaObject_Release,
    color_converter_IMediaObject_GetStreamCount,
    color_converter_IMediaObject_GetInputStreamInfo,
    color_converter_IMediaObject_GetOutputStreamInfo,
    color_converter_IMediaObject_GetInputType,
    color_converter_IMediaObject_GetOutputType,
    color_converter_IMediaObject_SetInputType,
    color_converter_IMediaObject_SetOutputType,
    color_converter_IMediaObject_GetInputCurrentType,
    color_converter_IMediaObject_GetOutputCurrentType,
    color_converter_IMediaObject_GetInputSizeInfo,
    color_converter_IMediaObject_GetOutputSizeInfo,
    color_converter_IMediaObject_GetInputMaxLatency,
    color_converter_IMediaObject_SetInputMaxLatency,
    color_converter_IMediaObject_Flush,
    color_converter_IMediaObject_Discontinuity,
    color_converter_IMediaObject_AllocateStreamingResources,
    color_converter_IMediaObject_FreeStreamingResources,
    color_converter_IMediaObject_GetInputStatus,
    color_converter_IMediaObject_ProcessInput,
    color_converter_IMediaObject_ProcessOutput,
    color_converter_IMediaObject_Lock,
};

static inline struct color_converter *color_converter_from_IPropertyBag(IPropertyBag *iface)
{
    return CONTAINING_RECORD(iface, struct color_converter, IPropertyBag_iface);
}

static HRESULT WINAPI color_converter_IPropertyBag_QueryInterface(IPropertyBag *iface, REFIID iid, void **out)
{
    return IUnknown_QueryInterface(color_converter_from_IPropertyBag(iface)->outer, iid, out);
}

static ULONG WINAPI color_converter_IPropertyBag_AddRef(IPropertyBag *iface)
{
    return IUnknown_AddRef(color_converter_from_IPropertyBag(iface)->outer);
}

static ULONG WINAPI color_converter_IPropertyBag_Release(IPropertyBag *iface)
{
    return IUnknown_Release(color_converter_from_IPropertyBag(iface)->outer);
}

static HRESULT WINAPI color_converter_IPropertyBag_Read(IPropertyBag *iface, const WCHAR *prop_name, VARIANT *value,
        IErrorLog *error_log)
{
    FIXME("converter %p, prop_name %s, value %p, error_log %p stub!\n", color_converter_from_IPropertyBag(iface), debugstr_w(prop_name), value, error_log);
    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_IPropertyBag_Write(IPropertyBag *iface, const WCHAR *prop_name, VARIANT *value)
{
    FIXME("converter %p, prop_name %s, value %p stub!\n", color_converter_from_IPropertyBag(iface), debugstr_w(prop_name), value);
    return S_OK;
}

static const IPropertyBagVtbl color_converter_IPropertyBag_vtbl =
{
    color_converter_IPropertyBag_QueryInterface,
    color_converter_IPropertyBag_AddRef,
    color_converter_IPropertyBag_Release,
    color_converter_IPropertyBag_Read,
    color_converter_IPropertyBag_Write,
};

static inline struct color_converter *color_converter_from_IPropertyStore(IPropertyStore *iface)
{
    return CONTAINING_RECORD(iface, struct color_converter, IPropertyStore_iface);
}

static HRESULT WINAPI color_converter_IPropertyStore_QueryInterface(IPropertyStore *iface, REFIID iid, void **out)
{
    return IUnknown_QueryInterface(color_converter_from_IPropertyStore(iface)->outer, iid, out);
}

static ULONG WINAPI color_converter_IPropertyStore_AddRef(IPropertyStore *iface)
{
    return IUnknown_AddRef(color_converter_from_IPropertyStore(iface)->outer);
}

static ULONG WINAPI color_converter_IPropertyStore_Release(IPropertyStore *iface)
{
    return IUnknown_Release(color_converter_from_IPropertyStore(iface)->outer);
}

static HRESULT WINAPI color_converter_IPropertyStore_GetCount(IPropertyStore *iface, DWORD *count)
{
    FIXME("converter %p, count %p stub!\n", color_converter_from_IPropertyStore(iface), count);
    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_IPropertyStore_GetAt(IPropertyStore *iface, DWORD index, PROPERTYKEY *key)
{
    FIXME("converter %p, index %lu, key %p stub!\n", color_converter_from_IPropertyStore(iface), index, key);
    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_IPropertyStore_GetValue(IPropertyStore *iface, REFPROPERTYKEY key, PROPVARIANT *value)
{
    FIXME("converter %p, key %p, value %p stub!\n", color_converter_from_IPropertyStore(iface), key, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_IPropertyStore_SetValue(IPropertyStore *iface, REFPROPERTYKEY key, REFPROPVARIANT value)
{
    FIXME("converter %p, key %p, value %p stub!\n", color_converter_from_IPropertyStore(iface), key, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_IPropertyStore_Commit(IPropertyStore *iface)
{
    FIXME("converter %p stub!\n", color_converter_from_IPropertyStore(iface));
    return E_NOTIMPL;
}

static const IPropertyStoreVtbl color_converter_IPropertyStore_vtbl =
{
    color_converter_IPropertyStore_QueryInterface,
    color_converter_IPropertyStore_AddRef,
    color_converter_IPropertyStore_Release,
    color_converter_IPropertyStore_GetCount,
    color_converter_IPropertyStore_GetAt,
    color_converter_IPropertyStore_GetValue,
    color_converter_IPropertyStore_SetValue,
    color_converter_IPropertyStore_Commit,
};

static HRESULT WINAPI color_converter_factory_CreateInstance(IClassFactory *iface, IUnknown *outer,
        REFIID riid, void **out)
{
    struct color_converter *converter;
    NTSTATUS status;
    HRESULT hr;

    TRACE("%p, %s, %p.\n", outer, debugstr_guid(riid), out);

    if ((status = winedmo_transform_check(MFMediaType_Video, MFVideoFormat_NV12, MFVideoFormat_RGB32)))
    {
        static const GUID CLSID_wg_color_converter = {0xf47e2da5,0xe370,0x47b7,{0x90,0x3a,0x07,0x8d,0xdd,0x45,0xa5,0xcc}};
        WARN("Unsupported winedmo transform, status %#lx.\n", status);
        return CoCreateInstance(&CLSID_wg_color_converter, outer, CLSCTX_INPROC_SERVER, riid, out);
    }

    *out = NULL;
    if (outer && !IsEqualGUID(riid, &IID_IUnknown))
        return E_NOINTERFACE;
    if (!(converter = calloc(1, sizeof(*converter))))
        return E_OUTOFMEMORY;

    converter->IUnknown_iface.lpVtbl = &color_converter_vtbl;
    converter->IMFTransform_iface.lpVtbl = &color_converter_IMFTransform_vtbl;
    converter->IMediaObject_iface.lpVtbl = &color_converter_IMediaObject_vtbl;
    converter->IPropertyBag_iface.lpVtbl = &color_converter_IPropertyBag_vtbl;
    converter->IPropertyStore_iface.lpVtbl = &color_converter_IPropertyStore_vtbl;
    converter->refcount = 1;
    converter->outer = outer ? outer : &converter->IUnknown_iface;

    converter->input_info.cbAlignment = 1;
    converter->output_info.cbAlignment = 1;
    TRACE("created %p\n", converter);

    hr = IUnknown_QueryInterface(&converter->IUnknown_iface, riid, out);
    IUnknown_Release(&converter->IUnknown_iface);
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

static const IClassFactoryVtbl color_converter_factory_vtbl =
{
    class_factory_QueryInterface,
    class_factory_AddRef,
    class_factory_Release,
    color_converter_factory_CreateInstance,
    class_factory_LockServer,
};

static IClassFactory color_converter_factory = {&color_converter_factory_vtbl};

/***********************************************************************
 *              DllGetClassObject (colorcnv.@)
 */
HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void **out)
{
    if (IsEqualGUID(clsid, &CLSID_CColorConvertDMO))
        return IClassFactory_QueryInterface(&color_converter_factory, riid, out);

    *out = NULL;
    FIXME("Unknown clsid %s.\n", debugstr_guid(clsid));
    return CLASS_E_CLASSNOTAVAILABLE;
}

/***********************************************************************
 *              DllRegisterServer (colorcnv.@)
 */
HRESULT WINAPI DllRegisterServer(void)
{
    MFT_REGISTER_TYPE_INFO color_converter_mft_inputs[] =
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
    MFT_REGISTER_TYPE_INFO color_converter_mft_outputs[] =
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
    DMO_PARTIAL_MEDIATYPE color_converter_dmo_inputs[] =
    {
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_YV12},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_YUY2},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_UYVY},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_AYUV},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_NV12},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_RGB32},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_RGB565},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_I420},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_IYUV},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_YVYU},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_RGB24},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_RGB555},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_RGB8},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_V216},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_V410},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_NV11},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_Y41P},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_Y41T},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_Y42T},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_YVU9},
    };
    DMO_PARTIAL_MEDIATYPE color_converter_dmo_outputs[] =
    {
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_YV12},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_YUY2},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_UYVY},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_AYUV},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_NV12},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_RGB32},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_RGB565},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_I420},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_IYUV},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_YVYU},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_RGB24},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_RGB555},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_RGB8},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_V216},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_V410},
        {.type = MEDIATYPE_Video, .subtype = MEDIASUBTYPE_NV11},
    };
    HRESULT hr;

    TRACE("\n");

    if (FAILED(hr = __wine_register_resources()))
        return hr;
    if (FAILED(hr = MFTRegister(CLSID_CColorConvertDMO, MFT_CATEGORY_VIDEO_EFFECT,
            (WCHAR *)L"Color Converter MFT", MFT_ENUM_FLAG_SYNCMFT,
            ARRAY_SIZE(color_converter_mft_inputs), color_converter_mft_inputs,
            ARRAY_SIZE(color_converter_mft_outputs), color_converter_mft_outputs, NULL)))
        return hr;
    if (FAILED(hr = DMORegister(L"Color Converter DMO", &CLSID_CColorConvertDMO, &DMOCATEGORY_VIDEO_EFFECT, 0,
            ARRAY_SIZE(color_converter_dmo_inputs), color_converter_dmo_inputs,
            ARRAY_SIZE(color_converter_dmo_outputs), color_converter_dmo_outputs)))
        return hr;

    return S_OK;
}

/***********************************************************************
 *              DllUnregisterServer (colorcnv.@)
 */
HRESULT WINAPI DllUnregisterServer(void)
{
    HRESULT hr;

    TRACE("\n");

    if (FAILED(hr = __wine_unregister_resources()))
        return hr;
    if (FAILED(hr = MFTUnregister(CLSID_CColorConvertDMO)))
        return hr;
    if (FAILED(hr = DMOUnregister(&CLSID_CColorConvertDMO, &DMOCATEGORY_VIDEO_EFFECT)))
        return hr;

    return S_OK;
}
