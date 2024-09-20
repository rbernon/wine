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

#include "quartz_private.h"
#include "mpegtype.h"

#include "wine/winedmo.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(quartz);

#include "initguid.h"

DEFINE_GUID(CLSID_MPEGLayer3Decoder,0x38be3000,0xdbf4,0x11d0,0x86,0x0e,0x00,0xa0,0x24,0xcf,0xef,0x6d);
DEFINE_GUID(MEDIASUBTYPE_MP3,WAVE_FORMAT_MPEGLAYER3,0x0000,0x0010,0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71);

struct transform
{
    struct strmbase_filter filter;
    IMpegAudioDecoder IMpegAudioDecoder_iface;

    struct strmbase_sink sink;
    struct strmbase_source source;
    struct strmbase_passthrough passthrough;

    IQualityControl sink_IQualityControl_iface;
    IQualityControl source_IQualityControl_iface;
    IQualityControl *qc_sink;

    struct winedmo_transform winedmo_transform;
    const struct transform_ops *ops;
    INT64 time_offset;
};

struct transform_ops
{
    HRESULT (*sink_query_accept)(struct transform *filter, const AM_MEDIA_TYPE *mt);
    HRESULT (*source_query_accept)(struct transform *filter, const AM_MEDIA_TYPE *mt);
    HRESULT (*source_get_media_type)(struct transform *filter, unsigned int index, AM_MEDIA_TYPE *mt);
    HRESULT (*source_decide_buffer_size)(struct transform *filter, IMemAllocator *allocator, ALLOCATOR_PROPERTIES *props);
    HRESULT (*source_qc_notify)(struct transform *filter, IBaseFilter *sender, Quality q);
};

static inline struct transform *impl_from_strmbase_filter(struct strmbase_filter *iface)
{
    return CONTAINING_RECORD(iface, struct transform, filter);
}

static struct strmbase_pin *transform_get_pin(struct strmbase_filter *iface, unsigned int index)
{
    struct transform *filter = impl_from_strmbase_filter(iface);
    if (index == 0)
        return &filter->sink.pin;
    if (index == 1)
        return &filter->source.pin;
    return NULL;
}

static void transform_destroy(struct strmbase_filter *iface)
{
    struct transform *filter = impl_from_strmbase_filter(iface);

    strmbase_passthrough_cleanup(&filter->passthrough);
    strmbase_source_cleanup(&filter->source);
    strmbase_sink_cleanup(&filter->sink);
    strmbase_filter_cleanup(&filter->filter);

    free(filter);
}

