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
#include "initguid.h"
#include "d3d9.h"
#include "evr.h"

WINE_DEFAULT_DEBUG_CHANNEL(mfplat);

#define ALIGN_SIZE(size, alignment) (((size) + (alignment)) & ~((alignment)))

struct buffer
{
    IMFMediaBuffer IMFMediaBuffer_iface;
    IMF2DBuffer2 IMF2DBuffer2_iface;
    IMFGetService IMFGetService_iface;
    LONG refcount;

    BYTE *data;
    DWORD max_length;
    DWORD current_length;

    struct
    {
        DWORD fourcc;
        BYTE *linear_buffer;
        DWORD plane_size;

        BYTE *scanline0;
        unsigned int width;
        unsigned int height;
        int pitch;
        unsigned int locks;
        MF2DBuffer_LockFlags lock_flags;
    } _2d;

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

static inline struct buffer *impl_from_IMFMediaBuffer(IMFMediaBuffer *iface)
{
    return CONTAINING_RECORD(iface, struct buffer, IMFMediaBuffer_iface);
}

static struct buffer *impl_from_IMF2DBuffer2(IMF2DBuffer2 *iface)
{
    return CONTAINING_RECORD(iface, struct buffer, IMF2DBuffer2_iface);
}

static struct buffer *impl_from_IMFGetService(IMFGetService *iface)
{
    return CONTAINING_RECORD(iface, struct buffer, IMFGetService_iface);
}

static HRESULT WINAPI memory_buffer_QueryInterface(IMFMediaBuffer *iface, REFIID riid, void **out)
{
    struct buffer *buffer = impl_from_IMFMediaBuffer(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), out);

    if (IsEqualIID(riid, &IID_IMFMediaBuffer) ||
            IsEqualIID(riid, &IID_IUnknown))
    {
        *out = &buffer->IMFMediaBuffer_iface;
        IMFMediaBuffer_AddRef(iface);
        return S_OK;
    }

