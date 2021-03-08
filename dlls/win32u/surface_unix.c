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
#ifdef HAVE_XCB_XCB_H
    xcb_window_t window;
    xcb_window_t pixmap;
#endif
    cairo_surface_t *cairo_surface;
#ifdef CAIRO_DEBUG
    cairo_surface_t *debug_surface;
#endif
    RECT position;
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
#ifdef HAVE_XCB_XCB_H
    surface->window = XCB_NONE;
    surface->pixmap = XCB_NONE;
#endif
    surface->cairo_surface = NULL;
#ifdef CAIRO_DEBUG
    surface->debug_surface = NULL;
#endif
    memset(&surface->position, 0, sizeof(surface->position));
    surface->parent = NULL;

    TRACE( "created surface %p.\n", surface );
    return surface;
}

struct unix_surface *CDECL cairo_surface_create_toplevel( HWND hwnd )
{
    TRACE( "hwnd %p.\n", hwnd );

    return cairo_surface_create( hwnd );
}

struct unix_surface *CDECL cairo_surface_create_client( HWND hwnd )
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

static xcb_window_t xcb_root_window(xcb_connection_t *xcb, int screen_id)
{
    xcb_screen_t *screen = xcb_screen_from_id(xcb, screen_id);
    if (!screen) return XCB_NONE;
    return screen->root;
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

    TRACE( "surface %p, param %lx.\n", surface, param );

    if (!param)
    {
        if (surface->cairo_surface) p_cairo_surface_destroy( surface->cairo_surface );
        surface->cairo_surface = NULL;
        surface->window = XCB_NONE;
        surface->pixmap = XCB_NONE;
        return;
    }

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
    surface->window = window;
    surface->pixmap = XCB_NONE;

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

#ifdef HAVE_XCB_PRESENT_H
static void cairo_surface_present_xcb_present( xcb_window_t target, xcb_window_t source, const POINT *target_pos, const RECT *source_rect, UINT clip_rect_count, const RECT *clip_rects )
{
    xcb_generic_error_t *err;
    xcb_xfixes_region_t valid = p_xcb_generate_id(xcb), update = p_xcb_generate_id(xcb);
    xcb_rectangle_t valid_rect, *update_rects;
    int i;

    TRACE( "target %x, source %x, pos %s, source_rect %s, clip_rect_count %u, clip_rects %p.\n",
           target, source, wine_dbgstr_point( target_pos ), wine_dbgstr_rect( source_rect ),
           clip_rect_count, clip_rects );

    if (!(update_rects = calloc(clip_rect_count, sizeof(*update_rects))))
    {
        ERR("failed to allocate clip rects\n");
        return;
    }

    valid_rect.x = source_rect->left;
    valid_rect.y = source_rect->top;
    valid_rect.width = source_rect->right - source_rect->left;
    valid_rect.height = source_rect->bottom - source_rect->top;

    for (i = 0; i < clip_rect_count; ++i)
    {
        update_rects[i].x = clip_rects[i].left - target_pos->x;
        update_rects[i].y = clip_rects[i].top - target_pos->y;
        update_rects[i].width = clip_rects[i].right - clip_rects[i].left;
        update_rects[i].height = clip_rects[i].bottom - clip_rects[i].top;
    }

    if ((err = p_xcb_request_check(xcb, p_xcb_xfixes_create_region_checked(xcb, valid, 1, &valid_rect ))))
    {
        ERR("failed to create region, error %d, resource %x, minor %d, major %d \n", err->error_code, err->resource_id, err->minor_code, err->major_code );
        free(err);
    }

    if ((err = p_xcb_request_check(xcb, p_xcb_xfixes_create_region_checked(xcb, update, clip_rect_count, update_rects ))))
    {
        ERR("failed to create region, error %d, resource %x, minor %d, major %d \n", err->error_code, err->resource_id, err->minor_code, err->major_code );
        free(err);
    }

    if ((err = p_xcb_request_check(xcb, p_xcb_present_pixmap_checked(xcb, target, source, 0, valid, update,
                  target_pos->x - source_rect->left, target_pos->y - source_rect->top,
                  XCB_NONE, XCB_NONE, XCB_NONE, 0, 0, 0, 0, 0, NULL))))
    {
        ERR("failed to present, error %d, resource %x, minor %d, major %d \n", err->error_code, err->resource_id, err->minor_code, err->major_code );
        free(err);
    }

    if ((err = p_xcb_request_check(xcb, p_xcb_xfixes_destroy_region_checked(xcb, update ))))
    {
        ERR("failed to destroy region, error %d, resource %x, minor %d, major %d \n", err->error_code, err->resource_id, err->minor_code, err->major_code );
        free(err);
    }

    if ((err = p_xcb_request_check(xcb, p_xcb_xfixes_destroy_region_checked(xcb, valid ))))
    {
        ERR("failed to destroy region, error %d, resource %x, minor %d, major %d \n", err->error_code, err->resource_id, err->minor_code, err->major_code );
        free(err);
    }

    if (update_rects) free(update_rects);
}
#endif

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

#ifdef HAVE_XCB_PRESENT_H
    if (target->window && source->pixmap)
    {
        cairo_surface_present_xcb_present( target->window, source->pixmap, target_pos, source_rect, clip_rect_count, clip_rects );
        return;
    }
#endif

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
    if (source->cairo_surface) p_cairo_surface_flush( source->cairo_surface );
    p_xcb_flush(xcb);

    feclearexcept(FE_ALL_EXCEPT);
    feupdateenv(&fpu_env);
}

