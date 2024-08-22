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

#include "dmoreg.h"
#include "dmort.h"
#include "dshow.h"
#include "mediaerr.h"
#include "mfapi.h"
#include "mferror.h"
#include "mfidl.h"
#include "rpcproxy.h"
#include "wmcodecdsp.h"

#include "wine/debug.h"
#include "wine/winedmo.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmo);

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

static const GUID *resampler_types[] =
{
    &MFAudioFormat_Float,
    &MFAudioFormat_PCM,
};

struct resampler
{
    IUnknown IUnknown_iface;
    IMFTransform IMFTransform_iface;
    IMediaObject IMediaObject_iface;
    IPropertyBag IPropertyBag_iface;
    IPropertyStore IPropertyStore_iface;
    IWMResamplerProps IWMResamplerProps_iface;
    IUnknown *outer;
    LONG refcount;

    DMO_MEDIA_TYPE input_type;
    DMO_MEDIA_TYPE output_type;

    MFT_INPUT_STREAM_INFO input_info;
    MFT_OUTPUT_STREAM_INFO output_info;

    IUnknown *input_sample;
    struct winedmo_transform winedmo_transform;
};

static HRESULT resampler_create_transform(struct resampler *resampler)
{
    NTSTATUS status;

    winedmo_transform_destroy(&resampler->winedmo_transform);

    if ((status = winedmo_transform_create(MFMediaType_Audio, (void *)resampler->input_type.pbFormat,
            (void *)resampler->output_type.pbFormat, &resampler->winedmo_transform)))
    {
        WARN("Failed to create resampler transform, status %#lx\n", status);
        return HRESULT_FROM_NT(status);
    }

    return S_OK;
}

static inline struct resampler *resampler_from_IUnknown(IUnknown *iface)
{
    return CONTAINING_RECORD(iface, struct resampler, IUnknown_iface);
}

static HRESULT WINAPI resampler_QueryInterface(IUnknown *iface, REFIID iid, void **out)
{
    struct resampler *resampler = resampler_from_IUnknown(iface);

    TRACE("resampler %p, iid %s, out %p.\n", resampler, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown))
        *out = &resampler->IUnknown_iface;
    else if (IsEqualGUID(iid, &IID_IMFTransform))
        *out = &resampler->IMFTransform_iface;
    else if (IsEqualGUID(iid, &IID_IMediaObject))
        *out = &resampler->IMediaObject_iface;
    else if (IsEqualIID(iid, &IID_IPropertyBag))
        *out = &resampler->IPropertyBag_iface;
    else if (IsEqualIID(iid, &IID_IPropertyStore))
        *out = &resampler->IPropertyStore_iface;
    else if (IsEqualIID(iid, &IID_IWMResamplerProps))
        *out = &resampler->IWMResamplerProps_iface;
    else
    {
        *out = NULL;
        WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static ULONG WINAPI resampler_AddRef(IUnknown *iface)
{
    struct resampler *resampler = resampler_from_IUnknown(iface);
    ULONG refcount = InterlockedIncrement(&resampler->refcount);
    TRACE("resampler %p increasing refcount to %lu.\n", resampler, refcount);
    return refcount;
}

static ULONG WINAPI resampler_Release(IUnknown *iface)
{
    struct resampler *resampler = resampler_from_IUnknown(iface);
    ULONG refcount = InterlockedDecrement(&resampler->refcount);

    TRACE("resampler %p decreasing refcount to %lu.\n", resampler, refcount);

    if (!refcount)
    {
        if (resampler->input_sample)
            IUnknown_Release(resampler->input_sample);
        winedmo_transform_destroy(&resampler->winedmo_transform);
        MoFreeMediaType(&resampler->input_type);
        MoFreeMediaType(&resampler->output_type);
        free(resampler);
    }

    return refcount;
}

static const IUnknownVtbl resampler_vtbl =
{
    resampler_QueryInterface,
    resampler_AddRef,
    resampler_Release,
};

static struct resampler *resampler_from_IMFTransform(IMFTransform *iface)
{
    return CONTAINING_RECORD(iface, struct resampler, IMFTransform_iface);
}

static HRESULT WINAPI resampler_IMFTransform_QueryInterface(IMFTransform *iface, REFIID iid, void **out)
{
    return IUnknown_QueryInterface(resampler_from_IMFTransform(iface)->outer, iid, out);
}

static ULONG WINAPI resampler_IMFTransform_AddRef(IMFTransform *iface)
{
    return IUnknown_AddRef(resampler_from_IMFTransform(iface)->outer);
}

static ULONG WINAPI resampler_IMFTransform_Release(IMFTransform *iface)
{
    return IUnknown_Release(resampler_from_IMFTransform(iface)->outer);
}

static HRESULT WINAPI resampler_IMFTransform_GetStreamLimits(IMFTransform *iface, DWORD *input_minimum,
        DWORD *input_maximum, DWORD *output_minimum, DWORD *output_maximum)
{
    TRACE("resampler %p, input_minimum %p, input_maximum %p, output_minimum %p, output_maximum %p.\n",
            iface, input_minimum, input_maximum, output_minimum, output_maximum);
    *input_minimum = *input_maximum = *output_minimum = *output_maximum = 1;
    return S_OK;
}

static HRESULT WINAPI resampler_IMFTransform_GetStreamCount(IMFTransform *iface, DWORD *inputs, DWORD *outputs)
{
    TRACE("resampler %p, inputs %p, outputs %p.\n", resampler_from_IMFTransform(iface), inputs, outputs);
    *inputs = *outputs = 1;
    return S_OK;
}

static HRESULT WINAPI resampler_IMFTransform_GetStreamIDs(IMFTransform *iface, DWORD input_size, DWORD *inputs,
        DWORD output_size, DWORD *outputs)
{
    TRACE("resampler %p, input_size %lu, inputs %p, output_size %lu, outputs %p.\n", resampler_from_IMFTransform(iface),
            input_size, inputs, output_size, outputs);
    return E_NOTIMPL;
}

static HRESULT WINAPI resampler_IMFTransform_GetInputStreamInfo(IMFTransform *iface, DWORD id, MFT_INPUT_STREAM_INFO *info)
{
    struct resampler *resampler = resampler_from_IMFTransform(iface);

    TRACE("resampler %p, id %#lx, info %p.\n", resampler, id, info);

    if (IsEqualGUID(&resampler->input_type.majortype, &GUID_NULL)
            || IsEqualGUID(&resampler->output_type.majortype, &GUID_NULL))
    {
        memset(info, 0, sizeof(*info));
        return MF_E_TRANSFORM_TYPE_NOT_SET;
    }

    *info = resampler->input_info;
    return S_OK;
}

static HRESULT WINAPI resampler_IMFTransform_GetOutputStreamInfo(IMFTransform *iface, DWORD id, MFT_OUTPUT_STREAM_INFO *info)
{
    struct resampler *resampler = resampler_from_IMFTransform(iface);

    TRACE("resampler %p, id %#lx, info %p.\n", resampler, id, info);

    if (IsEqualGUID(&resampler->input_type.majortype, &GUID_NULL)
            || IsEqualGUID(&resampler->output_type.majortype, &GUID_NULL))
    {
        memset(info, 0, sizeof(*info));
        return MF_E_TRANSFORM_TYPE_NOT_SET;
    }

    *info = resampler->output_info;
    return S_OK;
}

static HRESULT WINAPI resampler_IMFTransform_GetAttributes(IMFTransform *iface, IMFAttributes **attributes)
{
    TRACE("resampler %p, attributes %p.\n", resampler_from_IMFTransform(iface), attributes);
    return E_NOTIMPL;
}

static HRESULT WINAPI resampler_IMFTransform_GetInputStreamAttributes(IMFTransform *iface, DWORD id, IMFAttributes **attributes)
{
    TRACE("resampler %p, id %#lx, attributes %p.\n", resampler_from_IMFTransform(iface), id, attributes);
    return E_NOTIMPL;
}

static HRESULT WINAPI resampler_IMFTransform_GetOutputStreamAttributes(IMFTransform *iface, DWORD id, IMFAttributes **attributes)
{
    TRACE("resampler %p, id %#lx, attributes %p.\n", resampler_from_IMFTransform(iface), id, attributes);
    return E_NOTIMPL;
}

static HRESULT WINAPI resampler_IMFTransform_DeleteInputStream(IMFTransform *iface, DWORD id)
{
    TRACE("resampler %p, id %#lx.\n", resampler_from_IMFTransform(iface), id);
    return E_NOTIMPL;
}

static HRESULT WINAPI resampler_IMFTransform_AddInputStreams(IMFTransform *iface, DWORD streams, DWORD *ids)
{
    TRACE("resampler %p, streams %lu, ids %p.\n", resampler_from_IMFTransform(iface), streams, ids);
    return E_NOTIMPL;
}

static HRESULT WINAPI resampler_IMFTransform_GetInputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    struct resampler *resampler = resampler_from_IMFTransform(iface);
    DMO_MEDIA_TYPE mt = {0};
    HRESULT hr;

    TRACE("resampler %p, id %#lx, index %#lx, type %p.\n", resampler, id, index, type);

    if (FAILED(hr = MF_RESULT_FROM_DMO(IMediaObject_GetInputType(&resampler->IMediaObject_iface, id, index, &mt))))
        return hr;
    hr = MFCreateMediaTypeFromRepresentation(AM_MEDIA_TYPE_REPRESENTATION, &mt, type);
    MoFreeMediaType(&mt);
    return hr;
}

