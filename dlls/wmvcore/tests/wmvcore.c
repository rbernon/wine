/*
 * Copyright 2017 Alistair Leslie-Hughes
 * Copyright 2019 Vijay Kiran Kamuju
 * Copyright 2021 Zebediah Figura for CodeWeavers
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

#include <stdbool.h>
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include "initguid.h"
#include "wmsdk.h"
#include "wmsecure.h"
#include "amvideo.h"
#include "uuids.h"
#include "wmcodecdsp.h"
#include "dvdmedia.h"

#include "wine/list.h"
#include "wine/test.h"

DEFINE_GUID( WMProfile_V80_768Video, 
             0x74d01102,
             0xe71a,
             0x4820,
             0x8f, 0xd, 0x13, 0xd2, 0xec, 0x1e, 0x48, 0x72 );
DEFINE_GUID(WMMEDIASUBTYPE_PCM,0x00000001,0x0000,0x0010,0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71);
DEFINE_GUID(WMMEDIASUBTYPE_I420,0x30323449,0x0000,0x0010,0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71);

static const DWORD test_wmv_duration = 20540000;

HRESULT WINAPI WMCreateWriterPriv(IWMWriter **writer);

struct async_start_info
{
    QWORD start;
    QWORD duration;
    void *context;
};

struct async_op
{
    enum async_op_type
    {
        ASYNC_OP_OPEN_STREAM,
        ASYNC_OP_OPEN_FILE,
        ASYNC_OP_START,
        ASYNC_OP_STOP,
        ASYNC_OP_CLOSE,
    } type;
    void *data;
    struct list entry;
};

struct async_reader_stream
{
    struct async_reader *reader;
    DWORD index;

    bool sample_requested;
    CONDITION_VARIABLE cv;
    HANDLE stream_thread;

    INSSBuffer *sample;
    QWORD pts;
    QWORD duration;
    DWORD flags;
    DWORD output;
    HRESULT result;
};

struct async_reader
{
    IWMReader IWMReader_iface;
    IWMReaderAdvanced6 IWMReaderAdvanced6_iface;
    IWMReaderAccelerator IWMReaderAccelerator_iface;
    IWMReaderNetworkConfig2 IWMReaderNetworkConfig2_iface;
    IWMReaderStreamClock IWMReaderStreamClock_iface;
    IWMReaderTypeNegotiation IWMReaderTypeNegotiation_iface;
    IWMProfile3 IWMProfile3_iface;
    IWMHeaderInfo3 IWMHeaderInfo3_iface;
    IWMLanguageList IWMLanguageList_iface;
    IWMPacketSize2 IWMPacketSize2_iface;
    IWMReaderPlaylistBurn IWMReaderPlaylistBurn_iface;
    IWMReaderTimecode IWMReaderTimecode_iface;
    IUnknown *clock_inner;
    LONG refcount;

    IWMSyncReader2 *reader;
    IWMProfile3 *profile;
    IWMHeaderInfo3 *header_info;

    CRITICAL_SECTION cs;

    IWMReaderCallbackAdvanced *callback_advanced;
    IWMReaderAllocatorEx *allocator;
    IWMReaderCallback *callback;
    void *context;

    BOOL allocate_for_stream[64];
    BOOL allocate_for_output[64];

    REFERENCE_TIME clock_start;
    LARGE_INTEGER clock_frequency;

    struct async_reader_stream *streams;
    HANDLE callback_thread;
    CRITICAL_SECTION callback_cs;
    CONDITION_VARIABLE callback_cv;

    bool opened, running, sample_ready;
    struct list async_ops;

    bool user_clock;
    QWORD user_time;
};

struct allocator
{
    IWMReaderAllocatorEx IWMReaderAllocatorEx_iface;
    LONG refcount;

    IWMReaderCallbackAdvanced *callback;
    struct async_reader *reader;
};

static struct allocator *impl_from_IWMReaderAllocatorEx(IWMReaderAllocatorEx *iface)
{
    return CONTAINING_RECORD(iface, struct allocator, IWMReaderAllocatorEx_iface);
}

static HRESULT WINAPI allocator_QueryInterface(IWMReaderAllocatorEx *iface, REFIID iid, void **out)
{
    struct allocator *allocator = impl_from_IWMReaderAllocatorEx(iface);

    if (IsEqualIID(iid, &IID_IUnknown)
            || IsEqualIID(iid, &IID_IWMReaderAllocatorEx))
        *out = &allocator->IWMReaderAllocatorEx_iface;
    else
    {
        ok(0, "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static ULONG WINAPI allocator_AddRef(IWMReaderAllocatorEx *iface)
{
    struct allocator *allocator = impl_from_IWMReaderAllocatorEx(iface);
    ULONG refcount = InterlockedIncrement(&allocator->refcount);
    return refcount;
}

static ULONG WINAPI allocator_Release(IWMReaderAllocatorEx *iface)
{
    struct allocator *allocator = impl_from_IWMReaderAllocatorEx(iface);
    ULONG refcount = InterlockedDecrement(&allocator->refcount);

    if (!refcount)
    {
        if (allocator->callback)
            IWMReaderCallbackAdvanced_Release(allocator->callback);
        free(allocator);
    }

    return refcount;
}

struct async_buffer
{
    INSSBuffer3 INSSBuffer3_iface;
    LONG refcount;

    DWORD size, capacity;
    BYTE data[1];
};

static struct async_buffer *impl_from_INSSBuffer3(INSSBuffer3 *iface)
{
    return CONTAINING_RECORD(iface, struct async_buffer, INSSBuffer3_iface);
}

static HRESULT WINAPI async_buffer_QueryInterface(INSSBuffer3 *iface, REFIID iid, void **out)
{
    struct async_buffer *buffer = impl_from_INSSBuffer3(iface);

    if (IsEqualGUID(iid, &IID_IUnknown)
            || IsEqualGUID(iid, &IID_INSSBuffer)
            || IsEqualGUID(iid, &IID_INSSBuffer2)
            || IsEqualGUID(iid, &IID_INSSBuffer3))
        *out = &buffer->INSSBuffer3_iface;
    else
    {
        *out = NULL;
        ok(0, "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static ULONG WINAPI async_buffer_AddRef(INSSBuffer3 *iface)
{
    struct async_buffer *buffer = impl_from_INSSBuffer3(iface);
    ULONG refcount = InterlockedIncrement(&buffer->refcount);
    return refcount;
}

static ULONG WINAPI async_buffer_Release(INSSBuffer3 *iface)
{
    struct async_buffer *buffer = impl_from_INSSBuffer3(iface);
    ULONG refcount = InterlockedDecrement(&buffer->refcount);

    if (!refcount)
        free(buffer);

    return refcount;
}

static HRESULT WINAPI async_buffer_GetLength(INSSBuffer3 *iface, DWORD *size)
{
    struct async_buffer *buffer = impl_from_INSSBuffer3(iface);
    *size = buffer->size;
    return S_OK;
}

static HRESULT WINAPI async_buffer_SetLength(INSSBuffer3 *iface, DWORD size)
{
    struct async_buffer *buffer = impl_from_INSSBuffer3(iface);

    if (size > buffer->capacity)
        return E_INVALIDARG;

    buffer->size = size;
    return S_OK;
}

static HRESULT WINAPI async_buffer_GetMaxLength(INSSBuffer3 *iface, DWORD *size)
{
    struct async_buffer *buffer = impl_from_INSSBuffer3(iface);
    *size = buffer->capacity;
    return S_OK;
}

static HRESULT WINAPI async_buffer_GetBuffer(INSSBuffer3 *iface, BYTE **data)
{
    struct async_buffer *buffer = impl_from_INSSBuffer3(iface);
    *data = buffer->data;
    return S_OK;
}

static HRESULT WINAPI async_buffer_GetBufferAndLength(INSSBuffer3 *iface, BYTE **data, DWORD *size)
{
    struct async_buffer *buffer = impl_from_INSSBuffer3(iface);
    *size = buffer->size;
    *data = buffer->data;
    return S_OK;
}

static const INSSBuffer3Vtbl async_buffer_vtbl =
{
    async_buffer_QueryInterface,
    async_buffer_AddRef,
    async_buffer_Release,
    async_buffer_GetLength,
    async_buffer_SetLength,
    async_buffer_GetMaxLength,
    async_buffer_GetBuffer,
    async_buffer_GetBufferAndLength,
};

static HRESULT allocate_buffer(DWORD size, INSSBuffer **out)
{
    struct async_buffer *object;

    if (!(object = calloc(1, offsetof(struct async_buffer, data[size]))))
        return E_OUTOFMEMORY;

    object->INSSBuffer3_iface.lpVtbl = &async_buffer_vtbl;
    object->refcount = 1;
    object->capacity = size;

    *out = (INSSBuffer *)&object->INSSBuffer3_iface;
    return S_OK;
}

static HRESULT WINAPI allocator_AllocateForStreamEx(IWMReaderAllocatorEx *iface,
        WORD stream_number, DWORD size, INSSBuffer **sample, DWORD flags,
        QWORD pts, QWORD duration, void *context)
{
    struct allocator *allocator = impl_from_IWMReaderAllocatorEx(iface);
    struct async_reader *reader = allocator->reader;

    if (!reader->allocate_for_stream[stream_number - 1])
        return allocate_buffer(size, sample);

    if (allocator->callback)
        return IWMReaderCallbackAdvanced_AllocateForStream(allocator->callback,
                stream_number, size, sample, context);

    return E_NOTIMPL;
}

static HRESULT WINAPI allocator_AllocateForOutputEx(IWMReaderAllocatorEx *iface,
        DWORD output, DWORD size, INSSBuffer **sample, DWORD flags,
        QWORD pts, QWORD duration, void *context)
{
    struct allocator *allocator = impl_from_IWMReaderAllocatorEx(iface);
    struct async_reader *reader = allocator->reader;

    if (!reader->allocate_for_output[output])
        return allocate_buffer(size, sample);

    if (allocator->callback)
        return IWMReaderCallbackAdvanced_AllocateForOutput(allocator->callback,
                output, size, sample, context);

    return E_NOTIMPL;
}

static const IWMReaderAllocatorExVtbl allocator_vtbl =
{
    allocator_QueryInterface,
    allocator_AddRef,
    allocator_Release,
    allocator_AllocateForStreamEx,
    allocator_AllocateForOutputEx,
};

static HRESULT allocator_create(IWMReaderCallback *callback, struct async_reader *reader, IWMReaderAllocatorEx **out)
{
    struct allocator *allocator;
    HRESULT hr;

    if (!(allocator = calloc(1, sizeof(*allocator))))
        return E_OUTOFMEMORY;
    allocator->IWMReaderAllocatorEx_iface.lpVtbl = &allocator_vtbl;
    allocator->refcount = 1;
    allocator->reader = reader;

    if (FAILED(hr = IWMReaderCallback_QueryInterface(callback,
            &IID_IWMReaderCallbackAdvanced, (void **)&allocator->callback)))
    {
        ok(0, "Got hr %#lx.\n", hr);
        allocator->callback = NULL;
    }

    *out = &allocator->IWMReaderAllocatorEx_iface;
    return S_OK;
}

static REFERENCE_TIME async_reader_get_time(const struct async_reader *reader, bool absolute)
{
    LARGE_INTEGER time;
    QueryPerformanceCounter(&time);
    return (time.QuadPart * 1000) / reader->clock_frequency.QuadPart * 10000 - (absolute ? 0 : reader->clock_start);
}

static bool async_reader_needs_wait(struct async_reader *reader, QWORD pts, DWORD *timeout)
{
    if (!reader->user_clock)
    {
        REFERENCE_TIME current_time = async_reader_get_time(reader, false);
        *timeout = (pts - current_time) / 10000;
        return pts > current_time;
    }

    *timeout = INFINITE;
    return pts > reader->user_time;
}

static bool async_reader_wait_pts(struct async_reader *reader, QWORD pts)
{
    IWMReaderCallbackAdvanced *callback_advanced = reader->callback_advanced;
    DWORD timeout;
    bool ret;

    if (callback_advanced && reader->user_clock && pts > reader->user_time)
    {
        QWORD user_time = reader->user_time;
        LeaveCriticalSection(&reader->callback_cs);
        IWMReaderCallbackAdvanced_OnTime(callback_advanced, user_time, reader->context);
        EnterCriticalSection(&reader->callback_cs);
    }

    while ((ret = reader->running && list_empty(&reader->async_ops)) && async_reader_needs_wait(reader, pts, &timeout))
        SleepConditionVariableCS(&reader->callback_cv, &reader->callback_cs, timeout);

    return ret;
}

static void async_reader_deliver_sample(struct async_reader *reader, DWORD output, WORD stream,
        QWORD pts, QWORD duration, DWORD flags, INSSBuffer *sample)
{
    IWMReaderCallbackAdvanced *callback_advanced = reader->callback_advanced;
    IWMReaderCallback *callback = reader->callback;
    BOOL read_compressed;
    HRESULT hr;

    if (FAILED(hr = IWMSyncReader2_GetReadStreamSamples(reader->reader, stream,
            &read_compressed)))
    {
        ok(0, "Got hr %#lx.\n", hr);
        read_compressed = FALSE;
    }

    LeaveCriticalSection(&reader->callback_cs);
    if (read_compressed)
        hr = IWMReaderCallbackAdvanced_OnStreamSample(callback_advanced,
                stream, pts, duration, flags, sample, reader->context);
    else
        hr = IWMReaderCallback_OnSample(callback, output, pts, duration,
                flags, sample, reader->context);
    EnterCriticalSection(&reader->callback_cs);
}

static DWORD WINAPI async_reader_stream_thread(void *arg)
{
    struct async_reader_stream *stream = arg;
    struct async_reader *reader = stream->reader;
    HRESULT hr = S_OK;

    EnterCriticalSection(&reader->callback_cs);

    while (reader->running)
    {
        if (!stream->sample_requested)
        {
            SleepConditionVariableCS(&stream->cv, &reader->callback_cs, INFINITE);
            continue;
        }

        while (stream->sample_requested)
        {
            stream->sample_requested = false;
            LeaveCriticalSection(&reader->callback_cs);
            hr = IWMSyncReader2_GetNextSample(reader->reader, stream->index + 1,
                    &stream->sample, &stream->pts, &stream->duration, &stream->flags,
                    &stream->output, NULL);
            EnterCriticalSection(&reader->callback_cs);
        }

        if (FAILED(stream->result = hr))
            ok(hr == S_OK || hr == NS_E_NO_MORE_SAMPLES,
                    "Failed to get stream %lu sample, hr %#lx.\n", stream->index, hr);

        reader->sample_ready = true;
        WakeConditionVariable(&reader->callback_cv);
    }

    LeaveCriticalSection(&reader->callback_cs);
    return 0;
}

static void async_reader_close_stream(struct async_reader *reader, DWORD index)
{
    struct async_reader_stream *stream = reader->streams + index;

    if (stream->stream_thread)
    {
        WakeConditionVariable(&stream->cv);
        WaitForSingleObject(stream->stream_thread, INFINITE);
        CloseHandle(stream->stream_thread);
        stream->stream_thread = NULL;
    }
}

static HRESULT async_reader_open_stream(struct async_reader *reader, DWORD index)
{
    struct async_reader_stream *stream = reader->streams + index;

    if (stream->stream_thread)
        return S_OK;

    stream->index = index;
    stream->reader = reader;
    if (!(stream->stream_thread = CreateThread(NULL, 0, async_reader_stream_thread, stream, 0, NULL)))
        goto error;

    return S_OK;

error:
    async_reader_close_stream(reader, index);
    return E_OUTOFMEMORY;
}

static void async_reader_stream_request_sample(struct async_reader *reader, DWORD index)
{
    struct async_reader_stream *stream = reader->streams + index;

    stream->result = E_PENDING;
    stream->sample_requested = true;
    WakeConditionVariable(&stream->cv);
}

static HRESULT async_reader_find_next_stream(struct async_reader *reader, DWORD stream_count, struct async_reader_stream **out)
{
    struct async_reader_stream *first = NULL;
    WMT_STREAM_SELECTION selection;
    DWORD i;

    for (i = 0; i < stream_count; ++i)
    {
        struct async_reader_stream *stream = reader->streams + i;

        if (FAILED(IWMSyncReader2_GetStreamSelected(reader->reader, i + 1, &selection))
                || selection == WMT_OFF)
            continue;
        if (stream->result == E_PENDING)
            return E_PENDING;
        if (FAILED(stream->result))
            continue;

        if (!first || first->pts > stream->pts)
            first = stream;
    }

    if (!first)
        return NS_E_NO_MORE_SAMPLES;

    *out = first;
    return S_OK;
}

static void async_reader_deliver_samples(struct async_reader *reader, DWORD stream_count)
{
    static const DWORD zero;

    IWMReaderCallbackAdvanced *callback_advanced = reader->callback_advanced;
    IWMReaderCallback *callback = reader->callback;
    struct async_reader_stream *stream;
    QWORD pts, duration;
    DWORD output, flags;
    INSSBuffer *sample;
    HRESULT hr = S_OK;

    while (reader->running && list_empty(&reader->async_ops))
    {
        if (FAILED(hr = async_reader_find_next_stream(reader, stream_count, &stream)))
            break;

        sample = stream->sample;
        stream->sample = NULL;
        pts = stream->pts;
        duration = stream->duration;
        flags = stream->flags;
        output = stream->output;
        async_reader_stream_request_sample(reader, stream->index);

        if (async_reader_wait_pts(reader, pts))
            async_reader_deliver_sample(reader, output, stream->index + 1,
                    pts, duration, flags, sample);

        INSSBuffer_Release(sample);
    }

    if (hr == NS_E_NO_MORE_SAMPLES)
    {
        BOOL user_clock = reader->user_clock;
        QWORD user_time = reader->user_time;

        LeaveCriticalSection(&reader->callback_cs);

        IWMReaderCallback_OnStatus(callback, WMT_END_OF_STREAMING, S_OK,
                WMT_TYPE_DWORD, (BYTE *)&zero, reader->context);
        IWMReaderCallback_OnStatus(callback, WMT_EOF, S_OK,
                WMT_TYPE_DWORD, (BYTE *)&zero, reader->context);

        if (user_clock && callback_advanced)
        {
            /* We can only get here if user_time is greater than the PTS
             * of all samples, in which case we cannot have sent this
             * notification already. */
            IWMReaderCallbackAdvanced_OnTime(callback_advanced,
                    user_time, reader->context);
        }

        EnterCriticalSection(&reader->callback_cs);
    }
    else if (hr != S_OK && hr != E_PENDING)
    {
        ok(0, "Failed to get sample, hr %#lx.\n", hr);
    }
}

static DWORD WINAPI async_reader_callback_thread(void *arg)
{
    static const DWORD zero;

    struct async_reader *reader = arg;
    DWORD i, stream_count = 0;
    struct list *entry;
    HRESULT hr;

    EnterCriticalSection(&reader->callback_cs);

    while (reader->running)
    {
        if ((entry = list_head(&reader->async_ops)))
        {
            struct async_op *op = LIST_ENTRY(entry, struct async_op, entry);
            list_remove(&op->entry);

            hr = list_empty(&reader->async_ops) ? S_OK : E_ABORT;
            switch (op->type)
            {
                case ASYNC_OP_OPEN_FILE:
                    if (SUCCEEDED(hr) && FAILED(hr = IWMSyncReader2_Open(reader->reader, op->data)))
                    {
                        ok(0, "Got hr %#lx.\n", hr);
                        reader->running = false;
                    }
                    free(op->data);
                    goto case_ASYNC_OP_OPEN;

                case ASYNC_OP_OPEN_STREAM:
                    if (SUCCEEDED(hr) && FAILED(hr = IWMSyncReader2_OpenStream(reader->reader, op->data)))
                    {
                        ok(0, "Got hr %#lx.\n", hr);
                        reader->running = false;
                    }
                    IStream_Release(op->data);
                    goto case_ASYNC_OP_OPEN;

                case_ASYNC_OP_OPEN:
                    if (SUCCEEDED(hr) && FAILED(hr = IWMProfile3_GetStreamCount(reader->profile, &stream_count)))
                        reader->running = false;

                    if (SUCCEEDED(hr) && !(reader->streams = calloc(stream_count, sizeof(*reader->streams))))
                    {
                        reader->running = false;
                        hr = E_OUTOFMEMORY;
                    }

                    for (i = 0; SUCCEEDED(hr) && i < stream_count; ++i)
                    {
                        hr = IWMSyncReader2_SetAllocateForOutput(reader->reader, i, reader->allocator);
                        ok(hr == S_OK, "Got hr %#lx\n", hr);
                        hr = IWMSyncReader2_SetAllocateForStream(reader->reader, i + 1, reader->allocator);
                        ok(hr == S_OK, "Got hr %#lx\n", hr);

                        if (FAILED(hr = async_reader_open_stream(reader, i)))
                        {
                            ok(hr == S_OK, "Got hr %#lx\n", hr);
                            reader->running = false;
                        }
                    }

                    reader->opened = SUCCEEDED(hr);
                    LeaveCriticalSection(&reader->callback_cs);
                    IWMReaderCallback_OnStatus(reader->callback, WMT_OPENED, hr,
                            WMT_TYPE_DWORD, (BYTE *)&zero, reader->context);
                    EnterCriticalSection(&reader->callback_cs);
                    break;

                case ASYNC_OP_START:
                {
                    struct async_start_info *info = op->data;

                    reader->context = info->context;
                    if (SUCCEEDED(hr))
                        hr = IWMSyncReader2_SetRange(reader->reader, info->start, info->duration);
                    if (SUCCEEDED(hr))
                    {
                        reader->clock_start = async_reader_get_time(reader, true);

                        for (i = 0; i < stream_count; ++i)
                            async_reader_stream_request_sample(reader, i);

                        /* Try delivering EOF immediately if no streams are selected. */
                        reader->sample_ready = true;
                    }
                    free(info);

                    LeaveCriticalSection(&reader->callback_cs);
                    IWMReaderCallback_OnStatus(reader->callback, WMT_STARTED, hr,
                            WMT_TYPE_DWORD, (BYTE *)&zero, reader->context);
                    EnterCriticalSection(&reader->callback_cs);
                    break;
                }

                case ASYNC_OP_STOP:
                    if (SUCCEEDED(hr))
                        reader->clock_start = 0;

                    LeaveCriticalSection(&reader->callback_cs);
                    IWMReaderCallback_OnStatus(reader->callback, WMT_STOPPED, hr,
                            WMT_TYPE_DWORD, (BYTE *)&zero, reader->context);
                    EnterCriticalSection(&reader->callback_cs);
                    break;

                case ASYNC_OP_CLOSE:
                    LeaveCriticalSection(&reader->callback_cs);
                    IWMReaderCallback_OnStatus(reader->callback, WMT_CLOSED, hr,
                            WMT_TYPE_DWORD, (BYTE *)&zero, reader->context);
                    EnterCriticalSection(&reader->callback_cs);

                    if (SUCCEEDED(hr))
                        reader->running = false;
                    break;
            }

            free(op);
        }

        if (reader->sample_ready && reader->clock_start)
        {
            async_reader_deliver_samples(reader, stream_count);
            reader->sample_ready = false;
        }

        if (reader->running && list_empty(&reader->async_ops))
            SleepConditionVariableCS(&reader->callback_cv, &reader->callback_cs, INFINITE);
    }

    LeaveCriticalSection(&reader->callback_cs);

    for (i = 0; reader->streams && i < stream_count; ++i)
        async_reader_close_stream(reader, i);
    free(reader->streams);
    reader->streams = NULL;

    if (reader->opened)
    {
        ok(0, "closing\n");
        IWMSyncReader2_Close(reader->reader);
    }
    reader->opened = false;

    return 0;
}

static HRESULT async_reader_queue_op(struct async_reader *reader, enum async_op_type type, void *data)
{
    struct async_op *op;

    EnterCriticalSection(&reader->callback_cs);

    if (!(op = calloc(1, sizeof(*op))))
    {
        LeaveCriticalSection(&reader->callback_cs);
        return E_OUTOFMEMORY;
    }
    op->type = type;
    op->data = data;

    list_add_tail(&reader->async_ops, &op->entry);

    LeaveCriticalSection(&reader->callback_cs);
    WakeConditionVariable(&reader->callback_cv);

    return S_OK;
}

static void async_reader_close(struct async_reader *reader, bool force)
{
    struct async_op *op, *next;
    HRESULT hr;

    if (!force && FAILED(hr = async_reader_queue_op(reader, ASYNC_OP_CLOSE, NULL)))
    {
        ok(0, "Got hr %#lx.\n", hr);
        force = true;
    }

    if (force)
    {
        EnterCriticalSection(&reader->callback_cs);
        reader->running = false;
        LeaveCriticalSection(&reader->callback_cs);
        WakeConditionVariable(&reader->callback_cv);
    }

    if (reader->callback_thread)
    {
        WaitForSingleObject(reader->callback_thread, INFINITE);
        CloseHandle(reader->callback_thread);
        reader->callback_thread = NULL;
    }

    LIST_FOR_EACH_ENTRY_SAFE(op, next, &reader->async_ops, struct async_op, entry)
    {
        list_remove(&op->entry);
        free(op);
    }

    if (reader->allocator)
        IWMReaderAllocatorEx_Release(reader->allocator);
    reader->allocator = NULL;

    if (reader->callback_advanced)
        IWMReaderCallbackAdvanced_Release(reader->callback_advanced);
    reader->callback_advanced = NULL;

    if (reader->callback)
        IWMReaderCallback_Release(reader->callback);
    reader->callback = NULL;
    reader->context = NULL;
}

static HRESULT async_reader_open(struct async_reader *reader, IWMReaderCallback *callback, void *context)
{
    HRESULT hr = E_OUTOFMEMORY;

    IWMReaderCallback_AddRef((reader->callback = callback));
    reader->context = context;

    if (FAILED(hr = allocator_create(reader->callback, reader, &reader->allocator)))
    {
        ok(0, "Got hr %#lx.\n", hr);
        goto error;
    }

    if (FAILED(hr = IWMReaderCallback_QueryInterface(callback, &IID_IWMReaderCallbackAdvanced,
            (void **)&reader->callback_advanced)))
    {
        ok(0, "Got hr %#lx.\n", hr);
        reader->callback_advanced = NULL;
    }

    reader->running = true;
    if (!(reader->callback_thread = CreateThread(NULL, 0, async_reader_callback_thread, reader, 0, NULL)))
        goto error;

    return S_OK;

error:
    async_reader_close(reader, true);
    return hr;
}

static struct async_reader *impl_from_IWMReader(IWMReader *iface)
{
    return CONTAINING_RECORD(iface, struct async_reader, IWMReader_iface);
}

