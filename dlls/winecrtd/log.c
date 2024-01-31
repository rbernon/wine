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

#include <stdarg.h>
#include <stddef.h>

#include "windef.h"
#include "winbase.h"

#include "wine/debug.h"

int __wine_dbg_cdecl wine_dbg_vlog( enum __wine_debug_class cls, struct __wine_debug_channel *channel,
                                    const char *function, const char *format, va_list args )
{
    int ret;

    if (*format == '\1')  /* special magic to avoid standard prefix */
    {
        format++;
        function = NULL;
    }
    if ((ret = __wine_dbg_header( cls, channel, function )) != -1)
        ret += wine_dbg_vprintf( format, args );
    return ret;
}

int __wine_dbg_cdecl wine_dbg_log( enum __wine_debug_class cls, struct __wine_debug_channel *channel,
                                   const char *function, const char *format, ... )
{
    va_list args;
    int ret;

    va_start( args, format );
    ret = wine_dbg_vlog( cls, channel, function, format, args );
    va_end( args );
    return ret;
}
