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
#include "dmort.h"
#include "dshow.h"
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

static const GUID *const wma_decoder_inputs[] =
{
    &MEDIASUBTYPE_MSAUDIO1,
    &MFAudioFormat_WMAudioV8,
    &MFAudioFormat_WMAudioV9,
    &MFAudioFormat_WMAudio_Lossless,
};
static const GUID *const wma_decoder_outputs[] =
{
    &MFAudioFormat_Float,
    &MFAudioFormat_PCM,
};

struct wma_decoder
{
    IUnknown IUnknown_iface;
    IMFTransform IMFTransform_iface;
    IMediaObject IMediaObject_iface;
    IPropertyBag IPropertyBag_iface;
    IUnknown *outer;
    LONG refcount;

    DMO_MEDIA_TYPE input_type;
    DMO_MEDIA_TYPE output_type;

    DWORD input_buf_size;
    DWORD output_buf_size;

    IMFSample *input_sample;
    struct winedmo_transform winedmo_transform;
};

static inline struct wma_decoder *wma_decoder_from_IUnknown(IUnknown *iface)
{
    return CONTAINING_RECORD(iface, struct wma_decoder, IUnknown_iface);
}

static HRESULT wma_try_create_decoder(struct wma_decoder *decoder)
{
    NTSTATUS status;

    winedmo_transform_destroy(&decoder->winedmo_transform);

    if ((status = winedmo_transform_create(MFMediaType_Audio, (void *)decoder->input_type.pbFormat,
            (void *)decoder->output_type.pbFormat, &decoder->winedmo_transform)))
    {
        ERR("FAILED %#lx\n", status);
        return HRESULT_FROM_NT(status);
    }

    return S_OK;
}