static HRESULT WINAPI WMReader_QueryInterface(IWMReader *iface, REFIID iid, void **out)
{
    struct async_reader *reader = impl_from_IWMReader(iface);

    if (IsEqualIID(iid, &IID_IUnknown)
            || IsEqualIID(iid, &IID_IWMReader))
        *out = &reader->IWMReader_iface;
    else if (IsEqualIID(iid, &IID_IWMReaderAccelerator))
        *out = &reader->IWMReaderAccelerator_iface;
    else if (IsEqualIID(iid, &IID_IWMReaderAdvanced)
            || IsEqualIID(iid, &IID_IWMReaderAdvanced2)
            || IsEqualIID(iid, &IID_IWMReaderAdvanced3)
            || IsEqualIID(iid, &IID_IWMReaderAdvanced4)
            || IsEqualIID(iid, &IID_IWMReaderAdvanced5)
            || IsEqualIID(iid, &IID_IWMReaderAdvanced6))
        *out = &reader->IWMReaderAdvanced6_iface;
    else if (IsEqualIID(iid, &IID_IWMReaderNetworkConfig)
            || IsEqualIID(iid, &IID_IWMReaderNetworkConfig2))
        *out = &reader->IWMReaderNetworkConfig2_iface;
    else if (IsEqualIID(iid, &IID_IWMReaderStreamClock))
        *out = &reader->IWMReaderStreamClock_iface;
    else if (IsEqualIID(iid, &IID_IWMReaderTypeNegotiation))
        *out = &reader->IWMReaderTypeNegotiation_iface;
    else if (IsEqualIID(iid, &IID_IWMHeaderInfo)
            || IsEqualIID(iid, &IID_IWMHeaderInfo2)
            || IsEqualIID(iid, &IID_IWMHeaderInfo3))
        *out = &reader->IWMHeaderInfo3_iface;
    else if (IsEqualIID(iid, &IID_IWMLanguageList))
        *out = &reader->IWMLanguageList_iface;
    else if (IsEqualIID(iid, &IID_IWMPacketSize)
            || IsEqualIID(iid, &IID_IWMPacketSize2))
        *out = &reader->IWMPacketSize2_iface;
    else if (IsEqualIID(iid, &IID_IWMProfile)
            || IsEqualIID(iid, &IID_IWMProfile2)
            || IsEqualIID(iid, &IID_IWMProfile3))
        *out = &reader->IWMProfile3_iface;
    else if (IsEqualIID(iid, &IID_IWMReaderPlaylistBurn))
        *out = &reader->IWMReaderPlaylistBurn_iface;
    else if (IsEqualIID(iid, &IID_IWMReaderTimecode))
        *out = &reader->IWMReaderTimecode_iface;
    else if (IsEqualIID(iid, &IID_IReferenceClock))
        return IUnknown_QueryInterface(reader->clock_inner, iid, out);
    else
    {
        ok(0, "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static ULONG WINAPI WMReader_AddRef(IWMReader *iface)
{
    struct async_reader *reader = impl_from_IWMReader(iface);
    ULONG refcount = InterlockedIncrement(&reader->refcount);
    return refcount;
}

static ULONG WINAPI WMReader_Release(IWMReader *iface)
{
    struct async_reader *reader = impl_from_IWMReader(iface);
    ULONG refcount = InterlockedDecrement(&reader->refcount);

    if (!refcount)
    {
        async_reader_close(reader, true);

        DeleteCriticalSection(&reader->callback_cs);
        DeleteCriticalSection(&reader->cs);

        IWMHeaderInfo3_Release(reader->header_info);
        IWMProfile3_Release(reader->profile);
        IWMSyncReader2_Release(reader->reader);
        IUnknown_Release(reader->clock_inner);
        free(reader);
    }

    return refcount;
}

static HRESULT WINAPI WMReader_Open(IWMReader *iface, const WCHAR *url,
        IWMReaderCallback *callback, void *context)
{
    struct async_reader *reader = impl_from_IWMReader(iface);
    HRESULT hr;
    WCHAR *tmp;

    if (!(tmp = wcsdup(url)))
        return E_OUTOFMEMORY;

    EnterCriticalSection(&reader->cs);

    if (reader->opened)
        hr = E_UNEXPECTED;
    else if (FAILED(hr = async_reader_open(reader, callback, context)))
        ok(0, "Failed to open %s, hr %#lx.\n", debugstr_w(url), hr);
    else if (FAILED(hr = async_reader_queue_op(reader, ASYNC_OP_OPEN_FILE, tmp)))
    {
        ok(0, "Got hr %#lx.\n", hr);
        async_reader_close(reader, true);
    }

    LeaveCriticalSection(&reader->cs);

    if (FAILED(hr))
    {
        ok(0, "Got hr %#lx.\n", hr);
        free(tmp);
    }
    return hr;
}

static HRESULT WINAPI WMReader_Close(IWMReader *iface)
{
    struct async_reader *reader = impl_from_IWMReader(iface);
    HRESULT hr;

    EnterCriticalSection(&reader->cs);

    if (!reader->callback_thread)
        hr = NS_E_INVALID_REQUEST;
    else
    {
        async_reader_close(reader, false);
        hr = S_OK;
    }

    LeaveCriticalSection(&reader->cs);

    return hr;
}

static HRESULT WINAPI WMReader_GetOutputCount(IWMReader *iface, DWORD *count)
{
    struct async_reader *reader = impl_from_IWMReader(iface);
    return IWMSyncReader2_GetOutputCount(reader->reader, count);
}

static HRESULT WINAPI WMReader_GetOutputProps(IWMReader *iface, DWORD output, IWMOutputMediaProps **props)
{
    struct async_reader *reader = impl_from_IWMReader(iface);
    return IWMSyncReader2_GetOutputProps(reader->reader, output, props);
}

static HRESULT WINAPI WMReader_SetOutputProps(IWMReader *iface, DWORD output, IWMOutputMediaProps *props)
{
    struct async_reader *reader = impl_from_IWMReader(iface);
    return IWMSyncReader2_SetOutputProps(reader->reader, output, props);
}

static HRESULT WINAPI WMReader_GetOutputFormatCount(IWMReader *iface, DWORD output, DWORD *count)
{
    struct async_reader *reader = impl_from_IWMReader(iface);
    return IWMSyncReader2_GetOutputFormatCount(reader->reader, output, count);
}

static HRESULT WINAPI WMReader_GetOutputFormat(IWMReader *iface, DWORD output,
        DWORD index, IWMOutputMediaProps **props)
{
    struct async_reader *reader = impl_from_IWMReader(iface);
    return IWMSyncReader2_GetOutputFormat(reader->reader, output, index, props);
}

static HRESULT WINAPI WMReader_Start(IWMReader *iface,
        QWORD start, QWORD duration, float rate, void *context)
{
    struct async_reader *reader = impl_from_IWMReader(iface);
    struct async_start_info *info;
    HRESULT hr;

    if (!(info = calloc(1, sizeof(*info))))
        return E_OUTOFMEMORY;
    info->start = start;
    info->duration = duration;
    info->context = context;

    EnterCriticalSection(&reader->cs);

    if (!reader->callback_thread)
        hr = NS_E_INVALID_REQUEST;
    else
        hr = async_reader_queue_op(reader, ASYNC_OP_START, info);

    LeaveCriticalSection(&reader->cs);

    if (FAILED(hr))
        free(info);
    return hr;
}

static HRESULT WINAPI WMReader_Stop(IWMReader *iface)
{
    struct async_reader *reader = impl_from_IWMReader(iface);
    HRESULT hr;

    EnterCriticalSection(&reader->cs);

    if (!reader->callback_thread)
        hr = E_UNEXPECTED;
    else
        hr = async_reader_queue_op(reader, ASYNC_OP_STOP, NULL);

    LeaveCriticalSection(&reader->cs);

    return hr;
}

static HRESULT WINAPI WMReader_Pause(IWMReader *iface)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReader_Resume(IWMReader *iface)
{
    return E_NOTIMPL;
}

static const IWMReaderVtbl WMReaderVtbl = {
    WMReader_QueryInterface,
    WMReader_AddRef,
    WMReader_Release,
    WMReader_Open,
    WMReader_Close,
    WMReader_GetOutputCount,
    WMReader_GetOutputProps,
    WMReader_SetOutputProps,
    WMReader_GetOutputFormatCount,
    WMReader_GetOutputFormat,
    WMReader_Start,
    WMReader_Stop,
    WMReader_Pause,
    WMReader_Resume
};

static struct async_reader *impl_from_IWMReaderAdvanced6(IWMReaderAdvanced6 *iface)
{
    return CONTAINING_RECORD(iface, struct async_reader, IWMReaderAdvanced6_iface);
}

static HRESULT WINAPI WMReaderAdvanced_QueryInterface(IWMReaderAdvanced6 *iface, REFIID iid, void **out)
{
    struct async_reader *reader = impl_from_IWMReaderAdvanced6(iface);
    return IWMReader_QueryInterface(&reader->IWMReader_iface, iid, out);
}

static ULONG WINAPI WMReaderAdvanced_AddRef(IWMReaderAdvanced6 *iface)
{
    struct async_reader *reader = impl_from_IWMReaderAdvanced6(iface);
    return IWMReader_AddRef(&reader->IWMReader_iface);
}

static ULONG WINAPI WMReaderAdvanced_Release(IWMReaderAdvanced6 *iface)
{
    struct async_reader *reader = impl_from_IWMReaderAdvanced6(iface);
    return IWMReader_Release(&reader->IWMReader_iface);
}

static HRESULT WINAPI WMReaderAdvanced_SetUserProvidedClock(IWMReaderAdvanced6 *iface, BOOL user_clock)
{
    struct async_reader *reader = impl_from_IWMReaderAdvanced6(iface);

    EnterCriticalSection(&reader->callback_cs);
    reader->user_clock = !!user_clock;
    LeaveCriticalSection(&reader->callback_cs);
    WakeConditionVariable(&reader->callback_cv);
    return S_OK;
}

static HRESULT WINAPI WMReaderAdvanced_GetUserProvidedClock(IWMReaderAdvanced6 *iface, BOOL *user_clock)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced_DeliverTime(IWMReaderAdvanced6 *iface, QWORD time)
{
    struct async_reader *reader = impl_from_IWMReaderAdvanced6(iface);

    EnterCriticalSection(&reader->callback_cs);

    if (!reader->user_clock)
    {
        LeaveCriticalSection(&reader->callback_cs);
        return E_UNEXPECTED;
    }

    reader->user_time = time;

    LeaveCriticalSection(&reader->callback_cs);
    WakeConditionVariable(&reader->callback_cv);
    return S_OK;
}

static HRESULT WINAPI WMReaderAdvanced_SetManualStreamSelection(IWMReaderAdvanced6 *iface, BOOL selection)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced_GetManualStreamSelection(IWMReaderAdvanced6 *iface, BOOL *selection)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced_SetStreamsSelected(IWMReaderAdvanced6 *iface,
        WORD count, WORD *stream_numbers, WMT_STREAM_SELECTION *selections)
{
    struct async_reader *reader = impl_from_IWMReaderAdvanced6(iface);
    return IWMSyncReader2_SetStreamsSelected(reader->reader, count, stream_numbers, selections);
}

static HRESULT WINAPI WMReaderAdvanced_GetStreamSelected(IWMReaderAdvanced6 *iface,
        WORD stream_number, WMT_STREAM_SELECTION *selection)
{
    struct async_reader *reader = impl_from_IWMReaderAdvanced6(iface);
    return IWMSyncReader2_GetStreamSelected(reader->reader, stream_number, selection);
}

static HRESULT WINAPI WMReaderAdvanced_SetReceiveSelectionCallbacks(IWMReaderAdvanced6 *iface, BOOL get_callbacks)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced_GetReceiveSelectionCallbacks(IWMReaderAdvanced6 *iface, BOOL *get_callbacks)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced_SetReceiveStreamSamples(IWMReaderAdvanced6 *iface,
        WORD stream_number, BOOL compressed)
{
    struct async_reader *reader = impl_from_IWMReaderAdvanced6(iface);
    return IWMSyncReader2_SetReadStreamSamples(reader->reader, stream_number, compressed);
}

static HRESULT WINAPI WMReaderAdvanced_GetReceiveStreamSamples(IWMReaderAdvanced6 *iface, WORD stream_num,
        BOOL *receive_stream_samples)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced_SetAllocateForOutput(IWMReaderAdvanced6 *iface, DWORD output, BOOL allocate)
{
    struct async_reader *reader = impl_from_IWMReaderAdvanced6(iface);
    reader->allocate_for_output[output] = allocate;
    return S_OK;
}

static HRESULT WINAPI WMReaderAdvanced_GetAllocateForOutput(IWMReaderAdvanced6 *iface, DWORD output, BOOL *allocate)
{
    struct async_reader *reader = impl_from_IWMReaderAdvanced6(iface);
    *allocate = reader->allocate_for_output[output];
    return S_OK;
}

static HRESULT WINAPI WMReaderAdvanced_SetAllocateForStream(IWMReaderAdvanced6 *iface, WORD stream_number, BOOL allocate)
{
    struct async_reader *reader = impl_from_IWMReaderAdvanced6(iface);
    reader->allocate_for_stream[stream_number - 1] = allocate;
    return S_OK;
}

static HRESULT WINAPI WMReaderAdvanced_GetAllocateForStream(IWMReaderAdvanced6 *iface, WORD stream_number, BOOL *allocate)
{
    struct async_reader *reader = impl_from_IWMReaderAdvanced6(iface);
    *allocate = reader->allocate_for_stream[stream_number - 1];
    return S_OK;
}

static HRESULT WINAPI WMReaderAdvanced_GetStatistics(IWMReaderAdvanced6 *iface, WM_READER_STATISTICS *statistics)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced_SetClientInfo(IWMReaderAdvanced6 *iface, WM_READER_CLIENTINFO *client_info)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced_GetMaxOutputSampleSize(IWMReaderAdvanced6 *iface, DWORD output, DWORD *max)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced_GetMaxStreamSampleSize(IWMReaderAdvanced6 *iface,
        WORD stream_number, DWORD *size)
{
    struct async_reader *reader = impl_from_IWMReaderAdvanced6(iface);
    return IWMSyncReader2_GetMaxStreamSampleSize(reader->reader, stream_number, size);
}

static HRESULT WINAPI WMReaderAdvanced_NotifyLateDelivery(IWMReaderAdvanced6 *iface, QWORD lateness)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced2_SetPlayMode(IWMReaderAdvanced6 *iface, WMT_PLAY_MODE mode)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced2_GetPlayMode(IWMReaderAdvanced6 *iface, WMT_PLAY_MODE *mode)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced2_GetBufferProgress(IWMReaderAdvanced6 *iface, DWORD *percent, QWORD *buffering)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced2_GetDownloadProgress(IWMReaderAdvanced6 *iface, DWORD *percent,
        QWORD *bytes_downloaded, QWORD *download)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced2_GetSaveAsProgress(IWMReaderAdvanced6 *iface, DWORD *percent)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced2_SaveFileAs(IWMReaderAdvanced6 *iface, const WCHAR *filename)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced2_GetProtocolName(IWMReaderAdvanced6 *iface, WCHAR *protocol, DWORD *protocol_len)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced2_StartAtMarker(IWMReaderAdvanced6 *iface, WORD marker_index,
        QWORD duration, float rate, void *context)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced2_GetOutputSetting(IWMReaderAdvanced6 *iface, DWORD output_num,
        const WCHAR *name, WMT_ATTR_DATATYPE *type, BYTE *value, WORD *length)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced2_SetOutputSetting(IWMReaderAdvanced6 *iface, DWORD output_num,
        const WCHAR *name, WMT_ATTR_DATATYPE type, const BYTE *value, WORD length)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced2_Preroll(IWMReaderAdvanced6 *iface, QWORD start, QWORD duration, float rate)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced2_SetLogClientID(IWMReaderAdvanced6 *iface, BOOL log_client_id)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced2_GetLogClientID(IWMReaderAdvanced6 *iface, BOOL *log_client_id)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced2_StopBuffering(IWMReaderAdvanced6 *iface)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced2_OpenStream(IWMReaderAdvanced6 *iface,
        IStream *stream, IWMReaderCallback *callback, void *context)
{
    struct async_reader *reader = impl_from_IWMReaderAdvanced6(iface);
    HRESULT hr;

    IStream_AddRef(stream);

    EnterCriticalSection(&reader->cs);

    if (reader->opened)
        hr = E_UNEXPECTED;
    else if (FAILED(hr = async_reader_open(reader, callback, context)))
        ok(0, "Failed to open stream %p, hr %#lx.\n", stream, hr);
    else if (FAILED(hr = async_reader_queue_op(reader, ASYNC_OP_OPEN_STREAM, stream)))
    {
        ok(0, "Got hr %#lx.\n", hr);
        async_reader_close(reader, true);
    }

    LeaveCriticalSection(&reader->cs);

    if (FAILED(hr))
        IStream_Release(stream);
    return hr;
}

static HRESULT WINAPI WMReaderAdvanced3_StopNetStreaming(IWMReaderAdvanced6 *iface)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced3_StartAtPosition(IWMReaderAdvanced6 *iface, WORD stream_num,
        void *offset_start, void *duration, WMT_OFFSET_FORMAT format, float rate, void *context)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced4_GetLanguageCount(IWMReaderAdvanced6 *iface, DWORD output_num, WORD *language_count)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced4_GetLanguage(IWMReaderAdvanced6 *iface, DWORD output_num,
       WORD language, WCHAR *language_string, WORD *language_string_len)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced4_GetMaxSpeedFactor(IWMReaderAdvanced6 *iface, double *factor)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced4_IsUsingFastCache(IWMReaderAdvanced6 *iface, BOOL *using_fast_cache)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced4_AddLogParam(IWMReaderAdvanced6 *iface, const WCHAR *namespace,
        const WCHAR *name, const WCHAR *value)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced4_SendLogParams(IWMReaderAdvanced6 *iface)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced4_CanSaveFileAs(IWMReaderAdvanced6 *iface, BOOL *can_save)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced4_CancelSaveFileAs(IWMReaderAdvanced6 *iface)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced4_GetURL(IWMReaderAdvanced6 *iface, WCHAR *url, DWORD *url_len)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced5_SetPlayerHook(IWMReaderAdvanced6 *iface, DWORD output_num, IWMPlayerHook *hook)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI WMReaderAdvanced6_SetProtectStreamSamples(IWMReaderAdvanced6 *iface, BYTE *cert,
        DWORD cert_size, DWORD cert_type, DWORD flags, BYTE *initialization_vector, DWORD *initialization_vector_size)
{
    return E_NOTIMPL;
}

static const IWMReaderAdvanced6Vtbl WMReaderAdvanced6Vtbl = {
    WMReaderAdvanced_QueryInterface,
    WMReaderAdvanced_AddRef,
    WMReaderAdvanced_Release,
    WMReaderAdvanced_SetUserProvidedClock,
    WMReaderAdvanced_GetUserProvidedClock,
    WMReaderAdvanced_DeliverTime,
    WMReaderAdvanced_SetManualStreamSelection,
    WMReaderAdvanced_GetManualStreamSelection,
    WMReaderAdvanced_SetStreamsSelected,
    WMReaderAdvanced_GetStreamSelected,
    WMReaderAdvanced_SetReceiveSelectionCallbacks,
    WMReaderAdvanced_GetReceiveSelectionCallbacks,
    WMReaderAdvanced_SetReceiveStreamSamples,
    WMReaderAdvanced_GetReceiveStreamSamples,
    WMReaderAdvanced_SetAllocateForOutput,
    WMReaderAdvanced_GetAllocateForOutput,
    WMReaderAdvanced_SetAllocateForStream,
    WMReaderAdvanced_GetAllocateForStream,
    WMReaderAdvanced_GetStatistics,
    WMReaderAdvanced_SetClientInfo,
    WMReaderAdvanced_GetMaxOutputSampleSize,
    WMReaderAdvanced_GetMaxStreamSampleSize,
    WMReaderAdvanced_NotifyLateDelivery,
    WMReaderAdvanced2_SetPlayMode,
    WMReaderAdvanced2_GetPlayMode,
    WMReaderAdvanced2_GetBufferProgress,
    WMReaderAdvanced2_GetDownloadProgress,
    WMReaderAdvanced2_GetSaveAsProgress,
    WMReaderAdvanced2_SaveFileAs,
    WMReaderAdvanced2_GetProtocolName,
    WMReaderAdvanced2_StartAtMarker,
    WMReaderAdvanced2_GetOutputSetting,
    WMReaderAdvanced2_SetOutputSetting,
    WMReaderAdvanced2_Preroll,
    WMReaderAdvanced2_SetLogClientID,
    WMReaderAdvanced2_GetLogClientID,
    WMReaderAdvanced2_StopBuffering,
    WMReaderAdvanced2_OpenStream,
    WMReaderAdvanced3_StopNetStreaming,
    WMReaderAdvanced3_StartAtPosition,
    WMReaderAdvanced4_GetLanguageCount,
    WMReaderAdvanced4_GetLanguage,
    WMReaderAdvanced4_GetMaxSpeedFactor,
    WMReaderAdvanced4_IsUsingFastCache,
    WMReaderAdvanced4_AddLogParam,
    WMReaderAdvanced4_SendLogParams,
    WMReaderAdvanced4_CanSaveFileAs,
    WMReaderAdvanced4_CancelSaveFileAs,
    WMReaderAdvanced4_GetURL,
    WMReaderAdvanced5_SetPlayerHook,
    WMReaderAdvanced6_SetProtectStreamSamples
};

static struct async_reader *impl_from_IWMReaderAccelerator(IWMReaderAccelerator *iface)
{
    return CONTAINING_RECORD(iface, struct async_reader, IWMReaderAccelerator_iface);
}

static HRESULT WINAPI reader_accl_QueryInterface(IWMReaderAccelerator *iface, REFIID iid, void **out)
{
    struct async_reader *reader = impl_from_IWMReaderAccelerator(iface);
    return IWMReader_QueryInterface(&reader->IWMReader_iface, iid, out);
}

static ULONG WINAPI reader_accl_AddRef(IWMReaderAccelerator *iface)
{
    struct async_reader *reader = impl_from_IWMReaderAccelerator(iface);
    return IWMReader_AddRef(&reader->IWMReader_iface);
}

static ULONG WINAPI reader_accl_Release(IWMReaderAccelerator *iface)
{
    struct async_reader *reader = impl_from_IWMReaderAccelerator(iface);
    return IWMReader_Release(&reader->IWMReader_iface);
}

static HRESULT WINAPI reader_accl_GetCodecInterface(IWMReaderAccelerator *iface, DWORD output, REFIID riid, void **codec)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI reader_accl_Notify(IWMReaderAccelerator *iface, DWORD output, WM_MEDIA_TYPE *subtype)
{
    return E_NOTIMPL;
}

static const IWMReaderAcceleratorVtbl WMReaderAcceleratorVtbl = {
    reader_accl_QueryInterface,
    reader_accl_AddRef,
    reader_accl_Release,
    reader_accl_GetCodecInterface,
    reader_accl_Notify
};

static struct async_reader *impl_from_IWMReaderNetworkConfig2(IWMReaderNetworkConfig2 *iface)
{
    return CONTAINING_RECORD(iface, struct async_reader, IWMReaderNetworkConfig2_iface);
}

static HRESULT WINAPI networkconfig_QueryInterface(IWMReaderNetworkConfig2 *iface, REFIID iid, void **out)
{
    struct async_reader *reader = impl_from_IWMReaderNetworkConfig2(iface);
    return IWMReader_QueryInterface(&reader->IWMReader_iface, iid, out);
}

static ULONG WINAPI networkconfig_AddRef(IWMReaderNetworkConfig2 *iface)
{
    struct async_reader *reader = impl_from_IWMReaderNetworkConfig2(iface);
    return IWMReader_AddRef(&reader->IWMReader_iface);
}

static ULONG WINAPI networkconfig_Release(IWMReaderNetworkConfig2 *iface)
{
    struct async_reader *reader = impl_from_IWMReaderNetworkConfig2(iface);
    return IWMReader_Release(&reader->IWMReader_iface);
}

static HRESULT WINAPI networkconfig_GetBufferingTime(IWMReaderNetworkConfig2 *iface, QWORD *buffering_time)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_SetBufferingTime(IWMReaderNetworkConfig2 *iface, QWORD buffering_time)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_GetUDPPortRanges(IWMReaderNetworkConfig2 *iface, WM_PORT_NUMBER_RANGE *array,
        DWORD *ranges)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_SetUDPPortRanges(IWMReaderNetworkConfig2 *iface,
        WM_PORT_NUMBER_RANGE *ranges, DWORD count)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_GetProxySettings(IWMReaderNetworkConfig2 *iface, const WCHAR *protocol,
        WMT_PROXY_SETTINGS *proxy)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_SetProxySettings(IWMReaderNetworkConfig2 *iface, LPCWSTR protocol,
        WMT_PROXY_SETTINGS proxy)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_GetProxyHostName(IWMReaderNetworkConfig2 *iface, const WCHAR *protocol,
        WCHAR *hostname, DWORD *size)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_SetProxyHostName(IWMReaderNetworkConfig2 *iface, const WCHAR *protocol,
        const WCHAR *hostname)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_GetProxyPort(IWMReaderNetworkConfig2 *iface, const WCHAR *protocol,
        DWORD *port)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_SetProxyPort(IWMReaderNetworkConfig2 *iface, const WCHAR *protocol,
        DWORD port)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_GetProxyExceptionList(IWMReaderNetworkConfig2 *iface, const WCHAR *protocol,
        WCHAR *exceptions, DWORD *count)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_SetProxyExceptionList(IWMReaderNetworkConfig2 *iface, const WCHAR *protocol,
        const WCHAR *exceptions)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_GetProxyBypassForLocal(IWMReaderNetworkConfig2 *iface, const WCHAR *protocol,
        BOOL *bypass)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_SetProxyBypassForLocal(IWMReaderNetworkConfig2 *iface, const WCHAR *protocol,
        BOOL bypass)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_GetForceRerunAutoProxyDetection(IWMReaderNetworkConfig2 *iface,
        BOOL *detection)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_SetForceRerunAutoProxyDetection(IWMReaderNetworkConfig2 *iface,
        BOOL detection)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_GetEnableMulticast(IWMReaderNetworkConfig2 *iface, BOOL *multicast)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_SetEnableMulticast(IWMReaderNetworkConfig2 *iface, BOOL multicast)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_GetEnableHTTP(IWMReaderNetworkConfig2 *iface, BOOL *enable)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_SetEnableHTTP(IWMReaderNetworkConfig2 *iface, BOOL enable)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_GetEnableUDP(IWMReaderNetworkConfig2 *iface, BOOL *enable)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_SetEnableUDP(IWMReaderNetworkConfig2 *iface, BOOL enable)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_GetEnableTCP(IWMReaderNetworkConfig2 *iface, BOOL *enable)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_SetEnableTCP(IWMReaderNetworkConfig2 *iface, BOOL enable)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_ResetProtocolRollover(IWMReaderNetworkConfig2 *iface)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_GetConnectionBandwidth(IWMReaderNetworkConfig2 *iface, DWORD *bandwidth)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_SetConnectionBandwidth(IWMReaderNetworkConfig2 *iface, DWORD bandwidth)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_GetNumProtocolsSupported(IWMReaderNetworkConfig2 *iface, DWORD *protocols)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_GetSupportedProtocolName(IWMReaderNetworkConfig2 *iface, DWORD protocol_num,
        WCHAR *protocol, DWORD *size)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_AddLoggingUrl(IWMReaderNetworkConfig2 *iface, const WCHAR *url)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_GetLoggingUrl(IWMReaderNetworkConfig2 *iface, DWORD index, WCHAR *url,
        DWORD *size)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_GetLoggingUrlCount(IWMReaderNetworkConfig2 *iface, DWORD *count)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_ResetLoggingUrlList(IWMReaderNetworkConfig2 *iface)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_GetEnableContentCaching(IWMReaderNetworkConfig2 *iface, BOOL *enable)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_SetEnableContentCaching(IWMReaderNetworkConfig2 *iface, BOOL enable)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_GetEnableFastCache(IWMReaderNetworkConfig2 *iface, BOOL *enable)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_SetEnableFastCache(IWMReaderNetworkConfig2 *iface, BOOL enable)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_GetAcceleratedStreamingDuration(IWMReaderNetworkConfig2 *iface,
        QWORD *duration)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_SetAcceleratedStreamingDuration(IWMReaderNetworkConfig2 *iface,
        QWORD duration)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_GetAutoReconnectLimit(IWMReaderNetworkConfig2 *iface, DWORD *limit)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_SetAutoReconnectLimit(IWMReaderNetworkConfig2 *iface, DWORD limit)
{

    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_GetEnableResends(IWMReaderNetworkConfig2 *iface, BOOL *enable)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_SetEnableResends(IWMReaderNetworkConfig2 *iface, BOOL enable)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_GetEnableThinning(IWMReaderNetworkConfig2 *iface, BOOL *enable)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_SetEnableThinning(IWMReaderNetworkConfig2 *iface, BOOL enable)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI networkconfig_GetMaxNetPacketSize(IWMReaderNetworkConfig2 *iface, DWORD *packet_size)
{
    return E_NOTIMPL;
}

static const IWMReaderNetworkConfig2Vtbl WMReaderNetworkConfig2Vtbl =
{
    networkconfig_QueryInterface,
    networkconfig_AddRef,
    networkconfig_Release,
    networkconfig_GetBufferingTime,
    networkconfig_SetBufferingTime,
    networkconfig_GetUDPPortRanges,
    networkconfig_SetUDPPortRanges,
    networkconfig_GetProxySettings,
    networkconfig_SetProxySettings,
    networkconfig_GetProxyHostName,
    networkconfig_SetProxyHostName,
    networkconfig_GetProxyPort,
    networkconfig_SetProxyPort,
    networkconfig_GetProxyExceptionList,
    networkconfig_SetProxyExceptionList,
    networkconfig_GetProxyBypassForLocal,
    networkconfig_SetProxyBypassForLocal,
    networkconfig_GetForceRerunAutoProxyDetection,
    networkconfig_SetForceRerunAutoProxyDetection,
    networkconfig_GetEnableMulticast,
    networkconfig_SetEnableMulticast,
    networkconfig_GetEnableHTTP,
    networkconfig_SetEnableHTTP,
    networkconfig_GetEnableUDP,
    networkconfig_SetEnableUDP,
    networkconfig_GetEnableTCP,
    networkconfig_SetEnableTCP,
    networkconfig_ResetProtocolRollover,
    networkconfig_GetConnectionBandwidth,
    networkconfig_SetConnectionBandwidth,
    networkconfig_GetNumProtocolsSupported,
    networkconfig_GetSupportedProtocolName,
    networkconfig_AddLoggingUrl,
    networkconfig_GetLoggingUrl,
    networkconfig_GetLoggingUrlCount,
    networkconfig_ResetLoggingUrlList,
    networkconfig_GetEnableContentCaching,
    networkconfig_SetEnableContentCaching,
    networkconfig_GetEnableFastCache,
    networkconfig_SetEnableFastCache,
    networkconfig_GetAcceleratedStreamingDuration,
    networkconfig_SetAcceleratedStreamingDuration,
    networkconfig_GetAutoReconnectLimit,
    networkconfig_SetAutoReconnectLimit,
    networkconfig_GetEnableResends,
    networkconfig_SetEnableResends,
    networkconfig_GetEnableThinning,
    networkconfig_SetEnableThinning,
    networkconfig_GetMaxNetPacketSize
};

static struct async_reader *impl_from_IWMReaderStreamClock(IWMReaderStreamClock *iface)
{
    return CONTAINING_RECORD(iface, struct async_reader, IWMReaderStreamClock_iface);
}

static HRESULT WINAPI readclock_QueryInterface(IWMReaderStreamClock *iface, REFIID iid, void **out)
{
    struct async_reader *reader = impl_from_IWMReaderStreamClock(iface);
    return IWMReader_QueryInterface(&reader->IWMReader_iface, iid, out);
}

static ULONG WINAPI readclock_AddRef(IWMReaderStreamClock *iface)
{
    struct async_reader *reader = impl_from_IWMReaderStreamClock(iface);
    return IWMReader_AddRef(&reader->IWMReader_iface);
}

static ULONG WINAPI readclock_Release(IWMReaderStreamClock *iface)
{
    struct async_reader *reader = impl_from_IWMReaderStreamClock(iface);
    return IWMReader_Release(&reader->IWMReader_iface);
}

static HRESULT WINAPI readclock_GetTime(IWMReaderStreamClock *iface, QWORD *now)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI readclock_SetTimer(IWMReaderStreamClock *iface, QWORD when, void *param, DWORD *id)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI readclock_KillTimer(IWMReaderStreamClock *iface, DWORD id)
{
    return E_NOTIMPL;
}

static const IWMReaderStreamClockVtbl WMReaderStreamClockVtbl =
{
    readclock_QueryInterface,
    readclock_AddRef,
    readclock_Release,
    readclock_GetTime,
    readclock_SetTimer,
    readclock_KillTimer
};

static struct async_reader *impl_from_IWMReaderTypeNegotiation(IWMReaderTypeNegotiation *iface)
{
    return CONTAINING_RECORD(iface, struct async_reader, IWMReaderTypeNegotiation_iface);
}

static HRESULT WINAPI negotiation_QueryInterface(IWMReaderTypeNegotiation *iface, REFIID iid, void **out)
{
    struct async_reader *reader = impl_from_IWMReaderTypeNegotiation(iface);
    return IWMReader_QueryInterface(&reader->IWMReader_iface, iid, out);
}

static ULONG WINAPI negotiation_AddRef(IWMReaderTypeNegotiation *iface)
{
    struct async_reader *reader = impl_from_IWMReaderTypeNegotiation(iface);
    return IWMReader_AddRef(&reader->IWMReader_iface);
}

static ULONG WINAPI negotiation_Release(IWMReaderTypeNegotiation *iface)
{
    struct async_reader *reader = impl_from_IWMReaderTypeNegotiation(iface);
    return IWMReader_Release(&reader->IWMReader_iface);
}

static HRESULT WINAPI negotiation_TryOutputProps(IWMReaderTypeNegotiation *iface, DWORD output, IWMOutputMediaProps *props)
{
    return E_NOTIMPL;
}

static const IWMReaderTypeNegotiationVtbl WMReaderTypeNegotiationVtbl =
{
    negotiation_QueryInterface,
    negotiation_AddRef,
    negotiation_Release,
    negotiation_TryOutputProps
};

static struct async_reader *impl_from_IWMProfile3(IWMProfile3 *iface)
{
    return CONTAINING_RECORD(iface, struct async_reader, IWMProfile3_iface);
}

static HRESULT WINAPI profile_QueryInterface(IWMProfile3 *iface, REFIID iid, void **out)
{
    struct async_reader *reader = impl_from_IWMProfile3(iface);
    return IWMReader_QueryInterface(&reader->IWMReader_iface, iid, out);
}

static ULONG WINAPI profile_AddRef(IWMProfile3 *iface)
{
    struct async_reader *reader = impl_from_IWMProfile3(iface);
    return IWMReader_AddRef(&reader->IWMReader_iface);
}

static ULONG WINAPI profile_Release(IWMProfile3 *iface)
{
    struct async_reader *reader = impl_from_IWMProfile3(iface);
    return IWMReader_Release(&reader->IWMReader_iface);
}

static HRESULT WINAPI profile_GetVersion(IWMProfile3 *iface, WMT_VERSION *version)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_GetName(IWMProfile3 *iface, WCHAR *name, DWORD *length)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_SetName(IWMProfile3 *iface, const WCHAR *name)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_GetDescription(IWMProfile3 *iface, WCHAR *description, DWORD *length)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_SetDescription(IWMProfile3 *iface, const WCHAR *description)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_GetStreamCount(IWMProfile3 *iface, DWORD *count)
{
    struct async_reader *reader = impl_from_IWMProfile3(iface);
    return IWMProfile3_GetStreamCount(reader->profile, count);
}

struct stream_config
{
    IWMStreamConfig IWMStreamConfig_iface;
    IWMMediaProps IWMMediaProps_iface;
    LONG refcount;

    IWMStreamConfig *config;
    IWMMediaProps *props;
    IWMReader *reader;
};

static struct stream_config *impl_from_IWMStreamConfig(IWMStreamConfig *iface)
{
    return CONTAINING_RECORD(iface, struct stream_config, IWMStreamConfig_iface);
}

static HRESULT WINAPI stream_config_QueryInterface(IWMStreamConfig *iface, REFIID iid, void **out)
{
    struct stream_config *config = impl_from_IWMStreamConfig(iface);

    if (IsEqualGUID(iid, &IID_IUnknown)
            || IsEqualGUID(iid, &IID_IWMStreamConfig))
        *out = &config->IWMStreamConfig_iface;
    else if (IsEqualGUID(iid, &IID_IWMMediaProps))
        *out = &config->IWMMediaProps_iface;
    else
    {
        *out = NULL;
        ok(0, "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static ULONG WINAPI stream_config_AddRef(IWMStreamConfig *iface)
{
    struct stream_config *config = impl_from_IWMStreamConfig(iface);
    ULONG refcount = InterlockedIncrement(&config->refcount);
    return refcount;
}

static ULONG WINAPI stream_config_Release(IWMStreamConfig *iface)
{
    struct stream_config *config = impl_from_IWMStreamConfig(iface);
    ULONG refcount = InterlockedDecrement(&config->refcount);

    if (!refcount)
    {
        IWMReader_Release(config->reader);
        IWMMediaProps_Release(config->props);
        IWMStreamConfig_Release(config->config);
        free(config);
    }

    return refcount;
}

static HRESULT WINAPI stream_config_GetStreamType(IWMStreamConfig *iface, GUID *type)
{
    struct stream_config *config = impl_from_IWMStreamConfig(iface);
    return IWMStreamConfig_GetStreamType(config->config, type);
}

static HRESULT WINAPI stream_config_GetStreamNumber(IWMStreamConfig *iface, WORD *number)
{
    struct stream_config *config = impl_from_IWMStreamConfig(iface);
    return IWMStreamConfig_GetStreamNumber(config->config, number);
}

static HRESULT WINAPI stream_config_SetStreamNumber(IWMStreamConfig *iface, WORD number)
{
    struct stream_config *config = impl_from_IWMStreamConfig(iface);
    return IWMStreamConfig_SetStreamNumber(config->config, number);
}

static HRESULT WINAPI stream_config_GetStreamName(IWMStreamConfig *iface, WCHAR *name, WORD *len)
{
    struct stream_config *config = impl_from_IWMStreamConfig(iface);
    return IWMStreamConfig_GetStreamName(config->config, name, len);
}

static HRESULT WINAPI stream_config_SetStreamName(IWMStreamConfig *iface, const WCHAR *name)
{
    struct stream_config *config = impl_from_IWMStreamConfig(iface);
    return IWMStreamConfig_SetStreamName(config->config, name);
}

static HRESULT WINAPI stream_config_GetConnectionName(IWMStreamConfig *iface, WCHAR *name, WORD *len)
{
    struct stream_config *config = impl_from_IWMStreamConfig(iface);
    return IWMStreamConfig_GetConnectionName(config->config, name, len);
}

static HRESULT WINAPI stream_config_SetConnectionName(IWMStreamConfig *iface, const WCHAR *name)
{
    struct stream_config *config = impl_from_IWMStreamConfig(iface);
    return IWMStreamConfig_SetConnectionName(config->config, name);
}

static HRESULT WINAPI stream_config_GetBitrate(IWMStreamConfig *iface, DWORD *bitrate)
{
    struct stream_config *config = impl_from_IWMStreamConfig(iface);
    return IWMStreamConfig_GetBitrate(config->config, bitrate);
}

static HRESULT WINAPI stream_config_SetBitrate(IWMStreamConfig *iface, DWORD bitrate)
{
    struct stream_config *config = impl_from_IWMStreamConfig(iface);
    return IWMStreamConfig_SetBitrate(config->config, bitrate);
}

static HRESULT WINAPI stream_config_GetBufferWindow(IWMStreamConfig *iface, DWORD *window)
{
    struct stream_config *config = impl_from_IWMStreamConfig(iface);
    return IWMStreamConfig_GetBufferWindow(config->config, window);
}

static HRESULT WINAPI stream_config_SetBufferWindow(IWMStreamConfig *iface, DWORD window)
{
    struct stream_config *config = impl_from_IWMStreamConfig(iface);
    return IWMStreamConfig_SetBufferWindow(config->config, window);
}

static const IWMStreamConfigVtbl stream_config_vtbl =
{
    stream_config_QueryInterface,
    stream_config_AddRef,
    stream_config_Release,
    stream_config_GetStreamType,
    stream_config_GetStreamNumber,
    stream_config_SetStreamNumber,
    stream_config_GetStreamName,
    stream_config_SetStreamName,
    stream_config_GetConnectionName,
    stream_config_SetConnectionName,
    stream_config_GetBitrate,
    stream_config_SetBitrate,
    stream_config_GetBufferWindow,
    stream_config_SetBufferWindow,
};

static struct stream_config *impl_from_IWMMediaProps(IWMMediaProps *iface)
{
    return CONTAINING_RECORD(iface, struct stream_config, IWMMediaProps_iface);
}

static HRESULT WINAPI stream_props_QueryInterface(IWMMediaProps *iface, REFIID iid, void **out)
{
    struct stream_config *config = impl_from_IWMMediaProps(iface);
    return IWMStreamConfig_QueryInterface(&config->IWMStreamConfig_iface, iid, out);
}

static ULONG WINAPI stream_props_AddRef(IWMMediaProps *iface)
{
    struct stream_config *config = impl_from_IWMMediaProps(iface);
    return IWMStreamConfig_AddRef(&config->IWMStreamConfig_iface);
}

static ULONG WINAPI stream_props_Release(IWMMediaProps *iface)
{
    struct stream_config *config = impl_from_IWMMediaProps(iface);
    return IWMStreamConfig_Release(&config->IWMStreamConfig_iface);
}

static HRESULT WINAPI stream_props_GetType(IWMMediaProps *iface, GUID *major_type)
{
    struct stream_config *config = impl_from_IWMMediaProps(iface);
    return IWMMediaProps_GetType(config->props, major_type);
}

static HRESULT WINAPI stream_props_GetMediaType(IWMMediaProps *iface, WM_MEDIA_TYPE *mt, DWORD *size)
{
    struct stream_config *config = impl_from_IWMMediaProps(iface);
    return IWMMediaProps_GetMediaType(config->props, mt, size);
}

static HRESULT WINAPI stream_props_SetMediaType(IWMMediaProps *iface, WM_MEDIA_TYPE *mt)
{
    struct stream_config *config = impl_from_IWMMediaProps(iface);
    return IWMMediaProps_SetMediaType(config->props, mt);
}

static const IWMMediaPropsVtbl stream_props_vtbl =
{
    stream_props_QueryInterface,
    stream_props_AddRef,
    stream_props_Release,
    stream_props_GetType,
    stream_props_GetMediaType,
    stream_props_SetMediaType,
};

static HRESULT WINAPI profile_GetStream(IWMProfile3 *iface, DWORD index, IWMStreamConfig **config)
{
    struct async_reader *reader = impl_from_IWMProfile3(iface);
    struct stream_config *object;
    HRESULT hr;

    if (!(object = calloc(1, sizeof(*object))))
        return E_OUTOFMEMORY;

    object->IWMStreamConfig_iface.lpVtbl = &stream_config_vtbl;
    object->IWMMediaProps_iface.lpVtbl = &stream_props_vtbl;
    object->refcount = 1;

    if (FAILED(hr = IWMProfile3_GetStream(reader->profile, index, &object->config)))
    {
        ok(0, "Got hr %#lx.\n", hr);
        free(object);
        return hr;
    }
    if (FAILED(hr = IWMStreamConfig_QueryInterface(object->config, &IID_IWMMediaProps, (void **)&object->props)))
    {
        ok(0, "Got hr %#lx.\n", hr);
        IWMStreamConfig_Release(object->config);
        free(object);
        return hr;
    }
    IWMReader_AddRef((object->reader = &reader->IWMReader_iface));

    *config = &object->IWMStreamConfig_iface;
    return S_OK;
}

static HRESULT WINAPI profile_GetStreamByNumber(IWMProfile3 *iface, WORD stream_number, IWMStreamConfig **config)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_RemoveStream(IWMProfile3 *iface, IWMStreamConfig *config)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_RemoveStreamByNumber(IWMProfile3 *iface, WORD stream_number)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_AddStream(IWMProfile3 *iface, IWMStreamConfig *config)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_ReconfigStream(IWMProfile3 *iface, IWMStreamConfig *config)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_CreateNewStream(IWMProfile3 *iface, REFGUID type, IWMStreamConfig **config)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_GetMutualExclusionCount(IWMProfile3 *iface, DWORD *count)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_GetMutualExclusion(IWMProfile3 *iface, DWORD index, IWMMutualExclusion **excl)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_RemoveMutualExclusion(IWMProfile3 *iface, IWMMutualExclusion *excl)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_AddMutualExclusion(IWMProfile3 *iface, IWMMutualExclusion *excl)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_CreateNewMutualExclusion(IWMProfile3 *iface, IWMMutualExclusion **excl)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_GetProfileID(IWMProfile3 *iface, GUID *id)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_GetStorageFormat(IWMProfile3 *iface, WMT_STORAGE_FORMAT *format)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_SetStorageFormat(IWMProfile3 *iface, WMT_STORAGE_FORMAT format)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_GetBandwidthSharingCount(IWMProfile3 *iface, DWORD *count)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_GetBandwidthSharing(IWMProfile3 *iface, DWORD index, IWMBandwidthSharing **sharing)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_RemoveBandwidthSharing( IWMProfile3 *iface, IWMBandwidthSharing *sharing)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_AddBandwidthSharing(IWMProfile3 *iface, IWMBandwidthSharing *sharing)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_CreateNewBandwidthSharing( IWMProfile3 *iface, IWMBandwidthSharing **sharing)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_GetStreamPrioritization(IWMProfile3 *iface, IWMStreamPrioritization **stream)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_SetStreamPrioritization(IWMProfile3 *iface, IWMStreamPrioritization *stream)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_RemoveStreamPrioritization(IWMProfile3 *iface)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_CreateNewStreamPrioritization(IWMProfile3 *iface, IWMStreamPrioritization **stream)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI profile_GetExpectedPacketCount(IWMProfile3 *iface, QWORD duration, QWORD *count)
{
    return E_NOTIMPL;
}

static const IWMProfile3Vtbl profile_vtbl =
{
    profile_QueryInterface,
    profile_AddRef,
    profile_Release,
    profile_GetVersion,
    profile_GetName,
    profile_SetName,
    profile_GetDescription,
    profile_SetDescription,
    profile_GetStreamCount,
    profile_GetStream,
    profile_GetStreamByNumber,
    profile_RemoveStream,
    profile_RemoveStreamByNumber,
    profile_AddStream,
    profile_ReconfigStream,
    profile_CreateNewStream,
    profile_GetMutualExclusionCount,
    profile_GetMutualExclusion,
    profile_RemoveMutualExclusion,
    profile_AddMutualExclusion,
    profile_CreateNewMutualExclusion,
    profile_GetProfileID,
    profile_GetStorageFormat,
    profile_SetStorageFormat,
    profile_GetBandwidthSharingCount,
    profile_GetBandwidthSharing,
    profile_RemoveBandwidthSharing,
    profile_AddBandwidthSharing,
    profile_CreateNewBandwidthSharing,
    profile_GetStreamPrioritization,
    profile_SetStreamPrioritization,
    profile_RemoveStreamPrioritization,
    profile_CreateNewStreamPrioritization,
    profile_GetExpectedPacketCount,
};

static struct async_reader *impl_from_IWMHeaderInfo3(IWMHeaderInfo3 *iface)
{
    return CONTAINING_RECORD(iface, struct async_reader, IWMHeaderInfo3_iface);
}

static HRESULT WINAPI header_info_QueryInterface(IWMHeaderInfo3 *iface, REFIID iid, void **out)
{
    struct async_reader *reader = impl_from_IWMHeaderInfo3(iface);
    return IWMReader_QueryInterface(&reader->IWMReader_iface, iid, out);
}

static ULONG WINAPI header_info_AddRef(IWMHeaderInfo3 *iface)
{
    struct async_reader *reader = impl_from_IWMHeaderInfo3(iface);
    return IWMReader_AddRef(&reader->IWMReader_iface);
}

static ULONG WINAPI header_info_Release(IWMHeaderInfo3 *iface)
{
    struct async_reader *reader = impl_from_IWMHeaderInfo3(iface);
    return IWMReader_Release(&reader->IWMReader_iface);
}

static HRESULT WINAPI header_info_GetAttributeCount(IWMHeaderInfo3 *iface, WORD stream_number, WORD *count)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_GetAttributeByIndex(IWMHeaderInfo3 *iface, WORD index, WORD *stream_number,
        WCHAR *name, WORD *name_len, WMT_ATTR_DATATYPE *type, BYTE *value, WORD *size)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_GetAttributeByName(IWMHeaderInfo3 *iface, WORD *stream_number,
        const WCHAR *name, WMT_ATTR_DATATYPE *type, BYTE *value, WORD *size)
{
    struct async_reader *reader = impl_from_IWMHeaderInfo3(iface);
    return IWMHeaderInfo3_GetAttributeByName(reader->header_info, stream_number, name, type, value, size);
}

static HRESULT WINAPI header_info_SetAttribute(IWMHeaderInfo3 *iface, WORD stream_number,
        const WCHAR *name, WMT_ATTR_DATATYPE type, const BYTE *value, WORD size)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_GetMarkerCount(IWMHeaderInfo3 *iface, WORD *count)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_GetMarker(IWMHeaderInfo3 *iface,
        WORD index, WCHAR *name, WORD *len, QWORD *time)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_AddMarker(IWMHeaderInfo3 *iface, const WCHAR *name, QWORD time)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_RemoveMarker(IWMHeaderInfo3 *iface, WORD index)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_GetScriptCount(IWMHeaderInfo3 *iface, WORD *count)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_GetScript(IWMHeaderInfo3 *iface, WORD index, WCHAR *type,
        WORD *type_len, WCHAR *command, WORD *command_len, QWORD *time)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_AddScript(IWMHeaderInfo3 *iface,
        const WCHAR *type, const WCHAR *command, QWORD time)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_RemoveScript(IWMHeaderInfo3 *iface, WORD index)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_GetCodecInfoCount(IWMHeaderInfo3 *iface, DWORD *count)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_GetCodecInfo(IWMHeaderInfo3 *iface, DWORD index, WORD *name_len,
        WCHAR *name, WORD *desc_len, WCHAR *desc, WMT_CODEC_INFO_TYPE *type, WORD *size, BYTE *info)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_GetAttributeCountEx(IWMHeaderInfo3 *iface, WORD stream_number, WORD *count)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_GetAttributeIndices(IWMHeaderInfo3 *iface, WORD stream_number,
        const WCHAR *name, WORD *lang_index, WORD *indices, WORD *count)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_GetAttributeByIndexEx(IWMHeaderInfo3 *iface,
        WORD stream_number, WORD index, WCHAR *name, WORD *name_len,
        WMT_ATTR_DATATYPE *type, WORD *lang_index, BYTE *value, DWORD *size)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_ModifyAttribute(IWMHeaderInfo3 *iface, WORD stream_number,
        WORD index, WMT_ATTR_DATATYPE type, WORD lang_index, const BYTE *value, DWORD size)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_AddAttribute(IWMHeaderInfo3 *iface,
        WORD stream_number, const WCHAR *name, WORD *index,
        WMT_ATTR_DATATYPE type, WORD lang_index, const BYTE *value, DWORD size)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_DeleteAttribute(IWMHeaderInfo3 *iface, WORD stream_number, WORD index)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI header_info_AddCodecInfo(IWMHeaderInfo3 *iface, const WCHAR *name,
        const WCHAR *desc, WMT_CODEC_INFO_TYPE type, WORD size, BYTE *info)
{
    return E_NOTIMPL;
}

static const IWMHeaderInfo3Vtbl header_info_vtbl =
{
    header_info_QueryInterface,
    header_info_AddRef,
    header_info_Release,
    header_info_GetAttributeCount,
    header_info_GetAttributeByIndex,
    header_info_GetAttributeByName,
    header_info_SetAttribute,
    header_info_GetMarkerCount,
    header_info_GetMarker,
    header_info_AddMarker,
    header_info_RemoveMarker,
    header_info_GetScriptCount,
    header_info_GetScript,
    header_info_AddScript,
    header_info_RemoveScript,
    header_info_GetCodecInfoCount,
    header_info_GetCodecInfo,
    header_info_GetAttributeCountEx,
    header_info_GetAttributeIndices,
    header_info_GetAttributeByIndexEx,
    header_info_ModifyAttribute,
    header_info_AddAttribute,
    header_info_DeleteAttribute,
    header_info_AddCodecInfo,
};

static struct async_reader *impl_from_IWMLanguageList(IWMLanguageList *iface)
{
    return CONTAINING_RECORD(iface, struct async_reader, IWMLanguageList_iface);
}

static HRESULT WINAPI language_list_QueryInterface(IWMLanguageList *iface, REFIID iid, void **out)
{
    struct async_reader *reader = impl_from_IWMLanguageList(iface);
    return IWMReader_QueryInterface(&reader->IWMReader_iface, iid, out);
}

static ULONG WINAPI language_list_AddRef(IWMLanguageList *iface)
{
    struct async_reader *reader = impl_from_IWMLanguageList(iface);
    return IWMReader_AddRef(&reader->IWMReader_iface);
}

static ULONG WINAPI language_list_Release(IWMLanguageList *iface)
{
    struct async_reader *reader = impl_from_IWMLanguageList(iface);
    return IWMReader_Release(&reader->IWMReader_iface);
}

static HRESULT WINAPI language_list_GetLanguageCount(IWMLanguageList *iface, WORD *count)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI language_list_GetLanguageDetails(IWMLanguageList *iface,
        WORD index, WCHAR *lang, WORD *len)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI language_list_AddLanguageByRFC1766String(IWMLanguageList *iface,
        const WCHAR *lang, WORD *index)
{
    return E_NOTIMPL;
}

static const IWMLanguageListVtbl language_list_vtbl =
{
    language_list_QueryInterface,
    language_list_AddRef,
    language_list_Release,
    language_list_GetLanguageCount,
    language_list_GetLanguageDetails,
    language_list_AddLanguageByRFC1766String,
};

static struct async_reader *impl_from_IWMPacketSize2(IWMPacketSize2 *iface)
{
    return CONTAINING_RECORD(iface, struct async_reader, IWMPacketSize2_iface);
}

static HRESULT WINAPI packet_size_QueryInterface(IWMPacketSize2 *iface, REFIID iid, void **out)
{
    struct async_reader *reader = impl_from_IWMPacketSize2(iface);
    return IWMReader_QueryInterface(&reader->IWMReader_iface, iid, out);
}

static ULONG WINAPI packet_size_AddRef(IWMPacketSize2 *iface)
{
    struct async_reader *reader = impl_from_IWMPacketSize2(iface);
    return IWMReader_AddRef(&reader->IWMReader_iface);
}

static ULONG WINAPI packet_size_Release(IWMPacketSize2 *iface)
{
    struct async_reader *reader = impl_from_IWMPacketSize2(iface);
    return IWMReader_Release(&reader->IWMReader_iface);
}

static HRESULT WINAPI packet_size_GetMaxPacketSize(IWMPacketSize2 *iface, DWORD *size)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI packet_size_SetMaxPacketSize(IWMPacketSize2 *iface, DWORD size)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI packet_size_GetMinPacketSize(IWMPacketSize2 *iface, DWORD *size)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI packet_size_SetMinPacketSize(IWMPacketSize2 *iface, DWORD size)
{
    return E_NOTIMPL;
}

static const IWMPacketSize2Vtbl packet_size_vtbl =
{
    packet_size_QueryInterface,
    packet_size_AddRef,
    packet_size_Release,
    packet_size_GetMaxPacketSize,
    packet_size_SetMaxPacketSize,
    packet_size_GetMinPacketSize,
    packet_size_SetMinPacketSize,
};

static struct async_reader *impl_from_IWMReaderPlaylistBurn(IWMReaderPlaylistBurn *iface)
{
    return CONTAINING_RECORD(iface, struct async_reader, IWMReaderPlaylistBurn_iface);
}

static HRESULT WINAPI playlist_QueryInterface(IWMReaderPlaylistBurn *iface, REFIID iid, void **out)
{
    struct async_reader *reader = impl_from_IWMReaderPlaylistBurn(iface);
    return IWMReader_QueryInterface(&reader->IWMReader_iface, iid, out);
}

static ULONG WINAPI playlist_AddRef(IWMReaderPlaylistBurn *iface)
{
    struct async_reader *reader = impl_from_IWMReaderPlaylistBurn(iface);
    return IWMReader_AddRef(&reader->IWMReader_iface);
}

static ULONG WINAPI playlist_Release(IWMReaderPlaylistBurn *iface)
{
    struct async_reader *reader = impl_from_IWMReaderPlaylistBurn(iface);
    return IWMReader_Release(&reader->IWMReader_iface);
}

static HRESULT WINAPI playlist_InitPlaylistBurn(IWMReaderPlaylistBurn *iface, DWORD count,
        const WCHAR **filenames, IWMStatusCallback *callback, void *context)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI playlist_GetInitResults(IWMReaderPlaylistBurn *iface, DWORD count, HRESULT *hrs)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI playlist_Cancel(IWMReaderPlaylistBurn *iface)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI playlist_EndPlaylistBurn(IWMReaderPlaylistBurn *iface, HRESULT hr)
{
    return E_NOTIMPL;
}

static const IWMReaderPlaylistBurnVtbl playlist_vtbl =
{
    playlist_QueryInterface,
    playlist_AddRef,
    playlist_Release,
    playlist_InitPlaylistBurn,
    playlist_GetInitResults,
    playlist_Cancel,
    playlist_EndPlaylistBurn,
};

static struct async_reader *impl_from_IWMReaderTimecode(IWMReaderTimecode *iface)
{
    return CONTAINING_RECORD(iface, struct async_reader, IWMReaderTimecode_iface);
}

static HRESULT WINAPI timecode_QueryInterface(IWMReaderTimecode *iface, REFIID iid, void **out)
{
    struct async_reader *reader = impl_from_IWMReaderTimecode(iface);
    return IWMReader_QueryInterface(&reader->IWMReader_iface, iid, out);
}

static ULONG WINAPI timecode_AddRef(IWMReaderTimecode *iface)
{
    struct async_reader *reader = impl_from_IWMReaderTimecode(iface);
    return IWMReader_AddRef(&reader->IWMReader_iface);
}

static ULONG WINAPI timecode_Release(IWMReaderTimecode *iface)
{
    struct async_reader *reader = impl_from_IWMReaderTimecode(iface);
    return IWMReader_Release(&reader->IWMReader_iface);
}

static HRESULT WINAPI timecode_GetTimecodeRangeCount(IWMReaderTimecode *iface,
        WORD stream_number, WORD *count)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI timecode_GetTimecodeRangeBounds(IWMReaderTimecode *iface,
        WORD stream_number, WORD index, DWORD *start, DWORD *end)
{
    return E_NOTIMPL;
}

static const IWMReaderTimecodeVtbl timecode_vtbl =
{
    timecode_QueryInterface,
    timecode_AddRef,
    timecode_Release,
    timecode_GetTimecodeRangeCount,
    timecode_GetTimecodeRangeBounds,
};

HRESULT WINAPI winegstreamer_create_wm_async_reader(IWMReader **reader)
{
    struct async_reader *object;
    HRESULT hr;

    if (!(object = calloc(1, sizeof(*object))))
        return E_OUTOFMEMORY;

    object->IWMReader_iface.lpVtbl = &WMReaderVtbl;
    object->IWMReaderAdvanced6_iface.lpVtbl = &WMReaderAdvanced6Vtbl;
    object->IWMReaderAccelerator_iface.lpVtbl = &WMReaderAcceleratorVtbl;
    object->IWMReaderNetworkConfig2_iface.lpVtbl = &WMReaderNetworkConfig2Vtbl;
    object->IWMReaderStreamClock_iface.lpVtbl = &WMReaderStreamClockVtbl;
    object->IWMReaderTypeNegotiation_iface.lpVtbl = &WMReaderTypeNegotiationVtbl;
    object->IWMHeaderInfo3_iface.lpVtbl = &header_info_vtbl;
    object->IWMLanguageList_iface.lpVtbl = &language_list_vtbl;
    object->IWMPacketSize2_iface.lpVtbl = &packet_size_vtbl;
    object->IWMProfile3_iface.lpVtbl = &profile_vtbl;
    object->IWMReaderPlaylistBurn_iface.lpVtbl = &playlist_vtbl;
    object->IWMReaderTimecode_iface.lpVtbl = &timecode_vtbl;
    object->refcount = 1;

    if (FAILED(hr = CoCreateInstance(&CLSID_SystemClock, (IUnknown *)&object->IWMReader_iface,
            CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&object->clock_inner)))
    {
        ok(0, "Got hr %#lx.\n", hr);
        goto failed;
    }
    if (FAILED(hr = WMCreateSyncReader(NULL, 0, (IWMSyncReader **)&object->reader)))
    {
        ok(0, "Got hr %#lx.\n", hr);
        goto failed;
    }
    if (FAILED(hr = IUnknown_QueryInterface(object->reader, &IID_IWMProfile3,
            (void **)&object->profile)))
    {
        ok(0, "Got hr %#lx.\n", hr);
        goto failed;
    }
    if (FAILED(hr = IUnknown_QueryInterface(object->reader, &IID_IWMHeaderInfo3,
            (void **)&object->header_info)))
    {
        ok(0, "Got hr %#lx.\n", hr);
        goto failed;
    }

    InitializeCriticalSection(&object->cs);
    InitializeCriticalSection(&object->callback_cs);

    QueryPerformanceFrequency(&object->clock_frequency);
    list_init(&object->async_ops);

    *reader = (IWMReader *)&object->IWMReader_iface;
    return S_OK;

failed:
    if (object->header_info)
        IUnknown_Release(object->header_info);
    if (object->profile)
        IUnknown_Release(object->profile);
    if (object->reader)
        IUnknown_Release(object->reader);
    if (object->clock_inner)
        IUnknown_Release(object->clock_inner);
    free(object);
    return hr;
}

static BOOL compare_media_types(const WM_MEDIA_TYPE *a, const WM_MEDIA_TYPE *b)
{
    /* We can't use memcmp(), because WM_MEDIA_TYPE has a hole, which sometimes
     * contains junk. */

    return IsEqualGUID(&a->majortype, &b->majortype)
            && IsEqualGUID(&a->subtype, &b->subtype)
            && a->bFixedSizeSamples == b->bFixedSizeSamples
            && a->bTemporalCompression == b->bTemporalCompression
            && a->lSampleSize == b->lSampleSize
            && IsEqualGUID(&a->formattype, &b->formattype)
            && a->pUnk == b->pUnk
            && a->cbFormat == b->cbFormat
            && !memcmp(a->pbFormat, b->pbFormat, a->cbFormat);
}

static void init_audio_type(WM_MEDIA_TYPE *mt, const GUID *subtype, UINT bits, UINT channels, UINT rate)
{
    WAVEFORMATEX *format = (WAVEFORMATEX *)(mt + 1);

    format->wFormatTag = subtype->Data1;
    format->nChannels = channels;
    format->nSamplesPerSec = rate;
    format->wBitsPerSample = bits;
    format->nBlockAlign = format->nChannels * format->wBitsPerSample / 8;
    format->nAvgBytesPerSec = format->nSamplesPerSec * format->nBlockAlign;
    format->cbSize = 0;

    mt->majortype = MEDIATYPE_Audio;
    mt->subtype = *subtype;
    mt->bFixedSizeSamples = TRUE;
    mt->bTemporalCompression = FALSE;
    mt->lSampleSize = format->nBlockAlign;
    mt->formattype = FORMAT_WaveFormatEx;
    mt->pUnk = NULL;
    mt->cbFormat = sizeof(*format);
    mt->pbFormat = (BYTE *)format;
}

static void init_video_type(WM_MEDIA_TYPE *mt, const GUID *subtype, UINT depth, DWORD compression, const RECT *rect)
{
    VIDEOINFOHEADER *video_info = (VIDEOINFOHEADER *)(mt + 1);

    video_info->rcSource = *rect;
    video_info->rcTarget = *rect;
    video_info->dwBitRate = 0;
    video_info->dwBitErrorRate = 0;
    video_info->AvgTimePerFrame = 0;
    video_info->bmiHeader.biSize = sizeof(video_info->bmiHeader);
    video_info->bmiHeader.biWidth = rect->right;
    video_info->bmiHeader.biHeight = rect->bottom;
    video_info->bmiHeader.biPlanes = 1;
    video_info->bmiHeader.biBitCount = depth;
    video_info->bmiHeader.biCompression = compression;
    video_info->bmiHeader.biSizeImage = rect->right * rect->bottom * 4;
    video_info->bmiHeader.biXPelsPerMeter = 0;
    video_info->bmiHeader.biYPelsPerMeter = 0;
    video_info->bmiHeader.biClrUsed = 0;
    video_info->bmiHeader.biClrImportant = 0;

    mt->majortype = MEDIATYPE_Video;
    mt->subtype = *subtype;
    mt->bFixedSizeSamples = TRUE;
    mt->bTemporalCompression = FALSE;
    mt->lSampleSize = video_info->bmiHeader.biSizeImage;
    mt->formattype = FORMAT_VideoInfo;
    mt->pUnk = NULL;
    mt->cbFormat = sizeof(*video_info);
    mt->pbFormat = (BYTE *)video_info;
}

static WCHAR *load_resource(const WCHAR *name)
{
    static WCHAR pathW[MAX_PATH];
    DWORD written;
    HANDLE file;
    HRSRC res;
    void *ptr;

    GetTempPathW(ARRAY_SIZE(pathW), pathW);
    wcscat(pathW, name);

    file = CreateFileW(pathW, GENERIC_READ|GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);
    ok(file != INVALID_HANDLE_VALUE, "Failed to create file %s, error %lu.\n",
            wine_dbgstr_w(pathW), GetLastError());

    res = FindResourceW(NULL, name, (LPCWSTR)RT_RCDATA);
    ok(!!res, "Failed to load resource, error %lu.\n", GetLastError());
    ptr = LockResource(LoadResource(GetModuleHandleA(NULL), res));
    WriteFile(file, ptr, SizeofResource( GetModuleHandleA(NULL), res), &written, NULL);
    ok(written == SizeofResource(GetModuleHandleA(NULL), res), "Failed to write resource.\n");
    CloseHandle(file);

    return pathW;
}

#define check_interface(a, b, c) check_interface_(__LINE__, a, b, c)
static HRESULT check_interface_(unsigned int line, void *iface, REFIID riid, BOOL supported)
{
    HRESULT hr, expected_hr;
    IUnknown *unknown = iface, *out = (IUnknown *)0xbeef;

    expected_hr = supported ? S_OK : E_NOINTERFACE;

    hr = IUnknown_QueryInterface(unknown, riid, (void **)&out);
    ok_(__FILE__, line)(hr == expected_hr, "Got hr %#lx, expected %#lx.\n", hr, expected_hr);
    if (SUCCEEDED(hr))
        IUnknown_Release(out);
    else if (!supported)
        ok_(__FILE__, line)(out == NULL, "Expected out == NULL\n");
    return hr;
}

static void test_wmwriter_interfaces(void)
{
    HRESULT hr;
    IWMWriter *writer;

    hr = WMCreateWriter( NULL, &writer );
    ok(hr == S_OK, "WMCreateWriter failed 0x%08lx\n", hr);
    if(FAILED(hr))
    {
        win_skip("Failed to create IWMWriter\n");
        return;
    }

    check_interface(writer, &IID_IWMWriterSink, FALSE);

    check_interface(writer, &IID_IWMWriter, TRUE);
    check_interface(writer, &IID_IWMWriterAdvanced, TRUE);
    check_interface(writer, &IID_IWMWriterAdvanced2, TRUE);
    check_interface(writer, &IID_IWMWriterAdvanced3, TRUE);
    todo_wine check_interface(writer, &IID_IWMWriterPreprocess, TRUE);
    todo_wine check_interface(writer, &IID_IWMHeaderInfo, TRUE);
    todo_wine check_interface(writer, &IID_IWMHeaderInfo2, TRUE);
    todo_wine check_interface(writer, &IID_IWMHeaderInfo3, TRUE);

    IWMWriter_Release(writer);
}

static void test_wmwriter(void)
{
    INSSBuffer *audio_sample, *video_sample;
    WM_MEDIA_TYPE *audio_mt, *video_mt;
    VIDEOINFOHEADER *video_info;
    WCHAR output_path[MAX_PATH];
    IWMInputMediaProps *props;
    WAVEFORMATEX *audio_info;
    DWORD count, length;
    IWMWriter *writer;
    BYTE *buffer;
    HRESULT hr;
    QWORD pts;

    hr = WMCreateWriter(NULL, &writer);
    ok(hr == S_OK, "WMCreateWriter failed 0x%08lx\n", hr);
    if(FAILED(hr))
    {
        win_skip("Failed to create IWMWriter\n");
        return;
    }

    GetTempPathW(ARRAY_SIZE(output_path), output_path);
    lstrcatW(output_path, L"test.wmv");

    hr = IWMWriter_SetProfileByID(writer, &WMProfile_V80_768Video);
    todo_wine
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    if (hr != S_OK)
        goto skip_writer;
    hr = IWMWriter_SetOutputFilename(writer, output_path);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IWMWriter_GetInputCount(writer, &count);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(count == 2, "Got count %lu.\n", count);

    hr = IWMWriter_GetInputFormatCount(writer, 0, &count);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(count == 36, "Got count %lu.\n", count);
    hr = IWMWriter_GetInputFormatCount(writer, 1, &count);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(count == 12, "Got count %lu.\n", count);

    hr = IWMWriter_GetInputFormat(writer, 0, 0, &props);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    IWMInputMediaProps_GetMediaType(props, NULL, &count);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    audio_mt = calloc(1, count);
    ok(!!audio_mt, "Got audio_mt %p.\n", audio_mt);
    IWMInputMediaProps_GetMediaType(props, audio_mt, &count);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    audio_info = (WAVEFORMATEX *)audio_mt->pbFormat;
    ok(IsEqualGUID(&audio_mt->subtype, &WMMEDIASUBTYPE_PCM),
            "got subtype %s\n", debugstr_guid(&audio_mt->subtype));
    hr = IWMWriter_SetInputProps(writer, 0, props);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    IWMInputMediaProps_Release(props);

    hr = IWMWriter_GetInputFormat(writer, 1, 1, &props);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    IWMInputMediaProps_GetMediaType(props, NULL, &count);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    video_mt = calloc(1, count);
    ok(!!video_mt, "Got video_mt %p.\n", video_mt);
    IWMInputMediaProps_GetMediaType(props, video_mt, &count);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    video_info = (VIDEOINFOHEADER *)video_mt->pbFormat;
    ok(IsEqualGUID(&video_mt->subtype, &WMMEDIASUBTYPE_I420),
            "got subtype %s\n", debugstr_guid(&video_mt->subtype));
    hr = IWMWriter_SetInputProps(writer, 1, props);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    IWMInputMediaProps_Release(props);

    hr = IWMWriter_BeginWriting(writer);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    for (pts = 0; pts <= 20000000; pts += video_info->AvgTimePerFrame)
    {
        length = audio_info->nAvgBytesPerSec / 30;
        hr = IWMWriter_AllocateSample(writer, length, &audio_sample);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        hr = INSSBuffer_GetMaxLength(audio_sample, &length);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        hr = INSSBuffer_GetBuffer(audio_sample, &buffer);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        hr = INSSBuffer_SetLength(audio_sample, length);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        /* set something that's not trivial, or GStreamer may choke on decoding */
        while (length--) buffer[length] = length;
        hr = IWMWriter_WriteSample(writer, 0, pts, 0, audio_sample);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        INSSBuffer_Release(audio_sample);

        hr = IWMWriter_AllocateSample(writer, video_info->bmiHeader.biSizeImage, &video_sample);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        hr = INSSBuffer_GetMaxLength(video_sample, &length);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        hr = INSSBuffer_GetBuffer(video_sample, &buffer);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        hr = INSSBuffer_SetLength(video_sample, length);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        /* set something that's not trivial, or GStreamer may choke on decoding */
        while (length--) buffer[length] = length;
        hr = IWMWriter_WriteSample(writer, 1, pts, 0, video_sample);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        INSSBuffer_Release(video_sample);
    }

    hr = IWMWriter_Flush(writer);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMWriter_EndWriting(writer);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    trace("created %s\n", debugstr_w(output_path));

    free(audio_mt);
    free(video_mt);

skip_writer:
    IWMWriter_Release(writer);
}

static void test_wmreader_interfaces(void)
{
    HRESULT hr;
    IWMReader *reader;

    hr = WMCreateReader( NULL, 0, &reader );
    ok(hr == S_OK, "WMCreateReader failed 0x%08lx\n", hr);
    if(FAILED(hr))
    {
        win_skip("Failed to create IWMReader\n");
        return;
    }

    check_interface(reader, &IID_IWMDRMReader, FALSE);
    check_interface(reader, &IID_IWMDRMReader2, FALSE);
    check_interface(reader, &IID_IWMDRMReader3, FALSE);
    check_interface(reader, &IID_IWMSyncReader, FALSE);
    check_interface(reader, &IID_IWMSyncReader2, FALSE);

    check_interface(reader, &IID_IReferenceClock, TRUE);
    check_interface(reader, &IID_IWMHeaderInfo, TRUE);
    check_interface(reader, &IID_IWMHeaderInfo2, TRUE);
    check_interface(reader, &IID_IWMHeaderInfo3, TRUE);
    check_interface(reader, &IID_IWMLanguageList, TRUE);
    check_interface(reader, &IID_IWMPacketSize, TRUE);
    check_interface(reader, &IID_IWMPacketSize2, TRUE);
    check_interface(reader, &IID_IWMProfile, TRUE);
    check_interface(reader, &IID_IWMProfile2, TRUE);
    check_interface(reader, &IID_IWMProfile3, TRUE);
    check_interface(reader, &IID_IWMReader, TRUE);
    check_interface(reader, &IID_IWMReaderAccelerator, TRUE);
    check_interface(reader, &IID_IWMReaderAdvanced, TRUE);
    check_interface(reader, &IID_IWMReaderAdvanced2, TRUE);
    check_interface(reader, &IID_IWMReaderAdvanced3, TRUE);
    check_interface(reader, &IID_IWMReaderAdvanced4, TRUE);
    check_interface(reader, &IID_IWMReaderAdvanced5, TRUE);
    check_interface(reader, &IID_IWMReaderAdvanced6, TRUE);
    check_interface(reader, &IID_IWMReaderNetworkConfig, TRUE);
    check_interface(reader, &IID_IWMReaderNetworkConfig2, TRUE);
    check_interface(reader, &IID_IWMReaderPlaylistBurn, TRUE);
    check_interface(reader, &IID_IWMReaderStreamClock, TRUE);
    check_interface(reader, &IID_IWMReaderTimecode, TRUE);
    check_interface(reader, &IID_IWMReaderTypeNegotiation, TRUE);

    IWMReader_Release(reader);
}

static void test_wmsyncreader_interfaces(void)
{
    HRESULT hr;
    IWMSyncReader *reader;

    hr = WMCreateSyncReader( NULL, 0, &reader );
    ok(hr == S_OK, "WMCreateSyncReader failed 0x%08lx\n", hr);
    if(FAILED(hr))
    {
        win_skip("Failed to create IWMSyncReader\n");
        return;
    }

    check_interface(reader, &IID_IReferenceClock, FALSE);
    check_interface(reader, &IID_IWMDRMReader, FALSE);
    check_interface(reader, &IID_IWMDRMReader2, FALSE);
    check_interface(reader, &IID_IWMDRMReader3, FALSE);
    check_interface(reader, &IID_IWMReader, FALSE);
    check_interface(reader, &IID_IWMReaderAccelerator, FALSE);
    check_interface(reader, &IID_IWMReaderAdvanced, FALSE);
    check_interface(reader, &IID_IWMReaderAdvanced2, FALSE);
    check_interface(reader, &IID_IWMReaderAdvanced3, FALSE);
    check_interface(reader, &IID_IWMReaderAdvanced4, FALSE);
    check_interface(reader, &IID_IWMReaderAdvanced5, FALSE);
    check_interface(reader, &IID_IWMReaderAdvanced6, FALSE);
    check_interface(reader, &IID_IWMReaderNetworkConfig, FALSE);
    check_interface(reader, &IID_IWMReaderNetworkConfig2, FALSE);
    check_interface(reader, &IID_IWMReaderStreamClock, FALSE);
    check_interface(reader, &IID_IWMReaderTypeNegotiation, FALSE);

    check_interface(reader, &IID_IWMHeaderInfo, TRUE);
    check_interface(reader, &IID_IWMHeaderInfo2, TRUE);
    check_interface(reader, &IID_IWMHeaderInfo3, TRUE);
    check_interface(reader, &IID_IWMLanguageList, TRUE);
    check_interface(reader, &IID_IWMPacketSize, TRUE);
    check_interface(reader, &IID_IWMPacketSize2, TRUE);
    check_interface(reader, &IID_IWMProfile, TRUE);
    check_interface(reader, &IID_IWMProfile2, TRUE);
    check_interface(reader, &IID_IWMProfile3, TRUE);
    check_interface(reader, &IID_IWMReaderPlaylistBurn, TRUE);
    check_interface(reader, &IID_IWMReaderTimecode, TRUE);
    check_interface(reader, &IID_IWMSyncReader, TRUE);
    check_interface(reader, &IID_IWMSyncReader2, TRUE);

    IWMSyncReader_Release(reader);
}


static void test_profile_manager_interfaces(void)
{
    HRESULT hr;
    IWMProfileManager  *profile;

    hr = WMCreateProfileManager(&profile);
    ok(hr == S_OK, "WMCreateProfileManager failed 0x%08lx\n", hr);
    if(FAILED(hr))
    {
        win_skip("Failed to create IWMProfileManager\n");
        return;
    }

    IWMProfileManager_Release(profile);
}

static void test_WMCreateWriterPriv(void)
{
    IWMWriter *writer, *writer2;
    HRESULT hr;

    hr = WMCreateWriterPriv(&writer);
    ok(hr == S_OK, "got 0x%08lx\n", hr);

    hr = IWMWriter_QueryInterface(writer, &IID_IWMWriter, (void**)&writer2);
    ok(hr == S_OK, "got 0x%08lx\n", hr);

    IWMWriter_Release(writer);
    IWMWriter_Release(writer2);
}

static void test_urlextension(void)
{
    HRESULT hr;

    hr = WMCheckURLExtension(NULL);
    ok(hr == E_INVALIDARG, "WMCheckURLExtension failed 0x%08lx\n", hr);
    hr = WMCheckURLExtension(L"test.mkv");
    ok(hr == NS_E_INVALID_NAME, "WMCheckURLExtension failed 0x%08lx\n", hr);
    hr = WMCheckURLExtension(L"test.mp3");
    todo_wine ok(hr == S_OK, "WMCheckURLExtension failed 0x%08lx\n", hr);
    hr = WMCheckURLExtension(L"abcd://test/test.wmv");
    todo_wine ok(hr == S_OK, "WMCheckURLExtension failed 0x%08lx\n", hr);
    hr = WMCheckURLExtension(L"http://test/t.asf?alt=t.mkv");
    todo_wine ok(hr == S_OK, "WMCheckURLExtension failed 0x%08lx\n", hr);
}

static void test_iscontentprotected(void)
{
    HRESULT hr;
    BOOL drm;

    hr = WMIsContentProtected(NULL, NULL);
    ok(hr == E_INVALIDARG, "WMIsContentProtected failed 0x%08lx\n", hr);
    hr = WMIsContentProtected(NULL, &drm);
    ok(hr == E_INVALIDARG, "WMIsContentProtected failed 0x%08lx\n", hr);
    hr = WMIsContentProtected(L"test.mp3", NULL);
    ok(hr == E_INVALIDARG, "WMIsContentProtected failed 0x%08lx\n", hr);
    hr = WMIsContentProtected(L"test.mp3", &drm);
    ok(hr == S_FALSE, "WMIsContentProtected failed 0x%08lx\n", hr);
    ok(drm == FALSE, "got %0dx\n", drm);
}

static LONG outstanding_buffers;

struct buffer
{
    INSSBuffer INSSBuffer_iface;
    LONG refcount;

    DWORD capacity, size;
    BYTE data[1];
};

static inline struct buffer *impl_from_INSSBuffer(INSSBuffer *iface)
{
    return CONTAINING_RECORD(iface, struct buffer, INSSBuffer_iface);
}

static HRESULT WINAPI buffer_QueryInterface(INSSBuffer *iface, REFIID iid, void **out)
{
    if (winetest_debug > 1)
        trace("%04lx: INSSBuffer::QueryInterface(%s)\n", GetCurrentThreadId(), debugstr_guid(iid));

    if (!IsEqualGUID(iid, &IID_INSSBuffer3) && !IsEqualGUID(iid, &IID_IMediaBuffer))
        ok(0, "Unexpected IID %s.\n", debugstr_guid(iid));
    return E_NOINTERFACE;
}

static ULONG WINAPI buffer_AddRef(INSSBuffer *iface)
{
    struct buffer *buffer = impl_from_INSSBuffer(iface);

    return InterlockedIncrement(&buffer->refcount);
}

static ULONG WINAPI buffer_Release(INSSBuffer *iface)
{
    struct buffer *buffer = impl_from_INSSBuffer(iface);
    ULONG refcount = InterlockedDecrement(&buffer->refcount);

    if (!refcount)
    {
        InterlockedDecrement(&outstanding_buffers);
        free(buffer);
    }
    return refcount;
}

static HRESULT WINAPI buffer_GetLength(INSSBuffer *iface, DWORD *size)
{
    struct buffer *buffer = impl_from_INSSBuffer(iface);

    if (winetest_debug > 1)
        trace("%04lx: INSSBuffer::GetLength()\n", GetCurrentThreadId());

    *size = buffer->size;
    return S_OK;
}

static HRESULT WINAPI buffer_SetLength(INSSBuffer *iface, DWORD size)
{
    struct buffer *buffer = impl_from_INSSBuffer(iface);

    if (winetest_debug > 1)
        trace("%04lx: INSSBuffer::SetLength(%lu)\n", GetCurrentThreadId(), size);

    ok(size <= buffer->capacity, "Got size %lu, buffer capacity %lu.\n", size, buffer->capacity);

    buffer->size = size;
    return S_OK;
}

static HRESULT WINAPI buffer_GetMaxLength(INSSBuffer *iface, DWORD *size)
{
    struct buffer *buffer = impl_from_INSSBuffer(iface);

    if (winetest_debug > 1)
        trace("%04lx: INSSBuffer::GetMaxLength()\n", GetCurrentThreadId());

    *size = buffer->capacity;
    return S_OK;
}

static HRESULT WINAPI buffer_GetBuffer(INSSBuffer *iface, BYTE **data)
{
    struct buffer *buffer = impl_from_INSSBuffer(iface);

    if (winetest_debug > 1)
        trace("%04lx: INSSBuffer::GetBuffer()\n", GetCurrentThreadId());

    *data = buffer->data;
    return S_OK;
}

static HRESULT WINAPI buffer_GetBufferAndLength(INSSBuffer *iface, BYTE **data, DWORD *size)
{
    struct buffer *buffer = impl_from_INSSBuffer(iface);

    if (winetest_debug > 1)
        trace("%04lx: INSSBuffer::GetBufferAndLength()\n", GetCurrentThreadId());

    *size = buffer->size;
    *data = buffer->data;
    return S_OK;
}

static const INSSBufferVtbl buffer_vtbl =
{
    buffer_QueryInterface,
    buffer_AddRef,
    buffer_Release,
    buffer_GetLength,
    buffer_SetLength,
    buffer_GetMaxLength,
    buffer_GetBuffer,
    buffer_GetBufferAndLength,
};

struct teststream
{
    IStream IStream_iface;
    LONG refcount;
    HANDLE file;
    DWORD input_tid;
    DWORD main_tid;
};

static struct teststream *impl_from_IStream(IStream *iface)
{
    return CONTAINING_RECORD(iface, struct teststream, IStream_iface);
}

static HRESULT WINAPI stream_QueryInterface(IStream *iface, REFIID iid, void **out)
{
    if (winetest_debug > 1)
        trace("%04lx: IStream::QueryInterface(%s)\n", GetCurrentThreadId(), debugstr_guid(iid));

    if (!IsEqualGUID(iid, &IID_IWMGetSecureChannel) && !IsEqualGUID(iid, &IID_IWMIStreamProps))
        ok(0, "Unexpected IID %s.\n", debugstr_guid(iid));

    return E_NOINTERFACE;
}

static ULONG WINAPI stream_AddRef(IStream *iface)
{
    struct teststream *stream = impl_from_IStream(iface);

    return InterlockedIncrement(&stream->refcount);
}

static ULONG WINAPI stream_Release(IStream *iface)
{
    struct teststream *stream = impl_from_IStream(iface);

    return InterlockedDecrement(&stream->refcount);
}

static HRESULT WINAPI stream_Read(IStream *iface, void *data, ULONG size, ULONG *ret_size)
{
    struct teststream *stream = impl_from_IStream(iface);

    if (winetest_debug > 2)
        trace("%04lx: IStream::Read(size %lu)\n", GetCurrentThreadId(), size);

    if (!stream->input_tid)
        stream->input_tid = GetCurrentThreadId();
    else
    {
        todo_wine_if(stream->input_tid == stream->main_tid)
        ok(stream->input_tid == GetCurrentThreadId(), "got wrong thread\n");
    }

    ok(size > 0, "Got zero size.\n");
    ok(!!ret_size, "Got NULL ret_size pointer.\n");
    if (!ReadFile(stream->file, data, size, ret_size, NULL))
        return HRESULT_FROM_WIN32(GetLastError());
    return S_OK;
}

static HRESULT WINAPI stream_Write(IStream *iface, const void *data, ULONG size, ULONG *ret_size)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI stream_Seek(IStream *iface, LARGE_INTEGER offset, DWORD method, ULARGE_INTEGER *ret_offset)
{
    struct teststream *stream = impl_from_IStream(iface);
    LARGE_INTEGER size;

    if (winetest_debug > 2)
        trace("%04lx: IStream::Seek(offset %I64u, method %#lx)\n", GetCurrentThreadId(), offset.QuadPart, method);

    if (!stream->input_tid)
        stream->input_tid = GetCurrentThreadId();
    else
    {
        todo_wine_if(stream->input_tid == stream->main_tid)
        ok(stream->input_tid == GetCurrentThreadId(), "got wrong thread\n");
    }

    GetFileSizeEx(stream->file, &size);
    ok(offset.QuadPart < size.QuadPart, "Expected offset less than size %I64u, got %I64u.\n",
            size.QuadPart, offset.QuadPart);

    ok(method == STREAM_SEEK_SET, "Got method %#lx.\n", method);
    ok(!ret_offset, "Got unexpected ret_offset pointer %p\n", ret_offset);

    if (!SetFilePointerEx(stream->file, offset, &offset, method))
        return HRESULT_FROM_WIN32(GetLastError());
    return S_OK;
}

static HRESULT WINAPI stream_SetSize(IStream *iface, ULARGE_INTEGER size)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI stream_CopyTo(IStream *iface, IStream *dest, ULARGE_INTEGER size,
        ULARGE_INTEGER *read_size, ULARGE_INTEGER *write_size)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI stream_Commit(IStream *iface, DWORD flags)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI stream_Revert(IStream *iface)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI stream_LockRegion(IStream *iface, ULARGE_INTEGER offset, ULARGE_INTEGER size, DWORD type)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI stream_UnlockRegion(IStream *iface, ULARGE_INTEGER offset, ULARGE_INTEGER size, DWORD type)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI stream_Stat(IStream *iface, STATSTG *stat, DWORD flags)
{
    struct teststream *stream = impl_from_IStream(iface);
    LARGE_INTEGER size;

    if (winetest_debug > 1)
        trace("%04lx: IStream::Stat(flags %#lx)\n", GetCurrentThreadId(), flags);

    if (!stream->input_tid)
        stream->input_tid = GetCurrentThreadId();
    else
    {
        todo_wine_if(stream->input_tid == stream->main_tid)
        ok(stream->input_tid == GetCurrentThreadId(), "got wrong thread\n");
    }

    ok(flags == STATFLAG_NONAME, "Got flags %#lx.\n", flags);

    stat->type = 0xdeadbeef;
    GetFileSizeEx(stream->file, &size);
    stat->cbSize.QuadPart = size.QuadPart;
    stat->grfMode = 0;
    stat->grfLocksSupported = TRUE;

    return S_OK;
}

static HRESULT WINAPI stream_Clone(IStream *iface, IStream **out)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static const IStreamVtbl stream_vtbl =
{
    stream_QueryInterface,
    stream_AddRef,
    stream_Release,
    stream_Read,
    stream_Write,
    stream_Seek,
    stream_SetSize,
    stream_CopyTo,
    stream_Commit,
    stream_Revert,
    stream_LockRegion,
    stream_UnlockRegion,
    stream_Stat,
    stream_Clone,
};

static void teststream_init(struct teststream *stream, HANDLE file)
{
    memset(stream, 0, sizeof(*stream));
    stream->IStream_iface.lpVtbl = &stream_vtbl;
    stream->refcount = 1;
    stream->file = file;
    stream->main_tid = GetCurrentThreadId();
}

static void test_reader_attributes(IWMProfile *profile)
{
    WORD size, stream_number, ret_stream_number;
    IWMHeaderInfo *header_info;
    IWMStreamConfig *config;
    WMT_ATTR_DATATYPE type;
    ULONG count, i;
    QWORD duration;
    DWORD dword;
    HRESULT hr;

    IWMProfile_QueryInterface(profile, &IID_IWMHeaderInfo, (void **)&header_info);

    hr = IWMProfile_GetStreamCount(profile, &count);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(count == 2, "Got count %lu.\n", count);

    for (i = 0; i < count; ++i)
    {
        hr = IWMProfile_GetStream(profile, i, &config);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        hr = IWMStreamConfig_GetStreamNumber(config, &stream_number);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        ret_stream_number = stream_number;

        size = sizeof(DWORD);
        type = 0xdeadbeef;
        dword = 0xdeadbeef;
        hr = IWMHeaderInfo_GetAttributeByName(header_info, &ret_stream_number,
                L"WM/VideoFrameRate", &type, (BYTE *)&dword, &size);
        ok(hr == ASF_E_NOTFOUND, "Got hr %#lx.\n", hr);
        ok(type == 0xdeadbeef, "Got type %#x.\n", type);
        ok(size == sizeof(DWORD), "Got size %u.\n", size);
        ok(dword == 0xdeadbeef, "Got frame rate %lu.\n", dword);
        ok(ret_stream_number == stream_number, "Expected stream number %u, got %u.\n",
                stream_number, ret_stream_number);

        size = sizeof(QWORD);
        type = 0xdeadbeef;
        duration = 0xdeadbeef;
        hr = IWMHeaderInfo_GetAttributeByName(header_info, &ret_stream_number,
                L"Duration", &type, (BYTE *)&duration, &size);
        ok(hr == ASF_E_NOTFOUND, "Got hr %#lx.\n", hr);
        ok(type == 0xdeadbeef, "Got type %#x.\n", type);
        ok(size == sizeof(QWORD), "Got size %u.\n", size);
        ok(ret_stream_number == stream_number, "Expected stream number %u, got %u.\n",
                stream_number, ret_stream_number);

        size = sizeof(DWORD);
        type = 0xdeadbeef;
        dword = 0xdeadbeef;
        hr = IWMHeaderInfo_GetAttributeByName(header_info, &ret_stream_number,
                L"Seekable", &type, (BYTE *)&dword, &size);
        ok(hr == ASF_E_NOTFOUND, "Got hr %#lx.\n", hr);
        ok(type == 0xdeadbeef, "Got type %#x.\n", type);
        ok(size == sizeof(DWORD), "Got size %u.\n", size);
        ok(ret_stream_number == stream_number, "Expected stream number %u, got %u.\n",
                stream_number, ret_stream_number);

        IWMStreamConfig_Release(config);
    }

    /* WM/VideoFrameRate with a NULL stream number. */

    size = sizeof(DWORD);
    type = 0xdeadbeef;
    dword = 0xdeadbeef;
    hr = IWMHeaderInfo_GetAttributeByName(header_info, NULL,
            L"WM/VideoFrameRate", &type, (BYTE *)&dword, &size);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);
    ok(type == 0xdeadbeef, "Got type %#x.\n", type);
    ok(size == sizeof(DWORD), "Got size %u.\n", size);
    ok(dword == 0xdeadbeef, "Got frame rate %lu.\n", dword);

    /* And with a zero stream number. */

    stream_number = 0;
    size = sizeof(DWORD);
    type = 0xdeadbeef;
    dword = 0xdeadbeef;
    hr = IWMHeaderInfo_GetAttributeByName(header_info, &stream_number,
            L"WM/VideoFrameRate", &type, (BYTE *)&dword, &size);
    ok(hr == ASF_E_NOTFOUND, "Got hr %#lx.\n", hr);
    ok(type == 0xdeadbeef, "Got type %#x.\n", type);
    ok(size == sizeof(DWORD), "Got size %u.\n", size);
    ok(dword == 0xdeadbeef, "Got frame rate %lu.\n", dword);
    ok(stream_number == 0, "Got stream number %u.\n", stream_number);

    /* Duration with a NULL stream number. */

    size = sizeof(QWORD);
    type = 0xdeadbeef;
    duration = 0xdeadbeef;
    hr = IWMHeaderInfo_GetAttributeByName(header_info, NULL,
            L"Duration", &type, (BYTE *)&duration, &size);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);
    ok(type == 0xdeadbeef, "Got type %#x.\n", type);
    ok(size == sizeof(QWORD), "Got size %u.\n", size);
    ok(duration == 0xdeadbeef, "Got duration %I64u.\n", duration);

    /* And with a zero stream number. */

    size = sizeof(QWORD);
    type = 0xdeadbeef;
    duration = 0xdeadbeef;
    hr = IWMHeaderInfo_GetAttributeByName(header_info, &stream_number,
            L"Duration", &type, (BYTE *)&duration, &size);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(type == WMT_TYPE_QWORD, "Got type %#x.\n", type);
    ok(size == sizeof(QWORD), "Got size %u.\n", size);
    ok(duration == test_wmv_duration, "Got duration %I64u.\n", duration);
    ok(stream_number == 0, "Got stream number %u.\n", stream_number);

    /* Pass a too-small size. */

    size = sizeof(QWORD) - 1;
    type = 0xdeadbeef;
    duration = 0xdeadbeef;
    hr = IWMHeaderInfo_GetAttributeByName(header_info, &stream_number,
            L"Duration", &type, (BYTE *)&duration, &size);
    ok(hr == ASF_E_BUFFERTOOSMALL, "Got hr %#lx.\n", hr);
    ok(type == 0xdeadbeef, "Got type %#x.\n", type);
    ok(size == sizeof(QWORD), "Got size %u.\n", size);
    ok(duration == 0xdeadbeef, "Got duration %I64u.\n", duration);
    ok(stream_number == 0, "Got stream number %u.\n", stream_number);

    /* Pass a NULL buffer. */

    size = 0xdead;
    type = 0xdeadbeef;
    hr = IWMHeaderInfo_GetAttributeByName(header_info, &stream_number,
            L"Duration", &type, NULL, &size);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(type == WMT_TYPE_QWORD, "Got type %#x.\n", type);
    ok(size == sizeof(QWORD), "Got size %u.\n", size);
    ok(stream_number == 0, "Got stream number %u.\n", stream_number);

    size = sizeof(DWORD);
    type = 0xdeadbeef;
    dword = 0xdeadbeef;
    hr = IWMHeaderInfo_GetAttributeByName(header_info, &stream_number,
            L"Seekable", &type, (BYTE *)&dword, &size);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(type == WMT_TYPE_BOOL, "Got type %#x.\n", type);
    ok(size == sizeof(DWORD), "Got size %u.\n", size);
    ok(dword == TRUE, "Got duration %I64u.\n", duration);
    ok(stream_number == 0, "Got stream number %u.\n", stream_number);

    IWMHeaderInfo_Release(header_info);
}

static void test_sync_reader_selection(IWMSyncReader *reader)
{
    WMT_STREAM_SELECTION selections[2];
    WORD stream_numbers[2];
    QWORD pts, duration;
    INSSBuffer *sample;
    DWORD flags;
    HRESULT hr;

    selections[0] = 0xdeadbeef;
    hr = IWMSyncReader_GetStreamSelected(reader, 0, &selections[0]);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);
    ok(selections[0] == 0xdeadbeef, "Got selection %#x.\n", selections[0]);

    selections[0] = 0xdeadbeef;
    hr = IWMSyncReader_GetStreamSelected(reader, 1, &selections[0]);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(selections[0] == WMT_ON, "Got selection %#x.\n", selections[0]);

    selections[0] = 0xdeadbeef;
    hr = IWMSyncReader_GetStreamSelected(reader, 2, &selections[0]);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(selections[0] == WMT_ON, "Got selection %#x.\n", selections[0]);

    selections[0] = 0xdeadbeef;
    hr = IWMSyncReader_GetStreamSelected(reader, 3, &selections[0]);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);
    ok(selections[0] == 0xdeadbeef, "Got selection %#x.\n", selections[0]);

    hr = IWMSyncReader_SetStreamsSelected(reader, 0, NULL, NULL);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);

    stream_numbers[0] = 1;
    stream_numbers[1] = 0;
    selections[0] = selections[1] = WMT_OFF;
    hr = IWMSyncReader_SetStreamsSelected(reader, 2, stream_numbers, selections);
    ok(hr == NS_E_INVALID_REQUEST, "Got hr %#lx.\n", hr);

    selections[0] = 0xdeadbeef;
    hr = IWMSyncReader_GetStreamSelected(reader, 1, &selections[0]);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(selections[0] == WMT_ON, "Got selection %#x.\n", selections[0]);

    stream_numbers[0] = stream_numbers[1] = 1;
    selections[0] = selections[1] = WMT_OFF;
    hr = IWMSyncReader_SetStreamsSelected(reader, 2, stream_numbers, selections);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    selections[0] = 0xdeadbeef;
    hr = IWMSyncReader_GetStreamSelected(reader, 1, &selections[0]);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(selections[0] == WMT_OFF, "Got selection %#x.\n", selections[0]);

    selections[0] = 0xdeadbeef;
    hr = IWMSyncReader_GetStreamSelected(reader, 2, &selections[0]);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(selections[0] == WMT_ON, "Got selection %#x.\n", selections[0]);

    hr = IWMSyncReader_GetNextSample(reader, 1, &sample, &pts, &duration, &flags, NULL, NULL);
    ok(hr == NS_E_INVALID_REQUEST, "Got hr %#lx.\n", hr);

    hr = IWMSyncReader_GetNextSample(reader, 2, &sample, &pts, &duration, &flags, NULL, NULL);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    INSSBuffer_Release(sample);

    for (;;)
    {
        hr = IWMSyncReader_GetNextSample(reader, 2, &sample, &pts, &duration, &flags, NULL, NULL);
        if (hr == NS_E_NO_MORE_SAMPLES)
            break;
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        INSSBuffer_Release(sample);
    }

    hr = IWMSyncReader_GetNextSample(reader, 1, &sample, &pts, &duration, &flags, NULL, NULL);
    ok(hr == NS_E_INVALID_REQUEST, "Got hr %#lx.\n", hr);

    hr = IWMSyncReader_SetRange(reader, 0, 0);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IWMSyncReader_GetNextSample(reader, 0, &sample, &pts, &duration,
            &flags, NULL, &stream_numbers[0]);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(stream_numbers[0] == 2, "Got stream number %u.\n", stream_numbers[0]);
    INSSBuffer_Release(sample);

    for (;;)
    {
        hr = IWMSyncReader_GetNextSample(reader, 0, &sample, &pts, &duration,
                &flags, NULL, &stream_numbers[0]);
        if (hr == NS_E_NO_MORE_SAMPLES)
            break;
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        ok(stream_numbers[0] == 2, "Got stream number %u.\n", stream_numbers[0]);
        INSSBuffer_Release(sample);
    }

    stream_numbers[0] = stream_numbers[1] = 2;
    selections[0] = selections[1] = WMT_OFF;
    hr = IWMSyncReader_SetStreamsSelected(reader, 2, stream_numbers, selections);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IWMSyncReader_SetRange(reader, 0, 0);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IWMSyncReader_GetNextSample(reader, 0, &sample, &pts, &duration,
            &flags, NULL, &stream_numbers[0]);
    ok(hr == NS_E_NO_MORE_SAMPLES, "Got hr %#lx.\n", hr);

    stream_numbers[0] = 1;
    stream_numbers[1] = 2;
    selections[0] = selections[1] = WMT_ON;
    hr = IWMSyncReader_SetStreamsSelected(reader, 2, stream_numbers, selections);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
}

static void test_sync_reader_compressed(IWMSyncReader *reader)
{
    QWORD pts, duration;
    INSSBuffer *sample;
    WORD stream_number;
    DWORD flags;
    HRESULT hr;

    hr = IWMSyncReader_SetReadStreamSamples(reader, 0, TRUE);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);
    hr = IWMSyncReader_SetReadStreamSamples(reader, 1, TRUE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMSyncReader_SetReadStreamSamples(reader, 2, TRUE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMSyncReader_SetReadStreamSamples(reader, 3, TRUE);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);

    hr = IWMSyncReader_SetRange(reader, 0, 0);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IWMSyncReader_GetNextSample(reader, 0, &sample, &pts, &duration, &flags, NULL, &stream_number);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    INSSBuffer_Release(sample);

    for (;;)
    {
        hr = IWMSyncReader_GetNextSample(reader, 0, &sample, &pts, &duration, &flags, NULL, &stream_number);
        if (hr == NS_E_NO_MORE_SAMPLES)
            break;
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        INSSBuffer_Release(sample);
    }

    hr = IWMSyncReader_SetReadStreamSamples(reader, 1, FALSE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMSyncReader_SetReadStreamSamples(reader, 2, FALSE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
}

static void check_sync_get_output_setting(IWMSyncReader *reader, DWORD output, const WCHAR *name,
        WMT_ATTR_DATATYPE expect_type, DWORD expect_value, HRESULT expect_hr)
{
    WMT_ATTR_DATATYPE type;
    DWORD value;
    HRESULT hr;
    WORD size;

    winetest_push_context("%s", debugstr_w(name));

    value = 0;
    type = expect_type;
    if (expect_type == WMT_TYPE_BOOL)
        size = sizeof(BOOL);
    else if (expect_type == WMT_TYPE_WORD)
        size = sizeof(WORD);
    else
        size = sizeof(DWORD);

    hr = IWMSyncReader_GetOutputSetting(reader, output, name, &type, (BYTE *)&value, &size);
    todo_wine
    ok(hr == expect_hr, "Got hr %#lx.\n", hr);

    if (SUCCEEDED(hr))
    {
        ok(type == expect_type, "Got type %u.\n", type);
        ok(value == expect_value, "Got value %lu.\n", value);
        if (type == WMT_TYPE_BOOL)
            ok(size == sizeof(BOOL), "Got size %u\n", size);
        else if (type == WMT_TYPE_WORD)
            ok(size == sizeof(WORD), "Got size %u\n", size);
        else
            ok(size == sizeof(DWORD), "Got size %u\n", size);
    }

    winetest_pop_context();
}

static void check_sync_set_output_setting(IWMSyncReader *reader, DWORD output, const WCHAR *name,
        WMT_ATTR_DATATYPE type, DWORD value, HRESULT expect_hr, BOOL todo)
{
    HRESULT hr;
    WORD size;

    winetest_push_context("%s", debugstr_w(name));

    if (type == WMT_TYPE_BOOL)
        size = sizeof(BOOL);
    else if (type == WMT_TYPE_WORD)
        size = sizeof(WORD);
    else
        size = sizeof(DWORD);

    hr = IWMSyncReader_SetOutputSetting(reader, output, name, type, (BYTE *)&value, size);
    todo_wine_if(todo)
    ok(hr == expect_hr, "Got hr %#lx.\n", hr);

    winetest_pop_context();
}

static void test_sync_reader_settings(void)
{
    const WCHAR *filename = load_resource(L"test.wmv");
    struct teststream stream;
    WMT_ATTR_DATATYPE type;
    IWMSyncReader *reader;
    DWORD value;
    HRESULT hr;
    WORD size;
    HANDLE file;
    BOOL ret;

    hr = WMCreateSyncReader(NULL, 0, &reader);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    type = WMT_TYPE_BOOL;
    size = sizeof(BOOL);
    value = 0;
    hr = IWMSyncReader_GetOutputSetting(reader, 0, L"AllowInterlacedOutput",
            &type, (BYTE *)&value, &size);
    todo_wine
    ok(hr == E_UNEXPECTED, "Got hr %#lx.\n", hr);

    file = CreateFileW(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, 0);
    ok(file != INVALID_HANDLE_VALUE, "Failed to open %s, error %lu.\n", debugstr_w(file), GetLastError());

    teststream_init(&stream, file);

    hr = IWMSyncReader_OpenStream(reader, &stream.IStream_iface);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(stream.refcount > 1, "Got refcount %ld.\n", stream.refcount);

    check_sync_get_output_setting(reader, 0, L"AllowInterlacedOutput",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_sync_get_output_setting(reader, 0, L"DedicatedDeliveryThread",
            WMT_TYPE_BOOL, 0, NS_E_INVALID_REQUEST);
    check_sync_get_output_setting(reader, 0, L"DeliverOnReceive",
            WMT_TYPE_BOOL, 0, S_OK);
    check_sync_get_output_setting(reader, 0, L"EnableDiscreteOutput",
            WMT_TYPE_BOOL, 0, S_OK);
    check_sync_get_output_setting(reader, 0, L"EnableFrameInterpolation",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_sync_get_output_setting(reader, 0, L"JustInTimeDecode",
            WMT_TYPE_BOOL, 0, NS_E_INVALID_REQUEST);
    check_sync_get_output_setting(reader, 0, L"NeedsPreviousSample",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_sync_get_output_setting(reader, 0, L"ScrambledAudio",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_sync_get_output_setting(reader, 0, L"SingleOutputBuffer",
            WMT_TYPE_BOOL, 0, NS_E_INVALID_REQUEST);
    check_sync_get_output_setting(reader, 0, L"SoftwareScaling",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_sync_get_output_setting(reader, 0, L"VideoSampleDurations",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_sync_get_output_setting(reader, 0, L"EnableWMAProSPDIFOutput",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_sync_get_output_setting(reader, 0, L"StreamLanguage",
            WMT_TYPE_WORD, 0, NS_E_INVALID_REQUEST);
    check_sync_get_output_setting(reader, 0, L"DynamicRangeControl",
            WMT_TYPE_DWORD, -1, S_OK);
    check_sync_get_output_setting(reader, 0, L"EarlyDataDelivery",
            WMT_TYPE_DWORD, 0, S_OK);
    check_sync_get_output_setting(reader, 0, L"SpeakerConfig",
            WMT_TYPE_DWORD, -1, S_OK);

    check_sync_get_output_setting(reader, 1, L"AllowInterlacedOutput",
            WMT_TYPE_BOOL, 0, S_OK);
    check_sync_get_output_setting(reader, 1, L"DedicatedDeliveryThread",
            WMT_TYPE_BOOL, 0, NS_E_INVALID_REQUEST);
    check_sync_get_output_setting(reader, 1, L"DeliverOnReceive",
            WMT_TYPE_BOOL, 0, S_OK);
    check_sync_get_output_setting(reader, 1, L"EnableDiscreteOutput",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_sync_get_output_setting(reader, 1, L"EnableFrameInterpolation",
            WMT_TYPE_BOOL, 0, S_OK);
    check_sync_get_output_setting(reader, 1, L"JustInTimeDecode",
            WMT_TYPE_BOOL, 0, NS_E_INVALID_REQUEST);
    check_sync_get_output_setting(reader, 1, L"NeedsPreviousSample",
            WMT_TYPE_BOOL, 0, NS_E_INVALID_REQUEST);
    check_sync_get_output_setting(reader, 1, L"ScrambledAudio",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_sync_get_output_setting(reader, 1, L"SingleOutputBuffer",
            WMT_TYPE_BOOL, 0, NS_E_INVALID_REQUEST);
    check_sync_get_output_setting(reader, 1, L"SoftwareScaling",
            WMT_TYPE_BOOL, 1, S_OK);
    check_sync_get_output_setting(reader, 1, L"VideoSampleDurations",
            WMT_TYPE_BOOL, 0, S_OK);
    check_sync_get_output_setting(reader, 1, L"EnableWMAProSPDIFOutput",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_sync_get_output_setting(reader, 1, L"StreamLanguage",
            WMT_TYPE_WORD, 0, NS_E_INVALID_REQUEST);
    check_sync_get_output_setting(reader, 1, L"DynamicRangeControl",
            WMT_TYPE_DWORD, 0, E_INVALIDARG);
    check_sync_get_output_setting(reader, 1, L"EarlyDataDelivery",
            WMT_TYPE_DWORD, 0, S_OK);
    check_sync_get_output_setting(reader, 1, L"SpeakerConfig",
            WMT_TYPE_DWORD, 0, E_INVALIDARG);

    check_sync_set_output_setting(reader, 0, L"AllowInterlacedOutput",
            WMT_TYPE_BOOL, 0, E_INVALIDARG, TRUE);
    check_sync_set_output_setting(reader, 0, L"DedicatedDeliveryThread",
            WMT_TYPE_BOOL, 0, NS_E_INVALID_REQUEST, TRUE);
    check_sync_set_output_setting(reader, 0, L"DeliverOnReceive",
            WMT_TYPE_BOOL, 1, S_OK, TRUE);
    check_sync_set_output_setting(reader, 0, L"EnableDiscreteOutput",
            WMT_TYPE_BOOL, 1, S_OK, FALSE);
    check_sync_set_output_setting(reader, 0, L"EnableFrameInterpolation",
            WMT_TYPE_BOOL, 0, E_INVALIDARG, TRUE);
    check_sync_set_output_setting(reader, 0, L"JustInTimeDecode",
            WMT_TYPE_BOOL, 0, NS_E_INVALID_REQUEST, TRUE);
    check_sync_set_output_setting(reader, 0, L"NeedsPreviousSample",
            WMT_TYPE_BOOL, 0, E_INVALIDARG, TRUE);
    check_sync_set_output_setting(reader, 0, L"ScrambledAudio",
            WMT_TYPE_BOOL, 0, E_INVALIDARG, TRUE);
    check_sync_set_output_setting(reader, 0, L"SingleOutputBuffer",
            WMT_TYPE_BOOL, 0, NS_E_INVALID_REQUEST, TRUE);
    check_sync_set_output_setting(reader, 0, L"SoftwareScaling",
            WMT_TYPE_BOOL, 0, E_INVALIDARG, TRUE);
    check_sync_set_output_setting(reader, 0, L"VideoSampleDurations",
            WMT_TYPE_BOOL, 0, E_INVALIDARG, TRUE);
    check_sync_set_output_setting(reader, 0, L"EnableWMAProSPDIFOutput",
            WMT_TYPE_BOOL, 1, S_OK, TRUE);
    check_sync_set_output_setting(reader, 0, L"StreamLanguage",
            WMT_TYPE_WORD, 1, S_OK, TRUE);
    check_sync_set_output_setting(reader, 0, L"DynamicRangeControl",
            WMT_TYPE_DWORD, 1, S_OK, TRUE);
    check_sync_set_output_setting(reader, 0, L"EarlyDataDelivery",
            WMT_TYPE_DWORD, 1000, S_OK, TRUE);
    check_sync_set_output_setting(reader, 0, L"SpeakerConfig",
            WMT_TYPE_DWORD, 1, S_OK, FALSE);

    check_sync_set_output_setting(reader, 1, L"AllowInterlacedOutput",
            WMT_TYPE_BOOL, 1, S_OK, TRUE);
    check_sync_set_output_setting(reader, 1, L"DedicatedDeliveryThread",
            WMT_TYPE_BOOL, 0, NS_E_INVALID_REQUEST, TRUE);
    check_sync_set_output_setting(reader, 1, L"DeliverOnReceive",
            WMT_TYPE_BOOL, 1, S_OK, TRUE);
    check_sync_set_output_setting(reader, 1, L"EnableDiscreteOutput",
            WMT_TYPE_BOOL, 0, E_INVALIDARG, TRUE);
    check_sync_set_output_setting(reader, 1, L"EnableFrameInterpolation",
            WMT_TYPE_BOOL, 1, S_OK, TRUE);
    check_sync_set_output_setting(reader, 1, L"JustInTimeDecode",
            WMT_TYPE_BOOL, 0, NS_E_INVALID_REQUEST, TRUE);
    check_sync_set_output_setting(reader, 1, L"NeedsPreviousSample",
            WMT_TYPE_BOOL, 0, E_INVALIDARG, TRUE);
    check_sync_set_output_setting(reader, 1, L"ScrambledAudio",
            WMT_TYPE_BOOL, 0, E_INVALIDARG, TRUE);
    check_sync_set_output_setting(reader, 1, L"SingleOutputBuffer",
            WMT_TYPE_BOOL, 0, NS_E_INVALID_REQUEST, TRUE);
    check_sync_set_output_setting(reader, 1, L"SoftwareScaling",
            WMT_TYPE_BOOL, 1, S_OK, TRUE);
    check_sync_set_output_setting(reader, 1, L"VideoSampleDurations",
            WMT_TYPE_BOOL, 1, S_OK, FALSE);
    check_sync_set_output_setting(reader, 1, L"EnableWMAProSPDIFOutput",
            WMT_TYPE_BOOL, 0, E_INVALIDARG, TRUE);
    check_sync_set_output_setting(reader, 1, L"StreamLanguage",
            WMT_TYPE_WORD, 1, S_OK, TRUE);
    check_sync_set_output_setting(reader, 1, L"DynamicRangeControl",
            WMT_TYPE_DWORD, 0, E_INVALIDARG, TRUE);
    check_sync_set_output_setting(reader, 1, L"EarlyDataDelivery",
            WMT_TYPE_DWORD, 2000, S_OK, TRUE);
    check_sync_set_output_setting(reader, 1, L"SpeakerConfig",
            WMT_TYPE_DWORD, 0, E_INVALIDARG, TRUE);

    IWMSyncReader_Release(reader);

    ok(stream.refcount == 1, "Got outstanding refcount %ld.\n", stream.refcount);
    CloseHandle(stream.file);
    ret = DeleteFileW(filename);
    ok(ret, "Failed to delete %s, error %lu.\n", debugstr_w(filename), GetLastError());
}

static void test_sync_reader_streaming(void)
{
    DWORD size, capacity, flags, output_number, expect_output_number;
    const WCHAR *filename = load_resource(L"test.wmv");
    QWORD pts, duration, next_pts[2] = {0, 0};
    WORD stream_numbers[2], stream_number;
    IWMStreamConfig *config, *config2;
    bool eos[2] = {0}, first = true;
    struct teststream stream;
    ULONG i, j, count, ref;
    IWMSyncReader *reader;
    IWMProfile *profile;
    INSSBuffer *sample;
    BYTE *data, *data2;
    HANDLE file;
    HRESULT hr;
    BOOL ret;

    file = CreateFileW(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, 0);
    ok(file != INVALID_HANDLE_VALUE, "Failed to open %s, error %lu.\n", debugstr_w(filename), GetLastError());

    teststream_init(&stream, file);

    hr = WMCreateSyncReader(NULL, 0, &reader);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    IWMSyncReader_QueryInterface(reader, &IID_IWMProfile, (void **)&profile);

    hr = IWMSyncReader_OpenStream(reader, &stream.IStream_iface);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(stream.refcount > 1, "Got refcount %ld.\n", stream.refcount);

    hr = IWMProfile_GetStreamCount(profile, NULL);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);

    count = 0xdeadbeef;
    hr = IWMProfile_GetStreamCount(profile, &count);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(count == 2, "Got count %lu.\n", count);

    count = 0xdeadbeef;
    hr = IWMSyncReader_GetOutputCount(reader, &count);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(count == 2, "Got count %lu.\n", count);

    for (i = 0; i < 2; ++i)
    {
        hr = IWMProfile_GetStream(profile, i, &config);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);

        hr = IWMProfile_GetStream(profile, i, &config2);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        ok(config2 != config, "Expected different objects.\n");
        ref = IWMStreamConfig_Release(config2);
        ok(!ref, "Got outstanding refcount %ld.\n", ref);

        hr = IWMProfile_GetStreamByNumber(profile, i + 1, &config2);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        ok(config2 != config, "Expected different objects.\n");
        ref = IWMStreamConfig_Release(config2);
        ok(!ref, "Got outstanding refcount %ld.\n", ref);

        stream_numbers[i] = 0xdead;
        hr = IWMStreamConfig_GetStreamNumber(config, &stream_numbers[i]);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        ok(stream_numbers[i] == i + 1, "Got stream number %u.\n", stream_numbers[i]);

        ref = IWMStreamConfig_Release(config);
        ok(!ref, "Got outstanding refcount %ld.\n", ref);
    }

    hr = IWMProfile_GetStream(profile, 2, &config);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);
    hr = IWMProfile_GetStreamByNumber(profile, 0, &config);
    ok(hr == NS_E_NO_STREAM, "Got hr %#lx.\n", hr);
    hr = IWMProfile_GetStreamByNumber(profile, 3, &config);
    ok(hr == NS_E_NO_STREAM, "Got hr %#lx.\n", hr);

    while (!eos[0] || !eos[1])
    {
        for (j = 0; j < 2; ++j)
        {
            stream_number = pts = duration = flags = output_number = 0xdeadbeef;
            hr = IWMSyncReader_GetNextSample(reader, stream_numbers[j], &sample,
                    &pts, &duration, &flags, &output_number, &stream_number);
            if (first)
                ok(hr == S_OK, "Expected at least one valid sample; got hr %#lx.\n", hr);
            else if (eos[j])
                ok(hr == NS_E_NO_MORE_SAMPLES, "Got hr %#lx.\n", hr);
            else
                ok(hr == S_OK || hr == NS_E_NO_MORE_SAMPLES, "Got hr %#lx.\n", hr);

            if (hr == S_OK)
            {
                hr = INSSBuffer_GetBufferAndLength(sample, &data, &size);
                ok(hr == S_OK, "Got hr %#lx.\n", hr);

                hr = INSSBuffer_GetBuffer(sample, &data2);
                ok(hr == S_OK, "Got hr %#lx.\n", hr);
                ok(data2 == data, "Data pointers didn't match.\n");

                hr = INSSBuffer_GetMaxLength(sample, &capacity);
                ok(hr == S_OK, "Got hr %#lx.\n", hr);
                ok(size <= capacity, "Size %lu exceeds capacity %lu.\n", size, capacity);

                hr = INSSBuffer_SetLength(sample, capacity + 1);
                ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);

                hr = INSSBuffer_SetLength(sample, capacity - 1);
                ok(hr == S_OK, "Got hr %#lx.\n", hr);

                hr = INSSBuffer_GetBufferAndLength(sample, &data2, &size);
                ok(hr == S_OK, "Got hr %#lx.\n", hr);
                ok(data2 == data, "Data pointers didn't match.\n");
                ok(size == capacity - 1, "Expected size %lu, got %lu.\n", capacity - 1, size);

                ref = INSSBuffer_Release(sample);
                ok(!ref, "Got outstanding refcount %ld.\n", ref);

                hr = IWMSyncReader_GetOutputNumberForStream(reader, stream_number, &expect_output_number);
                ok(hr == S_OK, "Got hr %#lx.\n", hr);
                ok(output_number == expect_output_number, "Expected output number %lu, got %lu.\n",
                        expect_output_number, output_number);
            }
            else
            {
                ok(pts == 0xdeadbeef, "Got PTS %I64u.\n", pts);
                ok(duration == 0xdeadbeef, "Got duration %I64u.\n", duration);
                ok(flags == 0xdeadbeef, "Got flags %#lx.\n", flags);
                ok(output_number == 0xdeadbeef, "Got output number %lu.\n", output_number);
                eos[j] = true;
            }

            ok(stream_number == stream_numbers[j], "Expected stream number %u, got %u.\n",
                    stream_numbers[j], stream_number);
        }
        first = false;
    }

    hr = IWMSyncReader_GetNextSample(reader, stream_numbers[0], &sample,
            &pts, &duration, &flags, NULL, NULL);
    ok(hr == NS_E_NO_MORE_SAMPLES, "Got hr %#lx.\n", hr);

    hr = IWMSyncReader_GetNextSample(reader, stream_numbers[1], &sample,
            &pts, &duration, &flags, NULL, NULL);
    ok(hr == NS_E_NO_MORE_SAMPLES, "Got hr %#lx.\n", hr);

    hr = IWMSyncReader_SetRange(reader, 0, 0);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IWMSyncReader_GetNextSample(reader, stream_numbers[0], &sample, &pts, &duration, &flags, NULL, NULL);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(duration == 920000, "got duration %I64d\n", duration);
    INSSBuffer_Release(sample);

    hr = IWMSyncReader_GetNextSample(reader, stream_numbers[1], &sample, &pts, &duration, &flags, &output_number, NULL);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(duration == 10000, "got duration %I64d\n", duration);
    INSSBuffer_Release(sample);

    check_sync_set_output_setting(reader, output_number, L"VideoSampleDurations",
            WMT_TYPE_BOOL, 1, S_OK, FALSE);

    hr = IWMSyncReader_GetNextSample(reader, stream_numbers[1], &sample, &pts, &duration, &flags, NULL, NULL);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(duration == 330000, "got duration %I64d\n", duration);
    INSSBuffer_Release(sample);

    hr = IWMSyncReader_GetNextSample(reader, 0, &sample, &pts, &duration, &flags, NULL, NULL);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);

    hr = IWMSyncReader_GetNextSample(reader, 0, &sample, &pts, &duration, &flags, &output_number, NULL);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    INSSBuffer_Release(sample);

    hr = IWMSyncReader_GetNextSample(reader, 0, &sample, &pts, &duration, &flags, NULL, &stream_number);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    INSSBuffer_Release(sample);

    for (;;)
    {
        stream_number = pts = duration = flags = output_number = 0xdeadbeef;
        hr = IWMSyncReader_GetNextSample(reader, 0, &sample,
                &pts, &duration, &flags, &output_number, &stream_number);
        ok(hr == S_OK || hr == NS_E_NO_MORE_SAMPLES, "Got hr %#lx.\n", hr);

        if (hr == S_OK)
        {
            hr = INSSBuffer_GetBufferAndLength(sample, &data, &size);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);

            hr = INSSBuffer_GetBuffer(sample, &data2);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            ok(data2 == data, "Data pointers didn't match.\n");

            hr = INSSBuffer_GetMaxLength(sample, &capacity);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            ok(size <= capacity, "Size %lu exceeds capacity %lu.\n", size, capacity);

            ref = INSSBuffer_Release(sample);
            ok(!ref, "Got outstanding refcount %ld.\n", ref);
        }
        else
        {
            ok(pts == 0xdeadbeef, "Got PTS %I64u.\n", pts);
            ok(duration == 0xdeadbeef, "Got duration %I64u.\n", duration);
            ok(flags == 0xdeadbeef, "Got flags %#lx.\n", flags);
            ok(output_number == 0xdeadbeef, "Got output number %lu.\n", output_number);
            ok(stream_number == 0xbeef, "Got stream number %u.\n", stream_number);
            break;
        }
    }

    hr = IWMSyncReader_SetRange(reader, 0, 0);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    expect_output_number = next_pts[0] = next_pts[1] = 0;
    for (;;)
    {
        stream_number = pts = duration = flags = output_number = 0xdeadbeef;
        hr = IWMSyncReader_GetNextSample(reader, 0, &sample,
                &pts, &duration, &flags, &output_number, &stream_number);
        ok(hr == S_OK || hr == NS_E_NO_MORE_SAMPLES, "Got hr %#lx.\n", hr);
        if (hr == NS_E_NO_MORE_SAMPLES)
            break;

        ok(pts == next_pts[output_number], "got stream %u pts %I64d\n", stream_number, pts);
        ok(output_number == expect_output_number, "got output %lu\n", output_number);
        ref = INSSBuffer_Release(sample);
        ok(!ref, "Got outstanding refcount %ld.\n", ref);

        next_pts[output_number] = pts + duration;

        if (next_pts[0] >= test_wmv_duration)
            expect_output_number = 1;
        else if (output_number == 0 && next_pts[0] >= next_pts[1] + 3000000)
            expect_output_number = 1;
        else if (output_number == 1 && next_pts[0] <= next_pts[1] + 1500000)
            expect_output_number = 0;
    }

    check_sync_set_output_setting(reader, 0, L"EarlyDataDelivery",
            WMT_TYPE_DWORD, 500, S_OK, FALSE);
    check_sync_set_output_setting(reader, 1, L"EarlyDataDelivery",
            WMT_TYPE_DWORD, 0, S_OK, FALSE);

    hr = IWMSyncReader_SetRange(reader, 0, 0);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    expect_output_number = next_pts[0] = next_pts[1] = 0;
    for (;;)
    {
        stream_number = pts = duration = flags = output_number = 0xdeadbeef;
        hr = IWMSyncReader_GetNextSample(reader, 0, &sample,
                &pts, &duration, &flags, &output_number, &stream_number);
        ok(hr == S_OK || hr == NS_E_NO_MORE_SAMPLES, "Got hr %#lx.\n", hr);
        if (hr == NS_E_NO_MORE_SAMPLES)
            break;

        ok(pts == next_pts[output_number], "got stream %u pts %I64d\n", stream_number, pts);
        ok(output_number == expect_output_number, "got output %lu\n", output_number);
        ref = INSSBuffer_Release(sample);
        ok(!ref, "Got outstanding refcount %ld.\n", ref);

        next_pts[output_number] = pts + duration;

        if (next_pts[0] >= test_wmv_duration)
            expect_output_number = 1;
        else if (output_number == 0 && next_pts[0] + duration >= next_pts[1] + 6000000)
            expect_output_number = 1;
        else if (output_number == 1 && next_pts[0] <= next_pts[1] + 4000000)
            expect_output_number = 0;
    }

    check_sync_set_output_setting(reader, 0, L"EarlyDataDelivery",
            WMT_TYPE_DWORD, 0, S_OK, FALSE);
    check_sync_set_output_setting(reader, 1, L"EarlyDataDelivery",
            WMT_TYPE_DWORD, 500, S_OK, FALSE);

    hr = IWMSyncReader_SetRange(reader, 0, 0);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    expect_output_number = next_pts[0] = next_pts[1] = 0;
    for (;;)
    {
        stream_number = pts = duration = flags = output_number = 0xdeadbeef;
        hr = IWMSyncReader_GetNextSample(reader, 0, &sample,
                &pts, &duration, &flags, &output_number, &stream_number);
        ok(hr == S_OK || hr == NS_E_NO_MORE_SAMPLES, "Got hr %#lx.\n", hr);
        if (hr == NS_E_NO_MORE_SAMPLES)
            break;

        ok(pts == next_pts[output_number], "got stream %u pts %I64d\n", stream_number, pts);
        ok(output_number == expect_output_number, "got output %lu\n", output_number);
        ref = INSSBuffer_Release(sample);
        ok(!ref, "Got outstanding refcount %ld.\n", ref);

        next_pts[output_number] = pts + duration;

        if (next_pts[1] >= 20000000)
            expect_output_number = 0;
        else if (next_pts[0] <= 3000000)
            expect_output_number = 0;
        else if (output_number == 1 && next_pts[1] + duration >= next_pts[0] + 2800000)
            expect_output_number = 0;
        else if (output_number == 0 && next_pts[1] <= next_pts[0] + 1000000)
            expect_output_number = 1;
    }

    check_sync_set_output_setting(reader, 0, L"DeliverOnReceive",
            WMT_TYPE_BOOL, 1, S_OK, FALSE);
    check_sync_set_output_setting(reader, 1, L"DeliverOnReceive",
            WMT_TYPE_BOOL, 1, S_OK, FALSE);

    hr = IWMSyncReader_SetRange(reader, 0, 0);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    expect_output_number = next_pts[0] = next_pts[1] = 0;
    for (;;)
    {
        stream_number = pts = duration = flags = output_number = 0xdeadbeef;
        hr = IWMSyncReader_GetNextSample(reader, 0, &sample,
                &pts, &duration, &flags, &output_number, &stream_number);
        ok(hr == S_OK || hr == NS_E_NO_MORE_SAMPLES, "Got hr %#lx.\n", hr);
        if (hr == NS_E_NO_MORE_SAMPLES)
            break;

        ok(pts == next_pts[output_number], "got stream %u pts %I64d\n", stream_number, pts);
        ok(output_number == expect_output_number, "got output %lu\n", output_number);
        ref = INSSBuffer_Release(sample);
        ok(!ref, "Got outstanding refcount %ld.\n", ref);

        next_pts[output_number] = pts + duration;

        if (next_pts[0] >= test_wmv_duration)
            expect_output_number = 1;
    }

    hr = IWMSyncReader_GetNextSample(reader, 0, &sample,
            &pts, &duration, &flags, NULL, &stream_number);
    ok(hr == NS_E_NO_MORE_SAMPLES, "Got hr %#lx.\n", hr);

    hr = IWMSyncReader_GetNextSample(reader, stream_numbers[0], &sample,
            &pts, &duration, &flags, NULL, NULL);
    ok(hr == NS_E_NO_MORE_SAMPLES, "Got hr %#lx.\n", hr);

    hr = IWMSyncReader_GetNextSample(reader, stream_numbers[1], &sample,
            &pts, &duration, &flags, NULL, NULL);
    ok(hr == NS_E_NO_MORE_SAMPLES, "Got hr %#lx.\n", hr);

    hr = IWMSyncReader_SetRange(reader, 0, 0);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    test_sync_reader_selection(reader);
    test_sync_reader_compressed(reader);

    test_reader_attributes(profile);

    hr = IWMSyncReader_Close(reader);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IWMSyncReader_Close(reader);
    ok(hr == NS_E_INVALID_REQUEST, "Got hr %#lx.\n", hr);

    ok(stream.refcount == 1, "Got outstanding refcount %ld.\n", stream.refcount);

    stream.input_tid = 0; /* FIXME: currently required as Wine calls IStream_Stat synchronously in OpenStream */
    SetFilePointer(stream.file, 0, NULL, FILE_BEGIN);
    hr = IWMSyncReader_OpenStream(reader, &stream.IStream_iface);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(stream.refcount > 1, "Got refcount %ld.\n", stream.refcount);

    IWMProfile_Release(profile);
    ref = IWMSyncReader_Release(reader);
    ok(!ref, "Got outstanding refcount %ld.\n", ref);

    ok(stream.refcount == 1, "Got outstanding refcount %ld.\n", stream.refcount);
    CloseHandle(stream.file);
    ret = DeleteFileW(filename);
    ok(ret, "Failed to delete %s, error %lu.\n", debugstr_w(filename), GetLastError());
}

static void check_video_type(const WM_MEDIA_TYPE *mt)
{
    const VIDEOINFOHEADER *video_info = (const VIDEOINFOHEADER *)mt->pbFormat;
    static const RECT rect = {.right = 320, .bottom = 240};

    ok(IsEqualGUID(&mt->formattype, &FORMAT_VideoInfo), "Got format %s.\n", debugstr_guid(&mt->formattype));
    ok(mt->bFixedSizeSamples == TRUE, "Got fixed size %d.\n", mt->bFixedSizeSamples);
    ok(!mt->bTemporalCompression, "Got temporal compression %d.\n", mt->bTemporalCompression);
    ok(!mt->pUnk, "Got pUnk %p.\n", mt->pUnk);

    ok(EqualRect(&video_info->rcSource, &rect), "Got source rect %s.\n", wine_dbgstr_rect(&video_info->rcSource));
    ok(EqualRect(&video_info->rcTarget, &rect), "Got target rect %s.\n", wine_dbgstr_rect(&video_info->rcTarget));
    ok(!video_info->dwBitRate, "Got bit rate %lu.\n", video_info->dwBitRate);
    ok(!video_info->dwBitErrorRate, "Got bit error rate %lu.\n", video_info->dwBitErrorRate);
    ok(video_info->bmiHeader.biSize == sizeof(video_info->bmiHeader),
            "Got size %lu.\n", video_info->bmiHeader.biSize);
    ok(video_info->bmiHeader.biWidth == 320, "Got width %ld.\n", video_info->bmiHeader.biWidth);
    ok(video_info->bmiHeader.biHeight == 240, "Got height %ld.\n", video_info->bmiHeader.biHeight);
    ok(video_info->bmiHeader.biPlanes == 1, "Got planes %d.\n", video_info->bmiHeader.biPlanes);
}

static void check_audio_type(const WM_MEDIA_TYPE *mt)
{
    const WAVEFORMATEX *wave_format = (const WAVEFORMATEX *)mt->pbFormat;

    ok(IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_PCM), "Got subtype %s.\n", debugstr_guid(&mt->subtype));
    ok(IsEqualGUID(&mt->formattype, &FORMAT_WaveFormatEx), "Got format %s.\n", debugstr_guid(&mt->formattype));
    ok(mt->bFixedSizeSamples == TRUE, "Got fixed size %d.\n", mt->bFixedSizeSamples);
    ok(!mt->bTemporalCompression, "Got temporal compression %d.\n", mt->bTemporalCompression);
    ok(!mt->pUnk, "Got pUnk %p.\n", mt->pUnk);

    ok(wave_format->wFormatTag == WAVE_FORMAT_PCM, "Got tag %#x.\n", wave_format->wFormatTag);
}

static void test_stream_media_props(IWMStreamConfig *config,
        const GUID *majortype, const GUID *subtype, const GUID *formattype, BOOL todo_subtype)
{
    char mt_buffer[2000];
    WM_MEDIA_TYPE *mt = (WM_MEDIA_TYPE *)mt_buffer;
    IWMMediaProps *props;
    DWORD size, ret_size;
    HRESULT hr;

    hr = IWMStreamConfig_QueryInterface(config, &IID_IWMMediaProps, (void **)&props);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    size = 0xdeadbeef;
    hr = IWMMediaProps_GetMediaType(props, NULL, &size);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(size != 0xdeadbeef && size >= sizeof(WM_MEDIA_TYPE), "Got size %lu.\n", size);

    ret_size = size - 1;
    hr = IWMMediaProps_GetMediaType(props, mt, &ret_size);
    ok(hr == ASF_E_BUFFERTOOSMALL, "Got hr %#lx.\n", hr);
    ok(ret_size == size, "Expected size %lu, got %lu.\n", size, ret_size);

    ret_size = sizeof(mt_buffer);
    memset(mt_buffer, 0xcc, sizeof(mt_buffer));
    hr = IWMMediaProps_GetMediaType(props, mt, &ret_size);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(ret_size == size, "Expected size %lu, got %lu.\n", size, ret_size);
    ok(size == sizeof(WM_MEDIA_TYPE) + mt->cbFormat, "Expected size %Iu, got %lu.\n",
            sizeof(WM_MEDIA_TYPE) + mt->cbFormat, size);
    ok(IsEqualGUID(&mt->majortype, majortype), "Expected major type %s, got %s.\n",
            debugstr_guid(majortype), debugstr_guid(&mt->majortype));
    todo_wine_if(todo_subtype)
    ok(IsEqualGUID(&mt->subtype, subtype), "Expected sub type %s, got %s.\n",
            debugstr_guid(subtype), debugstr_guid(&mt->subtype));
    ok(IsEqualGUID(&mt->formattype, formattype), "Expected format type %s, got %s.\n",
            debugstr_guid(formattype), debugstr_guid(&mt->formattype));

    IWMMediaProps_Release(props);
}

static void test_sync_reader_types(void)
{
    char mt_buffer[2000], mt2_buffer[2000];
    const WCHAR *filename = load_resource(L"test.wmv");
    IWMOutputMediaProps *output_props, *output_props2;
    WM_MEDIA_TYPE *mt2 = (WM_MEDIA_TYPE *)mt2_buffer;
    WM_MEDIA_TYPE *mt = (WM_MEDIA_TYPE *)mt_buffer;
    bool got_video = false, got_audio = false;
    DWORD size, ret_size, output_number;
    WORD stream_number, stream_number2;
    GUID majortype, majortype2;
    struct teststream stream;
    IWMStreamConfig *config;
    ULONG count, ref, i, j;
    IWMSyncReader *reader;
    IWMProfile *profile;
    HANDLE file;
    HRESULT hr;
    BOOL ret;

    file = CreateFileW(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, 0);
    ok(file != INVALID_HANDLE_VALUE, "Failed to open %s, error %lu.\n", debugstr_w(filename), GetLastError());

    teststream_init(&stream, file);

    hr = WMCreateSyncReader(NULL, 0, &reader);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    IWMSyncReader_QueryInterface(reader, &IID_IWMProfile, (void **)&profile);

    hr = IWMSyncReader_OpenStream(reader, &stream.IStream_iface);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(stream.refcount > 1, "Got refcount %ld.\n", stream.refcount);

    for (i = 0; i < 2; ++i)
    {
        winetest_push_context("Stream %lu", i);

        hr = IWMProfile_GetStream(profile, i, &config);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);

        stream_number = 0xdead;
        hr = IWMStreamConfig_GetStreamNumber(config, &stream_number);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        ok(stream_number == i + 1, "Got stream number %u.\n", stream_number);

        hr = IWMStreamConfig_GetStreamType(config, &majortype);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        if (i)
        {
            todo_wine
            ok(IsEqualGUID(&majortype, &MEDIATYPE_Video), "Got major type %s.\n", debugstr_guid(&majortype));
        }
        else
        {
            todo_wine
            ok(IsEqualGUID(&majortype, &MEDIATYPE_Audio), "Got major type %s.\n", debugstr_guid(&majortype));
        }

        if (IsEqualGUID(&majortype, &MEDIATYPE_Audio))
            test_stream_media_props(config, &MEDIATYPE_Audio, &MEDIASUBTYPE_MSAUDIO1, &FORMAT_WaveFormatEx, TRUE);
        else
            test_stream_media_props(config, &MEDIATYPE_Video, &MEDIASUBTYPE_WMV1, &FORMAT_VideoInfo, FALSE);

        ref = IWMStreamConfig_Release(config);
        ok(!ref, "Got outstanding refcount %ld.\n", ref);

        output_number = 0xdeadbeef;
        hr = IWMSyncReader_GetOutputNumberForStream(reader, stream_number, &output_number);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        ok(output_number == i, "Got output number %lu.\n", output_number);

        stream_number2 = 0xdead;
        hr = IWMSyncReader_GetStreamNumberForOutput(reader, output_number, &stream_number2);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        ok(stream_number2 == stream_number, "Expected stream number %u, got %u.\n", stream_number, stream_number2);

        hr = IWMSyncReader_GetOutputProps(reader, output_number, &output_props);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);

        ret_size = sizeof(mt_buffer);
        hr = IWMOutputMediaProps_GetMediaType(output_props, mt, &ret_size);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);

        memset(&majortype2, 0xcc, sizeof(majortype2));
        hr = IWMOutputMediaProps_GetType(output_props, &majortype2);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        ok(IsEqualGUID(&majortype2, &majortype), "Expected major type %s, got %s.\n",
                debugstr_guid(&majortype), debugstr_guid(&majortype2));

        hr = IWMOutputMediaProps_SetMediaType(output_props, NULL);
        ok(hr == E_POINTER, "Got hr %#lx.\n", hr);

        memset(mt2_buffer, 0, sizeof(mt2_buffer));
        hr = IWMOutputMediaProps_SetMediaType(output_props, mt2);
        ok(hr == E_FAIL, "Got hr %#lx.\n", hr);

        if (IsEqualGUID(&majortype, &MEDIATYPE_Audio))
        {
            WAVEFORMATEX *format = (WAVEFORMATEX *)mt->pbFormat;

            init_audio_type(mt2, &MEDIASUBTYPE_IEEE_FLOAT, 32, format->nChannels * 2, format->nSamplesPerSec);
            hr = IWMOutputMediaProps_SetMediaType(output_props, mt2);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            hr = IWMSyncReader_SetOutputProps(reader, output_number, output_props);
            todo_wine
            ok(hr == NS_E_AUDIO_CODEC_NOT_INSTALLED, "Got hr %#lx.\n", hr);

            init_audio_type(mt2, &MEDIASUBTYPE_PCM, 8, 1, 11025);
            hr = IWMOutputMediaProps_SetMediaType(output_props, mt2);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            hr = IWMSyncReader_SetOutputProps(reader, output_number, output_props);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);

            init_audio_type(mt2, &MEDIASUBTYPE_IEEE_FLOAT, 32, format->nChannels, format->nSamplesPerSec / 4);
            hr = IWMOutputMediaProps_SetMediaType(output_props, mt2);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            hr = IWMSyncReader_SetOutputProps(reader, output_number, output_props);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
        }
        else
        {
            VIDEOINFO *info = (VIDEOINFO *)mt->pbFormat;
            RECT rect = info->rcTarget;

            init_video_type(mt2, &MEDIASUBTYPE_RGB32, 32, BI_RGB, &rect);
            hr = IWMOutputMediaProps_SetMediaType(output_props, mt2);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            hr = IWMSyncReader_SetOutputProps(reader, output_number, output_props);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);

            init_video_type(mt2, &MEDIASUBTYPE_NV12, 12, MAKEFOURCC('N','V','1','2'), &rect);
            hr = IWMOutputMediaProps_SetMediaType(output_props, mt2);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            hr = IWMSyncReader_SetOutputProps(reader, output_number, output_props);
            todo_wine
            ok(hr == NS_E_INVALID_OUTPUT_FORMAT, "Got hr %#lx.\n", hr);

            InflateRect(&rect, 10, 10);

            init_video_type(mt2, &MEDIASUBTYPE_RGB32, 32, BI_RGB, &rect);
            hr = IWMOutputMediaProps_SetMediaType(output_props, mt2);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            hr = IWMSyncReader_SetOutputProps(reader, output_number, output_props);
            ok(hr == NS_E_INVALID_OUTPUT_FORMAT, "Got hr %#lx.\n", hr);
        }

        hr = IWMOutputMediaProps_SetMediaType(output_props, mt);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        hr = IWMSyncReader_SetOutputProps(reader, output_number, output_props);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);

        ref = IWMOutputMediaProps_Release(output_props);
        ok(!ref, "Got outstanding refcount %ld.\n", ref);

        if (IsEqualGUID(&majortype, &MEDIATYPE_Audio))
        {
            got_audio = true;
            check_audio_type(mt);
        }
        else
        {
            ok(IsEqualGUID(&majortype, &MEDIATYPE_Video), "Got major type %s.\n", debugstr_guid(&majortype));
            ok(IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_RGB24), "Got subtype %s.\n", debugstr_guid(&mt->subtype));
            got_video = true;
            check_video_type(mt);
        }

        count = 0;
        hr = IWMSyncReader_GetOutputFormatCount(reader, output_number, &count);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        ok(count > 0, "Got count %lu.\n", count);

        for (j = 0; j < count; ++j)
        {
            winetest_push_context("Format %lu", j);

            hr = IWMSyncReader_GetOutputFormat(reader, output_number, j, &output_props);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);

            hr = IWMSyncReader_GetOutputFormat(reader, output_number, j, &output_props2);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            ok(output_props2 != output_props, "Expected different objects.\n");
            ref = IWMOutputMediaProps_Release(output_props2);
            ok(!ref, "Got outstanding refcount %ld.\n", ref);

            size = 0xdeadbeef;
            hr = IWMOutputMediaProps_GetMediaType(output_props, NULL, &size);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            ok(size != 0xdeadbeef && size >= sizeof(WM_MEDIA_TYPE), "Got size %lu.\n", size);

            ret_size = size - 1;
            hr = IWMOutputMediaProps_GetMediaType(output_props, mt, &ret_size);
            ok(hr == ASF_E_BUFFERTOOSMALL, "Got hr %#lx.\n", hr);
            ok(ret_size == size, "Expected size %lu, got %lu.\n", size, ret_size);

            ret_size = sizeof(mt_buffer);
            memset(mt_buffer, 0xcc, sizeof(mt_buffer));
            hr = IWMOutputMediaProps_GetMediaType(output_props, mt, &ret_size);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            ok(ret_size == size, "Expected size %lu, got %lu.\n", size, ret_size);
            ok(size == sizeof(WM_MEDIA_TYPE) + mt->cbFormat, "Expected size %Iu, got %lu.\n",
                    sizeof(WM_MEDIA_TYPE) + mt->cbFormat, size);

            ok(IsEqualGUID(&mt->majortype, &majortype), "Got major type %s.\n", debugstr_guid(&mt->majortype));

            if (IsEqualGUID(&mt->majortype, &MEDIATYPE_Audio))
            {
                ok(IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_PCM), "Got subtype %s.\n", debugstr_guid(&mt->subtype));
                check_audio_type(mt);
            }
            else
            {
                ok(!IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_AYUV), "Got subtype %s.\n", debugstr_guid(&mt->subtype));
                check_video_type(mt);
            }

            memset(&majortype2, 0xcc, sizeof(majortype2));
            hr = IWMOutputMediaProps_GetType(output_props, &majortype2);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            ok(IsEqualGUID(&majortype2, &majortype), "Expected major type %s, got %s.\n",
                    debugstr_guid(&majortype), debugstr_guid(&majortype2));

            hr = IWMSyncReader_SetOutputProps(reader, output_number, output_props);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            hr = IWMSyncReader_SetOutputProps(reader, 1 - output_number, output_props);
            if (i)
            {
                todo_wine
                ok(hr == NS_E_INVALID_REQUEST, "Got hr %#lx.\n", hr);
            }
            else
                ok(hr == NS_E_INCOMPATIBLE_FORMAT
                        || hr == NS_E_INVALID_OUTPUT_FORMAT /* win10 */, "Got hr %#lx.\n", hr);
            hr = IWMSyncReader_SetOutputProps(reader, 2, output_props);
            ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);

            hr = IWMSyncReader_GetOutputProps(reader, output_number, &output_props2);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            ok(output_props2 != output_props, "Expected different objects.\n");

            ret_size = sizeof(mt2_buffer);
            hr = IWMOutputMediaProps_GetMediaType(output_props2, mt2, &ret_size);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            ok(compare_media_types(mt, mt2), "Media types didn't match.\n");

            ref = IWMOutputMediaProps_Release(output_props2);
            ok(!ref, "Got outstanding refcount %ld.\n", ref);
            ref = IWMOutputMediaProps_Release(output_props);
            ok(!ref, "Got outstanding refcount %ld.\n", ref);

            winetest_pop_context();
        }

        hr = IWMSyncReader_GetOutputFormat(reader, output_number, count, &output_props);
        ok(hr == NS_E_INVALID_OUTPUT_FORMAT, "Got hr %#lx.\n", hr);

        hr = IWMSyncReader_GetOutputProps(reader, output_number, &output_props);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);

        hr = IWMSyncReader_GetOutputProps(reader, output_number, &output_props2);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        ok(output_props2 != output_props, "Expected different objects.\n");

        ref = IWMOutputMediaProps_Release(output_props2);
        ok(!ref, "Got outstanding refcount %ld.\n", ref);
        ref = IWMOutputMediaProps_Release(output_props);
        ok(!ref, "Got outstanding refcount %ld.\n", ref);

        winetest_pop_context();
    }

    ok(got_audio, "No audio stream was enumerated.\n");
    ok(got_video, "No video stream was enumerated.\n");

    count = 0xdeadbeef;
    hr = IWMSyncReader_GetOutputFormatCount(reader, 2, &count);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);
    ok(count == 0xdeadbeef, "Got count %#lx.\n", count);

    output_props = (void *)0xdeadbeef;
    hr = IWMSyncReader_GetOutputProps(reader, 2, &output_props);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);
    ok(output_props == (void *)0xdeadbeef, "Got output props %p.\n", output_props);

    output_props = (void *)0xdeadbeef;
    hr = IWMSyncReader_GetOutputFormat(reader, 2, 0, &output_props);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);
    ok(output_props == (void *)0xdeadbeef, "Got output props %p.\n", output_props);

    IWMProfile_Release(profile);
    ref = IWMSyncReader_Release(reader);
    ok(!ref, "Got outstanding refcount %ld.\n", ref);

    ok(stream.refcount == 1, "Got outstanding refcount %ld.\n", stream.refcount);
    CloseHandle(stream.file);
    ret = DeleteFileW(filename);
    ok(ret, "Failed to delete %s, error %lu.\n", debugstr_w(filename), GetLastError());
}