static HRESULT transform_query_interface(struct strmbase_filter *iface, REFIID iid, void **out)
{
    struct transform *filter = impl_from_strmbase_filter(iface);

    if (IsEqualGUID(iid, &IID_IMpegAudioDecoder) && filter->IMpegAudioDecoder_iface.lpVtbl)
        *out = &filter->IMpegAudioDecoder_iface;
    else
        return E_NOINTERFACE;

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static HRESULT am_media_type_to_winedmo_format(AM_MEDIA_TYPE *mt, union winedmo_format **format)
{
    IMFMediaType *media_type;
    HRESULT hr;
    UINT size;

    /* through IMFMediaType to normalize representation to MFVIDEOFORMAT / WAVEFORMATEX */
    if (FAILED(hr = MFCreateMediaTypeFromRepresentation(AM_MEDIA_TYPE_REPRESENTATION, mt, &media_type)))
        return hr;

    if (IsEqualGUID(&mt->majortype, &MEDIATYPE_Video))
        hr = MFCreateMFVideoFormatFromMFMediaType(media_type, (MFVIDEOFORMAT **)format, &size);
    else if (IsEqualGUID(&mt->majortype, &MEDIATYPE_Audio))
        hr = MFCreateWaveFormatExFromMFMediaType(media_type, (WAVEFORMATEX **)format, &size, 0);
    else
        hr = E_NOTIMPL;

    IMFMediaType_Release(media_type);

    return hr;
}

static HRESULT transform_init_stream(struct strmbase_filter *iface)
{
    struct transform *filter = impl_from_strmbase_filter(iface);
    NTSTATUS status;
    HRESULT hr;

    if (filter->source.pin.peer)
    {
        union winedmo_format *input_format, *output_format;

        /* through IMFMediaType to normalize representation to MFVIDEOFORMAT / WAVEFORMATEX */
        if (FAILED(hr = am_media_type_to_winedmo_format(&filter->sink.pin.mt, &input_format)))
            return hr;
        if (FAILED(hr = am_media_type_to_winedmo_format(&filter->source.pin.mt, &output_format)))
        {
            CoTaskMemFree(input_format);
            return hr;
        }

        if ((status = winedmo_transform_create(filter->sink.pin.mt.majortype, input_format, output_format, &filter->winedmo_transform)))
            hr = HRESULT_FROM_NT(status);
        CoTaskMemFree(output_format);
        CoTaskMemFree(input_format);

        if (FAILED(hr))
            return hr;

        hr = IMemAllocator_Commit(filter->source.pAllocator);
        if (FAILED(hr))
            ERR("Failed to commit allocator, hr %#lx.\n", hr);

        filter->time_offset = -1;
    }

    return S_OK;
}

static HRESULT transform_cleanup_stream(struct strmbase_filter *iface)
{
    struct transform *filter = impl_from_strmbase_filter(iface);

    if (filter->source.pin.peer)
    {
        IMemAllocator_Decommit(filter->source.pAllocator);

        EnterCriticalSection(&filter->filter.stream_cs);
        winedmo_transform_destroy(&filter->winedmo_transform);
        LeaveCriticalSection(&filter->filter.stream_cs);
    }

    return S_OK;
}

static const struct strmbase_filter_ops filter_ops =
{
    .filter_get_pin = transform_get_pin,
    .filter_destroy = transform_destroy,
    .filter_query_interface = transform_query_interface,
    .filter_init_stream = transform_init_stream,
    .filter_cleanup_stream = transform_cleanup_stream,
};

static struct transform *impl_from_IMpegAudioDecoder(IMpegAudioDecoder *iface)
{
    return CONTAINING_RECORD(iface, struct transform, IMpegAudioDecoder_iface);
}

static HRESULT WINAPI mpeg_audio_decoder_QueryInterface(IMpegAudioDecoder *iface,
        REFIID iid, void **out)
{
    struct transform *filter = impl_from_IMpegAudioDecoder(iface);
    return IUnknown_QueryInterface(filter->filter.outer_unk, iid, out);
}

static ULONG WINAPI mpeg_audio_decoder_AddRef(IMpegAudioDecoder *iface)
{
    struct transform *filter = impl_from_IMpegAudioDecoder(iface);
    return IUnknown_AddRef(filter->filter.outer_unk);
}

static ULONG WINAPI mpeg_audio_decoder_Release(IMpegAudioDecoder *iface)
{
    struct transform *filter = impl_from_IMpegAudioDecoder(iface);
    return IUnknown_Release(filter->filter.outer_unk);
}

static HRESULT WINAPI mpeg_audio_decoder_get_FrequencyDivider(IMpegAudioDecoder *iface, ULONG *divider)
{
    FIXME("iface %p, divider %p, stub!\n", iface, divider);
    return E_NOTIMPL;
}

static HRESULT WINAPI mpeg_audio_decoder_put_FrequencyDivider(IMpegAudioDecoder *iface, ULONG divider)
{
    FIXME("iface %p, divider %lu, stub!\n", iface, divider);
    return E_NOTIMPL;
}

static HRESULT WINAPI mpeg_audio_decoder_get_DecoderAccuracy(IMpegAudioDecoder *iface, ULONG *accuracy)
{
    FIXME("iface %p, accuracy %p, stub!\n", iface, accuracy);
    return E_NOTIMPL;
}

static HRESULT WINAPI mpeg_audio_decoder_put_DecoderAccuracy(IMpegAudioDecoder *iface, ULONG accuracy)
{
    FIXME("iface %p, accuracy %lu, stub!\n", iface, accuracy);
    return E_NOTIMPL;
}

static HRESULT WINAPI mpeg_audio_decoder_get_Stereo(IMpegAudioDecoder *iface, ULONG *stereo)
{
    FIXME("iface %p, stereo %p, stub!\n", iface, stereo);
    return E_NOTIMPL;
}

static HRESULT WINAPI mpeg_audio_decoder_put_Stereo(IMpegAudioDecoder *iface, ULONG stereo)
{
    FIXME("iface %p, stereo %lu, stub!\n", iface, stereo);
    return E_NOTIMPL;
}

static HRESULT WINAPI mpeg_audio_decoder_get_DecoderWordSize(IMpegAudioDecoder *iface, ULONG *word_size)
{
    FIXME("iface %p, word_size %p, stub!\n", iface, word_size);
    return E_NOTIMPL;
}

static HRESULT WINAPI mpeg_audio_decoder_put_DecoderWordSize(IMpegAudioDecoder *iface, ULONG word_size)
{
    FIXME("iface %p, word_size %lu, stub!\n", iface, word_size);
    return E_NOTIMPL;
}

static HRESULT WINAPI mpeg_audio_decoder_get_IntegerDecode(IMpegAudioDecoder *iface, ULONG *integer_decode)
{
    FIXME("iface %p, integer_decode %p, stub!\n", iface, integer_decode);
    return E_NOTIMPL;
}

static HRESULT WINAPI mpeg_audio_decoder_put_IntegerDecode(IMpegAudioDecoder *iface, ULONG integer_decode)
{
    FIXME("iface %p, integer_decode %lu, stub!\n", iface, integer_decode);
    return E_NOTIMPL;
}

static HRESULT WINAPI mpeg_audio_decoder_get_DualMode(IMpegAudioDecoder *iface, ULONG *dual_mode)
{
    FIXME("iface %p, dual_mode %p, stub!\n", iface, dual_mode);
    return E_NOTIMPL;
}

static HRESULT WINAPI mpeg_audio_decoder_put_DualMode(IMpegAudioDecoder *iface, ULONG dual_mode)
{
    FIXME("iface %p, dual_mode %lu, stub!\n", iface, dual_mode);
    return E_NOTIMPL;
}

static HRESULT WINAPI mpeg_audio_decoder_get_AudioFormat(IMpegAudioDecoder *iface, MPEG1WAVEFORMAT *format)
{
    FIXME("iface %p, format %p, stub!\n", iface, format);
    return E_NOTIMPL;
}

static const IMpegAudioDecoderVtbl mpeg_audio_decoder_vtbl =
{
    mpeg_audio_decoder_QueryInterface,
    mpeg_audio_decoder_AddRef,
    mpeg_audio_decoder_Release,
    mpeg_audio_decoder_get_FrequencyDivider,
    mpeg_audio_decoder_put_FrequencyDivider,
    mpeg_audio_decoder_get_DecoderAccuracy,
    mpeg_audio_decoder_put_DecoderAccuracy,
    mpeg_audio_decoder_get_Stereo,
    mpeg_audio_decoder_put_Stereo,
    mpeg_audio_decoder_get_DecoderWordSize,
    mpeg_audio_decoder_put_DecoderWordSize,
    mpeg_audio_decoder_get_IntegerDecode,
    mpeg_audio_decoder_put_IntegerDecode,
    mpeg_audio_decoder_get_DualMode,
    mpeg_audio_decoder_put_DualMode,
    mpeg_audio_decoder_get_AudioFormat,
};

static HRESULT transform_sink_query_accept(struct strmbase_pin *pin, const AM_MEDIA_TYPE *mt)
{
    struct transform *filter = impl_from_strmbase_filter(pin->filter);

    return filter->ops->sink_query_accept(filter, mt);
}

static HRESULT transform_sink_query_interface(struct strmbase_pin *pin, REFIID iid, void **out)
{
    struct transform *filter = impl_from_strmbase_filter(pin->filter);

    if (IsEqualGUID(iid, &IID_IMemInputPin))
        *out = &filter->sink.IMemInputPin_iface;
    else if (IsEqualGUID(iid, &IID_IQualityControl))
        *out = &filter->sink_IQualityControl_iface;
    else
        return E_NOINTERFACE;

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}


struct buffer_wrapper
{
    IMediaBuffer IMediaBuffer_iface;
    LONG refcount;

    IMediaSample *buffer;
};

static struct buffer_wrapper *buffer_wrapper_from_IMediaBuffer(IMediaBuffer *iface)
{
    return CONTAINING_RECORD(iface, struct buffer_wrapper, IMediaBuffer_iface);
}

static HRESULT WINAPI buffer_wrapper_QueryInterface(IMediaBuffer *iface, REFIID iid, void **out)
{
    struct buffer_wrapper *wrapper = buffer_wrapper_from_IMediaBuffer(iface);

    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IMediaBuffer))
        *out = &wrapper->IMediaBuffer_iface;
    else
    {
        *out = NULL;
        WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static ULONG WINAPI buffer_wrapper_AddRef(IMediaBuffer *iface)
{
    struct buffer_wrapper *wrapper = buffer_wrapper_from_IMediaBuffer(iface);
    ULONG refcount = InterlockedIncrement(&wrapper->refcount);
    TRACE("iface %p increasing refcount to %lu.\n", wrapper, refcount);
    return refcount;
}

static ULONG WINAPI buffer_wrapper_Release(IMediaBuffer *iface)
{
    struct buffer_wrapper *wrapper = buffer_wrapper_from_IMediaBuffer(iface);
    ULONG refcount = InterlockedDecrement(&wrapper->refcount);

    TRACE("iface %p decreasing refcount to %lu.\n", wrapper, refcount);

    if (!refcount)
    {
        IMediaSample_Release(wrapper->buffer);
        free(wrapper);
    }

    return refcount;
}

static HRESULT WINAPI buffer_wrapper_SetLength(IMediaBuffer *iface, DWORD length)
{
    struct buffer_wrapper *wrapper = buffer_wrapper_from_IMediaBuffer(iface);
    return IMediaSample_SetActualDataLength(wrapper->buffer, length);
}

static HRESULT WINAPI buffer_wrapper_GetMaxLength(IMediaBuffer *iface, DWORD *max_length)
{
    struct buffer_wrapper *wrapper = buffer_wrapper_from_IMediaBuffer(iface);
    *max_length = IMediaSample_GetSize(wrapper->buffer);
    return S_OK;
}

static HRESULT WINAPI buffer_wrapper_GetBufferAndLength(IMediaBuffer *iface, BYTE **buffer, DWORD *length)
{
    struct buffer_wrapper *wrapper = buffer_wrapper_from_IMediaBuffer(iface);
    *length = IMediaSample_GetActualDataLength(wrapper->buffer);
    return IMediaSample_GetPointer(wrapper->buffer, buffer);
}

static const IMediaBufferVtbl buffer_wrapper_vtbl =
{
    buffer_wrapper_QueryInterface,
    buffer_wrapper_AddRef,
    buffer_wrapper_Release,
    buffer_wrapper_SetLength,
    buffer_wrapper_GetMaxLength,
    buffer_wrapper_GetBufferAndLength,
};

static HRESULT buffer_wrapper_create( IMediaSample *buffer, IMediaBuffer **out )
{
    struct buffer_wrapper *wrapper;

    *out = NULL;
    if (!(wrapper = calloc(1, sizeof(*wrapper))))
        return E_OUTOFMEMORY;
    wrapper->IMediaBuffer_iface.lpVtbl = &buffer_wrapper_vtbl;
    wrapper->refcount = 1;

    IMediaSample_AddRef((wrapper->buffer = buffer));
    *out = &wrapper->IMediaBuffer_iface;
    return S_OK;
}


static HRESULT WINAPI transform_sink_receive(struct strmbase_sink *pin, IMediaSample *media_sample)
{
    struct transform *filter = impl_from_strmbase_filter(pin->pin.filter);
    DMO_OUTPUT_DATA_BUFFER input = {0};
    NTSTATUS status;
    HRESULT hr;

    /* We do not expect pin connection state to change while the filter is
     * running. This guarantee is necessary, since otherwise we would have to
     * take the filter lock, and we can't take the filter lock from a streaming
     * thread. */
    if (!filter->source.pMemInputPin)
    {
        WARN("Source is not connected, returning VFW_E_NOT_CONNECTED.\n");
        return VFW_E_NOT_CONNECTED;
    }

    if (filter->filter.state == State_Stopped)
        return VFW_E_WRONG_STATE;

    if (filter->sink.flushing)
        return S_FALSE;

    hr = buffer_wrapper_create(media_sample, &input.pBuffer);
    if (FAILED(hr))
        return hr;
    if (IMediaSample_GetTime(media_sample, &input.rtTimestamp, &input.rtTimelength) == S_OK)
        input.dwStatus |= DMO_OUTPUT_DATA_BUFFERF_TIMELENGTH;
    if (filter->time_offset == -1)
        filter->time_offset = input.rtTimestamp;
    input.rtTimestamp = 0;

    if (IMediaSample_IsSyncPoint(media_sample) == S_OK)
        input.dwStatus |= DMO_OUTPUT_DATA_BUFFERF_SYNCPOINT;

    if ((status = winedmo_transform_process_input(filter->winedmo_transform, &input)))
        hr = HRESULT_FROM_NT(status);
    IMediaBuffer_Release(input.pBuffer);
    if (FAILED(hr))
        return hr;

    for (;;)
    {
        DMO_OUTPUT_DATA_BUFFER output = {0};
        IMediaSample *output_sample;

        hr = IMemAllocator_GetBuffer(filter->source.pAllocator, &output_sample, NULL, NULL, 0);
        if (FAILED(hr))
            return hr;

        hr = buffer_wrapper_create(output_sample, &output.pBuffer);
        if (FAILED(hr))
        {
            IMediaSample_Release(output_sample);
            return hr;
        }

        if ((status = winedmo_transform_process_output(filter->winedmo_transform, &output)))
        {
            if (status == STATUS_PENDING) hr = E_PENDING;
            else hr = HRESULT_FROM_NT(status);
        }
        IMediaBuffer_Release(output.pBuffer);

        if (hr == E_PENDING)
        {
            IMediaSample_Release(output_sample);
            break;
        }
        if (FAILED(hr))
        {
            IMediaSample_Release(output_sample);
            return hr;
        }

        if (output.dwStatus & DMO_OUTPUT_DATA_BUFFERF_TIME)
        {
            output.rtTimestamp += filter->time_offset;
            if (!(output.dwStatus & DMO_OUTPUT_DATA_BUFFERF_TIMELENGTH))
                IMediaSample_SetTime(output_sample, &output.rtTimestamp, NULL);
            else
            {
                output.rtTimelength += output.rtTimestamp;
                IMediaSample_SetTime(output_sample, &output.rtTimestamp, &output.rtTimelength);
            }
        }

        if (output.dwStatus & DMO_OUTPUT_DATA_BUFFERF_SYNCPOINT)
            IMediaSample_SetSyncPoint(output_sample, TRUE);
        else
            IMediaSample_SetSyncPoint(output_sample, FALSE);

        hr = IMemInputPin_Receive(filter->source.pMemInputPin, output_sample);
        if (FAILED(hr))
        {
            IMediaSample_Release(output_sample);
            return hr;
        }

        IMediaSample_Release(output_sample);
    }

    return S_OK;
}

static const struct strmbase_sink_ops sink_ops =
{
    .base.pin_query_accept = transform_sink_query_accept,
    .base.pin_query_interface = transform_sink_query_interface,
    .pfnReceive = transform_sink_receive,
};

static HRESULT transform_source_query_accept(struct strmbase_pin *pin, const AM_MEDIA_TYPE *mt)
{
    struct transform *filter = impl_from_strmbase_filter(pin->filter);

    return filter->ops->source_query_accept(filter, mt);
}

static HRESULT transform_source_get_media_type(struct strmbase_pin *pin, unsigned int index, AM_MEDIA_TYPE *mt)
{
    struct transform *filter = impl_from_strmbase_filter(pin->filter);

    return filter->ops->source_get_media_type(filter, index, mt);
}

static HRESULT transform_source_query_interface(struct strmbase_pin *pin, REFIID iid, void **out)
{
    struct transform *filter = impl_from_strmbase_filter(pin->filter);

    if (IsEqualGUID(iid, &IID_IMediaPosition))
        *out = &filter->passthrough.IMediaPosition_iface;
    else if (IsEqualGUID(iid, &IID_IMediaSeeking))
        *out = &filter->passthrough.IMediaSeeking_iface;
    else if (IsEqualGUID(iid, &IID_IQualityControl))
        *out = &filter->source_IQualityControl_iface;
    else
        return E_NOINTERFACE;

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static HRESULT WINAPI transform_source_DecideBufferSize(struct strmbase_source *pin, IMemAllocator *allocator, ALLOCATOR_PROPERTIES *props)
{
    struct transform *filter = impl_from_strmbase_filter(pin->pin.filter);

    return filter->ops->source_decide_buffer_size(filter, allocator, props);
}

static const struct strmbase_source_ops source_ops =
{
    .base.pin_query_accept = transform_source_query_accept,
    .base.pin_get_media_type = transform_source_get_media_type,
    .base.pin_query_interface = transform_source_query_interface,
    .pfnAttemptConnection = BaseOutputPinImpl_AttemptConnection,
    .pfnDecideAllocator = BaseOutputPinImpl_DecideAllocator,
    .pfnDecideBufferSize = transform_source_DecideBufferSize,
};

static struct transform *impl_from_sink_IQualityControl(IQualityControl *iface)
{
    return CONTAINING_RECORD(iface, struct transform, sink_IQualityControl_iface);
}

static HRESULT WINAPI sink_quality_control_QueryInterface(IQualityControl *iface, REFIID iid, void **out)
{
    struct transform *filter = impl_from_sink_IQualityControl(iface);
    return IPin_QueryInterface(&filter->source.pin.IPin_iface, iid, out);
}

static ULONG WINAPI sink_quality_control_AddRef(IQualityControl *iface)
{
    struct transform *filter = impl_from_sink_IQualityControl(iface);
    return IPin_AddRef(&filter->source.pin.IPin_iface);
}

static ULONG WINAPI sink_quality_control_Release(IQualityControl *iface)
{
    struct transform *filter = impl_from_sink_IQualityControl(iface);
    return IPin_Release(&filter->source.pin.IPin_iface);
}

static HRESULT WINAPI sink_quality_control_Notify(IQualityControl *iface, IBaseFilter *sender, Quality q)
{
    struct transform *filter = impl_from_sink_IQualityControl(iface);

    TRACE("filter %p, sender %p, type %#x, proportion %ld, late %s, timestamp %s.\n",
            filter, sender, q.Type, q.Proportion, debugstr_time(q.Late), debugstr_time(q.TimeStamp));

    return S_OK;
}

static HRESULT WINAPI sink_quality_control_SetSink(IQualityControl *iface, IQualityControl *sink)
{
    struct transform *filter = impl_from_sink_IQualityControl(iface);

    TRACE("filter %p, sink %p.\n", filter, sink);

    filter->qc_sink = sink;

    return S_OK;
}

static const IQualityControlVtbl sink_quality_control_vtbl =
{
    sink_quality_control_QueryInterface,
    sink_quality_control_AddRef,
    sink_quality_control_Release,
    sink_quality_control_Notify,
    sink_quality_control_SetSink,
};

static struct transform *impl_from_source_IQualityControl(IQualityControl *iface)
{
    return CONTAINING_RECORD(iface, struct transform, source_IQualityControl_iface);
}

static HRESULT WINAPI source_quality_control_QueryInterface(IQualityControl *iface, REFIID iid, void **out)
{
    struct transform *filter = impl_from_source_IQualityControl(iface);
    return IPin_QueryInterface(&filter->source.pin.IPin_iface, iid, out);
}

static ULONG WINAPI source_quality_control_AddRef(IQualityControl *iface)
{
    struct transform *filter = impl_from_source_IQualityControl(iface);
    return IPin_AddRef(&filter->source.pin.IPin_iface);
}

static ULONG WINAPI source_quality_control_Release(IQualityControl *iface)
{
    struct transform *filter = impl_from_source_IQualityControl(iface);
    return IPin_Release(&filter->source.pin.IPin_iface);
}

static HRESULT WINAPI source_quality_control_Notify(IQualityControl *iface, IBaseFilter *sender, Quality q)
{
    struct transform *filter = impl_from_source_IQualityControl(iface);

    return filter->ops->source_qc_notify(filter, sender, q);
}

static HRESULT WINAPI source_quality_control_SetSink(IQualityControl *iface, IQualityControl *sink)
{
    struct transform *filter = impl_from_source_IQualityControl(iface);

    TRACE("filter %p, sink %p.\n", filter, sink);

    return S_OK;
}

static const IQualityControlVtbl source_quality_control_vtbl =
{
    source_quality_control_QueryInterface,
    source_quality_control_AddRef,
    source_quality_control_Release,
    source_quality_control_Notify,
    source_quality_control_SetSink,
};

static HRESULT transform_create(IUnknown *outer, const CLSID *clsid, const struct transform_ops *ops, struct transform **out)
{
    struct transform *object;

    object = calloc(1, sizeof(*object));
    if (!object)
        return E_OUTOFMEMORY;

    strmbase_filter_init(&object->filter, outer, clsid, &filter_ops);
    strmbase_sink_init(&object->sink, &object->filter, L"In", &sink_ops, NULL);
    strmbase_source_init(&object->source, &object->filter, L"Out", &source_ops);

    strmbase_passthrough_init(&object->passthrough, (IUnknown *)&object->source.pin.IPin_iface);
    ISeekingPassThru_Init(&object->passthrough.ISeekingPassThru_iface, FALSE,
            &object->sink.pin.IPin_iface);

    object->sink_IQualityControl_iface.lpVtbl = &sink_quality_control_vtbl;
    object->source_IQualityControl_iface.lpVtbl = &source_quality_control_vtbl;

    object->ops = ops;

    *out = object;
    return S_OK;
}

static HRESULT passthrough_source_qc_notify(struct transform *filter, IBaseFilter *sender, Quality q)
{
    IQualityControl *peer;
    HRESULT hr = VFW_E_NOT_FOUND;

    TRACE("filter %p, sender %p, type %s, proportion %ld, late %s, timestamp %s.\n",
            filter, sender, q.Type == Famine ? "Famine" : "Flood", q.Proportion,
            debugstr_time(q.Late), debugstr_time(q.TimeStamp));

    if (filter->qc_sink)
        return IQualityControl_Notify(filter->qc_sink, &filter->filter.IBaseFilter_iface, q);

    if (filter->sink.pin.peer
            && SUCCEEDED(IPin_QueryInterface(filter->sink.pin.peer, &IID_IQualityControl, (void **)&peer)))
    {
        hr = IQualityControl_Notify(peer, &filter->filter.IBaseFilter_iface, q);
        IQualityControl_Release(peer);
    }

    return hr;
}

static HRESULT handle_source_qc_notify(struct transform *filter, IBaseFilter *sender, Quality q)
{
    UINT64 timestamp;
    INT64 diff;

    TRACE("filter %p, sender %p, type %s, proportion %ld, late %s, timestamp %s.\n",
            filter, sender, q.Type == Famine ? "Famine" : "Flood", q.Proportion,
            debugstr_time(q.Late), debugstr_time(q.TimeStamp));

    /* DirectShow filters sometimes pass negative timestamps (Audiosurf uses the
     * current time instead of the time of the last buffer). GstClockTime is
     * unsigned, so clamp it to 0. */
    timestamp = max(q.TimeStamp, 0);

    /* The documentation specifies that timestamp + diff must be nonnegative. */
    diff = q.Late;
    if (diff < 0 && timestamp < (UINT64)-diff)
        diff = -timestamp;

    /* DirectShow "Proportion" describes what percentage of buffers the upstream
     * filter should keep (i.e. dropping the rest). If frames are late, the
     * proportion will be less than 1. For example, a proportion of 500 means
     * that the element should drop half of its frames, essentially because
     * frames are taking twice as long as they should to arrive.
     *
     * GStreamer "proportion" is the inverse of this; it describes how much
     * faster the upstream element should produce frames. I.e. if frames are
     * taking twice as long as they should to arrive, we want the frames to be
     * decoded twice as fast, and so we pass 2.0 to GStreamer. */

    if (!q.Proportion)
    {
        WARN("Ignoring quality message with zero proportion.\n");
        return S_OK;
    }

    return S_OK;
}

static HRESULT mpeg_layer3_decoder_sink_query_accept(struct transform *filter, const AM_MEDIA_TYPE *mt)
{
    const MPEGLAYER3WAVEFORMAT *format;

    if (!IsEqualGUID(&mt->majortype, &MEDIATYPE_Audio)
            || !IsEqualGUID(&mt->formattype, &FORMAT_WaveFormatEx)
            || mt->cbFormat < sizeof(MPEGLAYER3WAVEFORMAT))
        return S_FALSE;

    format = (const MPEGLAYER3WAVEFORMAT *)mt->pbFormat;

    if (format->wfx.wFormatTag != WAVE_FORMAT_MPEGLAYER3)
        return S_FALSE;

    return S_OK;
}

static HRESULT mpeg_layer3_decoder_source_query_accept(struct transform *filter, const AM_MEDIA_TYPE *mt)
{
    if (!filter->sink.pin.peer)
        return S_FALSE;

    if (!IsEqualGUID(&mt->majortype, &MEDIATYPE_Audio)
            || !IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_PCM))
        return S_FALSE;

    return S_OK;
}

static HRESULT mpeg_layer3_decoder_source_get_media_type(struct transform *filter, unsigned int index, AM_MEDIA_TYPE *mt)
{
    const MPEGLAYER3WAVEFORMAT *input_format;
    WAVEFORMATEX *output_format;

    if (!filter->sink.pin.peer)
        return VFW_S_NO_MORE_ITEMS;

    if (index > 0)
        return VFW_S_NO_MORE_ITEMS;

    input_format = (const MPEGLAYER3WAVEFORMAT *)filter->sink.pin.mt.pbFormat;

    output_format = CoTaskMemAlloc(sizeof(*output_format));
    if (!output_format)
        return E_OUTOFMEMORY;

    memset(output_format, 0, sizeof(*output_format));
    output_format->wFormatTag = WAVE_FORMAT_PCM;
    output_format->nSamplesPerSec = input_format->wfx.nSamplesPerSec;
    output_format->nChannels = input_format->wfx.nChannels;
    output_format->wBitsPerSample = 16;
    output_format->nBlockAlign = output_format->nChannels * output_format->wBitsPerSample / 8;
    output_format->nAvgBytesPerSec = output_format->nBlockAlign * output_format->nSamplesPerSec;

    memset(mt, 0, sizeof(*mt));
    mt->majortype = MEDIATYPE_Audio;
    mt->subtype = MEDIASUBTYPE_PCM;
    mt->bFixedSizeSamples = TRUE;
    mt->lSampleSize = 1152 * output_format->nBlockAlign;
    mt->formattype = FORMAT_WaveFormatEx;
    mt->cbFormat = sizeof(*output_format);
    mt->pbFormat = (BYTE *)output_format;

    return S_OK;
}

static HRESULT mpeg_layer3_decoder_source_decide_buffer_size(struct transform *filter, IMemAllocator *allocator, ALLOCATOR_PROPERTIES *props)
{
    ALLOCATOR_PROPERTIES ret_props;

    props->cBuffers = max(props->cBuffers, 8);
    props->cbBuffer = max(props->cbBuffer, filter->source.pin.mt.lSampleSize * 4);
    props->cbAlign = max(props->cbAlign, 1);

    return IMemAllocator_SetProperties(allocator, props, &ret_props);
}

static const struct transform_ops mpeg_layer3_decoder_transform_ops =
{
    mpeg_layer3_decoder_sink_query_accept,
    mpeg_layer3_decoder_source_query_accept,
    mpeg_layer3_decoder_source_get_media_type,
    mpeg_layer3_decoder_source_decide_buffer_size,
    passthrough_source_qc_notify,
};

HRESULT mpeg_layer3_decoder_create(IUnknown *outer, IUnknown **out)
{
    struct transform *object;
    NTSTATUS status;
    HRESULT hr;

    if ((status = winedmo_transform_check(MEDIATYPE_Audio, MEDIASUBTYPE_MP3, MEDIASUBTYPE_PCM)))
    {
        static const GUID CLSID_wg_mp3_decoder = {0x84cd8e3e,0xb221,0x434a,{0x88,0x82,0x9d,0x6c,0x8d,0xf4,0x90,0xe1}};
        WARN("Unsupported winedmo transform, status %#lx.\n", status);
        return CoCreateInstance(&CLSID_wg_mp3_decoder, outer, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)out);
    }

    hr = transform_create(outer, &CLSID_MPEGLayer3Decoder, &mpeg_layer3_decoder_transform_ops, &object);
    if (FAILED(hr))
        return hr;

    wcscpy(object->sink.pin.name, L"XForm In");
    wcscpy(object->source.pin.name, L"XForm Out");

    TRACE("Created MPEG layer-3 decoder %p.\n", object);
    *out = &object->filter.IUnknown_inner;
    return hr;
}

