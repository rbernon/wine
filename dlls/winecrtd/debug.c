/*
 * Copyright 2022 Rémi Bernon for CodeWeavers
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

#define _CRTIMP
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

struct debug_info
{
    unsigned int str_pos;       /* current position in strings buffer */
    unsigned int out_pos;       /* current position in output buffer */
    char         strings[1020]; /* buffer for temporary strings */
    char         output[1020];  /* current output line */
};

C_ASSERT( sizeof(struct debug_info) == 0x800 );

extern struct debug_info *__cdecl __wine_dbg_get_info(void);
extern void __cdecl __wine_dbg_init( struct __wine_debug_channel **options, LONG *options_count );

const char * __cdecl __wine_dbg_strdup( const char *str )
{
    struct debug_info *info = __wine_dbg_get_info();
    unsigned int pos = info->str_pos;
    size_t n = strlen( str ) + 1;

#ifdef __WINE_PE_BUILD
    if (n > sizeof(info->strings)) RtlRaiseStatus( STATUS_BUFFER_OVERFLOW );
#else
    if (n > sizeof(info->strings)) abort();
#endif

    if (pos + n > sizeof(info->strings)) pos = 0;
    info->str_pos = pos + n;
    return memcpy( info->strings + pos, str, n );
}

/* add a new debug option at the end of the option list */
static void dbg_add_option( struct __wine_debug_channel *options, int *option_count, unsigned char default_flags,
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
struct __wine_debug_channel *__wine_dbg_parse_options( const char *winedebug, LONG *option_count ) DECLSPEC_HIDDEN;
struct __wine_debug_channel *__wine_dbg_parse_options( const char *winedebug, LONG *option_count )
{
    static unsigned char default_flags = (1 << __WINE_DBCL_ERR) | (1 << __WINE_DBCL_FIXME);
    static const char * const debug_classes[] = { "fixme", "err", "warn", "trace" };
    static struct __wine_debug_channel option_buffer[1024];

    const char *opt, *next;
    unsigned int i;

    *option_count = 0;
    for (opt = winedebug; opt; opt = next)
    {
        struct __wine_debug_channel tmp_option = {0};
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
        if (p == end) continue;

        if (!strcmp( p, "all" ) || !p[0])
            default_flags = (default_flags & ~clear) | set;
        else if (end - p < sizeof(tmp_option.name))
        {
            memcpy( tmp_option.name, p, end - p );
            dbg_add_option( option_buffer, option_count, default_flags, tmp_option.name, set, clear );
        }
        if (*option_count >= ARRAY_SIZE(option_buffer) - 1) break; /* too many options */
    }

    option_buffer[*option_count].flags = default_flags;
    return option_buffer;
}

unsigned char __cdecl __wine_dbg_get_channel_flags( struct __wine_debug_channel *channel )
{
    static struct __wine_debug_channel *debug_options;
    static int nb_debug_options = -1;

    unsigned char default_flags, option_count;
    int min, max, pos, res;

    /* keep trying until unix side is initialized and returns count >= 0 */
    if (nb_debug_options < 0) __wine_dbg_init( &debug_options, &nb_debug_options );
    option_count = nb_debug_options >= 0 ? nb_debug_options : -(nb_debug_options + 1);

    min = 0;
    max = option_count - 1;
    while (min <= max)
    {
        pos = (min + max) / 2;
        res = strcmp( channel->name, debug_options[pos].name );
        if (!res) return debug_options[pos].flags;
        if (res < 0) max = pos - 1;
        else min = pos + 1;
    }
    /* no option for this channel */
    default_flags = debug_options[option_count].flags;
    if (channel->flags & (1 << __WINE_DBCL_INIT)) channel->flags = default_flags;
    return default_flags;
}

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
#ifdef __WINE_PE_BUILD
        RtlRaiseStatus( STATUS_BUFFER_OVERFLOW );
#else
        abort();
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
    if (*str) ret += append_output( info, str, strlen( str ));
    return ret;
}
