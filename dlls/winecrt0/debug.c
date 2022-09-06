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
static unsigned char (__cdecl *p__wine_dbg_get_channel_flags)( struct __wine_debug_channel *channel );
static int (__cdecl *p__wine_dbg_header)( enum __wine_debug_class cls,
                                          struct __wine_debug_channel *channel,
                                          const char *function );

static const char * const debug_classes[] = { "fixme", "err", "warn", "trace" };

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


/* add a new debug option at the end of the option list */
static void add_option( struct __wine_debug_channel *options, LONG *option_count, unsigned char default_flags,
                        const char *name, unsigned char set, unsigned char clear )
{
    struct __wine_debug_channel *tmp, *opt = options, *end = opt + *option_count;
    int res;

    while (opt < end)
    {
        tmp = opt + (end - opt) / 2;
        if (!(res = strcmp( name, tmp->name )))
        {
            tmp->flags = (tmp->flags & ~clear) | set;
            return;
        }
        if (res < 0) end = tmp;
        else opt = tmp + 1;
    }

    end = options + *option_count;
    memmove( opt + 1, opt, (char *)end - (char *)opt );
    strcpy( opt->name, name );
    opt->flags = (default_flags & ~clear) | set;
    (*option_count)++;
}

/* parse a set of debugging option specifications and add them to the option list */
static struct __wine_debug_channel *parse_options( const char *str, LONG *option_count )
{
    static struct __wine_debug_channel option_buffer[1024];

    unsigned char default_flags = (1 << __WINE_DBCL_ERR) | (1 << __WINE_DBCL_FIXME);
    char *opt, *next, *options;
    unsigned int i;

    *option_count = 0;

    if (!str) options = NULL;
    else options = _strdup( str );

    for (opt = options; opt; opt = next)
    {
        const char *p;
        unsigned char set = 0, clear = 0;

        if ((next = strchr( opt, ',' ))) *next++ = 0;

        p = opt + strcspn( opt, "+-" );
        if (!p[0]) p = opt;  /* assume it's a debug channel name */

        if (p > opt)
        {
            for (i = 0; i < ARRAY_SIZE(debug_classes); i++)
            {
                int len = strlen(debug_classes[i]);
                if (len != (p - opt)) continue;
                if (!memcmp( opt, debug_classes[i], len ))  /* found it */
                {
                    if (*p == '+') set |= 1 << i;
                    else clear |= 1 << i;
                    break;
                }
            }
            if (i == ARRAY_SIZE(debug_classes)) /* bad class name, skip it */
                continue;
        }
        else
        {
            if (*p == '-') clear = ~0;
            else set = ~0;
        }
        if (*p == '+' || *p == '-') p++;
        if (!p[0]) continue;

        if (!strcmp( p, "all" ) || !p[0])
            default_flags = (default_flags & ~clear) | set;
        else if (strlen( p ) < sizeof(option_buffer[0].name))
            add_option( option_buffer, option_count, default_flags, p, set, clear );
        if (*option_count >= ARRAY_SIZE(option_buffer) - 1) break; /* too many options */
    }
    free( options );

    option_buffer[*option_count].flags = default_flags;
    return option_buffer;
}

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
        *options = parse_options( getenv( "WINEDEBUG" ), option_count );

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
    if (function && cls < ARRAY_SIZE( debug_classes ))
        snprintf( pos, sizeof(buffer) - (pos - buffer), "%s:%s:%s ",
                  debug_classes[cls], channel->name, function );

    return fwrite( buffer, 1, strlen(buffer), stderr );
}

static unsigned char __cdecl fallback__wine_dbg_get_channel_flags( struct __wine_debug_channel *channel )
{
    static struct __wine_debug_channel *debug_options;
    static LONG nb_debug_options = -1;

    unsigned char default_flags;
    int min, max, pos, res;

    if (nb_debug_options == -1) __wine_dbg_init( &debug_options, &nb_debug_options );

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

int __cdecl __wine_dbg_output( const char *str )
{
    LOAD_FUNC( __wine_dbg_output );
    return p__wine_dbg_output( str );
}

unsigned char __cdecl __wine_dbg_get_channel_flags( struct __wine_debug_channel *channel )
{
    LOAD_FUNC( __wine_dbg_get_channel_flags );
    return p__wine_dbg_get_channel_flags( channel );
}

int __cdecl __wine_dbg_header( enum __wine_debug_class cls, struct __wine_debug_channel *channel,
                               const char *function )
{
    LOAD_FUNC( __wine_dbg_header );
    return p__wine_dbg_header( cls, channel, function );
}

#endif  /* __WINE_PE_BUILD */