static HRESULT WINAPI resampler_IMFTransform_GetOutputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    struct resampler *resampler = resampler_from_IMFTransform(iface);
    DMO_MEDIA_TYPE mt = {0};
    HRESULT hr;

    TRACE("resampler %p, id %#lx, index %#lx, type %p.\n", resampler, id, index, type);

    if (FAILED(hr = MF_RESULT_FROM_DMO(IMediaObject_GetOutputType(&resampler->IMediaObject_iface, id, index, &mt))))
        return hr;
    hr = MFCreateMediaTypeFromRepresentation(AM_MEDIA_TYPE_REPRESENTATION, &mt, type);
    MoFreeMediaType(&mt);
    return hr;
}

static HRESULT WINAPI resampler_IMFTransform_SetInputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    struct resampler *resampler = resampler_from_IMFTransform(iface);
    DMO_MEDIA_TYPE mt = {0};
    HRESULT hr;

    TRACE("resampler %p, id %#lx, type %p, flags %#lx.\n", resampler, id, type, flags);

    if (type && FAILED(hr = MFInitAMMediaTypeFromMFMediaType(type, GUID_NULL, (AM_MEDIA_TYPE *)&mt)))
        return hr;
    hr = MF_RESULT_FROM_DMO(IMediaObject_SetInputType(&resampler->IMediaObject_iface, id, type ? &mt : NULL,
            flags & MFT_SET_TYPE_TEST_ONLY ? DMO_SET_TYPEF_TEST_ONLY : 0));
    MoFreeMediaType(&mt);
    return hr;
}

static HRESULT WINAPI resampler_IMFTransform_SetOutputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    struct resampler *resampler = resampler_from_IMFTransform(iface);
    DMO_MEDIA_TYPE mt = {0};
    HRESULT hr;

    TRACE("resampler %p, id %#lx, type %p, flags %#lx.\n", resampler, id, type, flags);

    if (type && FAILED(hr = MFInitAMMediaTypeFromMFMediaType(type, GUID_NULL, (AM_MEDIA_TYPE *)&mt)))
        return hr;
    hr = MF_RESULT_FROM_DMO(IMediaObject_SetOutputType(&resampler->IMediaObject_iface, id, type ? &mt : NULL,
            flags & MFT_SET_TYPE_TEST_ONLY ? DMO_SET_TYPEF_TEST_ONLY : 0));
    MoFreeMediaType(&mt);
    return hr;
}

