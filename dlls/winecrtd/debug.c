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
void __wine_dbg_add_option( struct __wine_debug_channel *options, int *options_count, unsigned char default_flags,
                            const char *name, unsigned char set, unsigned char clear ) DECLSPEC_HIDDEN;
void __wine_dbg_add_option( struct __wine_debug_channel *options, int *option_count, unsigned char default_flags,
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
