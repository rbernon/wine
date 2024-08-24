/*
 * Copyright 2023 Ziqing Hui for CodeWeavers
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
#include "mfsrcsnk_private.h"

#include "wine/list.h"
#include "wine/debug.h"
#include "wine/winedmo.h"

WINE_DEFAULT_DEBUG_CHANNEL(mfplat);

#define DEFINE_MF_ASYNC_CALLBACK_(pfx, impl_type, impl_from, iface_mem, expr)                       \
    static inline impl_type *impl_from(IMFAsyncCallback *iface)                                     \
    {                                                                                               \
        return CONTAINING_RECORD(iface, impl_type, iface_mem);                                      \
    }                                                                                               \
    static HRESULT WINAPI pfx##_QueryInterface(IMFAsyncCallback *iface, REFIID iid, void **out)     \
    {                                                                                               \
        if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_IMFAsyncCallback))               \
        {                                                                                           \
            IMFAsyncCallback_AddRef((*out = iface));                                                \
            return S_OK;                                                                            \
        }                                                                                           \
        *out = NULL;                                                                                \
        return E_NOINTERFACE;                                                                       \
    }                                                                                               \
    static ULONG WINAPI pfx##_AddRef(IMFAsyncCallback *iface)                                       \
    {                                                                                               \
        impl_type *impl = impl_from(iface);                                                         \
        return IUnknown_AddRef((IUnknown *)(expr));                                                 \
    }                                                                                               \
    static ULONG WINAPI pfx##_Release(IMFAsyncCallback *iface)                                      \
    {                                                                                               \
        impl_type *impl = impl_from(iface);                                                         \
        return IUnknown_Release((IUnknown *)(expr));                                                \
    }                                                                                               \
    static HRESULT WINAPI pfx##_GetParameters(IMFAsyncCallback *iface, DWORD *flags, DWORD *queue)  \
    {                                                                                               \
        return E_NOTIMPL;                                                                           \
    }                                                                                               \
    static HRESULT WINAPI pfx##_Invoke(IMFAsyncCallback *iface, IMFAsyncResult *result);            \
    static const IMFAsyncCallbackVtbl pfx##_vtbl =                                                  \
    {                                                                                               \
        pfx##_QueryInterface,                                                                       \
        pfx##_AddRef,                                                                               \
        pfx##_Release,                                                                              \
        pfx##_GetParameters,                                                                        \
        pfx##_Invoke,                                                                               \
    };                                                                                              \

#define DEFINE_MF_ASYNC_CALLBACK(name, impl_type, base_iface) \
    DEFINE_MF_ASYNC_CALLBACK_(name, impl_type, impl_from_##name, name##_iface, &impl->base_iface)

struct async_process_params
{
    IUnknown IUnknown_iface;
    LONG refcount;

    IMFSample *sample;
    UINT32 stream_id;
};

struct stream_sink
{
    IMFStreamSink IMFStreamSink_iface;
    IMFMediaTypeHandler IMFMediaTypeHandler_iface;
    LONG refcount;
    DWORD id;

    IMFMediaType *type;
    IMFFinalizableMediaSink *media_sink;
    IMFMediaEventQueue *event_queue;

    struct list entry;
};

struct media_sink
{
    IMFFinalizableMediaSink IMFFinalizableMediaSink_iface;
    IMFMediaEventGenerator IMFMediaEventGenerator_iface;
    IMFClockStateSink IMFClockStateSink_iface;
    IMFAsyncCallback async_start_iface;
    IMFAsyncCallback async_stop_iface;
    IMFAsyncCallback async_pause_iface;
    IMFAsyncCallback async_process_iface;
    IMFAsyncCallback async_finalize_iface;
    LONG refcount;
    CRITICAL_SECTION cs;

    enum
    {
        STATE_OPENED,
        STATE_STARTED,
        STATE_STOPPED,
        STATE_PAUSED,
        STATE_FINALIZED,
        STATE_SHUTDOWN,
    } state;

    IMFByteStream *stream;
    IMFMediaEventQueue *event_queue;

    struct list stream_sinks;

    struct winedmo_stream winedmo_stream;
    struct winedmo_muxer winedmo_muxer;
};

static struct stream_sink *impl_from_IMFStreamSink(IMFStreamSink *iface)
{
    return CONTAINING_RECORD(iface, struct stream_sink, IMFStreamSink_iface);
}

static struct stream_sink *impl_from_IMFMediaTypeHandler(IMFMediaTypeHandler *iface)
{
    return CONTAINING_RECORD(iface, struct stream_sink, IMFMediaTypeHandler_iface);
}

static struct media_sink *impl_from_IMFFinalizableMediaSink(IMFFinalizableMediaSink *iface)
{
    return CONTAINING_RECORD(iface, struct media_sink, IMFFinalizableMediaSink_iface);
}

static struct media_sink *impl_from_IMFMediaEventGenerator(IMFMediaEventGenerator *iface)
{
    return CONTAINING_RECORD(iface, struct media_sink, IMFMediaEventGenerator_iface);
}

static struct media_sink *impl_from_IMFClockStateSink(IMFClockStateSink *iface)
{
    return CONTAINING_RECORD(iface, struct media_sink, IMFClockStateSink_iface);
}

static struct async_process_params *async_process_params_from_IUnknown(IUnknown *iface)
{
    return CONTAINING_RECORD(iface, struct async_process_params, IUnknown_iface);
}

static HRESULT WINAPI async_process_params_QueryInterface(IUnknown *iface, REFIID riid, void **obj)
{
    struct async_process_params *params = async_process_params_from_IUnknown(iface);

    TRACE("iface %p, riid %s, obj %p.\n", iface, debugstr_guid(riid), obj);

    if (IsEqualIID(riid, &IID_IUnknown))
        *obj = &params->IUnknown_iface;
    else
    {
        WARN("Unsupported interface %s.\n", debugstr_guid(riid));
        *obj = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown *)*obj);
    return S_OK;
}

static ULONG WINAPI async_process_params_AddRef(IUnknown *iface)
{
    struct async_process_params *params = async_process_params_from_IUnknown(iface);
    ULONG refcount = InterlockedIncrement(&params->refcount);
    TRACE("iface %p, refcount %lu.\n", iface, refcount);
    return refcount;
}

static ULONG WINAPI async_process_params_Release(IUnknown *iface)
{
    struct async_process_params *params = async_process_params_from_IUnknown(iface);
    ULONG refcount = InterlockedDecrement(&params->refcount);

    TRACE("iface %p, refcount %lu.\n", iface, refcount);

    if (!refcount)
    {
        IMFSample_Release(params->sample);
        free(params);
    }

    return refcount;
}

static const IUnknownVtbl async_process_params_vtbl =
{
    async_process_params_QueryInterface,
    async_process_params_AddRef,
    async_process_params_Release,
};

static HRESULT async_process_params_create(UINT32 stream_id, IMFSample *sample, IUnknown **out)
{
    struct async_process_params *params;

    if (!(params = calloc(1, sizeof(*params))))
        return E_OUTOFMEMORY;

    params->IUnknown_iface.lpVtbl = &async_process_params_vtbl;
    params->refcount = 1;
    params->stream_id = stream_id;
    IMFSample_AddRef((params->sample = sample));

    TRACE("Created async params %p.\n", params);
    *out = &params->IUnknown_iface;

    return S_OK;
}

static HRESULT WINAPI stream_sink_QueryInterface(IMFStreamSink *iface, REFIID riid, void **obj)
{
    struct stream_sink *stream_sink = impl_from_IMFStreamSink(iface);

    TRACE("iface %p, riid %s, obj %p.\n", iface, debugstr_guid(riid), obj);

    if (IsEqualIID(riid, &IID_IMFStreamSink) ||
            IsEqualGUID(riid, &IID_IMFMediaEventGenerator) ||
            IsEqualIID(riid, &IID_IUnknown))
    {
        *obj = &stream_sink->IMFStreamSink_iface;
    }
    else if (IsEqualIID(riid, &IID_IMFMediaTypeHandler))
    {
        *obj = &stream_sink->IMFMediaTypeHandler_iface;
    }
    else
    {
        WARN("Unsupported interface %s.\n", debugstr_guid(riid));
        *obj = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown *)*obj);

    return S_OK;
}

static ULONG WINAPI stream_sink_AddRef(IMFStreamSink *iface)
{
    struct stream_sink *stream_sink = impl_from_IMFStreamSink(iface);
    ULONG refcount = InterlockedIncrement(&stream_sink->refcount);
    TRACE("iface %p, refcount %lu.\n", iface, refcount);
    return refcount;
}

static ULONG WINAPI stream_sink_Release(IMFStreamSink *iface)
{
    struct stream_sink *stream_sink = impl_from_IMFStreamSink(iface);
    ULONG refcount = InterlockedDecrement(&stream_sink->refcount);

    TRACE("iface %p, refcount %lu.\n", iface, refcount);

    if (!refcount)
    {
        IMFMediaEventQueue_Release(stream_sink->event_queue);
        IMFFinalizableMediaSink_Release(stream_sink->media_sink);
        if (stream_sink->type)
            IMFMediaType_Release(stream_sink->type);
        free(stream_sink);
    }

    return refcount;
}

static HRESULT WINAPI stream_sink_GetEvent(IMFStreamSink *iface, DWORD flags, IMFMediaEvent **event)
{
    struct stream_sink *stream_sink = impl_from_IMFStreamSink(iface);

    TRACE("iface %p, flags %#lx, event %p.\n", iface, flags, event);

    return IMFMediaEventQueue_GetEvent(stream_sink->event_queue, flags, event);
}

static HRESULT WINAPI stream_sink_BeginGetEvent(IMFStreamSink *iface, IMFAsyncCallback *callback,
        IUnknown *state)
{
    struct stream_sink *stream_sink = impl_from_IMFStreamSink(iface);

    TRACE("iface %p, callback %p, state %p.\n", iface, callback, state);

    return IMFMediaEventQueue_BeginGetEvent(stream_sink->event_queue, callback, state);
}

static HRESULT WINAPI stream_sink_EndGetEvent(IMFStreamSink *iface, IMFAsyncResult *result,
        IMFMediaEvent **event)
{
    struct stream_sink *stream_sink = impl_from_IMFStreamSink(iface);

    TRACE("iface %p, result %p, event %p.\n", iface, result, event);

    return IMFMediaEventQueue_EndGetEvent(stream_sink->event_queue, result, event);
}

static HRESULT WINAPI stream_sink_QueueEvent(IMFStreamSink *iface, MediaEventType event_type,
        REFGUID ext_type, HRESULT hr, const PROPVARIANT *value)
{
    struct stream_sink *stream_sink = impl_from_IMFStreamSink(iface);

    TRACE("iface %p, event_type %lu, ext_type %s, hr %#lx, value %p.\n",
            iface, event_type, debugstr_guid(ext_type), hr, value);

    return IMFMediaEventQueue_QueueEventParamVar(stream_sink->event_queue, event_type, ext_type, hr, value);
}

static HRESULT WINAPI stream_sink_GetMediaSink(IMFStreamSink *iface, IMFMediaSink **ret)
{
    struct stream_sink *stream_sink = impl_from_IMFStreamSink(iface);

    TRACE("iface %p, ret %p.\n", iface, ret);

    IMFMediaSink_AddRef((*ret = (IMFMediaSink *)stream_sink->media_sink));

    return S_OK;
}

static HRESULT WINAPI stream_sink_GetIdentifier(IMFStreamSink *iface, DWORD *identifier)
{
    struct stream_sink *stream_sink = impl_from_IMFStreamSink(iface);

    TRACE("iface %p, identifier %p.\n", iface, identifier);

    *identifier = stream_sink->id;

    return S_OK;
}

static HRESULT WINAPI stream_sink_GetMediaTypeHandler(IMFStreamSink *iface, IMFMediaTypeHandler **handler)
{
    struct stream_sink *stream_sink = impl_from_IMFStreamSink(iface);

    TRACE("iface %p, handler %p.\n", iface, handler);

    IMFMediaTypeHandler_AddRef((*handler = &stream_sink->IMFMediaTypeHandler_iface));

    return S_OK;
}

static HRESULT WINAPI stream_sink_ProcessSample(IMFStreamSink *iface, IMFSample *sample)
{
    struct stream_sink *stream_sink = impl_from_IMFStreamSink(iface);
    struct media_sink *media_sink = impl_from_IMFFinalizableMediaSink(stream_sink->media_sink);
    IUnknown *params;
    HRESULT hr;

    TRACE("iface %p, sample %p.\n", iface, sample);

    EnterCriticalSection(&media_sink->cs);

    if (media_sink->state == STATE_SHUTDOWN)
    {
        LeaveCriticalSection(&media_sink->cs);
        return MF_E_SHUTDOWN;
    }

    if (media_sink->state != STATE_STARTED && media_sink->state != STATE_PAUSED)
    {
        LeaveCriticalSection(&media_sink->cs);
        return MF_E_INVALIDREQUEST;
    }

    if (SUCCEEDED(hr = async_process_params_create(stream_sink->id, sample, &params)))
    {
        hr = MFPutWorkItem(MFASYNC_CALLBACK_QUEUE_STANDARD,
                &media_sink->async_process_iface, params);
        IUnknown_Release(params);
    }

    LeaveCriticalSection(&media_sink->cs);

    return hr;
}

static HRESULT WINAPI stream_sink_PlaceMarker(IMFStreamSink *iface, MFSTREAMSINK_MARKER_TYPE marker_type,
        const PROPVARIANT *marker_value, const PROPVARIANT *context_value)
{
    FIXME("iface %p, marker_type %d, marker_value %p, context_value %p stub!\n",
            iface, marker_type, marker_value, context_value);

    return E_NOTIMPL;
}

static HRESULT WINAPI stream_sink_Flush(IMFStreamSink *iface)
{
    FIXME("iface %p stub!\n", iface);

    return E_NOTIMPL;
}

static const IMFStreamSinkVtbl stream_sink_vtbl =
{
    stream_sink_QueryInterface,
    stream_sink_AddRef,
    stream_sink_Release,
    stream_sink_GetEvent,
    stream_sink_BeginGetEvent,
    stream_sink_EndGetEvent,
    stream_sink_QueueEvent,
    stream_sink_GetMediaSink,
    stream_sink_GetIdentifier,
    stream_sink_GetMediaTypeHandler,
    stream_sink_ProcessSample,
    stream_sink_PlaceMarker,
    stream_sink_Flush,
};

static HRESULT WINAPI stream_sink_type_handler_QueryInterface(IMFMediaTypeHandler *iface, REFIID riid, void **obj)
{
    struct stream_sink *stream_sink = impl_from_IMFMediaTypeHandler(iface);
    return IMFStreamSink_QueryInterface(&stream_sink->IMFStreamSink_iface, riid, obj);
}

static ULONG WINAPI stream_sink_type_handler_AddRef(IMFMediaTypeHandler *iface)
{
    struct stream_sink *stream_sink = impl_from_IMFMediaTypeHandler(iface);
    return IMFStreamSink_AddRef(&stream_sink->IMFStreamSink_iface);
}

static ULONG WINAPI stream_sink_type_handler_Release(IMFMediaTypeHandler *iface)
{
    struct stream_sink *stream_sink = impl_from_IMFMediaTypeHandler(iface);
    return IMFStreamSink_Release(&stream_sink->IMFStreamSink_iface);
}

static HRESULT WINAPI stream_sink_type_handler_IsMediaTypeSupported(IMFMediaTypeHandler *iface,
        IMFMediaType *in_type, IMFMediaType **out_type)
{
    FIXME("iface %p, in_type %p, out_type %p.\n", iface, in_type, out_type);

    return E_NOTIMPL;
}

static HRESULT WINAPI stream_sink_type_handler_GetMediaTypeCount(IMFMediaTypeHandler *iface, DWORD *count)
{
    FIXME("iface %p, count %p.\n", iface, count);

    return E_NOTIMPL;
}

static HRESULT WINAPI stream_sink_type_handler_GetMediaTypeByIndex(IMFMediaTypeHandler *iface, DWORD index,
        IMFMediaType **type)
{
    FIXME("iface %p, index %lu, type %p.\n", iface, index, type);

    return E_NOTIMPL;
}

static HRESULT WINAPI stream_sink_type_handler_SetCurrentMediaType(IMFMediaTypeHandler *iface, IMFMediaType *type)
{
    FIXME("iface %p, type %p.\n", iface, type);

    return E_NOTIMPL;
}

static HRESULT WINAPI stream_sink_type_handler_GetCurrentMediaType(IMFMediaTypeHandler *iface, IMFMediaType **type)
{
    struct stream_sink *stream_sink = impl_from_IMFMediaTypeHandler(iface);

    TRACE("iface %p, type %p.\n", iface, type);

    if (!type)
        return E_POINTER;
    if (!stream_sink->type)
        return MF_E_NOT_INITIALIZED;

    IMFMediaType_AddRef((*type = stream_sink->type));

    return S_OK;
}

static HRESULT WINAPI stream_sink_type_handler_GetMajorType(IMFMediaTypeHandler *iface, GUID *type)
{
    FIXME("iface %p, type %p.\n", iface, type);

    return E_NOTIMPL;
}

static const IMFMediaTypeHandlerVtbl stream_sink_type_handler_vtbl =
{
    stream_sink_type_handler_QueryInterface,
    stream_sink_type_handler_AddRef,
    stream_sink_type_handler_Release,
    stream_sink_type_handler_IsMediaTypeSupported,
    stream_sink_type_handler_GetMediaTypeCount,
    stream_sink_type_handler_GetMediaTypeByIndex,
    stream_sink_type_handler_SetCurrentMediaType,
    stream_sink_type_handler_GetCurrentMediaType,
    stream_sink_type_handler_GetMajorType,
};

static HRESULT stream_sink_create(DWORD stream_sink_id, IMFMediaType *media_type, struct media_sink *media_sink,
        struct stream_sink **out)
{
    struct stream_sink *stream_sink;
    HRESULT hr;

    TRACE("stream_sink_id %#lx, media_type %p, media_sink %p, out %p.\n",
            stream_sink_id, media_type, media_sink, out);

    if (!(stream_sink = calloc(1, sizeof(*stream_sink))))
        return E_OUTOFMEMORY;

    if (FAILED(hr = MFCreateEventQueue(&stream_sink->event_queue)))
    {
        free(stream_sink);
        return hr;
    }

    stream_sink->IMFStreamSink_iface.lpVtbl = &stream_sink_vtbl;
    stream_sink->IMFMediaTypeHandler_iface.lpVtbl = &stream_sink_type_handler_vtbl;
    stream_sink->refcount = 1;
    stream_sink->id = stream_sink_id;
    if (media_type)
        IMFMediaType_AddRef((stream_sink->type = media_type));
    IMFFinalizableMediaSink_AddRef((stream_sink->media_sink = &media_sink->IMFFinalizableMediaSink_iface));

    TRACE("Created stream sink %p.\n", stream_sink);
    *out = stream_sink;

    return S_OK;
}

static struct stream_sink *media_sink_get_stream_sink_by_id(struct media_sink *media_sink, DWORD id)
{
    struct stream_sink *stream_sink;

    LIST_FOR_EACH_ENTRY(stream_sink, &media_sink->stream_sinks, struct stream_sink, entry)
    {
        if (stream_sink->id == id)
            return stream_sink;
    }

    return NULL;
}

static HRESULT media_sink_queue_stream_event(struct media_sink *media_sink, MediaEventType type)
{
    struct stream_sink *stream_sink;
    HRESULT hr;

    LIST_FOR_EACH_ENTRY(stream_sink, &media_sink->stream_sinks, struct stream_sink, entry)
    {
        if (FAILED(hr = IMFMediaEventQueue_QueueEventParamVar(stream_sink->event_queue, type, &GUID_NULL, S_OK, NULL)))
            return hr;
    }

    return S_OK;
}

static HRESULT media_sink_start(struct media_sink *media_sink)
{
    NTSTATUS status;

    if ((status = winedmo_muxer_start(media_sink->winedmo_muxer)))
        return HRESULT_FROM_NT(status);

    media_sink->state = STATE_STARTED;

    return media_sink_queue_stream_event(media_sink, MEStreamSinkStarted);
}

static HRESULT media_sink_stop(struct media_sink *media_sink)
{
    media_sink->state = STATE_STOPPED;
    return media_sink_queue_stream_event(media_sink, MEStreamSinkStopped);
}

static HRESULT media_sink_pause(struct media_sink *media_sink)
{
    media_sink->state = STATE_PAUSED;
    return media_sink_queue_stream_event(media_sink, MEStreamSinkPaused);
}

static HRESULT media_sink_finalize(struct media_sink *media_sink, IMFAsyncResult *result)
{
    HRESULT hr = S_OK;
    NTSTATUS status;

    media_sink->state = STATE_FINALIZED;

    if ((status = winedmo_muxer_destroy(&media_sink->winedmo_muxer)))
        hr = HRESULT_FROM_NT(status);

    IMFAsyncResult_SetStatus(result, hr);
    MFInvokeCallback(result);

    return hr;
}

static HRESULT WINAPI media_sink_QueryInterface(IMFFinalizableMediaSink *iface, REFIID riid, void **obj)
{
    struct media_sink *media_sink = impl_from_IMFFinalizableMediaSink(iface);

    TRACE("iface %p, riid %s, obj %p.\n", iface, debugstr_guid(riid), obj);

    if (IsEqualIID(riid, &IID_IMFFinalizableMediaSink) ||
            IsEqualIID(riid, &IID_IMFMediaSink) ||
            IsEqualIID(riid, &IID_IUnknown))
    {
        *obj = iface;
    }
    else if (IsEqualGUID(riid, &IID_IMFMediaEventGenerator))
    {
        *obj = &media_sink->IMFMediaEventGenerator_iface;
    }
    else if (IsEqualIID(riid, &IID_IMFClockStateSink))
    {
        *obj = &media_sink->IMFClockStateSink_iface;
    }
    else
    {
        WARN("Unsupported interface %s.\n", debugstr_guid(riid));
        *obj = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown *)*obj);

    return S_OK;
}

static ULONG WINAPI media_sink_AddRef(IMFFinalizableMediaSink *iface)
{
    struct media_sink *media_sink = impl_from_IMFFinalizableMediaSink(iface);
    ULONG refcount = InterlockedIncrement(&media_sink->refcount);
    TRACE("iface %p, refcount %lu.\n", iface, refcount);
    return refcount;
}

static ULONG WINAPI media_sink_Release(IMFFinalizableMediaSink *iface)
{
    struct media_sink *media_sink = impl_from_IMFFinalizableMediaSink(iface);
    ULONG refcount = InterlockedDecrement(&media_sink->refcount);

    TRACE("iface %p, refcount %lu.\n", iface, refcount);

    if (!refcount)
    {
        IMFFinalizableMediaSink_Shutdown(iface);
        IMFMediaEventQueue_Release(media_sink->event_queue);
        IMFByteStream_Release(media_sink->stream);
        media_sink->cs.DebugInfo->Spare[0] = 0;
        DeleteCriticalSection(&media_sink->cs);
        winedmo_muxer_destroy(&media_sink->winedmo_muxer);
        free(media_sink);
    }

    return refcount;
}

static HRESULT WINAPI media_sink_GetCharacteristics(IMFFinalizableMediaSink *iface, DWORD *flags)
{
    FIXME("iface %p, flags %p stub!\n", iface, flags);

    return E_NOTIMPL;
}


static HRESULT muxer_add_stream_with_media_type(struct winedmo_muxer muxer, DWORD id, IMFMediaType *media_type)
{
    union winedmo_format *format;
    UINT format_size;
    NTSTATUS status;
    HRESULT hr;
    GUID major;

    if (FAILED(hr = IMFMediaType_GetMajorType(media_type, &major)))
        return hr;

    if (IsEqualGUID(&major, &MFMediaType_Video))
    {
        if (SUCCEEDED(hr = MFCreateMFVideoFormatFromMFMediaType(media_type, (MFVIDEOFORMAT **)&format, &format_size)))
        {
            if ((status = winedmo_muxer_add_stream(muxer, id, major, format)))
                hr = HRESULT_FROM_NT(status);
            CoTaskMemFree(format);
        }
    }
    else if (IsEqualGUID(&major, &MFMediaType_Audio))
    {
        if (SUCCEEDED(hr = MFCreateWaveFormatExFromMFMediaType(media_type, (WAVEFORMATEX **)&format, &format_size, 0)))
        {
            if ((status = winedmo_muxer_add_stream(muxer, id, major, format)))
                hr = HRESULT_FROM_NT(status);
            CoTaskMemFree(format);
        }
    }
    else
    {
        FIXME("Unsupported major type %s\n", debugstr_guid(&major));
        hr = E_NOTIMPL;
    }

    return hr;
}

static HRESULT WINAPI media_sink_AddStreamSink(IMFFinalizableMediaSink *iface, DWORD stream_sink_id,
    IMFMediaType *media_type, IMFStreamSink **stream_sink)
{
    struct media_sink *media_sink = impl_from_IMFFinalizableMediaSink(iface);
    struct stream_sink *object;
    HRESULT hr;

    TRACE("iface %p, stream_sink_id %#lx, media_type %p, stream_sink %p.\n",
            iface, stream_sink_id, media_type, stream_sink);

    EnterCriticalSection(&media_sink->cs);

    if (media_sink_get_stream_sink_by_id(media_sink, stream_sink_id))
    {
        LeaveCriticalSection(&media_sink->cs);
        return MF_E_STREAMSINK_EXISTS;
    }

    if (FAILED(hr = stream_sink_create(stream_sink_id, media_type, media_sink, &object)))
    {
        WARN("Failed to create stream sink, hr %#lx.\n", hr);
        LeaveCriticalSection(&media_sink->cs);
        return hr;
    }

    if (FAILED(hr = muxer_add_stream_with_media_type(media_sink->winedmo_muxer, stream_sink_id, media_type)))
    {
        LeaveCriticalSection(&media_sink->cs);
        IMFStreamSink_Release(&object->IMFStreamSink_iface);
        return hr;
    }

    list_add_tail(&media_sink->stream_sinks, &object->entry);

    LeaveCriticalSection(&media_sink->cs);

    if (stream_sink)
        IMFStreamSink_AddRef((*stream_sink = &object->IMFStreamSink_iface));

    return S_OK;
}

static HRESULT WINAPI media_sink_RemoveStreamSink(IMFFinalizableMediaSink *iface, DWORD stream_sink_id)
{
    FIXME("iface %p, stream_sink_id %#lx stub!\n", iface, stream_sink_id);

    return E_NOTIMPL;
}

static HRESULT WINAPI media_sink_GetStreamSinkCount(IMFFinalizableMediaSink *iface, DWORD *count)
{
    struct media_sink *media_sink = impl_from_IMFFinalizableMediaSink(iface);

    TRACE("iface %p, count %p.\n", iface, count);

    if (!count)
        return E_POINTER;

    EnterCriticalSection(&media_sink->cs);
    *count = list_count(&media_sink->stream_sinks);
    LeaveCriticalSection(&media_sink->cs);

    return S_OK;
}

static HRESULT WINAPI media_sink_GetStreamSinkByIndex(IMFFinalizableMediaSink *iface, DWORD index,
        IMFStreamSink **stream)
{
    struct media_sink *media_sink = impl_from_IMFFinalizableMediaSink(iface);
    struct stream_sink *stream_sink;
    HRESULT hr = MF_E_INVALIDINDEX;
    DWORD entry_index = 0;

    TRACE("iface %p, index %lu, stream %p stub!\n", iface, index, stream);

    if (!stream)
        return E_POINTER;

    EnterCriticalSection(&media_sink->cs);

    LIST_FOR_EACH_ENTRY(stream_sink, &media_sink->stream_sinks, struct stream_sink, entry)
    {
        if (entry_index++ == index)
        {
            IMFStreamSink_AddRef((*stream = &stream_sink->IMFStreamSink_iface));
            hr = S_OK;
            break;
        }
    }

    LeaveCriticalSection(&media_sink->cs);

    return hr;
}

static HRESULT WINAPI media_sink_GetStreamSinkById(IMFFinalizableMediaSink *iface, DWORD stream_sink_id,
        IMFStreamSink **stream)
{
    struct media_sink *media_sink = impl_from_IMFFinalizableMediaSink(iface);
    struct stream_sink *stream_sink;
    HRESULT hr;

    TRACE("iface %p, stream_sink_id %#lx, stream %p.\n", iface, stream_sink_id, stream);

    if (!stream)
        return E_POINTER;

    EnterCriticalSection(&media_sink->cs);

    hr = MF_E_INVALIDSTREAMNUMBER;
    if ((stream_sink = media_sink_get_stream_sink_by_id(media_sink, stream_sink_id)))
    {
        IMFStreamSink_AddRef((*stream = &stream_sink->IMFStreamSink_iface));
        hr = S_OK;
    }

    LeaveCriticalSection(&media_sink->cs);

    return hr;
}

static HRESULT WINAPI media_sink_SetPresentationClock(IMFFinalizableMediaSink *iface, IMFPresentationClock *clock)
{
    FIXME("iface %p, clock %p stub!\n", iface, clock);

    return E_NOTIMPL;
}

static HRESULT WINAPI media_sink_GetPresentationClock(IMFFinalizableMediaSink *iface, IMFPresentationClock **clock)
{
    FIXME("iface %p, clock %p stub!\n", iface, clock);

    return E_NOTIMPL;
}

static HRESULT WINAPI media_sink_Shutdown(IMFFinalizableMediaSink *iface)
{
    struct media_sink *media_sink = impl_from_IMFFinalizableMediaSink(iface);
    struct stream_sink *stream_sink, *next;

    TRACE("iface %p.\n", iface);

    EnterCriticalSection(&media_sink->cs);

    if (media_sink->state == STATE_SHUTDOWN)
    {
        LeaveCriticalSection(&media_sink->cs);
        return MF_E_SHUTDOWN;
    }

    LIST_FOR_EACH_ENTRY_SAFE(stream_sink, next, &media_sink->stream_sinks, struct stream_sink, entry)
    {
        list_remove(&stream_sink->entry);
        IMFMediaEventQueue_Shutdown(stream_sink->event_queue);
        IMFStreamSink_Release(&stream_sink->IMFStreamSink_iface);
    }

    IMFMediaEventQueue_Shutdown(media_sink->event_queue);
    IMFByteStream_Close(media_sink->stream);

    media_sink->state = STATE_SHUTDOWN;

    LeaveCriticalSection(&media_sink->cs);

    return S_OK;
}

static HRESULT WINAPI media_sink_BeginFinalize(IMFFinalizableMediaSink *iface, IMFAsyncCallback *callback, IUnknown *state)
{
    struct media_sink *media_sink = impl_from_IMFFinalizableMediaSink(iface);
    IMFAsyncResult *result;
    HRESULT hr;

    TRACE("iface %p, callback %p, state %p.\n", iface, callback, state);

    EnterCriticalSection(&media_sink->cs);
    if (media_sink->state == STATE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else if (!callback)
        hr = S_OK;
    else if (SUCCEEDED(hr = MFCreateAsyncResult(NULL, callback, state, &result)))
    {
        hr = MFPutWorkItem(MFASYNC_CALLBACK_QUEUE_STANDARD, &media_sink->async_finalize_iface, (IUnknown *)result);
        IMFAsyncResult_Release(result);
    }

    LeaveCriticalSection(&media_sink->cs);

    return hr;
}

static HRESULT WINAPI media_sink_EndFinalize(IMFFinalizableMediaSink *iface, IMFAsyncResult *result)
{
    TRACE("iface %p, result %p.\n", iface, result);

    return result ? IMFAsyncResult_GetStatus(result) : E_INVALIDARG;
}

static const IMFFinalizableMediaSinkVtbl media_sink_vtbl =
{
    media_sink_QueryInterface,
    media_sink_AddRef,
    media_sink_Release,
    media_sink_GetCharacteristics,
    media_sink_AddStreamSink,
    media_sink_RemoveStreamSink,
    media_sink_GetStreamSinkCount,
    media_sink_GetStreamSinkByIndex,
    media_sink_GetStreamSinkById,
    media_sink_SetPresentationClock,
    media_sink_GetPresentationClock,
    media_sink_Shutdown,
    media_sink_BeginFinalize,
    media_sink_EndFinalize,
};

static HRESULT WINAPI media_sink_event_QueryInterface(IMFMediaEventGenerator *iface, REFIID riid, void **obj)
{
    struct media_sink *media_sink = impl_from_IMFMediaEventGenerator(iface);
    return IMFFinalizableMediaSink_QueryInterface(&media_sink->IMFFinalizableMediaSink_iface, riid, obj);
}

static ULONG WINAPI media_sink_event_AddRef(IMFMediaEventGenerator *iface)
{
    struct media_sink *media_sink = impl_from_IMFMediaEventGenerator(iface);
    return IMFFinalizableMediaSink_AddRef(&media_sink->IMFFinalizableMediaSink_iface);
}

static ULONG WINAPI media_sink_event_Release(IMFMediaEventGenerator *iface)
{
    struct media_sink *media_sink = impl_from_IMFMediaEventGenerator(iface);
    return IMFFinalizableMediaSink_Release(&media_sink->IMFFinalizableMediaSink_iface);
}

static HRESULT WINAPI media_sink_event_GetEvent(IMFMediaEventGenerator *iface, DWORD flags, IMFMediaEvent **event)
{
    struct media_sink *media_sink = impl_from_IMFMediaEventGenerator(iface);

    TRACE("iface %p, flags %#lx, event %p.\n", iface, flags, event);

    return IMFMediaEventQueue_GetEvent(media_sink->event_queue, flags, event);
}

static HRESULT WINAPI media_sink_event_BeginGetEvent(IMFMediaEventGenerator *iface, IMFAsyncCallback *callback,
        IUnknown *state)
{
    struct media_sink *media_sink = impl_from_IMFMediaEventGenerator(iface);

    TRACE("iface %p, callback %p, state %p.\n", iface, callback, state);

    return IMFMediaEventQueue_BeginGetEvent(media_sink->event_queue, callback, state);
}

static HRESULT WINAPI media_sink_event_EndGetEvent(IMFMediaEventGenerator *iface, IMFAsyncResult *result,
        IMFMediaEvent **event)
{
    struct media_sink *media_sink = impl_from_IMFMediaEventGenerator(iface);

    TRACE("iface %p, result %p, event %p.\n", iface, result, event);

    return IMFMediaEventQueue_EndGetEvent(media_sink->event_queue, result, event);
}

static HRESULT WINAPI media_sink_event_QueueEvent(IMFMediaEventGenerator *iface, MediaEventType event_type,
        REFGUID ext_type, HRESULT hr, const PROPVARIANT *value)
{
    struct media_sink *media_sink = impl_from_IMFMediaEventGenerator(iface);

    TRACE("iface %p, event_type %lu, ext_type %s, hr %#lx, value %p.\n",
            iface, event_type, debugstr_guid(ext_type), hr, value);

    return IMFMediaEventQueue_QueueEventParamVar(media_sink->event_queue, event_type, ext_type, hr, value);
}

static const IMFMediaEventGeneratorVtbl media_sink_event_vtbl =
{
    media_sink_event_QueryInterface,
    media_sink_event_AddRef,
    media_sink_event_Release,
    media_sink_event_GetEvent,
    media_sink_event_BeginGetEvent,
    media_sink_event_EndGetEvent,
    media_sink_event_QueueEvent,
};

static HRESULT WINAPI media_sink_clock_sink_QueryInterface(IMFClockStateSink *iface, REFIID riid, void **obj)
{
    struct media_sink *media_sink = impl_from_IMFClockStateSink(iface);
    return IMFFinalizableMediaSink_QueryInterface(&media_sink->IMFFinalizableMediaSink_iface, riid, obj);
}

static ULONG WINAPI media_sink_clock_sink_AddRef(IMFClockStateSink *iface)
{
    struct media_sink *media_sink = impl_from_IMFClockStateSink(iface);
    return IMFFinalizableMediaSink_AddRef(&media_sink->IMFFinalizableMediaSink_iface);
}

static ULONG WINAPI media_sink_clock_sink_Release(IMFClockStateSink *iface)
{
    struct media_sink *media_sink = impl_from_IMFClockStateSink(iface);
    return IMFFinalizableMediaSink_Release(&media_sink->IMFFinalizableMediaSink_iface);
}

static HRESULT WINAPI media_sink_clock_sink_OnClockStart(IMFClockStateSink *iface, MFTIME systime, LONGLONG offset)
{
    struct media_sink *media_sink = impl_from_IMFClockStateSink(iface);
    HRESULT hr;

    TRACE("iface %p, systime %s, offset %s.\n", iface, debugstr_time(systime), debugstr_time(offset));

    EnterCriticalSection(&media_sink->cs);

    if (media_sink->state == STATE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else
        hr = MFPutWorkItem(MFASYNC_CALLBACK_QUEUE_STANDARD, &media_sink->async_start_iface, NULL);

    LeaveCriticalSection(&media_sink->cs);
    return hr;
}

static HRESULT WINAPI media_sink_clock_sink_OnClockStop(IMFClockStateSink *iface, MFTIME systime)
{
    struct media_sink *media_sink = impl_from_IMFClockStateSink(iface);
    HRESULT hr;

    TRACE("iface %p, systime %s.\n", iface, debugstr_time(systime));

    EnterCriticalSection(&media_sink->cs);

    if (media_sink->state == STATE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else
        hr = MFPutWorkItem(MFASYNC_CALLBACK_QUEUE_STANDARD, &media_sink->async_stop_iface, NULL);

    LeaveCriticalSection(&media_sink->cs);
    return hr;
}

static HRESULT WINAPI media_sink_clock_sink_OnClockPause(IMFClockStateSink *iface, MFTIME systime)
{
    struct media_sink *media_sink = impl_from_IMFClockStateSink(iface);
    HRESULT hr;

    TRACE("iface %p, systime %s.\n", iface, debugstr_time(systime));

    EnterCriticalSection(&media_sink->cs);

    if (media_sink->state == STATE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else
        hr = MFPutWorkItem(MFASYNC_CALLBACK_QUEUE_STANDARD, &media_sink->async_pause_iface, NULL);

    LeaveCriticalSection(&media_sink->cs);
    return hr;
}

static HRESULT WINAPI media_sink_clock_sink_OnClockRestart(IMFClockStateSink *iface, MFTIME systime)
{
    struct media_sink *media_sink = impl_from_IMFClockStateSink(iface);
    HRESULT hr;

    TRACE("iface %p, systime %s.\n", iface, debugstr_time(systime));

    EnterCriticalSection(&media_sink->cs);

    if (media_sink->state == STATE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else
        hr = MFPutWorkItem(MFASYNC_CALLBACK_QUEUE_STANDARD, &media_sink->async_start_iface, NULL);

    LeaveCriticalSection(&media_sink->cs);
    return hr;
}

static HRESULT WINAPI media_sink_clock_sink_OnClockSetRate(IMFClockStateSink *iface, MFTIME systime, float rate)
{
    FIXME("iface %p, systime %s, rate %f stub!\n", iface, debugstr_time(systime), rate);

    return E_NOTIMPL;
}

static const IMFClockStateSinkVtbl media_sink_clock_sink_vtbl =
{
    media_sink_clock_sink_QueryInterface,
    media_sink_clock_sink_AddRef,
    media_sink_clock_sink_Release,
    media_sink_clock_sink_OnClockStart,
    media_sink_clock_sink_OnClockStop,
    media_sink_clock_sink_OnClockPause,
    media_sink_clock_sink_OnClockRestart,
    media_sink_clock_sink_OnClockSetRate,
};

DEFINE_MF_ASYNC_CALLBACK(async_start, struct media_sink, IMFFinalizableMediaSink_iface)

static HRESULT WINAPI async_start_Invoke(IMFAsyncCallback *iface, IMFAsyncResult *async_result)
{
    struct media_sink *media_sink = impl_from_async_start(iface);
    HRESULT hr = E_FAIL;

    TRACE("iface %p, async_result %p.\n", iface, async_result);

    EnterCriticalSection(&media_sink->cs);

    if (FAILED(hr = media_sink_start(media_sink)))
        WARN("Failed to start media sink.\n");

    LeaveCriticalSection(&media_sink->cs);

    return hr;
}

DEFINE_MF_ASYNC_CALLBACK(async_stop, struct media_sink, IMFFinalizableMediaSink_iface)

static HRESULT WINAPI async_stop_Invoke(IMFAsyncCallback *iface, IMFAsyncResult *async_result)
{
    struct media_sink *media_sink = impl_from_async_stop(iface);
    HRESULT hr;

    TRACE("iface %p, async_result %p.\n", iface, async_result);

    EnterCriticalSection(&media_sink->cs);
    hr = media_sink_stop(media_sink);
    LeaveCriticalSection(&media_sink->cs);

    return hr;
}

DEFINE_MF_ASYNC_CALLBACK(async_pause, struct media_sink, IMFFinalizableMediaSink_iface)

static HRESULT WINAPI async_pause_Invoke(IMFAsyncCallback *iface, IMFAsyncResult *async_result)
{
    struct media_sink *media_sink = impl_from_async_stop(iface);
    HRESULT hr;

    TRACE("iface %p, async_result %p.\n", iface, async_result);

    EnterCriticalSection(&media_sink->cs);
    hr = media_sink_pause(media_sink);
    LeaveCriticalSection(&media_sink->cs);

    return hr;
}

DEFINE_MF_ASYNC_CALLBACK(async_process, struct media_sink, IMFFinalizableMediaSink_iface)

static HRESULT WINAPI async_process_Invoke(IMFAsyncCallback *iface, IMFAsyncResult *async_result)
{
    struct media_sink *media_sink = impl_from_async_process(iface);
    IUnknown *state = IMFAsyncResult_GetStateNoAddRef(async_result);
    struct async_process_params *params = async_process_params_from_IUnknown(state);
    DMO_OUTPUT_DATA_BUFFER input = {0};
    IMFMediaBuffer *buffer;
    NTSTATUS status;
    HRESULT hr;

    TRACE("iface %p, async_result %p.\n", iface, async_result);

    EnterCriticalSection(&media_sink->cs);

    if (FAILED(hr = IMFSample_ConvertToContiguousBuffer(params->sample, &buffer)))
        return hr;
    if (SUCCEEDED(hr = MFCreateLegacyMediaBufferOnMFMediaBuffer(params->sample, buffer, 0, &input.pBuffer)))
    {
        if ((status = winedmo_muxer_write(media_sink->winedmo_muxer, params->stream_id, &input)))
        {
            WARN("Failed to process sample, status %#lx.\n", status);
            hr = HRESULT_FROM_NT(status);
        }
        IMediaBuffer_Release(input.pBuffer);
    }
    IMFMediaBuffer_Release(buffer);

    LeaveCriticalSection(&media_sink->cs);

    return hr;
}

DEFINE_MF_ASYNC_CALLBACK(async_finalize, struct media_sink, IMFFinalizableMediaSink_iface)

static HRESULT WINAPI async_finalize_Invoke(IMFAsyncCallback *iface, IMFAsyncResult *async_result)
{
    struct media_sink *media_sink = impl_from_async_finalize(iface);
    IMFAsyncResult *result = (IMFAsyncResult *)IMFAsyncResult_GetStateNoAddRef(async_result);
    HRESULT hr = E_FAIL;

    TRACE("iface %p, async_result %p.\n", iface, async_result);

    EnterCriticalSection(&media_sink->cs);

    if (FAILED(hr = media_sink_finalize(media_sink, result)))
        WARN("Failed to finalize, hr %#lx.\n", hr);

    LeaveCriticalSection(&media_sink->cs);

    return hr;
}

static NTSTATUS CDECL media_sink_seek_cb( struct winedmo_stream *stream, UINT64 *pos )
{
    struct media_sink *sink = CONTAINING_RECORD(stream, struct media_sink, winedmo_stream);
    if (FAILED(IMFByteStream_Seek(sink->stream, msoBegin, *pos, 0, pos))) return STATUS_UNSUCCESSFUL;
    return STATUS_SUCCESS;
}

static NTSTATUS CDECL media_sink_read_cb( struct winedmo_stream *stream, BYTE *buffer, ULONG *size )
{
    struct media_sink *sink = CONTAINING_RECORD(stream, struct media_sink, winedmo_stream);
    if (FAILED(IMFByteStream_Read(sink->stream, buffer, *size, size))) return STATUS_UNSUCCESSFUL;
    return STATUS_SUCCESS;
}

static NTSTATUS CDECL media_sink_write_cb( struct winedmo_stream *stream, const BYTE *buffer, ULONG *size )
{
    struct media_sink *sink = CONTAINING_RECORD(stream, struct media_sink, winedmo_stream);
    if (FAILED(IMFByteStream_Write(sink->stream, buffer, *size, size))) return STATUS_UNSUCCESSFUL;
    return STATUS_SUCCESS;
}

static HRESULT media_sink_create(IMFByteStream *stream, const WCHAR *format, struct media_sink **out)
{
    struct media_sink *media_sink;
    NTSTATUS status;
    HRESULT hr;

    TRACE("stream %p, out %p.\n", stream, out);

    if (!stream)
        return E_POINTER;

    if (!(media_sink = calloc(1, sizeof(*media_sink))))
        return E_OUTOFMEMORY;
    media_sink->winedmo_stream.p_seek = media_sink_seek_cb;
    media_sink->winedmo_stream.p_read = media_sink_read_cb;
    media_sink->winedmo_stream.p_write = media_sink_write_cb;

    IMFByteStream_SetCurrentPosition(stream, 0);
    if ((status = winedmo_muxer_create(NULL, format, &media_sink->winedmo_stream, &media_sink->winedmo_muxer)))
    {
        hr = HRESULT_FROM_NT(status);
        goto fail;
    }
    if (FAILED(hr = MFCreateEventQueue(&media_sink->event_queue)))
        goto fail;

    media_sink->IMFFinalizableMediaSink_iface.lpVtbl = &media_sink_vtbl;
    media_sink->IMFMediaEventGenerator_iface.lpVtbl = &media_sink_event_vtbl;
    media_sink->IMFClockStateSink_iface.lpVtbl = &media_sink_clock_sink_vtbl;
    media_sink->async_start_iface.lpVtbl = &async_start_vtbl;
    media_sink->async_stop_iface.lpVtbl = &async_stop_vtbl;
    media_sink->async_pause_iface.lpVtbl = &async_pause_vtbl;
    media_sink->async_process_iface.lpVtbl = &async_process_vtbl;
    media_sink->async_finalize_iface.lpVtbl = &async_finalize_vtbl;
    media_sink->refcount = 1;
    media_sink->state = STATE_OPENED;
    InitializeCriticalSectionEx(&media_sink->cs, 0, RTL_CRITICAL_SECTION_FLAG_FORCE_DEBUG_INFO);
    media_sink->cs.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": cs");
    IMFByteStream_AddRef((media_sink->stream = stream));
    list_init(&media_sink->stream_sinks);

    *out = media_sink;
    TRACE("Created media sink %p.\n", media_sink);

    return S_OK;

fail:
    winedmo_muxer_destroy(&media_sink->winedmo_muxer);
    free(media_sink);
    return hr;
}

static HRESULT WINAPI sink_factory_create_media_sink(IMFSinkClassFactory *iface, IMFByteStream *bytestream,
        const WCHAR *format, IMFMediaType *video_type, IMFMediaType *audio_type, IMFMediaSink **out)
{
    IMFFinalizableMediaSink *media_sink_iface;
    struct media_sink *media_sink;
    HRESULT hr;

    TRACE("iface %p, bytestream %p, video_type %p, audio_type %p, out %p.\n",
            iface, bytestream, video_type, audio_type, out);

    if (FAILED(hr = media_sink_create(bytestream, format, &media_sink)))
        return hr;
    media_sink_iface = &media_sink->IMFFinalizableMediaSink_iface;

    if (video_type)
    {
        if (FAILED(hr = IMFFinalizableMediaSink_AddStreamSink(media_sink_iface, 1, video_type, NULL)))
        {
            IMFFinalizableMediaSink_Shutdown(media_sink_iface);
            IMFFinalizableMediaSink_Release(media_sink_iface);
            return hr;
        }
    }
    if (audio_type)
    {
        if (FAILED(hr = IMFFinalizableMediaSink_AddStreamSink(media_sink_iface, 2, audio_type, NULL)))
        {
            IMFFinalizableMediaSink_Shutdown(media_sink_iface);
            IMFFinalizableMediaSink_Release(media_sink_iface);
            return hr;
        }
    }

    *out = (IMFMediaSink *)media_sink_iface;
    return S_OK;
}

static HRESULT WINAPI mp3_sink_factory_CreateMediaSink(IMFSinkClassFactory *iface, IMFByteStream *bytestream,
        IMFMediaType *video_type, IMFMediaType *audio_type, IMFMediaSink **out)
{
    const WCHAR *format = L"application/x-id3";
    return sink_factory_create_media_sink(iface, bytestream, format, video_type, audio_type, out);
}

static const IMFSinkClassFactoryVtbl mp3_sink_factory_vtbl =
{
    sink_class_factory_QueryInterface,
    sink_class_factory_AddRef,
    sink_class_factory_Release,
    mp3_sink_factory_CreateMediaSink,
};

static HRESULT WINAPI mp3_sink_class_factory_CreateInstance(IClassFactory *iface, IUnknown *outer, REFIID riid, void **out)
{
    struct sink_class_factory *factory = sink_class_factory_from_IClassFactory(iface);
    NTSTATUS status;

    if ((status = winedmo_muxer_check("video/mp3")))
    {
        static const GUID CLSID_wg_mp3_sink_factory = {0x1f302877,0xaaab,0x40a3,{0xb9,0xe0,0x9f,0x48,0xda,0xf3,0x5b,0xc8}};
        WARN("Unsupported muxer, status %#lx.\n", status);
        return CoCreateInstance(&CLSID_wg_mp3_sink_factory, outer, CLSCTX_INPROC_SERVER, riid, out);
    }

    TRACE("iface %p, outer %p, riid %s, out %p stub!.\n", iface, outer, debugstr_guid(riid), out);

    *out = NULL;
    if (outer)
        return CLASS_E_NOAGGREGATION;
    return IMFSinkClassFactory_QueryInterface(&factory->IMFSinkClassFactory_iface, riid, out);
}

static const IClassFactoryVtbl mp3_sink_class_factory_vtbl =
{
    class_factory_QueryInterface,
    class_factory_AddRef,
    class_factory_Release,
    mp3_sink_class_factory_CreateInstance,
    class_factory_LockServer,
};

struct sink_class_factory mp3_sink_factory =
{
    {&mp3_sink_class_factory_vtbl},
    {&mp3_sink_factory_vtbl},
};

IClassFactory *mp3_sink_class_factory = &mp3_sink_factory.IClassFactory_iface;

static HRESULT WINAPI mpeg4_sink_factory_CreateMediaSink(IMFSinkClassFactory *iface, IMFByteStream *bytestream,
        IMFMediaType *video_type, IMFMediaType *audio_type, IMFMediaSink **out)
{
    const WCHAR *format = L"video/mp4";

    return sink_factory_create_media_sink(iface, bytestream, format, video_type, audio_type, out);
}

static const IMFSinkClassFactoryVtbl mpeg4_sink_factory_vtbl =
{
    sink_class_factory_QueryInterface,
    sink_class_factory_AddRef,
    sink_class_factory_Release,
    mpeg4_sink_factory_CreateMediaSink,
};

static HRESULT WINAPI mpeg4_sink_class_factory_CreateInstance(IClassFactory *iface, IUnknown *outer, REFIID riid, void **out)
{
    struct sink_class_factory *factory = sink_class_factory_from_IClassFactory(iface);
    NTSTATUS status;

    if ((status = winedmo_muxer_check("video/mp4")))
    {
        static const GUID CLSID_wg_mpeg4_sink_factory = {0x5d5407d9,0xc6ca,0x4770,{0xa7,0xcc,0x27,0xc0,0xcb,0x8a,0x76,0x27}};
        WARN("Unsupported muxer, status %#lx.\n", status);
        return CoCreateInstance(&CLSID_wg_mpeg4_sink_factory, outer, CLSCTX_INPROC_SERVER, riid, out);
    }

    TRACE("iface %p, outer %p, riid %s, out %p stub!.\n", iface, outer, debugstr_guid(riid), out);

    *out = NULL;
    if (outer)
        return CLASS_E_NOAGGREGATION;
    return IMFSinkClassFactory_QueryInterface(&factory->IMFSinkClassFactory_iface, riid, out);
}

static const IClassFactoryVtbl mpeg4_sink_class_factory_vtbl =
{
    class_factory_QueryInterface,
    class_factory_AddRef,
    class_factory_Release,
    mpeg4_sink_class_factory_CreateInstance,
    class_factory_LockServer,
};

struct sink_class_factory mpeg4_sink_factory =
{
    {&mpeg4_sink_class_factory_vtbl},
    {&mpeg4_sink_factory_vtbl},
};

IClassFactory *mpeg4_sink_class_factory = &mpeg4_sink_factory.IClassFactory_iface;
