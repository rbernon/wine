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

static int __cdecl (__cdecl *p__wine_dbg_init)( struct __wine_debug_channel **options );
static const char * (__cdecl *p__wine_dbg_strdup)( const char *str );
static int (__cdecl *p__wine_dbg_output)( const char *str );
static unsigned char (__cdecl *p__wine_dbg_get_channel_flags)( struct __wine_debug_channel *channel );
static int (__cdecl *p__wine_dbg_header)( enum __wine_debug_class cls,
                                          struct __wine_debug_channel *channel,
                                          const char *function );

static const char * const debug_classes[] = { "fixme", "err", "warn", "trace" };

static const int max_debug_options = 2048; /* see ntdll/unix/debug.c */
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
static int add_option( struct __wine_debug_channel *options, int option_count, unsigned char default_flags,
                       const char *name, unsigned char set, unsigned char clear )
{
    struct __wine_debug_channel *tmp, *opt = options, *end = opt + option_count;
    int res;

    while (opt < end)
    {
        tmp = opt + (end - opt) / 2;
        if (!(res = strcmp( name, tmp->name )))
        {
            tmp->flags = (tmp->flags & ~clear) | set;
            return option_count;
        }
        if (res < 0) end = tmp;
        else opt = tmp + 1;
    }

    end = options + option_count;
    memmove( opt + 1, opt, (char *)end - (char *)opt );
    strcpy( opt->name, name );
    opt->flags = (default_flags & ~clear) | set;
    return option_count + 1;
}

/* parse a set of debugging option specifications and add them to the option list */
static int parse_options( struct __wine_debug_channel *options, int max_options,
                          const char *wine_debug )
{
    unsigned char default_flags = (1 << __WINE_DBCL_ERR) | (1 << __WINE_DBCL_FIXME);
    const char *opt, *next;
    unsigned int i, count = 0;

    for (opt = wine_debug; opt; opt = next)
    {
        struct __wine_debug_channel option = {0};
        const char *p, *end;
        unsigned char set = 0, clear = 0;

        if ((next = strchr( opt, ',' ))) end = next++;
        else end = opt + strlen( opt );

        p = opt + strcspn( opt, "+-" );
        if (p == end) p = opt;  /* assume it's a debug channel name */

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
        if (end - p >= sizeof(option.name)) continue; /* name too long */
        memcpy( option.name, p, end - p );

        if (!strcmp( option.name, "all" ) || !option.name[0])
            default_flags = (default_flags & ~clear) | set;
        else
        {
            count = add_option( options, count, default_flags, option.name, set, clear );
            if (count >= max_options - 1) break; /* too many options */
        }
    }

    options[count++].flags = default_flags;
    return count;
}

/* initialize all options at startup */
static int __cdecl fallback__wine_dbg_init( struct __wine_debug_channel **options )
{
    if (!(*options = heap_alloc( max_debug_options * sizeof(**options) ))) return 0;
    return parse_options( *options, max_debug_options, getenv( "WINEDEBUG" ) );
}

int __cdecl __wine_dbg_init( struct __wine_debug_channel **options )
{
    LOAD_FUNC( __wine_dbg_init );
    return p__wine_dbg_init( options );
}

/* FIXME: this is not 100% thread-safe */
static const char * __cdecl fallback__wine_dbg_strdup( const char *str )
{
    static char *list[32];
    static LONG pos;
    char *ret = strdup( str );
    int idx;

    idx = InterlockedIncrement( &pos ) % ARRAY_SIZE(list);
    free( InterlockedExchangePointer( (void **)&list[idx], ret ));
    return ret;
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
    static int nb_debug_options = -1;

    unsigned char default_flags;
    int min, max, pos, res, count;

    if (nb_debug_options < 0) nb_debug_options = __wine_dbg_init( &debug_options );
    if (!nb_debug_options) return (1 << __WINE_DBCL_ERR) | (1 << __WINE_DBCL_FIXME);
    count = nb_debug_options < 0 ? -nb_debug_options : nb_debug_options;

    min = 0;
    max = count - 2;
    while (min <= max)
    {
        pos = (min + max) / 2;
        res = strcmp( channel->name, debug_options[pos].name );
        if (!res) return debug_options[pos].flags;
        if (res < 0) max = pos - 1;
        else min = pos + 1;
    }
    /* no option for this channel */
    default_flags = debug_options[count - 1].flags;
    if (channel->flags & (1 << __WINE_DBCL_INIT)) channel->flags = default_flags;
    return default_flags;
}

const char * __cdecl __wine_dbg_strdup( const char *str )
{
    LOAD_FUNC( __wine_dbg_strdup );
    return p__wine_dbg_strdup( str );
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
