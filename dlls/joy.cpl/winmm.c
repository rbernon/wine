/*
 * Copyright 2022 Rémi Bernon for CodeWeavers.
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
 *
 */

#include "stdarg.h"
#include "stddef.h"

#include "windef.h"
#include "winbase.h"

#include "wine/debug.h"
#include "wine/list.h"

#include "joy_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(joycpl);

static DWORD WINAPI input_thread_proc( void *param )
{
    HANDLE stop_event = param;

    while (WaitForSingleObject( stop_event, 20 ) == WAIT_TIMEOUT)
    {
    }

    return 0;
}

extern INT_PTR CALLBACK test_wmm_dialog_proc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    static HANDLE input_thread, input_thread_stop;

    TRACE( "hwnd %p, msg %#x, wparam %#Ix, lparam %#Ix\n", hwnd, msg, wparam, lparam );

    switch (msg)
    {
    case WM_INITDIALOG:
        return TRUE;

    case WM_COMMAND:
        return TRUE;

    case WM_NOTIFY:
        switch (((NMHDR *)lparam)->code)
        {
        case PSN_SETACTIVE:
            input_thread_stop = CreateEventW( NULL, FALSE, FALSE, NULL );
            input_thread = CreateThread( NULL, 0, input_thread_proc, (void *)input_thread_stop, 0, NULL );
            break;

        case PSN_RESET:
        case PSN_KILLACTIVE:
            SetEvent( input_thread_stop );
            MsgWaitForMultipleObjects( 1, &input_thread, FALSE, INFINITE, 0 );
            CloseHandle( input_thread_stop );
            CloseHandle( input_thread );
            break;
        }
        return TRUE;

    case WM_USER:
        return TRUE;
    }

    return FALSE;
}
