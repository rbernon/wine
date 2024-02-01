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

/* avoid __wine_dbg_write import */
#define _NTSYSTEM_

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "windef.h"
#include "winbase.h"
#include "wine/debug.h"
#include "wine/heap.h"

static int __cdecl (__cdecl *p__wine_dbg_init)( struct __wine_debug_channel **options );
static struct debug_info *(__cdecl *p__wine_dbg_get_info)(void);
static int (WINAPI *p__wine_dbg_write)( const char *str, unsigned int len );
static const int max_debug_options = 2048; /* see ntdll/unix/debug.c */

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


/* winecrtd/options.c */
/* parse a set of debugging option specifications and add them to the option list */
extern int __wine_dbg_parse_options( struct __wine_debug_channel *options, int max_options,
                                     const char *wine_debug, const char *app_name );

/* initialize all options at startup */
static int __cdecl fallback__wine_dbg_init( struct __wine_debug_channel **options )
{
    if (!(*options = heap_alloc( max_debug_options * sizeof(**options) ))) return 0;
    return __wine_dbg_parse_options( *options, max_debug_options, getenv( "WINEDEBUG" ), "" );
}

int __cdecl __wine_dbg_init( struct __wine_debug_channel **options )
{
    LOAD_FUNC( __wine_dbg_init );
    return p__wine_dbg_init( options );
}

static struct debug_info *__cdecl fallback__wine_dbg_get_info(void)
{
    static DWORD debug_info_tls;
    struct debug_info *debug_info;

    if (!debug_info_tls) debug_info_tls = TlsAlloc();
    if ((debug_info = TlsGetValue( debug_info_tls ))) return debug_info;
    debug_info = heap_alloc_zero( sizeof(struct debug_info) );
    TlsSetValue( debug_info_tls, debug_info );
    return debug_info;
}

struct debug_info *__cdecl __wine_dbg_get_info(void)
{
    LOAD_FUNC( __wine_dbg_get_info );
    return p__wine_dbg_get_info();
}

static int WINAPI fallback__wine_dbg_write( const char *str, unsigned int len )
{
    len = fwrite( str, 1, len, stderr );
    fflush( stderr );
    return len;
}

int WINAPI __wine_dbg_write( const char *str, unsigned int len )
{
    LOAD_FUNC( __wine_dbg_write );
    return p__wine_dbg_write( str, len );
}

#endif  /* __WINE_PE_BUILD */
