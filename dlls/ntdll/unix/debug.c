/*
 * Debugging functions
 *
 * Copyright 2000 Alexandre Julliard
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

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <assert.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winnt.h"
#include "winternl.h"
#include "unix_private.h"
#include "wine/debug.h"

WINE_DECLARE_DEBUG_CHANNEL(pid);
WINE_DECLARE_DEBUG_CHANNEL(timestamp);
WINE_DEFAULT_DEBUG_CHANNEL(ntdll);

struct debug_info
{
    unsigned int str_pos;       /* current position in strings buffer */
    unsigned int out_pos;       /* current position in output buffer */
    char         strings[1020]; /* buffer for temporary strings */
    char         output[1020];  /* current output line */
};

C_ASSERT( sizeof(struct debug_info) == 0x800 );

static BOOL init_done;
static struct debug_info initial_info;  /* debug info for initial thread */
static LONG nb_debug_options = -1;
static struct __wine_debug_channel *debug_options;

/* get the debug info pointer for the current thread */
struct debug_info *__cdecl __wine_dbg_get_info(void)
{
    if (!init_done) return &initial_info;
#ifdef _WIN64
    return (struct debug_info *)((TEB32 *)((char *)NtCurrentTeb() + teb_offset) + 1);
#else
    return (struct debug_info *)(NtCurrentTeb() + 1);
#endif
}

/* add a string to the output buffer */
static int append_output( struct debug_info *info, const char *str, size_t len )
{
    if (len >= sizeof(info->output) - info->out_pos)
    {
       fprintf( stderr, "wine_dbg_output: debugstr buffer overflow (contents: '%s')\n", info->output );
       info->out_pos = 0;
       abort();
    }
    memcpy( info->output + info->out_pos, str, len );
    info->out_pos += len;
    return len;
}

/* parse a set of debugging option specifications and add them to the option list */
struct __wine_debug_channel *__wine_dbg_parse_options( const char *winedebug, LONG *option_count ) DECLSPEC_HIDDEN;;

/* print the usage message */
static void debug_usage(void)
{
    static const char usage[] =
        "Syntax of the WINEDEBUG variable:\n"
        "  WINEDEBUG=[class]+xxx,[class]-yyy,...\n\n"
        "Example: WINEDEBUG=+relay,warn-heap\n"
        "    turns on relay traces, disable heap warnings\n"
        "Available message classes: err, warn, fixme, trace\n";
    write( 2, usage, sizeof(usage) - 1 );
    exit(1);
}

/* initialize all options at startup */
static void init_options(void)
{
    const char *wine_debug = getenv("WINEDEBUG");
    struct stat st1, st2;

    /* check for stderr pointing to /dev/null */
    if (!fstat( 2, &st1 ) && S_ISCHR(st1.st_mode) &&
        !stat( "/dev/null", &st2 ) && S_ISCHR(st2.st_mode) &&
        st1.st_rdev == st2.st_rdev) wine_debug = "-all";

    if (wine_debug && !strcmp( wine_debug, "help" )) debug_usage();
    debug_options = __wine_dbg_parse_options( wine_debug, &nb_debug_options );
}

/***********************************************************************
 *		__wine_dbg_get_channel_flags  (NTDLL.@)
 *
 * Get the flags to use for a given channel, possibly setting them too in case of lazy init
 */
unsigned char __cdecl __wine_dbg_get_channel_flags( struct __wine_debug_channel *channel )
{
    unsigned char default_flags;
    int min, max, pos, res;

    if (nb_debug_options == -1) init_options();

    min = 0;
    max = nb_debug_options - 1;
    while (min <= max)
    {
        pos = (min + max) / 2;
        res = strcmp( channel->name, debug_options[pos].name );
        if (!res) return debug_options[pos].flags;
        if (res < 0) max = pos - 1;
        else min = pos + 1;
    }
    /* no option for this channel */
    default_flags = debug_options[nb_debug_options].flags;
    if (channel->flags & (1 << __WINE_DBCL_INIT)) channel->flags = default_flags;
    return default_flags;
}

/***********************************************************************
 *		__wine_dbg_write  (NTDLL.@)
 */
int WINAPI __wine_dbg_write( const char *str, unsigned int len )
{
    return write( 2, str, len );
}

/***********************************************************************
 *		__wine_dbg_output  (NTDLL.@)
 */
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
    if (*str) ret += append_output( info, str, strlen( str ));
    return ret;
}

/***********************************************************************
 *		__wine_dbg_header  (NTDLL.@)
 */
int __cdecl __wine_dbg_header( enum __wine_debug_class cls, struct __wine_debug_channel *channel,
                               const char *function )
{
    static const char * const classes[] = { "fixme", "err", "warn", "trace" };
    struct debug_info *info = __wine_dbg_get_info();
    char *pos = info->output;

    if (!(__wine_dbg_get_channel_flags( channel ) & (1 << cls))) return -1;

    /* only print header if we are at the beginning of the line */
    if (info->out_pos) return 0;

    if (init_done)
    {
        if (TRACE_ON(timestamp))
        {
            UINT ticks = NtGetTickCount();
            pos += sprintf( pos, "%3u.%03u:", ticks / 1000, ticks % 1000 );
        }
        if (TRACE_ON(pid)) pos += sprintf( pos, "%04x:", (UINT)GetCurrentProcessId() );
        pos += sprintf( pos, "%04x:", (UINT)GetCurrentThreadId() );
    }
    if (function && cls < ARRAY_SIZE( classes ))
        pos += snprintf( pos, sizeof(info->output) - (pos - info->output), "%s:%s:%s ",
                         classes[cls], channel->name, function );
    info->out_pos = pos - info->output;
    return info->out_pos;
}

/***********************************************************************
 *		dbg_init
 */
void dbg_init(void)
{
    struct __wine_debug_channel *options;

    setbuf( stdout, NULL );
    setbuf( stderr, NULL );

    if (nb_debug_options == -1) init_options();

    options = (struct __wine_debug_channel *)((char *)peb + (is_win64 ? 2 : 1) * page_size);
    memcpy( options, debug_options, (nb_debug_options + 1) * sizeof(*options) );
    debug_options = options;
    init_done = TRUE;
}


/***********************************************************************
 *              NtTraceControl  (NTDLL.@)
 */
NTSTATUS WINAPI NtTraceControl( ULONG code, void *inbuf, ULONG inbuf_len,
                                void *outbuf, ULONG outbuf_len, ULONG *size )
{
    FIXME( "code %u, inbuf %p, inbuf_len %u, outbuf %p, outbuf_len %u, size %p\n",
           (int)code, inbuf, (int)inbuf_len, outbuf, (int)outbuf_len, size );
    return STATUS_SUCCESS;
}


/***********************************************************************
 *              NtSetDebugFilterState  (NTDLL.@)
 */
NTSTATUS WINAPI NtSetDebugFilterState( ULONG component_id, ULONG level, BOOLEAN state )
{
    FIXME( "component_id %#x, level %u, state %#x stub.\n", (int)component_id, (int)level, state );

    return STATUS_SUCCESS;
}
