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

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "quartz_private.h"

#include "wine/debug.h"
#include "wine/winedmo.h"

WINE_DEFAULT_DEBUG_CHANNEL(quartz);

#include "initguid.h"

DEFINE_GUID(MEDIASUBTYPE_MP3,WAVE_FORMAT_MPEGLAYER3,0x0000,0x0010,0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71);

struct async_op
{
    enum
    {
        ASYNC_OP_READ,
        ASYNC_OP_STOP,
        ASYNC_OP_EOS,
        ASYNC_OP_SAMPLE,
    } type;
    struct list entry;
    DMO_OUTPUT_DATA_BUFFER output;
};

struct parser_source
{
    struct strmbase_source pin;
    IQualityControl IQualityControl_iface;
    SourceSeeking seek;
    UINT stream_index;

    CONDITION_VARIABLE cond;
    struct list queue;
    HANDLE thread;
    BOOL eos;

    BOOL need_segment;
    BOOL interpolate_timestamps;
    UINT64 prev_end_pts;
};

struct parser
{
    struct strmbase_filter filter;
    IAMStreamSelect IAMStreamSelect_iface;

    struct strmbase_sink sink;
    IAsyncReader *reader;
    UINT stream_count;
    UINT64 file_size;
    UINT64 offset;

    struct parser_source **sources;
    unsigned int source_count;
    BOOL enum_sink_first;

    struct winedmo_stream winedmo_stream;
    struct winedmo_demuxer winedmo_demuxer;

    CONDITION_VARIABLE stop_cond;
    ULONG stream_stop;
    ULONG sink_stop;

    CONDITION_VARIABLE idle_cond;
    ULONG stream_idle;
    ULONG sink_idle;

    CONDITION_VARIABLE sink_cond;
    struct list sink_queue;
    HANDLE sink_thread;
    BOOL sink_eos;
};

static inline struct parser *filter_from_strmbase_filter(struct strmbase_filter *iface)
{
    return CONTAINING_RECORD(iface, struct parser, filter);
}

static struct async_op *parser_wait_async_op(struct parser *filter, struct list *queue,
        CONDITION_VARIABLE *cond, CRITICAL_SECTION *cs, ULONG *idle_count)
{
    struct async_op *op;
    struct list *ptr;

    while (!(ptr = list_head(queue)))
    {
        *idle_count += 1;
        WakeConditionVariable(&filter->idle_cond);
        SleepConditionVariableCS(cond, cs, INFINITE);
        *idle_count -= 1;
    }

    op = LIST_ENTRY(ptr, struct async_op, entry);
    list_remove(&op->entry);
    return op;
}

/* queue an operation to the sink's thread, filter cs needs to be held */
static void parser_queue_sink_op(struct parser *filter, struct async_op *op)
{
    list_add_tail(&filter->sink_queue, &op->entry);
    WakeConditionVariable(&filter->sink_cond);
}

static struct async_op *parser_wait_sink_op(struct parser *filter)
{
    return parser_wait_async_op(filter, &filter->sink_queue, &filter->sink_cond,
            &filter->filter.filter_cs, &filter->sink_idle);
}

/* queue an operation to the source's thread, filter cs needs to be held */
static void parser_queue_source_op(struct parser *filter, struct parser_source *source, struct async_op *op)
{
    list_add_tail(&source->queue, &op->entry);
    WakeConditionVariable(&source->cond);
}

static struct async_op *parser_wait_source_op(struct parser *filter, struct parser_source *source)
{
    return parser_wait_async_op(filter, &source->queue, &source->cond,
            &filter->filter.filter_cs, &filter->stream_idle);
}

static struct async_op *async_op_create(UINT type)
{
    struct async_op *op;
    if (!(op = calloc(1, sizeof(*op))))
        return NULL;
    op->type = type;
    return op;
}

static struct async_op *async_sample_create(DMO_OUTPUT_DATA_BUFFER *output)
{
    struct async_op *op;
    if (!(op = calloc(1, sizeof(*op))))
        return NULL;
    op->type = ASYNC_OP_SAMPLE;
    IMediaBuffer_AddRef(output->pBuffer);
    op->output = *output;
    return op;
}

/*
 * scale_uint64() is based on gst_util_scale_int() from GStreamer, which is
 * covered by the following license:
 *
 * GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *                    2002 Thomas Vander Stichele <thomas@apestaart.org>
 *                    2004 Wim Taymans <wim@fluendo.com>
 *                    2015 Jan Schmidt <jan@centricular.com>
 *
 * gstutils.c: Utility functions
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
static UINT64 scale_uint64(UINT64 value, UINT numerator, UINT denominator)
{
    ULARGE_INTEGER i, high, low;

    if (!value)
        return 0;

    i.QuadPart = value;
    low.QuadPart = (ULONGLONG)i.u.LowPart * numerator;
    high.QuadPart = (ULONGLONG)i.u.HighPart * numerator + low.u.HighPart;
    low.u.HighPart = 0;

    if (high.u.HighPart >= denominator)
        return ULLONG_MAX;

    low.QuadPart += (high.QuadPart % denominator) << 32;
    return ((high.QuadPart / denominator) << 32) + (low.QuadPart / denominator);
}

/* Fill and send a single IMediaSample. */
static HRESULT parser_source_send_sample(struct parser_source *source, IMediaSample *sample,
        const DMO_OUTPUT_DATA_BUFFER *output, UINT offset, UINT size, DWORD bytes_per_second)
{
    HRESULT hr;
    BYTE *ptr = NULL, *data;
    DWORD data_size;

    TRACE("offset %u, size %u, sample size %lu.\n", offset, size, IMediaSample_GetSize(sample));

    if (SUCCEEDED(hr = IMediaBuffer_GetBufferAndLength(output->pBuffer, &data, &data_size))
            && SUCCEEDED(hr = IMediaSample_GetPointer(sample, &ptr)))
    {
        memcpy(ptr, data + offset, min(data_size - offset, size));
        hr = IMediaSample_SetActualDataLength(sample, min(data_size - offset, size));
    }
    if (FAILED(hr))
    {
        ERR("Failed to set sample size, hr %#lx.\n", hr);
        return hr;
    }

    if ((output->dwStatus & DMO_OUTPUT_DATA_BUFFERF_TIME) || (source->interpolate_timestamps && source->prev_end_pts != 0))
    {
        UINT64 start_pts = ((output->dwStatus & DMO_OUTPUT_DATA_BUFFERF_TIME) ? output->rtTimestamp : source->prev_end_pts);
        REFERENCE_TIME start_reftime = start_pts;

        if (offset)
            start_reftime += scale_uint64(offset, 10000000, bytes_per_second);
        start_reftime -= source->seek.llCurrent;
        start_reftime *= source->seek.dRate;

        if (output->dwStatus & DMO_OUTPUT_DATA_BUFFERF_TIMELENGTH)
        {
            UINT64 end_pts = start_pts + output->rtTimelength;
            REFERENCE_TIME end_reftime = end_pts;

            source->prev_end_pts = end_pts;
            if (offset + size < data_size)
                end_reftime = end_reftime + scale_uint64(offset + size, 10000000, bytes_per_second);
            end_reftime -= source->seek.llCurrent;
            end_reftime *= source->seek.dRate;

            IMediaSample_SetTime(sample, &start_reftime, &end_reftime);
            IMediaSample_SetMediaTime(sample, &start_reftime, &end_reftime);
        }
        else
        {
            source->prev_end_pts = 0;
            IMediaSample_SetTime(sample, &start_reftime, NULL);
            IMediaSample_SetMediaTime(sample, NULL, NULL);
        }
    }
    else
    {
        IMediaSample_SetTime(sample, NULL, NULL);
        IMediaSample_SetMediaTime(sample, NULL, NULL);
    }

    /* IMediaSample_SetDiscontinuity(sample, !offset && buffer->discontinuity); */
    /* IMediaSample_SetPreroll(sample, buffer->preroll); */
    IMediaSample_SetSyncPoint(sample, !!(output->dwStatus & DMO_OUTPUT_DATA_BUFFERF_SYNCPOINT));

    if (!source->pin.pin.peer)
        return VFW_E_NOT_CONNECTED;

    hr = IMemInputPin_Receive(source->pin.pMemInputPin, sample);
    TRACE("Receive() returned hr %#lx.\n", hr);
    if (FAILED(hr)) return hr;
    if (offset + size < data_size) return S_FALSE;
    return S_OK;
}

