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

struct hwnd_surface
{
    struct wine_rb_entry entry;
    HWND hwnd;
    struct unix_surface *unix_surface;
};

static RTL_CRITICAL_SECTION surfaces_cs;
static RTL_CRITICAL_SECTION_DEBUG surfaces_cs_debug =
{
    0, 0, &surfaces_cs,
    { &surfaces_cs_debug.ProcessLocksList, &surfaces_cs_debug.ProcessLocksList },
    0, 0, { (DWORD_PTR)(__FILE__ ": surfaces_cs") }
};
static RTL_CRITICAL_SECTION surfaces_cs = { &surfaces_cs_debug, -1, 0, 0, 0, 0 };

static int hwnd_surface_compare( const void *key, const struct wine_rb_entry *entry )
{
    HWND hwnd = (HWND)key;
    return (LONG_PTR)hwnd - (LONG_PTR)WINE_RB_ENTRY_VALUE( entry, struct hwnd_surface, entry )->hwnd;
}
static struct wine_rb_tree toplevel_surfaces = { hwnd_surface_compare, NULL };

static struct hwnd_surface *toplevel_surface_create( HWND hwnd )
{
    struct hwnd_surface *toplevel;

    if (!(toplevel = malloc(sizeof(*toplevel)))) return NULL;
    toplevel->hwnd = hwnd;
    toplevel->unix_surface = unix_funcs->surface_create_toplevel( hwnd );

    TRACE( "created toplevel surface %p for hwnd %p.\n", toplevel, hwnd );

    return toplevel;
}

static void hwnd_surface_delete( struct hwnd_surface *surface )
{
    TRACE( "surface %p.\n", surface );

    unix_funcs->surface_delete( surface->unix_surface );

    free(surface);
}

static void set_toplevel_surface_for_hwnd( HWND hwnd, struct hwnd_surface *toplevel )
{
    RtlEnterCriticalSection( &surfaces_cs );
    wine_rb_put( &toplevel_surfaces, hwnd, &toplevel->entry );
    RtlLeaveCriticalSection( &surfaces_cs );
}

static struct hwnd_surface *get_toplevel_surface_for_hwnd( HWND hwnd )
{
    struct wine_rb_entry *entry;
    struct hwnd_surface *toplevel;
    RtlEnterCriticalSection( &surfaces_cs );
    if (!(entry = wine_rb_get( &toplevel_surfaces, hwnd ))) toplevel = NULL;
    else toplevel = WINE_RB_ENTRY_VALUE( entry, struct hwnd_surface, entry );
    RtlLeaveCriticalSection( &surfaces_cs );
    return toplevel;
}

void win32u_create_toplevel_surface( HWND hwnd )
{
    struct hwnd_surface *toplevel;

    TRACE( "hwnd %p.\n", hwnd );

    RtlEnterCriticalSection( &surfaces_cs );
    if (!(toplevel = get_toplevel_surface_for_hwnd( hwnd )))
    {
        toplevel = toplevel_surface_create( hwnd );
        set_toplevel_surface_for_hwnd( hwnd, toplevel );
    }
    RtlLeaveCriticalSection( &surfaces_cs );
}

void win32u_delete_toplevel_surface( HWND hwnd )
{
    struct hwnd_surface *toplevel;

    TRACE( "hwnd %p.\n", hwnd );

    RtlEnterCriticalSection( &surfaces_cs );
    if ((toplevel = get_toplevel_surface_for_hwnd( hwnd )))
    {
        wine_rb_remove( &toplevel_surfaces, &toplevel->entry );
        hwnd_surface_delete( toplevel );
    }
    RtlLeaveCriticalSection( &surfaces_cs );
}
