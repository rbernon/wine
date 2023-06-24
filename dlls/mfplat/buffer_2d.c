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

#define COBJMACROS

#include <malloc.h>

#include "mfplat_private.h"
#include "rtworkq.h"

#include "d3d11.h"
#include "d3d9.h"
#include "evr.h"

WINE_DEFAULT_DEBUG_CHANNEL(mfplat);

#define ALIGN_SIZE(size, alignment) (((size) + (alignment)) & ~((alignment)))

struct buffer_2d
{
    IMFMediaBuffer IMFMediaBuffer_iface;
    IMF2DBuffer2 IMF2DBuffer2_iface;
    IMFGetService IMFGetService_iface;
    LONG refcount;

    BYTE *data;
    DWORD max_length;
    DWORD current_length;
    DWORD plane_size;

    DWORD fourcc;
    UINT width;
    UINT height;

    UINT locks;

    BYTE *linear_buffer;
    BYTE *scanline0;
    LONG pitch;

    CRITICAL_SECTION cs;
};

void buffer_2d_copy_image(DWORD fourcc, BYTE *dest, LONG dest_stride,
        const BYTE *src, LONG src_stride, DWORD width, DWORD lines)
{
    MFCopyImage(dest, dest_stride, src, src_stride, width, lines);
    dest += dest_stride * lines;
    src += src_stride * lines;

    switch (fourcc)
    {
    case MAKEFOURCC('I','M','C','1'):
    case MAKEFOURCC('I','M','C','3'):
        MFCopyImage(dest, dest_stride, src, src_stride, width / 2, lines);
        break;
    case MAKEFOURCC('I','M','C','2'):
    case MAKEFOURCC('I','M','C','4'):
    case MAKEFOURCC('N','V','1','1'):
    case MAKEFOURCC('Y','V','1','2'):
    case MAKEFOURCC('I','4','2','0'):
    case MAKEFOURCC('I','Y','U','V'):
        MFCopyImage(dest, dest_stride / 2, src, src_stride / 2, width / 2, lines);
        break;
    case MAKEFOURCC('N','V','1','2'):
        MFCopyImage(dest, dest_stride, src, src_stride, width, lines / 2);
        break;
    default:
        break;
    }
}

static HRESULT buffer_2d_lock(struct buffer_2d *buffer, BYTE **scanline0, LONG *pitch,
        BYTE **buffer_start, DWORD *buffer_length)
{
    HRESULT hr = S_OK;

    if (buffer->linear_buffer)
        hr = MF_E_UNEXPECTED;
    else
    {
        ++buffer->locks;
        *scanline0 = buffer->scanline0;
        *pitch = buffer->pitch;
        if (buffer_start)
            *buffer_start = buffer->data;
        if (buffer_length)
            *buffer_length = buffer->max_length;
    }

    return hr;
}

static inline struct buffer_2d *impl_from_IMFMediaBuffer(IMFMediaBuffer *iface)
{
    return CONTAINING_RECORD(iface, struct buffer_2d, IMFMediaBuffer_iface);
}

static HRESULT WINAPI media_buffer_QueryInterface(IMFMediaBuffer *iface, REFIID riid, void **out)
{
    struct buffer_2d *buffer = impl_from_IMFMediaBuffer(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), out);

    if (IsEqualIID(riid, &IID_IMFMediaBuffer) ||
            IsEqualIID(riid, &IID_IUnknown))
    {
        *out = &buffer->IMFMediaBuffer_iface;
    }
    else if (IsEqualIID(riid, &IID_IMF2DBuffer2) ||
            IsEqualIID(riid, &IID_IMF2DBuffer))
    {
        *out = &buffer->IMF2DBuffer2_iface;
    }
    else if (IsEqualIID(riid, &IID_IMFGetService))
    {
        *out = &buffer->IMFGetService_iface;
    }
    else
    {
        WARN("Unsupported interface %s.\n", debugstr_guid(riid));
        *out = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*out);
    return S_OK;
}

static ULONG WINAPI media_buffer_AddRef(IMFMediaBuffer *iface)
{
    struct buffer_2d *buffer = impl_from_IMFMediaBuffer(iface);
    ULONG refcount = InterlockedIncrement(&buffer->refcount);

    TRACE("%p, refcount %lu.\n", buffer, refcount);

    return refcount;
}

static ULONG WINAPI media_buffer_Release(IMFMediaBuffer *iface)
{
    struct buffer_2d *buffer = impl_from_IMFMediaBuffer(iface);
    ULONG refcount = InterlockedDecrement(&buffer->refcount);

    TRACE("%p, refcount %lu.\n", iface, refcount);

    if (!refcount)
    {
        DeleteCriticalSection(&buffer->cs);
        free(buffer->linear_buffer);
        _aligned_free(buffer->data);
        free(buffer);
    }

    return refcount;
}

