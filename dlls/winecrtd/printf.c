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

/* pull _vsnprintf from CRT or ntdll */
#define _CRTIMP

#include <stdarg.h>
#include <stddef.h>

#include "windef.h"
#include "winbase.h"

#include "wine/debug.h"

int __wine_dbg_cdecl wine_dbg_vprintf( const char *format, va_list args )
{
    char buffer[1024];

    vsnprintf( buffer, sizeof(buffer), format, args );
    return __wine_dbg_output( buffer );
}

int __wine_dbg_cdecl wine_dbg_printf( const char *format, ... )
{
    int ret;
    va_list args;

    va_start( args, format );
    ret = wine_dbg_vprintf( format, args );
    va_end( args );
    return ret;
}