static void test_sync_reader_file(void)
{
    const WCHAR *filename = load_resource(L"test.wmv");
    IWMSyncReader *reader;
    IWMProfile *profile;
    DWORD count;
    HRESULT hr;
    ULONG ref;
    BOOL ret;

    hr = WMCreateSyncReader(NULL, 0, &reader);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    IWMSyncReader_QueryInterface(reader, &IID_IWMProfile, (void **)&profile);

    hr = IWMSyncReader_Open(reader, filename);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    count = 0xdeadbeef;
    hr = IWMSyncReader_GetOutputCount(reader, &count);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(count == 2, "Got count %lu.\n", count);

    hr = IWMSyncReader_Close(reader);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IWMSyncReader_Close(reader);
    ok(hr == NS_E_INVALID_REQUEST, "Got hr %#lx.\n", hr);

    hr = IWMSyncReader_Open(reader, filename);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    IWMProfile_Release(profile);
    ref = IWMSyncReader_Release(reader);
    ok(!ref, "Got outstanding refcount %ld.\n", ref);

    ret = DeleteFileW(filename);
    ok(ret, "Failed to delete %s, error %lu.\n", debugstr_w(filename), GetLastError());
}

struct callback
{
    IWMReaderCallback IWMReaderCallback_iface;
    IWMReaderCallbackAdvanced IWMReaderCallbackAdvanced_iface;
    IWMReaderAllocatorEx IWMReaderAllocatorEx_iface;
    LONG refcount;
    HANDLE expect_opened, got_opened;
    HANDLE expect_preroll, got_preroll;
    HANDLE expect_started, got_started;
    HANDLE expect_stopped, got_stopped;
    HANDLE expect_eof, got_eof;
    unsigned int closed_count, started_count, end_of_streaming_count, eof_count, sample_count;
    bool all_streams_off;
    bool allocated_samples;
    bool allocated_compressed_samples;
    void *expect_context;

