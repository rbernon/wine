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

#include "ole2.h"
#include "guiddef.h"
#include "mfapi.h"
#include "mfidl.h"
#include "mferror.h"
#include "mfreadwrite.h"
#include "propvarutil.h"
#include "initguid.h"
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
#define ok_hr( e, r ) ok_eq( e, r, HRESULT, "%#lx" )

#define dump_media_type(a) dump_attributes_(__LINE__, (IMFAttributes *)a)
#define dump_attributes(a) dump_attributes_(__LINE__, a)
extern void dump_attributes_(int line, IMFAttributes *attributes);
extern void dump_properties(IPropertyStore *store);

extern const char *debugstr_mf_guid(const GUID *guid);
extern void dump_attributes_(int line, IMFAttributes *attributes);

START_TEST(vrchat)
{
    IMFMediaType *media_type, *tmp_type;
    ID3D11Multithread *multithread;
    IMFDXGIDeviceManager *manager;
    IMFDXGIBuffer *dxgi_buffer;
    IMFAttributes *attributes;
    DWORD count, sample_flags;
    ID3D11Texture2D *texture;
    IMFSourceReader *reader;
    IMFTransform *transform;
    IMFMediaBuffer *buffer;
    UINT32 token, value32;
    ID3D11Device *d3d11;
    GUID major, subtype;
    LONGLONG timestamp;
    PROPVARIANT value;
    IMFSample *sample;
    UINT64 value64;

    ok_hr(S_OK, MFStartup(MF_VERSION, MFSTARTUP_FULL));

    ok_hr(S_OK, D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_VIDEO_SUPPORT, NULL, 0,
            D3D11_SDK_VERSION, &d3d11, NULL, NULL));
    ok_hr(S_OK, ID3D11Device_QueryInterface(d3d11, &IID_ID3D11Multithread, (void **)&multithread));
    ID3D11Multithread_SetMultithreadProtected(multithread, TRUE);
    ID3D11Multithread_Release(multithread);

    ok_hr(S_OK, MFCreateAttributes(&attributes, 4));
    ok_hr(S_OK, IMFAttributes_SetUINT32(attributes, &MF_SOURCE_READER_DISABLE_DXVA, 0));
    ok_hr(S_OK, IMFAttributes_SetUINT32(attributes, &MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, 1));
    ok_hr(S_OK, IMFAttributes_SetUINT32(attributes, &MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, 1));
    ok_hr(S_OK, IMFAttributes_SetUINT32(attributes, &MF_SOURCE_READER_D3D11_BIND_FLAGS, 8));
    ok_hr(S_OK, IMFAttributes_SetUINT32(attributes, &MF_XVP_DISABLE_FRC, 1));
    ok_hr(S_OK, IMFAttributes_SetUINT32(attributes, &MF_SA_D3D11_SHARED_WITHOUT_MUTEX, 1));

    ok_hr(S_OK, MFCreateDXGIDeviceManager(&token, &manager));
    ok_hr(S_OK, IMFDXGIDeviceManager_ResetDevice(manager, (IUnknown *)d3d11, token));
    /* ok_hr(S_OK, IMFDXGIDeviceManager_QueryInterface( 0000000269AF3260, {00000000-0000-0000-c000-000000000046}, 00000000685FF0F0)); */
    ok_hr(S_OK, IMFAttributes_SetUnknown(attributes, &MF_SOURCE_READER_D3D_MANAGER, (IUnknown *)manager));
    /* ok_hr(S_OK, IMFAttributes_SetUnknown(attributes, MF_SOURCE_READER_ASYNC_CALLBACK, 00000000E3B73580)); */

    ok_hr(S_OK, MFCreateSourceReaderFromURL(L"http://192.168.1.32:8000/Final fantasy 8 (pc) ： credits opening [A9pDAFS4oOA].mp4", attributes, &reader));
    IMFAttributes_Release(attributes);

    ok_hr(S_OK, IMFSourceReader_GetPresentationAttribute(reader, -1, &MF_PD_MIME_TYPE, &value));
    ok(0, "got %s\n", wine_dbgstr_variant((VARIANT *)&value));


    ok_hr(S_OK, MFCreateMediaType(&media_type));
    ok_hr(S_OK, IMFMediaType_SetGUID(media_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video));
    ok_hr(S_OK, IMFMediaType_SetGUID(media_type, &MF_MT_SUBTYPE, &MFVideoFormat_ARGB32));

    ok_hr(S_OK, IMFSourceReader_GetNativeMediaType(reader, 0, 0, &tmp_type));
winetest_pop_context();
winetest_push_context("native 0");
dump_media_type(tmp_type);
    ok_hr(S_OK, IMFMediaType_GetGUID(tmp_type, &MF_MT_MAJOR_TYPE, &major));
    IMFMediaType_Release(tmp_type);
    ok_hr(MF_E_NO_MORE_TYPES, IMFSourceReader_GetNativeMediaType(reader, 0, 1, &tmp_type));

    ok_hr(S_OK, IMFSourceReader_GetNativeMediaType(reader, 1, 0, &tmp_type));
winetest_push_context("native 1");
dump_media_type(tmp_type);
winetest_pop_context();
    ok_hr(S_OK, IMFMediaType_GetGUID(tmp_type, &MF_MT_MAJOR_TYPE, &major));
    ok_hr(S_OK, IMFSourceReader_SetStreamSelection(reader, 1, 0));
    ok_hr(S_OK, IMFMediaType_GetUINT32(tmp_type, &MF_MT_VIDEO_PROFILE, &value32));
    IMFMediaType_Release(tmp_type);

    ok_hr(MF_E_NO_MORE_TYPES, IMFSourceReader_GetNativeMediaType(reader, 1, 1, &tmp_type));
    ok_hr(MF_E_INVALIDSTREAMNUMBER, IMFSourceReader_GetNativeMediaType(reader, 2, 0, &tmp_type));

    ok_hr(S_OK, IMFMediaType_SetUINT32(media_type, &MF_MT_VIDEO_ROTATION, 0));
    ok_hr(S_OK, IMFSourceReader_SetCurrentMediaType(reader, 1, 0, media_type));


    ok_hr(S_OK, IMFSourceReader_GetServiceForStream(reader, 1, &GUID_NULL, &IID_IMFTransform, (void **)&transform));
winetest_push_context("transform 1 input");
ok_hr(S_OK, IMFTransform_GetInputCurrentType(transform, 0, &tmp_type));
dump_media_type(tmp_type);
IMFMediaType_Release(tmp_type);
winetest_pop_context();
winetest_push_context("transform 1 output");
ok_hr(S_OK, IMFTransform_GetOutputCurrentType(transform, 0, &tmp_type));
dump_media_type(tmp_type);
IMFMediaType_Release(tmp_type);
winetest_pop_context();

    ok_hr(S_OK, IMFTransform_GetAttributes(transform, &attributes));
winetest_push_context("transform attributes");
dump_attributes(attributes);
winetest_pop_context();
    IMFAttributes_Release(attributes);

    ok_hr(S_OK, IMFTransform_GetOutputStreamAttributes(transform, 0, &attributes));
winetest_push_context("transform output attributes");
dump_attributes(attributes);
winetest_pop_context();
    ok_hr(S_OK, IMFAttributes_SetUINT32(attributes, &MF_SA_D3D11_USAGE, 0));
    ok_hr(S_OK, IMFAttributes_SetUINT32(attributes, &MF_SA_D3D11_BINDFLAGS, 520));
    ok_hr(S_OK, IMFAttributes_SetUINT32(attributes, &MF_SA_D3D11_SHARED_WITHOUT_MUTEX, 1));
    IMFAttributes_Release(attributes);
    IMFTransform_Release(transform);


    ok_hr(S_OK, IMFSourceReader_SetStreamSelection(reader, 1, 1));
    IMFMediaType_Release(media_type);


    ok_hr(S_OK, IMFSourceReader_GetNativeMediaType(reader, 0, 0, &tmp_type));
    ok_hr(S_OK, IMFMediaType_GetGUID(tmp_type, &MF_MT_MAJOR_TYPE, &major));
    IMFMediaType_Release(tmp_type);
    ok_hr(MF_E_NO_MORE_TYPES, IMFSourceReader_GetNativeMediaType(reader, 0, 1, &tmp_type));

    ok_hr(S_OK, MFCreateMediaType(&media_type));
    ok_hr(S_OK, IMFMediaType_SetGUID(media_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio));
    ok_hr(S_OK, IMFMediaType_SetGUID(media_type, &MF_MT_SUBTYPE, &MFAudioFormat_Float));
    ok_hr(S_OK, IMFSourceReader_SetCurrentMediaType(reader, 0, 0, media_type));

    ok_hr(S_OK, IMFSourceReader_SetStreamSelection(reader, 1, 1));
    IMFMediaType_Release(media_type);


    ok_hr(S_OK, IMFSourceReader_GetNativeMediaType(reader, 1, 0, &tmp_type));
    ok_hr(S_OK, IMFMediaType_GetGUID(tmp_type, &MF_MT_SUBTYPE, &subtype));
    IMFMediaType_Release(tmp_type);

    ok_hr(S_OK, IMFSourceReader_GetCurrentMediaType(reader, 1, &tmp_type));
    ok_hr(S_OK, IMFMediaType_GetUINT64(tmp_type, &MF_MT_FRAME_RATE, &value64));
    IMFMediaType_Release(tmp_type);

    ok_hr(S_OK, IMFSourceReader_GetNativeMediaType(reader, 0, 0, &tmp_type));
    ok_hr(S_OK, IMFMediaType_GetGUID(tmp_type, &MF_MT_SUBTYPE, &subtype));
    IMFMediaType_Release(tmp_type);

    ok_hr(S_OK, IMFSourceReader_GetPresentationAttribute(reader, -1, &MF_PD_DURATION, &value));
    ok(0, "got %s\n", wine_dbgstr_variant((VARIANT *)&value));

    ok_hr(S_OK, IMFSourceReader_GetPresentationAttribute(reader, -1, &MF_SOURCE_READER_MEDIASOURCE_CHARACTERISTICS, &value));
    ok(0, "got %s\n", wine_dbgstr_variant((VARIANT *)&value));

    ok_hr(S_OK, IMFSourceReader_ReadSample(reader, 0, 0, NULL, &sample_flags, NULL, &sample));
    ok(0, "got %#lx\n", sample_flags);
    ok(0, "got %p\n", sample);
    IMFSample_Release(sample);
    ok_hr(S_OK, IMFSourceReader_ReadSample(reader, 1, 0, NULL, &sample_flags, NULL, &sample));
    ok(0, "got %#lx\n", sample_flags);
    ok(0, "got %p\n", sample);
    IMFSample_Release(sample);

