/* MP3 Decoder Transform
 *
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
#include <stdio.h>
#include <mpg123.h>

#include "gst_private.h"

#include "mfapi.h"
#include "mferror.h"
#include "mfobjects.h"
#include "mftransform.h"
#include "wmcodecdsp.h"
#include "dmo.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(mfplat);
WINE_DECLARE_DEBUG_CHANNEL(winediag);

static const GUID *const mp3_decoder_input_types[] =
{
    &MFAudioFormat_MP3,
};
static const GUID *const mp3_decoder_output_types[] =
{
    &MFAudioFormat_PCM,
};

struct mp3_decoder
{
    IUnknown IUnknown_inner;
    IMFTransform IMFTransform_iface;
    IMediaObject IMediaObject_iface;
    IPropertyStore IPropertyStore_iface;
    IUnknown *outer;
    LONG refcount;

    IMFMediaType *input_type;
    IMFMediaType *output_type;

    struct wg_format input_format, output_format;
    struct wg_transform *wg_transform;
    struct wg_sample_queue *wg_sample_queue;
};

static struct mp3_decoder *impl_from_IMFTransform(IMFTransform *iface)
{
    return CONTAINING_RECORD(iface, struct mp3_decoder, IMFTransform_iface);
}

static HRESULT try_create_wg_transform(struct mp3_decoder *impl)
{
    struct wg_format input_format, output_format;

    if (impl->wg_transform)
        wg_transform_destroy(impl->wg_transform);
    impl->wg_transform = NULL;

    mf_media_type_to_wg_format(impl->input_type, &input_format);
    if (input_format.major_type == WG_MAJOR_TYPE_UNKNOWN)
        return MF_E_INVALIDMEDIATYPE;

    mf_media_type_to_wg_format(impl->output_type, &output_format);
    if (output_format.major_type == WG_MAJOR_TYPE_UNKNOWN)
        return MF_E_INVALIDMEDIATYPE;

    if (!(impl->wg_transform = wg_transform_create(&input_format, &output_format)))
        return E_FAIL;

    return S_OK;
}

static inline struct mp3_decoder *impl_from_IUnknown(IUnknown *iface)
{
    return CONTAINING_RECORD(iface, struct mp3_decoder, IUnknown_inner);
}

static HRESULT WINAPI unknown_QueryInterface(IUnknown *iface, REFIID iid, void **out)
{
    struct mp3_decoder *impl = impl_from_IUnknown(iface);

    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown))
        *out = &impl->IUnknown_inner;
    else if (IsEqualGUID(iid, &IID_IMFTransform))
        *out = &impl->IMFTransform_iface;
    else if (IsEqualGUID(iid, &IID_IMediaObject))
        *out = &impl->IMediaObject_iface;
    else if (IsEqualIID(iid, &IID_IPropertyStore))
        *out = &impl->IPropertyStore_iface;
    else
    {
        *out = NULL;
        WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static ULONG WINAPI unknown_AddRef(IUnknown *iface)
{
    struct mp3_decoder *impl = impl_from_IUnknown(iface);
    ULONG refcount = InterlockedIncrement(&impl->refcount);

    TRACE("iface %p increasing refcount to %lu.\n", iface, refcount);

    return refcount;
}

static ULONG WINAPI unknown_Release(IUnknown *iface)
{
    struct mp3_decoder *impl = impl_from_IUnknown(iface);
    ULONG refcount = InterlockedDecrement(&impl->refcount);

    TRACE("iface %p decreasing refcount to %lu.\n", iface, refcount);

    if (!refcount)
    {
        if (impl->wg_transform)
            wg_transform_destroy(impl->wg_transform);
        if (impl->input_type)
            IMFMediaType_Release(impl->input_type);
        if (impl->output_type)
            IMFMediaType_Release(impl->output_type);

        wg_sample_queue_destroy(impl->wg_sample_queue);
        free(impl);
    }

    return refcount;
}

static const IUnknownVtbl unknown_vtbl =
{
    unknown_QueryInterface,
    unknown_AddRef,
    unknown_Release,
};

static HRESULT WINAPI transform_QueryInterface(IMFTransform *iface, REFIID iid, void **out)
{
    struct mp3_decoder *impl = impl_from_IMFTransform(iface);

    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown))
        *out = &impl->IUnknown_inner;
    else if (IsEqualGUID(iid, &IID_IMFTransform))
        *out = &impl->IMFTransform_iface;
    else if (IsEqualGUID(iid, &IID_IMediaObject))
        *out = &impl->IMediaObject_iface;
    else if (IsEqualIID(iid, &IID_IPropertyStore))
        *out = &impl->IPropertyStore_iface;
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
    struct mp3_decoder *impl = impl_from_IMFTransform(iface);
    ULONG refcount = InterlockedIncrement(&impl->refcount);

    TRACE("iface %p increasing refcount to %lu.\n", impl, refcount);

    return refcount;
}

static ULONG WINAPI transform_Release(IMFTransform *iface)
{
    struct mp3_decoder *impl = impl_from_IMFTransform(iface);
    ULONG refcount = InterlockedDecrement(&impl->refcount);

    TRACE("iface %p decreasing refcount to %lu.\n", impl, refcount);

    if (!refcount)
    {
        if (impl->wg_transform)
            wg_transform_destroy(impl->wg_transform);
        if (impl->input_type)
            IMFMediaType_Release(impl->input_type);
        if (impl->output_type)
            IMFMediaType_Release(impl->output_type);
        wg_sample_queue_destroy(impl->wg_sample_queue);
        free(impl);
    }

    return refcount;
}

static HRESULT WINAPI transform_GetStreamLimits(IMFTransform *iface, DWORD *input_minimum,
        DWORD *input_maximum, DWORD *output_minimum, DWORD *output_maximum)
{
    TRACE("iface %p, input_minimum %p, input_maximum %p, output_minimum %p, output_maximum %p.\n",
            iface, input_minimum, input_maximum, output_minimum, output_maximum);

    if (input_minimum)
        *input_minimum = 1;
    if (input_maximum)
        *input_maximum = 1;
    if (output_minimum)
        *output_minimum = 1;
    if (output_maximum)
        *output_maximum = 1;

    return S_OK;
}

static HRESULT WINAPI transform_GetStreamCount(IMFTransform *iface, DWORD *inputs, DWORD *outputs)
{
    TRACE("iface %p, inputs %p, outputs %p.\n", iface, inputs, outputs);

    if (inputs)
        *inputs = 1;
    if (outputs)
        *outputs = 1;

    return S_OK;
}

static HRESULT WINAPI transform_GetStreamIDs(IMFTransform *iface, DWORD input_size, DWORD *inputs,
        DWORD output_size, DWORD *outputs)
{
    TRACE("iface %p, input_size %lu, inputs %p, output_size %lu, outputs %p.\n", iface,
            input_size, inputs, output_size, outputs);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_GetInputStreamInfo(IMFTransform *iface, DWORD id, MFT_INPUT_STREAM_INFO *info)
{
    struct mp3_decoder *impl = impl_from_IMFTransform(iface);
    UINT32 block_alignment;
    HRESULT hr;

    TRACE("iface %p, id %#lx, info %p.\n", iface, id, info);

    if (!impl->input_type || !impl->output_type)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (FAILED(hr = IMFMediaType_GetUINT32(impl->input_type, &MF_MT_AUDIO_BLOCK_ALIGNMENT, &block_alignment)))
        return hr;

    info->hnsMaxLatency = 0;
    info->dwFlags = MFT_INPUT_STREAM_WHOLE_SAMPLES|MFT_INPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER
            |MFT_INPUT_STREAM_FIXED_SAMPLE_SIZE|MFT_INPUT_STREAM_HOLDS_BUFFERS;
    info->cbSize = 0;
    info->cbMaxLookahead = 0;
    info->cbAlignment = 0;

    return S_OK;
}

static HRESULT WINAPI transform_GetOutputStreamInfo(IMFTransform *iface, DWORD id, MFT_OUTPUT_STREAM_INFO *info)
{
    struct mp3_decoder *impl = impl_from_IMFTransform(iface);
    UINT32 channel_count, block_alignment;
    HRESULT hr;

    TRACE("iface %p, id %#lx, info %p.\n", iface, id, info);

    if (!impl->input_type || !impl->output_type)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (FAILED(hr = IMFMediaType_GetUINT32(impl->output_type, &MF_MT_AUDIO_NUM_CHANNELS, &channel_count)))
        return hr;
    if (FAILED(hr = IMFMediaType_GetUINT32(impl->output_type, &MF_MT_AUDIO_BLOCK_ALIGNMENT, &block_alignment)))
        return hr;

    info->dwFlags = 0;
    info->cbSize = 0x1800 * block_alignment * channel_count;
    info->cbAlignment = 0;

    return S_OK;
}

static HRESULT WINAPI transform_GetAttributes(IMFTransform *iface, IMFAttributes **attributes)
{
    FIXME("iface %p, attributes %p stub!\n", iface, attributes);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_GetInputStreamAttributes(IMFTransform *iface, DWORD id,
        IMFAttributes **attributes)
{
    FIXME("iface %p, id %#lx, attributes %p stub!\n", iface, id, attributes);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_GetOutputStreamAttributes(IMFTransform *iface, DWORD id,
        IMFAttributes **attributes)
{
    FIXME("iface %p, id %#lx, attributes %p stub!\n", iface, id, attributes);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_DeleteInputStream(IMFTransform *iface, DWORD id)
{
    TRACE("iface %p, id %#lx.\n", iface, id);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_AddInputStreams(IMFTransform *iface, DWORD streams, DWORD *ids)
{
    TRACE("iface %p, streams %lu, ids %p.\n", iface, streams, ids);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_GetInputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    IMFMediaType *media_type;
    const GUID *output_type;
    HRESULT hr;

    TRACE("iface %p, id %#lx, index %#lx, type %p.\n", iface, id, index, type);

    *type = NULL;

    if (index >= ARRAY_SIZE(mp3_decoder_input_types))
        return MF_E_NO_MORE_TYPES;
    index = ARRAY_SIZE(mp3_decoder_input_types) - index - 1;
    output_type = mp3_decoder_input_types[index];

    if (FAILED(hr = MFCreateMediaType(&media_type)))
        return hr;

    if (FAILED(hr = IMFMediaType_SetGUID(media_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio)))
        goto done;
    if (FAILED(hr = IMFMediaType_SetGUID(media_type, &MF_MT_SUBTYPE, output_type)))
        goto done;

    if (FAILED(hr = IMFMediaType_SetUINT32(media_type, &MF_MT_ALL_SAMPLES_INDEPENDENT, 1)))
        goto done;

done:
    if (SUCCEEDED(hr))
        IMFMediaType_AddRef((*type = media_type));

    IMFMediaType_Release(media_type);
    return hr;
}

static HRESULT WINAPI transform_GetOutputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    UINT32 channel_count, sample_size, sample_rate, block_alignment;
    struct mp3_decoder *impl = impl_from_IMFTransform(iface);
    IMFMediaType *media_type;
    const GUID *output_type;
    HRESULT hr;

    TRACE("iface %p, id %#lx, index %#lx, type %p.\n", iface, id, index, type);

    if (!impl->input_type)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    *type = NULL;

    if (index >= ARRAY_SIZE(mp3_decoder_output_types))
        return MF_E_NO_MORE_TYPES;
    index = ARRAY_SIZE(mp3_decoder_output_types) - index - 1;
    output_type = mp3_decoder_output_types[index];

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

    if (FAILED(hr = IMFMediaType_SetUINT32(media_type, &MF_MT_AUDIO_BITS_PER_SAMPLE, sample_size)))
        goto done;

    if (FAILED(hr = IMFMediaType_GetUINT32(impl->input_type, &MF_MT_AUDIO_NUM_CHANNELS, &channel_count)))
        goto done;
    if (FAILED(hr = IMFMediaType_SetUINT32(media_type, &MF_MT_AUDIO_NUM_CHANNELS, channel_count)))
        goto done;

    if (FAILED(hr = IMFMediaType_GetUINT32(impl->input_type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &sample_rate)))
        goto done;
    if (FAILED(hr = IMFMediaType_SetUINT32(media_type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, sample_rate)))
        goto done;

    block_alignment = sample_size * channel_count / 8;
    if (FAILED(hr = IMFMediaType_SetUINT32(media_type, &MF_MT_AUDIO_BLOCK_ALIGNMENT, block_alignment)))
        goto done;
    if (FAILED(hr = IMFMediaType_SetUINT32(media_type, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND, sample_rate * block_alignment)))
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

static HRESULT WINAPI transform_SetInputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    struct mp3_decoder *impl = impl_from_IMFTransform(iface);
    MF_ATTRIBUTE_TYPE item_type;
    GUID major, subtype;
    HRESULT hr;
    ULONG i;

    TRACE("iface %p, id %#lx, type %p, flags %#lx.\n", iface, id, type, flags);

    if (FAILED(hr = IMFMediaType_GetGUID(type, &MF_MT_MAJOR_TYPE, &major)) ||
        FAILED(hr = IMFMediaType_GetGUID(type, &MF_MT_SUBTYPE, &subtype)))
        return hr;

    if (!IsEqualGUID(&major, &MFMediaType_Audio))
        return MF_E_INVALIDMEDIATYPE;

    for (i = 0; i < ARRAY_SIZE(mp3_decoder_input_types); ++i)
        if (IsEqualGUID(&subtype, mp3_decoder_input_types[i]))
            break;
    if (i == ARRAY_SIZE(mp3_decoder_input_types))
        return MF_E_INVALIDMEDIATYPE;

    if (FAILED(IMFMediaType_GetItemType(type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &item_type)) ||
        item_type != MF_ATTRIBUTE_UINT32)
        return MF_E_INVALIDMEDIATYPE;
    if (FAILED(IMFMediaType_GetItemType(type, &MF_MT_AUDIO_NUM_CHANNELS, &item_type)) ||
        item_type != MF_ATTRIBUTE_UINT32)
        return MF_E_INVALIDMEDIATYPE;

    if (!impl->input_type && FAILED(hr = MFCreateMediaType(&impl->input_type)))
        return hr;

    if (impl->output_type)
    {
        IMFMediaType_Release(impl->output_type);
        impl->output_type = NULL;
    }

    return IMFMediaType_CopyAllItems(type, (IMFAttributes *)impl->input_type);
}

static HRESULT WINAPI transform_SetOutputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    struct mp3_decoder *impl = impl_from_IMFTransform(iface);
    MF_ATTRIBUTE_TYPE item_type;
    ULONG i, sample_size;
    GUID major, subtype;
    HRESULT hr;

    TRACE("iface %p, id %#lx, type %p, flags %#lx.\n", iface, id, type, flags);

    if (FAILED(hr = IMFMediaType_GetGUID(type, &MF_MT_MAJOR_TYPE, &major)) ||
        FAILED(hr = IMFMediaType_GetGUID(type, &MF_MT_SUBTYPE, &subtype)))
        return hr;

    if (!IsEqualGUID(&major, &MFMediaType_Audio))
        return MF_E_INVALIDMEDIATYPE;

    for (i = 0; i < ARRAY_SIZE(mp3_decoder_output_types); ++i)
        if (IsEqualGUID(&subtype, mp3_decoder_output_types[i]))
            break;
    if (i == ARRAY_SIZE(mp3_decoder_output_types))
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

    if (FAILED(IMFMediaType_SetUINT32(impl->input_type, &MF_MT_AUDIO_BITS_PER_SAMPLE, sample_size)))
        return MF_E_INVALIDMEDIATYPE;

    if (FAILED(IMFMediaType_GetItemType(type, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &item_type)) ||
        item_type != MF_ATTRIBUTE_UINT32)
        return MF_E_INVALIDMEDIATYPE;
    if (FAILED(IMFMediaType_GetItemType(type, &MF_MT_AUDIO_BITS_PER_SAMPLE, &item_type)) ||
        item_type != MF_ATTRIBUTE_UINT32)
        return MF_E_INVALIDMEDIATYPE;
    if (FAILED(IMFMediaType_GetItemType(type, &MF_MT_AUDIO_NUM_CHANNELS, &item_type)) ||
        item_type != MF_ATTRIBUTE_UINT32)
        return MF_E_INVALIDMEDIATYPE;
    if (FAILED(IMFMediaType_GetItemType(type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &item_type)) ||
        item_type != MF_ATTRIBUTE_UINT32)
        return MF_E_INVALIDMEDIATYPE;
    if (FAILED(IMFMediaType_GetItemType(type, &MF_MT_AUDIO_BLOCK_ALIGNMENT, &item_type)) ||
        item_type != MF_ATTRIBUTE_UINT32)
        return MF_E_INVALIDMEDIATYPE;

    if (!impl->output_type && FAILED(hr = MFCreateMediaType(&impl->output_type)))
        return hr;

    if (FAILED(hr = IMFMediaType_CopyAllItems(type, (IMFAttributes *)impl->output_type)))
        return hr;

    if (FAILED(hr = try_create_wg_transform(impl)))
        goto failed;

    return S_OK;

failed:
    IMFMediaType_Release(impl->output_type);
    impl->output_type = NULL;
    return hr;
}

static HRESULT WINAPI transform_GetInputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **type)
{
    struct mp3_decoder *impl = impl_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, id %#lx, type %p.\n", iface, id, type);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (!impl->input_type)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (FAILED(hr = MFCreateMediaType(type)))
        return hr;

    return IMFMediaType_CopyAllItems(impl->input_type, (IMFAttributes *)*type);
}

static HRESULT WINAPI transform_GetOutputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **type)
{
    struct mp3_decoder *impl = impl_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, id %#lx, type %p.\n", iface, id, type);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (!impl->output_type)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (FAILED(hr = MFCreateMediaType(type)))
        return hr;

    return IMFMediaType_CopyAllItems(impl->output_type, (IMFAttributes *)*type);
}

static HRESULT WINAPI transform_GetInputStatus(IMFTransform *iface, DWORD id, DWORD *flags)
{
    FIXME("iface %p, id %#lx, flags %p stub!\n", iface, id, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_GetOutputStatus(IMFTransform *iface, DWORD *flags)
{
    FIXME("iface %p, flags %p stub!\n", iface, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_SetOutputBounds(IMFTransform *iface, LONGLONG lower, LONGLONG upper)
{
    TRACE("iface %p, lower %I64d, upper %I64d.\n", iface, lower, upper);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_ProcessEvent(IMFTransform *iface, DWORD id, IMFMediaEvent *event)
{
    FIXME("iface %p, id %#lx, event %p stub!\n", iface, id, event);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_ProcessMessage(IMFTransform *iface, MFT_MESSAGE_TYPE message, ULONG_PTR param)
{
    FIXME("iface %p, message %#x, param %p stub!\n", iface, message, (void *)param);
    return S_OK;
}

static HRESULT WINAPI transform_ProcessInput(IMFTransform *iface, DWORD id, IMFSample *sample, DWORD flags)
{
    struct mp3_decoder *impl = impl_from_IMFTransform(iface);

    TRACE("iface %p, id %#lx, sample %p, flags %#lx.\n", iface, id, sample, flags);

    return wg_transform_push_mf(impl->wg_transform, sample, impl->wg_sample_queue);
}

static HRESULT WINAPI transform_ProcessOutput(IMFTransform *iface, DWORD flags, DWORD count,
        MFT_OUTPUT_DATA_BUFFER *samples, DWORD *status)
{
    struct mp3_decoder *impl = impl_from_IMFTransform(iface);
    MFT_OUTPUT_STREAM_INFO info;
    HRESULT hr;

    TRACE("iface %p, flags %#lx, count %lu, samples %p, status %p.\n", iface, flags, count, samples, status);

    if (count != 1)
        return E_INVALIDARG;

    if (FAILED(hr = IMFTransform_GetOutputStreamInfo(iface, 0, &info)))
        return hr;

    *status = 0;

    if (SUCCEEDED(hr = wg_transform_read_mf(impl->wg_transform, samples[0].pSample,
            info.cbSize, NULL, &samples[0].dwStatus)))
        wg_sample_queue_flush(impl->wg_sample_queue, false);

    return hr;
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

static inline struct mp3_decoder *impl_from_IMediaObject(IMediaObject *iface)
{
    return CONTAINING_RECORD(iface, struct mp3_decoder, IMediaObject_iface);
}

static HRESULT WINAPI media_object_QueryInterface(IMediaObject *iface, REFIID iid, void **obj)
{
    struct mp3_decoder *impl = impl_from_IMediaObject(iface);
    return IUnknown_QueryInterface(impl->outer, iid, obj);
}

static ULONG WINAPI media_object_AddRef(IMediaObject *iface)
{
    struct mp3_decoder *impl = impl_from_IMediaObject(iface);
    return IUnknown_AddRef(impl->outer);
}

static ULONG WINAPI media_object_Release(IMediaObject *iface)
{
    struct mp3_decoder *impl = impl_from_IMediaObject(iface);
    return IUnknown_Release(impl->outer);
}

static HRESULT WINAPI media_object_GetStreamCount(IMediaObject *iface, DWORD *input, DWORD *output)
{
    TRACE("iface %p, input %p, output %p.\n", iface, input, output);
    *input = *output = 1;
    return S_OK;
}

static HRESULT WINAPI media_object_GetInputStreamInfo(IMediaObject *iface, DWORD index, DWORD *flags)
{
    TRACE("iface %p, index %lu, flags %p.\n", iface, index, flags);
    *flags = 0;
    return S_OK;
}

static HRESULT WINAPI media_object_GetOutputStreamInfo(IMediaObject *iface, DWORD index, DWORD *flags)
{
    TRACE("iface %p, index %lu, flags %p.\n", iface, index, flags);
    *flags = 0;
    return S_OK;
}

static HRESULT WINAPI media_object_GetInputType(IMediaObject *iface, DWORD index, DWORD type_index,
        DMO_MEDIA_TYPE *type)
{
    struct mp3_decoder *impl = impl_from_IMediaObject(iface);
    struct wg_format format = impl->input_format;

    FIXME("iface %p, index %lu, type_index %lu, type %p semi-stub!\n", iface, index, type_index, type);

    if (type_index >= 5)
        return VFW_E_NO_TYPES;

    format.major_type = WG_MAJOR_TYPE_AUDIO_MPEG1;
    format.u.audio_mpeg1.layer = 3;

    if (!amt_from_wg_format((AM_MEDIA_TYPE *)type, &format, false))
        return VFW_E_NO_TYPES;

    return S_OK;
}

static HRESULT WINAPI media_object_GetOutputType(IMediaObject *iface, DWORD index, DWORD type_index,
        DMO_MEDIA_TYPE *type)
{
    static enum wg_audio_format const audio_formats[] =
    {
        WG_AUDIO_FORMAT_S16LE,
    };

    struct mp3_decoder *impl = impl_from_IMediaObject(iface);
    struct wg_format format = impl->output_format;

    FIXME("iface %p, index %lu, type_index %lu, type %p semi-stub!\n", iface, index, type_index, type);

    if (type_index >= ARRAY_SIZE(audio_formats))
        return VFW_E_NO_TYPES;

    format.major_type = WG_MAJOR_TYPE_AUDIO;
    format.u.audio.format = audio_formats[index];
    if (!format.u.audio.channels)
        format.u.audio.channels = impl->input_format.u.audio_mpeg1.channels;
    if (!format.u.audio.rate)
        format.u.audio.rate = impl->input_format.u.audio_mpeg1.rate;

    if (!amt_from_wg_format((AM_MEDIA_TYPE *)type, &format, false))
        return VFW_E_NO_TYPES;

    return S_OK;
}

static HRESULT WINAPI media_object_SetInputType(IMediaObject *iface, DWORD index,
        const DMO_MEDIA_TYPE *type, DWORD flags)
{
    struct mp3_decoder *impl = impl_from_IMediaObject(iface);
    struct wg_format wg_format;

    FIXME("iface %p, index %lu, type %p, flags %#lx semi-stub!\n", iface, index, type, flags);

    if (flags & DMO_SET_TYPEF_CLEAR)
    {
        memset(&impl->input_format, 0, sizeof(impl->input_format));
        return S_OK;
    }

    if (!amt_to_wg_format((const AM_MEDIA_TYPE *)type, &wg_format))
        return VFW_E_INVALIDMEDIATYPE;

    if (wg_format.major_type != WG_MAJOR_TYPE_AUDIO_MPEG1
            || wg_format.u.audio_mpeg1.layer != 3)
        return VFW_E_INVALIDMEDIATYPE;
    if (!(flags & DMO_SET_TYPEF_TEST_ONLY))
    {
        impl->input_format = wg_format;
        if (!impl->output_format.major_type)
            return S_OK;

        if (impl->wg_transform)
            wg_transform_destroy(impl->wg_transform);
        impl->wg_transform = NULL;

        if (!(impl->wg_transform = wg_transform_create(&impl->input_format, &impl->output_format)))
            return E_FAIL;
    }

    return S_OK;
}

static HRESULT WINAPI media_object_SetOutputType(IMediaObject *iface, DWORD index,
        const DMO_MEDIA_TYPE *type, DWORD flags)
{
    struct mp3_decoder *impl = impl_from_IMediaObject(iface);
    struct wg_format wg_format;

    FIXME("iface %p, index %lu, type %p, flags %#lx semi-stub!\n", iface, index, type, flags);

    if (flags & DMO_SET_TYPEF_CLEAR)
    {
        memset(&impl->output_format, 0, sizeof(impl->output_format));
        return S_OK;
    }

    if (!amt_to_wg_format((const AM_MEDIA_TYPE *)type, &wg_format))
        return VFW_E_INVALIDMEDIATYPE;

    if (wg_format.major_type != WG_MAJOR_TYPE_AUDIO)
        return VFW_E_INVALIDMEDIATYPE;
    if (!(flags & DMO_SET_TYPEF_TEST_ONLY))
    {
        impl->output_format = wg_format;
        if (!impl->input_format.major_type)
            return S_OK;

        if (impl->wg_transform)
            wg_transform_destroy(impl->wg_transform);
        impl->wg_transform = NULL;

        if (!(impl->wg_transform = wg_transform_create(&impl->input_format, &impl->output_format)))
            return E_FAIL;
    }

    return S_OK;
}

static HRESULT WINAPI media_object_GetInputCurrentType(IMediaObject *iface, DWORD index, DMO_MEDIA_TYPE *type)
{
    FIXME("iface %p, index %lu, type %p stub!\n", iface, index, type);
    return E_NOTIMPL;
}

static HRESULT WINAPI media_object_GetOutputCurrentType(IMediaObject *iface, DWORD index, DMO_MEDIA_TYPE *type)
{
    FIXME("iface %p, index %lu, type %p stub!\n", iface, index, type);
    return E_NOTIMPL;
}

static HRESULT WINAPI media_object_GetInputSizeInfo(IMediaObject *iface, DWORD index, DWORD *size,
        DWORD *lookahead, DWORD *alignment)
{
    TRACE("iface %p, index %lu, size %p, lookahead %p, alignment %p.\n", iface, index, size,
            lookahead, alignment);
    *size = 0;
    *alignment = 1;
    return S_OK;
}

static HRESULT WINAPI media_object_GetOutputSizeInfo(IMediaObject *iface, DWORD index, DWORD *size, DWORD *alignment)
{
    TRACE("iface %p, index %lu, size %p, alignment %p semi-stub!\n", iface, index, size, alignment);
    *size = 2 * 1152 * 16;
    *alignment = 1;
    return S_OK;
}

static HRESULT WINAPI media_object_GetInputMaxLatency(IMediaObject *iface, DWORD index, REFERENCE_TIME *latency)
{
    FIXME("iface %p, index %lu, latency %p stub!\n", iface, index, latency);
    return E_NOTIMPL;
}

static HRESULT WINAPI media_object_SetInputMaxLatency(IMediaObject *iface, DWORD index, REFERENCE_TIME latency)
{
    FIXME("iface %p, index %lu, latency %s stub!\n", iface, index, wine_dbgstr_longlong(latency));
    return E_NOTIMPL;
}

static HRESULT WINAPI media_object_Flush(IMediaObject *iface)
{
    TRACE("iface %p.\n", iface);
    return S_OK;
}

static HRESULT WINAPI media_object_Discontinuity(IMediaObject *iface, DWORD index)
{
    TRACE("iface %p, index %lu.\n", iface, index);
    return S_OK;
}

static HRESULT WINAPI media_object_AllocateStreamingResources(IMediaObject *iface)
{
    FIXME("iface %p stub!\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI media_object_FreeStreamingResources(IMediaObject *iface)
{
    FIXME("iface %p stub!\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI media_object_GetInputStatus(IMediaObject *iface, DWORD index, DWORD *flags)
{
    FIXME("iface %p, index %lu, flags %p stub!\n", iface, index, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI media_object_ProcessInput(IMediaObject *iface, DWORD index,
        IMediaBuffer *buffer, DWORD flags, REFERENCE_TIME timestamp, REFERENCE_TIME timelength)
{
    struct mp3_decoder *impl = impl_from_IMediaObject(iface);

    TRACE("iface %p, index %lu, buffer %p, flags %#lx, timestamp %s, timelength %s.\n", iface,
            index, buffer, flags, wine_dbgstr_longlong(timestamp), wine_dbgstr_longlong(timelength));

    return wg_transform_push_dmo(impl->wg_transform, buffer, timestamp, timelength, impl->wg_sample_queue);
}

static HRESULT WINAPI media_object_ProcessOutput(IMediaObject *iface, DWORD flags, DWORD count,
        DMO_OUTPUT_DATA_BUFFER *buffers, DWORD *status)
{
    struct mp3_decoder *impl = impl_from_IMediaObject(iface);
    HRESULT hr;

    TRACE("iface %p, flags %#lx, count %lu, buffers %p, status %p.\n", iface, flags, count, buffers, status);

    *status = 0;

    if (SUCCEEDED(hr = wg_transform_read_dmo(impl->wg_transform, buffers[0].pBuffer,
            &buffers[0].dwStatus, &buffers[0].rtTimestamp, &buffers[0].rtTimelength)))
        wg_sample_queue_flush(impl->wg_sample_queue, false);

    return hr;
}

static HRESULT WINAPI media_object_Lock(IMediaObject *iface, LONG lock)
{
    FIXME("iface %p, lock %ld stub!\n", iface, lock);
    return E_NOTIMPL;
}

static const IMediaObjectVtbl media_object_vtbl =
{
    media_object_QueryInterface,
    media_object_AddRef,
    media_object_Release,
    media_object_GetStreamCount,
    media_object_GetInputStreamInfo,
    media_object_GetOutputStreamInfo,
    media_object_GetInputType,
    media_object_GetOutputType,
    media_object_SetInputType,
    media_object_SetOutputType,
    media_object_GetInputCurrentType,
    media_object_GetOutputCurrentType,
    media_object_GetInputSizeInfo,
    media_object_GetOutputSizeInfo,
    media_object_GetInputMaxLatency,
    media_object_SetInputMaxLatency,
    media_object_Flush,
    media_object_Discontinuity,
    media_object_AllocateStreamingResources,
    media_object_FreeStreamingResources,
    media_object_GetInputStatus,
    media_object_ProcessInput,
    media_object_ProcessOutput,
    media_object_Lock,
};

static inline struct mp3_decoder *impl_from_IPropertyStore(IPropertyStore *iface)
{
    return CONTAINING_RECORD(iface, struct mp3_decoder, IPropertyStore_iface);
}

static HRESULT WINAPI property_store_QueryInterface(IPropertyStore *iface, REFIID iid, void **out)
{
    return IUnknown_QueryInterface(impl_from_IPropertyStore(iface)->outer, iid, out);
}

static ULONG WINAPI property_store_AddRef(IPropertyStore *iface)
{
    return IUnknown_AddRef(impl_from_IPropertyStore(iface)->outer);
}

static ULONG WINAPI property_store_Release(IPropertyStore *iface)
{
    return IUnknown_Release(impl_from_IPropertyStore(iface)->outer);
}

static HRESULT WINAPI property_store_GetCount(IPropertyStore *iface, DWORD *count)
{
    FIXME("iface %p, count %p stub!\n", iface, count);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_store_GetAt(IPropertyStore *iface, DWORD index, PROPERTYKEY *key)
{
    FIXME("iface %p, index %lu, key %p stub!\n", iface, index, key);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_store_GetValue(IPropertyStore *iface, REFPROPERTYKEY key, PROPVARIANT *value)
{
    FIXME("iface %p, key %p, value %p stub!\n", iface, key, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_store_SetValue(IPropertyStore *iface, REFPROPERTYKEY key, REFPROPVARIANT value)
{
    FIXME("iface %p, key %p, value %p stub!\n", iface, key, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_store_Commit(IPropertyStore *iface)
{
    FIXME("iface %p stub!\n", iface);
    return E_NOTIMPL;
}

static const IPropertyStoreVtbl property_store_vtbl =
{
    property_store_QueryInterface,
    property_store_AddRef,
    property_store_Release,
    property_store_GetCount,
    property_store_GetAt,
    property_store_GetValue,
    property_store_SetValue,
    property_store_Commit,
};

HRESULT mp3_decoder_create(IUnknown *outer, IUnknown **out)
{
    static const struct wg_format input_format =
    {
        .major_type = WG_MAJOR_TYPE_AUDIO_MPEG1,
        .u.audio_mpeg1 =
        {
            .layer = 3,
            .channels = 1,
            .rate = 44100,
        },
    };
    static const struct wg_format output_format =
    {
        .major_type = WG_MAJOR_TYPE_AUDIO,
        .u.audio =
        {
            .format = WG_AUDIO_FORMAT_S16LE,
            .channel_mask = 1,
            .channels = 1,
            .rate = 44100,
        },
    };
    struct wg_transform *transform;
    struct mp3_decoder *impl;
    HRESULT hr;

    TRACE("outer %p, out %p.\n", outer, out);

    if (!(transform = wg_transform_create(&input_format, &output_format)))
    {
        ERR_(winediag)("GStreamer doesn't support audio resampling, please install appropriate plugins.\n");
        return E_FAIL;
    }
    wg_transform_destroy(transform);

    if (!(impl = calloc(1, sizeof(*impl))))
        return E_OUTOFMEMORY;

    if (FAILED(hr = wg_sample_queue_create(&impl->wg_sample_queue)))
    {
        free(impl);
        return hr;
    }

    impl->IUnknown_inner.lpVtbl = &unknown_vtbl;
    impl->IMFTransform_iface.lpVtbl = &transform_vtbl;
    impl->IMediaObject_iface.lpVtbl = &media_object_vtbl;
    impl->IPropertyStore_iface.lpVtbl = &property_store_vtbl;
    impl->refcount = 1;
    impl->outer = outer ? outer : &impl->IUnknown_inner;

    *out = &impl->IUnknown_inner;
    TRACE("Created %p\n", *out);
    return S_OK;
}