static HRESULT WINAPI wma_decoder_QueryInterface(IUnknown *iface, REFIID iid, void **out)
{
    struct wma_decoder *decoder = wma_decoder_from_IUnknown(iface);

    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown))
        *out = &decoder->IUnknown_iface;
    else if (IsEqualGUID(iid, &IID_IMFTransform))
        *out = &decoder->IMFTransform_iface;
    else if (IsEqualGUID(iid, &IID_IMediaObject))
        *out = &decoder->IMediaObject_iface;
    else if (IsEqualIID(iid, &IID_IPropertyBag))
        *out = &decoder->IPropertyBag_iface;
    else
    {
        *out = NULL;
        WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static ULONG WINAPI wma_decoder_AddRef(IUnknown *iface)
{
    struct wma_decoder *decoder = wma_decoder_from_IUnknown(iface);
    ULONG refcount = InterlockedIncrement(&decoder->refcount);

    TRACE("iface %p increasing refcount to %lu.\n", decoder, refcount);

    return refcount;
}

static ULONG WINAPI wma_decoder_Release(IUnknown *iface)
{
    struct wma_decoder *decoder = wma_decoder_from_IUnknown(iface);
    ULONG refcount = InterlockedDecrement(&decoder->refcount);

    TRACE("iface %p decreasing refcount to %lu.\n", decoder, refcount);

    if (!refcount)
    {
        if (decoder->input_sample)
            IMFSample_Release(decoder->input_sample);
        winedmo_transform_destroy(&decoder->winedmo_transform);

        MoFreeMediaType(&decoder->input_type);
        MoFreeMediaType(&decoder->output_type);
        free(decoder);
    }

    return refcount;
}

static const IUnknownVtbl wma_decoder_vtbl =
{
    wma_decoder_QueryInterface,
    wma_decoder_AddRef,
    wma_decoder_Release,
};

static struct wma_decoder *wma_decoder_from_IMFTransform(IMFTransform *iface)
{
    return CONTAINING_RECORD(iface, struct wma_decoder, IMFTransform_iface);
}

static HRESULT WINAPI wma_decoder_IMFTransform_QueryInterface(IMFTransform *iface, REFIID iid, void **out)
{
    struct wma_decoder *decoder = wma_decoder_from_IMFTransform(iface);
    return IUnknown_QueryInterface(decoder->outer, iid, out);
}

static ULONG WINAPI wma_decoder_IMFTransform_AddRef(IMFTransform *iface)
{
    struct wma_decoder *decoder = wma_decoder_from_IMFTransform(iface);
    return IUnknown_AddRef(decoder->outer);
}

static ULONG WINAPI wma_decoder_IMFTransform_Release(IMFTransform *iface)
{
    struct wma_decoder *decoder = wma_decoder_from_IMFTransform(iface);
    return IUnknown_Release(decoder->outer);
}

static HRESULT WINAPI wma_decoder_IMFTransform_GetStreamLimits(IMFTransform *iface, DWORD *input_minimum,
        DWORD *input_maximum, DWORD *output_minimum, DWORD *output_maximum)
{
    TRACE("iface %p, input_minimum %p, input_maximum %p, output_minimum %p, output_maximum %p.\n",
            iface, input_minimum, input_maximum, output_minimum, output_maximum);
    *input_minimum = *input_maximum = *output_minimum = *output_maximum = 1;
    return S_OK;
}

static HRESULT WINAPI wma_decoder_IMFTransform_GetStreamCount(IMFTransform *iface, DWORD *inputs, DWORD *outputs)
{
    TRACE("iface %p, inputs %p, outputs %p.\n", iface, inputs, outputs);
    *inputs = *outputs = 1;
    return S_OK;
}

static HRESULT WINAPI wma_decoder_IMFTransform_GetStreamIDs(IMFTransform *iface, DWORD input_size, DWORD *inputs,
        DWORD output_size, DWORD *outputs)
{
    TRACE("iface %p, input_size %lu, inputs %p, output_size %lu, outputs %p.\n", iface,
            input_size, inputs, output_size, outputs);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_IMFTransform_GetInputStreamInfo(IMFTransform *iface, DWORD id, MFT_INPUT_STREAM_INFO *info)
{
    struct wma_decoder *decoder = wma_decoder_from_IMFTransform(iface);

    TRACE("iface %p, id %lu, info %p.\n", iface, id, info);

    if (IsEqualGUID(&decoder->input_type.majortype, &GUID_NULL)
            || IsEqualGUID(&decoder->output_type.majortype, &GUID_NULL))
    {
        memset(info, 0, sizeof(*info));
        return MF_E_TRANSFORM_TYPE_NOT_SET;
    }

    info->hnsMaxLatency = 0;
    info->dwFlags = 0;
    info->cbSize = decoder->input_buf_size;
    info->cbMaxLookahead = 0;
    info->cbAlignment = 1;
    return S_OK;
}

static HRESULT WINAPI wma_decoder_IMFTransform_GetOutputStreamInfo(IMFTransform *iface, DWORD id, MFT_OUTPUT_STREAM_INFO *info)
{
    struct wma_decoder *decoder = wma_decoder_from_IMFTransform(iface);

    TRACE("iface %p, id %lu, info %p.\n", iface, id, info);

    if (IsEqualGUID(&decoder->input_type.majortype, &GUID_NULL)
            || IsEqualGUID(&decoder->output_type.majortype, &GUID_NULL))
    {
        memset(info, 0, sizeof(*info));
        return MF_E_TRANSFORM_TYPE_NOT_SET;
    }

    info->dwFlags = 0;
    info->cbSize = decoder->output_buf_size;
    info->cbAlignment = 1;
    return S_OK;
}

static HRESULT WINAPI wma_decoder_IMFTransform_GetAttributes(IMFTransform *iface, IMFAttributes **attributes)
{
    TRACE("iface %p, attributes %p.\n", iface, attributes);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_IMFTransform_GetInputStreamAttributes(IMFTransform *iface, DWORD id, IMFAttributes **attributes)
{
    TRACE("iface %p, id %#lx, attributes %p.\n", iface, id, attributes);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_IMFTransform_GetOutputStreamAttributes(IMFTransform *iface, DWORD id, IMFAttributes **attributes)
{
    TRACE("iface %p, id %#lx, attributes %p.\n", iface, id, attributes);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_IMFTransform_DeleteInputStream(IMFTransform *iface, DWORD id)
{
    TRACE("iface %p, id %#lx.\n", iface, id);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_IMFTransform_AddInputStreams(IMFTransform *iface, DWORD streams, DWORD *ids)
{
    TRACE("iface %p, streams %lu, ids %p.\n", iface, streams, ids);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_IMFTransform_GetInputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    FIXME("iface %p, id %lu, index %lu, type %p stub!\n", iface, id, index, type);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_IMFTransform_GetOutputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    UINT32 sample_size, block_alignment;
    struct wma_decoder *decoder = wma_decoder_from_IMFTransform(iface);
    IMFMediaType *media_type;
    const GUID *output_type;
    WAVEFORMATEX *wfx;
    HRESULT hr;

    TRACE("iface %p, id %lu, index %lu, type %p.\n", iface, id, index, type);

    if (IsEqualGUID(&decoder->input_type.majortype, &GUID_NULL))
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    *type = NULL;

    if (index >= ARRAY_SIZE(wma_decoder_outputs))
        return MF_E_NO_MORE_TYPES;
    output_type = wma_decoder_outputs[index];

    if (FAILED(hr = MFCreateMediaType(&media_type)))
        return hr;

    if (FAILED(hr = IMFMediaType_SetGUID(media_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio)))
        goto done;
    if (FAILED(hr = IMFMediaType_SetGUID(media_type, &MF_MT_SUBTYPE, output_type)))
        goto done;

    if (IsEqualGUID(output_type, &MFAudioFormat_Float))
        sample_size = 32;
    else if (IsEqualGUID(output_type, &MFAudioFormat_PCM))
        sample_size = 16;
    else
    {
        FIXME("Subtype %s not implemented!\n", debugstr_guid(output_type));
        hr = E_NOTIMPL;
        goto done;
    }

    if (FAILED(hr = IMFMediaType_SetUINT32(media_type, &MF_MT_AUDIO_BITS_PER_SAMPLE,
            sample_size)))
        goto done;

    wfx = (WAVEFORMATEX *)decoder->input_type.pbFormat;
    if (FAILED(hr = IMFMediaType_SetUINT32(media_type, &MF_MT_AUDIO_NUM_CHANNELS, wfx->nChannels)))
        goto done;
    if (FAILED(hr = IMFMediaType_SetUINT32(media_type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, wfx->nSamplesPerSec)))
        goto done;

    block_alignment = sample_size * wfx->nChannels / 8;
    if (FAILED(hr = IMFMediaType_SetUINT32(media_type, &MF_MT_AUDIO_BLOCK_ALIGNMENT, block_alignment)))
        goto done;
    if (FAILED(hr = IMFMediaType_SetUINT32(media_type, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND, wfx->nSamplesPerSec * block_alignment)))
        goto done;

    if (FAILED(hr = IMFMediaType_SetUINT32(media_type, &MF_MT_ALL_SAMPLES_INDEPENDENT, 1)))
        goto done;
    if (FAILED(hr = IMFMediaType_SetUINT32(media_type, &MF_MT_FIXED_SIZE_SAMPLES, 1)))
        goto done;
    if (FAILED(hr = IMFMediaType_SetUINT32(media_type, &MF_MT_AUDIO_PREFER_WAVEFORMATEX, 1)))
        goto done;

done:
    if (SUCCEEDED(hr))
        IMFMediaType_AddRef((*type = media_type));

    IMFMediaType_Release(media_type);
    return hr;
}

static HRESULT WINAPI wma_decoder_IMFTransform_SetInputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    struct wma_decoder *decoder = wma_decoder_from_IMFTransform(iface);
    MF_ATTRIBUTE_TYPE item_type;
    UINT32 block_alignment;
    GUID major, subtype;
    HRESULT hr;
    ULONG i;

    TRACE("iface %p, id %lu, type %p, flags %#lx.\n", iface, id, type, flags);

    if (FAILED(hr = IMFMediaType_GetGUID(type, &MF_MT_MAJOR_TYPE, &major)) ||
        FAILED(hr = IMFMediaType_GetGUID(type, &MF_MT_SUBTYPE, &subtype)))
        return hr;

    if (!IsEqualGUID(&major, &MFMediaType_Audio))
        return MF_E_INVALIDMEDIATYPE;

    for (i = 0; i < ARRAY_SIZE(wma_decoder_inputs); ++i)
        if (IsEqualGUID(&subtype, wma_decoder_inputs[i]))
            break;
    if (i == ARRAY_SIZE(wma_decoder_inputs))
        return MF_E_INVALIDMEDIATYPE;

    if (FAILED(IMFMediaType_GetItemType(type, &MF_MT_USER_DATA, &item_type)) ||
        item_type != MF_ATTRIBUTE_BLOB)
        return MF_E_INVALIDMEDIATYPE;
    if (FAILED(IMFMediaType_GetUINT32(type, &MF_MT_AUDIO_BLOCK_ALIGNMENT, &block_alignment)))
        return MF_E_INVALIDMEDIATYPE;
    if (FAILED(IMFMediaType_GetItemType(type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &item_type)) ||
        item_type != MF_ATTRIBUTE_UINT32)
        return MF_E_INVALIDMEDIATYPE;
    if (FAILED(IMFMediaType_GetItemType(type, &MF_MT_AUDIO_NUM_CHANNELS, &item_type)) ||
        item_type != MF_ATTRIBUTE_UINT32)
        return MF_E_INVALIDMEDIATYPE;
    if (flags & MFT_SET_TYPE_TEST_ONLY)
        return S_OK;

    MoFreeMediaType(&decoder->output_type);
    memset(&decoder->output_type, 0, sizeof(decoder->output_type));
    MoFreeMediaType(&decoder->input_type);
    memset(&decoder->input_type, 0, sizeof(decoder->input_type));

    if (SUCCEEDED(hr = MFInitAMMediaTypeFromMFMediaType(type, GUID_NULL, (AM_MEDIA_TYPE *)&decoder->input_type)))
        decoder->input_buf_size = block_alignment;

    return hr;
}

static HRESULT WINAPI wma_decoder_IMFTransform_SetOutputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    struct wma_decoder *decoder = wma_decoder_from_IMFTransform(iface);
    UINT32 channel_count, block_alignment;
    MF_ATTRIBUTE_TYPE item_type;
    ULONG i, sample_size;
    GUID major, subtype;
    HRESULT hr;

    TRACE("iface %p, id %lu, type %p, flags %#lx.\n", iface, id, type, flags);

    if (IsEqualGUID(&decoder->input_type.majortype, &GUID_NULL))
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (FAILED(hr = IMFMediaType_GetGUID(type, &MF_MT_MAJOR_TYPE, &major)) ||
        FAILED(hr = IMFMediaType_GetGUID(type, &MF_MT_SUBTYPE, &subtype)))
        return hr;

    if (!IsEqualGUID(&major, &MFMediaType_Audio))
        return MF_E_INVALIDMEDIATYPE;

    for (i = 0; i < ARRAY_SIZE(wma_decoder_outputs); ++i)
        if (IsEqualGUID(&subtype, wma_decoder_outputs[i]))
            break;
    if (i == ARRAY_SIZE(wma_decoder_outputs))
        return MF_E_INVALIDMEDIATYPE;

    if (IsEqualGUID(&subtype, &MFAudioFormat_Float))
        sample_size = 32;
    else if (IsEqualGUID(&subtype, &MFAudioFormat_PCM))
        sample_size = 16;
    else
    {
        FIXME("Subtype %s not implemented!\n", debugstr_guid(&subtype));
        hr = E_NOTIMPL;
        return hr;
    }

    if (FAILED(IMFMediaType_GetItemType(type, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &item_type)) ||
        item_type != MF_ATTRIBUTE_UINT32)
        return MF_E_INVALIDMEDIATYPE;
    if (FAILED(IMFMediaType_GetItemType(type, &MF_MT_AUDIO_BITS_PER_SAMPLE, &item_type)) ||
        item_type != MF_ATTRIBUTE_UINT32)
        return MF_E_INVALIDMEDIATYPE;
    if (FAILED(IMFMediaType_GetUINT32(type, &MF_MT_AUDIO_NUM_CHANNELS, &channel_count)))
        return MF_E_INVALIDMEDIATYPE;
    if (FAILED(IMFMediaType_GetItemType(type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &item_type)) ||
        item_type != MF_ATTRIBUTE_UINT32)
        return MF_E_INVALIDMEDIATYPE;
    if (FAILED(IMFMediaType_GetUINT32(type, &MF_MT_AUDIO_BLOCK_ALIGNMENT, &block_alignment)))
        return MF_E_INVALIDMEDIATYPE;
    if (flags & MFT_SET_TYPE_TEST_ONLY)
        return S_OK;

    MoFreeMediaType(&decoder->output_type);
    memset(&decoder->output_type, 0, sizeof(decoder->output_type));

    if (SUCCEEDED(hr = MFInitAMMediaTypeFromMFMediaType(type, GUID_NULL, (AM_MEDIA_TYPE *)&decoder->output_type)))
    {
        WAVEFORMATEX *wfx = (WAVEFORMATEX *)decoder->input_type.pbFormat;
        wfx->wBitsPerSample = sample_size;
        decoder->output_buf_size = 1024 * block_alignment * channel_count;
    }

    if (FAILED(hr = wma_try_create_decoder(decoder)))
        goto failed;

    return S_OK;

failed:
    MoFreeMediaType(&decoder->output_type);
    memset(&decoder->output_type, 0, sizeof(decoder->output_type));
    return hr;
}

static HRESULT WINAPI wma_decoder_IMFTransform_GetInputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **type)
{
    FIXME("iface %p, id %lu, type %p stub!\n", iface, id, type);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_IMFTransform_GetOutputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **out)
{
    struct wma_decoder *decoder = wma_decoder_from_IMFTransform(iface);

    TRACE("iface %p, id %#lx, out %p.\n", iface, id, out);

    if (id)
        return MF_E_INVALIDSTREAMNUMBER;
    if (IsEqualGUID(&decoder->output_type.majortype, &GUID_NULL))
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    return MFCreateMediaTypeFromRepresentation(AM_MEDIA_TYPE_REPRESENTATION, &decoder->output_type, out);
}

static HRESULT WINAPI wma_decoder_IMFTransform_GetInputStatus(IMFTransform *iface, DWORD id, DWORD *flags)
{
    FIXME("iface %p, id %lu, flags %p stub!\n", iface, id, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_IMFTransform_GetOutputStatus(IMFTransform *iface, DWORD *flags)
{
    FIXME("iface %p, flags %p stub!\n", iface, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_IMFTransform_SetOutputBounds(IMFTransform *iface, LONGLONG lower, LONGLONG upper)
{
    TRACE("iface %p, lower %I64d, upper %I64d.\n", iface, lower, upper);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_IMFTransform_ProcessEvent(IMFTransform *iface, DWORD id, IMFMediaEvent *event)
{
    FIXME("iface %p, id %lu, event %p stub!\n", iface, id, event);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_IMFTransform_ProcessMessage(IMFTransform *iface, MFT_MESSAGE_TYPE message, ULONG_PTR param)
{
    struct wma_decoder *decoder = wma_decoder_from_IMFTransform(iface);

    TRACE("iface %p, message %#x, param %Ix.\n", iface, message, param);

    switch (message)
    {
    case MFT_MESSAGE_COMMAND_DRAIN:
        if (winedmo_transform_drain(decoder->winedmo_transform, FALSE)) return E_FAIL;
        return S_OK;
    case MFT_MESSAGE_COMMAND_FLUSH:
        if (winedmo_transform_drain(decoder->winedmo_transform, TRUE)) return E_FAIL;
        return S_OK;

    default:
        FIXME("Ignoring message %#x.\n", message);
        return S_OK;
    }
}

static HRESULT WINAPI wma_decoder_IMFTransform_ProcessInput(IMFTransform *iface, DWORD id, IMFSample *sample, DWORD flags)
{
    struct wma_decoder *decoder = wma_decoder_from_IMFTransform(iface);
    DMO_OUTPUT_DATA_BUFFER input = {0};
    MFT_INPUT_STREAM_INFO info;
    IMFMediaBuffer *buffer;
    DWORD total_length;
    NTSTATUS status;
    HRESULT hr;

    TRACE("iface %p, id %lu, sample %p, flags %#lx.\n", iface, id, sample, flags);

    if (!decoder->winedmo_transform.handle)
        return MF_E_TRANSFORM_TYPE_NOT_SET;
    if (decoder->input_sample)
        return MF_E_NOTACCEPTING;

    if (FAILED(hr = IMFTransform_GetInputStreamInfo(iface, 0, &info))
            || FAILED(hr = IMFSample_GetTotalLength(sample, &total_length)))
        return hr;

    /* WMA transform uses fixed size input samples and ignores samples with invalid sizes */
    if (total_length % info.cbSize)
        return S_OK;

    if (FAILED(hr = IMFSample_ConvertToContiguousBuffer(sample, &buffer)))
        return hr;
    if (SUCCEEDED(hr = MFCreateLegacyMediaBufferOnMFMediaBuffer(sample, buffer, 0, &input.pBuffer)))
    {
        if (!(status = winedmo_transform_process_input(decoder->winedmo_transform, &input)))
            IMFSample_AddRef((decoder->input_sample = sample));
        else
        {
            if (status == STATUS_PENDING) hr = MF_E_NOTACCEPTING;
            else hr = HRESULT_FROM_NT(status);
        }
        IMediaBuffer_Release(input.pBuffer);
    }
    IMFMediaBuffer_Release(buffer);

    return hr;
}

static HRESULT WINAPI wma_decoder_IMFTransform_ProcessOutput(IMFTransform *iface, DWORD flags, DWORD count,
        MFT_OUTPUT_DATA_BUFFER *samples, DWORD *samples_status)
{
    struct wma_decoder *decoder = wma_decoder_from_IMFTransform(iface);
    DMO_OUTPUT_DATA_BUFFER output = {0};
    MFT_OUTPUT_STREAM_INFO info;
    IMFMediaBuffer *buffer;
    NTSTATUS status;
    HRESULT hr;

    TRACE("iface %p, flags %#lx, count %lu, samples %p, samples_status %p.\n", iface, flags, count, samples, samples_status);

    if (count != 1)
        return E_INVALIDARG;

    if (!decoder->winedmo_transform.handle)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    *samples_status = samples->dwStatus = 0;
    if (!samples->pSample)
    {
        samples[0].dwStatus = MFT_OUTPUT_DATA_BUFFER_NO_SAMPLE;
        return MF_E_TRANSFORM_NEED_MORE_INPUT;
    }

    if (FAILED(hr = IMFTransform_GetOutputStreamInfo(iface, 0, &info)))
        return hr;

    if (FAILED(hr = IMFSample_ConvertToContiguousBuffer(samples->pSample, &buffer)))
        return hr;
    if (SUCCEEDED(hr = MFCreateLegacyMediaBufferOnMFMediaBuffer(samples->pSample, buffer, 0, &output.pBuffer)))
    {
        if ((status = winedmo_transform_process_output(decoder->winedmo_transform, &output)))
        {
            if (status == STATUS_PENDING) hr = MF_E_TRANSFORM_NEED_MORE_INPUT;
            else hr = HRESULT_FROM_NT(status);
        }
        IMediaBuffer_Release(output.pBuffer);
    }
    IMFMediaBuffer_Release(buffer);

    if (FAILED(hr))
        WARN("Failed to process output, hr %#lx\n", hr);
    else
    {
        if (output.dwStatus & DMO_OUTPUT_DATA_BUFFERF_INCOMPLETE)
            samples->dwStatus = MFT_OUTPUT_DATA_BUFFER_INCOMPLETE;
    }

    if (decoder->input_sample) IMFSample_Release(decoder->input_sample);
    decoder->input_sample = NULL;
    return hr;
}

static const IMFTransformVtbl wma_decoder_IMFTransform_vtbl =
{
    wma_decoder_IMFTransform_QueryInterface,
    wma_decoder_IMFTransform_AddRef,
    wma_decoder_IMFTransform_Release,
    wma_decoder_IMFTransform_GetStreamLimits,
    wma_decoder_IMFTransform_GetStreamCount,
    wma_decoder_IMFTransform_GetStreamIDs,
    wma_decoder_IMFTransform_GetInputStreamInfo,
    wma_decoder_IMFTransform_GetOutputStreamInfo,
    wma_decoder_IMFTransform_GetAttributes,
    wma_decoder_IMFTransform_GetInputStreamAttributes,
    wma_decoder_IMFTransform_GetOutputStreamAttributes,
    wma_decoder_IMFTransform_DeleteInputStream,
    wma_decoder_IMFTransform_AddInputStreams,
    wma_decoder_IMFTransform_GetInputAvailableType,
    wma_decoder_IMFTransform_GetOutputAvailableType,
    wma_decoder_IMFTransform_SetInputType,
    wma_decoder_IMFTransform_SetOutputType,
    wma_decoder_IMFTransform_GetInputCurrentType,
    wma_decoder_IMFTransform_GetOutputCurrentType,
    wma_decoder_IMFTransform_GetInputStatus,
    wma_decoder_IMFTransform_GetOutputStatus,
    wma_decoder_IMFTransform_SetOutputBounds,
    wma_decoder_IMFTransform_ProcessEvent,
    wma_decoder_IMFTransform_ProcessMessage,
    wma_decoder_IMFTransform_ProcessInput,
    wma_decoder_IMFTransform_ProcessOutput,
};

static inline struct wma_decoder *wma_decoder_from_IMediaObject(IMediaObject *iface)
{
    return CONTAINING_RECORD(iface, struct wma_decoder, IMediaObject_iface);
}

static HRESULT WINAPI wma_decoder_IMediaObject_QueryInterface(IMediaObject *iface, REFIID iid, void **obj)
{
    struct wma_decoder *decoder = wma_decoder_from_IMediaObject(iface);
    return IUnknown_QueryInterface(decoder->outer, iid, obj);
}

static ULONG WINAPI wma_decoder_IMediaObject_AddRef(IMediaObject *iface)
{
    struct wma_decoder *decoder = wma_decoder_from_IMediaObject(iface);
    return IUnknown_AddRef(decoder->outer);
}

static ULONG WINAPI wma_decoder_IMediaObject_Release(IMediaObject *iface)
{
    struct wma_decoder *decoder = wma_decoder_from_IMediaObject(iface);
    return IUnknown_Release(decoder->outer);
}

static HRESULT WINAPI wma_decoder_IMediaObject_GetStreamCount(IMediaObject *iface, DWORD *input, DWORD *output)
{
    FIXME("iface %p, input %p, output %p semi-stub!\n", iface, input, output);
    *input = *output = 1;
    return S_OK;
}

static HRESULT WINAPI wma_decoder_IMediaObject_GetInputStreamInfo(IMediaObject *iface, DWORD index, DWORD *flags)
{
    FIXME("iface %p, index %lu, flags %p stub!\n", iface, index, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_IMediaObject_GetOutputStreamInfo(IMediaObject *iface, DWORD index, DWORD *flags)
{
    FIXME("iface %p, index %lu, flags %p stub!\n", iface, index, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_IMediaObject_GetInputType(IMediaObject *iface, DWORD index, DWORD type_index,
        DMO_MEDIA_TYPE *type)
{
    TRACE("iface %p, index %lu, type_index %lu, type %p.\n", iface, index, type_index, type);

    if (index > 0)
        return DMO_E_INVALIDSTREAMINDEX;
    if (type_index >= ARRAY_SIZE(wma_decoder_inputs))
        return DMO_E_NO_MORE_ITEMS;
    if (!type)
        return S_OK;

    memset(type, 0, sizeof(*type));
    type->majortype = MFMediaType_Audio;
    type->subtype = *wma_decoder_inputs[type_index];
    type->bFixedSizeSamples = FALSE;
    type->bTemporalCompression = TRUE;
    type->lSampleSize = 0;

    return S_OK;
}

static HRESULT WINAPI wma_decoder_IMediaObject_GetOutputType(IMediaObject *iface, DWORD index, DWORD type_index,
        DMO_MEDIA_TYPE *type)
{
    struct wma_decoder *decoder = wma_decoder_from_IMediaObject(iface);
    UINT32 depth, channels, rate;
    IMFMediaType *media_type;
    HRESULT hr;

    TRACE("iface %p, index %lu, type_index %lu, type %p\n", iface, index, type_index, type);

    if (index > 0)
        return DMO_E_INVALIDSTREAMINDEX;
    if (type_index >= 1)
        return DMO_E_NO_MORE_ITEMS;
    if (IsEqualGUID(&decoder->input_type.majortype, &GUID_NULL))
        return DMO_E_TYPE_NOT_SET;
    if (!type)
        return S_OK;

    if (FAILED(hr = MFCreateMediaTypeFromRepresentation(AM_MEDIA_TYPE_REPRESENTATION,
            &decoder->input_type, &media_type)))
        return hr;

    if (SUCCEEDED(IMFMediaType_GetUINT32(media_type, &MF_MT_AUDIO_BITS_PER_SAMPLE, &depth))
            && depth == 32)
        hr = IMFMediaType_SetGUID(media_type, &MF_MT_SUBTYPE, &MFAudioFormat_Float);
    else
        hr = IMFMediaType_SetGUID(media_type, &MF_MT_SUBTYPE, &MFAudioFormat_PCM);

    if (SUCCEEDED(hr))
        hr = IMFMediaType_GetUINT32(media_type, &MF_MT_AUDIO_NUM_CHANNELS, &channels);
    if (SUCCEEDED(hr))
        hr = IMFMediaType_SetUINT32(media_type, &MF_MT_AUDIO_BLOCK_ALIGNMENT, depth * channels / 8);

    if (SUCCEEDED(hr))
        hr = IMFMediaType_GetUINT32(media_type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
    if (SUCCEEDED(hr))
        hr = IMFMediaType_SetUINT32(media_type, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND, depth * channels / 8 * rate);

    if (SUCCEEDED(hr))
        hr = IMFMediaType_DeleteItem(media_type, &MF_MT_USER_DATA);
    if (SUCCEEDED(hr))
        hr = MFInitAMMediaTypeFromMFMediaType(media_type, GUID_NULL, (AM_MEDIA_TYPE *)type);

    IMFMediaType_Release(media_type);
    return hr;
}

static HRESULT WINAPI wma_decoder_IMediaObject_SetInputType(IMediaObject *iface, DWORD index,
        const DMO_MEDIA_TYPE *type, DWORD flags)
{
    struct wma_decoder *decoder = wma_decoder_from_IMediaObject(iface);
    unsigned int i;

    TRACE("iface %p, index %lu, type %p, flags %#lx.\n", iface, index, type, flags);

    if (index > 0)
        return DMO_E_INVALIDSTREAMINDEX;

    if (flags & DMO_SET_TYPEF_CLEAR)
    {
        if (flags != DMO_SET_TYPEF_CLEAR)
            return E_INVALIDARG;
        MoFreeMediaType(&decoder->input_type);
        memset(&decoder->input_type, 0, sizeof(decoder->input_type));
        winedmo_transform_destroy(&decoder->winedmo_transform);
        return S_OK;
    }
    if (!type)
        return E_POINTER;
    if (flags & ~DMO_SET_TYPEF_TEST_ONLY)
        return E_INVALIDARG;

    if (!IsEqualGUID(&type->majortype, &MFMediaType_Audio))
        return DMO_E_TYPE_NOT_ACCEPTED;

    for (i = 0; i < ARRAY_SIZE(wma_decoder_inputs); ++i)
        if (IsEqualGUID(&type->subtype, wma_decoder_inputs[i]))
            break;
    if (i == ARRAY_SIZE(wma_decoder_inputs))
        return DMO_E_TYPE_NOT_ACCEPTED;

    if (flags & DMO_SET_TYPEF_TEST_ONLY)
        return S_OK;

    MoFreeMediaType(&decoder->input_type);
    memset(&decoder->input_type, 0, sizeof(decoder->input_type));
    MoCopyMediaType(&decoder->input_type, type);

    winedmo_transform_destroy(&decoder->winedmo_transform);

    return S_OK;
}

static HRESULT WINAPI wma_decoder_IMediaObject_SetOutputType(IMediaObject *iface, DWORD index,
        const DMO_MEDIA_TYPE *type, DWORD flags)
{
    struct wma_decoder *decoder = wma_decoder_from_IMediaObject(iface);
    unsigned int i;

    TRACE("iface %p, index %lu, type %p, flags %#lx,\n", iface, index, type, flags);

    if (index > 0)
        return DMO_E_INVALIDSTREAMINDEX;

    if (flags & DMO_SET_TYPEF_CLEAR)
    {
        if (flags != DMO_SET_TYPEF_CLEAR)
            return E_INVALIDARG;
        MoFreeMediaType(&decoder->output_type);
        memset(&decoder->output_type, 0, sizeof(decoder->output_type));
        winedmo_transform_destroy(&decoder->winedmo_transform);
        return S_OK;
    }
    if (!type)
        return E_POINTER;
    if (flags & ~DMO_SET_TYPEF_TEST_ONLY)
        return E_INVALIDARG;

    if (!IsEqualGUID(&type->majortype, &MFMediaType_Audio))
        return DMO_E_TYPE_NOT_ACCEPTED;

    for (i = 0; i < ARRAY_SIZE(wma_decoder_outputs); ++i)
        if (IsEqualGUID(&type->subtype, wma_decoder_outputs[i]))
            break;
    if (i == ARRAY_SIZE(wma_decoder_outputs))
        return DMO_E_TYPE_NOT_ACCEPTED;

    if (IsEqualGUID(&decoder->input_type.majortype, &GUID_NULL))
        return DMO_E_TYPE_NOT_SET;
    if (flags & DMO_SET_TYPEF_TEST_ONLY)
        return S_OK;

    MoFreeMediaType(&decoder->output_type);
    memset(&decoder->output_type, 0, sizeof(decoder->output_type));
    MoCopyMediaType(&decoder->output_type, type);

    return wma_try_create_decoder(decoder);
}

static HRESULT WINAPI wma_decoder_IMediaObject_GetInputCurrentType(IMediaObject *iface, DWORD index, DMO_MEDIA_TYPE *type)
{
    struct wma_decoder *decoder = wma_decoder_from_IMediaObject(iface);

    TRACE("iface %p, index %lu, type %p\n", iface, index, type);

    if (index)
        return DMO_E_INVALIDSTREAMINDEX;
    if (IsEqualGUID(&decoder->input_type.majortype, &GUID_NULL))
        return DMO_E_TYPE_NOT_SET;
    if (!type)
        return E_POINTER;
    return MoCopyMediaType(type, &decoder->input_type);
}

static HRESULT WINAPI wma_decoder_IMediaObject_GetOutputCurrentType(IMediaObject *iface, DWORD index, DMO_MEDIA_TYPE *type)
{
    struct wma_decoder *decoder = wma_decoder_from_IMediaObject(iface);

    TRACE("iface %p, index %lu, type %p\n", iface, index, type);

    if (index)
        return DMO_E_INVALIDSTREAMINDEX;
    if (IsEqualGUID(&decoder->output_type.majortype, &GUID_NULL))
        return DMO_E_TYPE_NOT_SET;
    if (!type)
        return E_POINTER;
    return MoCopyMediaType(type, &decoder->output_type);
}

static HRESULT WINAPI wma_decoder_IMediaObject_GetInputSizeInfo(IMediaObject *iface, DWORD index, DWORD *size,
        DWORD *lookahead, DWORD *alignment)
{
    FIXME("iface %p, index %lu, size %p, lookahead %p, alignment %p stub!\n", iface, index, size,
            lookahead, alignment);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_IMediaObject_GetOutputSizeInfo(IMediaObject *iface, DWORD index, DWORD *size, DWORD *alignment)
{
    struct wma_decoder *decoder = wma_decoder_from_IMediaObject(iface);

    TRACE("iface %p, index %lu, size %p, alignment %p.\n", iface, index, size, alignment);

    if (!size || !alignment)
        return E_POINTER;
    if (index > 0)
        return DMO_E_INVALIDSTREAMINDEX;
    if (IsEqualGUID(&decoder->output_type.majortype, &GUID_NULL))
        return DMO_E_TYPE_NOT_SET;

    *size = 8192;
    *alignment = 1;

    return S_OK;
}

static HRESULT WINAPI wma_decoder_IMediaObject_GetInputMaxLatency(IMediaObject *iface, DWORD index, REFERENCE_TIME *latency)
{
    FIXME("iface %p, index %lu, latency %p stub!\n", iface, index, latency);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_IMediaObject_SetInputMaxLatency(IMediaObject *iface, DWORD index, REFERENCE_TIME latency)
{
    FIXME("iface %p, index %lu, latency %s stub!\n", iface, index, wine_dbgstr_longlong(latency));
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_IMediaObject_Flush(IMediaObject *iface)
{
    struct wma_decoder *decoder = wma_decoder_from_IMediaObject(iface);
    NTSTATUS status;

    TRACE("iface %p.\n", iface);

    if ((status = winedmo_transform_drain(decoder->winedmo_transform, TRUE)))
        return HRESULT_FROM_NT(status);

    return S_OK;
}

static HRESULT WINAPI wma_decoder_IMediaObject_Discontinuity(IMediaObject *iface, DWORD index)
{
    TRACE("iface %p, index %lu.\n", iface, index);

    if (index > 0)
        return DMO_E_INVALIDSTREAMINDEX;

    return S_OK;
}

static HRESULT WINAPI wma_decoder_IMediaObject_AllocateStreamingResources(IMediaObject *iface)
{
    FIXME("iface %p stub!\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_IMediaObject_FreeStreamingResources(IMediaObject *iface)
{
    FIXME("iface %p stub!\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_IMediaObject_GetInputStatus(IMediaObject *iface, DWORD index, DWORD *flags)
{
    FIXME("iface %p, index %lu, flags %p stub!\n", iface, index, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_IMediaObject_ProcessInput(IMediaObject *iface, DWORD index,
        IMediaBuffer *buffer, DWORD flags, REFERENCE_TIME timestamp, REFERENCE_TIME timelength)
{
    DMO_OUTPUT_DATA_BUFFER input = {.pBuffer = buffer, .dwStatus = flags, .rtTimestamp = timestamp, .rtTimelength = timelength};
    struct wma_decoder *decoder = wma_decoder_from_IMediaObject(iface);
    NTSTATUS status;

    TRACE("iface %p, index %lu, buffer %p, flags %#lx, timestamp %s, timelength %s.\n", iface,
             index, buffer, flags, wine_dbgstr_longlong(timestamp), wine_dbgstr_longlong(timelength));

    if (!decoder->winedmo_transform.handle)
        return DMO_E_TYPE_NOT_SET;

    if ((status = winedmo_transform_process_input(decoder->winedmo_transform, &input)))
    {
        if (status == STATUS_PENDING) return DMO_E_NOTACCEPTING;
        return HRESULT_FROM_NT(status);
    }

    return S_OK;
}

static HRESULT WINAPI wma_decoder_IMediaObject_ProcessOutput(IMediaObject *iface, DWORD flags, DWORD count,
        DMO_OUTPUT_DATA_BUFFER *buffers, DWORD *buffers_status)
{
    struct wma_decoder *decoder = wma_decoder_from_IMediaObject(iface);
    NTSTATUS status;

    TRACE("iface %p, flags %#lx, count %lu, buffers %p, status %p.\n", iface, flags, count, buffers, buffers_status);

    if (!decoder->winedmo_transform.handle)
        return DMO_E_TYPE_NOT_SET;

    if ((status = winedmo_transform_process_output(decoder->winedmo_transform, buffers)))
    {
        if (status == STATUS_PENDING) return S_FALSE;
        return HRESULT_FROM_NT(S_FALSE);
    }

    return S_OK;
}

static HRESULT WINAPI wma_decoder_IMediaObject_Lock(IMediaObject *iface, LONG lock)
{
    FIXME("iface %p, lock %ld stub!\n", iface, lock);
    return E_NOTIMPL;
}

static const IMediaObjectVtbl wma_decoder_IMediaObject_vtbl =
{
    wma_decoder_IMediaObject_QueryInterface,
    wma_decoder_IMediaObject_AddRef,
    wma_decoder_IMediaObject_Release,
    wma_decoder_IMediaObject_GetStreamCount,
    wma_decoder_IMediaObject_GetInputStreamInfo,
    wma_decoder_IMediaObject_GetOutputStreamInfo,
    wma_decoder_IMediaObject_GetInputType,
    wma_decoder_IMediaObject_GetOutputType,
    wma_decoder_IMediaObject_SetInputType,
    wma_decoder_IMediaObject_SetOutputType,
    wma_decoder_IMediaObject_GetInputCurrentType,
    wma_decoder_IMediaObject_GetOutputCurrentType,
    wma_decoder_IMediaObject_GetInputSizeInfo,
    wma_decoder_IMediaObject_GetOutputSizeInfo,
    wma_decoder_IMediaObject_GetInputMaxLatency,
    wma_decoder_IMediaObject_SetInputMaxLatency,
    wma_decoder_IMediaObject_Flush,
    wma_decoder_IMediaObject_Discontinuity,
    wma_decoder_IMediaObject_AllocateStreamingResources,
    wma_decoder_IMediaObject_FreeStreamingResources,
    wma_decoder_IMediaObject_GetInputStatus,
    wma_decoder_IMediaObject_ProcessInput,
    wma_decoder_IMediaObject_ProcessOutput,
    wma_decoder_IMediaObject_Lock,
};

static inline struct wma_decoder *wma_decoder_from_IPropertyBag(IPropertyBag *iface)
{
    return CONTAINING_RECORD(iface, struct wma_decoder, IPropertyBag_iface);
}

static HRESULT WINAPI wma_decoder_IPropertyBag_QueryInterface(IPropertyBag *iface, REFIID iid, void **out)
{
    struct wma_decoder *filter = wma_decoder_from_IPropertyBag(iface);
    return IUnknown_QueryInterface(filter->outer, iid, out);
}

static ULONG WINAPI wma_decoder_IPropertyBag_AddRef(IPropertyBag *iface)
{
    struct wma_decoder *filter = wma_decoder_from_IPropertyBag(iface);
    return IUnknown_AddRef(filter->outer);
}

static ULONG WINAPI wma_decoder_IPropertyBag_Release(IPropertyBag *iface)
{
    struct wma_decoder *filter = wma_decoder_from_IPropertyBag(iface);
    return IUnknown_Release(filter->outer);
}

static HRESULT WINAPI wma_decoder_IPropertyBag_Read(IPropertyBag *iface, const WCHAR *prop_name, VARIANT *value,
        IErrorLog *error_log)
{
    FIXME("iface %p, prop_name %s, value %p, error_log %p stub!\n", iface, debugstr_w(prop_name), value, error_log);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_IPropertyBag_Write(IPropertyBag *iface, const WCHAR *prop_name, VARIANT *value)
{
    FIXME("iface %p, prop_name %s, value %p stub!\n", iface, debugstr_w(prop_name), value);
    return S_OK;
}

static const IPropertyBagVtbl wma_decoder_IPropertyBag_vtbl =
{
    wma_decoder_IPropertyBag_QueryInterface,
    wma_decoder_IPropertyBag_AddRef,
    wma_decoder_IPropertyBag_Release,
    wma_decoder_IPropertyBag_Read,
    wma_decoder_IPropertyBag_Write,
};

static HRESULT WINAPI wma_decoder_factory_CreateInstance(IClassFactory *iface, IUnknown *outer,
        REFIID riid, void **out)
{
    struct wma_decoder *decoder;
    NTSTATUS status;
    HRESULT hr;

    TRACE("%p, %s, %p.\n", outer, debugstr_guid(riid), out);

    if ((status = winedmo_transform_check(MFMediaType_Audio, MFAudioFormat_WMAudioV8, MFAudioFormat_Float)))
    {
        WARN("Unsupported winedmo transform, status %#lx.\n", status);
        return E_NOTIMPL;
    }

    *out = NULL;
    if (outer && !IsEqualGUID(riid, &IID_IUnknown))
        return E_NOINTERFACE;
    if (!(decoder = calloc(1, sizeof(*decoder))))
        return E_OUTOFMEMORY;

    decoder->IUnknown_iface.lpVtbl = &wma_decoder_vtbl;
    decoder->IMFTransform_iface.lpVtbl = &wma_decoder_IMFTransform_vtbl;
    decoder->IMediaObject_iface.lpVtbl = &wma_decoder_IMediaObject_vtbl;
    decoder->IPropertyBag_iface.lpVtbl = &wma_decoder_IPropertyBag_vtbl;
    decoder->refcount = 1;
    decoder->outer = outer ? outer : &decoder->IUnknown_iface;
    TRACE("Created %p\n", decoder);

    hr = IUnknown_QueryInterface(&decoder->IUnknown_iface, riid, out);
    IUnknown_Release(&decoder->IUnknown_iface);
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

static const IClassFactoryVtbl wma_decoder_factory_vtbl =
{
    class_factory_QueryInterface,
    class_factory_AddRef,
    class_factory_Release,
    wma_decoder_factory_CreateInstance,
    class_factory_LockServer,
};

static IClassFactory wma_decoder_factory = {&wma_decoder_factory_vtbl};

/***********************************************************************
 *              DllGetClassObject (wmadmod.@)
 */
HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void **out)
{
    if (IsEqualGUID(clsid, &CLSID_WMADecMediaObject))
        return IClassFactory_QueryInterface(&wma_decoder_factory, riid, out);

    *out = NULL;
    FIXME("Unknown clsid %s.\n", debugstr_guid(clsid));
    return CLASS_E_CLASSNOTAVAILABLE;
}

/***********************************************************************
 *              DllRegisterServer (wmadmod.@)
 */
HRESULT WINAPI DllRegisterServer(void)
{
    MFT_REGISTER_TYPE_INFO wma_decoder_mft_inputs[] =
    {
        {MFMediaType_Audio, MEDIASUBTYPE_MSAUDIO1},
        {MFMediaType_Audio, MFAudioFormat_WMAudioV8},
        {MFMediaType_Audio, MFAudioFormat_WMAudioV9},
        {MFMediaType_Audio, MFAudioFormat_WMAudio_Lossless},
    };
    MFT_REGISTER_TYPE_INFO wma_decoder_mft_outputs[] =
    {
        {MFMediaType_Audio, MFAudioFormat_PCM},
        {MFMediaType_Audio, MFAudioFormat_Float},
    };
    DMO_PARTIAL_MEDIATYPE wma_decoder_dmo_outputs[] =
    {
        {.type = MEDIATYPE_Audio, .subtype = MEDIASUBTYPE_PCM},
        {.type = MEDIATYPE_Audio, .subtype = MEDIASUBTYPE_IEEE_FLOAT},
    };
    DMO_PARTIAL_MEDIATYPE wma_decoder_dmo_inputs[] =
    {
        {.type = MEDIATYPE_Audio, .subtype = MEDIASUBTYPE_MSAUDIO1},
        {.type = MEDIATYPE_Audio, .subtype = MEDIASUBTYPE_WMAUDIO2},
        {.type = MEDIATYPE_Audio, .subtype = MEDIASUBTYPE_WMAUDIO3},
        {.type = MEDIATYPE_Audio, .subtype = MEDIASUBTYPE_WMAUDIO_LOSSLESS},
    };
    HRESULT hr;

    TRACE("\n");

    if (FAILED(hr = __wine_register_resources()))
        return hr;
    if (FAILED(hr = MFTRegister(CLSID_WMADecMediaObject, MFT_CATEGORY_AUDIO_DECODER,
            (WCHAR *)L"WMAudio Decoder MFT", MFT_ENUM_FLAG_SYNCMFT,
            ARRAY_SIZE(wma_decoder_mft_inputs), wma_decoder_mft_inputs,
            ARRAY_SIZE(wma_decoder_mft_outputs), wma_decoder_mft_outputs, NULL)))
        return hr;
    if (FAILED(hr = DMORegister(L"WMAudio Decoder DMO", &CLSID_WMADecMediaObject, &DMOCATEGORY_AUDIO_DECODER, 0,
            ARRAY_SIZE(wma_decoder_dmo_inputs), wma_decoder_dmo_inputs,
            ARRAY_SIZE(wma_decoder_dmo_outputs), wma_decoder_dmo_outputs)))
        return hr;

    return S_OK;
}

/***********************************************************************
 *              DllUnregisterServer (wmadmod.@)
 */
HRESULT WINAPI DllUnregisterServer(void)
{
    HRESULT hr;

    TRACE("\n");

    if (FAILED(hr = __wine_unregister_resources()))
        return hr;
    if (FAILED(hr = MFTUnregister(CLSID_WMADecMediaObject)))
        return hr;
    if (FAILED(hr = DMOUnregister(&CLSID_WMADecMediaObject, &DMOCATEGORY_AUDIO_DECODER)))
        return hr;

    return S_OK;
}