/* Send a single GStreamer buffer (splitting it into multiple IMediaSamples if
 * necessary). */
static void parser_source_send_buffer(struct parser_source *source, const DMO_OUTPUT_DATA_BUFFER *output)
{
    HRESULT hr;
    IMediaSample *sample;

    if (source->need_segment)
    {
        if (FAILED(hr = IPin_NewSegment(source->pin.pin.peer, source->seek.llCurrent,
                source->seek.llStop, source->seek.dRate)))
            WARN("Failed to deliver new segment, hr %#lx.\n", hr);
        source->need_segment = FALSE;
    }

    if (IsEqualGUID(&source->pin.pin.mt.formattype, &FORMAT_WaveFormatEx)
            && (IsEqualGUID(&source->pin.pin.mt.subtype, &MEDIASUBTYPE_PCM)
            || IsEqualGUID(&source->pin.pin.mt.subtype, &MEDIASUBTYPE_IEEE_FLOAT)))
    {
        WAVEFORMATEX *format = (WAVEFORMATEX *)source->pin.pin.mt.pbFormat;
        DWORD offset = 0;

        do
        {
            if (FAILED(hr = IMemAllocator_GetBuffer(source->pin.pAllocator, &sample, NULL, NULL, 0)))
                ERR("Failed to get a sample, hr %#lx.\n", hr);
            else
            {
                DWORD size = IMediaSample_GetSize(sample);
                hr = parser_source_send_sample(source, sample, output, offset, size, format->nAvgBytesPerSec);
                IMediaSample_Release(sample);
                offset += size;
            }
        }
        while (hr == S_FALSE);
    }
    else
    {
        if (FAILED(hr = IMemAllocator_GetBuffer(source->pin.pAllocator, &sample, NULL, NULL, 0)))
            ERR("Failed to get a sample, hr %#lx.\n", hr);
        else
        {
            DWORD size = IMediaSample_GetSize(sample);
            hr = parser_source_send_sample(source, sample, output, 0, size, 0);
            IMediaSample_Release(sample);
        }
    }
}

/* removes all queued ops, filter cs needs to be held */
static void parser_flush_async_ops(struct parser *filter, struct list *queue)
{
    struct list *ptr;

    while ((ptr = list_head(queue)))
    {
        struct async_op *entry = LIST_ENTRY(ptr, struct async_op, entry);
        list_remove(&entry->entry);
        if (entry->type == ASYNC_OP_SAMPLE)
            IMediaBuffer_Release(entry->output.pBuffer);
        free(entry);
    }
}

static DWORD CALLBACK parser_source_thread(void *arg)
{
    struct parser_source *source = arg;
    struct parser *filter = filter_from_strmbase_filter(source->pin.pin.filter);
    struct async_op *op;
    BOOL running = TRUE;

    TRACE("Starting streaming thread for source %p.\n", source);

    EnterCriticalSection(&filter->filter.filter_cs);
    while (running && (op = parser_wait_source_op(filter, source)))
    {
        if (op->type == ASYNC_OP_READ && !filter->sink_eos && list_empty(&filter->sink_queue)
                && list_empty(&source->queue))
            parser_queue_sink_op(filter, async_op_create(ASYNC_OP_READ));
        LeaveCriticalSection(&filter->filter.filter_cs);

        switch (op->type)
        {
        default:
            ERR("Unexpected operation %#x\n", op->type);
            break;
        case ASYNC_OP_STOP:
            running = FALSE;
            break;
        case ASYNC_OP_EOS:
            if (!source->eos)
                IPin_EndOfStream(source->pin.pin.peer);
            source->eos = TRUE;
            break;
        case ASYNC_OP_SAMPLE:
            parser_source_send_buffer(source, &op->output);
            IMediaBuffer_Release(op->output.pBuffer);
            break;
        }

        free(op);
        EnterCriticalSection(&filter->filter.filter_cs);
    }

    filter->stream_stop++;
    LeaveCriticalSection(&filter->filter.filter_cs);
    WakeConditionVariable(&filter->stop_cond);

    TRACE("Streaming stopped; exiting.\n");
    return 0;
}

struct buffer
{
    IMediaBuffer IMediaBuffer_iface;
    LONG refcount;
    UINT max_length;
    UINT length;
    BYTE data[];
};

static struct buffer *buffer_from_IMediaBuffer(IMediaBuffer *iface)
{
    return CONTAINING_RECORD(iface, struct buffer, IMediaBuffer_iface);
}

