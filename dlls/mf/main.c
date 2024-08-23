/*
 * Copyright (C) 2015 Austin English
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

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "mfidl.h"
#include "rpcproxy.h"

#include "mf_private.h"

#include "wine/debug.h"
#include "wine/list.h"
#include "wine/mfinternal.h"

WINE_DEFAULT_DEBUG_CHANNEL(mfplat);

extern const GUID CLSID_FileSchemePlugin;

struct activate_object
{
    IMFActivate IMFActivate_iface;
    LONG refcount;
    IMFAttributes *attributes;
    IUnknown *object;
    const struct activate_funcs *funcs;
    void *context;
};

static struct activate_object *impl_from_IMFActivate(IMFActivate *iface)
{
    return CONTAINING_RECORD(iface, struct activate_object, IMFActivate_iface);
}

static HRESULT WINAPI activate_object_QueryInterface(IMFActivate *iface, REFIID riid, void **obj)
{
    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), obj);

    if (IsEqualIID(riid, &IID_IMFActivate) ||
            IsEqualIID(riid, &IID_IMFAttributes) ||
            IsEqualIID(riid, &IID_IUnknown))
    {
        *obj = iface;
        IMFActivate_AddRef(iface);
        return S_OK;
    }

    WARN("Unsupported %s.\n", debugstr_guid(riid));
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI activate_object_AddRef(IMFActivate *iface)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);
    ULONG refcount = InterlockedIncrement(&activate->refcount);

    TRACE("%p, refcount %lu.\n", iface, refcount);

    return refcount;
}

static ULONG WINAPI activate_object_Release(IMFActivate *iface)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);
    ULONG refcount = InterlockedDecrement(&activate->refcount);

    TRACE("%p, refcount %lu.\n", iface, refcount);

    if (!refcount)
    {
        if (activate->funcs->free_private)
            activate->funcs->free_private(activate->context);
        if (activate->object)
            IUnknown_Release(activate->object);
        IMFAttributes_Release(activate->attributes);
        free(activate);
    }

    return refcount;
}

static HRESULT WINAPI activate_object_GetItem(IMFActivate *iface, REFGUID key, PROPVARIANT *value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(key), value);

    return IMFAttributes_GetItem(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_GetItemType(IMFActivate *iface, REFGUID key, MF_ATTRIBUTE_TYPE *type)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(key), type);

    return IMFAttributes_GetItemType(activate->attributes, key, type);
}

static HRESULT WINAPI activate_object_CompareItem(IMFActivate *iface, REFGUID key, REFPROPVARIANT value, BOOL *result)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p, %p.\n", iface, debugstr_guid(key), value, result);

    return IMFAttributes_CompareItem(activate->attributes, key, value, result);
}

static HRESULT WINAPI activate_object_Compare(IMFActivate *iface, IMFAttributes *theirs, MF_ATTRIBUTES_MATCH_TYPE type,
        BOOL *result)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %p, %d, %p.\n", iface, theirs, type, result);

    return IMFAttributes_Compare(activate->attributes, theirs, type, result);
}

static HRESULT WINAPI activate_object_GetUINT32(IMFActivate *iface, REFGUID key, UINT32 *value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(key), value);

    return IMFAttributes_GetUINT32(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_GetUINT64(IMFActivate *iface, REFGUID key, UINT64 *value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(key), value);

    return IMFAttributes_GetUINT64(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_GetDouble(IMFActivate *iface, REFGUID key, double *value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(key), value);

    return IMFAttributes_GetDouble(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_GetGUID(IMFActivate *iface, REFGUID key, GUID *value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(key), value);

    return IMFAttributes_GetGUID(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_GetStringLength(IMFActivate *iface, REFGUID key, UINT32 *length)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(key), length);

    return IMFAttributes_GetStringLength(activate->attributes, key, length);
}

static HRESULT WINAPI activate_object_GetString(IMFActivate *iface, REFGUID key, WCHAR *value,
        UINT32 size, UINT32 *length)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p, %d, %p.\n", iface, debugstr_guid(key), value, size, length);

    return IMFAttributes_GetString(activate->attributes, key, value, size, length);
}

static HRESULT WINAPI activate_object_GetAllocatedString(IMFActivate *iface, REFGUID key,
        WCHAR **value, UINT32 *length)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p, %p.\n", iface, debugstr_guid(key), value, length);

    return IMFAttributes_GetAllocatedString(activate->attributes, key, value, length);
}

static HRESULT WINAPI activate_object_GetBlobSize(IMFActivate *iface, REFGUID key, UINT32 *size)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(key), size);

    return IMFAttributes_GetBlobSize(activate->attributes, key, size);
}

static HRESULT WINAPI activate_object_GetBlob(IMFActivate *iface, REFGUID key, UINT8 *buf,
        UINT32 bufsize, UINT32 *blobsize)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p, %d, %p.\n", iface, debugstr_guid(key), buf, bufsize, blobsize);

    return IMFAttributes_GetBlob(activate->attributes, key, buf, bufsize, blobsize);
}

static HRESULT WINAPI activate_object_GetAllocatedBlob(IMFActivate *iface, REFGUID key, UINT8 **buf, UINT32 *size)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p, %p.\n", iface, debugstr_guid(key), buf, size);

    return IMFAttributes_GetAllocatedBlob(activate->attributes, key, buf, size);
}

static HRESULT WINAPI activate_object_GetUnknown(IMFActivate *iface, REFGUID key, REFIID riid, void **ppv)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %s, %p.\n", iface, debugstr_guid(key), debugstr_guid(riid), ppv);

    return IMFAttributes_GetUnknown(activate->attributes, key, riid, ppv);
}

static HRESULT WINAPI activate_object_SetItem(IMFActivate *iface, REFGUID key, REFPROPVARIANT value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(key), value);

    return IMFAttributes_SetItem(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_DeleteItem(IMFActivate *iface, REFGUID key)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s.\n", iface, debugstr_guid(key));

    return IMFAttributes_DeleteItem(activate->attributes, key);
}

static HRESULT WINAPI activate_object_DeleteAllItems(IMFActivate *iface)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p.\n", iface);

    return IMFAttributes_DeleteAllItems(activate->attributes);
}

static HRESULT WINAPI activate_object_SetUINT32(IMFActivate *iface, REFGUID key, UINT32 value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %d.\n", iface, debugstr_guid(key), value);

    return IMFAttributes_SetUINT32(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_SetUINT64(IMFActivate *iface, REFGUID key, UINT64 value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %s.\n", iface, debugstr_guid(key), wine_dbgstr_longlong(value));

    return IMFAttributes_SetUINT64(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_SetDouble(IMFActivate *iface, REFGUID key, double value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %f.\n", iface, debugstr_guid(key), value);

    return IMFAttributes_SetDouble(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_SetGUID(IMFActivate *iface, REFGUID key, REFGUID value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %s.\n", iface, debugstr_guid(key), debugstr_guid(value));

    return IMFAttributes_SetGUID(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_SetString(IMFActivate *iface, REFGUID key, const WCHAR *value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %s.\n", iface, debugstr_guid(key), debugstr_w(value));

    return IMFAttributes_SetString(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_SetBlob(IMFActivate *iface, REFGUID key, const UINT8 *buf, UINT32 size)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p, %d.\n", iface, debugstr_guid(key), buf, size);

    return IMFAttributes_SetBlob(activate->attributes, key, buf, size);
}

static HRESULT WINAPI activate_object_SetUnknown(IMFActivate *iface, REFGUID key, IUnknown *unknown)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("(%p)->(%s, %p)\n", iface, debugstr_guid(key), unknown);

    return IMFAttributes_SetUnknown(activate->attributes, key, unknown);
}

static HRESULT WINAPI activate_object_LockStore(IMFActivate *iface)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p.\n", iface);

    return IMFAttributes_LockStore(activate->attributes);
}

static HRESULT WINAPI activate_object_UnlockStore(IMFActivate *iface)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p.\n", iface);

    return IMFAttributes_UnlockStore(activate->attributes);
}

static HRESULT WINAPI activate_object_GetCount(IMFActivate *iface, UINT32 *count)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %p.\n", iface, count);

    return IMFAttributes_GetCount(activate->attributes, count);
}

static HRESULT WINAPI activate_object_GetItemByIndex(IMFActivate *iface, UINT32 index, GUID *key, PROPVARIANT *value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %u, %p, %p.\n", iface, index, key, value);

    return IMFAttributes_GetItemByIndex(activate->attributes, index, key, value);
}

static HRESULT WINAPI activate_object_CopyAllItems(IMFActivate *iface, IMFAttributes *dest)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %p.\n", iface, dest);

    return IMFAttributes_CopyAllItems(activate->attributes, dest);
}

static HRESULT WINAPI activate_object_ActivateObject(IMFActivate *iface, REFIID riid, void **obj)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);
    IUnknown *object;
    HRESULT hr;

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), obj);

    if (!activate->object)
    {
        if (FAILED(hr = activate->funcs->create_object((IMFAttributes *)iface, activate->context, &object)))
            return hr;

        if (InterlockedCompareExchangePointer((void **)&activate->object, object, NULL))
            IUnknown_Release(object);
    }

    return IUnknown_QueryInterface(activate->object, riid, obj);
}

static HRESULT WINAPI activate_object_ShutdownObject(IMFActivate *iface)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);
    IUnknown *object;

    TRACE("%p.\n", iface);

    if ((object = InterlockedCompareExchangePointer((void **)&activate->object, NULL, activate->object)))
    {
        activate->funcs->shutdown_object(activate->context, object);
        IUnknown_Release(object);
    }

    return S_OK;
}

static HRESULT WINAPI activate_object_DetachObject(IMFActivate *iface)
{
    TRACE("%p.\n", iface);

    return E_NOTIMPL;
}

static const IMFActivateVtbl activate_object_vtbl =
{
    activate_object_QueryInterface,
    activate_object_AddRef,
    activate_object_Release,
    activate_object_GetItem,
    activate_object_GetItemType,
    activate_object_CompareItem,
    activate_object_Compare,
    activate_object_GetUINT32,
    activate_object_GetUINT64,
    activate_object_GetDouble,
    activate_object_GetGUID,
    activate_object_GetStringLength,
    activate_object_GetString,
    activate_object_GetAllocatedString,
    activate_object_GetBlobSize,
    activate_object_GetBlob,
    activate_object_GetAllocatedBlob,
    activate_object_GetUnknown,
    activate_object_SetItem,
    activate_object_DeleteItem,
    activate_object_DeleteAllItems,
    activate_object_SetUINT32,
    activate_object_SetUINT64,
    activate_object_SetDouble,
    activate_object_SetGUID,
    activate_object_SetString,
    activate_object_SetBlob,
    activate_object_SetUnknown,
    activate_object_LockStore,
    activate_object_UnlockStore,
    activate_object_GetCount,
    activate_object_GetItemByIndex,
    activate_object_CopyAllItems,
    activate_object_ActivateObject,
    activate_object_ShutdownObject,
    activate_object_DetachObject,
};

HRESULT create_activation_object(void *context, const struct activate_funcs *funcs, IMFActivate **ret)
{
    struct activate_object *object;
    HRESULT hr;

    if (!(object = calloc(1, sizeof(*object))))
        return E_OUTOFMEMORY;

    object->IMFActivate_iface.lpVtbl = &activate_object_vtbl;
    object->refcount = 1;
    if (FAILED(hr = MFCreateAttributes(&object->attributes, 0)))
    {
        free(object);
        return hr;
    }
    object->funcs = funcs;
    object->context = context;

    *ret = &object->IMFActivate_iface;

    return S_OK;
}

struct class_factory
{
    IClassFactory IClassFactory_iface;
    HRESULT (*create_instance)(REFIID riid, void **obj);
};

static inline struct class_factory *impl_from_IClassFactory(IClassFactory *iface)
{
    return CONTAINING_RECORD(iface, struct class_factory, IClassFactory_iface);
}

static HRESULT WINAPI class_factory_QueryInterface(IClassFactory *iface, REFIID riid, void **obj)
{
    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), obj);

    if (IsEqualGUID(riid, &IID_IClassFactory) ||
            IsEqualGUID(riid, &IID_IUnknown))
    {
        *obj = iface;
        IClassFactory_AddRef(iface);
        return S_OK;
    }

    WARN("%s is not supported.\n", debugstr_guid(riid));
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI class_factory_AddRef(IClassFactory *iface)
{
    return 2;
}

static ULONG WINAPI class_factory_Release(IClassFactory *iface)
{
    return 1;
}

static HRESULT WINAPI class_factory_CreateInstance(IClassFactory *iface, IUnknown *outer, REFIID riid, void **obj)
{
    struct class_factory *factory = impl_from_IClassFactory(iface);

    TRACE("%p, %p, %s, %p.\n", iface, outer, debugstr_guid(riid), obj);

    if (outer)
    {
        *obj = NULL;
        return CLASS_E_NOAGGREGATION;
    }

    return factory->create_instance(riid, obj);
}

static HRESULT WINAPI class_factory_LockServer(IClassFactory *iface, BOOL dolock)
{
    FIXME("%d.\n", dolock);

    return S_OK;
}

static const IClassFactoryVtbl class_factory_vtbl =
{
    class_factory_QueryInterface,
    class_factory_AddRef,
    class_factory_Release,
    class_factory_CreateInstance,
    class_factory_LockServer,
};

static struct class_factory file_scheme_handler_factory = { { &class_factory_vtbl }, file_scheme_handler_construct };
static struct class_factory urlmon_scheme_handler_factory = { { &class_factory_vtbl }, urlmon_scheme_handler_construct };

static const struct class_object
{
    const GUID *clsid;
    IClassFactory *factory;
}
class_objects[] =
{
/*

[HKEY_CLASSES_ROOT\MediaFoundation\MapVideo4cc]
"33495631"=dword:5634504d
"33495632"=dword:5634504d
"33495658"=dword:5634504d
"33697631"=dword:5634504d
"33697632"=dword:5634504d
"33697658"=dword:5634504d
"44495633"=dword:3334504d
"44495634"=dword:5634504d
"44495635"=dword:5634504d
"44495658"=dword:5634504d
"444D3456"=dword:5634504d
"44583530"=dword:5634504d
"464D5034"=dword:5634504d
"4D344343"=dword:5634504d
"524D5034"=dword:5634504d
"53454447"=dword:5634504d
"58323634"=dword:34363248
"58564944"=dword:5634504d
"64697633"=dword:3334504d
"64697634"=dword:5634504d
"64697635"=dword:5634504d
"64697678"=dword:5634504d
"646D3476"=dword:5634504d
"64783530"=dword:5634504d
"666D7034"=dword:5634504d
"68323634"=dword:34363248
"6D346363"=dword:5634504d
"6D703476"=dword:5634504d
"726D7034"=dword:5634504d
"73656467"=dword:5634504d
"78323634"=dword:34363248
"78766964"=dword:5634504d


[HKEY_CLASSES_ROOT\MediaFoundation\MediaSources\Preferred]
"mcrecv:"="{FA6D33D4-9405-4BA5-9983-12604AC8E77A}"
".3g2"="{271C3902-6095-4c45-A22F-20091816EE9E}"
".3gp"="{271C3902-6095-4c45-A22F-20091816EE9E}"
".3gp2"="{271C3902-6095-4c45-A22F-20091816EE9E}"
".3gpp"="{271C3902-6095-4c45-A22F-20091816EE9E}"
".aac"="{926f41f7-003e-4382-9e84-9e953be10562}"
".ac3"="{46031ba1-083f-47d9-8369-23c92bdab2ff}"
".adt"="{926f41f7-003e-4382-9e84-9e953be10562}"
".adts"="{926f41f7-003e-4382-9e84-9e953be10562}"
".amr"="{EFE6208A-0A2C-49FA-8A01-3768B559B6DA}"
".asf"="{41457294-644C-4298-A28A-BD69F2C0CF3B}"
".avi"="{7AFA253E-F823-42f6-A5D9-714BDE467412}"
".dvr-ms"="{65964407-A5D8-4060-85B0-1CCD63F768E2}"
".ec3"="{46031ba1-083f-47d9-8369-23c92bdab2ff}"
".flac"="{0E41CFB8-0506-40F4-A516-77CC23642D91}"
".m1v"="{40871C59-AB40-471F-8DC3-1F259D862479}"
".m2t"="{40871C59-AB40-471F-8DC3-1F259D862479}"
".m2ts"="{40871C59-AB40-471F-8DC3-1F259D862479}"
".m2v"="{40871C59-AB40-471F-8DC3-1F259D862479}"
".m4a"="{271C3902-6095-4c45-A22F-20091816EE9E}"
".m4v"="{271C3902-6095-4c45-A22F-20091816EE9E}"
".mk3d"="{1f9a2c18-d89e-463e-b4f4-bb90152acc64}"
".mka"="{1f9a2c18-d89e-463e-b4f4-bb90152acc64}"
".mks"="{1f9a2c18-d89e-463e-b4f4-bb90152acc64}"
".mkv"="{1f9a2c18-d89e-463e-b4f4-bb90152acc64}"
".mod"="{40871C59-AB40-471F-8DC3-1F259D862479}"
".mov"="{271C3902-6095-4c45-A22F-20091816EE9E}"
".mp2"="{40871C59-AB40-471F-8DC3-1F259D862479}"
".mp2v"="{40871C59-AB40-471F-8DC3-1F259D862479}"
".mp3"="{A82E50BA-8E92-41eb-9DF2-433F50EC2993}"
".mp4"="{271C3902-6095-4c45-A22F-20091816EE9E}"
".mp4v"="{271C3902-6095-4c45-A22F-20091816EE9E}"
".mpa"="{A82E50BA-8E92-41eb-9DF2-433F50EC2993}"
".mpe"="{40871C59-AB40-471F-8DC3-1F259D862479}"
".mpeg"="{40871C59-AB40-471F-8DC3-1F259D862479}"
".mpg"="{40871C59-AB40-471F-8DC3-1F259D862479}"
".mpv2"="{40871C59-AB40-471F-8DC3-1F259D862479}"
".mts"="{40871C59-AB40-471F-8DC3-1F259D862479}"
".nsc"="{B084785C-DDE0-4d30-8CA8-05A373E185BE}"
".sami"="{7A56C4CB-D678-4188-85A8-BA2EF68FA10D}"
".smi"="{7A56C4CB-D678-4188-85A8-BA2EF68FA10D}"
".tod"="{40871C59-AB40-471F-8DC3-1F259D862479}"
".ts"="{40871C59-AB40-471F-8DC3-1F259D862479}"
".tts"="{40871C59-AB40-471F-8DC3-1F259D862479}"
".uvu"="{271C3902-6095-4c45-A22F-20091816EE9E}"
".vob"="{40871C59-AB40-471F-8DC3-1F259D862479}"
".wav"="{42C9B9F5-16FC-47ef-AF22-DA05F7C842E3}"
".weba"="{1f9a2c18-d89e-463e-b4f4-bb90152acc64}"
".webm"="{1f9a2c18-d89e-463e-b4f4-bb90152acc64}"
".wm"="{41457294-644C-4298-A28A-BD69F2C0CF3B}"
".wma"="{41457294-644C-4298-A28A-BD69F2C0CF3B}"
".wmv"="{41457294-644C-4298-A28A-BD69F2C0CF3B}"
".wtv"="{65964407-A5D8-4060-85B0-1CCD63F768E2}"
"audio/3gpp"="{271C3902-6095-4c45-A22F-20091816EE9E}"
"audio/3gpp2"="{271C3902-6095-4c45-A22F-20091816EE9E}"
"audio/aac"="{926f41f7-003e-4382-9e84-9e953be10562}"
"audio/aacp"="{926f41f7-003e-4382-9e84-9e953be10562}"
"audio/eac3"="{46031ba1-083f-47d9-8369-23c92bdab2ff}"
"audio/flac"="{0E41CFB8-0506-40F4-A516-77CC23642D91}"
"audio/L16"="{3FFB3B8C-EB99-472b-8902-E1C1B05F07CF}"
"audio/mp3"="{A82E50BA-8E92-41eb-9DF2-433F50EC2993}"
"audio/mp4"="{271C3902-6095-4c45-A22F-20091816EE9E}"
"audio/MP4A-LATM"="{271C3902-6095-4c45-A22F-20091816EE9E}"
"audio/mpa"="{A82E50BA-8E92-41eb-9DF2-433F50EC2993}"
"audio/mpeg"="{A82E50BA-8E92-41eb-9DF2-433F50EC2993}"
"audio/mpeg3"="{A82E50BA-8E92-41eb-9DF2-433F50EC2993}"
"audio/vnd.dlna.adts"="{926f41f7-003e-4382-9e84-9e953be10562}"
"audio/vnd.dolby.dd-raw"="{46031ba1-083f-47d9-8369-23c92bdab2ff}"
"audio/wav"="{42C9B9F5-16FC-47ef-AF22-DA05F7C842E3}"
"audio/webm"="{1f9a2c18-d89e-463e-b4f4-bb90152acc64}"
"audio/x-aac"="{926f41f7-003e-4382-9e84-9e953be10562}"
"audio/x-flac"="{0E41CFB8-0506-40F4-A516-77CC23642D91}"
"audio/x-m4a"="{271C3902-6095-4c45-A22F-20091816EE9E}"
"audio/x-matroska"="{1f9a2c18-d89e-463e-b4f4-bb90152acc64}"
"audio/x-mp3"="{A82E50BA-8E92-41eb-9DF2-433F50EC2993}"
"audio/x-mpeg"="{A82E50BA-8E92-41eb-9DF2-433F50EC2993}"
"audio/x-ms-wma"="{41457294-644C-4298-A28A-BD69F2C0CF3B}"
"audio/x-wav"="{42C9B9F5-16FC-47ef-AF22-DA05F7C842E3}"
"file:"="{477EC299-1421-4bdd-971F-7CCB933F21AD}"
"http:"="{E9F4EBAB-D97B-463e-A2B1-C54EE3F9414D}"
"httpd:"="{44CB442B-9DA9-49df-B3FD-023777B16E50}"
"https:"="{37A61C8B-7F8E-4d08-B12B-248D73E9AB4F}"
"httpsd:"="{37A61C8B-7F8E-4d08-B12B-248D73E9AB4F}"
"httpt:"="{E9F4EBAB-D97B-463e-A2B1-C54EE3F9414D}"
"httpu:"="{E9F4EBAB-D97B-463e-A2B1-C54EE3F9414D}"
"mcast:"="{E9F4EBAB-D97B-463e-A2B1-C54EE3F9414D}"
"mms:"="{E9F4EBAB-D97B-463e-A2B1-C54EE3F9414D}"
"ms-appdata:"="{CFC81939-3886-4ACF-9692-DA58037AE716}"
"ms-appx-web:"="{8DB0224B-3D65-4F6F-8E12-BEB4B78B8974}"
"ms-appx:"="{8DB0224B-3D65-4F6F-8E12-BEB4B78B8974}"
"ms-winsoundevent:"="{F79A6BF9-7415-4CF3-AE10-4559509ABC3C}"
"rtsp:"="{E9F4EBAB-D97B-463e-A2B1-C54EE3F9414D}"
"rtsps:"="{E9F4EBAB-D97B-463e-A2B1-C54EE3F9414D}"
"rtspt:"="{E9F4EBAB-D97B-463e-A2B1-C54EE3F9414D}"
"rtspu:"="{E9F4EBAB-D97B-463e-A2B1-C54EE3F9414D}"
"sdp:"="{E9F4EBAB-D97B-463e-A2B1-C54EE3F9414D}"
"video/3gpp"="{271C3902-6095-4c45-A22F-20091816EE9E}"
"video/3gpp2"="{271C3902-6095-4c45-A22F-20091816EE9E}"
"video/avi"="{7AFA253E-F823-42f6-A5D9-714BDE467412}"
"video/mp4"="{271C3902-6095-4c45-A22F-20091816EE9E}"
"video/mpeg"="{40871C59-AB40-471F-8DC3-1F259D862479}"
"video/msvideo"="{7AFA253E-F823-42f6-A5D9-714BDE467412}"
"video/vnd.dece.mp4"="{271C3902-6095-4c45-A22F-20091816EE9E}"
"video/vnd.dlna.mpeg-tts"="{40871C59-AB40-471F-8DC3-1F259D862479}"
"video/webm"="{1f9a2c18-d89e-463e-b4f4-bb90152acc64}"
"video/x-m4v"="{271C3902-6095-4c45-A22F-20091816EE9E}"
"video/x-matroska"="{1f9a2c18-d89e-463e-b4f4-bb90152acc64}"
"video/x-ms-asf"="{41457294-644C-4298-A28A-BD69F2C0CF3B}"
"video/x-ms-wm"="{41457294-644C-4298-A28A-BD69F2C0CF3B}"
"video/x-ms-wmv"="{41457294-644C-4298-A28A-BD69F2C0CF3B}"
"video/x-msvideo"="{7AFA253E-F823-42f6-A5D9-714BDE467412}"


[HKEY_CLASSES_ROOT\MediaFoundation\Transforms\Preferred]
"{0000000A-0000-0010-8000-00AA00389B71}"="{874131CB-4ECC-443B-8948-746B89595D20}"
"{0000000B-0000-0010-8000-00AA00389B71}"="{874131CB-4ECC-443B-8948-746B89595D20}"
"{00000002-0000-0010-8000-00AA00389B71}"="{CA34FE0A-5722-43AD-AF23-05F7650257DD}"
"{00000006-0000-0010-8000-00AA00389B71}"="{36CB6E0C-78C1-42B2-9943-846262F31786}"
"{00000007-0000-0010-8000-00AA00389B71}"="{92B66080-5E2D-449E-90C4-C41F268E5514}"
"{00000011-0000-0010-8000-00AA00389B71}"="{A16E1BFF-A80D-48AD-AECD-A35C005685FE}"
"{00000031-0000-0010-8000-00AA00389B71}"="{4A76B469-7B66-4DD4-BA2D-DDF244C766DC}"
"{00000050-0000-0010-8000-00aa00389b71}"="{70707B39-B2CA-4015-ABEA-F8447D22D88B}"
"{00000055-0000-0010-8000-00AA00389B71}"="{BBEEA841-0A63-4F52-A7AB-A9B3A84ED38A}"
"{000000FF-0000-0010-8000-00aa00389b71}"="{32d186a7-218f-4c75-8876-dd77273a8999}"
"{00000160-0000-0010-8000-00aa00389b71}"="{2eeb4adf-4578-4d10-bca7-bb955f56320a}"
"{00000161-0000-0010-8000-00aa00389b71}"="{2eeb4adf-4578-4d10-bca7-bb955f56320a}"
"{00000162-0000-0010-8000-00aa00389b71}"="{2eeb4adf-4578-4d10-bca7-bb955f56320a}"
"{00000163-0000-0010-8000-00aa00389b71}"="{2eeb4adf-4578-4d10-bca7-bb955f56320a}"
"{00001600-0000-0010-8000-00aa00389b71}"="{32d186a7-218f-4c75-8876-dd77273a8999}"
"{00001610-0000-0010-8000-00aa00389b71}"="{32d186a7-218f-4c75-8876-dd77273a8999}"
"{00002000-0000-0010-8000-00aa00389b71}"="{177C0AFE-900B-48d4-9E4C-57ADD250B3D4}"
"{00002001-0000-0010-8000-00AA00389B71}"="{d035e24c-c877-42d7-a795-2a8a339b472f}"
"{00006C61-0000-0010-8000-00AA00389B71}"="{C0CD7D12-31FC-4BBC-B363-7322EE3E1879}"
"{0000704F-0000-0010-8000-00AA00389B71}"="{63e17c10-2d43-4c42-8fe3-8d8b63e46a6a}"
"{00007361-0000-0010-8000-00AA00389B71}"="{265011AE-5481-4f77-A295-ABB6FFE8D63E}"
"{0000F1AC-0000-0010-8000-00AA00389B71}"="{6B0B3E6B-A2C5-4514-8055-AFE8A95242D9}"
"{20637664-0000-0010-8000-00AA00389B71}"="{404A6DE5-D4D6-4260-9BC7-5A6CBD882432}"
"{31435657-0000-0010-8000-00AA00389B71}"="{82d353df-90bd-4382-8bc2-3f6192b76e34}"
"{3147504d-0000-0010-8000-00aa00389b71}"="{2D709E52-123F-49b5-9CBC-9AF5CDE28FB9}"
"{3153534D-0000-0010-8000-00AA00389B71}"="{7BAFB3B1-D8F4-4279-9253-27DA423108DE}"
"{31564D57-0000-0010-8000-00AA00389B71}"="{82d353df-90bd-4382-8bc2-3f6192b76e34}"
"{3234504D-0000-0010-8000-00AA00389B71}"="{F371728A-6052-4D47-827C-D039335DFE0A}"
"{3234706D-0000-0010-8000-00AA00389B71}"="{F371728A-6052-4D47-827C-D039335DFE0A}"
"{32505657-0000-0010-8000-00AA00389B71}"="{82d353df-90bd-4382-8bc2-3f6192b76e34}"
"{3253344D-0000-0010-8000-00AA00389B71}"="{5686A0D9-FE39-409F-9DFF-3FDBC849F9F5}"
"{3253534D-0000-0010-8000-00AA00389B71}"="{7BAFB3B1-D8F4-4279-9253-27DA423108DE}"
"{32564D57-0000-0010-8000-00AA00389B71}"="{82d353df-90bd-4382-8bc2-3f6192b76e34}"
"{3273346D-0000-0010-8000-00AA00389B71}"="{5686A0D9-FE39-409F-9DFF-3FDBC849F9F5}"
"{3334504D-0000-0010-8000-00AA00389B71}"="{CBA9E78B-49A3-49EA-93D4-6BCBA8C4DE07}"
"{3334706D-0000-0010-8000-00AA00389B71}"="{CBA9E78B-49A3-49EA-93D4-6BCBA8C4DE07}"
"{33363248-0000-0010-8000-00AA00389B71}"="{5686A0D9-FE39-409F-9DFF-3FDBC849F9F5}"
"{33564D57-0000-0010-8000-00AA00389B71}"="{82d353df-90bd-4382-8bc2-3f6192b76e34}"
"{34363248-0000-0010-8000-00AA00389B71}"="{62CE7E72-4C71-4D20-B15D-452831A87D9D}"
"{3447504D-0000-0010-8000-00AA00389B71}"="{F371728A-6052-4D47-827C-D039335DFE0A}"
"{3467706D-0000-0010-8000-00AA00389B71}"="{F371728A-6052-4D47-827C-D039335DFE0A}"
"{3F40F4F0-5622-4FF8-B6D8-A17A584BEE5E}"="{62CE7E72-4C71-4D20-B15D-452831A87D9D}"
"{41564D57-0000-0010-8000-00AA00389B71}"="{82d353df-90bd-4382-8bc2-3f6192b76e34}"
"{47504A4D-0000-0010-8000-00AA00389B71}"="{CB17E772-E1CC-4633-8450-5617AF577905}"
"{50564D57-0000-0010-8000-00AA00389B71}"="{82d353df-90bd-4382-8bc2-3f6192b76e34}"
"{52564D57-0000-0010-8000-00AA00389B71}"="{82d353df-90bd-4382-8bc2-3f6192b76e34}"
"{53314356-0000-0010-8000-00AA00389B71}"="{82d353df-90bd-4382-8bc2-3f6192b76e34}"
"{5334504D-0000-0010-8000-00AA00389B71}"="{5686A0D9-FE39-409F-9DFF-3FDBC849F9F5}"
"{5634504D-0000-0010-8000-00AA00389B71}"="{5686A0D9-FE39-409F-9DFF-3FDBC849F9F5}"
"{63616C61-0000-0010-8000-00AA00389B71}"="{C0CD7D12-31FC-4BBC-B363-7322EE3E1879}"
"{64687664-0000-0010-8000-00AA00389B71}"="{404A6DE5-D4D6-4260-9BC7-5A6CBD882432}"
"{64737664-0000-0010-8000-00AA00389B71}"="{404A6DE5-D4D6-4260-9BC7-5A6CBD882432}"
"{6c737664-0000-0010-8000-00AA00389B71}"="{404A6DE5-D4D6-4260-9BC7-5A6CBD882432}"
"{7334706D-0000-0010-8000-00AA00389B71}"="{5686A0D9-FE39-409F-9DFF-3FDBC849F9F5}"
"{73616d72-767a-494d-b478-f29d25dc9037}"="{265011AE-5481-4f77-A295-ABB6FFE8D63E}"
"{7634706D-0000-0010-8000-00AA00389B71}"="{5686A0D9-FE39-409F-9DFF-3FDBC849F9F5}"
"{A2E58EB7-0FA9-48bb-A40C-FA0E156D0645}"="{d035e24c-c877-42d7-a795-2a8a339b472f}"
"{A61AC364-AD0E-4744-89FF-213CE0DF8804}"="{d035e24c-c877-42d7-a795-2a8a339b472f}"
"{a7fb87af-2d02-42fb-a4d4-05cd93843bdd}"="{177C0AFE-900B-48d4-9E4C-57ADD250B3D4}"
"{e06d8026-db46-11cf-b4d1-00805f6cbbea}"="{2D709E52-123F-49b5-9CBC-9AF5CDE28FB9}"
"{e06d802c-db46-11cf-b4d1-00805f6cbbea}"="{177C0AFE-900B-48d4-9E4C-57ADD250B3D4}"
"{eb27cec4-163e-4ca3-8b74-8e25f91b517e}"="{cf5eeedf-0e92-4b3b-a161-bd0ffe545e4b}"

[HKEY_CLASSES_ROOT\MediaFoundation\Transforms\PreferredByOutputType]
"{0000000A-0000-0010-8000-00AA00389B71}"="{1f1f4e1a-2252-4063-84bb-eee75f8856d5}"
"{00000050-0000-0010-8000-00aa00389b71}"="{46A4DD5C-73F8-4304-94DF-308F760974F4}"
"{00000055-0000-0010-8000-00aa00389b71}"="{11103421-354c-4cca-a7a3-1aff9a5b6701}"
"{00000161-0000-0010-8000-00aa00389b71}"="{70f598e9-f4ab-495a-99e2-a7c4d3d89abf}"
"{00000162-0000-0010-8000-00aa00389b71}"="{70f598e9-f4ab-495a-99e2-a7c4d3d89abf}"
"{00000163-0000-0010-8000-00aa00389b71}"="{70f598e9-f4ab-495a-99e2-a7c4d3d89abf}"
"{00001610-0000-0010-8000-00aa00389b71}"="{93AF0C51-2275-45D2-A35B-F2BA21CAED00}"
"{00006C61-0000-0010-8000-00AA00389B71}"="{9AB6A28C-748E-4B6A-BFFF-CC443B8E8FB4}"
"{00007361-0000-0010-8000-00AA00389B71}"="{2FAE8AFE-04A3-423a-A814-85DB454712B0}"
"{0000F1AC-0000-0010-8000-00AA00389B71}"="{128509e9-c44e-45dc-95e9-c255b8f466a6}"
"{263067d1-d330-45dc-b669-34d986e4e3e1}"="{ab300f71-01ab-46d2-ab6c-64906cb03258}"
"{31435657-0000-0010-8000-00AA00389B71}"="{d23b90d0-144f-46bd-841d-59e4eb19dc59}"
"{31564D57-0000-0010-8000-00AA00389B71}"="{7e320092-596a-41b2-bbeb-175d10504eb6}"
"{32505657-0000-0010-8000-00AA00389B71}"="{d23b90d0-144f-46bd-841d-59e4eb19dc59}"
"{32564D57-0000-0010-8000-00AA00389B71}"="{7e320092-596a-41b2-bbeb-175d10504eb6}"
"{33363248-0000-0010-8000-00AA00389B71}"="{bc47fcfe-98a0-4f27-bb07-698af24f2b38}"
"{33564D57-0000-0010-8000-00AA00389B71}"="{d23b90d0-144f-46bd-841d-59e4eb19dc59}"
"{34363248-0000-0010-8000-00AA00389B71}"="{6ca50344-051a-4ded-9779-a43305165e35}"
"{41564D57-0000-0010-8000-00AA00389B71}"="{d23b90d0-144f-46bd-841d-59e4eb19dc59}"
"{50564D57-0000-0010-8000-00AA00389B71}"="{d23b90d0-144f-46bd-841d-59e4eb19dc59}"
"{e06d8023-db46-11cf-b4d1-00805f6cbbea}"="{ab300f71-01ab-46d2-ab6c-64906cb03258}"
"{e06d8026-db46-11cf-b4d1-00805f6cbbea}"="{e6335f02-80b7-4dc4-adfa-dfe7210d20d5}"
"{e06d802c-db46-11cf-b4d1-00805f6cbbea}"="{AC3315C9-F481-45D7-826C-0B406C1F64B8}"

{01f36ce2-0907-4d8b-979d-f151be91c883} = "Frame Rate Converter"
{05a47ebb-8bf0-4cbf-ad2f-3b71d75866f5} = "Microsoft H264 Video Remux (MPEG2TSToMP4) MFT"
{068a8476-9229-4cc0-9d49-2fc699dcd30a} = "LPCM DVD-Audio MFT"
{11103421-354c-4cca-a7a3-1aff9a5b6701} = "MP3 Encoder ACM Wrapper MFT"
{128509e9-c44e-45dc-95e9-c255b8f466a6} = "Microsoft FLAC Audio Encoder MFT"
{177C0AFE-900B-48d4-9E4C-57ADD250B3D4} = "Microsoft Dolby Digital Plus Decoder MFT"
{1ea1ea14-48f4-4054-ad1a-e8aee10ac805} = "Resizer MFT"
{1f1f4e1a-2252-4063-84bb-eee75f8856d5} = "WM Speech Encoder DMO"
{265011AE-5481-4f77-A295-ABB6FFE8D63E} = "MS AMRNB Decoder MFT"
{2D709E52-123F-49b5-9CBC-9AF5CDE28FB9} = "Microsoft MPEG Video Decoder MFT"
{2eeb4adf-4578-4d10-bca7-bb955f56320a} = "WMAudio Decoder MFT"
{2FAE8AFE-04A3-423a-A814-85DB454712B0} = "MS AMRNB Encoder MFT"
{32d186a7-218f-4c75-8876-dd77273a8999} = "Microsoft AAC Audio Decoder MFT"
{36CB6E0C-78C1-42B2-9943-846262F31786} = "A-law Wrapper MFT"
{404a6de5-d4d6-4260-9bc7-5a6cbd882432} = "DV Decoder MFT"
{46A4DD5C-73F8-4304-94DF-308F760974F4} = "Microsoft MPEG-2 Audio Encoder MFT"
{4a76b469-7b66-4dd4-ba2d-ddf244c766dc} = "GSM ACM Wrapper MFT"
{51571744-7fe4-4ff2-a498-2dc34ff74f1b} = "VideoStabilization MFT"
{5210f8e4-b0bb-47c3-a8d9-7b2282cc79ed} = "WMAPro over S/PDIF MFT"
{5686a0d9-fe39-409f-9dff-3fdbc849f9f5} = "Mpeg4s Decoder MFT"
{62ce7e72-4c71-4d20-b15d-452831a87d9d} = "Microsoft H264 Video Decoder MFT"
{63e17c10-2d43-4c42-8fe3-8d8b63e46a6a} = "Microsoft Opus Audio Decoder MFT"
{6B0B3E6B-A2C5-4514-8055-AFE8A95242D9} = "Microsoft FLAC Audio Decoder MFT"
{6ca50344-051a-4ded-9779-a43305165e35} = "H264 Encoder MFT"
{70707B39-B2CA-4015-ABEA-F8447D22D88B} = "Microsoft MPEG Audio Decoder MFT"
{70f598e9-f4ab-495a-99e2-a7c4d3d89abf} = "WMAudio Encoder MFT"
{745057c7-f353-4f2d-a7ee-58434477730e} = "AEC"
{798059f0-89ca-4160-b325-aeb48efe4f9a} = "Color Control"
{7bafb3b1-d8f4-4279-9253-27da423108de} = "WMV Screen decoder MFT"
{7e320092-596a-41b2-bbeb-175d10504eb6} = "WMVideo8 Encoder MFT"
{82d353df-90bd-4382-8bc2-3f6192b76e34} = "WMVideo Decoder MFT"
{874131cb-4ecc-443b-8948-746b89595d20} = "WMSpeech Decoder DMO"
{88753B26-5B24-49bd-B2E7-0C445C78C982} = "Microsoft Video Processor MFT"
{92b66080-5e2d-449e-90c4-c41f268e5514} = "G711 Wrapper MFT"
{93af0c51-2275-45d2-a35b-f2ba21caed00} = "Microsoft AAC Audio Encoder MFT"
{98230571-0087-4204-b020-3282538e57d3} = "Color Converter MFT"
{9AB6A28C-748E-4B6A-BFFF-CC443B8E8FB4} = "Microsoft ALAC Audio Encoder MFT"
{a16e1bff-a80d-48ad-aecd-a35c005685fe} = "IMA ADPCM ACM Wrapper MFT"
{ab300f71-01ab-46d2-ab6c-64906cb03258} = "Microsoft MPEG2 Multiplexer MFT"
{AC3315C9-F481-45D7-826C-0B406C1F64B8} = "Microsoft Dolby Digital Encoder MFT"
{bbeea841-0a63-4f52-a7ab-a9b3a84ed38a} = "MP3 Decoder MFT"
{bc47fcfe-98a0-4f27-bb07-698af24f2b38} = "H263 Encoder MFT"
{C0CD7D12-31FC-4BBC-B363-7322EE3E1879} = "Microsoft ALAC Audio Decoder MFT"
{ca34fe0a-5722-43ad-af23-05f7650257dd} = "ADPCM ACM Wrapper MFT"
{CB17E772-E1CC-4633-8450-5617AF577905} = "MJPEG Decoder MFT"
{cba9e78b-49a3-49ea-93d4-6bcba8c4de07} = "Mpeg43 Decoder MFT"
{cf5eeedf-0e92-4b3b-a161-bd0ffe545e4b} = "Dolby TrueHD IEC-61937 converter MFT"
{d035e24c-c877-42d7-a795-2a8a339b472f} = "DTS IEC-61937 converter MFT"
{d23b90d0-144f-46bd-841d-59e4eb19dc59} = "WMVideo9 Encoder MFT"
{e6335f02-80b7-4dc4-adfa-dfe7210d20d5} = "Microsoft MPEG-2 Video Encoder MFT"
{f371728a-6052-4d47-827c-d039335dfe0a} = "Mpeg4 Decoder MFT"
{f447b69e-1884-4a7e-8055-346f74d6edb3} = "Resampler MFT"


{EFE6208A-0A2C-49FA-8A01-3768B559B6DA}  MF AMRNB Media Source ByteStreamHandler
{a8721937-e2fb-4d7a-a9ee-4eb08c890b6e}  MF SBE Source ByteStreamHandler
{FA6D33D4-9405-4BA5-9983-12604AC8E77A}  Miracast Sink Scheme Handler
*/

    { &CLSID_FileSchemePlugin, &file_scheme_handler_factory.IClassFactory_iface }, /* mfcore.dll */
