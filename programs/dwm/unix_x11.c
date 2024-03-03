/*
 * Copyright 2022 Rémi Bernon for CodeWeavers
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

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <stdarg.h>
#include <stddef.h>

#include <unistd.h>
#include <poll.h>

#include <cairo/cairo.h>
#include <cairo/cairo-xcb.h>

#ifdef HAVE_XCB_XCB_H
#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_event.h>
#endif

#include "unix_private.h"

#include "wine/server.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dwmsrv);

struct x11_display
{
    struct display base;
    xcb_connection_t *xcb;
    int screen_num;
    xcb_window_t root;
};

struct x11_window
{
    struct window base;
    xcb_window_t id;
    cairo_surface_t *surface;
};

static xcb_visualtype_t *xcb_visualtype_from_id(xcb_connection_t *xcb, int screen_num, xcb_visualid_t id)
{
    xcb_visualtype_iterator_t visual;
    xcb_depth_iterator_t depth;
    xcb_screen_t *screen;

    if (!(screen = xcb_aux_get_screen( xcb, screen_num ))) return NULL;

    for (depth = xcb_screen_allowed_depths_iterator(screen); depth.rem; xcb_depth_next(&depth))
        for (visual = xcb_depth_visuals_iterator(depth.data); visual.rem; xcb_visualtype_next(&visual))
            if (screen->root_visual == visual.data->visual_id)
                return visual.data;

    return NULL;
}

static void handle_xcb_generic_error( xcb_generic_error_t *error )
{
    const char *label = xcb_event_get_error_label( error->error_code );
    ERR( "got error %s sequence %u resource %#x major %u minor %u\n", debugstr_a(label),
         error->sequence, error->resource_id, error->minor_code, error->major_code );
}

static void x11_dwm_display_poll( struct display *iface, int req_fd )
{
    struct x11_display *display = CONTAINING_RECORD(iface, struct x11_display, base);

    for (;;)
    {
        struct pollfd fds[2] =
        {
            {.fd = req_fd, .events = POLLIN},
            {.fd = xcb_get_file_descriptor( display->xcb ), .events = POLLIN | POLLOUT},
        };

        while (poll( fds, ARRAY_SIZE(fds), -1) <= 0) {}

        if (fds[1].revents & POLLOUT) xcb_flush( display->xcb );
        if (fds[1].revents & POLLIN)
        {
            xcb_generic_event_t *event;
            while ((event = xcb_poll_for_event( display->xcb )))
            {
                ERR( "event %p type %u\n", event, event->response_type );
                if (!event->response_type) handle_xcb_generic_error( (xcb_generic_error_t *)event );
                free( event );
            }
        }

        if (fds[0].revents & POLLIN) break;
    }
}

static void x11_dwm_display_destroy( struct display *iface )
{
    struct x11_display *display = CONTAINING_RECORD(iface, struct x11_display, base);
    xcb_disconnect( display->xcb );
    free( display );
}

static struct window *x11_dwm_window_create( struct display *iface, HWND hwnd, UINT64 native )
{
    struct x11_display *display = CONTAINING_RECORD(iface, struct x11_display, base);
    xcb_get_window_attributes_reply_t *attrs;
    struct x11_window *window;
    xcb_void_cookie_t cookie;

    if (!(window = calloc( 1, sizeof(*window) ))) return NULL;
    window->base.display = display_acquire( iface );
    window->base.ref = 1;
    pthread_mutex_init( &window->base.lock, NULL );
    window->id = native;

    if ((attrs = xcb_get_window_attributes_reply( display->xcb, xcb_get_window_attributes( display->xcb, window->id ), NULL )))
    {
        xcb_visualtype_t *visual_type = xcb_visualtype_from_id( display->xcb, display->screen_num, attrs->visual );
        window->surface = cairo_xcb_surface_create( display->xcb, window->id, visual_type, 1, 1 );
        free( attrs );
    }

if (0)
{
    window->id = xcb_generate_id( display->xcb );

    TRACE("\n");

    cookie = xcb_create_window( display->xcb, XCB_COPY_FROM_PARENT, window->id, display->root, 0, 0, 100, 100, 0,
                                XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, NULL );
    if (!ERR_ON(dwmsrv)) xcb_discard_reply( display->xcb, cookie.sequence );

    cookie = xcb_map_window( display->xcb, window->id );
    if (!ERR_ON(dwmsrv)) xcb_discard_reply( display->xcb, cookie.sequence );
}

    return &window->base;
}

static void x11_dwm_window_update( struct window *iface, const RECT *window_rect )
{
    struct x11_display *display = CONTAINING_RECORD(iface->display, struct x11_display, base);
    struct x11_window *window = CONTAINING_RECORD(iface, struct x11_window, base);
    const uint32_t values[] =
    {
        window_rect->left,
        window_rect->top,
        max(1, window_rect->right - window_rect->left),
        max(1, window_rect->bottom - window_rect->top),
    };
    xcb_void_cookie_t cookie;
    uint16_t mask = 0;

    mask |= XCB_CONFIG_WINDOW_X;
    mask |= XCB_CONFIG_WINDOW_Y;
    mask |= XCB_CONFIG_WINDOW_WIDTH;
    mask |= XCB_CONFIG_WINDOW_HEIGHT;

    TRACE("\n");

    cookie = xcb_configure_window( display->xcb, window->id, mask, values );
    if (!ERR_ON(dwmsrv)) xcb_discard_reply( display->xcb, cookie.sequence );
}

static void x11_dwm_window_destroy( struct window *iface )
{
    struct x11_display *display = CONTAINING_RECORD(iface->display, struct x11_display, base);
    struct x11_window *window = CONTAINING_RECORD(iface, struct x11_window, base);
    xcb_void_cookie_t cookie;

if (0)
{
    cookie = xcb_destroy_window( display->xcb, window->id );
    if (!ERR_ON(dwmsrv)) xcb_discard_reply( display->xcb, cookie.sequence );
}

    pthread_mutex_destroy( &window->base.lock );
    cairo_surface_destroy(window->surface);
    free( window );
}

static cairo_surface_t *x11_dwm_window_surface( struct window *iface, const RECT *rect )
{
    struct x11_window *window = CONTAINING_RECORD(iface, struct x11_window, base);
    cairo_xcb_surface_set_size( window->surface, rect->right - rect->left, rect->bottom - rect->top );
    return cairo_surface_reference( window->surface );
}

static const struct display_ops dwm_display_ops_x11 =
{
    x11_dwm_display_poll,
    x11_dwm_display_destroy,
    x11_dwm_window_create,
    x11_dwm_window_update,
    x11_dwm_window_destroy,
    x11_dwm_window_surface,
};

struct display *x11_display_create( const char *name )
{
    struct x11_display *display;
    xcb_screen_t *screen;

    TRACE( "name %s\n", debugstr_a(name) );

    if (!(display = calloc( 1, sizeof(*display) ))) return NULL;
    display->base.ops = &dwm_display_ops_x11;
    display->base.ref = 1;

    display->xcb = xcb_connect( *name ? name : "localhost:10.0", &display->screen_num );
    if (xcb_connection_has_error( display->xcb )) goto failed;
    if (!(screen = xcb_aux_get_screen( display->xcb, display->screen_num ))) goto failed;
    display->root = screen->root;

    TRACE( "display %p connected to xcb %p (%s)\n", display, display->xcb, debugstr_a(name) );
    return &display->base;

failed:
    xcb_disconnect( display->xcb );
    free( display );
    return NULL;
}
