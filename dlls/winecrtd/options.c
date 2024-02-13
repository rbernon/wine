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

/* add a new debug option at the end of the option list */
static int dbg_add_option( struct __wine_debug_channel *options, int option_count, unsigned char default_flags,
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
int __wine_dbg_parse_options( struct __wine_debug_channel *options, int max_options,
                              const char *wine_debug, const char *app_name )
{
    static const char *const debug_classes[] = {"fixme", "err", "warn", "trace"};
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

        if ((p = strchr( opt, ':' )))
        {
            if (strncasecmp( opt, app_name, p - opt )) continue;
            opt = p + 1;
        }

        p = opt + strcspn( opt, "+-" );
        if (p == end) p = opt;  /* assume it's a debug channel name */

        if (p > opt)
        {
            for (i = 0; i < ARRAY_SIZE( debug_classes ); i++)
            {
                int len = strlen( debug_classes[i] );
                if (len != (p - opt)) continue;
                if (!memcmp( opt, debug_classes[i], len ))  /* found it */
                {
                    if (*p == '+') set |= 1 << i;
                    else clear |= 1 << i;
                    break;
                }
            }
            if (i == ARRAY_SIZE( debug_classes )) /* bad class name, skip it */
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
            count = dbg_add_option( options, count, default_flags, option.name, set, clear );
            if (count >= max_options - 1) break; /* too many options */
        }
    }

    options[count++].flags = default_flags;
    return count;
}
