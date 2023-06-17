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

struct buffer
{
    IMFMediaBuffer IMFMediaBuffer_iface;
    LONG refcount;

    BYTE *data;
    DWORD max_length;
    DWORD current_length;
};

static inline struct buffer *impl_from_IMFMediaBuffer(IMFMediaBuffer *iface)
{
    return CONTAINING_RECORD(iface, struct buffer, IMFMediaBuffer_iface);
}

static HRESULT WINAPI media_buffer_QueryInterface(IMFMediaBuffer *iface, REFIID riid, void **out)
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

static ULONG WINAPI media_buffer_AddRef(IMFMediaBuffer *iface)
{
    struct buffer *buffer = impl_from_IMFMediaBuffer(iface);
    ULONG refcount = InterlockedIncrement(&buffer->refcount);

    TRACE("%p, refcount %lu.\n", buffer, refcount);

    return refcount;
}

static ULONG WINAPI media_buffer_Release(IMFMediaBuffer *iface)
{
    struct buffer *buffer = impl_from_IMFMediaBuffer(iface);
    ULONG refcount = InterlockedDecrement(&buffer->refcount);

    TRACE("%p, refcount %lu.\n", iface, refcount);

    if (!refcount)
        _aligned_free(buffer->data);

    return refcount;
}

static HRESULT WINAPI media_buffer_Lock(IMFMediaBuffer *iface, BYTE **data, DWORD *max_length, DWORD *current_length)
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

static HRESULT WINAPI media_buffer_Unlock(IMFMediaBuffer *iface)
{
    TRACE("%p.\n", iface);

    return S_OK;
}

static HRESULT WINAPI media_buffer_GetCurrentLength(IMFMediaBuffer *iface, DWORD *current_length)
{
    struct buffer *buffer = impl_from_IMFMediaBuffer(iface);

    TRACE("%p.\n", iface);

    if (!current_length)
        return E_INVALIDARG;

    *current_length = buffer->current_length;

    return S_OK;
}

static HRESULT WINAPI media_buffer_SetCurrentLength(IMFMediaBuffer *iface, DWORD current_length)
{
    struct buffer *buffer = impl_from_IMFMediaBuffer(iface);

    TRACE("%p, %lu.\n", iface, current_length);

    if (current_length > buffer->max_length)
        return E_INVALIDARG;

    buffer->current_length = current_length;

    return S_OK;
}

static HRESULT WINAPI media_buffer_GetMaxLength(IMFMediaBuffer *iface, DWORD *max_length)
{
    struct buffer *buffer = impl_from_IMFMediaBuffer(iface);

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

static struct buffer *buffer_alloc(DWORD max_length, DWORD alignment)
{
    SIZE_T alloc_size = max_length + sizeof(struct buffer);
    struct buffer *buffer;
    BYTE *data;

    if (!(data = _aligned_malloc(alloc_size, alignment)))
        return NULL;

    buffer = (struct buffer *)(data + max_length);
    buffer->IMFMediaBuffer_iface.lpVtbl = &media_buffer_vtbl;
    buffer->refcount = 1;
    buffer->data = data;
    buffer->max_length = max_length;
    buffer->current_length = 0;

    return buffer;
}

static HRESULT create_1d_buffer(DWORD max_length, DWORD alignment, IMFMediaBuffer **out)
{
    struct buffer *buffer;

    if (!out)
        return E_POINTER;
    *out = NULL;

    if (!(buffer = buffer_alloc(max_length, alignment)))
        return E_OUTOFMEMORY;

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

    *out = &buffer->IMFMediaBuffer_iface;
    TRACE("Created buffer %p\n", *out);
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