static HRESULT WINAPI media_buffer_Lock(IMFMediaBuffer *iface, BYTE **data, DWORD *max_length, DWORD *current_length)
{
    struct buffer_2d *buffer = impl_from_IMFMediaBuffer(iface);
    HRESULT hr = S_OK;

    TRACE("%p, %p, %p, %p.\n", iface, data, max_length, current_length);

    if (!data)
        return E_POINTER;

    /* Allocate linear buffer and return it as a copy of current content. Maximum and current length are
       unrelated to 2D buffer maximum allocate length, or maintained current length. */

    EnterCriticalSection(&buffer->cs);

    if (!buffer->linear_buffer && buffer->locks)
        hr = MF_E_INVALIDREQUEST;
    else if (!buffer->linear_buffer)
    {
        if (!(buffer->linear_buffer = malloc(buffer->plane_size)))
            hr = E_OUTOFMEMORY;

        if (SUCCEEDED(hr))
            buffer_2d_copy_image(buffer->fourcc, buffer->linear_buffer, buffer->width, buffer->data, buffer->pitch,
                    buffer->width, buffer->height);
    }

    if (SUCCEEDED(hr))
    {
        ++buffer->locks;
        *data = buffer->linear_buffer;
        if (max_length)
            *max_length = buffer->plane_size;
        if (current_length)
            *current_length = buffer->plane_size;
    }

    LeaveCriticalSection(&buffer->cs);

    return hr;
}

static HRESULT WINAPI media_buffer_Unlock(IMFMediaBuffer *iface)
{
    struct buffer_2d *buffer = impl_from_IMFMediaBuffer(iface);

    TRACE("%p.\n", iface);

    EnterCriticalSection(&buffer->cs);

    if (buffer->linear_buffer && !--buffer->locks)
    {
        buffer_2d_copy_image(buffer->fourcc, buffer->data, buffer->pitch, buffer->linear_buffer, buffer->width,
                buffer->width, buffer->height);

        free(buffer->linear_buffer);
        buffer->linear_buffer = NULL;
    }

    LeaveCriticalSection(&buffer->cs);

    return S_OK;
}

static HRESULT WINAPI media_buffer_GetCurrentLength(IMFMediaBuffer *iface, DWORD *current_length)
{
    struct buffer_2d *buffer = impl_from_IMFMediaBuffer(iface);

    TRACE("%p.\n", iface);

    if (!current_length)
        return E_INVALIDARG;

    *current_length = buffer->current_length;

    return S_OK;
}

static HRESULT WINAPI media_buffer_SetCurrentLength(IMFMediaBuffer *iface, DWORD current_length)
{
    struct buffer_2d *buffer = impl_from_IMFMediaBuffer(iface);

    TRACE("%p, %lu.\n", iface, current_length);

    if (current_length > buffer->max_length)
        return E_INVALIDARG;

    buffer->current_length = current_length;

    return S_OK;
}

static HRESULT WINAPI media_buffer_GetMaxLength(IMFMediaBuffer *iface, DWORD *max_length)
{
    struct buffer_2d *buffer = impl_from_IMFMediaBuffer(iface);

    TRACE("%p, %p.\n", iface, max_length);

    if (!max_length)
        return E_INVALIDARG;

    *max_length = buffer->max_length;

    return S_OK;
}

static const IMFMediaBufferVtbl media_buffer_vtbl =
{
    media_buffer_QueryInterface,
    media_buffer_AddRef,
    media_buffer_Release,
    media_buffer_Lock,
    media_buffer_Unlock,
    media_buffer_GetCurrentLength,
    media_buffer_SetCurrentLength,
    media_buffer_GetMaxLength,
};

static struct buffer_2d *impl_from_IMF2DBuffer2(IMF2DBuffer2 *iface)
{
    return CONTAINING_RECORD(iface, struct buffer_2d, IMF2DBuffer2_iface);
}

static HRESULT WINAPI buffer_2d_QueryInterface(IMF2DBuffer2 *iface, REFIID riid, void **obj)
{
    struct buffer_2d *buffer = impl_from_IMF2DBuffer2(iface);
    return IMFMediaBuffer_QueryInterface(&buffer->IMFMediaBuffer_iface, riid, obj);
}

static ULONG WINAPI buffer_2d_AddRef(IMF2DBuffer2 *iface)
{
    struct buffer_2d *buffer = impl_from_IMF2DBuffer2(iface);
    return IMFMediaBuffer_AddRef(&buffer->IMFMediaBuffer_iface);
}

