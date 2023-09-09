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

struct param_control_track
{
    struct track track;
};

static inline struct param_control_track *impl_from_track(struct track *iface)
{
    return CONTAINING_RECORD(iface, struct param_control_track, track);
}

static void param_control_track_destroy(struct track *track)
{
    struct param_control_track *This = impl_from_track(track);
    TRACE("(%p)\n", This);
    free(This);
}

static HRESULT param_control_track_get_param(struct track *track, const GUID *type,
        MUSIC_TIME time, MUSIC_TIME *next, void *param)
{
    struct param_control_track *This = impl_from_track(track);
    FIXME("(%p, %p): stub", This, type);
    return DMUS_E_GET_UNSUPPORTED;
}

static HRESULT param_control_track_set_param(struct track *track, const GUID *type,
        MUSIC_TIME time, void *param)
{
    struct param_control_track *This = impl_from_track(track);
    FIXME("(%p, %p): stub", This, type);
    return DMUS_E_SET_UNSUPPORTED;
}

static HRESULT param_control_track_has_param(struct track *track, const GUID *type)
{
    struct param_control_track *This = impl_from_track(track);
    FIXME("(%p, %p): stub", This, type);
    return DMUS_E_TYPE_UNSUPPORTED;
}

static HRESULT param_control_track_load(struct track *track, IStream *stream)
{
    struct param_control_track *This = impl_from_track(track);
    FIXME("(%p, %p): stub\n", This, stream);
    return S_OK;
}

static const struct track_vtbl param_control_track_vtbl =
{
    param_control_track_destroy,
    param_control_track_get_param,
    param_control_track_set_param,
    param_control_track_has_param,
    param_control_track_load,
};

HRESULT create_dmparamcontroltrack(REFIID lpcGUID, void **ppobj)
{
    struct param_control_track *track;
    HRESULT hr;

    *ppobj = NULL;
    if (!(track = calloc(1, sizeof(*track)))) return E_OUTOFMEMORY;
    track_init(&track->track, &CLSID_DirectMusicParamControlTrack, &param_control_track_vtbl);

    hr = IDirectMusicTrack8_QueryInterface(&track->track.IDirectMusicTrack8_iface, lpcGUID, ppobj);
    IDirectMusicTrack8_Release(&track->track.IDirectMusicTrack8_iface);
    return hr;
}