/*
{11103421-354C-4CCA-A7A3-1AFF9A5B6701}  MP3 ACM Wrapper MFT (C:\Windows\System32\mfcore.dll)
{14DD9A1C-7CFF-41be-B1B9-BA1AC6ECB571}  MF AudCap Source Plug-in (C:\Windows\System32\mfcore.dll)
{36CB6E0C-78C1-42B2-9943-846262F31786}  A-law ACM Wrapper MFT (C:\Windows\System32\mfcore.dll)
{477EC299-1421-4bdd-971F-7CCB933F21AD}  File Scheme Handler (C:\Windows\System32\mfcore.dll)
{4a76b469-7b66-4dd4-ba2d-ddf244c766dc}  GSM ACM Wrapper MFT (C:\Windows\System32\mfcore.dll)
{770e8e77-4916-441c-a9a7-b342d0eebc71}  MF ByteStream Proxy Class Factory (C:\Windows\System32\mfcore.dll)
{8ac3587a-4ae7-42d8-99e0-0a6013eef90f}  Device Proxy MFT (C:\Windows\System32\mfcore.dll)
{92b66080-5e2d-449e-90c4-c41f268e5514}  G711 ACM Wrapper MFT (C:\Windows\System32\mfcore.dll)
{a16e1bff-a80d-48ad-aecd-a35c005685fe}  IMA ADPCM ACM Wrapper MFT (C:\Windows\System32\mfcore.dll)
{c2b94e6d-6ebd-44ee-aac1-eb809c984699}  SeamlessAudio mft (C:\Windows\System32\mfcore.dll)
{ca34fe0a-5722-43ad-af23-05f7650257dd}  ADPCM ACM Wrapper MFT (C:\Windows\System32\mfcore.dll)
{e79167d7-1b85-4d78-b603-798e0e1a4c67}  MF Media Source Activate (C:\Windows\System32\mfcore.dll)
{F79A6BF9-7415-4CF3-AE10-4559509ABC3C}  Sound Event Scheme Handler (C:\Windows\System32\mfcore.dll)
*/
    { &CLSID_UrlmonSchemePlugin, &urlmon_scheme_handler_factory.IClassFactory_iface }, /* mfnetcore.dll */