static HRESULT mpeg_audio_codec_sink_query_accept(struct transform *filter, const AM_MEDIA_TYPE *mt)
{
    const MPEG1WAVEFORMAT *format;

    if (!IsEqualGUID(&mt->majortype, &MEDIATYPE_Audio))
        return S_FALSE;

    if (!IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_MPEG1Packet)
            && !IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_MPEG1Payload)
            && !IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_MPEG1AudioPayload)
            && !IsEqualGUID(&mt->subtype, &GUID_NULL))
        return S_FALSE;

    if (!IsEqualGUID(&mt->formattype, &FORMAT_WaveFormatEx)
            || mt->cbFormat < sizeof(MPEG1WAVEFORMAT))
        return S_FALSE;

    format = (const MPEG1WAVEFORMAT *)mt->pbFormat;

    if (format->wfx.wFormatTag != WAVE_FORMAT_MPEG
            || format->fwHeadLayer == ACM_MPEG_LAYER3)
        return S_FALSE;

    return S_OK;
}

static HRESULT mpeg_audio_codec_source_query_accept(struct transform *filter, const AM_MEDIA_TYPE *mt)
{
    const MPEG1WAVEFORMAT *input_format;
    const WAVEFORMATEX *output_format;
    DWORD expected_avg_bytes_per_sec;
    WORD expected_block_align;

    if (!filter->sink.pin.peer)
        return S_FALSE;

    if (!IsEqualGUID(&mt->majortype, &MEDIATYPE_Audio)
            || !IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_PCM)
            || !IsEqualGUID(&mt->formattype, &FORMAT_WaveFormatEx)
            || mt->cbFormat < sizeof(WAVEFORMATEX))
        return S_FALSE;

    input_format = (const MPEG1WAVEFORMAT *)filter->sink.pin.mt.pbFormat;
    output_format = (const WAVEFORMATEX *)mt->pbFormat;

    if (output_format->wFormatTag != WAVE_FORMAT_PCM
            || input_format->wfx.nSamplesPerSec != output_format->nSamplesPerSec
            || input_format->wfx.nChannels != output_format->nChannels
            || (output_format->wBitsPerSample != 8 && output_format->wBitsPerSample != 16))
        return S_FALSE;

    expected_block_align = output_format->nChannels * output_format->wBitsPerSample / 8;
    expected_avg_bytes_per_sec = expected_block_align * output_format->nSamplesPerSec;

    if (output_format->nBlockAlign != expected_block_align
            || output_format->nAvgBytesPerSec != expected_avg_bytes_per_sec)
        return S_FALSE;

    return S_OK;
}

