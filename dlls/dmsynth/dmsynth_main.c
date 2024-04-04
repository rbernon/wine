/* DirectMusicSynthesizer Main
 *
 * Copyright (C) 2003-2004 Rok Mandeljc
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


#include <stdio.h>

#include "dmsynth_private.h"
#include "rpcproxy.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmsynth);

typedef struct {
        IClassFactory IClassFactory_iface;
        HRESULT (*create_instance)(IUnknown **ret_iface);
} IClassFactoryImpl;

/******************************************************************
 *      IClassFactory implementation
 */
static inline IClassFactoryImpl *impl_from_IClassFactory(IClassFactory *iface)
{
        return CONTAINING_RECORD(iface, IClassFactoryImpl, IClassFactory_iface);
}

static HRESULT WINAPI ClassFactory_QueryInterface(IClassFactory *iface, REFIID riid, void **ppv)
{
        if (ppv == NULL)
                return E_POINTER;

        if (IsEqualGUID(&IID_IUnknown, riid))
                TRACE("(%p)->(IID_IUnknown %p)\n", iface, ppv);
        else if (IsEqualGUID(&IID_IClassFactory, riid))
                TRACE("(%p)->(IID_IClassFactory %p)\n", iface, ppv);
        else {
                FIXME("(%p)->(%s %p)\n", iface, debugstr_guid(riid), ppv);
                *ppv = NULL;
                return E_NOINTERFACE;
        }

        *ppv = iface;
        IUnknown_AddRef((IUnknown*)*ppv);
        return S_OK;
}

static ULONG WINAPI ClassFactory_AddRef(IClassFactory *iface)
{
        return 2; /* non-heap based object */
}

static ULONG WINAPI ClassFactory_Release(IClassFactory *iface)
{
        return 1; /* non-heap based object */
}

static HRESULT WINAPI ClassFactory_CreateInstance(IClassFactory *iface, IUnknown *unk_outer,
        REFIID riid, void **ret_iface)
{
    IClassFactoryImpl *This = impl_from_IClassFactory(iface);
    IUnknown *object;
    HRESULT hr;

    TRACE("(%p, %s, %p)\n", unk_outer, debugstr_dmguid(riid), ret_iface);

    *ret_iface = NULL;
    if (unk_outer) return CLASS_E_NOAGGREGATION;
    if (SUCCEEDED(hr = This->create_instance(&object)))
    {
        hr = IUnknown_QueryInterface(object, riid, ret_iface);
        IUnknown_Release(object);
    }

    return hr;
}

static HRESULT WINAPI ClassFactory_LockServer(IClassFactory *iface, BOOL dolock)
{
        TRACE("(%d)\n", dolock);
        return S_OK;
}

static const IClassFactoryVtbl classfactory_vtbl = {
        ClassFactory_QueryInterface,
        ClassFactory_AddRef,
        ClassFactory_Release,
        ClassFactory_CreateInstance,
        ClassFactory_LockServer
};

static IClassFactoryImpl Synth_CF = {{&classfactory_vtbl}, synth_create};
static IClassFactoryImpl SynthSink_CF = {{&classfactory_vtbl}, synth_sink_create};


/******************************************************************
 *		DllGetClassObject (DMSYNTH.@)
 *
 *
 */
HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv)
{
    TRACE("(%s, %s, %p)\n", debugstr_dmguid(rclsid), debugstr_dmguid(riid), ppv);

    if (IsEqualCLSID(rclsid, &CLSID_DirectMusicSynth) && IsEqualIID(riid, &IID_IClassFactory)) {
        *ppv = &Synth_CF;
        IClassFactory_AddRef((IClassFactory*)*ppv);
        return S_OK;
    } else if (IsEqualCLSID(rclsid, &CLSID_DirectMusicSynthSink) && IsEqualIID(riid, &IID_IClassFactory)) {
        *ppv = &SynthSink_CF;
        IClassFactory_AddRef((IClassFactory*)*ppv);
        return S_OK;
    }

    WARN("(%s,%s,%p): no interface found.\n", debugstr_dmguid(rclsid), debugstr_dmguid(riid), ppv);
    return CLASS_E_CLASSNOTAVAILABLE;
}
