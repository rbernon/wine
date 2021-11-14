/*
 * USER driver support
 *
 * Copyright 2000, 2005 Alexandre Julliard
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
#include <stdio.h>
#include <wchar.h>

#include "user_private.h"
#include "winnls.h"
#include "wine/debug.h"
#include "controls.h"

WINE_DEFAULT_DEBUG_CHANNEL(user);
WINE_DECLARE_DEBUG_CHANNEL(winediag);

static struct user_driver_funcs null_driver, lazy_load_driver;

const struct user_driver_funcs *USER_Driver = &lazy_load_driver;
static char driver_load_error[80];

static BOOL CDECL nodrv_CreateWindow( HWND hwnd );

static BOOL load_desktop_driver( HWND hwnd )
{
    BOOL ret = FALSE;
    HKEY hkey;
    DWORD size;
    WCHAR path[MAX_PATH];
    WCHAR key[ARRAY_SIZE(L"System\\CurrentControlSet\\Control\\Video\\{}\\0000") + 40];
    UINT guid_atom;

    USER_CheckNotLock();

    strcpy( driver_load_error, "The explorer process failed to start." );  /* default error */
    wait_graphics_driver_ready();

    guid_atom = HandleToULong( GetPropW( hwnd, L"__wine_display_device_guid" ));
    lstrcpyW( key, L"System\\CurrentControlSet\\Control\\Video\\{" );
    if (!GlobalGetAtomNameW( guid_atom, key + lstrlenW(key), 40 )) return 0;
    lstrcatW( key, L"}\\0000" );
    if (RegOpenKeyW( HKEY_LOCAL_MACHINE, key, &hkey )) return 0;
    size = sizeof(path);
    if (!RegQueryValueExW( hkey, L"GraphicsDriver", NULL, NULL, (BYTE *)path, &size ))
    {
        if (wcscmp( path, L"null" ))
        {
            ret = LoadLibraryW( path ) != NULL;
            if (!ret) ERR( "failed to load %s\n", debugstr_w(path) );
        }
        else
        {
            __wine_set_user_driver( &null_driver, WINE_GDI_DRIVER_VERSION );
            ret = TRUE;
        }
        TRACE( "%s\n", debugstr_w(path) );
    }
    else
    {
        size = sizeof(driver_load_error);
        RegQueryValueExA( hkey, "DriverError", NULL, NULL, (BYTE *)driver_load_error, &size );
    }
    RegCloseKey( hkey );
    return ret;
}

/* load the graphics driver */
static const struct user_driver_funcs *load_driver(void)
{
    struct user_driver_funcs driver;
    USEROBJECTFLAGS flags;
    HWINSTA winstation;

    if (!load_desktop_driver( GetDesktopWindow() ) || USER_Driver == &lazy_load_driver)
    {
        memset( &driver, 0, sizeof(driver) );
        winstation = NtUserGetProcessWindowStation();
        if (!NtUserGetObjectInformation( winstation, UOI_FLAGS, &flags, sizeof(flags), NULL )
            || (flags.dwFlags & WSF_VISIBLE))
            driver.pCreateWindow = nodrv_CreateWindow;

        __wine_set_user_driver( &driver, WINE_GDI_DRIVER_VERSION );
    }

    return USER_Driver;
}

/* unload the graphics driver on process exit */
void USER_unload_driver(void)
{
    struct user_driver_funcs *prev;
    __wine_set_display_driver( &null_driver, WINE_GDI_DRIVER_VERSION );
    /* make sure we don't try to call the driver after it has been detached */
    prev = InterlockedExchangePointer( (void **)&USER_Driver, &null_driver );
    if (prev != &lazy_load_driver && prev != &null_driver)
        HeapFree( GetProcessHeap(), 0, prev );
}


/**********************************************************************
 * Null user driver
 *
 * These are fallbacks for entry points that are not implemented in the real driver.
 */

static void CDECL nulldrv_DestroyCursorIcon( HCURSOR cursor )
{
}

static BOOL CDECL nodrv_CreateWindow( HWND hwnd )
{
    static int warned;
    HWND parent = GetAncestor( hwnd, GA_PARENT );

    /* HWND_MESSAGE windows don't need a graphics driver */
    if (!parent || parent == get_user_thread_info()->msg_window) return TRUE;
    if (warned++) return FALSE;

    ERR_(winediag)( "Application tried to create a window, but no driver could be loaded.\n" );
    if (driver_load_error[0]) ERR_(winediag)( "%s\n", driver_load_error );
    return FALSE;
}

static void CDECL nulldrv_DestroyWindow( HWND hwnd )
{
}

