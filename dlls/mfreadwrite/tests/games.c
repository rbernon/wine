/*
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
#include <stddef.h>

#define COBJMACROS
#include "windef.h"
#include "winbase.h"

#include "mfapi.h"
#include "mfidl.h"
#include "mfobjects.h"
#include "mfreadwrite.h"
#include "d3d11_4.h"

#include "wine/test.h"

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
#define ok_ret( e, r ) ok_eq( e, r, UINT_PTR, "%Iu, error %ld", GetLastError() )
#define ok_hr( e, r )  ok_eq( e, r, HRESULT, "hr %#lx" )

#define check_interface(a, b, c) check_interface_(__LINE__, a, b, c)
static void check_interface_(unsigned int line, void *iface_ptr, REFIID iid, BOOL supported)
{
    IUnknown *iface = iface_ptr;
    HRESULT hr, expected_hr;
    IUnknown *unk;

    expected_hr = supported ? S_OK : E_NOINTERFACE;

    hr = IUnknown_QueryInterface(iface, iid, (void **)&unk);
    ok_(__FILE__, line)(hr == expected_hr, "Got hr %#lx, expected %#lx.\n", hr, expected_hr);
    if (SUCCEEDED(hr))
        IUnknown_Release(unk);
}

struct async_callback
{
    IMFSourceReaderCallback IMFSourceReaderCallback_iface;
    LONG ref;

    IMFSourceReader *reader;
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
    return InterlockedIncrement(&callback->ref);
}

static ULONG WINAPI async_callback_Release(IMFSourceReaderCallback *iface)
{
    struct async_callback *callback = impl_from_IMFSourceReaderCallback(iface);
    ULONG ref = InterlockedDecrement(&callback->ref);
    if (!ref) free(callback);
    return ref;
}

static HRESULT WINAPI async_callback_OnReadSample(IMFSourceReaderCallback *iface, HRESULT hr, DWORD stream_index,
        DWORD stream_flags, LONGLONG timestamp, IMFSample *sample)
{
    struct async_callback *callback = impl_from_IMFSourceReaderCallback(iface);
    IMFMediaBuffer *buffer;
    DWORD total = 0;

    if (sample)
    {
        ok_hr(S_OK, IMFSample_GetTotalLength(sample, &total));
        check_interface(sample, &IID_IMFSample, TRUE);
    }

    ok(0, "hr %#lx stream_index %lu steam_flags %lu timestamp %I64d sample %p total %lu.\n", hr, stream_index, stream_flags, timestamp, sample, total);
    if (!sample) return S_OK;

    ok_hr(S_OK, IMFSample_GetBufferByIndex(sample, 0, &buffer));
    if (stream_index == 0)
    {
        check_interface(sample, &IID_IMFTrackedSample, FALSE);
        check_interface(buffer, &IID_IMF2DBuffer, FALSE);
        check_interface(buffer, &IID_IMF2DBuffer2, FALSE);
        check_interface(buffer, &IID_IMFDXGIBuffer, FALSE);
    }
    else
    {
        check_interface(sample, &IID_IMFTrackedSample, TRUE);
        check_interface(buffer, &IID_IMF2DBuffer, TRUE);
        check_interface(buffer, &IID_IMF2DBuffer2, TRUE);
        check_interface(buffer, &IID_IMFDXGIBuffer, TRUE);
    }
    IMFMediaBuffer_Release(buffer);

    IMFSourceReader_ReadSample(callback->reader, stream_index, 0, NULL, NULL, NULL, NULL);
    return S_OK;
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

static HRESULT async_callback_create(IMFSourceReaderCallback **out)
{
    struct async_callback *impl;

    *out = NULL;
    if (!(impl = calloc(1, sizeof(*impl)))) return E_OUTOFMEMORY;
    impl->IMFSourceReaderCallback_iface.lpVtbl = &async_callback_vtbl;
    impl->ref = 1;

    *out = &impl->IMFSourceReaderCallback_iface;
    return S_OK;
}

static void have_a_nice_night(void)
{
    /* const WCHAR *path = L"file:///media/rbernon/LaCie/Games/Have A Nice Death/" L"Assets/Video/Mds Logo Vfx.mp4"; */
    /* const WCHAR *path = L"file://Z:/home/rbernon/Code/debug/TEMP-GBP-Logo-Anim.mp4"; */
    const WCHAR *path = L"file://Z:/home/rbernon/Code/build-wine/THQNORDIC_LOGO_Color_white.mp4";

    IMFSourceResolver *resolver;
    IMFMediaSource *media_source;
    IMFAttributes *attributes;
    MF_OBJECT_TYPE object_type;
    IMFMediaType *media_type, *native_type;
    IMFSourceReaderCallback *callback;
    IMFDXGIDeviceManager *manager;
    IMFSourceReader *reader;
    IMFTransform *transform;
    ID3D11Device *d3d11;
    GUID major, subtype;
    UINT64 frame_rate;
    UINT32 profile;
    BOOL selected;
    UINT token;

    if (!winetest_platform_is_wine) path = L"file://Z:/debug/TEMP-GBP-Logo-Anim.mp4";
    if (!winetest_platform_is_wine) path = L"file://Z:/build-wine/THQNORDIC_LOGO_Color_white.mp4";

    ok_hr(S_OK, async_callback_create(&callback));
    ok_hr(S_OK, D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_VIDEO_SUPPORT, NULL, 0,
            D3D11_SDK_VERSION, &d3d11, NULL, NULL));

    ok_hr(S_OK, MFCreateAttributes(&attributes, 4));
    ok_hr(S_OK, IMFAttributes_SetUINT32(attributes, &MF_SOURCE_READER_DISABLE_DXVA, 0));
    ok_hr(S_OK, IMFAttributes_SetUINT32(attributes, &MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, 1));
    ok_hr(S_OK, IMFAttributes_SetUINT32(attributes, &MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, 1));
    ok_hr(S_OK, IMFAttributes_SetUINT32(attributes, &MF_SOURCE_READER_D3D11_BIND_FLAGS, 8));
    ok_hr(S_OK, IMFAttributes_SetUINT32(attributes, &MF_XVP_DISABLE_FRC, 1));
    ok_hr(S_OK, IMFAttributes_SetUINT32(attributes, &MF_SA_D3D11_SHARED_WITHOUT_MUTEX, 1));

    ok_hr(S_OK, MFCreateDXGIDeviceManager(&token, &manager));
    ok_hr(S_OK, IMFDXGIDeviceManager_ResetDevice(manager, (IUnknown *)d3d11, token));
    ID3D11Device_Release(d3d11);
    ok_hr(S_OK, IMFAttributes_SetUnknown(attributes, &MF_SOURCE_READER_D3D_MANAGER, (IUnknown *)manager));
    IMFDXGIDeviceManager_Release(manager);

    ok_hr(S_OK, IMFAttributes_SetUnknown(attributes, &MF_SOURCE_READER_ASYNC_CALLBACK, (IUnknown *)callback));
    ok_hr(S_OK, MFCreateSourceResolver(&resolver));
    /* ok_hr(S_OK, IMFSourceResolver_CreateObjectFromByteStream(resolver, 0x0A581260, path, 0x1, NULL, 0x102EFCD4, 0x102EFCF4)); */
    ok_hr(S_OK, IMFSourceResolver_CreateObjectFromURL(resolver, path, 0x1, NULL, &object_type, (IUnknown **)&media_source));
    ok_hr(S_OK, MFCreateSourceReaderFromMediaSource(media_source, attributes, &reader));
