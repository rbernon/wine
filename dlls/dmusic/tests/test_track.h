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

#include "wine/test.h"

struct test_track
{
    IDirectMusicTrack8 IDirectMusicTrack8_iface;
    LONG ref;
};

static struct test_track *impl_from_IDirectMusicTrack8(IDirectMusicTrack8 *iface)
{
    return CONTAINING_RECORD(iface, struct test_track, IDirectMusicTrack8_iface);
}

static HRESULT WINAPI test_track_QueryInterface(IDirectMusicTrack8 *iface, REFIID iid, void **out)
{
    if (IsEqualIID(iid, &IID_IUnknown)
            || IsEqualIID(iid, &IID_IDirectMusicTrack8))
    {
        IDirectMusicTrack8_AddRef(iface);
        *out = iface;
        return S_OK;
    }

    ok(0,"unexpected %s, iid %s\n", __func__, debugstr_guid(iid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI test_track_AddRef(IDirectMusicTrack8 *iface)
{
    struct test_track *track = impl_from_IDirectMusicTrack8(iface);
    return InterlockedIncrement(&track->ref);
}

static ULONG WINAPI test_track_Release(IDirectMusicTrack8 *iface)
{
    struct test_track *track = impl_from_IDirectMusicTrack8(iface);
    ULONG ref = InterlockedDecrement(&track->ref);

    if (!ref)
        free(track);

    return ref;
}

static HRESULT WINAPI test_track_Init(IDirectMusicTrack8 *iface, IDirectMusicSegment *segment)
{
    ok(0,"unexpected %s, segment %p\n", __func__, segment);
    return S_OK;
}

static HRESULT WINAPI test_track_InitPlay(IDirectMusicTrack8 *iface,
        IDirectMusicSegmentState *state, IDirectMusicPerformance *performance,
        void **state_data, DWORD track_id, DWORD flags)
{
    ok(0,"unexpected %s state %p performance %p state_data %p track_id %#lx flags %#lx\n", __func__, state, performance, state_data, track_id, flags);
    *state_data = NULL;
    return S_OK;
}

static HRESULT WINAPI test_track_EndPlay(IDirectMusicTrack8 *iface, void *pStateData)
{
    ok(0,"unexpected %s\n", __func__);
    return S_OK;
}

static HRESULT WINAPI test_track_Play(IDirectMusicTrack8 *iface, void *pStateData,
        MUSIC_TIME mtStart, MUSIC_TIME mtEnd, MUSIC_TIME mtOffset, DWORD dwFlags,
        IDirectMusicPerformance *pPerf, IDirectMusicSegmentState *pSegSt, DWORD dwVirtualID)
{
    ok(0,"unexpected %s\n", __func__);
    return S_OK;
}

static HRESULT WINAPI test_track_GetParam(IDirectMusicTrack8 *iface, REFGUID type,
        MUSIC_TIME time, MUSIC_TIME *next, void *param)
{
    ok(0,"unexpected %s\n", __func__);
    return S_OK;
}

static HRESULT WINAPI test_track_SetParam(IDirectMusicTrack8 *iface, REFGUID type,
        MUSIC_TIME time, void *param)
{
    ok(0,"unexpected %s\n", __func__);
    return S_OK;
}

static HRESULT WINAPI test_track_IsParamSupported(IDirectMusicTrack8 *iface, REFGUID type)
{
    ok(0,"unexpected %s type %s\n", __func__, debugstr_guid(type));
    if (IsEqualGUID(type, &GUID_TimeSignature)) return S_OK;
    if (IsEqualGUID(type, &GUID_TempoParam)) return S_OK;
    return DMUS_E_TYPE_UNSUPPORTED;
}

static HRESULT WINAPI test_track_AddNotificationType(IDirectMusicTrack8 *iface,
        REFGUID notiftype)
{
    ok(0,"unexpected %s\n", __func__);
    return S_OK;
}

static HRESULT WINAPI test_track_RemoveNotificationType(IDirectMusicTrack8 *iface,
        REFGUID notiftype)
{
    ok(0,"unexpected %s\n", __func__);
    return S_OK;
}

static HRESULT WINAPI test_track_Clone(IDirectMusicTrack8 *iface, MUSIC_TIME mtStart,
        MUSIC_TIME mtEnd, IDirectMusicTrack **ppTrack)
{
    ok(0,"unexpected %s\n", __func__);
    return S_OK;
}

static HRESULT WINAPI test_track_PlayEx(IDirectMusicTrack8 *iface, void *state_data,
        REFERENCE_TIME start, REFERENCE_TIME end, REFERENCE_TIME offset, DWORD flags,
        IDirectMusicPerformance *performance, IDirectMusicSegmentState *state, DWORD track_id)
{
    ok(0,"unexpected %s state %p start %I64d end %I64d offset %I64d flags %#lx performance %p state_data %p track_id %#lx\n", __func__,
        state, start, end, offset, flags, performance, state_data, track_id);
    return S_OK;
}

static HRESULT WINAPI test_track_GetParamEx(IDirectMusicTrack8 *iface, REFGUID type,
        REFERENCE_TIME time, REFERENCE_TIME *next, void *param, void *state, DWORD flags)
{
    ok(0,"unexpected %s time %I64d next %p param %p state %p flags %#lx\n", __func__, time, next, param, state, flags);
    return S_OK;
}

static HRESULT WINAPI test_track_SetParamEx(IDirectMusicTrack8 *iface, REFGUID type,
        REFERENCE_TIME time, void *param, void *state, DWORD flags)
{
    ok(0,"unexpected %s\n", __func__);
    return S_OK;
}

static HRESULT WINAPI test_track_Compose(IDirectMusicTrack8 *iface, IUnknown *context,
        DWORD trackgroup, IDirectMusicTrack **track)
{
    ok(0,"unexpected %s\n", __func__);
    return S_OK;
}

static HRESULT WINAPI test_track_Join(IDirectMusicTrack8 *iface, IDirectMusicTrack *newtrack,
        MUSIC_TIME join, IUnknown *context, DWORD trackgroup, IDirectMusicTrack **resulttrack)
{
    ok(0,"unexpected %s\n", __func__);
    return S_OK;
}

static IDirectMusicTrack8Vtbl test_track_vtbl =
{
    test_track_QueryInterface,
    test_track_AddRef,
    test_track_Release,
    test_track_Init,
    test_track_InitPlay,
    test_track_EndPlay,
    test_track_Play,
    test_track_GetParam,
    test_track_SetParam,
    test_track_IsParamSupported,
    test_track_AddNotificationType,
    test_track_RemoveNotificationType,
    test_track_Clone,
    test_track_PlayEx,
    test_track_GetParamEx,
    test_track_SetParamEx,
    test_track_Compose,
    test_track_Join,
};

static inline HRESULT test_track_create(IDirectMusicTrack8 **out)
{
    struct test_track *track;

    *out = NULL;
    if (!(track = calloc(1, sizeof(*track)))) return E_OUTOFMEMORY;
    track->IDirectMusicTrack8_iface.lpVtbl = &test_track_vtbl;
    track->ref = 1;

    *out = &track->IDirectMusicTrack8_iface;
    return S_OK;
}
