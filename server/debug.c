/*
 * Server-side debugging functions
 *
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

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>

#include "windef.h"
#include "winbase.h"
#include "wine/debug.h"

#include "process.h"
#include "request.h"
#include "file.h"

WINE_DECLARE_DEBUG_CHANNEL(pid);
WINE_DECLARE_DEBUG_CHANNEL(timestamp);

static struct debug_info debug_info;
static const int max_debug_options = 2048; /* see ntdll/unix/debug.c */

/* winecrtd/options.c */
/* parse a set of debugging option specifications and add them to the option list */
extern int __wine_dbg_parse_options( struct __wine_debug_channel *options, int max_options,
                                     const char *wine_debug, const char *app_name );

int __cdecl __wine_dbg_init( struct __wine_debug_channel **options )
{
    if (!(*options = mem_alloc( max_debug_options * sizeof(**options) ))) return 0;
    return __wine_dbg_parse_options( *options, max_debug_options, getenv( "WINEDEBUG" ), "wineserver" );
}

struct debug_info *__cdecl __wine_dbg_get_info(void)
{
    return &debug_info;
}

int WINAPI __wine_dbg_write( const char *str, unsigned int len )
{
    return write( 2, str, len );
}

int __cdecl __wine_dbg_header( enum __wine_debug_class cls, struct __wine_debug_channel *channel, const char *function )
{
    static const char *const classes[] = {"fixme", "err", "warn", "trace"};
    struct debug_info *info = __wine_dbg_get_info();
    char *pos = info->output;

    if (!(__wine_dbg_get_channel_flags( channel ) & (1 << cls))) return -1;

    /* only print header if we are at the beginning of the line */
    if (info->out_pos) return 0;

    if (TRACE_ON(timestamp))
    {
        unsigned int ticks = monotonic_time / 10000;
        pos += snprintf( pos, sizeof(info->output) - (pos - info->output), "%3u.%03u:", ticks / 1000, ticks % 1000 );
    }
    if (TRACE_ON(pid)) pos += snprintf( pos, sizeof(info->output) - (pos - info->output), "%04x:", current ? current->process->id : 0 );
    pos += snprintf( pos, sizeof(info->output) - (pos - info->output), "%04x:", current ? current->id : 0 );
    if (function && cls < ARRAY_SIZE( classes ))
        pos += snprintf( pos, sizeof(info->output) - (pos - info->output), "%s:%s:%s ",
                         classes[cls], channel->name, function );
    info->out_pos = pos - info->output;
    return info->out_pos;
}