extern POINT virtual_screen_to_root( INT x, INT y ) DECLSPEC_HIDDEN;

static void CDECL cairo_surface_apply_resize( struct unix_surface *surface, struct unix_surface *new_parent, const RECT *rect, BOOL notify )
{
    struct unix_surface *old_parent;
    xcb_generic_error_t *err;
    xcb_window_t new_parent_window, old_parent_window;
    uint32_t values[4] = {0, 0, 0, 0}, i = 0;
    uint16_t mask = 0;
    POINT pos = { rect->left, rect->top };
    RECT old_pos, new_pos = *rect;

    if (new_parent && !new_parent->cairo_surface) return;

    old_parent = surface->parent;
    surface->parent = new_parent;

    new_pos.right = new_pos.left + min( max( 1, new_pos.right - new_pos.left ), 65535 );
    new_pos.bottom = new_pos.top + min( max( 1, new_pos.bottom - new_pos.top ), 65535 );

    if (!new_parent) pos = virtual_screen_to_root( pos.x, pos.y );
    OffsetRect( &new_pos, pos.x - new_pos.left, pos.y - new_pos.top );

    old_pos = surface->position;
    surface->position = new_pos;

    if (!surface->cairo_surface) return;

    if (!notify)
    {
        old_parent_window = old_parent ? old_parent->window : xcb_root_window(xcb, 0);
        new_parent_window = new_parent ? new_parent->window : xcb_root_window(xcb, 0);

        if (old_parent_window != new_parent_window)
        {
            if ((err = p_xcb_request_check( xcb,  p_xcb_reparent_window_checked( xcb, surface->window, new_parent->window, new_pos.left, new_pos.top ) )))
            {
                ERR("failed to reparent window, error %d, resource %x, minor %d, major %d \n", err->error_code, err->resource_id, err->minor_code, err->major_code );
                free(err);
            }
            else
                OffsetRect( &old_pos, new_pos.left - old_pos.left, new_pos.top - old_pos.top );
        }

        if (old_pos.left != new_pos.left)
        {
            values[i++] = new_pos.left;
            mask |= XCB_CONFIG_WINDOW_X;
        }
        if (old_pos.top != new_pos.top)
        {
            values[i++] = new_pos.top;
            mask |= XCB_CONFIG_WINDOW_Y;
        }
        if ((old_pos.right - old_pos.left) != (new_pos.right - new_pos.left))
        {
            values[i++] = (new_pos.right - new_pos.left);
            mask |= XCB_CONFIG_WINDOW_WIDTH;
        }
        if ((old_pos.bottom - old_pos.top) != (new_pos.bottom - new_pos.top))
        {
            values[i++] = (new_pos.bottom - new_pos.top);
            mask |= XCB_CONFIG_WINDOW_HEIGHT;
        }
        if (mask)
        {
            if ((err = p_xcb_request_check( xcb,  p_xcb_configure_window_checked( xcb, surface->window, mask, values ) )))
            {
                ERR("failed to reconfigure window, error %d, resource %x, minor %d, major %d \n", err->error_code, err->resource_id, err->minor_code, err->major_code );
                free(err);
            }
        }
    }

#ifdef HAVE_XCB_XCB_H
    p_cairo_xcb_surface_set_size( surface->cairo_surface, new_pos.right - new_pos.left, new_pos.bottom - new_pos.top );
#else
#error
#endif

#ifdef CAIRO_DEBUG
    if (surface->debug_surface) p_cairo_surface_destroy( surface->debug_surface );
    surface->debug_surface = p_cairo_surface_create_similar_image( surface->cairo_surface, CAIRO_FORMAT_RGB24, new_pos.right - new_pos.left, new_pos.bottom - new_pos.top );
#endif

    if (surface->pixmap)
    {
        surface->pixmap = p_xcb_generate_id(xcb);
        if ((err = p_xcb_request_check( xcb, p_xcb_composite_name_window_pixmap_checked(xcb, surface->window, surface->pixmap))))
        {
            ERR("failed to name window %x pixmap %x, error %d, resource %x, minor %d, major %d \n", surface->window, surface->pixmap, err->error_code, err->resource_id, err->minor_code, err->major_code );
            free(err);
        }
    }
}

