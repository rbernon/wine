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
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#endif

#ifdef HAVE_CAIRO_CAIRO_H
#include <cairo/cairo.h>
#endif
#ifdef HAVE_CAIRO_CAIRO_XLIB_H
#include <cairo/cairo-xlib.h>
#endif
#ifdef HAVE_CAIRO_CAIRO_XLIB_XRENDER_H
#include <cairo/cairo-xlib-xrender.h>
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

#ifdef HAVE_X11_XLIB_H
#define MAKE_FUNCPTR(f) extern typeof(f) *p##f DECLSPEC_HIDDEN;
MAKE_FUNCPTR(XCreateColormap)
MAKE_FUNCPTR(XCreatePixmap)
MAKE_FUNCPTR(XCreateWindow)
MAKE_FUNCPTR(XDestroyWindow)
MAKE_FUNCPTR(XFlush)
MAKE_FUNCPTR(XFree)
MAKE_FUNCPTR(XFreeColormap)
MAKE_FUNCPTR(XFreePixmap)
MAKE_FUNCPTR(XGetWindowAttributes)
MAKE_FUNCPTR(XInitThreads)
MAKE_FUNCPTR(XMapWindow)
MAKE_FUNCPTR(XOpenDisplay)
MAKE_FUNCPTR(XQueryExtension)
MAKE_FUNCPTR(XSync)
#undef MAKE_FUNCPTR
#endif

#ifdef HAVE_CAIRO_CAIRO_H
#define MAKE_FUNCPTR(f) extern typeof(f) *p_##f DECLSPEC_HIDDEN;
MAKE_FUNCPTR(cairo_surface_create_similar_image)
MAKE_FUNCPTR(cairo_surface_map_to_image)
MAKE_FUNCPTR(cairo_surface_unmap_image)
MAKE_FUNCPTR(cairo_surface_destroy)
MAKE_FUNCPTR(cairo_surface_flush)
MAKE_FUNCPTR(cairo_surface_write_to_png_stream)
MAKE_FUNCPTR(cairo_surface_mark_dirty)
MAKE_FUNCPTR(cairo_surface_mark_dirty_rectangle)
MAKE_FUNCPTR(cairo_image_surface_get_data)
MAKE_FUNCPTR(cairo_image_surface_get_width)
MAKE_FUNCPTR(cairo_image_surface_get_height)
MAKE_FUNCPTR(cairo_image_surface_get_stride)
#undef MAKE_FUNCPTR
#endif

#ifdef HAVE_CAIRO_CAIRO_XLIB_H
#define MAKE_FUNCPTR(f) extern typeof(f) *p_##f DECLSPEC_HIDDEN;
MAKE_FUNCPTR(cairo_xlib_surface_create)
MAKE_FUNCPTR(cairo_xlib_surface_set_drawable)
MAKE_FUNCPTR(cairo_xlib_surface_set_size)
#undef MAKE_FUNCPTR
#endif

#ifdef HAVE_CAIRO_CAIRO_XLIB_XRENDER_H
#define MAKE_FUNCPTR(f) extern typeof(f) *p_##f DECLSPEC_HIDDEN;
MAKE_FUNCPTR(cairo_xlib_surface_create_with_xrender_format)
#undef MAKE_FUNCPTR
#endif

extern struct window_surface *CDECL x11_create_window_surface( const RECT *screen_rect,
                                                               const RECT *visible_rect, HWND hwnd,
                                                               UINT64 unix_handle ) DECLSPEC_HIDDEN;
extern struct window_surface *CDECL x11_resize_window_surface( struct window_surface *base, const RECT *screen_rect,
                                                               const RECT *visible_rect, HWND hwnd,
                                                               UINT64 unix_handle ) DECLSPEC_HIDDEN;

#endif /* __WINE_WIN32U_UNIX_X11_H */