/*
sample.c:205:sample_tracked_Release 0000000269D5B640, refcount 2.
sample.c:515:sample_GetSampleTime 0000000269D5B640, 00000000685FF098.
sample.c:263:sample_GetUINT64 0000000269D5B640, MFSampleExtension_DecodeTimestamp, 00000000685FF090.
sample.c:205:sample_tracked_Release 0000000269D5B640, refcount 1.
mediatype.c:208:IMFMediaType_Release 0000000269B85B00, refcount 0.
reader.c:1934:IMFSourceReader_GetNativeMediaType(reader, index 2, type_index 0, type 00000000685FEFC0));
reader.c:1936:IMFSourceReader_GetNativeMediaType(reader, type 0000000000000000, ret 0xc00d36b3));
reader.c:1944:IMFSourceReader_GetCurrentMediaType(reader, index 1, type 00000000685FEE00));
reader.c:1970:IMFSourceReader_GetCurrentMediaType(reader, type 0000000269B85880, ret 0));
mediatype.c:289:IMFMediaType_GetGUID 0000000269B85880, MF_MT_SUBTYPE, 00000000685FEEE8.
reader.c:1934:IMFSourceReader_GetNativeMediaType(reader, index 1, type_index 0, type 00000000685FEE40));
reader.c:1936:IMFSourceReader_GetNativeMediaType(reader, type 0000000269B85980, ret 0));
mediatype.c:262:IMFMediaType_GetUINT32 0000000269B85980, MF_MT_VIDEO_ROTATION, 00000000685FF098.
mediatype.c:198:IMFMediaType_AddRef 0000000269B85880, refcount 2.
reader.c:2559:IMFSourceReader_GetServiceForStream(reader, index 1, service {00000000-0000-0000-0000-000000000000}, riid {bf94c121-5b05-4e6f-8000-ba598961414d}, object 00000000685FEE28));
reader.c:2608:IMFSourceReader_GetServiceForStream(reader, object 0000000269B859B0, ret 0));
video_processor.c:648:IMFTransform_GetInputCurrentType iface 0000000269B859B0, id 0, type 00000000685FEDF8.
mediatype.c:208:IMFMediaType_Release 0000000269B85880, refcount 1.
video_processor.c:367:IMFTransform_Release iface 0000000269B859B0 decreasing refcount to 2.
mediatype.c:271:IMFMediaType_GetUINT64 0000000269B85B00, MF_MT_FRAME_SIZE, 00000000685FEE38.
mediatype.c:262:IMFMediaType_GetUINT32 0000000269B85B00, MF_MT_PAN_SCAN_ENABLED, 00000000685FEE10.
mediatype.c:337:IMFMediaType_GetBlob 0000000269B85B00, MF_MT_MINIMUM_DISPLAY_APERTURE, 00000000685FEDD0, 16, 0000000000000000.
mediatype.c:337:IMFMediaType_GetBlob 0000000269B85B00, MF_MT_GEOMETRIC_APERTURE, 00000000685FEDD0, 16, 0000000000000000.
mediatype.c:271:IMFMediaType_GetUINT64 0000000269B85B00, MF_MT_FRAME_SIZE, 00000000685FEED0.
mediatype.c:271:IMFMediaType_GetUINT64 0000000269B85B00, MF_MT_PIXEL_ASPECT_RATIO, 00000000685FEED8.
mediatype.c:262:IMFMediaType_GetUINT32 0000000269B85880, MF_MT_DEFAULT_STRIDE, 00000000685FED90.
mediatype.c:262:IMFMediaType_GetUINT32 0000000269B85880, MF_MT_VIDEO_PRIMARIES, 00000000685FF090.
mediatype.c:262:IMFMediaType_GetUINT32 0000000269B85B00, MF_MT_VIDEO_PRIMARIES, 00000000685FEE14.
ok_hr(S_OK, IMFMediaType_SetUINT32 0000000269B85880, MF_MT_VIDEO_PRIMARIES, 0);
mediatype.c:271:IMFMediaType_GetUINT64 0000000269B85880, MF_MT_PIXEL_ASPECT_RATIO, 00000000685FEEE0.
reader.c:1716:IMFSourceReader_QueryInterface(reader, riid {7b981cf0-560e-4116-9875-b099895f23d7}, out 00000000685FED10));
reader.c:1732:IMFSourceReader_QueryInterface(reader, out 00000000756C7C80, ret 0));
reader.c:2703:IMFSourceReader_GetTransformForStream(reader, stream_index 1, transform_index 0, category {00000500-0000-0000-70ed-5f6800000000}, transform 00000000685FED00));
reader.c:2687:get_transform_at_index stream 0000000075997A78, index 0.
reader.c:2691:get_transform_at_index stream 0000000075997A78, ret 0000000269C4DBB0.
reader.c:2731:IMFSourceReader_GetTransformForStream(reader, category 00000000669B3355, transform 0000000075997AE0, ret 0));
video_decoder.c:267:transform_Release iface 0000000075997AE0 decreasing refcount to 1.
reader.c:2703:IMFSourceReader_GetTransformForStream(reader, stream_index 1, transform_index 1, category {d6c02d4b-6833-45b4-971a-05a4b04bab91}, transform 00000000685FED00));
reader.c:2687:get_transform_at_index stream 0000000075997A78, index 1.
reader.c:2691:get_transform_at_index stream 0000000075997A78, ret 0000000269C4DC30.
reader.c:2731:IMFSourceReader_GetTransformForStream(reader, category 00000000669B33A3, transform 0000000269B859B0, ret 0));
video_processor.c:400:IMFTransform_GetStreamCount iface 0000000269B859B0, inputs 00000000685FEDA0, outputs 00000000685FED90.
video_processor.c:408:IMFTransform_GetStreamIDs iface 0000000269B859B0, input_size 1, inputs 00000000C1670010, output_size 1, outputs 00000000C1670030.
video_processor.c:648:IMFTransform_GetInputCurrentType iface 0000000269B859B0, id 0, type 00000000685FED08.
mediatype.c:262:IMFMediaType_GetUINT32 0000000269B85B80, {cb5e88cf-7b5b-476b-85aa-1ca5ae187555}, 00000000685FEDA8.
mediatype.c:208:IMFMediaType_Release 0000000269B85B80, refcount 0.
video_processor.c:367:IMFTransform_Release iface 0000000269B859B0 decreasing refcount to 2.
reader.c:1769:IMFSourceReader_Release(reader));
reader.c:1793:IMFSourceReader_Release(reader, ret 1));
reader.c:1944:IMFSourceReader_GetCurrentMediaType(reader, index 0, type 00000000685FEDE0));
reader.c:1970:IMFSourceReader_GetCurrentMediaType(reader, type 0000000269B85B80, ret 0));
mediatype.c:262:IMFMediaType_GetUINT32 0000000269B85B80, MF_MT_AUDIO_NUM_CHANNELS, 00000000685FF088.
mediatype.c:262:IMFMediaType_GetUINT32 0000000269B85B80, MF_MT_AUDIO_SAMPLES_PER_SECOND, 00000000685FEE18.
reader.c:2619:IMFSourceReader_GetPresentationAttribute(reader, index 0, guid {00af2180-bdc2-423c-abca-f503593bc121}, value 00000000685FEF48));
reader.c:2656:IMFSourceReader_GetPresentationAttribute(reader, value 00000000685FEF48, ret 0xc00d36e6));
mediatype.c:208:IMFMediaType_Release 0000000269B85B80, refcount 0.
mediatype.c:208:IMFMediaType_Release 0000000269B85B00, refcount 0.
mediatype.c:208:IMFMediaType_Release 0000000269B85980, refcount 0.
mediatype.c:208:IMFMediaType_Release 0000000269B85880, refcount 0.
reader.c:2559:IMFSourceReader_GetServiceForStream(reader, index 1, service {00000000-0000-0000-0000-000000000000}, riid {bf94c121-5b05-4e6f-8000-ba598961414d}, object 00000000685FF020));
reader.c:2608:IMFSourceReader_GetServiceForStream(reader, object 0000000269B859B0, ret 0));
video_processor.c:670:IMFTransform_GetOutputCurrentType iface 0000000269B859B0, id 0, type 00000000685FF008.
mediatype.c:271:IMFMediaType_GetUINT64 0000000269B85880, MF_MT_FRAME_SIZE, 00000000685FF038.
mediatype.c:208:IMFMediaType_Release 0000000269B85880, refcount 0.
video_processor.c:443:IMFTransform_GetAttributes iface 0000000269B859B0, attributes 00000000685FF008 semi-stub!
main.c:3051:IMFAttributes_GetUINT32 000000000119AE70, MF_SA_MINIMUM_OUTPUT_SAMPLE_COUNT, 00000000685FF038.
reader.c:1934:IMFSourceReader_GetNativeMediaType(reader, index 0, type_index 0, type 00000000685FECE0));
reader.c:1936:IMFSourceReader_GetNativeMediaType(reader, type 0000000269B85880, ret 0));
mediatype.c:289:IMFMediaType_GetGUID 0000000269B85880, MF_MT_MAJOR_TYPE, 00000000685FED18.
mediatype.c:289:IMFMediaType_GetGUID 0000000269B85880, MF_MT_SUBTYPE, 00000000685FED30.
mediatype.c:208:IMFMediaType_Release 0000000269B85880, refcount 0.
reader.c:1934:IMFSourceReader_GetNativeMediaType(reader, index 0, type_index 1, type 00000000685FECE0));
reader.c:1936:IMFSourceReader_GetNativeMediaType(reader, type 0000000000000000, ret 0xc00d36b9));
reader.c:1934:IMFSourceReader_GetNativeMediaType(reader, index 1, type_index 0, type 00000000685FECE0));
reader.c:1936:IMFSourceReader_GetNativeMediaType(reader, type 0000000269B85880, ret 0));
mediatype.c:289:IMFMediaType_GetGUID 0000000269B85880, MF_MT_MAJOR_TYPE, 00000000685FED18.
mediatype.c:289:IMFMediaType_GetGUID 0000000269B85880, MF_MT_SUBTYPE, 00000000685FED30.
mediatype.c:271:IMFMediaType_GetUINT64 0000000269B85880, MF_MT_FRAME_SIZE, 00000000685FEDB8.
mediatype.c:208:IMFMediaType_Release 0000000269B85880, refcount 0.
reader.c:1934:IMFSourceReader_GetNativeMediaType(reader, index 1, type_index 1, type 00000000685FECE0));
reader.c:1936:IMFSourceReader_GetNativeMediaType(reader, type 0000000000000000, ret 0xc00d36b9));
reader.c:1934:IMFSourceReader_GetNativeMediaType(reader, index 2, type_index 0, type 00000000685FECE0));
reader.c:1936:IMFSourceReader_GetNativeMediaType(reader, type 0000000000000000, ret 0xc00d36b3));
main.c:2376:IMFAttributes_Release 000000000119AE70, refcount 1.
mediatype.c:1540:MFCreateMediaType 00000000685FF000.
mediatype.c:1550:MFCreateMediaType Created media type 0000000269B85880.
ok_hr(S_OK, IMFMediaType_SetGUID 0000000269B85880, MF_MT_MAJOR_TYPE, MFMediaType_Video);
ok_hr(S_OK, IMFMediaType_SetGUID 0000000269B85880, MF_MT_SUBTYPE, MFVideoFormat_ARGB32);
ok_hr(S_OK, IMFMediaType_SetUINT64 0000000269B85880, MF_MT_FRAME_SIZE, 500000002d0);
sample.c:1856:MFCreateVideoSampleAllocatorEx {545b3a48-3283-4f62-866f-a62d8f598f9f}, 00000000685FF018.
sample.c:1290:sample_allocator_SetDirectXManager 0000000075997C60, 0000000269AF3260.
main.c:3358:MFCreateAttributes 00000000685FF030, 4
sample.c:1679:sample_allocator_InitializeSampleAllocatorEx 0000000075997C60, 0, 6, 000000000119AC90, 0000000269B85880.
main.c:2376:IMFAttributes_Release 000000000119AC90, refcount 1.
mediatype.c:208:IMFMediaType_Release 0000000269B85880, refcount 1.
video_processor.c:367:IMFTransform_Release iface 0000000269B859B0 decreasing refcount to 2.
*/

    ok_hr(S_OK, IMFSourceReader_Flush(reader, -2));

    PropVariantInit(&value);
    value.vt = VT_I8;
    value.hVal.QuadPart = 0;
    ok_hr(S_OK, IMFSourceReader_SetCurrentPosition(reader, &GUID_NULL, &value));

    ok_hr(S_OK, IMFSourceReader_ReadSample(reader, 1, 0, NULL, &sample_flags, NULL, &sample));
    ok(0, "got %#lx\n", sample_flags);
    ok(0, "got %p\n", sample);
