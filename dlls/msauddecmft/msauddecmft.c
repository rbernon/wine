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

#include "ks.h"
#include "ksmedia.h"
#include "mfapi.h"
#include "mferror.h"
#include "mfidl.h"
#include "rpcproxy.h"
#include "wmcodecdsp.h"

#include "wine/debug.h"
#include "wine/winedmo.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmo);

#include "initguid.h"

DEFINE_MEDIATYPE_GUID(MFAudioFormat_RAW_AAC,WAVE_FORMAT_RAW_AAC1);

#define NEXT_WAVEFORMATEXTENSIBLE(format) (WAVEFORMATEXTENSIBLE *)((BYTE *)(&(format)->Format + 1) + (format)->Format.cbSize)

static WAVEFORMATEXTENSIBLE const aac_decoder_outputs[] =
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

struct aac_decoder
{
    IMFTransform IMFTransform_iface;
    LONG refcount;

    UINT input_type_count;
    WAVEFORMATEXTENSIBLE *input_types;

    IMFMediaType *input_type;
    IMFMediaType *output_type;

    IMFSample *input_sample;
    struct winedmo_transform winedmo_transform;
};

static struct aac_decoder *aac_decoder_from_IMFTransform(IMFTransform *iface)
{
    return CONTAINING_RECORD(iface, struct aac_decoder, IMFTransform_iface);
}

static HRESULT try_create_decoder(struct aac_decoder *decoder)
{
    union winedmo_format *input_format, *output_format;
    UINT format_size;
    NTSTATUS status;
    HRESULT hr;

    winedmo_transform_destroy(&decoder->winedmo_transform);

    if (FAILED(hr = MFCreateWaveFormatExFromMFMediaType(decoder->input_type, (WAVEFORMATEX **)&input_format, &format_size, 0)))
        return hr;
    if (FAILED(hr = MFCreateWaveFormatExFromMFMediaType(decoder->output_type, (WAVEFORMATEX **)&output_format, &format_size, 0)))
    {
        CoTaskMemFree(input_format);
        return hr;
    }

    if ((status = winedmo_transform_create(MFMediaType_Audio, input_format, output_format, &decoder->winedmo_transform)))
    {
        ERR("FAILED %#lx\n", status);
        hr = HRESULT_FROM_NT(status);
    }

    CoTaskMemFree(output_format);
    CoTaskMemFree(input_format);
    return hr;
}

