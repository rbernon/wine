/*
 * Copyright (C) 2003-2004 Rok Mandeljc
 * Copyright (C) 2003-2004 Raphael Junqueira
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
#include "dmtrack.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmime);

struct segment_item {
    struct list entry;
    DMUS_IO_SEGMENT_ITEM_HEADER header;
    IDirectMusicObject *dmobj;
    WCHAR name[DMUS_MAX_NAME];
};

struct seg_trigger_track
{
    struct track track;
    struct list Items;
};

static inline struct seg_trigger_track *impl_from_track(struct track *iface)
{
    return CONTAINING_RECORD(iface, struct seg_trigger_track, track);
}

static void seg_trigger_track_destroy(struct track *track)
{
    struct seg_trigger_track *This = impl_from_track(track);
    struct list *cursor, *cursor2;
    struct segment_item *item;

    TRACE("(%p)\n", This);

    LIST_FOR_EACH_SAFE(cursor, cursor2, &This->Items)
    {
        item = LIST_ENTRY(cursor, struct segment_item, entry);
        list_remove(cursor);
        if (item->dmobj) IDirectMusicObject_Release(item->dmobj);
        free(item);
    }

    free(This);
}

static HRESULT seg_trigger_track_get_param(struct track *track, const GUID *type,
        MUSIC_TIME time, MUSIC_TIME *next, void *param)
{
    struct seg_trigger_track *This = impl_from_track(track);
    FIXME("(%p, %p): stub", This, type);
    return DMUS_E_GET_UNSUPPORTED;
}

static HRESULT seg_trigger_track_set_param(struct track *track, const GUID *type,
        MUSIC_TIME time, void *param)
{
    struct seg_trigger_track *This = impl_from_track(track);
    FIXME("(%p, %p): stub", This, type);
    return DMUS_E_SET_UNSUPPORTED;
}

static HRESULT seg_trigger_track_has_param(struct track *track, const GUID *type)
{
    struct seg_trigger_track *This = impl_from_track(track);
    FIXME("(%p, %p): stub", This, type);
    return DMUS_E_TYPE_UNSUPPORTED;
}

static HRESULT parse_segment_item(struct seg_trigger_track *This, IStream *stream,
        const struct chunk_entry *lseg)
{
    struct chunk_entry chunk = {.parent = lseg};
    struct segment_item *item;
    HRESULT hr;

    /* First chunk is a header */
    if (stream_get_chunk(stream, &chunk) != S_OK || chunk.id != DMUS_FOURCC_SEGMENTITEM_CHUNK)
        return DMUS_E_TRACK_HDR_NOT_FIRST_CK;
    if (!(item = calloc(1, sizeof(*item)))) return E_OUTOFMEMORY;
    hr = stream_chunk_get_data(stream, &chunk, &item->header, sizeof(DMUS_IO_SEGMENT_ITEM_HEADER));
    if (FAILED(hr))
        goto error;

    TRACE("Found DMUS_IO_SEGMENT_ITEM_HEADER\n");
    TRACE("\tlTimePhysical: %lu\n", item->header.lTimeLogical);
    TRACE("\tlTimePhysical: %lu\n", item->header.lTimePhysical);
    TRACE("\tdwPlayFlags: %#08lx\n", item->header.dwPlayFlags);
    TRACE("\tdwFlags: %#08lx\n", item->header.dwFlags);

    /* Second chunk is a reference list */
    if (stream_next_chunk(stream, &chunk) != S_OK || chunk.id != FOURCC_LIST ||
            chunk.type != DMUS_FOURCC_REF_LIST) {
        hr = DMUS_E_INVALID_SEGMENTTRIGGERTRACK;
        goto error;
    }
    if (FAILED(hr = dmobj_parsereference(stream, &chunk, &item->dmobj)))
        goto error;

    /* Optional third chunk if the reference is a motif */
    if (item->header.dwFlags & DMUS_SEGMENTTRACKF_MOTIF) {
        if (FAILED(hr = stream_next_chunk(stream, &chunk)))
            goto error;
        if (chunk.id == DMUS_FOURCC_SEGMENTITEMNAME_CHUNK)
            if (FAILED(hr = stream_chunk_get_wstr(stream, &chunk, item->name, DMUS_MAX_NAME)))
                goto error;

        TRACE("Found motif name: %s\n", debugstr_w(item->name));
    }

    list_add_tail(&This->Items, &item->entry);

    return S_OK;