/*
{37A61C8B-7F8E-4d08-B12B-248D73E9AB4F}  Secure Http Scheme Handler (C:\Windows\System32\mfnetcore.dll)
{3FFB3B8C-EB99-472b-8902-E1C1B05F07CF}  LPCM Byte Stream Handler (C:\Windows\System32\mfnetcore.dll)
{44CB442B-9DA9-49df-B3FD-023777B16E50}  Http Scheme Handler (C:\Windows\System32\mfnetcore.dll)
{9EC4B4F9-3029-45ad-947B-344DE2A249E2}  Urlmon Scheme Handler (C:\Windows\System32\mfnetcore.dll)
*/
    { &CLSID_MPEG4ByteStreamHandlerPlugin, &byte_stream_handler_factory.IClassFactory_iface }, /* mfmp4srcsnk.dll */
/*
{11275A82-5E5A-47fd-A01C-3683C12FB196}  MF 3GP Sink Class Factory (C:\Windows\System32\mfmp4srcsnk.dll)
{271C3902-6095-4c45-A22F-20091816EE9E}  MPEG4 Byte Stream Handler (C:\Windows\System32\mfmp4srcsnk.dll)
{59899957-02f6-44ac-a369-7704e65a1364}  MF MPEG4 Parser Factory (C:\Windows\System32\mfmp4srcsnk.dll)
{60F9F51E-4613-4b35-AE88-332542B567B8}  MF Fragmented MPEG4 Sink Class Factory (C:\Windows\System32\mfmp4srcsnk.dll)
{620E38AE-D62C-486B-8339-3DF3C04D88C3}  MF MPEG4 Media Segment Index Parser (C:\Windows\System32\mfmp4srcsnk.dll)
{A22C4FC7-6E91-4e1d-89E9-53B2667B72BA}  MF MPEG4 Sink Class Factory (C:\Windows\System32\mfmp4srcsnk.dll)
{f81b1b56-7613-4ee4-bc05-1fab5de5c07e}  MF MPEG-4 Property Handler (C:\Windows\System32\mfmp4srcsnk.dll)
*/
    { &CLSID_MSAACDecMFT, &aac_decoder_factory.IClassFactory_iface }, /* MSAudDecMFT.dll */
