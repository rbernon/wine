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

#include "mf_private.h"

#include "wine/list.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(mfplat);

typedef UINT64 demuxer_t;

extern HRESULT CDECL wineav_demuxer_create(const WCHAR *url, IStream *stream, UINT64 *file_size, INT64 *duration,
        UINT *stream_count, WCHAR *mime_type, demuxer_t *context);
extern void CDECL wineav_demuxer_destroy(demuxer_t context);
extern HRESULT CDECL wineav_demuxer_read(demuxer_t context, UINT *stream, IMFSample **sample);
extern HRESULT CDECL wineav_demuxer_seek(demuxer_t context, INT64 timestamp);
extern HRESULT CDECL wineav_demuxer_stream_lang(demuxer_t context, UINT stream, WCHAR *buffer, UINT len);
extern HRESULT CDECL wineav_demuxer_stream_name(demuxer_t context, UINT stream, WCHAR *buffer, UINT len);
extern HRESULT CDECL wineav_demuxer_stream_type(demuxer_t context, UINT stream, IMFMediaType **media_type);

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

struct object_entry
{
    struct list entry;
    IUnknown *object;
};

static void object_entry_destroy(struct object_entry *entry)
{
    if (entry->object) IUnknown_AddRef( entry->object );
    free(entry);
}

static HRESULT object_entry_create(IUnknown *object, struct object_entry **out)
{
    struct object_entry *entry;

    if (!(entry = calloc(1, sizeof(*entry)))) return E_OUTOFMEMORY;
    if ((entry->object = object)) IUnknown_AddRef( entry->object );

    *out = entry;
    return S_OK;
}

struct object_context
{
    IUnknown IUnknown_iface;
    LONG refcount;

    WCHAR *url;
    IMFByteStream *stream;
    IMFAsyncResult *result;
    IUnknown *object;
};

static struct object_context *impl_from_IUnknown(IUnknown *iface)
{
    return CONTAINING_RECORD(iface, struct object_context, IUnknown_iface);
}

static HRESULT WINAPI object_context_QueryInterface(IUnknown *iface, REFIID riid, void **obj)
{
    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), obj);

    if (IsEqualIID(riid, &IID_IUnknown))
    {
        *obj = iface;
        IUnknown_AddRef(iface);
        return S_OK;
    }

    WARN("Unsupported %s.\n", debugstr_guid(riid));
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI object_context_AddRef(IUnknown *iface)
{
    struct object_context *context = impl_from_IUnknown(iface);
    ULONG refcount = InterlockedIncrement(&context->refcount);

    TRACE("%p, refcount %lu.\n", iface, refcount);

    return refcount;
}

static ULONG WINAPI object_context_Release(IUnknown *iface)
{
    struct object_context *context = impl_from_IUnknown(iface);
    ULONG refcount = InterlockedDecrement(&context->refcount);

    TRACE("%p, refcount %lu.\n", iface, refcount);

    if (!refcount)
    {
        if (context->object)
            IUnknown_Release(context->object);
        IMFAsyncResult_Release(context->result);
        IMFByteStream_Release(context->stream);
        free(context->url);
        free(context);
    }

    return refcount;
}

static const IUnknownVtbl object_context_vtbl =
{
    object_context_QueryInterface,
    object_context_AddRef,
    object_context_Release,
};

static WCHAR *get_byte_stream_url(IMFByteStream *stream, const WCHAR *url)
{
    IMFAttributes *attributes;
    WCHAR buffer[MAX_PATH];
    UINT32 size;
    HRESULT hr;

    if (SUCCEEDED(hr = IMFByteStream_QueryInterface(stream, &IID_IMFAttributes, (void **)&attributes)))
    {
        if (FAILED(hr = IMFAttributes_GetString(attributes, &MF_BYTESTREAM_ORIGIN_NAME,
                buffer, ARRAY_SIZE(buffer), &size)))
            WARN("Failed to get MF_BYTESTREAM_ORIGIN_NAME got size %#x, hr %#lx\n", size, hr);
        else
            url = buffer;
        IMFAttributes_Release(attributes);
    }

    return url ? wcsdup(url) : NULL;
}

static HRESULT object_context_create(const WCHAR *url, IMFByteStream *stream,
        IMFAsyncResult *result, IUnknown **out)
{
    WCHAR *tmp_url = get_byte_stream_url(stream, url);
    struct object_context *context;

    if (!(context = calloc(1, sizeof(*context))))
    {
        free(tmp_url);
        free(context);
        return E_OUTOFMEMORY;
    }

    context->IUnknown_iface.lpVtbl = &object_context_vtbl;
    context->refcount = 1;

    context->url = tmp_url;
    IMFByteStream_AddRef((context->stream = stream));
    IMFAsyncResult_AddRef((context->result = result));

    *out = &context->IUnknown_iface;
    return S_OK;
}

struct media_stream
{
    IMFMediaStream IMFMediaStream_iface;
    LONG ref;

    IMFMediaSource *media_source;
    IMFMediaEventQueue *event_queue;
    IMFStreamDescriptor *descriptor;
    struct list samples;
    struct list tokens;

    BOOL active;
    BOOL eos;
};

struct async_start_params
{
    IUnknown IUnknown_iface;
    LONG refcount;
    IMFPresentationDescriptor *descriptor;
    GUID format;
    PROPVARIANT position;
};

struct media_source
{
    IMFMediaSource IMFMediaSource_iface;
    IMFGetService IMFGetService_iface;
    IMFRateSupport IMFRateSupport_iface;
    IMFRateControl IMFRateControl_iface;
    IMFAsyncCallback async_start_iface;
    IMFAsyncCallback async_pause_iface;
    IMFAsyncCallback async_stop_iface;
    IMFAsyncCallback async_read_iface;
    LONG ref;
    DWORD async_commands_queue;
    IMFMediaEventQueue *event_queue;
    IMFByteStream *byte_stream;

    CRITICAL_SECTION cs;

    demuxer_t demuxer;
    WCHAR mime_type[256];
    UINT64 file_size;
    INT64 duration;

    UINT32 buffer_size;
    BYTE *buffer;

    IMFStreamDescriptor **descriptors;
    struct media_stream **streams;
    ULONG stream_count;
    UINT *stream_map;

    enum
    {
        SOURCE_OPENING,
        SOURCE_STOPPED,
        SOURCE_PAUSED,
        SOURCE_RUNNING,
        SOURCE_SHUTDOWN,
    } state;
    float rate;
};

static inline struct media_stream *impl_from_IMFMediaStream(IMFMediaStream *iface)
{
    return CONTAINING_RECORD(iface, struct media_stream, IMFMediaStream_iface);
}

static inline struct media_source *impl_from_IMFMediaSource(IMFMediaSource *iface)
{
    return CONTAINING_RECORD(iface, struct media_source, IMFMediaSource_iface);
}

static inline struct media_source *impl_from_IMFGetService(IMFGetService *iface)
{
    return CONTAINING_RECORD(iface, struct media_source, IMFGetService_iface);
}

static inline struct media_source *impl_from_IMFRateSupport(IMFRateSupport *iface)
{
    return CONTAINING_RECORD(iface, struct media_source, IMFRateSupport_iface);
}

static inline struct media_source *impl_from_IMFRateControl(IMFRateControl *iface)
{
    return CONTAINING_RECORD(iface, struct media_source, IMFRateControl_iface);
}

static inline struct async_start_params *impl_from_async_start_params_IUnknown(IUnknown *iface)
{
    return CONTAINING_RECORD(iface, struct async_start_params, IUnknown_iface);
}

