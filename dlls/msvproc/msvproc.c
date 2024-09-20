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

#include "d3d9.h"
#include "d3d11.h"
#include "dmoreg.h"
#include "dshow.h"
#include "mfapi.h"
#include "mferror.h"
#include "mfidl.h"
#include "mfobjects.h"
#include "mftransform.h"
#include "rpcproxy.h"
#include "wmcodecdsp.h"

#include "wine/debug.h"
#include "wine/winedmo.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmo);

#include "initguid.h"

DEFINE_MEDIATYPE_GUID(MFVideoFormat_ABGR32, D3DFMT_A8B8G8R8);
DEFINE_MEDIATYPE_GUID(MFVideoFormat_P208,MAKEFOURCC('P','2','0','8'));

static const GUID *const video_processor_inputs[] =
{
    &MFVideoFormat_IYUV,
    &MFVideoFormat_YV12,
    &MFVideoFormat_NV12,
    &MFVideoFormat_420O,
    &MFVideoFormat_UYVY,
    &MFVideoFormat_YUY2,
    &MEDIASUBTYPE_P208,
    &MFVideoFormat_NV11,
    &MFVideoFormat_AYUV,
    &MFVideoFormat_ARGB32,
    &MFVideoFormat_RGB32,
    &MFVideoFormat_RGB24,
    &MFVideoFormat_I420,
    &MFVideoFormat_YVYU,
    &MFVideoFormat_RGB555,
    &MFVideoFormat_RGB565,
    &MFVideoFormat_RGB8,
    &MFVideoFormat_Y216,
    &MFVideoFormat_v410,
    &MFVideoFormat_Y41P,
    &MFVideoFormat_Y41T,
    &MFVideoFormat_Y42T,
};
static const GUID *const video_processor_outputs[] =
{
    &MFVideoFormat_YUY2,
    &MFVideoFormat_IYUV,
    &MFVideoFormat_I420,
    &MFVideoFormat_NV12,
    &MFVideoFormat_RGB24,
    &MFVideoFormat_ARGB32,
    &MFVideoFormat_RGB32,
    &MFVideoFormat_YV12,
    &MFVideoFormat_AYUV,
    &MFVideoFormat_RGB555,
    &MFVideoFormat_RGB565,
    &MFVideoFormat_ABGR32,
};

struct video_processor
{
    IMFTransform IMFTransform_iface;
    LONG refcount;

    IMFAttributes *attributes;
    IMFAttributes *output_attributes;

    IMFMediaType *input_type;
    MFT_INPUT_STREAM_INFO input_info;
    IMFMediaType *output_type;
    MFT_OUTPUT_STREAM_INFO output_info;

    struct winedmo_transform winedmo_transform;
    IMFSample *input_sample;

    IUnknown *device_manager;
    IMFVideoSampleAllocatorEx *allocator;
};

static inline const char *debugstr_area( const MFVideoArea *area )
{
    return wine_dbg_sprintf( "(%d,%d)-(%d,%d)", (int)area->OffsetX.value, (int)area->OffsetY.value,
                             (int)area->Area.cx, (int)area->Area.cy );
}

static MFVideoArea normalize_video_info_area(const MFVideoInfo *info)
{
    MFVideoArea area = info->MinimumDisplayAperture;

    if (!area.OffsetX.value && !area.OffsetY.value && !area.Area.cx && !area.Area.cy)
    {
        area.Area.cx = info->dwWidth;
        area.Area.cy = info->dwHeight;
    }

    return area;
}

static void update_video_aperture(MFVideoInfo *input_info, MFVideoInfo *output_info)
{
    input_info->MinimumDisplayAperture = normalize_video_info_area(input_info);
    output_info->MinimumDisplayAperture = normalize_video_info_area(output_info);

    if (memcmp(&input_info->MinimumDisplayAperture, &output_info->MinimumDisplayAperture, sizeof(MFVideoArea)))
        FIXME("Mismatched content size %s vs %s\n", debugstr_area(&input_info->MinimumDisplayAperture),
              debugstr_area(&output_info->MinimumDisplayAperture));
}

