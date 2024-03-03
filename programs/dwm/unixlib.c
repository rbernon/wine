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

#include <stdarg.h>
#include <stddef.h>

#include <unistd.h>
#include <cairo/cairo.h>
#include <cairo/cairo-tee.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "wingdi.h"
#include "ntuser.h"
#include "ntgdi.h"

#include "unixlib.h"
#include "unix_private.h"

#include "wine/dwmapi.h"
#include "wine/server.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dwmsrv);

static cairo_rectangle_t virtual_screen_rect;
static pthread_mutex_t framebuffer_lock = PTHREAD_MUTEX_INITIALIZER;
static cairo_surface_t *framebuffer_image;
static cairo_surface_t *framebuffer;

static cairo_rectangle_t rect_to_cairo_rectangle( RECT rect )
{
    cairo_rectangle_t rectangle = {rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top};
    return rectangle;
}

static void paint_framebuffer( cairo_surface_t *surface )
{
    cairo_rectangle_t rect = virtual_screen_rect;
    cairo_t *cairo = cairo_create( surface );
    cairo_set_source_rgb( cairo, 0, 0, 0 );
    cairo_paint( cairo );
    cairo_set_source_rgb( cairo, 1, 0, 0 );
    cairo_rectangle( cairo, rect.x + 10, rect.y + 10, rect.width - 20, rect.height - 20 );
    cairo_stroke( cairo );
    cairo_destroy( cairo );
}


void add_framebuffer_output( cairo_surface_t *surface )
{
    pthread_mutex_lock( &framebuffer_lock );
    cairo_tee_surface_add( framebuffer, surface );
    paint_framebuffer( framebuffer );
    pthread_mutex_unlock( &framebuffer_lock );
}

static NTSTATUS dwm_init( void *args )
{
    cairo_rectangle_t rect;

    setenv( "WINENODWM", "1", TRUE );
    virtual_screen_rect = rect = rect_to_cairo_rectangle( NtUserGetVirtualScreenRect() );
    framebuffer_image = cairo_image_surface_create( CAIRO_FORMAT_RGB24, rect.width, rect.height );
    framebuffer = cairo_tee_surface_create( framebuffer_image );
    paint_framebuffer( framebuffer );
    return 0;
}

