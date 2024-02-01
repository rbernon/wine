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

/* pull strcmp from CRT or ntdll */
#define _CRTIMP

#include <stdarg.h>
#include <stddef.h>

#include "windef.h"
#include "winbase.h"

#include "wine/debug.h"

unsigned char __cdecl __wine_dbg_get_channel_flags( struct __wine_debug_channel *channel )
{
    static struct __wine_debug_channel *debug_options;
    static int nb_debug_options = -1;

    int min, max, pos, res, count;
    unsigned char flags;

    if (!(channel->flags & (1 << __WINE_DBCL_INIT))) return channel->flags;

    if (nb_debug_options < 0) nb_debug_options = __wine_dbg_init( &debug_options );
    if (!nb_debug_options) return (1 << __WINE_DBCL_ERR) | (1 << __WINE_DBCL_FIXME);
    count = nb_debug_options < 0 ? -nb_debug_options : nb_debug_options;

    flags = debug_options[count - 1].flags;
    min = 0;
    max = count - 2;
    while (min <= max)
    {
        pos = (min + max) / 2;
        res = strcmp( channel->name, debug_options[pos].name );
        if (!res)
        {
            flags = debug_options[pos].flags;
            break;
        }
        if (res < 0) max = pos - 1;
        else min = pos + 1;
    }

    if (!(flags & (1 << __WINE_DBCL_INIT))) channel->flags = flags; /* not dynamically changeable */
    return flags;
}