static HRESULT WINAPI aac_decoder_QueryInterface(IMFTransform *iface, REFIID iid, void **out)
{
    struct aac_decoder *decoder = aac_decoder_from_IMFTransform(iface);

    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IMFTransform))
        *out = &decoder->IMFTransform_iface;
    else
    {
        *out = NULL;
        WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static ULONG WINAPI aac_decoder_AddRef(IMFTransform *iface)
{
    struct aac_decoder *decoder = aac_decoder_from_IMFTransform(iface);
    ULONG refcount = InterlockedIncrement(&decoder->refcount);
    TRACE("iface %p increasing refcount to %lu.\n", decoder, refcount);
    return refcount;
}

static ULONG WINAPI aac_decoder_Release(IMFTransform *iface)
{
    struct aac_decoder *decoder = aac_decoder_from_IMFTransform(iface);
    ULONG refcount = InterlockedDecrement(&decoder->refcount);

    TRACE("iface %p decreasing refcount to %lu.\n", decoder, refcount);

    if (!refcount)
    {
        if (decoder->input_sample)
            IMFSample_Release(decoder->input_sample);
        winedmo_transform_destroy(&decoder->winedmo_transform);
        if (decoder->input_type)
            IMFMediaType_Release(decoder->input_type);
        if (decoder->output_type)
            IMFMediaType_Release(decoder->output_type);
        free(decoder);
    }

    return refcount;
}

static HRESULT WINAPI aac_decoder_GetStreamLimits(IMFTransform *iface, DWORD *input_minimum,
        DWORD *input_maximum, DWORD *output_minimum, DWORD *output_maximum)
{
    TRACE("iface %p, input_minimum %p, input_maximum %p, output_minimum %p, output_maximum %p.\n",
            iface, input_minimum, input_maximum, output_minimum, output_maximum);
    *input_minimum = *input_maximum = *output_minimum = *output_maximum = 1;
    return S_OK;
}

static HRESULT WINAPI aac_decoder_GetStreamCount(IMFTransform *iface, DWORD *inputs, DWORD *outputs)
{
    TRACE("iface %p, inputs %p, outputs %p.\n", iface, inputs, outputs);
    *inputs = *outputs = 1;
    return S_OK;
}

static HRESULT WINAPI aac_decoder_GetStreamIDs(IMFTransform *iface, DWORD input_size, DWORD *inputs,
        DWORD output_size, DWORD *outputs)
{
    TRACE("iface %p, input_size %lu, inputs %p, output_size %lu, outputs %p.\n", iface,
            input_size, inputs, output_size, outputs);
    return E_NOTIMPL;
}

static HRESULT WINAPI aac_decoder_GetInputStreamInfo(IMFTransform *iface, DWORD id, MFT_INPUT_STREAM_INFO *info)
{
    TRACE("iface %p, id %#lx, info %p.\n", iface, id, info);

    if (id)
        return MF_E_INVALIDSTREAMNUMBER;

    memset(info, 0, sizeof(*info));
    info->dwFlags = MFT_INPUT_STREAM_WHOLE_SAMPLES | MFT_INPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER
            | MFT_INPUT_STREAM_FIXED_SAMPLE_SIZE | MFT_INPUT_STREAM_HOLDS_BUFFERS;

    return S_OK;
}

static HRESULT WINAPI aac_decoder_GetOutputStreamInfo(IMFTransform *iface, DWORD id, MFT_OUTPUT_STREAM_INFO *info)
{
    TRACE("iface %p, id %#lx, info %p.\n", iface, id, info);

    if (id)
        return MF_E_INVALIDSTREAMNUMBER;

    memset(info, 0, sizeof(*info));
    info->dwFlags = MFT_INPUT_STREAM_WHOLE_SAMPLES;
    info->cbSize = 0xc000;

    return S_OK;
}

static HRESULT WINAPI aac_decoder_GetAttributes(IMFTransform *iface, IMFAttributes **attributes)
{
    TRACE("iface %p, attributes %p.\n", iface, attributes);
    return E_NOTIMPL;
}

static HRESULT WINAPI aac_decoder_GetInputStreamAttributes(IMFTransform *iface, DWORD id, IMFAttributes **attributes)
{
    TRACE("iface %p, id %#lx, attributes %p.\n", iface, id, attributes);
    return E_NOTIMPL;
}

static HRESULT WINAPI aac_decoder_GetOutputStreamAttributes(IMFTransform *iface, DWORD id, IMFAttributes **attributes)
{
    TRACE("iface %p, id %#lx, attributes %p.\n", iface, id, attributes);
    return E_NOTIMPL;
}

static HRESULT WINAPI aac_decoder_DeleteInputStream(IMFTransform *iface, DWORD id)
{
    TRACE("iface %p, id %#lx.\n", iface, id);
    return E_NOTIMPL;
}

static HRESULT WINAPI aac_decoder_AddInputStreams(IMFTransform *iface, DWORD streams, DWORD *ids)
{
    TRACE("iface %p, streams %lu, ids %p.\n", iface, streams, ids);
    return E_NOTIMPL;
}

static HRESULT WINAPI aac_decoder_GetInputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    struct aac_decoder *decoder = aac_decoder_from_IMFTransform(iface);
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

static HRESULT WINAPI aac_decoder_GetOutputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    struct aac_decoder *decoder = aac_decoder_from_IMFTransform(iface);
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

    wfx = aac_decoder_outputs[index % ARRAY_SIZE(aac_decoder_outputs)];

    if (FAILED(hr = IMFMediaType_GetUINT32(decoder->input_type, &MF_MT_AUDIO_NUM_CHANNELS, &channel_count))
            || !channel_count)
        channel_count = wfx.Format.nChannels;
    if (FAILED(hr = IMFMediaType_GetUINT32(decoder->input_type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &sample_rate)))
        sample_rate = wfx.Format.nSamplesPerSec;

    if (channel_count >= ARRAY_SIZE(default_channel_mask))
        return MF_E_INVALIDMEDIATYPE;

    if (channel_count > 2 && index >= ARRAY_SIZE(aac_decoder_outputs))
    {
        /* If there are more than two channels in the input type GetOutputAvailableType additionally lists
         * types with 2 channels. */
        index -= ARRAY_SIZE(aac_decoder_outputs);
        channel_count = 2;
    }

    if (index >= ARRAY_SIZE(aac_decoder_outputs))
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

static HRESULT WINAPI aac_decoder_SetInputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    struct aac_decoder *decoder = aac_decoder_from_IMFTransform(iface);
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
        winedmo_transform_destroy(&decoder->winedmo_transform);

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
    if (flags & MFT_SET_TYPE_TEST_ONLY)
        return S_OK;

    if (!decoder->input_type && FAILED(hr = MFCreateMediaType(&decoder->input_type)))
        return hr;

    if (decoder->output_type)
    {
        IMFMediaType_Release(decoder->output_type);
        decoder->output_type = NULL;
    }

    return IMFMediaType_CopyAllItems(type, (IMFAttributes *)decoder->input_type);
}