    bool read_compressed;
    bool todo_compressed;
    DWORD max_stream_sample_size[2];

    bool dedicated_threads;
    DWORD callback_tid;
    DWORD output_tid[2];
    struct teststream *stream;

    DWORD early_data_delivery[2];
    QWORD last_pts[2];
    QWORD next_pts[2];
    QWORD expect_time;
    HANDLE expect_ontime, got_ontime;
};

static struct callback *impl_from_IWMReaderCallback(IWMReaderCallback *iface)
{
    return CONTAINING_RECORD(iface, struct callback, IWMReaderCallback_iface);
}

static HRESULT WINAPI callback_QueryInterface(IWMReaderCallback *iface, REFIID iid, void **out)
{
    struct callback *callback = impl_from_IWMReaderCallback(iface);

    if (winetest_debug > 1)
        trace("%04lx: IWMReaderCallback::QueryInterface(%s)\n", GetCurrentThreadId(), debugstr_guid(iid));

    if (IsEqualGUID(iid, &IID_IWMReaderAllocatorEx))
        *out = &callback->IWMReaderAllocatorEx_iface;
    else if (IsEqualGUID(iid, &IID_IWMReaderCallbackAdvanced))
        *out = &callback->IWMReaderCallbackAdvanced_iface;
    else
    {
        if (!IsEqualGUID(iid, &IID_IWMCredentialCallback))
            ok(0, "Unexpected IID %s.\n", debugstr_guid(iid));

        return E_NOINTERFACE;
    }

    IWMReaderCallback_AddRef(iface);
    return S_OK;
}