/*
{32d186a7-218f-4c75-8876-dd77273a8999}  Microsoft AAC Audio Decoder MFT (C:\Windows\System32\MSAudDecMFT.dll)
{70707B39-B2CA-4015-ABEA-F8447D22D88B}  Microsoft MPEG Audio Decoder MFT (C:\Windows\System32\MSAudDecMFT.dll)
{BBA922ED-0AB7-40A7-9E69-495984866A23}  (C:\Windows\System32\MSAudDecMFT.dll)
*/
    { &CLSID_WMADecMediaObject, &wma_decoder_factory.IClassFactory_iface }, /* WMADMOD.DLL */
/*
{2eeb4adf-4578-4d10-bca7-bb955f56320a}  WMAudio Decoder DMO (C:\Windows\System32\WMADMOD.DLL)
{5210f8e4-b0bb-47c3-a8d9-7b2282cc79ed}  WMAPro over S/PDIF DMO (C:\Windows\System32\WMADMOD.DLL)
*/
    { &CLSID_MSH264DecoderMFT, &h264_decoder_factory.IClassFactory_iface }, /* msmpeg2vdec.dll */
/*
{05A47EBB-8BF0-4CBF-AD2F-3B71D75866F5}  Microsoft H264 Video Remux (MPEG2TSToMP4) MFT (C:\Windows\System32\msmpeg2vdec.dll)
{10FEF81C-0DAA-4af0-B714-1F1689C08C8C}  DXVA Allocator (C:\Windows\System32\msmpeg2vdec.dll)
{212690FB-83E5-4526-8FD7-74478B7939CD}  Microsoft DTV-DVD Video Decoder (C:\Windows\System32\msmpeg2vdec.dll)
{2D709E52-123F-49b5-9CBC-9AF5CDE28FB9}  Microsoft MPEG Video Decoder MFT (C:\Windows\System32\msmpeg2vdec.dll)
{62CE7E72-4C71-4D20-B15D-452831A87D9D}  Microsoft H264 Video Decoder MFT (C:\Windows\System32\msmpeg2vdec.dll)
*/
    { &CLSID_WMVDecoderMFT, &wmv_decoder_factory.IClassFactory_iface }, /* wmvdecod.dll */
