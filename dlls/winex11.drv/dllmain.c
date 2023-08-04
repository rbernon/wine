/*
 * winex11.drv entry points
 *
 * Copyright 2022 Jacek Caban for CodeWeavers
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
#include "ntgdi.h"
#include "unixlib.h"
#include "wine/debug.h"
#include "dbt.h"

#include "initguid.h"
#include "ddk/hidclass.h"

#include "wine/debug.h"

HMODULE x11drv_module = 0;

WINE_DEFAULT_DEBUG_CHANNEL(rawinput);

static DWORD CALLBACK rawinput_thread( void *arg )
{
    DEV_BROADCAST_DEVICEINTERFACE_W filter =
    {
        .dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE_W),
        .dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE,
        .dbcc_classguid = GUID_DEVINTERFACE_HID,
    };
    HWND hwnd;

    TRACE( "Starting rawinput thread\n" );

    SetThreadDescription( GetCurrentThread(), L"wine_winex11_rawinput" );

    /* wait for the desktop thread to fully initialize before creating our message queue */
    SendMessageW( GetDesktopWindow(), WM_NULL, 0, 0 );

    ImmDisableIME( 0 );
    hwnd = CreateWindowW( L"Message", NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL );
    NtUserDestroyInputContext( ImmGetContext( hwnd ) );

    if (!RegisterDeviceNotificationW( hwnd, &filter, DEVICE_NOTIFY_WINDOW_HANDLE ))
        WARN( "Failed to register for rawinput devices notifications\n" );

    return NtUserCallHwndParam( hwnd, 0, NtUserCallHwndParam_RawInputThread );
}

BOOL WINAPI DllMain( HINSTANCE instance, DWORD reason, void *reserved )
{
    if (reason != DLL_PROCESS_ATTACH) return TRUE;

    DisableThreadLibraryCalls( instance );
    if (__wine_init_unix_call()) return FALSE;
    if (X11DRV_CALL( init, NULL )) return FALSE;

    CloseHandle( CreateThread( NULL, 0, rawinput_thread, NULL, 0, NULL ) );
    return TRUE;
}

/***********************************************************************
 *           AttachEventQueueToTablet (winex11.@)
 */
int CDECL X11DRV_AttachEventQueueToTablet( HWND owner )
{
    return X11DRV_CALL( tablet_attach_queue, owner );
}

/***********************************************************************
 *           GetCurrentPacket (winex11.@)
 */
int CDECL X11DRV_GetCurrentPacket( void *packet )
{
    return X11DRV_CALL( tablet_get_packet, packet );
}

/***********************************************************************
 *           LoadTabletInfo (winex11.@)
 */
BOOL CDECL X11DRV_LoadTabletInfo( HWND hwnd )
{
    return X11DRV_CALL( tablet_load_info, hwnd );
}

/***********************************************************************
 *          WTInfoW (winex11.@)
 */
UINT CDECL X11DRV_WTInfoW( UINT category, UINT index, void *output )
{
    struct tablet_info_params params;
    params.category = category;
    params.index = index;
    params.output = output;
    return X11DRV_CALL( tablet_info, &params );
}
