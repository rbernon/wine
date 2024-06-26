/*
 * Copyright 2019 Jactry Zeng for CodeWeavers
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
#include <math.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"

#include "mfapi.h"
#include "mfidl.h"
#include "mfmediaengine.h"
#include "mferror.h"
#include "dxgi.h"
#include "mmdeviceapi.h"
#include "audiosessiontypes.h"

#include "initguid.h"
#include "d3d11_4.h"

#include "wine/test.h"

static const char *debugstr_event( UINT event )
{
    switch (event)
    {
    case MF_MEDIA_ENGINE_EVENT_LOADSTART: return "MF_MEDIA_ENGINE_EVENT_LOADSTART";
    case MF_MEDIA_ENGINE_EVENT_PROGRESS: return "MF_MEDIA_ENGINE_EVENT_PROGRESS";
    case MF_MEDIA_ENGINE_EVENT_SUSPEND: return "MF_MEDIA_ENGINE_EVENT_SUSPEND";
    case MF_MEDIA_ENGINE_EVENT_ABORT: return "MF_MEDIA_ENGINE_EVENT_ABORT";
    case MF_MEDIA_ENGINE_EVENT_ERROR: return "MF_MEDIA_ENGINE_EVENT_ERROR";
    case MF_MEDIA_ENGINE_EVENT_EMPTIED: return "MF_MEDIA_ENGINE_EVENT_EMPTIED";
    case MF_MEDIA_ENGINE_EVENT_STALLED: return "MF_MEDIA_ENGINE_EVENT_STALLED";
    case MF_MEDIA_ENGINE_EVENT_PLAY: return "MF_MEDIA_ENGINE_EVENT_PLAY";
    case MF_MEDIA_ENGINE_EVENT_PAUSE: return "MF_MEDIA_ENGINE_EVENT_PAUSE";
    case MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA: return "MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA";
    case MF_MEDIA_ENGINE_EVENT_LOADEDDATA: return "MF_MEDIA_ENGINE_EVENT_LOADEDDATA";
    case MF_MEDIA_ENGINE_EVENT_WAITING: return "MF_MEDIA_ENGINE_EVENT_WAITING";
    case MF_MEDIA_ENGINE_EVENT_PLAYING: return "MF_MEDIA_ENGINE_EVENT_PLAYING";
    case MF_MEDIA_ENGINE_EVENT_CANPLAY: return "MF_MEDIA_ENGINE_EVENT_CANPLAY";
    case MF_MEDIA_ENGINE_EVENT_CANPLAYTHROUGH: return "MF_MEDIA_ENGINE_EVENT_CANPLAYTHROUGH";
    case MF_MEDIA_ENGINE_EVENT_SEEKING: return "MF_MEDIA_ENGINE_EVENT_SEEKING";
    case MF_MEDIA_ENGINE_EVENT_SEEKED: return "MF_MEDIA_ENGINE_EVENT_SEEKED";
    case MF_MEDIA_ENGINE_EVENT_TIMEUPDATE: return "MF_MEDIA_ENGINE_EVENT_TIMEUPDATE";
    case MF_MEDIA_ENGINE_EVENT_ENDED: return "MF_MEDIA_ENGINE_EVENT_ENDED";
    case MF_MEDIA_ENGINE_EVENT_RATECHANGE: return "MF_MEDIA_ENGINE_EVENT_RATECHANGE";
    case MF_MEDIA_ENGINE_EVENT_DURATIONCHANGE: return "MF_MEDIA_ENGINE_EVENT_DURATIONCHANGE";
    case MF_MEDIA_ENGINE_EVENT_VOLUMECHANGE: return "MF_MEDIA_ENGINE_EVENT_VOLUMECHANGE";
    case MF_MEDIA_ENGINE_EVENT_FORMATCHANGE: return "MF_MEDIA_ENGINE_EVENT_FORMATCHANGE";
    case MF_MEDIA_ENGINE_EVENT_PURGEQUEUEDEVENTS: return "MF_MEDIA_ENGINE_EVENT_PURGEQUEUEDEVENTS";
    case MF_MEDIA_ENGINE_EVENT_TIMELINE_MARKER: return "MF_MEDIA_ENGINE_EVENT_TIMELINE_MARKER";
    case MF_MEDIA_ENGINE_EVENT_BALANCECHANGE: return "MF_MEDIA_ENGINE_EVENT_BALANCECHANGE";
    case MF_MEDIA_ENGINE_EVENT_DOWNLOADCOMPLETE: return "MF_MEDIA_ENGINE_EVENT_DOWNLOADCOMPLETE";
    case MF_MEDIA_ENGINE_EVENT_BUFFERINGSTARTED: return "MF_MEDIA_ENGINE_EVENT_BUFFERINGSTARTED";
    case MF_MEDIA_ENGINE_EVENT_BUFFERINGENDED: return "MF_MEDIA_ENGINE_EVENT_BUFFERINGENDED";
    case MF_MEDIA_ENGINE_EVENT_FRAMESTEPCOMPLETED: return "MF_MEDIA_ENGINE_EVENT_FRAMESTEPCOMPLETED";
    case MF_MEDIA_ENGINE_EVENT_NOTIFYSTABLESTATE: return "MF_MEDIA_ENGINE_EVENT_NOTIFYSTABLESTATE";
    case MF_MEDIA_ENGINE_EVENT_FIRSTFRAMEREADY: return "MF_MEDIA_ENGINE_EVENT_FIRSTFRAMEREADY";
    case MF_MEDIA_ENGINE_EVENT_TRACKSCHANGE: return "MF_MEDIA_ENGINE_EVENT_TRACKSCHANGE";
    case MF_MEDIA_ENGINE_EVENT_OPMINFO: return "MF_MEDIA_ENGINE_EVENT_OPMINFO";
    case MF_MEDIA_ENGINE_EVENT_RESOURCELOST: return "MF_MEDIA_ENGINE_EVENT_RESOURCELOST";
    case MF_MEDIA_ENGINE_EVENT_DELAYLOADEVENT_CHANGED: return "MF_MEDIA_ENGINE_EVENT_DELAYLOADEVENT_CHANGED";
    case MF_MEDIA_ENGINE_EVENT_STREAMRENDERINGERROR: return "MF_MEDIA_ENGINE_EVENT_STREAMRENDERINGERROR";
    case MF_MEDIA_ENGINE_EVENT_SUPPORTEDRATES_CHANGED: return "MF_MEDIA_ENGINE_EVENT_SUPPORTEDRATES_CHANGED";
    case MF_MEDIA_ENGINE_EVENT_AUDIOENDPOINTCHANGE: return "MF_MEDIA_ENGINE_EVENT_AUDIOENDPOINTCHANGE";
    }

    return wine_dbg_sprintf("%#x", event);
}

static const char *debugstr_ok( const char *cond )
{
    int c, n = 0;
    /* skip possible casts */
    while ((c = *cond++))
    {
        if (c == '(') n++;
        if (!n) break;
        if (c == ')') n--;
    }
    if (!strchr( cond - 1, '(' )) return wine_dbg_sprintf( "got %s", cond - 1 );
    return wine_dbg_sprintf( "%.*s returned", (int)strcspn( cond - 1, "( " ), cond - 1 );
}

