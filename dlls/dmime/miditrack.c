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

#include "dmime_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmime);

struct midi_track
{
    IDirectMusicTrack8 IDirectMusicTrack8_iface;
    LONG ref;

    UINT events_size;
    BYTE *events;
};

static inline struct midi_track *impl_from_IDirectMusicTrack8(IDirectMusicTrack8 *iface)
{
    return CONTAINING_RECORD(iface, struct midi_track, IDirectMusicTrack8_iface);
}

static HRESULT WINAPI midi_track_QueryInterface(IDirectMusicTrack8 *iface, REFIID riid,
        void **ret_iface)
{
    struct midi_track *This = impl_from_IDirectMusicTrack8(iface);

    TRACE("(%p, %s, %p)\n", This, debugstr_dmguid(riid), ret_iface);

    if (IsEqualIID(riid, &IID_IUnknown)
            || IsEqualIID(riid, &IID_IDirectMusicTrack)
            || IsEqualIID(riid, &IID_IDirectMusicTrack8))
    {
        *ret_iface = &This->IDirectMusicTrack8_iface;
        IDirectMusicTrack8_AddRef(&This->IDirectMusicTrack8_iface);
        return S_OK;
    }

    *ret_iface = NULL;
    WARN("(%p, %s, %p): not found\n", This, debugstr_dmguid(riid), ret_iface);
    return E_NOINTERFACE;
}

static ULONG WINAPI midi_track_AddRef(IDirectMusicTrack8 *iface)
{
    struct midi_track *This = impl_from_IDirectMusicTrack8(iface);
    LONG ref = InterlockedIncrement(&This->ref);
    TRACE("(%p) ref=%ld\n", This, ref);
    return ref;
}