static HRESULT WINAPI buffer_QueryInterface(IMediaBuffer *iface, REFIID iid, void **out)
{
    struct buffer *buffer = buffer_from_IMediaBuffer(iface);

    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IMediaBuffer))
        *out = &buffer->IMediaBuffer_iface;
    else
    {
        *out = NULL;
        WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static ULONG WINAPI buffer_AddRef(IMediaBuffer *iface)
{
    struct buffer *buffer = buffer_from_IMediaBuffer(iface);
    ULONG refcount = InterlockedIncrement(&buffer->refcount);
    TRACE("iface %p increasing refcount to %lu.\n", buffer, refcount);
    return refcount;
}

static ULONG WINAPI buffer_Release(IMediaBuffer *iface)
{
    struct buffer *buffer = buffer_from_IMediaBuffer(iface);
    ULONG refcount = InterlockedDecrement(&buffer->refcount);
    TRACE("iface %p decreasing refcount to %lu.\n", buffer, refcount);
    if (!refcount)
        free(buffer);
    return refcount;
}

static HRESULT WINAPI buffer_SetLength(IMediaBuffer *iface, DWORD length)
{
    struct buffer *buffer = buffer_from_IMediaBuffer(iface);
    TRACE("iface %p, length %lu.\n", buffer, length);
    buffer->length = length;
    return S_OK;
}

static HRESULT WINAPI buffer_GetMaxLength(IMediaBuffer *iface, DWORD *max_length)
{
    struct buffer *buffer = buffer_from_IMediaBuffer(iface);
    TRACE("iface %p, max_length %p.\n", buffer, max_length);
    *max_length = buffer->max_length;
    return S_OK;
}

static HRESULT WINAPI buffer_GetBufferAndLength(IMediaBuffer *iface, BYTE **data, DWORD *length)
{
    struct buffer *buffer = buffer_from_IMediaBuffer(iface);
    TRACE("iface %p, length %p.\n", buffer, length);
    *data = buffer->data;
    *length = buffer->length;
    return S_OK;
}

static const IMediaBufferVtbl buffer_vtbl =
{
    buffer_QueryInterface,
    buffer_AddRef,
    buffer_Release,
    buffer_SetLength,
    buffer_GetMaxLength,
    buffer_GetBufferAndLength,
};

static HRESULT buffer_create(UINT max_length, IMediaBuffer **out)
{
    struct buffer *buffer;

    *out = NULL;
    if (!(buffer = calloc(1, offsetof(struct buffer, data[max_length]))))
        return E_OUTOFMEMORY;
    buffer->IMediaBuffer_iface.lpVtbl = &buffer_vtbl;
    buffer->refcount = 1;

    buffer->max_length = max_length;
    *out = &buffer->IMediaBuffer_iface;
    return S_OK;
}

static HRESULT demuxer_read_sample(struct winedmo_demuxer demuxer, UINT *index, DMO_OUTPUT_DATA_BUFFER *output)
{
    UINT buffer_size = 0x1000;
    NTSTATUS status;
    HRESULT hr;

    do
    {
        if (FAILED(hr = buffer_create(buffer_size, &output->pBuffer)))
            return hr;
        if ((status = winedmo_demuxer_read(demuxer, index, output, &buffer_size)))
        {
            if (status == STATUS_BUFFER_TOO_SMALL) hr = S_FALSE;
            else if (status == STATUS_END_OF_FILE) hr = VFW_S_NO_MORE_ITEMS;
            else hr = HRESULT_FROM_NT(status);
            IMediaBuffer_Release(output->pBuffer);
            output->pBuffer = NULL;
        }
    } while (hr == S_FALSE);

    return hr;
}

static HRESULT media_type_from_mf_video_format( const MFVIDEOFORMAT *format, AM_MEDIA_TYPE *mt )
{
    VIDEOINFOHEADER *vih;
    MPEG1VIDEOINFO *mp1;
    UINT format_size;

    if (IsEqualGUID(&format->guidFormat, &MEDIASUBTYPE_MPEG1Payload))
        format_size = sizeof(*mp1) + format->dwSize - sizeof(*format);
    else
        format_size = sizeof(*vih) + format->dwSize - sizeof(*format);

    if (!(vih = CoTaskMemAlloc(format_size)))
        return E_OUTOFMEMORY;
    mp1 = (MPEG1VIDEOINFO *)vih;

    memset(vih, 0, format_size);
    vih->bmiHeader.biSize = sizeof(vih->bmiHeader);
    vih->bmiHeader.biPlanes = 1;
    vih->bmiHeader.biWidth = format->videoInfo.dwWidth;
    vih->bmiHeader.biHeight = format->videoInfo.dwHeight;
    vih->bmiHeader.biCompression = format->guidFormat.Data1;
    if (format->videoInfo.FramesPerSecond.Denominator && format->videoInfo.FramesPerSecond.Numerator)
        vih->AvgTimePerFrame = MulDiv(10000000, format->videoInfo.FramesPerSecond.Denominator, format->videoInfo.FramesPerSecond.Numerator);
    else
        vih->AvgTimePerFrame = 10000000;
    if (FAILED(MFCalculateImageSize(&format->guidFormat, format->videoInfo.dwWidth, format->videoInfo.dwHeight, (UINT *)&vih->bmiHeader.biSizeImage)))
        MFCalculateImageSize(&MEDIASUBTYPE_RGB32, format->videoInfo.dwWidth, format->videoInfo.dwHeight, (UINT *)&vih->bmiHeader.biSizeImage);
    vih->bmiHeader.biBitCount = vih->bmiHeader.biSizeImage * 8 / format->videoInfo.dwWidth / format->videoInfo.dwHeight;

    if (IsEqualGUID(&format->guidFormat, &MEDIASUBTYPE_MPEG1Payload))
        memcpy(mp1 + 1, format + 1, format->dwSize - sizeof(*format));
    else
        memcpy(vih + 1, format + 1, format->dwSize - sizeof(*format));

    memset(mt, 0, sizeof(*mt));
    mt->majortype = MEDIATYPE_Video;
    mt->subtype = format->guidFormat;
    mt->lSampleSize = 1;
    if (IsEqualGUID(&format->guidFormat, &MEDIASUBTYPE_MPEG1Payload))
        mt->formattype = FORMAT_MPEGVideo;
    else
        mt->formattype = FORMAT_VideoInfo;
    mt->pbFormat = (BYTE *)vih;
    mt->cbFormat = format_size;

    return S_OK;
}

static HRESULT media_type_from_wave_format_ex( const WAVEFORMATEX *format, AM_MEDIA_TYPE *mt )
{
    WAVEFORMATEX *wfx;
    UINT format_size;

    format_size = sizeof(*wfx) + format->cbSize;
    if (!(wfx = CoTaskMemAlloc(format_size)))
        return E_OUTOFMEMORY;

    memcpy(wfx, format, format_size);

    memset(mt, 0, sizeof(*mt));
    mt->majortype = MEDIATYPE_Audio;
    if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
    {
        WAVEFORMATEXTENSIBLE *wfxext = (WAVEFORMATEXTENSIBLE *)format;
        mt->subtype = wfxext->SubFormat;
    }
    else
    {
        mt->subtype = MEDIASUBTYPE_PCM;
        mt->subtype.Data1 = format->wFormatTag;
    }
    mt->bFixedSizeSamples = TRUE;
    mt->lSampleSize = 1;
    mt->formattype = FORMAT_WaveFormatEx;
    mt->pbFormat = (BYTE *)wfx;
    mt->cbFormat = format_size;

    return S_OK;
}

static HRESULT media_type_from_winedmo_format( GUID major, union winedmo_format *format, AM_MEDIA_TYPE *mt )
{
    if (IsEqualGUID( &major, &MFMediaType_Video ))
        return media_type_from_mf_video_format( &format->video, mt );
    if (IsEqualGUID( &major, &MFMediaType_Audio ))
        return media_type_from_wave_format_ex( &format->audio, mt );

    FIXME( "Unsupported major type %s\n", debugstr_guid( &major ) );
    return E_NOTIMPL;
}

static HRESULT get_stream_media_type(struct winedmo_demuxer demuxer, UINT index, AM_MEDIA_TYPE *mt)
{
    union winedmo_format *format;
    NTSTATUS status;
    HRESULT hr;
    GUID major;

    if ((status = winedmo_demuxer_stream_type(demuxer, index, &major, &format)))
        hr = HRESULT_FROM_NT(status);
    else
    {
        hr = media_type_from_winedmo_format(major, format, mt);
        free(format);
    }

    return hr;
}

static void parser_sink_read_sample(struct parser *filter)
{
    DMO_OUTPUT_DATA_BUFFER output = {0};
    UINT stream;
    HRESULT hr;

    if (FAILED(hr = demuxer_read_sample(filter->winedmo_demuxer, &stream, &output)))
    {
        ERR("Failed to read sample hr %#lx\n", hr);
        return;
    }

    if (hr == VFW_S_NO_MORE_ITEMS)
    {
        for (stream = 0; stream < filter->source_count; stream++)
        {
            struct parser_source *source = filter->sources[stream];
            if (!source->pin.pin.peer)
                continue;
            parser_queue_source_op(filter, source, async_op_create(ASYNC_OP_EOS));
        }

        filter->sink_eos = TRUE;
        return;
    }

    if (stream < filter->source_count)
    {
        struct parser_source *source = filter->sources[stream];
        if (source->pin.pin.peer)
            parser_queue_source_op(filter, source, async_sample_create(&output));
    }

    IMediaBuffer_Release(output.pBuffer);
}

static DWORD CALLBACK parser_sink_thread(void *arg)
{
    struct parser *filter = arg;
    struct async_op *op;
    BOOL running = TRUE;

    TRACE("Starting read thread for filter %p.\n", filter);

    EnterCriticalSection(&filter->filter.filter_cs);
    while (running && (op = parser_wait_sink_op(filter)))
    {
        switch (op->type)
        {
        default:
            ERR("Unexpected operation %#x\n", op->type);
            break;
        case ASYNC_OP_READ:
            if (filter->sink_eos)
                break;
            parser_sink_read_sample(filter);
            break;
        case ASYNC_OP_STOP:
            running = FALSE;
            break;
        }

        if (op->type == ASYNC_OP_READ && !filter->sink_eos && list_empty(&filter->sink_queue))
            list_add_tail(&filter->sink_queue, &op->entry);
        else
            free(op);
    }

    filter->sink_stop++;
    LeaveCriticalSection(&filter->filter.filter_cs);
    WakeConditionVariable(&filter->stop_cond);

    TRACE("Streaming stopped; exiting.\n");
    return 0;
}

static inline struct parser_source *parser_source_from_IMediaSeeking(IMediaSeeking *iface)
{
    return CONTAINING_RECORD(iface, struct parser_source, seek.IMediaSeeking_iface);
}

static struct strmbase_pin *parser_get_pin(struct strmbase_filter *base, unsigned int index)
{
    struct parser *filter = filter_from_strmbase_filter(base);

    if (filter->enum_sink_first)
    {
        if (!index)
            return &filter->sink.pin;
        else if (index <= filter->source_count)
            return &filter->sources[index - 1]->pin.pin;
    }
    else
    {
        if (index < filter->source_count)
            return &filter->sources[index]->pin.pin;
        else if (index == filter->source_count)
            return &filter->sink.pin;
    }
    return NULL;
}

static void parser_destroy(struct strmbase_filter *iface)
{
    struct parser *filter = filter_from_strmbase_filter(iface);
    HRESULT hr;

    /* Don't need to clean up output pins, disconnecting input pin will do that */
    if (filter->sink.pin.peer)
    {
        if (FAILED(hr = IPin_Disconnect(filter->sink.pin.peer)))
            ERR("Failed to disconnect peer, hr %#lx\n", hr);
        if (FAILED(hr = IPin_Disconnect(&filter->sink.pin.IPin_iface)))
            ERR("Failed to disconnect pin, hr %#lx\n", hr);
    }

    if (filter->reader)
        IAsyncReader_Release(filter->reader);
    filter->reader = NULL;

    winedmo_demuxer_destroy(&filter->winedmo_demuxer);

    strmbase_sink_cleanup(&filter->sink);
    strmbase_filter_cleanup(&filter->filter);
    free(filter);
}

static HRESULT parser_init_stream(struct strmbase_filter *iface)
{
    struct parser *filter = filter_from_strmbase_filter(iface);
    const SourceSeeking *seeking;
    unsigned int i;

    if (!filter->sink_thread)
        return S_OK;

    /* DirectShow retains the old seek positions, but resets to them every time
     * it transitions from stopped -> paused. */
    seeking = &filter->sources[0]->seek;

    /* FIXME: seeking->dRate, seeking->llStop */
    winedmo_demuxer_seek(filter->winedmo_demuxer, seeking->llCurrent);

    filter->stream_idle = 0;
    for (i = 0; i < filter->source_count; ++i)
    {
        struct parser_source *source = filter->sources[i];
        HRESULT hr;

        if (!source->pin.pin.peer)
            continue;
        if (FAILED(hr = IMemAllocator_Commit(source->pin.pAllocator)))
            ERR("Failed to commit allocator, hr %#lx.\n", hr);

        source->need_segment = TRUE;
        source->eos = FALSE;
        source->thread = CreateThread(NULL, 0, parser_source_thread, source, 0, NULL);
    }

    filter->sink_eos = FALSE;
    parser_queue_sink_op(filter, async_op_create(ASYNC_OP_READ));
    return S_OK;
}

static HRESULT parser_cleanup_stream(struct strmbase_filter *iface)
{
    struct parser *filter = filter_from_strmbase_filter(iface);
    UINT i;

    if (!filter->sink_thread)
        return S_OK;

    for (i = 0, filter->stream_stop = 0; i < filter->source_count; ++i)
    {
        struct parser_source *source = filter->sources[i];
        if (!source->pin.pin.peer)
        {
            filter->stream_stop++;
            continue;
        }
        parser_flush_async_ops(filter, &source->queue);
        parser_queue_source_op(filter, source, async_op_create(ASYNC_OP_STOP));
    }

    while (filter->stream_stop != filter->source_count)
        SleepConditionVariableCS(&filter->stop_cond, &filter->filter.filter_cs, INFINITE);

    for (i = 0; i < filter->source_count; ++i)
    {
        struct parser_source *source = filter->sources[i];
        if (!source->pin.pin.peer)
            continue;
        IMemAllocator_Decommit(source->pin.pAllocator);
        WaitForSingleObject(source->thread, INFINITE);
        CloseHandle(source->thread);
        source->thread = NULL;
    }

    return S_OK;
}

static const struct strmbase_filter_ops filter_ops =
{
    .filter_get_pin = parser_get_pin,
    .filter_destroy = parser_destroy,
    .filter_init_stream = parser_init_stream,
    .filter_cleanup_stream = parser_cleanup_stream,
};

static inline struct parser *parser_from_strmbase_sink(struct strmbase_sink *iface)
{
    return CONTAINING_RECORD(iface, struct parser, sink);
}

static NTSTATUS CDECL parser_seek_stream_cb( struct winedmo_stream *stream, UINT64 *pos )
{
    struct parser *filter = CONTAINING_RECORD(stream, struct parser, winedmo_stream);
    if (*pos > filter->file_size)
        *pos = filter->file_size;
    filter->offset = *pos;
    return STATUS_SUCCESS;
}

static NTSTATUS CDECL parser_read_stream_cb( struct winedmo_stream *stream, BYTE *buffer, ULONG *size )
{
    struct parser *filter = CONTAINING_RECORD(stream, struct parser, winedmo_stream);
    if (filter->offset + *size > filter->file_size)
        *size = filter->file_size - filter->offset;
    if (FAILED(IAsyncReader_SyncRead(filter->reader, filter->offset, *size, buffer)))
        return STATUS_UNSUCCESSFUL;
    filter->offset += *size;
    return STATUS_SUCCESS;
}

static HRESULT parser_sink_connect(struct strmbase_sink *iface, IPin *peer, const AM_MEDIA_TYPE *pmt, INT64 *duration)
{
    struct parser *filter = parser_from_strmbase_sink(iface);
    WCHAR mime_type[256];
    LONGLONG unused;
    HRESULT hr = S_OK;

    filter->reader = NULL;
    if (FAILED(hr = IPin_QueryInterface(peer, &IID_IAsyncReader, (void **)&filter->reader)))
        return hr;

    IAsyncReader_Length(filter->reader, (LONGLONG *)&filter->file_size, &unused);
    filter->winedmo_stream.p_seek = parser_seek_stream_cb;
    filter->winedmo_stream.p_read = parser_read_stream_cb;

    if (FAILED(hr = winedmo_demuxer_create(NULL, &filter->winedmo_stream, filter->file_size,
            duration, &filter->stream_count, mime_type, &filter->winedmo_demuxer)))
        goto err;

    filter->sink_idle = 0;
    filter->sink_thread = CreateThread(NULL, 0, parser_sink_thread, filter, 0, NULL);
    return S_OK;

err:
    IAsyncReader_Release(filter->reader);
    filter->reader = NULL;
    return hr;
}

static void parser_source_destroy(struct parser_source *source)
{
    if (source->pin.pin.peer)
    {
        if (SUCCEEDED(IMemAllocator_Decommit(source->pin.pAllocator)))
            IPin_Disconnect(source->pin.pin.peer);
        IPin_Disconnect(&source->pin.pin.IPin_iface);
    }

    strmbase_seeking_cleanup(&source->seek);
    strmbase_source_cleanup(&source->pin);
    free(source);
}

static void parser_sink_disconnect(struct strmbase_sink *iface)
{
    struct parser *filter = parser_from_strmbase_sink(iface);
    unsigned int i;

    TRACE("(%p)\n", filter);

    if (!filter->sink_thread)
        return;

    filter->sink_stop = 0;
    parser_flush_async_ops(filter, &filter->sink_queue);
    parser_queue_sink_op(filter, async_op_create(ASYNC_OP_STOP));
    while (!filter->sink_stop)
        SleepConditionVariableCS(&filter->stop_cond, &filter->filter.filter_cs, INFINITE);

    WaitForSingleObject(filter->sink_thread, INFINITE);
    CloseHandle(filter->sink_thread);
    filter->sink_thread = 0;

    for (i = 0; i < filter->source_count; ++i)
        parser_source_destroy(filter->sources[i]);
    filter->source_count = 0;

    free(filter->sources);
    filter->sources = NULL;

    IAsyncReader_Release(filter->reader);
    filter->reader = NULL;

    BaseFilterImpl_IncrementPinVersion(&filter->filter);
}

static HRESULT parser_create(BOOL output_compressed, struct parser **parser)
{
    struct parser *object;

    if (!(object = calloc(1, sizeof(*object))))
        return E_OUTOFMEMORY;

    InitializeConditionVariable(&object->sink_cond);
    InitializeConditionVariable(&object->idle_cond);
    InitializeConditionVariable(&object->stop_cond);
    list_init(&object->sink_queue);

    *parser = object;
    return S_OK;
}

static struct parser *parser_from_IAMStreamSelect(IAMStreamSelect *iface)
{
    return CONTAINING_RECORD(iface, struct parser, IAMStreamSelect_iface);
}

static HRESULT WINAPI stream_select_QueryInterface(IAMStreamSelect *iface, REFIID iid, void **out)
{
    struct parser *filter = parser_from_IAMStreamSelect(iface);
    return IUnknown_QueryInterface(filter->filter.outer_unk, iid, out);
}

static ULONG WINAPI stream_select_AddRef(IAMStreamSelect *iface)
{
    struct parser *filter = parser_from_IAMStreamSelect(iface);
    return IUnknown_AddRef(filter->filter.outer_unk);
}

static ULONG WINAPI stream_select_Release(IAMStreamSelect *iface)
{
    struct parser *filter = parser_from_IAMStreamSelect(iface);
    return IUnknown_Release(filter->filter.outer_unk);
}

static HRESULT WINAPI stream_select_Count(IAMStreamSelect *iface, DWORD *count)
{
    struct parser *filter = parser_from_IAMStreamSelect(iface);
    TRACE("filter %p, count %p\n", filter, count);
    EnterCriticalSection(&filter->filter.filter_cs);
    if (filter->sink.pin.peer)
        *count = filter->stream_count;
    else
        *count = 0;
    LeaveCriticalSection(&filter->filter.filter_cs);
    return S_OK;
}

static HRESULT WINAPI stream_select_Info(IAMStreamSelect *iface, LONG index,
        AM_MEDIA_TYPE **mt, DWORD *flags, LCID *lcid, DWORD *group, WCHAR **name,
        IUnknown **object, IUnknown **unknown)
{
    struct parser *filter = parser_from_IAMStreamSelect(iface);
    HRESULT hr = S_OK;

    FIXME("filter %p, index %ld, mt %p, flags %p, lcid %p, group %p, name %p, object %p, unknown %p, semi-stub!\n",
            filter, index, mt, flags, lcid, group, name, object, unknown);
    EnterCriticalSection(&filter->filter.filter_cs);

    if (!filter->sink.pin.peer)
    {
        LeaveCriticalSection(&filter->filter.filter_cs);
        return VFW_E_NOT_CONNECTED;
    }
    if (index < 0 || index >= filter->source_count)
    {
        LeaveCriticalSection(&filter->filter.filter_cs);
        return S_FALSE;
    }

    if (mt)
        *mt = CreateMediaType(&filter->sources[index]->pin.pin.mt);
    if (flags) /* todo */
        *flags = 0;
    if (lcid) /* todo */
        *lcid = 0;
    if (group) /* todo */
        *group = 0;
    if (name) /* todo */
        *name = NULL;
    if (object) /* todo */
        *object = NULL;
    if (unknown)
        *unknown = NULL;

    LeaveCriticalSection(&filter->filter.filter_cs);
    return hr;
}

static HRESULT WINAPI stream_select_Enable(IAMStreamSelect *iface, LONG index, DWORD flags)
{
    FIXME("iface %p, index %ld, flags %#lx, stub!\n", iface, index, flags);
    return E_NOTIMPL;
}

static const IAMStreamSelectVtbl stream_select_vtbl =
{
    stream_select_QueryInterface,
    stream_select_AddRef,
    stream_select_Release,
    stream_select_Count,
    stream_select_Info,
    stream_select_Enable,
};

static HRESULT WINAPI parser_change_current(IMediaSeeking *iface)
{
    struct parser_source *This = parser_source_from_IMediaSeeking(iface);
    TRACE("(%p)\n", This);
    return S_OK;
}

static HRESULT WINAPI parser_change_stop(IMediaSeeking *iface)
{
    struct parser_source *This = parser_source_from_IMediaSeeking(iface);
    TRACE("(%p)\n", This);
    return S_OK;
}

static HRESULT WINAPI parser_change_rate(IMediaSeeking *iface)
{
/*
    struct parser_source *source = parser_source_from_IMediaSeeking(iface);
    struct parser *filter = filter_from_strmbase_filter(source->pin.pin.filter);

    winedmo_demuxer_stream_seek(source->stream_index, source->seek.dRate, 0, 0,
            AM_SEEKING_NoPositioning, AM_SEEKING_NoPositioning);
*/
    return S_OK;
}

static HRESULT WINAPI parser_IMediaSeeking_queryinterface(IMediaSeeking *iface, REFIID riid, void **ppv)
{
    struct parser_source *This = parser_source_from_IMediaSeeking(iface);
    return IPin_QueryInterface(&This->pin.pin.IPin_iface, riid, ppv);
}

static ULONG WINAPI parser_IMediaSeeking_addref(IMediaSeeking *iface)
{
    struct parser_source *This = parser_source_from_IMediaSeeking(iface);
    return IPin_AddRef(&This->pin.pin.IPin_iface);
}

static ULONG WINAPI parser_IMediaSeeking_release(IMediaSeeking *iface)
{
    struct parser_source *This = parser_source_from_IMediaSeeking(iface);
    return IPin_Release(&This->pin.pin.IPin_iface);
}

static HRESULT WINAPI parser_IMediaSeeking_setpositions(IMediaSeeking *iface,
        LONGLONG *current, DWORD current_flags, LONGLONG *stop, DWORD stop_flags)
{
    struct parser_source *source = parser_source_from_IMediaSeeking(iface);
    struct parser *filter = filter_from_strmbase_filter(source->pin.pin.filter);
    UINT flush_count;
    int i;

    TRACE("source %p, current %s, current_flags %#lx, stop %s, stop_flags %#lx.\n",
            source, current ? debugstr_time(*current) : "<null>", current_flags,
            stop ? debugstr_time(*stop) : "<null>", stop_flags);

    if (source->pin.pin.filter->state == State_Stopped)
    {
        SourceSeekingImpl_SetPositions(iface, current, current_flags, stop, stop_flags);
        return S_OK;
    }

    EnterCriticalSection(&filter->filter.filter_cs);

    if (!(current_flags & AM_SEEKING_NoFlush))
    {
        for (i = 0; i < filter->source_count; ++i)
        {
            if (filter->sources[i]->pin.pin.peer)
                IPin_BeginFlush(filter->sources[i]->pin.pin.peer);
        }

        if (filter->reader)
            IAsyncReader_BeginFlush(filter->reader);
    }

    /* signal the sink thread to pause. */
    parser_flush_async_ops(filter, &filter->sink_queue);
    while (!filter->sink_idle)
        SleepConditionVariableCS(&filter->idle_cond, &filter->filter.filter_cs, INFINITE);

    for (i = 0, flush_count = 0; i < filter->source_count; ++i)
    {
        struct parser_source *source = filter->sources[i];
        if (!source->pin.pin.peer)
            continue;
        parser_flush_async_ops(filter, &source->queue);
        flush_count++;
    }

    while (filter->stream_idle != flush_count)
        SleepConditionVariableCS(&filter->idle_cond, &filter->filter.filter_cs, INFINITE);

    SourceSeekingImpl_SetPositions(iface, current, current_flags, stop, stop_flags);

    /* source->seek.dRate, source->seek.llStop */
    winedmo_demuxer_seek(filter->winedmo_demuxer, source->seek.llCurrent);

    if (!(current_flags & AM_SEEKING_NoFlush))
    {
        for (i = 0; i < filter->source_count; ++i)
        {
            struct parser_source *flush = filter->sources[i];
            if (flush->pin.pin.peer)
                IPin_EndFlush(flush->pin.pin.peer);
        }

        if (filter->reader)
            IAsyncReader_EndFlush(filter->reader);
    }

    /* Signal the sink thread to resume. */
    for (i = filter->source_count - 1; i >= 0; --i)
    {
        struct parser_source *flush = filter->sources[i];
        flush->need_segment = TRUE;
        flush->eos = FALSE;
    }

    filter->sink_eos = FALSE;
    parser_queue_sink_op(filter, async_op_create(ASYNC_OP_READ));
    LeaveCriticalSection(&filter->filter.filter_cs);

    return S_OK;
}

static const IMediaSeekingVtbl parser_IMediaSeeking_vtbl =
{
    parser_IMediaSeeking_queryinterface,
    parser_IMediaSeeking_addref,
    parser_IMediaSeeking_release,
    SourceSeekingImpl_GetCapabilities,
    SourceSeekingImpl_CheckCapabilities,
    SourceSeekingImpl_IsFormatSupported,
    SourceSeekingImpl_QueryPreferredFormat,
    SourceSeekingImpl_GetTimeFormat,
    SourceSeekingImpl_IsUsingTimeFormat,
    SourceSeekingImpl_SetTimeFormat,
    SourceSeekingImpl_GetDuration,
    SourceSeekingImpl_GetStopPosition,
    SourceSeekingImpl_GetCurrentPosition,
    SourceSeekingImpl_ConvertTimeFormat,
    parser_IMediaSeeking_setpositions,
    SourceSeekingImpl_GetPositions,
    SourceSeekingImpl_GetAvailable,
    SourceSeekingImpl_SetRate,
    SourceSeekingImpl_GetRate,
    SourceSeekingImpl_GetPreroll
};

static inline struct parser_source *parser_source_from_IQualityControl( IQualityControl *iface )
{
    return CONTAINING_RECORD(iface, struct parser_source, IQualityControl_iface);
}

static HRESULT WINAPI parser_source_IQualityControl_QueryInterface(IQualityControl *iface, REFIID riid, void **ppv)
{
    struct parser_source *source = parser_source_from_IQualityControl(iface);
    return IPin_QueryInterface(&source->pin.pin.IPin_iface, riid, ppv);
}

static ULONG WINAPI parser_source_IQualityControl_AddRef(IQualityControl *iface)
{
    struct parser_source *source = parser_source_from_IQualityControl(iface);
    return IPin_AddRef(&source->pin.pin.IPin_iface);
}

static ULONG WINAPI parser_source_IQualityControl_Release(IQualityControl *iface)
{
    struct parser_source *source = parser_source_from_IQualityControl(iface);
    return IPin_Release(&source->pin.pin.IPin_iface);
}

static HRESULT WINAPI parser_source_IQualityControl_Notify(IQualityControl *iface, IBaseFilter *sender, Quality q)
{
    struct parser_source *source = parser_source_from_IQualityControl(iface);
    UINT64 timestamp;
    INT64 diff;

    TRACE("source %p, sender %p, type %s, proportion %ld, late %s, timestamp %s.\n",
            source, sender, q.Type == Famine ? "Famine" : "Flood", q.Proportion,
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

    /* GST_QOS_TYPE_OVERFLOW is also used for buffers that arrive on time, but
     * DirectShow filters might use Famine, so check that there actually is an
     * underrun. */
    /*
    winedmo_demuxer_stream_notify_qos(source->stream_index, q.Type == Famine && q.Proportion < 1000,
            1000.0 / q.Proportion, diff, timestamp);
    */

    return S_OK;
}

static HRESULT WINAPI parser_source_IQualityControl_SetSink(IQualityControl *iface, IQualityControl *tonotify)
{
    struct parser_source *source = parser_source_from_IQualityControl(iface);
    TRACE("(%p)->(%p)\n", source, source);
    /* Do nothing */
    return S_OK;
}

static const IQualityControlVtbl parser_source_IQualityControl_vtbl =
{
    parser_source_IQualityControl_QueryInterface,
    parser_source_IQualityControl_AddRef,
    parser_source_IQualityControl_Release,
    parser_source_IQualityControl_Notify,
    parser_source_IQualityControl_SetSink
};

static inline struct parser_source *parser_source_from_IPin(IPin *iface)
{
    return CONTAINING_RECORD(iface, struct parser_source, pin.pin.IPin_iface);
}

static HRESULT source_query_interface(struct strmbase_pin *iface, REFIID iid, void **out)
{
    struct parser_source *source = parser_source_from_IPin(&iface->IPin_iface);

    if (IsEqualGUID(iid, &IID_IMediaSeeking))
        *out = &source->seek.IMediaSeeking_iface;
    else if (IsEqualGUID(iid, &IID_IQualityControl))
        *out = &source->IQualityControl_iface;
    else
        return E_NOINTERFACE;

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static HRESULT WINAPI GSTOutPin_DecideBufferSize(struct strmbase_source *iface,
        IMemAllocator *allocator, ALLOCATOR_PROPERTIES *props)
{
    struct parser_source *source = parser_source_from_IPin(&iface->pin.IPin_iface);
    unsigned int buffer_count = 1;
    unsigned int buffer_size = 16384;
    ALLOCATOR_PROPERTIES ret_props;

    if (IsEqualGUID(&source->pin.pin.mt.formattype, &FORMAT_VideoInfo))
    {
        VIDEOINFOHEADER *format = (VIDEOINFOHEADER *)source->pin.pin.mt.pbFormat;
        buffer_size = format->bmiHeader.biSizeImage;
    }
    else if (IsEqualGUID(&source->pin.pin.mt.formattype, &FORMAT_MPEGVideo))
    {
        MPEG1VIDEOINFO *format = (MPEG1VIDEOINFO *)source->pin.pin.mt.pbFormat;
        buffer_size = format->hdr.bmiHeader.biSizeImage;
        buffer_count = 8;
    }
    else if (IsEqualGUID(&source->pin.pin.mt.formattype, &FORMAT_WaveFormatEx)
            && (IsEqualGUID(&source->pin.pin.mt.subtype, &MEDIASUBTYPE_PCM)
            || IsEqualGUID(&source->pin.pin.mt.subtype, &MEDIASUBTYPE_IEEE_FLOAT)))
    {
        WAVEFORMATEX *format = (WAVEFORMATEX *)source->pin.pin.mt.pbFormat;
        buffer_size = format->nAvgBytesPerSec;
    }
    else if (IsEqualGUID(&source->pin.pin.mt.subtype, &MEDIASUBTYPE_MPEG1AudioPayload)
            || IsEqualGUID(&source->pin.pin.mt.subtype, &MEDIASUBTYPE_MP3))
    {
        /* mpg123audiodec requires at least 3 buffers as it will keep
         * references to the last 2 samples. */
        buffer_count = 3;
    }

    /* We do need to drop any buffers that might have been sent with the old
     * caps, but this will be handled in parser_init_stream(). */

    props->cBuffers = max(props->cBuffers, buffer_count);
    props->cbBuffer = max(props->cbBuffer, buffer_size);
    props->cbAlign = max(props->cbAlign, 1);
    return IMemAllocator_SetProperties(allocator, props, &ret_props);
}

static BOOL compare_media_types(const AM_MEDIA_TYPE *a, const AM_MEDIA_TYPE *b)
{
    return IsEqualGUID(&a->majortype, &b->majortype)
            && IsEqualGUID(&a->subtype, &b->subtype)
            && IsEqualGUID(&a->formattype, &b->formattype)
            && a->cbFormat == b->cbFormat
            && !memcmp(a->pbFormat, b->pbFormat, a->cbFormat);
}

static HRESULT source_query_accept(struct strmbase_pin *iface, const AM_MEDIA_TYPE *mt)
{
    struct parser_source *source = parser_source_from_IPin(&iface->IPin_iface);
    struct parser *filter = filter_from_strmbase_filter(source->pin.pin.filter);
    AM_MEDIA_TYPE pad_mt;
    HRESULT hr;

    if (FAILED(hr = get_stream_media_type(filter->winedmo_demuxer, source->stream_index, &pad_mt)))
        return hr;
    hr = compare_media_types(mt, &pad_mt) ? S_OK : S_FALSE;
    FreeMediaType(&pad_mt);
    return hr;
}

static HRESULT source_get_media_type(struct strmbase_pin *iface,
        unsigned int index, AM_MEDIA_TYPE *mt)
{
    struct parser_source *source = parser_source_from_IPin(&iface->IPin_iface);
    struct parser *filter = filter_from_strmbase_filter(source->pin.pin.filter);

    if (index > 0)
        return VFW_S_NO_MORE_ITEMS;
    return get_stream_media_type(filter->winedmo_demuxer, source->stream_index, mt);
}

static const struct strmbase_source_ops source_ops =
{
    .base.pin_query_interface = source_query_interface,
    .base.pin_query_accept = source_query_accept,
    .base.pin_get_media_type = source_get_media_type,
    .pfnAttemptConnection = BaseOutputPinImpl_AttemptConnection,
    .pfnDecideAllocator = BaseOutputPinImpl_DecideAllocator,
    .pfnDecideBufferSize = GSTOutPin_DecideBufferSize,
};

static struct parser_source *parser_source_create(struct parser *filter, UINT stream_index, const WCHAR *name)
{
    struct parser_source *source, **new_array;

    if (!(new_array = realloc(filter->sources, (filter->source_count + 1) * sizeof(*filter->sources))))
        return NULL;
    filter->sources = new_array;

    if (!(source = calloc(1, sizeof(*source))))
        return NULL;

    source->stream_index = stream_index;
    strmbase_source_init(&source->pin, &filter->filter, name, &source_ops);
    source->IQualityControl_iface.lpVtbl = &parser_source_IQualityControl_vtbl;
    strmbase_seeking_init(&source->seek, &parser_IMediaSeeking_vtbl, parser_change_stop,
            parser_change_current, parser_change_rate);
    BaseFilterImpl_IncrementPinVersion(&filter->filter);

    InitializeConditionVariable(&source->cond);
    list_init(&source->queue);

    filter->sources[filter->source_count++] = source;
    return source;
}

static HRESULT avi_splitter_sink_query_accept(struct strmbase_pin *iface, const AM_MEDIA_TYPE *mt)
{
    if (IsEqualGUID(&mt->majortype, &MEDIATYPE_Stream)
            && IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_Avi))
        return S_OK;
    return S_FALSE;
}

static HRESULT avi_splitter_sink_connect(struct strmbase_sink *iface, IPin *peer, const AM_MEDIA_TYPE *pmt)
{
    struct parser *filter = parser_from_strmbase_sink(iface);
    struct parser_source *src;
    WCHAR source_name[20];
    INT64 duration;
    HRESULT hr;
    UINT i;

    if (FAILED(hr = parser_sink_connect(iface, peer, pmt, &duration)))
        return hr;

    for (i = 0; i < filter->stream_count; ++i)
    {
        swprintf(source_name, ARRAY_SIZE(source_name), L"Stream %02u", i);
        src = parser_source_create(filter, i, source_name);
        if (!src)
            return E_FAIL;
        src->interpolate_timestamps = TRUE;
    }

    for (i = 0; i < filter->source_count; ++i)
    {
        struct parser_source *source = filter->sources[i];
        source->seek.llDuration = source->seek.llStop = duration;
        source->seek.llCurrent = 0;
    }

    return hr;
}

static const struct strmbase_sink_ops avi_splitter_sink_ops =
{
    .base.pin_query_accept = avi_splitter_sink_query_accept,
    .sink_connect = avi_splitter_sink_connect,
    .sink_disconnect = parser_sink_disconnect,
};

HRESULT avi_splitter_create(IUnknown *outer, IUnknown **out)
{
    struct parser *object;
    NTSTATUS status;
    HRESULT hr;

    if ((status = winedmo_demuxer_check("video/avi")))
    {
        static const GUID CLSID_wg_avi_splitter = {0x272bfbfb,0x50d0,0x4078,{0xb6,0x00,0x1e,0x95,0x9c,0x30,0x13,0x37}};
        WARN("Unsupported demuxer, status %#lx.\n", status);
    	return CoCreateInstance(&CLSID_wg_avi_splitter, outer, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)out);
    }

    if (FAILED(hr = parser_create(TRUE, &object)))
        return hr;

    strmbase_filter_init(&object->filter, outer, &CLSID_AviSplitter, &filter_ops);
    strmbase_sink_init(&object->sink, &object->filter, L"input pin", &avi_splitter_sink_ops, NULL);

    TRACE("Created AVI splitter %p.\n", object);
    *out = &object->filter.IUnknown_inner;
    return hr;
}

static HRESULT mpeg_splitter_sink_query_accept(struct strmbase_pin *iface, const AM_MEDIA_TYPE *mt)
{
    if (!IsEqualGUID(&mt->majortype, &MEDIATYPE_Stream))
        return S_FALSE;
    if (IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_MPEG1Audio)
            || IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_MPEG1System))
        return S_OK;
    if (IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_MPEG1Video)
            || IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_MPEG1VideoCD))
        FIXME("Unsupported subtype %s.\n", wine_dbgstr_guid(&mt->subtype));
    return S_FALSE;
}

