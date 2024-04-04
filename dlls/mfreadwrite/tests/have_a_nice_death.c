/*
 * Copyright 2018 Alistair Leslie-Hughes
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
#include <stdbool.h>
#include <string.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "winreg.h"

#include "initguid.h"
#include "ole2.h"
#include "propvarutil.h"

DEFINE_GUID(GUID_NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

#undef INITGUID
#include "guiddef.h"
#include "mfapi.h"
#include "mfidl.h"
#include "mferror.h"
#include "mfreadwrite.h"
#include "d3d9.h"
#include "dxva2api.h"

#include "wine/test.h"

struct attribute_desc
{
    const GUID *key;
    const char *name;
    PROPVARIANT value;
    BOOL ratio;
    BOOL required;
    BOOL todo;
    BOOL todo_value;
};
typedef struct attribute_desc media_type_desc[32];

#define ATTR_GUID(k, g, ...)      {.key = &k, .name = #k, {.vt = VT_CLSID, .puuid = (GUID *)&g}, __VA_ARGS__ }
#define ATTR_UINT32(k, v, ...)    {.key = &k, .name = #k, {.vt = VT_UI4, .ulVal = v}, __VA_ARGS__ }
#define ATTR_BLOB(k, p, n, ...)   {.key = &k, .name = #k, {.vt = VT_VECTOR | VT_UI1, .caub = {.pElems = (void *)p, .cElems = n}}, __VA_ARGS__ }
#define ATTR_RATIO(k, n, d, ...)  {.key = &k, .name = #k, {.vt = VT_UI8, .uhVal = {.HighPart = n, .LowPart = d}}, .ratio = TRUE, __VA_ARGS__ }
#define ATTR_UINT64(k, v, ...)    {.key = &k, .name = #k, {.vt = VT_UI8, .uhVal = {.QuadPart = v}}, __VA_ARGS__ }

#define check_media_type(a, b, c) check_attributes_(__FILE__, __LINE__, (IMFAttributes *)a, b, c)
#define check_attributes(a, b, c) check_attributes_(__FILE__, __LINE__, a, b, c)
extern void check_attributes_(const char *file, int line, IMFAttributes *attributes,
        const struct attribute_desc *desc, ULONG limit);
extern void init_media_type(IMFMediaType *mediatype, const struct attribute_desc *desc, ULONG limit);

void check_attributes_(const char *file, int line, IMFAttributes *attributes,
        const struct attribute_desc *desc, ULONG limit)
{
    char buffer[256], *buf = buffer;
    PROPVARIANT value;
    int i, j, ret;
    HRESULT hr;

    for (i = 0; i < limit && desc[i].key; ++i)
    {
        hr = IMFAttributes_GetItem(attributes, desc[i].key, &value);
        todo_wine_if(desc[i].todo)
        ok_(file, line)(hr == S_OK, "%s missing, hr %#lx\n", debugstr_a(desc[i].name), hr);
        if (hr != S_OK) continue;

        switch (value.vt)
        {
        default: sprintf(buffer, "??"); break;
        case VT_CLSID: sprintf(buffer, "%s", debugstr_guid(value.puuid)); break;
        case VT_UI4: sprintf(buffer, "%lu", value.ulVal); break;
        case VT_UI8:
            if (desc[i].ratio)
                sprintf(buffer, "%lu:%lu", value.uhVal.HighPart, value.uhVal.LowPart);
            else
                sprintf(buffer, "%I64u", value.uhVal.QuadPart);
            break;
        case VT_VECTOR | VT_UI1:
            buf += sprintf(buf, "size %lu, data {", value.caub.cElems);
            for (j = 0; j < 16 && j < value.caub.cElems; ++j)
                buf += sprintf(buf, "0x%02x,", value.caub.pElems[j]);
            if (value.caub.cElems > 16)
                buf += sprintf(buf, "...}");
            else
                buf += sprintf(buf - (j ? 1 : 0), "}");
            break;
        }

        ret = PropVariantCompareEx(&value, &desc[i].value, 0, 0);
        todo_wine_if(desc[i].todo_value)
        ok_(file, line)(ret == 0, "%s mismatch, type %u, value %s\n",
                debugstr_a(desc[i].name), value.vt, buffer);
        PropVariantClear(&value);
    }
}

void init_media_type(IMFMediaType *mediatype, const struct attribute_desc *desc, ULONG limit)
{
    HRESULT hr;
    ULONG i;

    hr = IMFMediaType_DeleteAllItems(mediatype);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

    for (i = 0; i < limit && desc[i].key; ++i)
    {
        hr = IMFMediaType_SetItem(mediatype, desc[i].key, &desc[i].value);
        ok(hr == S_OK, "SetItem %s returned %#lx\n", debugstr_a(desc[i].name), hr);
    }
}

static ULONG get_refcount(void *iface)
{
    IUnknown *unknown = iface;
    IUnknown_AddRef(unknown);
    return IUnknown_Release(unknown);
}

static HWND create_window(void)
{
    RECT r = {0, 0, 640, 480};

    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW | WS_VISIBLE, FALSE);

    return CreateWindowA("static", "mfreadwrite_test", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            0, 0, r.right - r.left, r.bottom - r.top, NULL, NULL, NULL, NULL);
}

static IDirect3DDevice9 *create_d3d9_device(IDirect3D9 *d3d9, HWND focus_window)
{
    D3DPRESENT_PARAMETERS present_parameters = {0};
    IDirect3DDevice9 *device = NULL;

    present_parameters.BackBufferWidth = 640;
    present_parameters.BackBufferHeight = 480;
    present_parameters.BackBufferFormat = D3DFMT_X8R8G8B8;
    present_parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    present_parameters.hDeviceWindow = focus_window;
    present_parameters.Windowed = TRUE;
    present_parameters.EnableAutoDepthStencil = TRUE;
    present_parameters.AutoDepthStencilFormat = D3DFMT_D24S8;
    present_parameters.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;

    IDirect3D9_CreateDevice(d3d9, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, focus_window,
            D3DCREATE_HARDWARE_VERTEXPROCESSING, &present_parameters, &device);

    return device;
}

static HRESULT (WINAPI *pMFCreateMFByteStreamOnStream)(IStream *stream, IMFByteStream **bytestream);

static void init_functions(void)
{
    HMODULE mod = GetModuleHandleA("mfplat.dll");

#define X(f) if (!(p##f = (void*)GetProcAddress(mod, #f))) return;
    X(MFCreateMFByteStreamOnStream);
#undef X
}

enum source_state
{
    SOURCE_STOPPED = 0,
    SOURCE_RUNNING,
};

struct test_media_stream
{
    IMFMediaStream IMFMediaStream_iface;
    LONG refcount;
    IMFMediaSource *source;
    IMFStreamDescriptor *sd;
    IMFMediaEventQueue *event_queue;
    BOOL is_new;
    LONGLONG sample_duration, sample_time;
};

static struct test_media_stream *impl_from_IMFMediaStream(IMFMediaStream *iface)
{
    return CONTAINING_RECORD(iface, struct test_media_stream, IMFMediaStream_iface);
}

static HRESULT WINAPI test_media_stream_QueryInterface(IMFMediaStream *iface, REFIID riid, void **out)
{
    if (IsEqualIID(riid, &IID_IMFMediaStream)
            || IsEqualIID(riid, &IID_IMFMediaEventGenerator)
            || IsEqualIID(riid, &IID_IUnknown))
    {
        *out = iface;
    }
    else
    {
        *out = NULL;
        return E_NOINTERFACE;
    }

    IMFMediaStream_AddRef(iface);
    return S_OK;
}

static ULONG WINAPI test_media_stream_AddRef(IMFMediaStream *iface)
{
    struct test_media_stream *stream = impl_from_IMFMediaStream(iface);
    return InterlockedIncrement(&stream->refcount);
}

static ULONG WINAPI test_media_stream_Release(IMFMediaStream *iface)
{
    struct test_media_stream *stream = impl_from_IMFMediaStream(iface);
    ULONG refcount = InterlockedDecrement(&stream->refcount);

    if (!refcount)
    {
        IMFMediaEventQueue_Release(stream->event_queue);
        free(stream);
    }

    return refcount;
}

static HRESULT WINAPI test_media_stream_GetEvent(IMFMediaStream *iface, DWORD flags, IMFMediaEvent **event)
{
    struct test_media_stream *stream = impl_from_IMFMediaStream(iface);
    return IMFMediaEventQueue_GetEvent(stream->event_queue, flags, event);
}

static HRESULT WINAPI test_media_stream_BeginGetEvent(IMFMediaStream *iface, IMFAsyncCallback *callback, IUnknown *state)
{
    struct test_media_stream *stream = impl_from_IMFMediaStream(iface);
    ok(callback != NULL && state == (IUnknown *)iface, "Unexpected arguments.\n");
    return IMFMediaEventQueue_BeginGetEvent(stream->event_queue, callback, state);
}

static HRESULT WINAPI test_media_stream_EndGetEvent(IMFMediaStream *iface, IMFAsyncResult *result, IMFMediaEvent **event)
{
    struct test_media_stream *stream = impl_from_IMFMediaStream(iface);
    ok(!!result && !!event, "Unexpected arguments.\n");
    return IMFMediaEventQueue_EndGetEvent(stream->event_queue, result, event);
}

static HRESULT WINAPI test_media_stream_QueueEvent(IMFMediaStream *iface, MediaEventType event_type, REFGUID ext_type,
        HRESULT hr, const PROPVARIANT *value)
{
    struct test_media_stream *stream = impl_from_IMFMediaStream(iface);
    return IMFMediaEventQueue_QueueEventParamVar(stream->event_queue, event_type, ext_type, hr, value);
}

static HRESULT WINAPI test_media_stream_GetMediaSource(IMFMediaStream *iface, IMFMediaSource **source)
{
    struct test_media_stream *stream = impl_from_IMFMediaStream(iface);

    *source = stream->source;
    IMFMediaSource_AddRef(*source);

    return S_OK;
}

static HRESULT WINAPI test_media_stream_GetStreamDescriptor(IMFMediaStream *iface, IMFStreamDescriptor **sd)
{
    struct test_media_stream *stream = impl_from_IMFMediaStream(iface);

    *sd = stream->sd;
    IMFStreamDescriptor_AddRef(*sd);

    return S_OK;
}

static BOOL fail_request_sample;

static HRESULT WINAPI test_media_stream_RequestSample(IMFMediaStream *iface, IUnknown *token)
{
    struct test_media_stream *stream = impl_from_IMFMediaStream(iface);
    IMFMediaBuffer *buffer;
    IMFSample *sample;
    HRESULT hr;

    if (fail_request_sample)
        return E_NOTIMPL;

    hr = MFCreateSample(&sample);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    if (stream->sample_duration)
    {
        hr = IMFSample_SetSampleDuration(sample, stream->sample_duration);
        ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

        hr = IMFSample_SetSampleTime(sample, stream->sample_time);
        ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

        stream->sample_time += stream->sample_duration;
    }
    else
    {
        hr = IMFSample_SetSampleTime(sample, 123);
        ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

        hr = IMFSample_SetSampleDuration(sample, 1);
        ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    }

    if (token)
        IMFSample_SetUnknown(sample, &MFSampleExtension_Token, token);

    /* Reader expects buffers, empty samples are considered an error. */
    hr = MFCreateMemoryBuffer(8, &buffer);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    hr = IMFSample_AddBuffer(sample, buffer);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    IMFMediaBuffer_Release(buffer);

    hr = IMFMediaEventQueue_QueueEventParamUnk(stream->event_queue, MEMediaSample, &GUID_NULL, S_OK,
            (IUnknown *)sample);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    IMFSample_Release(sample);

    return S_OK;
}