#define ok_eq( e, r, t, f, ... )                                                                   \
    do                                                                                             \
    {                                                                                              \
        t v = (r);                                                                                 \
        ok( v == (e), "%s " f "\n", debugstr_ok( #r ), v, ##__VA_ARGS__ );                         \
    } while (0)
#define ok_ne( e, r, t, f, ... )                                                                   \
    do                                                                                             \
    {                                                                                              \
        t v = (r);                                                                                 \
        ok( v != (e), "%s " f "\n", debugstr_ok( #r ), v, ##__VA_ARGS__ );                         \
    } while (0)
#define ok_wcs( e, r )                                                                             \
    do                                                                                             \
    {                                                                                              \
        const WCHAR *v = (r);                                                                      \
        ok( !wcscmp( v, (e) ), "%s %s\n", debugstr_ok(#r), debugstr_w(v) );                        \
    } while (0)
#define ok_str( e, r )                                                                             \
    do                                                                                             \
    {                                                                                              \
        const char *v = (r);                                                                       \
        ok( !strcmp( v, (e) ), "%s %s\n", debugstr_ok(#r), debugstr_a(v) );                        \
    } while (0)
#define ok_hr( e, r ) ok_eq( e, r, HRESULT, "%#lx" )

struct media_engine_notify
{
    IMFMediaEngineNotify IMFMediaEngineNotify_iface;
    LONG refcount;

    IMFMediaEngine *engine;
    ID3D11Device *device;
    ID3D11Texture2D *texture;
    RECT dst_rect;

    CONDITION_VARIABLE cond;
    CRITICAL_SECTION cs;
    BOOL ended;
};

static inline struct media_engine_notify *impl_from_IMFMediaEngineNotify(IMFMediaEngineNotify *iface)
{
    return CONTAINING_RECORD(iface, struct media_engine_notify, IMFMediaEngineNotify_iface);
}

static HRESULT WINAPI media_engine_notify_QueryInterface(IMFMediaEngineNotify *iface, REFIID riid, void **obj)
{
    struct media_engine_notify *notify = impl_from_IMFMediaEngineNotify(iface);

    if (IsEqualIID(riid, &IID_IMFMediaEngineNotify)
            || IsEqualIID(riid, &IID_IUnknown))
    {
        IMFMediaEngineNotify_AddRef(&notify->IMFMediaEngineNotify_iface);
        *obj = &notify->IMFMediaEngineNotify_iface;
        return S_OK;
    }

    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI media_engine_notify_AddRef(IMFMediaEngineNotify *iface)
{
    struct media_engine_notify *notify = impl_from_IMFMediaEngineNotify(iface);
    return InterlockedIncrement(&notify->refcount);
}

static ULONG WINAPI media_engine_notify_Release(IMFMediaEngineNotify *iface)
{
    struct media_engine_notify *notify = impl_from_IMFMediaEngineNotify(iface);
    ULONG refcount = InterlockedDecrement(&notify->refcount);

    if (!refcount)
    {
        if (notify->engine) IMFMediaEngine_Release(notify->engine);
        if (notify->texture) ID3D11Texture2D_Release(notify->texture);
        ID3D11Device_Release(notify->device);
        DeleteCriticalSection(&notify->cs);
        free(notify);
    }

    return refcount;
}

static DWORD CALLBACK notify_thread(void *arg)
{
    struct media_engine_notify *notify = arg;
    LONGLONG timestamp;
    double volume;
    HRESULT hr;

    EnterCriticalSection(&notify->cs);

    for (;;)
    {
        MFVideoNormalizedRect src_rect = {0, 0, 1.0, 1.0};
        MFARGB color = {0};

        SleepConditionVariableCS(&notify->cond, &notify->cs, 100);
        if (notify->ended)
            break;

        hr = IMFMediaEngine_OnVideoStreamTick(notify->engine, &timestamp);
        ok(hr == S_FALSE, "iface %p hr %#lx timestamp %I64d\n", &notify->IMFMediaEngineNotify_iface, hr, timestamp);
        volume = IMFMediaEngine_GetVolume(notify->engine);
        ok(volume == 1.0, "volume %f\n", volume);

        if (hr == S_OK)
        {
            ok_hr(S_OK, IMFMediaEngine_TransferVideoFrame(notify->engine, (IUnknown *)notify->texture,
                    &src_rect, &notify->dst_rect, &color));
        }
    }

    LeaveCriticalSection(&notify->cs);

    IMFMediaEngineNotify_Release(&notify->IMFMediaEngineNotify_iface);
    return 0;
}

static HRESULT WINAPI media_engine_notify_EventNotify(IMFMediaEngineNotify *iface, DWORD event, DWORD_PTR param1, DWORD param2)
{
    struct media_engine_notify *notify = impl_from_IMFMediaEngineNotify(iface);

    ok(0, "%ld: iface %p, event %s, param1 %#Ix, param2 %#lx\n", GetTickCount(), iface, debugstr_event(event), param1, param2);

    if (event == MF_MEDIA_ENGINE_EVENT_CANPLAY)
    {
        ok_hr(S_OK, IMFMediaEngine_Play(notify->engine));

        IMFMediaEngineNotify_AddRef(&notify->IMFMediaEngineNotify_iface);
        CloseHandle(CreateThread(NULL, 0, notify_thread, notify, 0, NULL));
    }

    if (event == MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA)
    {
        D3D11_TEXTURE2D_DESC desc = {0};

        ok_hr(S_OK, IMFMediaEngine_GetNativeVideoSize(notify->engine, (DWORD *)&desc.Width, (DWORD *)&desc.Height));
        ok(0, "got %dx%d\n", desc.Width, desc.Height);

        notify->dst_rect.right = desc.Width;
        notify->dst_rect.bottom = desc.Height;

        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        desc.SampleDesc.Count = 1;
        ok_hr(S_OK, ID3D11Device_CreateTexture2D(notify->device, &desc, NULL, &notify->texture));
    }

    if (event == MF_MEDIA_ENGINE_EVENT_ENDED)
    {
        EnterCriticalSection(&notify->cs);
        notify->ended = TRUE;
        LeaveCriticalSection(&notify->cs);
        WakeAllConditionVariable(&notify->cond);
    }

    return S_OK;
}

static void media_engine_notify_set_engine(IMFMediaEngineNotify *iface, IMFMediaEngine *engine)
{
    struct media_engine_notify *notify = impl_from_IMFMediaEngineNotify(iface);
    if (notify->engine) IMFMediaEngine_Release(notify->engine);
    if ((notify->engine = engine)) IMFMediaEngine_AddRef(notify->engine);
}

static void media_engine_notify_wait_ended(IMFMediaEngineNotify *iface)
{
    struct media_engine_notify *notify = impl_from_IMFMediaEngineNotify(iface);

    EnterCriticalSection(&notify->cs);
    while (!notify->ended)
        SleepConditionVariableCS(&notify->cond, &notify->cs, 100);
    LeaveCriticalSection(&notify->cs);
}

static IMFMediaEngineNotifyVtbl media_engine_notify_vtbl =
{
    media_engine_notify_QueryInterface,
    media_engine_notify_AddRef,
    media_engine_notify_Release,
    media_engine_notify_EventNotify,
};

static IMFMediaEngineNotify *create_callback(ID3D11Device *device)
{
    struct media_engine_notify *object;

    if (!(object = calloc(1, sizeof(*object)))) return NULL;

    object->IMFMediaEngineNotify_iface.lpVtbl = &media_engine_notify_vtbl;
    object->refcount = 1;

    object->device = device;
    ID3D11Device_AddRef(object->device);
    InitializeCriticalSection(&object->cs);
    InitializeConditionVariable(&object->cond);

    return &object->IMFMediaEngineNotify_iface;
}

START_TEST(games)
{
    IMFDXGIDeviceManager *device_manager;
    IMFMediaEngineClassFactory *factory;
    IMFMediaEngineNotify *callback_back, *callback_front;
    ID3D11Multithread *multithread;
    IMFAttributes *attributes;
    IMFMediaEngine *engine;
    ID3D11Device *d3d11;
    UINT32 token;
    HRESULT hr;

    WCHAR *wine_urls[] =
    {
        (WCHAR *)L"Z:\\media\\rbernon\\LaCie\\STEAM\\STEAMAPPS\\COMMON\\MICROSOFTFLIGHTSIMULATOR\\Packages\\fs-base-videos\\Videos\\PressStartVideo-4K.mp4",
        (WCHAR *)L"Z:\\media\\rbernon\\LaCie\\STEAM\\STEAMAPPS\\COMMON\\MICROSOFTFLIGHTSIMULATOR\\Packages\\fs-base-videos\\Videos\\Logo_Mgs.mp4",
        (WCHAR *)L"Z:\\media\\rbernon\\LaCie\\STEAM\\STEAMAPPS\\COMMON\\MICROSOFTFLIGHTSIMULATOR\\Packages\\fs-base-videos\\Videos\\Logo_Asobo.mp4",
    };
    WCHAR *windows_urls[] =
    {
        (WCHAR *)L"Y:\\STEAM\\STEAMAPPS\\COMMON\\MICROSOFTFLIGHTSIMULATOR\\Packages\\fs-base-videos\\Videos\\PressStartVideo-4K.mp4",
        (WCHAR *)L"Y:\\STEAM\\STEAMAPPS\\COMMON\\MICROSOFTFLIGHTSIMULATOR\\Packages\\fs-base-videos\\Videos\\Logo_Mgs.mp4",
        (WCHAR *)L"Y:\\STEAM\\STEAMAPPS\\COMMON\\MICROSOFTFLIGHTSIMULATOR\\Packages\\fs-base-videos\\Videos\\Logo_Asobo.mp4",
    };
    WCHAR **urls = winetest_platform_is_wine ? wine_urls : windows_urls;


    CoInitialize(NULL);
    hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
    ok(hr == S_OK, "MFStartup failed: %#lx.\n", hr);

    ok_hr(S_OK, D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_VIDEO_SUPPORT, NULL, 0,
            D3D11_SDK_VERSION, &d3d11, NULL, NULL));

    ok_hr(S_OK, ID3D11Device_QueryInterface(d3d11, &IID_ID3D11Multithread, (void **)&multithread));
    ok_hr(S_OK, ID3D11Multithread_SetMultithreadProtected(multithread, TRUE));
    ID3D11Multithread_Release(multithread);

    ok_hr(S_OK, CoCreateInstance(&CLSID_MFMediaEngineClassFactory, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMFMediaEngineClassFactory, (void **)&factory));

    ok_hr(S_OK, MFCreateDXGIDeviceManager(&token, &device_manager));
    ok_hr(S_OK, IMFDXGIDeviceManager_ResetDevice(device_manager, (IUnknown *)d3d11, token));
    ok_hr(S_OK, MFCreateAttributes(&attributes, 1));
    ok_hr(S_OK, IMFAttributes_SetUnknown(attributes, &MF_MEDIA_ENGINE_DXGI_MANAGER, (IUnknown *)device_manager));
    IMFDXGIDeviceManager_Release(device_manager);
    callback_back = create_callback(d3d11);
    ok_hr(S_OK, IMFAttributes_SetUnknown(attributes, &MF_MEDIA_ENGINE_CALLBACK, (IUnknown *)callback_back));
    ok_hr(S_OK, IMFAttributes_SetUINT32(attributes, &MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT, DXGI_FORMAT_B8G8R8A8_UNORM));

    ok_hr(S_OK, IMFMediaEngineClassFactory_CreateInstance(factory, 0, attributes, &engine));
    IMFAttributes_Release(attributes);
    media_engine_notify_set_engine(callback_back, engine);

    ok(0, "ticks %ld\n", GetTickCount());
    ok_hr(S_OK, IMFMediaEngine_SetSource(engine, urls[0]));
    IMFMediaEngine_Release(engine);

    ok_hr(S_OK, MFCreateDXGIDeviceManager(&token, &device_manager));
    ok_hr(S_OK, IMFDXGIDeviceManager_ResetDevice(device_manager, (IUnknown *)d3d11, token));
    ok_hr(S_OK, MFCreateAttributes(&attributes, 1));
    ok_hr(S_OK, IMFAttributes_SetUnknown(attributes, &MF_MEDIA_ENGINE_DXGI_MANAGER, (IUnknown *)device_manager));
    IMFDXGIDeviceManager_Release(device_manager);
    callback_front = create_callback(d3d11);
    ok_hr(S_OK, IMFAttributes_SetUnknown(attributes, &MF_MEDIA_ENGINE_CALLBACK, (IUnknown *)callback_front));
    ok_hr(S_OK, IMFAttributes_SetUINT32(attributes, &MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT, DXGI_FORMAT_B8G8R8A8_UNORM));

    ok_hr(S_OK, IMFMediaEngineClassFactory_CreateInstance(factory, 0, attributes, &engine));
    IMFAttributes_Release(attributes);
    media_engine_notify_set_engine(callback_front, engine);

    ok(0, "ticks %ld\n", GetTickCount());
    ok_hr(S_OK, IMFMediaEngine_SetSource(engine, urls[1]));
    IMFMediaEngine_Release(engine);

    media_engine_notify_wait_ended(callback_front);
    IMFMediaEngineNotify_Release(callback_front);

    ok_hr(S_OK, MFCreateDXGIDeviceManager(&token, &device_manager));
    ok_hr(S_OK, IMFDXGIDeviceManager_ResetDevice(device_manager, (IUnknown *)d3d11, token));
    ok_hr(S_OK, MFCreateAttributes(&attributes, 1));
    ok_hr(S_OK, IMFAttributes_SetUnknown(attributes, &MF_MEDIA_ENGINE_DXGI_MANAGER, (IUnknown *)device_manager));
    IMFDXGIDeviceManager_Release(device_manager);
    callback_front = create_callback(d3d11);
    ok_hr(S_OK, IMFAttributes_SetUnknown(attributes, &MF_MEDIA_ENGINE_CALLBACK, (IUnknown *)callback_front));
    ok_hr(S_OK, IMFAttributes_SetUINT32(attributes, &MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT, DXGI_FORMAT_B8G8R8A8_UNORM));

    ok_hr(S_OK, IMFMediaEngineClassFactory_CreateInstance(factory, 0, attributes, &engine));
    IMFAttributes_Release(attributes);
    media_engine_notify_set_engine(callback_front, engine);

    ok(0, "ticks %ld\n", GetTickCount());
    ok_hr(S_OK, IMFMediaEngine_SetSource(engine, urls[2]));
    IMFMediaEngine_Release(engine);

    media_engine_notify_wait_ended(callback_front);
    IMFMediaEngineNotify_Release(callback_front);

    media_engine_notify_wait_ended(callback_back);
    IMFMediaEngineNotify_Release(callback_back);

    IMFMediaEngineClassFactory_Release(factory);
    ID3D11Device_Release(d3d11);

    MFShutdown();
    CoUninitialize();
}