static HRESULT mpeg_audio_codec_source_get_media_type(struct transform *filter, unsigned int index, AM_MEDIA_TYPE *mt)
{
    const MPEG1WAVEFORMAT *input_format;
    WAVEFORMATEX *output_format;

    if (!filter->sink.pin.peer)
        return VFW_S_NO_MORE_ITEMS;

    if (index > 1)
        return VFW_S_NO_MORE_ITEMS;

    input_format = (const MPEG1WAVEFORMAT *)filter->sink.pin.mt.pbFormat;

    output_format = CoTaskMemAlloc(sizeof(*output_format));
    if (!output_format)
        return E_OUTOFMEMORY;

    memset(output_format, 0, sizeof(*output_format));
    output_format->wFormatTag = WAVE_FORMAT_PCM;
    output_format->nSamplesPerSec = input_format->wfx.nSamplesPerSec;
    output_format->nChannels = input_format->wfx.nChannels;
    output_format->wBitsPerSample = index ? 8 : 16;
    output_format->nBlockAlign = output_format->nChannels * output_format->wBitsPerSample / 8;
    output_format->nAvgBytesPerSec = output_format->nBlockAlign * output_format->nSamplesPerSec;

    memset(mt, 0, sizeof(*mt));
    mt->majortype = MEDIATYPE_Audio;
    mt->subtype = MEDIASUBTYPE_PCM;
    mt->bFixedSizeSamples = TRUE;
    mt->lSampleSize = output_format->nBlockAlign;
    mt->formattype = FORMAT_WaveFormatEx;
    mt->cbFormat = sizeof(*output_format);
    mt->pbFormat = (BYTE *)output_format;

    return S_OK;
}