static HRESULT normalize_media_types(BOOL bottom_up, IMFMediaType *input_type, IMFMediaType *output_type,
        MFVIDEOFORMAT **input_format, MFVIDEOFORMAT **output_format)
{
    DWORD format;
    LONG stride;
    UINT32 size;
    HRESULT hr;

    if (FAILED(hr = MFCreateMFVideoFormatFromMFMediaType(input_type, input_format, &size)))
        return hr;
    if (FAILED(hr = MFCreateMFVideoFormatFromMFMediaType(output_type, output_format, &size)))
    {
        CoTaskMemFree(*input_format);
        return hr;
    }

    format = (*input_format)->guidFormat.Data1;
    if (bottom_up && FAILED(IMFMediaType_GetItem(input_type, &MF_MT_DEFAULT_STRIDE, NULL))
            && SUCCEEDED(MFGetStrideForBitmapInfoHeader(format, 1, &stride)) && stride < 0)
        (*input_format)->videoInfo.VideoFlags |= MFVideoFlag_BottomUpLinearRep;

    format = (*output_format)->guidFormat.Data1;
    if (bottom_up && FAILED(IMFMediaType_GetItem(output_type, &MF_MT_DEFAULT_STRIDE, NULL))
            && SUCCEEDED(MFGetStrideForBitmapInfoHeader(format, 1, &stride)) && stride < 0)
        (*output_format)->videoInfo.VideoFlags |= MFVideoFlag_BottomUpLinearRep;

    update_video_aperture(&(*input_format)->videoInfo, &(*output_format)->videoInfo);
    return S_OK;
}

static HRESULT try_create_converter(struct video_processor *processor)
{
    BOOL bottom_up = !processor->device_manager; /* when not D3D-enabled, the transform outputs bottom up RGB buffers */
    union winedmo_format *input_format, *output_format;
    NTSTATUS status;
    HRESULT hr;

    winedmo_transform_destroy(&processor->winedmo_transform);

    if (FAILED(hr = normalize_media_types(bottom_up, processor->input_type, processor->output_type,
            (MFVIDEOFORMAT **)&input_format, (MFVIDEOFORMAT **)&output_format)))
        return hr;
    if ((status = winedmo_transform_create(MFMediaType_Video, input_format, output_format, &processor->winedmo_transform)))
    {
        ERR("FAILED %#lx\n", status);
        hr = HRESULT_FROM_NT(status);
    }
    CoTaskMemFree(output_format);
    CoTaskMemFree(input_format);
    return hr;
}

static HRESULT video_processor_init_allocator(struct video_processor *processor)
{
    IMFVideoSampleAllocatorEx *allocator;
    UINT32 count;
    HRESULT hr;

    if (processor->allocator)
        return S_OK;

    if (FAILED(hr = MFCreateVideoSampleAllocatorEx(&IID_IMFVideoSampleAllocatorEx, (void **)&allocator)))
        return hr;
    if (FAILED(IMFAttributes_GetUINT32(processor->attributes, &MF_SA_MINIMUM_OUTPUT_SAMPLE_COUNT, &count)))
        count = 2;
    if (FAILED(hr = IMFVideoSampleAllocatorEx_SetDirectXManager(allocator, processor->device_manager))
            || FAILED(hr = IMFVideoSampleAllocatorEx_InitializeSampleAllocatorEx(allocator, count, max(count + 2, 10),
            processor->output_attributes, processor->output_type)))
    {
        IMFVideoSampleAllocatorEx_Release(allocator);
        return hr;
    }

    processor->allocator = allocator;
    return S_OK;
}

static HRESULT video_processor_uninit_allocator(struct video_processor *processor)
{
    HRESULT hr;

    if (!processor->allocator)
        return S_OK;

    if (SUCCEEDED(hr = IMFVideoSampleAllocatorEx_UninitializeSampleAllocator(processor->allocator)))
        hr = IMFVideoSampleAllocatorEx_SetDirectXManager(processor->allocator, NULL);
    IMFVideoSampleAllocatorEx_Release(processor->allocator);
    processor->allocator = NULL;

    return hr;
}

static struct video_processor *video_processor_from_IMFTransform(IMFTransform *iface)
{
    return CONTAINING_RECORD(iface, struct video_processor, IMFTransform_iface);
}