static const IMFMediaStreamVtbl test_media_stream_vtbl =
{
    test_media_stream_QueryInterface,
    test_media_stream_AddRef,
    test_media_stream_Release,
    test_media_stream_GetEvent,
    test_media_stream_BeginGetEvent,
    test_media_stream_EndGetEvent,
    test_media_stream_QueueEvent,
    test_media_stream_GetMediaSource,
    test_media_stream_GetStreamDescriptor,
    test_media_stream_RequestSample,
};

#define TEST_SOURCE_NUM_STREAMS 3

struct test_source
{
    IMFMediaSource IMFMediaSource_iface;
    LONG refcount;
    IMFMediaEventQueue *event_queue;
    IMFPresentationDescriptor *pd;
    struct test_media_stream *streams[TEST_SOURCE_NUM_STREAMS];
    unsigned stream_count;
    enum source_state state;
    CRITICAL_SECTION cs;
};

static struct test_source *impl_from_IMFMediaSource(IMFMediaSource *iface)
{
    return CONTAINING_RECORD(iface, struct test_source, IMFMediaSource_iface);
}

static HRESULT WINAPI test_source_QueryInterface(IMFMediaSource *iface, REFIID riid, void **out)
{
    if (IsEqualIID(riid, &IID_IMFMediaSource)
            || IsEqualIID(riid, &IID_IMFMediaEventGenerator)
            || IsEqualIID(riid, &IID_IUnknown))
    {
        *out = iface;
    }
    else
    {
        *out = NULL;
        return E_NOINTERFACE;
    }

    IMFMediaSource_AddRef(iface);
    return S_OK;
}

