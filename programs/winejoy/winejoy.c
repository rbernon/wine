/*
 * Copyright 2022 Rémi Bernon for CodeWeavers
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

#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

#include "windef.h"
#include "winbase.h"
#include "winuser.h"

#include "cpl.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(joycpl);

extern LONG CALLBACK CPlApplet( HWND hwnd, UINT command, LPARAM lparam1, LPARAM lparam2 );

int WINAPI wWinMain( HINSTANCE instance, HINSTANCE previous, WCHAR *cmdline, int cmdshow )
{
    CPLINFO info;
    MSG msg;

    TRACE( "instance %p, previous %p, cmdline %s, cmdshow %d.\n", instance, previous, debugstr_w(cmdline), cmdshow );

    CPlApplet( GetDesktopWindow(), CPL_INIT, 0, 0 );
    CPlApplet( GetDesktopWindow(), CPL_INQUIRE, 0, (LPARAM)&info );
    CPlApplet( GetDesktopWindow(), CPL_DBLCLK, 0, info.lData );

    while (GetMessageW( &msg, 0, 0, 0 ))
    {
        TranslateMessage( &msg );
        DispatchMessageW( &msg );
    }

    return 0;
}