static HRESULT WINAPI aac_decoder_SetOutputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    struct aac_decoder *decoder = aac_decoder_from_IMFTransform(iface);
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
        winedmo_transform_destroy(&decoder->winedmo_transform);

        return S_OK;
    }

    if (!decoder->input_type)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (FAILED(hr = MFCreateWaveFormatExFromMFMediaType(type, (WAVEFORMATEX **)&format, &size,
            MFWaveFormatExConvertFlag_ForceExtensible)))
        return hr;
    wfx = *format;
    CoTaskMemFree(format);

    for (i = 0; i < ARRAY_SIZE(aac_decoder_outputs); ++i)
        if (matches_format(&aac_decoder_outputs[i], &wfx))
            break;
    if (i == ARRAY_SIZE(aac_decoder_outputs))
        return MF_E_INVALIDMEDIATYPE;

    if (!wfx.Format.wBitsPerSample || !wfx.Format.nChannels || !wfx.Format.nSamplesPerSec)
        return MF_E_INVALIDMEDIATYPE;
    if (flags & MFT_SET_TYPE_TEST_ONLY)
        return S_OK;

    if (!decoder->output_type && FAILED(hr = MFCreateMediaType(&decoder->output_type)))
        return hr;

    if (FAILED(hr = IMFMediaType_CopyAllItems(type, (IMFAttributes *)decoder->output_type)))
        return hr;

    if (FAILED(hr = try_create_decoder(decoder)))
        goto failed;

    return S_OK;

failed:
    IMFMediaType_Release(decoder->output_type);
    decoder->output_type = NULL;
    return hr;
}