static HRESULT WINAPI video_processor_QueryInterface(IMFTransform *iface, REFIID iid, void **out)
{
    struct video_processor *processor = video_processor_from_IMFTransform(iface);

    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IMFTransform))
        *out = &processor->IMFTransform_iface;
    else
    {
        *out = NULL;
        WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static ULONG WINAPI video_processor_AddRef(IMFTransform *iface)
{
    struct video_processor *processor = video_processor_from_IMFTransform(iface);
    ULONG refcount = InterlockedIncrement(&processor->refcount);

    TRACE("iface %p increasing refcount to %lu.\n", iface, refcount);

    return refcount;
}

static ULONG WINAPI video_processor_Release(IMFTransform *iface)
{
    struct video_processor *processor = video_processor_from_IMFTransform(iface);
    ULONG refcount = InterlockedDecrement(&processor->refcount);

    TRACE("iface %p decreasing refcount to %lu.\n", iface, refcount);

    if (!refcount)
    {
        video_processor_uninit_allocator(processor);
        if (processor->device_manager)
            IUnknown_Release(processor->device_manager);
        winedmo_transform_destroy(&processor->winedmo_transform);
        if (processor->input_type)
            IMFMediaType_Release(processor->input_type);
        if (processor->output_type)
            IMFMediaType_Release(processor->output_type);
        if (processor->attributes)
            IMFAttributes_Release(processor->attributes);
        if (processor->output_attributes)
            IMFAttributes_Release(processor->output_attributes);

        free(processor);
    }

    return refcount;
}

static HRESULT WINAPI video_processor_GetStreamLimits(IMFTransform *iface, DWORD *input_minimum,
        DWORD *input_maximum, DWORD *output_minimum, DWORD *output_maximum)
{
    TRACE("iface %p, input_minimum %p, input_maximum %p, output_minimum %p, output_maximum %p.\n",
            iface, input_minimum, input_maximum, output_minimum, output_maximum);
    *input_minimum = *input_maximum = *output_minimum = *output_maximum = 1;
    return S_OK;
}

static HRESULT WINAPI video_processor_GetStreamCount(IMFTransform *iface, DWORD *inputs, DWORD *outputs)
{
    TRACE("iface %p, inputs %p, outputs %p.\n", iface, inputs, outputs);
    *inputs = *outputs = 1;
    return S_OK;
}

static HRESULT WINAPI video_processor_GetStreamIDs(IMFTransform *iface, DWORD input_size, DWORD *inputs,
        DWORD output_size, DWORD *outputs)
{
    TRACE("iface %p, input_size %lu, inputs %p, output_size %lu, outputs %p.\n", iface,
            input_size, inputs, output_size, outputs);
    return E_NOTIMPL;
}

static HRESULT WINAPI video_processor_GetInputStreamInfo(IMFTransform *iface, DWORD id, MFT_INPUT_STREAM_INFO *info)
{
    struct video_processor *processor = video_processor_from_IMFTransform(iface);

    TRACE("iface %p, id %#lx, info %p.\n", iface, id, info);

    if (id)
        return MF_E_INVALIDSTREAMNUMBER;

    *info = processor->input_info;
    return S_OK;
}

static HRESULT WINAPI video_processor_GetOutputStreamInfo(IMFTransform *iface, DWORD id, MFT_OUTPUT_STREAM_INFO *info)
{
    struct video_processor *processor = video_processor_from_IMFTransform(iface);

    TRACE("iface %p, id %#lx, info %p.\n", iface, id, info);

    if (id)
        return MF_E_INVALIDSTREAMNUMBER;

    *info = processor->output_info;
    return S_OK;
}

static HRESULT WINAPI video_processor_GetAttributes(IMFTransform *iface, IMFAttributes **attributes)
{
    struct video_processor *processor = video_processor_from_IMFTransform(iface);

    TRACE("iface %p, attributes %p\n", iface, attributes);

    if (!attributes)
        return E_POINTER;

    IMFAttributes_AddRef((*attributes = processor->attributes));
    return S_OK;
}

static HRESULT WINAPI video_processor_GetInputStreamAttributes(IMFTransform *iface, DWORD id, IMFAttributes **attributes)
{
    TRACE("iface %p, id %#lx, attributes %p.\n", iface, id, attributes);
    return E_NOTIMPL;
}

static HRESULT WINAPI video_processor_GetOutputStreamAttributes(IMFTransform *iface, DWORD id, IMFAttributes **attributes)
{
    struct video_processor *processor = video_processor_from_IMFTransform(iface);

    TRACE("iface %p, id %#lx, attributes %p\n", iface, id, attributes);

    if (!attributes)
        return E_POINTER;
    if (id)
        return MF_E_INVALIDSTREAMNUMBER;

    IMFAttributes_AddRef((*attributes = processor->output_attributes));
    return S_OK;
}

static HRESULT WINAPI video_processor_DeleteInputStream(IMFTransform *iface, DWORD id)
{
    TRACE("iface %p, id %#lx.\n", iface, id);
    return E_NOTIMPL;
}

static HRESULT WINAPI video_processor_AddInputStreams(IMFTransform *iface, DWORD streams, DWORD *ids)
{
    TRACE("iface %p, streams %lu, ids %p.\n", iface, streams, ids);
    return E_NOTIMPL;
}

static HRESULT WINAPI video_processor_GetInputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    IMFMediaType *media_type;
    const GUID *subtype;
    HRESULT hr;

    TRACE("iface %p, id %#lx, index %#lx, type %p.\n", iface, id, index, type);

    *type = NULL;

    if (index >= ARRAY_SIZE(video_processor_inputs))
        return MF_E_NO_MORE_TYPES;
    subtype = video_processor_inputs[index];

    if (FAILED(hr = MFCreateMediaType(&media_type)))
        return hr;

    if (FAILED(hr = IMFMediaType_SetGUID(media_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video)))
        goto done;
    if (FAILED(hr = IMFMediaType_SetGUID(media_type, &MF_MT_SUBTYPE, subtype)))
        goto done;

    IMFMediaType_AddRef((*type = media_type));

done:
    IMFMediaType_Release(media_type);
    return hr;
}

