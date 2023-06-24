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

struct dxgi_buffer
{
    IMFMediaBuffer IMFMediaBuffer_iface;
    IMF2DBuffer2 IMF2DBuffer2_iface;
    IMFDXGIBuffer IMFDXGIBuffer_iface;
    IMFGetService IMFGetService_iface;
    LONG refcount;

    ID3D11Texture2D *texture;
    UINT sub_resource_idx;

    DWORD current_length;
    DWORD plane_size;

    DWORD fourcc;
    UINT width;
    UINT height;

    UINT locks;
    MF2DBuffer_LockFlags lock_flags;

    BYTE *linear_buffer;
    ID3D11Texture2D *rb_texture;
    D3D11_MAPPED_SUBRESOURCE map_desc;
    struct attributes attributes;

    CRITICAL_SECTION cs;
};

static HRESULT dxgi_surface_buffer_create_readback_texture(struct dxgi_buffer *buffer)
{
    D3D11_TEXTURE2D_DESC texture_desc;
    ID3D11Device *device;
    HRESULT hr;

    if (buffer->rb_texture)
        return S_OK;

    ID3D11Texture2D_GetDevice(buffer->texture, &device);

    ID3D11Texture2D_GetDesc(buffer->texture, &texture_desc);
    texture_desc.Usage = D3D11_USAGE_STAGING;
    texture_desc.BindFlags = 0;
    texture_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
    texture_desc.MiscFlags = 0;
    texture_desc.MipLevels = 1;
    if (FAILED(hr = ID3D11Device_CreateTexture2D(device, &texture_desc, NULL, &buffer->rb_texture)))
        WARN("Failed to create readback texture, hr %#lx.\n", hr);

    ID3D11Device_Release(device);

    return hr;
}

static HRESULT dxgi_surface_buffer_map(struct dxgi_buffer *buffer, MF2DBuffer_LockFlags flags)
{
    ID3D11DeviceContext *immediate_context;
    ID3D11Device *device;
    HRESULT hr;

    if (FAILED(hr = dxgi_surface_buffer_create_readback_texture(buffer)))
        return hr;

    ID3D11Texture2D_GetDevice(buffer->texture, &device);
    ID3D11Device_GetImmediateContext(device, &immediate_context);

    if (flags == MF2DBuffer_LockFlags_Read || flags == MF2DBuffer_LockFlags_ReadWrite)
    {
        ID3D11DeviceContext_CopySubresourceRegion(immediate_context, (ID3D11Resource *)buffer->rb_texture,
                0, 0, 0, 0, (ID3D11Resource *)buffer->texture, buffer->sub_resource_idx, NULL);
    }

    memset(&buffer->map_desc, 0, sizeof(buffer->map_desc));
    if (FAILED(hr = ID3D11DeviceContext_Map(immediate_context, (ID3D11Resource *)buffer->rb_texture,
            0, D3D11_MAP_READ_WRITE, 0, &buffer->map_desc)))
    {
        WARN("Failed to map readback texture, hr %#lx.\n", hr);
    }

    ID3D11DeviceContext_Release(immediate_context);
    ID3D11Device_Release(device);

    return hr;
}

static void dxgi_surface_buffer_unmap(struct dxgi_buffer *buffer, MF2DBuffer_LockFlags flags)
{
    ID3D11DeviceContext *immediate_context;
    ID3D11Device *device;

    ID3D11Texture2D_GetDevice(buffer->texture, &device);
    ID3D11Device_GetImmediateContext(device, &immediate_context);
    ID3D11DeviceContext_Unmap(immediate_context, (ID3D11Resource *)buffer->rb_texture, 0);
    memset(&buffer->map_desc, 0, sizeof(buffer->map_desc));

    if (flags == MF2DBuffer_LockFlags_Write || flags == MF2DBuffer_LockFlags_ReadWrite)
    {
        ID3D11DeviceContext_CopySubresourceRegion(immediate_context, (ID3D11Resource *)buffer->texture,
                buffer->sub_resource_idx, 0, 0, 0, (ID3D11Resource *)buffer->rb_texture, 0, NULL);
    }

    ID3D11DeviceContext_Release(immediate_context);
    ID3D11Device_Release(device);
}

