/* Audio Decoder Transform
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

#include "gst_private.h"

#include "mfapi.h"
#include "mferror.h"
#include "mfobjects.h"
#include "mftransform.h"
#include "wmcodecdsp.h"
#include "ks.h"
#include "ksmedia.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(mfplat);
WINE_DECLARE_DEBUG_CHANNEL(winediag);

#define NEXT_WAVEFORMATEXTENSIBLE(format) (WAVEFORMATEXTENSIBLE *)((BYTE *)(&(format)->Format + 1) + (format)->Format.cbSize)

static WAVEFORMATEXTENSIBLE const audio_decoder_output_types[] =
{
    {.Format = {.wFormatTag = WAVE_FORMAT_IEEE_FLOAT, .wBitsPerSample = 32, .nSamplesPerSec = 48000, .nChannels = 2,
                .cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)}},
    {.Format = {.wFormatTag = WAVE_FORMAT_PCM, .wBitsPerSample = 16, .nSamplesPerSec = 48000, .nChannels = 2,
                .cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)}},
};

static const UINT32 default_channel_mask[7] =
{
    0,
    0,
    0,
    SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_BACK_CENTER,
    KSAUDIO_SPEAKER_QUAD,
    KSAUDIO_SPEAKER_QUAD | SPEAKER_FRONT_CENTER,
    KSAUDIO_SPEAKER_5POINT1,
};

struct audio_decoder
{
    IUnknown IUnknown_inner;
    IMFTransform IMFTransform_iface;
    IMediaObject IMediaObject_iface;
    IPropertyBag IPropertyBag_iface;
    IUnknown *outer;
    LONG refcount;

    UINT input_type_count;
    WAVEFORMATEXTENSIBLE *input_types;

    MFT_INPUT_STREAM_INFO input_info;
    BOOL input_info_needs_type;
    IMFMediaType *input_type;
    MFT_OUTPUT_STREAM_INFO output_info;
    BOOL output_info_needs_type;
    IMFMediaType *output_type;

    wg_transform_t wg_transform;
    struct wg_sample_queue *wg_sample_queue;
};

static HRESULT try_create_wg_transform(struct audio_decoder *decoder)
{
    struct wg_transform_attrs attrs =
    {
        .input_queue_length = 7,
    };

    if (decoder->wg_transform)
    {
        wg_transform_destroy(decoder->wg_transform);
        decoder->wg_transform = 0;
    }

    return wg_transform_create_mf(decoder->input_type, decoder->output_type, &attrs, &decoder->wg_transform);
}

static inline struct audio_decoder *impl_from_IUnknown(IUnknown *iface)
{
    return CONTAINING_RECORD(iface, struct audio_decoder, IUnknown_inner);
}

static HRESULT WINAPI unknown_QueryInterface(IUnknown *iface, REFIID iid, void **out)
{
    struct audio_decoder *decoder = impl_from_IUnknown(iface);

    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown))
        *out = &decoder->IUnknown_inner;
    else if (IsEqualGUID(iid, &IID_IMFTransform))
        *out = &decoder->IMFTransform_iface;
    else if (IsEqualGUID(iid, &IID_IMediaObject) && decoder->IMediaObject_iface.lpVtbl)
        *out = &decoder->IMediaObject_iface;
    else if (IsEqualIID(iid, &IID_IPropertyBag) && decoder->IPropertyBag_iface.lpVtbl)
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

static ULONG WINAPI unknown_AddRef(IUnknown *iface)
{
    struct audio_decoder *decoder = impl_from_IUnknown(iface);
    ULONG refcount = InterlockedIncrement(&decoder->refcount);

    TRACE("iface %p increasing refcount to %lu.\n", decoder, refcount);

    return refcount;
}

static ULONG WINAPI unknown_Release(IUnknown *iface)
{
    struct audio_decoder *decoder = impl_from_IUnknown(iface);
    ULONG refcount = InterlockedDecrement(&decoder->refcount);

    TRACE("iface %p decreasing refcount to %lu.\n", decoder, refcount);

    if (!refcount)
    {
        if (decoder->wg_transform)
            wg_transform_destroy(decoder->wg_transform);
        if (decoder->input_type)
            IMFMediaType_Release(decoder->input_type);
        if (decoder->output_type)
            IMFMediaType_Release(decoder->output_type);

        wg_sample_queue_destroy(decoder->wg_sample_queue);
        free(decoder);
    }

    return refcount;
}

static const IUnknownVtbl unknown_vtbl =
{
    unknown_QueryInterface,
    unknown_AddRef,
    unknown_Release,
};

static struct audio_decoder *impl_from_IMFTransform(IMFTransform *iface)
{
    return CONTAINING_RECORD(iface, struct audio_decoder, IMFTransform_iface);
}

static HRESULT WINAPI transform_QueryInterface(IMFTransform *iface, REFIID iid, void **out)
{
    struct audio_decoder *decoder = impl_from_IMFTransform(iface);
    return IUnknown_QueryInterface(decoder->outer, iid, out);
}

static ULONG WINAPI transform_AddRef(IMFTransform *iface)
{
    struct audio_decoder *decoder = impl_from_IMFTransform(iface);
    return IUnknown_AddRef(decoder->outer);
}

static ULONG WINAPI transform_Release(IMFTransform *iface)
{
    struct audio_decoder *decoder = impl_from_IMFTransform(iface);
    return IUnknown_Release(decoder->outer);
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
    TRACE("iface %p, input_size %lu, inputs %p, output_size %lu, outputs %p.\n", iface,
            input_size, inputs, output_size, outputs);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_GetInputStreamInfo(IMFTransform *iface, DWORD id, MFT_INPUT_STREAM_INFO *info)
{
    struct audio_decoder *decoder = impl_from_IMFTransform(iface);

    TRACE("iface %p, id %lu, info %p.\n", iface, id, info);

    if (id)
        return MF_E_INVALIDSTREAMNUMBER;
    if (decoder->input_info_needs_type && (!decoder->input_type || !decoder->output_type))
    {
        memset(info, 0, sizeof(*info));
        return MF_E_TRANSFORM_TYPE_NOT_SET;
    }

    *info = decoder->input_info;
    return S_OK;
}

static HRESULT WINAPI transform_GetOutputStreamInfo(IMFTransform *iface, DWORD id, MFT_OUTPUT_STREAM_INFO *info)
{
    struct audio_decoder *decoder = impl_from_IMFTransform(iface);

    TRACE("iface %p, id %lu, info %p.\n", iface, id, info);

    if (id)
        return MF_E_INVALIDSTREAMNUMBER;
    if (decoder->output_info_needs_type && (!decoder->input_type || !decoder->output_type))
    {
        memset(info, 0, sizeof(*info));
        return MF_E_TRANSFORM_TYPE_NOT_SET;
    }

    *info = decoder->output_info;
    return S_OK;
}

static HRESULT WINAPI transform_GetAttributes(IMFTransform *iface, IMFAttributes **attributes)
{
    TRACE("iface %p, attributes %p.\n", iface, attributes);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_GetInputStreamAttributes(IMFTransform *iface, DWORD id, IMFAttributes **attributes)
{
    TRACE("iface %p, id %#lx, attributes %p.\n", iface, id, attributes);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_GetOutputStreamAttributes(IMFTransform *iface, DWORD id, IMFAttributes **attributes)
{
    TRACE("iface %p, id %#lx, attributes %p.\n", iface, id, attributes);
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
    struct audio_decoder *decoder = impl_from_IMFTransform(iface);
    const WAVEFORMATEXTENSIBLE *format = decoder->input_types;
    UINT count = decoder->input_type_count;

    TRACE("iface %p, id %#lx, index %#lx, type %p.\n", iface, id, index, type);

    *type = NULL;
    if (id)
        return MF_E_INVALIDSTREAMNUMBER;
    for (format = decoder->input_types; index > 0 && count > 0; index--, count--)
        format = NEXT_WAVEFORMATEXTENSIBLE(format);
    return count ? MFCreateAudioMediaType(&format->Format, (IMFAudioMediaType **)type) : MF_E_NO_MORE_TYPES;
}

static HRESULT WINAPI transform_GetOutputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    struct audio_decoder *decoder = impl_from_IMFTransform(iface);
    UINT32 channel_count, sample_rate;
    WAVEFORMATEXTENSIBLE wfx = {{0}};
    IMFMediaType *media_type;
    HRESULT hr;

    TRACE("iface %p, id %#lx, index %#lx, type %p.\n", iface, id, index, type);

    *type = NULL;

    if (id)
        return MF_E_INVALIDSTREAMNUMBER;
    if (!decoder->input_type)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    wfx = audio_decoder_output_types[index % ARRAY_SIZE(audio_decoder_output_types)];

    if (FAILED(hr = IMFMediaType_GetUINT32(decoder->input_type, &MF_MT_AUDIO_NUM_CHANNELS, &channel_count))
            || !channel_count)
        channel_count = wfx.Format.nChannels;
    if (FAILED(hr = IMFMediaType_GetUINT32(decoder->input_type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &sample_rate)))
        sample_rate = wfx.Format.nSamplesPerSec;

    if (channel_count >= ARRAY_SIZE(default_channel_mask))
        return MF_E_INVALIDMEDIATYPE;

    if (channel_count > 2 && index >= ARRAY_SIZE(audio_decoder_output_types))
    {
        /* If there are more than two channels in the input type GetOutputAvailableType additionally lists
         * types with 2 channels. */
        index -= ARRAY_SIZE(audio_decoder_output_types);
        channel_count = 2;
    }

    if (index >= ARRAY_SIZE(audio_decoder_output_types))
        return MF_E_NO_MORE_TYPES;

    wfx.Format.nChannels = channel_count;
    wfx.Format.nSamplesPerSec = sample_rate;
    wfx.Format.nBlockAlign = wfx.Format.wBitsPerSample * wfx.Format.nChannels / 8;
    wfx.Format.nAvgBytesPerSec = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;

    if (wfx.Format.nChannels >= 3)
    {
        wfx.SubFormat = MFAudioFormat_Base;
        wfx.SubFormat.Data1 = wfx.Format.wFormatTag;
        wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        wfx.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
        wfx.dwChannelMask = default_channel_mask[wfx.Format.nChannels];
    }

    if (FAILED(hr = MFCreateAudioMediaType(&wfx.Format, (IMFAudioMediaType **)&media_type)))
        return hr;
    if (FAILED(hr = IMFMediaType_SetUINT32(media_type, &MF_MT_FIXED_SIZE_SAMPLES, 1)))
        goto done;

