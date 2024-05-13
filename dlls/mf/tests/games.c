/*
 * Unit tests for mf.dll.
 *
 * Copyright 2017 Nikolay Sivov
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
#include <string.h>
#include <float.h>

#define COBJMACROS
#include "windef.h"
#include "winbase.h"

#include "d3d9.h"
#include "mfapi.h"
#include "mferror.h"
#include "mfidl.h"
#include "mmdeviceapi.h"
#include "uuids.h"
#include "wmcodecdsp.h"
#include "nserror.h"

#include "mf_test.h"

#include "wine/test.h"

#include "evr9.h"

struct test_grabber_callback
{
    IMFSampleGrabberSinkCallback IMFSampleGrabberSinkCallback_iface;
    LONG refcount;

    IMFCollection *samples;
    HANDLE ready_event;
    HANDLE done_event;
};

static struct test_grabber_callback *impl_from_IMFSampleGrabberSinkCallback(IMFSampleGrabberSinkCallback *iface)
{
    return CONTAINING_RECORD(iface, struct test_grabber_callback, IMFSampleGrabberSinkCallback_iface);
}

static HRESULT WINAPI test_grabber_callback_QueryInterface(IMFSampleGrabberSinkCallback *iface, REFIID riid, void **obj)
{
    if (IsEqualIID(riid, &IID_IMFSampleGrabberSinkCallback) ||
            IsEqualIID(riid, &IID_IMFClockStateSink) ||
            IsEqualIID(riid, &IID_IUnknown))
    {
        *obj = iface;
        IMFSampleGrabberSinkCallback_AddRef(iface);
        return S_OK;
    }

    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI test_grabber_callback_AddRef(IMFSampleGrabberSinkCallback *iface)
{
    struct test_grabber_callback *grabber = impl_from_IMFSampleGrabberSinkCallback(iface);
    return InterlockedIncrement(&grabber->refcount);
}

static ULONG WINAPI test_grabber_callback_Release(IMFSampleGrabberSinkCallback *iface)
{
    struct test_grabber_callback *grabber = impl_from_IMFSampleGrabberSinkCallback(iface);
    ULONG refcount = InterlockedDecrement(&grabber->refcount);

    if (!refcount)
    {
        IMFCollection_Release(grabber->samples);
        if (grabber->ready_event)
            CloseHandle(grabber->ready_event);
        if (grabber->done_event)
            CloseHandle(grabber->done_event);
        free(grabber);
    }

    return refcount;
}

static HRESULT WINAPI test_grabber_callback_OnClockStart(IMFSampleGrabberSinkCallback *iface, MFTIME time, LONGLONG offset)
{
    return S_OK;
}

static HRESULT WINAPI test_grabber_callback_OnClockStop(IMFSampleGrabberSinkCallback *iface, MFTIME time)
{
    return S_OK;
}

static HRESULT WINAPI test_grabber_callback_OnClockPause(IMFSampleGrabberSinkCallback *iface, MFTIME time)
{
    return S_OK;
}

static HRESULT WINAPI test_grabber_callback_OnClockRestart(IMFSampleGrabberSinkCallback *iface, MFTIME time)
{
    return S_OK;
}

static HRESULT WINAPI test_grabber_callback_OnClockSetRate(IMFSampleGrabberSinkCallback *iface, MFTIME time, float rate)
{
    return S_OK;
}

static HRESULT WINAPI test_grabber_callback_OnSetPresentationClock(IMFSampleGrabberSinkCallback *iface,
        IMFPresentationClock *clock)
{
    return S_OK;
}

static HRESULT WINAPI test_grabber_callback_OnProcessSample(IMFSampleGrabberSinkCallback *iface, REFGUID major_type,
        DWORD sample_flags, LONGLONG sample_time, LONGLONG sample_duration, const BYTE *buffer, DWORD sample_size)
{
    ok(0, "major %s flags %#lx time %I64d duration %I64d buffer %p size %lu\n", debugstr_guid(major_type),
            sample_flags, sample_time, sample_duration, buffer, sample_size);
    return S_OK;
}

static HRESULT WINAPI test_grabber_callback_OnShutdown(IMFSampleGrabberSinkCallback *iface)
{
    return S_OK;
}

static const IMFSampleGrabberSinkCallbackVtbl test_grabber_callback_vtbl =
{
    test_grabber_callback_QueryInterface,
    test_grabber_callback_AddRef,
    test_grabber_callback_Release,
    test_grabber_callback_OnClockStart,
    test_grabber_callback_OnClockStop,
    test_grabber_callback_OnClockPause,
    test_grabber_callback_OnClockRestart,
    test_grabber_callback_OnClockSetRate,
    test_grabber_callback_OnSetPresentationClock,
    test_grabber_callback_OnProcessSample,
    test_grabber_callback_OnShutdown,
};

static IMFSampleGrabberSinkCallback *create_test_grabber_callback(void)
{
    struct test_grabber_callback *grabber;
    HRESULT hr;

    if (!(grabber = calloc(1, sizeof(*grabber))))
        return NULL;

    grabber->IMFSampleGrabberSinkCallback_iface.lpVtbl = &test_grabber_callback_vtbl;
    grabber->refcount = 1;
    hr = MFCreateCollection(&grabber->samples);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

    return &grabber->IMFSampleGrabberSinkCallback_iface;
}

struct test_callback
{
    IMFAsyncCallback IMFAsyncCallback_iface;
    LONG refcount;

    HANDLE event;
    IMFMediaEvent *media_event;
    BOOL check_media_event;
};

static struct test_callback *impl_from_IMFAsyncCallback(IMFAsyncCallback *iface)
{
    return CONTAINING_RECORD(iface, struct test_callback, IMFAsyncCallback_iface);
}

static HRESULT WINAPI testcallback_QueryInterface(IMFAsyncCallback *iface, REFIID riid, void **obj)
{
    if (IsEqualIID(riid, &IID_IMFAsyncCallback) ||
            IsEqualIID(riid, &IID_IUnknown))
    {
        *obj = iface;
        IMFAsyncCallback_AddRef(iface);
        return S_OK;
    }

    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI testcallback_AddRef(IMFAsyncCallback *iface)
{
    struct test_callback *callback = impl_from_IMFAsyncCallback(iface);
    return InterlockedIncrement(&callback->refcount);
}

static ULONG WINAPI testcallback_Release(IMFAsyncCallback *iface)
{
    struct test_callback *callback = impl_from_IMFAsyncCallback(iface);
    ULONG refcount = InterlockedDecrement(&callback->refcount);

    if (!refcount)
    {
        if (callback->media_event)
            IMFMediaEvent_Release(callback->media_event);
        CloseHandle(callback->event);
        free(callback);
    }

    return refcount;
}

static HRESULT WINAPI testcallback_GetParameters(IMFAsyncCallback *iface, DWORD *flags, DWORD *queue)
{
    ok(flags != NULL && queue != NULL, "Unexpected arguments.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI testcallback_Invoke(IMFAsyncCallback *iface, IMFAsyncResult *result)
{
    struct test_callback *callback = CONTAINING_RECORD(iface, struct test_callback, IMFAsyncCallback_iface);
    IUnknown *object;
    HRESULT hr;

    ok(result != NULL, "Unexpected result object.\n");

    if (callback->media_event)
        IMFMediaEvent_Release(callback->media_event);

    if (callback->check_media_event)
    {
        hr = IMFAsyncResult_GetObject(result, &object);
        ok(hr == E_POINTER, "Unexpected hr %#lx.\n", hr);

        hr = IMFAsyncResult_GetState(result, &object);
        ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
        hr = IMFMediaEventGenerator_EndGetEvent((IMFMediaEventGenerator *)object,
                result, &callback->media_event);
        ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
        IUnknown_Release(object);
    }

    SetEvent(callback->event);

    return S_OK;
}

static const IMFAsyncCallbackVtbl testcallbackvtbl =
{
    testcallback_QueryInterface,
    testcallback_AddRef,
    testcallback_Release,
    testcallback_GetParameters,
    testcallback_Invoke,
};

static IMFAsyncCallback *create_test_callback(BOOL check_media_event)
{
    struct test_callback *callback;

    if (!(callback = calloc(1, sizeof(*callback))))
        return NULL;

    callback->refcount = 1;
    callback->check_media_event = check_media_event;
    callback->IMFAsyncCallback_iface.lpVtbl = &testcallbackvtbl;
    callback->event = CreateEventW(NULL, FALSE, FALSE, NULL);
    ok(!!callback->event, "CreateEventW failed, error %lu\n", GetLastError());

    return &callback->IMFAsyncCallback_iface;
}

#define wait_media_event(a, b, c, d, e) wait_media_event_(__LINE__, a, b, c, d, e)
static HRESULT wait_media_event_(int line, IMFMediaSession *session, IMFAsyncCallback *callback,
        MediaEventType expect_type, DWORD timeout, PROPVARIANT *value)
{
    struct test_callback *impl = impl_from_IMFAsyncCallback(callback);
    MediaEventType type;
    HRESULT hr, status;
    DWORD ret;
    GUID guid;

    do
    {
        hr = IMFMediaSession_BeginGetEvent(session, &impl->IMFAsyncCallback_iface, (IUnknown *)session);
        ok_(__FILE__, line)(hr == S_OK, "Unexpected hr %#lx.\n", hr);
        ret = WaitForSingleObject(impl->event, timeout);
        ok_(__FILE__, line)(ret == WAIT_OBJECT_0, "WaitForSingleObject returned %lu\n", ret);
        hr = IMFMediaEvent_GetType(impl->media_event, &type);
        ok_(__FILE__, line)(hr == S_OK, "Unexpected hr %#lx.\n", hr);
        if (type != expect_type) ok(0, "got %lu\n", type);
    } while (type != expect_type);

    ok_(__FILE__, line)(type == expect_type, "got type %lu\n", type);

    hr = IMFMediaEvent_GetExtendedType(impl->media_event, &guid);
    ok_(__FILE__, line)(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    ok_(__FILE__, line)(IsEqualGUID(&guid, &GUID_NULL), "got extended type %s\n", debugstr_guid(&guid));

    hr = IMFMediaEvent_GetValue(impl->media_event, value);
    ok_(__FILE__, line)(hr == S_OK, "Unexpected hr %#lx.\n", hr);

    hr = IMFMediaEvent_GetStatus(impl->media_event, &status);
    ok_(__FILE__, line)(hr == S_OK, "Unexpected hr %#lx.\n", hr);

    return status;
}

static void test_disintegration(void)
{
    const WCHAR *url = L"Z:/media/rbernon/LaCie/Steam/steamapps/common/Disintegration/Robogore/Content/Movies/Splash/GravIcon_1sec.mp4";
    IMFSampleGrabberSinkCallback *grabber;
    IMFTopologyNode *src_node, *sink_node;
    IMFPresentationDescriptor *pd;
    IMFMediaTypeHandler *handler;
    IMFSourceResolver *resolver;
    IMFActivate *sink_activate;
    IMFAsyncCallback *callback;
    IMFMediaType *output_type;
    IMFMediaSession *session;
    IMFMediaType *media_type;
    IMFStreamDescriptor *sd;
    MF_OBJECT_TYPE obj_type;
    IMFMediaSource *source;
    IMFTopology *topology;
    IMFByteStream *stream;
    PROPVARIANT propvar;
    UINT64 frame_size;
    UINT index = 0;
    BOOL selected;
    HRESULT hr;
    GUID major;

    if (!winetest_platform_is_wine) url = L"Y:/Steam/steamapps/common/Disintegration/Robogore/Content/Movies/Splash/GravIcon_1sec.mp4";

    callback = create_test_callback(TRUE);

    hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
    ok(hr == S_OK, "Startup failure, hr %#lx.\n", hr);

    hr = MFCreateFile(MF_ACCESSMODE_READ, MF_OPENMODE_FAIL_IF_NOT_EXIST, 0, url, &stream);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

    hr = MFCreateSourceResolver(&resolver);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    hr = IMFSourceResolver_CreateObjectFromByteStream(resolver, stream, NULL, MF_RESOLUTION_MEDIASOURCE, NULL,
            &obj_type, (IUnknown **)&source);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    IMFSourceResolver_Release(resolver);
    IMFByteStream_Release(stream);

    hr = IMFMediaSource_CreatePresentationDescriptor(source, &pd);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

    do
    {
        hr = IMFPresentationDescriptor_GetStreamDescriptorByIndex(pd, index++, &selected, &sd);
        ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
        hr = IMFStreamDescriptor_GetMediaTypeHandler(sd, &handler);
        ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
        hr = IMFMediaTypeHandler_GetMajorType(handler, &major);
        ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
        IMFMediaTypeHandler_Release(handler);
        IMFStreamDescriptor_Release(sd);
    } while (!IsEqualGUID(&major, &MFMediaType_Video));

    hr = IMFPresentationDescriptor_SelectStream(pd, index - 1);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

    hr = IMFPresentationDescriptor_GetStreamDescriptorByIndex(pd, index - 1, &selected, &sd);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    hr = IMFStreamDescriptor_GetMediaTypeHandler(sd, &handler);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    hr = IMFMediaTypeHandler_GetCurrentMediaType(handler, &media_type);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    hr = IMFMediaType_GetUINT64(media_type, &MF_MT_FRAME_SIZE, &frame_size);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    ok(frame_size == 0, "got %#I64x\n", frame_size);
    hr = IMFMediaType_GetItem(media_type, &MF_MT_DEFAULT_STRIDE, NULL);
    ok(hr == MF_E_ATTRIBUTENOTFOUND, "Unexpected hr %#lx.\n", hr);
    hr = IMFMediaType_GetItem(media_type, &MF_MT_GEOMETRIC_APERTURE, NULL);
    ok(hr == MF_E_ATTRIBUTENOTFOUND, "Unexpected hr %#lx.\n", hr);
    IMFMediaTypeHandler_Release(handler);


    hr = MFCreateTopology(&topology);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

    hr = MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &src_node);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    hr = IMFTopologyNode_SetUnknown(src_node, &MF_TOPONODE_PRESENTATION_DESCRIPTOR, (IUnknown *)pd);
    ok(hr == S_OK, "Failed to set node pd, hr %#lx.\n", hr);
    hr = IMFTopologyNode_SetUnknown(src_node, &MF_TOPONODE_STREAM_DESCRIPTOR, (IUnknown *)sd);
    ok(hr == S_OK, "Failed to set node sd, hr %#lx.\n", hr);
    hr = IMFTopologyNode_SetUnknown(src_node, &MF_TOPONODE_SOURCE, (IUnknown *)source);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    hr = IMFTopology_AddNode(topology, src_node);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);


    hr = MFCreateMediaType(&output_type);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    hr = IMFMediaType_SetGUID(output_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    hr = IMFMediaType_SetGUID(output_type, &MF_MT_SUBTYPE, &MFVideoFormat_YUY2);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

    grabber = create_test_grabber_callback();
    hr = MFCreateSampleGrabberSinkActivate(output_type, grabber, &sink_activate);
    ok(hr == S_OK, "Failed to create grabber sink, hr %#lx.\n", hr);
    IMFMediaType_Release(output_type);
    hr = MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &sink_node);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    hr = IMFTopologyNode_SetObject(sink_node, (IUnknown *)sink_activate);
    ok(hr == S_OK, "Failed to set object, hr %#lx.\n", hr);

    hr = IMFTopology_AddNode(topology, sink_node);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    hr = IMFTopologyNode_ConnectOutput(src_node, 0, sink_node, 0);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);


    hr = MFCreateMediaSession(NULL, &session);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    hr = IMFMediaSession_SetTopology(session, 0, topology);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

    propvar.vt = VT_EMPTY;
    hr = wait_media_event(session, callback, MESessionTopologySet, 1000, &propvar);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    ok(propvar.vt == VT_UNKNOWN, "got vt %u\n", propvar.vt);
    ok(propvar.punkVal != (IUnknown *)topology, "got punkVal %p\n", propvar.punkVal);
    PropVariantClear(&propvar);

    propvar.vt = VT_EMPTY;
    hr = IMFMediaSession_Start(session, &GUID_NULL, &propvar);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

    propvar.vt = VT_EMPTY;
    hr = wait_media_event(session, callback, MESessionNotifyPresentationTime, 1000, &propvar);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    ok(propvar.vt == VT_EMPTY, "got vt %u\n", propvar.vt);
    hr = wait_media_event(session, callback, MESessionCapabilitiesChanged, 1000, &propvar);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    ok(propvar.vt == VT_EMPTY, "got vt %u\n", propvar.vt);
    hr = wait_media_event(session, callback, MESessionTopologyStatus, 1000, &propvar);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    ok(propvar.vt == VT_UNKNOWN, "got vt %u\n", propvar.vt);
    ok(propvar.punkVal != (IUnknown *)topology, "got punkVal %p\n", propvar.punkVal);
    PropVariantClear(&propvar);
    hr = wait_media_event(session, callback, MESessionTopologyStatus, 1000, &propvar);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    ok(propvar.vt == VT_UNKNOWN, "got vt %u\n", propvar.vt);
    ok(propvar.punkVal != (IUnknown *)topology, "got punkVal %p\n", propvar.punkVal);
    PropVariantClear(&propvar);
    hr = wait_media_event(session, callback, MESessionCapabilitiesChanged, 1000, &propvar);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    ok(propvar.vt == VT_EMPTY, "got vt %u\n", propvar.vt);
    hr = wait_media_event(session, callback, MESessionStarted, 1000, &propvar);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    ok(propvar.vt == VT_EMPTY, "got vt %u\n", propvar.vt);


Sleep(10000);

    IMFMediaSource_Release(source);
}

START_TEST(games)
{
    init_functions();

    test_disintegration();
}
