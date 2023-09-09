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

#include "dmime_private.h"
#include "dmtrack.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmime);

struct lyrics_track
{
    struct track track;
};

static inline struct lyrics_track *impl_from_track(struct track *iface)
{
    return CONTAINING_RECORD(iface, struct lyrics_track, track);
}

static void lyrics_track_destroy(struct track *track)
{
    struct lyrics_track *This = impl_from_track(track);
    TRACE("(%p)\n", This);
    free(This);
}

static HRESULT lyrics_track_get_param(struct track *track, const GUID *type,
        MUSIC_TIME time, MUSIC_TIME *next, void *param)
{
    struct lyrics_track *This = impl_from_track(track);
    FIXME("(%p, %p): stub", This, type);
    return DMUS_E_GET_UNSUPPORTED;
}

static HRESULT lyrics_track_set_param(struct track *track, const GUID *type,
        MUSIC_TIME time, void *param)
{
    struct lyrics_track *This = impl_from_track(track);
    FIXME("(%p, %p): stub", This, type);
    return DMUS_E_SET_UNSUPPORTED;
}

static HRESULT lyrics_track_has_param(struct track *track, const GUID *type)
{
    struct lyrics_track *This = impl_from_track(track);
    FIXME("(%p, %p): stub", This, type);
    return DMUS_E_TYPE_UNSUPPORTED;
}

static HRESULT parse_lyrics_track_events(struct lyrics_track *This, IStream *stream,
                struct chunk_entry *lyric)
{
    struct chunk_entry chunk = {.parent = lyric};
    HRESULT hr;
    DMUS_IO_LYRICSTRACK_EVENTHEADER header;
    WCHAR name[256];

    TRACE("Parsing segment form in %p: %s\n", stream, debugstr_chunk(lyric));

    while ((hr = stream_next_chunk(stream, &chunk)) == S_OK) {
        if (chunk.id == FOURCC_LIST && chunk.type == DMUS_FOURCC_LYRICSTRACKEVENT_LIST) {
            struct chunk_entry child = {.parent = &chunk};

            if (FAILED(hr = stream_next_chunk(stream, &child)))
                return  hr;

            if (child.id != DMUS_FOURCC_LYRICSTRACKEVENTHEADER_CHUNK)
                return DMUS_E_UNSUPPORTED_STREAM;

            if (FAILED(hr = stream_chunk_get_data(stream, &child, &header, child.size))) {
                WARN("Failed to read data of %s\n", debugstr_chunk(&child));
                return hr;
            }

            TRACE("Found DMUS_IO_LYRICSTRACK_EVENTHEADER\n");
            TRACE("  - dwFlags %#lx\n", header.dwFlags);
            TRACE("  - dwTimingFlags %#lx\n", header.dwTimingFlags);
            TRACE("  - lTimeLogical %ld\n", header.lTimeLogical);
            TRACE("  - lTimePhysical %ld\n", header.lTimePhysical);

            if (FAILED(hr = stream_next_chunk(stream, &child)))
                return  hr;

            if (child.id != DMUS_FOURCC_LYRICSTRACKEVENTTEXT_CHUNK)
                return DMUS_E_UNSUPPORTED_STREAM;

            if (FAILED(hr = stream_chunk_get_data(stream, &child, &name, child.size))) {
                WARN("Failed to read data of %s\n", debugstr_chunk(&child));
                return hr;
            }

            TRACE("Found DMUS_FOURCC_LYRICSTRACKEVENTTEXT_CHUNK\n");
            TRACE("  - name %s\n", debugstr_w(name));
        }
    }

    return SUCCEEDED(hr) ? S_OK : hr;
}

static HRESULT parse_lyricstrack_list(struct lyrics_track *This, IStream *stream, struct chunk_entry *lyric)
{
    HRESULT hr;
    struct chunk_entry chunk = {.parent = lyric};

    TRACE("Parsing segment form in %p: %s\n", stream, debugstr_chunk(lyric));

    if (FAILED(hr = stream_next_chunk(stream, &chunk)))
        return hr;

    if (chunk.id == FOURCC_LIST && chunk.type == DMUS_FOURCC_LYRICSTRACKEVENTS_LIST)
        hr = parse_lyrics_track_events(This, stream, &chunk);
    else
        hr = DMUS_E_UNSUPPORTED_STREAM;

    return SUCCEEDED(hr) ? S_OK : hr;
}

static HRESULT lyrics_track_load(struct track *track, IStream *stream)
{
    struct lyrics_track *This = impl_from_track(track);
    struct chunk_entry chunk = {0};
    HRESULT hr;

    TRACE("(%p, %p)\n", This, stream);

    if ((hr = stream_get_chunk(stream, &chunk)) != S_OK) return hr;

    if (chunk.id == FOURCC_LIST && chunk.type == DMUS_FOURCC_LYRICSTRACK_LIST)
        hr = parse_lyricstrack_list(This, stream, &chunk);
    else
        hr = DMUS_E_UNSUPPORTED_STREAM;

    return S_OK;
}

static const struct track_vtbl lyrics_track_vtbl =
{
    lyrics_track_destroy,
    lyrics_track_get_param,
    lyrics_track_set_param,
    lyrics_track_has_param,
    lyrics_track_load,
};

HRESULT create_dmlyricstrack(REFIID lpcGUID, void **ppobj)
{
    struct lyrics_track *track;
    HRESULT hr;

    *ppobj = NULL;
    if (!(track = calloc(1, sizeof(*track)))) return E_OUTOFMEMORY;
    track_init(&track->track, &CLSID_DirectMusicLyricsTrack, &lyrics_track_vtbl);

    hr = IDirectMusicTrack8_QueryInterface(&track->track.IDirectMusicTrack8_iface, lpcGUID, ppobj);
    IDirectMusicTrack8_Release(&track->track.IDirectMusicTrack8_iface);
    return hr;
}