ok_hr(S_OK, IMFSample_GetBufferCount(sample, &count));
    ok(0, "got %lu\n", count);
ok_hr(S_OK, IMFSample_GetBufferByIndex(sample, 0, &buffer));
    ok(0, "got %p\n", buffer);
ok_hr(S_OK, IMFMediaBuffer_QueryInterface(buffer, &IID_IMFDXGIBuffer, (void **)&dxgi_buffer));
    ok(0, "got %p\n", dxgi_buffer);
ok_hr(S_OK, IMFDXGIBuffer_GetResource(dxgi_buffer, &IID_ID3D11Texture2D, (void **)&texture));
    ok(0, "got %p\n", texture);
ok_hr(S_OK, IMFSample_GetSampleTime(sample, &timestamp));
    ok(0, "got %I64d\n", timestamp);
    IMFSample_Release(sample);

    ok_hr(S_OK, IMFSourceReader_ReadSample(reader, 0, 0, NULL, &sample_flags, NULL, &sample));
    ok(0, "got %#lx\n", sample_flags);
    ok(0, "got %p\n", sample);
ok_hr(S_OK, IMFSample_GetBufferCount(sample, &count));
    ok(0, "got %lu\n", count);
ok_hr(S_OK, IMFSample_GetBufferByIndex(sample, 0, &buffer));
    ok(0, "got %p\n", buffer);
ok_hr(S_OK, IMFSample_GetSampleTime(sample, &timestamp));
    ok(0, "got %I64d\n", timestamp);
    IMFSample_Release(sample);
    ok_hr(S_OK, IMFSourceReader_ReadSample(reader, 0, 0, NULL, &sample_flags, NULL, &sample));
    ok(0, "got %#lx\n", sample_flags);
    ok(0, "got %p\n", sample);
ok_hr(S_OK, IMFSample_GetBufferCount(sample, &count));
    ok(0, "got %lu\n", count);
ok_hr(S_OK, IMFSample_GetBufferByIndex(sample, 0, &buffer));
    ok(0, "got %p\n", buffer);
ok_hr(S_OK, IMFSample_GetSampleTime(sample, &timestamp));
    ok(0, "got %I64d\n", timestamp);
    IMFSample_Release(sample);
    ok_hr(S_OK, IMFSourceReader_ReadSample(reader, 0, 0, NULL, &sample_flags, NULL, &sample));
    ok(0, "got %#lx\n", sample_flags);
    ok(0, "got %p\n", sample);
ok_hr(S_OK, IMFSample_GetBufferCount(sample, &count));
    ok(0, "got %lu\n", count);
ok_hr(S_OK, IMFSample_GetBufferByIndex(sample, 0, &buffer));
    ok(0, "got %p\n", buffer);
ok_hr(S_OK, IMFSample_GetSampleTime(sample, &timestamp));
    ok(0, "got %I64d\n", timestamp);
    IMFSample_Release(sample);


    ok_hr(S_OK, IMFSourceReader_GetServiceForStream(reader, 1, &GUID_NULL, &IID_IMFTransform, (void **)&transform));
winetest_push_context("transform 1 input");
ok_hr(S_OK, IMFTransform_GetInputCurrentType(transform, 0, &tmp_type));
dump_media_type(tmp_type);
IMFMediaType_Release(tmp_type);
winetest_pop_context();
winetest_push_context("transform 1 output");
ok_hr(S_OK, IMFTransform_GetOutputCurrentType(transform, 0, &tmp_type));
dump_media_type(tmp_type);
IMFMediaType_Release(tmp_type);
winetest_pop_context();

    ok_hr(S_OK, IMFTransform_GetAttributes(transform, &attributes));
winetest_push_context("transform attributes");
dump_attributes(attributes);
winetest_pop_context();
    IMFAttributes_Release(attributes);

    ok_hr(S_OK, IMFTransform_GetOutputStreamAttributes(transform, 0, &attributes));
winetest_push_context("transform output attributes");
dump_attributes(attributes);
winetest_pop_context();
    IMFAttributes_Release(attributes);

    IMFTransform_Release(transform);

    ok_hr(S_OK, IMFSourceReader_ReadSample(reader, 1, 0, NULL, &sample_flags, NULL, &sample));
    ok(0, "got %#lx\n", sample_flags);
    ok(0, "got %p\n", sample);
ok_hr(S_OK, IMFSample_GetBufferCount(sample, &count));
    ok(0, "got %lu\n", count);
ok_hr(S_OK, IMFSample_GetBufferByIndex(sample, 0, &buffer));
    ok(0, "got %p\n", buffer);