static HRESULT WINAPI async_start_params_QueryInterface(IUnknown *iface, REFIID riid, void **obj)
{
    if (IsEqualIID(riid, &IID_IUnknown))
    {
        *obj = iface;
        IUnknown_AddRef(iface);
        return S_OK;
    }

    WARN("Unsupported interface %s.\n", debugstr_guid(riid));
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI async_start_params_AddRef(IUnknown *iface)
{
    struct async_start_params *params = impl_from_async_start_params_IUnknown(iface);
    return InterlockedIncrement(&params->refcount);
}

static ULONG WINAPI async_start_params_Release(IUnknown *iface)
{
    struct async_start_params *params = impl_from_async_start_params_IUnknown(iface);
    ULONG refcount = InterlockedDecrement(&params->refcount);

    if (!refcount)
    {
        IMFPresentationDescriptor_Release(params->descriptor);
        PropVariantClear(&params->position);
        free(params);
    }

    return refcount;
}

static const IUnknownVtbl async_start_params_vtbl =
{
    async_start_params_QueryInterface,
    async_start_params_AddRef,
    async_start_params_Release,
};

static HRESULT async_start_params_create(IMFPresentationDescriptor *descriptor, const GUID *time_format,
        const PROPVARIANT *position, IUnknown **out)
{
    struct async_start_params *params;

    if (!(params = calloc(1, sizeof(*params))))
        return E_OUTOFMEMORY;

    params->IUnknown_iface.lpVtbl = &async_start_params_vtbl;
    params->refcount = 1;

    params->descriptor = descriptor;
    IMFPresentationDescriptor_AddRef(descriptor);
    params->format = *time_format;
    PropVariantCopy(&params->position, position);

    *out = &params->IUnknown_iface;
    return S_OK;
}

DEFINE_MF_ASYNC_CALLBACK(async_start, struct media_source, IMFMediaSource_iface)

static HRESULT stream_descriptor_get_major_type(IMFStreamDescriptor *descriptor, GUID *major)
{
    IMFMediaTypeHandler *handler;
    HRESULT hr;

    if (FAILED(hr = IMFStreamDescriptor_GetMediaTypeHandler(descriptor, &handler)))
        return hr;
    hr = IMFMediaTypeHandler_GetMajorType(handler, major);
    IMFMediaTypeHandler_Release(handler);

    return hr;
}

static HRESULT stream_descriptor_create(UINT32 id, IMFMediaType *media_type, IMFStreamDescriptor **out)
{
    IMFStreamDescriptor *descriptor;
    IMFMediaTypeHandler *handler;
    HRESULT hr;

    *out = NULL;
    if (FAILED(hr = MFCreateStreamDescriptor(id, 1, &media_type, &descriptor)))
        return hr;

    if (FAILED(hr = IMFStreamDescriptor_GetMediaTypeHandler(descriptor, &handler)))
        IMFStreamDescriptor_Release(descriptor);
    else
    {
        if (SUCCEEDED(hr = IMFMediaTypeHandler_SetCurrentMediaType(handler, media_type)))
            *out = descriptor;
        IMFMediaTypeHandler_Release(handler);
    }

    return hr;
}

static HRESULT media_stream_queue_sample(struct media_stream *stream, IMFSample *sample)
{
    struct object_entry *entry;
    HRESULT hr;

    if (SUCCEEDED(hr = object_entry_create((IUnknown *)sample, &entry)))
        list_add_tail(&stream->samples, &entry->entry);
    return S_FALSE;
}

static HRESULT media_stream_queue_token(struct media_stream *stream, IUnknown *token)
{
    struct object_entry *entry;
    HRESULT hr;

    if (SUCCEEDED(hr = object_entry_create(token, &entry)))
        list_add_tail(&stream->tokens, &entry->entry);
    return S_FALSE;
}

static HRESULT media_stream_pop_sample(struct media_stream *stream, IMFSample **sample)
{
    struct object_entry *entry;
    struct list *ptr;

    if (!(ptr = list_head(&stream->samples))) return E_PENDING;
    entry = LIST_ENTRY(ptr, struct object_entry, entry);

    if ((*sample = (IMFSample *)entry->object)) IMFSample_AddRef(*sample);
    list_remove(&entry->entry);
    object_entry_destroy(entry);

    return S_OK;
}

static HRESULT media_stream_pop_token(struct media_stream *stream, IUnknown **token)
{
    struct object_entry *entry;
    struct list *ptr;

    if (!(ptr = list_head(&stream->tokens))) return E_PENDING;
    entry = LIST_ENTRY(ptr, struct object_entry, entry);

    if ((*token = entry->object)) IUnknown_AddRef(*token);
    list_remove(&entry->entry);
    object_entry_destroy(entry);

    return S_OK;
}

static HRESULT media_stream_send_sample(struct media_stream *stream, IMFSample *sample, IUnknown *token)
{
    HRESULT hr;

    if (!token || SUCCEEDED(hr = IMFSample_SetUnknown(sample, &MFSampleExtension_Token, token)))
        hr = IMFMediaEventQueue_QueueEventParamUnk(stream->event_queue, MEMediaSample,
                &GUID_NULL, S_OK, (IUnknown *)sample);

    return hr;
}

static HRESULT media_stream_try_send_sample(struct media_stream *stream, IMFSample *sample)
{
    IUnknown *token;
    HRESULT hr;

    if (FAILED(hr = media_stream_pop_token(stream, &token)))
        hr = media_stream_queue_sample(stream, sample);
    else
    {
        hr = media_stream_send_sample(stream, sample, token);
        if (token) IUnknown_Release(token);
    }

    return hr;
}


static HRESULT media_stream_start(struct media_stream *stream, BOOL active, BOOL seeking, const PROPVARIANT *position)
{
    struct media_source *source = impl_from_IMFMediaSource(stream->media_source);
    struct object_entry *entry, *next;
    HRESULT hr;

    TRACE("source %p, stream %p\n", source, stream);

    if (position->vt == VT_EMPTY)
    {
        IMFSample *sample;
        IUnknown *token;

        while (media_stream_pop_token(stream, &token) != E_PENDING) if (token) IUnknown_Release(token);
        while (media_stream_pop_sample(stream, &sample) != E_PENDING) IMFSample_Release(sample);
    }

    if (FAILED(hr = IMFMediaEventQueue_QueueEventParamUnk(source->event_queue, active ? MEUpdatedStream : MENewStream,
            &GUID_NULL, S_OK, (IUnknown *)&stream->IMFMediaStream_iface)))
        WARN("Failed to send source stream event, hr %#lx\n", hr);
    if (FAILED(hr = IMFMediaEventQueue_QueueEventParamVar(stream->event_queue, seeking ? MEStreamSeeked : MEStreamStarted,
            &GUID_NULL, S_OK, position)))
        WARN("Failed to send source stream event, hr %#lx\n", hr);

    if (position->vt != VT_EMPTY)
    {
        struct list samples = LIST_INIT(samples);

        list_move_head(&samples, &stream->samples);

        LIST_FOR_EACH_ENTRY_SAFE(entry, next, &samples, struct object_entry, entry)
        {
            media_stream_try_send_sample(stream, (IMFSample *)entry->object);
            list_remove(&entry->entry);
            object_entry_destroy(entry);
        }

        if (!list_empty(&stream->tokens))
            hr = MFPutWorkItem(source->async_commands_queue, &source->async_read_iface, NULL);
    }

    return hr;
}

static HRESULT media_stream_stop(struct media_stream *stream)
{
    HRESULT hr = S_OK;
    IMFSample *sample;
    IUnknown *token;

    while (media_stream_pop_token(stream, &token) != E_PENDING) if (token) IUnknown_Release(token);
    while (media_stream_pop_sample(stream, &sample) != E_PENDING) IMFSample_Release(sample);

    if (stream->active && FAILED(hr = IMFMediaEventQueue_QueueEventParamVar(stream->event_queue, MEStreamStopped,
                &GUID_NULL, S_OK, NULL)))
        WARN("Failed to queue MEStreamStopped event, hr %#lx\n", hr);

    return hr;
}

static HRESULT media_source_start(struct media_source *source, IMFPresentationDescriptor *descriptor,
        GUID *format, PROPVARIANT *position)
{
    BOOL starting = source->state == SOURCE_STOPPED, seek_message = !starting && position->vt != VT_EMPTY;
    IMFStreamDescriptor **descriptors;
    DWORD i, count;
    HRESULT hr;

    TRACE("source %p, descriptor %p, format %s, position %s\n", source, descriptor,
            debugstr_guid(format), wine_dbgstr_variant((VARIANT *)position));

    if (source->state == SOURCE_SHUTDOWN)
        return MF_E_SHUTDOWN;

    /* seek to beginning on stop->play */
    if (source->state == SOURCE_STOPPED && position->vt == VT_EMPTY)
    {
        position->vt = VT_I8;
        position->hVal.QuadPart = 0;
    }

    if (!(descriptors = calloc(source->stream_count, sizeof(*descriptors))))
        return E_OUTOFMEMORY;

    if (FAILED(hr = IMFPresentationDescriptor_GetStreamDescriptorCount(descriptor, &count)))
        WARN("Failed to get presentation descriptor stream count, hr %#lx\n", hr);

    for (i = 0; i < count; i++)
    {
        IMFStreamDescriptor *stream_descriptor;
        BOOL selected;
        DWORD id;

        if (FAILED(hr = IMFPresentationDescriptor_GetStreamDescriptorByIndex(descriptor, i,
                &selected, &stream_descriptor)))
            WARN("Failed to get presentation stream descriptor, hr %#lx\n", hr);
        else
        {
            if (FAILED(hr = IMFStreamDescriptor_GetStreamIdentifier(stream_descriptor, &id)))
                WARN("Failed to get stream descriptor id, hr %#lx\n", hr);
            else if (id > source->stream_count)
                WARN("Invalid stream descriptor id %lu, hr %#lx\n", id, hr);
            else if (selected)
                IMFStreamDescriptor_AddRef((descriptors[id - 1] = stream_descriptor));

            IMFStreamDescriptor_Release(stream_descriptor);
        }
    }

    for (i = 0; i < source->stream_count; i++)
    {
        struct media_stream *stream = source->streams[i];
        BOOL was_active = !starting && stream->active;

        if (position->vt != VT_EMPTY)
            stream->eos = FALSE;

        if ((stream->active = !!descriptors[i]))
        {
            if (FAILED(hr = media_stream_start(stream, was_active, seek_message, position)))
                WARN("Failed to start media stream, hr %#lx\n", hr);
            IMFStreamDescriptor_Release(descriptors[i]);
        }
    }

    free(descriptors);

    source->state = SOURCE_RUNNING;
    if (position->vt == VT_I8 && (FAILED(hr = wineav_demuxer_seek(source->demuxer, position->hVal.QuadPart))))
        WARN("Failed to seek to %I64d\n", position->hVal.QuadPart);

    return IMFMediaEventQueue_QueueEventParamVar(source->event_queue,
            seek_message ? MESourceSeeked : MESourceStarted, &GUID_NULL, S_OK, position);
}

static HRESULT WINAPI async_start_Invoke(IMFAsyncCallback *iface, IMFAsyncResult *result)
{
    struct media_source *source = impl_from_async_start(iface);
    struct async_start_params *params;
    IUnknown *state;
    HRESULT hr;

    if (!(state = IMFAsyncResult_GetStateNoAddRef(result))) return E_INVALIDARG;
    params = impl_from_async_start_params_IUnknown(state);

    EnterCriticalSection(&source->cs);

    if (FAILED(hr = media_source_start(source, params->descriptor, &params->format, &params->position)))
        WARN("Failed to start source %p, hr %#lx\n", source, hr);

    LeaveCriticalSection(&source->cs);

    return hr;
}

static HRESULT media_source_pause(struct media_source *source)
{
    unsigned int i;
    HRESULT hr;

    TRACE("source %p\n", source);

    if (source->state == SOURCE_SHUTDOWN)
        return MF_E_SHUTDOWN;

    for (i = 0; i < source->stream_count; i++)
    {
        struct media_stream *stream = source->streams[i];
        if (stream->active && FAILED(hr = IMFMediaEventQueue_QueueEventParamVar(stream->event_queue, MEStreamPaused,
                    &GUID_NULL, S_OK, NULL)))
            WARN("Failed to queue MEStreamPaused event, hr %#lx\n", hr);
    }

    source->state = SOURCE_PAUSED;
    return IMFMediaEventQueue_QueueEventParamVar(source->event_queue, MESourcePaused, &GUID_NULL, S_OK, NULL);
}

DEFINE_MF_ASYNC_CALLBACK(async_pause, struct media_source, IMFMediaSource_iface)

static HRESULT WINAPI async_pause_Invoke(IMFAsyncCallback *iface, IMFAsyncResult *result)
{
    struct media_source *source = impl_from_async_pause(iface);
    HRESULT hr;

    EnterCriticalSection(&source->cs);

    if (FAILED(hr = media_source_pause(source)))
        WARN("Failed to pause source %p, hr %#lx\n", source, hr);

    LeaveCriticalSection(&source->cs);

    return hr;
}

static HRESULT media_source_stop(struct media_source *source)
{
    unsigned int i;

    TRACE("source %p\n", source);

    if (source->state == SOURCE_SHUTDOWN)
        return MF_E_SHUTDOWN;

    for (i = 0; i < source->stream_count; i++)
    {
        struct media_stream *stream = source->streams[i];
        media_stream_stop(stream);
    }

    source->state = SOURCE_STOPPED;
    return IMFMediaEventQueue_QueueEventParamVar(source->event_queue, MESourceStopped, &GUID_NULL, S_OK, NULL);
}

DEFINE_MF_ASYNC_CALLBACK(async_stop, struct media_source, IMFMediaSource_iface)

static HRESULT WINAPI async_stop_Invoke(IMFAsyncCallback *iface, IMFAsyncResult *result)
{
    struct media_source *source = impl_from_async_stop(iface);
    HRESULT hr;

    EnterCriticalSection(&source->cs);

    if (FAILED(hr = media_source_stop(source)))
        WARN("Failed to stop source %p, hr %#lx\n", source, hr);

    LeaveCriticalSection(&source->cs);

    return hr;
}

static HRESULT media_source_read(struct media_source *source)
{
    IMFSample *sample;
    UINT i, index;
    HRESULT hr;

    if (source->state != SOURCE_RUNNING)
        return S_OK;

    if (FAILED(hr = wineav_demuxer_read(source->demuxer, &index, &sample)) && hr != MF_E_END_OF_STREAM)
    {
        WARN("Failed to read stream %u data, hr %#lx\n", index, hr);
        return hr;
    }

    if (hr == MF_E_END_OF_STREAM)
    {
        PROPVARIANT empty = {.vt = VT_EMPTY};

        for (i = 0; i < source->stream_count; i++)
        {
            struct media_stream *stream = source->streams[i];

            if (stream->active && FAILED(hr = IMFMediaEventQueue_QueueEventParamVar(stream->event_queue,
                    MEEndOfStream, &GUID_NULL, S_OK, &empty)))
                WARN("Failed to queue MEEndOfStream event, hr %#lx\n", hr);
            stream->eos = TRUE;
        }

        if (FAILED(hr = IMFMediaEventQueue_QueueEventParamVar(source->event_queue,
                MEEndOfPresentation, &GUID_NULL, S_OK, &empty)))
            WARN("Failed to queue MEEndOfPresentation event, hr %#lx\n", hr);

        return S_OK;
    }

    for (i = 0; i < source->stream_count; i++)
    {
        if (source->stream_map[i] == index)
        {
            struct media_stream *stream = source->streams[i];
            if (stream->active)
                hr = media_stream_try_send_sample(stream, sample);
            IMFSample_Release(sample);
            return hr;
        }
    }

    WARN("Failed to find stream with index %u\n", index);
    IMFSample_Release(sample);
    return hr;
}

DEFINE_MF_ASYNC_CALLBACK(async_read, struct media_source, IMFMediaSource_iface)

static HRESULT WINAPI async_read_Invoke(IMFAsyncCallback *iface, IMFAsyncResult *result)
{
    struct media_source *source = impl_from_async_read(iface);
    HRESULT hr;

    EnterCriticalSection(&source->cs);

    if (FAILED(hr = media_source_read(source)))
        WARN("Failed to request sample, hr %#lx\n", hr);

    if (hr == S_FALSE)
        hr = MFPutWorkItem(source->async_commands_queue, &source->async_read_iface, NULL);

    LeaveCriticalSection(&source->cs);

    return S_OK;
}

static HRESULT WINAPI media_stream_QueryInterface(IMFMediaStream *iface, REFIID riid, void **out)
{
    struct media_stream *stream = impl_from_IMFMediaStream(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), out);

    if (IsEqualIID(riid, &IID_IMFMediaStream) ||
        IsEqualIID(riid, &IID_IMFMediaEventGenerator) ||
        IsEqualIID(riid, &IID_IUnknown))
    {
        *out = &stream->IMFMediaStream_iface;
    }
    else
    {
        FIXME("(%s, %p)\n", debugstr_guid(riid), out);
        *out = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*out);
    return S_OK;
}