static NTSTATUS dwm_client_loop( void *args )
{
    struct dwm_client_loop_params *params = args;
    int req_fd, req_size, reply_fd, reply_size;
    UINT status;

    if ((status = wine_server_handle_to_fd( params->request, GENERIC_READ, &req_fd, NULL )))
        return status;
    if ((status = wine_server_handle_to_fd( params->reply, GENERIC_WRITE, &reply_fd, NULL )))
    {
        close( req_fd );
        return status;
    }

    for (;;)
    {
        union dwm_request req;
        union dwm_reply reply;

        if ((req_size = read( req_fd, &req, sizeof(req) )) < sizeof(req.header) || req_size < req.header.req_size)
        {
            ERR( "failed to read request, read %d, size %u\n", req_size, req.header.req_size );
            break;
        }

        switch (req.header.type)
        {
        case DWM_REQ_CONNECT:
            TRACE("DWM_REQ_CONNECT\n");
            if (req_size < sizeof(req.connect)) status = STATUS_BUFFER_TOO_SMALL;
            else if (req.connect.version != DWM_PROTOCOL_VERSION)
            {
                ERR( "client version mismatch, version %u != %u\n", req.connect.version, DWM_PROTOCOL_VERSION );
                status = STATUS_INVALID_PARAMETER;
            }
            break;

        case DWM_REQ_SURFACE_FLUSH:
            TRACE("DWM_REQ_SURFACE_FLUSH\n");
            if (req_size < sizeof(req.surface_flush)) status = STATUS_BUFFER_TOO_SMALL;
            else
            {
                UINT width, height, stride, size, i;
                cairo_surface_t *surface;
                RECT new_rect, old_rect;
                BYTE *buffer, *line;

                new_rect = req.surface_flush.new_window_rect;
                old_rect = req.surface_flush.old_window_rect;
                OffsetRect( &new_rect, -virtual_screen_rect.x, -virtual_screen_rect.y );
                OffsetRect( &old_rect, -virtual_screen_rect.x, -virtual_screen_rect.y );

                if (!EqualRect( &old_rect, &new_rect ))
                {
                    UINT old_width = old_rect.right - old_rect.left, old_height = old_rect.bottom - old_rect.top;
                    cairo_surface_t *surface = cairo_image_surface_create( CAIRO_FORMAT_RGB24, old_width, old_height );
                    cairo_t *cairo;

                    width = new_rect.right - new_rect.left;
                    height = new_rect.bottom - new_rect.top;

                    pthread_mutex_lock( &framebuffer_lock );
                    {
                        cairo = cairo_create( surface );
                        cairo_set_source_surface( cairo, framebuffer_image, -old_rect.left, -old_rect.top );
                        cairo_rectangle( cairo, 0, 0, old_width, old_height );
                        cairo_fill( cairo );
                        cairo_destroy( cairo );


                        cairo = cairo_create( framebuffer );
                        cairo_set_source_surface( cairo, surface, new_rect.left, new_rect.top );
                        cairo_rectangle( cairo, new_rect.left, new_rect.top, width, height );
                        cairo_fill( cairo );
                        cairo_destroy( cairo );
                    }
                    pthread_mutex_unlock( &framebuffer_lock );

                    cairo_surface_destroy( surface );
                }

                width = req.surface_flush.rect.right - req.surface_flush.rect.left;
                height = req.surface_flush.rect.bottom - req.surface_flush.rect.top;
                stride = req.surface_flush.stride;

                if ((size = height * stride) && (buffer = malloc( size )))
                {
                    for (i = 0, line = buffer; i < height; i++, line += stride)
                    {
                        if ((size = read( req_fd, line, stride )) != stride)
                        {
                            ERR( "failed to read request, read %d, size %u\n", size, stride );
                            break;
                        }
                    }

                    surface = cairo_image_surface_create_for_data( buffer, CAIRO_FORMAT_RGB24, width, height, stride);
                    OffsetRect( &new_rect, req.surface_flush.rect.left, req.surface_flush.rect.top );

                    pthread_mutex_lock( &framebuffer_lock );
                    {
                        cairo_t *cairo = cairo_create( framebuffer );
                        cairo_set_source_surface( cairo, surface, new_rect.left, new_rect.top );
                        cairo_rectangle( cairo, new_rect.left, new_rect.top, width, height );
                        cairo_fill( cairo );
                        cairo_destroy( cairo );
                    }
                    pthread_mutex_unlock( &framebuffer_lock );

                    cairo_surface_destroy( surface );
                    free( buffer );
                }
            }
            continue; /* no reply */

        default:
            FIXME( "not implemented %u\n", req.header.type );
            status = STATUS_NOT_IMPLEMENTED;
            break;
        }

        if ((reply.header.status = status)) ERR( "got status %#x\n", status );
        if ((reply_size = write( reply_fd, &reply, sizeof(reply) )) != sizeof(reply))
        {
            ERR( "failed to write reply, written %u, size %zu\n", reply_size, sizeof(reply) );
            break;
        }
    }

    close( req_fd );
    close( reply_fd );
    return 0;
}

static NTSTATUS dwm_debug_loop( void *args )
{
    const char *env = getenv( "DISPLAY" );
    struct display *display = x11_display_create( env ? env : "" );
    for (;;) display->ops->poll( display, 0 );
    display_release( display );
    return 0;
}

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    dwm_init,
    dwm_client_loop,
    dwm_debug_loop,
};

C_ASSERT(ARRAY_SIZE(__wine_unix_call_funcs) == unix_dwm_funcs_count);
