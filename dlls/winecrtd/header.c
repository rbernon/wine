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
/* avoid NtGetTickCount import in ntdll.dll */
#define _NTSYSTEM_

#include <stdarg.h>
#include <stddef.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"

#include "wine/debug.h"

WINE_DECLARE_DEBUG_CHANNEL(pid);
WINE_DECLARE_DEBUG_CHANNEL(timestamp);

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
        UINT ticks = NtGetTickCount();
        pos += snprintf( pos, sizeof(info->output) - (pos - info->output), "%3u.%03u:", ticks / 1000, ticks % 1000 );
    }
    if (TRACE_ON(pid)) pos += snprintf( pos, sizeof(info->output) - (pos - info->output), "%04x:", (UINT)GetCurrentProcessId() );
    pos += snprintf( pos, sizeof(info->output) - (pos - info->output), "%04x:", (UINT)GetCurrentThreadId() );
    if (function && cls < ARRAY_SIZE( classes ))
        pos += snprintf( pos, sizeof(info->output) - (pos - info->output), "%s:%s:%s ",
                         classes[cls], channel->name, function );
    info->out_pos = pos - info->output;
    return info->out_pos;
}
