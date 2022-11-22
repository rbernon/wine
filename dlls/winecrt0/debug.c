/*
 * Fallbacks for debugging functions when running on Windows
 *
 * Copyright 2019 Alexandre Julliard
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

#ifdef __WINE_PE_BUILD

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "windef.h"
#include "winbase.h"
#include "wine/debug.h"
#include "wine/heap.h"

WINE_DECLARE_DEBUG_CHANNEL(pid);
WINE_DECLARE_DEBUG_CHANNEL(timestamp);

static DWORD debug_info_tls;
struct debug_info
{
    unsigned int str_pos;       /* current position in strings buffer */
    unsigned int out_pos;       /* current position in output buffer */
    char         strings[1020]; /* buffer for temporary strings */
    char         output[1020];  /* current output line */
};

C_ASSERT( sizeof(struct debug_info) == 0x800 );

static int (__cdecl *p__wine_dbg_output)( const char *str );
static int (__cdecl *p__wine_dbg_header)( enum __wine_debug_class cls,
                                          struct __wine_debug_channel *channel,
                                          const char *function );

static DWORD partial_line_tid;  /* id of the last thread to output a partial line */

static void load_func( void **func, const char *name, void *def )
{
    if (!*func)
    {
        DWORD err = GetLastError();
        HMODULE module = GetModuleHandleW( L"ntdll.dll" );
        void *proc = GetProcAddress( module, name );
        InterlockedExchangePointer( func, proc ? proc : def );
        SetLastError( err );
    }
}
#define LOAD_FUNC(name) load_func( (void **)&p ## name, #name, fallback ## name )

/* parse a set of debugging option specifications and add them to the option list */
struct __wine_debug_channel *__wine_dbg_parse_options( const char *winedebug, LONG *option_count ) DECLSPEC_HIDDEN;

static void spin_lock( LONG *lock )
{
    while (InterlockedCompareExchange( lock, -1, 0 ))
        YieldProcessor();
}

static void spin_unlock( LONG *lock )
{
    InterlockedExchange( lock, 0 );
}

static void dbg_info_init(void)
{
    static LONG lock;

    if (debug_info_tls)
        return;

    spin_lock( &lock );

    if (debug_info_tls)
        debug_info_tls = TlsAlloc();

    spin_unlock( &lock );
}

/* initialize all options at startup */
static void __cdecl fallback__wine_dbg_init( struct __wine_debug_channel **options, LONG *option_count )
{
    static LONG lock;

    if (*option_count != -1)
        return;

    spin_lock( &lock );

    if (*option_count == -1)
        *options = __wine_dbg_parse_options( getenv( "WINEDEBUG" ), option_count );

    spin_unlock( &lock );
}

void __cdecl __wine_dbg_init( struct __wine_debug_channel **options, LONG *option_count )
{
    static typeof(fallback__wine_dbg_init) *p__wine_dbg_init;
    LOAD_FUNC( __wine_dbg_init );
    p__wine_dbg_init( options, option_count );
}

static struct debug_info *__cdecl fallback__wine_dbg_get_info(void)
{
    struct debug_info *debug_info;

    dbg_info_init();

    if ((debug_info = TlsGetValue( debug_info_tls ))) return debug_info;
    debug_info = calloc( 1, sizeof(struct debug_info) );
    TlsSetValue( debug_info_tls, debug_info );
    return debug_info;
}

struct debug_info *__cdecl __wine_dbg_get_info(void)
{
    static typeof(fallback__wine_dbg_get_info) *p__wine_dbg_get_info;
    LOAD_FUNC( __wine_dbg_get_info );
    return p__wine_dbg_get_info();
}

static int __cdecl fallback__wine_dbg_output( const char *str )
{
    size_t len = strlen( str );

    if (!len) return 0;
    InterlockedExchange( (LONG *)&partial_line_tid, str[len - 1] != '\n' ? GetCurrentThreadId() : 0 );
    return fwrite( str, 1, len, stderr );
}

static int __cdecl fallback__wine_dbg_header( enum __wine_debug_class cls,
                                              struct __wine_debug_channel *channel,
                                              const char *function )
{
    static const char * const classes[] = { "fixme", "err", "warn", "trace" };
    char buffer[200], *pos = buffer;

    if (!(__wine_dbg_get_channel_flags( channel ) & (1 << cls))) return -1;

    /* skip header if partial line and no other thread came in between */
    if (partial_line_tid == GetCurrentThreadId()) return 0;

    if (TRACE_ON(timestamp))
    {
        UINT ticks = GetTickCount();
        pos += sprintf( pos, "%3u.%03u:", ticks / 1000, ticks % 1000 );
    }
    if (TRACE_ON(pid)) pos += sprintf( pos, "%04x:", (UINT)GetCurrentProcessId() );
    pos += sprintf( pos, "%04x:", (UINT)GetCurrentThreadId() );
    if (function && cls < ARRAY_SIZE( classes ))
        snprintf( pos, sizeof(buffer) - (pos - buffer), "%s:%s:%s ",
                  classes[cls], channel->name, function );

    return fwrite( buffer, 1, strlen(buffer), stderr );
}

int __cdecl __wine_dbg_output( const char *str )
{
    LOAD_FUNC( __wine_dbg_output );
    return p__wine_dbg_output( str );
}

int __cdecl __wine_dbg_header( enum __wine_debug_class cls, struct __wine_debug_channel *channel,
                               const char *function )
{
    LOAD_FUNC( __wine_dbg_header );
    return p__wine_dbg_header( cls, channel, function );
}

#endif  /* __WINE_PE_BUILD */