static ULONG WINAPI media_stream_AddRef(IMFMediaStream *iface)
{
    struct media_stream *stream = impl_from_IMFMediaStream(iface);
    ULONG ref = InterlockedIncrement(&stream->ref);

    TRACE("%p, refcount %lu.\n", iface, ref);

    return ref;
}

static ULONG WINAPI media_stream_Release(IMFMediaStream *iface)
{
    struct media_stream *stream = impl_from_IMFMediaStream(iface);
    ULONG ref = InterlockedDecrement(&stream->ref);

    TRACE("%p, refcount %lu.\n", iface, ref);

    if (!ref)
    {
        stream->active = FALSE;
        media_stream_stop(stream);
        IMFMediaSource_Release(stream->media_source);
        IMFStreamDescriptor_Release(stream->descriptor);
        IMFMediaEventQueue_Release(stream->event_queue);
        free(stream);
    }

    return ref;
}

static HRESULT WINAPI media_stream_GetEvent(IMFMediaStream *iface, DWORD flags, IMFMediaEvent **event)
{
    struct media_stream *stream = impl_from_IMFMediaStream(iface);

    TRACE("%p, %#lx, %p.\n", iface, flags, event);

    return IMFMediaEventQueue_GetEvent(stream->event_queue, flags, event);
}

