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

struct unix_surface *CDECL cairo_surface_create_drawable( struct unix_surface *target, BITMAP *bitmap )
{
    struct unix_surface *surface;

    TRACE( "target %p, bitmap %p.\n", target, bitmap );

    if (!(surface = cairo_surface_create( target->hwnd ))) return NULL;

    if (!target->cairo_surface || !bitmap->bmWidth || !bitmap->bmHeight) memset( bitmap, 0, sizeof(*bitmap) );
    else
    {
        surface->cairo_surface = p_cairo_surface_create_similar_image( target->cairo_surface, CAIRO_FORMAT_RGB24, bitmap->bmWidth, bitmap->bmHeight );

        bitmap->bmType = 0;
        bitmap->bmWidth = p_cairo_image_surface_get_width( surface->cairo_surface );
        bitmap->bmHeight = p_cairo_image_surface_get_height( surface->cairo_surface );
        bitmap->bmWidthBytes = p_cairo_image_surface_get_stride( surface->cairo_surface );
        bitmap->bmPlanes = 1;
        bitmap->bmBitsPixel = 32;
        bitmap->bmBits = p_cairo_image_surface_get_data( surface->cairo_surface );
    }

    return surface;
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

void CDECL cairo_surface_present( struct unix_surface *target, struct unix_surface *source, const POINT *target_pos, const RECT *source_rect, UINT clip_rect_count, const RECT *clip_rects )
{
    cairo_t *cr;
    POINT *source_pos = (POINT *)source_rect;
    UINT i, width, height;

    TRACE( "target %p, source %p, pos %s, source_rect %s, clip_rect_count %u, clip_rects %p.\n",
           target, source, wine_dbgstr_point( target_pos ), wine_dbgstr_rect( source_rect ),
           clip_rect_count, clip_rects );

    if (!target->cairo_surface)
    {
        ERR( "no target surface to present to!\n" );
        return;
    }

    cr = p_cairo_create( target->cairo_surface );
    for (i = 0; i < clip_rect_count; ++i)
        p_cairo_rectangle( cr, clip_rects[i].left, clip_rects[i].top,
                           clip_rects[i].right - clip_rects[i].left,
                           clip_rects[i].bottom - clip_rects[i].top );
    if (clip_rect_count) p_cairo_clip( cr );

    width = source_rect->right - source_rect->left;
    height = source_rect->bottom - source_rect->top;

    if (!source->cairo_surface) p_cairo_set_source_rgba( cr, 1., 0., 0., 1. );
    else
    {
        p_cairo_surface_mark_dirty_rectangle( source->cairo_surface, source_pos->x, source_pos->y,
                                              width, height );
        p_cairo_set_source_surface( cr, source->cairo_surface, target_pos->x - source_pos->x, target_pos->y - source_pos->y );
    }
    p_cairo_rectangle( cr, target_pos->x, target_pos->y, width, height );
    p_cairo_fill( cr );
    p_cairo_destroy( cr );

    p_cairo_surface_flush( target->cairo_surface );
}

void CDECL cairo_surface_resize_notify( struct unix_surface *surface, const RECT *rect )
{
    TRACE( "surface %p, rect %s stub!\n", surface, wine_dbgstr_rect( rect ) );

    if (!surface->cairo_surface) return;

    p_cairo_xlib_surface_set_size( surface->cairo_surface, rect->right - rect->left, rect->bottom - rect->top );
}