static HRESULT dxgi_surface_buffer_lock(struct dxgi_buffer *buffer, MF2DBuffer_LockFlags flags,
        BYTE **scanline0, LONG *pitch, BYTE **buffer_start, DWORD *buffer_length)
{
    HRESULT hr = S_OK;

    if (buffer->linear_buffer)
        hr = MF_E_UNEXPECTED;
    else if (!buffer->locks)
        hr = dxgi_surface_buffer_map(buffer, flags);
    else if (buffer->lock_flags == MF2DBuffer_LockFlags_Write && flags != MF2DBuffer_LockFlags_Write)
        hr = HRESULT_FROM_WIN32(ERROR_WAS_LOCKED);

    if (SUCCEEDED(hr))
    {
        if (!buffer->locks)
            buffer->lock_flags = flags;
        else
            buffer->lock_flags |= flags;
        buffer->locks++;
        *scanline0 = buffer->map_desc.pData;
        *pitch = buffer->map_desc.RowPitch;
        if (buffer_start)
            *buffer_start = *scanline0;
        if (buffer_length)
            *buffer_length = buffer->map_desc.DepthPitch;
    }

    return hr;
}

static inline struct dxgi_buffer *impl_from_IMFMediaBuffer(IMFMediaBuffer *iface)
{
    return CONTAINING_RECORD(iface, struct dxgi_buffer, IMFMediaBuffer_iface);
}