static HRESULT WINAPI media_stream_BeginGetEvent(IMFMediaStream *iface, IMFAsyncCallback *callback, IUnknown *state)
{
    struct media_stream *stream = impl_from_IMFMediaStream(iface);

    TRACE("%p, %p, %p.\n", iface, callback, state);

    return IMFMediaEventQueue_BeginGetEvent(stream->event_queue, callback, state);
}

static HRESULT WINAPI media_stream_EndGetEvent(IMFMediaStream *iface, IMFAsyncResult *result, IMFMediaEvent **event)
{
    struct media_stream *stream = impl_from_IMFMediaStream(iface);

    TRACE("%p, %p, %p.\n", stream, result, event);

    return IMFMediaEventQueue_EndGetEvent(stream->event_queue, result, event);
}

static HRESULT WINAPI media_stream_QueueEvent(IMFMediaStream *iface, MediaEventType event_type, REFGUID ext_type,
        HRESULT hr, const PROPVARIANT *value)
{
    struct media_stream *stream = impl_from_IMFMediaStream(iface);

    TRACE("%p, %lu, %s, %#lx, %p.\n", iface, event_type, debugstr_guid(ext_type), hr, value);

    return IMFMediaEventQueue_QueueEventParamVar(stream->event_queue, event_type, ext_type, hr, value);
}

