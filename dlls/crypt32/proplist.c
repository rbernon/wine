/*
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
 */
#include <assert.h>
#include <stdarg.h>
#include "windef.h"
#include "winbase.h"
#include "wincrypt.h"
#include "wine/debug.h"
#include "wine/list.h"
#include "crypt32_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(crypt);

struct properties
{
    CRITICAL_SECTION cs;
    struct list list;
};

struct property
{
    DWORD id;
    DWORD size;
    BYTE *data;
    struct list entry;
};

struct properties *ContextPropertyList_Create(void)
{
    struct properties *props;

    if (list)
    {
        InitializeCriticalSectionEx(&list->cs, 0, RTL_CRITICAL_SECTION_FLAG_FORCE_DEBUG_INFO);
        list->cs.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": PCONTEXT_PROPERTY_LIST->cs");
        list_init(&list->properties);
    }
    return list;
}

void ContextPropertyList_Free( struct properties *props )
{
    struct property *prop, *next;

    LIST_FOR_EACH_ENTRY_SAFE( prop, next, &props->list, struct property, entry )
    {
        list_remove( &prop->entry );
        CryptMemFree( prop->data );
        CryptMemFree( prop );
    }
    props->cs.DebugInfo->Spare[0] = 0;
    DeleteCriticalSection( &props->cs );
    CryptMemFree( props );
}

static struct property *find_property_from_id( struct properties *props, DWORD id )
{
    struct property *prop;
    LIST_FOR_EACH_ENTRY( prop, &props->list, struct property, entry )
        if (prop->id == id) return prop;
    return NULL;
}

BOOL ContextPropertyList_FindProperty( struct properties *props, DWORD id, PCRYPT_DATA_BLOB blob )
{
    struct property *prop;
    BOOL ret = FALSE;

    TRACE( "(%p, %ld, %p)\n", props, id, blob );

    EnterCriticalSection( &props->cs );
    if ((prop = find_property_from_id( props, id )))
    {
        blob->cbData = prop->size;
        blob->pbData = prop->data;
        ret = TRUE;
    }
    LeaveCriticalSection( &props->cs );
    return ret;
}

BOOL ContextPropertyList_SetProperty( struct properties *props, DWORD id, const BYTE *pbData, size_t cbData )
{
    LPBYTE data;
    BOOL ret = FALSE;

    if (cbData)
    {
        data = CryptMemAlloc(cbData);
        if (data)
            memcpy(data, pbData, cbData);
    }
    else
        data = NULL;
    if (!cbData || data)
    {
        struct property *prop;

        EnterCriticalSection( &props->cs );
        if ((prop = find_property_from_id( props, id )))
        {
            CryptMemFree( prop->data );
            prop->size = cbData;
            prop->data = data;
            ret = TRUE;
        }
        else
        {
            prop = CryptMemAlloc( sizeof(struct property) );
            if (prop)
            {
                prop->id = id;
                prop->size = cbData;
                prop->data = data;
                list_add_tail(&props->list, &prop->entry);
                ret = TRUE;
            }
            else
                CryptMemFree(data);
        }
        LeaveCriticalSection( &props->cs );
    }
    return ret;
}

void ContextPropertyList_RemoveProperty( struct properties *props, DWORD id )
{
    struct property *prop;

    EnterCriticalSection( &props->cs );
    if ((prop = find_property_from_id( props, id )))
    {
        list_remove( &prop->entry );
        CryptMemFree( prop->data );
        CryptMemFree( prop );
    }
    LeaveCriticalSection( &props->cs );
}

/* Since the properties are stored in a list, this is a tad inefficient
 * (O(n^2)) since I have to find the previous position every time.
 */
DWORD ContextPropertyList_EnumPropIDs( struct properties *props, DWORD id )
{
    struct property *prop;
    struct list *entry;
    DWORD ret = 0;

    EnterCriticalSection( &props->cs );
    if (!id) entry = list_head( &props->list );
    else if (!(prop = find_property_from_id( props, id ))) entry = NULL;
    else entry = list_next( &props->list, &prop->entry );

    if (entry) ret = LIST_ENTRY( entry, struct property, entry )->id;
    LeaveCriticalSection( &props->cs );

    return ret;
}

void ContextPropertyList_Copy( struct properties *dst, struct properties *src )
{
    struct property *prop;

    EnterCriticalSection( &src->cs );
    LIST_FOR_EACH_ENTRY( prop, &src->list, struct property, entry )
        ContextPropertyList_SetProperty( dst, prop->id, prop->data, prop->size );
    LeaveCriticalSection( &src->cs );
}
