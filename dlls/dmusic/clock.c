/*
 * IReferenceClock Implementation
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

#include "dmusic_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmusic);

struct reference_clock
{
    IReferenceClock IReferenceClock_iface;
    LONG ref;

    REFERENCE_TIME time;
    DMUS_CLOCKINFO info;
};

static inline struct reference_clock *impl_from_IReferenceClock(IReferenceClock *iface)
{
    return CONTAINING_RECORD(iface, struct reference_clock, IReferenceClock_iface);
}

static HRESULT WINAPI reference_clock_QueryInterface(IReferenceClock *iface, REFIID riid, void **ref_iface)
{
	struct reference_clock *This = impl_from_IReferenceClock(iface);
	TRACE("(%p, %s, %p)\n", This, debugstr_dmguid(riid), ref_iface);

	if (IsEqualIID (riid, &IID_IUnknown) || 
	    IsEqualIID (riid, &IID_IReferenceClock)) {
		IUnknown_AddRef(iface);
		*ref_iface = This;
		return S_OK;
	}
	WARN("(%p, %s, %p): not found\n", This, debugstr_dmguid(riid), ref_iface);
	return E_NOINTERFACE;
}

static ULONG WINAPI reference_clock_AddRef(IReferenceClock *iface)
{
    struct reference_clock *This = impl_from_IReferenceClock(iface);
    ULONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p): new ref = %lu\n", This, ref);

    return ref;
}

static ULONG WINAPI reference_clock_Release(IReferenceClock *iface)
{
    struct reference_clock *This = impl_from_IReferenceClock(iface);
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p): new ref = %lu\n", This, ref);

    if (!ref) {
        free(This);
    }

    return ref;
}

static HRESULT WINAPI reference_clock_GetTime(IReferenceClock *iface, REFERENCE_TIME *time)
{
    struct reference_clock *This = impl_from_IReferenceClock(iface);

    TRACE("(%p)->(%p)\n", This, time);

    *time = This->time;

    return S_OK;
}

static HRESULT WINAPI reference_clock_AdviseTime(IReferenceClock *iface, REFERENCE_TIME base,
        REFERENCE_TIME offset, HEVENT event, DWORD_PTR *cookie)
{
    struct reference_clock *This = impl_from_IReferenceClock(iface);
    FIXME("(%p)->(%I64d, %I64d, %#Ix, %p): stub\n", This, base, offset, event, cookie);
    return S_OK;
}

static HRESULT WINAPI reference_clock_AdvisePeriodic(IReferenceClock *iface, REFERENCE_TIME start,
        REFERENCE_TIME period, HSEMAPHORE semaphore, DWORD_PTR *cookie)
{
    struct reference_clock *This = impl_from_IReferenceClock(iface);
    FIXME("(%p)->(%I64d, %I64d, %#Ix, %p): stub\n", This, start, period, semaphore, cookie);
    return S_OK;
}

static HRESULT WINAPI reference_clock_Unadvise(IReferenceClock *iface, DWORD_PTR cookie)
{
    struct reference_clock *This = impl_from_IReferenceClock(iface);
    FIXME("(%p, %#Ix): stub\n", This, cookie);
    return S_OK;
}

static const IReferenceClockVtbl reference_clock_vtbl =
{
	reference_clock_QueryInterface,
	reference_clock_AddRef,
	reference_clock_Release,
	reference_clock_GetTime,
	reference_clock_AdviseTime,
	reference_clock_AdvisePeriodic,
	reference_clock_Unadvise,
};

HRESULT reference_clock_create(IReferenceClock **ret_iface)
{
    struct reference_clock *clock;

    TRACE("(%p)\n", ret_iface);

    *ret_iface = NULL;
    if (!(clock = calloc(1, sizeof(*clock)))) return E_OUTOFMEMORY;
    clock->IReferenceClock_iface.lpVtbl = &reference_clock_vtbl;
    clock->ref = 1;
    clock->time = 0;
    clock->info.dwSize = sizeof(DMUS_CLOCKINFO);

    TRACE("Created ReferenceClock %p\n", clock);
    *ret_iface = &clock->IReferenceClock_iface;
    return S_OK;
}
