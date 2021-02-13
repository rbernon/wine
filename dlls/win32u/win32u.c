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

#include "basetsd.h"
#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winnt.h"
#include "winternl.h"
#include "winuser.h"
#include "wingdi.h"

#include "wine/debug.h"
#include "wine/win32u.h"
#include "wine/gdi_driver.h"

#include "unixlib.h"

WINE_DEFAULT_DEBUG_CHANNEL(win32u);

static struct unix_funcs *unix_funcs;

static inline BOOL set_ntstatus( NTSTATUS status )
{
    if (status) SetLastError( RtlNtStatusToDosError( status ));
    return !status;
}

static BOOL desktop_ioctl( DWORD code, void *in_buff, DWORD in_count,
                           void *out_buff, DWORD out_count, DWORD *read )
{
    IO_STATUS_BLOCK io;
    NTSTATUS status;

    status = NtDeviceIoControlFile( GetThreadDesktop(GetCurrentThreadId()), NULL, NULL, NULL,
                                    &io, code, in_buff, in_count, out_buff, out_count );
    switch( status )
    {
    case STATUS_SUCCESS:
        if (read) *read = io.Information;
        return TRUE;
    case STATUS_INVALID_PARAMETER:
        ERR("%s (code=%08x) ioctl failed: invalid parameter!\n", debugstr_win32u_ioctl(code), code);
        break;
    default:
        ERR("%s (code=%08x) ioctl failed: %08x!\n", debugstr_win32u_ioctl(code), code, status);
        status = STATUS_INVALID_HANDLE;
        break;
    }
    if (read) *read = 0;
    return set_ntstatus( status );
}

BOOL CDECL WIN32U_CreateWindow(HWND hwnd)
{
    struct win32u_create_window_input in;

    TRACE("hwnd %p\n", hwnd);

    in.hwnd = HandleToULong(hwnd);

    if (!desktop_ioctl(IOCTL_WIN32U_CREATE_WINDOW, &in, sizeof(in), NULL, 0, NULL))
        return FALSE;

    return TRUE;
}

void CDECL WIN32U_DestroyWindow( HWND hwnd )
{
    struct win32u_destroy_window_input in;

    TRACE("hwnd %p\n", hwnd);

    in.hwnd = HandleToULong(hwnd);

    if (!desktop_ioctl(IOCTL_WIN32U_DESTROY_WINDOW, &in, sizeof(in), NULL, 0, NULL))
        return;
}

void CDECL WIN32U_WindowPosChanging( HWND hwnd, HWND insert_after, UINT swp_flags,
                                     const RECT *window_rect, const RECT *client_rect, RECT *visible_rect,
                                     struct window_surface **surface )
{
    struct win32u_window_pos_changing_input in;
    struct win32u_window_pos_changing_output out;

    TRACE("hwnd %p, insert_after %p, swp_flags %x, window_rect %s, client_rect %s, visible_rect %p, surface %p\n",
          hwnd, insert_after, swp_flags, wine_dbgstr_rect(window_rect), wine_dbgstr_rect(client_rect),
          visible_rect, surface);

    in.hwnd = HandleToUlong(hwnd);
    in.insert_after = HandleToUlong(insert_after);
    in.swp_flags = swp_flags;
    in.window_rect = *window_rect;
    in.client_rect = *client_rect;

    memset(&out, 0, sizeof(out));

    if (!desktop_ioctl(IOCTL_WIN32U_WINDOW_POS_CHANGING, &in, sizeof(in), &out, sizeof(out), NULL))
        return;

    TRACE("returned: visible_rect %s\n", wine_dbgstr_rect(&out.visible_rect));

    *visible_rect = out.visible_rect;
    if (!*surface) *surface = unix_funcs->create_window_surface(visible_rect);
}

void CDECL WIN32U_WindowPosChanged( HWND hwnd, HWND insert_after, UINT swp_flags,
                                    const RECT *window_rect, const RECT *client_rect,
                                    const RECT *visible_rect, const RECT *valid_rects,
                                    struct window_surface *surface )
{
    struct win32u_window_pos_changed_input in;

    TRACE("hwnd %p, insert_after %p, swp_flags %x, window_rect %s, client_rect %s, visible_rect %s, valid_rects %s, surface %p\n",
          hwnd, insert_after, swp_flags, wine_dbgstr_rect(window_rect), wine_dbgstr_rect(client_rect),
          wine_dbgstr_rect(visible_rect), wine_dbgstr_rect(valid_rects), surface);