static HRESULT WINAPI aac_decoder_GetInputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **out)
{
    struct aac_decoder *decoder = aac_decoder_from_IMFTransform(iface);
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

static HRESULT WINAPI aac_decoder_GetOutputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **out)
{
    struct aac_decoder *decoder = aac_decoder_from_IMFTransform(iface);
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

static HRESULT WINAPI aac_decoder_GetInputStatus(IMFTransform *iface, DWORD id, DWORD *flags)
{
    struct aac_decoder *decoder = aac_decoder_from_IMFTransform(iface);

    TRACE("iface %p, id %#lx, flags %p.\n", iface, id, flags);

    if (!decoder->winedmo_transform.handle)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    *flags = decoder->input_sample ? 0 : MFT_INPUT_STATUS_ACCEPT_DATA;
    return S_OK;
}

static HRESULT WINAPI aac_decoder_GetOutputStatus(IMFTransform *iface, DWORD *flags)
{
    FIXME("iface %p, flags %p stub!\n", iface, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI aac_decoder_SetOutputBounds(IMFTransform *iface, LONGLONG lower, LONGLONG upper)
{
    TRACE("iface %p, lower %I64d, upper %I64d.\n", iface, lower, upper);
    return E_NOTIMPL;
}

static HRESULT WINAPI aac_decoder_ProcessEvent(IMFTransform *iface, DWORD id, IMFMediaEvent *event)
{
    FIXME("iface %p, id %#lx, event %p stub!\n", iface, id, event);
    return E_NOTIMPL;
}

static HRESULT WINAPI aac_decoder_ProcessMessage(IMFTransform *iface, MFT_MESSAGE_TYPE message, ULONG_PTR param)
{
    struct aac_decoder *decoder = aac_decoder_from_IMFTransform(iface);

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

static HRESULT WINAPI aac_decoder_ProcessInput(IMFTransform *iface, DWORD id, IMFSample *sample, DWORD flags)
{
    struct aac_decoder *decoder = aac_decoder_from_IMFTransform(iface);
    DMO_OUTPUT_DATA_BUFFER input = {0};
    IMFMediaBuffer *buffer;
    NTSTATUS status;
    HRESULT hr;

    TRACE("iface %p, id %#lx, sample %p, flags %#lx.\n", iface, id, sample, flags);

    if (!decoder->winedmo_transform.handle)
        return MF_E_TRANSFORM_TYPE_NOT_SET;
    if (decoder->input_sample)
        return MF_E_NOTACCEPTING;

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

static HRESULT WINAPI aac_decoder_ProcessOutput(IMFTransform *iface, DWORD flags, DWORD count,
        MFT_OUTPUT_DATA_BUFFER *samples, DWORD *samples_status)
{
    struct aac_decoder *decoder = aac_decoder_from_IMFTransform(iface);
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
        return E_INVALIDARG;

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
    {
        WARN("Failed to process output, hr %#lx\n", hr);
        samples->dwStatus = MFT_OUTPUT_DATA_BUFFER_NO_SAMPLE;
    }
    else
    {
        if (output.dwStatus & DMO_OUTPUT_DATA_BUFFERF_INCOMPLETE)
            samples->dwStatus = MFT_OUTPUT_DATA_BUFFER_INCOMPLETE;
    }

    if (decoder->input_sample) IMFSample_Release(decoder->input_sample);
    decoder->input_sample = NULL;
    return hr;
}

static const IMFTransformVtbl aac_decoder_vtbl =
{
    aac_decoder_QueryInterface,
    aac_decoder_AddRef,
    aac_decoder_Release,
    aac_decoder_GetStreamLimits,
    aac_decoder_GetStreamCount,
    aac_decoder_GetStreamIDs,
    aac_decoder_GetInputStreamInfo,
    aac_decoder_GetOutputStreamInfo,
    aac_decoder_GetAttributes,
    aac_decoder_GetInputStreamAttributes,
    aac_decoder_GetOutputStreamAttributes,
    aac_decoder_DeleteInputStream,
    aac_decoder_AddInputStreams,
    aac_decoder_GetInputAvailableType,
    aac_decoder_GetOutputAvailableType,
    aac_decoder_SetInputType,
    aac_decoder_SetOutputType,
    aac_decoder_GetInputCurrentType,
    aac_decoder_GetOutputCurrentType,
    aac_decoder_GetInputStatus,
    aac_decoder_GetOutputStatus,
    aac_decoder_SetOutputBounds,
    aac_decoder_ProcessEvent,
    aac_decoder_ProcessMessage,
    aac_decoder_ProcessInput,
    aac_decoder_ProcessOutput,
};

static HEAACWAVEINFO aac_decoder_inputs[] =
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

static HRESULT WINAPI aac_decoder_factory_CreateInstance(IClassFactory *iface, IUnknown *outer,
        REFIID riid, void **out)
{
    struct aac_decoder *decoder;
    NTSTATUS status;
    HRESULT hr;

    TRACE("%p, %s, %p.\n", outer, debugstr_guid(riid), out);

    if ((status = winedmo_transform_check(MFMediaType_Audio, MFAudioFormat_AAC, MFAudioFormat_Float)))
    {
        WARN("Unsupported winedmo transform, status %#lx.\n", status);
        return E_NOTIMPL;
    }

    *out = NULL;
    if (outer)
        return CLASS_E_NOAGGREGATION;
    if (!(decoder = calloc(1, sizeof(*decoder))))
        return E_OUTOFMEMORY;
    decoder->IMFTransform_iface.lpVtbl = &aac_decoder_vtbl;
    decoder->refcount = 1;

    decoder->input_types = (WAVEFORMATEXTENSIBLE *)aac_decoder_inputs;
    decoder->input_type_count = ARRAY_SIZE(aac_decoder_inputs);
    TRACE("created %p\n", decoder);

    hr = IUnknown_QueryInterface(&decoder->IMFTransform_iface, riid, out);
    IUnknown_Release(&decoder->IMFTransform_iface);
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

static const IClassFactoryVtbl aac_decoder_factory_vtbl =
{
    class_factory_QueryInterface,
    class_factory_AddRef,
    class_factory_Release,
    aac_decoder_factory_CreateInstance,
    class_factory_LockServer,
};

static IClassFactory aac_decoder_factory = {&aac_decoder_factory_vtbl};

/***********************************************************************
 *              DllGetClassObject (msauddecmft.@)
 */
HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void **out)
{
    if (IsEqualGUID(clsid, &CLSID_MSAACDecMFT))
        return IClassFactory_QueryInterface(&aac_decoder_factory, riid, out);

    *out = NULL;
    FIXME("Unknown clsid %s.\n", debugstr_guid(clsid));
    return CLASS_E_CLASSNOTAVAILABLE;
}

/***********************************************************************
 *              DllRegisterServer (msauddecmft.@)
 */
HRESULT WINAPI DllRegisterServer(void)
{
    MFT_REGISTER_TYPE_INFO aac_decoder_mft_inputs[] =
    {
        {MFMediaType_Audio, MFAudioFormat_AAC},
        {MFMediaType_Audio, MFAudioFormat_RAW_AAC},
        {MFMediaType_Audio, MFAudioFormat_ADTS},
    };
    MFT_REGISTER_TYPE_INFO aac_decoder_mft_outputs[] =
    {
        {MFMediaType_Audio, MFAudioFormat_Float},
        {MFMediaType_Audio, MFAudioFormat_PCM},
    };
    HRESULT hr;

    TRACE("\n");

    if (FAILED(hr = __wine_register_resources()))
        return hr;
    if (FAILED(hr = MFTRegister(CLSID_MSAACDecMFT, MFT_CATEGORY_AUDIO_DECODER,
            (WCHAR *)L"Microsoft AAC Audio Decoder MFT", MFT_ENUM_FLAG_SYNCMFT,
            ARRAY_SIZE(aac_decoder_mft_inputs), aac_decoder_mft_inputs,
            ARRAY_SIZE(aac_decoder_mft_outputs), aac_decoder_mft_outputs, NULL)))
        return hr;

    return S_OK;
}

/***********************************************************************
 *              DllUnregisterServer (msauddecmft.@)
 */
HRESULT WINAPI DllUnregisterServer(void)
{
    HRESULT hr;

    TRACE("\n");

    if (FAILED(hr = __wine_unregister_resources()))
        return hr;
    if (FAILED(hr = MFTUnregister(CLSID_MSAACDecMFT)))
        return hr;

    return S_OK;
}