static HRESULT mpeg_splitter_sink_get_media_type(struct strmbase_pin *pin,
        unsigned int index, AM_MEDIA_TYPE *mt)
{
    static const GUID* const subtypes[] = {
        &MEDIASUBTYPE_MPEG1System,
        &MEDIASUBTYPE_MPEG1VideoCD,
        &MEDIASUBTYPE_MPEG1Video,
        &MEDIASUBTYPE_MPEG1Audio,
    };
    if (index >= ARRAY_SIZE(subtypes))
        return S_FALSE;

    memset(mt, 0, sizeof(*mt));
    mt->majortype = MEDIATYPE_Stream;
    mt->subtype = *subtypes[index];
    mt->bFixedSizeSamples = TRUE;
    mt->bTemporalCompression = TRUE;
    mt->lSampleSize = 1;

    return S_OK;
}

static HRESULT mpeg_splitter_sink_connect(struct strmbase_sink *iface, IPin *peer, const AM_MEDIA_TYPE *pmt)
{
    struct parser *filter = parser_from_strmbase_sink(iface);
    INT64 duration;
    HRESULT hr;
    UINT i;

    if (FAILED(hr = parser_sink_connect(iface, peer, pmt, &duration)))
        return hr;

    /* Seek the reader to the end. RE:D Cherish! depends on this. */
    if (IsEqualGUID(&pmt->subtype, &MEDIASUBTYPE_MPEG1System)
            && IsEqualGUID(&pmt->majortype, &MEDIATYPE_Stream))
    {
        LONGLONG file_size, unused;
        IAsyncReader_Length(filter->reader, &file_size, &unused);
        IAsyncReader_SyncRead(filter->reader, file_size, 0, NULL);
    }

    for (i = 0; i < filter->stream_count; ++i)
    {
        AM_MEDIA_TYPE mt;

        if (FAILED(hr = get_stream_media_type(filter->winedmo_demuxer, i, &mt)))
            continue;

        if (IsEqualGUID(&mt.majortype, &MEDIATYPE_Video))
        {
            FreeMediaType(&mt);
            if (!parser_source_create(filter, i, L"Video"))
                return E_FAIL;
        }
        else if (IsEqualGUID(&mt.majortype, &MEDIATYPE_Audio))
        {
            FreeMediaType(&mt);
            if (!parser_source_create(filter, i, L"Audio"))
                return E_FAIL;
        }
        else
        {
            TRACE("unexpected format %s\n", debugstr_guid(&mt.majortype));
            FreeMediaType(&mt);
            return E_FAIL;
        }
    }

    for (i = 0; i < filter->source_count; ++i)
    {
        struct parser_source *source = filter->sources[i];
        source->seek.llDuration = source->seek.llStop = duration;
        source->seek.llCurrent = 0;
    }

    return hr;
}

