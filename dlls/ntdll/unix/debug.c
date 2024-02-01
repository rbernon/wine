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

static BOOL init_done;
static struct debug_info initial_info;  /* debug info for initial thread */
static int nb_debug_options = -1;
static struct __wine_debug_channel *debug_options;
static const int peb_options_offset = (is_win64 ? 2 : 1) * page_size;
static const int max_debug_options = ((signal_stack_mask + 1) - peb_options_offset) / sizeof(*debug_options);

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

/* winecrtd/options.c */
/* parse a set of debugging option specifications and add them to the option list */
extern int __wine_dbg_parse_options( struct __wine_debug_channel *options, int max_options,
                                     const char *wine_debug, const char *app_name );

/* print the usage message */
static void debug_usage(void)
{
    static const char usage[] =
        "Syntax of the WINEDEBUG variable:\n"
        "  WINEDEBUG=[[process:]class]+xxx,[[process:]class]-yyy,...\n\n"
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
    const char *app_name, *p;
    struct stat st1, st2;

    /* check for stderr pointing to /dev/null */
    if (!fstat( 2, &st1 ) && S_ISCHR(st1.st_mode) &&
        !stat( "/dev/null", &st2 ) && S_ISCHR(st2.st_mode) &&
        st1.st_rdev == st2.st_rdev) wine_debug = "-all";

    if (wine_debug && !strcmp( wine_debug, "help" )) debug_usage();

    app_name = main_argv[1];
    while ((p = strpbrk( app_name, "/\\" ))) app_name = p + 1;

    assert( max_debug_options >= 2048 );
    if (!(debug_options = malloc( max_debug_options * sizeof(*debug_options) ))) nb_debug_options = 0;
    else nb_debug_options = __wine_dbg_parse_options( debug_options, max_debug_options, wine_debug, app_name );
}

int WINAPI __wine_dbg_write( const char *str, unsigned int len )
{
    return write( 2, str, len );
}

/***********************************************************************
 *		unixcall_wine_dbg_write
 */
NTSTATUS unixcall_wine_dbg_write( void *args )
{
    struct wine_dbg_write_params *params = args;

    return write( 2, params->str, params->len );
}

#ifdef _WIN64
/***********************************************************************
 *		wow64_wine_dbg_write
 */
NTSTATUS wow64_wine_dbg_write( void *args )
{
    struct
    {
        ULONG        str;
        unsigned int len;
    } const *params32 = args;

    return write( 2, ULongToPtr(params32->str), params32->len );
}
#endif

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
            pos += snprintf( pos, sizeof(info->output) - (pos - info->output), "%3u.%03u:", ticks / 1000, ticks % 1000 );
        }
        if (TRACE_ON(pid)) pos += snprintf( pos, sizeof(info->output) - (pos - info->output), "%04x:", (UINT)GetCurrentProcessId() );
        pos += snprintf( pos, sizeof(info->output) - (pos - info->output), "%04x:", (UINT)GetCurrentThreadId() );
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

    options = (struct __wine_debug_channel *)((char *)peb + peb_options_offset);
    memcpy( options, debug_options, nb_debug_options * sizeof(*options) );
    free( debug_options );
    debug_options = options;
    init_done = TRUE;
}

int __cdecl __wine_dbg_init( struct __wine_debug_channel **options )
{
    if (nb_debug_options == -1) init_options();
    *options = debug_options;
    return init_done ? nb_debug_options : -nb_debug_options;
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
