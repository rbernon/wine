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
/* avoid RtlRaiseStatus import in ntdll.dll */
#define _NTSYSTEM_

#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"

#include "wine/debug.h"

const char * __cdecl __wine_dbg_strdup( const char *str )
{
    struct debug_info *info = __wine_dbg_get_info();
    unsigned int pos = info->str_pos;
    size_t n = strlen( str ) + 1;

#ifdef WINE_UNIX_LIB
    if (n > sizeof(info->strings)) abort();
#else
    if (n > sizeof(info->strings)) RtlRaiseStatus( STATUS_BUFFER_OVERFLOW );
#endif

    if (pos + n > sizeof(info->strings)) pos = 0;
    info->str_pos = pos + n;
    return memcpy( info->strings + pos, str, n );
}

const char * __wine_dbg_cdecl wine_dbg_vsprintf( const char *format, va_list args )
{
    char buffer[200];

    vsnprintf( buffer, sizeof(buffer), format, args );
    return __wine_dbg_strdup( buffer );
}

const char * __wine_dbg_cdecl wine_dbg_sprintf( const char *format, ... )
{
    const char *ret;
    va_list args;

    va_start( args, format );
    ret = wine_dbg_vsprintf( format, args );
    va_end( args );
    return ret;
}
