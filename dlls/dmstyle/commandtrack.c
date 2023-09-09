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
WINE_DECLARE_DEBUG_CHANNEL(dmfile);

struct command_track
{
    struct track track;
    struct list Commands;
};

static inline struct command_track *impl_from_track(struct track *iface)
{
    return CONTAINING_RECORD(iface, struct command_track, track);
}

static void command_track_destroy(struct track *track)
{
    struct command_track *This = impl_from_track(track);
    TRACE("(%p)\n", This);
    free(This);
}

static HRESULT command_track_has_param(struct track *track, const GUID *type)
{
    if (IsEqualGUID(type, &GUID_CommandParam)
        || IsEqualGUID(type, &GUID_CommandParam2)
        || IsEqualGUID(type, &GUID_CommandParamNext))
        return S_OK;
    return DMUS_E_TYPE_UNSUPPORTED;
}

static HRESULT command_track_get_param(struct track *track, const GUID *type)
{
    if (IsEqualGUID(type, &GUID_CommandParam)) {
        FIXME("GUID_CommandParam not handled yet\n");
        return S_OK;
    } else if (IsEqualGUID(type, &GUID_CommandParam2)) {
        FIXME("GUID_CommandParam2 not handled yet\n");
        return S_OK;
    } else if (IsEqualGUID(type, &GUID_CommandParamNext)) {
        FIXME("GUID_CommandParamNext not handled yet\n");
        return S_OK;
    }

    return DMUS_E_GET_UNSUPPORTED;
}

static HRESULT command_track_set_param(struct track *track, const GUID *type)
{
    if (IsEqualGUID(type, &GUID_CommandParam)) {
        FIXME("GUID_CommandParam not handled yet\n");
        return S_OK;
    } else if (IsEqualGUID(type, &GUID_CommandParamNext)) {
        FIXME("GUID_CommandParamNext not handled yet\n");
        return S_OK;
    }

    return DMUS_E_SET_UNSUPPORTED;
}

static HRESULT command_track_load(struct track *track, IStream *pStm)
{
        struct command_track *This = impl_from_track(track);
	FOURCC chunkID;
	DWORD chunkSize, dwSizeOfStruct, nrCommands;
	LARGE_INTEGER liMove; /* used when skipping chunks */
	
	IStream_Read (pStm, &chunkID, sizeof(FOURCC), NULL);
	IStream_Read (pStm, &chunkSize, sizeof(DWORD), NULL);
	TRACE_(dmfile)(": %s chunk (size = %ld)", debugstr_fourcc (chunkID), chunkSize);
	switch (chunkID) {
		case DMUS_FOURCC_COMMANDTRACK_CHUNK: {
			DWORD count;
			TRACE_(dmfile)(": command track chunk\n");
			IStream_Read (pStm, &dwSizeOfStruct, sizeof(DWORD), NULL);
			if (dwSizeOfStruct != sizeof(DMUS_IO_COMMAND)) {
				TRACE_(dmfile)(": declared size of struct (=%ld) != actual size; indicates older direct music version\n", dwSizeOfStruct);
			}
			chunkSize -= sizeof(DWORD); /* now chunk size is one DWORD shorter */
			nrCommands = chunkSize/dwSizeOfStruct; /* and this is the number of commands */
			/* load each command separately in new entry */
			for (count = 0; count < nrCommands; count++) {
				LPDMUS_PRIVATE_COMMAND pNewCommand = calloc(1, sizeof(*pNewCommand));
				IStream_Read (pStm, &pNewCommand->pCommand, dwSizeOfStruct, NULL);
				list_add_tail (&This->Commands, &pNewCommand->entry);
			}
			TRACE_(dmfile)(": reading finished\n");
			This->track.desc.dwValidData |= DMUS_OBJ_LOADED;
			break;
		}	
		default: {
			TRACE_(dmfile)(": unexpected chunk; loading failed)\n");
			liMove.QuadPart = chunkSize;
			IStream_Seek (pStm, liMove, STREAM_SEEK_CUR, NULL); /* skip the rest of the chunk */
			return E_FAIL;
		}
	}

	/* DEBUG: dumps whole band track object tree: */
	if (TRACE_ON(dmstyle)) {
		int r = 0;
		DMUS_PRIVATE_COMMAND *tmpEntry;
		struct list *listEntry;
		TRACE("*** struct command_track (%p) ***\n", This);
		TRACE(" - Commands:\n");
		LIST_FOR_EACH (listEntry, &This->Commands) {
			tmpEntry = LIST_ENTRY (listEntry, DMUS_PRIVATE_COMMAND, entry);
			TRACE("    - Command[%i]:\n", r);
			TRACE("       - mtTime = %li\n", tmpEntry->pCommand.mtTime);
			TRACE("       - wMeasure = %d\n", tmpEntry->pCommand.wMeasure);
			TRACE("       - bBeat = %i\n", tmpEntry->pCommand.bBeat);
			TRACE("       - bCommand = %i\n", tmpEntry->pCommand.bCommand);
			TRACE("       - bGrooveLevel = %i\n", tmpEntry->pCommand.bGrooveLevel);
			TRACE("       - bGrooveRange = %i\n", tmpEntry->pCommand.bGrooveRange);
			TRACE("       - bRepeatMode = %i\n", tmpEntry->pCommand.bRepeatMode);			
			r++;
		}
	}

	return S_OK;
}

static const struct track_vtbl command_track_vtbl =
{
    command_track_destroy,
    command_track_has_param,
    command_track_get_param,
    command_track_set_param,
    command_track_load,
};

HRESULT create_dmcommandtrack(REFIID lpcGUID, void **ppobj)
{
    struct command_track *track;
    HRESULT hr;

    *ppobj = NULL;
    if (!(track = calloc(1, sizeof(*track)))) return E_OUTOFMEMORY;
    track_init(&track->track, &CLSID_DirectMusicCommandTrack, &command_track_vtbl);
    list_init (&track->Commands);

    hr = IDirectMusicTrack8_QueryInterface(&track->track.IDirectMusicTrack8_iface, lpcGUID, ppobj);
    IDirectMusicTrack8_Release(&track->track.IDirectMusicTrack8_iface);
    return hr;
}