static ULONG WINAPI buffer_2d_Release(IMF2DBuffer2 *iface)
{
    struct buffer_2d *buffer = impl_from_IMF2DBuffer2(iface);
    return IMFMediaBuffer_Release(&buffer->IMFMediaBuffer_iface);
}

static HRESULT WINAPI buffer_2d_Lock2D(IMF2DBuffer2 *iface, BYTE **scanline0, LONG *pitch)
{
    struct buffer_2d *buffer = impl_from_IMF2DBuffer2(iface);
    HRESULT hr;

    TRACE("%p, %p, %p.\n", iface, scanline0, pitch);

    if (!scanline0 || !pitch)
        return E_POINTER;

    EnterCriticalSection(&buffer->cs);

    hr = buffer_2d_lock(buffer, scanline0, pitch, NULL, NULL);

    LeaveCriticalSection(&buffer->cs);

    return hr;
}

static HRESULT WINAPI buffer_2d_Unlock2D(IMF2DBuffer2 *iface)
{
    struct buffer_2d *buffer = impl_from_IMF2DBuffer2(iface);
    HRESULT hr = S_OK;

    TRACE("%p.\n", iface);

    EnterCriticalSection(&buffer->cs);

    if (!buffer->linear_buffer)
    {
        if (buffer->locks)
            --buffer->locks;
        else
            hr = HRESULT_FROM_WIN32(ERROR_WAS_UNLOCKED);
    }

    LeaveCriticalSection(&buffer->cs);

    return hr;
}

static HRESULT WINAPI buffer_2d_GetScanline0AndPitch(IMF2DBuffer2 *iface, BYTE **scanline0, LONG *pitch)
{
    struct buffer_2d *buffer = impl_from_IMF2DBuffer2(iface);
    HRESULT hr = S_OK;

    TRACE("%p, %p, %p.\n", iface, scanline0, pitch);

    if (!scanline0 || !pitch)
        return E_POINTER;

    EnterCriticalSection(&buffer->cs);

    if (buffer->linear_buffer || !buffer->locks)
        hr = HRESULT_FROM_WIN32(ERROR_WAS_UNLOCKED);
    else
    {
        *scanline0 = buffer->scanline0;
        *pitch = buffer->pitch;
    }

    LeaveCriticalSection(&buffer->cs);

    return hr;
}

static HRESULT WINAPI buffer_2d_IsContiguousFormat(IMF2DBuffer2 *iface, BOOL *is_contiguous)
{
    TRACE("%p, %p.\n", iface, is_contiguous);

    if (!is_contiguous)
        return E_POINTER;

    *is_contiguous = FALSE;

    return S_OK;
}

static HRESULT WINAPI buffer_2d_GetContiguousLength(IMF2DBuffer2 *iface, DWORD *length)
{
    struct buffer_2d *buffer = impl_from_IMF2DBuffer2(iface);

    TRACE("%p, %p.\n", iface, length);

    if (!length)
        return E_POINTER;

    *length = buffer->plane_size;

    return S_OK;
}

static HRESULT WINAPI buffer_2d_ContiguousCopyTo(IMF2DBuffer2 *iface, BYTE *dest_buffer, DWORD dest_length)
{
    struct buffer_2d *buffer = impl_from_IMF2DBuffer2(iface);
    BYTE *src_scanline0, *src_buffer_start;
    DWORD src_length;
    LONG src_pitch;
    HRESULT hr;

    TRACE("%p, %p, %lu.\n", iface, dest_buffer, dest_length);

    if (dest_length < buffer->plane_size)
        return E_INVALIDARG;

    hr = IMF2DBuffer2_Lock2DSize(iface, MF2DBuffer_LockFlags_Read, &src_scanline0, &src_pitch, &src_buffer_start, &src_length);

    if (SUCCEEDED(hr))
    {
        buffer_2d_copy_image(buffer->fourcc, dest_buffer, buffer->width, src_scanline0, src_pitch, buffer->width, buffer->height);

        if (FAILED(IMF2DBuffer2_Unlock2D(iface)))
            WARN("Couldn't unlock source buffer %p, hr %#lx.\n", iface, hr);
    }

    return S_OK;
}