ok_hr(S_OK, IMFSample_GetSampleTime(sample, &timestamp));
    ok(0, "got %I64d\n", timestamp);
    IMFSample_Release(sample);
    ok_hr(S_OK, IMFSourceReader_ReadSample(reader, 1, 0, NULL, &sample_flags, NULL, &sample));
    ok(0, "got %#lx\n", sample_flags);
    ok(0, "got %p\n", sample);
ok_hr(S_OK, IMFSample_GetBufferCount(sample, &count));
    ok(0, "got %lu\n", count);
ok_hr(S_OK, IMFSample_GetBufferByIndex(sample, 0, &buffer));
    ok(0, "got %p\n", buffer);
ok_hr(S_OK, IMFSample_GetSampleTime(sample, &timestamp));
    ok(0, "got %I64d\n", timestamp);
    IMFSample_Release(sample);
    ok_hr(S_OK, IMFSourceReader_ReadSample(reader, 1, 0, NULL, &sample_flags, NULL, &sample));
    ok(0, "got %#lx\n", sample_flags);
    ok(0, "got %p\n", sample);
ok_hr(S_OK, IMFSample_GetBufferCount(sample, &count));
    ok(0, "got %lu\n", count);
ok_hr(S_OK, IMFSample_GetBufferByIndex(sample, 0, &buffer));
    ok(0, "got %p\n", buffer);
ok_hr(S_OK, IMFSample_GetSampleTime(sample, &timestamp));
    ok(0, "got %I64d\n", timestamp);
    IMFSample_Release(sample);
    ok_hr(S_OK, IMFSourceReader_ReadSample(reader, 1, 0, NULL, &sample_flags, NULL, &sample));
    ok(0, "got %#lx\n", sample_flags);
    ok(0, "got %p\n", sample);
ok_hr(S_OK, IMFSample_GetBufferCount(sample, &count));
    ok(0, "got %lu\n", count);
ok_hr(S_OK, IMFSample_GetBufferByIndex(sample, 0, &buffer));
    ok(0, "got %p\n", buffer);
ok_hr(S_OK, IMFSample_GetSampleTime(sample, &timestamp));
    ok(0, "got %I64d\n", timestamp);
    IMFSample_Release(sample);
    ok_hr(S_OK, IMFSourceReader_ReadSample(reader, 1, 0, NULL, &sample_flags, NULL, &sample));
    ok(0, "got %#lx\n", sample_flags);
    ok(0, "got %p\n", sample);
ok_hr(S_OK, IMFSample_GetBufferCount(sample, &count));
    ok(0, "got %lu\n", count);
ok_hr(S_OK, IMFSample_GetBufferByIndex(sample, 0, &buffer));
    ok(0, "got %p\n", buffer);
ok_hr(S_OK, IMFSample_GetSampleTime(sample, &timestamp));
    ok(0, "got %I64d\n", timestamp);
    IMFSample_Release(sample);

    ok_hr(S_OK, IMFSourceReader_GetServiceForStream(reader, 1, &GUID_NULL, &IID_IMFTransform, (void **)&transform));
winetest_push_context("transform 1 input");
ok_hr(S_OK, IMFTransform_GetInputCurrentType(transform, 0, &tmp_type));
dump_media_type(tmp_type);
IMFMediaType_Release(tmp_type);
winetest_pop_context();
winetest_push_context("transform 1 output");
ok_hr(S_OK, IMFTransform_GetOutputCurrentType(transform, 0, &tmp_type));
dump_media_type(tmp_type);
IMFMediaType_Release(tmp_type);
winetest_pop_context();

    ok_hr(S_OK, IMFTransform_GetAttributes(transform, &attributes));
winetest_push_context("transform attributes");
dump_attributes(attributes);
winetest_pop_context();
    IMFAttributes_Release(attributes);

    ok_hr(S_OK, IMFTransform_GetOutputStreamAttributes(transform, 0, &attributes));
winetest_push_context("transform output attributes");
dump_attributes(attributes);
winetest_pop_context();
    IMFAttributes_Release(attributes);

    IMFTransform_Release(transform);