static ULONG WINAPI test_source_AddRef(IMFMediaSource *iface)
{
    struct test_source *source = impl_from_IMFMediaSource(iface);
    return InterlockedIncrement(&source->refcount);
}

static ULONG WINAPI test_source_Release(IMFMediaSource *iface)
{
    struct test_source *source = impl_from_IMFMediaSource(iface);
    ULONG refcount = InterlockedDecrement(&source->refcount);

    if (!refcount)
    {
        IMFMediaEventQueue_Release(source->event_queue);
        free(source);
    }

    return refcount;
}

static HRESULT WINAPI test_source_GetEvent(IMFMediaSource *iface, DWORD flags, IMFMediaEvent **event)
{
    struct test_source *source = impl_from_IMFMediaSource(iface);
    return IMFMediaEventQueue_GetEvent(source->event_queue, flags, event);
}

static HRESULT WINAPI test_source_BeginGetEvent(IMFMediaSource *iface, IMFAsyncCallback *callback, IUnknown *state)
{
    struct test_source *source = impl_from_IMFMediaSource(iface);
    ok(callback != NULL && state == (IUnknown *)iface, "Unexpected arguments source %p, %p, state %p.\n", iface, callback, state);
    return IMFMediaEventQueue_BeginGetEvent(source->event_queue, callback, state);
}

static HRESULT WINAPI test_source_EndGetEvent(IMFMediaSource *iface, IMFAsyncResult *result, IMFMediaEvent **event)
{
    struct test_source *source = impl_from_IMFMediaSource(iface);
    return IMFMediaEventQueue_EndGetEvent(source->event_queue, result, event);
}

static HRESULT WINAPI test_source_QueueEvent(IMFMediaSource *iface, MediaEventType event_type, REFGUID ext_type,
        HRESULT hr, const PROPVARIANT *value)
{
    struct test_source *source = impl_from_IMFMediaSource(iface);
    return IMFMediaEventQueue_QueueEventParamVar(source->event_queue, event_type, ext_type, hr, value);
}

static HRESULT WINAPI test_source_GetCharacteristics(IMFMediaSource *iface, DWORD *flags)
{
    *flags = MFMEDIASOURCE_CAN_SEEK;

    return S_OK;
}