error:
    free(item);
    return hr;
}

static HRESULT parse_segments_list(struct seg_trigger_track *This, IStream *stream,
        const struct chunk_entry *lsgl)
{
    struct chunk_entry chunk = {.parent = lsgl};
    HRESULT hr;

    TRACE("Parsing segment list in %p: %s\n", stream, debugstr_chunk(lsgl));

    while ((hr = stream_next_chunk(stream, &chunk)) == S_OK)
        if (chunk.id == FOURCC_LIST && chunk.type == DMUS_FOURCC_SEGMENT_LIST)
            if (FAILED(hr = parse_segment_item(This, stream, &chunk)))
                break;

    return SUCCEEDED(hr) ? S_OK : hr;
}

static HRESULT seg_trigger_track_load(struct track *track, IStream *stream)
{
    struct seg_trigger_track *This = impl_from_track(track);
    struct chunk_entry segt = {0};
    struct chunk_entry chunk = {.parent = &segt};
    DMUS_IO_SEGMENT_TRACK_HEADER header;
    HRESULT hr;

    TRACE("(%p, %p): Loading\n", This, stream);

    if (stream_get_chunk(stream, &segt) != S_OK || segt.id != FOURCC_LIST ||
            segt.type != DMUS_FOURCC_SEGTRACK_LIST)
        return DMUS_E_INVALID_SEGMENTTRIGGERTRACK;

    if ((hr = stream_get_chunk(stream, &chunk)) != S_OK)
        return FAILED(hr) ? hr : DMUS_E_INVALID_SEGMENTTRIGGERTRACK;

    /* Optional and useless header chunk */
    if (chunk.id == DMUS_FOURCC_SEGTRACK_CHUNK) {
        hr = stream_chunk_get_data(stream, &chunk, &header, sizeof(DMUS_IO_SEGMENT_TRACK_HEADER));
        if (FAILED(hr))
            return hr;
        if (header.dwFlags)
            WARN("Got flags %#lx; must be zero\n", header.dwFlags);

        if ((hr = stream_get_chunk(stream, &chunk)) != S_OK)
            return FAILED(hr) ? hr : DMUS_E_INVALID_SEGMENTTRIGGERTRACK;
    }

    if (chunk.id != FOURCC_LIST || chunk.type != DMUS_FOURCC_SEGMENTS_LIST)
        return DMUS_E_INVALID_SEGMENTTRIGGERTRACK;

    return parse_segments_list(This, stream, &chunk);
}

static const struct track_vtbl seg_trigger_track_vtbl =
{
    seg_trigger_track_destroy,
    seg_trigger_track_get_param,
    seg_trigger_track_set_param,
    seg_trigger_track_has_param,
    seg_trigger_track_load,
};

HRESULT create_dmsegtriggertrack(REFIID lpcGUID, void **ppobj)
{
    struct seg_trigger_track *track;
    HRESULT hr;

    *ppobj = NULL;
    if (!(track = calloc(1, sizeof(*track)))) return E_OUTOFMEMORY;
    track_init(&track->track, &CLSID_DirectMusicSegTriggerTrack, &seg_trigger_track_vtbl);
    list_init(&track->Items);

    hr = IDirectMusicTrack8_QueryInterface(&track->track.IDirectMusicTrack8_iface, lpcGUID, ppobj);
    IDirectMusicTrack8_Release(&track->track.IDirectMusicTrack8_iface);
    return hr;
}
