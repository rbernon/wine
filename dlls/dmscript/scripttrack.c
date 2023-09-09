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

#include "dmscript_private.h"
#include "dmtrack.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmscript);

struct script_track
{
    struct track track;
};

static inline struct script_track *impl_from_track(struct track *iface)
{
    return CONTAINING_RECORD(iface, struct script_track, track);
}

static void script_track_destroy(struct track *track)
{
    struct script_track *This = impl_from_track(track);
    TRACE("(%p)\n", This);
    free(This);
}

static HRESULT script_track_has_param(struct track *track, const GUID *type)
{
    return DMUS_E_TYPE_UNSUPPORTED;
}

static HRESULT script_track_get_param(struct track *track, const GUID *type)
{
    return DMUS_E_GET_UNSUPPORTED;
}

static HRESULT script_track_set_param(struct track *track, const GUID *type)
{
    return DMUS_E_SET_UNSUPPORTED;
}

static HRESULT script_track_load(struct track *track, IStream *stream)
{
    struct script_track *This = impl_from_track(track);
    FIXME("(%p, %p): stub\n", This, stream);
    return S_OK;
}

static const struct track_vtbl script_track_vtbl =
{
    script_track_destroy,
    script_track_has_param,
    script_track_get_param,
    script_track_set_param,
    script_track_load,
};

HRESULT DMUSIC_CreateDirectMusicScriptTrack(REFIID riid, void **ret_iface, IUnknown *pUnkOuter)
{
    struct script_track *track;
    HRESULT hr;

    *ret_iface = NULL;

    if (pUnkOuter)
        return CLASS_E_NOAGGREGATION;

    if (!(track = calloc(1, sizeof(*track)))) return E_OUTOFMEMORY;
    track_init(&track->track, &CLSID_DirectMusicScriptTrack, &script_track_vtbl);

    hr = IDirectMusicTrack8_QueryInterface(&track->track.IDirectMusicTrack8_iface, riid, ret_iface);
    IDirectMusicTrack8_Release(&track->track.IDirectMusicTrack8_iface);
    return hr;
}