/*
{82d353df-90bd-4382-8bc2-3f6192b76e34}  WMVideo Decoder DMO (C:\Windows\System32\wmvdecod.dll)
*/
    { &CLSID_VideoProcessorMFT, &video_processor_factory.IClassFactory_iface }, /* msvproc.dll */
/*
{7B82D688-3B9E-4090-8ECD-FC84E454C923}  (C:\Windows\System32\msvproc.dll)
{88753B26-5B24-49bd-B2E7-0C445C78C982}  (C:\Windows\System32\msvproc.dll)
{becab11e-6629-4237-a9a4-5684579392a2}  (C:\Windows\System32\msvproc.dll)
*/
    { &CLSID_AudioResamplerMediaObject, &resampler_factory.IClassFactory_iface }, /* resampledmo.dll */
/*
{f447b69e-1884-4a7e-8055-346f74d6edb3}  Resampler DMO (C:\Windows\System32\resampledmo.dll)
*/
    { &CLSID_MSH264EncoderMFT, &h264_encoder_factory.IClassFactory_iface }, /* mfh264enc.dll */
/*
{6ca50344-051a-4ded-9779-a43305165e35}  (C:\Windows\System32\mfh264enc.dll)
*/
    { &CLSID_CColorConvertDMO, &color_converter_factory.IClassFactory_iface }, /* colorcnv.dll */
/*
{98230571-0087-4204-b020-3282538e57d3}  Color Converter DMO (C:\Windows\System32\colorcnv.dll)
*/
    { &CLSID_MFMP3SinkClassFactory, &mp3_sink_class_factory.IClassFactory_iface }, /* mfmp4srcsnk.dll */
    { &CLSID_MFMPEG4SinkClassFactory, &mpeg4_sink_class_factory.IClassFactory_iface }, /* mfmp4srcsnk.dll */