static HRESULT mpeg_audio_codec_source_decide_buffer_size(struct transform *filter, IMemAllocator *allocator, ALLOCATOR_PROPERTIES *props)
{
    MPEG1WAVEFORMAT *input_format = (MPEG1WAVEFORMAT *)filter->sink.pin.mt.pbFormat;
    WAVEFORMATEX *output_format = (WAVEFORMATEX *)filter->source.pin.mt.pbFormat;
    LONG frame_samples = (input_format->fwHeadLayer & ACM_MPEG_LAYER2) ? 1152 : 384;
    LONG frame_size = frame_samples * output_format->nBlockAlign;
    ALLOCATOR_PROPERTIES ret_props;

    props->cBuffers = max(props->cBuffers, 8);
    props->cbBuffer = max(props->cbBuffer, frame_size * 4);
    props->cbAlign = max(props->cbAlign, 1);

    return IMemAllocator_SetProperties(allocator, props, &ret_props);
}

static const struct transform_ops mpeg_audio_codec_transform_ops =
{
    mpeg_audio_codec_sink_query_accept,
    mpeg_audio_codec_source_query_accept,
    mpeg_audio_codec_source_get_media_type,
    mpeg_audio_codec_source_decide_buffer_size,
    passthrough_source_qc_notify,
};