done:
    if (SUCCEEDED(hr))
        IMFMediaType_AddRef((*type = media_type));

    IMFMediaType_Release(media_type);
    return hr;
}

static BOOL matches_format(const WAVEFORMATEXTENSIBLE *a, const WAVEFORMATEXTENSIBLE *b)
{
    if (a->Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE && b->Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE)
        return IsEqualGUID(&a->SubFormat, &b->SubFormat);
    if (a->Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE)
        return a->SubFormat.Data1 == b->Format.wFormatTag;
    if (b->Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE)
        return b->SubFormat.Data1 == a->Format.wFormatTag;
    return a->Format.wFormatTag == b->Format.wFormatTag;
}

static HRESULT WINAPI transform_SetInputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    struct audio_decoder *decoder = impl_from_IMFTransform(iface);
    UINT32 size, count = decoder->input_type_count;
    WAVEFORMATEXTENSIBLE *format, wfx;
    HRESULT hr;

    TRACE("iface %p, id %#lx, type %p, flags %#lx.\n", iface, id, type, flags);

    if (id)
        return MF_E_INVALIDSTREAMNUMBER;

    if (!type)
    {
        if (decoder->input_type)
        {
            IMFMediaType_Release(decoder->input_type);
            decoder->input_type = NULL;
        }
        if (decoder->output_type)
        {
            IMFMediaType_Release(decoder->output_type);
            decoder->output_type = NULL;
        }
        if (decoder->wg_transform)
        {
            wg_transform_destroy(decoder->wg_transform);
            decoder->wg_transform = 0;
        }

        return S_OK;
    }

    if (FAILED(hr = MFCreateWaveFormatExFromMFMediaType(type, (WAVEFORMATEX **)&format, &size,
            MFWaveFormatExConvertFlag_ForceExtensible)))
        return hr;
    wfx = *format;
    CoTaskMemFree(format);

    for (format = decoder->input_types; count > 0 && !matches_format(format, &wfx); count--)
        format = NEXT_WAVEFORMATEXTENSIBLE(format);
    if (!count)
        return MF_E_INVALIDMEDIATYPE;

    if (wfx.Format.nChannels >= ARRAY_SIZE(default_channel_mask) || !wfx.Format.nSamplesPerSec || !wfx.Format.cbSize)
        return MF_E_INVALIDMEDIATYPE;
    if (decoder->IMediaObject_iface.lpVtbl && (!wfx.Format.nChannels || !wfx.Format.nBlockAlign))
        return MF_E_INVALIDMEDIATYPE;
    if (flags & MFT_SET_TYPE_TEST_ONLY)
        return S_OK;

    if (!decoder->input_type && FAILED(hr = MFCreateMediaType(&decoder->input_type)))
        return hr;

    if (decoder->output_type)
    {
        IMFMediaType_Release(decoder->output_type);
        decoder->output_type = NULL;
    }

    if (SUCCEEDED(hr = IMFMediaType_CopyAllItems(type, (IMFAttributes *)decoder->input_type)))
        decoder->input_info.cbSize = wfx.Format.nBlockAlign;
    else
    {
        IMFMediaType_Release(decoder->input_type);
        decoder->input_info.cbSize = 0;
        decoder->input_type = NULL;
    }

    return hr;
}