static ULONG WINAPI callback_AddRef(IWMReaderCallback *iface)
{
    struct callback *callback = impl_from_IWMReaderCallback(iface);

    return InterlockedIncrement(&callback->refcount);
}

static ULONG WINAPI callback_Release(IWMReaderCallback *iface)
{
    struct callback *callback = impl_from_IWMReaderCallback(iface);

    return InterlockedDecrement(&callback->refcount);
}

static HRESULT WINAPI callback_OnStatus(IWMReaderCallback *iface, WMT_STATUS status,
        HRESULT hr, WMT_ATTR_DATATYPE type, BYTE *value, void *context)
{
    struct callback *callback = impl_from_IWMReaderCallback(iface);
    DWORD ret;

    if (winetest_debug > 1)
        trace("%lu: %04lx: IWMReaderCallback::OnStatus(status %u, hr %#lx, type %#x, value %p)\n",
                GetTickCount(), GetCurrentThreadId(), status, hr, type, value);

    switch (status)
    {
        case WMT_OPENED:
            ok(type == WMT_TYPE_DWORD, "Got type %#x.\n", type);
            ok(!*(DWORD *)value, "Got value %#lx.\n", *(DWORD *)value);
            ok(context == (void *)0xdeadbeef, "Got unexpected context %p.\n", context);
            ret = WaitForSingleObject(callback->expect_opened, 100);
            ok(!ret, "Wait timed out.\n");
            SetEvent(callback->got_opened);
            break;

        case WMT_STARTED:
            callback->callback_tid = GetCurrentThreadId();
            ok(type == WMT_TYPE_DWORD, "Got type %#x.\n", type);
            ok(!*(DWORD *)value, "Got value %#lx.\n", *(DWORD *)value);
            ok(context == (void *)callback->expect_context, "Got unexpected context %p.\n", context);
            ret = WaitForSingleObject(callback->expect_started, 100);
            ok(!ret, "Wait timed out.\n");
            callback->end_of_streaming_count = callback->eof_count = callback->sample_count = 0;
            ++callback->started_count;
            ResetEvent(callback->got_eof);
            SetEvent(callback->got_started);
            break;

        case WMT_STOPPED:
            ok(callback->callback_tid == GetCurrentThreadId(), "got wrong thread\n");
            ok(type == WMT_TYPE_DWORD, "Got type %#x.\n", type);
            ok(!*(DWORD *)value, "Got value %#lx.\n", *(DWORD *)value);
            ok(context == (void *)callback->expect_context, "Got unexpected context %p.\n", context);
            ret = WaitForSingleObject(callback->expect_stopped, 100);
            ok(!ret, "Wait timed out.\n");
            SetEvent(callback->got_stopped);
            break;

        case WMT_CLOSED:
            ok(type == WMT_TYPE_DWORD, "Got type %#x.\n", type);
            ok(!*(DWORD *)value, "Got value %#lx.\n", *(DWORD *)value);
            ok(context == (void *)callback->expect_context, "Got unexpected context %p.\n", context);
            ++callback->closed_count;
            break;

        case WMT_END_OF_STREAMING:
            ok(callback->callback_tid == GetCurrentThreadId(), "got wrong thread\n");
            ok(type == WMT_TYPE_DWORD, "Got type %#x.\n", type);
            ok(!*(DWORD *)value, "Got value %#lx.\n", *(DWORD *)value);
            ok(context == (void *)callback->expect_context, "Got unexpected context %p.\n", context);
            ++callback->end_of_streaming_count;
            break;

        case WMT_EOF:
            ok(callback->callback_tid == GetCurrentThreadId(), "got wrong thread\n");
            ok(type == WMT_TYPE_DWORD, "Got type %#x.\n", type);
            ok(!*(DWORD *)value, "Got value %#lx.\n", *(DWORD *)value);
            ok(context == (void *)callback->expect_context, "Got unexpected context %p.\n", context);
            if (callback->all_streams_off)
                ok(callback->sample_count == 0, "Got %u samples.\n", callback->sample_count);
            else
                ok(callback->sample_count > 0, "Got no samples.\n");
            ret = WaitForSingleObject(callback->expect_eof, 100);
            ok(!ret, "Wait timed out.\n");
            ++callback->eof_count;
            SetEvent(callback->got_eof);
            ok(callback->end_of_streaming_count == 1, "Got %u WMT_END_OF_STREAMING callbacks.\n",
                    callback->end_of_streaming_count);
            break;

        /* Not sent when not using IWMReaderAdvanced::DeliverTime(). */
        case WMT_END_OF_SEGMENT:
            ok(callback->callback_tid == GetCurrentThreadId(), "got wrong thread\n");
            ok(type == WMT_TYPE_QWORD, "Got type %#x.\n", type);
            ok(*(QWORD *)value == 3000, "Got value %#lx.\n", *(DWORD *)value);
            ok(context == (void *)callback->expect_context, "Got unexpected context %p.\n", context);
            if (callback->all_streams_off)
                ok(callback->sample_count == 0, "Got %u samples.\n", callback->sample_count);
            else
                ok(callback->sample_count > 0, "Got no samples.\n");
            ok(callback->eof_count == 1, "Got %u WMT_EOF callbacks.\n",
                    callback->eof_count);
            break;

        case WMT_PREROLL_READY:
            break;

        case WMT_PREROLL_COMPLETE:
            ok(callback->callback_tid == GetCurrentThreadId(), "got wrong thread\n");
            ok(type == WMT_TYPE_DWORD, "Got type %#x.\n", type);
            ok(!*(DWORD *)value, "Got value %#lx.\n", *(DWORD *)value);
            ok(context == (void *)0xfacade, "Got unexpected context %p.\n", context);
            ret = WaitForSingleObject(callback->expect_preroll, 100);
            ok(!ret, "Wait timed out.\n");
            SetEvent(callback->got_preroll);
            break;

        default:
            ok(0, "Unexpected status %#x.\n", status);
    }

    ok(hr == S_OK || hr == E_ABORT, "Got hr %#lx.\n", hr);
    return S_OK;
}

