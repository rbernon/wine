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

static HRESULT (WINAPI *pMFCreateMFByteStreamOnStream)(IStream *stream, IMFByteStream **bytestream);

static void init_functions(void)
{
    HMODULE mod = GetModuleHandleA("mfplat.dll");

#define X(f) if (!(p##f = (void*)GetProcAddress(mod, #f))) return;
    X(MFCreateMFByteStreamOnStream);
#undef X
}

struct async_callback
{
    IMFSourceReaderCallback IMFSourceReaderCallback_iface;
    IMFSourceReader *reader;
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

static HRESULT WINAPI async_callback_OnReadSample(IMFSourceReaderCallback *iface, HRESULT hr, DWORD index,
        DWORD flags, LONGLONG timestamp, IMFSample *sample)
{
    struct async_callback *callback = impl_from_IMFSourceReaderCallback(iface);
LONGLONG duration = 0, time = 0;
DWORD length = 0;

if (sample)
{
hr = IMFSample_GetTotalLength(sample, &length);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFSample_GetSampleTime(sample, &time);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFSample_GetSampleDuration(sample, &duration);
ok(hr == S_OK, "got hr %#lx\n", hr);
}
ok(0, "got index %lu, flags %#lx, time %I64d, sample %p (len %lu time %I64d duration %I64d)\n", index, flags, timestamp, sample, length, time, duration);

hr = IMFSourceReader_ReadSample(callback->reader, index, 0, NULL, NULL, NULL, NULL);
ok(hr == S_OK, "got hr %#lx\n", hr);
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

static struct async_callback *create_async_callback(void)
{
    struct async_callback *callback;

    callback = calloc(1, sizeof(*callback));
    callback->IMFSourceReaderCallback_iface.lpVtbl = &async_callback_vtbl;
    callback->refcount = 1;

    return callback;
}

START_TEST(games)
{
    IMFMediaType *native_type, *media_type;
    IMFSourceResolver *resolver;
    IMFAttributes *attributes;
    IMFSourceReader *reader;
    IMFMediaSource *source;
    IMFSeekInfo *seek_info;
    MF_OBJECT_TYPE type;
    PROPVARIANT propvar;
    LONGLONG timestamp, time, duration;
    IMFSample *sample;
    DWORD flags, index, length;
    HRESULT hr;
    struct async_callback *callback = create_async_callback();

const WCHAR *path = L"file://Z:/tmp/stream-0000000001754B70.mp4";
if (!winetest_platform_is_wine) path = L"file://Y:/Games/Little Witch Nobeta/LittleWitchNobeta_Data/StreamingAssets/Video/Comic_Funny_01.mp4";

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
hr = MFCreateSourceResolver(&resolver);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFSourceResolver_CreateObjectFromURL(resolver, path, 0x1, 0, &type, (IUnknown **)&source);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = MFCreateSourceReaderFromMediaSource(source, attributes, &reader);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = MFGetService((IUnknown *)reader, &MF_SCRUBBING_SERVICE, &IID_IMFSeekInfo, (void **)&seek_info);
ok(hr == E_NOINTERFACE, "got hr %#lx\n", hr);
IMFMediaSource_Release(source);
IMFSourceResolver_Release(resolver);

if (0)
{
hr = IMFSourceReader_GetPresentationAttribute(reader, MF_SOURCE_READER_MEDIASOURCE, &MF_PD_MIME_TYPE, &propvar);
ok(hr == S_OK, "got hr %#lx\n", hr);
PropVariantClear(&propvar);


hr = IMFSourceReader_GetNativeMediaType(reader, 0, 0, &native_type);
ok(hr == S_OK, "got hr %#lx\n", hr);
if (0) dump_media_type(native_type);
IMFMediaType_Release(native_type);
/*
games.c:189: Test failed: ATTR_BLOB(MF_MT_MPEG4_SAMPLE_DESCRIPTION, {0x00,0x00,0x00,0x5b,0x73,0x74,0x73,0x64,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,...}, 91),
games.c:189: Test failed: ATTR_BLOB(MF_MT_USER_DATA, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x11,0x90}, 14),
games.c:189: Test failed: ATTR_GUID(MF_MT_AM_FORMAT_TYPE, {05589f81-c356-11ce-bf01-00aa0055595a}),
games.c:189: Test failed: ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
games.c:189: Test failed: ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_AAC),
games.c:189: Test failed: ATTR_UINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 0),
games.c:189: Test failed: ATTR_UINT32(MF_MT_AAC_PAYLOAD_TYPE, 0),
games.c:189: Test failed: ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
games.c:189: Test failed: ATTR_UINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 39654),
games.c:189: Test failed: ATTR_UINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16),
games.c:189: Test failed: ATTR_UINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 1),
games.c:189: Test failed: ATTR_UINT32(MF_MT_AUDIO_NUM_CHANNELS, 2),
games.c:189: Test failed: ATTR_UINT32(MF_MT_AUDIO_PREFER_WAVEFORMATEX, 1),
games.c:189: Test failed: ATTR_UINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000),
games.c:189: Test failed: ATTR_UINT32(MF_MT_AVG_BITRATE, 317232),
games.c:189: Test failed: ATTR_UINT32(MF_MT_FIXED_SIZE_SAMPLES, 1),
games.c:189: Test failed: ATTR_UINT32(MF_MT_MPEG4_CURRENT_SAMPLE_ENTRY, 0),
games.c:189: Test failed: ATTR_UINT32(MF_MT_SAMPLE_SIZE, 1),
*/

/*
games.c:189: Test failed: ATTR_BLOB(MF_MT_USER_DATA, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x11,0x90}, 14),
games.c:189: Test failed: ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
games.c:189: Test failed: ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_AAC),
games.c:189: Test failed: ATTR_UINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 0),
games.c:189: Test failed: ATTR_UINT32(MF_MT_AAC_PAYLOAD_TYPE, 0),
games.c:189: Test failed: ATTR_UINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 39654),
games.c:189: Test failed: ATTR_UINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16),
games.c:189: Test failed: ATTR_UINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 4),
games.c:189: Test failed: ATTR_UINT32(MF_MT_AUDIO_NUM_CHANNELS, 2),
games.c:189: Test failed: ATTR_UINT32(MF_MT_AUDIO_PREFER_WAVEFORMATEX, 1),
games.c:189: Test failed: ATTR_UINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000),
*/
hr = IMFSourceReader_GetNativeMediaType(reader, 0, 1, &native_type);
ok(hr == MF_E_NO_MORE_TYPES, "got hr %#lx\n", hr);

hr = IMFSourceReader_GetNativeMediaType(reader, 1, 0, &native_type);
ok(hr == S_OK, "got hr %#lx\n", hr);
if (0) dump_media_type(native_type);
IMFMediaType_Release(native_type);
/*
games.c:196: Test failed: ATTR_BLOB(MF_MT_MPEG4_SAMPLE_DESCRIPTION, {0x00,0x00,0x00,0x92,0x73,0x74,0x73,0x64,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,...}, 146),
games.c:196: Test failed: ATTR_BLOB(MF_MT_MPEG_SEQUENCE_HEADER, {0x00,0x00,0x01,0x67,0x42,0xc0,0x2a,0x95,0x90,0x07,0x80,0x22,0x7e,0x5c,0x04,0x40,...}, 31),
games.c:196: Test failed: ATTR_GUID(MF_MT_AM_FORMAT_TYPE, {e06d80e3-db46-11cf-b4d1-00805f6cbbea}),
games.c:196: Test failed: ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Video),
games.c:196: Test failed: ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_H264),
games.c:196: Test failed: ATTR_RATIO(MF_MT_FRAME_RATE, 30, 1),
games.c:196: Test failed: ATTR_RATIO(MF_MT_FRAME_SIZE, 1920, 1080),
games.c:196: Test failed: ATTR_RATIO(MF_MT_PIXEL_ASPECT_RATIO, 1, 1),
games.c:196: Test failed: ATTR_UINT32(MF_MT_AVG_BITRATE, 5352500),
games.c:196: Test failed: ATTR_UINT32(MF_MT_INTERLACE_MODE, 7),
games.c:196: Test failed: ATTR_UINT32(MF_MT_MPEG4_CURRENT_SAMPLE_ENTRY, 0),
games.c:196: Test failed: ATTR_UINT32(MF_MT_SAMPLE_SIZE, 1),
games.c:196: Test failed: ATTR_UINT32(MF_MT_VIDEO_LEVEL, 42),
games.c:196: Test failed: ATTR_UINT32(MF_MT_VIDEO_PROFILE, 66),
games.c:196: Test failed: ATTR_UINT32(MF_MT_VIDEO_ROTATION, 0),
games.c:196: Test failed: ATTR_UINT32({8f020eea-1508-471f-9da6-507d7cfa40db}, 1),
games.c:196: Test failed: ATTR_UINT32({a7911d53-12a4-4965-ae70-6eadd6ff0551}, 1),
*/

/*
games.c:216: Test failed: ATTR_BLOB(MF_MT_USER_DATA, {0x01,0x42,0xc0,0x2a,0xff,0xe1,0x00,0x15,0x67,0x42,0xc0,0x2a,0x95,0x90,0x07,0x80,...}, 36),
games.c:216: Test failed: ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Video),
games.c:216: Test failed: ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_H264),
games.c:216: Test failed: ATTR_RATIO(MF_MT_FRAME_RATE, 30, 1),
games.c:216: Test failed: ATTR_RATIO(MF_MT_FRAME_SIZE, 1920, 1080),
games.c:216: Test failed: ATTR_RATIO(MF_MT_PIXEL_ASPECT_RATIO, 1, 1),
games.c:216: Test failed: ATTR_UINT32(MF_MT_VIDEO_ROTATION, 0),
*/
hr = IMFSourceReader_GetNativeMediaType(reader, 1, 1, &native_type);
ok(hr == MF_E_NO_MORE_TYPES, "got hr %#lx\n", hr);


hr = MFCreateMediaType(&media_type);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFMediaType_SetGUID(media_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFMediaType_SetGUID(media_type, &MF_MT_SUBTYPE, &MFVideoFormat_NV12);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFMediaType_SetUINT32(media_type, &MF_MT_VIDEO_ROTATION, 0);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFSourceReader_SetCurrentMediaType(reader, 0x1, 0, media_type);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFSourceReader_SetStreamSelection(reader, 1, 1);
ok(hr == S_OK, "got hr %#lx\n", hr);
IMFMediaType_Release(media_type);


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


hr = IMFSourceReader_GetCurrentMediaType(reader, 0, &media_type);
ok(hr == S_OK, "got hr %#lx\n", hr);
if (0) dump_media_type(media_type);
/*
games.c:232: Test failed: ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
games.c:232: Test failed: ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_Float),
games.c:232: Test failed: ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
games.c:232: Test failed: ATTR_UINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 384000),
games.c:232: Test failed: ATTR_UINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 32),
games.c:232: Test failed: ATTR_UINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 8),
games.c:232: Test failed: ATTR_UINT32(MF_MT_AUDIO_NUM_CHANNELS, 2),
games.c:232: Test failed: ATTR_UINT32(MF_MT_AUDIO_PREFER_WAVEFORMATEX, 1),
games.c:232: Test failed: ATTR_UINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000),
*/

/*
games.c:271: Test failed: ATTR_BLOB(MF_MT_USER_DATA, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,...}, 22),
games.c:271: Test failed: ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
games.c:271: Test failed: ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_Float),
games.c:271: Test failed: ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
games.c:271: Test failed: ATTR_UINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 384000),
games.c:271: Test failed: ATTR_UINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 32),
games.c:271: Test failed: ATTR_UINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 8),
games.c:271: Test failed: ATTR_UINT32(MF_MT_AUDIO_NUM_CHANNELS, 2),
games.c:271: Test failed: ATTR_UINT32(MF_MT_AUDIO_PREFER_WAVEFORMATEX, 1),
games.c:271: Test failed: ATTR_UINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000),
games.c:271: Test failed: ATTR_UINT32(MF_MT_FIXED_SIZE_SAMPLES, 1),
*/
IMFMediaType_Release(media_type);

hr = IMFSourceReader_GetCurrentMediaType(reader, 1, &media_type);
ok(hr == S_OK, "got hr %#lx\n", hr);
if (0) dump_media_type(media_type);
/*
games.c:237: Test failed: ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Video),
games.c:237: Test failed: ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_NV12),
games.c:237: Test failed: ATTR_RATIO(MF_MT_FRAME_RATE, 30, 1),
games.c:237: Test failed: ATTR_RATIO(MF_MT_FRAME_SIZE, 1920, 1080),
games.c:237: Test failed: ATTR_RATIO(MF_MT_PIXEL_ASPECT_RATIO, 1, 1),
games.c:237: Test failed: ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
games.c:237: Test failed: ATTR_UINT32(MF_MT_AVG_BIT_ERROR_RATE, 0),
games.c:237: Test failed: ATTR_UINT32(MF_MT_AVG_BITRATE, 5352500),
games.c:237: Test failed: ATTR_UINT32(MF_MT_COMPRESSED, 0),
games.c:237: Test failed: ATTR_UINT32(MF_MT_DEFAULT_STRIDE, 1920),
games.c:237: Test failed: ATTR_UINT32(MF_MT_FIXED_SIZE_SAMPLES, 1),
games.c:237: Test failed: ATTR_UINT32(MF_MT_INTERLACE_MODE, 7),
games.c:237: Test failed: ATTR_UINT32(MF_MT_SAMPLE_SIZE, 3110400),
games.c:237: Test failed: ATTR_UINT32(MF_MT_VIDEO_ROTATION, 0),
*/

/*
games.c:287: Test failed: ATTR_BLOB(MF_MT_GEOMETRIC_APERTURE, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x07,0x00,0x00,0x38,0x04,0x00,0x00}, 16),
games.c:287: Test failed: ATTR_BLOB(MF_MT_MINIMUM_DISPLAY_APERTURE, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x07,0x00,0x00,0x38,0x04,0x00,0x00}, 16),
games.c:287: Test failed: ATTR_BLOB(MF_MT_PAN_SCAN_APERTURE, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x07,0x00,0x00,0x38,0x04,0x00,0x00}, 16),
games.c:287: Test failed: ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Video),
games.c:287: Test failed: ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_NV12),
games.c:287: Test failed: ATTR_RATIO(MF_MT_FRAME_RATE, 30, 1),
games.c:287: Test failed: ATTR_RATIO(MF_MT_FRAME_SIZE, 1920, 1080),
games.c:287: Test failed: ATTR_RATIO(MF_MT_PIXEL_ASPECT_RATIO, 1, 1),
games.c:287: Test failed: ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
games.c:287: Test failed: ATTR_UINT32(MF_MT_DEFAULT_STRIDE, 1920),
games.c:287: Test failed: ATTR_UINT32(MF_MT_FIXED_SIZE_SAMPLES, 1),
games.c:287: Test failed: ATTR_UINT32(MF_MT_INTERLACE_MODE, 7),
games.c:287: Test failed: ATTR_UINT32(MF_MT_SAMPLE_SIZE, 3110400),
games.c:287: Test failed: ATTR_UINT32(MF_MT_VIDEO_NOMINAL_RANGE, 2),
games.c:287: Test failed: ATTR_UINT32(MF_MT_VIDEO_ROTATION, 0),
*/
IMFMediaType_Release(media_type);
}


callback->reader = reader;

for (UINT i = 0; i < 100000; i++)
{
hr = IMFSourceReader_ReadSample(reader, MF_SOURCE_READER_ANY_STREAM, 0, &index, &flags, &timestamp, &sample);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFSample_GetTotalLength(sample, &length);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFSample_GetSampleTime(sample, &time);
ok(hr == S_OK, "got hr %#lx\n", hr);
hr = IMFSample_GetSampleDuration(sample, &duration);
ok(hr == S_OK, "got hr %#lx\n", hr);
ok(0, "got index %lu, flags %#lx, time %I64d, sample %p (len %lu time %I64d duration %I64d)\n", index, flags, timestamp, sample, length, time, duration);
IMFSample_Release(sample);
}


    hr = MFShutdown();
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
}