void CDECL cairo_surface_resize( struct unix_surface *surface, struct unix_surface *parent, const RECT *rect )
{
    TRACE( "surface %p, parent %p, rect %s stub!\n", surface, parent, wine_dbgstr_rect( rect ) );

    cairo_surface_apply_resize( surface, parent, rect, FALSE );
}

void CDECL cairo_surface_resize_notify( struct unix_surface *surface, struct unix_surface *parent, const RECT *rect )
{
    TRACE( "surface %p, parent %p, rect %s stub!\n", surface, parent, wine_dbgstr_rect( rect ) );

    cairo_surface_apply_resize( surface, parent, rect, TRUE );
}

void CDECL cairo_surface_set_offscreen( struct unix_surface *surface, BOOL offscreen )
{
    xcb_generic_error_t *err;
    xcb_void_cookie_t cookie;

    TRACE( "surface %p, offscreen %d.\n", surface, offscreen );

    if (!surface->cairo_surface) return;

    if (offscreen) cookie = p_xcb_composite_redirect_window_checked( xcb, surface->window, XCB_COMPOSITE_REDIRECT_MANUAL );
    else cookie = p_xcb_composite_unredirect_window_checked( xcb, surface->window, XCB_COMPOSITE_REDIRECT_MANUAL );
    if ((err = p_xcb_request_check( xcb, cookie )))
    {
        ERR("failed to %sset windows %x offscreen, error %d, resource %x, minor %d, major %d \n", offscreen ? "" : "un", surface->window, err->error_code, err->resource_id, err->minor_code, err->major_code );
        free(err);
    }

    if (!offscreen) surface->pixmap = XCB_NONE;
    else
    {
        surface->pixmap = p_xcb_generate_id(xcb);
        if ((err = p_xcb_request_check( xcb, p_xcb_composite_name_window_pixmap_checked(xcb, surface->window, surface->pixmap))))
        {
            ERR("failed to name window %x pixmap %x, error %d, resource %x, minor %d, major %d \n", surface->window, surface->pixmap, err->error_code, err->resource_id, err->minor_code, err->major_code );
            free(err);
        }
    }
}