static void check_async_sample(struct callback *callback, INSSBuffer *sample)
{
    DWORD size, capacity;
    BYTE *data, *data2;
    HRESULT hr;

    if (callback->read_compressed ? callback->allocated_compressed_samples : callback->allocated_samples)
    {
        struct buffer *buffer = impl_from_INSSBuffer(sample);

        /* FIXME: Wine badly synchronize compressed reads and user time, spurious
         * samples are possible making todo condition inconsistent. */
        todo_wine_if(callback->todo_compressed && sample->lpVtbl != &buffer_vtbl)
        ok(sample->lpVtbl == &buffer_vtbl, "Buffer vtbl didn't match.\n");
        if (sample->lpVtbl != &buffer_vtbl)
            return;

        ok(buffer->size > 0 && buffer->size <= buffer->capacity, "Got size %ld.\n", buffer->size);
    }
    else
    {
        /* FIXME: Wine badly synchronize compressed reads and user time, spurious
         * samples are possible making todo condition inconsistent. */
        todo_wine_if(callback->todo_compressed && sample->lpVtbl == &buffer_vtbl)
        ok(sample->lpVtbl != &buffer_vtbl, "Buffer vtbl shouldn't match.\n");
        if (sample->lpVtbl == &buffer_vtbl)
            return;

        hr = INSSBuffer_GetBufferAndLength(sample, &data, &size);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);

        hr = INSSBuffer_GetBuffer(sample, &data2);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        ok(data2 == data, "Data pointers didn't match.\n");

        hr = INSSBuffer_GetMaxLength(sample, &capacity);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        ok(size <= capacity, "Size %lu exceeds capacity %lu.\n", size, capacity);

        hr = INSSBuffer_SetLength(sample, capacity + 1);
        ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);

        hr = INSSBuffer_SetLength(sample, capacity - 1);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);

        hr = INSSBuffer_GetBufferAndLength(sample, &data2, &size);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        ok(data2 == data, "Data pointers didn't match.\n");
        ok(size == capacity - 1, "Expected size %lu, got %lu.\n", capacity - 1, size);
    }
}

static HRESULT WINAPI callback_OnSample(IWMReaderCallback *iface, DWORD output,
        QWORD time, QWORD duration, DWORD flags, INSSBuffer *sample, void *context)
{
    struct callback *callback = impl_from_IWMReaderCallback(iface);
    struct teststream *stream = callback->stream;

    if (winetest_debug > 1)
        trace("%lu: %04lx: IWMReaderCallback::OnSample(output %lu, time %I64u, duration %I64u, flags %#lx)\n",
                GetTickCount(), GetCurrentThreadId(), output, time, duration, flags);

    /* uncompressed samples are slightly out of order because of decoding delay */
    ok(callback->last_pts[output] <= time, "got time %I64d\n", time);
    callback->last_pts[output] = time;
    callback->next_pts[output] = time + duration;

    if (callback->dedicated_threads)
        ok(callback->callback_tid != GetCurrentThreadId(), "got wrong thread\n");
    else
        ok(callback->callback_tid == GetCurrentThreadId(), "got wrong thread\n");

    if (!callback->output_tid[output])
        callback->output_tid[output] = GetCurrentThreadId();
    else
        ok(callback->output_tid[output] == GetCurrentThreadId(), "got wrong thread\n");

    if (callback->dedicated_threads && callback->output_tid[1 - output])
        ok(callback->output_tid[1 - output] != GetCurrentThreadId(), "got wrong thread\n");

    if (stream)
        ok(stream->input_tid != GetCurrentThreadId(), "got wrong thread\n");

    ok(context == (void *)callback->expect_context, "Got unexpected context %p.\n", context);

    check_async_sample(callback, sample);

    ok(!callback->read_compressed, "OnSample() should not be called when reading compressed samples.\n");
    ok(callback->started_count > 0, "Got %u WMT_STARTED callbacks.\n", callback->started_count);
    ok(!callback->eof_count, "Got %u WMT_EOF callbacks.\n", callback->eof_count);
    ++callback->sample_count;

    return S_OK;
}

static const IWMReaderCallbackVtbl callback_vtbl =
{
    callback_QueryInterface,
    callback_AddRef,
    callback_Release,
    callback_OnStatus,
    callback_OnSample,
};

static struct callback *impl_from_IWMReaderCallbackAdvanced(IWMReaderCallbackAdvanced *iface)
{
    return CONTAINING_RECORD(iface, struct callback, IWMReaderCallbackAdvanced_iface);
}

static HRESULT WINAPI callback_advanced_QueryInterface(IWMReaderCallbackAdvanced *iface, REFIID iid, void **out)
{
    struct callback *callback = impl_from_IWMReaderCallbackAdvanced(iface);
    return IWMReaderCallback_QueryInterface(&callback->IWMReaderCallback_iface, iid, out);
}

static ULONG WINAPI callback_advanced_AddRef(IWMReaderCallbackAdvanced *iface)
{
    struct callback *callback = impl_from_IWMReaderCallbackAdvanced(iface);
    return IWMReaderCallback_AddRef(&callback->IWMReaderCallback_iface);
}

static ULONG WINAPI callback_advanced_Release(IWMReaderCallbackAdvanced *iface)
{
    struct callback *callback = impl_from_IWMReaderCallbackAdvanced(iface);
    return IWMReaderCallback_Release(&callback->IWMReaderCallback_iface);
}

static HRESULT WINAPI callback_advanced_OnStreamSample(IWMReaderCallbackAdvanced *iface,
        WORD stream_number, QWORD pts, QWORD duration, DWORD flags, INSSBuffer *sample, void *context)
{
    struct callback *callback = impl_from_IWMReaderCallbackAdvanced(iface);
    struct teststream *stream = callback->stream;
    QWORD pts_offset = callback->early_data_delivery[stream_number - 1] * 10000;
    DWORD output = 2 - stream_number;

    if (winetest_debug > 1)
        trace("%lu: %04lx: IWMReaderCallbackAdvanced::OnStreamSample(stream %u, pts %I64u, duration %I64u, flags %#lx)\n",
                GetTickCount(), GetCurrentThreadId(), stream_number, pts, duration, flags);

    ok(callback->last_pts[output] <= pts, "got pts %I64d\n", pts);
    callback->last_pts[output] = pts;
    callback->next_pts[output] = pts + duration;

    if (callback->dedicated_threads)
        ok(callback->callback_tid != GetCurrentThreadId(), "got wrong thread\n");
    else
    {
        ok(callback->callback_tid == GetCurrentThreadId(), "got wrong thread\n");
        ok(callback->last_pts[1 - output] <= pts, "got pts %I64d\n", pts);
        ok(callback->last_pts[1 - output] <= pts + pts_offset, "got pts %I64d\n", pts);
    }

    if (!callback->output_tid[output])
        callback->output_tid[output] = GetCurrentThreadId();
    else
        ok(callback->output_tid[output] == GetCurrentThreadId(), "got wrong thread\n");

    if (callback->dedicated_threads && callback->output_tid[1 - output])
        ok(callback->output_tid[1 - output] != GetCurrentThreadId(), "got wrong thread\n");

    if (stream)
        ok(stream->input_tid != GetCurrentThreadId(), "got wrong thread\n");

    ok(context == (void *)callback->expect_context, "Got unexpected context %p.\n", context);

    check_async_sample(callback, sample);

    ok(callback->read_compressed, "OnStreamSample() should not be called unless reading compressed samples.\n");
    ok(callback->started_count > 0, "Got %u WMT_STARTED callbacks.\n", callback->started_count);
    ok(!callback->eof_count, "Got %u WMT_EOF callbacks.\n", callback->eof_count);
    ++callback->sample_count;

    return S_OK;
}

static HRESULT WINAPI callback_advanced_OnTime(IWMReaderCallbackAdvanced *iface, QWORD time, void *context)
{
    struct callback *callback = impl_from_IWMReaderCallbackAdvanced(iface);
    DWORD ret;

    if (winetest_debug > 1)
        trace("%lu: %04lx: IWMReaderCallbackAdvanced::OnTime(time %I64u)\n",
                GetTickCount(), GetCurrentThreadId(), time);

    ok(callback->callback_tid == GetCurrentThreadId(), "got wrong thread\n");

    todo_wine_if(time % 10000)
    ok(time == callback->expect_time, "Got time %I64u.\n", time);
    ok(context == (void *)callback->expect_context, "Got unexpected context %p.\n", context);
    ret = WaitForSingleObject(callback->expect_ontime, 100);
    ok(!ret, "Wait timed out.\n");
    SetEvent(callback->got_ontime);
    return S_OK;
}

static HRESULT WINAPI callback_advanced_OnStreamSelection(IWMReaderCallbackAdvanced *iface,
        WORD count, WORD *stream_numbers, WMT_STREAM_SELECTION *selections, void *context)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI callback_advanced_OnOutputPropsChanged(IWMReaderCallbackAdvanced *iface,
        DWORD output, WM_MEDIA_TYPE *mt, void *context)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI callback_advanced_AllocateForStream(IWMReaderCallbackAdvanced *iface,
        WORD stream_number, DWORD size, INSSBuffer **sample, void *context)
{
    struct callback *callback = impl_from_IWMReaderCallbackAdvanced(iface);
    DWORD max_size = callback->max_stream_sample_size[stream_number - 1];
    struct teststream *stream = callback->stream;
    struct buffer *object;

    if (winetest_debug > 1)
        trace("%lu: %04lx: IWMReaderCallbackAdvanced::AllocateForStream(output %u, size %lu)\n",
                GetTickCount(), GetCurrentThreadId(), stream_number, size);

    ok(callback->callback_tid != GetCurrentThreadId(), "got wrong thread\n");
    ok(callback->output_tid[stream_number - 1] != GetCurrentThreadId(), "got wrong thread\n");
    if (stream)
    {
        todo_wine
        ok(stream->input_tid == GetCurrentThreadId(), "got wrong thread\n");
    }

    ok(callback->read_compressed, "AllocateForStream() should only be called when reading compressed samples.\n");
    ok(callback->allocated_compressed_samples,
            "AllocateForStream() should only be called when using a custom allocator.\n");

    ok(size <= max_size, "Got size %lu, max stream sample size %lu.\n", size, max_size);

    if (!(object = malloc(offsetof(struct buffer, data[size]))))
        return E_OUTOFMEMORY;

    object->INSSBuffer_iface.lpVtbl = &buffer_vtbl;
    object->refcount = 1;
    object->capacity = size;
    /* Native seems to break if we set the size to zero. */
    object->size = size;
    *sample = &object->INSSBuffer_iface;

    InterlockedIncrement(&outstanding_buffers);

    ok(!context, "Got unexpected context %p.\n", context);
    return S_OK;
}

static HRESULT WINAPI callback_advanced_AllocateForOutput(IWMReaderCallbackAdvanced *iface,
        DWORD output, DWORD size, INSSBuffer **sample, void *context)
{
    struct callback *callback = impl_from_IWMReaderCallbackAdvanced(iface);
    struct teststream *stream = callback->stream;
    struct buffer *object;

    if (winetest_debug > 1)
        trace("%lu: %04lx: IWMReaderCallbackAdvanced::AllocateForOutput(output %lu, size %lu)\n",
                GetTickCount(), GetCurrentThreadId(), output, size);

    ok(callback->callback_tid != GetCurrentThreadId(), "got wrong thread\n");
    ok(callback->output_tid[output] != GetCurrentThreadId(), "got wrong thread\n");
    if (stream)
    {
        todo_wine
        ok(stream->input_tid == GetCurrentThreadId(), "got wrong thread\n");
    }

    if (!callback->read_compressed)
    {
        /* Actually AllocateForOutput() isn't called when reading compressed
         * samples either, but native seems to have some sort of race that
         * causes one call to this function to happen in
         * test_async_reader_allocate_compressed(). */
        ok(callback->allocated_samples,
                "AllocateForOutput() should only be called when using a custom allocator.\n");
    }

    if (!(object = malloc(offsetof(struct buffer, data[size]))))
        return E_OUTOFMEMORY;

    object->INSSBuffer_iface.lpVtbl = &buffer_vtbl;
    object->refcount = 1;
    object->capacity = size;
    object->size = 0;
    *sample = &object->INSSBuffer_iface;

    InterlockedIncrement(&outstanding_buffers);

    ok(!context, "Got unexpected context %p.\n", context);
    return S_OK;
}

static const IWMReaderCallbackAdvancedVtbl callback_advanced_vtbl =
{
    callback_advanced_QueryInterface,
    callback_advanced_AddRef,
    callback_advanced_Release,
    callback_advanced_OnStreamSample,
    callback_advanced_OnTime,
    callback_advanced_OnStreamSelection,
    callback_advanced_OnOutputPropsChanged,
    callback_advanced_AllocateForStream,
    callback_advanced_AllocateForOutput,
};

static struct callback *cb_impl_from_IWMReaderAllocatorEx(IWMReaderAllocatorEx *iface)
{
    return CONTAINING_RECORD(iface, struct callback, IWMReaderAllocatorEx_iface);
}

static HRESULT WINAPI callback_allocator_QueryInterface(IWMReaderAllocatorEx *iface, REFIID iid, void **out)
{
    struct callback *callback = cb_impl_from_IWMReaderAllocatorEx(iface);
    return IWMReaderCallback_QueryInterface(&callback->IWMReaderCallback_iface, iid, out);
}

static ULONG WINAPI callback_allocator_AddRef(IWMReaderAllocatorEx *iface)
{
    struct callback *callback = cb_impl_from_IWMReaderAllocatorEx(iface);
    return IWMReaderCallback_AddRef(&callback->IWMReaderCallback_iface);
}

static ULONG WINAPI callback_allocator_Release(IWMReaderAllocatorEx *iface)
{
    struct callback *callback = cb_impl_from_IWMReaderAllocatorEx(iface);
    return IWMReaderCallback_Release(&callback->IWMReaderCallback_iface);
}

static HRESULT WINAPI callback_allocator_AllocateForStreamEx(IWMReaderAllocatorEx *iface,
        WORD stream_number, DWORD size, INSSBuffer **sample, DWORD flags,
        QWORD pts, QWORD duration, void *context)
{
    struct callback *callback = cb_impl_from_IWMReaderAllocatorEx(iface);
    struct buffer *object;

    ok(callback->allocated_compressed_samples, "Unexpected call.\n");
    ok(stream_number, "got stream_number %u.\n", stream_number);
    ok(!flags, "got flags %#lx.\n", flags);
    ok(!pts, "got pts %I64d.\n", pts);
    ok(!duration, "got duration %I64d.\n", duration);
    ok(!context, "got context %p.\n", context);

    if (!(object = malloc(offsetof(struct buffer, data[size]))))
        return E_OUTOFMEMORY;
    object->INSSBuffer_iface.lpVtbl = &buffer_vtbl;
    object->refcount = 1;
    object->capacity = size;
    object->size = size;

    *sample = &object->INSSBuffer_iface;
    InterlockedIncrement(&outstanding_buffers);
    return S_OK;
}

static HRESULT WINAPI callback_allocator_AllocateForOutputEx(IWMReaderAllocatorEx *iface,
        DWORD output, DWORD size, INSSBuffer **sample, DWORD flags,
        QWORD pts, QWORD duration, void *context)
{
    struct callback *callback = cb_impl_from_IWMReaderAllocatorEx(iface);
    struct buffer *object;

    ok(callback->allocated_samples, "Unexpected call.\n");
    ok(!flags, "got flags %#lx.\n", flags);
    ok(!pts, "got pts %I64d.\n", pts);
    ok(!duration, "got duration %I64d.\n", duration);
    ok(!context, "got context %p.\n", context);

    if (!(object = malloc(offsetof(struct buffer, data[size]))))
        return E_OUTOFMEMORY;
    object->INSSBuffer_iface.lpVtbl = &buffer_vtbl;
    object->refcount = 1;
    object->capacity = size;
    object->size = size;

    *sample = &object->INSSBuffer_iface;
    InterlockedIncrement(&outstanding_buffers);
    return S_OK;
}

static const IWMReaderAllocatorExVtbl callback_allocator_vtbl =
{
    callback_allocator_QueryInterface,
    callback_allocator_AddRef,
    callback_allocator_Release,
    callback_allocator_AllocateForStreamEx,
    callback_allocator_AllocateForOutputEx,
};

static void callback_init(struct callback *callback, struct teststream *stream)
{
    memset(callback, 0, sizeof(*callback));
    callback->IWMReaderCallback_iface.lpVtbl = &callback_vtbl;
    callback->IWMReaderCallbackAdvanced_iface.lpVtbl = &callback_advanced_vtbl;
    callback->IWMReaderAllocatorEx_iface.lpVtbl = &callback_allocator_vtbl;
    callback->refcount = 1;
    callback->expect_opened = CreateEventW(NULL, FALSE, FALSE, NULL);
    callback->got_opened = CreateEventW(NULL, FALSE, FALSE, NULL);
    callback->expect_preroll = CreateEventW(NULL, FALSE, FALSE, NULL);
    callback->got_preroll = CreateEventW(NULL, FALSE, FALSE, NULL);
    callback->expect_started = CreateEventW(NULL, FALSE, FALSE, NULL);
    callback->got_started = CreateEventW(NULL, FALSE, FALSE, NULL);
    callback->expect_stopped = CreateEventW(NULL, FALSE, FALSE, NULL);
    callback->got_stopped = CreateEventW(NULL, FALSE, FALSE, NULL);
    callback->expect_eof = CreateEventW(NULL, FALSE, FALSE, NULL);
    callback->got_eof = CreateEventW(NULL, FALSE, FALSE, NULL);
    callback->expect_ontime = CreateEventW(NULL, FALSE, FALSE, NULL);
    callback->got_ontime = CreateEventW(NULL, FALSE, FALSE, NULL);
    callback->stream = stream;
}

static void callback_cleanup(struct callback *callback)
{
    CloseHandle(callback->got_opened);
    CloseHandle(callback->expect_opened);
    CloseHandle(callback->got_preroll);
    CloseHandle(callback->expect_preroll);
    CloseHandle(callback->got_started);
    CloseHandle(callback->expect_started);
    CloseHandle(callback->got_stopped);
    CloseHandle(callback->expect_stopped);
    CloseHandle(callback->got_eof);
    CloseHandle(callback->expect_eof);
    CloseHandle(callback->got_ontime);
    CloseHandle(callback->expect_ontime);
}

#define wait_opened_callback(a) wait_opened_callback_(__LINE__, a)
static void wait_opened_callback_(int line, struct callback *callback)
{
    DWORD ret;

    ret = WaitForSingleObject(callback->got_opened, 0);
    ok_(__FILE__, line)(ret == WAIT_TIMEOUT, "Got unexpected WMT_OPENED.\n");
    SetEvent(callback->expect_opened);
    ret = WaitForSingleObject(callback->got_opened, 1000);
    ok_(__FILE__, line)(!ret, "Wait timed out.\n");
}

#define wait_preroll_callback(a) wait_preroll_callback_(__LINE__, a)
static void wait_preroll_callback_(int line, struct callback *callback)
{
    DWORD ret;

    ret = WaitForSingleObject(callback->got_preroll, 0);
    ok_(__FILE__, line)(ret == WAIT_TIMEOUT, "Got unexpected WMT_PREROLL_COMPLETE.\n");
    SetEvent(callback->expect_preroll);
    ret = WaitForSingleObject(callback->got_preroll, 1000);
    ok_(__FILE__, line)(!ret, "Wait timed out.\n");
}

#define wait_started_callback(a) wait_started_callback_(__LINE__, a)
static void wait_started_callback_(int line, struct callback *callback)
{
    DWORD ret;

    ret = WaitForSingleObject(callback->got_started, 0);
    ok_(__FILE__, line)(ret == WAIT_TIMEOUT, "Got unexpected WMT_STARTED.\n");
    SetEvent(callback->expect_started);
    ret = WaitForSingleObject(callback->got_started, 1000);
    ok_(__FILE__, line)(!ret, "Wait timed out.\n");
}

#define wait_stopped_callback(a) wait_stopped_callback_(__LINE__, a)
static void wait_stopped_callback_(int line, struct callback *callback)
{
    DWORD ret;

    ret = WaitForSingleObject(callback->got_stopped, 0);
    ok_(__FILE__, line)(ret == WAIT_TIMEOUT, "Got unexpected WMT_STOPPED.\n");
    SetEvent(callback->expect_stopped);
    ret = WaitForSingleObject(callback->got_stopped, 1000);
    ok_(__FILE__, line)(!ret, "Wait timed out.\n");
}

#define wait_eof_callback(a) wait_eof_callback_(__LINE__, a)
static void wait_eof_callback_(int line, struct callback *callback)
{
    DWORD ret;

    ret = WaitForSingleObject(callback->got_eof, 0);
    ok_(__FILE__, line)(ret == WAIT_TIMEOUT, "Got unexpected WMT_EOF.\n");
    SetEvent(callback->expect_eof);
    ret = WaitForSingleObject(callback->got_eof, 1000);
    ok_(__FILE__, line)(!ret, "Wait timed out.\n");
    ok_(__FILE__, line)(callback->eof_count == 1, "Got %u WMT_EOF callbacks.\n", callback->eof_count);
}

#define wait_ontime_callback(a) wait_ontime_callback_(__LINE__, a, FALSE)
static void wait_ontime_callback_(int line, struct callback *callback, BOOL todo)
{
    DWORD ret;

    ret = WaitForSingleObject(callback->got_ontime, 0);
    ok_(__FILE__, line)(ret == WAIT_TIMEOUT, "Got unexpected OnTime.\n");
    SetEvent(callback->expect_ontime);
    ret = WaitForSingleObject(callback->got_ontime, (todo && !strcmp(winetest_platform, "wine")) ? 100 : 1000);
    todo_wine_if(todo)
    ok_(__FILE__, line)(!ret, "Wait timed out.\n");
    if (todo && ret == WAIT_TIMEOUT)
        ResetEvent(callback->expect_ontime);
}