static ULONG WINAPI midi_track_Release(IDirectMusicTrack8 *iface)
{
    struct midi_track *This = impl_from_IDirectMusicTrack8(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    if (!ref)
        free(This);

    return ref;
}

static HRESULT WINAPI midi_track_Init(IDirectMusicTrack8 *iface, IDirectMusicSegment *segment)
{
    struct midi_track *This = impl_from_IDirectMusicTrack8(iface);
    TRACE("(%p, %p)\n", This, segment);
    return S_OK;
}

static HRESULT WINAPI midi_track_InitPlay(IDirectMusicTrack8 *iface,
        IDirectMusicSegmentState *segment_state, IDirectMusicPerformance *performance,
        void **state_data, DWORD track_id, DWORD track_flags)
{
    struct midi_track *This = impl_from_IDirectMusicTrack8(iface);
    FIXME("(%p, %p, %p, %p, %ld, %ld): stub\n", This, segment_state, performance, state_data,
            track_id, track_flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI midi_track_EndPlay(IDirectMusicTrack8 *iface, void *state_data)
{
    struct midi_track *This = impl_from_IDirectMusicTrack8(iface);
    FIXME("(%p, %p): stub\n", This, state_data);
    return E_NOTIMPL;
}

static HRESULT WINAPI midi_track_Play(IDirectMusicTrack8 *iface, void *state_data,
        MUSIC_TIME start_time, MUSIC_TIME end_time, MUSIC_TIME time_offset, DWORD segment_flags,
        IDirectMusicPerformance *performance, IDirectMusicSegmentState *segment_state, DWORD track_id)
{
    struct midi_track *This = impl_from_IDirectMusicTrack8(iface);
    FIXME("(%p, %p, %ld, %ld, %ld, %#lx, %p, %p, %ld): stub\n", This, state_data, start_time, end_time,
            time_offset, segment_flags, performance, segment_state, track_id);
    return E_NOTIMPL;
}

static HRESULT WINAPI midi_track_GetParam(IDirectMusicTrack8 *iface, REFGUID type, MUSIC_TIME time,
        MUSIC_TIME *next, void *param)
{
    struct midi_track *This = impl_from_IDirectMusicTrack8(iface);
    TRACE("(%p, %s, %ld, %p, %p)\n", This, debugstr_dmguid(type), time, next, param);
    return DMUS_E_GET_UNSUPPORTED;
}

static HRESULT WINAPI midi_track_SetParam(IDirectMusicTrack8 *iface, REFGUID type, MUSIC_TIME time,
        void *param)
{
    struct midi_track *This = impl_from_IDirectMusicTrack8(iface);
    TRACE("(%p, %s, %ld, %p)\n", This, debugstr_dmguid(type), time, param);
    return DMUS_E_SET_UNSUPPORTED;
}

static HRESULT WINAPI midi_track_IsParamSupported(IDirectMusicTrack8 *iface, REFGUID type)
{
    struct midi_track *This = impl_from_IDirectMusicTrack8(iface);
    TRACE("(%p, %s)\n", This, debugstr_dmguid(type));
    return DMUS_E_TYPE_UNSUPPORTED;
}

static HRESULT WINAPI midi_track_AddNotificationType(IDirectMusicTrack8 *iface, REFGUID type)
{
    struct midi_track *This = impl_from_IDirectMusicTrack8(iface);
    FIXME("(%p, %s): stub\n", This, debugstr_dmguid(type));
    return E_NOTIMPL;
}

static HRESULT WINAPI midi_track_RemoveNotificationType(IDirectMusicTrack8 *iface, REFGUID type)
{
    struct midi_track *This = impl_from_IDirectMusicTrack8(iface);
    FIXME("(%p, %s): stub\n", This, debugstr_dmguid(type));
    return E_NOTIMPL;
}

static HRESULT WINAPI midi_track_Clone(IDirectMusicTrack8 *iface, MUSIC_TIME start_time,
        MUSIC_TIME end_time, IDirectMusicTrack **ret_track)
{
    struct midi_track *This = impl_from_IDirectMusicTrack8(iface);
    FIXME("(%p, %ld, %ld, %p): stub\n", This, start_time, end_time, ret_track);
    return E_NOTIMPL;
}

static HRESULT WINAPI midi_track_PlayEx(IDirectMusicTrack8 *iface, void *state_data,
        REFERENCE_TIME start_time, REFERENCE_TIME end_time, REFERENCE_TIME time_offset, DWORD track_flags,
        IDirectMusicPerformance *performance, IDirectMusicSegmentState *segment_state, DWORD track_id)
{
    struct midi_track *This = impl_from_IDirectMusicTrack8(iface);
    FIXME("(%p, %p, %I64d, %I64d, %I64d, %ld, %p, %p, %ld): stub\n", This, state_data, start_time, end_time,
            time_offset, track_flags, performance, segment_state, track_id);
    return E_NOTIMPL;
}

static HRESULT WINAPI midi_track_GetParamEx(IDirectMusicTrack8 *iface, REFGUID type,
        REFERENCE_TIME time, REFERENCE_TIME *next_time, void *param, void *state_data,
        DWORD track_flags)
{
    struct midi_track *This = impl_from_IDirectMusicTrack8(iface);
    FIXME("(%p, %s, %I64d, %p, %p, %p, %ld): stub\n", This, debugstr_dmguid(type),
            time, next_time, param, state_data, track_flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI midi_track_SetParamEx(IDirectMusicTrack8 *iface, REFGUID type,
        REFERENCE_TIME time, void *param, void *state_data, DWORD track_flags)
{
    struct midi_track *This = impl_from_IDirectMusicTrack8(iface);
    FIXME("(%p, %s, %I64d, %p, %p, %ld): stub\n", This, debugstr_dmguid(type),
            time, param, state_data, track_flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI midi_track_Compose(IDirectMusicTrack8 *iface, IUnknown *context,
        DWORD group, IDirectMusicTrack **track)
{
    struct midi_track *This = impl_from_IDirectMusicTrack8(iface);
    TRACE("(%p, %p, %ld, %p): stub\n", This, context, group, track);
    return E_NOTIMPL;
}

static HRESULT WINAPI midi_track_Join(IDirectMusicTrack8 *iface, IDirectMusicTrack *other,
        MUSIC_TIME join_time, IUnknown *context, DWORD group, IDirectMusicTrack **ret_track)
{
    struct midi_track *This = impl_from_IDirectMusicTrack8(iface);
    TRACE("(%p, %p, %ld, %p, %ld, %p): stub\n", This, other, join_time, context,
            group, ret_track);
    return E_NOTIMPL;
}

static const IDirectMusicTrack8Vtbl track_vtbl =
{
    midi_track_QueryInterface,
    midi_track_AddRef,
    midi_track_Release,
    midi_track_Init,
    midi_track_InitPlay,
    midi_track_EndPlay,
    midi_track_Play,
    midi_track_GetParam,
    midi_track_SetParam,
    midi_track_IsParamSupported,
    midi_track_AddNotificationType,
    midi_track_RemoveNotificationType,
    midi_track_Clone,
    midi_track_PlayEx,
    midi_track_GetParamEx,
    midi_track_SetParamEx,
    midi_track_Compose,
    midi_track_Join
};

static HRESULT midi_track_create(IDirectMusicTrack8 **ret_iface)
{
    struct midi_track *track;

    *ret_iface = NULL;
    if (!(track = calloc(1, sizeof(*track)))) return E_OUTOFMEMORY;
    track->IDirectMusicTrack8_iface.lpVtbl = &track_vtbl;
    track->ref = 1;

    *ret_iface = &track->IDirectMusicTrack8_iface;
    return S_OK;
}

HRESULT midi_track_create_from_chunk(IStream *stream, struct chunk_entry *chunk,
        IDirectMusicTrack8 **ret_iface)
{
    IDirectMusicTrack8 *iface;
    struct midi_track *This;
    HRESULT hr;

    if (FAILED(hr = midi_track_create(&iface))) return hr;
    This = impl_from_IDirectMusicTrack8(iface);

    switch (MAKE_IDTYPE(chunk->id, chunk->type))
    {
    case mmioFOURCC('M','T','r','k'):
        if (!(This->events = malloc(chunk->size))) hr = E_OUTOFMEMORY;
        else
        {
            hr = stream_chunk_get_data(stream, chunk, This->events, chunk->size);
            if (SUCCEEDED(hr)) This->events_size = chunk->size;
        }
        break;

    default:
        FIXME("Invalid midi track chunk %s %s\n", debugstr_fourcc(chunk->id), debugstr_fourcc(chunk->type));
        hr = DMUS_E_UNSUPPORTED_STREAM;
        break;
    }

    if (SUCCEEDED(hr)) *ret_iface = iface;
    else IDirectMusicTrack8_Release(iface);
    return hr;
}