HRESULT mpeg_audio_codec_create(IUnknown *outer, IUnknown **out)
{
    struct transform *object;
    NTSTATUS status;
    HRESULT hr;

    if ((status = winedmo_transform_check(MEDIATYPE_Audio, MEDIASUBTYPE_MPEG1Audio, MEDIASUBTYPE_PCM)))
    {
        static const GUID CLSID_wg_mpeg_audio_decoder = {0xc9f285f8,0x4380,0x4121,{0x97,0x1f,0x49,0xa9,0x53,0x16,0xc2,0x7b}};
        WARN("Unsupported winedmo transform, status %#lx.\n", status);
        return CoCreateInstance(&CLSID_wg_mpeg_audio_decoder, outer, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)out);
    }

    hr = transform_create(outer, &CLSID_CMpegAudioCodec, &mpeg_audio_codec_transform_ops, &object);
    if (FAILED(hr))
        return hr;

    wcscpy(object->sink.pin.name, L"XForm In");
    wcscpy(object->source.pin.name, L"XForm Out");

    object->IMpegAudioDecoder_iface.lpVtbl = &mpeg_audio_decoder_vtbl;

    TRACE("Created MPEG audio decoder %p.\n", object);
    *out = &object->filter.IUnknown_inner;
    return hr;
}

