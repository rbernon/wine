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

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"

#include "unixlib.h"
#include "unix_private.h"

#include "wine/dwmapi.h"
#include "wine/server.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dwm);

struct context
{
    cairo_t *cairo;
    cairo_surface_t *target;
    cairo_surface_t *source;
    unsigned char *image_data;
};

static struct context *context_create(void)
{
    return calloc(1, sizeof(struct context));
}

static void context_release( struct context *context )
{
    if (context->source) cairo_surface_destroy( context->source );
    if (context->target) cairo_surface_destroy( context->target );
    if (context->cairo) cairo_destroy( context->cairo );
    free( context->image_data );
    free( context );
}

static cairo_format_t format_from_bpp( UINT32 bpp )
{
    switch (bpp)
    {
    case 1: return CAIRO_FORMAT_A1;
    case 8: return CAIRO_FORMAT_A8;
    case 16: return CAIRO_FORMAT_RGB16_565;
    case 24: return CAIRO_FORMAT_RGB24;
    case 32: return CAIRO_FORMAT_ARGB32;
    }

    return CAIRO_FORMAT_INVALID;
}

static struct display *display_from_handle(dwm_display_t handle)
{
    return (struct display *)(ULONG_PTR)handle;
}

static dwm_display_t handle_from_display(struct display *display)
{
    return (UINT_PTR)display;
}

static struct window *window_from_handle(dwm_window_t handle)
{
    return (struct window *)(ULONG_PTR)handle;
}

static dwm_window_t handle_from_window(struct window *window)
{
    return (UINT_PTR)window;
}

static struct context *context_from_handle(dwm_context_t handle)
{
    return (struct context *)(ULONG_PTR)handle;
}

static dwm_context_t handle_from_context(struct context *window)
{
    return (UINT_PTR)window;
}

