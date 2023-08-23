/*
 * Copyright (C) 2023 Rémi Bernon for CodeWeavers
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
#include <stddef.h>

#include <windef.h>
#include <winbase.h>

#define COBJMACROS
#include "objidl.h"

#include "dmusici.h"
#include "dmusicf.h"
#include "dmusics.h"

#include "wine/test.h"

static const GUID IID_IDirectMusicInstrumentPRIVATE = { 0xbcb20080, 0xa40c, 0x11d1, { 0x86, 0xbc, 0x00, 0xc0, 0x4f, 0xbf, 0x8f, 0xef } };

typedef struct IDirectMusicInstrumentPRIVATE
{
    void *lpVtbl;
} IDirectMusicInstrumentPRIVATE;

struct test_instrument
{
    IDirectMusicInstrument IDirectMusicInstrument_iface;
    IDirectMusicInstrumentPRIVATE IDirectMusicInstrumentPRIVATE_iface;
    LONG ref;

    char buffer[1024];
};

static struct test_instrument *impl_from_IDirectMusicInstrument(IDirectMusicInstrument *iface)
{
    return CONTAINING_RECORD(iface, struct test_instrument, IDirectMusicInstrument_iface);
}

static HRESULT WINAPI test_instrument_QueryInterface(IDirectMusicInstrument *iface, REFIID iid, void **out)
{
    struct test_instrument *instrument = impl_from_IDirectMusicInstrument(iface);

    if (IsEqualIID(iid, &IID_IUnknown)
            || IsEqualIID(iid, &IID_IDirectMusicInstrument))
    {
        IDirectMusicInstrument_AddRef(iface);
        *out = iface;
        return S_OK;
    }

    if (IsEqualCLSID(iid, &IID_IDirectMusicInstrumentPRIVATE))
    {
        IDirectMusicInstrument_AddRef(iface);
        *out = &instrument->IDirectMusicInstrumentPRIVATE_iface;
        return S_OK;
    }

    ok(0,"unexpected %s, iid %s\n", __func__, debugstr_guid(iid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI test_instrument_AddRef(IDirectMusicInstrument *iface)
{
    struct test_instrument *instrument = impl_from_IDirectMusicInstrument(iface);
    return InterlockedIncrement(&instrument->ref);
}

static ULONG WINAPI test_instrument_Release(IDirectMusicInstrument *iface)
{
    struct test_instrument *instrument = impl_from_IDirectMusicInstrument(iface);
    ULONG ref = InterlockedDecrement(&instrument->ref);

    if (!ref)
        free(instrument);

    return ref;
}

static HRESULT WINAPI test_instrument_GetPatch(IDirectMusicInstrument *iface, DWORD *patch)
{
    ok(0, "unexpected %s\n", __func__);
    return S_OK;
}

static HRESULT WINAPI test_instrument_SetPatch(IDirectMusicInstrument *iface, DWORD patch)
{
    ok(0, "unexpected %s\n", __func__);
    return S_OK;
}

static IDirectMusicInstrumentVtbl test_instrument_vtbl =
{
    test_instrument_QueryInterface,
    test_instrument_AddRef,
    test_instrument_Release,
    test_instrument_GetPatch,
    test_instrument_SetPatch,
};

static HRESULT WINAPI test_instrument_private_QueryInterface(void *iface, REFIID iid, void **out)
{
    struct test_instrument *instrument = CONTAINING_RECORD(iface, struct test_instrument, IDirectMusicInstrumentPRIVATE_iface);
    return IDirectMusicInstrument_QueryInterface(&instrument->IDirectMusicInstrument_iface, iid, out);
}

static ULONG WINAPI test_instrument_private_AddRef(void *iface)
{
    struct test_instrument *instrument = CONTAINING_RECORD(iface, struct test_instrument, IDirectMusicInstrumentPRIVATE_iface);
    return IDirectMusicInstrument_AddRef(&instrument->IDirectMusicInstrument_iface);
}

static ULONG WINAPI test_instrument_private_Release(void *iface)
{
    struct test_instrument *instrument = CONTAINING_RECORD(iface, struct test_instrument, IDirectMusicInstrumentPRIVATE_iface);
    return IDirectMusicInstrument_Release(&instrument->IDirectMusicInstrument_iface);
}

static HRESULT WINAPI test_instrument_private_Unknown(void *iface)
{
    ok(0, "unexpected %s\n", __func__);
    return S_OK;
}

static HRESULT (WINAPI *test_instrument_private_vtbl[])(void *iface) =
{
    (void *)test_instrument_private_QueryInterface,
    (void *)test_instrument_private_AddRef,
    (void *)test_instrument_private_Release,
    test_instrument_private_Unknown,
    test_instrument_private_Unknown,
    test_instrument_private_Unknown,
    test_instrument_private_Unknown,
    test_instrument_private_Unknown,
    test_instrument_private_Unknown,
    test_instrument_private_Unknown,
    test_instrument_private_Unknown,
    test_instrument_private_Unknown,
    test_instrument_private_Unknown,
    test_instrument_private_Unknown,
    test_instrument_private_Unknown,
    test_instrument_private_Unknown,
    test_instrument_private_Unknown,
};

static HRESULT test_instrument_create(IDirectMusicInstrument **out)
{
    struct test_instrument *instrument;

    *out = NULL;
    if (!(instrument = calloc(1, sizeof(*instrument)))) return E_OUTOFMEMORY;
    instrument->IDirectMusicInstrument_iface.lpVtbl = &test_instrument_vtbl;
    instrument->IDirectMusicInstrumentPRIVATE_iface.lpVtbl = &test_instrument_private_vtbl;
    instrument->ref = 1;

    *out = &instrument->IDirectMusicInstrument_iface;
    return S_OK;
}
