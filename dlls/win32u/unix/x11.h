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

#ifndef __WINE_WIN32U_UNIX_X11_H
#define __WINE_WIN32U_UNIX_X11_H

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

#ifdef HAVE_X11_XLIB_H
#include <X11/Xlib.h>
#endif

#ifdef HAVE_X11_EXTENSIONS_XCOMPOSITE_H
#include <X11/extensions/Xcomposite.h>
#endif

#ifdef HAVE_CAIRO_CAIRO_H
#include <cairo/cairo.h>
#endif
#ifdef HAVE_CAIRO_CAIRO_XLIB_H
#include <cairo/cairo-xlib.h>
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

#include "wine/gdi_driver.h"
#include "wine/debug.h"

#include "unixlib.h"

#ifdef HAVE_X11_XLIB_H
#define MAKE_FUNCPTR(f) extern typeof(f) *p##f DECLSPEC_HIDDEN;
MAKE_FUNCPTR(XGetWindowAttributes)
MAKE_FUNCPTR(XConfigureWindow)
MAKE_FUNCPTR(XReparentWindow)
#undef MAKE_FUNCPTR
#endif

#ifdef HAVE_X11_EXTENSIONS_XCOMPOSITE_H
#define MAKE_FUNCPTR(f) extern typeof(f) * p##f DECLSPEC_HIDDEN;
MAKE_FUNCPTR(XCompositeQueryExtension)
MAKE_FUNCPTR(XCompositeQueryVersion)
MAKE_FUNCPTR(XCompositeRedirectWindow)
MAKE_FUNCPTR(XCompositeUnredirectWindow)
#undef MAKE_FUNCPTR
#endif

#ifdef HAVE_CAIRO_CAIRO_H
#define MAKE_FUNCPTR(f) extern typeof(f) *p_##f DECLSPEC_HIDDEN;
MAKE_FUNCPTR(cairo_clip)
MAKE_FUNCPTR(cairo_create)
MAKE_FUNCPTR(cairo_destroy)
MAKE_FUNCPTR(cairo_fill)
MAKE_FUNCPTR(cairo_image_surface_get_data)
MAKE_FUNCPTR(cairo_image_surface_get_height)
MAKE_FUNCPTR(cairo_image_surface_get_stride)
MAKE_FUNCPTR(cairo_image_surface_get_width)
MAKE_FUNCPTR(cairo_move_to)
MAKE_FUNCPTR(cairo_paint_with_alpha)
MAKE_FUNCPTR(cairo_rectangle)
MAKE_FUNCPTR(cairo_set_source_rgba)
MAKE_FUNCPTR(cairo_set_source_surface)
MAKE_FUNCPTR(cairo_surface_create_similar_image)
MAKE_FUNCPTR(cairo_surface_destroy)
MAKE_FUNCPTR(cairo_surface_flush)
MAKE_FUNCPTR(cairo_surface_mark_dirty_rectangle)
MAKE_FUNCPTR(cairo_surface_reference)
MAKE_FUNCPTR(cairo_set_source_rgba)
MAKE_FUNCPTR(cairo_stroke)
MAKE_FUNCPTR(cairo_text_path)
#undef MAKE_FUNCPTR
#endif

#ifdef HAVE_CAIRO_CAIRO_XLIB_H
#define MAKE_FUNCPTR(f) extern typeof(f) *p_##f DECLSPEC_HIDDEN;
MAKE_FUNCPTR(cairo_xlib_surface_create)
MAKE_FUNCPTR(cairo_xlib_surface_get_drawable)
MAKE_FUNCPTR(cairo_xlib_surface_get_display)
MAKE_FUNCPTR(cairo_xlib_surface_set_size)
#undef MAKE_FUNCPTR
#endif

extern struct unix_surface *CDECL cairo_surface_create_toplevel( HWND hwnd ) DECLSPEC_HIDDEN;
extern struct unix_surface *CDECL cairo_surface_create_drawable( struct unix_surface *target, BITMAP *bitmap ) DECLSPEC_HIDDEN;
extern struct unix_surface *CDECL cairo_surface_create_client( HWND hwnd ) DECLSPEC_HIDDEN;
extern void CDECL cairo_surface_create_notify( struct unix_surface *surface, LPARAM param ) DECLSPEC_HIDDEN;
extern void CDECL cairo_surface_delete( struct unix_surface *surface ) DECLSPEC_HIDDEN;
extern void CDECL cairo_surface_present( struct unix_surface *target, struct unix_surface *source, const POINT *target_pos, const RECT *source_rect, UINT clip_rect_count, const RECT *clip_rects ) DECLSPEC_HIDDEN;
extern void CDECL cairo_surface_resize( struct unix_surface *surface, struct unix_surface *parent, const RECT *rect ) DECLSPEC_HIDDEN;
extern void CDECL cairo_surface_resize_notify( struct unix_surface *surface, struct unix_surface *parent, const RECT *rect ) DECLSPEC_HIDDEN;
extern void CDECL cairo_surface_set_offscreen( struct unix_surface *surface, BOOL offscreen ) DECLSPEC_HIDDEN;

#endif /* __WINE_WIN32U_UNIX_X11_H */
