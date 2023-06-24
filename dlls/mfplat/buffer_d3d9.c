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

struct d3d9_buffer
{
    IMFMediaBuffer IMFMediaBuffer_iface;
    IMF2DBuffer2 IMF2DBuffer2_iface;
    IMFGetService IMFGetService_iface;
    LONG refcount;

    IDirect3DSurface9 *surface;

    DWORD current_length;
    DWORD plane_size;

    DWORD fourcc;
    UINT width;
    UINT height;

    UINT locks;
    MF2DBuffer_LockFlags lock_flags;

    BYTE *linear_buffer;
    D3DLOCKED_RECT rect;

    CRITICAL_SECTION cs;
};

static HRESULT d3d9_surface_buffer_lock(struct d3d9_buffer *buffer, MF2DBuffer_LockFlags flags, BYTE **scanline0,
        LONG *pitch, BYTE **buffer_start, DWORD *buffer_length)
{
    HRESULT hr = S_OK;

    if (buffer->linear_buffer)
        hr = MF_E_UNEXPECTED;
    else if (!buffer->locks)
        hr = IDirect3DSurface9_LockRect(buffer->surface, &buffer->rect, NULL, 0);
    else if (buffer->lock_flags == MF2DBuffer_LockFlags_Write && flags != MF2DBuffer_LockFlags_Write)
        hr = HRESULT_FROM_WIN32(ERROR_WAS_LOCKED);

    if (SUCCEEDED(hr))
    {
        if (!buffer->locks)
            buffer->lock_flags = flags;
        buffer->locks++;
        *scanline0 = buffer->rect.pBits;
        *pitch = buffer->rect.Pitch;
        if (buffer_start)
            *buffer_start = *scanline0;
        if (buffer_length)
            *buffer_length = buffer->rect.Pitch * buffer->height;
    }

    return hr;
}

static inline struct d3d9_buffer *impl_from_IMFMediaBuffer(IMFMediaBuffer *iface)
{
    return CONTAINING_RECORD(iface, struct d3d9_buffer, IMFMediaBuffer_iface);
}

