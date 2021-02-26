/*
 * Copyright 2020 Rémi Bernon for CodeWeavers
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

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winnt.h"
#include "winuser.h"
#include "wingdi.h"

#include "wine/gdi_driver.h"
#include "wine/debug.h"

#include "unixlib.h"
#include "win32u.h"

WINE_DEFAULT_DEBUG_CHANNEL(win32u);

struct unix_funcs *unix_funcs;

void CDECL X11DRV_WindowPosChanging( HWND hwnd, HWND insert_after, UINT swp_flags,
                                     const RECT *window_rect, const RECT *client_rect,
                                     RECT *visible_rect, struct window_surface **surface );

void CDECL win32u_WindowPosChanging( HWND hwnd, HWND insert_after, UINT swp_flags,
                                     const RECT *window_rect, const RECT *client_rect,
                                     RECT *visible_rect, struct window_surface **surface )
{
    HWND parent;

    TRACE( "hwnd %p, insert_after %p, swp_flags %x, window_rect %s, client_rect %s, "
           "visible_rect %p, surface %p.\n", hwnd, insert_after, swp_flags,
           wine_dbgstr_rect( window_rect ), wine_dbgstr_rect( client_rect ),
           visible_rect, surface );

    /* create a unix / window surface for top-level windows */
    if ((parent = GetAncestor( hwnd, GA_PARENT )) && parent == GetDesktopWindow())
        win32u_create_toplevel_surface( hwnd );
    else
        win32u_delete_toplevel_surface( hwnd );

    X11DRV_WindowPosChanging( hwnd, insert_after, swp_flags, window_rect, client_rect,
                              visible_rect, surface );
}

void CDECL X11DRV_WindowPosChanged( HWND hwnd, HWND insert_after, UINT swp_flags,
                                    const RECT *window_rect, const RECT *client_rect,
                                    const RECT *visible_rect, const RECT *valid_rects,
                                    struct window_surface *surface );

void CDECL win32u_WindowPosChanged( HWND hwnd, HWND insert_after, UINT swp_flags,
                                    const RECT *window_rect, const RECT *client_rect,
                                    const RECT *visible_rect, const RECT *valid_rects,
                                    struct window_surface *surface )
{
    TRACE( "hwnd %p, insert_after %p, swp_flags %x, window_rect %s, client_rect %s, "
           "visible_rect %s, valid_rects %s, surface %p.\n", hwnd, insert_after, swp_flags,
           wine_dbgstr_rect( window_rect ), wine_dbgstr_rect( client_rect ),
           wine_dbgstr_rect( visible_rect ), wine_dbgstr_rect( valid_rects ), surface );

    X11DRV_WindowPosChanged( hwnd, insert_after, swp_flags, window_rect, client_rect,
                             visible_rect, valid_rects, surface );

    win32u_resize_hwnd_surfaces( hwnd );
}

void CDECL X11DRV_SetParent( HWND hwnd, HWND parent, HWND old_parent );

void CDECL win32u_SetParent( HWND hwnd, HWND parent, HWND old_parent )
{
    TRACE( "hwnd %p, parent %p, old_parent %p.\n", hwnd, parent, old_parent );

    /* create a unix / window surface for top-level windows */
    if (parent == GetDesktopWindow())
        win32u_create_toplevel_surface( hwnd );
    else
        win32u_delete_toplevel_surface( hwnd );

    X11DRV_SetParent( hwnd, parent, old_parent );
}

void CDECL X11DRV_DestroyWindow( HWND hwnd );

void CDECL win32u_DestroyWindow( HWND hwnd )
{
    TRACE( "hwnd %p.\n", hwnd );

    win32u_delete_toplevel_surface( hwnd );

    X11DRV_DestroyWindow( hwnd );
}

LRESULT CDECL X11DRV_WindowMessage( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp );

LRESULT CDECL win32u_WindowMessage( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    TRACE( "hwnd %p, msg %x, wp %lx, lp %lx.\n", hwnd, msg, wp, lp );

    switch (msg)
    {
    case WM_X11DRV_NOTIFY_SURFACE_CREATED:
        if (wp) win32u_create_toplevel_surface_notify( hwnd, lp );
        break;
    }

    return X11DRV_WindowMessage( hwnd, msg, wp, lp );
}

BOOL WINAPI DllMain( HINSTANCE instance, DWORD reason, LPVOID reserved )
{
    TRACE( "instance %p, reason %d, reserved %p.\n", instance, reason, reserved );

    switch (reason)
    {
    case DLL_PROCESS_ATTACH: DisableThreadLibraryCalls( instance ); break;
    case DLL_PROCESS_DETACH: break;
    }

    if (__wine_init_unix_lib( instance, reason, NULL, &unix_funcs )) return FALSE;
    return TRUE;
}
