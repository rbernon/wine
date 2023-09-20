/*
 * Copyright (C) 2003-2004 Rok Mandeljc
 * Copyright 2023 Rémi Bernon for CodeWeavers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "dmloader_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmloader);

struct loader_stream
{
    IStream IStream_iface;
    IDirectMusicGetLoader IDirectMusicGetLoader_iface;
    LONG ref;

    IStream *stream;
    IDirectMusicLoader *loader;
};

static struct loader_stream *impl_from_IStream(IStream *iface)
{
    return CONTAINING_RECORD(iface, struct loader_stream, IStream_iface);
}

static HRESULT WINAPI loader_stream_QueryInterface(IStream *iface, REFIID riid, void **ret_iface)
{
    struct loader_stream *This = impl_from_IStream(iface);

    TRACE("(%p, %s, %p)\n", This, debugstr_dmguid(riid), ret_iface);

    if (IsEqualGUID(riid, &IID_IUnknown)
            || IsEqualGUID(riid, &IID_IStream))
    {
        IStream_AddRef(&This->IStream_iface);
        *ret_iface = &This->IStream_iface;
        return S_OK;
    }

    if (IsEqualGUID(riid, &IID_IDirectMusicGetLoader))
    {
        IDirectMusicGetLoader_AddRef(&This->IDirectMusicGetLoader_iface);
        *ret_iface = &This->IDirectMusicGetLoader_iface;
        return S_OK;
    }

    WARN("(%p, %s, %p): not found\n", iface, debugstr_dmguid(riid), ret_iface);
    *ret_iface = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI loader_stream_AddRef(IStream *iface)
{
    struct loader_stream *This = impl_from_IStream(iface);
    ULONG ref = InterlockedIncrement(&This->ref);
    TRACE("(%p): new ref = %lu\n", This, ref);
    return ref;
}

static ULONG WINAPI loader_stream_Release(IStream *iface)
{
    struct loader_stream *This = impl_from_IStream(iface);
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p): new ref = %lu\n", This, ref);

    if (!ref)
    {
        IDirectMusicLoader_Release(This->loader);
        IStream_Release(This->stream);
        free(This);
    }

    return ref;
}

static HRESULT WINAPI loader_stream_Read(IStream *iface, void *data, ULONG size, ULONG *ret_size)
{
    struct loader_stream *This = impl_from_IStream(iface);
    TRACE("(%p, %p, %#lx, %p)\n", This, data, size, ret_size);
    return IStream_Read(This->stream, data, size, ret_size);
}

static HRESULT WINAPI loader_stream_Write(IStream *iface, const void *data, ULONG size, ULONG *ret_size)
{
    struct loader_stream *This = impl_from_IStream(iface);
    FIXME("(%p): stub\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI loader_stream_Seek(IStream *iface, LARGE_INTEGER offset, DWORD method, ULARGE_INTEGER *ret_offset)
{
    struct loader_stream *This = impl_from_IStream(iface);
    TRACE("(%p, %I64d, %#lx, %p)\n", This, offset.QuadPart, method, ret_offset);
    return IStream_Seek(This->stream, offset, method, ret_offset);
}

static HRESULT WINAPI loader_stream_SetSize(IStream *iface, ULARGE_INTEGER size)
{
    struct loader_stream *This = impl_from_IStream(iface);
    FIXME("(%p): stub\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI loader_stream_CopyTo(IStream *iface, IStream *dest, ULARGE_INTEGER size,
        ULARGE_INTEGER *read_size, ULARGE_INTEGER *write_size)
{
    struct loader_stream *This = impl_from_IStream(iface);
    FIXME("(%p): stub\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI loader_stream_Commit(IStream *iface, DWORD flags)
{
    struct loader_stream *This = impl_from_IStream(iface);
    FIXME("(%p): stub\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI loader_stream_Revert(IStream *iface)
{
    struct loader_stream *This = impl_from_IStream(iface);
    FIXME("(%p): stub\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI loader_stream_LockRegion(IStream *iface, ULARGE_INTEGER offset, ULARGE_INTEGER size, DWORD type)
{
    struct loader_stream *This = impl_from_IStream(iface);
    FIXME("(%p): stub\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI loader_stream_UnlockRegion(IStream *iface, ULARGE_INTEGER offset,
        ULARGE_INTEGER size, DWORD type)
{
    struct loader_stream *This = impl_from_IStream(iface);
    FIXME("(%p): stub\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI loader_stream_Stat(IStream *iface, STATSTG *stat, DWORD flags)
{
    struct loader_stream *This = impl_from_IStream(iface);
    FIXME("(%p): stub\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI loader_stream_Clone(IStream *iface, IStream **ret_iface)
{
    struct loader_stream *This = impl_from_IStream(iface);
    IStream *stream;
    HRESULT hr;

    TRACE("(%p, %p)\n", This, ret_iface);

    if (SUCCEEDED(hr = IStream_Clone(This->stream, &stream)))
    {
        hr = loader_stream_create(This->loader, stream, ret_iface);
        IStream_Release(stream);
    }

    return hr;
}

static const IStreamVtbl loader_stream_vtbl =
{
    loader_stream_QueryInterface,
    loader_stream_AddRef,
    loader_stream_Release,
    loader_stream_Read,
    loader_stream_Write,
    loader_stream_Seek,
    loader_stream_SetSize,
    loader_stream_CopyTo,
    loader_stream_Commit,
    loader_stream_Revert,
    loader_stream_LockRegion,
    loader_stream_UnlockRegion,
    loader_stream_Stat,
    loader_stream_Clone,
};

static struct loader_stream *impl_from_IDirectMusicGetLoader(IDirectMusicGetLoader *iface)
{
    return CONTAINING_RECORD(iface, struct loader_stream, IDirectMusicGetLoader_iface);
}

static HRESULT WINAPI loader_stream_getter_QueryInterface(IDirectMusicGetLoader *iface, REFIID iid, void **out)
{
    struct loader_stream *This = impl_from_IDirectMusicGetLoader(iface);
    return IStream_QueryInterface(&This->IStream_iface, iid, out);
}

static ULONG WINAPI loader_stream_getter_AddRef(IDirectMusicGetLoader *iface)
{
    struct loader_stream *This = impl_from_IDirectMusicGetLoader(iface);
    return IStream_AddRef(&This->IStream_iface);
}

static ULONG WINAPI loader_stream_getter_Release(IDirectMusicGetLoader *iface)
{
    struct loader_stream *This = impl_from_IDirectMusicGetLoader(iface);
    return IStream_Release(&This->IStream_iface);
}

static HRESULT WINAPI loader_stream_getter_GetLoader(IDirectMusicGetLoader *iface, IDirectMusicLoader **ret_loader)
{
    struct loader_stream *This = impl_from_IDirectMusicGetLoader(iface);

    TRACE("(%p, %p)\n", This, ret_loader);

    *ret_loader = This->loader;
    IDirectMusicLoader_AddRef(This->loader);
    return S_OK;
}

static const IDirectMusicGetLoaderVtbl loader_stream_getter_vtbl =
{
    loader_stream_getter_QueryInterface,
    loader_stream_getter_AddRef,
    loader_stream_getter_Release,
    loader_stream_getter_GetLoader,
};

HRESULT loader_stream_create(IDirectMusicLoader *loader, IStream *stream,
        IStream **ret_iface)
{
    struct loader_stream *obj;

    *ret_iface = NULL;
    if (!(obj = calloc(1, sizeof(*obj)))) return E_OUTOFMEMORY;
    obj->IStream_iface.lpVtbl = &loader_stream_vtbl;
    obj->IDirectMusicGetLoader_iface.lpVtbl = &loader_stream_getter_vtbl;
    obj->ref = 1;

    obj->stream = stream;
    IStream_AddRef(stream);
    obj->loader = loader;
    IDirectMusicLoader_AddRef(loader);

    *ret_iface = &obj->IStream_iface;
    return S_OK;
}

struct file_stream
{
    IStream IStream_iface;
    LONG ref;

    WCHAR path[MAX_PATH];
    HANDLE file;
};

static struct file_stream *file_stream_from_IStream(IStream *iface)
{
    return CONTAINING_RECORD(iface, struct file_stream, IStream_iface);
}

static HRESULT WINAPI file_stream_QueryInterface(IStream *iface, REFIID riid, void **ret_iface)
{
    struct file_stream *This = file_stream_from_IStream(iface);

    TRACE("(%p, %s, %p)\n", This, debugstr_dmguid(riid), ret_iface);

    if (IsEqualGUID(riid, &IID_IUnknown)
            || IsEqualGUID(riid, &IID_IStream))
    {
        IStream_AddRef(&This->IStream_iface);
        *ret_iface = &This->IStream_iface;
        return S_OK;
    }

    WARN("(%p, %s, %p): not found\n", iface, debugstr_dmguid(riid), ret_iface);
    *ret_iface = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI file_stream_AddRef(IStream *iface)
{
    struct file_stream *This = file_stream_from_IStream(iface);
    ULONG ref = InterlockedIncrement(&This->ref);
    TRACE("(%p): new ref = %lu\n", This, ref);
    return ref;
}

static ULONG WINAPI file_stream_Release(IStream *iface)
{
    struct file_stream *This = file_stream_from_IStream(iface);
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p): new ref = %lu\n", This, ref);

    if (!ref)
    {
        CloseHandle(This->file);
        free(This);
    }

    return ref;
}

static HRESULT WINAPI file_stream_Read(IStream *iface, void *data, ULONG size, ULONG *ret_size)
{
    struct file_stream *This = file_stream_from_IStream(iface);
    DWORD dummy;

    TRACE("(%p, %p, %#lx, %p)\n", This, data, size, ret_size);

    if (!ret_size) ret_size = &dummy;
    if (!ReadFile(This->file, data, size, ret_size, NULL)) return HRESULT_FROM_WIN32(GetLastError());
    return *ret_size == size ? S_OK : S_FALSE;
}

static HRESULT WINAPI file_stream_Write(IStream *iface, const void *data, ULONG size, ULONG *ret_size)
{
    struct file_stream *This = file_stream_from_IStream(iface);
    FIXME("(%p): stub\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI file_stream_Seek(IStream *iface, LARGE_INTEGER offset, DWORD method, ULARGE_INTEGER *ret_offset)
{
    struct file_stream *This = file_stream_from_IStream(iface);
    DWORD position;

    TRACE("(%p, %I64d, %#lx, %p)\n", This, offset.QuadPart, method, ret_offset);

    position = SetFilePointer(This->file, offset.u.LowPart, NULL, method);
    if (position == INVALID_SET_FILE_POINTER) return HRESULT_FROM_WIN32(GetLastError());
    if (ret_offset) ret_offset->QuadPart = position;
    return S_OK;
}

static HRESULT WINAPI file_stream_SetSize(IStream *iface, ULARGE_INTEGER size)
{
    struct file_stream *This = file_stream_from_IStream(iface);
    FIXME("(%p): stub\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI file_stream_CopyTo(IStream *iface, IStream *dest, ULARGE_INTEGER size,
        ULARGE_INTEGER *read_size, ULARGE_INTEGER *write_size)
{
    struct file_stream *This = file_stream_from_IStream(iface);
    FIXME("(%p): stub\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI file_stream_Commit(IStream *iface, DWORD flags)
{
    struct file_stream *This = file_stream_from_IStream(iface);
    FIXME("(%p): stub\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI file_stream_Revert(IStream *iface)
{
    struct file_stream *This = file_stream_from_IStream(iface);
    FIXME("(%p): stub\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI file_stream_LockRegion(IStream *iface, ULARGE_INTEGER offset, ULARGE_INTEGER size, DWORD type)
{
    struct file_stream *This = file_stream_from_IStream(iface);
    FIXME("(%p): stub\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI file_stream_UnlockRegion(IStream *iface, ULARGE_INTEGER offset,
        ULARGE_INTEGER size, DWORD type)
{
    struct file_stream *This = file_stream_from_IStream(iface);
    FIXME("(%p): stub\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI file_stream_Stat(IStream *iface, STATSTG *stat, DWORD flags)
{
    struct file_stream *This = file_stream_from_IStream(iface);
    FIXME("(%p): stub\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI file_stream_Clone(IStream *iface, IStream **ret_iface)
{
    struct file_stream *This = file_stream_from_IStream(iface);
    HRESULT hr;

    TRACE("(%p, %p)\n", This, ret_iface);

    if (SUCCEEDED(hr = file_stream_create(This->path, ret_iface)))
    {
        LARGE_INTEGER position = {0};
        position.LowPart = SetFilePointer(This->file, 0, NULL, FILE_CURRENT);
        hr = IStream_Seek(*ret_iface, position, STREAM_SEEK_SET, NULL);
    }

    return hr;
}

static const IStreamVtbl file_stream_vtbl =
{
    file_stream_QueryInterface,
    file_stream_AddRef,
    file_stream_Release,
    file_stream_Read,
    file_stream_Write,
    file_stream_Seek,
    file_stream_SetSize,
    file_stream_CopyTo,
    file_stream_Commit,
    file_stream_Revert,
    file_stream_LockRegion,
    file_stream_UnlockRegion,
    file_stream_Stat,
    file_stream_Clone,
};

HRESULT file_stream_create(const WCHAR *path, IStream **ret_iface)
{
    struct file_stream *stream;

    *ret_iface = NULL;
    if (!(stream = calloc(1, sizeof(*stream)))) return E_OUTOFMEMORY;
    stream->IStream_iface.lpVtbl = &file_stream_vtbl;
    stream->ref = 1;

    wcscpy(stream->path, path);
    stream->file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (stream->file == INVALID_HANDLE_VALUE)
    {
        free(stream);
        return DMUS_E_LOADER_FAILEDOPEN;
    }

    *ret_iface = &stream->IStream_iface;
    return S_OK;
}