static NTSTATUS dwm_client_loop( void *args )
{
    struct dwm_client_loop_params *params = args;
    int req_fd, req_size, reply_fd, reply_size;
    struct display *display = NULL;
    struct context *context = NULL;
    struct window *window = NULL;
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

        if (display) display->ops->poll( display, req_fd );

        if ((req_size = read( req_fd, &req, sizeof(req) )) < sizeof(req.header) || req_size < req.header.req_size)
        {
            ERR( "failed to read request, read %d, size %u\n", req_size, req.header.req_size );
            break;
        }

        switch (req.header.type)
        {
        case DWM_REQ_CONNECT:
            if (req_size < sizeof(req.connect)) status = STATUS_BUFFER_TOO_SMALL;
            else if (req.connect.version != DWM_PROTOCOL_VERSION)
            {
                ERR( "client version mismatch, version %u != %u\n", req.connect.version, DWM_PROTOCOL_VERSION );
                status = STATUS_INVALID_PARAMETER;
            }
            else
            {
                if (!strcmp( req.connect.display_type, "x11" ))
                {
                    display = x11_display_create( req.connect.display_name );
                    reply.connect.dwm_display = handle_from_display( display );
                }
                else status = STATUS_INVALID_PARAMETER;
            }
            break;

        case DWM_REQ_WINDOW_CREATE:
            if (req_size < sizeof(req.window_create)) status = STATUS_BUFFER_TOO_SMALL;
            else if (!(window = display->ops->window_create( display, req.window_create.hwnd, req.window_create.native )))status = STATUS_UNSUCCESSFUL;
            else reply.window_create.dwm_window = handle_from_window( window );
            break;

        case DWM_REQ_WINDOW_UPDATE:
            if (req_size < sizeof(req.window_update)) status = STATUS_BUFFER_TOO_SMALL;
            else if ((window = window_from_handle(req.window_update.dwm_window)))
            {
                display->ops->window_update( window, &req.window_update.window_rect );
                window = NULL;
            }
            break;

        case DWM_REQ_WINDOW_DESTROY:
            if (req_size < sizeof(req.window_destroy)) status = STATUS_BUFFER_TOO_SMALL;
            else if ((window = window_from_handle(req.window_destroy.dwm_window)))
            {
                window_release( window );
                window = NULL;
            }
            break;

        case DWM_REQ_GDI_CONTEXT_CREATE:
            if (req_size < sizeof(req.gdi_context_create)) status = STATUS_BUFFER_TOO_SMALL;
            else
            {
                context = context_create();
                reply.gdi_context_create.dwm_context = handle_from_context( context );
            }
            break;

        case DWM_REQ_GDI_CONTEXT_DESTROY:
            if (req_size < sizeof(req.gdi_context_destroy)) status = STATUS_BUFFER_TOO_SMALL;
            else if ((context = context_from_handle(req.gdi_context_destroy.dwm_context)))
            {
                context_release( context );
                context = NULL;
            }
            break;

        case DWM_REQ_GDI_SET_SOURCE:
            if (req_size < sizeof(req.gdi_set_source)) status = STATUS_BUFFER_TOO_SMALL;
            else if (!(context = context_from_handle(req.gdi_context_destroy.dwm_context))) status = STATUS_INVALID_PARAMETER;
            else if ((context->image_data = realloc( context->image_data, req.gdi_set_source.data_size )))
            {
                int data_size;

                if (context->source) cairo_surface_destroy( context->source );
                context->source = NULL;

                if ((data_size = read( req_fd, context->image_data, req.gdi_set_source.data_size )) != req.gdi_set_source.data_size)
                {
                    ERR( "failed to read request, read %d, size %u\n", data_size, req.gdi_set_source.data_size );
                    break;
                }

                context->source = cairo_image_surface_create_for_data( context->image_data, format_from_bpp( req.gdi_set_source.bpp ), req.gdi_set_source.rect.right, req.gdi_set_source.rect.bottom, req.gdi_set_source.stride );
                TRACE( "image bpp %u stride %u rect %s data %p size %u\n", req.gdi_set_source.bpp, req.gdi_set_source.stride, wine_dbgstr_rect(&req.gdi_set_source.rect), context->image_data, data_size );
            }
            continue; /* no reply */

        case DWM_REQ_GDI_PUT_IMAGE:
            if (req_size < sizeof(req.gdi_put_image)) status = STATUS_BUFFER_TOO_SMALL;
            else if (!(context = context_from_handle(req.gdi_put_image.dwm_context))) status = STATUS_INVALID_PARAMETER;
            else if (!(window = window_from_handle(req.gdi_put_image.dwm_window))) status = STATUS_INVALID_PARAMETER;
            else
            {
                cairo_surface_t *target = window->display->ops->window_surface( window, &req.gdi_put_image.rect );

                if (target == context->target) cairo_surface_destroy( target );
                else
                {
                    if (context->cairo) cairo_destroy( context->cairo );
                    if (context->target) cairo_surface_destroy(context->target);
                    context->target = target;
                    context->cairo = cairo_create( context->target );
                }

                TRACE("put image %s\n", wine_dbgstr_rect(&req.gdi_put_image.rect));

if (0)
{
                cairo_set_operator( context->cairo, CAIRO_OPERATOR_SOURCE );
                cairo_set_source_rgb( context->cairo, 0, 0, 0 );
                cairo_paint_with_alpha( context->cairo, 0.01 );
}

                cairo_set_operator( context->cairo, CAIRO_OPERATOR_SOURCE );
                cairo_set_source_surface( context->cairo, context->source, req.gdi_put_image.rect.left, req.gdi_put_image.rect.top );
                cairo_rectangle( context->cairo, req.gdi_put_image.rect.left, req.gdi_put_image.rect.top,
                                 req.gdi_put_image.rect.right - req.gdi_put_image.rect.left,
                                 req.gdi_put_image.rect.bottom - req.gdi_put_image.rect.top );
                cairo_fill( context->cairo );

if (0)
{
                cairo_set_operator( context->cairo, CAIRO_OPERATOR_SOURCE );
                cairo_set_source_rgb( context->cairo, 1, 0, 0 );
                cairo_rectangle( context->cairo, req.gdi_put_image.rect.left, req.gdi_put_image.rect.top,
                                 req.gdi_put_image.rect.right - req.gdi_put_image.rect.left,
                                 req.gdi_put_image.rect.bottom - req.gdi_put_image.rect.top );
                cairo_stroke( context->cairo );
}
            }
            continue; /* no reply */

        case DWM_REQ_GDI_STRETCH_BLT:
            if (req_size < sizeof(req.gdi_stretch_blt)) status = STATUS_BUFFER_TOO_SMALL;
            else if ((window = window_from_handle(req.gdi_stretch_blt.dwm_window)))
            {
                char data_buffer[1024];
                int data_size;

                if ((data_size = read( req_fd, data_buffer, req.gdi_stretch_blt.data_size )) != req.gdi_stretch_blt.data_size)
                {
                    ERR( "failed to read request, read %d, size %u\n", data_size, req.gdi_stretch_blt.data_size );
                    break;
                }

                ERR( "read %u data\n", data_size );
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

    if (context) context_release( context );
    if (window) window_release( window );
    if (display) display_release( display );
    close( req_fd );
    close( reply_fd );
    return 0;
}

static NTSTATUS dwm_connect( void *args )
{
    struct dwm_connect_params *params = args;

    ERR( "%s\n", debugstr_a(params->display_name) );

    if (!strncmp( params->display_name, "x11:", 4 ))
    {
        struct display *display = x11_display_create( params->display_name + 4 );
        params->dwm_display = handle_from_display( display );
    }

    return 0;
}

static NTSTATUS dwm_disconnect( void *args )
{
    struct dwm_disconnect_params *params = args;
    struct display *display = display_from_handle(params->dwm_display);

    ERR( "display %p\n", display );

    display_release( display );
    return 0;
}

static NTSTATUS dwm_window_pos_changing( void *args )
{
    ERR("\n");
    return 0;
}

static NTSTATUS dwm_window_pos_changed( void *args )
{
    ERR("\n");
    return 0;
}

static NTSTATUS dwm_window_destroy( void *args )
{
    ERR("\n");
    return 0;
}

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    dwm_client_loop,
    dwm_connect,
    dwm_disconnect,
    dwm_window_pos_changing,
    dwm_window_pos_changed,
    dwm_window_destroy,
};

C_ASSERT(ARRAY_SIZE(__wine_unix_call_funcs) == unix_dwm_funcs_count);