static const struct strmbase_sink_ops mpeg_splitter_sink_ops =
{
    .base.pin_query_accept = mpeg_splitter_sink_query_accept,
    .base.pin_get_media_type = mpeg_splitter_sink_get_media_type,
    .sink_connect = mpeg_splitter_sink_connect,
    .sink_disconnect = parser_sink_disconnect,
};

static HRESULT mpeg_splitter_query_interface(struct strmbase_filter *iface, REFIID iid, void **out)
{
    struct parser *filter = filter_from_strmbase_filter(iface);

    if (IsEqualGUID(iid, &IID_IAMStreamSelect))
    {
        *out = &filter->IAMStreamSelect_iface;
        IUnknown_AddRef((IUnknown *)*out);
        return S_OK;
    }

    return E_NOINTERFACE;
}

static const struct strmbase_filter_ops mpeg_splitter_ops =
{
    .filter_query_interface = mpeg_splitter_query_interface,
    .filter_get_pin = parser_get_pin,
    .filter_destroy = parser_destroy,
    .filter_init_stream = parser_init_stream,
    .filter_cleanup_stream = parser_cleanup_stream,
};

HRESULT mpeg1_splitter_create(IUnknown *outer, IUnknown **out)
{
    struct parser *object;
    NTSTATUS status;
    HRESULT hr;

    if ((status = winedmo_demuxer_check("video/mpeg")))
    {
        static const GUID CLSID_wg_mpeg1_splitter = {0xa8edbf98,0x2442,0x42c5,{0x85,0xa1,0xab,0x05,0xa5,0x80,0xdf,0x53}};
        WARN("Unsupported demuxer, status %#lx.\n", status);
    	return CoCreateInstance(&CLSID_wg_mpeg1_splitter, outer, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)out);
    }

    if (FAILED(hr = parser_create(TRUE, &object)))
        return hr;

    strmbase_filter_init(&object->filter, outer, &CLSID_MPEG1Splitter, &mpeg_splitter_ops);
    strmbase_sink_init(&object->sink, &object->filter, L"Input", &mpeg_splitter_sink_ops, NULL);
    object->IAMStreamSelect_iface.lpVtbl = &stream_select_vtbl;
    object->enum_sink_first = TRUE;

    TRACE("Created MPEG-1 splitter %p.\n", object);
    *out = &object->filter.IUnknown_inner;
    return hr;
}

