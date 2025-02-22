/*
 * Copyright 2009 Henri Verbeet for CodeWeavers
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
 *
 */

#include "dxgi_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(dxgi);

/* Inner IUnknown methods */

static inline struct dxgi_resource *impl_from_IUnknown(IUnknown *iface)
{
    return CONTAINING_RECORD(iface, struct dxgi_resource, IUnknown_iface);
}

static HRESULT STDMETHODCALLTYPE dxgi_resource_inner_QueryInterface(IUnknown *iface, REFIID riid, void **out)
{
    struct dxgi_resource *resource = impl_from_IUnknown(iface);

    TRACE("iface %p, riid %s, out %p.\n", iface, debugstr_guid(riid), out);

    if (IsEqualGUID(riid, &IID_IDXGIKeyedMutex))
    {
        IDXGIKeyedMutex_AddRef(&resource->IDXGIKeyedMutex_iface);
        *out = &resource->IDXGIKeyedMutex_iface;
        return S_OK;
    }
    if ((IsEqualGUID(riid, &IID_IDXGISurface1)
            || IsEqualGUID(riid, &IID_IDXGISurface)) && resource->IDXGISurface1_iface.lpVtbl != NULL)
    {
        IDXGISurface1_AddRef(&resource->IDXGISurface1_iface);
        *out = &resource->IDXGISurface1_iface;
        return S_OK;
    }
    if (IsEqualGUID(riid, &IID_IDXGIResource1)
            || IsEqualGUID(riid, &IID_IDXGIResource)
            || IsEqualGUID(riid, &IID_IDXGIDeviceSubObject)
            || IsEqualGUID(riid, &IID_IDXGIObject)
            || IsEqualGUID(riid, &IID_IUnknown))
    {
        IDXGIResource1_AddRef(&resource->IDXGIResource1_iface);
        *out = &resource->IDXGIResource1_iface;
        return S_OK;
    }

    WARN("%s not implemented, returning E_NOINTERFACE\n", debugstr_guid(riid));

    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE dxgi_resource_inner_AddRef(IUnknown *iface)
{
    struct dxgi_resource *resource = impl_from_IUnknown(iface);
    ULONG refcount = InterlockedIncrement(&resource->refcount);

    TRACE("%p increasing refcount to %lu.\n", resource, refcount);

    return refcount;
}

static ULONG STDMETHODCALLTYPE dxgi_resource_inner_Release(IUnknown *iface)
{
    struct dxgi_resource *resource = impl_from_IUnknown(iface);
    ULONG refcount = InterlockedDecrement(&resource->refcount);

    TRACE("%p decreasing refcount to %lu.\n", resource, refcount);

    if (!refcount)
    {
        wined3d_private_store_cleanup(&resource->private_store);
        free(resource);
    }

    return refcount;
}

static inline struct dxgi_resource *impl_from_IDXGISurface1(IDXGISurface1 *iface)
{
    return CONTAINING_RECORD(iface, struct dxgi_resource, IDXGISurface1_iface);
}

/* IUnknown methods */

static HRESULT STDMETHODCALLTYPE dxgi_surface_QueryInterface(IDXGISurface1 *iface, REFIID riid,
        void **object)
{
    struct dxgi_resource *resource = impl_from_IDXGISurface1(iface);
    return IDXGIResource1_QueryInterface(&resource->IDXGIResource1_iface, riid, object);
}

static ULONG STDMETHODCALLTYPE dxgi_surface_AddRef(IDXGISurface1 *iface)
{
    struct dxgi_resource *resource = impl_from_IDXGISurface1(iface);
    return IDXGIResource1_AddRef(&resource->IDXGIResource1_iface);
}

static ULONG STDMETHODCALLTYPE dxgi_surface_Release(IDXGISurface1 *iface)
{
    struct dxgi_resource *resource = impl_from_IDXGISurface1(iface);
    return IDXGIResource1_Release(&resource->IDXGIResource1_iface);
}

/* IDXGIObject methods */

static HRESULT STDMETHODCALLTYPE dxgi_surface_SetPrivateData(IDXGISurface1 *iface,
        REFGUID guid, UINT data_size, const void *data)
{
    struct dxgi_resource *resource = impl_from_IDXGISurface1(iface);
    return IDXGIResource1_SetPrivateData(&resource->IDXGIResource1_iface, guid, data_size, data);
}

static HRESULT STDMETHODCALLTYPE dxgi_surface_SetPrivateDataInterface(IDXGISurface1 *iface,
        REFGUID guid, const IUnknown *object)
{
    struct dxgi_resource *resource = impl_from_IDXGISurface1(iface);
    return IDXGIResource1_SetPrivateDataInterface(&resource->IDXGIResource1_iface, guid, object);
}

static HRESULT STDMETHODCALLTYPE dxgi_surface_GetPrivateData(IDXGISurface1 *iface,
        REFGUID guid, UINT *data_size, void *data)
{
    struct dxgi_resource *resource = impl_from_IDXGISurface1(iface);
    return IDXGIResource1_GetPrivateData(&resource->IDXGIResource1_iface, guid, data_size, data);
}

static HRESULT STDMETHODCALLTYPE dxgi_surface_GetParent(IDXGISurface1 *iface, REFIID riid, void **parent)
{
    struct dxgi_resource *resource = impl_from_IDXGISurface1(iface);
    return IDXGIResource1_GetParent(&resource->IDXGIResource1_iface, riid, parent);
}

/* IDXGIDeviceSubObject methods */

static HRESULT STDMETHODCALLTYPE dxgi_surface_GetDevice(IDXGISurface1 *iface, REFIID riid, void **device)
{
    struct dxgi_resource *resource = impl_from_IDXGISurface1(iface);
    return IDXGIResource1_GetDevice(&resource->IDXGIResource1_iface, riid, device);
}

/* IDXGISurface methods */
static HRESULT STDMETHODCALLTYPE dxgi_surface_GetDesc(IDXGISurface1 *iface, DXGI_SURFACE_DESC *desc)
{
    struct dxgi_resource *resource = impl_from_IDXGISurface1(iface);
    struct wined3d_resource_desc wined3d_desc;

    TRACE("iface %p, desc %p.\n", iface, desc);

    wined3d_mutex_lock();
    wined3d_resource_get_desc(resource->wined3d_resource, &wined3d_desc);
    wined3d_mutex_unlock();
    desc->Width = wined3d_desc.width;
    desc->Height = wined3d_desc.height;
    desc->Format = dxgi_format_from_wined3dformat(wined3d_desc.format);
    dxgi_sample_desc_from_wined3d(&desc->SampleDesc, wined3d_desc.multisample_type, wined3d_desc.multisample_quality);

    return S_OK;
}

static HRESULT STDMETHODCALLTYPE dxgi_surface_Map(IDXGISurface1 *iface, DXGI_MAPPED_RECT *mapped_rect, UINT flags)
{
    struct dxgi_resource *resource = impl_from_IDXGISurface1(iface);
    struct wined3d_map_desc wined3d_map_desc;
    DWORD wined3d_map_flags = 0;
    HRESULT hr;

    TRACE("iface %p, mapped_rect %p, flags %#x.\n", iface, mapped_rect, flags);

    if (flags & DXGI_MAP_READ)
        wined3d_map_flags |= WINED3D_MAP_READ;
    if (flags & DXGI_MAP_WRITE)
        wined3d_map_flags |= WINED3D_MAP_WRITE;
    if (flags & DXGI_MAP_DISCARD)
        wined3d_map_flags |= WINED3D_MAP_DISCARD;

    if (SUCCEEDED(hr = wined3d_resource_map(resource->wined3d_resource, 0, &wined3d_map_desc, NULL, wined3d_map_flags)))
    {
        mapped_rect->Pitch = wined3d_map_desc.row_pitch;
        mapped_rect->pBits = wined3d_map_desc.data;
    }

    return hr;
}

static HRESULT STDMETHODCALLTYPE dxgi_surface_Unmap(IDXGISurface1 *iface)
{
    struct dxgi_resource *resource = impl_from_IDXGISurface1(iface);

    TRACE("iface %p.\n", iface);
    wined3d_resource_unmap(resource->wined3d_resource, 0);
    return S_OK;
}

/* IDXGISurface1 methods */
static HRESULT STDMETHODCALLTYPE dxgi_surface_GetDC(IDXGISurface1 *iface, BOOL discard, HDC *hdc)
{
    struct dxgi_resource *resource = impl_from_IDXGISurface1(iface);
    HRESULT hr;

    FIXME("iface %p, discard %d, hdc %p semi-stub!\n", iface, discard, hdc);

    if (!hdc)
        return E_INVALIDARG;

    wined3d_mutex_lock();
    hr = wined3d_texture_get_dc(wined3d_texture_from_resource(resource->wined3d_resource), 0, hdc);
    wined3d_mutex_unlock();

    if (SUCCEEDED(hr))
       resource->dc = *hdc;

    return hr;
}

static HRESULT STDMETHODCALLTYPE dxgi_surface_ReleaseDC(IDXGISurface1 *iface, RECT *dirty_rect)
{
    struct dxgi_resource *resource = impl_from_IDXGISurface1(iface);
    HRESULT hr;

    TRACE("iface %p, rect %s\n", iface, wine_dbgstr_rect(dirty_rect));

    if (!IsRectEmpty(dirty_rect))
        FIXME("dirty rectangle is ignored.\n");

    wined3d_mutex_lock();
    hr = wined3d_texture_release_dc(wined3d_texture_from_resource(resource->wined3d_resource), 0, resource->dc);
    wined3d_mutex_unlock();

    return hr;
}

static const struct IDXGISurface1Vtbl dxgi_surface_vtbl =
{
    /* IUnknown methods */
    dxgi_surface_QueryInterface,
    dxgi_surface_AddRef,
    dxgi_surface_Release,
    /* IDXGIObject methods */
    dxgi_surface_SetPrivateData,
    dxgi_surface_SetPrivateDataInterface,
    dxgi_surface_GetPrivateData,
    dxgi_surface_GetParent,
    /* IDXGIDeviceSubObject methods */
    dxgi_surface_GetDevice,
    /* IDXGISurface methods */
    dxgi_surface_GetDesc,
    dxgi_surface_Map,
    dxgi_surface_Unmap,
    /* IDXGISurface1 methods */
    dxgi_surface_GetDC,
    dxgi_surface_ReleaseDC,
};

static inline struct dxgi_resource *impl_from_IDXGIResource1(IDXGIResource1 *iface)
{
    return CONTAINING_RECORD(iface, struct dxgi_resource, IDXGIResource1_iface);
}

/* IUnknown methods */

static HRESULT STDMETHODCALLTYPE dxgi_resource_QueryInterface(IDXGIResource1 *iface, REFIID riid,
        void **object)
{
    struct dxgi_resource *resource = impl_from_IDXGIResource1(iface);
    TRACE("Forwarding to outer IUnknown\n");
    return IUnknown_QueryInterface(resource->outer_unknown, riid, object);
}

static ULONG STDMETHODCALLTYPE dxgi_resource_AddRef(IDXGIResource1 *iface)
{
    struct dxgi_resource *resource = impl_from_IDXGIResource1(iface);
    TRACE("Forwarding to outer IUnknown\n");
    return IUnknown_AddRef(resource->outer_unknown);
}

static ULONG STDMETHODCALLTYPE dxgi_resource_Release(IDXGIResource1 *iface)
{
    struct dxgi_resource *resource = impl_from_IDXGIResource1(iface);
    TRACE("Forwarding to outer IUnknown\n");
    return IUnknown_Release(resource->outer_unknown);
}

/* IDXGIObject methods */

static HRESULT STDMETHODCALLTYPE dxgi_resource_SetPrivateData(IDXGIResource1 *iface,
        REFGUID guid, UINT data_size, const void *data)
{
    struct dxgi_resource *resource = impl_from_IDXGIResource1(iface);

    TRACE("iface %p, guid %s, data_size %u, data %p.\n", iface, debugstr_guid(guid), data_size, data);

    return dxgi_set_private_data(&resource->private_store, guid, data_size, data);
}

static HRESULT STDMETHODCALLTYPE dxgi_resource_SetPrivateDataInterface(IDXGIResource1 *iface,
        REFGUID guid, const IUnknown *object)
{
    struct dxgi_resource *resource = impl_from_IDXGIResource1(iface);

    TRACE("iface %p, guid %s, object %p.\n", iface, debugstr_guid(guid), object);

    return dxgi_set_private_data_interface(&resource->private_store, guid, object);
}

static HRESULT STDMETHODCALLTYPE dxgi_resource_GetPrivateData(IDXGIResource1 *iface,
        REFGUID guid, UINT *data_size, void *data)
{
    struct dxgi_resource *resource = impl_from_IDXGIResource1(iface);

    TRACE("iface %p, guid %s, data_size %p, data %p.\n", iface, debugstr_guid(guid), data_size, data);

    return dxgi_get_private_data(&resource->private_store, guid, data_size, data);
}

static HRESULT STDMETHODCALLTYPE dxgi_resource_GetParent(IDXGIResource1 *iface, REFIID riid, void **parent)
{
    struct dxgi_resource *resource = impl_from_IDXGIResource1(iface);

    TRACE("iface %p, riid %s, parent %p.\n", iface, debugstr_guid(riid), parent);

    return IDXGIDevice_QueryInterface(resource->device, riid, parent);
}

/* IDXGIDeviceSubObject methods */

static HRESULT STDMETHODCALLTYPE dxgi_resource_GetDevice(IDXGIResource1 *iface, REFIID riid, void **device)
{
    struct dxgi_resource *resource = impl_from_IDXGIResource1(iface);

    TRACE("iface %p, riid %s, device %p.\n", iface, debugstr_guid(riid), device);

    return IDXGIDevice_QueryInterface(resource->device, riid, device);
}

/* IDXGIResource methods */

static HRESULT STDMETHODCALLTYPE dxgi_resource_GetSharedHandle(IDXGIResource1 *iface, HANDLE *shared_handle)
{
    struct dxgi_resource *resource = impl_from_IDXGIResource1(iface);
    FIXME("iface %p, shared_handle %p stub!\n", iface, shared_handle);
    *shared_handle = resource->outer_unknown;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE dxgi_resource_GetUsage(IDXGIResource1 *iface, DXGI_USAGE *usage)
{
    struct dxgi_resource *resource = impl_from_IDXGIResource1(iface);
    struct wined3d_resource_desc resource_desc;

    TRACE("iface %p, usage %p.\n", iface, usage);

    wined3d_resource_get_desc(resource->wined3d_resource, &resource_desc);

    *usage = dxgi_usage_from_wined3d_bind_flags(resource_desc.bind_flags);

    if (resource_desc.resource_type != WINED3D_RTYPE_BUFFER)
    {
        struct wined3d_texture *texture = wined3d_texture_from_resource(resource->wined3d_resource);
        struct wined3d_swapchain_desc swapchain_desc;
        struct wined3d_swapchain *swapchain;

        if ((swapchain = wined3d_texture_get_swapchain(texture)))
        {
            *usage |= DXGI_USAGE_BACK_BUFFER;

            wined3d_swapchain_get_desc(swapchain, &swapchain_desc);
            if (swapchain_desc.swap_effect == WINED3D_SWAP_EFFECT_DISCARD)
                *usage |= DXGI_USAGE_DISCARD_ON_PRESENT;

            if (wined3d_swapchain_get_back_buffer(swapchain, 0) != texture)
                *usage |= DXGI_USAGE_READ_ONLY;
        }
    }

    return S_OK;
}

static HRESULT STDMETHODCALLTYPE dxgi_resource_SetEvictionPriority(IDXGIResource1 *iface, UINT eviction_priority)
{
    FIXME("iface %p, eviction_priority %u stub!\n", iface, eviction_priority);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE dxgi_resource_GetEvictionPriority(IDXGIResource1 *iface, UINT *eviction_priority)
{
    FIXME("iface %p, eviction_priority %p stub!\n", iface, eviction_priority);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE dxgi_resource_CreateSubresourceSurface(IDXGIResource1 *iface, UINT index, IDXGISurface2 **surface)
{
    FIXME("stub!\n");
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE dxgi_resource_CreateSharedHandle(IDXGIResource1 *iface, const SECURITY_ATTRIBUTES *attributes,
        DWORD access, const WCHAR *name, HANDLE *handle)
{
    struct dxgi_resource *resource = impl_from_IDXGIResource1(iface);
    FIXME("stub!\n");
    *handle = resource->outer_unknown;
    return S_OK;
}

static const struct IDXGIResource1Vtbl dxgi_resource_vtbl =
{
    /* IUnknown methods */
    dxgi_resource_QueryInterface,
    dxgi_resource_AddRef,
    dxgi_resource_Release,
    /* IDXGIObject methods */
    dxgi_resource_SetPrivateData,
    dxgi_resource_SetPrivateDataInterface,
    dxgi_resource_GetPrivateData,
    dxgi_resource_GetParent,
    /* IDXGIDeviceSubObject methods */
    dxgi_resource_GetDevice,
    /* IDXGIResource methods */
    dxgi_resource_GetSharedHandle,
    dxgi_resource_GetUsage,
    dxgi_resource_SetEvictionPriority,
    dxgi_resource_GetEvictionPriority,
    /* IDXGIResource1 methods */
    dxgi_resource_CreateSubresourceSurface,
    dxgi_resource_CreateSharedHandle,
};

static inline struct dxgi_resource *impl_from_IDXGIKeyedMutex(IDXGIKeyedMutex *iface)
{
    return CONTAINING_RECORD(iface, struct dxgi_resource, IDXGIKeyedMutex_iface);
}

/* IUnknown methods */

static HRESULT STDMETHODCALLTYPE dxgi_keyed_mutex_QueryInterface(IDXGIKeyedMutex *iface, REFIID riid,
        void **object)
{
    struct dxgi_resource *resource = impl_from_IDXGIKeyedMutex(iface);
    return IDXGIResource1_QueryInterface(&resource->IDXGIResource1_iface, riid, object);
}

static ULONG STDMETHODCALLTYPE dxgi_keyed_mutex_AddRef(IDXGIKeyedMutex *iface)
{
    struct dxgi_resource *resource = impl_from_IDXGIKeyedMutex(iface);
    return IDXGIResource1_AddRef(&resource->IDXGIResource1_iface);
}

static ULONG STDMETHODCALLTYPE dxgi_keyed_mutex_Release(IDXGIKeyedMutex *iface)
{
    struct dxgi_resource *resource = impl_from_IDXGIKeyedMutex(iface);
    return IDXGIResource1_Release(&resource->IDXGIResource1_iface);
}

/* IDXGIObject methods */

static HRESULT STDMETHODCALLTYPE dxgi_keyed_mutex_SetPrivateData(IDXGIKeyedMutex *iface,
        REFGUID guid, UINT data_size, const void *data)
{
    struct dxgi_resource *resource = impl_from_IDXGIKeyedMutex(iface);
    return IDXGIResource1_SetPrivateData(&resource->IDXGIResource1_iface, guid, data_size, data);
}

static HRESULT STDMETHODCALLTYPE dxgi_keyed_mutex_SetPrivateDataInterface(IDXGIKeyedMutex *iface,
        REFGUID guid, const IUnknown *object)
{
    struct dxgi_resource *resource = impl_from_IDXGIKeyedMutex(iface);
    return IDXGIResource1_SetPrivateDataInterface(&resource->IDXGIResource1_iface, guid, object);
}

static HRESULT STDMETHODCALLTYPE dxgi_keyed_mutex_GetPrivateData(IDXGIKeyedMutex *iface,
        REFGUID guid, UINT *data_size, void *data)
{
    struct dxgi_resource *resource = impl_from_IDXGIKeyedMutex(iface);
    return IDXGIResource1_GetPrivateData(&resource->IDXGIResource1_iface, guid, data_size, data);
}

static HRESULT STDMETHODCALLTYPE dxgi_keyed_mutex_GetParent(IDXGIKeyedMutex *iface, REFIID riid, void **parent)
{
    struct dxgi_resource *resource = impl_from_IDXGIKeyedMutex(iface);
    return IDXGIResource1_GetParent(&resource->IDXGIResource1_iface, riid, parent);
}

/* IDXGIDeviceSubObject methods */

static HRESULT STDMETHODCALLTYPE dxgi_keyed_mutex_GetDevice(IDXGIKeyedMutex *iface, REFIID riid, void **device)
{
    struct dxgi_resource *resource = impl_from_IDXGIKeyedMutex(iface);
    return IDXGIResource1_GetDevice(&resource->IDXGIResource1_iface, riid, device);
}

/* IDXGIKeyedMutex methods */

static HRESULT STDMETHODCALLTYPE dxgi_keyed_mutex_AcquireSync(IDXGIKeyedMutex *iface, UINT64 key, DWORD milliseconds)
{
    FIXME("stub!\n");
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE dxgi_keyed_mutex_ReleaseSync(IDXGIKeyedMutex *iface, UINT64 key)
{
    FIXME("stub!\n");
    return E_NOTIMPL;
}

static const struct IDXGIKeyedMutexVtbl dxgi_keyed_mutex_vtbl =
{
    /* IUnknown methods */
    dxgi_keyed_mutex_QueryInterface,
    dxgi_keyed_mutex_AddRef,
    dxgi_keyed_mutex_Release,
    /* IDXGIObject methods */
    dxgi_keyed_mutex_SetPrivateData,
    dxgi_keyed_mutex_SetPrivateDataInterface,
    dxgi_keyed_mutex_GetPrivateData,
    dxgi_keyed_mutex_GetParent,
    /* IDXGIDeviceSubObject methods */
    dxgi_keyed_mutex_GetDevice,
    /* IDXGIKeyedMutex methods */
    dxgi_keyed_mutex_AcquireSync,
    dxgi_keyed_mutex_ReleaseSync,
};

static const struct IUnknownVtbl dxgi_resource_inner_unknown_vtbl =
{
    /* IUnknown methods */
    dxgi_resource_inner_QueryInterface,
    dxgi_resource_inner_AddRef,
    dxgi_resource_inner_Release,
};

HRESULT dxgi_resource_init(struct dxgi_resource *resource, IDXGIDevice *device,
        IUnknown *outer, BOOL needs_surface, struct wined3d_resource *wined3d_resource)
{
    struct wined3d_resource_desc desc;

    wined3d_resource_get_desc(wined3d_resource, &desc);
    if ((desc.resource_type == WINED3D_RTYPE_TEXTURE_1D || desc.resource_type == WINED3D_RTYPE_TEXTURE_2D)
            && needs_surface)
    {
        resource->IDXGISurface1_iface.lpVtbl = &dxgi_surface_vtbl;
    }
    else
        resource->IDXGISurface1_iface.lpVtbl = NULL;
    resource->IDXGIResource1_iface.lpVtbl = &dxgi_resource_vtbl;
    resource->IDXGIKeyedMutex_iface.lpVtbl = &dxgi_keyed_mutex_vtbl;
    resource->IUnknown_iface.lpVtbl = &dxgi_resource_inner_unknown_vtbl;
    resource->refcount = 1;
    wined3d_private_store_init(&resource->private_store);
    resource->outer_unknown = outer ? outer : &resource->IUnknown_iface;
    resource->device = device;
    resource->wined3d_resource = wined3d_resource;
    resource->dc = NULL;

    return S_OK;
}