static HRESULT WINAPI transform_SetOutputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    struct audio_decoder *decoder = impl_from_IMFTransform(iface);
    WAVEFORMATEXTENSIBLE *format, wfx;
    UINT32 size;
    HRESULT hr;
    ULONG i;

    TRACE("iface %p, id %#lx, type %p, flags %#lx.\n", iface, id, type, flags);

    if (id)
        return MF_E_INVALIDSTREAMNUMBER;

    if (!type)
    {
        if (decoder->output_type)
        {
            IMFMediaType_Release(decoder->output_type);
            decoder->input_type = NULL;
        }
        if (decoder->wg_transform)
        {
            wg_transform_destroy(decoder->wg_transform);
            decoder->wg_transform = 0;
        }

        return S_OK;
    }

    if (!decoder->input_type)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (FAILED(hr = MFCreateWaveFormatExFromMFMediaType(type, (WAVEFORMATEX **)&format, &size,
            MFWaveFormatExConvertFlag_ForceExtensible)))
        return hr;
    wfx = *format;
    CoTaskMemFree(format);

    for (i = 0; i < ARRAY_SIZE(audio_decoder_output_types); ++i)
        if (matches_format(&audio_decoder_output_types[i], &wfx))
            break;
    if (i == ARRAY_SIZE(audio_decoder_output_types))
        return MF_E_INVALIDMEDIATYPE;

    if (!wfx.Format.wBitsPerSample || !wfx.Format.nChannels || !wfx.Format.nSamplesPerSec)
        return MF_E_INVALIDMEDIATYPE;
    if (decoder->IMediaObject_iface.lpVtbl && (!wfx.Format.nAvgBytesPerSec || !wfx.Format.nBlockAlign))
        return MF_E_INVALIDMEDIATYPE;
    if (flags & MFT_SET_TYPE_TEST_ONLY)
        return S_OK;

    if (!decoder->output_type && FAILED(hr = MFCreateMediaType(&decoder->output_type)))
        return hr;

    if (FAILED(hr = IMFMediaType_CopyAllItems(type, (IMFAttributes *)decoder->output_type)))
        return hr;

    if (FAILED(hr = try_create_wg_transform(decoder)))
        goto failed;

    return S_OK;