static HRESULT wave_parser_sink_query_accept(struct strmbase_pin *iface, const AM_MEDIA_TYPE *mt)
{
    if (!IsEqualGUID(&mt->majortype, &MEDIATYPE_Stream))
        return S_FALSE;
    if (IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_WAVE))
        return S_OK;
    if (IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_AU) || IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_AIFF))
        FIXME("AU and AIFF files are not yet supported.\n");
    return S_FALSE;
}

static HRESULT wave_parser_sink_connect(struct strmbase_sink *iface, IPin *peer, const AM_MEDIA_TYPE *pmt)
{
    struct parser *filter = parser_from_strmbase_sink(iface);
    INT64 duration;
    HRESULT hr;
    UINT i;

    if (FAILED(hr = parser_sink_connect(iface, peer, pmt, &duration)))
        return hr;
    if (!parser_source_create(filter, 0, L"output"))
        return E_FAIL;

    for (i = 0; i < filter->source_count; ++i)
    {
        struct parser_source *source = filter->sources[i];
        source->seek.llDuration = source->seek.llStop = duration;
        source->seek.llCurrent = 0;
    }

    return hr;
}

static const struct strmbase_sink_ops wave_parser_sink_ops =
{
    .base.pin_query_accept = wave_parser_sink_query_accept,
    .sink_connect = wave_parser_sink_connect,
    .sink_disconnect = parser_sink_disconnect,
};

HRESULT wave_parser_create(IUnknown *outer, IUnknown **out)
{
    struct parser *object;
    NTSTATUS status;
    HRESULT hr;

    if ((status = winedmo_demuxer_check("audio/wav")))
    {
        static const GUID CLSID_wg_wave_parser = {0x3f839ec7,0x5ea6,0x49e1,{0x80,0xc2,0x1e,0xa3,0x00,0xf8,0xb0,0xe0}};
        WARN("Unsupported demuxer, status %#lx.\n", status);
    	return CoCreateInstance(&CLSID_wg_wave_parser, outer, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)out);
    }

    if (FAILED(hr = parser_create(TRUE, &object)))
        return hr;

    strmbase_filter_init(&object->filter, outer, &CLSID_WAVEParser, &filter_ops);
    strmbase_sink_init(&object->sink, &object->filter, L"input pin", &wave_parser_sink_ops, NULL);

    TRACE("Created WAVE parser %p.\n", object);
    *out = &object->filter.IUnknown_inner;
    return hr;
}