static HRESULT WINAPI test_source_CreatePresentationDescriptor(IMFMediaSource *iface, IMFPresentationDescriptor **pd)
{
    struct test_source *source = impl_from_IMFMediaSource(iface);
    IMFStreamDescriptor *sds[ARRAY_SIZE(source->streams)];
    IMFMediaType *media_type;
    HRESULT hr = S_OK;
    int i;

    EnterCriticalSection(&source->cs);

    if (source->pd)
    {
        *pd = source->pd;
        IMFPresentationDescriptor_AddRef(*pd);
    }
    else
    {
        for (i = 0; i < source->stream_count; ++i)
        {
            hr = MFCreateMediaType(&media_type);
            ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

            hr = IMFMediaType_SetGUID(media_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
            ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
            hr = IMFMediaType_SetGUID(media_type, &MF_MT_SUBTYPE, &MFAudioFormat_PCM);
            ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
            hr = IMFMediaType_SetUINT32(media_type, &MF_MT_AUDIO_BITS_PER_SAMPLE, 32);
            ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

            hr = MFCreateStreamDescriptor(i, 1, &media_type, &sds[i]);
            ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

            IMFMediaType_Release(media_type);
        }

        hr = MFCreatePresentationDescriptor(source->stream_count, sds, &source->pd);
        ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
        for (i = 0; i < source->stream_count; ++i)
            IMFStreamDescriptor_Release(sds[i]);

        *pd = source->pd;
        IMFPresentationDescriptor_AddRef(*pd);
    }

    LeaveCriticalSection(&source->cs);

    return hr;
}

static BOOL is_stream_selected(IMFPresentationDescriptor *pd, DWORD index)
{
    IMFStreamDescriptor *sd;
    BOOL selected = FALSE;

    if (SUCCEEDED(IMFPresentationDescriptor_GetStreamDescriptorByIndex(pd, index, &selected, &sd)))
        IMFStreamDescriptor_Release(sd);

    return selected;
}

static HRESULT WINAPI test_source_Start(IMFMediaSource *iface, IMFPresentationDescriptor *pd, const GUID *time_format,
        const PROPVARIANT *start_position)
{
    struct test_source *source = impl_from_IMFMediaSource(iface);
    MediaEventType event_type;
    PROPVARIANT var;
    HRESULT hr;
    int i;

    ok(time_format && IsEqualGUID(time_format, &GUID_NULL), "Unexpected time format %s.\n",
            wine_dbgstr_guid(time_format));
    ok(start_position && (start_position->vt == VT_I8 || start_position->vt == VT_EMPTY),
            "Unexpected position type.\n");

    EnterCriticalSection(&source->cs);

    event_type = source->state == SOURCE_RUNNING ? MESourceSeeked : MESourceStarted;
    hr = IMFMediaEventQueue_QueueEventParamVar(source->event_queue, event_type, &GUID_NULL, S_OK, NULL);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

    for (i = 0; i < source->stream_count; ++i)
    {
        if (!is_stream_selected(pd, i))
            continue;

        var.vt = VT_UNKNOWN;
        var.punkVal = (IUnknown *)&source->streams[i]->IMFMediaStream_iface;
        event_type = source->streams[i]->is_new ? MENewStream : MEUpdatedStream;
        source->streams[i]->is_new = FALSE;
        hr = IMFMediaEventQueue_QueueEventParamVar(source->event_queue, event_type, &GUID_NULL, S_OK, &var);
        ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

        event_type = source->state == SOURCE_RUNNING ? MEStreamSeeked : MEStreamStarted;
        hr = IMFMediaEventQueue_QueueEventParamVar(source->streams[i]->event_queue, event_type, &GUID_NULL,
                S_OK, NULL);
        ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    }

    source->state = SOURCE_RUNNING;

    LeaveCriticalSection(&source->cs);

    return S_OK;
}

static HRESULT WINAPI test_source_Stop(IMFMediaSource *iface)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI test_source_Pause(IMFMediaSource *iface)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI test_source_Shutdown(IMFMediaSource *iface)
{
    struct test_source *source = impl_from_IMFMediaSource(iface);
    HRESULT hr;

    hr = IMFMediaEventQueue_Shutdown(source->event_queue);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

    return S_OK;
}

static const IMFMediaSourceVtbl test_source_vtbl =
{
    test_source_QueryInterface,
    test_source_AddRef,
    test_source_Release,
    test_source_GetEvent,
    test_source_BeginGetEvent,
    test_source_EndGetEvent,
    test_source_QueueEvent,
    test_source_GetCharacteristics,
    test_source_CreatePresentationDescriptor,
    test_source_Start,
    test_source_Stop,
    test_source_Pause,
    test_source_Shutdown,
};

static struct test_media_stream *create_test_stream(DWORD stream_index, IMFMediaSource *source)
{
    struct test_media_stream *stream;
    IMFPresentationDescriptor *pd;
    BOOL selected;
    HRESULT hr;

    stream = calloc(1, sizeof(*stream));
    stream->IMFMediaStream_iface.lpVtbl = &test_media_stream_vtbl;
    stream->refcount = 1;
    hr = MFCreateEventQueue(&stream->event_queue);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    stream->source = source;
    IMFMediaSource_AddRef(stream->source);
    stream->is_new = TRUE;

    IMFMediaSource_CreatePresentationDescriptor(source, &pd);
    IMFPresentationDescriptor_GetStreamDescriptorByIndex(pd, stream_index, &selected, &stream->sd);
    IMFPresentationDescriptor_Release(pd);

    return stream;
}

static IMFMediaSource *create_test_source(int stream_count)
{
    struct test_source *source;
    int i;

    source = calloc(1, sizeof(*source));
    source->IMFMediaSource_iface.lpVtbl = &test_source_vtbl;
    source->refcount = 1;
    source->stream_count = stream_count;
    MFCreateEventQueue(&source->event_queue);
    InitializeCriticalSection(&source->cs);
    for (i = 0; i < source->stream_count; ++i)
        source->streams[i] = create_test_stream(i, &source->IMFMediaSource_iface);

    return &source->IMFMediaSource_iface;
}

static IMFByteStream *get_resource_stream(const char *name)
{
    IMFByteStream *bytestream;
    IStream *stream;
    DWORD written;
    HRESULT hr;
    HRSRC res;
    void *ptr;

    res = FindResourceA(NULL, name, (const char *)RT_RCDATA);
    ok(res != 0, "Resource %s wasn't found.\n", name);

    ptr = LockResource(LoadResource(GetModuleHandleA(NULL), res));

    hr = CreateStreamOnHGlobal(NULL, TRUE, &stream);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

    hr = IStream_Write(stream, ptr, SizeofResource(GetModuleHandleA(NULL), res), &written);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

    hr = pMFCreateMFByteStreamOnStream(stream, &bytestream);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    IStream_Release(stream);

    return bytestream;
}

struct async_callback
{
    IMFSourceReaderCallback IMFSourceReaderCallback_iface;
    LONG refcount;
    HANDLE event;
};

static struct async_callback *impl_from_IMFSourceReaderCallback(IMFSourceReaderCallback *iface)
{
    return CONTAINING_RECORD(iface, struct async_callback, IMFSourceReaderCallback_iface);
}

static HRESULT WINAPI async_callback_QueryInterface(IMFSourceReaderCallback *iface, REFIID riid, void **out)
{
    if (IsEqualIID(riid, &IID_IMFSourceReaderCallback) ||
            IsEqualIID(riid, &IID_IUnknown))
    {
        *out = iface;
        IMFSourceReaderCallback_AddRef(iface);
        return S_OK;
    }

    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI async_callback_AddRef(IMFSourceReaderCallback *iface)
{
    struct async_callback *callback = impl_from_IMFSourceReaderCallback(iface);
    return InterlockedIncrement(&callback->refcount);
}

static ULONG WINAPI async_callback_Release(IMFSourceReaderCallback *iface)
{
    struct async_callback *callback = impl_from_IMFSourceReaderCallback(iface);
    ULONG refcount = InterlockedDecrement(&callback->refcount);

    if (!refcount)
        free(callback);

    return refcount;
}

static HRESULT WINAPI async_callback_OnReadSample(IMFSourceReaderCallback *iface, HRESULT hr, DWORD stream_index,
        DWORD stream_flags, LONGLONG timestamp, IMFSample *sample)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI async_callback_OnFlush(IMFSourceReaderCallback *iface, DWORD stream_index)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI async_callback_OnEvent(IMFSourceReaderCallback *iface, DWORD stream_index, IMFMediaEvent *event)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static const IMFSourceReaderCallbackVtbl async_callback_vtbl =
{
    async_callback_QueryInterface,
    async_callback_AddRef,
    async_callback_Release,
    async_callback_OnReadSample,
    async_callback_OnFlush,
    async_callback_OnEvent,
};

static struct async_callback *create_async_callback(void)
{
    struct async_callback *callback;

    callback = calloc(1, sizeof(*callback));
    callback->IMFSourceReaderCallback_iface.lpVtbl = &async_callback_vtbl;
    callback->refcount = 1;

    return callback;
}

START_TEST(have_a_nice_death)
{
    IMFMediaType *native_type, *media_type;
    IMFDXGIDeviceManager *manager;
    IMFSourceResolver *resolver;
    IMFAttributes *attributes;
    IMFSourceReader *reader;
    IMFTransform *transform;
    IMFMediaSource *source;
    IMFSeekInfo *seek_info;
    UINT32 token, profile;
    MF_OBJECT_TYPE type;
    PROPVARIANT propvar;
    GUID major, subtype;
    HRESULT hr;

    hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

    init_functions();

hr = MFCreateAttributes(&attributes, 4);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFAttributes_SetUINT32(attributes, &MF_SOURCE_READER_DISABLE_DXVA, 0);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFAttributes_SetUINT32(attributes, &MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, 1);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFAttributes_SetUINT32(attributes, &MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, 1);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFAttributes_SetUINT32(attributes, &MF_SOURCE_READER_D3D11_BIND_FLAGS, 8);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFAttributes_SetUINT32(attributes, &MF_XVP_DISABLE_FRC, 1);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFAttributes_SetUINT32(attributes, &MF_SA_D3D11_SHARED_WITHOUT_MUTEX, 1);
ok(hr == S_OK, "got hr %#lx\n", hr);
/*
hr = MFCreateDXGIDeviceManager(&token, &manager);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFDXGIDeviceManager_ResetDevice(manager, 24D32168, 17B3E9C8, 18087);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFDXGIDeviceManager_QueryInterface(manager, 24D32168, &IID_IUnknown, 1477FCA0);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFAttributes_SetUnknown(attributes, &MF_SOURCE_READER_D3D_MANAGER, manager);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFDXGIDeviceManager_Release(manager, 24D32168, refcount 2);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFAttributes_SetUnknown(attributes, &MF_SOURCE_READER_ASYNC_CALLBACK, 068C8F90);
ok(hr == S_OK, "got hr %#lx\n", hr);
*/
hr = MFCreateSourceResolver(&resolver);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFSourceResolver_CreateObjectFromURL(resolver, L"file://Z:/home/rbernon/Code/debug/TEMP-GBP-Logo-Anim.mp4",
        0x1, 0, &type, (IUnknown **)&source);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = MFCreateSourceReaderFromMediaSource(source, attributes, &reader);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = MFGetService((IUnknown *)reader, &MF_SCRUBBING_SERVICE, &IID_IMFSeekInfo, (void **)&seek_info);
ok(hr == S_OK, "got hr %#lx\n", hr);
IMFMediaSource_Release(source);
IMFSourceResolver_Release(resolver);

hr = IMFSourceReader_GetPresentationAttribute(reader, MF_SOURCE_READER_MEDIASOURCE, &MF_PD_MIME_TYPE, &propvar);
ok(hr == S_OK, "got hr %#lx\n", hr);
PropVariantClear(&propvar);

hr = MFCreateMediaType(&media_type);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFMediaType_SetGUID(media_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFMediaType_SetGUID(media_type, &MF_MT_SUBTYPE, &MFVideoFormat_ARGB32);
ok(hr == S_OK, "got hr %#lx\n", hr);

hr = IMFSourceReader_GetNativeMediaType(reader, 0, 0, &native_type);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFMediaType_GetGUID(native_type, &MF_MT_MAJOR_TYPE, &major);
ok(hr == S_OK, "got hr %#lx\n", hr);
ok(IsEqualGUID(&major, &MFMediaType_Audio), "got major %s\n", debugstr_guid(&major));
IMFMediaType_Release(native_type);

hr = IMFSourceReader_GetNativeMediaType(reader, 0, 0x1, &native_type);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFSourceReader_GetNativeMediaType(reader, 0x1, 0, &native_type);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFMediaType_GetGUID(native_type, &MF_MT_MAJOR_TYPE, &major);
ok(hr == S_OK, "got hr %#lx\n", hr);
ok(IsEqualGUID(&major, &MFMediaType_Video), "got major %s\n", debugstr_guid(&major));
hr = IMFSourceReader_SetStreamSelection(reader, 0x1, 0);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFMediaType_GetUINT32(native_type, &MF_MT_VIDEO_PROFILE, &profile);
ok(hr == S_OK, "got hr %#lx\n", hr);
ok(profile == 0, "got profile %#x\n", profile);
IMFMediaType_Release(native_type);
hr = IMFSourceReader_GetNativeMediaType(reader, 0x1, 0x1, &native_type);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFSourceReader_GetNativeMediaType(reader, 0x2, 0, &native_type);
ok(hr == S_OK, "got hr %#lx\n", hr);

hr = IMFMediaType_SetUINT32(media_type, &MF_MT_VIDEO_ROTATION, 0);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFSourceReader_SetCurrentMediaType(reader, 0x1, 0, media_type);
ok(hr == S_OK, "got hr %#lx\n", hr);

hr = IMFSourceReader_GetServiceForStream(reader, 0x1, &GUID_NULL, &IID_IMFTransform, (void **)&transform);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFTransform_GetOutputStreamAttributes(transform, 0, &attributes);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFAttributes_SetUINT32(attributes, &MF_SA_D3D11_USAGE, 0);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFAttributes_SetUINT32(attributes, &MF_SA_D3D11_BINDFLAGS, 520);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFAttributes_SetUINT32(attributes, &MF_SA_D3D11_SHARED_WITHOUT_MUTEX, 1);
ok(hr == S_OK, "got hr %#lx\n", hr);
IMFAttributes_Release(attributes);

IMFTransform_Release(transform);

hr = IMFSourceReader_SetStreamSelection(reader, 0x1, 1);
ok(hr == S_OK, "got hr %#lx\n", hr);
IMFMediaType_Release(media_type);

hr = IMFSourceReader_GetNativeMediaType(reader, 0, 0, &native_type);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFMediaType_GetGUID(native_type, &MF_MT_MAJOR_TYPE, &major);
ok(hr == S_OK, "got hr %#lx\n", hr);
ok(IsEqualGUID(&major, &MFMediaType_Audio), "got major %s\n", debugstr_guid(&major));

hr = MFCreateMediaType(&media_type);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFMediaType_SetGUID(media_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFMediaType_SetGUID(media_type, &MF_MT_SUBTYPE, &MFAudioFormat_Float);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFSourceReader_SetCurrentMediaType(reader, 0, 0, media_type);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFSourceReader_SetStreamSelection(reader, 0, 1);
ok(hr == S_OK, "got hr %#lx\n", hr);
IMFMediaType_Release(media_type);
IMFMediaType_Release(native_type);

hr = IMFSourceReader_GetNativeMediaType(reader, 0, 0x1, &native_type);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFSourceReader_GetNativeMediaType(reader, 0x1, 0, &native_type);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFMediaType_GetGUID(native_type, &MF_MT_MAJOR_TYPE, &major);
ok(hr == S_OK, "got hr %#lx\n", hr);
ok(IsEqualGUID(&major, &MFMediaType_Video), "got major %s\n", debugstr_guid(&major));
IMFMediaType_Release(native_type);

hr = IMFSourceReader_GetNativeMediaType(reader, 0x1, 0x1, &native_type);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFSourceReader_GetNativeMediaType(reader, 0x2, 0, &native_type);
ok(hr == S_OK, "got hr %#lx\n", hr);

hr = IMFSourceReader_GetCurrentMediaType(reader, 0x1, &media_type);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFSourceReader_GetNativeMediaType(reader, 0x1, 0, &native_type);
ok(hr == S_OK, "got hr %#lx\n", hr);
IMFMediaType_GetGUID media_type, MF_MT_SUBTYPE, 1477FBF0.
IMFMediaType_GetUINT64 24D323C4, MF_MT_FRAME_RATE, 1477FC98.
hr = IMFSourceReader_GetPresentationAttribute(reader, MF_SOURCE_READER_MEDIASOURCE, &MF_PD_DURATION, 1477FB80);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFSourceReader_GetNativeMediaType(reader, 0, 0, 1477FCD0);
ok(hr == S_OK, "got hr %#lx\n", hr);
IMFMediaType_GetGUID 24D32504, MF_MT_SUBTYPE, 1477FC74.
IMFMediaType_Release 24D32504, refcount 0.
IMFMediaType_Release media_type, refcount 0.
IMFMediaType_Release 24D323C4, refcount 0.
hr = IMFSourceReader_GetPresentationAttribute(reader, MF_SOURCE_READER_MEDIASOURCE, &MF_SOURCE_READER_MEDIASOURCE_CHARACTERISTICS, 1477FBE0);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFSourceReader_GetNativeMediaType(reader, 0, 0, 1477F59C);
ok(hr == S_OK, "got hr %#lx\n", hr);
IMFMediaType_GetGUID 24D323C4, MF_MT_MAJOR_TYPE, 1477F564.
hr = IMFSourceReader_GetStreamSelection(reader, 0, 1477F5A8);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFSourceReader_ReadSample(reader, 0, 0, 00000000, 00000000, 00000000, 0000000);
ok(hr == S_OK, "got hr %#lx\n", hr);
:sample_AddRef 24D31774, refcount 3.
:sample_AddRef 24D31774, refcount 3.
:sample_Release 24D31774, refcount 2.
:sample_Release 24D31774, refcount 1.
IMFMediaType_Release 24D323C4, refcount 0.
hr = IMFSourceReader_GetNativeMediaType(reader, 0x1, 0, 1477F59C);
ok(hr == S_OK, "got hr %#lx\n", hr);
IMFMediaType_GetGUID 24D323C4, MF_MT_MAJOR_TYPE, 1477F564.
hr = IMFSourceReader_GetStreamSelection(reader, 0x1, 1477F5A8);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFSourceReader_ReadSample(reader, 0x1, 0, 00000000, 00000000, 00000000, 0000000);
ok(hr == S_OK, "got hr %#lx\n", hr);
:sample_AddRef 24D18E84, refcount 3.
:sample_AddRef 24D18E84, refcount 3.
:sample_tracked_Release 24D18E84, refcount 2.
:sample_GetSampleTime 24D18E84, 1477F58C.
:sample_GetUINT64 24D18E84, MFSampleExtension_DecodeTimestamp, 1477F584.
:sample_tracked_Release 24D18E84, refcount 1.
IMFMediaType_Release 24D323C4, refcount 0.
hr = IMFSourceReader_GetNativeMediaType(reader, 0x2, 0, 1477F59C);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFSourceReader_GetCurrentMediaType(reader, 0x1, 1477F588);
ok(hr == S_OK, "got hr %#lx\n", hr);
IMFMediaType_GetGUID 24D32324, MF_MT_SUBTYPE, 1477F4C4.
hr = IMFSourceReader_GetNativeMediaType(reader, 0x1, 0, 1477F564);
ok(hr == S_OK, "got hr %#lx\n", hr);
IMFMediaType_GetUINT32 24D32284, MF_MT_VIDEO_ROTATION, 1477F590.
IMFMediaType_AddRef 24D32324, refcount 2.
hr = IMFSourceReader_GetServiceForStream(reader, 0x1, {00000000-0000-0000-0000-000000000000}, {bf94c121-5b05-4e6f-8000-ba598961414d}, 1477F56);
ok(hr == S_OK, "got hr %#lx\n", hr);
IMFTransform_GetInputCurrentType(transform, id 0, type 1477F5A0 stub!
IMFMediaType_Release 24D32324, refcount 1.
IMFTransform_Release(transform decreasing refcount to 1.
IMFMediaType_GetUINT64 24D32374, MF_MT_FRAME_SIZE, 1477F52C.
IMFMediaType_GetUINT32 24D32374, MF_MT_PAN_SCAN_ENABLED, 1477F59C.
IMFMediaType_GetBlob 24D32374, MF_MT_MINIMUM_DISPLAY_APERTURE, 1477F570, 16, 00000000.
IMFMediaType_GetBlob 24D32374, MF_MT_GEOMETRIC_APERTURE, 1477F570, 16, 00000000.
IMFMediaType_GetUINT64 24D32374, MF_MT_FRAME_SIZE, 1477F5AC.
IMFMediaType_GetUINT64 24D32374, MF_MT_PIXEL_ASPECT_RATIO, 1477F450.
IMFMediaType_GetUINT32 24D32324, MF_MT_DEFAULT_STRIDE, 1477F450.
IMFMediaType_GetUINT32 24D32324, MF_MT_VIDEO_PRIMARIES, 1477F59C.
IMFMediaType_GetUINT32 24D32374, MF_MT_VIDEO_PRIMARIES, 1477F594.
IMFMediaType_SetUINT32 24D32324, MF_MT_VIDEO_PRIMARIES, 0.
IMFMediaType_GetUINT64 24D32324, MF_MT_PIXEL_ASPECT_RATIO, 1477F450.
hr = IMFSourceReader_QueryInterface(reader, {7b981cf0-560e-4116-9875-b099895f23d7}, 1477F448);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFSourceReader_QueryInterface(reader-560e-4116-9875-b099895f23d7}, 1477F448);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFSourceReader_GetCurrentMediaType(reader, 0, 1477F5AC);
ok(hr == S_OK, "got hr %#lx\n", hr);
IMFMediaType_GetUINT32 24D323C4, MF_MT_AUDIO_NUM_CHANNELS, 1477F594.
IMFMediaType_GetUINT32 24D323C4, MF_MT_AUDIO_SAMPLES_PER_SECOND, 1477F59C.
hr = IMFSourceReader_GetPresentationAttribute(reader, 0, {00af2180-bdc2-423c-abca-f503593bc121}, 1477F524);
ok(hr == S_OK, "got hr %#lx\n", hr);
IMFMediaType_Release 24D323C4, refcount 0.
IMFMediaType_Release 24D32374, refcount 0.
IMFMediaType_Release 24D32284, refcount 0.
IMFMediaType_Release 24D32324, refcount 0.
hr = IMFSourceReader_GetServiceForStream(reader, 0x1, {00000000-0000-0000-0000-000000000000}, {bf94c121-5b05-4e6f-8000-ba598961414d}, 1477F57);
ok(hr == S_OK, "got hr %#lx\n", hr);
IMFTransform_GetOutputCurrentType(transform, id 0, type 1477F56C stub!
IMFMediaType_GetUINT64 24D32284, MF_MT_FRAME_SIZE, 1477F538.
IMFMediaType_Release 24D32284, refcount 0.
IMFTransform_GetAttributes(transform, attributes 1477F574 semi-stub!
IMFAttributes_GetUINT32 24D31DD0, MF_SA_MINIMUM_OUTPUT_SAMPLE_COUNT, 1477F554.
hr = IMFSourceReader_GetNativeMediaType(reader, 0, 0, 1477F4E0);
ok(hr == S_OK, "got hr %#lx\n", hr);
IMFMediaType_GetGUID 24D32284, MF_MT_MAJOR_TYPE, 1477F4A0.
IMFMediaType_GetGUID 24D32284, MF_MT_SUBTYPE, 1477F430.
IMFMediaType_Release 24D32284, refcount 0.
hr = IMFSourceReader_GetNativeMediaType(reader, 0, 0x1, 1477F4E0);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFSourceReader_GetNativeMediaType(reader, 0x1, 0, 1477F4E0);
ok(hr == S_OK, "got hr %#lx\n", hr);
IMFMediaType_GetGUID 24D32284, MF_MT_MAJOR_TYPE, 1477F4A0.
IMFMediaType_GetGUID 24D32284, MF_MT_SUBTYPE, 1477F430.
IMFMediaType_GetUINT64 24D32284, MF_MT_FRAME_SIZE, 1477F440.
IMFMediaType_Release 24D32284, refcount 0.
hr = IMFSourceReader_GetNativeMediaType(reader, 0x1, 0x1, 1477F4E0);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFSourceReader_GetNativeMediaType(reader, 0x2, 0, 1477F4E0);
ok(hr == S_OK, "got hr %#lx\n", hr);
IMFAttributes_Release 24D31DD0, refcount 1.
MFCreateMediaType 1477F580.
MFCreateMediaType Created media type 24D32284.
IMFMediaType_SetGUID 24D32284, MF_MT_MAJOR_TYPE, MFMediaType_Video.
IMFMediaType_SetGUID 24D32284, MF_MT_SUBTYPE, MFVideoFormat_ARGB32.
IMFMediaType_SetUINT64 24D32284, MF_MT_FRAME_SIZE, f0000000870.
MFCreateVideoSampleAllocatorEx {545b3a48-3283-4f62-866f-a62d8f598f9f}, 1477F57C.
:sample_allocator_SetDirectXManager 24D2E1E8, 24D32168.
MFCreateAttributes 1477F570, 4
:sample_allocator_InitializeSampleAllocatorEx 24D2E1E8, 0, 6, 24D31F50, 24D32284.
IMFAttributes_Release 24D31F50, refcount 1.
IMFMediaType_Release 24D32284, refcount 1.
IMFTransform_Release(transform decreasing refcount to 1.
hr = IMFSourceReader_Flush(reader, 0xfffffffe);
ok(hr == S_OK, "got hr %#lx\n", hr);

    hr = MFShutdown();
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
}
