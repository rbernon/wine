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

/* pull strlen/strrchr from CRT or ntdll */
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

/* add a string to the output buffer */
static int append_output( struct debug_info *info, const char *str, size_t len )
{
    if (len >= sizeof(info->output) - info->out_pos)
    {
        static const char overflow_msg[] = "wine: debug buffer overflow:\n";
        __wine_dbg_write( info->output, info->out_pos );
        info->out_pos = 0;
        __wine_dbg_write( overflow_msg, sizeof(overflow_msg) - 1 );
        __wine_dbg_write( str, len );
#ifdef WINE_UNIX_LIB
        abort();
#else
        RtlRaiseStatus( STATUS_BUFFER_OVERFLOW );
#endif
    }
    memcpy( info->output + info->out_pos, str, len );
    info->out_pos += len;
    return len;
}

int __cdecl __wine_dbg_output( const char *str )
{
    struct debug_info *info = __wine_dbg_get_info();
    const char *end = strrchr( str, '\n' );
    int ret = 0;

    if (end)
    {
        ret += append_output( info, str, end + 1 - str );
        __wine_dbg_write( info->output, info->out_pos );
        info->out_pos = 0;
        str = end + 1;
    }
    if (*str) ret += append_output( info, str, strlen( str ) );
    return ret;
}