static DWORD CDECL nulldrv_MsgWaitForMultipleObjectsEx( DWORD count, const HANDLE *handles, DWORD timeout,
                                                        DWORD mask, DWORD flags )
{
    if (!count && !timeout) return WAIT_TIMEOUT;
    return WaitForMultipleObjectsEx( count, handles, flags & MWMO_WAITALL,
                                     timeout, flags & MWMO_ALERTABLE );
}

static void CDECL nulldrv_ReleaseDC( HWND hwnd, HDC hdc )
{
}

static void CDECL nulldrv_SetActiveWindow( HWND hwnd )
{
}

static void CDECL nulldrv_SetCapture( HWND hwnd, UINT flags )
{
}

static void CDECL nulldrv_SetFocus( HWND hwnd )
{
}

static BOOL CDECL nulldrv_SetForegroundWindow( HWND hwnd )
{
    return TRUE;
}

static void CDECL nulldrv_SetParent( HWND hwnd, HWND parent, HWND old_parent )
{
}

static void CDECL nulldrv_SetWindowIcon( HWND hwnd, UINT type, HICON icon )
{
}

static void CDECL nulldrv_SetWindowStyle( HWND hwnd, INT offset, STYLESTRUCT *style )
{
}

static void CDECL nulldrv_SetWindowText( HWND hwnd, LPCWSTR text )
{
}

static UINT CDECL nulldrv_ShowWindow( HWND hwnd, INT cmd, RECT *rect, UINT swp )
{
    return ~0; /* use default implementation */
}

static LRESULT CDECL nulldrv_SysCommand( HWND hwnd, WPARAM wparam, LPARAM lparam )
{
    return -1;
}

static LRESULT CDECL nulldrv_WindowMessage( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    return 0;
}

static BOOL CDECL nulldrv_WindowPosChanging( HWND hwnd, HWND insert_after, UINT swp_flags,
                                             const RECT *window_rect, const RECT *client_rect,
                                             RECT *visible_rect, struct window_surface **surface )
{
    return FALSE;
}

static void CDECL nulldrv_WindowPosChanged( HWND hwnd, HWND insert_after, UINT swp_flags,
                                            const RECT *window_rect, const RECT *client_rect,
                                            const RECT *visible_rect, const RECT *valid_rects,
                                            struct window_surface *surface )
{
}

static BOOL CDECL nulldrv_SystemParametersInfo( UINT action, UINT int_param, void *ptr_param, UINT flags )
{
    return FALSE;
}

static void CDECL nulldrv_ThreadDetach( void )
{
}


/**********************************************************************
 * Lazy loading user driver
 *
 * Initial driver used before another driver is loaded.
 * Each entry point simply loads the real driver and chains to it.
 */

static void CDECL loaderdrv_Beep(void)
{
    load_driver()->pBeep();
}

static BOOL CDECL loaderdrv_RegisterHotKey( HWND hwnd, UINT modifiers, UINT vk )
{
    return load_driver()->pRegisterHotKey( hwnd, modifiers, vk );
}

static void CDECL loaderdrv_UnregisterHotKey( HWND hwnd, UINT modifiers, UINT vk )
{
    load_driver()->pUnregisterHotKey( hwnd, modifiers, vk );
}

static void CDECL loaderdrv_SetCursor( HCURSOR cursor )
{
    load_driver()->pSetCursor( cursor );
}

static BOOL CDECL loaderdrv_GetCursorPos( LPPOINT pt )
{
    return load_driver()->pGetCursorPos( pt );
}

static BOOL CDECL loaderdrv_SetCursorPos( INT x, INT y )
{
    return load_driver()->pSetCursorPos( x, y );
}

static BOOL CDECL loaderdrv_ClipCursor( LPCRECT clip )
{
    return load_driver()->pClipCursor( clip );
}

static void CDECL loaderdrv_UpdateClipboard(void)
{
    load_driver()->pUpdateClipboard();
}

static LONG CDECL loaderdrv_ChangeDisplaySettingsEx( LPCWSTR name, LPDEVMODEW mode, HWND hwnd,
                                                     DWORD flags, LPVOID lparam )
{
    return load_driver()->pChangeDisplaySettingsEx( name, mode, hwnd, flags, lparam );
}

static INT CDECL loaderdrv_EnumDisplayMonitors( HDC hdc, LPRECT rect, MONITORENUMPROC proc, LPARAM lp )
{
    return load_driver()->pEnumDisplayMonitors( hdc, rect, proc, lp );
}

static INT CDECL loaderdrv_EnumDisplaySettingsEx( const WCHAR *name, DWORD num, DEVMODEW *mode, DWORD flags )
{
    return load_driver()->pEnumDisplaySettingsEx( name, num, mode, flags );
}

