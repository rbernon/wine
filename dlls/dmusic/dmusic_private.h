/*
 * DirectMusic Private Include
 *
 * Copyright (C) 2003-2004 Rok Mandeljc
 * Copyright (C) 2012 Christian Costa
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

#ifndef __WINE_DMUSIC_PRIVATE_H
#define __WINE_DMUSIC_PRIVATE_H

#include <stdarg.h>

#define COBJMACROS
#include "windef.h"
#include "winbase.h"
#include "winnt.h"
#include "wingdi.h"
#include "winuser.h"

#include "wine/debug.h"
#include "wine/list.h"
#include "winreg.h"
#include "objbase.h"

#include "dmusici.h"
#include "dmusicf.h"
#include "dmusics.h"
#include "dmksctrl.h"

#include "dmobject.h"
#include "dmusic_wave.h"

/*****************************************************************************
 * Interfaces
 */
typedef struct IDirectMusic8Impl IDirectMusic8Impl;
typedef struct IReferenceClockImpl IReferenceClockImpl;

/*****************************************************************************
 * Some stuff to make my life easier :=)
 */
 
typedef struct port_info {
    DMUS_PORTCAPS caps;
    HRESULT (*create)(IDirectMusic8Impl *parent, DMUS_PORTPARAMS *port_params,
            DMUS_PORTCAPS *port_caps, IDirectMusicPort **port);
    ULONG device;
} port_info;

/*****************************************************************************
 * ClassFactory
 */

struct collection;
extern void collection_internal_addref(struct collection *collection);
extern void collection_internal_release(struct collection *collection);
extern HRESULT collection_get_wave(struct collection *collection, DWORD index, IDirectMusicObject **out);

/* CLSID */
extern HRESULT music_create(IUnknown **ret_iface);
extern HRESULT collection_create(IUnknown **ret_iface);
extern HRESULT wave_create(IUnknown **ret_iface);

extern HRESULT buffer_create(DMUS_BUFFERDESC *desc, IDirectMusicBuffer **ret_iface);
extern HRESULT download_create(DWORD size, IDirectMusicDownload **ret_iface);
extern HRESULT reference_clock_create(IReferenceClock **ret_iface);

extern HRESULT instrument_create_from_soundfont(struct soundfont *soundfont, UINT index,
        struct collection *collection, DMUS_OBJECTDESC *desc, IDirectMusicInstrument **ret_iface);
extern HRESULT instrument_create_from_chunk(IStream *stream, struct chunk_entry *parent,
        struct collection *collection, DMUS_OBJECTDESC *desc, IDirectMusicInstrument **ret_iface);
extern HRESULT instrument_download_to_port(IDirectMusicInstrument *iface, IDirectMusicPortDownload *port,
        IDirectMusicDownloadedInstrument **downloaded);
extern HRESULT instrument_unload_from_port(IDirectMusicDownloadedInstrument *iface, IDirectMusicPortDownload *port);

/*****************************************************************************
 * IDirectMusic8Impl implementation structure
 */
struct IDirectMusic8Impl {
    IDirectMusic8 IDirectMusic8_iface;
    LONG ref;
    IDirectSound *dsound;
    IReferenceClock *master_clock;
    IDirectMusicPort **ports;
    int num_ports;
    port_info *system_ports;
    int num_system_ports;
};

/** Internal factory */
extern HRESULT synth_port_create(IDirectMusic8Impl *parent, DMUS_PORTPARAMS *port_params,
        DMUS_PORTCAPS *port_caps, IDirectMusicPort **port);
extern HRESULT midi_out_port_create(IDirectMusic8Impl *parent, DMUS_PORTPARAMS *port_params,
        DMUS_PORTCAPS *port_caps, IDirectMusicPort **port);
extern HRESULT midi_in_port_create(IDirectMusic8Impl *parent, DMUS_PORTPARAMS *port_params,
        DMUS_PORTCAPS *port_caps, IDirectMusicPort **port);

/*****************************************************************************
 * IReferenceClockImpl implementation structure
 */
struct IReferenceClockImpl {
    /* IUnknown fields */
    IReferenceClock IReferenceClock_iface;
    LONG ref;

    /* IReferenceClockImpl fields */
    REFERENCE_TIME rtTime;
    DMUS_CLOCKINFO pClockInfo;
};

/*****************************************************************************
 * Misc.
 */
void dmusic_remove_port(IDirectMusic8Impl *dmusic, IDirectMusicPort *port);

/* used for generic dumping (copied from ddraw) */
typedef struct {
    DWORD val;
    const char* name;
} flag_info;

#define FE(x) { x, #x }	

/* dwPatch from MIDILOCALE */
extern DWORD MIDILOCALE2Patch (const MIDILOCALE *pLocale);
/* MIDILOCALE from dwPatch */
extern void Patch2MIDILOCALE (DWORD dwPatch, LPMIDILOCALE pLocale);

/* Dump whole DMUS_PORTPARAMS struct */
extern void dump_DMUS_PORTPARAMS(LPDMUS_PORTPARAMS params);

#endif /* __WINE_DMUSIC_PRIVATE_H */