/*
{1f9a2c18-d89e-463e-b4f4-bb90152acc64}  MKV Byte Stream Handler (C:\Windows\System32\mfmkvsrcsnk.dll)
{C591F150-4106-4141-B5C1-30B2101453BD}  MF MKV Property Handler (C:\Windows\System32\mfmkvsrcsnk.dll)

{40871C59-AB40-471F-8DC3-1F259D862479}  MPEG2 Byte Stream Handler (C:\Windows\System32\mfmpeg2srcsnk.dll)
{1E589E9D-8A8D-46d9-A2F9-E6D4F8161EE9}  MF MPEG Property Handler (C:\Windows\System32\mfmpeg2srcsnk.dll)
{26A1F020-DDD8-472D-8B7C-2F98B80024F3}  MF MPEG2 Parser Class Factory (C:\Windows\System32\mfmpeg2srcsnk.dll)
{aa1b0e54-7593-40a0-8135-ed0b6cbc0154}  Media Foundation MPEG2 MUX MFT Activate (C:\Windows\System32\mfmpeg2srcsnk.dll)
{AB300F71-01AB-46D2-AB6C-64906CB03258}  Media Foundation MPEG2 MUX MFT (C:\Windows\System32\mfmpeg2srcsnk.dll)
{FD215A13-A26A-44FF-BA3A-9109F278E28F}  MF Mux Sink Class Factory (C:\Windows\System32\mfmpeg2srcsnk.dll)

{41457294-644C-4298-A28A-BD69F2C0CF3B}  ASF Byte Stream Handler (C:\Windows\System32\mfasfsrcsnk.dll)
{AEB16279-B750-48f1-8586-97956060175A}  MF ASF Property Handler (C:\Windows\System32\mfasfsrcsnk.dll)

{42C9B9F5-16FC-47ef-AF22-DA05F7C842E3}  WAV Byte Stream Handler (C:\Windows\System32\mfsrcsnk.dll)
{46031ba1-083f-47d9-8369-23c92bdab2ff}  AC-3 Byte Stream Handler (C:\Windows\System32\mfsrcsnk.dll)
{7A56C4CB-D678-4188-85A8-BA2EF68FA10D}  SAMI Byte Stream Handler (C:\Windows\System32\mfsrcsnk.dll)
{7AFA253E-F823-42f6-A5D9-714BDE467412}  AVI Byte Stream Handler (C:\Windows\System32\mfsrcsnk.dll)
{926f41f7-003e-4382-9e84-9e953be10562}  ADTS Byte Stream Handler (C:\Windows\System32\mfsrcsnk.dll)
{A82E50BA-8E92-41eb-9DF2-433F50EC2993}  MP3 Byte Stream Handler (C:\Windows\System32\mfsrcsnk.dll)
{255a6fda-6f93-4e8a-9611-ded1169eefb4}  MF Dolby AC-3 Sink Class Factory (C:\Windows\System32\mfsrcsnk.dll)
{36F99745-23C9-4C9C-8DD5-CC31CE964390}  MF WAVE Sink Class Factory (C:\Windows\System32\mfsrcsnk.dll)
{487BA7B8-4DB0-465F-B122-C74A445A095D}  MF Media Source Pack Property Handler (C:\Windows\System32\mfsrcsnk.dll)
{5AA730D8-0F13-4AD6-B6C6-1EA85218C9A0}  MF MP3 Parser Class Factory (C:\Windows\System32\mfsrcsnk.dll)
{80009818-f38f-4af1-87b5-eadab9433e58}  MF ADTS Property Handler (C:\Windows\System32\mfsrcsnk.dll)
{9B67E7B7-12BA-4B81-9874-E96D8C7C07F8}  MF AC3 Parser Class Factory (C:\Windows\System32\mfsrcsnk.dll)
{AF4B1274-B78A-4979-AEF5-20E78FEE102E}  MF AVI Sink Class Factory (C:\Windows\System32\mfsrcsnk.dll)
{C120DE80-FDE4-49f5-A713-E902EF062B8A}  MF MP3 Property Handler (C:\Windows\System32\mfsrcsnk.dll)
{d7ca55ab-5022-4db3-a599-abafa358e6f3}  MF ADTS Sink Class Factory (C:\Windows\System32\mfsrcsnk.dll)
{e2fb4720-f45f-4a3c-8cb2-2060e12425c3}  MF AVI Property Handler (C:\Windows\System32\mfsrcsnk.dll)
{e46787a1-4629-4423-a693-be1f003b2742}  MF WAV Property Handler (C:\Windows\System32\mfsrcsnk.dll)
{E54CDFAF-2381-4cad-AB99-F38517127D5C}  MF MP3 Sink Class Factory (C:\Windows\System32\mfsrcsnk.dll)
{edc3a8b5-2e25-466a-a1ad-21e2f19414ac}  MF AC3 Property Handler (C:\Windows\System32\mfsrcsnk.dll)
{FA869D29-BA80-4D7D-A867-0DF126F980A5}  MF ADTS Parser Class Factory (C:\Windows\System32\mfsrcsnk.dll)

{B084785C-DDE0-4d30-8CA8-05A373E185BE}  NSC Byte Stream Handler (C:\Windows\System32\mfnetsrc.dll)
{8DB0224B-3D65-4F6F-8E12-BEB4B78B8974}  MsAppx Scheme Handler (C:\Windows\System32\mfnetsrc.dll)
{CFC81939-3886-4ACF-9692-DA58037AE716}  MsAppData Scheme Handler (C:\Windows\System32\mfnetsrc.dll)
{E9F4EBAB-D97B-463e-A2B1-C54EE3F9414D}  Net Scheme Handler (C:\Windows\System32\mfnetsrc.dll)

mfds:

{0344ec28-5339-4124-a186-2e8eef168785}  MFSourceFilter (C:\Windows\System32\mfds.dll)
{14d7a407-396b-44b3-be85-5199a0f0f80a}  Media Foundation DShow Source Resolver (C:\Windows\System32\mfds.dll)
{65964407-a5d8-4060-85b0-1ccd63f768e2}  WTV Bytestream Plugin (C:\Windows\System32\mfds.dll)
{743a6e3b-a5df-43ed-b615-4256add790b8}  MFPsiFilter (C:\Windows\System32\mfds.dll)
{CA8EB365-E97F-4cfa-965C-B3E8986ABD8F}  Media Foundation DShow Transform Wrapper (C:\Windows\System32\mfds.dll)
{eb4d075a-65c0-476b-956c-c605eade03f7}  DemuxRender (C:\Windows\System32\mfds.dll)
{f792beee-aeaf-4ebb-ab14-8bc5c8c695a8}  Media Foundation MP2demux (C:\Windows\System32\mfds.dll)


dmo:

{01F36CE2-0907-4d8b-979D-F151BE91C883}  CFrameRateConvertDmo (C:\Windows\System32\mfvdsp.dll)
{798059F0-89CA-4160-B325-AEB48EFE4F9A}  CColorControlDmo (C:\Windows\System32\mfvdsp.dll)

{1ea1ea14-48f4-4054-ad1a-e8aee10ac805}  Resizer DMO (C:\Windows\System32\vidreszr.dll)

{120CED89-3BF4-4173-A132-3CB406CF3231}  DirectSoundParamEqDMO (C:\Windows\System32\dsdmo.dll)
{87FC0268-9A55-4360-95AA-004A1D9DE26C}  DirectSoundWavesReverbDMO (C:\Windows\System32\dsdmo.dll)
{DAFD8210-5711-4B91-9FE3-F75B7AE279BF}  DirectSoundGargleDMO (C:\Windows\System32\dsdmo.dll)
{EF011F79-4000-406D-87AF-BFFB3FC39D57}  DirectSoundCompressorDMO (C:\Windows\System32\dsdmo.dll)
{EF114C90-CD1D-484E-96E5-09CFAF912A21}  DirectSoundDistortionDMO (C:\Windows\System32\dsdmo.dll)
{EF3E932C-D40B-4F51-8CCF-3F98F1B29D5D}  DirectSoundEchoDMO (C:\Windows\System32\dsdmo.dll)
{EF985E71-D5C7-42D4-BA4D-2D073E2E96F4}  DirectSoundI3DL2ReverbDMO (C:\Windows\System32\dsdmo.dll)
{EFCA3D92-DFD8-4672-A603-7420894BAD98}  DirectSoundFlangerDMO (C:\Windows\System32\dsdmo.dll)
{EFE6629C-81F7-4281-BD91-C9D604A95AF6}  DirectSoundChorusDMO (C:\Windows\System32\dsdmo.dll)

{1f1f4e1a-2252-4063-84bb-eee75f8856d5}  WM Speech Encoder DMO (C:\Windows\System32\WMSPDMOE.DLL)

{2a11bae2-fe6e-4249-864b-9e9ed6e8dbc2}  Mpeg4s Decoder DMO (C:\Windows\System32\mp4sdecd.dll)
{5686a0d9-fe39-409f-9dff-3fdbc849f9f5}  Mpeg4s Decoder MFT (C:\Windows\System32\mp4sdecd.dll)

{11993195-1244-4840-AB44-480975C4FFE4}  CFileIo (C:\Windows\System32\wmvdspa.dll)
{3A8CCCBC-0EFD-43a3-B838-F38A552BA237}  CTocEntryList (C:\Windows\System32\wmvdspa.dll)
{3ADCE5CC-13C8-4573-B328-ED438EB694F9}  CAviTocParser (C:\Windows\System32\wmvdspa.dll)
{47354492-827E-4b8a-B318-C80EBA1381F0}  CClusterDetectorEx (C:\Windows\System32\wmvdspa.dll)
{499EAEEA-2737-4849-8BB6-47F107EAF358}  CTocParser (C:\Windows\System32\wmvdspa.dll)
{4DDA1941-77A0-4fb1-A518-E2185041D70C}  CTocGeneratorDmo (C:\Windows\System32\wmvdspa.dll)
{4FE24495-28CE-4920-A4C4-E556E1F0DF2A}  CToc (C:\Windows\System32\wmvdspa.dll)
{5058292D-A244-4840-AB44-480975C4FFE4}  CTocCollection (C:\Windows\System32\wmvdspa.dll)
{559C6BAD-1EA8-4963-A087-8A6810F9218B}  CThumbnailGeneratorDmo (C:\Windows\System32\wmvdspa.dll)
{9B77C0F2-8735-46c5-B90F-5F0B303EF6AB}  CAsfTocParser (C:\Windows\System32\wmvdspa.dll)
{F22F5E05-585C-4def-8523-6555CFBC0CB3}  CTocEntry (C:\Windows\System32\wmvdspa.dll)

{70f598e9-f4ab-495a-99e2-a7c4d3d89abf}  WMAudio Encoder DMO (C:\Windows\System32\WMADMOE.DLL)

{187463A0-5BB7-11D3-ACBE-0080C75E246E}  WM ASF Reader (C:\Windows\System32\qasf.dll)
{71932D43-3CA5-46EF-B013-3F9A695996ED}  DMO Wrapper Filter (C:\Windows\System32\qasf.dll)
{7C23220E-55BB-11D3-8B16-00C04FB6BD3D}  WM ASF Writer (C:\Windows\System32\qasf.dll)
{94297043-BD82-4DFD-B0DE-8177739C6D20}  DMO Wrapper Filter (C:\Windows\System32\qasf.dll)
{D9F9C262-6231-11D3-8B1D-00C04FB6BD3D}  WM ASF Writer Properties (C:\Windows\System32\qasf.dll)

{7bafb3b1-d8f4-4279-9253-27da423108de}  WMV Screen decoder DMO (C:\Windows\System32\wmvsdecd.dll)

{7e320092-596a-41b2-bbeb-175d10504eb6}  WMVideo8 Encoder DMO (C:\Windows\System32\wmvxencd.dll)

{874131cb-4ecc-443b-8948-746b89595d20}  WMSpeech Decoder DMO (C:\Windows\System32\WMSPDMOD.DLL)

{bbeea841-0a63-4f52-a7ab-a9b3a84ed38a}  MP3 Decoder DMO (C:\Windows\System32\mp3dmod.dll)

{cba9e78b-49a3-49ea-93d4-6bcba8c4de07}  Mpeg43 Decoder DMO (C:\Windows\System32\mp43decd.dll)

{d23b90d0-144f-46bd-841d-59e4eb19dc59}  WMVideo9 Encoder DMO (C:\Windows\System32\wmvencod.dll)

{f371728a-6052-4d47-827c-d039335dfe0a}  Mpeg4 Decoder DMO (C:\Windows\System32\mpg4decd.dll)

{f7ffe0a0-a4f5-44b5-949e-15ed2bc66f9d}  MSScreen 9 encoder DMO (C:\Windows\System32\wmvsencd.dll)


mfts

{0567cea8-7b61-4cd4-a9d0-58073b9888f0}  HDCP Decryptor MFT (C:\Windows\System32\HdcpHandler.dll)
{10794fd1-36a6-4527-8e61-691856d5aa34}  HDCP Receiver Control (C:\Windows\System32\HdcpHandler.dll)
{2b8f8a9e-51b6-46db-9d45-ffb33e023823}  HDCP Manager (C:\Windows\System32\HdcpHandler.dll)

{068a8476-9229-4cc0-9d49-2fc699dcd30a}  LPCM DVD-Audio MFT (C:\Windows\System32\mfaudiocnv.dll)
{cf5eeedf-0e92-4b3b-a161-bd0ffe545e4b}  Dolby TrueHD IEC-61937 converter MFT (C:\Windows\System32\mfaudiocnv.dll)
{d035e24c-c877-42d7-a795-2a8a339b472f}  DTS IEC-61937 converter MFT (C:\Windows\System32\mfaudiocnv.dll)

{128509e9-c44e-45dc-95e9-c255b8f466a6}  Microsoft FLAC Audio Encoder MFT (C:\Windows\System32\MSFlacEncoder.dll)
{7d39c56f-6075-47c9-9bae-8cf9e531b5f5}  MF FLAC Sink Class Factory (C:\Windows\System32\msflacencoder.dll)

{177C0AFE-900B-48d4-9E4C-57ADD250B3D4}  Microsoft Dolby Digital Plus Decoder MFT (C:\Windows\System32\DolbyDecMFT.dll)

{265011AE-5481-4f77-A295-ABB6FFE8D63E}  MS AMRNB Decoder MFT (C:\Windows\System32\MSAMRNBDecoder.dll)

{2FAE8AFE-04A3-423a-A814-85DB454712B0}  MS AMRNB Encoder MFT (C:\Windows\System32\MSAMRNBEncoder.dll)

{42150CD9-CA9A-4EA5-9939-30EE037F6E74}  Microsoft MPEG-2 Video Encoder (C:\Windows\System32\msmpeg2enc.dll)
{46A4DD5C-73F8-4304-94DF-308F760974F4}  Microsoft MPEG-2 Audio Encoder MFT (C:\Windows\System32\msmpeg2enc.dll)
{5F5AFF4A-2F7F-4279-88C2-CD88EB39D144}  Microsoft MPEG-2 Encoder (C:\Windows\System32\msmpeg2enc.dll)
{ACD453BC-C58A-44D1-BBF5-BFB325BE2D78}  Microsoft MPEG-2 Audio Encoder (C:\Windows\System32\msmpeg2enc.dll)
{E6335F02-80B7-4DC4-ADFA-DFE7210D20D5}  Microsoft MPEG-2 Video Encoder MFT (C:\Windows\System32\msmpeg2enc.dll)
{FA5FE7C5-6A1D-4B11-B41F-F959D6C76500}  Microsoft MPEG-2 Media Foundation Media Sink (C:\Windows\System32\msmpeg2enc.dll)

{63e17c10-2d43-4c42-8fe3-8d8b63e46a6a}  Microsoft Opus Audio Decoder MFT (C:\Windows\System32\MSOpusDecoder.dll)

{0E41CFB8-0506-40F4-A516-77CC23642D91}  MF FLAC Media Source ByteStreamHandler (C:\Windows\System32\MSFlacDecoder.dll)
{6B0B3E6B-A2C5-4514-8055-AFE8A95242D9}  Microsoft FLAC Audio Decoder MFT (C:\Windows\System32\MSFlacDecoder.dll)
{C10E23A2-BFF0-4113-BCE8-C91BE754E281}  MF FLAC Media Property Handler (C:\Windows\System32\msflacdecoder.dll)

{9AB6A28C-748E-4B6A-BFFF-CC443B8E8FB4}  Microsoft ALAC Audio Encoder MFT (C:\Windows\System32\MSAlacEncoder.dll)

{AC3315C9-F481-45D7-826C-0B406C1F64B8}  Microsoft Dolby Digital Encoder MFT (C:\Windows\System32\msac3enc.dll)
{C6B400E2-20A7-4E58-A2FE-24619682CE6C}  Microsoft AC3 Encoder (C:\Windows\System32\msac3enc.dll)

{C0CD7D12-31FC-4BBC-B363-7322EE3E1879}  Microsoft ALAC Audio Decoder MFT (C:\Windows\System32\MSAlacDecoder.dll)

{C1E565E2-F2DE-4537-9612-2F30A160EB5C}  FaceDetectionMFT (C:\Windows\System32\MSPhotography.dll)
{DF5440C1-FFB1-4F14-99A9-D692CC978E07}  SceneAnalysisMFT (C:\Windows\System32\MSPhotography.dll)

quartz:

{05589FAF-C356-11CE-BF01-00AA0055595A}  Audio Renderer Property Page (C:\Windows\System32\quartz.dll)
{060AF76C-68DD-11D0-8FC1-00C04FD9189D}  Seeking (C:\Windows\System32\quartz.dll)
{06B32AEE-77DA-484B-973B-5D64F47201B0}  VMR Mixer (C:\Windows\System32\quartz.dll)
{07B65360-C445-11CE-AFDE-00AA006C14F4}  MIDI Renderer (C:\Windows\System32\quartz.dll)
{1643E180-90F5-11CE-97D5-00AA0055595A}  Color Space Converter (C:\Windows\System32\quartz.dll)
{1B544C20-FD0B-11CE-8C63-00AA0044B51E}  AVI Splitter (C:\Windows\System32\quartz.dll)
{1DA08500-9EDC-11CF-BC10-00AA00AC74F6}  VGA 16 color ditherer (C:\Windows\System32\quartz.dll)
{1E651CC0-B199-11D0-8212-00C04FC32C45}  Memory Allocator (C:\Windows\System32\quartz.dll)
{2D2E24CB-0CD5-458F-86EA-3E6FA22C8E64}  VMR Allocator Presenter 9 (C:\Windows\System32\quartz.dll)
{301056D0-6DFF-11D2-9EEB-006008039E37}  MJPEG Decompressor (C:\Windows\System32\quartz.dll)
{336475D0-942A-11CE-A870-00AA002FEAB5}  MPEG-I Stream Splitter (C:\Windows\System32\quartz.dll)
{33FACFE0-A9BE-11D0-A520-00A0D10129C0}  SAMI (CC) Reader (C:\Windows\System32\quartz.dll)
{37E92A92-D9AA-11D2-BF84-8EF2B1555AED}  Audio Renderer Advanced Properties (C:\Windows\System32\quartz.dll)
{418AFB70-F8B8-11CE-AAC6-0020AF0B99A3}  Quality Management Property Page (C:\Windows\System32\quartz.dll)
{4444AC9E-242E-471B-A3C7-45DCD46352BC}  VMR Allocator Presenter DDXcl Mode (C:\Windows\System32\quartz.dll)
{48025243-2D39-11CE-875D-00608CB78066}  Internal Text Renderer (C:\Windows\System32\quartz.dll)
{4A2286E0-7BEF-11CE-9BD9-0000E202599C}  MPEG Audio Codec (C:\Windows\System32\quartz.dll)
{51B4ABF3-748F-4E3B-A276-C828330E926A}  Video Mixing Renderer 9 (C:\Windows\System32\quartz.dll)
{59CE6880-ACF8-11CF-B56E-0080C7C4B68A}  Performance Property Page (C:\Windows\System32\quartz.dll)
{6A08CF80-0E18-11CF-A24D-0020AFD79767}  ACM Wrapper (C:\Windows\System32\quartz.dll)
{6BC1CFFA-8FC1-4261-AC22-CFB4CC38DB50}  Default Video Renderer (C:\Windows\System32\quartz.dll)
{6F26A6CD-967B-47FD-874A-7AED2C9D25A2}  Video Port Manager (C:\Windows\System32\quartz.dll)
{70E102B0-5556-11CE-97C0-00AA0055595A}  Video Renderer (C:\Windows\System32\quartz.dll)
{79376820-07D0-11CF-A24D-0020AFD79767}  DirectSound Audio Renderer (C:\Windows\System32\quartz.dll)
{7D8AA343-6E63-4663-BE90-6B80F66540A3}  VMR ImageSync (C:\Windows\System32\quartz.dll)
{8670C736-F614-427b-8ADA-BBADC587194B}  DirectShow Plugin Manager (C:\Windows\System32\quartz.dll)
{944D4C00-DD52-11CE-BF0E-00AA0055595A}  DirectDraw Property Page (C:\Windows\System32\quartz.dll)
{99D54F63-1A69-41AE-AA4D-C976EB3F0713}  VMR Allocator Presenter (C:\Windows\System32\quartz.dll)
{A3ECBC41-581A-4476-B693-A63340462D8B}  Filter Graph Private Thread (C:\Windows\System32\quartz.dll)
{A888DF60-1E90-11CF-AC98-00AA004C0FA9}  AVI Draw Filter (C:\Windows\System32\quartz.dll)
{A8DFB9A0-8A20-479F-B538-9387C5EEBA2B}  VMR Mixer 9 (C:\Windows\System32\quartz.dll)
{B80AB0A0-7416-11D2-9EEB-006008039E37}  MJPEG Compressor (C:\Windows\System32\quartz.dll)
{B87BEB7B-8D29-423F-AE4D-6582C10175AC}  Video Mixing Renderer (C:\Windows\System32\quartz.dll)
{CC785860-B2CA-11CE-8D2B-0000E202599C}  MPEG Audio Decoder Property Page (C:\Windows\System32\quartz.dll)
{CDA42200-BD88-11D0-BD4E-00A0C911CE86}  Filter Mapper2 (C:\Windows\System32\quartz.dll)
{CDBD8D00-C193-11D0-BD4E-00A0C911CE86}  CMediaPropertyBag (C:\Windows\System32\quartz.dll)
{CF49D4E0-1115-11CE-B03A-0020AF0BA770}  AVI Decompressor (C:\Windows\System32\quartz.dll)
{D3588AB0-0781-11CE-B03A-0020AF0BA770}  AVI/WAV File Source (C:\Windows\System32\quartz.dll)
{D51BD5A1-7548-11CF-A520-0080C77EF58A}  Wave Parser (C:\Windows\System32\quartz.dll)
{D51BD5A2-7548-11CF-A520-0080C77EF58A}  MIDI Parser (C:\Windows\System32\quartz.dll)
{D51BD5A3-7548-11CF-A520-0080C77EF58A}  Multi-file Parser (C:\Windows\System32\quartz.dll)
{D51BD5A5-7548-11CF-A520-0080C77EF58A}  File stream renderer (C:\Windows\System32\quartz.dll)
{E30629D1-27E5-11CE-875D-00608CB78066}  Audio Renderer (C:\Windows\System32\quartz.dll)
{E4206432-01A1-4BEE-B3E1-3702C8EDC574}  Line 21 Decoder 2 (C:\Windows\System32\quartz.dll)
{E436EBB1-524F-11CE-9F53-0020AF0BA770}  System Clock (C:\Windows\System32\quartz.dll)
{E436EBB2-524F-11CE-9F53-0020AF0BA770}  Filter Mapper (C:\Windows\System32\quartz.dll)
{E436EBB3-524F-11CE-9F53-0020AF0BA770}  Filter Graph (C:\Windows\System32\quartz.dll)
{E436EBB5-524F-11CE-9F53-0020AF0BA770}  File Source (Async.) (C:\Windows\System32\quartz.dll)
{E436EBB6-524F-11CE-9F53-0020AF0BA770}  File Source (URL) (C:\Windows\System32\quartz.dll)
{E436EBB7-524F-11CE-9F53-0020AF0BA770}  IPersistMoniker Plug In Distributor (C:\Windows\System32\quartz.dll)
{E436EBB8-524F-11CE-9F53-0020AF0BA770}  Filter Graph no thread (C:\Windows\System32\quartz.dll)
{E4979309-7A32-495E-8A92-7B014AAD4961}  VMR ImageSync 9 (C:\Windows\System32\quartz.dll)
{E5B4EAA0-B2CA-11CE-8D2B-0000E202599C}  MPEG Video Decoder Property Page (C:\Windows\System32\quartz.dll)
{FEB50740-7BEF-11CE-9BD9-0000E202599C}  MPEG Video Codec (C:\Windows\System32\quartz.dll)


{2df7b51e-797b-4d06-be71-d14a52cf8421}  MF Store Object Activate (C:\Windows\System32\mfsvr.dll)
{4d4dedaa-43c5-480d-9ee0-1464f9f4ff4b}  MF SVR Activate (C:\Windows\System32\mfsvr.dll)
{63df056d-a8c7-4a2e-87ff-215de332c21f}  MF SVR Proxy/Stub (C:\Windows\System32\mfsvr.dll)


{404A6DE5-D4D6-4260-9BC7-5A6CBD882432}  (C:\Windows\System32\mfdvdec.dll)

{745057c7-f353-4f2d-a7ee-58434477730e}  AEC (C:\Windows\System32\mfwmaaec.dll)

{93AF0C51-2275-45D2-A35B-F2BA21CAED00}  (C:\Windows\System32\mfAACEnc.dll)

{bc47fcfe-98a0-4f27-bb07-698af24f2b38}  (C:\Windows\System32\mfh263enc.dll)

{CB17E772-E1CC-4633-8450-5617AF577905}  (C:\Windows\System32\mfmjpegdec.dll)

*/
};

