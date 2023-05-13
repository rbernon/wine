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

typedef struct _WINE_MEMSTORE
{
    WINECRYPT_CERTSTORE hdr;
    CRITICAL_SECTION cs;
    struct list certs;
    struct list crls;
    struct list ctls;
} WINE_MEMSTORE;

static BOOL store_memory_add_context( WINE_MEMSTORE *store, struct list *list, context_t *orig_context,
                                      context_t *existing, context_t **ret_context, BOOL use_link )
{
    context_t *context;

    context = orig_context->vtbl->clone( orig_context, &store->hdr, use_link );
    if (!context) return FALSE;

    TRACE( "adding %p\n", context );
    EnterCriticalSection( &store->cs );
    if (existing)
    {
        context->u.entry.prev = existing->u.entry.prev;
        context->u.entry.next = existing->u.entry.next;
        context->u.entry.prev->next = &context->u.entry;
        context->u.entry.next->prev = &context->u.entry;
        list_init( &existing->u.entry );
        if (!existing->ref) Context_Release( existing );
    }
    else
    {
        list_add_head( list, &context->u.entry );
    }
    LeaveCriticalSection( &store->cs );

    if (ret_context) *ret_context = context;
    else Context_Release( context );
    return TRUE;
}

static context_t *store_memory_enum_context( WINE_MEMSTORE *store, struct list *list, context_t *prev )
{
    struct list *next;
    context_t *ret;

    EnterCriticalSection( &store->cs );
    if (prev)
    {
        next = list_next( list, &prev->u.entry );
        Context_Release( prev );
    }
    else
    {
        next = list_next( list, list );
    }
    LeaveCriticalSection( &store->cs );

    if (!next)
    {
        SetLastError( CRYPT_E_NOT_FOUND );
        return NULL;
    }

    ret = LIST_ENTRY( next, context_t, u.entry );
    Context_AddRef( ret );
    return ret;
}

static BOOL store_memory_delete_context( WINE_MEMSTORE *store, context_t *context )
{
    BOOL in_list = FALSE;

    EnterCriticalSection( &store->cs );
    if (!list_empty( &context->u.entry ))
    {
        list_remove( &context->u.entry );
        list_init( &context->u.entry );
        in_list = TRUE;
    }
    LeaveCriticalSection( &store->cs );

    if (in_list && !context->ref) Context_Free( context );
    return TRUE;
}

static void free_contexts( struct list *list )
{
    context_t *context, *next;

    LIST_FOR_EACH_ENTRY_SAFE ( context, next, list, context_t, u.entry )
    {
        TRACE( "freeing %p\n", context );
        list_remove( &context->u.entry );
        Context_Free( context );
    }
}

static void store_memory_release_context( WINECRYPT_CERTSTORE *store, context_t *context )
{
    /* Free the context only if it's not in a list. Otherwise it may be reused later. */
    if (list_empty( &context->u.entry )) Context_Free( context );
}

static BOOL store_memory_add_cert( WINECRYPT_CERTSTORE *store, context_t *cert,
                                   context_t *toReplace, context_t **ppStoreContext, BOOL use_link )
{
    WINE_MEMSTORE *ms = (WINE_MEMSTORE *)store;

    TRACE( "(%p, %p, %p, %p)\n", store, cert, toReplace, ppStoreContext );
    return store_memory_add_context( ms, &ms->certs, cert, toReplace, ppStoreContext, use_link );
}

static context_t *store_memory_enum_cert( WINECRYPT_CERTSTORE *store, context_t *prev )
{
    WINE_MEMSTORE *ms = (WINE_MEMSTORE *)store;

    TRACE( "(%p, %p)\n", store, prev );

    return store_memory_enum_context( ms, &ms->certs, prev );
}

static BOOL store_memory_delete_cert( WINECRYPT_CERTSTORE *store, context_t *context )
{
    WINE_MEMSTORE *ms = (WINE_MEMSTORE *)store;

    TRACE( "(%p, %p)\n", store, context );

    return store_memory_delete_context( ms, context );
}

static BOOL store_memory_add_crl( WINECRYPT_CERTSTORE *store, context_t *crl, context_t *toReplace,
                                  context_t **ppStoreContext, BOOL use_link )
{
    WINE_MEMSTORE *ms = (WINE_MEMSTORE *)store;

    TRACE( "(%p, %p, %p, %p)\n", store, crl, toReplace, ppStoreContext );

    return store_memory_add_context( ms, &ms->crls, crl, toReplace, ppStoreContext, use_link );
}

static context_t *store_memory_enum_crl( WINECRYPT_CERTSTORE *store, context_t *prev )
{
    WINE_MEMSTORE *ms = (WINE_MEMSTORE *)store;

    TRACE( "(%p, %p)\n", store, prev );

    return store_memory_enum_context( ms, &ms->crls, prev );
}