static HRESULT WINAPI buffer_2d_ContiguousCopyFrom(IMF2DBuffer2 *iface, const BYTE *src_buffer, DWORD src_length)
{
    struct buffer_2d *buffer = impl_from_IMF2DBuffer2(iface);
    BYTE *dst_scanline0, *dst_buffer_start;
    DWORD dst_length;
    LONG dst_pitch;
    HRESULT hr;

    TRACE("%p, %p, %lu.\n", iface, src_buffer, src_length);

    if (src_length < buffer->plane_size)
        return E_INVALIDARG;

    hr = IMF2DBuffer2_Lock2DSize(iface, MF2DBuffer_LockFlags_Write, &dst_scanline0, &dst_pitch, &dst_buffer_start, &dst_length);

    if (SUCCEEDED(hr))
    {
        buffer_2d_copy_image(buffer->fourcc, dst_scanline0, dst_pitch, src_buffer, buffer->width, buffer->width, buffer->height);

        if (FAILED(IMF2DBuffer2_Unlock2D(iface)))
            WARN("Couldn't unlock destination buffer %p, hr %#lx.\n", iface, hr);
    }

    return hr;
}

static HRESULT WINAPI buffer_2d_Lock2DSize(IMF2DBuffer2 *iface, MF2DBuffer_LockFlags flags, BYTE **scanline0,
        LONG *pitch, BYTE **buffer_start, DWORD *buffer_length)
{
    struct buffer_2d *buffer = impl_from_IMF2DBuffer2(iface);
    HRESULT hr;

    TRACE("%p, %#x, %p, %p, %p, %p.\n", iface, flags, scanline0, pitch, buffer_start, buffer_length);

    if (!scanline0 || !pitch || !buffer_start || !buffer_length)
        return E_POINTER;

    EnterCriticalSection(&buffer->cs);

    hr = buffer_2d_lock(buffer, scanline0, pitch, buffer_start, buffer_length);

    LeaveCriticalSection(&buffer->cs);

    return hr;
}

static HRESULT WINAPI buffer_2d_Copy2DTo(IMF2DBuffer2 *iface, IMF2DBuffer2 *dest_buffer)
{
    FIXME("%p, %p.\n", iface, dest_buffer);

    return E_NOTIMPL;
}

static const IMF2DBuffer2Vtbl buffer_2d_vtbl =
{
    buffer_2d_QueryInterface,
    buffer_2d_AddRef,
    buffer_2d_Release,
    buffer_2d_Lock2D,
    buffer_2d_Unlock2D,
    buffer_2d_GetScanline0AndPitch,
    buffer_2d_IsContiguousFormat,
    buffer_2d_GetContiguousLength,
    buffer_2d_ContiguousCopyTo,
    buffer_2d_ContiguousCopyFrom,
    buffer_2d_Lock2DSize,
    buffer_2d_Copy2DTo,
};

static struct buffer_2d *impl_from_IMFGetService(IMFGetService *iface)
{
    return CONTAINING_RECORD(iface, struct buffer_2d, IMFGetService_iface);
}

static HRESULT WINAPI get_service_QueryInterface(IMFGetService *iface, REFIID riid, void **obj)
{
    struct buffer_2d *buffer = impl_from_IMFGetService(iface);
    return IMFMediaBuffer_QueryInterface(&buffer->IMFMediaBuffer_iface, riid, obj);
}

static ULONG WINAPI get_service_AddRef(IMFGetService *iface)
{
    struct buffer_2d *buffer = impl_from_IMFGetService(iface);
    return IMFMediaBuffer_AddRef(&buffer->IMFMediaBuffer_iface);
}

static ULONG WINAPI get_service_Release(IMFGetService *iface)
{
    struct buffer_2d *buffer = impl_from_IMFGetService(iface);
    return IMFMediaBuffer_Release(&buffer->IMFMediaBuffer_iface);
}

static HRESULT WINAPI get_service_GetService(IMFGetService *iface, REFGUID service, REFIID riid, void **obj)
{
    TRACE("%p, %s, %s, %p.\n", iface, debugstr_guid(service), debugstr_guid(riid), obj);

    return E_NOTIMPL;
}

static const IMFGetServiceVtbl get_service_vtbl =
{
    get_service_QueryInterface,
    get_service_AddRef,
    get_service_Release,
    get_service_GetService,
};

static HRESULT media_buffer_init(struct buffer_2d *buffer, DWORD max_length, DWORD alignment,
        const IMFMediaBufferVtbl *vtbl)
{
    if (alignment < MF_16_BYTE_ALIGNMENT)
        alignment = MF_16_BYTE_ALIGNMENT;
    alignment++;

    if (alignment & (alignment - 1))
    {
        alignment--;
        alignment |= alignment >> 1;
        alignment |= alignment >> 2;
        alignment |= alignment >> 4;
        alignment |= alignment >> 8;
        alignment |= alignment >> 16;
        alignment++;
    }

    if (!(buffer->data = _aligned_malloc(max_length, alignment)))
        return E_OUTOFMEMORY;
    memset(buffer->data, 0, max_length);

    buffer->IMFMediaBuffer_iface.lpVtbl = vtbl;
    buffer->refcount = 1;
    buffer->max_length = max_length;
    buffer->current_length = 0;
    InitializeCriticalSection(&buffer->cs);

    return S_OK;
}

