/*
 * Copyright (C) 2003-2004 Rok Mandeljc
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

#include "dmstyle_private.h"
#include "dmtrack.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmstyle);

/*****************************************************************************
 * struct style_track implementation
 */
struct style_item {
    struct list entry;
    DWORD timestamp;
    IDirectMusicStyle8 *dmstyle;
};

struct style_track
{
    struct track track;
    struct list Items;
};

static inline struct style_track *impl_from_track(struct track *iface)
{
    return CONTAINING_RECORD(iface, struct style_track, track);
}

static void style_track_destroy(struct track *track)
{
    struct style_track *This = impl_from_track(track);
    struct style_item *item, *item2;

    TRACE("(%p)\n", track);

    LIST_FOR_EACH_ENTRY_SAFE(item, item2, &This->Items, struct style_item, entry)
    {
        list_remove(&item->entry);
        IDirectMusicStyle8_Release(item->dmstyle);
        free(item);
    }

    free(track);
}

static HRESULT style_track_has_param(struct track *track, const GUID *type)
{
    if (IsEqualGUID(type, &GUID_DisableTimeSig)
        || IsEqualGUID(type, &GUID_EnableTimeSig)
        || IsEqualGUID(type, &GUID_IDirectMusicStyle)
        || IsEqualGUID(type, &GUID_SeedVariations)
        || IsEqualGUID(type, &GUID_TimeSignature))
        return S_OK;
    return DMUS_E_TYPE_UNSUPPORTED;
}

static HRESULT style_track_get_param(struct track *track, const GUID *type)
{
    if (IsEqualGUID(&GUID_IDirectMusicStyle, type)) {
        LIST_FOR_EACH_ENTRY(item, &This->Items, struct style_item, entry) {
            IDirectMusicStyle8_AddRef(item->dmstyle);
            *((IDirectMusicStyle8 **)param) = item->dmstyle;

            return S_OK;
        }

        return DMUS_E_NOT_FOUND;
    } else if (IsEqualGUID(&GUID_TimeSignature, type)) {
        FIXME("GUID_TimeSignature not handled yet\n");
        return S_OK;
    }

    return DMUS_E_GET_UNSUPPORTED;
}

static HRESULT style_track_set_param(struct track *track, const GUID *type)
{
    if (IsEqualGUID(type, &GUID_DisableTimeSig)) {
        FIXME("GUID_DisableTimeSig not handled yet\n");
        return S_OK;
    } else if (IsEqualGUID(type, &GUID_EnableTimeSig)) {
        FIXME("GUID_EnableTimeSig not handled yet\n");
        return S_OK;
    } else if (IsEqualGUID(type, &GUID_IDirectMusicStyle)) {
        FIXME("GUID_IDirectMusicStyle not handled yet\n");
        return S_OK;
    } else if (IsEqualGUID(type, &GUID_SeedVariations)) {
        FIXME("GUID_SeedVariations not handled yet\n");
        return S_OK;
    }

    return DMUS_E_SET_UNSUPPORTED;
}

static HRESULT parse_style_ref(struct style_track *This, IStream *stream, const struct chunk_entry *strf)
{
    struct chunk_entry chunk = {.parent = strf};
    IDirectMusicObject *dmobj;
    struct style_item *item;
    HRESULT hr;

    /* First chunk is a timestamp */
    if (stream_get_chunk(stream, &chunk) != S_OK || chunk.id != DMUS_FOURCC_TIME_STAMP_CHUNK)
        return DMUS_E_CHUNKNOTFOUND;
    if (!(item = malloc(sizeof(*item))))
        return E_OUTOFMEMORY;
    hr = stream_chunk_get_data(stream, &chunk, &item->timestamp, sizeof(item->timestamp));
    if (FAILED(hr))
        goto error;

    /* Second chunk is a reference list */
    if (stream_next_chunk(stream, &chunk) != S_OK || chunk.id != FOURCC_LIST ||
            chunk.type != DMUS_FOURCC_REF_LIST) {
        hr = DMUS_E_INVALID_SEGMENTTRIGGERTRACK;
        goto error;
    }
    if (FAILED(hr = dmobj_parsereference(stream, &chunk, &dmobj))) {
        WARN("Failed to load reference: %#lx\n", hr);
        goto error;
    }
    hr = IDirectMusicObject_QueryInterface(dmobj, &IID_IDirectMusicStyle8, (void **)&item->dmstyle);
    if (FAILED(hr)) {
        WARN("Reference not an IDirectMusicStyle8\n");
        IDirectMusicObject_Release(dmobj);
        goto error;
    }

    list_add_tail(&This->Items, &item->entry);
    TRACE("Found reference to style %p with timestamp %lu\n", item->dmstyle, item->timestamp);

    return S_OK;

error:
    free(item);
    return hr;
}

static HRESULT style_track_load(struct track *track, IStream *stream)
{
    struct style_track *This = impl_from_track(track);
    struct chunk_entry parent = {0};
    struct chunk_entry chunk = {.parent = &parent};
    HRESULT hr;

    TRACE("(%p, %p): Loading\n", This, stream);

    if (stream_get_chunk(stream, &parent) != S_OK || parent.id != FOURCC_LIST ||
            parent.type != DMUS_FOURCC_STYLE_TRACK_LIST)
        return DMUS_E_TRACK_NOT_FOUND;

    while ((hr = stream_next_chunk(stream, &chunk)) == S_OK)
        if (chunk.id == FOURCC_LIST && chunk.type == DMUS_FOURCC_STYLE_REF_LIST)
            if (FAILED(hr = parse_style_ref(This, stream, &chunk)))
                break;

    return SUCCEEDED(hr) ? S_OK : hr;
    return S_OK;
}

static const struct track_vtbl style_track_vtbl =
{
    style_track_destroy,
    style_track_has_param,
    style_track_get_param,
    style_track_set_param,
    style_track_load,
};

HRESULT create_dmstyletrack(REFIID lpcGUID, void **ppobj)
{
    struct style_track *track;
    HRESULT hr;

    *ppobj = NULL;
    if (!(track = calloc(1, sizeof(*track)))) return E_OUTOFMEMORY;
    track_init(&track->track, &CLSID_DirectMusicStyleTrack, &style_track_vtbl);
    list_init (&track->Items);

    hr = IDirectMusicTrack8_QueryInterface(&track->track.IDirectMusicTrack8_iface, lpcGUID, ppobj);
    IDirectMusicTrack8_Release(&track->track.IDirectMusicTrack8_iface);
    return hr;
}
