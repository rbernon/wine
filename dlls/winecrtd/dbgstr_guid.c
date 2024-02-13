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

/* pull qsort / bsearch / memcmp from CRT or ntdll */
#define _CRTIMP
/* avoid RtlRunOnceExecuteOnce import in ntdll.dll */
#define _NTSYSTEM_

#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

#ifdef WINE_UNIX_LIB
#include <pthread.h>
#endif

#define WINBASEAPI
#include "windef.h"
#include "winbase.h"
#include "winternl.h"

#define OUR_GUID_ENTRY(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    static const GUID name = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } };

#include "uuids.h"

#include "wine/debug.h"

static const GUID GUID_NULL;

struct guid_def
{
    const GUID *guid;
    const char *name;
};

static int guid_def_cmp( const void *a, const void *b )
{
    const struct guid_def *a_def = a, *b_def = b;
    return memcmp( a_def->guid, b_def->guid, sizeof(GUID) );
}

static struct guid_def guid_defs[] =
{
#define X( g ) { &(g), #g }
    X( GUID_NULL ),

#undef OUR_GUID_ENTRY
#define OUR_GUID_ENTRY( name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8 ) X( name ),
#include "uuids.h"

#undef X
};

static DWORD WINAPI sort_guid_defs( RTL_RUN_ONCE *once, void *param, void **context )
{
    qsort( guid_defs, ARRAY_SIZE(guid_defs), sizeof(struct guid_def), guid_def_cmp );
    return TRUE;
}

const char *wine_dbgstr_guid( const GUID *guid )
{
#ifdef WINE_UNIX_LIB
    static pthread_once_t init_once = PTHREAD_ONCE_INIT;
#else
    static RTL_RUN_ONCE once = RTL_RUN_ONCE_INIT;
#endif
    struct guid_def *ret = NULL, tmp = {.guid = guid};

    if (!guid) return "(null)";
    if (!((ULONG_PTR)guid >> 16)) return wine_dbg_sprintf( "<guid-0x%04hx>", (WORD)(ULONG_PTR)guid );

#ifdef WINE_UNIX_LIB
    pthread_once( &init_once, (void(*)(void))sort_guid_defs );
#else
    RtlRunOnceExecuteOnce( &once, sort_guid_defs, NULL, NULL );
#endif

    ret = bsearch( &tmp, guid_defs, ARRAY_SIZE(guid_defs), sizeof(*guid_defs), guid_def_cmp );
    if (ret) return wine_dbg_sprintf( "%s", ret->name );

    return wine_dbg_sprintf( "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
                             (unsigned int)guid->Data1, guid->Data2, guid->Data3,
                             guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3],
                             guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7] );
}