static HRESULT create_2d_buffer(DWORD width, DWORD height, DWORD fourcc, BOOL bottom_up, IMFMediaBuffer **buffer)
{
    UINT stride, max_length;
    UINT row_alignment;
    struct buffer_2d *object;
    DWORD plane_size;
    GUID subtype;
    BOOL is_yuv;
    HRESULT hr;
    int pitch;

    if (!buffer)
        return E_POINTER;

    *buffer = NULL;

    memcpy(&subtype, &MFVideoFormat_Base, sizeof(subtype));
    subtype.Data1 = fourcc;

    if (!(stride = mf_format_get_stride(&subtype, width, &is_yuv)))
        return MF_E_INVALIDMEDIATYPE;

    if (is_yuv && bottom_up)
        return MF_E_INVALIDMEDIATYPE;

    switch (fourcc)
    {
        case MAKEFOURCC('I','M','C','1'):
        case MAKEFOURCC('I','M','C','3'):
            plane_size = stride * height * 2;
            break;
        case MAKEFOURCC('I','M','C','2'):
        case MAKEFOURCC('I','M','C','4'):
        case MAKEFOURCC('N','V','1','1'):
        case MAKEFOURCC('Y','V','1','2'):
        case MAKEFOURCC('I','4','2','0'):
        case MAKEFOURCC('I','Y','U','V'):
            plane_size = stride * 3 / 2 * height;
            break;
        case MAKEFOURCC('N','V','1','2'):
            plane_size = stride * height * 3 / 2;
            break;
        default:
            plane_size = stride * height;
    }

    if (!(object = calloc(1, sizeof(*object))))
        return E_OUTOFMEMORY;

    switch (fourcc)
    {
        case MAKEFOURCC('I','M','C','1'):
        case MAKEFOURCC('I','M','C','2'):
        case MAKEFOURCC('I','M','C','3'):
        case MAKEFOURCC('I','M','C','4'):
        case MAKEFOURCC('Y','V','1','2'):
        case MAKEFOURCC('N','V','1','1'):
        case MAKEFOURCC('I','4','2','0'):
        case MAKEFOURCC('I','Y','U','V'):
            row_alignment = MF_128_BYTE_ALIGNMENT;
            break;
        default:
            row_alignment = MF_64_BYTE_ALIGNMENT;
    }

    pitch = ALIGN_SIZE(stride, row_alignment);

    switch (fourcc)
    {
        case MAKEFOURCC('I','M','C','1'):
        case MAKEFOURCC('I','M','C','3'):
            max_length = pitch * height * 2;
            break;
        case MAKEFOURCC('N','V','1','2'):
        case MAKEFOURCC('Y','V','1','2'):
        case MAKEFOURCC('I','M','C','2'):
        case MAKEFOURCC('I','M','C','4'):
        case MAKEFOURCC('N','V','1','1'):
        case MAKEFOURCC('I','4','2','0'):
        case MAKEFOURCC('I','Y','U','V'):
            max_length = pitch * height * 3 / 2;
            break;
        default:
            max_length = pitch * height;
    }

    if (FAILED(hr = media_buffer_init(object, max_length, row_alignment, &media_buffer_vtbl)))
    {
        free(object);
        return hr;
    }

    object->IMF2DBuffer2_iface.lpVtbl = &buffer_2d_vtbl;
    object->IMFGetService_iface.lpVtbl = &get_service_vtbl;
    object->fourcc = fourcc;
    object->plane_size = plane_size;
    object->width = stride;
    object->height = height;
    object->pitch = bottom_up ? -pitch : pitch;
    object->scanline0 = bottom_up ? object->data + pitch * (object->height - 1) : object->data;

    *buffer = &object->IMFMediaBuffer_iface;

    return S_OK;
}

/***********************************************************************
 *      MFCreate2DMediaBuffer (mfplat.@)
 */
HRESULT WINAPI MFCreate2DMediaBuffer(DWORD width, DWORD height, DWORD fourcc, BOOL bottom_up, IMFMediaBuffer **buffer)
{
    TRACE("%lu, %lu, %s, %d, %p.\n", width, height, debugstr_fourcc(fourcc), bottom_up, buffer);

    return create_2d_buffer(width, height, fourcc, bottom_up, buffer);
}