static INT CDECL loaderdrv_GetMonitorInfo( HMONITOR handle, LPMONITORINFO info )
{
    return load_driver()->pGetMonitorInfo( handle, info );
}

static BOOL CDECL loaderdrv_CreateDesktopWindow( HWND hwnd )
{
    return load_driver()->pCreateDesktopWindow( hwnd );
}

static BOOL CDECL loaderdrv_CreateWindow( HWND hwnd )
{
    return load_driver()->pCreateWindow( hwnd );
}

static void CDECL loaderdrv_FlashWindowEx( FLASHWINFO *info )
{
    load_driver()->pFlashWindowEx( info );
}

static void CDECL loaderdrv_GetDC( HDC hdc, HWND hwnd, HWND top_win, const RECT *win_rect,
                                   const RECT *top_rect, DWORD flags )
{
    load_driver()->pGetDC( hdc, hwnd, top_win, win_rect, top_rect, flags );
}

static void CDECL loaderdrv_SetLayeredWindowAttributes( HWND hwnd, COLORREF key, BYTE alpha, DWORD flags )
{
    load_driver()->pSetLayeredWindowAttributes( hwnd, key, alpha, flags );
}

static void CDECL loaderdrv_SetWindowRgn( HWND hwnd, HRGN hrgn, BOOL redraw )
{
    load_driver()->pSetWindowRgn( hwnd, hrgn, redraw );
}

static BOOL CDECL loaderdrv_UpdateLayeredWindow( HWND hwnd, const UPDATELAYEREDWINDOWINFO *info,
                                                 const RECT *window_rect )
{
    return load_driver()->pUpdateLayeredWindow( hwnd, info, window_rect );
}

static struct user_driver_funcs lazy_load_driver =
{
    { NULL },
    /* keyboard functions */
    NULL,
    loaderdrv_Beep,
    NULL,
    NULL,
    NULL,
    loaderdrv_RegisterHotKey,
    NULL,
    loaderdrv_UnregisterHotKey,
    NULL,
    /* cursor/icon functions */
    nulldrv_DestroyCursorIcon,
    loaderdrv_SetCursor,
    loaderdrv_GetCursorPos,
    loaderdrv_SetCursorPos,
    loaderdrv_ClipCursor,
    /* clipboard functions */
    loaderdrv_UpdateClipboard,
    /* display modes */
    loaderdrv_ChangeDisplaySettingsEx,
    loaderdrv_EnumDisplayMonitors,
    loaderdrv_EnumDisplaySettingsEx,
    loaderdrv_GetMonitorInfo,
    /* windowing functions */
    loaderdrv_CreateDesktopWindow,
    loaderdrv_CreateWindow,
    nulldrv_DestroyWindow,
    loaderdrv_FlashWindowEx,
    loaderdrv_GetDC,
    nulldrv_MsgWaitForMultipleObjectsEx,
    nulldrv_ReleaseDC,
    NULL,
    nulldrv_SetActiveWindow,
    nulldrv_SetCapture,
    nulldrv_SetFocus,
    nulldrv_SetForegroundWindow,
    loaderdrv_SetLayeredWindowAttributes,
    nulldrv_SetParent,
    loaderdrv_SetWindowRgn,
    nulldrv_SetWindowIcon,
    nulldrv_SetWindowStyle,
    nulldrv_SetWindowText,
    nulldrv_ShowWindow,
    nulldrv_SysCommand,
    loaderdrv_UpdateLayeredWindow,
    nulldrv_WindowMessage,
    nulldrv_WindowPosChanging,
    nulldrv_WindowPosChanged,
    /* system parameters */
    nulldrv_SystemParametersInfo,
    /* thread management */
    nulldrv_ThreadDetach
};

void CDECL __wine_set_user_driver( const struct user_driver_funcs *funcs, UINT version )
{
    struct user_driver_funcs *driver, *prev;

    if (version != WINE_GDI_DRIVER_VERSION)
    {
        ERR( "version mismatch, driver wants %u but user32 has %u\n", version, WINE_GDI_DRIVER_VERSION );
        return;
    }

    driver = HeapAlloc( GetProcessHeap(), 0, sizeof(*driver) );
    *driver = *funcs;

    prev = InterlockedCompareExchangePointer( (void **)&USER_Driver, driver, &lazy_load_driver );
    if (prev != &lazy_load_driver)
    {
        /* another thread beat us to it */
        HeapFree( GetProcessHeap(), 0, driver );
        driver = prev;
    }

    __wine_set_display_driver( driver, version );
}