    in.hwnd = HandleToUlong(hwnd);
    in.insert_after = HandleToUlong(insert_after);
    in.swp_flags = swp_flags;
    in.window_rect = *window_rect;
    in.client_rect = *client_rect;
    in.visible_rect = *visible_rect;
    in.valid_rects_set = valid_rects != NULL;
    if (valid_rects) in.valid_rects = *valid_rects;

    if (!desktop_ioctl(IOCTL_WIN32U_WINDOW_POS_CHANGED, &in, sizeof(in), NULL, 0, NULL))
        return;
}

UINT CDECL WIN32U_ShowWindow( HWND hwnd, INT cmd, RECT *rect, UINT swp )
{
    struct win32u_show_window_input in;
    struct win32u_show_window_output out;

    TRACE("hwnd %p, cmd %d, rect %s, swp %x\n", hwnd, cmd, wine_dbgstr_rect(rect), swp);

    in.hwnd = HandleToUlong(hwnd);
    in.cmd = cmd;
    in.rect = *rect;
    in.swp = swp;

    if (!desktop_ioctl(IOCTL_WIN32U_SHOW_WINDOW, &in, sizeof(in), &out, sizeof(out), NULL))
        return 0;

    TRACE("returned: swp %x\n", out.swp);

    return out.swp;
}

void CDECL WIN32U_SetWindowStyle( HWND hwnd, INT offset, STYLESTRUCT *style )
{
    struct win32u_set_window_style_input in;

    TRACE("hwnd %p, offset %x, style %p\n", hwnd, offset, style);

    in.hwnd = HandleToUlong(hwnd);
    in.offset = offset;
    in.style = *style;

    if (!desktop_ioctl(IOCTL_WIN32U_SET_WINDOW_STYLE, &in, sizeof(in), NULL, 0, NULL))
        return;
}

void CDECL WIN32U_SetFocus( HWND hwnd )
{
    struct win32u_set_focus_input in;

    TRACE("hwnd %p\n", hwnd);

    in.hwnd = HandleToUlong(hwnd);

    if (!desktop_ioctl(IOCTL_WIN32U_SET_FOCUS, &in, sizeof(in), NULL, 0, NULL))
        return;
}

void CDECL WIN32U_SetParent( HWND hwnd, HWND parent, HWND old_parent )
{
    struct win32u_set_parent_input in;

    TRACE("hwnd %p, parent %p, old_parent %p\n", hwnd, parent, old_parent);

    in.hwnd = HandleToUlong(hwnd);
    in.parent = HandleToUlong(parent);
    in.old_parent = HandleToUlong(old_parent);

    if (!desktop_ioctl(IOCTL_WIN32U_SET_PARENT, &in, sizeof(in), NULL, 0, NULL))
        return;
}

INT CDECL WIN32U_ToUnicodeEx(UINT vkey, UINT scancode, const BYTE *keystate,
                             WCHAR *buf, int buf_size, UINT flags, HKL hkl)
{
    struct win32u_to_unicode_input in;
    DWORD ret;

    TRACE("vkey %x, scancode %x, keystate %p, buf %p, buf_size %d, flags %x, hkl %p\n",
          vkey, scancode, keystate, buf, buf_size, flags, hkl);

    in.vkey = vkey;
    in.scancode = scancode;
    memcpy(in.keystate, keystate, sizeof(in.keystate));
    in.flags = flags;
    in.hkl = HandleToUlong(hkl);

    if (!desktop_ioctl(IOCTL_WIN32U_TO_UNICODE, &in, sizeof(in), buf, buf_size, &ret))
        return 0;

    return ret / sizeof(WCHAR);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    TRACE("instance %p, reason %d, reserved %p\n", instance, reason, reserved);

    switch(reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(instance);
        break;
    case DLL_PROCESS_DETACH:
        break;
    }

    if (__wine_init_unix_lib(instance, reason, NULL, &unix_funcs)) return FALSE;
    return TRUE;
}

DWORD CDECL WIN32U_MsgWaitForMultipleObjectsEx( DWORD count, const HANDLE *handles, DWORD timeout,
                                                DWORD mask, DWORD flags )
{
    return WaitForMultipleObjectsEx( count, handles, flags & MWMO_WAITALL,
                                     timeout, flags & MWMO_ALERTABLE );
}
