/*
 * Copyright 2021 Rémi Bernon for CodeWeavers
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
#include <stdlib.h>

#include "windef.h"
#include "winbase.h"
#include "winnt.h"
#include "winuser.h"
#include "wingdi.h"

#include "wine/rbtree.h"
#include "wine/debug.h"

#include "unixlib.h"
#include "win32u.h"

WINE_DEFAULT_DEBUG_CHANNEL(win32u);

struct hwnd_surfaces
{
    struct wine_rb_entry entry;
    HWND hwnd;
    struct unix_surface *toplevel;
};

static CRITICAL_SECTION surfaces_cs;
static CRITICAL_SECTION_DEBUG surfaces_cs_debug =
{
    0, 0, &surfaces_cs,
    { &surfaces_cs_debug.ProcessLocksList, &surfaces_cs_debug.ProcessLocksList },
    0, 0, { (DWORD_PTR)(__FILE__ ": surfaces_cs") }
};
static CRITICAL_SECTION surfaces_cs = { &surfaces_cs_debug, -1, 0, 0, 0, 0 };

static int hwnd_surfaces_compare( const void *key, const struct wine_rb_entry *entry )
{
    return (LONG_PTR)(HWND)key - (LONG_PTR)WINE_RB_ENTRY_VALUE( entry, struct hwnd_surfaces, entry )->hwnd;
}
static struct wine_rb_tree hwnd_surfaces = { hwnd_surfaces_compare, NULL };

static struct hwnd_surfaces *hwnd_surfaces_create( HWND hwnd )
{
    struct hwnd_surfaces *surfaces;

    if (!(surfaces = malloc(sizeof(*surfaces)))) return NULL;
    surfaces->hwnd = hwnd;
    surfaces->toplevel = NULL;

    TRACE( "created surfaces %p for hwnd %p.\n", surfaces, hwnd );

    return surfaces;
}

static void hwnd_surfaces_delete( struct hwnd_surfaces *surfaces )
{
    TRACE( "surfaces %p.\n", surfaces );

    if (surfaces->toplevel) unix_funcs->surface_delete( surfaces->toplevel );

    free(surfaces);
}

static struct hwnd_surfaces *find_surfaces_for_hwnd( HWND hwnd )
{
    struct wine_rb_entry *entry;
    struct hwnd_surfaces *surfaces;

    if (!(entry = wine_rb_get( &hwnd_surfaces, hwnd ))) surfaces = NULL;
    else surfaces = WINE_RB_ENTRY_VALUE( entry, struct hwnd_surfaces, entry );

    return surfaces;
}

static struct hwnd_surfaces *create_surfaces_for_hwnd( HWND hwnd )
{
    struct hwnd_surfaces *surfaces;

    if ((surfaces = find_surfaces_for_hwnd( hwnd ))) return surfaces;
    if (!(surfaces = hwnd_surfaces_create( hwnd ))) return NULL;
    wine_rb_put( &hwnd_surfaces, hwnd, &surfaces->entry );

    return surfaces;
}

void win32u_create_toplevel_surface( HWND hwnd )
{
    struct hwnd_surfaces *surfaces;

    TRACE( "hwnd %p.\n", hwnd );

    EnterCriticalSection( &surfaces_cs );
    if ((surfaces = create_surfaces_for_hwnd( hwnd )) && !surfaces->toplevel)
        surfaces->toplevel = unix_funcs->surface_create_toplevel( hwnd );
    LeaveCriticalSection( &surfaces_cs );

    if (!surfaces) ERR( "failed to create surfaces for hwnd %p!\n", hwnd );
}

void win32u_delete_toplevel_surface( HWND hwnd )
{
    struct hwnd_surfaces *surfaces;

    TRACE( "hwnd %p.\n", hwnd );

    EnterCriticalSection( &surfaces_cs );
    if ((surfaces = find_surfaces_for_hwnd( hwnd )) && surfaces->toplevel)
    {
        unix_funcs->surface_delete( surfaces->toplevel );
        surfaces->toplevel = NULL;
    }
    LeaveCriticalSection( &surfaces_cs );
}

void win32u_delete_hwnd_surfaces( HWND hwnd )
{
    struct hwnd_surfaces *surfaces;

    TRACE( "hwnd %p.\n", hwnd );

    EnterCriticalSection( &surfaces_cs );
    if ((surfaces = find_surfaces_for_hwnd( hwnd )))
    {
        wine_rb_remove( &hwnd_surfaces, &surfaces->entry );
        hwnd_surfaces_delete( surfaces );
    }
    LeaveCriticalSection( &surfaces_cs );
}
