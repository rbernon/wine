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

WINE_DEFAULT_DEBUG_CHANNEL(win32u);

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

    return TRUE;
}

static BOOL (CDECL *fallback_CreateWindow)(HWND hwnd);

static void CDECL win32u_surface_lock( struct window_surface *surface )
{
    FIXME("stub!\n");
}
static void CDECL win32u_surface_unlock( struct window_surface *surface )
{
    FIXME("stub!\n");
}
static void *CDECL win32u_surface_get_info( struct window_surface *surface, BITMAPINFO *info )
{
    static DWORD dummy_data;

    FIXME("stub!\n");

    info->bmiHeader.biSize          = sizeof( info->bmiHeader );
    info->bmiHeader.biWidth         = surface->rect.right;
    info->bmiHeader.biHeight        = surface->rect.bottom;
    info->bmiHeader.biPlanes        = 1;
    info->bmiHeader.biBitCount      = 32;
    info->bmiHeader.biCompression   = BI_RGB;
    info->bmiHeader.biSizeImage     = 0;
    info->bmiHeader.biXPelsPerMeter = 0;
    info->bmiHeader.biYPelsPerMeter = 0;
    info->bmiHeader.biClrUsed       = 0;
    info->bmiHeader.biClrImportant  = 0;

    return &dummy_data;
}
static RECT *CDECL win32u_surface_get_bounds( struct window_surface *surface )
{
    static RECT dummy_bounds;
    FIXME("stub!\n");
    return &dummy_bounds;
}
static void CDECL win32u_surface_set_region( struct window_surface *surface, HRGN region )
{
    FIXME("stub!\n");
}
static void CDECL win32u_surface_flush( struct window_surface *surface )
{
    FIXME("stub!\n");
}
static void CDECL win32u_surface_destroy( struct window_surface *surface )
{
    FIXME("stub!\n");
}

static const struct window_surface_funcs win32u_surface_funcs =
{
    win32u_surface_lock,
    win32u_surface_unlock,
    win32u_surface_get_info,
    win32u_surface_get_bounds,
    win32u_surface_set_region,
    win32u_surface_flush,
    win32u_surface_destroy
};

struct win32u_surface
{
    struct window_surface surface;
    UINT64 desktop_surface;
};

static struct window_surface *win32u_create_surface( UINT64 desktop_surface )
{
    struct win32u_surface *surface = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(struct win32u_surface) );
    if (!surface) return NULL;

    surface->surface.funcs = &win32u_surface_funcs;
    surface->desktop_surface = desktop_surface;
    return &surface->surface;
}

static UINT64 win32u_get_desktop_surface( struct window_surface *surface )
{
    if (!surface) return 0;
    if (surface->funcs != &win32u_surface_funcs) return WIN32U_DUMMY_DESKTOP_SURFACE;
    return ((struct win32u_surface *)surface)->desktop_surface;
}

BOOL CDECL __wine_set_fallback_driver(HMODULE module)
{
    TRACE("module %p\n", module);

    if (!(fallback_CreateWindow = (void *)GetProcAddress(module, "CreateWindow")))
    {
        WARN("unable to load fallback for CreateWindow.\n");
        return FALSE;
    }

    return TRUE;
}

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
        ERR("%08x ioctl failed: invalid parameter!\n", code);
        break;
    default:
        ERR("%08x ioctl failed: %08x!\n", code, status);
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

    TRACE("hwnd %p, insert_after %p, swp_flags %x, window_rect %s, client_rect %s, visible_rect %p, surface %p (%I64x)\n",
          hwnd, insert_after, swp_flags, wine_dbgstr_rect(window_rect), wine_dbgstr_rect(client_rect),
          visible_rect, surface, win32u_get_desktop_surface(*surface));

    in.hwnd = HandleToUlong(hwnd);
    in.insert_after = HandleToUlong(insert_after);
    in.swp_flags = swp_flags;
    in.window_rect = *window_rect;
    in.client_rect = *client_rect;
    in.desktop_surface = win32u_get_desktop_surface(*surface);

    memset(&out, 0, sizeof(out));

    if (!desktop_ioctl(IOCTL_WIN32U_WINDOW_POS_CHANGING, &in, sizeof(in), &out, sizeof(out), NULL))
        return;

    TRACE("returned: visible_rect %s, surface %I64x\n", wine_dbgstr_rect(&out.visible_rect), out.desktop_surface);

    *visible_rect = out.visible_rect;
    if (out.desktop_surface == in.desktop_surface) return;
    if (*surface) window_surface_release(*surface);
    *surface = NULL;
    // if (!out.desktop_surface)
    // else *surface = win32u_create_surface( out.desktop_surface );
}

void CDECL WIN32U_WindowPosChanged( HWND hwnd, HWND insert_after, UINT swp_flags,
                                    const RECT *window_rect, const RECT *client_rect,
                                    const RECT *visible_rect, const RECT *valid_rects,
                                    struct window_surface *surface )
{
    struct win32u_window_pos_changed_input in;

    TRACE("hwnd %p, insert_after %p, swp_flags %x, window_rect %s, client_rect %s, visible_rect %s, valid_rects %s, surface %p (%I64x)\n",
          hwnd, insert_after, swp_flags, wine_dbgstr_rect(window_rect), wine_dbgstr_rect(client_rect),
          wine_dbgstr_rect(visible_rect), wine_dbgstr_rect(valid_rects), surface, win32u_get_desktop_surface(surface));

    in.hwnd = HandleToUlong(hwnd);
    in.insert_after = HandleToUlong(insert_after);
    in.swp_flags = swp_flags;
    in.window_rect = *window_rect;
    in.client_rect = *client_rect;
    in.visible_rect = *visible_rect;
    in.valid_rects_set = valid_rects != NULL;
    if (valid_rects) in.valid_rects = *valid_rects;
    in.desktop_surface = win32u_get_desktop_surface(surface);

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