static HRESULT mpeg_video_codec_sink_query_accept(struct transform *filter, const AM_MEDIA_TYPE *mt)
{
    if (!IsEqualGUID(&mt->majortype, &MEDIATYPE_Video)
            || !IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_MPEG1Payload)
            || !IsEqualGUID(&mt->formattype, &FORMAT_MPEGVideo)
            || mt->cbFormat < sizeof(MPEG1VIDEOINFO))
        return S_FALSE;

    return S_OK;
}

static HRESULT mpeg_video_codec_source_query_accept(struct transform *filter, const AM_MEDIA_TYPE *mt)
{
    if (!filter->sink.pin.peer)
        return S_FALSE;

    if (!IsEqualGUID(&mt->majortype, &MEDIATYPE_Video)
            || !IsEqualGUID(&mt->formattype, &FORMAT_VideoInfo)
            || mt->cbFormat < sizeof(VIDEOINFOHEADER))
        return S_FALSE;

    if (!IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_YV12)
            /* missing: MEDIASUBTYPE_Y41P, not supported by GStreamer */
            && !IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_YUY2)
            && !IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_UYVY)
            && !IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_RGB24)
            && !IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_RGB32)
            && !IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_RGB565)
            && !IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_RGB555)
            /* missing: MEDIASUBTYPE_RGB8, not supported by GStreamer */)
        return S_FALSE;

    return S_OK;
}