/*
    ok_hr(S_OK, MFGetService(017E95E0, MF_SCRUBBING_SERVICE, IID_IMFSeekInfo, 40B7A368));
*/
    IMFMediaSource_Release(media_source);
    IMFSourceResolver_Release(resolver);

    impl_from_IMFSourceReaderCallback(callback)->reader = reader;

/*
    ok_hr(S_OK, IMFSourceReader_GetPresentationAttribute(reader, index -1, &MF_PD_MIME_TYPE, value 102EFBA0));
*/
    ok_hr(S_OK, MFCreateMediaType(&media_type));
    ok_hr(S_OK, IMFMediaType_SetGUID(media_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video));
    ok_hr(S_OK, IMFMediaType_SetGUID(media_type, &MF_MT_SUBTYPE, &MFVideoFormat_ARGB32));
    ok_hr(S_OK, IMFSourceReader_GetNativeMediaType(reader, 0, 0, &native_type));
    ok_hr(S_OK, IMFMediaType_GetGUID(native_type, &MF_MT_MAJOR_TYPE, &major));
    IMFMediaType_Release(native_type);
    ok_hr(0xc00d36b9, IMFSourceReader_GetNativeMediaType(reader, 0, 1, &native_type));
    ok_hr(S_OK, IMFSourceReader_GetNativeMediaType(reader, 1, 0, &native_type));
    ok_hr(S_OK, IMFMediaType_GetGUID(native_type, &MF_MT_MAJOR_TYPE, &major));
    ok_hr(S_OK, IMFSourceReader_SetStreamSelection(reader, 1, 0));
    ok_hr(S_OK, IMFMediaType_GetUINT32(native_type, &MF_MT_VIDEO_PROFILE, &profile));
    IMFMediaType_Release(native_type);
    ok_hr(0xc00d36b9, IMFSourceReader_GetNativeMediaType(reader, 1, 1, &native_type));
    ok_hr(0xc00d36b3, IMFSourceReader_GetNativeMediaType(reader, 2, 0, &native_type));
    ok_hr(S_OK, IMFMediaType_SetUINT32(media_type, &MF_MT_VIDEO_ROTATION, 0));
    ok_hr(S_OK, IMFSourceReader_SetCurrentMediaType(reader, 1, 0, media_type));
    ok_hr(S_OK, IMFSourceReader_GetServiceForStream(reader, 1, &GUID_NULL, &IID_IMFTransform, (void **)&transform));
    ok_hr(S_OK, IMFTransform_GetOutputStreamAttributes(transform, 0, &attributes));
    ok_hr(S_OK, IMFAttributes_SetUINT32(attributes, &MF_SA_D3D11_USAGE, 0));
    ok_hr(S_OK, IMFAttributes_SetUINT32(attributes, &MF_SA_D3D11_BINDFLAGS, 520));
    ok_hr(S_OK, IMFAttributes_SetUINT32(attributes, &MF_SA_D3D11_SHARED_WITHOUT_MUTEX, 1));
    IMFAttributes_Release(attributes);
    IMFTransform_Release(transform);
    ok_hr(S_OK, IMFSourceReader_SetStreamSelection(reader, 1, 1));
    IMFMediaType_Release(media_type);
    ok_hr(S_OK, IMFSourceReader_GetNativeMediaType(reader, 0, 0, &native_type));
    ok_hr(S_OK, IMFMediaType_GetGUID(native_type, &MF_MT_MAJOR_TYPE, &major));
    ok_hr(S_OK, MFCreateMediaType(&media_type));
    ok_hr(S_OK, IMFMediaType_SetGUID(media_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio));
    ok_hr(S_OK, IMFMediaType_SetGUID(media_type, &MF_MT_SUBTYPE, &MFAudioFormat_Float));
    ok_hr(S_OK, IMFSourceReader_SetCurrentMediaType(reader, 0, 0, media_type));
    ok_hr(S_OK, IMFSourceReader_SetStreamSelection(reader, 0, 1));
    IMFMediaType_Release(media_type);
    IMFMediaType_Release(native_type);
    ok_hr(0xc00d36b9, IMFSourceReader_GetNativeMediaType(reader, 0, 1, &native_type));
    ok_hr(S_OK, IMFSourceReader_GetNativeMediaType(reader, 1, 0, &native_type));
    ok_hr(S_OK, IMFMediaType_GetGUID(native_type, &MF_MT_MAJOR_TYPE, &major));
    IMFMediaType_Release(native_type);
    ok_hr(0xc00d36b9, IMFSourceReader_GetNativeMediaType(reader, 1, 1, &native_type));
    ok_hr(0xc00d36b3, IMFSourceReader_GetNativeMediaType(reader, 2, 0, &native_type));
    ok_hr(S_OK, IMFSourceReader_GetCurrentMediaType(reader, 1, &media_type));
    ok_hr(S_OK, IMFSourceReader_GetNativeMediaType(reader, 1, 0, &native_type));
    ok_hr(S_OK, IMFMediaType_GetGUID(native_type, &MF_MT_SUBTYPE, &subtype));
    IMFMediaType_Release(native_type);
    ok_hr(S_OK, IMFMediaType_GetUINT64(media_type, &MF_MT_FRAME_RATE, &frame_rate));
