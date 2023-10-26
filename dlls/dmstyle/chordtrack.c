/*
 * Copyright (C) 2003-2004 Rok Mandeljc
 * Copyright (C) 2003-2004 Raphael Junqueira
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

#include "dmstyle_private.h"
#include "dmobject.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmstyle);

struct chord_entry
{
    DMUS_IO_CHORD chord;
    UINT subchord_count;
    struct list entry;
    DMUS_IO_SUBCHORD subchord[];
};

C_ASSERT(sizeof(struct chord_entry) == offsetof(struct chord_entry, subchord[0]));

struct chord_track
{
    IDirectMusicTrack8 IDirectMusicTrack8_iface;
    struct dmobject dmobj;  /* IPersistStream only */
    LONG ref;
    DWORD dwScale;

    struct list chords;
};

static inline struct chord_track *impl_from_IDirectMusicTrack8(IDirectMusicTrack8 *iface)
{
    return CONTAINING_RECORD(iface, struct chord_track, IDirectMusicTrack8_iface);
}

static HRESULT WINAPI chord_track_QueryInterface(IDirectMusicTrack8 *iface, REFIID riid,
        void **ret_iface)
{
    struct chord_track *This = impl_from_IDirectMusicTrack8(iface);

    TRACE("(%p, %s, %p)\n", This, debugstr_dmguid(riid), ret_iface);

    *ret_iface = NULL;

    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDirectMusicTrack) ||
            IsEqualIID(riid, &IID_IDirectMusicTrack8))
        *ret_iface = iface;
    else if (IsEqualIID(riid, &IID_IPersistStream))
        *ret_iface = &This->dmobj.IPersistStream_iface;
    else {
        WARN("(%p, %s, %p): not found\n", This, debugstr_dmguid(riid), ret_iface);
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ret_iface);
    return S_OK;
}

