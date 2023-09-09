/*
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

#define COBJMACROS
#include "dmtrack.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmtrack);

static inline struct track *impl_from_IDirectMusicTrack8(IDirectMusicTrack8 *iface)
{
    return CONTAINING_RECORD(iface, struct track, IDirectMusicTrack8_iface);
}

static HRESULT WINAPI track_QueryInterface(IDirectMusicTrack8 *iface, REFIID riid, void **ret_iface)
{
    struct track *track = impl_from_IDirectMusicTrack8(iface);

    TRACE("(%p, %s, %p)\n", track, debugstr_dmguid(riid), ret_iface);

    if (IsEqualIID(riid, &IID_IUnknown)
            || IsEqualIID(riid, &IID_IDirectMusicTrack)
            || IsEqualIID(riid, &IID_IDirectMusicTrack8))
    {
        *ret_iface = &track->IDirectMusicTrack8_iface;
        IDirectMusicTrack8_AddRef(&track->IDirectMusicTrack8_iface);
        return S_OK;
    }

    if (IsEqualIID(riid, &IID_IPersistStream))
    {
        *ret_iface = &track->IPersistStream_iface;
        IPersistStream_AddRef(&track->IPersistStream_iface);
        return S_OK;
    }

    WARN("(%p, %s, %p): not found\n", track, debugstr_dmguid(riid), ret_iface);
    *ret_iface = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI track_AddRef(IDirectMusicTrack8 *iface)
{
    struct track *track = impl_from_IDirectMusicTrack8(iface);
    LONG ref = InterlockedIncrement(&track->ref);
    TRACE("(%p) ref=%ld\n", track, ref);
    return ref;
}

static ULONG WINAPI track_Release(IDirectMusicTrack8 *iface)
{
    struct track *track = impl_from_IDirectMusicTrack8(iface);
    LONG ref = InterlockedDecrement(&track->ref);
    TRACE("(%p) ref=%ld\n", track, ref);
    if (!ref) track->vtbl->destroy(track);
    return ref;
}

static HRESULT WINAPI track_Init(IDirectMusicTrack8 *iface, IDirectMusicSegment *pSegment)
{
    struct track *track = impl_from_IDirectMusicTrack8(iface);
    FIXME("(%p, %p): stub\n", track, pSegment);
    return S_OK;
}

static HRESULT WINAPI track_InitPlay(IDirectMusicTrack8 *iface, IDirectMusicSegmentState *state,
        IDirectMusicPerformance *performance, void **state_data, DWORD track_id, DWORD flags)
{
    struct track *track = impl_from_IDirectMusicTrack8(iface);
    TRACE("(%p, %p, %p, %p, %ld, %lx)\n", track, state, performance, state_data, track_id, flags);
    return track->vtbl->init_play(track, state_data);
}

static HRESULT WINAPI track_EndPlay(IDirectMusicTrack8 *iface, void *state_data)
{
    struct track *track = impl_from_IDirectMusicTrack8(iface);
    TRACE("(%p, %p)\n", track, state_data);
    return track->vtbl->end_play(track, state_data);
}

static HRESULT WINAPI track_Play(IDirectMusicTrack8 *iface, void *state_data, MUSIC_TIME start,
        MUSIC_TIME end, MUSIC_TIME offset, DWORD flags, IDirectMusicPerformance *performance,
        IDirectMusicSegmentState *state, DWORD track_id)
{
    struct track *track = impl_from_IDirectMusicTrack8(iface);

    TRACE("(%p, %p, %ld, %ld, %ld, %lx, %p, %p, %ld)\n", track, state_data, start, end,
            offset, flags, performance, state, track_id);

    if (flags & DMUS_TRACKF_CLOCK) return E_INVALIDARG;

    return IDirectMusicTrack8_PlayEx(iface, state_data, start, end, offset, flags, performance,
            state, track_id);
}

static HRESULT WINAPI track_GetParam(IDirectMusicTrack8 *iface, const GUID *type,
        MUSIC_TIME time, MUSIC_TIME *next, void *param)
{
    struct track *track = impl_from_IDirectMusicTrack8(iface);
    REFERENCE_TIME next_time;
    HRESULT hr;

    TRACE("(%p, %s, %ld, %p, %p)\n", track, debugstr_dmguid(type), time, next, param);

    if (FAILED(hr = IDirectMusicTrack8_GetParamEx(iface, type, time, &next_time, param, NULL, 0))) return hr;
    if (next) *next = next_time;
    return hr;
}

static HRESULT WINAPI track_SetParam(IDirectMusicTrack8 *iface, const GUID *type,
        MUSIC_TIME time, void *param)
{
    struct track *track = impl_from_IDirectMusicTrack8(iface);
    TRACE("(%p, %s, %ld, %p)\n", track, debugstr_dmguid(type), time, param);
    return IDirectMusicTrack8_SetParamEx(iface, type, time, param, NULL, 0);
}

static HRESULT WINAPI track_IsParamSupported(IDirectMusicTrack8 *iface, const GUID *type)
{
    struct track *track = impl_from_IDirectMusicTrack8(iface);

    TRACE("(%p, %s)\n", track, debugstr_dmguid(type));

    if (!type) return E_POINTER;
    return track->vtbl->has_param(track, type);
}

static HRESULT WINAPI track_AddNotificationType(IDirectMusicTrack8 *iface, const GUID *notiftype)
{
    struct track *track = impl_from_IDirectMusicTrack8(iface);
    FIXME("(%p, %s): stub\n", track, debugstr_dmguid(notiftype));
    return E_NOTIMPL;
}

static HRESULT WINAPI track_RemoveNotificationType(IDirectMusicTrack8 *iface, const GUID *type)
{
    struct track *track = impl_from_IDirectMusicTrack8(iface);
    FIXME("(%p, %s): stub\n", track, debugstr_dmguid(type));
    return E_NOTIMPL;
}

static HRESULT WINAPI track_Clone(IDirectMusicTrack8 *iface, MUSIC_TIME start, MUSIC_TIME end,
        IDirectMusicTrack **ret_track)
{
    struct track *track = impl_from_IDirectMusicTrack8(iface);
    FIXME("(%p, %ld, %ld, %p): stub\n", track, start, end, ret_track);
    return S_OK;
}

static HRESULT WINAPI track_PlayEx(IDirectMusicTrack8 *iface, void *state_data, REFERENCE_TIME start,
        REFERENCE_TIME end, REFERENCE_TIME offset, DWORD flags, IDirectMusicPerformance *performance,
        IDirectMusicSegmentState *state, DWORD track_id)
{
    struct track *track = impl_from_IDirectMusicTrack8(iface);

    TRACE("(%p, %p, %I64d, %I64d, %I64d, %lx, %p, %p, %ld)\n", track, state_data, start, end,
            offset, flags, performance, state, track_id);

    if ((flags & DMUS_TRACKF_CLOCK)) FIXME("DMUS_TRACKF_CLOCK not implemented\n");

    return track->vtbl->play(track, state_data, start, end, offset, flags, performance, state, track_id);
}

static HRESULT WINAPI track_GetParamEx(IDirectMusicTrack8 *iface, const GUID *type, REFERENCE_TIME time,
        REFERENCE_TIME *next, void *param, void *state_data, DWORD flags)
{
    struct track *track = impl_from_IDirectMusicTrack8(iface);
    REFERENCE_TIME dummy;
    HRESULT hr;

    TRACE("(%p, %s, %I64d, %p, %p, %p, %lx)\n", track, debugstr_dmguid(type), time, next,
            param, state_data, flags);

    if ((flags & DMUS_TRACK_PARAMF_CLOCK)) FIXME("DMUS_TRACK_PARAMF_CLOCK not implemented\n");
    if (state_data) FIXME("state_data not implemented\n");

    if (!type) return E_POINTER;
    if (FAILED(hr = track->vtbl->has_param(track, type))) return hr;
    return track->vtbl->get_param(track, type, time, next ? next : &dummy, param);
}

static HRESULT WINAPI track_SetParamEx(IDirectMusicTrack8 *iface, const GUID *type, REFERENCE_TIME time,
        void *param, void *state_data, DWORD flags)
{
    struct track *track = impl_from_IDirectMusicTrack8(iface);
    HRESULT hr;

    TRACE("(%p, %s, %I64d, %p, %p, %lx)\n", track, debugstr_dmguid(type), time, param, state_data, flags);

    if ((flags & DMUS_TRACK_PARAMF_CLOCK)) FIXME("DMUS_TRACK_PARAMF_CLOCK not implemented\n");
    if (state_data) FIXME("state_data not implemented\n");

    if (!type) return E_POINTER;
    if (FAILED(hr = track->vtbl->has_param(track, type))) return hr;
    return track->vtbl->set_param(track, type, time, param);
}

static HRESULT WINAPI track_Compose(IDirectMusicTrack8 *iface, IUnknown *context, DWORD group,
        IDirectMusicTrack **ret_track)
{
    struct track *track = impl_from_IDirectMusicTrack8(iface);
    TRACE("(%p, %p, %ld, %p): method not implemented\n", track, context, group, ret_track);
    return E_NOTIMPL;
}

static HRESULT WINAPI track_Join(IDirectMusicTrack8 *iface, IDirectMusicTrack *other, MUSIC_TIME time,
        IUnknown *context, DWORD group, IDirectMusicTrack **ret_track)
{
    struct track *track = impl_from_IDirectMusicTrack8(iface);
    FIXME("(%p, %p, %ld, %p, %ld, %p): stub\n", track, other, time, context, group, ret_track);
    return S_OK;
}

static const IDirectMusicTrack8Vtbl track_vtbl =
{
    track_QueryInterface,
    track_AddRef,
    track_Release,
    track_Init,
    track_InitPlay,
    track_EndPlay,
    track_Play,
    track_GetParam,
    track_SetParam,
    track_IsParamSupported,
    track_AddNotificationType,
    track_RemoveNotificationType,
    track_Clone,
    track_PlayEx,
    track_GetParamEx,
    track_SetParamEx,
    track_Compose,
    track_Join,
};

static inline struct track *impl_from_IPersistStream(IPersistStream *iface)
{
    return CONTAINING_RECORD(iface, struct track, IPersistStream_iface);
}

static HRESULT WINAPI track_persist_stream_QueryInterface(IPersistStream *iface, REFIID riid, void **ret_iface)
{
    struct track *track = impl_from_IPersistStream(iface);
    return IDirectMusicTrack8_QueryInterface(&track->IDirectMusicTrack8_iface, riid, ret_iface);
}

static ULONG WINAPI track_persist_stream_AddRef(IPersistStream *iface)
{
    struct track *track = impl_from_IPersistStream(iface);
    return IDirectMusicTrack8_AddRef(&track->IDirectMusicTrack8_iface);
}

static ULONG WINAPI track_persist_stream_Release(IPersistStream *iface)
{
    struct track *track = impl_from_IPersistStream(iface);
    return IDirectMusicTrack8_Release(&track->IDirectMusicTrack8_iface);
}

static HRESULT WINAPI track_persist_stream_GetClassID(IPersistStream *iface, GUID *class_id)
{
    struct track *track = impl_from_IPersistStream(iface);

    TRACE("(%p, %p)\n", track, class_id);

    if (!class_id) return E_POINTER;
    *class_id = track->desc.guidClass;
    return S_OK;
}

static HRESULT WINAPI track_persist_stream_Load(IPersistStream *iface, IStream *stream)
{
    struct track *track = impl_from_IPersistStream(iface);

    TRACE("(%p, %p)\n", track, stream);

    if (!stream) return E_POINTER;
    return track->vtbl->load(track, stream);
}

static HRESULT WINAPI track_persist_stream_IsDirty(IPersistStream *iface)
{
    struct track *track = impl_from_IPersistStream(iface);
    TRACE("(%p)\n", track);
    return S_FALSE;
}

static HRESULT WINAPI track_persist_stream_Save(IPersistStream *iface, IStream *stream, BOOL clear_dirty)
{
    struct track *track = impl_from_IPersistStream(iface);
    TRACE("(%p, %p, %u)\n", track, stream, clear_dirty);
    return E_NOTIMPL;
}

static HRESULT WINAPI track_persist_stream_GetSizeMax(IPersistStream *iface, ULARGE_INTEGER *size)
{
    struct track *track = impl_from_IPersistStream(iface);
    TRACE("(%p, %p)\n", track, size);
    return E_NOTIMPL;
}

static const IPersistStreamVtbl track_persist_stream_vtbl =
{
    track_persist_stream_QueryInterface,
    track_persist_stream_AddRef,
    track_persist_stream_Release,
    track_persist_stream_GetClassID,
    track_persist_stream_IsDirty,
    track_persist_stream_Load,
    track_persist_stream_Save,
    track_persist_stream_GetSizeMax,
};

void track_init(struct track *track, const GUID *class_id, const struct track_vtbl *vtbl)
{
    track->IDirectMusicTrack8_iface.lpVtbl = &track_vtbl;
    track->IPersistStream_iface.lpVtbl = &track_persist_stream_vtbl;
    track->desc.guidClass = *class_id;
    track->vtbl = vtbl;
    track->ref = 1;
}
