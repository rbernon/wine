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

#define CAIRO_DEBUG

struct unix_surface
{
    HWND hwnd;
    cairo_surface_t *cairo_surface;
#ifdef CAIRO_DEBUG
    cairo_surface_t *debug_surface;
#endif
    struct unix_surface *parent;
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
#ifdef CAIRO_DEBUG
    surface->debug_surface = NULL;
#endif
    surface->parent = NULL;

    TRACE( "created surface %p.\n", surface );
    return surface;
}

struct unix_surface *CDECL cairo_surface_create_toplevel( HWND hwnd )
{
    TRACE( "hwnd %p.\n", hwnd );

    return cairo_surface_create( hwnd );
}

struct unix_surface *CDECL cairo_surface_create_foreign( HWND hwnd )
{
    TRACE( "hwnd %p.\n", hwnd );

    return cairo_surface_create( hwnd );
}

struct unix_surface *CDECL cairo_surface_create_client( HWND hwnd, LPARAM *id )
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

#ifdef CAIRO_DEBUG
        if (surface->debug_surface) p_cairo_surface_destroy( surface->debug_surface );
        surface->debug_surface = NULL;
#endif
        return;
    }

    if (!(geom = p_xcb_get_geometry_reply(xcb, geom_cookie, NULL)))
    {
        ERR( "failed to get attributes for window %x\n", window );

#ifdef CAIRO_DEBUG
        if (surface->debug_surface) p_cairo_surface_destroy( surface->debug_surface );
        surface->debug_surface = NULL;
#endif
        return;
    }

    if (surface->cairo_surface) p_cairo_surface_destroy( surface->cairo_surface );
    surface->cairo_surface = p_cairo_xcb_surface_create( xcb, window, xcb_visualtype_from_id( xcb, 0, attrs->visual ), geom->width, geom->height );

    TRACE( "updated surface %p, hwnd %p, window %x.\n", surface, hwnd, window );

    free( geom );
    free( attrs );
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

#ifdef CAIRO_DEBUG
    if (surface->debug_surface) p_cairo_surface_destroy( surface->debug_surface );
    surface->debug_surface = p_cairo_surface_create_similar_image( surface->cairo_surface, CAIRO_FORMAT_RGB24, attr.width, attr.height );
#endif

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

#ifdef CAIRO_DEBUG
    if (surface->debug_surface) p_cairo_surface_destroy( surface->debug_surface );
#endif
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

#ifdef CAIRO_DEBUG
    cr = p_cairo_create( target->debug_surface );
#else
    cr = p_cairo_create( target->cairo_surface );
#endif
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

#ifdef CAIRO_DEBUG
{
    cr = p_cairo_create( target->cairo_surface );
    p_cairo_set_source_surface( cr, target->debug_surface, 0, 0 );
    p_cairo_paint_with_alpha( cr, 0.5 );

    p_cairo_set_source_rgba( cr, 1., 0., 0., 1. );
    for (i = 0; i < clip_rect_count; ++i)
        p_cairo_rectangle( cr, clip_rects[i].left, clip_rects[i].top,
                           clip_rects[i].right - clip_rects[i].left,
                           clip_rects[i].bottom - clip_rects[i].top );
    p_cairo_stroke( cr );

    p_cairo_set_source_rgba( cr, 1., 0., 0., 1. );
    for (i = 0; i < clip_rect_count; ++i)
    {
        p_cairo_move_to( cr, clip_rects[i].left, clip_rects[i].top + 10 );
        p_cairo_text_path( cr, wine_dbgstr_rect( clip_rects + i ) );
    }
    p_cairo_fill( cr );

    p_cairo_set_source_rgba( cr, 0., 1., 0., 1. );
    p_cairo_rectangle( cr, target_pos->x, target_pos->y, width, height );
    p_cairo_stroke( cr );

    for (i = 0; i < clip_rect_count; ++i)
        p_cairo_rectangle( cr, clip_rects[i].left, clip_rects[i].top,
                           clip_rects[i].right - clip_rects[i].left,
                           clip_rects[i].bottom - clip_rects[i].top );
    if (clip_rect_count) p_cairo_clip( cr );

    p_cairo_set_source_rgba( cr, rand() * 1. / RAND_MAX, rand() * 1. / RAND_MAX, rand() * 1. / RAND_MAX, 0.5 );
    p_cairo_rectangle( cr, target_pos->x, target_pos->y, width, height );
    p_cairo_fill( cr );

    p_cairo_destroy( cr );
}
#endif

    p_cairo_surface_flush( target->cairo_surface );
    p_cairo_surface_flush( source->cairo_surface );
    p_xcb_flush(xcb);

    feclearexcept(FE_ALL_EXCEPT);
    feupdateenv(&fpu_env);
}

void CDECL cairo_surface_resize_notify( struct unix_surface *surface, struct unix_surface *parent, const RECT *rect )
{
    int width, height;

    TRACE( "surface %p, parent %p, rect %s stub!\n", surface, parent, wine_dbgstr_rect( rect ) );

    surface->parent = parent;

    width = min( max( 1, rect->right - rect->left ), 65535 );
    height = min( max( 1, rect->bottom - rect->top ), 65535 );

    if (!surface->cairo_surface) return;

#ifdef HAVE_XCB_XCB_H
    p_cairo_xcb_surface_set_size( surface->cairo_surface, width, height );
#else
#error
#endif

#ifdef CAIRO_DEBUG
    if (surface->debug_surface) p_cairo_surface_destroy( surface->debug_surface );
    surface->debug_surface = p_cairo_surface_create_similar_image( surface->cairo_surface, CAIRO_FORMAT_RGB24, rect->right - rect->left, rect->bottom - rect->top );
#endif
}