/*******************************************************************************
 *      DllGetClassObject (mf.@)
 */
HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void **obj)
{
    unsigned int i;

    TRACE("%s, %s, %p.\n", debugstr_guid(rclsid), debugstr_guid(riid), obj);

    for (i = 0; i < ARRAY_SIZE(class_objects); ++i)
    {
        if (IsEqualGUID(class_objects[i].clsid, rclsid))
            return IClassFactory_QueryInterface(class_objects[i].factory, riid, obj);
    }

    WARN("%s: class not found.\n", debugstr_guid(rclsid));
    return CLASS_E_CLASSNOTAVAILABLE;
}

static HRESULT prop_string_vector_append(PROPVARIANT *vector, unsigned int *capacity, BOOL unique, const WCHAR *str)
{
    WCHAR *ptrW;
    int len, i;

    if (unique)
    {
        for (i = 0; i < vector->calpwstr.cElems; ++i)
        {
            if (!lstrcmpW(vector->calpwstr.pElems[i], str))
                return S_OK;
        }
    }

    if (!*capacity || *capacity - 1 < vector->calpwstr.cElems)
    {
        unsigned int new_count;
        WCHAR **ptr;

        new_count = *capacity ? *capacity * 2 : 10;
        ptr = CoTaskMemRealloc(vector->calpwstr.pElems, new_count * sizeof(*vector->calpwstr.pElems));
        if (!ptr)
            return E_OUTOFMEMORY;
        vector->calpwstr.pElems = ptr;
        *capacity = new_count;
    }

    len = lstrlenW(str);
    if (!(vector->calpwstr.pElems[vector->calpwstr.cElems] = ptrW = CoTaskMemAlloc((len + 1) * sizeof(WCHAR))))
        return E_OUTOFMEMORY;

    lstrcpyW(ptrW, str);
    vector->calpwstr.cElems++;

    return S_OK;
}

static int __cdecl qsort_string_compare(const void *a, const void *b)
{
    const WCHAR *left = *(const WCHAR **)a, *right = *(const WCHAR **)b;
    return lstrcmpW(left, right);
}

static HRESULT mf_get_handler_strings(const WCHAR *path, WCHAR filter, unsigned int maxlen, PROPVARIANT *dst)
{
    static const HKEY hkey_roots[2] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };
    unsigned int capacity = 0, count;
    HRESULT hr = S_OK;
    int i, index;
    WCHAR *buffW;
    DWORD size;

    if (!(buffW = calloc(maxlen, sizeof(*buffW))))
        return E_OUTOFMEMORY;

    memset(dst, 0, sizeof(*dst));
    dst->vt = VT_VECTOR | VT_LPWSTR;

    for (i = 0; i < ARRAY_SIZE(hkey_roots); ++i)
    {
        HKEY hkey;

        if (RegOpenKeyW(hkey_roots[i], path, &hkey))
            continue;

        index = 0;
        size = maxlen;
        count = dst->calpwstr.cElems;
        while (!RegEnumKeyExW(hkey, index++, buffW, &size, NULL, NULL, NULL, NULL))
        {
            if (filter && !wcschr(buffW, filter))
                continue;

            if (FAILED(hr = prop_string_vector_append(dst, &capacity, i > 0, buffW)))
                break;
            size = maxlen;
        }

        /* Sort last pass results. */
        qsort(&dst->calpwstr.pElems[count], dst->calpwstr.cElems - count, sizeof(*dst->calpwstr.pElems),
                qsort_string_compare);

        RegCloseKey(hkey);
    }

    if (FAILED(hr))
        PropVariantClear(dst);

    free(buffW);

    return hr;
}

/***********************************************************************
 *      MFGetSupportedMimeTypes (mf.@)
 */
HRESULT WINAPI MFGetSupportedMimeTypes(PROPVARIANT *dst)
{
    unsigned int maxlen;

    TRACE("%p.\n", dst);

    if (!dst)
        return E_POINTER;

    /* According to RFC4288 it's 127/127 characters. */
    maxlen = 127 /* type */ + 1 /* / */ + 127 /* subtype */ + 1;
    return mf_get_handler_strings(L"Software\\Microsoft\\Windows Media Foundation\\ByteStreamHandlers", '/',
            maxlen,  dst);
}

/***********************************************************************
 *      MFGetSupportedSchemes (mf.@)
 */
HRESULT WINAPI MFGetSupportedSchemes(PROPVARIANT *dst)
{
    TRACE("%p.\n", dst);

    if (!dst)
        return E_POINTER;

    return mf_get_handler_strings(L"Software\\Microsoft\\Windows Media Foundation\\SchemeHandlers", 0, 64, dst);
}

/***********************************************************************
 *      MFGetService (mf.@)
 */
HRESULT WINAPI MFGetService(IUnknown *object, REFGUID service, REFIID riid, void **obj)
{
    IMFGetService *gs;
    HRESULT hr;

    TRACE("(%p, %s, %s, %p)\n", object, debugstr_guid(service), debugstr_guid(riid), obj);

    if (!object)
        return E_POINTER;

    if (FAILED(hr = IUnknown_QueryInterface(object, &IID_IMFGetService, (void **)&gs)))
        return hr;

    hr = IMFGetService_GetService(gs, service, riid, obj);
    IMFGetService_Release(gs);
    return hr;
}

/***********************************************************************
 *      MFShutdownObject (mf.@)
 */
