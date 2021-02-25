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

#ifndef __WINE_WIN32U_WIN32U_H
#define __WINE_WIN32U_WIN32U_H

#include <stdarg.h>
#include <stdlib.h>

#include "windef.h"
#include "winbase.h"
#include "winnt.h"
#include "winuser.h"
#include "wingdi.h"

#include "wine/gdi_driver.h"

extern struct unix_funcs *unix_funcs DECLSPEC_HIDDEN;

struct toplevel_surface;

enum x11drv_window_messages
{
    WM_X11DRV_UPDATE_CLIPBOARD = 0x80001000,
    WM_X11DRV_SET_WIN_REGION,
    WM_X11DRV_RESIZE_DESKTOP,
    WM_X11DRV_SET_CURSOR,
    WM_X11DRV_NOTIFY_HWND_SURFACE_CREATED,
    WM_X11DRV_NOTIFY_HWND_SURFACE_RESIZE,
    WM_X11DRV_NOTIFY_HWND_SURFACE_REPARENT,
    WM_X11DRV_DESKTOP_SET_HICON_CURSOR,
    WM_X11DRV_DESKTOP_SET_HICON_COLOR,
    WM_X11DRV_DESKTOP_SET_HICON_MASK,
    WM_X11DRV_DESKTOP_SET_WINDOW_CURSOR,
    WM_X11DRV_DESKTOP_CLIP_CURSOR,
    WM_X11DRV_WM_DELETE_WINDOW,
    WM_X11DRV_WM_TAKE_FOCUS,
};

enum x11drv_escape_codes
{
    X11DRV_SET_DRAWABLE,     /* set current drawable for a DC */
    X11DRV_GET_DRAWABLE,     /* get current drawable for a DC */
    X11DRV_START_EXPOSURES,  /* start graphics exposures */
    X11DRV_END_EXPOSURES,    /* end graphics exposures */
    X11DRV_FLUSH_GL_DRAWABLE,/* flush changes made to the gl drawable */
    X11DRV_FLUSH_VK_DRAWABLE,/* flush changes made to the vulkan drawable */
};

extern void win32u_create_toplevel_surface( HWND hwnd ) DECLSPEC_HIDDEN;
extern void win32u_create_toplevel_surface_notify( HWND hwnd, LPARAM param ) DECLSPEC_HIDDEN;
extern void win32u_create_client_surface( HWND hwnd ) DECLSPEC_HIDDEN;
extern void win32u_create_client_surface_notify( HWND hwnd, LPARAM param ) DECLSPEC_HIDDEN;
extern void win32u_delete_toplevel_surface( HWND hwnd ) DECLSPEC_HIDDEN;
extern void win32u_delete_hwnd_surfaces( HWND hwnd ) DECLSPEC_HIDDEN;
extern void win32u_resize_hwnd_surfaces( HWND hwnd ) DECLSPEC_HIDDEN;
extern void win32u_resize_hwnd_surfaces_notify( HWND hwnd, BOOL enable );
extern void win32u_reparent_hwnd_surfaces_notify( HWND hwnd, BOOL enable );
extern void win32u_present_client_surface( HWND hwnd, HRGN region ) DECLSPEC_HIDDEN;
extern void win32u_update_window_surface( HWND hwnd, const RECT *visible_rect, struct window_surface **window_surface ) DECLSPEC_HIDDEN;

#endif /* __WINE_WIN32U_WIN32U_H */
