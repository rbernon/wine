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

#include "win32u_unix.h"

#include "wine/debug.h"

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

#ifdef HAVE_XCB_XCB_H
static xcb_screen_t *xcb_screen_from_id(xcb_connection_t *xcb, int id)
{
    xcb_screen_iterator_t iter;
    for (iter = p_xcb_setup_roots_iterator(p_xcb_get_setup(xcb)); iter.rem; p_xcb_screen_next(&iter))
        if (!id--) return iter.data;
    return NULL;
}

static xcb_visualtype_t *xcb_visualtype_from_id(xcb_connection_t *xcb, int screen_id, xcb_visualid_t id)
{
    xcb_visualtype_iterator_t visual;
    xcb_depth_iterator_t depth;
    xcb_screen_t *screen;

    screen = xcb_screen_from_id(xcb, screen_id);
    if (!screen) return NULL;

    for (depth = p_xcb_screen_allowed_depths_iterator(screen); depth.rem; p_xcb_depth_next(&depth))
        for (visual = p_xcb_depth_visuals_iterator(depth.data); visual.rem; p_xcb_visualtype_next(&visual))
            if (screen->root_visual == visual.data->visual_id)
                return visual.data;

    return NULL;
}

void CDECL cairo_surface_create_notify_xcb( struct unix_surface *surface, LPARAM param )
{
    xcb_get_window_attributes_reply_t *attrs;
    xcb_window_t window = param;
    HWND hwnd = surface->hwnd;

    if (!(attrs = p_xcb_get_window_attributes_reply(xcb, p_xcb_get_window_attributes( xcb, window ), NULL)))
    {
        ERR( "failed to get attributes for window %x\n", window );
        return;
    }

    if (surface->cairo_surface) p_cairo_surface_destroy( surface->cairo_surface );
    surface->cairo_surface = p_cairo_xcb_surface_create( xcb, window, xcb_visualtype_from_id( xcb, 0, attrs->visual ), 1, 1 );

    TRACE( "updated surface %p, hwnd %p, window %x.\n", surface, hwnd, window );
}
#endif

#if 0
static void CDECL cairo_surface_create_notify_xlib( struct unix_surface *surface, LPARAM param )
{
    XWindowAttributes attr;
    Display *display;
    Window window = (Window)param;
    HWND hwnd = surface->hwnd;

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
#endif

void CDECL cairo_surface_create_notify( struct unix_surface *surface, LPARAM param )
{
    TRACE( "surface %p, param %lx.\n", surface, param );

    if (!param)
    {
        if (surface->cairo_surface) p_cairo_surface_destroy( surface->cairo_surface );
        surface->cairo_surface = NULL;
        return;
    }

#ifdef HAVE_XCB_XCB_H
    cairo_surface_create_notify_xcb( surface, param );
#else
#error
#endif
}

void CDECL cairo_surface_delete( struct unix_surface *surface )
{
    TRACE( "surface %p.\n", surface );

    if (surface->cairo_surface) p_cairo_surface_destroy( surface->cairo_surface );
    free(surface);
}