/*
main.c:8933:IMFDXGIDeviceManager_Release 0000000269AF3260, refcount 5.
main.c:2376:IMFAttributes_Release attributes, refcount 1.
sample.c:576:sample_GetBufferCount 0000000269D5B7A0, 000000006518FCF0.
sample.c:593:sample_GetBufferByIndex 0000000269D5B7A0, 0, 000000006518FCA0.
buffer.c:910:dxgi_1d_2d_buffer_QueryInterface 0000000269D5D1E0, {e7174cfa-1c9e-48b1-8866-626226bfc258}, 000000006518FC98.
buffer.c:1241:dxgi_buffer_GetResource 0000000269D5D1F0, {6f15aaf2-d208-4e89-9ab4-489535d34f9c}, 000000006518FC90.
sample.c:515:sample_GetSampleTime 0000000269D5B7A0, 000000006518FCA8.
sample.c:153:sample_AddRef 0000000269D5B7A0, refcount 3.
buffer.c:161:memory_buffer_Release 0000000269D5D1E0, refcount 2.
buffer.c:161:memory_buffer_Release 0000000269D5D1E0, refcount 1.
sample.c:205:sample_tracked_Release 0000000269D5B7A0, refcount 2.
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
sample.c:153:sample_AddRef 0000000269D5B640, refcount 3.
sample.c:153:sample_AddRef 0000000269D5B640, refcount 4.
sample.c:205:sample_tracked_Release 0000000269D5B640, refcount 2.
sample.c:515:sample_GetSampleTime 0000000269D5B640, 000000006538FD00.
sample.c:576:sample_GetBufferCount 0000000269D5B640, 000000006538FCF0.
sample.c:593:sample_GetBufferByIndex 0000000269D5B640, 0, 000000006538FCA0.
buffer.c:910:dxgi_1d_2d_buffer_QueryInterface 0000000269D5D0B0, {e7174cfa-1c9e-48b1-8866-626226bfc258}, 000000006538FC98.
buffer.c:1241:dxgi_buffer_GetResource 0000000269D5D0C0, {6f15aaf2-d208-4e89-9ab4-489535d34f9c}, 000000006538FC90.
sample.c:515:sample_GetSampleTime 0000000269D5B640, 000000006538FCA8.
sample.c:153:sample_AddRef 0000000269D5B640, refcount 3.
buffer.c:161:memory_buffer_Release 0000000269D5D0B0, refcount 2.
buffer.c:161:memory_buffer_Release 0000000269D5D0B0, refcount 1.
sample.c:205:sample_tracked_Release 0000000269D5B640, refcount 2.
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
sample.c:153:sample_AddRef 0000000269D5B900, refcount 3.
sample.c:153:sample_AddRef 0000000269D5B900, refcount 4.
sample.c:205:sample_tracked_Release 0000000269D5B900, refcount 2.
sample.c:515:sample_GetSampleTime 0000000269D5B900, 00000000668DFD00.
sample.c:576:sample_GetBufferCount 0000000269D5B900, 00000000668DFCF0.
sample.c:593:sample_GetBufferByIndex 0000000269D5B900, 0, 00000000668DFCA0.
buffer.c:910:dxgi_1d_2d_buffer_QueryInterface 0000000269D5D310, {e7174cfa-1c9e-48b1-8866-626226bfc258}, 00000000668DFC98.
buffer.c:1241:dxgi_buffer_GetResource 0000000269D5D320, {6f15aaf2-d208-4e89-9ab4-489535d34f9c}, 00000000668DFC90.
sample.c:515:sample_GetSampleTime 0000000269D5B900, 00000000668DFCA8.
sample.c:153:sample_AddRef 0000000269D5B900, refcount 3.
buffer.c:161:memory_buffer_Release 0000000269D5D310, refcount 2.
buffer.c:161:memory_buffer_Release 0000000269D5D310, refcount 1.
sample.c:205:sample_tracked_Release 0000000269D5B900, refcount 2.
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
sample.c:153:sample_AddRef 0000000269D5BBC0, refcount 3.
sample.c:153:sample_AddRef 0000000269D5BBC0, refcount 4.
sample.c:205:sample_tracked_Release 0000000269D5BBC0, refcount 2.
sample.c:515:sample_GetSampleTime 0000000269D5BBC0, 00000000668DFD00.
sample.c:576:sample_GetBufferCount 0000000269D5BBC0, 00000000668DFCF0.
sample.c:593:sample_GetBufferByIndex 0000000269D5BBC0, 0, 00000000668DFCA0.
buffer.c:910:dxgi_1d_2d_buffer_QueryInterface 0000000269D5D440, {e7174cfa-1c9e-48b1-8866-626226bfc258}, 00000000668DFC98.
buffer.c:1241:dxgi_buffer_GetResource 0000000269D5D450, {6f15aaf2-d208-4e89-9ab4-489535d34f9c}, 00000000668DFC90.
sample.c:515:sample_GetSampleTime 0000000269D5BBC0, 00000000668DFCA8.
sample.c:153:sample_AddRef 0000000269D5BBC0, refcount 3.
buffer.c:161:memory_buffer_Release 0000000269D5D440, refcount 2.
buffer.c:161:memory_buffer_Release 0000000269D5D440, refcount 1.
sample.c:205:sample_tracked_Release 0000000269D5BBC0, refcount 2.
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
sample.c:153:sample_AddRef 0000000269D5C090, refcount 3.
sample.c:153:sample_AddRef 0000000269D5C090, refcount 4.
sample.c:205:sample_tracked_Release 0000000269D5C090, refcount 2.
sample.c:515:sample_GetSampleTime 0000000269D5C090, 000000006518FD00.
sample.c:576:sample_GetBufferCount 0000000269D5C090, 000000006518FCF0.
sample.c:593:sample_GetBufferByIndex 0000000269D5C090, 0, 000000006518FCA0.
buffer.c:910:dxgi_1d_2d_buffer_QueryInterface 0000000269D5D7D0, {e7174cfa-1c9e-48b1-8866-626226bfc258}, 000000006518FC98.
buffer.c:1241:dxgi_buffer_GetResource 0000000269D5D7E0, {6f15aaf2-d208-4e89-9ab4-489535d34f9c}, 000000006518FC90.
sample.c:515:sample_GetSampleTime 0000000269D5C090, 000000006518FCA8.
sample.c:153:sample_AddRef 0000000269D5C090, refcount 3.
buffer.c:161:memory_buffer_Release 0000000269D5D7D0, refcount 2.
buffer.c:161:memory_buffer_Release 0000000269D5D7D0, refcount 1.
sample.c:205:sample_tracked_Release 0000000269D5C090, refcount 2.
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
sample.c:153:sample_AddRef 0000000269D5F610, refcount 3.
sample.c:153:sample_AddRef 0000000269D5F610, refcount 4.
sample.c:205:sample_tracked_Release 0000000269D5F610, refcount 2.
sample.c:515:sample_GetSampleTime 0000000269D5F610, 000000006518FD00.
sample.c:576:sample_GetBufferCount 0000000269D5F610, 000000006518FCF0.
sample.c:593:sample_GetBufferByIndex 0000000269D5F610, 0, 000000006518FCA0.
buffer.c:910:dxgi_1d_2d_buffer_QueryInterface 0000000269D5D900, {e7174cfa-1c9e-48b1-8866-626226bfc258}, 000000006518FC98.
buffer.c:1241:dxgi_buffer_GetResource 0000000269D5D910, {6f15aaf2-d208-4e89-9ab4-489535d34f9c}, 000000006518FC90.
sample.c:515:sample_GetSampleTime 0000000269D5F610, 000000006518FCA8.
sample.c:153:sample_AddRef 0000000269D5F610, refcount 3.
buffer.c:161:memory_buffer_Release 0000000269D5D900, refcount 2.
buffer.c:161:memory_buffer_Release 0000000269D5D900, refcount 1.
sample.c:205:sample_tracked_Release 0000000269D5F610, refcount 2.
sample.c:205:sample_tracked_Release 0000000269D5B7A0, refcount 1.
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
sample.c:153:sample_AddRef 0000000269D5F820, refcount 3.
sample.c:153:sample_AddRef 0000000269D5F820, refcount 3.
sample.c:205:sample_tracked_Release 0000000269D5F820, refcount 2.
sample.c:515:sample_GetSampleTime 0000000269D5F820, 000000006518FD00.
sample.c:576:sample_GetBufferCount 0000000269D5F820, 000000006518FCF0.
sample.c:593:sample_GetBufferByIndex 0000000269D5F820, 0, 000000006518FCA0.
buffer.c:910:dxgi_1d_2d_buffer_QueryInterface 0000000269D5DA30, {e7174cfa-1c9e-48b1-8866-626226bfc258}, 000000006518FC98.
buffer.c:1241:dxgi_buffer_GetResource 0000000269D5DA40, {6f15aaf2-d208-4e89-9ab4-489535d34f9c}, 000000006518FC90.
sample.c:515:sample_GetSampleTime 0000000269D5F820, 000000006518FCA8.
sample.c:153:sample_AddRef 0000000269D5F820, refcount 3.
buffer.c:161:memory_buffer_Release 0000000269D5DA30, refcount 2.
buffer.c:161:memory_buffer_Release 0000000269D5DA30, refcount 1.
sample.c:205:sample_tracked_Release 0000000269D5F820, refcount 2.
sample.c:205:sample_tracked_Release 0000000269D5B640, refcount 1.
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
sample.c:153:sample_AddRef 0000000269D5B7A0, refcount 3.
sample.c:153:sample_AddRef 0000000269D5B7A0, refcount 4.
sample.c:205:sample_tracked_Release 0000000269D5B7A0, refcount 2.
sample.c:515:sample_GetSampleTime 0000000269D5B7A0, 000000006518FD00.
sample.c:576:sample_GetBufferCount 0000000269D5B7A0, 000000006518FCF0.
sample.c:593:sample_GetBufferByIndex 0000000269D5B7A0, 0, 000000006518FCA0.
buffer.c:910:dxgi_1d_2d_buffer_QueryInterface 0000000269D5D1E0, {e7174cfa-1c9e-48b1-8866-626226bfc258}, 000000006518FC98.
buffer.c:1241:dxgi_buffer_GetResource 0000000269D5D1F0, {6f15aaf2-d208-4e89-9ab4-489535d34f9c}, 000000006518FC90.
sample.c:515:sample_GetSampleTime 0000000269D5B7A0, 000000006518FCA8.
sample.c:153:sample_AddRef 0000000269D5B7A0, refcount 3.
buffer.c:161:memory_buffer_Release 0000000269D5D1E0, refcount 2.
buffer.c:161:memory_buffer_Release 0000000269D5D1E0, refcount 1.
sample.c:205:sample_tracked_Release 0000000269D5B7A0, refcount 2.
sample.c:205:sample_tracked_Release 0000000269D5B900, refcount 1.
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
sample.c:153:sample_AddRef 0000000269D5B640, refcount 3.
sample.c:153:sample_AddRef 0000000269D5B640, refcount 3.
sample.c:205:sample_tracked_Release 0000000269D5B640, refcount 2.
sample.c:515:sample_GetSampleTime 0000000269D5B640, 000000006518FD00.
sample.c:576:sample_GetBufferCount 0000000269D5B640, 000000006518FCF0.
sample.c:593:sample_GetBufferByIndex 0000000269D5B640, 0, 000000006518FCA0.
buffer.c:910:dxgi_1d_2d_buffer_QueryInterface 0000000269D5D0B0, {e7174cfa-1c9e-48b1-8866-626226bfc258}, 000000006518FC98.
buffer.c:1241:dxgi_buffer_GetResource 0000000269D5D0C0, {6f15aaf2-d208-4e89-9ab4-489535d34f9c}, 000000006518FC90.
sample.c:515:sample_GetSampleTime 0000000269D5B640, 000000006518FCA8.
sample.c:153:sample_AddRef 0000000269D5B640, refcount 3.
buffer.c:161:memory_buffer_Release 0000000269D5D0B0, refcount 2.
buffer.c:161:memory_buffer_Release 0000000269D5D0B0, refcount 1.
sample.c:205:sample_tracked_Release 0000000269D5B640, refcount 2.
sample.c:205:sample_tracked_Release 0000000269D5BBC0, refcount 1.
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
sample.c:153:sample_AddRef 0000000269D5B900, refcount 3.
sample.c:153:sample_AddRef 0000000269D5B900, refcount 4.
sample.c:205:sample_tracked_Release 0000000269D5B900, refcount 2.
sample.c:515:sample_GetSampleTime 0000000269D5B900, 000000006518FD00.
sample.c:576:sample_GetBufferCount 0000000269D5B900, 000000006518FCF0.
sample.c:593:sample_GetBufferByIndex 0000000269D5B900, 0, 000000006518FCA0.
buffer.c:910:dxgi_1d_2d_buffer_QueryInterface 0000000269D5D310, {e7174cfa-1c9e-48b1-8866-626226bfc258}, 000000006518FC98.
buffer.c:1241:dxgi_buffer_GetResource 0000000269D5D320, {6f15aaf2-d208-4e89-9ab4-489535d34f9c}, 000000006518FC90.
sample.c:515:sample_GetSampleTime 0000000269D5B900, 000000006518FCA8.
sample.c:153:sample_AddRef 0000000269D5B900, refcount 3.
buffer.c:161:memory_buffer_Release 0000000269D5D310, refcount 2.
buffer.c:161:memory_buffer_Release 0000000269D5D310, refcount 1.
sample.c:205:sample_tracked_Release 0000000269D5B900, refcount 2.
sample.c:205:sample_tracked_Release 0000000269D5C090, refcount 1.
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
sample.c:153:sample_AddRef 0000000269D5BBC0, refcount 3.
sample.c:153:sample_AddRef 0000000269D5BBC0, refcount 4.
sample.c:205:sample_tracked_Release 0000000269D5BBC0, refcount 2.
sample.c:515:sample_GetSampleTime 0000000269D5BBC0, 000000006518FD00.
sample.c:576:sample_GetBufferCount 0000000269D5BBC0, 000000006518FCF0.
sample.c:593:sample_GetBufferByIndex 0000000269D5BBC0, 0, 000000006518FCA0.
buffer.c:910:dxgi_1d_2d_buffer_QueryInterface 0000000269D5D440, {e7174cfa-1c9e-48b1-8866-626226bfc258}, 000000006518FC98.
buffer.c:1241:dxgi_buffer_GetResource 0000000269D5D450, {6f15aaf2-d208-4e89-9ab4-489535d34f9c}, 000000006518FC90.
sample.c:515:sample_GetSampleTime 0000000269D5BBC0, 000000006518FCA8.
sample.c:153:sample_AddRef 0000000269D5BBC0, refcount 3.
buffer.c:161:memory_buffer_Release 0000000269D5D440, refcount 2.
buffer.c:161:memory_buffer_Release 0000000269D5D440, refcount 1.
sample.c:205:sample_tracked_Release 0000000269D5BBC0, refcount 2.
sample.c:205:sample_tracked_Release 0000000269D5F610, refcount 1.
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
sample.c:153:sample_AddRef 0000000269D5C090, refcount 3.
sample.c:153:sample_AddRef 0000000269D5C090, refcount 4.
sample.c:205:sample_tracked_Release 0000000269D5C090, refcount 2.
sample.c:515:sample_GetSampleTime 0000000269D5C090, 000000006518FD00.
sample.c:576:sample_GetBufferCount 0000000269D5C090, 000000006518FCF0.
sample.c:593:sample_GetBufferByIndex 0000000269D5C090, 0, 000000006518FCA0.
buffer.c:910:dxgi_1d_2d_buffer_QueryInterface 0000000269D5D7D0, {e7174cfa-1c9e-48b1-8866-626226bfc258}, 000000006518FC98.
buffer.c:1241:dxgi_buffer_GetResource 0000000269D5D7E0, {6f15aaf2-d208-4e89-9ab4-489535d34f9c}, 000000006518FC90.
sample.c:515:sample_GetSampleTime 0000000269D5C090, 000000006518FCA8.
sample.c:153:sample_AddRef 0000000269D5C090, refcount 3.
buffer.c:161:memory_buffer_Release 0000000269D5D7D0, refcount 2.
buffer.c:161:memory_buffer_Release 0000000269D5D7D0, refcount 1.
sample.c:205:sample_tracked_Release 0000000269D5C090, refcount 2.
sample.c:205:sample_tracked_Release 0000000269D5F820, refcount 1.
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
sample.c:153:sample_AddRef 0000000269D5F610, refcount 3.
sample.c:153:sample_AddRef 0000000269D5F610, refcount 4.
sample.c:205:sample_tracked_Release 0000000269D5F610, refcount 2.
sample.c:515:sample_GetSampleTime 0000000269D5F610, 000000006518FD00.
sample.c:576:sample_GetBufferCount 0000000269D5F610, 000000006518FCF0.
sample.c:593:sample_GetBufferByIndex 0000000269D5F610, 0, 000000006518FCA0.
buffer.c:910:dxgi_1d_2d_buffer_QueryInterface 0000000269D5D900, {e7174cfa-1c9e-48b1-8866-626226bfc258}, 000000006518FC98.
buffer.c:1241:dxgi_buffer_GetResource 0000000269D5D910, {6f15aaf2-d208-4e89-9ab4-489535d34f9c}, 000000006518FC90.
sample.c:515:sample_GetSampleTime 0000000269D5F610, 000000006518FCA8.
sample.c:153:sample_AddRef 0000000269D5F610, refcount 3.
buffer.c:161:memory_buffer_Release 0000000269D5D900, refcount 2.
buffer.c:161:memory_buffer_Release 0000000269D5D900, refcount 1.
sample.c:205:sample_tracked_Release 0000000269D5F610, refcount 2.
sample.c:205:sample_tracked_Release 0000000269D5B7A0, refcount 1.
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
sample.c:153:sample_AddRef 0000000269D5F820, refcount 3.
sample.c:153:sample_AddRef 0000000269D5F820, refcount 4.
sample.c:205:sample_tracked_Release 0000000269D5F820, refcount 2.
sample.c:515:sample_GetSampleTime 0000000269D5F820, 000000006518FD00.
sample.c:576:sample_GetBufferCount 0000000269D5F820, 000000006518FCF0.
sample.c:593:sample_GetBufferByIndex 0000000269D5F820, 0, 000000006518FCA0.
buffer.c:910:dxgi_1d_2d_buffer_QueryInterface 0000000269D5DA30, {e7174cfa-1c9e-48b1-8866-626226bfc258}, 000000006518FC98.
buffer.c:1241:dxgi_buffer_GetResource 0000000269D5DA40, {6f15aaf2-d208-4e89-9ab4-489535d34f9c}, 000000006518FC90.
sample.c:515:sample_GetSampleTime 0000000269D5F820, 000000006518FCA8.
sample.c:153:sample_AddRef 0000000269D5F820, refcount 3.
buffer.c:161:memory_buffer_Release 0000000269D5DA30, refcount 2.
buffer.c:161:memory_buffer_Release 0000000269D5DA30, refcount 1.
sample.c:205:sample_tracked_Release 0000000269D5F820, refcount 2.
sample.c:205:sample_tracked_Release 0000000269D5B640, refcount 1.
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
sample.c:153:sample_AddRef 0000000269D5B7A0, refcount 3.
sample.c:153:sample_AddRef 0000000269D5B7A0, refcount 4.
sample.c:205:sample_tracked_Release 0000000269D5B7A0, refcount 2.
sample.c:515:sample_GetSampleTime 0000000269D5B7A0, 000000006518FD00.
sample.c:576:sample_GetBufferCount 0000000269D5B7A0, 000000006518FCF0.
sample.c:593:sample_GetBufferByIndex 0000000269D5B7A0, 0, 000000006518FCA0.
buffer.c:910:dxgi_1d_2d_buffer_QueryInterface 0000000269D5D1E0, {e7174cfa-1c9e-48b1-8866-626226bfc258}, 000000006518FC98.
buffer.c:1241:dxgi_buffer_GetResource 0000000269D5D1F0, {6f15aaf2-d208-4e89-9ab4-489535d34f9c}, 000000006518FC90.
sample.c:515:sample_GetSampleTime 0000000269D5B7A0, 000000006518FCA8.
sample.c:153:sample_AddRef 0000000269D5B7A0, refcount 3.
buffer.c:161:memory_buffer_Release 0000000269D5D1E0, refcount 2.
buffer.c:161:memory_buffer_Release 0000000269D5D1E0, refcount 1.
sample.c:205:sample_tracked_Release 0000000269D5B7A0, refcount 2.
sample.c:205:sample_tracked_Release 0000000269D5B900, refcount 1.
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
sample.c:153:sample_AddRef 0000000269D5B640, refcount 3.
sample.c:153:sample_AddRef 0000000269D5B640, refcount 4.
sample.c:205:sample_tracked_Release 0000000269D5B640, refcount 2.
sample.c:515:sample_GetSampleTime 0000000269D5B640, 000000006518FD00.
sample.c:576:sample_GetBufferCount 0000000269D5B640, 000000006518FCF0.
sample.c:593:sample_GetBufferByIndex 0000000269D5B640, 0, 000000006518FCA0.
buffer.c:910:dxgi_1d_2d_buffer_QueryInterface 0000000269D5D0B0, {e7174cfa-1c9e-48b1-8866-626226bfc258}, 000000006518FC98.
buffer.c:1241:dxgi_buffer_GetResource 0000000269D5D0C0, {6f15aaf2-d208-4e89-9ab4-489535d34f9c}, 000000006518FC90.
sample.c:515:sample_GetSampleTime 0000000269D5B640, 000000006518FCA8.
sample.c:153:sample_AddRef 0000000269D5B640, refcount 3.
buffer.c:161:memory_buffer_Release 0000000269D5D0B0, refcount 2.
buffer.c:161:memory_buffer_Release 0000000269D5D0B0, refcount 1.
sample.c:205:sample_tracked_Release 0000000269D5B640, refcount 2.
sample.c:205:sample_tracked_Release 0000000269D5BBC0, refcount 1.
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
sample.c:153:sample_AddRef 0000000269D5B900, refcount 3.
sample.c:153:sample_AddRef 0000000269D5B900, refcount 4.
sample.c:205:sample_tracked_Release 0000000269D5B900, refcount 2.
sample.c:515:sample_GetSampleTime 0000000269D5B900, 000000006518FD00.
sample.c:576:sample_GetBufferCount 0000000269D5B900, 000000006518FCF0.
sample.c:593:sample_GetBufferByIndex 0000000269D5B900, 0, 000000006518FCA0.
buffer.c:910:dxgi_1d_2d_buffer_QueryInterface 0000000269D5D310, {e7174cfa-1c9e-48b1-8866-626226bfc258}, 000000006518FC98.
buffer.c:1241:dxgi_buffer_GetResource 0000000269D5D320, {6f15aaf2-d208-4e89-9ab4-489535d34f9c}, 000000006518FC90.
sample.c:515:sample_GetSampleTime 0000000269D5B900, 000000006518FCA8.
sample.c:153:sample_AddRef 0000000269D5B900, refcount 3.
buffer.c:161:memory_buffer_Release 0000000269D5D310, refcount 2.
buffer.c:161:memory_buffer_Release 0000000269D5D310, refcount 1.
sample.c:205:sample_tracked_Release 0000000269D5B900, refcount 2.
sample.c:205:sample_tracked_Release 0000000269D5C090, refcount 1.
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
sample.c:153:sample_AddRef 0000000269D5BBC0, refcount 3.
sample.c:153:sample_AddRef 0000000269D5BBC0, refcount 4.
sample.c:205:sample_tracked_Release 0000000269D5BBC0, refcount 2.
sample.c:515:sample_GetSampleTime 0000000269D5BBC0, 000000006518FD00.
sample.c:576:sample_GetBufferCount 0000000269D5BBC0, 000000006518FCF0.
sample.c:593:sample_GetBufferByIndex 0000000269D5BBC0, 0, 000000006518FCA0.
buffer.c:910:dxgi_1d_2d_buffer_QueryInterface 0000000269D5D440, {e7174cfa-1c9e-48b1-8866-626226bfc258}, 000000006518FC98.
buffer.c:1241:dxgi_buffer_GetResource 0000000269D5D450, {6f15aaf2-d208-4e89-9ab4-489535d34f9c}, 000000006518FC90.
sample.c:515:sample_GetSampleTime 0000000269D5BBC0, 000000006518FCA8.
sample.c:153:sample_AddRef 0000000269D5BBC0, refcount 3.
buffer.c:161:memory_buffer_Release 0000000269D5D440, refcount 2.
buffer.c:161:memory_buffer_Release 0000000269D5D440, refcount 1.
sample.c:205:sample_tracked_Release 0000000269D5BBC0, refcount 2.
sample.c:205:sample_tracked_Release 0000000269D5F610, refcount 1.
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
sample.c:153:sample_AddRef 0000000269D5C090, refcount 3.
sample.c:153:sample_AddRef 0000000269D5C090, refcount 4.
sample.c:205:sample_tracked_Release 0000000269D5C090, refcount 2.
sample.c:515:sample_GetSampleTime 0000000269D5C090, 000000006518FD00.
sample.c:576:sample_GetBufferCount 0000000269D5C090, 000000006518FCF0.
sample.c:593:sample_GetBufferByIndex 0000000269D5C090, 0, 000000006518FCA0.
buffer.c:910:dxgi_1d_2d_buffer_QueryInterface 0000000269D5D7D0, {e7174cfa-1c9e-48b1-8866-626226bfc258}, 000000006518FC98.
buffer.c:1241:dxgi_buffer_GetResource 0000000269D5D7E0, {6f15aaf2-d208-4e89-9ab4-489535d34f9c}, 000000006518FC90.
sample.c:515:sample_GetSampleTime 0000000269D5C090, 000000006518FCA8.
sample.c:153:sample_AddRef 0000000269D5C090, refcount 3.
buffer.c:161:memory_buffer_Release 0000000269D5D7D0, refcount 2.
buffer.c:161:memory_buffer_Release 0000000269D5D7D0, refcount 1.
sample.c:205:sample_tracked_Release 0000000269D5C090, refcount 2.
sample.c:205:sample_tracked_Release 0000000269D5F820, refcount 1.
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
sample.c:153:sample_AddRef 0000000269D5F610, refcount 3.
sample.c:153:sample_AddRef 0000000269D5F610, refcount 4.
sample.c:205:sample_tracked_Release 0000000269D5F610, refcount 2.
sample.c:515:sample_GetSampleTime 0000000269D5F610, 000000006518FD00.
sample.c:576:sample_GetBufferCount 0000000269D5F610, 000000006518FCF0.
sample.c:593:sample_GetBufferByIndex 0000000269D5F610, 0, 000000006518FCA0.
buffer.c:910:dxgi_1d_2d_buffer_QueryInterface 0000000269D5D900, {e7174cfa-1c9e-48b1-8866-626226bfc258}, 000000006518FC98.
buffer.c:1241:dxgi_buffer_GetResource 0000000269D5D910, {6f15aaf2-d208-4e89-9ab4-489535d34f9c}, 000000006518FC90.
sample.c:515:sample_GetSampleTime 0000000269D5F610, 000000006518FCA8.
sample.c:153:sample_AddRef 0000000269D5F610, refcount 3.
buffer.c:161:memory_buffer_Release 0000000269D5D900, refcount 2.
buffer.c:161:memory_buffer_Release 0000000269D5D900, refcount 1.
sample.c:205:sample_tracked_Release 0000000269D5F610, refcount 2.
sample.c:205:sample_tracked_Release 0000000269D5B7A0, refcount 1.
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
sample.c:153:sample_AddRef 0000000269D5F820, refcount 3.
sample.c:153:sample_AddRef 0000000269D5F820, refcount 4.
sample.c:205:sample_tracked_Release 0000000269D5F820, refcount 2.
sample.c:515:sample_GetSampleTime 0000000269D5F820, 000000006518FD00.
sample.c:576:sample_GetBufferCount 0000000269D5F820, 000000006518FCF0.
sample.c:593:sample_GetBufferByIndex 0000000269D5F820, 0, 000000006518FCA0.
buffer.c:910:dxgi_1d_2d_buffer_QueryInterface 0000000269D5DA30, {e7174cfa-1c9e-48b1-8866-626226bfc258}, 000000006518FC98.
buffer.c:1241:dxgi_buffer_GetResource 0000000269D5DA40, {6f15aaf2-d208-4e89-9ab4-489535d34f9c}, 000000006518FC90.
sample.c:515:sample_GetSampleTime 0000000269D5F820, 000000006518FCA8.
sample.c:153:sample_AddRef 0000000269D5F820, refcount 3.
buffer.c:161:memory_buffer_Release 0000000269D5DA30, refcount 2.
buffer.c:161:memory_buffer_Release 0000000269D5DA30, refcount 1.
sample.c:205:sample_tracked_Release 0000000269D5F820, refcount 2.
sample.c:205:sample_tracked_Release 0000000269D5B640, refcount 1.
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
sample.c:153:sample_AddRef 0000000269D5B7A0, refcount 3.
sample.c:153:sample_AddRef 0000000269D5B7A0, refcount 4.
sample.c:205:sample_tracked_Release 0000000269D5B7A0, refcount 2.
sample.c:515:sample_GetSampleTime 0000000269D5B7A0, 000000006518FD00.
sample.c:576:sample_GetBufferCount 0000000269D5B7A0, 000000006518FCF0.
sample.c:593:sample_GetBufferByIndex 0000000269D5B7A0, 0, 000000006518FCA0.
buffer.c:910:dxgi_1d_2d_buffer_QueryInterface 0000000269D5D1E0, {e7174cfa-1c9e-48b1-8866-626226bfc258}, 000000006518FC98.
buffer.c:1241:dxgi_buffer_GetResource 0000000269D5D1F0, {6f15aaf2-d208-4e89-9ab4-489535d34f9c}, 000000006518FC90.
sample.c:515:sample_GetSampleTime 0000000269D5B7A0, 000000006518FCA8.
sample.c:153:sample_AddRef 0000000269D5B7A0, refcount 3.
buffer.c:161:memory_buffer_Release 0000000269D5D1E0, refcount 2.
buffer.c:161:memory_buffer_Release 0000000269D5D1E0, refcount 1.
sample.c:205:sample_tracked_Release 0000000269D5B7A0, refcount 2.
sample.c:205:sample_tracked_Release 0000000269D5B900, refcount 1.
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
sample.c:153:sample_AddRef 0000000269D5B640, refcount 3.
sample.c:153:sample_AddRef 0000000269D5B640, refcount 4.
sample.c:205:sample_tracked_Release 0000000269D5B640, refcount 2.
sample.c:515:sample_GetSampleTime 0000000269D5B640, 000000006518FD00.
sample.c:576:sample_GetBufferCount 0000000269D5B640, 000000006518FCF0.
sample.c:593:sample_GetBufferByIndex 0000000269D5B640, 0, 000000006518FCA0.
buffer.c:910:dxgi_1d_2d_buffer_QueryInterface 0000000269D5D0B0, {e7174cfa-1c9e-48b1-8866-626226bfc258}, 000000006518FC98.
buffer.c:1241:dxgi_buffer_GetResource 0000000269D5D0C0, {6f15aaf2-d208-4e89-9ab4-489535d34f9c}, 000000006518FC90.
sample.c:515:sample_GetSampleTime 0000000269D5B640, 000000006518FCA8.
sample.c:153:sample_AddRef 0000000269D5B640, refcount 3.
buffer.c:161:memory_buffer_Release 0000000269D5D0B0, refcount 2.
buffer.c:161:memory_buffer_Release 0000000269D5D0B0, refcount 1.
sample.c:205:sample_tracked_Release 0000000269D5B640, refcount 2.
sample.c:205:sample_tracked_Release 0000000269D5BBC0, refcount 1.
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
sample.c:153:sample_AddRef 0000000269D5B900, refcount 3.
sample.c:153:sample_AddRef 0000000269D5B900, refcount 4.
sample.c:205:sample_tracked_Release 0000000269D5B900, refcount 2.
sample.c:515:sample_GetSampleTime 0000000269D5B900, 000000006518FD00.
sample.c:576:sample_GetBufferCount 0000000269D5B900, 000000006518FCF0.
sample.c:593:sample_GetBufferByIndex 0000000269D5B900, 0, 000000006518FCA0.
buffer.c:910:dxgi_1d_2d_buffer_QueryInterface 0000000269D5D310, {e7174cfa-1c9e-48b1-8866-626226bfc258}, 000000006518FC98.
buffer.c:1241:dxgi_buffer_GetResource 0000000269D5D320, {6f15aaf2-d208-4e89-9ab4-489535d34f9c}, 000000006518FC90.
sample.c:515:sample_GetSampleTime 0000000269D5B900, 000000006518FCA8.
sample.c:153:sample_AddRef 0000000269D5B900, refcount 3.
buffer.c:161:memory_buffer_Release 0000000269D5D310, refcount 2.
buffer.c:161:memory_buffer_Release 0000000269D5D310, refcount 1.
sample.c:205:sample_tracked_Release 0000000269D5B900, refcount 2.
sample.c:205:sample_tracked_Release 0000000269D5C090, refcount 1.
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
sample.c:153:sample_AddRef 0000000269D5BBC0, refcount 3.
sample.c:153:sample_AddRef 0000000269D5BBC0, refcount 4.
sample.c:205:sample_tracked_Release 0000000269D5BBC0, refcount 2.
sample.c:515:sample_GetSampleTime 0000000269D5BBC0, 000000006518FD00.
sample.c:576:sample_GetBufferCount 0000000269D5BBC0, 000000006518FCF0.
sample.c:593:sample_GetBufferByIndex 0000000269D5BBC0, 0, 000000006518FCA0.
buffer.c:910:dxgi_1d_2d_buffer_QueryInterface 0000000269D5D440, {e7174cfa-1c9e-48b1-8866-626226bfc258}, 000000006518FC98.
buffer.c:1241:dxgi_buffer_GetResource 0000000269D5D450, {6f15aaf2-d208-4e89-9ab4-489535d34f9c}, 000000006518FC90.
sample.c:515:sample_GetSampleTime 0000000269D5BBC0, 000000006518FCA8.
sample.c:153:sample_AddRef 0000000269D5BBC0, refcount 3.
buffer.c:161:memory_buffer_Release 0000000269D5D440, refcount 2.
buffer.c:161:memory_buffer_Release 0000000269D5D440, refcount 1.
sample.c:205:sample_tracked_Release 0000000269D5BBC0, refcount 2.
sample.c:205:sample_tracked_Release 0000000269D5F610, refcount 1.
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
sample.c:153:sample_AddRef 0000000269D5C090, refcount 3.
sample.c:153:sample_AddRef 0000000269D5C090, refcount 4.
sample.c:205:sample_tracked_Release 0000000269D5C090, refcount 2.
sample.c:515:sample_GetSampleTime 0000000269D5C090, 000000006518FD00.
sample.c:576:sample_GetBufferCount 0000000269D5C090, 000000006518FCF0.
sample.c:593:sample_GetBufferByIndex 0000000269D5C090, 0, 000000006518FCA0.
buffer.c:910:dxgi_1d_2d_buffer_QueryInterface 0000000269D5D7D0, {e7174cfa-1c9e-48b1-8866-626226bfc258}, 000000006518FC98.
buffer.c:1241:dxgi_buffer_GetResource 0000000269D5D7E0, {6f15aaf2-d208-4e89-9ab4-489535d34f9c}, 000000006518FC90.
sample.c:515:sample_GetSampleTime 0000000269D5C090, 000000006518FCA8.
sample.c:153:sample_AddRef 0000000269D5C090, refcount 3.
buffer.c:161:memory_buffer_Release 0000000269D5D7D0, refcount 2.
buffer.c:161:memory_buffer_Release 0000000269D5D7D0, refcount 1.
sample.c:205:sample_tracked_Release 0000000269D5C090, refcount 2.
sample.c:205:sample_tracked_Release 0000000269D5F820, refcount 1.
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
sample.c:153:sample_AddRef 0000000269D5F610, refcount 3.
sample.c:153:sample_AddRef 0000000269D5F610, refcount 4.
sample.c:205:sample_tracked_Release 0000000269D5F610, refcount 2.
sample.c:515:sample_GetSampleTime 0000000269D5F610, 000000006518FD00.
sample.c:576:sample_GetBufferCount 0000000269D5F610, 000000006518FCF0.
sample.c:593:sample_GetBufferByIndex 0000000269D5F610, 0, 000000006518FCA0.
buffer.c:910:dxgi_1d_2d_buffer_QueryInterface 0000000269D5D900, {e7174cfa-1c9e-48b1-8866-626226bfc258}, 000000006518FC98.
buffer.c:1241:dxgi_buffer_GetResource 0000000269D5D910, {6f15aaf2-d208-4e89-9ab4-489535d34f9c}, 000000006518FC90.
sample.c:515:sample_GetSampleTime 0000000269D5F610, 000000006518FCA8.
sample.c:153:sample_AddRef 0000000269D5F610, refcount 3.
buffer.c:161:memory_buffer_Release 0000000269D5D900, refcount 2.
buffer.c:161:memory_buffer_Release 0000000269D5D900, refcount 1.
sample.c:205:sample_tracked_Release 0000000269D5F610, refcount 2.
sample.c:205:sample_tracked_Release 0000000269D5B7A0, refcount 1.
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
sample.c:153:sample_AddRef 0000000269D5F820, refcount 3.
sample.c:153:sample_AddRef 0000000269D5F820, refcount 4.
sample.c:205:sample_tracked_Release 0000000269D5F820, refcount 2.
sample.c:515:sample_GetSampleTime 0000000269D5F820, 000000006518FD00.
sample.c:576:sample_GetBufferCount 0000000269D5F820, 000000006518FCF0.
sample.c:593:sample_GetBufferByIndex 0000000269D5F820, 0, 000000006518FCA0.
buffer.c:910:dxgi_1d_2d_buffer_QueryInterface 0000000269D5DA30, {e7174cfa-1c9e-48b1-8866-626226bfc258}, 000000006518FC98.
buffer.c:1241:dxgi_buffer_GetResource 0000000269D5DA40, {6f15aaf2-d208-4e89-9ab4-489535d34f9c}, 000000006518FC90.
sample.c:515:sample_GetSampleTime 0000000269D5F820, 000000006518FCA8.
sample.c:153:sample_AddRef 0000000269D5F820, refcount 3.
buffer.c:161:memory_buffer_Release 0000000269D5DA30, refcount 2.
buffer.c:161:memory_buffer_Release 0000000269D5DA30, refcount 1.
sample.c:205:sample_tracked_Release 0000000269D5F820, refcount 2.
sample.c:205:sample_tracked_Release 0000000269D5B640, refcount 1.
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
sample.c:153:sample_AddRef 0000000269D5B7A0, refcount 3.
sample.c:153:sample_AddRef 0000000269D5B7A0, refcount 4.
sample.c:205:sample_tracked_Release 0000000269D5B7A0, refcount 2.
sample.c:515:sample_GetSampleTime 0000000269D5B7A0, 000000006518FD00.
sample.c:576:sample_GetBufferCount 0000000269D5B7A0, 000000006518FCF0.
sample.c:593:sample_GetBufferByIndex 0000000269D5B7A0, 0, 000000006518FCA0.
buffer.c:910:dxgi_1d_2d_buffer_QueryInterface 0000000269D5D1E0, {e7174cfa-1c9e-48b1-8866-626226bfc258}, 000000006518FC98.
buffer.c:1241:dxgi_buffer_GetResource 0000000269D5D1F0, {6f15aaf2-d208-4e89-9ab4-489535d34f9c}, 000000006518FC90.
sample.c:515:sample_GetSampleTime 0000000269D5B7A0, 000000006518FCA8.
sample.c:153:sample_AddRef 0000000269D5B7A0, refcount 3.
buffer.c:161:memory_buffer_Release 0000000269D5D1E0, refcount 2.
buffer.c:161:memory_buffer_Release 0000000269D5D1E0, refcount 1.
sample.c:205:sample_tracked_Release 0000000269D5B7A0, refcount 2.
sample.c:205:sample_tracked_Release 0000000269D5B900, refcount 1.
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
sample.c:153:sample_AddRef 0000000269D5B640, refcount 3.
sample.c:153:sample_AddRef 0000000269D5B640, refcount 4.
sample.c:205:sample_tracked_Release 0000000269D5B640, refcount 2.
sample.c:515:sample_GetSampleTime 0000000269D5B640, 000000006518FD00.
sample.c:576:sample_GetBufferCount 0000000269D5B640, 000000006518FCF0.
sample.c:593:sample_GetBufferByIndex 0000000269D5B640, 0, 000000006518FCA0.
buffer.c:910:dxgi_1d_2d_buffer_QueryInterface 0000000269D5D0B0, {e7174cfa-1c9e-48b1-8866-626226bfc258}, 000000006518FC98.
buffer.c:1241:dxgi_buffer_GetResource 0000000269D5D0C0, {6f15aaf2-d208-4e89-9ab4-489535d34f9c}, 000000006518FC90.
sample.c:515:sample_GetSampleTime 0000000269D5B640, 000000006518FCA8.
sample.c:153:sample_AddRef 0000000269D5B640, refcount 3.
buffer.c:161:memory_buffer_Release 0000000269D5D0B0, refcount 2.
buffer.c:161:memory_buffer_Release 0000000269D5D0B0, refcount 1.
sample.c:205:sample_tracked_Release 0000000269D5B640, refcount 2.
sample.c:205:sample_tracked_Release 0000000269D5BBC0, refcount 1.
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
sample.c:205:sample_tracked_Release 0000000269D5C090, refcount 1.
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
sample.c:205:sample_tracked_Release 0000000269D5F610, refcount 1.
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
sample.c:205:sample_tracked_Release 0000000269D5F820, refcount 1.
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
sample.c:205:sample_tracked_Release 0000000269D5B7A0, refcount 1.
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
sample.c:205:sample_tracked_Release 0000000269D5B640, refcount 1.
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));
reader.c:2463:IMFSourceReader_ReadSample(reader, index 1, flags 0, actual_index 0000000000000000, stream_flags 0000000000000000, timestamp 0000000000000000, sample 0000000000000000));
reader.c:2480:IMFSourceReader_ReadSample(reader, actual_index 0, stream_flags 0, timestamp 0, sample 0000000000000000, ret 0));

*/
}