HRESULT WINAPI MFShutdownObject(IUnknown *object)
{
    IMFShutdown *shutdown;

    TRACE("%p.\n", object);

    if (object && SUCCEEDED(IUnknown_QueryInterface(object, &IID_IMFShutdown, (void **)&shutdown)))
    {
        IMFShutdown_Shutdown(shutdown);
        IMFShutdown_Release(shutdown);
    }

    return S_OK;
}

/***********************************************************************
 *      MFEnumDeviceSources (mf.@)
 */
HRESULT WINAPI MFEnumDeviceSources(IMFAttributes *attributes, IMFActivate ***sources, UINT32 *ret_count)
{
    GUID source_type;
    HRESULT hr;

    TRACE("%p, %p, %p.\n", attributes, sources, ret_count);

    if (!attributes || !sources || !ret_count)
        return E_INVALIDARG;

    if (FAILED(hr = IMFAttributes_GetGUID(attributes, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, &source_type)))
        return hr;

    if (IsEqualGUID(&source_type, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID))
    {
        FIXME("Not implemented for video capture devices.\n");
        *ret_count = 0;
        return S_OK;
    }
    if (IsEqualGUID(&source_type, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID))
        return enum_audio_capture_sources(attributes, sources, ret_count);

    return E_INVALIDARG;
}

struct simple_type_handler
{
    IMFMediaTypeHandler IMFMediaTypeHandler_iface;
    LONG refcount;
    IMFMediaType *media_type;
    CRITICAL_SECTION cs;
};

static struct simple_type_handler *impl_from_IMFMediaTypeHandler(IMFMediaTypeHandler *iface)
{
    return CONTAINING_RECORD(iface, struct simple_type_handler, IMFMediaTypeHandler_iface);
}

static HRESULT WINAPI simple_type_handler_QueryInterface(IMFMediaTypeHandler *iface, REFIID riid, void **obj)
{
    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), obj);

    if (IsEqualIID(riid, &IID_IMFMediaTypeHandler) ||
            IsEqualIID(riid, &IID_IUnknown))
    {
        *obj = iface;
        IMFMediaTypeHandler_AddRef(iface);
        return S_OK;
    }

    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI simple_type_handler_AddRef(IMFMediaTypeHandler *iface)
{
    struct simple_type_handler *handler = impl_from_IMFMediaTypeHandler(iface);
    ULONG refcount = InterlockedIncrement(&handler->refcount);

    TRACE("%p, refcount %lu.\n", iface, refcount);

    return refcount;
}

static ULONG WINAPI simple_type_handler_Release(IMFMediaTypeHandler *iface)
{
    struct simple_type_handler *handler = impl_from_IMFMediaTypeHandler(iface);
    ULONG refcount = InterlockedDecrement(&handler->refcount);

    TRACE("%p, refcount %lu.\n", iface, refcount);

    if (!refcount)
    {
        if (handler->media_type)
            IMFMediaType_Release(handler->media_type);
        DeleteCriticalSection(&handler->cs);
        free(handler);
    }

    return refcount;
}

static HRESULT WINAPI simple_type_handler_IsMediaTypeSupported(IMFMediaTypeHandler *iface, IMFMediaType *in_type,
        IMFMediaType **out_type)
{
    struct simple_type_handler *handler = impl_from_IMFMediaTypeHandler(iface);
    DWORD flags = 0;
    HRESULT hr;

    TRACE("%p, %p, %p.\n", iface, in_type, out_type);

    if (out_type)
        *out_type = NULL;

    EnterCriticalSection(&handler->cs);
    if (!handler->media_type)
        hr = MF_E_UNEXPECTED;
    else
    {
        if (SUCCEEDED(hr = IMFMediaType_IsEqual(handler->media_type, in_type, &flags)))
            hr = (flags & (MF_MEDIATYPE_EQUAL_MAJOR_TYPES | MF_MEDIATYPE_EQUAL_FORMAT_TYPES)) ==
                    (MF_MEDIATYPE_EQUAL_MAJOR_TYPES | MF_MEDIATYPE_EQUAL_FORMAT_TYPES) ? S_OK : E_FAIL;
    }
    LeaveCriticalSection(&handler->cs);

    return hr;
}

static HRESULT WINAPI simple_type_handler_GetMediaTypeCount(IMFMediaTypeHandler *iface, DWORD *count)
{
    TRACE("%p, %p.\n", iface, count);

    if (!count)
        return E_POINTER;

    *count = 1;

    return S_OK;
}

static HRESULT WINAPI simple_type_handler_GetMediaTypeByIndex(IMFMediaTypeHandler *iface, DWORD index,
        IMFMediaType **type)
{
    struct simple_type_handler *handler = impl_from_IMFMediaTypeHandler(iface);

    TRACE("%p, %lu, %p.\n", iface, index, type);

    if (index > 0)
        return MF_E_NO_MORE_TYPES;

    EnterCriticalSection(&handler->cs);
    *type = handler->media_type;
    if (*type)
        IMFMediaType_AddRef(*type);
    LeaveCriticalSection(&handler->cs);

    return S_OK;
}

static HRESULT WINAPI simple_type_handler_SetCurrentMediaType(IMFMediaTypeHandler *iface, IMFMediaType *media_type)
{
    struct simple_type_handler *handler = impl_from_IMFMediaTypeHandler(iface);

    TRACE("%p, %p.\n", iface, media_type);

    EnterCriticalSection(&handler->cs);
    if (handler->media_type)
        IMFMediaType_Release(handler->media_type);
    handler->media_type = media_type;
    if (handler->media_type)
        IMFMediaType_AddRef(handler->media_type);
    LeaveCriticalSection(&handler->cs);

    return S_OK;
}

static HRESULT WINAPI simple_type_handler_GetCurrentMediaType(IMFMediaTypeHandler *iface, IMFMediaType **media_type)
{
    struct simple_type_handler *handler = impl_from_IMFMediaTypeHandler(iface);

    TRACE("%p, %p.\n", iface, media_type);

    if (!media_type)
        return E_POINTER;

    EnterCriticalSection(&handler->cs);
    *media_type = handler->media_type;
    if (*media_type)
        IMFMediaType_AddRef(*media_type);
    LeaveCriticalSection(&handler->cs);

    return S_OK;
}

static HRESULT WINAPI simple_type_handler_GetMajorType(IMFMediaTypeHandler *iface, GUID *type)
{
    struct simple_type_handler *handler = impl_from_IMFMediaTypeHandler(iface);
    HRESULT hr;

    TRACE("%p, %p.\n", iface, type);

    EnterCriticalSection(&handler->cs);
    if (handler->media_type)
        hr = IMFMediaType_GetGUID(handler->media_type, &MF_MT_MAJOR_TYPE, type);
    else
        hr = MF_E_NOT_INITIALIZED;
    LeaveCriticalSection(&handler->cs);

    return hr;
}

static const IMFMediaTypeHandlerVtbl simple_type_handler_vtbl =
{
    simple_type_handler_QueryInterface,
    simple_type_handler_AddRef,
    simple_type_handler_Release,
    simple_type_handler_IsMediaTypeSupported,
    simple_type_handler_GetMediaTypeCount,
    simple_type_handler_GetMediaTypeByIndex,
    simple_type_handler_SetCurrentMediaType,
    simple_type_handler_GetCurrentMediaType,
    simple_type_handler_GetMajorType,
};

/*******************************************************************************
 *      MFCreateSimpleTypeHandler (mf.@)
 */
HRESULT WINAPI MFCreateSimpleTypeHandler(IMFMediaTypeHandler **handler)
{
    struct simple_type_handler *object;

    TRACE("%p.\n", handler);

    if (!(object = calloc(1, sizeof(*object))))
        return E_OUTOFMEMORY;

    object->IMFMediaTypeHandler_iface.lpVtbl = &simple_type_handler_vtbl;
    object->refcount = 1;
    InitializeCriticalSection(&object->cs);

    *handler = &object->IMFMediaTypeHandler_iface;

    return S_OK;
}

/*******************************************************************************
 *      MFRequireProtectedEnvironment (mf.@)
 */
HRESULT WINAPI MFRequireProtectedEnvironment(IMFPresentationDescriptor *pd)
{
    BOOL selected, protected = FALSE;
    unsigned int i = 0, value;
    IMFStreamDescriptor *sd;

    TRACE("%p.\n", pd);

    while (SUCCEEDED(IMFPresentationDescriptor_GetStreamDescriptorByIndex(pd, i++, &selected, &sd)))
    {
        value = 0;
        protected = SUCCEEDED(IMFStreamDescriptor_GetUINT32(sd, &MF_SD_PROTECTED, &value)) && value;
        IMFStreamDescriptor_Release(sd);
        if (protected) break;
    }

    return protected ? S_OK : S_FALSE;
}

static HRESULT create_media_sink(const CLSID *clsid, IMFByteStream *stream, IMFMediaType *video_type,
        IMFMediaType *audio_type, IMFMediaSink **sink)
{
    IMFSinkClassFactory *factory;
    HRESULT hr;

    if (FAILED(hr = CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IMFSinkClassFactory, (void **)&factory)))
        return hr;

    hr = IMFSinkClassFactory_CreateMediaSink(factory, stream, video_type, audio_type, sink);
    IMFSinkClassFactory_Release(factory);

    return hr;
}

/*******************************************************************************
 *      MFCreate3GPMediaSink (mf.@)
 */
HRESULT WINAPI MFCreate3GPMediaSink(IMFByteStream *stream, IMFMediaType *video_type,
         IMFMediaType *audio_type, IMFMediaSink **sink)
{
    TRACE("%p, %p, %p, %p.\n", stream, video_type, audio_type, sink);

    return create_media_sink(&CLSID_MF3GPSinkClassFactory, stream, video_type, audio_type, sink);
}

/*******************************************************************************
 *      MFCreateAC3MediaSink (mf.@)
 */
HRESULT WINAPI MFCreateAC3MediaSink(IMFByteStream *stream, IMFMediaType *audio_type, IMFMediaSink **sink)
{
    TRACE("%p, %p, %p.\n", stream, audio_type, sink);

    return create_media_sink(&CLSID_MFAC3SinkClassFactory, stream, NULL, audio_type, sink);
}

/*******************************************************************************
 *      MFCreateADTSMediaSink (mf.@)
 */
HRESULT WINAPI MFCreateADTSMediaSink(IMFByteStream *stream, IMFMediaType *audio_type, IMFMediaSink **sink)
{
    TRACE("%p, %p, %p.\n", stream, audio_type, sink);

    return create_media_sink(&CLSID_MFADTSSinkClassFactory, stream, NULL, audio_type, sink);
}

/*******************************************************************************
 *      MFCreateMP3MediaSink (mf.@)
 */
HRESULT WINAPI MFCreateMP3MediaSink(IMFByteStream *stream, IMFMediaSink **sink)
{
    TRACE("%p, %p.\n", stream, sink);

    return create_media_sink(&CLSID_MFMP3SinkClassFactory, stream, NULL, NULL, sink);
}

/*******************************************************************************
 *      MFCreateMPEG4MediaSink (mf.@)
 */
HRESULT WINAPI MFCreateMPEG4MediaSink(IMFByteStream *stream, IMFMediaType *video_type,
         IMFMediaType *audio_type, IMFMediaSink **sink)
{
    TRACE("%p, %p, %p, %p.\n", stream, video_type, audio_type, sink);

    return create_media_sink(&CLSID_MFMPEG4SinkClassFactory, stream, video_type, audio_type, sink);
}

/*******************************************************************************
 *      MFCreateFMPEG4MediaSink (mf.@)
 */
HRESULT WINAPI MFCreateFMPEG4MediaSink(IMFByteStream *stream, IMFMediaType *video_type,
         IMFMediaType *audio_type, IMFMediaSink **sink)
{
    TRACE("%p, %p, %p, %p.\n", stream, video_type, audio_type, sink);

    return create_media_sink(&CLSID_MFFMPEG4SinkClassFactory, stream, video_type, audio_type, sink);
}
