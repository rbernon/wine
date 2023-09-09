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

#include "stddef.h"
#include "stdarg.h"

#include "windef.h"
#include "winbase.h"

#include "dmusici.h"

#include "dmobject.h"

struct track;
struct track_vtbl
{
    void (*destroy)(struct track *track);

    HRESULT (*get_param)(struct track *track, const GUID *type,
            REFERENCE_TIME time, REFERENCE_TIME *next, void *param);
    HRESULT (*set_param)(struct track *track, const GUID *type,
            REFERENCE_TIME time, void *param);
    HRESULT (*has_param)(struct track *track, const GUID *type);

    HRESULT (*init_play)(struct track *track, void **state_data);
    HRESULT (*end_play)(struct track *track, void *state_data);
    HRESULT (*play)(struct track *track, void *state_data, REFERENCE_TIME start, REFERENCE_TIME end,
            REFERENCE_TIME offset, DWORD flags, IDirectMusicPerformance *performance,
            IDirectMusicSegmentState *state, DWORD track_id);

    HRESULT (*load)(struct track *track, IStream *stream);
};

struct track
{
    IDirectMusicTrack8 IDirectMusicTrack8_iface;
    IPersistStream IPersistStream_iface;
    const struct track_vtbl *vtbl;
    DMUS_OBJECTDESC desc;
    LONG ref;
};

void track_init(struct track *track, const GUID *class_id, const struct track_vtbl *vtbl);
