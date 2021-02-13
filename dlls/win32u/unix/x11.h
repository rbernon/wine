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
# error You must include config.h to use this header
#endif

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_X11_XLIB_H
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
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
#ifdef HAVE_CAIRO_CAIRO_XLIB_XRENDER_H
#include <cairo/cairo-xlib-xrender.h>
#endif

#undef Status  /* avoid conflict with wintrnl.h */
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
#define MAKE_FUNCPTR(f) extern typeof(f) * p##f DECLSPEC_HIDDEN;
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

#ifdef HAVE_X11_EXTENSIONS_XCOMPOSITE_H
#define MAKE_FUNCPTR(f) extern typeof(f) * p##f DECLSPEC_HIDDEN;
MAKE_FUNCPTR(XCompositeCreateRegionFromBorderClip)
MAKE_FUNCPTR(XCompositeNameWindowPixmap)
MAKE_FUNCPTR(XCompositeQueryExtension)
MAKE_FUNCPTR(XCompositeQueryVersion)
MAKE_FUNCPTR(XCompositeRedirectSubwindows)
MAKE_FUNCPTR(XCompositeRedirectWindow)
MAKE_FUNCPTR(XCompositeUnredirectSubwindows)
MAKE_FUNCPTR(XCompositeUnredirectWindow)
MAKE_FUNCPTR(XCompositeVersion)
#undef MAKE_FUNCPTR
#endif

#ifdef HAVE_CAIRO_CAIRO_XLIB_H
#define MAKE_FUNCPTR(f) extern typeof(f) *p_##f DECLSPEC_HIDDEN;
MAKE_FUNCPTR(cairo_xlib_surface_create)
#undef MAKE_FUNCPTR
#endif

extern Display *gdi_display DECLSPEC_HIDDEN;
extern XVisualInfo default_visual DECLSPEC_HIDDEN;
extern BOOL usexcomposite DECLSPEC_HIDDEN;
extern Window root_window DECLSPEC_HIDDEN;

typedef int (*x11drv_error_callback)( Display *display, XErrorEvent *event, void *arg );
extern void X11DRV_expect_error( Display *display, x11drv_error_callback callback, void *arg ) DECLSPEC_HIDDEN;
extern int X11DRV_check_error(void) DECLSPEC_HIDDEN;

extern Window create_client_window( HWND hwnd, const XVisualInfo *visual ) DECLSPEC_HIDDEN;
extern void destroy_gl_drawable( HWND hwnd ) DECLSPEC_HIDDEN;

#define X11DRV_ESCAPE 6789
enum x11drv_escape_codes
{
    X11DRV_SET_DRAWABLE,     /* set current drawable for a DC */
    X11DRV_GET_DRAWABLE,     /* get current drawable for a DC */
    X11DRV_START_EXPOSURES,  /* start graphics exposures */
    X11DRV_END_EXPOSURES,    /* end graphics exposures */
    X11DRV_FLUSH_GL_DRAWABLE /* flush changes made to the gl drawable */
};

struct x11drv_escape_set_drawable
{
    enum x11drv_escape_codes code;         /* escape code (X11DRV_SET_DRAWABLE) */
    Drawable                 drawable;     /* X drawable */
    int                      mode;         /* ClipByChildren or IncludeInferiors */
    RECT                     dc_rect;      /* DC rectangle relative to drawable */
};

struct x11drv_escape_flush_gl_drawable
{
    enum x11drv_escape_codes code;         /* escape code (X11DRV_FLUSH_GL_DRAWABLE) */
    Drawable                 gl_drawable;  /* GL drawable */
    BOOL                     flush;        /* flush X11 before copying */
};

extern struct window_surface* CDECL x11_create_window_surface(const RECT *screen_rect, const RECT *visible_rect, HWND hwnd, UINT64 unix_handle) DECLSPEC_HIDDEN;
extern struct window_surface *CDECL x11_resize_window_surface(struct window_surface *base, const RECT *screen_rect, const RECT *visible_rect, HWND hwnd, UINT64 unix_handle) DECLSPEC_HIDDEN;

#endif