    WARN("Unsupported %s.\n", debugstr_guid(riid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI memory_buffer_AddRef(IMFMediaBuffer *iface)
{
    struct buffer *buffer = impl_from_IMFMediaBuffer(iface);
    ULONG refcount = InterlockedIncrement(&buffer->refcount);

    TRACE("%p, refcount %lu.\n", buffer, refcount);

    return refcount;
}

static ULONG WINAPI memory_buffer_Release(IMFMediaBuffer *iface)
{
    struct buffer *buffer = impl_from_IMFMediaBuffer(iface);
    ULONG refcount = InterlockedDecrement(&buffer->refcount);

    TRACE("%p, refcount %lu.\n", iface, refcount);

    if (!refcount)
    {
        DeleteCriticalSection(&buffer->cs);
        free(buffer->_2d.linear_buffer);
        _aligned_free(buffer->data);
        free(buffer);
    }

    return refcount;
}

static HRESULT WINAPI memory_buffer_Lock(IMFMediaBuffer *iface, BYTE **data, DWORD *max_length, DWORD *current_length)
{
    struct buffer *buffer = impl_from_IMFMediaBuffer(iface);

    TRACE("%p, %p %p, %p.\n", iface, data, max_length, current_length);

    if (!data)
        return E_INVALIDARG;

    *data = buffer->data;
    if (max_length)
        *max_length = buffer->max_length;
    if (current_length)
        *current_length = buffer->current_length;

    return S_OK;
}

static HRESULT WINAPI memory_buffer_Unlock(IMFMediaBuffer *iface)
{
    TRACE("%p.\n", iface);

    return S_OK;
}

static HRESULT WINAPI memory_buffer_GetCurrentLength(IMFMediaBuffer *iface, DWORD *current_length)
{
    struct buffer *buffer = impl_from_IMFMediaBuffer(iface);

    TRACE("%p.\n", iface);

    if (!current_length)
        return E_INVALIDARG;

    *current_length = buffer->current_length;

    return S_OK;
}

static HRESULT WINAPI memory_buffer_SetCurrentLength(IMFMediaBuffer *iface, DWORD current_length)
{
    struct buffer *buffer = impl_from_IMFMediaBuffer(iface);

    TRACE("%p, %lu.\n", iface, current_length);

    if (current_length > buffer->max_length)
        return E_INVALIDARG;

    buffer->current_length = current_length;

    return S_OK;
}

static HRESULT WINAPI memory_buffer_GetMaxLength(IMFMediaBuffer *iface, DWORD *max_length)
{
    struct buffer *buffer = impl_from_IMFMediaBuffer(iface);

    TRACE("%p, %p.\n", iface, max_length);

    if (!max_length)
        return E_INVALIDARG;

    *max_length = buffer->max_length;

    return S_OK;
}

static const IMFMediaBufferVtbl memory_1d_buffer_vtbl =
{
    memory_buffer_QueryInterface,
    memory_buffer_AddRef,
    memory_buffer_Release,
    memory_buffer_Lock,
    memory_buffer_Unlock,
    memory_buffer_GetCurrentLength,
    memory_buffer_SetCurrentLength,
    memory_buffer_GetMaxLength,
};

static HRESULT WINAPI memory_1d_2d_buffer_QueryInterface(IMFMediaBuffer *iface, REFIID riid, void **out)
{
    struct buffer *buffer = impl_from_IMFMediaBuffer(iface);

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

static HRESULT WINAPI memory_1d_2d_buffer_Lock(IMFMediaBuffer *iface, BYTE **data, DWORD *max_length, DWORD *current_length)
{
    struct buffer *buffer = impl_from_IMFMediaBuffer(iface);
    HRESULT hr = S_OK;

    TRACE("%p, %p, %p, %p.\n", iface, data, max_length, current_length);

    if (!data)
        return E_POINTER;

    /* Allocate linear buffer and return it as a copy of current content. Maximum and current length are
       unrelated to 2D buffer maximum allocate length, or maintained current length. */

    EnterCriticalSection(&buffer->cs);

    if (!buffer->_2d.linear_buffer && buffer->_2d.locks)
        hr = MF_E_INVALIDREQUEST;
    else if (!buffer->_2d.linear_buffer)
    {
        if (!(buffer->_2d.linear_buffer = malloc(buffer->_2d.plane_size)))
            hr = E_OUTOFMEMORY;

        if (SUCCEEDED(hr))
        {
            int pitch = buffer->_2d.pitch;

            if (pitch < 0)
                pitch = -pitch;
            buffer_2d_copy_image(buffer->_2d.fourcc, buffer->_2d.linear_buffer, buffer->_2d.width, buffer->data, pitch,
                    buffer->_2d.width, buffer->_2d.height);
        }
    }

    if (SUCCEEDED(hr))
    {
        ++buffer->_2d.locks;
        *data = buffer->_2d.linear_buffer;
        if (max_length)
            *max_length = buffer->_2d.plane_size;
        if (current_length)
            *current_length = buffer->_2d.plane_size;
    }

    LeaveCriticalSection(&buffer->cs);

    return hr;
}

static HRESULT WINAPI memory_1d_2d_buffer_Unlock(IMFMediaBuffer *iface)
{
    struct buffer *buffer = impl_from_IMFMediaBuffer(iface);

    TRACE("%p.\n", iface);

    EnterCriticalSection(&buffer->cs);

    if (buffer->_2d.linear_buffer && !--buffer->_2d.locks)
    {
        int pitch = buffer->_2d.pitch;

        if (pitch < 0)
            pitch = -pitch;
        buffer_2d_copy_image(buffer->_2d.fourcc, buffer->data, pitch, buffer->_2d.linear_buffer, buffer->_2d.width,
                buffer->_2d.width, buffer->_2d.height);

        free(buffer->_2d.linear_buffer);
        buffer->_2d.linear_buffer = NULL;
    }

    LeaveCriticalSection(&buffer->cs);

    return S_OK;
}

static const IMFMediaBufferVtbl memory_1d_2d_buffer_vtbl =
{
    memory_1d_2d_buffer_QueryInterface,
    memory_buffer_AddRef,
    memory_buffer_Release,
    memory_1d_2d_buffer_Lock,
    memory_1d_2d_buffer_Unlock,
    memory_buffer_GetCurrentLength,
    memory_buffer_SetCurrentLength,
    memory_buffer_GetMaxLength,
};

static HRESULT WINAPI memory_2d_buffer_QueryInterface(IMF2DBuffer2 *iface, REFIID riid, void **obj)
{
    struct buffer *buffer = impl_from_IMF2DBuffer2(iface);
    return IMFMediaBuffer_QueryInterface(&buffer->IMFMediaBuffer_iface, riid, obj);
}

static ULONG WINAPI memory_2d_buffer_AddRef(IMF2DBuffer2 *iface)
{
    struct buffer *buffer = impl_from_IMF2DBuffer2(iface);
    return IMFMediaBuffer_AddRef(&buffer->IMFMediaBuffer_iface);
}

static ULONG WINAPI memory_2d_buffer_Release(IMF2DBuffer2 *iface)
{
    struct buffer *buffer = impl_from_IMF2DBuffer2(iface);
    return IMFMediaBuffer_Release(&buffer->IMFMediaBuffer_iface);
}

static HRESULT memory_2d_buffer_lock(struct buffer *buffer, BYTE **scanline0, LONG *pitch,
        BYTE **buffer_start, DWORD *buffer_length)
{
    HRESULT hr = S_OK;

    if (buffer->_2d.linear_buffer)
        hr = MF_E_UNEXPECTED;
    else
    {
        ++buffer->_2d.locks;
        *scanline0 = buffer->_2d.scanline0;
        *pitch = buffer->_2d.pitch;
        if (buffer_start)
            *buffer_start = buffer->data;
        if (buffer_length)
            *buffer_length = buffer->max_length;
    }

    return hr;
}

static HRESULT WINAPI memory_2d_buffer_Lock2D(IMF2DBuffer2 *iface, BYTE **scanline0, LONG *pitch)
{
    struct buffer *buffer = impl_from_IMF2DBuffer2(iface);
    HRESULT hr;

    TRACE("%p, %p, %p.\n", iface, scanline0, pitch);

    if (!scanline0 || !pitch)
        return E_POINTER;

    EnterCriticalSection(&buffer->cs);

    hr = memory_2d_buffer_lock(buffer, scanline0, pitch, NULL, NULL);

    LeaveCriticalSection(&buffer->cs);

    return hr;
}

static HRESULT WINAPI memory_2d_buffer_Unlock2D(IMF2DBuffer2 *iface)
{
    struct buffer *buffer = impl_from_IMF2DBuffer2(iface);
    HRESULT hr = S_OK;

    TRACE("%p.\n", iface);

    EnterCriticalSection(&buffer->cs);

    if (!buffer->_2d.linear_buffer)
    {
        if (buffer->_2d.locks)
            --buffer->_2d.locks;
        else
            hr = HRESULT_FROM_WIN32(ERROR_WAS_UNLOCKED);
    }

    LeaveCriticalSection(&buffer->cs);

    return hr;
}

static HRESULT WINAPI memory_2d_buffer_GetScanline0AndPitch(IMF2DBuffer2 *iface, BYTE **scanline0, LONG *pitch)
{
    struct buffer *buffer = impl_from_IMF2DBuffer2(iface);
    HRESULT hr = S_OK;

    TRACE("%p, %p, %p.\n", iface, scanline0, pitch);

    if (!scanline0 || !pitch)
        return E_POINTER;

    EnterCriticalSection(&buffer->cs);

    if (buffer->_2d.linear_buffer || !buffer->_2d.locks)
        hr = HRESULT_FROM_WIN32(ERROR_WAS_UNLOCKED);
    else
    {
        *scanline0 = buffer->_2d.scanline0;
        *pitch = buffer->_2d.pitch;
    }

    LeaveCriticalSection(&buffer->cs);

    return hr;
}

static HRESULT WINAPI memory_2d_buffer_IsContiguousFormat(IMF2DBuffer2 *iface, BOOL *is_contiguous)
{
    TRACE("%p, %p.\n", iface, is_contiguous);

    if (!is_contiguous)
        return E_POINTER;

    *is_contiguous = FALSE;

    return S_OK;
}

static HRESULT WINAPI memory_2d_buffer_GetContiguousLength(IMF2DBuffer2 *iface, DWORD *length)
{
    struct buffer *buffer = impl_from_IMF2DBuffer2(iface);

    TRACE("%p, %p.\n", iface, length);

    if (!length)
        return E_POINTER;

    *length = buffer->_2d.plane_size;

    return S_OK;
}

static HRESULT WINAPI memory_2d_buffer_ContiguousCopyTo(IMF2DBuffer2 *iface, BYTE *dest_buffer, DWORD dest_length)
{
    struct buffer *buffer = impl_from_IMF2DBuffer2(iface);
    BYTE *src_scanline0, *src_buffer_start;
    DWORD src_length;
    LONG src_pitch;
    HRESULT hr;

    TRACE("%p, %p, %lu.\n", iface, dest_buffer, dest_length);

    if (dest_length < buffer->_2d.plane_size)
        return E_INVALIDARG;

    hr = IMF2DBuffer2_Lock2DSize(iface, MF2DBuffer_LockFlags_Read, &src_scanline0, &src_pitch, &src_buffer_start, &src_length);

    if (SUCCEEDED(hr))
    {
        if (src_pitch < 0)
            src_pitch = -src_pitch;
        buffer_2d_copy_image(buffer->_2d.fourcc, dest_buffer, buffer->_2d.width, src_buffer_start, src_pitch,
                buffer->_2d.width, buffer->_2d.height);

        if (FAILED(IMF2DBuffer2_Unlock2D(iface)))
            WARN("Couldn't unlock source buffer %p, hr %#lx.\n", iface, hr);
    }

    return S_OK;
}

static HRESULT WINAPI memory_2d_buffer_ContiguousCopyFrom(IMF2DBuffer2 *iface, const BYTE *src_buffer, DWORD src_length)
{
    struct buffer *buffer = impl_from_IMF2DBuffer2(iface);
    BYTE *dst_scanline0, *dst_buffer_start;
    DWORD dst_length;
    LONG dst_pitch;
    HRESULT hr;

    TRACE("%p, %p, %lu.\n", iface, src_buffer, src_length);

    if (src_length < buffer->_2d.plane_size)
        return E_INVALIDARG;

    hr = IMF2DBuffer2_Lock2DSize(iface, MF2DBuffer_LockFlags_Write, &dst_scanline0, &dst_pitch, &dst_buffer_start, &dst_length);

    if (SUCCEEDED(hr))
    {
        if (dst_pitch < 0)
            dst_pitch = -dst_pitch;
        buffer_2d_copy_image(buffer->_2d.fourcc, dst_buffer_start, dst_pitch, src_buffer, buffer->_2d.width,
                buffer->_2d.width, buffer->_2d.height);

        if (FAILED(IMF2DBuffer2_Unlock2D(iface)))
            WARN("Couldn't unlock destination buffer %p, hr %#lx.\n", iface, hr);
    }

    return hr;
}

static HRESULT WINAPI memory_2d_buffer_Lock2DSize(IMF2DBuffer2 *iface, MF2DBuffer_LockFlags flags, BYTE **scanline0,
        LONG *pitch, BYTE **buffer_start, DWORD *buffer_length)
{
    struct buffer *buffer = impl_from_IMF2DBuffer2(iface);
    HRESULT hr;

    TRACE("%p, %#x, %p, %p, %p, %p.\n", iface, flags, scanline0, pitch, buffer_start, buffer_length);

    if (!scanline0 || !pitch || !buffer_start || !buffer_length)
        return E_POINTER;

    EnterCriticalSection(&buffer->cs);

    hr = memory_2d_buffer_lock(buffer, scanline0, pitch, buffer_start, buffer_length);

    LeaveCriticalSection(&buffer->cs);

    return hr;
}

static HRESULT WINAPI memory_2d_buffer_Copy2DTo(IMF2DBuffer2 *iface, IMF2DBuffer2 *dst_buffer)
{
    struct buffer *buffer = impl_from_IMF2DBuffer2(iface);
    BYTE *src_scanline0, *dst_scanline0, *src_buffer_start, *dst_buffer_start;
    DWORD src_length, dst_length;
    LONG src_pitch, dst_pitch;
    BOOL src_locked = FALSE, dst_locked = FALSE;
    HRESULT hr, hr2;

    TRACE("%p, %p.\n", iface, dst_buffer);

    hr = IMF2DBuffer2_Lock2DSize(iface, MF2DBuffer_LockFlags_Read, &src_scanline0,
            &src_pitch, &src_buffer_start, &src_length);
    if (FAILED(hr))
        goto end;
    src_locked = TRUE;

    hr = IMF2DBuffer2_Lock2DSize(dst_buffer, MF2DBuffer_LockFlags_Write, &dst_scanline0,
            &dst_pitch, &dst_buffer_start, &dst_length);
    if (FAILED(hr))
        goto end;
    dst_locked = TRUE;

    buffer_2d_copy_image(buffer->_2d.fourcc, dst_scanline0, dst_pitch, src_scanline0, src_pitch,
            buffer->_2d.width, buffer->_2d.height);

end:
    if (src_locked)
    {
        hr2 = IMF2DBuffer2_Unlock2D(iface);
        if (FAILED(hr2))
            WARN("Unlocking source buffer %p failed with hr %#lx.\n", iface, hr2);
        if (FAILED(hr2) && SUCCEEDED(hr))
            hr = hr2;
    }

    if (dst_locked)
    {
        hr2 = IMF2DBuffer2_Unlock2D(dst_buffer);
        if (FAILED(hr2))
            WARN("Unlocking destination buffer %p failed with hr %#lx.\n", dst_buffer, hr2);
        if (FAILED(hr2) && SUCCEEDED(hr))
            hr = hr2;
    }

    return hr;
}

static const IMF2DBuffer2Vtbl memory_2d_buffer_vtbl =
{
    memory_2d_buffer_QueryInterface,
    memory_2d_buffer_AddRef,
    memory_2d_buffer_Release,
    memory_2d_buffer_Lock2D,
    memory_2d_buffer_Unlock2D,
    memory_2d_buffer_GetScanline0AndPitch,
    memory_2d_buffer_IsContiguousFormat,
    memory_2d_buffer_GetContiguousLength,
    memory_2d_buffer_ContiguousCopyTo,
    memory_2d_buffer_ContiguousCopyFrom,
    memory_2d_buffer_Lock2DSize,
    memory_2d_buffer_Copy2DTo,
};

static HRESULT WINAPI memory_2d_buffer_gs_QueryInterface(IMFGetService *iface, REFIID riid, void **obj)
{
    struct buffer *buffer = impl_from_IMFGetService(iface);
    return IMFMediaBuffer_QueryInterface(&buffer->IMFMediaBuffer_iface, riid, obj);
}

static ULONG WINAPI memory_2d_buffer_gs_AddRef(IMFGetService *iface)
{
    struct buffer *buffer = impl_from_IMFGetService(iface);
    return IMFMediaBuffer_AddRef(&buffer->IMFMediaBuffer_iface);
}

static ULONG WINAPI memory_2d_buffer_gs_Release(IMFGetService *iface)
{
    struct buffer *buffer = impl_from_IMFGetService(iface);
    return IMFMediaBuffer_Release(&buffer->IMFMediaBuffer_iface);
}

static HRESULT WINAPI memory_2d_buffer_gs_GetService(IMFGetService *iface, REFGUID service, REFIID riid, void **obj)
{
    TRACE("%p, %s, %s, %p.\n", iface, debugstr_guid(service), debugstr_guid(riid), obj);

    return E_NOTIMPL;
}

static const IMFGetServiceVtbl memory_2d_buffer_gs_vtbl =
{
    memory_2d_buffer_gs_QueryInterface,
    memory_2d_buffer_gs_AddRef,
    memory_2d_buffer_gs_Release,
    memory_2d_buffer_gs_GetService,
};

static HRESULT memory_buffer_init(struct buffer *buffer, DWORD max_length, DWORD alignment,
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

static HRESULT create_1d_buffer(DWORD max_length, DWORD alignment, IMFMediaBuffer **buffer)
{
    struct buffer *object;
    HRESULT hr;

    if (!buffer)
        return E_POINTER;

    *buffer = NULL;

    if (!(object = calloc(1, sizeof(*object))))
        return E_OUTOFMEMORY;

    hr = memory_buffer_init(object, max_length, alignment, &memory_1d_buffer_vtbl);
    if (FAILED(hr))
    {
        free(object);
        return hr;
    }

    *buffer = &object->IMFMediaBuffer_iface;

    return S_OK;
}

static HRESULT create_2d_buffer(DWORD width, DWORD height, DWORD fourcc, BOOL bottom_up, IMFMediaBuffer **buffer)
{
    unsigned int stride, max_length;
    unsigned int row_alignment;
    struct buffer *object;
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

    if (FAILED(hr = memory_buffer_init(object, max_length, row_alignment, &memory_1d_2d_buffer_vtbl)))
    {
        free(object);
        return hr;
    }

    object->IMF2DBuffer2_iface.lpVtbl = &memory_2d_buffer_vtbl;
    object->IMFGetService_iface.lpVtbl = &memory_2d_buffer_gs_vtbl;
    object->_2d.fourcc = fourcc;
    object->_2d.plane_size = plane_size;
    object->_2d.width = stride;
    object->_2d.height = height;
    object->_2d.pitch = bottom_up ? -pitch : pitch;
    object->_2d.scanline0 = bottom_up ? object->data + pitch * (object->_2d.height - 1) : object->data;

    *buffer = &object->IMFMediaBuffer_iface;

    return S_OK;
}

/***********************************************************************
 *      MFCreateMemoryBuffer (mfplat.@)
 */
HRESULT WINAPI MFCreateMemoryBuffer(DWORD max_length, IMFMediaBuffer **buffer)
{
    TRACE("%lu, %p.\n", max_length, buffer);

    return create_1d_buffer(max_length, MF_1_BYTE_ALIGNMENT, buffer);
}

/***********************************************************************
 *      MFCreateAlignedMemoryBuffer (mfplat.@)
 */
HRESULT WINAPI MFCreateAlignedMemoryBuffer(DWORD max_length, DWORD alignment, IMFMediaBuffer **buffer)
{
    TRACE("%lu, %lu, %p.\n", max_length, alignment, buffer);

    return create_1d_buffer(max_length, alignment, buffer);
}

/***********************************************************************
 *      MFCreate2DMediaBuffer (mfplat.@)
 */
HRESULT WINAPI MFCreate2DMediaBuffer(DWORD width, DWORD height, DWORD fourcc, BOOL bottom_up, IMFMediaBuffer **buffer)
{
    TRACE("%lu, %lu, %s, %d, %p.\n", width, height, mf_debugstr_fourcc(fourcc), bottom_up, buffer);

    return create_2d_buffer(width, height, fourcc, bottom_up, buffer);
}

static unsigned int buffer_get_aligned_length(unsigned int length, unsigned int alignment)
{
    length = (length + alignment) / alignment;
    length *= alignment;

    return length;
}

HRESULT WINAPI MFCreateMediaBufferFromMediaType(IMFMediaType *media_type, LONGLONG duration, DWORD min_length,
        DWORD alignment, IMFMediaBuffer **buffer)
{
    UINT32 length = 0, block_alignment;
    LONGLONG avg_length;
    HRESULT hr;
    GUID major;

    TRACE("%p, %s, %lu, %lu, %p.\n", media_type, debugstr_time(duration), min_length, alignment, buffer);

    if (!media_type)
        return E_INVALIDARG;

    if (FAILED(hr = IMFMediaType_GetMajorType(media_type, &major)))
        return hr;

    if (IsEqualGUID(&major, &MFMediaType_Audio))
    {
        block_alignment = 0;
        if (FAILED(IMFMediaType_GetUINT32(media_type, &MF_MT_AUDIO_BLOCK_ALIGNMENT, &block_alignment)))
            WARN("Block alignment was not specified.\n");

        alignment = max(16, alignment);

        if (block_alignment)
        {
            avg_length = 0;

            if (duration)
            {
                length = 0;
                if (SUCCEEDED(IMFMediaType_GetUINT32(media_type, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &length)))
                {
                    /* 100 ns -> 1 s */
                    avg_length = length * duration / (10 * 1000 * 1000);
                }
            }

            length = buffer_get_aligned_length(avg_length + 1, alignment);
            length = buffer_get_aligned_length(length, block_alignment);
        }
        else
            length = 0;

        length = max(length, min_length);

        return create_1d_buffer(length, alignment - 1, buffer);
    }
    else
        FIXME("Major type %s is not supported.\n", debugstr_guid(&major));

    return E_NOTIMPL;
}