failed:
    IMFMediaType_Release(decoder->output_type);
    decoder->output_type = NULL;
    return hr;
}

static HRESULT WINAPI transform_GetInputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **out)
{
    struct audio_decoder *decoder = impl_from_IMFTransform(iface);
    IMFMediaType *type;
    HRESULT hr;

    TRACE("iface %p, id %#lx, out %p.\n", iface, id, out);

    if (id)
        return MF_E_INVALIDSTREAMNUMBER;
    if (!decoder->input_type)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (FAILED(hr = MFCreateMediaType(&type)))
        return hr;
    if (SUCCEEDED(hr = IMFMediaType_CopyAllItems(decoder->input_type, (IMFAttributes *)type)))
        IMFMediaType_AddRef(*out = type);
    IMFMediaType_Release(type);

    return hr;
}

static HRESULT WINAPI transform_GetOutputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **out)
{
    struct audio_decoder *decoder = impl_from_IMFTransform(iface);
    IMFMediaType *type;
    HRESULT hr;

    TRACE("iface %p, id %#lx, out %p.\n", iface, id, out);

    if (id)
        return MF_E_INVALIDSTREAMNUMBER;
    if (!decoder->output_type)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (FAILED(hr = MFCreateMediaType(&type)))
        return hr;
    if (SUCCEEDED(hr = IMFMediaType_CopyAllItems(decoder->output_type, (IMFAttributes *)type)))
        IMFMediaType_AddRef(*out = type);
    IMFMediaType_Release(type);

    return hr;
}

