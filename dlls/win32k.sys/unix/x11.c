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

#include "config.h"
#include "wine/port.h"

#include "x11.h"
#include "unixlib.h"

#include "x11drv.h"

WINE_DEFAULT_DEBUG_CHANNEL(win32k);

BOOL CDECL X11DRV_CreateWindow( HWND hwnd );
void CDECL X11DRV_DestroyWindow( HWND hwnd );
void CDECL X11DRV_WindowPosChanging( HWND hwnd, HWND insert_after, UINT swp_flags,
                                     const RECT *window_rect, const RECT *client_rect,
                                     RECT *visible_rect, struct window_surface **surface,
                                     RECT *screen_rect, void **driver_handle );
void CDECL X11DRV_WindowPosChanged( HWND hwnd, HWND insert_after, UINT swp_flags, const RECT *rectWindow,
                                    const RECT *rectClient, const RECT *visible_rect,
                                    const RECT *valid_rects, struct window_surface *surface );
UINT CDECL X11DRV_ShowWindow( HWND hwnd, INT cmd, RECT *rect, UINT swp );
void CDECL X11DRV_SetWindowStyle( HWND hwnd, INT offset, STYLESTRUCT *style );
void CDECL X11DRV_SetParent( HWND hwnd, HWND parent, HWND old_parent );
INT CDECL X11DRV_ToUnicodeEx( UINT virtKey, UINT scanCode, const BYTE *lpKeyState, LPWSTR bufW,
                              int bufW_size, UINT flags, HKL hkl );

static struct unix_funcs unix_funcs = {
    X11DRV_MsgWaitForMultipleObjectsEx,
    X11DRV_CreateWindow,
    X11DRV_DestroyWindow,
    X11DRV_WindowPosChanging,
    X11DRV_WindowPosChanged,
    X11DRV_ShowWindow,
    X11DRV_SetWindowStyle,
    X11DRV_SetFocus,
    X11DRV_SetParent,
    X11DRV_ToUnicodeEx,
};

NTSTATUS CDECL __wine_init_unix_lib( HMODULE module, DWORD reason, const void *ptr_in, void *ptr_out )
{
    TRACE( "module %p, reason %x, ptr_in %p, ptr_out %p.\n", module, reason, ptr_in, ptr_out );

    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        x11drv_module = module;
        if (!x11drv_process_attach()) return STATUS_DLL_NOT_FOUND;
        break;
    case DLL_PROCESS_DETACH: break;
    }

    *(void **)ptr_out = &unix_funcs;
    return STATUS_SUCCESS;
}