static void check_async_get_output_setting(IWMReaderAdvanced2 *reader, DWORD output, const WCHAR *name,
        WMT_ATTR_DATATYPE expect_type, DWORD expect_value, HRESULT expect_hr)
{
    WMT_ATTR_DATATYPE type;
    DWORD value;
    HRESULT hr;
    WORD size;

    winetest_push_context("%s", debugstr_w(name));

    value = 0;
    type = expect_type;
    if (expect_type == WMT_TYPE_BOOL)
        size = sizeof(BOOL);
    else if (expect_type == WMT_TYPE_WORD)
        size = sizeof(WORD);
    else
        size = sizeof(DWORD);

    hr = IWMReaderAdvanced2_GetOutputSetting(reader, output, name, &type, (BYTE *)&value, &size);
    todo_wine
    ok(hr == expect_hr, "Got hr %#lx.\n", hr);

    if (SUCCEEDED(hr))
    {
        ok(type == expect_type, "Got type %u.\n", type);
        ok(value == expect_value, "Got value %lu.\n", value);
        if (type == WMT_TYPE_BOOL)
            ok(size == sizeof(BOOL), "Got size %u\n", size);
        else if (type == WMT_TYPE_WORD)
            ok(size == sizeof(WORD), "Got size %u\n", size);
        else
            ok(size == sizeof(DWORD), "Got size %u\n", size);
    }

    winetest_pop_context();
}

static void check_async_set_output_setting(IWMReaderAdvanced2 *reader, DWORD output, const WCHAR *name,
        WMT_ATTR_DATATYPE type, DWORD value, HRESULT expect_hr)
{
    HRESULT hr;
    WORD size;

    winetest_push_context("%s", debugstr_w(name));

    if (type == WMT_TYPE_BOOL)
        size = sizeof(BOOL);
    else if (type == WMT_TYPE_WORD)
        size = sizeof(WORD);
    else
        size = sizeof(DWORD);

    hr = IWMReaderAdvanced2_SetOutputSetting(reader, output, name, type, (BYTE *)&value, size);
    todo_wine_if(wcscmp(name, L"DedicatedDeliveryThread"))
    ok(hr == expect_hr, "Got hr %#lx.\n", hr);

    winetest_pop_context();
}

static void run_async_reader(IWMReader *reader, IWMReaderAdvanced2 *advanced, struct callback *callback)
{
    HRESULT hr;

    callback->closed_count = 0;
    callback->started_count = 0;
    callback->sample_count = 0;
    callback->end_of_streaming_count = 0;
    callback->eof_count = 0;
    callback->callback_tid = 0;
    callback->last_pts[0] = 0;
    callback->next_pts[0] = 0;
    callback->last_pts[1] = 0;
    callback->next_pts[1] = 0;
    memset(callback->output_tid, 0, sizeof(callback->output_tid));
    if (callback->stream)
        callback->stream->input_tid = 0;

    check_async_set_output_setting(advanced, 0, L"DedicatedDeliveryThread",
            WMT_TYPE_BOOL, callback->dedicated_threads, S_OK);
    check_async_set_output_setting(advanced, 1, L"DedicatedDeliveryThread",
            WMT_TYPE_BOOL, callback->dedicated_threads, S_OK);

    check_async_set_output_setting(advanced, 0, L"EarlyDataDelivery",
            WMT_TYPE_DWORD, callback->early_data_delivery[0], S_OK);
    check_async_set_output_setting(advanced, 1, L"EarlyDataDelivery",
            WMT_TYPE_DWORD, callback->early_data_delivery[1], S_OK);

    if (callback->early_data_delivery[0] || callback->early_data_delivery[1])
    {
        hr = IWMReaderAdvanced2_Preroll(advanced, 0, 0, 1.0f);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        wait_preroll_callback(callback);
    }

    callback->expect_context = (void *)0xfacade;
    hr = IWMReader_Start(reader, 0, 0, 1.0f, (void *)0xfacade);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    /* When all streams are disabled we may get an EOF callback right
     * after the first Start, or after the second if it took a bit more
     * time to be processed. This is unpredictable so skip the test
     */
    if (!callback->all_streams_off)
    {
        hr = IWMReader_Start(reader, 0, 0, 1.0f, (void *)0xfacade);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        wait_started_callback(callback);
    }

    wait_started_callback(callback);

    hr = IWMReaderAdvanced2_SetUserProvidedClock(advanced, TRUE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    if (!callback->all_streams_off)
    {
        callback->expect_time = 2780000;
        hr = IWMReaderAdvanced2_DeliverTime(advanced, 2770001);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        wait_ontime_callback(callback);
        if (callback->last_pts[0])
        {
            todo_wine
            ok(callback->last_pts[0] == 2780000, "Got pts %I64d.\n", callback->last_pts[0]);
            todo_wine
            ok(callback->next_pts[0] == 3240000, "Got pts %I64d.\n", callback->next_pts[0]);
        }
        if (callback->last_pts[1])
        {
            todo_wine_if(!callback->read_compressed)
            ok(callback->last_pts[1] == 2460000, "Got pts %I64d.\n", callback->last_pts[1]);
            todo_wine
            ok(callback->next_pts[1] == 2470000, "Got pts %I64d.\n", callback->next_pts[1]);
        }

        callback->expect_time = 2850000;
        hr = IWMReaderAdvanced2_DeliverTime(advanced, 2849999);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        wait_ontime_callback(callback);
        if (callback->last_pts[0])
        {
            todo_wine_if(!callback->read_compressed)
            ok(callback->last_pts[0] == 2780000, "Got pts %I64d.\n", callback->last_pts[0]);
            todo_wine
            ok(callback->next_pts[0] == 3240000, "Got pts %I64d.\n", callback->next_pts[0]);
        }
        if (callback->last_pts[1])
        {
            todo_wine_if(!callback->read_compressed)
            ok(callback->last_pts[1] == 2460000, "Got pts %I64d.\n", callback->last_pts[1]);
            todo_wine
            ok(callback->next_pts[1] == 2470000, "Got pts %I64d.\n", callback->next_pts[1]);
        }

        callback->expect_time = 2860000;
        hr = IWMReaderAdvanced2_DeliverTime(advanced, 2850001);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        wait_ontime_callback_(__LINE__, callback, TRUE);
        if (callback->last_pts[0])
        {
            todo_wine_if(!callback->read_compressed)
            ok(callback->last_pts[0] == 2780000, "Got pts %I64d.\n", callback->last_pts[0]);
            todo_wine
            ok(callback->next_pts[0] == 3240000, "Got pts %I64d.\n", callback->next_pts[0]);
        }
        if (callback->last_pts[1])
        {
            todo_wine
            ok(callback->last_pts[1] == 2860000, "Got pts %I64d.\n", callback->last_pts[1]);
            todo_wine
            ok(callback->next_pts[1] == 2870000, "Got pts %I64d.\n", callback->next_pts[1]);
        }
    }

    /* try changing compressed / allocation mode dynamically */
    if (callback->read_compressed)
    {
        callback->expect_time = 6500000;
        hr = IWMReaderAdvanced2_DeliverTime(advanced, 6500000);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        wait_ontime_callback(callback);
        ok(callback->last_pts[0] == 6500000, "Got pts %I64d.\n", callback->last_pts[0]);
        todo_wine
        ok(callback->next_pts[0] == 6960000, "Got pts %I64d.\n", callback->next_pts[0]);
        ok(callback->last_pts[1] == 6460000, "Got pts %I64d.\n", callback->last_pts[1]);
        todo_wine
        ok(callback->next_pts[1] == 6470000, "Got pts %I64d.\n", callback->next_pts[1]);
        ok(callback->sample_count > 0, "Got no samples.\n");
        callback->sample_count = 0;

        callback->todo_compressed = true;
        callback->read_compressed = false;
        hr = IWMReaderAdvanced2_SetReceiveStreamSamples(advanced, 1, FALSE);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        hr = IWMReaderAdvanced2_SetReceiveStreamSamples(advanced, 2, FALSE);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);

        hr = IWMReaderAdvanced2_SetAllocateForOutput(advanced, 0, TRUE);
        todo_wine
        ok(hr == E_UNEXPECTED, "Got hr %#lx.\n", hr);
        hr = IWMReaderAdvanced2_SetAllocateForOutput(advanced, 1, TRUE);
        todo_wine
        ok(hr == E_UNEXPECTED, "Got hr %#lx.\n", hr);
        hr = IWMReaderAdvanced2_SetAllocateForStream(advanced, 1, TRUE);
        todo_wine
        ok(hr == E_UNEXPECTED, "Got hr %#lx.\n", hr);
        hr = IWMReaderAdvanced2_SetAllocateForStream(advanced, 2, TRUE);
        todo_wine
        ok(hr == E_UNEXPECTED, "Got hr %#lx.\n", hr);

        hr = IWMReaderAdvanced2_SetAllocateForOutput(advanced, 0, callback->allocated_samples);
        todo_wine
        ok(hr == E_UNEXPECTED, "Got hr %#lx.\n", hr);
        hr = IWMReaderAdvanced2_SetAllocateForOutput(advanced, 1, callback->allocated_samples);
        todo_wine
        ok(hr == E_UNEXPECTED, "Got hr %#lx.\n", hr);
        hr = IWMReaderAdvanced2_SetAllocateForStream(advanced, 1, callback->allocated_compressed_samples);
        todo_wine
        ok(hr == E_UNEXPECTED, "Got hr %#lx.\n", hr);
        hr = IWMReaderAdvanced2_SetAllocateForStream(advanced, 2, callback->allocated_compressed_samples);
        todo_wine
        ok(hr == E_UNEXPECTED, "Got hr %#lx.\n", hr);

        callback->expect_time = 13460000;
        hr = IWMReaderAdvanced2_DeliverTime(advanced, 13460000);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        wait_ontime_callback(callback);
        todo_wine
        ok(callback->last_pts[0] == 13460000, "Got pts %I64d.\n", callback->last_pts[0]);
        todo_wine
        ok(callback->next_pts[0] == 13930000, "Got pts %I64d.\n", callback->next_pts[0]);
        todo_wine
        ok(callback->last_pts[1] == 13260000, "Got pts %I64d.\n", callback->last_pts[1]);
        todo_wine
        ok(callback->next_pts[1] == 13270000, "Got pts %I64d.\n", callback->next_pts[1]);
        ok(callback->sample_count > 0, "Got no samples.\n");
        callback->sample_count = 0;

        callback->read_compressed = true;
        hr = IWMReaderAdvanced2_SetReceiveStreamSamples(advanced, 1, TRUE);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        hr = IWMReaderAdvanced2_SetReceiveStreamSamples(advanced, 2, TRUE);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
    }

    callback->expect_time = test_wmv_duration * 2;
    hr = IWMReaderAdvanced2_DeliverTime(advanced, test_wmv_duration * 2);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    wait_eof_callback(callback);
    wait_ontime_callback(callback);
    if (callback->last_pts[0])
    {
        todo_wine_if(callback->last_pts[0] == 19500000 || callback->last_pts[0] == 20060000)
        ok(callback->last_pts[0] == 19960000, "Got pts %I64d.\n", callback->last_pts[0]);
        todo_wine
        ok(callback->next_pts[0] == 20420000, "Got pts %I64d.\n", callback->next_pts[0]);
    }
    if (callback->last_pts[1])
    {
        todo_wine_if(!callback->read_compressed)
        ok(callback->last_pts[1] == 20060000, "Got pts %I64d.\n", callback->last_pts[1]);
        todo_wine
        ok(callback->next_pts[1] == 20070000, "Got pts %I64d.\n", callback->next_pts[1]);
    }
    callback->last_pts[0] = 0;
    callback->next_pts[0] = 0;
    callback->last_pts[1] = 0;
    callback->next_pts[1] = 0;

    hr = IWMReader_Stop(reader);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMReader_Stop(reader);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    wait_stopped_callback(callback);
    wait_stopped_callback(callback);

    callback->todo_compressed = false;
    ok(!outstanding_buffers, "Got %ld outstanding buffers.\n", outstanding_buffers);
}

