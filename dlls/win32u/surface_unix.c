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
#include <fenv.h>

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

void CDECL cairo_surface_create_xcb( struct unix_surface *surface, LPARAM param )
{
    xcb_get_window_attributes_cookie_t attr_cookie;
    xcb_get_window_attributes_reply_t *attrs;
    xcb_get_geometry_cookie_t geom_cookie;
    xcb_get_geometry_reply_t *geom;
    xcb_window_t window = param;
    HWND hwnd = surface->hwnd;

    attr_cookie = p_xcb_get_window_attributes( xcb, window );
    geom_cookie = p_xcb_get_geometry( xcb, window );

    if (!(attrs = p_xcb_get_window_attributes_reply(xcb, attr_cookie, NULL)))
    {
        ERR( "failed to get attributes for window %x\n", window );
        return;
    }

    if (!(geom = p_xcb_get_geometry_reply(xcb, geom_cookie, NULL)))
    {
        ERR( "failed to get attributes for window %x\n", window );
        return;
    }

    if (surface->cairo_surface) p_cairo_surface_destroy( surface->cairo_surface );
    surface->cairo_surface = p_cairo_xcb_surface_create( xcb, window, xcb_visualtype_from_id( xcb, 0, attrs->visual ), geom->width, geom->height );

    TRACE( "updated surface %p, hwnd %p, window %x.\n", surface, hwnd, window );

    free( geom );
    free( attrs );
}
#endif

struct unix_surface *CDECL cairo_surface_create_foreign( HWND hwnd, LPARAM id )
{
    struct unix_surface *surface;

    TRACE( "hwnd %p, id %lx.\n", hwnd, id );

    if ((surface = cairo_surface_create( hwnd )))
#ifdef HAVE_XCB_XCB_H
        cairo_surface_create_xcb( surface, id );
#endif
    return surface;
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
    fenv_t fpu_env;
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

    feholdexcept(&fpu_env);
    fesetenv(FE_DFL_ENV);
    feenableexcept(FE_DIVBYZERO|FE_INVALID);

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
    p_cairo_surface_flush( source->cairo_surface );
    p_xcb_flush(xcb);

    feclearexcept(FE_ALL_EXCEPT);
    feupdateenv(&fpu_env);
}
