/*
 * Copyright 2002 Mike McCormack for CodeWeavers
 * Copyright 2004-2006 Juan Lang
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
 *
 * FIXME:
 * - The concept of physical stores and locations isn't implemented.  (This
 *   doesn't mean registry stores et al aren't implemented.  See the PSDK for
 *   registering and enumerating physical stores and locations.)
 * - Many flags, options and whatnot are unimplemented.
 */

#include <stdarg.h>
#include <stddef.h>

#include "windef.h"
#include "winbase.h"
#include "wincrypt.h"

#include "crypt32_private.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(crypt);

static void store_empty_addref( WINECRYPT_CERTSTORE *store )
{
    TRACE( "(%p)\n", store );
}

static DWORD store_empty_release( WINECRYPT_CERTSTORE *store, DWORD flags )
{
    TRACE( "(%p)\n", store );
    return E_UNEXPECTED;
}

static void store_empty_release_context( WINECRYPT_CERTSTORE *store, context_t *context )
{
    Context_Free( context );
}

static BOOL store_empty_control( WINECRYPT_CERTSTORE *store, DWORD flags, DWORD ctrl_type, void const *ctrl_para )
{
    TRACE( "()\n" );
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return FALSE;
}

static BOOL store_empty_add( WINECRYPT_CERTSTORE *store, context_t *context, context_t *replace,
                             context_t **ret_context, BOOL use_link )
{
    TRACE( "(%p, %p, %p, %p)\n", store, context, replace, ret_context );

    /* FIXME: We should clone the context */
    if (ret_context)
    {
        Context_AddRef( context );
        *ret_context = context;
    }

    return TRUE;
}

static context_t *store_empty_enum( WINECRYPT_CERTSTORE *store, context_t *prev )
{
    TRACE( "(%p, %p)\n", store, prev );
    SetLastError( CRYPT_E_NOT_FOUND );
    return NULL;
}

static BOOL store_empty_delete( WINECRYPT_CERTSTORE *store, context_t *context )
{
    return TRUE;
}

static const store_vtbl_t store_empty_vtbl =
{
    store_empty_addref,
    store_empty_release,
    store_empty_release_context,
    store_empty_control,
    {store_empty_add, store_empty_enum, store_empty_delete},
    {store_empty_add, store_empty_enum, store_empty_delete},
    {store_empty_add, store_empty_enum, store_empty_delete},
};

WINECRYPT_CERTSTORE empty_store;

void init_empty_store(void)
{
    CRYPT_InitStore( &empty_store, CERT_STORE_READONLY_FLAG, StoreTypeEmpty, &store_empty_vtbl );
}
