/*
 * Copyright 2024 Rémi Bernon for CodeWeavers
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

#ifdef ENABLE_MESA

#include <stdarg.h>
#include <stddef.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "corecrt_startup.h"

#include "wine/glu.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(opengl);

extern BOOL WINAPI mesa_dllmain( HINSTANCE hinst, DWORD reason, LPVOID reserved );
extern void (*__glapi_noop_table[])(void);

BOOL WINAPI DllMain( HINSTANCE hinst, DWORD reason, LPVOID reserved )
{
   NtCurrentTeb()->glTable = __glapi_noop_table;
   return mesa_dllmain( hinst, reason, reserved );
}

GLint WINAPI glDebugEntry( GLint unknown1, GLint unknown2 )
{
    FIXME( "sub!\n" );
    return 0;
}

PROC WINAPI wglGetDefaultProcAddress( const char *name )
{
    FIXME( "sub!\n" );
    return NULL;
}

#endif /* ENABLE_MESA */
