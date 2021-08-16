/*
 * Copyright (c) 2015 Andrew Eikum for CodeWeavers
 * Copyright (c) 2018 Ethan Lee for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "windef.h"
#include "wine/list.h"

#include "xaudio2.h"
#include "xapo.h"

#include <FAudio.h>
#include <FAPO.h>

#if XAUDIO2_VER == 0
#define COMPAT_E_INVALID_CALL E_INVALIDARG
#define COMPAT_E_DEVICE_INVALIDATED XAUDIO20_E_DEVICE_INVALIDATED
#else
#define COMPAT_E_INVALID_CALL XAUDIO2_E_INVALID_CALL
#define COMPAT_E_DEVICE_INVALIDATED XAUDIO2_E_DEVICE_INVALIDATED
#endif

typedef struct _XA2XAPOImpl {
    IXAPO *xapo;
    IXAPOParameters *xapo_params;

    LONG ref;

    FAPO FAPO_vtbl;
} XA2XAPOImpl;

typedef struct _XA2XAPOFXImpl {
    IXAPO IXAPO_iface;
    IXAPOParameters IXAPOParameters_iface;

    FAPO *fapo;
} XA2XAPOFXImpl;

/* xapo.c */
extern HRESULT make_xapo_factory(REFCLSID clsid, REFIID riid, void **ppv) DECLSPEC_HIDDEN;

/* xaudio_allocator.c */
extern void* XAudio_Internal_Malloc(size_t size) DECLSPEC_HIDDEN;
extern void XAudio_Internal_Free(void* ptr) DECLSPEC_HIDDEN;
extern void* XAudio_Internal_Realloc(void* ptr, size_t size) DECLSPEC_HIDDEN;