static ULONG WINAPI chord_track_AddRef(IDirectMusicTrack8 *iface)
{
    struct chord_track *This = impl_from_IDirectMusicTrack8(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    return ref;
}

static ULONG WINAPI chord_track_Release(IDirectMusicTrack8 *iface)
{
    struct chord_track *This = impl_from_IDirectMusicTrack8(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    if (!ref)
    {
        struct chord_entry *entry, *next;

        LIST_FOR_EACH_ENTRY_SAFE(entry, next, &This->chords, struct chord_entry, entry)
        {
            list_remove(&entry->entry);
            free(entry);
        }

        free(This);
    }

    return ref;
}

static HRESULT WINAPI chord_track_Init(IDirectMusicTrack8 *iface, IDirectMusicSegment *pSegment)
{
  struct chord_track *This = impl_from_IDirectMusicTrack8(iface);
  FIXME("(%p, %p): stub\n", This, pSegment);
  return S_OK;
}

static HRESULT WINAPI chord_track_InitPlay(IDirectMusicTrack8 *iface,
        IDirectMusicSegmentState *pSegmentState, IDirectMusicPerformance *pPerformance,
        void **ppStateData, DWORD dwVirtualTrack8ID, DWORD dwFlags)
{
  struct chord_track *This = impl_from_IDirectMusicTrack8(iface);
  FIXME("(%p, %p, %p, %p, %ld, %ld): stub\n", This, pSegmentState, pPerformance, ppStateData, dwVirtualTrack8ID, dwFlags);
  return S_OK;
}

static HRESULT WINAPI chord_track_EndPlay(IDirectMusicTrack8 *iface, void *pStateData)
{
  struct chord_track *This = impl_from_IDirectMusicTrack8(iface);
  FIXME("(%p, %p): stub\n", This, pStateData);
  return S_OK;
}

static HRESULT WINAPI chord_track_Play(IDirectMusicTrack8 *iface, void *pStateData,
        MUSIC_TIME mtStart, MUSIC_TIME mtEnd, MUSIC_TIME mtOffset, DWORD dwFlags,
        IDirectMusicPerformance *pPerf, IDirectMusicSegmentState *pSegSt, DWORD dwVirtualID)
{
  struct chord_track *This = impl_from_IDirectMusicTrack8(iface);
  FIXME("(%p, %p, %ld, %ld, %ld, %ld, %p, %p, %ld): stub\n", This, pStateData, mtStart, mtEnd, mtOffset, dwFlags, pPerf, pSegSt, dwVirtualID);
  return S_OK;
}

static HRESULT WINAPI chord_track_GetParam(IDirectMusicTrack8 *iface, REFGUID type, MUSIC_TIME time,
        MUSIC_TIME *next, void *param)
{
    struct chord_track *This = impl_from_IDirectMusicTrack8(iface);

    TRACE("(%p, %s, %ld, %p, %p):\n", This, debugstr_dmguid(type), time, next, param);

    if (!type)
        return E_POINTER;

    if (IsEqualGUID(type, &GUID_ChordParam)) {
        FIXME("GUID_ChordParam not handled yet\n");
        return S_OK;
    } else if (IsEqualGUID(type, &GUID_RhythmParam)) {
        FIXME("GUID_RhythmParam not handled yet\n");
        return S_OK;
    }

    return DMUS_E_GET_UNSUPPORTED;
}

static HRESULT WINAPI chord_track_SetParam(IDirectMusicTrack8 *iface, REFGUID type, MUSIC_TIME time,
        void *param)
{
    struct chord_track *This = impl_from_IDirectMusicTrack8(iface);

    TRACE("(%p, %s, %ld, %p)\n", This, debugstr_dmguid(type), time, param);

    if (!type)
        return E_POINTER;
    if (!IsEqualGUID(type, &GUID_ChordParam))
        return DMUS_E_SET_UNSUPPORTED;

    FIXME("GUID_ChordParam not handled yet\n");

    return S_OK;
}

static HRESULT WINAPI chord_track_IsParamSupported(IDirectMusicTrack8 *iface, REFGUID type)
{
    struct chord_track *This = impl_from_IDirectMusicTrack8(iface);

    TRACE("(%p, %s)\n", This, debugstr_dmguid(type));

    if (!type)
        return E_POINTER;

    if (IsEqualGUID(type, &GUID_ChordParam) || IsEqualGUID(type, &GUID_RhythmParam))
        return S_OK;

    TRACE("param unsupported\n");
    return DMUS_E_TYPE_UNSUPPORTED;
}

static HRESULT WINAPI chord_track_AddNotificationType(IDirectMusicTrack8 *iface,
        REFGUID rguidNotificationType)
{
  struct chord_track *This = impl_from_IDirectMusicTrack8(iface);
  FIXME("(%p, %s): stub\n", This, debugstr_dmguid(rguidNotificationType));
  return S_OK;
}

static HRESULT WINAPI chord_track_RemoveNotificationType(IDirectMusicTrack8 *iface,
        REFGUID rguidNotificationType)
{
  struct chord_track *This = impl_from_IDirectMusicTrack8(iface);
  FIXME("(%p, %s): stub\n", This, debugstr_dmguid(rguidNotificationType));
  return S_OK;
}

static HRESULT WINAPI chord_track_Clone(IDirectMusicTrack8 *iface, MUSIC_TIME mtStart,
        MUSIC_TIME mtEnd, IDirectMusicTrack **ppTrack)
{
  struct chord_track *This = impl_from_IDirectMusicTrack8(iface);
  FIXME("(%p, %ld, %ld, %p): stub\n", This, mtStart, mtEnd, ppTrack);
  return S_OK;
}

static HRESULT WINAPI chord_track_PlayEx(IDirectMusicTrack8 *iface, void *pStateData,
        REFERENCE_TIME rtStart, REFERENCE_TIME rtEnd, REFERENCE_TIME rtOffset, DWORD dwFlags,
        IDirectMusicPerformance *pPerf, IDirectMusicSegmentState *pSegSt, DWORD dwVirtualID)
{
  struct chord_track *This = impl_from_IDirectMusicTrack8(iface);
  FIXME("(%p, %p, 0x%s, 0x%s, 0x%s, %ld, %p, %p, %ld): stub\n", This, pStateData, wine_dbgstr_longlong(rtStart),
      wine_dbgstr_longlong(rtEnd), wine_dbgstr_longlong(rtOffset), dwFlags, pPerf, pSegSt, dwVirtualID);
  return S_OK;
}

static HRESULT WINAPI chord_track_GetParamEx(IDirectMusicTrack8 *iface, REFGUID rguidType,
        REFERENCE_TIME rtTime, REFERENCE_TIME *prtNext, void *pParam, void *pStateData,
        DWORD dwFlags)
{
  struct chord_track *This = impl_from_IDirectMusicTrack8(iface);
  FIXME("(%p, %s, 0x%s, %p, %p, %p, %ld): stub\n", This, debugstr_dmguid(rguidType),
      wine_dbgstr_longlong(rtTime), prtNext, pParam, pStateData, dwFlags);
  return S_OK;
}

static HRESULT WINAPI chord_track_SetParamEx(IDirectMusicTrack8 *iface, REFGUID rguidType,
        REFERENCE_TIME rtTime, void *pParam, void *pStateData, DWORD dwFlags)
{
  struct chord_track *This = impl_from_IDirectMusicTrack8(iface);
  FIXME("(%p, %s, 0x%s, %p, %p, %ld): stub\n", This, debugstr_dmguid(rguidType),
      wine_dbgstr_longlong(rtTime), pParam, pStateData, dwFlags);
  return S_OK;
}

static HRESULT WINAPI chord_track_Compose(IDirectMusicTrack8 *iface, IUnknown *context,
        DWORD trackgroup, IDirectMusicTrack **track)
{
    struct chord_track *This = impl_from_IDirectMusicTrack8(iface);

    TRACE("(%p, %p, %ld, %p): method not implemented\n", This, context, trackgroup, track);
    return E_NOTIMPL;
}

static HRESULT WINAPI chord_track_Join(IDirectMusicTrack8 *iface, IDirectMusicTrack *pNewTrack,
        MUSIC_TIME mtJoin, IUnknown *pContext, DWORD dwTrackGroup,
        IDirectMusicTrack **ppResultTrack)
{
  struct chord_track *This = impl_from_IDirectMusicTrack8(iface);
  FIXME("(%p, %p, %ld, %p, %ld, %p): stub\n", This, pNewTrack, mtJoin, pContext, dwTrackGroup, ppResultTrack);
  return S_OK;
}

static const IDirectMusicTrack8Vtbl dmtrack8_vtbl = {
    chord_track_QueryInterface,
    chord_track_AddRef,
    chord_track_Release,
    chord_track_Init,
    chord_track_InitPlay,
    chord_track_EndPlay,
    chord_track_Play,
    chord_track_GetParam,
    chord_track_SetParam,
    chord_track_IsParamSupported,
    chord_track_AddNotificationType,
    chord_track_RemoveNotificationType,
    chord_track_Clone,
    chord_track_PlayEx,
    chord_track_GetParamEx,
    chord_track_SetParamEx,
    chord_track_Compose,
    chord_track_Join
};

static HRESULT parse_crdb_chunk(struct chord_track *This, IStream *stream, struct chunk_entry *chunk)
{
    DWORD i, size, sub_size, count;
    struct chord_entry *entry;
    DMUS_IO_CHORD chord = {0};
    HRESULT hr;

    if (chunk->size < sizeof(DWORD)) return E_INVALIDARG;
    if (FAILED(hr = stream_read(stream, &size, sizeof(size)))) return hr;

    if (size > sizeof(chord)) return E_INVALIDARG;
    if (chunk->size < sizeof(DWORD) + size) return E_INVALIDARG;
    if (FAILED(hr = stream_read(stream, &chord, size))) return hr;

    if (chunk->size < 3 * sizeof(DWORD) + size) return E_INVALIDARG;
    if (FAILED(hr = stream_read(stream, &count, sizeof(count)))) return hr;
    if (FAILED(hr = stream_read(stream, &sub_size, sizeof(sub_size)))) return hr;
    if (chunk->size != 3 * sizeof(DWORD) + size + count * sub_size) return E_INVALIDARG;

    size = offsetof(struct chord_entry, subchord[count]);
    if (!(entry = calloc(1, size))) return E_OUTOFMEMORY;
    entry->chord = chord;
    entry->subchord_count = count;
    for (i = 0; SUCCEEDED(hr) && i < count; i++) hr = stream_read(stream, entry->subchord + i, sub_size);

    if (SUCCEEDED(hr)) list_add_tail(&This->chords, &entry->entry);
    else free(entry);

    return hr;
}

static HRESULT parse_cord_list(struct chord_track *This, IStream *stream, struct chunk_entry *parent)
{
    struct chunk_entry chunk = {.parent = parent};
    HRESULT hr;

    while ((hr = stream_next_chunk(stream, &chunk)) == S_OK)
    {
        switch (MAKE_IDTYPE(chunk.id, chunk.type))
        {
        case DMUS_FOURCC_CHORDTRACKHEADER_CHUNK:
            hr = stream_chunk_get_data(stream, &chunk, &This->dwScale, sizeof(This->dwScale));
            break;

        case DMUS_FOURCC_CHORDTRACKBODY_CHUNK:
            hr = parse_crdb_chunk(This, stream, &chunk);
            break;

        default:
            FIXME("Ignoring chunk %s %s\n", debugstr_fourcc(chunk.id), debugstr_fourcc(chunk.type));
            break;
        }

        if (FAILED(hr)) break;
    }

    return hr;
}

static inline struct chord_track *impl_from_IPersistStream(IPersistStream *iface)
{
    return CONTAINING_RECORD(iface, struct chord_track, dmobj.IPersistStream_iface);
}

static HRESULT WINAPI IPersistStreamImpl_Load(IPersistStream *iface, IStream *stream)
{
    struct chord_track *This = impl_from_IPersistStream(iface);
    struct chunk_entry chunk = {0};
    HRESULT hr;

    TRACE("(%p, %p)\n", This, stream);

    if ((hr = stream_get_chunk(stream, &chunk)) == S_OK)
    {
        switch (MAKE_IDTYPE(chunk.id, chunk.type))
        {
        case MAKE_IDTYPE(FOURCC_LIST, DMUS_FOURCC_CHORDTRACK_LIST):
            hr = parse_cord_list(This, stream, &chunk);
            break;

        default:
            WARN("Invalid chord track chunk %s %s\n", debugstr_fourcc(chunk.id), debugstr_fourcc(chunk.type));
            hr = DMUS_E_UNSUPPORTED_STREAM;
            break;
        }
    }

    stream_skip_chunk(stream, &chunk);
    if (FAILED(hr)) return hr;

    if (TRACE_ON(dmstyle))
    {
        struct chord_entry *entry;
        UINT i = 0, j;

        TRACE("Loaded DirectMusicChordTrack %p\n", This);

        LIST_FOR_EACH_ENTRY(entry, &This->chords, struct chord_entry, entry)
        {
            TRACE("  - DMUS_IO_CHORD[%u]\n", i++);
            TRACE("    - wszName: %s\n", debugstr_w(entry->chord.wszName));
            TRACE("    - mtTime: %ld\n", entry->chord.mtTime);
            TRACE("    - wMeasure: %d\n", entry->chord.wMeasure);
            TRACE("    - bBeat: %d\n", entry->chord.bBeat);
            TRACE("    - bFlags: %d\n", entry->chord.bFlags);

            for (j = 0; j < entry->subchord_count; j++)
            {
                TRACE("    - DMUS_IO_SUBCHORD[%u]\n", j);
                TRACE("      - dwChordPattern: %ld\n", entry->subchord[j].dwChordPattern);
                TRACE("      - dwScalePattern: %ld\n", entry->subchord[j].dwScalePattern);
                TRACE("      - dwInversionPoints: %ld\n", entry->subchord[j].dwInversionPoints);
                TRACE("      - dwLevels: %ld\n", entry->subchord[j].dwLevels);
                TRACE("      - bChordRoot: %d\n", entry->subchord[j].bChordRoot);
                TRACE("      - bScaleRoot: %d\n", entry->subchord[j].bScaleRoot);
            }
        }
    }

    return S_OK;
}

static const IPersistStreamVtbl persiststream_vtbl =
{
    dmobj_IPersistStream_QueryInterface,
    dmobj_IPersistStream_AddRef,
    dmobj_IPersistStream_Release,
    dmobj_IPersistStream_GetClassID,
    unimpl_IPersistStream_IsDirty,
    IPersistStreamImpl_Load,
    unimpl_IPersistStream_Save,
    unimpl_IPersistStream_GetSizeMax,
};

/* for ClassFactory */
HRESULT create_dmchordtrack(REFIID lpcGUID, void **ppobj)
{
    struct chord_track *track;
    HRESULT hr;

    *ppobj = NULL;
    if (!(track = calloc(1, sizeof(*track)))) return E_OUTOFMEMORY;
    track->IDirectMusicTrack8_iface.lpVtbl = &dmtrack8_vtbl;
    track->ref = 1;
    dmobject_init(&track->dmobj, &CLSID_DirectMusicChordTrack,
                  (IUnknown *)&track->IDirectMusicTrack8_iface);
    track->dmobj.IPersistStream_iface.lpVtbl = &persiststream_vtbl;
    list_init(&track->chords);

    hr = IDirectMusicTrack8_QueryInterface(&track->IDirectMusicTrack8_iface, lpcGUID, ppobj);
    IDirectMusicTrack8_Release(&track->IDirectMusicTrack8_iface);

    return hr;
}