static HRESULT WINAPI transform_GetInputStatus(IMFTransform *iface, DWORD id, DWORD *flags)
{
    struct audio_decoder *decoder = impl_from_IMFTransform(iface);
    bool accepts_input;

    TRACE("iface %p, id %#lx, flags %p.\n", iface, id, flags);

    if (!decoder->wg_transform)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (!wg_transform_get_status(decoder->wg_transform, &accepts_input))
        return E_FAIL;

    *flags = accepts_input ? MFT_INPUT_STATUS_ACCEPT_DATA : 0;
    return S_OK;
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
    struct audio_decoder *decoder = impl_from_IMFTransform(iface);

    TRACE("iface %p, message %#x, param %p.\n", iface, message, (void *)param);

    if (!decoder->wg_transform)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (message == MFT_MESSAGE_COMMAND_DRAIN)
        return wg_transform_drain(decoder->wg_transform);

    FIXME("Ignoring message %#x.\n", message);

    return S_OK;
}

static HRESULT WINAPI transform_ProcessInput(IMFTransform *iface, DWORD id, IMFSample *sample, DWORD flags)
{
    struct audio_decoder *decoder = impl_from_IMFTransform(iface);

    TRACE("iface %p, id %#lx, sample %p, flags %#lx.\n", iface, id, sample, flags);

    if (!decoder->wg_transform)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    return wg_transform_push_mf(decoder->wg_transform, sample, decoder->wg_sample_queue);
}

