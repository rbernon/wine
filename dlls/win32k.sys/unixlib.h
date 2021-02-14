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

#ifndef __WIN32K_UNIXLIB_H
#define __WIN32K_UNIXLIB_H

#include "winternl.h"
#include "wingdi.h"

#include "wine/gdi_driver.h"

struct unix_funcs
{
    DWORD (CDECL *msg_wait_for_multiple_objects)( DWORD count, const HANDLE *handles, DWORD timeout,
                                                  DWORD mask, DWORD flags );
    BOOL (CDECL *create_window)( HWND hwnd );
    void (CDECL *destroy_window)( HWND hwnd );
    void (CDECL *window_pos_changing)( HWND hwnd, HWND insert_after, UINT swp_flags,
                                     const RECT *window_rect, const RECT *client_rect, RECT *visible_rect,
                                     struct window_surface **surface, RECT *screen_rect, void **driver_handle );
    void (CDECL *window_pos_changed)( HWND hwnd, HWND insert_after, UINT swp_flags,
                                    const RECT *rectWindow, const RECT *rectClient,
                                    const RECT *visible_rect, const RECT *valid_rects,
                                    struct window_surface *surface );
    UINT (CDECL *show_window)( HWND hwnd, INT cmd, RECT *rect, UINT swp );
    void (CDECL *set_window_style)( HWND hwnd, INT offset, STYLESTRUCT *style );
    void (CDECL *set_focus)( HWND hwnd );
    void (CDECL *set_parent)( HWND hwnd, HWND parent, HWND old_parent );
    INT (CDECL *to_unicode)( UINT virtKey, UINT scanCode, const BYTE *lpKeyState,
                             LPWSTR bufW, int bufW_size, UINT flags, HKL hkl );
};

extern NTSTATUS CDECL __wine_init_unix_lib( HMODULE module, DWORD reason, const void *ptr_in,
                                            void *ptr_out ) DECLSPEC_HIDDEN;

#endif /* __WIN32K_UNIXLIB_H */
