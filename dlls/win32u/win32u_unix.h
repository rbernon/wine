/*
 * Copyright 2021 Rémi Bernon for CodeWeavers
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

#ifndef __WINE_WIN32U_UNIX_H
#define __WINE_WIN32U_UNIX_H

#ifndef __WINE_CONFIG_H
#error You must include config.h to use this header
#endif

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_XCB_XCB_H
#include <xcb/xcb.h>
#endif

#ifdef HAVE_CAIRO_CAIRO_H
#include <cairo/cairo.h>
#endif
#ifdef HAVE_CAIRO_CAIRO_XCB_H
#include <cairo/cairo-xcb.h>
#endif

#undef Status /* avoid conflict with wintrnl.h */
typedef int Status;

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "winternl.h"
#include "winnt.h"
#include "winbase.h"
#include "winuser.h"
#include "wingdi.h"

#include "unixlib.h"

#ifdef HAVE_XCB_XCB_H
#define MAKE_FUNCPTR(f) extern typeof(f) *p_##f DECLSPEC_HIDDEN;
MAKE_FUNCPTR(xcb_connect)
MAKE_FUNCPTR(xcb_connection_has_error)
MAKE_FUNCPTR(xcb_depth_next)
MAKE_FUNCPTR(xcb_depth_visuals_iterator)
MAKE_FUNCPTR(xcb_get_setup)
MAKE_FUNCPTR(xcb_get_geometry)
MAKE_FUNCPTR(xcb_get_geometry_reply)
MAKE_FUNCPTR(xcb_get_window_attributes)
MAKE_FUNCPTR(xcb_get_window_attributes_reply)
MAKE_FUNCPTR(xcb_screen_allowed_depths_iterator)
MAKE_FUNCPTR(xcb_screen_next)
MAKE_FUNCPTR(xcb_setup_roots_iterator)
MAKE_FUNCPTR(xcb_visualtype_next)
#undef MAKE_FUNCPTR
extern xcb_connection_t *xcb DECLSPEC_HIDDEN;
#endif

#ifdef HAVE_CAIRO_CAIRO_H
#define MAKE_FUNCPTR(f) extern typeof(f) *p_##f DECLSPEC_HIDDEN;
MAKE_FUNCPTR(cairo_surface_destroy)
#undef MAKE_FUNCPTR
#endif

#ifdef HAVE_CAIRO_CAIRO_XCB_H
#define MAKE_FUNCPTR(f) extern typeof(f) *p_##f DECLSPEC_HIDDEN;
MAKE_FUNCPTR(cairo_xcb_surface_create)
#undef MAKE_FUNCPTR
#endif

extern struct unix_surface *CDECL cairo_surface_create_foreign( HWND hwnd, LPARAM id ) DECLSPEC_HIDDEN;
extern void CDECL cairo_surface_delete( struct unix_surface *surface ) DECLSPEC_HIDDEN;

#endif /* __WINE_WIN32U_UNIX_H */