static BOOL store_memory_delete_crl( WINECRYPT_CERTSTORE *store, context_t *context )
{
    WINE_MEMSTORE *ms = (WINE_MEMSTORE *)store;

    TRACE( "(%p, %p)\n", store, context );

    return store_memory_delete_context( ms, context );
}

static BOOL store_memory_add_ctl( WINECRYPT_CERTSTORE *store, context_t *ctl, context_t *toReplace,
                                  context_t **ppStoreContext, BOOL use_link )
{
    WINE_MEMSTORE *ms = (WINE_MEMSTORE *)store;

    TRACE( "(%p, %p, %p, %p)\n", store, ctl, toReplace, ppStoreContext );

    return store_memory_add_context( ms, &ms->ctls, ctl, toReplace, ppStoreContext, use_link );
}

static context_t *store_memory_enum_ctl( WINECRYPT_CERTSTORE *store, context_t *prev )
{
    WINE_MEMSTORE *ms = (WINE_MEMSTORE *)store;

    TRACE( "(%p, %p)\n", store, prev );

    return store_memory_enum_context( ms, &ms->ctls, prev );
}

static BOOL store_memory_delete_ctl( WINECRYPT_CERTSTORE *store, context_t *context )
{
    WINE_MEMSTORE *ms = (WINE_MEMSTORE *)store;

    TRACE( "(%p, %p)\n", store, context );

    return store_memory_delete_context( ms, context );
}

static void store_memory_addref( WINECRYPT_CERTSTORE *store )
{
    LONG ref = InterlockedIncrement( &store->ref );
    TRACE( "ref = %ld\n", ref );
}

static DWORD store_memory_release( WINECRYPT_CERTSTORE *cert_store, DWORD flags )
{
    WINE_MEMSTORE *store = (WINE_MEMSTORE *)cert_store;
    LONG ref;

    if (flags & ~CERT_CLOSE_STORE_CHECK_FLAG) FIXME( "Unimplemented flags %lx\n", flags );

    ref = InterlockedDecrement( &store->hdr.ref );
    TRACE( "(%p) ref=%ld\n", store, ref );
    if (ref) return (flags & CERT_CLOSE_STORE_CHECK_FLAG) ? CRYPT_E_PENDING_CLOSE : ERROR_SUCCESS;

    free_contexts( &store->certs );
    free_contexts( &store->crls );
    free_contexts( &store->ctls );
    store->cs.DebugInfo->Spare[0] = 0;
    DeleteCriticalSection( &store->cs );
    CRYPT_FreeStore( &store->hdr );
    return ERROR_SUCCESS;
}

static BOOL store_memory_control( WINECRYPT_CERTSTORE *store, DWORD dwFlags, DWORD dwCtrlType, void const *pvCtrlPara )
{
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return FALSE;
}

static const store_vtbl_t store_memory_vtbl =
{
    store_memory_addref,
    store_memory_release,
    store_memory_release_context,
    store_memory_control,
    {store_memory_add_cert, store_memory_enum_cert, store_memory_delete_cert},
    {store_memory_add_crl, store_memory_enum_crl, store_memory_delete_crl},
    {store_memory_add_ctl, store_memory_enum_ctl, store_memory_delete_ctl},
};

WINECRYPT_CERTSTORE *CRYPT_MemOpenStore( HCRYPTPROV hCryptProv, DWORD dwFlags, const void *pvPara )
{
    WINE_MEMSTORE *store;

    TRACE( "(%Id, %08lx, %p)\n", hCryptProv, dwFlags, pvPara );

    if (dwFlags & CERT_STORE_DELETE_FLAG)
    {
        SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
        store = NULL;
    }
    else
    {
        store = CryptMemAlloc( sizeof(WINE_MEMSTORE) );
        if (store)
        {
            memset( store, 0, sizeof(WINE_MEMSTORE) );
            CRYPT_InitStore( &store->hdr, dwFlags, StoreTypeMem, &store_memory_vtbl );
            InitializeCriticalSection( &store->cs );
            store->cs.DebugInfo->Spare[0] = (DWORD_PTR)( __FILE__ ": ContextList.cs" );
            list_init( &store->certs );
            list_init( &store->crls );
            list_init( &store->ctls );
            /* Mem store doesn't need crypto provider, so close it */
            if (hCryptProv && !(dwFlags & CERT_STORE_NO_CRYPT_RELEASE_FLAG))
                CryptReleaseContext( hCryptProv, 0 );
        }
    }
    return (WINECRYPT_CERTSTORE *)store;
}