static HRESULT WINAPI resampler_IMFTransform_GetInputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **type)
{
    struct resampler *resampler = resampler_from_IMFTransform(iface);

    TRACE("resampler %p, id %#lx, type %p.\n", resampler, id, type);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;
    if (IsEqualGUID(&resampler->input_type.majortype, &GUID_NULL))
        return MF_E_TRANSFORM_TYPE_NOT_SET;
    return MFCreateMediaTypeFromRepresentation(AM_MEDIA_TYPE_REPRESENTATION, &resampler->input_type, type);
}

static HRESULT WINAPI resampler_IMFTransform_GetOutputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **type)
{
    struct resampler *resampler = resampler_from_IMFTransform(iface);

    TRACE("resampler %p, id %#lx, type %p.\n", resampler, id, type);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;
    if (IsEqualGUID(&resampler->output_type.majortype, &GUID_NULL))
        return MF_E_TRANSFORM_TYPE_NOT_SET;
    return MFCreateMediaTypeFromRepresentation(AM_MEDIA_TYPE_REPRESENTATION, &resampler->output_type, type);
}

static HRESULT WINAPI resampler_IMFTransform_GetInputStatus(IMFTransform *iface, DWORD id, DWORD *flags)
{
    FIXME("resampler %p, id %#lx, flags %p stub!\n", resampler_from_IMFTransform(iface), id, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI resampler_IMFTransform_GetOutputStatus(IMFTransform *iface, DWORD *flags)
{
    FIXME("resampler %p, flags %p stub!\n", resampler_from_IMFTransform(iface), flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI resampler_IMFTransform_SetOutputBounds(IMFTransform *iface, LONGLONG lower, LONGLONG upper)
{
    TRACE("resampler %p, lower %I64d, upper %I64d.\n", resampler_from_IMFTransform(iface), lower, upper);
    return E_NOTIMPL;
}

static HRESULT WINAPI resampler_IMFTransform_ProcessEvent(IMFTransform *iface, DWORD id, IMFMediaEvent *event)
{
    FIXME("resampler %p, id %#lx, event %p stub!\n", resampler_from_IMFTransform(iface), id, event);
    return E_NOTIMPL;
}

static HRESULT WINAPI resampler_IMFTransform_ProcessMessage(IMFTransform *iface, MFT_MESSAGE_TYPE message, ULONG_PTR param)
{
    FIXME("resampler %p, message %#x, param %p stub!\n", resampler_from_IMFTransform(iface), message, (void *)param);
    return S_OK;
}

static HRESULT WINAPI resampler_IMFTransform_ProcessInput(IMFTransform *iface, DWORD id, IMFSample *sample, DWORD flags)
{
    struct resampler *resampler = resampler_from_IMFTransform(iface);
    IMediaBuffer *media_buffer;
    IMFMediaBuffer *buffer;
    HRESULT hr;

    TRACE("resampler %p, id %#lx, sample %p, flags %#lx.\n", resampler, id, sample, flags);

    if (FAILED(hr = IMFSample_ConvertToContiguousBuffer(sample, &buffer)))
        return hr;
    if (SUCCEEDED(hr = MFCreateLegacyMediaBufferOnMFMediaBuffer(sample, buffer, 0, &media_buffer)))
    {
        hr = MF_RESULT_FROM_DMO(IMediaObject_ProcessInput(&resampler->IMediaObject_iface, id, media_buffer, 0, 0, 0));
        IMediaBuffer_Release(media_buffer);
    }
    IMFMediaBuffer_Release(buffer);

    return hr;
}

static HRESULT WINAPI resampler_IMFTransform_ProcessOutput(IMFTransform *iface, DWORD flags, DWORD count,
        MFT_OUTPUT_DATA_BUFFER *output, DWORD *output_status)
{
    struct resampler *resampler = resampler_from_IMFTransform(iface);
    DMO_OUTPUT_DATA_BUFFER dmo_output = {0};
    IMFMediaBuffer *buffer;
    DWORD dmo_status;
    HRESULT hr;

    TRACE("resampler %p, flags %#lx, count %lu, output %p, output_status %p.\n", resampler, flags, count, output, output_status);

    if (count != 1)
        return E_INVALIDARG;

    if (FAILED(hr = IMFSample_ConvertToContiguousBuffer(output->pSample, &buffer)))
        return hr;
    if (SUCCEEDED(hr = MFCreateLegacyMediaBufferOnMFMediaBuffer(output->pSample, buffer, 0, &dmo_output.pBuffer)))
    {
        hr = MF_RESULT_FROM_DMO(IMediaObject_ProcessOutput(&resampler->IMediaObject_iface, flags, 1, &dmo_output, &dmo_status));
        IMediaBuffer_Release(dmo_output.pBuffer);
    }
    IMFMediaBuffer_Release(buffer);

    output->dwStatus = *output_status = 0;
    if (hr == S_FALSE)
        output->dwStatus = MFT_OUTPUT_DATA_BUFFER_NO_SAMPLE;
    else if (dmo_output.dwStatus & DMO_OUTPUT_DATA_BUFFERF_INCOMPLETE)
        output->dwStatus = MFT_OUTPUT_DATA_BUFFER_INCOMPLETE;
    return hr;
}

static const IMFTransformVtbl resampler_IMFTransform_vtbl =
{
    resampler_IMFTransform_QueryInterface,
    resampler_IMFTransform_AddRef,
    resampler_IMFTransform_Release,
    resampler_IMFTransform_GetStreamLimits,
    resampler_IMFTransform_GetStreamCount,
    resampler_IMFTransform_GetStreamIDs,
    resampler_IMFTransform_GetInputStreamInfo,
    resampler_IMFTransform_GetOutputStreamInfo,
    resampler_IMFTransform_GetAttributes,
    resampler_IMFTransform_GetInputStreamAttributes,
    resampler_IMFTransform_GetOutputStreamAttributes,
    resampler_IMFTransform_DeleteInputStream,
    resampler_IMFTransform_AddInputStreams,
    resampler_IMFTransform_GetInputAvailableType,
    resampler_IMFTransform_GetOutputAvailableType,
    resampler_IMFTransform_SetInputType,
    resampler_IMFTransform_SetOutputType,
    resampler_IMFTransform_GetInputCurrentType,
    resampler_IMFTransform_GetOutputCurrentType,
    resampler_IMFTransform_GetInputStatus,
    resampler_IMFTransform_GetOutputStatus,
    resampler_IMFTransform_SetOutputBounds,
    resampler_IMFTransform_ProcessEvent,
    resampler_IMFTransform_ProcessMessage,
    resampler_IMFTransform_ProcessInput,
    resampler_IMFTransform_ProcessOutput,
};

static inline struct resampler *resampler_from_IMediaObject(IMediaObject *iface)
{
    return CONTAINING_RECORD(iface, struct resampler, IMediaObject_iface);
}

static HRESULT WINAPI resampler_IMediaObject_QueryInterface(IMediaObject *iface, REFIID iid, void **obj)
{
    return IUnknown_QueryInterface(resampler_from_IMediaObject(iface)->outer, iid, obj);
}

static ULONG WINAPI resampler_IMediaObject_AddRef(IMediaObject *iface)
{
    return IUnknown_AddRef(resampler_from_IMediaObject(iface)->outer);
}

static ULONG WINAPI resampler_IMediaObject_Release(IMediaObject *iface)
{
    return IUnknown_Release(resampler_from_IMediaObject(iface)->outer);
}

static HRESULT WINAPI resampler_IMediaObject_GetStreamCount(IMediaObject *iface, DWORD *input, DWORD *output)
{
    TRACE("resampler %p, input %p, output %p\n", resampler_from_IMediaObject(iface), input, output);
    *input = *output = 1;
    return S_OK;
}

static HRESULT WINAPI resampler_IMediaObject_GetInputStreamInfo(IMediaObject *iface, DWORD index, DWORD *flags)
{
    FIXME("resampler %p, index %lu, flags %p stub!\n", resampler_from_IMediaObject(iface), index, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI resampler_IMediaObject_GetOutputStreamInfo(IMediaObject *iface, DWORD index, DWORD *flags)
{
    FIXME("resampler %p, index %lu, flags %p stub!\n", resampler_from_IMediaObject(iface), index, flags);
    return E_NOTIMPL;
}

static HRESULT get_available_media_type(DWORD index, DMO_MEDIA_TYPE *type, BOOL output)
{
    const GUID *subtype;
    WAVEFORMATEX *wfx;

    if (!(wfx = CoTaskMemAlloc(sizeof(*wfx))))
        return E_OUTOFMEMORY;
    subtype = resampler_types[index % ARRAY_SIZE(resampler_types)];

    memset(wfx, 0, sizeof(*wfx));
    wfx->wFormatTag = subtype->Data1;

    memset(type, 0, sizeof(*type));
    type->majortype = MFMediaType_Audio;
    type->formattype = FORMAT_WaveFormatEx;
    type->subtype = *subtype;
    type->pbFormat = (BYTE *)wfx;
    type->cbFormat = sizeof(*wfx);
    if (index < ARRAY_SIZE(resampler_types))
        return S_OK;

    wfx->wBitsPerSample = IsEqualGUID(subtype, &MFAudioFormat_Float) ? 32 : 16;
    wfx->nChannels = 2;
    wfx->nSamplesPerSec = 48000;
    wfx->nBlockAlign = wfx->wBitsPerSample * wfx->nChannels / 8;
    wfx->nAvgBytesPerSec = wfx->nSamplesPerSec * wfx->nBlockAlign;
    return S_OK;
}

static HRESULT WINAPI resampler_IMediaObject_GetInputType(IMediaObject *iface, DWORD index, DWORD type_index,
        DMO_MEDIA_TYPE *type)
{
    TRACE("resampler %p, index %lu, type_index %lu, type %p\n", resampler_from_IMediaObject(iface), index, type_index, type);

    if (index > 0)
        return DMO_E_INVALIDSTREAMINDEX;
    if (type_index >= ARRAY_SIZE(resampler_types))
        return DMO_E_NO_MORE_ITEMS;
    return type ? get_available_media_type(type_index, type, FALSE) : S_OK;
}

static HRESULT WINAPI resampler_IMediaObject_GetOutputType(IMediaObject *iface, DWORD index, DWORD type_index,
        DMO_MEDIA_TYPE *type)
{
    TRACE("resampler %p, index %lu, type_index %lu, type %p\n", resampler_from_IMediaObject(iface), index, type_index, type);

    if (index > 0)
        return DMO_E_INVALIDSTREAMINDEX;
    if (type_index >= 2 * ARRAY_SIZE(resampler_types))
        return DMO_E_NO_MORE_ITEMS;
    return type ? get_available_media_type(type_index, type, TRUE) : S_OK;
}

static HRESULT check_dmo_media_type(const DMO_MEDIA_TYPE *type, UINT *block_alignment)
{
    WAVEFORMATEX *wfx;
    ULONG i;

    if (!IsEqualGUID(&type->majortype, &MEDIATYPE_Audio))
        return DMO_E_INVALIDTYPE;
    if (!IsEqualGUID(&type->formattype, &FORMAT_WaveFormatEx) || type->cbFormat < sizeof(*wfx))
        return DMO_E_INVALIDTYPE;

    for (i = 0; i < ARRAY_SIZE(resampler_types); ++i)
        if (IsEqualGUID(&type->subtype, resampler_types[i]))
            break;
    if (i == ARRAY_SIZE(resampler_types))
        return DMO_E_INVALIDTYPE;

    wfx = (WAVEFORMATEX *)type->pbFormat;
    if (!wfx->wBitsPerSample || !wfx->nAvgBytesPerSec || !wfx->nChannels || !wfx->nSamplesPerSec || !wfx->nBlockAlign)
        return DMO_E_INVALIDTYPE;

    *block_alignment = wfx->nBlockAlign;
    return S_OK;
}

static HRESULT WINAPI resampler_IMediaObject_SetInputType(IMediaObject *iface, DWORD index,
        const DMO_MEDIA_TYPE *type, DWORD flags)
{
    struct resampler *resampler = resampler_from_IMediaObject(iface);
    UINT32 block_alignment;
    HRESULT hr;

    TRACE("resampler %p, index %#lx, type %p, flags %#lx.\n", resampler, index, type, flags);

    if (index != 0)
        return DMO_E_INVALIDSTREAMINDEX;
    if (!type)
    {
        MoFreeMediaType(&resampler->input_type);
        memset(&resampler->input_type, 0, sizeof(resampler->input_type));
        MoFreeMediaType(&resampler->output_type);
        memset(&resampler->output_type, 0, sizeof(resampler->output_type));
        winedmo_transform_destroy(&resampler->winedmo_transform);
        return S_OK;
    }

    if (FAILED(hr = check_dmo_media_type(type, &block_alignment)))
        return hr;
    if (flags & DMO_SET_TYPEF_TEST_ONLY)
        return S_OK;

    MoFreeMediaType(&resampler->input_type);
    memset(&resampler->input_type, 0, sizeof(resampler->input_type));
    MoFreeMediaType(&resampler->output_type);
    memset(&resampler->output_type, 0, sizeof(resampler->output_type));

    if (FAILED(hr = MoCopyMediaType(&resampler->input_type, type)))
        WARN("Failed to create input type from media type, hr %#lx\n", hr);
    else
        resampler->input_info.cbSize = block_alignment;

    return hr;
}

static HRESULT WINAPI resampler_IMediaObject_SetOutputType(IMediaObject *iface, DWORD index,
        const DMO_MEDIA_TYPE *type, DWORD flags)
{
    struct resampler *resampler = resampler_from_IMediaObject(iface);
    UINT32 block_alignment;
    HRESULT hr;

    TRACE("resampler %p, index %#lx, type %p, flags %#lx.\n", resampler, index, type, flags);

    if (index != 0)
        return DMO_E_INVALIDSTREAMINDEX;
    if (!type)
    {
        MoFreeMediaType(&resampler->output_type);
        memset(&resampler->output_type, 0, sizeof(resampler->output_type));
        winedmo_transform_destroy(&resampler->winedmo_transform);
        return S_OK;
    }

    if (IsEqualGUID(&resampler->input_type.majortype, &GUID_NULL))
        return DMO_E_TYPE_NOT_SET;
    if (FAILED(hr = check_dmo_media_type(type, &block_alignment)))
        return hr;
    if (flags & DMO_SET_TYPEF_TEST_ONLY)
        return S_OK;

    MoFreeMediaType(&resampler->output_type);
    memset(&resampler->output_type, 0, sizeof(resampler->output_type));

    if (FAILED(hr = MoCopyMediaType(&resampler->output_type, type)))
        WARN("Failed to create output type from media type, hr %#lx\n", hr);
    else
    {
        resampler->output_info.cbSize = block_alignment;
        hr = resampler_create_transform(resampler);
    }

    if (FAILED(hr))
    {
        MoFreeMediaType(&resampler->output_type);
        memset(&resampler->output_type, 0, sizeof(resampler->output_type));
    }

    return hr;
}

static HRESULT WINAPI resampler_IMediaObject_GetInputCurrentType(IMediaObject *iface, DWORD index, DMO_MEDIA_TYPE *type)
{
    struct resampler *resampler = resampler_from_IMediaObject(iface);

    TRACE("resampler %p, index %#lx, type %p.\n", resampler, index, type);

    if (index != 0)
        return DMO_E_INVALIDSTREAMINDEX;
    if (IsEqualGUID(&resampler->input_type.majortype, &GUID_NULL))
        return DMO_E_TYPE_NOT_SET;
    return MoCopyMediaType(type, &resampler->input_type);
}

static HRESULT WINAPI resampler_IMediaObject_GetOutputCurrentType(IMediaObject *iface, DWORD index, DMO_MEDIA_TYPE *type)
{
    struct resampler *resampler = resampler_from_IMediaObject(iface);

    TRACE("resampler %p, index %#lx, type %p.\n", resampler, index, type);

    if (index != 0)
        return DMO_E_INVALIDSTREAMINDEX;
    if (IsEqualGUID(&resampler->output_type.majortype, &GUID_NULL))
        return DMO_E_TYPE_NOT_SET;
    return MoCopyMediaType(type, &resampler->output_type);
}

static HRESULT WINAPI resampler_IMediaObject_GetInputSizeInfo(IMediaObject *iface, DWORD index, DWORD *size,
        DWORD *lookahead, DWORD *alignment)
{
    FIXME("resampler %p, index %lu, size %p, lookahead %p, alignment %p stub!\n", resampler_from_IMediaObject(iface), index, size,
            lookahead, alignment);
    return E_NOTIMPL;
}

static HRESULT WINAPI resampler_IMediaObject_GetOutputSizeInfo(IMediaObject *iface, DWORD index, DWORD *size, DWORD *alignment)
{
    FIXME("resampler %p, index %lu, size %p, alignment %p stub!\n", resampler_from_IMediaObject(iface), index, size, alignment);
    return E_NOTIMPL;
}

static HRESULT WINAPI resampler_IMediaObject_GetInputMaxLatency(IMediaObject *iface, DWORD index, REFERENCE_TIME *latency)
{
    FIXME("resampler %p, index %lu, latency %p stub!\n", resampler_from_IMediaObject(iface), index, latency);
    return E_NOTIMPL;
}

static HRESULT WINAPI resampler_IMediaObject_SetInputMaxLatency(IMediaObject *iface, DWORD index, REFERENCE_TIME latency)
{
    FIXME("resampler %p, index %lu, latency %I64d stub!\n", resampler_from_IMediaObject(iface), index, latency);
    return E_NOTIMPL;
}

static HRESULT WINAPI resampler_IMediaObject_Flush(IMediaObject *iface)
{
    FIXME("resampler %p stub!\n", resampler_from_IMediaObject(iface));
    return E_NOTIMPL;
}

static HRESULT WINAPI resampler_IMediaObject_Discontinuity(IMediaObject *iface, DWORD index)
{
    FIXME("resampler %p, index %lu stub!\n", resampler_from_IMediaObject(iface), index);
    return E_NOTIMPL;
}

static HRESULT WINAPI resampler_IMediaObject_AllocateStreamingResources(IMediaObject *iface)
{
    FIXME("resampler %p stub!\n", resampler_from_IMediaObject(iface));
    return E_NOTIMPL;
}

static HRESULT WINAPI resampler_IMediaObject_FreeStreamingResources(IMediaObject *iface)
{
    FIXME("resampler %p stub!\n", resampler_from_IMediaObject(iface));
    return E_NOTIMPL;
}

static HRESULT WINAPI resampler_IMediaObject_GetInputStatus(IMediaObject *iface, DWORD index, DWORD *flags)
{
    FIXME("resampler %p, index %lu, flags %p stub!\n", resampler_from_IMediaObject(iface), index, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI resampler_IMediaObject_ProcessInput(IMediaObject *iface, DWORD index,
        IMediaBuffer *buffer, DWORD flags, REFERENCE_TIME timestamp, REFERENCE_TIME timelength)
{
    DMO_OUTPUT_DATA_BUFFER input = {.pBuffer = buffer, .dwStatus = flags, .rtTimestamp = timestamp, .rtTimelength = timelength};
    struct resampler *resampler = resampler_from_IMediaObject(iface);
    HRESULT hr = S_OK;
    NTSTATUS status;

    TRACE("resampler %p, index %lu, buffer %p, flags %#lx, timestamp %I64d, timelength %I64d\n", resampler,
            index, buffer, flags, timestamp, timelength);

    if (!resampler->winedmo_transform.handle)
        return DMO_E_TYPE_NOT_SET;
    if (resampler->input_sample)
        return DMO_E_NOTACCEPTING;

    if (!(status = winedmo_transform_process_input(resampler->winedmo_transform, &input)))
    {
        resampler->input_sample = (IUnknown *)buffer;
        IUnknown_AddRef(resampler->input_sample);
    }
    else
    {
        if (status == STATUS_PENDING) hr = DMO_E_NOTACCEPTING;
        else hr = HRESULT_FROM_NT(status);
    }

    return hr;
}

static HRESULT WINAPI resampler_IMediaObject_ProcessOutput(IMediaObject *iface, DWORD flags, DWORD count,
        DMO_OUTPUT_DATA_BUFFER *output, DWORD *output_status)
{
    struct resampler *resampler = resampler_from_IMediaObject(iface);
    IUnknown *input_sample;
    HRESULT hr = S_OK;
    NTSTATUS status;

    TRACE("resampler %p, flags %#lx, count %lu, output %p, output_status %p\n", resampler,
            flags, count, output, output_status);

    if (!resampler->winedmo_transform.handle)
        return DMO_E_TYPE_NOT_SET;
    if (count != 1)
        return E_INVALIDARG;
    if (flags)
        FIXME("Unimplemented flags %#lx\n", flags);

    if (!(input_sample = resampler->input_sample))
        return S_FALSE;
    resampler->input_sample = NULL;

    if ((status = winedmo_transform_process_output(resampler->winedmo_transform, output)))
    {
        if (status == STATUS_PENDING) hr = S_FALSE;
        else hr = HRESULT_FROM_NT(status);
    }

    IUnknown_Release(input_sample);
    return hr;
}

static HRESULT WINAPI resampler_IMediaObject_Lock(IMediaObject *iface, LONG lock)
{
    FIXME("resampler %p, lock %ld stub!\n", resampler_from_IMediaObject(iface), lock);
    return E_NOTIMPL;
}

static const IMediaObjectVtbl resampler_IMediaObject_vtbl =
{
    resampler_IMediaObject_QueryInterface,
    resampler_IMediaObject_AddRef,
    resampler_IMediaObject_Release,
    resampler_IMediaObject_GetStreamCount,
    resampler_IMediaObject_GetInputStreamInfo,
    resampler_IMediaObject_GetOutputStreamInfo,
    resampler_IMediaObject_GetInputType,
    resampler_IMediaObject_GetOutputType,
    resampler_IMediaObject_SetInputType,
    resampler_IMediaObject_SetOutputType,
    resampler_IMediaObject_GetInputCurrentType,
    resampler_IMediaObject_GetOutputCurrentType,
    resampler_IMediaObject_GetInputSizeInfo,
    resampler_IMediaObject_GetOutputSizeInfo,
    resampler_IMediaObject_GetInputMaxLatency,
    resampler_IMediaObject_SetInputMaxLatency,
    resampler_IMediaObject_Flush,
    resampler_IMediaObject_Discontinuity,
    resampler_IMediaObject_AllocateStreamingResources,
    resampler_IMediaObject_FreeStreamingResources,
    resampler_IMediaObject_GetInputStatus,
    resampler_IMediaObject_ProcessInput,
    resampler_IMediaObject_ProcessOutput,
    resampler_IMediaObject_Lock,
};

static inline struct resampler *resampler_from_IPropertyBag(IPropertyBag *iface)
{
    return CONTAINING_RECORD(iface, struct resampler, IPropertyBag_iface);
}

static HRESULT WINAPI resampler_IPropertyBag_QueryInterface(IPropertyBag *iface, REFIID iid, void **out)
{
    return IUnknown_QueryInterface(resampler_from_IPropertyBag(iface)->outer, iid, out);
}

static ULONG WINAPI resampler_IPropertyBag_AddRef(IPropertyBag *iface)
{
    return IUnknown_AddRef(resampler_from_IPropertyBag(iface)->outer);
}

static ULONG WINAPI resampler_IPropertyBag_Release(IPropertyBag *iface)
{
    return IUnknown_Release(resampler_from_IPropertyBag(iface)->outer);
}

static HRESULT WINAPI resampler_IPropertyBag_Read(IPropertyBag *iface, const WCHAR *prop_name, VARIANT *value,
        IErrorLog *error_log)
{
    FIXME("resampler %p, prop_name %s, value %p, error_log %p stub!\n", resampler_from_IPropertyBag(iface), debugstr_w(prop_name), value, error_log);
    return E_NOTIMPL;
}

static HRESULT WINAPI resampler_IPropertyBag_Write(IPropertyBag *iface, const WCHAR *prop_name, VARIANT *value)
{
    FIXME("resampler %p, prop_name %s, value %p stub!\n", resampler_from_IPropertyBag(iface), debugstr_w(prop_name), value);
    return S_OK;
}

static const IPropertyBagVtbl resampler_IPropertyBag_vtbl =
{
    resampler_IPropertyBag_QueryInterface,
    resampler_IPropertyBag_AddRef,
    resampler_IPropertyBag_Release,
    resampler_IPropertyBag_Read,
    resampler_IPropertyBag_Write,
};

static inline struct resampler *resampler_from_IPropertyStore(IPropertyStore *iface)
{
    return CONTAINING_RECORD(iface, struct resampler, IPropertyStore_iface);
}

static HRESULT WINAPI resampler_IPropertyStore_QueryInterface(IPropertyStore *iface, REFIID iid, void **out)
{
    return IUnknown_QueryInterface(resampler_from_IPropertyStore(iface)->outer, iid, out);
}

static ULONG WINAPI resampler_IPropertyStore_AddRef(IPropertyStore *iface)
{
    return IUnknown_AddRef(resampler_from_IPropertyStore(iface)->outer);
}

static ULONG WINAPI resampler_IPropertyStore_Release(IPropertyStore *iface)
{
    return IUnknown_Release(resampler_from_IPropertyStore(iface)->outer);
}

static HRESULT WINAPI resampler_IPropertyStore_GetCount(IPropertyStore *iface, DWORD *count)
{
    FIXME("resampler %p, count %p stub!\n", resampler_from_IPropertyStore(iface), count);
    return E_NOTIMPL;
}

static HRESULT WINAPI resampler_IPropertyStore_GetAt(IPropertyStore *iface, DWORD index, PROPERTYKEY *key)
{
    FIXME("resampler %p, index %lu, key %p stub!\n", resampler_from_IPropertyStore(iface), index, key);
    return E_NOTIMPL;
}

static HRESULT WINAPI resampler_IPropertyStore_GetValue(IPropertyStore *iface, REFPROPERTYKEY key, PROPVARIANT *value)
{
    FIXME("resampler %p, key %p, value %p stub!\n", resampler_from_IPropertyStore(iface), key, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI resampler_IPropertyStore_SetValue(IPropertyStore *iface, REFPROPERTYKEY key, REFPROPVARIANT value)
{
    FIXME("resampler %p, key %p, value %p stub!\n", resampler_from_IPropertyStore(iface), key, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI resampler_IPropertyStore_Commit(IPropertyStore *iface)
{
    FIXME("resampler %p stub!\n", resampler_from_IPropertyStore(iface));
    return E_NOTIMPL;
}

static const IPropertyStoreVtbl resampler_IPropertyStore_vtbl =
{
    resampler_IPropertyStore_QueryInterface,
    resampler_IPropertyStore_AddRef,
    resampler_IPropertyStore_Release,
    resampler_IPropertyStore_GetCount,
    resampler_IPropertyStore_GetAt,
    resampler_IPropertyStore_GetValue,
    resampler_IPropertyStore_SetValue,
    resampler_IPropertyStore_Commit,
};

static inline struct resampler *resampler_from_IWMResamplerProps(IWMResamplerProps *iface)
{
    return CONTAINING_RECORD(iface, struct resampler, IWMResamplerProps_iface);
}

static HRESULT WINAPI resampler_IWMResamplerProps_QueryInterface(IWMResamplerProps *iface, REFIID iid, void **out)
{
    return IUnknown_QueryInterface(resampler_from_IWMResamplerProps(iface)->outer, iid, out);
}

static ULONG WINAPI resampler_IWMResamplerProps_AddRef(IWMResamplerProps *iface)
{
    return IUnknown_AddRef(resampler_from_IWMResamplerProps(iface)->outer);
}

static ULONG WINAPI resampler_IWMResamplerProps_Release(IWMResamplerProps *iface)
{
    return IUnknown_Release(resampler_from_IWMResamplerProps(iface)->outer);
}

static HRESULT WINAPI resampler_IWMResamplerProps_SetHalfFilterLength(IWMResamplerProps *iface, LONG length)
{
    FIXME("resampler %p, count %lu stub!\n", resampler_from_IWMResamplerProps(iface), length);
    return E_NOTIMPL;
}

static HRESULT WINAPI resampler_IWMResamplerProps_SetUserChannelMtx(IWMResamplerProps *iface, ChMtxType *conversion_matrix)
{
    FIXME("resampler %p, userChannelMtx %p stub!\n", resampler_from_IWMResamplerProps(iface), conversion_matrix);
    return E_NOTIMPL;
}

static const IWMResamplerPropsVtbl resampler_IWMResamplerProps_vtbl =
{
    resampler_IWMResamplerProps_QueryInterface,
    resampler_IWMResamplerProps_AddRef,
    resampler_IWMResamplerProps_Release,
    resampler_IWMResamplerProps_SetHalfFilterLength,
    resampler_IWMResamplerProps_SetUserChannelMtx,
};

static HRESULT WINAPI resampler_factory_CreateInstance(IClassFactory *iface, IUnknown *outer,
        REFIID riid, void **out)
{
    struct resampler *resampler;
    NTSTATUS status;
    HRESULT hr;

    TRACE("%p, %s, %p.\n", outer, debugstr_guid(riid), out);

    if ((status = winedmo_transform_check(MFMediaType_Audio, MFAudioFormat_PCM, MFAudioFormat_Float)))
    {
        static const GUID CLSID_wg_resampler = {0x92f35e78,0x15a5,0x486b,{0x88,0x8e,0x57,0x5f,0x99,0x65,0x1c,0xe2}};
        WARN("Unsupported winedmo transform, status %#lx.\n", status);
        return CoCreateInstance(&CLSID_wg_resampler, outer, CLSCTX_INPROC_SERVER, riid, out);
    }

    *out = NULL;
    if (outer && !IsEqualGUID(riid, &IID_IUnknown))
        return E_NOINTERFACE;
    if (!(resampler = calloc(1, sizeof(*resampler))))
        return E_OUTOFMEMORY;

    resampler->IUnknown_iface.lpVtbl = &resampler_vtbl;
    resampler->IMFTransform_iface.lpVtbl = &resampler_IMFTransform_vtbl;
    resampler->IMediaObject_iface.lpVtbl = &resampler_IMediaObject_vtbl;
    resampler->IPropertyBag_iface.lpVtbl = &resampler_IPropertyBag_vtbl;
    resampler->IPropertyStore_iface.lpVtbl = &resampler_IPropertyStore_vtbl;
    resampler->IWMResamplerProps_iface.lpVtbl = &resampler_IWMResamplerProps_vtbl;
    resampler->refcount = 1;
    resampler->outer = outer ? outer : &resampler->IUnknown_iface;

    resampler->input_info.cbAlignment = 1;
    resampler->output_info.cbAlignment = 1;
    TRACE("created %p\n", resampler);

    hr = IUnknown_QueryInterface(&resampler->IUnknown_iface, riid, out);
    IUnknown_Release(&resampler->IUnknown_iface);
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

static const IClassFactoryVtbl resampler_factory_vtbl =
{
    class_factory_QueryInterface,
    class_factory_AddRef,
    class_factory_Release,
    resampler_factory_CreateInstance,
    class_factory_LockServer,
};

static IClassFactory resampler_factory = {&resampler_factory_vtbl};

/***********************************************************************
 *              DllGetClassObject (resampledmo.@)
 */
HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void **out)
{
    if (IsEqualGUID(clsid, &CLSID_CResamplerMediaObject))
        return IClassFactory_QueryInterface(&resampler_factory, riid, out);

    *out = NULL;
    FIXME("Unknown clsid %s.\n", debugstr_guid(clsid));
    return CLASS_E_CLASSNOTAVAILABLE;
}

/***********************************************************************
 *              DllRegisterServer (resampledmo.@)
 */
HRESULT WINAPI DllRegisterServer(void)
{
    MFT_REGISTER_TYPE_INFO resampler_mft_types[] =
    {
        {MFMediaType_Audio, MFAudioFormat_PCM},
        {MFMediaType_Audio, MFAudioFormat_Float},
    };
    DMO_PARTIAL_MEDIATYPE resampler_dmo_types[] =
    {
        {.type = MEDIATYPE_Audio, .subtype = MEDIASUBTYPE_PCM},
        {.type = MEDIATYPE_Audio, .subtype = MEDIASUBTYPE_IEEE_FLOAT},
    };
    HRESULT hr;

    TRACE("\n");

    if (FAILED(hr = __wine_register_resources()))
        return hr;
    if (FAILED(hr = MFTRegister(CLSID_CResamplerMediaObject, MFT_CATEGORY_AUDIO_EFFECT,
            (WCHAR *)L"Resampler MFT", MFT_ENUM_FLAG_SYNCMFT,
            ARRAY_SIZE(resampler_mft_types), resampler_mft_types,
            ARRAY_SIZE(resampler_mft_types), resampler_mft_types, NULL)))
        return hr;
    if (FAILED(hr = DMORegister(L"Resampler DMO", &CLSID_CResamplerMediaObject, &DMOCATEGORY_AUDIO_EFFECT, 0,
            ARRAY_SIZE(resampler_dmo_types), resampler_dmo_types,
            ARRAY_SIZE(resampler_dmo_types), resampler_dmo_types)))
        return hr;

    return S_OK;
}

/***********************************************************************
 *              DllUnregisterServer (resampledmo.@)
 */
HRESULT WINAPI DllUnregisterServer(void)
{
    HRESULT hr;

    TRACE("\n");

    if (FAILED(hr = __wine_unregister_resources()))
        return hr;
    if (FAILED(hr = MFTUnregister(CLSID_CResamplerMediaObject)))
        return hr;
    if (FAILED(hr = DMOUnregister(&CLSID_CResamplerMediaObject, &DMOCATEGORY_AUDIO_EFFECT)))
        return hr;

    return S_OK;
}