static HRESULT WINAPI video_processor_GetOutputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    struct video_processor *processor = video_processor_from_IMFTransform(iface);
    IMFMediaType *media_type;
    UINT64 frame_size;
    GUID subtype;
    HRESULT hr;

    TRACE("iface %p, id %#lx, index %#lx, type %p.\n", iface, id, index, type);

    *type = NULL;

    if (!processor->input_type)
        return MF_E_NO_MORE_TYPES;

    if (FAILED(hr = IMFMediaType_GetGUID(processor->input_type, &MF_MT_SUBTYPE, &subtype))
            || FAILED(hr = IMFMediaType_GetUINT64(processor->input_type, &MF_MT_FRAME_SIZE, &frame_size)))
        return hr;

    if (index > ARRAY_SIZE(video_processor_outputs))
        return MF_E_NO_MORE_TYPES;
    if (index > 0)
        subtype = *video_processor_outputs[index - 1];

    if (FAILED(hr = MFCreateMediaType(&media_type)))
        return hr;

    if (FAILED(hr = IMFMediaType_SetGUID(media_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video)))
        goto done;
    if (FAILED(hr = IMFMediaType_SetGUID(media_type, &MF_MT_SUBTYPE, &subtype)))
        goto done;
    if (FAILED(hr = IMFMediaType_SetUINT64(media_type, &MF_MT_FRAME_SIZE, frame_size)))
        goto done;

    IMFMediaType_AddRef((*type = media_type));

done:
    IMFMediaType_Release(media_type);
    return hr;
}

static HRESULT WINAPI video_processor_SetInputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    struct video_processor *processor = video_processor_from_IMFTransform(iface);
    GUID major, subtype;
    UINT64 frame_size;
    HRESULT hr;
    ULONG i;

    TRACE("iface %p, id %#lx, type %p, flags %#lx.\n", iface, id, type, flags);

    if (!type)
    {
        if (processor->input_type)
        {
            IMFMediaType_Release(processor->input_type);
            processor->input_type = NULL;
        }
        winedmo_transform_destroy(&processor->winedmo_transform);

        return S_OK;
    }

    if (FAILED(IMFMediaType_GetGUID(type, &MF_MT_MAJOR_TYPE, &major))
            || !IsEqualGUID(&major, &MFMediaType_Video))
        return E_INVALIDARG;
    if (FAILED(IMFMediaType_GetGUID(type, &MF_MT_SUBTYPE, &subtype)))
        return MF_E_INVALIDMEDIATYPE;
    if (FAILED(hr = IMFMediaType_GetUINT64(type, &MF_MT_FRAME_SIZE, &frame_size)))
        return hr;

    for (i = 0; i < ARRAY_SIZE(video_processor_inputs); ++i)
        if (IsEqualGUID(&subtype, video_processor_inputs[i]))
            break;
    if (i == ARRAY_SIZE(video_processor_inputs))
        return MF_E_INVALIDMEDIATYPE;
    if (flags & MFT_SET_TYPE_TEST_ONLY)
        return S_OK;

    if (processor->input_type)
        IMFMediaType_Release(processor->input_type);
    IMFMediaType_AddRef((processor->input_type = type));

    if (processor->output_type && FAILED(hr = try_create_converter(processor)))
    {
        IMFMediaType_Release(processor->input_type);
        processor->input_type = NULL;
    }

    if (FAILED(hr) || FAILED(MFCalculateImageSize(&subtype, frame_size >> 32, (UINT32)frame_size,
            (UINT32 *)&processor->input_info.cbSize)))
        processor->input_info.cbSize = 0;

    return hr;
}