/*
    ok_hr(S_OK, IMFSourceReader_GetPresentationAttribute(reader, -1, &MF_PD_DURATION, &duration));
*/
    ok_hr(S_OK, IMFSourceReader_GetNativeMediaType(reader, 0, 0, &native_type));
    ok_hr(S_OK, IMFMediaType_GetGUID(native_type, &MF_MT_SUBTYPE, &subtype));
    IMFMediaType_Release(native_type);
    IMFMediaType_Release(media_type);
/*
    ok_hr(S_OK, IMFSourceReader_GetPresentationAttribute(reader, index -1, guid {6d23f5c8-c5d7-4a9b-9971-5d11f8bca880}, value 102EFC20));
*/
    ok_hr(S_OK, IMFSourceReader_GetNativeMediaType(reader, 0, 0, &native_type));
    ok_hr(S_OK, IMFMediaType_GetGUID(native_type, &MF_MT_MAJOR_TYPE, &major));
    ok_hr(S_OK, IMFSourceReader_GetStreamSelection(reader, 0, &selected));
    ok_hr(S_OK, IMFSourceReader_ReadSample(reader, 0, 0, 0, 0, 0, 0));
    IMFMediaType_Release(native_type);
    ok_hr(S_OK, IMFSourceReader_GetNativeMediaType(reader, 1, 0, &native_type));
    ok_hr(S_OK, IMFMediaType_GetGUID(native_type, &MF_MT_MAJOR_TYPE, &major));
    ok_hr(S_OK, IMFSourceReader_GetStreamSelection(reader, 1, &selected));
    ok_hr(S_OK, IMFSourceReader_ReadSample(reader, 1, 0, 0, 0, 0, 0));

Sleep(10000000);
}

START_TEST(games)
{
    HRESULT hr;

    hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

    have_a_nice_night();

    hr = MFShutdown();
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
}