static HRESULT WINAPI d3d9_media_buffer_QueryInterface(IMFMediaBuffer *iface, REFIID riid, void **out)
{
    struct d3d9_buffer *buffer = impl_from_IMFMediaBuffer(iface);

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

static ULONG WINAPI d3d9_media_buffer_AddRef(IMFMediaBuffer *iface)
{
    struct d3d9_buffer *buffer = impl_from_IMFMediaBuffer(iface);
    ULONG refcount = InterlockedIncrement(&buffer->refcount);

    TRACE("%p, refcount %lu.\n", buffer, refcount);

    return refcount;
}

static ULONG WINAPI d3d9_media_buffer_Release(IMFMediaBuffer *iface)
{
    struct d3d9_buffer *buffer = impl_from_IMFMediaBuffer(iface);
    ULONG refcount = InterlockedDecrement(&buffer->refcount);

    TRACE("%p, refcount %lu.\n", iface, refcount);

    if (!refcount)
    {
        if (buffer->surface)
            IDirect3DSurface9_Release(buffer->surface);
        DeleteCriticalSection(&buffer->cs);
        free(buffer->linear_buffer);
        free(buffer);
    }

    return refcount;
}

static HRESULT WINAPI d3d9_media_buffer_Lock(IMFMediaBuffer *iface, BYTE **data, DWORD *max_length, DWORD *current_length)
{
    struct d3d9_buffer *buffer = impl_from_IMFMediaBuffer(iface);
    HRESULT hr = S_OK;

    TRACE("%p, %p, %p, %p.\n", iface, data, max_length, current_length);

    if (!data)
        return E_POINTER;

    EnterCriticalSection(&buffer->cs);

    if (!buffer->linear_buffer && buffer->locks)
        hr = MF_E_INVALIDREQUEST;
    else if (!buffer->linear_buffer)
    {
        D3DLOCKED_RECT rect;

        if (!(buffer->linear_buffer = malloc(buffer->plane_size)))
            hr = E_OUTOFMEMORY;

        if (SUCCEEDED(hr))
        {
            hr = IDirect3DSurface9_LockRect(buffer->surface, &rect, NULL, 0);
            if (SUCCEEDED(hr))
            {
                buffer_2d_copy_image(buffer->fourcc, buffer->linear_buffer, buffer->width, rect.pBits, rect.Pitch,
                        buffer->width, buffer->height);
                IDirect3DSurface9_UnlockRect(buffer->surface);
            }
        }
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

static HRESULT WINAPI d3d9_media_buffer_Unlock(IMFMediaBuffer *iface)
{
    struct d3d9_buffer *buffer = impl_from_IMFMediaBuffer(iface);
    HRESULT hr = S_OK;

    TRACE("%p.\n", iface);

    EnterCriticalSection(&buffer->cs);

    if (!buffer->linear_buffer)
        hr = HRESULT_FROM_WIN32(ERROR_WAS_UNLOCKED);
    else if (!--buffer->locks)
    {
        D3DLOCKED_RECT rect;

        if (SUCCEEDED(hr = IDirect3DSurface9_LockRect(buffer->surface, &rect, NULL, 0)))
        {
            buffer_2d_copy_image(buffer->fourcc, rect.pBits, rect.Pitch, buffer->linear_buffer, buffer->width,
                    buffer->width, buffer->height);
            IDirect3DSurface9_UnlockRect(buffer->surface);
        }

        free(buffer->linear_buffer);
        buffer->linear_buffer = NULL;
    }

    LeaveCriticalSection(&buffer->cs);

    return hr;
}

static HRESULT WINAPI d3d9_media_buffer_GetCurrentLength(IMFMediaBuffer *iface, DWORD *current_length)
{
    struct d3d9_buffer *buffer = impl_from_IMFMediaBuffer(iface);

    TRACE("%p.\n", iface);

    if (!current_length)
        return E_INVALIDARG;

    *current_length = buffer->current_length;

    return S_OK;
}

static HRESULT WINAPI d3d9_media_buffer_SetCurrentLength(IMFMediaBuffer *iface, DWORD current_length)
{
    struct d3d9_buffer *buffer = impl_from_IMFMediaBuffer(iface);

    TRACE("%p, %lu.\n", iface, current_length);

    buffer->current_length = current_length;

    return S_OK;
}

static HRESULT WINAPI d3d9_media_buffer_GetMaxLength(IMFMediaBuffer *iface, DWORD *max_length)
{
    struct d3d9_buffer *buffer = impl_from_IMFMediaBuffer(iface);

    TRACE("%p, %p.\n", iface, max_length);

    if (!max_length)
        return E_INVALIDARG;

    *max_length = buffer->plane_size;

    return S_OK;
}

static const IMFMediaBufferVtbl d3d9_surface_1d_buffer_vtbl =
{
    d3d9_media_buffer_QueryInterface,
    d3d9_media_buffer_AddRef,
    d3d9_media_buffer_Release,
    d3d9_media_buffer_Lock,
    d3d9_media_buffer_Unlock,
    d3d9_media_buffer_GetCurrentLength,
    d3d9_media_buffer_SetCurrentLength,
    d3d9_media_buffer_GetMaxLength,
};

static struct d3d9_buffer *impl_from_IMF2DBuffer2(IMF2DBuffer2 *iface)
{
    return CONTAINING_RECORD(iface, struct d3d9_buffer, IMF2DBuffer2_iface);
}

static HRESULT WINAPI d3d9_2d_buffer_QueryInterface(IMF2DBuffer2 *iface, REFIID riid, void **obj)
{
    struct d3d9_buffer *buffer = impl_from_IMF2DBuffer2(iface);
    return IMFMediaBuffer_QueryInterface(&buffer->IMFMediaBuffer_iface, riid, obj);
}

static ULONG WINAPI d3d9_2d_buffer_AddRef(IMF2DBuffer2 *iface)
{
    struct d3d9_buffer *buffer = impl_from_IMF2DBuffer2(iface);
    return IMFMediaBuffer_AddRef(&buffer->IMFMediaBuffer_iface);
}

static ULONG WINAPI d3d9_2d_buffer_Release(IMF2DBuffer2 *iface)
{
    struct d3d9_buffer *buffer = impl_from_IMF2DBuffer2(iface);
    return IMFMediaBuffer_Release(&buffer->IMFMediaBuffer_iface);
}

static HRESULT WINAPI d3d9_2d_buffer_Lock2D(IMF2DBuffer2 *iface, BYTE **scanline0, LONG *pitch)
{
    struct d3d9_buffer *buffer = impl_from_IMF2DBuffer2(iface);
    HRESULT hr;

    TRACE("%p, %p, %p.\n", iface, scanline0, pitch);

    if (!scanline0 || !pitch)
        return E_POINTER;

    EnterCriticalSection(&buffer->cs);

    hr = d3d9_surface_buffer_lock(buffer, MF2DBuffer_LockFlags_ReadWrite, scanline0, pitch, NULL, NULL);

    LeaveCriticalSection(&buffer->cs);

    return hr;
}

static HRESULT WINAPI d3d9_2d_buffer_Unlock2D(IMF2DBuffer2 *iface)
{
    struct d3d9_buffer *buffer = impl_from_IMF2DBuffer2(iface);
    HRESULT hr = S_OK;

    TRACE("%p.\n", iface);

    EnterCriticalSection(&buffer->cs);

    if (buffer->locks)
    {
        if (!--buffer->locks)
        {
            IDirect3DSurface9_UnlockRect(buffer->surface);
            memset(&buffer->rect, 0, sizeof(buffer->rect));
            buffer->lock_flags = 0;
        }
    }
    else
        hr = HRESULT_FROM_WIN32(ERROR_WAS_UNLOCKED);

    LeaveCriticalSection(&buffer->cs);

    return hr;
}

static HRESULT WINAPI d3d9_2d_buffer_GetScanline0AndPitch(IMF2DBuffer2 *iface, BYTE **scanline0, LONG *pitch)
{
    struct d3d9_buffer *buffer = impl_from_IMF2DBuffer2(iface);
    HRESULT hr = S_OK;

    TRACE("%p, %p, %p.\n", iface, scanline0, pitch);

    if (!scanline0 || !pitch)
        return E_POINTER;

    EnterCriticalSection(&buffer->cs);

    if (!buffer->locks)
    {
        *scanline0 = NULL;
        *pitch = 0;
        hr = HRESULT_FROM_WIN32(ERROR_WAS_UNLOCKED);
    }
    else
    {
        *scanline0 = buffer->rect.pBits;
        *pitch = buffer->rect.Pitch;
    }

    LeaveCriticalSection(&buffer->cs);

    return hr;
}

static HRESULT WINAPI d3d9_2d_buffer_IsContiguousFormat(IMF2DBuffer2 *iface, BOOL *is_contiguous)
{
    TRACE("%p, %p.\n", iface, is_contiguous);

    if (!is_contiguous)
        return E_POINTER;

    *is_contiguous = FALSE;

    return S_OK;
}

static HRESULT WINAPI d3d9_2d_buffer_GetContiguousLength(IMF2DBuffer2 *iface, DWORD *length)
{
    struct d3d9_buffer *buffer = impl_from_IMF2DBuffer2(iface);

    TRACE("%p, %p.\n", iface, length);

    if (!length)
        return E_POINTER;

    *length = buffer->plane_size;

    return S_OK;
}

static HRESULT WINAPI d3d9_2d_buffer_ContiguousCopyTo(IMF2DBuffer2 *iface, BYTE *dest_buffer, DWORD dest_length)
{
    struct d3d9_buffer *buffer = impl_from_IMF2DBuffer2(iface);
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

static HRESULT WINAPI d3d9_2d_buffer_ContiguousCopyFrom(IMF2DBuffer2 *iface, const BYTE *src_buffer, DWORD src_length)
{
    struct d3d9_buffer *buffer = impl_from_IMF2DBuffer2(iface);
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

static HRESULT WINAPI d3d9_2d_buffer_Lock2DSize(IMF2DBuffer2 *iface, MF2DBuffer_LockFlags flags, BYTE **scanline0,
        LONG *pitch, BYTE **buffer_start, DWORD *buffer_length)
{
    struct d3d9_buffer *buffer = impl_from_IMF2DBuffer2(iface);
    HRESULT hr;

    TRACE("%p, %#x, %p, %p, %p, %p.\n", iface, flags, scanline0, pitch, buffer_start, buffer_length);

    if (!scanline0 || !pitch || !buffer_start || !buffer_length)
        return E_POINTER;

    EnterCriticalSection(&buffer->cs);

    hr = d3d9_surface_buffer_lock(buffer, flags, scanline0, pitch, buffer_start, buffer_length);

    LeaveCriticalSection(&buffer->cs);

    return hr;
}

static HRESULT WINAPI d3d9_2d_buffer_Copy2DTo(IMF2DBuffer2 *iface, IMF2DBuffer2 *dest_buffer)
{
    FIXME("%p, %p.\n", iface, dest_buffer);

    return E_NOTIMPL;
}

static const IMF2DBuffer2Vtbl d3d9_surface_buffer_vtbl =
{
    d3d9_2d_buffer_QueryInterface,
    d3d9_2d_buffer_AddRef,
    d3d9_2d_buffer_Release,
    d3d9_2d_buffer_Lock2D,
    d3d9_2d_buffer_Unlock2D,
    d3d9_2d_buffer_GetScanline0AndPitch,
    d3d9_2d_buffer_IsContiguousFormat,
    d3d9_2d_buffer_GetContiguousLength,
    d3d9_2d_buffer_ContiguousCopyTo,
    d3d9_2d_buffer_ContiguousCopyFrom,
    d3d9_2d_buffer_Lock2DSize,
    d3d9_2d_buffer_Copy2DTo,
};

static struct d3d9_buffer *impl_from_IMFGetService(IMFGetService *iface)
{
    return CONTAINING_RECORD(iface, struct d3d9_buffer, IMFGetService_iface);
}

static HRESULT WINAPI d3d9_get_service_QueryInterface(IMFGetService *iface, REFIID riid, void **obj)
{
    struct d3d9_buffer *buffer = impl_from_IMFGetService(iface);
    return IMFMediaBuffer_QueryInterface(&buffer->IMFMediaBuffer_iface, riid, obj);
}

static ULONG WINAPI d3d9_get_service_AddRef(IMFGetService *iface)
{
    struct d3d9_buffer *buffer = impl_from_IMFGetService(iface);
    return IMFMediaBuffer_AddRef(&buffer->IMFMediaBuffer_iface);
}

static ULONG WINAPI d3d9_get_service_Release(IMFGetService *iface)
{
    struct d3d9_buffer *buffer = impl_from_IMFGetService(iface);
    return IMFMediaBuffer_Release(&buffer->IMFMediaBuffer_iface);
}

static HRESULT WINAPI d3d9_get_service_GetService(IMFGetService *iface, REFGUID service, REFIID riid, void **obj)
{
    struct d3d9_buffer *buffer = impl_from_IMFGetService(iface);

    TRACE("%p, %s, %s, %p.\n", iface, debugstr_guid(service), debugstr_guid(riid), obj);

    if (IsEqualGUID(service, &MR_BUFFER_SERVICE))
    {
        return IDirect3DSurface9_QueryInterface(buffer->surface, riid, obj);
    }

    return E_NOTIMPL;
}

static const IMFGetServiceVtbl d3d9_get_service_vtbl =
{
    d3d9_get_service_QueryInterface,
    d3d9_get_service_AddRef,
    d3d9_get_service_Release,
    d3d9_get_service_GetService,
};

static HRESULT create_d3d9_surface_buffer(IUnknown *surface, BOOL bottom_up, IMFMediaBuffer **buffer)
{
    struct d3d9_buffer *object;
    D3DSURFACE_DESC desc;
    UINT stride;
    GUID subtype;
    BOOL is_yuv;

    IDirect3DSurface9_GetDesc((IDirect3DSurface9 *)surface, &desc);
    TRACE("format %#x, %u x %u.\n", desc.Format, desc.Width, desc.Height);

    memcpy(&subtype, &MFVideoFormat_Base, sizeof(subtype));
    subtype.Data1 = desc.Format;

    if (!(stride = mf_format_get_stride(&subtype, desc.Width, &is_yuv)))
        return MF_E_INVALIDMEDIATYPE;

    if (!(object = calloc(1, sizeof(*object))))
        return E_OUTOFMEMORY;

    object->IMFMediaBuffer_iface.lpVtbl = &d3d9_surface_1d_buffer_vtbl;
    object->IMF2DBuffer2_iface.lpVtbl = &d3d9_surface_buffer_vtbl;
    object->IMFGetService_iface.lpVtbl = &d3d9_get_service_vtbl;
    object->refcount = 1;
    InitializeCriticalSection(&object->cs);
    object->surface = (IDirect3DSurface9 *)surface;
    IUnknown_AddRef(surface);

    MFGetPlaneSize(desc.Format, desc.Width, desc.Height, &object->plane_size);
    object->fourcc = desc.Format;
    object->width = stride;
    object->height = desc.Height;

    *buffer = &object->IMFMediaBuffer_iface;

    return S_OK;
}

/***********************************************************************
 *      MFCreateDXSurfaceBuffer (mfplat.@)
 */
HRESULT WINAPI MFCreateDXSurfaceBuffer(REFIID riid, IUnknown *surface, BOOL bottom_up, IMFMediaBuffer **buffer)
{
    TRACE("%s, %p, %d, %p.\n", debugstr_guid(riid), surface, bottom_up, buffer);

    if (!IsEqualIID(riid, &IID_IDirect3DSurface9))
        return E_INVALIDARG;

    return create_d3d9_surface_buffer(surface, bottom_up, buffer);
}