static HRESULT WINAPI video_processor_SetOutputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    struct video_processor *processor = video_processor_from_IMFTransform(iface);
    GUID major, subtype;
    UINT64 frame_size;
    HRESULT hr;
    ULONG i;

    TRACE("iface %p, id %#lx, type %p, flags %#lx.\n", iface, id, type, flags);

    if (!type)
    {
        if (processor->output_type)
        {
            IMFMediaType_Release(processor->output_type);
            processor->output_type = NULL;
        }
        winedmo_transform_destroy(&processor->winedmo_transform);

        return S_OK;
    }

    if (FAILED(IMFMediaType_GetGUID(type, &MF_MT_MAJOR_TYPE, &major))
            || !IsEqualGUID(&major, &MFMediaType_Video))
        return E_INVALIDARG;
    if (FAILED(IMFMediaType_GetGUID(type, &MF_MT_SUBTYPE, &subtype)))
        return MF_E_INVALIDMEDIATYPE;
    if (FAILED(hr = IMFMediaType_GetUINT64(type, &MF_MT_FRAME_SIZE, &frame_size)))
        return hr;

    for (i = 0; i < ARRAY_SIZE(video_processor_outputs); ++i)
        if (IsEqualGUID(&subtype, video_processor_outputs[i]))
            break;
    if (i == ARRAY_SIZE(video_processor_outputs))
        return MF_E_INVALIDMEDIATYPE;
    if (flags & MFT_SET_TYPE_TEST_ONLY)
        return S_OK;

    if (FAILED(hr = video_processor_uninit_allocator(processor)))
        return hr;

    if (processor->output_type)
        IMFMediaType_Release(processor->output_type);
    IMFMediaType_AddRef((processor->output_type = type));

    if (processor->input_type && FAILED(hr = try_create_converter(processor)))
    {
        IMFMediaType_Release(processor->output_type);
        processor->output_type = NULL;
    }

    if (FAILED(hr) || FAILED(MFCalculateImageSize(&subtype, frame_size >> 32, (UINT32)frame_size,
            (UINT32 *)&processor->output_info.cbSize)))
        processor->output_info.cbSize = 0;

    return hr;
}

static HRESULT WINAPI video_processor_GetInputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **type)
{
    struct video_processor *processor = video_processor_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, id %#lx, type %p.\n", iface, id, type);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (!processor->input_type)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (FAILED(hr = MFCreateMediaType(type)))
        return hr;

    if (FAILED(hr = IMFMediaType_CopyAllItems(processor->input_type, (IMFAttributes *)*type)))
        IMFMediaType_Release(*type);

    return hr;
}

static HRESULT WINAPI video_processor_GetOutputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **type)
{
    struct video_processor *processor = video_processor_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, id %#lx, type %p.\n", iface, id, type);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (!processor->output_type)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (FAILED(hr = MFCreateMediaType(type)))
        return hr;

    if (FAILED(hr = IMFMediaType_CopyAllItems(processor->output_type, (IMFAttributes *)*type)))
        IMFMediaType_Release(*type);

    return hr;
}

static HRESULT WINAPI video_processor_GetInputStatus(IMFTransform *iface, DWORD id, DWORD *flags)
{
    struct video_processor *processor = video_processor_from_IMFTransform(iface);

    FIXME("iface %p, id %#lx, flags %p stub!\n", iface, id, flags);

    if (!processor->input_type)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    *flags = MFT_INPUT_STATUS_ACCEPT_DATA;
    return S_OK;
}

static HRESULT WINAPI video_processor_GetOutputStatus(IMFTransform *iface, DWORD *flags)
{
    struct video_processor *processor = video_processor_from_IMFTransform(iface);

    FIXME("iface %p, flags %p stub!\n", iface, flags);

    if (!processor->output_type)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    return E_NOTIMPL;
}

static HRESULT WINAPI video_processor_SetOutputBounds(IMFTransform *iface, LONGLONG lower, LONGLONG upper)
{
    TRACE("iface %p, lower %I64d, upper %I64d.\n", iface, lower, upper);
    return E_NOTIMPL;
}

static HRESULT WINAPI video_processor_ProcessEvent(IMFTransform *iface, DWORD id, IMFMediaEvent *event)
{
    FIXME("iface %p, id %#lx, event %p stub!\n", iface, id, event);
    return E_NOTIMPL;
}

