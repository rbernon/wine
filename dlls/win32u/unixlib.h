/*
 * Win32u Unix interface
 *
 * Copyright (C) 2021 Rémi Bernon for CodeWeavers
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

#ifndef __WIN32U_UNIXLIB_H
#define __WIN32U_UNIXLIB_H

#include "winternl.h"
#include "wingdi.h"

struct unix_surface;

struct unix_funcs
{
    struct unix_surface *(CDECL *surface_create_toplevel)( HWND );
    struct unix_surface *(CDECL *surface_create_foreign)( HWND );
    struct unix_surface *(CDECL *surface_create_drawable)( struct unix_surface *, BITMAP * );
    struct unix_surface *(CDECL *surface_create_client)( HWND, LPARAM *id );
    void (CDECL *surface_create_notify)( struct unix_surface *, LPARAM param );
    void (CDECL *surface_delete)( struct unix_surface * );
    void (CDECL *surface_present)( struct unix_surface *, struct unix_surface *, const POINT *, const RECT *, UINT, const RECT * );
    void (CDECL *surface_resize)( struct unix_surface *, struct unix_surface *, const RECT * );
    void (CDECL *surface_resize_notify)( struct unix_surface *, struct unix_surface *, const RECT * );
    void (CDECL *surface_set_offscreen)( struct unix_surface *, BOOL );
};

extern NTSTATUS CDECL __wine_init_unix_lib( HMODULE module, DWORD reason, const void *ptr_in,
                                            void *ptr_out ) DECLSPEC_HIDDEN;

#endif /* __WIN32U_UNIXLIB_H */
