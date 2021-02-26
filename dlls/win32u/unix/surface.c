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

#include "config.h"
#include "wine/port.h"

#include <limits.h>

#include "wine/rbtree.h"

#include "x11.h"
#include "x11drv.h"

WINE_DEFAULT_DEBUG_CHANNEL(win32u);

struct unix_surface
{
    HWND hwnd;
    cairo_surface_t *cairo_surface;
};

static struct unix_surface *cairo_surface_create( HWND hwnd )
{
    struct unix_surface *surface;

    if (!(surface = malloc(sizeof(*surface))))
    {
        ERR( "failed to allocate unix surface for hwnd %p\n", hwnd );
        return NULL;
    }

    surface->hwnd = hwnd;
    surface->cairo_surface = NULL;

    TRACE( "created surface %p.\n", surface );
    return surface;
}

struct unix_surface *CDECL cairo_surface_create_toplevel( HWND hwnd )
{
    TRACE( "hwnd %p.\n", hwnd );

    return cairo_surface_create( hwnd );
}

void CDECL cairo_surface_create_notify( struct unix_surface *surface, LPARAM param )
{
    XWindowAttributes attr;
    Display *display;
    Window window = (Window)param;
    HWND hwnd = surface->hwnd;

    TRACE( "surface %p, param %lx.\n", surface, param );

    if (!(display = x11drv_thread_data()->display))
    {
        ERR( "failed to get X11 thread display for window %lx\n", window );
        return;
    }

    if (!pXGetWindowAttributes( display, window, &attr ))
    {
        ERR( "XGetWindowAttributes failed for window %lx\n", window );
        return;
    }

    if (surface->cairo_surface && p_cairo_xlib_surface_get_drawable( surface->cairo_surface ) == window)
        return;

    if (surface->cairo_surface) p_cairo_surface_destroy( surface->cairo_surface );
    surface->cairo_surface = p_cairo_xlib_surface_create( display, window, attr.visual, attr.width, attr.height );

    TRACE( "updated surface %p, hwnd %p, window %lx.\n", surface, hwnd, window );
}

void CDECL cairo_surface_delete( struct unix_surface *surface )
{
    TRACE( "surface %p.\n", surface );

    if (surface->cairo_surface) p_cairo_surface_destroy( surface->cairo_surface );
    free(surface);
}