static HRESULT WINAPI video_processor_ProcessMessage(IMFTransform *iface, MFT_MESSAGE_TYPE message, ULONG_PTR param)
{
    struct video_processor *processor = video_processor_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, message %#x, param %Ix.\n", iface, message, param);

    switch (message)
    {
    case MFT_MESSAGE_SET_D3D_MANAGER:
        if (FAILED(hr = video_processor_uninit_allocator(processor)))
            return hr;

        if (processor->device_manager)
        {
            processor->output_info.dwFlags &= ~MFT_OUTPUT_STREAM_PROVIDES_SAMPLES;
            IUnknown_Release(processor->device_manager);
        }
        if ((processor->device_manager = (IUnknown *)param))
        {
            IUnknown_AddRef(processor->device_manager);
            processor->output_info.dwFlags |= MFT_OUTPUT_STREAM_PROVIDES_SAMPLES;
        }
        return S_OK;

    default:
        FIXME("Ignoring message %#x.\n", message);
        return S_OK;
    }
}

static HRESULT WINAPI video_processor_ProcessInput(IMFTransform *iface, DWORD id, IMFSample *sample, DWORD flags)
{
    struct video_processor *processor = video_processor_from_IMFTransform(iface);
    DMO_OUTPUT_DATA_BUFFER input = {0};
    IMFMediaBuffer *buffer;
    NTSTATUS status;
    HRESULT hr;

    TRACE("iface %p, id %#lx, sample %p, flags %#lx.\n", iface, id, sample, flags);

    if (!processor->winedmo_transform.handle)
        return MF_E_TRANSFORM_TYPE_NOT_SET;
    if (processor->input_sample)
        return MF_E_NOTACCEPTING;

    if (FAILED(hr = IMFSample_ConvertToContiguousBuffer(sample, &buffer)))
        return hr;
    if (SUCCEEDED(hr = MFCreateLegacyMediaBufferOnMFMediaBuffer(sample, buffer, 0, &input.pBuffer)))
    {
        if ((status = winedmo_transform_process_input(processor->winedmo_transform, &input)))
        {
            if (status == STATUS_PENDING) hr = MF_E_NOTACCEPTING;
            else hr = HRESULT_FROM_NT(status);
        }
        IMediaBuffer_Release(input.pBuffer);
    }
    IMFMediaBuffer_Release(buffer);

    if (SUCCEEDED(hr))
        IMFSample_AddRef((processor->input_sample = sample));
    return S_OK;
}

static HRESULT WINAPI video_processor_ProcessOutput(IMFTransform *iface, DWORD flags, DWORD count,
        MFT_OUTPUT_DATA_BUFFER *samples, DWORD *samples_status)
{
    struct video_processor *processor = video_processor_from_IMFTransform(iface);
    IMFSample *output_sample, *input_sample;
    DMO_OUTPUT_DATA_BUFFER output = {0};
    MFT_OUTPUT_STREAM_INFO info;
    IMFMediaBuffer *buffer;
    NTSTATUS status;
    HRESULT hr;

    TRACE("iface %p, flags %#lx, count %lu, samples %p, samples_status %p.\n", iface, flags, count, samples, samples_status);

    if (count != 1)
        return E_INVALIDARG;

    if (!processor->winedmo_transform.handle)
        return MF_E_TRANSFORM_TYPE_NOT_SET;
    if (!(input_sample = processor->input_sample))
        return MF_E_TRANSFORM_NEED_MORE_INPUT;
    processor->input_sample = NULL;

    samples->dwStatus = 0;
    if (FAILED(hr = IMFTransform_GetOutputStreamInfo(iface, 0, &info)))
    {
        IMFSample_Release(input_sample);
        return hr;
    }

    if (processor->output_info.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES)
    {
        if (FAILED(hr = video_processor_init_allocator(processor))
                || FAILED(hr = IMFVideoSampleAllocatorEx_AllocateSample(processor->allocator, &output_sample)))
        {
            IMFSample_Release(input_sample);
            return hr;
        }
    }
    else
    {
        if (!(output_sample = samples->pSample))
        {
            IMFSample_Release(input_sample);
            return E_INVALIDARG;
        }
        IMFSample_AddRef(output_sample);
    }

    if (FAILED(hr = IMFSample_ConvertToContiguousBuffer(output_sample, &buffer)))
        goto done;
    if (SUCCEEDED(hr = MFCreateLegacyMediaBufferOnMFMediaBuffer(output_sample, buffer, 0, &output.pBuffer)))
    {
        if ((status = winedmo_transform_process_output(processor->winedmo_transform, &output)))
        {
            if (status == STATUS_PENDING) hr = MF_E_TRANSFORM_NEED_MORE_INPUT;
            else hr = HRESULT_FROM_NT(status);
        }
        IMediaBuffer_Release(output.pBuffer);
    }
    IMFMediaBuffer_Release(buffer);

    if (SUCCEEDED(hr) && processor->output_info.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES)
    {
        samples->pSample = output_sample;
        IMFSample_AddRef(output_sample);
    }

done:
    IMFSample_Release(output_sample);
    IMFSample_Release(input_sample);
    return hr;
}