static HRESULT mpeg_video_codec_source_get_media_type(struct transform *filter, unsigned int index, AM_MEDIA_TYPE *mt)
{
    static const GUID *formats[] =
    {
        &MEDIASUBTYPE_YV12,
        &MEDIASUBTYPE_YUY2,
        &MEDIASUBTYPE_UYVY,
        &MEDIASUBTYPE_RGB24,
        &MEDIASUBTYPE_RGB32,
        &MEDIASUBTYPE_RGB565,
        &MEDIASUBTYPE_RGB555,
    };

    const MPEG1VIDEOINFO *mp1 = (MPEG1VIDEOINFO *)filter->sink.pin.mt.pbFormat;
    VIDEOINFOHEADER *vih;

    if (!filter->sink.pin.peer)
        return VFW_S_NO_MORE_ITEMS;
    if (index >= ARRAY_SIZE(formats))
        return VFW_S_NO_MORE_ITEMS;
    if (!(vih = CoTaskMemAlloc(sizeof(*vih))))
        return E_OUTOFMEMORY;

    memset(vih, 0, sizeof(*vih));
    vih->bmiHeader.biSize = sizeof(vih->bmiHeader);
    vih->bmiHeader.biPlanes = 1;
    vih->bmiHeader.biWidth = mp1->hdr.bmiHeader.biWidth;
    vih->bmiHeader.biHeight = abs(mp1->hdr.bmiHeader.biHeight);
    vih->bmiHeader.biCompression = index == 5 ? BI_BITFIELDS : index >= 3 ? BI_RGB : formats[index]->Data1;
    SetRect(&vih->rcSource, 0, 0, vih->bmiHeader.biWidth, vih->bmiHeader.biHeight);
    vih->bmiHeader.biXPelsPerMeter = 2000;
    vih->bmiHeader.biYPelsPerMeter = 2000;
    if (FAILED(MFCalculateImageSize(formats[index], vih->bmiHeader.biWidth, vih->bmiHeader.biHeight, (UINT *)&vih->bmiHeader.biSizeImage)))
        MFCalculateImageSize(&MEDIASUBTYPE_RGB32, vih->bmiHeader.biWidth, vih->bmiHeader.biHeight, (UINT *)&vih->bmiHeader.biSizeImage);
    vih->bmiHeader.biBitCount = vih->bmiHeader.biSizeImage * 8 / vih->bmiHeader.biWidth / vih->bmiHeader.biHeight;
    vih->AvgTimePerFrame = mp1->hdr.AvgTimePerFrame;
    vih->dwBitRate = MulDiv(vih->bmiHeader.biSizeImage * 8, 10000000, vih->AvgTimePerFrame);

    memset(mt, 0, sizeof(*mt));
    mt->majortype = MEDIATYPE_Video;
    mt->subtype = *formats[index];
    mt->formattype = FORMAT_VideoInfo;
    mt->pbFormat = (BYTE *)vih;
    mt->cbFormat = sizeof(*vih);
    mt->lSampleSize = vih->bmiHeader.biSizeImage;
    mt->bTemporalCompression = FALSE;
    mt->bFixedSizeSamples = TRUE;

    return S_OK;
}

static HRESULT mpeg_video_codec_source_decide_buffer_size(struct transform *filter, IMemAllocator *allocator, ALLOCATOR_PROPERTIES *props)
{
    VIDEOINFOHEADER *output_format = (VIDEOINFOHEADER *)filter->source.pin.mt.pbFormat;
    ALLOCATOR_PROPERTIES ret_props;

    props->cBuffers = max(props->cBuffers, 1);
    props->cbBuffer = max(props->cbBuffer, output_format->bmiHeader.biSizeImage);
    props->cbAlign = max(props->cbAlign, 1);

    return IMemAllocator_SetProperties(allocator, props, &ret_props);
}

static const struct transform_ops mpeg_video_codec_transform_ops =
{
    mpeg_video_codec_sink_query_accept,
    mpeg_video_codec_source_query_accept,
    mpeg_video_codec_source_get_media_type,
    mpeg_video_codec_source_decide_buffer_size,
    handle_source_qc_notify,
};

HRESULT mpeg_video_codec_create(IUnknown *outer, IUnknown **out)
{
    struct transform *object;
    NTSTATUS status;
    HRESULT hr;

    if ((status = winedmo_transform_check(MEDIATYPE_Video, MEDIASUBTYPE_MPEG1Video, MEDIASUBTYPE_NV12)))
    {
        static const GUID CLSID_wg_mpeg_video_decoder = {0x5ed2e5f6,0xbf3e,0x4180,{0x83,0xa4,0x48,0x47,0xcc,0x5b,0x4e,0xa3}};
        WARN("Unsupported winedmo transform, status %#lx.\n", status);
        return CoCreateInstance(&CLSID_wg_mpeg_video_decoder, outer, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)out);
    }

    hr = transform_create(outer, &CLSID_CMpegVideoCodec, &mpeg_video_codec_transform_ops, &object);
    if (FAILED(hr))
        return hr;

    wcscpy(object->sink.pin.name, L"Input");
    wcscpy(object->source.pin.name, L"Output");

    TRACE("Created MPEG video decoder %p.\n", object);
    *out = &object->filter.IUnknown_inner;
    return hr;
}