static HRESULT WINAPI media_stream_GetMediaSource(IMFMediaStream *iface, IMFMediaSource **out)
{
    struct media_stream *stream = impl_from_IMFMediaStream(iface);
    struct media_source *source = impl_from_IMFMediaSource(stream->media_source);
    HRESULT hr = S_OK;

    TRACE("%p, %p.\n", iface, out);

    EnterCriticalSection(&source->cs);

    if (source->state == SOURCE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else
    {
        IMFMediaSource_AddRef(&source->IMFMediaSource_iface);
        *out = &source->IMFMediaSource_iface;
    }

    LeaveCriticalSection(&source->cs);

    return hr;
}

static HRESULT WINAPI media_stream_GetStreamDescriptor(IMFMediaStream* iface, IMFStreamDescriptor **descriptor)
{
    struct media_stream *stream = impl_from_IMFMediaStream(iface);
    struct media_source *source = impl_from_IMFMediaSource(stream->media_source);
    HRESULT hr = S_OK;

    TRACE("%p, %p.\n", iface, descriptor);

    EnterCriticalSection(&source->cs);

    if (source->state == SOURCE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else
    {
        IMFStreamDescriptor_AddRef(stream->descriptor);
        *descriptor = stream->descriptor;
    }

    LeaveCriticalSection(&source->cs);

    return hr;
}

static HRESULT WINAPI media_stream_RequestSample(IMFMediaStream *iface, IUnknown *token)
{
    struct media_stream *stream = impl_from_IMFMediaStream(iface);
    struct media_source *source = impl_from_IMFMediaSource(stream->media_source);
    IMFSample *sample;
    HRESULT hr;

    TRACE("%p, %p.\n", iface, token);

    EnterCriticalSection(&source->cs);

    if (source->state == SOURCE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else if (!stream->active)
        hr = MF_E_MEDIA_SOURCE_WRONGSTATE;
    else if (stream->eos)
        hr = MF_E_END_OF_STREAM;
    else if (source->state == SOURCE_RUNNING && SUCCEEDED(hr = media_stream_pop_sample(stream, &sample)))
        hr = media_stream_send_sample(stream, sample, token);
    else if (SUCCEEDED(hr = media_stream_queue_token(stream, token)))
        hr = MFPutWorkItem(source->async_commands_queue, &source->async_read_iface, NULL);

    LeaveCriticalSection(&source->cs);

    return hr;
}

static const IMFMediaStreamVtbl media_stream_vtbl =
{
    media_stream_QueryInterface,
    media_stream_AddRef,
    media_stream_Release,
    media_stream_GetEvent,
    media_stream_BeginGetEvent,
    media_stream_EndGetEvent,
    media_stream_QueueEvent,
    media_stream_GetMediaSource,
    media_stream_GetStreamDescriptor,
    media_stream_RequestSample,
};

static HRESULT media_stream_create(IMFMediaSource *source, IMFStreamDescriptor *descriptor, struct media_stream **out)
{
    struct media_stream *object;
    HRESULT hr;

    TRACE("source %p, descriptor %p.\n", source, descriptor);

    if (!(object = calloc(1, sizeof(*object))))
        return E_OUTOFMEMORY;

    object->IMFMediaStream_iface.lpVtbl = &media_stream_vtbl;
    object->ref = 1;

    if (FAILED(hr = MFCreateEventQueue(&object->event_queue)))
    {
        free(object);
        return hr;
    }

    IMFMediaSource_AddRef(source);
    object->media_source = source;
    IMFStreamDescriptor_AddRef(descriptor);
    object->descriptor = descriptor;

    list_init(&object->tokens);
    list_init(&object->samples);

    TRACE("Created stream object %p.\n", object);

    *out = object;
    return S_OK;
}

static HRESULT WINAPI media_source_get_service_QueryInterface(IMFGetService *iface, REFIID riid, void **obj)
{
    struct media_source *source = impl_from_IMFGetService(iface);
    return IMFMediaSource_QueryInterface(&source->IMFMediaSource_iface, riid, obj);
}

static ULONG WINAPI media_source_get_service_AddRef(IMFGetService *iface)
{
    struct media_source *source = impl_from_IMFGetService(iface);
    return IMFMediaSource_AddRef(&source->IMFMediaSource_iface);
}

static ULONG WINAPI media_source_get_service_Release(IMFGetService *iface)
{
    struct media_source *source = impl_from_IMFGetService(iface);
    return IMFMediaSource_Release(&source->IMFMediaSource_iface);
}

static HRESULT WINAPI media_source_get_service_GetService(IMFGetService *iface, REFGUID service, REFIID riid, void **obj)
{
    struct media_source *source = impl_from_IMFGetService(iface);

    TRACE("%p, %s, %s, %p.\n", iface, debugstr_guid(service), debugstr_guid(riid), obj);

    *obj = NULL;

    if (IsEqualGUID(service, &MF_RATE_CONTROL_SERVICE))
    {
        if (IsEqualIID(riid, &IID_IMFRateSupport))
        {
            *obj = &source->IMFRateSupport_iface;
        }
        else if (IsEqualIID(riid, &IID_IMFRateControl))
        {
            *obj = &source->IMFRateControl_iface;
        }
    }
    else
        FIXME("Unsupported service %s.\n", debugstr_guid(service));

    if (*obj)
        IUnknown_AddRef((IUnknown *)*obj);

    return *obj ? S_OK : E_NOINTERFACE;
}

static const IMFGetServiceVtbl media_source_get_service_vtbl =
{
    media_source_get_service_QueryInterface,
    media_source_get_service_AddRef,
    media_source_get_service_Release,
    media_source_get_service_GetService,
};

static HRESULT WINAPI media_source_rate_support_QueryInterface(IMFRateSupport *iface, REFIID riid, void **obj)
{
    struct media_source *source = impl_from_IMFRateSupport(iface);
    return IMFMediaSource_QueryInterface(&source->IMFMediaSource_iface, riid, obj);
}

static ULONG WINAPI media_source_rate_support_AddRef(IMFRateSupport *iface)
{
    struct media_source *source = impl_from_IMFRateSupport(iface);
    return IMFMediaSource_AddRef(&source->IMFMediaSource_iface);
}

static ULONG WINAPI media_source_rate_support_Release(IMFRateSupport *iface)
{
    struct media_source *source = impl_from_IMFRateSupport(iface);
    return IMFMediaSource_Release(&source->IMFMediaSource_iface);
}

static HRESULT WINAPI media_source_rate_support_GetSlowestRate(IMFRateSupport *iface, MFRATE_DIRECTION direction, BOOL thin, float *rate)
{
    TRACE("%p, %d, %d, %p.\n", iface, direction, thin, rate);

    *rate = 0.0f;

    return S_OK;
}

static HRESULT WINAPI media_source_rate_support_GetFastestRate(IMFRateSupport *iface, MFRATE_DIRECTION direction, BOOL thin, float *rate)
{
    TRACE("%p, %d, %d, %p.\n", iface, direction, thin, rate);

    *rate = direction == MFRATE_FORWARD ? 1e6f : -1e6f;

    return S_OK;
}

static HRESULT WINAPI media_source_rate_support_IsRateSupported(IMFRateSupport *iface, BOOL thin, float rate,
        float *nearest_rate)
{
    TRACE("%p, %d, %f, %p.\n", iface, thin, rate, nearest_rate);

    if (nearest_rate)
        *nearest_rate = rate;

    return rate >= -1e6f && rate <= 1e6f ? S_OK : MF_E_UNSUPPORTED_RATE;
}

static const IMFRateSupportVtbl media_source_rate_support_vtbl =
{
    media_source_rate_support_QueryInterface,
    media_source_rate_support_AddRef,
    media_source_rate_support_Release,
    media_source_rate_support_GetSlowestRate,
    media_source_rate_support_GetFastestRate,
    media_source_rate_support_IsRateSupported,
};

static HRESULT WINAPI media_source_rate_control_QueryInterface(IMFRateControl *iface, REFIID riid, void **obj)
{
    struct media_source *source = impl_from_IMFRateControl(iface);
    return IMFMediaSource_QueryInterface(&source->IMFMediaSource_iface, riid, obj);
}

static ULONG WINAPI media_source_rate_control_AddRef(IMFRateControl *iface)
{
    struct media_source *source = impl_from_IMFRateControl(iface);
    return IMFMediaSource_AddRef(&source->IMFMediaSource_iface);
}

static ULONG WINAPI media_source_rate_control_Release(IMFRateControl *iface)
{
    struct media_source *source = impl_from_IMFRateControl(iface);
    return IMFMediaSource_Release(&source->IMFMediaSource_iface);
}

static HRESULT WINAPI media_source_rate_control_SetRate(IMFRateControl *iface, BOOL thin, float rate)
{
    struct media_source *source = impl_from_IMFRateControl(iface);
    HRESULT hr;

    FIXME("%p, %d, %f.\n", iface, thin, rate);

    if (rate < 0.0f)
        return MF_E_REVERSE_UNSUPPORTED;

    if (thin)
        return MF_E_THINNING_UNSUPPORTED;

    if (FAILED(hr = IMFRateSupport_IsRateSupported(&source->IMFRateSupport_iface, thin, rate, NULL)))
        return hr;

    EnterCriticalSection(&source->cs);
    source->rate = rate;
    LeaveCriticalSection(&source->cs);

    return IMFMediaEventQueue_QueueEventParamVar(source->event_queue, MESourceRateChanged, &GUID_NULL, S_OK, NULL);
}

static HRESULT WINAPI media_source_rate_control_GetRate(IMFRateControl *iface, BOOL *thin, float *rate)
{
    struct media_source *source = impl_from_IMFRateControl(iface);

    TRACE("%p, %p, %p.\n", iface, thin, rate);

    if (thin)
        *thin = FALSE;

    EnterCriticalSection(&source->cs);
    *rate = source->rate;
    LeaveCriticalSection(&source->cs);

    return S_OK;
}

static const IMFRateControlVtbl media_source_rate_control_vtbl =
{
    media_source_rate_control_QueryInterface,
    media_source_rate_control_AddRef,
    media_source_rate_control_Release,
    media_source_rate_control_SetRate,
    media_source_rate_control_GetRate,
};

static HRESULT WINAPI media_source_QueryInterface(IMFMediaSource *iface, REFIID riid, void **out)
{
    struct media_source *source = impl_from_IMFMediaSource(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), out);

    if (IsEqualIID(riid, &IID_IMFMediaSource) ||
        IsEqualIID(riid, &IID_IMFMediaEventGenerator) ||
        IsEqualIID(riid, &IID_IUnknown))
    {
        *out = &source->IMFMediaSource_iface;
    }
    else if (IsEqualIID(riid, &IID_IMFGetService))
    {
        *out = &source->IMFGetService_iface;
    }
    else
    {
        FIXME("%s, %p.\n", debugstr_guid(riid), out);
        *out = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*out);
    return S_OK;
}

static ULONG WINAPI media_source_AddRef(IMFMediaSource *iface)
{
    struct media_source *source = impl_from_IMFMediaSource(iface);
    ULONG ref = InterlockedIncrement(&source->ref);

    TRACE("%p, refcount %lu.\n", iface, ref);

    return ref;
}

static ULONG WINAPI media_source_Release(IMFMediaSource *iface)
{
    struct media_source *source = impl_from_IMFMediaSource(iface);
    ULONG ref = InterlockedDecrement(&source->ref);

    TRACE("%p, refcount %lu.\n", iface, ref);

    if (!ref)
    {
        IMFMediaSource_Shutdown(iface);
        IMFMediaEventQueue_Release(source->event_queue);
        IMFByteStream_Release(source->byte_stream);
        wineav_demuxer_destroy(source->demuxer);
        free(source->buffer);
        source->cs.DebugInfo->Spare[0] = 0;
        DeleteCriticalSection(&source->cs);
        free(source);
    }

    return ref;
}

static HRESULT WINAPI media_source_GetEvent(IMFMediaSource *iface, DWORD flags, IMFMediaEvent **event)
{
    struct media_source *source = impl_from_IMFMediaSource(iface);

    TRACE("%p, %#lx, %p.\n", iface, flags, event);

    return IMFMediaEventQueue_GetEvent(source->event_queue, flags, event);
}

static HRESULT WINAPI media_source_BeginGetEvent(IMFMediaSource *iface, IMFAsyncCallback *callback, IUnknown *state)
{
    struct media_source *source = impl_from_IMFMediaSource(iface);

    TRACE("%p, %p, %p.\n", iface, callback, state);

    return IMFMediaEventQueue_BeginGetEvent(source->event_queue, callback, state);
}

static HRESULT WINAPI media_source_EndGetEvent(IMFMediaSource *iface, IMFAsyncResult *result, IMFMediaEvent **event)
{
    struct media_source *source = impl_from_IMFMediaSource(iface);

    TRACE("%p, %p, %p.\n", iface, result, event);

    return IMFMediaEventQueue_EndGetEvent(source->event_queue, result, event);
}

static HRESULT WINAPI media_source_QueueEvent(IMFMediaSource *iface, MediaEventType event_type, REFGUID ext_type,
        HRESULT hr, const PROPVARIANT *value)
{
    struct media_source *source = impl_from_IMFMediaSource(iface);

    TRACE("%p, %lu, %s, %#lx, %p.\n", iface, event_type, debugstr_guid(ext_type), hr, value);

    return IMFMediaEventQueue_QueueEventParamVar(source->event_queue, event_type, ext_type, hr, value);
}

static HRESULT WINAPI media_source_GetCharacteristics(IMFMediaSource *iface, DWORD *characteristics)
{
    struct media_source *source = impl_from_IMFMediaSource(iface);
    HRESULT hr = S_OK;

    TRACE("%p, %p.\n", iface, characteristics);

    EnterCriticalSection(&source->cs);

    if (source->state == SOURCE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else
        *characteristics = MFMEDIASOURCE_CAN_SEEK | MFMEDIASOURCE_CAN_PAUSE;

    LeaveCriticalSection(&source->cs);

    return hr;
}

static HRESULT WINAPI media_source_CreatePresentationDescriptor(IMFMediaSource *iface, IMFPresentationDescriptor **descriptor)
{
    struct media_source *source = impl_from_IMFMediaSource(iface);
    HRESULT hr;
    UINT i;

    TRACE("%p, %p.\n", iface, descriptor);

    EnterCriticalSection(&source->cs);

    if (source->state == SOURCE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else if (SUCCEEDED(hr = MFCreatePresentationDescriptor(source->stream_count, source->descriptors, descriptor)))
    {
        if (FAILED(hr = IMFPresentationDescriptor_SetString(*descriptor, &MF_PD_MIME_TYPE, source->mime_type)))
            WARN("Failed to set presentation descriptor MF_PD_MIME_TYPE, hr %#lx\n", hr);
        if (FAILED(hr = IMFPresentationDescriptor_SetUINT64(*descriptor, &MF_PD_TOTAL_FILE_SIZE, source->file_size)))
            WARN("Failed to set presentation descriptor MF_PD_TOTAL_FILE_SIZE, hr %#lx\n", hr);
        if (FAILED(hr = IMFPresentationDescriptor_SetUINT64(*descriptor, &MF_PD_DURATION, source->duration)))
            WARN("Failed to set presentation descriptor MF_PD_DURATION, hr %#lx\n", hr);

        for (i = 0; i < source->stream_count; ++i)
        {
            if (!source->streams[i]->active)
                continue;
            if (FAILED(hr = IMFPresentationDescriptor_SelectStream(*descriptor, i)))
                WARN("Failed to select stream %u, hr %#lx\n", i, hr);
        }

        hr = S_OK;
    }

    LeaveCriticalSection(&source->cs);

    return hr;
}

static HRESULT WINAPI media_source_Start(IMFMediaSource *iface, IMFPresentationDescriptor *descriptor,
                                     const GUID *time_format, const PROPVARIANT *position)
{
    struct media_source *source = impl_from_IMFMediaSource(iface);
    IUnknown *op;
    HRESULT hr;

    TRACE("%p, %p, %p, %p.\n", iface, descriptor, time_format, position);

    EnterCriticalSection(&source->cs);

    if (source->state == SOURCE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else if (!(IsEqualIID(time_format, &GUID_NULL)))
        hr = MF_E_UNSUPPORTED_TIME_FORMAT;
    else if (SUCCEEDED(hr = async_start_params_create(descriptor, time_format, position, &op)))
    {
        hr = MFPutWorkItem(source->async_commands_queue, &source->async_start_iface, op);
        IUnknown_Release(op);
    }

    LeaveCriticalSection(&source->cs);

    return hr;
}

static HRESULT WINAPI media_source_Stop(IMFMediaSource *iface)
{
    struct media_source *source = impl_from_IMFMediaSource(iface);
    HRESULT hr;

    TRACE("%p.\n", iface);

    EnterCriticalSection(&source->cs);

    if (source->state == SOURCE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else
        hr = MFPutWorkItem(source->async_commands_queue, &source->async_stop_iface, NULL);

    LeaveCriticalSection(&source->cs);

    return hr;
}

static HRESULT WINAPI media_source_Pause(IMFMediaSource *iface)
{
    struct media_source *source = impl_from_IMFMediaSource(iface);
    HRESULT hr;

    TRACE("%p.\n", iface);

    EnterCriticalSection(&source->cs);

    if (source->state == SOURCE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else if (source->state != SOURCE_RUNNING)
        hr = MF_E_INVALID_STATE_TRANSITION;
    else
        hr = MFPutWorkItem(source->async_commands_queue, &source->async_pause_iface, NULL);

    LeaveCriticalSection(&source->cs);

    return hr;
}

static HRESULT WINAPI media_source_Shutdown(IMFMediaSource *iface)
{
    struct media_source *source = impl_from_IMFMediaSource(iface);

    TRACE("%p.\n", iface);

    EnterCriticalSection(&source->cs);

    if (source->state == SOURCE_SHUTDOWN)
    {
        LeaveCriticalSection(&source->cs);
        return MF_E_SHUTDOWN;
    }

    source->state = SOURCE_SHUTDOWN;

    IMFMediaEventQueue_Shutdown(source->event_queue);
    IMFByteStream_Close(source->byte_stream);

    while (source->stream_count--)
    {
        struct media_stream *stream = source->streams[source->stream_count];
        IMFStreamDescriptor_Release(source->descriptors[source->stream_count]);
        IMFMediaEventQueue_Shutdown(stream->event_queue);
        IMFMediaStream_Release(&stream->IMFMediaStream_iface);
    }
    free(source->stream_map);
    free(source->descriptors);
    free(source->streams);

    LeaveCriticalSection(&source->cs);

    MFUnlockWorkQueue(source->async_commands_queue);

    return S_OK;
}

static const IMFMediaSourceVtbl IMFMediaSource_vtbl =
{
    media_source_QueryInterface,
    media_source_AddRef,
    media_source_Release,
    media_source_GetEvent,
    media_source_BeginGetEvent,
    media_source_EndGetEvent,
    media_source_QueueEvent,
    media_source_GetCharacteristics,
    media_source_CreatePresentationDescriptor,
    media_source_Start,
    media_source_Stop,
    media_source_Pause,
    media_source_Shutdown,
};

static void media_source_init_stream_map(struct media_source *source, UINT stream_count)
{
    IMFMediaType *media_type;
    int i, n = 0;
    GUID major;

    if (wcscmp(source->mime_type, L"video/mp4"))
    {
        for (i = stream_count - 1; i >= 0; i--)
        {
            TRACE("mapping source %p stream %u to demuxer stream %u\n", source, i, i);
            source->stream_map[i] = i;
        }
        return;
    }

    for (i = stream_count - 1; i >= 0; i--)
    {
        if (SUCCEEDED(wineav_demuxer_stream_type(source->demuxer, i, &media_type)))
        {
            if (SUCCEEDED(IMFMediaType_GetMajorType(media_type, &major))
                    && IsEqualGUID(&major, &MFMediaType_Audio))
            {
                TRACE("mapping source %p stream %u to demuxer stream %u\n", source, n, i);
                source->stream_map[n++] = i;
            }
            IMFMediaType_Release(media_type);
        }
    }
    for (i = stream_count - 1; i >= 0; i--)
    {
        if (SUCCEEDED(wineav_demuxer_stream_type(source->demuxer, i, &media_type)))
        {
            if (SUCCEEDED(IMFMediaType_GetMajorType(media_type, &major))
                    && IsEqualGUID(&major, &MFMediaType_Video))
            {
                TRACE("mapping source %p stream %u to demuxer stream %u\n", source, n, i);
                source->stream_map[n++] = i;
            }
            IMFMediaType_Release(media_type);
        }
    }
    for (i = stream_count - 1; i >= 0; i--)
    {
        if (SUCCEEDED(wineav_demuxer_stream_type(source->demuxer, i, &media_type)))
        {
            if (SUCCEEDED(IMFMediaType_GetMajorType(media_type, &major))
                    && !IsEqualGUID(&major, &MFMediaType_Audio)
                    && !IsEqualGUID(&major, &MFMediaType_Video))
            {
                TRACE("mapping source %p stream %u to demuxer stream %u\n", source, n, i);
                source->stream_map[n++] = i;
            }
            IMFMediaType_Release(media_type);
        }
    }
}

static void media_source_init_descriptors(struct media_source *source)
{
    UINT i, last_audio = -1, last_video = -1, first_audio = -1, first_video = -1;
    HRESULT hr;

    for (i = 0; i < source->stream_count; i++)
    {
        IMFStreamDescriptor *descriptor = source->descriptors[i];
        GUID major = GUID_NULL;
        UINT exclude = -1;
        WCHAR buffer[512];

        if (FAILED(hr = stream_descriptor_get_major_type(descriptor, &major)))
            WARN("Failed to get major type from stream descriptor, hr %#lx\n", hr);

        if (IsEqualGUID(&major, &MFMediaType_Audio))
        {
            if (first_audio == -1)
                first_audio = i;
            exclude = last_audio;
            last_audio = i;
        }
        else if (IsEqualGUID(&major, &MFMediaType_Video))
        {
            if (first_video == -1)
                first_video = i;
            exclude = last_video;
            last_video = i;
        }

        if (exclude != -1)
        {
            if (FAILED(IMFStreamDescriptor_SetUINT32(source->descriptors[exclude], &MF_SD_MUTUALLY_EXCLUSIVE, 1)))
                WARN("Failed to set stream %u MF_SD_MUTUALLY_EXCLUSIVE\n", exclude);
            else if (FAILED(IMFStreamDescriptor_SetUINT32(descriptor, &MF_SD_MUTUALLY_EXCLUSIVE, 1)))
                WARN("Failed to set stream %u MF_SD_MUTUALLY_EXCLUSIVE\n", i);
        }

        if (FAILED(hr = wineav_demuxer_stream_lang(source->demuxer, source->stream_map[i], buffer, ARRAY_SIZE(buffer)))
                || FAILED(IMFStreamDescriptor_SetString(descriptor, &MF_SD_LANGUAGE, buffer)))
            WARN("Failed to set stream descriptor language, hr %#lx\n", hr);
        if (FAILED(hr = wineav_demuxer_stream_name(source->demuxer, source->stream_map[i], buffer, ARRAY_SIZE(buffer)))
                || FAILED(IMFStreamDescriptor_SetString(descriptor, &MF_SD_STREAM_NAME, buffer)))
            WARN("Failed to set stream descriptor name, hr %#lx\n", hr);
    }

    if (!wcscmp(source->mime_type, L"video/mp4"))
    {
        if (last_audio != -1)
            source->streams[last_audio]->active = TRUE;
        if (last_video != -1)
            source->streams[last_video]->active = TRUE;
    }
    else
    {
        if (first_audio != -1)
            source->streams[first_audio]->active = TRUE;
        if (first_video != -1)
            source->streams[first_video]->active = TRUE;
    }
}

static HRESULT media_source_create(struct object_context *context, IMFMediaSource **out)
{
    struct media_source *object;
    UINT stream_count;
    IStream *stream;
    unsigned int i;
    HRESULT hr;

    if (!(object = calloc(1, sizeof(*object))))
        return E_OUTOFMEMORY;

    object->IMFMediaSource_iface.lpVtbl = &IMFMediaSource_vtbl;
    object->IMFGetService_iface.lpVtbl = &media_source_get_service_vtbl;
    object->IMFRateSupport_iface.lpVtbl = &media_source_rate_support_vtbl;
    object->IMFRateControl_iface.lpVtbl = &media_source_rate_control_vtbl;
    object->async_start_iface.lpVtbl = &async_start_vtbl;
    object->async_pause_iface.lpVtbl = &async_pause_vtbl;
    object->async_stop_iface.lpVtbl = &async_stop_vtbl;
    object->async_read_iface.lpVtbl = &async_read_vtbl;
    object->ref = 1;
    object->byte_stream = context->stream;
    IMFByteStream_AddRef(context->stream);
    object->rate = 1.0f;
    InitializeCriticalSectionEx(&object->cs, 0, RTL_CRITICAL_SECTION_FLAG_FORCE_DEBUG_INFO);
    object->cs.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": cs");

    if (FAILED(hr = MFCreateEventQueue(&object->event_queue)))
        goto fail;
    if (FAILED(hr = MFAllocateWorkQueue(&object->async_commands_queue)))
        goto fail;

    if (FAILED(hr = MFCreateStreamOnMFByteStream(context->stream, &stream)))
        goto fail;
    if (FAILED(hr = wineav_demuxer_create(context->url, stream, &object->file_size, &object->duration,
            &stream_count, object->mime_type, &object->demuxer)))
    {
        IStream_Release(stream);
        goto fail;
    }
    IStream_Release(stream);
    object->state = SOURCE_OPENING;

    if (!(object->descriptors = calloc(stream_count, sizeof(*object->descriptors)))
            || !(object->stream_map = calloc(stream_count, sizeof(*object->stream_map)))
            || !(object->streams = calloc(stream_count, sizeof(*object->streams))))
    {
        hr = E_OUTOFMEMORY;
        goto fail;
    }

    media_source_init_stream_map(object, stream_count);

    for (i = 0; i < stream_count; ++i)
    {
        IMFStreamDescriptor *descriptor;
        struct media_stream *stream;
        IMFMediaType *media_type;

        if (FAILED(hr = wineav_demuxer_stream_type(object->demuxer, object->stream_map[i], &media_type)))
            goto fail;
        if (FAILED(hr = stream_descriptor_create(i + 1, media_type, &descriptor)))
        {
            IMFMediaType_Release(media_type);
            goto fail;
        }
        IMFMediaType_Release(media_type);
        if (FAILED(hr = media_stream_create(&object->IMFMediaSource_iface, descriptor, &stream)))
        {
            IMFStreamDescriptor_Release(descriptor);
            goto fail;
        }

        IMFStreamDescriptor_AddRef(descriptor);
        object->descriptors[i] = descriptor;
        object->streams[i] = stream;
        object->stream_count++;
    }

    media_source_init_descriptors(object);
    object->state = SOURCE_STOPPED;

    *out = &object->IMFMediaSource_iface;
    TRACE("Created IMFMediaSource %p\n", *out);
    return S_OK;

fail:
    WARN("Failed to construct MFMediaSource, hr %#lx.\n", hr);

    while (object->streams && object->stream_count--)
    {
        struct media_stream *stream = object->streams[object->stream_count];
        IMFStreamDescriptor_Release(object->descriptors[object->stream_count]);
        IMFMediaStream_Release(&stream->IMFMediaStream_iface);
    }
    free(object->stream_map);
    free(object->descriptors);
    free(object->streams);

    if (object->demuxer)
        wineav_demuxer_destroy(object->demuxer);
    if (object->async_commands_queue)
        MFUnlockWorkQueue(object->async_commands_queue);
    if (object->event_queue)
        IMFMediaEventQueue_Release(object->event_queue);
    IMFByteStream_Release(object->byte_stream);
    free(object->buffer);
    free(object);
    return hr;
}

struct byte_stream_handler
{
    IMFByteStreamHandler IMFByteStreamHandler_iface;
    IMFAsyncCallback async_create_object_iface;
    LONG refcount;
    struct list results;
    CRITICAL_SECTION cs;
};

static HRESULT byte_stream_handler_pop_result(struct byte_stream_handler *handler, IMFAsyncResult *result, IUnknown **object)
{
    struct object_context *context;
    struct object_entry *entry;

    LIST_FOR_EACH_ENTRY(entry, &handler->results, struct object_entry, entry)
    {
        context = impl_from_IUnknown(entry->object);
        if (context->result == result) break;
    }
    if (&entry->entry == &handler->results) return MF_E_UNEXPECTED;

    if ((*object = entry->object)) IUnknown_AddRef(*object);
    list_remove(&entry->entry);
    object_entry_destroy(entry);

    return S_OK;
}

static struct byte_stream_handler *impl_from_IMFByteStreamHandler(IMFByteStreamHandler *iface)
{
    return CONTAINING_RECORD(iface, struct byte_stream_handler, IMFByteStreamHandler_iface);
}

static HRESULT WINAPI byte_stream_handler_QueryInterface(IMFByteStreamHandler *iface, REFIID riid, void **obj)
{
    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), obj);

    if (IsEqualIID(riid, &IID_IMFByteStreamHandler) ||
            IsEqualIID(riid, &IID_IUnknown))
    {
        *obj = iface;
        IMFByteStreamHandler_AddRef(iface);
        return S_OK;
    }

    WARN("Unsupported %s.\n", debugstr_guid(riid));
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI byte_stream_handler_AddRef(IMFByteStreamHandler *iface)
{
    struct byte_stream_handler *handler = impl_from_IMFByteStreamHandler(iface);
    ULONG refcount = InterlockedIncrement(&handler->refcount);

    TRACE("%p, refcount %lu.\n", handler, refcount);

    return refcount;
}

static ULONG WINAPI byte_stream_handler_Release(IMFByteStreamHandler *iface)
{
    struct byte_stream_handler *handler = impl_from_IMFByteStreamHandler(iface);
    ULONG refcount = InterlockedDecrement(&handler->refcount);
    struct object_entry *enrtry, *next;

    TRACE("%p, refcount %lu.\n", iface, refcount);

    if (!refcount)
    {
        LIST_FOR_EACH_ENTRY_SAFE(enrtry, next, &handler->results, struct object_entry, entry)
        {
            list_remove(&enrtry->entry);
            object_entry_destroy(enrtry);
        }
        DeleteCriticalSection(&handler->cs);
        free(handler);
    }

    return refcount;
}

static HRESULT WINAPI byte_stream_handler_BeginCreateObject(IMFByteStreamHandler *iface, IMFByteStream *stream, const WCHAR *url, DWORD flags,
        IPropertyStore *props, IUnknown **cancel_cookie, IMFAsyncCallback *callback, IUnknown *state)
{
    struct byte_stream_handler *handler = impl_from_IMFByteStreamHandler(iface);
    IMFAsyncResult *result;
    IUnknown *context;
    HRESULT hr;
    DWORD caps;

    TRACE("%p, %s, %#lx, %p, %p, %p, %p.\n", iface, debugstr_w(url), flags, props, cancel_cookie, callback, state);

    if (cancel_cookie)
        *cancel_cookie = NULL;

    if (!stream)
        return E_INVALIDARG;
    if (flags != MF_RESOLUTION_MEDIASOURCE)
        FIXME("Unimplemented flags %#lx\n", flags);

    if (FAILED(hr = IMFByteStream_GetCapabilities(stream, &caps)))
        return hr;
    if (!(caps & MFBYTESTREAM_IS_SEEKABLE))
    {
        FIXME("Non-seekable bytestreams not supported.\n");
        return MF_E_BYTESTREAM_NOT_SEEKABLE;
    }
    if (FAILED(hr = MFCreateAsyncResult(NULL, callback, state, &result)))
        return hr;
    if (FAILED(hr = object_context_create(url, stream, result, &context)))
        goto done;

    hr = MFPutWorkItem(MFASYNC_CALLBACK_QUEUE_IO, &handler->async_create_object_iface, context);
    IUnknown_Release(context);

    if (SUCCEEDED(hr) && cancel_cookie)
    {
        *cancel_cookie = (IUnknown *)result;
        IUnknown_AddRef(*cancel_cookie);
    }

done:
    IMFAsyncResult_Release(result);

    return hr;
}

static HRESULT WINAPI byte_stream_handler_EndCreateObject(IMFByteStreamHandler *iface, IMFAsyncResult *result,
        MF_OBJECT_TYPE *type, IUnknown **object)
{
    struct byte_stream_handler *handler = impl_from_IMFByteStreamHandler(iface);
    IUnknown *unk;
    HRESULT hr;

    TRACE("%p, %p, %p, %p.\n", iface, result, type, object);

    *type = MF_OBJECT_INVALID;
    *object = NULL;

    if (SUCCEEDED(hr = byte_stream_handler_pop_result(handler, result, &unk)))
    {
        struct object_context *context = impl_from_IUnknown(unk);
        hr = IMFAsyncResult_GetStatus(context->result);
        *type = MF_OBJECT_MEDIASOURCE;
        *object = context->object;
        IUnknown_AddRef(*object);
        IUnknown_Release(unk);
    }

    return hr;
}

static HRESULT WINAPI byte_stream_handler_CancelObjectCreation(IMFByteStreamHandler *iface, IUnknown *cookie)
{
    struct byte_stream_handler *handler = impl_from_IMFByteStreamHandler(iface);
    IMFAsyncResult *result = (IMFAsyncResult *)cookie;
    IUnknown *object;
    HRESULT hr;

    TRACE("%p, %p.\n", iface, cookie);

    if (SUCCEEDED(hr = byte_stream_handler_pop_result(handler, result, &object)))
        IUnknown_Release(object);

    return hr;
}

static HRESULT WINAPI byte_stream_handler_GetMaxNumberOfBytesRequiredForResolution(IMFByteStreamHandler *iface, QWORD *bytes)
{
    FIXME("stub (%p %p)\n", iface, bytes);
    return E_NOTIMPL;
}

static const IMFByteStreamHandlerVtbl byte_stream_handler_vtbl =
{
    byte_stream_handler_QueryInterface,
    byte_stream_handler_AddRef,
    byte_stream_handler_Release,
    byte_stream_handler_BeginCreateObject,
    byte_stream_handler_EndCreateObject,
    byte_stream_handler_CancelObjectCreation,
    byte_stream_handler_GetMaxNumberOfBytesRequiredForResolution,
};

DEFINE_MF_ASYNC_CALLBACK(async_create_object, struct byte_stream_handler, IMFByteStreamHandler_iface)

static HRESULT WINAPI async_create_object_Invoke(IMFAsyncCallback *iface, IMFAsyncResult *result)
{
    struct byte_stream_handler *handler = impl_from_async_create_object(iface);
    IUnknown *state = IMFAsyncResult_GetStateNoAddRef(result);
    struct object_context *context;
    struct object_entry *entry;
    HRESULT hr;

    if (!state || !(context = impl_from_IUnknown(state)))
        return E_INVALIDARG;

    if (FAILED(hr = media_source_create(context, (IMFMediaSource **)&context->object)))
        WARN("Failed to create media source, hr %#lx\n", hr);
    else if (FAILED(hr = object_entry_create(state, &entry)))
        WARN("Failed to create handler result, hr %#lx\n", hr);
    else
    {
        EnterCriticalSection(&handler->cs);
        list_add_tail(&handler->results, &entry->entry);
        LeaveCriticalSection(&handler->cs);
    }

    IMFAsyncResult_SetStatus(context->result, hr);
    MFInvokeCallback(context->result);

    return S_OK;
}

HRESULT byte_stream_handler_construct(REFIID riid, void **obj)
{
    struct byte_stream_handler *handler;
    HRESULT hr;

    TRACE("%s, %p.\n", debugstr_guid(riid), obj);

    if (!(handler = calloc(1, sizeof(*handler))))
        return E_OUTOFMEMORY;

    list_init(&handler->results);
    InitializeCriticalSection(&handler->cs);

    handler->IMFByteStreamHandler_iface.lpVtbl = &byte_stream_handler_vtbl;
    handler->async_create_object_iface.lpVtbl = &async_create_object_vtbl;
    handler->refcount = 1;

    hr = IMFByteStreamHandler_QueryInterface(&handler->IMFByteStreamHandler_iface, riid, obj);
    IMFByteStreamHandler_Release(&handler->IMFByteStreamHandler_iface);

    return hr;
}