static HRESULT WINAPI dxgi_media_buffer_QueryInterface(IMFMediaBuffer *iface, REFIID riid, void **out)
{
    struct dxgi_buffer *buffer = impl_from_IMFMediaBuffer(iface);

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
    else if (IsEqualIID(riid, &IID_IMFDXGIBuffer))
    {
        *out = &buffer->IMFDXGIBuffer_iface;
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

static ULONG WINAPI dxgi_media_buffer_AddRef(IMFMediaBuffer *iface)
{
    struct dxgi_buffer *buffer = impl_from_IMFMediaBuffer(iface);
    ULONG refcount = InterlockedIncrement(&buffer->refcount);

    TRACE("%p, refcount %lu.\n", buffer, refcount);

    return refcount;
}

static ULONG WINAPI dxgi_media_buffer_Release(IMFMediaBuffer *iface)
{
    struct dxgi_buffer *buffer = impl_from_IMFMediaBuffer(iface);
    ULONG refcount = InterlockedDecrement(&buffer->refcount);

    TRACE("%p, refcount %lu.\n", iface, refcount);

    if (!refcount)
    {
        if (buffer->texture)
        {
            ID3D11Texture2D_Release(buffer->texture);
            if (buffer->rb_texture)
                ID3D11Texture2D_Release(buffer->rb_texture);
            clear_attributes_object(&buffer->attributes);
        }
        DeleteCriticalSection(&buffer->cs);
        free(buffer->linear_buffer);
        free(buffer);
    }

    return refcount;
}

static HRESULT WINAPI dxgi_media_buffer_GetCurrentLength(IMFMediaBuffer *iface, DWORD *current_length)
{
    struct dxgi_buffer *buffer = impl_from_IMFMediaBuffer(iface);

    TRACE("%p.\n", iface);

    if (!current_length)
        return E_INVALIDARG;

    *current_length = buffer->current_length;

    return S_OK;
}

static HRESULT WINAPI dxgi_media_buffer_Lock(IMFMediaBuffer *iface, BYTE **data, DWORD *max_length,
        DWORD *current_length)
{
    struct dxgi_buffer *buffer = impl_from_IMFMediaBuffer(iface);
    HRESULT hr = S_OK;

    TRACE("%p, %p, %p, %p.\n", iface, data, max_length, current_length);

    if (!data)
        return E_POINTER;

    EnterCriticalSection(&buffer->cs);

    if (!buffer->linear_buffer && buffer->locks)
        hr = MF_E_INVALIDREQUEST;
    else if (!buffer->linear_buffer)
    {
        if (!(buffer->linear_buffer = malloc(buffer->plane_size)))
            hr = E_OUTOFMEMORY;

        if (SUCCEEDED(hr))
        {
            hr = dxgi_surface_buffer_map(buffer, MF2DBuffer_LockFlags_ReadWrite);
            if (SUCCEEDED(hr))
            {
                buffer_2d_copy_image(buffer->fourcc, buffer->linear_buffer, buffer->width, buffer->map_desc.pData,
                        buffer->map_desc.RowPitch, buffer->width, buffer->height);
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

static HRESULT WINAPI dxgi_media_buffer_Unlock(IMFMediaBuffer *iface)
{
    struct dxgi_buffer *buffer = impl_from_IMFMediaBuffer(iface);
    HRESULT hr = S_OK;

    TRACE("%p.\n", iface);

    EnterCriticalSection(&buffer->cs);

    if (!buffer->linear_buffer)
        hr = HRESULT_FROM_WIN32(ERROR_WAS_UNLOCKED);
    else if (!--buffer->locks)
    {
        buffer_2d_copy_image(buffer->fourcc, buffer->map_desc.pData, buffer->map_desc.RowPitch,
                buffer->linear_buffer, buffer->width, buffer->width, buffer->height);
        dxgi_surface_buffer_unmap(buffer, MF2DBuffer_LockFlags_ReadWrite);

        free(buffer->linear_buffer);
        buffer->linear_buffer = NULL;
    }

    LeaveCriticalSection(&buffer->cs);

    return hr;
}

static HRESULT WINAPI dxgi_media_buffer_SetCurrentLength(IMFMediaBuffer *iface, DWORD current_length)
{
    struct dxgi_buffer *buffer = impl_from_IMFMediaBuffer(iface);

    TRACE("%p, %lu.\n", iface, current_length);

    buffer->current_length = current_length;

    return S_OK;
}

static HRESULT WINAPI dxgi_media_buffer_GetMaxLength(IMFMediaBuffer *iface, DWORD *max_length)
{
    struct dxgi_buffer *buffer = impl_from_IMFMediaBuffer(iface);

    TRACE("%p, %p.\n", iface, max_length);

    if (!max_length)
        return E_INVALIDARG;

    *max_length = buffer->plane_size;

    return S_OK;
}

static const IMFMediaBufferVtbl dxgi_surface_1d_buffer_vtbl =
{
    dxgi_media_buffer_QueryInterface,
    dxgi_media_buffer_AddRef,
    dxgi_media_buffer_Release,
    dxgi_media_buffer_Lock,
    dxgi_media_buffer_Unlock,
    dxgi_media_buffer_GetCurrentLength,
    dxgi_media_buffer_SetCurrentLength,
    dxgi_media_buffer_GetMaxLength,
};

static struct dxgi_buffer *impl_from_IMF2DBuffer2(IMF2DBuffer2 *iface)
{
    return CONTAINING_RECORD(iface, struct dxgi_buffer, IMF2DBuffer2_iface);
}

static HRESULT WINAPI dxgi_2d_buffer_QueryInterface(IMF2DBuffer2 *iface, REFIID riid, void **obj)
{
    struct dxgi_buffer *buffer = impl_from_IMF2DBuffer2(iface);
    return IMFMediaBuffer_QueryInterface(&buffer->IMFMediaBuffer_iface, riid, obj);
}

static ULONG WINAPI dxgi_2d_buffer_AddRef(IMF2DBuffer2 *iface)
{
    struct dxgi_buffer *buffer = impl_from_IMF2DBuffer2(iface);
    return IMFMediaBuffer_AddRef(&buffer->IMFMediaBuffer_iface);
}

static ULONG WINAPI dxgi_2d_buffer_Release(IMF2DBuffer2 *iface)
{
    struct dxgi_buffer *buffer = impl_from_IMF2DBuffer2(iface);
    return IMFMediaBuffer_Release(&buffer->IMFMediaBuffer_iface);
}

static HRESULT WINAPI dxgi_2d_buffer_Lock2D(IMF2DBuffer2 *iface, BYTE **scanline0, LONG *pitch)
{
    struct dxgi_buffer *buffer = impl_from_IMF2DBuffer2(iface);
    HRESULT hr;

    TRACE("%p, %p, %p.\n", iface, scanline0, pitch);

    if (!scanline0 || !pitch)
        return E_POINTER;

    EnterCriticalSection(&buffer->cs);

    hr = dxgi_surface_buffer_lock(buffer, MF2DBuffer_LockFlags_ReadWrite, scanline0, pitch, NULL, NULL);

    LeaveCriticalSection(&buffer->cs);

    return hr;
}

static HRESULT WINAPI dxgi_2d_buffer_Unlock2D(IMF2DBuffer2 *iface)
{
    struct dxgi_buffer *buffer = impl_from_IMF2DBuffer2(iface);
    HRESULT hr = S_OK;

    TRACE("%p.\n", iface);

    EnterCriticalSection(&buffer->cs);

    if (buffer->locks)
    {
        if (!--buffer->locks)
        {
            dxgi_surface_buffer_unmap(buffer, buffer->lock_flags);
            buffer->lock_flags = 0;
        }
    }
    else
        hr = HRESULT_FROM_WIN32(ERROR_WAS_UNLOCKED);

    LeaveCriticalSection(&buffer->cs);

    return hr;
}

static HRESULT WINAPI dxgi_2d_buffer_GetScanline0AndPitch(IMF2DBuffer2 *iface, BYTE **scanline0, LONG *pitch)
{
    struct dxgi_buffer *buffer = impl_from_IMF2DBuffer2(iface);
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
        *scanline0 = buffer->map_desc.pData;
        *pitch = buffer->map_desc.RowPitch;
    }

    LeaveCriticalSection(&buffer->cs);

    return hr;
}

static HRESULT WINAPI dxgi_2d_buffer_IsContiguousFormat(IMF2DBuffer2 *iface, BOOL *is_contiguous)
{
    TRACE("%p, %p.\n", iface, is_contiguous);

    if (!is_contiguous)
        return E_POINTER;

    *is_contiguous = FALSE;

    return S_OK;
}

static HRESULT WINAPI dxgi_2d_buffer_GetContiguousLength(IMF2DBuffer2 *iface, DWORD *length)
{
    struct dxgi_buffer *buffer = impl_from_IMF2DBuffer2(iface);

    TRACE("%p, %p.\n", iface, length);

    if (!length)
        return E_POINTER;

    *length = buffer->plane_size;

    return S_OK;
}

static HRESULT WINAPI dxgi_2d_buffer_ContiguousCopyTo(IMF2DBuffer2 *iface, BYTE *dest_buffer, DWORD dest_length)
{
    struct dxgi_buffer *buffer = impl_from_IMF2DBuffer2(iface);
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

static HRESULT WINAPI dxgi_2d_buffer_ContiguousCopyFrom(IMF2DBuffer2 *iface, const BYTE *src_buffer, DWORD src_length)
{
    struct dxgi_buffer *buffer = impl_from_IMF2DBuffer2(iface);
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

static HRESULT WINAPI dxgi_2d_buffer_Lock2DSize(IMF2DBuffer2 *iface, MF2DBuffer_LockFlags flags,
        BYTE **scanline0, LONG *pitch, BYTE **buffer_start, DWORD *buffer_length)
{
    struct dxgi_buffer *buffer = impl_from_IMF2DBuffer2(iface);
    HRESULT hr;

    TRACE("%p, %#x, %p, %p, %p, %p.\n", iface, flags, scanline0, pitch, buffer_start, buffer_length);

    if (!scanline0 || !pitch || !buffer_start || !buffer_length)
        return E_POINTER;

    EnterCriticalSection(&buffer->cs);

    hr = dxgi_surface_buffer_lock(buffer, flags, scanline0, pitch, buffer_start, buffer_length);

    LeaveCriticalSection(&buffer->cs);

    return hr;
}

static HRESULT WINAPI dxgi_2d_buffer_Copy2DTo(IMF2DBuffer2 *iface, IMF2DBuffer2 *dest_buffer)
{
    FIXME("%p, %p.\n", iface, dest_buffer);

    return E_NOTIMPL;
}

static const IMF2DBuffer2Vtbl dxgi_surface_buffer_vtbl =
{
    dxgi_2d_buffer_QueryInterface,
    dxgi_2d_buffer_AddRef,
    dxgi_2d_buffer_Release,
    dxgi_2d_buffer_Lock2D,
    dxgi_2d_buffer_Unlock2D,
    dxgi_2d_buffer_GetScanline0AndPitch,
    dxgi_2d_buffer_IsContiguousFormat,
    dxgi_2d_buffer_GetContiguousLength,
    dxgi_2d_buffer_ContiguousCopyTo,
    dxgi_2d_buffer_ContiguousCopyFrom,
    dxgi_2d_buffer_Lock2DSize,
    dxgi_2d_buffer_Copy2DTo,
};

static struct dxgi_buffer *impl_from_IMFDXGIBuffer(IMFDXGIBuffer *iface)
{
    return CONTAINING_RECORD(iface, struct dxgi_buffer, IMFDXGIBuffer_iface);
}

static HRESULT WINAPI dxgi_buffer_QueryInterface(IMFDXGIBuffer *iface, REFIID riid, void **obj)
{
    struct dxgi_buffer *buffer = impl_from_IMFDXGIBuffer(iface);
    return IMFMediaBuffer_QueryInterface(&buffer->IMFMediaBuffer_iface, riid, obj);
}

static ULONG WINAPI dxgi_buffer_AddRef(IMFDXGIBuffer *iface)
{
    struct dxgi_buffer *buffer = impl_from_IMFDXGIBuffer(iface);
    return IMFMediaBuffer_AddRef(&buffer->IMFMediaBuffer_iface);
}

static ULONG WINAPI dxgi_buffer_Release(IMFDXGIBuffer *iface)
{
    struct dxgi_buffer *buffer = impl_from_IMFDXGIBuffer(iface);
    return IMFMediaBuffer_Release(&buffer->IMFMediaBuffer_iface);
}

static HRESULT WINAPI dxgi_buffer_GetResource(IMFDXGIBuffer *iface, REFIID riid, void **obj)
{
    struct dxgi_buffer *buffer = impl_from_IMFDXGIBuffer(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), obj);

    return ID3D11Texture2D_QueryInterface(buffer->texture, riid, obj);
}

static HRESULT WINAPI dxgi_buffer_GetSubresourceIndex(IMFDXGIBuffer *iface, UINT *index)
{
    struct dxgi_buffer *buffer = impl_from_IMFDXGIBuffer(iface);

    TRACE("%p, %p.\n", iface, index);

    if (!index)
        return E_POINTER;

    *index = buffer->sub_resource_idx;

    return S_OK;
}

static HRESULT WINAPI dxgi_buffer_GetUnknown(IMFDXGIBuffer *iface, REFIID guid, REFIID riid, void **object)
{
    struct dxgi_buffer *buffer = impl_from_IMFDXGIBuffer(iface);

    TRACE("%p, %s, %s, %p.\n", iface, debugstr_guid(guid), debugstr_guid(riid), object);

    if (attributes_GetUnknown(&buffer->attributes, guid, riid, object) == MF_E_ATTRIBUTENOTFOUND)
        return MF_E_NOT_FOUND;

    return S_OK;
}

static HRESULT WINAPI dxgi_buffer_SetUnknown(IMFDXGIBuffer *iface, REFIID guid, IUnknown *data)
{
    struct dxgi_buffer *buffer = impl_from_IMFDXGIBuffer(iface);
    HRESULT hr = S_OK;

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(guid), data);

    EnterCriticalSection(&buffer->attributes.cs);
    if (data)
    {
        if (SUCCEEDED(attributes_GetItem(&buffer->attributes, guid, NULL)))
            hr = HRESULT_FROM_WIN32(ERROR_OBJECT_ALREADY_EXISTS);
        else
            hr = attributes_SetUnknown(&buffer->attributes, guid, data);
    }
    else
    {
        attributes_DeleteItem(&buffer->attributes, guid);
    }
    LeaveCriticalSection(&buffer->attributes.cs);

    return hr;
}

static const IMFDXGIBufferVtbl dxgi_buffer_vtbl =
{
    dxgi_buffer_QueryInterface,
    dxgi_buffer_AddRef,
    dxgi_buffer_Release,
    dxgi_buffer_GetResource,
    dxgi_buffer_GetSubresourceIndex,
    dxgi_buffer_GetUnknown,
    dxgi_buffer_SetUnknown,
};

static HRESULT create_dxgi_surface_buffer(IUnknown *surface, UINT sub_resource_idx,
        BOOL bottom_up, IMFMediaBuffer **buffer)
{
    struct dxgi_buffer *object;
    D3D11_TEXTURE2D_DESC desc;
    ID3D11Texture2D *texture;
    UINT stride;
    D3DFORMAT format;
    GUID subtype;
    BOOL is_yuv;
    HRESULT hr;

    if (FAILED(hr = IUnknown_QueryInterface(surface, &IID_ID3D11Texture2D, (void **)&texture)))
    {
        WARN("Failed to get texture interface, hr %#lx.\n", hr);
        return hr;
    }

    ID3D11Texture2D_GetDesc(texture, &desc);
    TRACE("format %#x, %u x %u.\n", desc.Format, desc.Width, desc.Height);

    memcpy(&subtype, &MFVideoFormat_Base, sizeof(subtype));
    subtype.Data1 = format = MFMapDXGIFormatToDX9Format(desc.Format);

    if (!(stride = mf_format_get_stride(&subtype, desc.Width, &is_yuv)))
    {
        ID3D11Texture2D_Release(texture);
        return MF_E_INVALIDMEDIATYPE;
    }

    if (!(object = calloc(1, sizeof(*object))))
    {
        ID3D11Texture2D_Release(texture);
        return E_OUTOFMEMORY;
    }

    object->IMFMediaBuffer_iface.lpVtbl = &dxgi_surface_1d_buffer_vtbl;
    object->IMF2DBuffer2_iface.lpVtbl = &dxgi_surface_buffer_vtbl;
    object->IMFDXGIBuffer_iface.lpVtbl = &dxgi_buffer_vtbl;
    object->refcount = 1;
    InitializeCriticalSection(&object->cs);
    object->texture = texture;
    object->sub_resource_idx = sub_resource_idx;

    MFGetPlaneSize(format, desc.Width, desc.Height, &object->plane_size);
    object->fourcc = format;
    object->width = stride;
    object->height = desc.Height;

    if (FAILED(hr = init_attributes_object(&object->attributes, 0)))
    {
        IMFMediaBuffer_Release(&object->IMFMediaBuffer_iface);
        return hr;
    }

    *buffer = &object->IMFMediaBuffer_iface;

    return S_OK;
}

/***********************************************************************
 *      MFCreateDXGISurfaceBuffer (mfplat.@)
 */
HRESULT WINAPI MFCreateDXGISurfaceBuffer(REFIID riid, IUnknown *surface, UINT subresource, BOOL bottom_up,
        IMFMediaBuffer **buffer)
{
    TRACE("%s, %p, %u, %d, %p.\n", debugstr_guid(riid), surface, subresource, bottom_up, buffer);

    if (!IsEqualIID(riid, &IID_ID3D11Texture2D))
        return E_INVALIDARG;

    return create_dxgi_surface_buffer(surface, subresource, bottom_up, buffer);
}
