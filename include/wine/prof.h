/*
 * Wine profiling interface
 *
 * Copyright 2019 Rémi Bernon for CodeWeavers
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

#ifndef __WINE_WINE_PROF_H
#define __WINE_WINE_PROF_H

#include <stdarg.h>
#include <windef.h>
#include <winbase.h>

#include "wine/debug.h"

#ifdef __cplusplus
extern "C"
{
#endif

struct __wine_prof_data
{
    const char *name;
    size_t print_ticks;
    size_t cumul_ticks;
    size_t limit_ns;
};

extern struct __wine_prof_data *__cdecl __wine_prof_data_alloc( size_t size );
extern size_t __cdecl __wine_prof_start( struct __wine_prof_data * );
extern void __cdecl __wine_prof_stop( struct __wine_prof_data *data, size_t start_ticks,
                                      const char *file, int line, const char *function, void *retaddr );

#define PROF_STOP( prof, start )  __wine_prof_stop( prof, start, __FILE__, __LINE__, __func__, \
                                                    __builtin_extract_return_addr(__builtin_return_address(0)) )

#define PROF_STR_I( x ) #x
#define PROF_STR( x ) PROF_STR_I( x )

#define PROF_SCOPE_START( n ) PROF_SCOPE_START_LIMIT( n, 0 )
#define PROF_SCOPE_START_LIMIT( n, l )                                                             \
    do {                                                                                           \
        static struct __wine_prof_data __prof = {PROF_STR( n ), 0, 0, l};                          \
        size_t __prof_start = __wine_prof_start( &__prof )

#define PROF_SCOPE_END()                                                                           \
        PROF_STOP( &__prof, __prof_start );                                                        \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* __WINE_WINE_PROF_H */