static void test_async_reader_allocate(IWMReader *reader,
        IWMReaderAdvanced2 *advanced, struct callback *callback)
{
    BOOL allocate;
    HRESULT hr;

    callback->allocated_samples = true;
    callback->allocated_compressed_samples = false;

    hr = IWMReaderAdvanced2_GetAllocateForOutput(advanced, 0, &allocate);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(!allocate, "Got allocate %d.\n", allocate);
    hr = IWMReaderAdvanced2_GetAllocateForOutput(advanced, 1, &allocate);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(!allocate, "Got allocate %d.\n", allocate);
    hr = IWMReaderAdvanced2_GetAllocateForOutput(advanced, 2, &allocate);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);

    hr = IWMReaderAdvanced2_GetAllocateForStream(advanced, 0, &allocate);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);
    hr = IWMReaderAdvanced2_GetAllocateForStream(advanced, 1, &allocate);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(!allocate, "Got allocate %d.\n", allocate);
    hr = IWMReaderAdvanced2_GetAllocateForStream(advanced, 2, &allocate);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(!allocate, "Got allocate %d.\n", allocate);
    hr = IWMReaderAdvanced2_GetAllocateForStream(advanced, 3, &allocate);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);

    hr = IWMReaderAdvanced2_SetAllocateForOutput(advanced, 0, TRUE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMReaderAdvanced2_SetAllocateForOutput(advanced, 1, TRUE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMReaderAdvanced2_SetAllocateForOutput(advanced, 2, TRUE);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);

    hr = IWMReaderAdvanced2_GetAllocateForOutput(advanced, 0, &allocate);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(allocate == TRUE, "Got allocate %d.\n", allocate);
    hr = IWMReaderAdvanced2_GetAllocateForOutput(advanced, 1, &allocate);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(allocate == TRUE, "Got allocate %d.\n", allocate);

    hr = IWMReaderAdvanced2_GetAllocateForStream(advanced, 1, &allocate);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(!allocate, "Got allocate %d.\n", allocate);
    hr = IWMReaderAdvanced2_GetAllocateForStream(advanced, 2, &allocate);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(!allocate, "Got allocate %d.\n", allocate);

    run_async_reader(reader, advanced, callback);

    callback->allocated_samples = false;
    callback->allocated_compressed_samples = true;

    hr = IWMReaderAdvanced2_SetAllocateForOutput(advanced, 0, FALSE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMReaderAdvanced2_SetAllocateForOutput(advanced, 1, FALSE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IWMReaderAdvanced2_SetAllocateForStream(advanced, 0, TRUE);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);
    hr = IWMReaderAdvanced2_SetAllocateForStream(advanced, 1, TRUE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMReaderAdvanced2_SetAllocateForStream(advanced, 2, TRUE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMReaderAdvanced2_SetAllocateForStream(advanced, 3, TRUE);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);

    hr = IWMReaderAdvanced2_GetAllocateForOutput(advanced, 0, &allocate);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(!allocate, "Got allocate %d.\n", allocate);
    hr = IWMReaderAdvanced2_GetAllocateForOutput(advanced, 1, &allocate);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(!allocate, "Got allocate %d.\n", allocate);

    hr = IWMReaderAdvanced2_GetAllocateForStream(advanced, 1, &allocate);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(allocate == TRUE, "Got allocate %d.\n", allocate);
    hr = IWMReaderAdvanced2_GetAllocateForStream(advanced, 2, &allocate);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(allocate == TRUE, "Got allocate %d.\n", allocate);

    run_async_reader(reader, advanced, callback);

    hr = IWMReaderAdvanced2_SetAllocateForStream(advanced, 1, FALSE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMReaderAdvanced2_SetAllocateForStream(advanced, 2, FALSE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    callback->allocated_compressed_samples = false;
}

static void test_async_reader_selection(IWMReader *reader,
        IWMReaderAdvanced2 *advanced, struct callback *callback)
{
    WMT_STREAM_SELECTION selections[2];
    WORD stream_numbers[2];
    HRESULT hr;

    selections[0] = 0xdeadbeef;
    hr = IWMReaderAdvanced2_GetStreamSelected(advanced, 0, &selections[0]);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);
    ok(selections[0] == 0xdeadbeef, "Got selection %#x.\n", selections[0]);

    selections[0] = 0xdeadbeef;
    hr = IWMReaderAdvanced2_GetStreamSelected(advanced, 1, &selections[0]);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(selections[0] == WMT_ON, "Got selection %#x.\n", selections[0]);

    selections[0] = 0xdeadbeef;
    hr = IWMReaderAdvanced2_GetStreamSelected(advanced, 2, &selections[0]);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(selections[0] == WMT_ON, "Got selection %#x.\n", selections[0]);

    selections[0] = 0xdeadbeef;
    hr = IWMReaderAdvanced2_GetStreamSelected(advanced, 3, &selections[0]);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);
    ok(selections[0] == 0xdeadbeef, "Got selection %#x.\n", selections[0]);

    hr = IWMReaderAdvanced2_SetStreamsSelected(advanced, 0, NULL, NULL);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);

    stream_numbers[0] = 1;
    stream_numbers[1] = 0;
    selections[0] = selections[1] = WMT_OFF;
    hr = IWMReaderAdvanced2_SetStreamsSelected(advanced, 2, stream_numbers, selections);
    ok(hr == NS_E_INVALID_REQUEST, "Got hr %#lx.\n", hr);

    selections[0] = 0xdeadbeef;
    hr = IWMReaderAdvanced2_GetStreamSelected(advanced, 1, &selections[0]);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(selections[0] == WMT_ON, "Got selection %#x.\n", selections[0]);

    stream_numbers[0] = stream_numbers[1] = 1;
    selections[0] = selections[1] = WMT_OFF;
    hr = IWMReaderAdvanced2_SetStreamsSelected(advanced, 2, stream_numbers, selections);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    selections[0] = 0xdeadbeef;
    hr = IWMReaderAdvanced2_GetStreamSelected(advanced, 1, &selections[0]);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(selections[0] == WMT_OFF, "Got selection %#x.\n", selections[0]);

    selections[0] = 0xdeadbeef;
    hr = IWMReaderAdvanced2_GetStreamSelected(advanced, 2, &selections[0]);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(selections[0] == WMT_ON, "Got selection %#x.\n", selections[0]);

    run_async_reader(reader, advanced, callback);

    stream_numbers[0] = stream_numbers[1] = 2;
    selections[0] = selections[1] = WMT_OFF;
    hr = IWMReaderAdvanced2_SetStreamsSelected(advanced, 2, stream_numbers, selections);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    callback->all_streams_off = true;
    run_async_reader(reader, advanced, callback);
    callback->all_streams_off = false;

    stream_numbers[0] = 1;
    stream_numbers[1] = 2;
    selections[0] = selections[1] = WMT_ON;
    hr = IWMReaderAdvanced2_SetStreamsSelected(advanced, 2, stream_numbers, selections);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
}

static void test_async_reader_compressed(IWMReader *reader,
        IWMReaderAdvanced2 *advanced, struct callback *callback)
{
    HRESULT hr;

    hr = IWMReaderAdvanced2_GetMaxStreamSampleSize(advanced, 0, &callback->max_stream_sample_size[0]);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);
    hr = IWMReaderAdvanced2_GetMaxStreamSampleSize(advanced, 3, &callback->max_stream_sample_size[0]);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);
    hr = IWMReaderAdvanced2_GetMaxStreamSampleSize(advanced, 1, &callback->max_stream_sample_size[0]);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(callback->max_stream_sample_size[0] > 0, "Expected nonzero size.\n");
    hr = IWMReaderAdvanced2_GetMaxStreamSampleSize(advanced, 2, &callback->max_stream_sample_size[1]);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(callback->max_stream_sample_size[1] > 0, "Expected nonzero size.\n");

    hr = IWMReaderAdvanced2_SetReceiveStreamSamples(advanced, 0, TRUE);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);
    hr = IWMReaderAdvanced2_SetReceiveStreamSamples(advanced, 3, TRUE);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);
    hr = IWMReaderAdvanced2_SetReceiveStreamSamples(advanced, 1, TRUE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMReaderAdvanced2_SetReceiveStreamSamples(advanced, 2, TRUE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    callback->read_compressed = true;
    run_async_reader(reader, advanced, callback);
    callback->read_compressed = false;

    hr = IWMReaderAdvanced2_SetReceiveStreamSamples(advanced, 1, FALSE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMReaderAdvanced2_SetReceiveStreamSamples(advanced, 2, FALSE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
}

static void test_async_reader_allocate_compressed(IWMReader *reader,
        IWMReaderAdvanced2 *advanced, struct callback *callback)
{
    HRESULT hr;

    callback->read_compressed = true;

    hr = IWMReaderAdvanced2_SetReceiveStreamSamples(advanced, 1, TRUE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMReaderAdvanced2_SetReceiveStreamSamples(advanced, 2, TRUE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    callback->allocated_compressed_samples = true;

    hr = IWMReaderAdvanced2_SetAllocateForStream(advanced, 1, TRUE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMReaderAdvanced2_SetAllocateForStream(advanced, 2, TRUE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    run_async_reader(reader, advanced, callback);

    hr = IWMReaderAdvanced2_SetAllocateForStream(advanced, 1, FALSE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMReaderAdvanced2_SetAllocateForStream(advanced, 2, FALSE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IWMReaderAdvanced2_SetAllocateForOutput(advanced, 0, TRUE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMReaderAdvanced2_SetAllocateForOutput(advanced, 1, TRUE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    callback->allocated_compressed_samples = false;
    callback->allocated_samples = true;

    run_async_reader(reader, advanced, callback);

    hr = IWMReaderAdvanced2_SetAllocateForOutput(advanced, 0, FALSE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMReaderAdvanced2_SetAllocateForOutput(advanced, 1, FALSE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IWMReaderAdvanced2_SetReceiveStreamSamples(advanced, 1, FALSE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMReaderAdvanced2_SetReceiveStreamSamples(advanced, 2, FALSE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    callback->allocated_samples = false;
    callback->read_compressed = false;
}

static void test_async_reader_settings(void)
{
    const WCHAR *filename = load_resource(L"test.wmv");
    IWMReaderAdvanced2 *reader_advanced;
    struct callback callback;
    WMT_ATTR_DATATYPE type;
    IWMReader *reader;
    DWORD value;
    HRESULT hr;
    WORD size;
    BOOL ret;

    callback_init(&callback, NULL);

    hr = winegstreamer_create_wm_async_reader(&reader);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMReader_QueryInterface(reader, &IID_IWMReaderAdvanced2, (void **)&reader_advanced);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    type = WMT_TYPE_BOOL;
    size = sizeof(BOOL);
    value = 0;
    hr = IWMReaderAdvanced2_GetOutputSetting(reader_advanced, 0, L"AllowInterlacedOutput",
            &type, (BYTE *)&value, &size);
    todo_wine
    ok(hr == E_UNEXPECTED, "Got hr %#lx.\n", hr);

    hr = IWMReader_Open(reader, filename, &callback.IWMReaderCallback_iface, (void **)0xdeadbeef);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    wait_opened_callback(&callback);

    check_async_get_output_setting(reader_advanced, 0, L"AllowInterlacedOutput",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_async_get_output_setting(reader_advanced, 0, L"DedicatedDeliveryThread",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_async_get_output_setting(reader_advanced, 0, L"DeliverOnReceive",
            WMT_TYPE_BOOL, 0, S_OK);
    check_async_get_output_setting(reader_advanced, 0, L"EnableDiscreteOutput",
            WMT_TYPE_BOOL, 0, S_OK);
    check_async_get_output_setting(reader_advanced, 0, L"EnableFrameInterpolation",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_async_get_output_setting(reader_advanced, 0, L"JustInTimeDecode",
            WMT_TYPE_BOOL, 0, S_OK);
    check_async_get_output_setting(reader_advanced, 0, L"NeedsPreviousSample",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_async_get_output_setting(reader_advanced, 0, L"ScrambledAudio",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_async_get_output_setting(reader_advanced, 0, L"SingleOutputBuffer",
            WMT_TYPE_BOOL, 0, S_OK);
    check_async_get_output_setting(reader_advanced, 0, L"SoftwareScaling",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_async_get_output_setting(reader_advanced, 0, L"VideoSampleDurations",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_async_get_output_setting(reader_advanced, 0, L"EnableWMAProSPDIFOutput",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_async_get_output_setting(reader_advanced, 0, L"StreamLanguage",
            WMT_TYPE_WORD, 0, NS_E_INVALID_REQUEST);
    check_async_get_output_setting(reader_advanced, 0, L"DynamicRangeControl",
            WMT_TYPE_DWORD, -1, S_OK);
    check_async_get_output_setting(reader_advanced, 0, L"EarlyDataDelivery",
            WMT_TYPE_DWORD, 0, S_OK);
    check_async_get_output_setting(reader_advanced, 0, L"SpeakerConfig",
            WMT_TYPE_DWORD, -1, S_OK);

    check_async_get_output_setting(reader_advanced, 1, L"AllowInterlacedOutput",
            WMT_TYPE_BOOL, 0, S_OK);
    check_async_get_output_setting(reader_advanced, 1, L"DedicatedDeliveryThread",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_async_get_output_setting(reader_advanced, 1, L"DeliverOnReceive",
            WMT_TYPE_BOOL, 0, S_OK);
    check_async_get_output_setting(reader_advanced, 1, L"EnableDiscreteOutput",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_async_get_output_setting(reader_advanced, 1, L"EnableFrameInterpolation",
            WMT_TYPE_BOOL, 0, S_OK);
    check_async_get_output_setting(reader_advanced, 1, L"JustInTimeDecode",
            WMT_TYPE_BOOL, 0, S_OK);
    check_async_get_output_setting(reader_advanced, 1, L"NeedsPreviousSample",
            WMT_TYPE_BOOL, 0, NS_E_INVALID_REQUEST);
    check_async_get_output_setting(reader_advanced, 1, L"ScrambledAudio",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_async_get_output_setting(reader_advanced, 1, L"SingleOutputBuffer",
            WMT_TYPE_BOOL, 0, S_OK);
    check_async_get_output_setting(reader_advanced, 1, L"SoftwareScaling",
            WMT_TYPE_BOOL, 1, S_OK);
    check_async_get_output_setting(reader_advanced, 1, L"VideoSampleDurations",
            WMT_TYPE_BOOL, 0, S_OK);
    check_async_get_output_setting(reader_advanced, 1, L"EnableWMAProSPDIFOutput",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_async_get_output_setting(reader_advanced, 1, L"StreamLanguage",
            WMT_TYPE_WORD, 0, NS_E_INVALID_REQUEST);
    check_async_get_output_setting(reader_advanced, 1, L"DynamicRangeControl",
            WMT_TYPE_DWORD, 0, E_INVALIDARG);
    check_async_get_output_setting(reader_advanced, 1, L"EarlyDataDelivery",
            WMT_TYPE_DWORD, 0, S_OK);
    check_async_get_output_setting(reader_advanced, 1, L"SpeakerConfig",
            WMT_TYPE_DWORD, 0, E_INVALIDARG);

    check_async_set_output_setting(reader_advanced, 0, L"AllowInterlacedOutput",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_async_set_output_setting(reader_advanced, 0, L"DedicatedDeliveryThread",
            WMT_TYPE_BOOL, 1, S_OK);
    check_async_set_output_setting(reader_advanced, 0, L"DeliverOnReceive",
            WMT_TYPE_BOOL, 1, S_OK);
    check_async_set_output_setting(reader_advanced, 0, L"EnableDiscreteOutput",
            WMT_TYPE_BOOL, 1, S_OK);
    check_async_set_output_setting(reader_advanced, 0, L"EnableFrameInterpolation",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_async_set_output_setting(reader_advanced, 0, L"JustInTimeDecode",
            WMT_TYPE_BOOL, 1, S_OK);
    check_async_set_output_setting(reader_advanced, 0, L"NeedsPreviousSample",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_async_set_output_setting(reader_advanced, 0, L"ScrambledAudio",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_async_set_output_setting(reader_advanced, 0, L"SingleOutputBuffer",
            WMT_TYPE_BOOL, 1, S_OK);
    check_async_set_output_setting(reader_advanced, 0, L"SoftwareScaling",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_async_set_output_setting(reader_advanced, 0, L"VideoSampleDurations",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_async_set_output_setting(reader_advanced, 0, L"EnableWMAProSPDIFOutput",
            WMT_TYPE_BOOL, 1, S_OK);
    check_async_set_output_setting(reader_advanced, 0, L"StreamLanguage",
            WMT_TYPE_WORD, 0, S_OK);
    check_async_set_output_setting(reader_advanced, 0, L"DynamicRangeControl",
            WMT_TYPE_DWORD, 1, S_OK);
    check_async_set_output_setting(reader_advanced, 0, L"EarlyDataDelivery",
            WMT_TYPE_DWORD, 1000, S_OK);
    check_async_set_output_setting(reader_advanced, 0, L"SpeakerConfig",
            WMT_TYPE_DWORD, 1, S_OK);

    check_async_set_output_setting(reader_advanced, 1, L"AllowInterlacedOutput",
            WMT_TYPE_BOOL, 1, S_OK);
    check_async_set_output_setting(reader_advanced, 1, L"DedicatedDeliveryThread",
            WMT_TYPE_BOOL, 1, S_OK);
    check_async_set_output_setting(reader_advanced, 1, L"DeliverOnReceive",
            WMT_TYPE_BOOL, 1, S_OK);
    check_async_set_output_setting(reader_advanced, 1, L"EnableDiscreteOutput",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_async_set_output_setting(reader_advanced, 1, L"EnableFrameInterpolation",
            WMT_TYPE_BOOL, 1, S_OK);
    check_async_set_output_setting(reader_advanced, 1, L"JustInTimeDecode",
            WMT_TYPE_BOOL, 1, S_OK);
    check_async_set_output_setting(reader_advanced, 1, L"NeedsPreviousSample",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_async_set_output_setting(reader_advanced, 1, L"ScrambledAudio",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_async_set_output_setting(reader_advanced, 1, L"SingleOutputBuffer",
            WMT_TYPE_BOOL, 1, S_OK);
    check_async_set_output_setting(reader_advanced, 1, L"SoftwareScaling",
            WMT_TYPE_BOOL, 1, S_OK);
    check_async_set_output_setting(reader_advanced, 1, L"VideoSampleDurations",
            WMT_TYPE_BOOL, 1, S_OK);
    check_async_set_output_setting(reader_advanced, 1, L"EnableWMAProSPDIFOutput",
            WMT_TYPE_BOOL, 0, E_INVALIDARG);
    check_async_set_output_setting(reader_advanced, 1, L"StreamLanguage",
            WMT_TYPE_WORD, 0, S_OK);
    check_async_set_output_setting(reader_advanced, 1, L"DynamicRangeControl",
            WMT_TYPE_DWORD, 0, E_INVALIDARG);
    check_async_set_output_setting(reader_advanced, 1, L"EarlyDataDelivery",
            WMT_TYPE_DWORD, 2000, S_OK);
    check_async_set_output_setting(reader_advanced, 1, L"SpeakerConfig",
            WMT_TYPE_DWORD, 0, E_INVALIDARG);

    SetEvent(callback.expect_started);
    hr = IWMReader_Start(reader, 0, 0, 1, NULL);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMReader_Close(reader);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    IWMReaderAdvanced2_Release(reader_advanced);
    IWMReader_Release(reader);

    callback_cleanup(&callback);

    ret = DeleteFileW(filename);
    ok(ret, "Failed to delete %s, error %lu.\n", debugstr_w(filename), GetLastError());
}

static void test_async_reader_streaming(void)
{
    const WCHAR *filename = load_resource(L"test.wmv");
    IWMReaderAdvanced2 *advanced;
    struct teststream stream;
    struct callback callback;
    IWMStreamConfig *config;
    WORD stream_numbers[2];
    IWMProfile *profile;
    ULONG i, count, ref;
    IWMReader *reader;
    HANDLE file;
    HRESULT hr;
    BOOL ret;

    file = CreateFileW(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, 0);
    ok(file != INVALID_HANDLE_VALUE, "Failed to open %s, error %lu.\n", debugstr_w(filename), GetLastError());

    teststream_init(&stream, file);
    callback_init(&callback, &stream);

    hr = winegstreamer_create_wm_async_reader(&reader);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    IWMReader_QueryInterface(reader, &IID_IWMProfile, (void **)&profile);
    IWMReader_QueryInterface(reader, &IID_IWMReaderAdvanced2, (void **)&advanced);

    hr = IWMReader_Stop(reader);
    ok(hr == E_UNEXPECTED, "Got hr %#lx.\n", hr);
    hr = IWMReader_Start(reader, 0, 0, 1.0, NULL);
    ok(hr == NS_E_INVALID_REQUEST, "Got hr %#lx.\n", hr);

    hr = IWMReaderAdvanced2_OpenStream(advanced, &stream.IStream_iface, &callback.IWMReaderCallback_iface, (void **)0xdeadbeef);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(stream.refcount > 1, "Got refcount %ld.\n", stream.refcount);
    ok(callback.refcount > 1, "Got refcount %ld.\n", callback.refcount);
    wait_opened_callback(&callback);

    stream.input_tid = 0; /* FIXME: currently required as Wine calls IStream_Stat synchronously in OpenStream */
    hr = IWMReaderAdvanced2_OpenStream(advanced, &stream.IStream_iface, &callback.IWMReaderCallback_iface, (void **)0xdeadbee0);
    ok(hr == E_UNEXPECTED, "Got hr %#lx.\n", hr);

    count = 0xdeadbeef;
    hr = IWMReader_GetOutputCount(reader, &count);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(count == 2, "Got count %lu.\n", count);

    for (i = 0; i < 2; ++i)
    {
        hr = IWMProfile_GetStream(profile, i, &config);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);

        stream_numbers[i] = 0xdead;
        hr = IWMStreamConfig_GetStreamNumber(config, &stream_numbers[i]);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        ok(stream_numbers[i] == i + 1, "Got stream number %u.\n", stream_numbers[i]);

        ref = IWMStreamConfig_Release(config);
        ok(!ref, "Got outstanding refcount %ld.\n", ref);
    }

    hr = IWMReader_Start(reader, 0, 0, 1.0f, NULL);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    wait_started_callback(&callback);

    /* By default the reader will time itself, and attempt to deliver samples
     * according to their presentation time. Call DeliverTime with the file
     * duration in order to request all samples as fast as possible. */
    hr = IWMReaderAdvanced2_DeliverTime(advanced, test_wmv_duration * 2);
    ok(hr == E_UNEXPECTED, "Got hr %#lx.\n", hr);
    hr = IWMReaderAdvanced2_SetUserProvidedClock(advanced, TRUE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    callback.expect_time = 0;
    hr = IWMReaderAdvanced2_DeliverTime(advanced, 0);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    wait_ontime_callback(&callback);
    callback.expect_time = test_wmv_duration / 2;
    hr = IWMReaderAdvanced2_DeliverTime(advanced, test_wmv_duration / 2);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    wait_ontime_callback(&callback);
    callback.expect_time = test_wmv_duration * 2;
    hr = IWMReaderAdvanced2_DeliverTime(advanced, test_wmv_duration * 2);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    wait_eof_callback(&callback);
    wait_ontime_callback(&callback);
    todo_wine
    ok(callback.last_pts[0] == 19960000, "Got pts %I64d.\n", callback.last_pts[0]);
    todo_wine
    ok(callback.next_pts[0] == 20420000, "Got pts %I64d.\n", callback.next_pts[0]);
    todo_wine
    ok(callback.last_pts[1] == 20060000, "Got pts %I64d.\n", callback.last_pts[1]);
    todo_wine
    ok(callback.next_pts[1] == 20070000, "Got pts %I64d.\n", callback.next_pts[1]);
    callback.last_pts[0] = 0;
    callback.next_pts[0] = 0;
    callback.last_pts[1] = 0;
    callback.next_pts[1] = 0;

    hr = IWMReader_Start(reader, 0, 0, 1.0f, NULL);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    wait_started_callback(&callback);

    hr = IWMReaderAdvanced2_DeliverTime(advanced, test_wmv_duration * 2);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    wait_eof_callback(&callback);
    wait_ontime_callback(&callback);
    todo_wine
    ok(callback.last_pts[0] == 19960000, "Got pts %I64d.\n", callback.last_pts[0]);
    todo_wine
    ok(callback.next_pts[0] == 20420000, "Got pts %I64d.\n", callback.next_pts[0]);
    todo_wine
    ok(callback.last_pts[1] == 20060000, "Got pts %I64d.\n", callback.last_pts[1]);
    todo_wine
    ok(callback.next_pts[1] == 20070000, "Got pts %I64d.\n", callback.next_pts[1]);
    callback.last_pts[0] = 0;
    callback.next_pts[0] = 0;
    callback.last_pts[1] = 0;
    callback.next_pts[1] = 0;

    hr = IWMReader_Stop(reader);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    wait_stopped_callback(&callback);

    hr = IWMReader_Stop(reader);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    wait_stopped_callback(&callback);

    test_reader_attributes(profile);

    trace("Checking default settings.\n");
    trace("  with stream selection\n");
    test_async_reader_selection(reader, advanced, &callback);
    trace("  with sample allocation\n");
    test_async_reader_allocate(reader, advanced, &callback);
    trace("  with compressed sample\n");
    test_async_reader_compressed(reader, advanced, &callback);
    trace("  with compressed sample allocation\n");
    test_async_reader_allocate_compressed(reader, advanced, &callback);

    callback.dedicated_threads = TRUE;
    trace("Checking DedicatedDeliveryThread.\n");
    trace("  with stream selection\n");
    test_async_reader_selection(reader, advanced, &callback);
    trace("  with sample allocation\n");
    test_async_reader_allocate(reader, advanced, &callback);
    trace("  with compressed sample\n");
    test_async_reader_compressed(reader, advanced, &callback);
    trace("  with compressed sample allocation\n");
    test_async_reader_allocate_compressed(reader, advanced, &callback);
    callback.dedicated_threads = FALSE;

    callback.early_data_delivery[0] = 500;
    trace("Checking EarlyDataDelivery[0] = 500.\n");
    trace("  with stream selection\n");
    test_async_reader_selection(reader, advanced, &callback);
    trace("  with sample allocation\n");
    test_async_reader_allocate(reader, advanced, &callback);
    trace("  with compressed sample\n");
    test_async_reader_compressed(reader, advanced, &callback);
    trace("  with compressed sample allocation\n");
    test_async_reader_allocate_compressed(reader, advanced, &callback);
    callback.early_data_delivery[0] = 0;

    callback.early_data_delivery[1] = 500;
    trace("Checking EarlyDataDelivery[1] = 500.\n");
    trace("  with stream selection\n");
    test_async_reader_selection(reader, advanced, &callback);
    trace("  with sample allocation\n");
    test_async_reader_allocate(reader, advanced, &callback);
    trace("  with compressed sample\n");
    test_async_reader_compressed(reader, advanced, &callback);
    trace("  with compressed sample allocation\n");
    test_async_reader_allocate_compressed(reader, advanced, &callback);
    callback.early_data_delivery[1] = 0;

    hr = IWMReader_Close(reader);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(callback.closed_count == 1, "Got %u WMT_CLOSED callbacks.\n", callback.closed_count);
    ok(callback.refcount == 1, "Got outstanding refcount %ld.\n", callback.refcount);
    ret = WaitForSingleObject(callback.got_stopped, 0);
    ok(ret == WAIT_TIMEOUT, "Got unexpected WMT_STOPPED.\n");
    callback_cleanup(&callback);

    hr = IWMReader_Stop(reader);
    ok(hr == E_UNEXPECTED, "Got hr %#lx.\n", hr);

    ok(stream.refcount == 1, "Got outstanding refcount %ld.\n", stream.refcount);
    CloseHandle(stream.file);
    ret = DeleteFileW(filename);
    ok(ret, "Failed to delete %s, error %lu.\n", debugstr_w(filename), GetLastError());

    hr = IWMReader_Close(reader);
    ok(hr == NS_E_INVALID_REQUEST, "Got hr %#lx.\n", hr);

    IWMReaderAdvanced2_Release(advanced);
    IWMProfile_Release(profile);
    ref = IWMReader_Release(reader);
    ok(!ref, "Got outstanding refcount %ld.\n", ref);
}

static void test_async_reader_types(void)
{
    char mt_buffer[2000], mt2_buffer[2000];
    const WCHAR *filename = load_resource(L"test.wmv");
    IWMOutputMediaProps *output_props, *output_props2;
    WM_MEDIA_TYPE *mt2 = (WM_MEDIA_TYPE *)mt2_buffer;
    WM_MEDIA_TYPE *mt = (WM_MEDIA_TYPE *)mt_buffer;
    bool got_video = false, got_audio = false;
    DWORD size, ret_size, output_number;
    IWMReaderAdvanced2 *advanced;
    GUID majortype, majortype2;
    struct teststream stream;
    struct callback callback;
    IWMStreamConfig *config;
    ULONG count, ref, i, j;
    IWMProfile *profile;
    IWMReader *reader;
    HANDLE file;
    HRESULT hr;
    BOOL ret;

    file = CreateFileW(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, 0);
    ok(file != INVALID_HANDLE_VALUE, "Failed to open %s, error %lu.\n", debugstr_w(filename), GetLastError());

    teststream_init(&stream, file);
    callback_init(&callback, &stream);

    hr = winegstreamer_create_wm_async_reader(&reader);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    IWMReader_QueryInterface(reader, &IID_IWMProfile, (void **)&profile);
    IWMReader_QueryInterface(reader, &IID_IWMReaderAdvanced2, (void **)&advanced);

    hr = IWMReaderAdvanced2_OpenStream(advanced, &stream.IStream_iface, &callback.IWMReaderCallback_iface, (void **)0xdeadbeef);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(stream.refcount > 1, "Got refcount %ld.\n", stream.refcount);
    ok(callback.refcount > 1, "Got refcount %ld.\n", callback.refcount);
    wait_opened_callback(&callback);

    for (i = 0; i < 2; ++i)
    {
        winetest_push_context("Stream %lu", i);

        hr = IWMProfile_GetStream(profile, i, &config);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);

        hr = IWMStreamConfig_GetStreamType(config, &majortype);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        if (i)
        {
            todo_wine
            ok(IsEqualGUID(&majortype, &MEDIATYPE_Video), "Got major type %s.\n", debugstr_guid(&majortype));
        }
        else
        {
            todo_wine
            ok(IsEqualGUID(&majortype, &MEDIATYPE_Audio), "Got major type %s.\n", debugstr_guid(&majortype));
        }

        if (IsEqualGUID(&majortype, &MEDIATYPE_Audio))
            test_stream_media_props(config, &MEDIATYPE_Audio, &MEDIASUBTYPE_MSAUDIO1, &FORMAT_WaveFormatEx, TRUE);
        else
            test_stream_media_props(config, &MEDIATYPE_Video, &MEDIASUBTYPE_WMV1, &FORMAT_VideoInfo, FALSE);

        ref = IWMStreamConfig_Release(config);
        ok(!ref, "Got outstanding refcount %ld.\n", ref);

        winetest_pop_context();
    }

    for (i = 0; i < 2; ++i)
    {
        winetest_push_context("Output %lu", i);
        output_number = i;

        hr = IWMReader_GetOutputProps(reader, output_number, &output_props);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);

        ret_size = sizeof(mt_buffer);
        hr = IWMOutputMediaProps_GetMediaType(output_props, mt, &ret_size);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        majortype = mt->majortype;

        memset(&majortype2, 0xcc, sizeof(majortype2));
        hr = IWMOutputMediaProps_GetType(output_props, &majortype2);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        ok(IsEqualGUID(&majortype2, &majortype), "Expected major type %s, got %s.\n",
                debugstr_guid(&majortype), debugstr_guid(&majortype2));

        hr = IWMOutputMediaProps_SetMediaType(output_props, NULL);
        ok(hr == E_POINTER, "Got hr %#lx.\n", hr);

        memset(mt2_buffer, 0, sizeof(mt2_buffer));
        hr = IWMOutputMediaProps_SetMediaType(output_props, mt2);
        ok(hr == E_FAIL, "Got hr %#lx.\n", hr);

        if (IsEqualGUID(&majortype, &MEDIATYPE_Audio))
        {
            WAVEFORMATEX *format = (WAVEFORMATEX *)mt->pbFormat;

            init_audio_type(mt2, &MEDIASUBTYPE_IEEE_FLOAT, 32, format->nChannels * 2, format->nSamplesPerSec);
            hr = IWMOutputMediaProps_SetMediaType(output_props, mt2);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            hr = IWMReader_SetOutputProps(reader, output_number, output_props);
            todo_wine
            ok(hr == NS_E_AUDIO_CODEC_NOT_INSTALLED, "Got hr %#lx.\n", hr);

            init_audio_type(mt2, &MEDIASUBTYPE_PCM, 8, 1, 11025);
            hr = IWMOutputMediaProps_SetMediaType(output_props, mt2);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            hr = IWMReader_SetOutputProps(reader, output_number, output_props);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);

            init_audio_type(mt2, &MEDIASUBTYPE_IEEE_FLOAT, 32, format->nChannels, format->nSamplesPerSec / 4);
            hr = IWMOutputMediaProps_SetMediaType(output_props, mt2);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            hr = IWMReader_SetOutputProps(reader, output_number, output_props);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
        }
        else
        {
            VIDEOINFO *info = (VIDEOINFO *)mt->pbFormat;
            RECT rect = info->rcTarget;

            init_video_type(mt2, &MEDIASUBTYPE_RGB32, 32, BI_RGB, &rect);
            hr = IWMOutputMediaProps_SetMediaType(output_props, mt2);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            hr = IWMReader_SetOutputProps(reader, output_number, output_props);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);

            init_video_type(mt2, &MEDIASUBTYPE_NV12, 12, MAKEFOURCC('N','V','1','2'), &rect);
            hr = IWMOutputMediaProps_SetMediaType(output_props, mt2);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            hr = IWMReader_SetOutputProps(reader, output_number, output_props);
            todo_wine
            ok(hr == NS_E_INVALID_OUTPUT_FORMAT, "Got hr %#lx.\n", hr);

            InflateRect(&rect, 10, 10);

            init_video_type(mt2, &MEDIASUBTYPE_RGB32, 32, BI_RGB, &rect);
            hr = IWMOutputMediaProps_SetMediaType(output_props, mt2);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            hr = IWMReader_SetOutputProps(reader, output_number, output_props);
            ok(hr == NS_E_INVALID_OUTPUT_FORMAT, "Got hr %#lx.\n", hr);
        }

        hr = IWMOutputMediaProps_SetMediaType(output_props, mt);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        hr = IWMReader_SetOutputProps(reader, output_number, output_props);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);

        ref = IWMOutputMediaProps_Release(output_props);
        ok(!ref, "Got outstanding refcount %ld.\n", ref);

        if (IsEqualGUID(&majortype, &MEDIATYPE_Audio))
        {
            got_audio = true;
            check_audio_type(mt);

            /* R.U.S.E. enumerates all audio formats, picks the first one it
             * likes, and then sets the wrong stream to that format.
             * Accordingly we need the first audio format to be the default
             * format, and we need it to be a format that the game is happy
             * with. In particular it has to be PCM. */

            hr = IWMReader_GetOutputFormat(reader, output_number, 0, &output_props);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);

            ret_size = sizeof(mt2_buffer);
            hr = IWMOutputMediaProps_GetMediaType(output_props, mt2, &ret_size);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);

            ref = IWMOutputMediaProps_Release(output_props);
            ok(!ref, "Got outstanding refcount %ld.\n", ref);

            /* The sample size might differ. */
            mt2->lSampleSize = mt->lSampleSize;
            ok(compare_media_types(mt, mt2), "Media types didn't match.\n");
        }
        else
        {
            ok(IsEqualGUID(&majortype, &MEDIATYPE_Video), "Got major type %s.\n", debugstr_guid(&majortype));
            /* Shadowgrounds assumes that the initial video type will be RGB24. */
            ok(IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_RGB24), "Got subtype %s.\n", debugstr_guid(&mt->subtype));
            got_video = true;
            check_video_type(mt);
        }

        count = 0;
        hr = IWMReader_GetOutputFormatCount(reader, output_number, &count);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        ok(count > 0, "Got count %lu.\n", count);

        for (j = 0; j < count; ++j)
        {
            winetest_push_context("Format %lu", j);

            hr = IWMReader_GetOutputFormat(reader, output_number, j, &output_props);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);

            hr = IWMReader_GetOutputFormat(reader, output_number, j, &output_props2);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            ok(output_props2 != output_props, "Expected different objects.\n");
            ref = IWMOutputMediaProps_Release(output_props2);
            ok(!ref, "Got outstanding refcount %ld.\n", ref);

            size = 0xdeadbeef;
            hr = IWMOutputMediaProps_GetMediaType(output_props, NULL, &size);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            ok(size != 0xdeadbeef && size >= sizeof(WM_MEDIA_TYPE), "Got size %lu.\n", size);

            ret_size = size - 1;
            hr = IWMOutputMediaProps_GetMediaType(output_props, mt, &ret_size);
            ok(hr == ASF_E_BUFFERTOOSMALL, "Got hr %#lx.\n", hr);
            ok(ret_size == size, "Expected size %lu, got %lu.\n", size, ret_size);

            ret_size = sizeof(mt_buffer);
            memset(mt_buffer, 0xcc, sizeof(mt_buffer));
            hr = IWMOutputMediaProps_GetMediaType(output_props, mt, &ret_size);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            ok(ret_size == size, "Expected size %lu, got %lu.\n", size, ret_size);
            ok(size == sizeof(WM_MEDIA_TYPE) + mt->cbFormat, "Expected size %Iu, got %lu.\n",
                    sizeof(WM_MEDIA_TYPE) + mt->cbFormat, size);

            ok(IsEqualGUID(&mt->majortype, &majortype), "Got major type %s.\n", debugstr_guid(&mt->majortype));

            if (IsEqualGUID(&mt->majortype, &MEDIATYPE_Audio))
            {
                ok(IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_PCM), "Got subtype %s.\n", debugstr_guid(&mt->subtype));
                check_audio_type(mt);
            }
            else
            {
                ok(!IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_AYUV), "Got subtype %s.\n", debugstr_guid(&mt->subtype));
                check_video_type(mt);
            }

            memset(&majortype2, 0xcc, sizeof(majortype2));
            hr = IWMOutputMediaProps_GetType(output_props, &majortype2);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            ok(IsEqualGUID(&majortype2, &majortype), "Expected major type %s, got %s.\n",
                    debugstr_guid(&majortype), debugstr_guid(&majortype2));

            hr = IWMReader_SetOutputProps(reader, output_number, output_props);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            hr = IWMReader_SetOutputProps(reader, 1 - output_number, output_props);
            if (!i)
                ok(hr == NS_E_INCOMPATIBLE_FORMAT /* win < 8, win10 1507-1809 */
                        || hr == ASF_E_BADMEDIATYPE /* win8, win10 1909+ */, "Got hr %#lx.\n", hr);
            else
                todo_wine ok(hr == NS_E_INVALID_REQUEST, "Got hr %#lx.\n", hr);
            hr = IWMReader_SetOutputProps(reader, 2, output_props);
            ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);

            hr = IWMReader_GetOutputProps(reader, output_number, &output_props2);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            ok(output_props2 != output_props, "Expected different objects.\n");

            ret_size = sizeof(mt2_buffer);
            hr = IWMOutputMediaProps_GetMediaType(output_props2, mt2, &ret_size);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            ok(compare_media_types(mt, mt2), "Media types didn't match.\n");

            ref = IWMOutputMediaProps_Release(output_props2);
            ok(!ref, "Got outstanding refcount %ld.\n", ref);
            ref = IWMOutputMediaProps_Release(output_props);
            ok(!ref, "Got outstanding refcount %ld.\n", ref);

            winetest_pop_context();
        }

        hr = IWMReader_GetOutputFormat(reader, output_number, count, &output_props);
        ok(hr == NS_E_INVALID_OUTPUT_FORMAT, "Got hr %#lx.\n", hr);

        hr = IWMReader_GetOutputProps(reader, output_number, &output_props);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);

        hr = IWMReader_GetOutputProps(reader, output_number, &output_props2);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        ok(output_props2 != output_props, "Expected different objects.\n");

        ref = IWMOutputMediaProps_Release(output_props2);
        ok(!ref, "Got outstanding refcount %ld.\n", ref);
        ref = IWMOutputMediaProps_Release(output_props);
        ok(!ref, "Got outstanding refcount %ld.\n", ref);

        winetest_pop_context();
    }

    ok(got_audio, "No audio stream was enumerated.\n");
    ok(got_video, "No video stream was enumerated.\n");

    count = 0xdeadbeef;
    hr = IWMReader_GetOutputFormatCount(reader, 2, &count);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);
    ok(count == 0xdeadbeef, "Got count %#lx.\n", count);

    output_props = (void *)0xdeadbeef;
    hr = IWMReader_GetOutputProps(reader, 2, &output_props);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);
    ok(output_props == (void *)0xdeadbeef, "Got output props %p.\n", output_props);

    output_props = (void *)0xdeadbeef;
    hr = IWMReader_GetOutputFormat(reader, 2, 0, &output_props);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);
    ok(output_props == (void *)0xdeadbeef, "Got output props %p.\n", output_props);

    IWMReaderAdvanced2_Release(advanced);
    IWMProfile_Release(profile);
    ref = IWMReader_Release(reader);
    ok(!ref, "Got outstanding refcount %ld.\n", ref);

    ok(stream.refcount == 1, "Got outstanding refcount %ld.\n", stream.refcount);
    CloseHandle(stream.file);
    ret = DeleteFileW(filename);
    ok(ret, "Failed to delete %s, error %lu.\n", debugstr_w(filename), GetLastError());
}

static void test_async_reader_file(void)
{
    const WCHAR *filename = load_resource(L"test.wmv");
    struct callback callback;
    IWMReader *reader;
    DWORD count;
    HRESULT hr;
    ULONG ref;
    BOOL ret;

    callback_init(&callback, NULL);

    hr = winegstreamer_create_wm_async_reader(&reader);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IWMReader_Open(reader, filename, &callback.IWMReaderCallback_iface, (void **)0xdeadbeef);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(callback.refcount > 1, "Got refcount %ld.\n", callback.refcount);
    wait_opened_callback(&callback);

    hr = IWMReader_Open(reader, filename, &callback.IWMReaderCallback_iface, (void **)0xdeadbee0);
    ok(hr == E_UNEXPECTED, "Got hr %#lx.\n", hr);

    count = 0xdeadbeef;
    hr = IWMReader_GetOutputCount(reader, &count);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(count == 2, "Got count %lu.\n", count);

    callback.expect_context = (void *)0xfacade;
    hr = IWMReader_Start(reader, 0, 0, 1.0f, (void *)0xfacade);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    wait_started_callback(&callback);

    hr = IWMReader_Close(reader);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(callback.closed_count == 1, "Got %u WMT_CLOSED callbacks.\n", callback.closed_count);
    ok(callback.refcount == 1, "Got outstanding refcount %ld.\n", callback.refcount);
    ret = WaitForSingleObject(callback.got_stopped, 0);
    ok(ret == WAIT_TIMEOUT, "Got unexpected WMT_STOPPED.\n");
    callback_cleanup(&callback);

    hr = IWMReader_Close(reader);
    ok(hr == NS_E_INVALID_REQUEST, "Got hr %#lx.\n", hr);

    ref = IWMReader_Release(reader);
    ok(!ref, "Got outstanding refcount %ld.\n", ref);
    ok(callback.closed_count == 1, "Got %u WMT_CLOSED callbacks.\n", callback.closed_count);
    ok(callback.refcount == 1, "Got outstanding refcount %ld.\n", callback.refcount);
    callback_cleanup(&callback);

    ret = DeleteFileW(filename);
    ok(ret, "Failed to delete %s, error %lu.\n", debugstr_w(filename), GetLastError());
}

static void test_sync_reader_allocator(void)
{
    const WCHAR *filename = load_resource(L"test.wmv");
    IWMReaderAllocatorEx *allocator;
    struct teststream stream;
    struct callback callback;
    DWORD output_num, flags;
    IWMSyncReader2 *reader;
    QWORD pts, duration;
    INSSBuffer *sample;
    WORD stream_num;
    HANDLE file;
    HRESULT hr;
    BOOL ret;

    hr = WMCreateSyncReader(NULL, 0, (IWMSyncReader **)&reader);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    file = CreateFileW(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, 0);
    ok(file != INVALID_HANDLE_VALUE, "Failed to open %s, error %lu.\n", debugstr_w(file), GetLastError());

    teststream_init(&stream, file);
    callback_init(&callback, &stream);

    hr = IWMSyncReader2_OpenStream(reader, &stream.IStream_iface);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(stream.refcount > 1, "Got refcount %ld.\n", stream.refcount);


    hr = IWMSyncReader2_GetAllocateForOutput(reader, -1, &allocator);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);
    hr = IWMSyncReader2_GetAllocateForStream(reader, 0, &allocator);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);
    hr = IWMSyncReader2_GetAllocateForOutput(reader, 0, NULL);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);
    hr = IWMSyncReader2_GetAllocateForStream(reader, 1, NULL);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);
    hr = IWMSyncReader2_SetAllocateForOutput(reader, -1, &callback.IWMReaderAllocatorEx_iface);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);
    hr = IWMSyncReader2_SetAllocateForStream(reader, 0, &callback.IWMReaderAllocatorEx_iface);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);


    allocator = (void *)0xdeadbeef;
    hr = IWMSyncReader2_GetAllocateForOutput(reader, 0, &allocator);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(!allocator, "Got allocator %p.\n", allocator);
    allocator = (void *)0xdeadbeef;
    hr = IWMSyncReader2_GetAllocateForStream(reader, 1, &allocator);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(!allocator, "Got allocator %p.\n", allocator);


    hr = IWMSyncReader2_SetAllocateForStream(reader, 1, &callback.IWMReaderAllocatorEx_iface);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    allocator = (void *)0xdeadbeef;
    hr = IWMSyncReader2_GetAllocateForOutput(reader, 0, &allocator);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(!allocator, "Got allocator %p.\n", allocator);
    allocator = (void *)0xdeadbeef;
    hr = IWMSyncReader2_GetAllocateForStream(reader, 2, &allocator);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(!allocator, "Got allocator %p.\n", allocator);
    hr = IWMSyncReader2_GetAllocateForStream(reader, 1, &allocator);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(allocator == &callback.IWMReaderAllocatorEx_iface, "Got allocator %p.\n", allocator);

    hr = IWMSyncReader2_SetAllocateForStream(reader, 1, NULL);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    allocator = (void *)0xdeadbeef;
    hr = IWMSyncReader2_GetAllocateForOutput(reader, 0, &allocator);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(!allocator, "Got allocator %p.\n", allocator);
    allocator = (void *)0xdeadbeef;
    hr = IWMSyncReader2_GetAllocateForStream(reader, 1, &allocator);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(!allocator, "Got allocator %p.\n", allocator);


    hr = IWMSyncReader2_SetAllocateForOutput(reader, 0, &callback.IWMReaderAllocatorEx_iface);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    allocator = (void *)0xdeadbeef;
    hr = IWMSyncReader2_GetAllocateForOutput(reader, 1, &allocator);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(!allocator, "Got allocator %p.\n", allocator);
    allocator = (void *)0xdeadbeef;
    hr = IWMSyncReader2_GetAllocateForStream(reader, 1, &allocator);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(!allocator, "Got allocator %p.\n", allocator);
    hr = IWMSyncReader2_GetAllocateForOutput(reader, 0, &allocator);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(allocator == &callback.IWMReaderAllocatorEx_iface, "Got allocator %p.\n", allocator);

    hr = IWMSyncReader2_SetAllocateForOutput(reader, 0, NULL);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    allocator = (void *)0xdeadbeef;
    hr = IWMSyncReader2_GetAllocateForOutput(reader, 0, &allocator);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(!allocator, "Got allocator %p.\n", allocator);
    allocator = (void *)0xdeadbeef;
    hr = IWMSyncReader2_GetAllocateForStream(reader, 1, &allocator);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(!allocator, "Got allocator %p.\n", allocator);


    hr = IWMSyncReader2_GetStreamNumberForOutput(reader, 0, &stream_num);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMSyncReader2_SetAllocateForStream(reader, stream_num, &callback.IWMReaderAllocatorEx_iface);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMSyncReader2_SetReadStreamSamples(reader, stream_num, TRUE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMSyncReader2_SetAllocateForOutput(reader, 1, &callback.IWMReaderAllocatorEx_iface);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    callback.allocated_samples = true;
    callback.allocated_compressed_samples = true;

    hr = IWMSyncReader2_GetStreamNumberForOutput(reader, 0, &stream_num);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMSyncReader2_GetNextSample(reader, stream_num, &sample, &pts, &duration, &flags,
            &output_num, &stream_num);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(sample->lpVtbl == &buffer_vtbl, "Buffer vtbl didn't match.\n");
    INSSBuffer_Release(sample);

    hr = IWMSyncReader2_GetStreamNumberForOutput(reader, 1, &stream_num);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMSyncReader2_GetNextSample(reader, stream_num, &sample, &pts, &duration, &flags,
            &output_num, &stream_num);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(sample->lpVtbl == &buffer_vtbl, "Buffer vtbl didn't match.\n");
    INSSBuffer_Release(sample);

    callback.allocated_samples = false;
    callback.allocated_compressed_samples = false;

    /* without compressed sample read, allocator isn't used */
    hr = IWMSyncReader2_GetStreamNumberForOutput(reader, 0, &stream_num);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMSyncReader2_SetReadStreamSamples(reader, stream_num, FALSE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMSyncReader2_GetNextSample(reader, stream_num, &sample, &pts, &duration, &flags,
            &output_num, &stream_num);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(sample->lpVtbl != &buffer_vtbl, "Unexpected buffer vtbl.\n");
    INSSBuffer_Release(sample);


    /* cannot change or remove allocators after they've been used */
    hr = IWMSyncReader2_GetStreamNumberForOutput(reader, 0, &stream_num);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMSyncReader2_SetAllocateForStream(reader, stream_num, NULL);
    todo_wine
    ok(hr == NS_E_INVALID_REQUEST, "Got hr %#lx.\n", hr);
    hr = IWMSyncReader2_SetAllocateForOutput(reader, 0, &callback.IWMReaderAllocatorEx_iface);
    todo_wine
    ok(hr == NS_E_INVALID_REQUEST, "Got hr %#lx.\n", hr);
    hr = IWMSyncReader2_SetAllocateForOutput(reader, 1, NULL);
    todo_wine
    ok(hr == NS_E_INVALID_REQUEST, "Got hr %#lx.\n", hr);
    hr = IWMSyncReader2_GetStreamNumberForOutput(reader, 0, &stream_num);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMSyncReader2_SetReadStreamSamples(reader, stream_num, TRUE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    callback.allocated_samples = true;
    callback.allocated_compressed_samples = true;

    hr = IWMSyncReader2_GetStreamNumberForOutput(reader, 1, &stream_num);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMSyncReader2_GetNextSample(reader, stream_num, &sample, &pts, &duration, &flags,
            &output_num, &stream_num);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    todo_wine
    ok(sample->lpVtbl == &buffer_vtbl, "Buffer vtbl didn't match.\n");
    INSSBuffer_Release(sample);

    hr = IWMSyncReader2_GetStreamNumberForOutput(reader, 0, &stream_num);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMSyncReader2_GetNextSample(reader, stream_num, &sample, &pts, &duration, &flags,
            &output_num, &stream_num);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    todo_wine
    ok(sample->lpVtbl == &buffer_vtbl, "Unexpected buffer vtbl.\n");
    INSSBuffer_Release(sample);

    callback.allocated_samples = false;
    callback.allocated_compressed_samples = false;

    IWMSyncReader2_Release(reader);

    ok(stream.refcount == 1, "Got outstanding refcount %ld.\n", stream.refcount);
    CloseHandle(stream.file);
    ret = DeleteFileW(filename);
    ok(ret, "Failed to delete %s, error %lu.\n", debugstr_w(filename), GetLastError());

    callback_cleanup(&callback);
}

static void test_async_reader_clock(void)
{
    const WCHAR *filename = load_resource(L"test.wmv");
    IWMReaderAdvanced2 *advanced;
    REFERENCE_TIME start_time;
    struct callback callback;
    IReferenceClock *clock;
    IWMReader *reader;
    DWORD_PTR cookie;
    ULONG ref, ret;
    HANDLE event;
    HRESULT hr;
    BOOL res;

    callback_init(&callback);

    event = CreateEventW(NULL, FALSE, FALSE, NULL);
    ok(!!event, "CreateEventW failed, error %lu\n", GetLastError());

    hr = WMCreateReader(NULL, 0, &reader);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IWMReader_QueryInterface(reader, &IID_IWMReaderAdvanced2, (void **)&advanced);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMReader_QueryInterface(reader, &IID_IReferenceClock, (void **)&clock);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IReferenceClock_GetTime(clock, &start_time);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(start_time, "Got time %I64d.\n", start_time);

    hr = IReferenceClock_AdviseTime(clock, 0, 0, (HEVENT)event, &cookie);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(!!cookie, "Got cookie %#Ix.\n", cookie);
    ret = WaitForSingleObject(event, 100);
    ok(!ret, "WaitForSingleObject returned %#lx.\n", ret);
    hr = IReferenceClock_Unadvise(clock, cookie);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IReferenceClock_AdviseTime(clock, start_time, 1000 * 10000, (HEVENT)event, &cookie);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(!!cookie, "Got cookie %#Ix.\n", cookie);
    ret = WaitForSingleObject(event, 100);
    ok(ret == WAIT_TIMEOUT, "WaitForSingleObject returned %#lx.\n", ret);
    hr = IReferenceClock_Unadvise(clock, cookie);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IWMReaderAdvanced2_SetUserProvidedClock(advanced, TRUE);
    ok(hr == NS_E_INVALID_REQUEST, "Got hr %#lx.\n", hr);
    hr = IWMReaderAdvanced2_DeliverTime(advanced, 1000 * 10000);
    ok(hr == E_UNEXPECTED, "Got hr %#lx.\n", hr);

    hr = IWMReader_Open(reader, filename, &callback.IWMReaderCallback_iface, (void **)0xdeadbeef);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(callback.refcount > 1, "Got refcount %ld.\n", callback.refcount);
    wait_opened_callback(&callback);

    hr = IWMReaderAdvanced2_SetUserProvidedClock(advanced, TRUE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMReaderAdvanced2_DeliverTime(advanced, 0);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IReferenceClock_GetTime(clock, &start_time);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(start_time, "Got time %I64d.\n", start_time);

    hr = IReferenceClock_AdviseTime(clock, start_time, 10000, (HEVENT)event, &cookie);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(!!cookie, "Got cookie %#Ix.\n", cookie);
    ret = WaitForSingleObject(event, 100);
    ok(!ret, "WaitForSingleObject returned %#lx.\n", ret);
    hr = IReferenceClock_Unadvise(clock, cookie);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IWMReader_Start(reader, 0, 0, 1.0f, (void *)0xfacade);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    wait_started_callback(&callback);

    hr = IWMReaderAdvanced2_SetUserProvidedClock(advanced, FALSE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IWMReaderAdvanced2_DeliverTime(advanced, 1000 * 10000);
    ok(hr == E_UNEXPECTED, "Got hr %#lx.\n", hr);

    hr = IWMReader_Close(reader);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(callback.closed_count == 1, "Got %u WMT_CLOSED callbacks.\n", callback.closed_count);
    ok(callback.refcount == 1, "Got outstanding refcount %ld.\n", callback.refcount);
    ret = WaitForSingleObject(callback.got_stopped, 0);
    ok(ret == WAIT_TIMEOUT, "Got unexpected WMT_STOPPED.\n");
    callback_cleanup(&callback);

    IWMReaderAdvanced2_Release(advanced);
    IReferenceClock_Release(clock);
    CloseHandle(event);

    ref = IWMReader_Release(reader);
    ok(!ref, "Got outstanding refcount %ld.\n", ref);

    callback_cleanup(&callback);

    res = DeleteFileW(filename);
    ok(res, "Failed to delete %s, error %lu.\n", debugstr_w(filename), GetLastError());
}

START_TEST(wmvcore)
{
    HRESULT hr;

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    ok(hr == S_OK, "failed to init com\n");
    if(hr != S_OK)
        return;

    test_wmreader_interfaces();
    test_wmsyncreader_interfaces();
    test_wmwriter_interfaces();
    test_profile_manager_interfaces();
    test_WMCreateWriterPriv();
    test_urlextension();
    test_iscontentprotected();
    test_sync_reader_allocator();
    test_sync_reader_settings();
    test_sync_reader_streaming();
    test_sync_reader_types();
    test_sync_reader_file();
    if (0) test_async_reader_settings();
    test_async_reader_streaming();
    test_async_reader_types();
    test_async_reader_file();
    if (0) test_async_reader_clock();

    /* Keep last so that test.wmv doesn't get overwritten */
    test_wmwriter();

    CoUninitialize();
}