static const IMFTransformVtbl video_processor_vtbl =
{
    video_processor_QueryInterface,
    video_processor_AddRef,
    video_processor_Release,
    video_processor_GetStreamLimits,
    video_processor_GetStreamCount,
    video_processor_GetStreamIDs,
    video_processor_GetInputStreamInfo,
    video_processor_GetOutputStreamInfo,
    video_processor_GetAttributes,
    video_processor_GetInputStreamAttributes,
    video_processor_GetOutputStreamAttributes,
    video_processor_DeleteInputStream,
    video_processor_AddInputStreams,
    video_processor_GetInputAvailableType,
    video_processor_GetOutputAvailableType,
    video_processor_SetInputType,
    video_processor_SetOutputType,
    video_processor_GetInputCurrentType,
    video_processor_GetOutputCurrentType,
    video_processor_GetInputStatus,
    video_processor_GetOutputStatus,
    video_processor_SetOutputBounds,
    video_processor_ProcessEvent,
    video_processor_ProcessMessage,
    video_processor_ProcessInput,
    video_processor_ProcessOutput,
};

static HRESULT WINAPI video_processor_factory_CreateInstance(IClassFactory *iface, IUnknown *outer,
        REFIID riid, void **out)
{
    struct video_processor *processor;
    NTSTATUS status;
    HRESULT hr;

    TRACE("%p, %s, %p.\n", outer, debugstr_guid(riid), out);

    if ((status = winedmo_transform_check(MFMediaType_Video, MFVideoFormat_NV12, MFVideoFormat_RGB32)))
    {
        static const GUID CLSID_wg_video_processor = {0xd527607f,0x89cb,0x4e94,{0x95,0x71,0xbc,0xfe,0x62,0x17,0x56,0x13}};
        WARN("Unsupported winedmo transform, status %#lx.\n", status);
        return CoCreateInstance(&CLSID_wg_video_processor, outer, CLSCTX_INPROC_SERVER, riid, out);
    }

    *out = NULL;
    if (outer)
        return CLASS_E_NOAGGREGATION;
    if (!(processor = calloc(1, sizeof(*processor))))
        return E_OUTOFMEMORY;

    if (FAILED(hr = MFCreateAttributes(&processor->attributes, 0)))
        goto failed;
    if (FAILED(hr = IMFAttributes_SetUINT32(processor->attributes, &MF_SA_D3D11_AWARE, TRUE)))
        goto failed;
    /* native only has MF_SA_D3D_AWARE on Win7, but it is useful to have in mfreadwrite */
    if (FAILED(hr = IMFAttributes_SetUINT32(processor->attributes, &MF_SA_D3D_AWARE, TRUE)))
        goto failed;
    if (FAILED(hr = MFCreateAttributes(&processor->output_attributes, 0)))
        goto failed;

    processor->IMFTransform_iface.lpVtbl = &video_processor_vtbl;
    processor->refcount = 1;
    TRACE("created %p\n", processor);

    hr = IMFTransform_QueryInterface(&processor->IMFTransform_iface, riid, out);
    IMFTransform_Release(&processor->IMFTransform_iface);
    return hr;

failed:
    if (processor->output_attributes)
        IMFAttributes_Release(processor->output_attributes);
    if (processor->attributes)
        IMFAttributes_Release(processor->attributes);
    free(processor);
    return hr;
}

static HRESULT WINAPI class_factory_QueryInterface(IClassFactory *iface, REFIID riid, void **out)
{
    *out = IsEqualGUID(riid, &IID_IClassFactory) || IsEqualGUID(riid, &IID_IUnknown) ? iface : NULL;
    return *out ? S_OK : E_NOINTERFACE;
}
static ULONG WINAPI class_factory_AddRef(IClassFactory *iface)
{
    return 2;
}
static ULONG WINAPI class_factory_Release(IClassFactory *iface)
{
    return 1;
}
static HRESULT WINAPI class_factory_LockServer(IClassFactory *iface, BOOL dolock)
{
    return S_OK;
}

static const IClassFactoryVtbl video_processor_factory_vtbl =
{
    class_factory_QueryInterface,
    class_factory_AddRef,
    class_factory_Release,
    video_processor_factory_CreateInstance,
    class_factory_LockServer,
};

static IClassFactory video_processor_factory = {&video_processor_factory_vtbl};

/***********************************************************************
 *              DllGetClassObject (msvproc.@)
 */
HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void **out)
{
    if (IsEqualGUID(clsid, &CLSID_VideoProcessorMFT))
        return IClassFactory_QueryInterface(&video_processor_factory, riid, out);

    *out = NULL;
    FIXME("Unknown clsid %s.\n", debugstr_guid(clsid));
    return CLASS_E_CLASSNOTAVAILABLE;
}

/***********************************************************************
 *              DllRegisterServer (msvproc.@)
 */
HRESULT WINAPI DllRegisterServer(void)
{
    MFT_REGISTER_TYPE_INFO video_processor_mft_inputs[] =
    {
        {MFMediaType_Video, MFVideoFormat_IYUV},
        {MFMediaType_Video, MFVideoFormat_YV12},
        {MFMediaType_Video, MFVideoFormat_NV12},
        {MFMediaType_Video, MFVideoFormat_YUY2},
        {MFMediaType_Video, MFVideoFormat_ARGB32},
        {MFMediaType_Video, MFVideoFormat_RGB32},
        {MFMediaType_Video, MFVideoFormat_NV11},
        {MFMediaType_Video, MFVideoFormat_AYUV},
        {MFMediaType_Video, MFVideoFormat_UYVY},
        {MFMediaType_Video, MFVideoFormat_P208},
        {MFMediaType_Video, MFVideoFormat_RGB24},
        {MFMediaType_Video, MFVideoFormat_RGB555},
        {MFMediaType_Video, MFVideoFormat_RGB565},
        {MFMediaType_Video, MFVideoFormat_RGB8},
        {MFMediaType_Video, MFVideoFormat_I420},
        {MFMediaType_Video, MFVideoFormat_Y216},
        {MFMediaType_Video, MFVideoFormat_v410},
        {MFMediaType_Video, MFVideoFormat_Y41P},
        {MFMediaType_Video, MFVideoFormat_Y41T},
        {MFMediaType_Video, MFVideoFormat_Y42T},
        {MFMediaType_Video, MFVideoFormat_YVYU},
        {MFMediaType_Video, MFVideoFormat_420O},
    };
    MFT_REGISTER_TYPE_INFO video_processor_mft_outputs[] =
    {
        {MFMediaType_Video, MFVideoFormat_IYUV},
        {MFMediaType_Video, MFVideoFormat_YV12},
        {MFMediaType_Video, MFVideoFormat_NV12},
        {MFMediaType_Video, MFVideoFormat_YUY2},
        {MFMediaType_Video, MFVideoFormat_ARGB32},
        {MFMediaType_Video, MFVideoFormat_RGB32},
        {MFMediaType_Video, MFVideoFormat_NV11},
        {MFMediaType_Video, MFVideoFormat_AYUV},
        {MFMediaType_Video, MFVideoFormat_UYVY},
        {MFMediaType_Video, MFVideoFormat_P208},
        {MFMediaType_Video, MFVideoFormat_RGB24},
        {MFMediaType_Video, MFVideoFormat_RGB555},
        {MFMediaType_Video, MFVideoFormat_RGB565},
        {MFMediaType_Video, MFVideoFormat_RGB8},
        {MFMediaType_Video, MFVideoFormat_I420},
        {MFMediaType_Video, MFVideoFormat_Y216},
        {MFMediaType_Video, MFVideoFormat_v410},
        {MFMediaType_Video, MFVideoFormat_Y41P},
        {MFMediaType_Video, MFVideoFormat_Y41T},
        {MFMediaType_Video, MFVideoFormat_Y42T},
        {MFMediaType_Video, MFVideoFormat_YVYU},
    };

    HRESULT hr;

    TRACE("\n");

    if (FAILED(hr = __wine_register_resources()))
        return hr;
    if (FAILED(hr = MFTRegister(CLSID_VideoProcessorMFT, MFT_CATEGORY_VIDEO_PROCESSOR,
            (WCHAR *)L"Microsoft Video Processor MFT", MFT_ENUM_FLAG_SYNCMFT,
            ARRAY_SIZE(video_processor_mft_inputs), video_processor_mft_inputs,
            ARRAY_SIZE(video_processor_mft_outputs), video_processor_mft_outputs, NULL)))
        return hr;

    return S_OK;
}

/***********************************************************************
 *              DllUnregisterServer (msvproc.@)
 */
HRESULT WINAPI DllUnregisterServer(void)
{
    HRESULT hr;

    TRACE("\n");

    if (FAILED(hr = __wine_unregister_resources()))
        return hr;
    if (FAILED(hr = MFTUnregister(CLSID_VideoProcessorMFT)))
        return hr;

    return S_OK;
}