static HRESULT WINAPI transform_ProcessOutput(IMFTransform *iface, DWORD flags, DWORD count,
        MFT_OUTPUT_DATA_BUFFER *samples, DWORD *status)
{
    struct audio_decoder *decoder = impl_from_IMFTransform(iface);
    MFT_OUTPUT_STREAM_INFO info;
    HRESULT hr;

    TRACE("iface %p, flags %#lx, count %lu, samples %p, status %p.\n", iface, flags, count, samples, status);

    if (count != 1)
        return E_INVALIDARG;

    if (!decoder->wg_transform)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    *status = samples->dwStatus = 0;
    if (!samples->pSample)
        return E_INVALIDARG;

    if (FAILED(hr = IMFTransform_GetOutputStreamInfo(iface, 0, &info)))
        return hr;

    if (SUCCEEDED(hr = wg_transform_read_mf(decoder->wg_transform, samples->pSample,
            info.cbSize, &samples->dwStatus)))
        wg_sample_queue_flush(decoder->wg_sample_queue, false);
    else
        samples->dwStatus = MFT_OUTPUT_DATA_BUFFER_NO_SAMPLE;

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

static inline struct audio_decoder *impl_from_IMediaObject(IMediaObject *iface)
{
    return CONTAINING_RECORD(iface, struct audio_decoder, IMediaObject_iface);
}

static HRESULT WINAPI media_object_QueryInterface(IMediaObject *iface, REFIID iid, void **obj)
{
    struct audio_decoder *decoder = impl_from_IMediaObject(iface);
    return IUnknown_QueryInterface(decoder->outer, iid, obj);
}

static ULONG WINAPI media_object_AddRef(IMediaObject *iface)
{
    struct audio_decoder *decoder = impl_from_IMediaObject(iface);
    return IUnknown_AddRef(decoder->outer);
}

static ULONG WINAPI media_object_Release(IMediaObject *iface)
{
    struct audio_decoder *decoder = impl_from_IMediaObject(iface);
    return IUnknown_Release(decoder->outer);
}

static HRESULT WINAPI media_object_GetStreamCount(IMediaObject *iface, DWORD *input, DWORD *output)
{
    FIXME("iface %p, input %p, output %p semi-stub!\n", iface, input, output);
    *input = *output = 1;
    return S_OK;
}

static HRESULT WINAPI media_object_GetInputStreamInfo(IMediaObject *iface, DWORD index, DWORD *flags)
{
    FIXME("iface %p, index %lu, flags %p stub!\n", iface, index, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI media_object_GetOutputStreamInfo(IMediaObject *iface, DWORD index, DWORD *flags)
{
    FIXME("iface %p, index %lu, flags %p stub!\n", iface, index, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI media_object_GetInputType(IMediaObject *iface, DWORD index, DWORD type_index,
        DMO_MEDIA_TYPE *type)
{
    FIXME("iface %p, index %lu, type_index %lu, type %p stub!\n", iface, index, type_index, type);
    return E_NOTIMPL;
}

static HRESULT WINAPI media_object_GetOutputType(IMediaObject *iface, DWORD index, DWORD type_index,
        DMO_MEDIA_TYPE *type)
{
    FIXME("iface %p, index %lu, type_index %lu, type %p stub!\n", iface, index, type_index, type);
    return E_NOTIMPL;
}

static HRESULT WINAPI media_object_SetInputType(IMediaObject *iface, DWORD index,
        const DMO_MEDIA_TYPE *type, DWORD flags)
{
    FIXME("iface %p, index %lu, type %p, flags %#lx stub!\n", iface, index, type, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI media_object_SetOutputType(IMediaObject *iface, DWORD index,
        const DMO_MEDIA_TYPE *type, DWORD flags)
{
    FIXME("iface %p, index %lu, type %p, flags %#lx stub!\n", iface, index, type, flags);
    return E_NOTIMPL;
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
    FIXME("iface %p, index %lu, size %p, lookahead %p, alignment %p stub!\n", iface, index, size,
            lookahead, alignment);
    return E_NOTIMPL;
}

static HRESULT WINAPI media_object_GetOutputSizeInfo(IMediaObject *iface, DWORD index, DWORD *size, DWORD *alignment)
{
    FIXME("iface %p, index %lu, size %p, alignment %p stub!\n", iface, index, size, alignment);
    return E_NOTIMPL;
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
    FIXME("iface %p stub!\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI media_object_Discontinuity(IMediaObject *iface, DWORD index)
{
    FIXME("iface %p, index %lu stub!\n", iface, index);
    return E_NOTIMPL;
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
    FIXME("iface %p, index %lu, buffer %p, flags %#lx, timestamp %s, timelength %s stub!\n", iface,
            index, buffer, flags, wine_dbgstr_longlong(timestamp), wine_dbgstr_longlong(timelength));
    return E_NOTIMPL;
}

static HRESULT WINAPI media_object_ProcessOutput(IMediaObject *iface, DWORD flags, DWORD count,
        DMO_OUTPUT_DATA_BUFFER *buffers, DWORD *status)
{
    FIXME("iface %p, flags %#lx, count %lu, buffers %p, status %p stub!\n", iface, flags, count, buffers, status);
    return E_NOTIMPL;
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

static inline struct audio_decoder *impl_from_IPropertyBag(IPropertyBag *iface)
{
    return CONTAINING_RECORD(iface, struct audio_decoder, IPropertyBag_iface);
}

static HRESULT WINAPI property_bag_QueryInterface(IPropertyBag *iface, REFIID iid, void **out)
{
    struct audio_decoder *filter = impl_from_IPropertyBag(iface);
    return IUnknown_QueryInterface(filter->outer, iid, out);
}

static ULONG WINAPI property_bag_AddRef(IPropertyBag *iface)
{
    struct audio_decoder *filter = impl_from_IPropertyBag(iface);
    return IUnknown_AddRef(filter->outer);
}

static ULONG WINAPI property_bag_Release(IPropertyBag *iface)
{
    struct audio_decoder *filter = impl_from_IPropertyBag(iface);
    return IUnknown_Release(filter->outer);
}

static HRESULT WINAPI property_bag_Read(IPropertyBag *iface, const WCHAR *prop_name, VARIANT *value,
        IErrorLog *error_log)
{
    FIXME("iface %p, prop_name %s, value %p, error_log %p stub!\n", iface, debugstr_w(prop_name), value, error_log);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_bag_Write(IPropertyBag *iface, const WCHAR *prop_name, VARIANT *value)
{
    FIXME("iface %p, prop_name %s, value %p stub!\n", iface, debugstr_w(prop_name), value);
    return S_OK;
}

static const IPropertyBagVtbl property_bag_vtbl =
{
    property_bag_QueryInterface,
    property_bag_AddRef,
    property_bag_Release,
    property_bag_Read,
    property_bag_Write,
};

static HEAACWAVEINFO aac_decoder_input_types[] =
{
#define MAKE_HEAACWAVEINFO(format, payload) \
    {.wfx = {.wFormatTag = format, .nChannels = 6, .nSamplesPerSec = 48000, .nAvgBytesPerSec = 1152000, \
             .nBlockAlign = 24, .wBitsPerSample = 32, .cbSize = sizeof(HEAACWAVEINFO) - sizeof(WAVEFORMATEX)}, \
     .wPayloadType = payload}

    MAKE_HEAACWAVEINFO(WAVE_FORMAT_MPEG_HEAAC, 0),
    MAKE_HEAACWAVEINFO(WAVE_FORMAT_RAW_AAC1, 0),
    MAKE_HEAACWAVEINFO(WAVE_FORMAT_MPEG_HEAAC, 1),
    MAKE_HEAACWAVEINFO(WAVE_FORMAT_MPEG_HEAAC, 3),
    MAKE_HEAACWAVEINFO(WAVE_FORMAT_MPEG_ADTS_AAC, 0),

#undef MAKE_HEAACWAVEINFO
};

HRESULT aac_decoder_create(REFIID riid, void **ret)
{
    struct audio_decoder *decoder;
    HRESULT hr;

    TRACE("riid %s, ret %p.\n", debugstr_guid(riid), ret);

    if (FAILED(hr = check_audio_transform_support(&aac_decoder_input_types[0].wfx, &aac_decoder_output_types[0].Format)))
    {
        ERR_(winediag)("GStreamer doesn't support AAC decoding, please install appropriate plugins\n");
        return hr;
    }

    if (!(decoder = calloc(1, sizeof(*decoder))))
        return E_OUTOFMEMORY;

    decoder->IUnknown_inner.lpVtbl = &unknown_vtbl;
    decoder->IMFTransform_iface.lpVtbl = &transform_vtbl;
    decoder->outer = &decoder->IUnknown_inner;
    decoder->refcount = 1;

    decoder->input_types = (WAVEFORMATEXTENSIBLE *)aac_decoder_input_types;
    decoder->input_type_count = ARRAY_SIZE(aac_decoder_input_types);

    decoder->input_info.dwFlags = MFT_INPUT_STREAM_WHOLE_SAMPLES | MFT_INPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER
            | MFT_INPUT_STREAM_FIXED_SAMPLE_SIZE | MFT_INPUT_STREAM_HOLDS_BUFFERS;
    decoder->output_info.dwFlags = MFT_INPUT_STREAM_WHOLE_SAMPLES;
    decoder->output_info.cbSize = 0xc000;

    if (FAILED(hr = wg_sample_queue_create(&decoder->wg_sample_queue)))
    {
        free(decoder);
        return hr;
    }

    *ret = &decoder->IMFTransform_iface;
    TRACE("Created decoder %p\n", *ret);
    return S_OK;
}

static WAVEFORMATEXTENSIBLE wma_decoder_input_types[] =
{
#define MAKE_WAVEFORMATEXTENSIBLE(format) \
    {.Format = {.wFormatTag = WAVE_FORMAT_EXTENSIBLE, .nChannels = 6, .nSamplesPerSec = 48000, .nAvgBytesPerSec = 1152000, \
                .nBlockAlign = 24, .wBitsPerSample = 32, .cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)}, \
     .SubFormat = {format,0x0000,0x0010,{0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}}}

    MAKE_WAVEFORMATEXTENSIBLE(WAVE_FORMAT_MSAUDIO1),
    MAKE_WAVEFORMATEXTENSIBLE(WAVE_FORMAT_WMAUDIO2),
    MAKE_WAVEFORMATEXTENSIBLE(WAVE_FORMAT_WMAUDIO3),
    MAKE_WAVEFORMATEXTENSIBLE(WAVE_FORMAT_WMAUDIO_LOSSLESS),

#undef MAKE_WAVEFORMATEXTENSIBLE
};

HRESULT wma_decoder_create(IUnknown *outer, IUnknown **out)
{
    static const struct wg_format output_format =
    {
        .major_type = WG_MAJOR_TYPE_AUDIO,
        .u.audio =
        {
            .format = WG_AUDIO_FORMAT_F32LE,
            .channel_mask = 1,
            .channels = 1,
            .rate = 44100,
        },
    };
    static const struct wg_format input_format = {.major_type = WG_MAJOR_TYPE_AUDIO_WMA};
    struct wg_transform_attrs attrs = {0};
    struct audio_decoder *decoder;
    wg_transform_t transform;
    HRESULT hr;

    TRACE("outer %p, out %p.\n", outer, out);

    if (!(transform = wg_transform_create(&input_format, &output_format, &attrs)))
    {
        ERR_(winediag)("GStreamer doesn't support WMA decoding, please install appropriate plugins\n");
        return E_FAIL;
    }
    wg_transform_destroy(transform);

    if (!(decoder = calloc(1, sizeof(*decoder))))
        return E_OUTOFMEMORY;

    decoder->IUnknown_inner.lpVtbl = &unknown_vtbl;
    decoder->IMFTransform_iface.lpVtbl = &transform_vtbl;
    decoder->IMediaObject_iface.lpVtbl = &media_object_vtbl;
    decoder->IPropertyBag_iface.lpVtbl = &property_bag_vtbl;
    decoder->outer = outer ? outer : &decoder->IUnknown_inner;
    decoder->refcount = 1;

    decoder->input_types = wma_decoder_input_types;
    decoder->input_type_count = ARRAY_SIZE(wma_decoder_input_types);
    decoder->input_info.cbAlignment = 1;
    decoder->input_info_needs_type = TRUE;
    decoder->output_info.cbAlignment = 1;
    decoder->output_info_needs_type = TRUE;

    if (FAILED(hr = wg_sample_queue_create(&decoder->wg_sample_queue)))
    {
        free(decoder);
        return hr;
    }

    *out = (IUnknown *)&decoder->IMFTransform_iface;
    TRACE("Created decoder %p\n", *out);
    return S_OK;
}

static WAVEFORMATEXTENSIBLE audio_decoder_input_types[] =
{
#define MAKE_WAVEFORMATEXTENSIBLE(format) \
    {.Format = {.wFormatTag = WAVE_FORMAT_EXTENSIBLE, .nChannels = 6, .nSamplesPerSec = 48000, .nAvgBytesPerSec = 1152000, \
                .nBlockAlign = 24, .wBitsPerSample = 32, .cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)}, \
     .SubFormat = {format,0x0000,0x0010,{0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}}}

    MAKE_WAVEFORMATEXTENSIBLE(MAKEFOURCC('G','S','T','a')),
    MAKE_WAVEFORMATEXTENSIBLE(WAVE_FORMAT_MPEGLAYER3),

#undef MAKE_WAVEFORMATEXTENSIBLE
};

HRESULT audio_decoder_create(REFIID riid, void **ret)
{
    struct audio_decoder *decoder;
    HRESULT hr;

    TRACE("riid %s, ret %p.\n", debugstr_guid(riid), ret);

    if (!(decoder = calloc(1, sizeof(*decoder))))
        return E_OUTOFMEMORY;

    decoder->IUnknown_inner.lpVtbl = &unknown_vtbl;
    decoder->IMFTransform_iface.lpVtbl = &transform_vtbl;
    decoder->outer = &decoder->IUnknown_inner;
    decoder->refcount = 1;

    decoder->input_types = audio_decoder_input_types;
    decoder->input_type_count = ARRAY_SIZE(audio_decoder_input_types);

    if (FAILED(hr = wg_sample_queue_create(&decoder->wg_sample_queue)))
    {
        free(decoder);
        return hr;
    }

    *ret = &decoder->IMFTransform_iface;
    TRACE("Created decoder %p\n", *ret);
    return S_OK;
}
