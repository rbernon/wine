/*
 * Server-side X11 host integration
 *
 * Copyright 2023 Rémi Bernon for CodeWeavers
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

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "winternl.h"

#include "handle.h"
#include "process.h"
#include "request.h"
#include "file.h"
#include "user.h"

#include "wine/debug.h"

#ifdef ENABLE_SERVER_X11

#include <xcb/xcb.h>
#include <xcb/xcbext.h>
#include <xcb/xcb_util.h>
#include <xcb/xinput.h>

WINE_DEFAULT_DEBUG_CHANNEL(x11host);

static struct list connections = LIST_INIT( connections );

struct window_data
{
    user_handle_t  window;   /* handle to the window */
    xcb_window_t   x11_win;  /* top-level X11 window */
};

struct host_x11
{
    struct object               obj;              /* object header */
    struct list                 entry;            /* entry in the connections list */
    struct fd                  *fd;               /* file descriptor for this connection */
    char                       *display;          /* X display for this connection */
    xcb_connection_t           *xcb;              /* xcb connection to the host display */
    int                         needs_flush;      /* whether xcb_flush should be called */

    xcb_window_t                root_window;      /* default root window */
    struct window_data         *windows;          /* X11 host window data array */
    unsigned int                windows_count;    /* number of windows in the mapping */
    unsigned int                windows_capacity; /* capacity of windows in the mapping */
};

static void host_x11_dump( struct object *obj, int verbose );
static struct fd *host_x11_get_fd( struct object *obj );
static void host_x11_destroy( struct object *obj );

static const struct object_ops host_x11_object_ops =
{
    sizeof(struct host_x11),          /* size */
    &no_type,                         /* type */
    host_x11_dump,                    /* dump */
    add_queue,                        /* add_queue */
    remove_queue,                     /* remove_queue */
    default_fd_signaled,              /* signaled */
    no_satisfied,                     /* satisfied */
    no_signal,                        /* signal */
    host_x11_get_fd,                  /* get_fd */
    default_map_access,               /* map_access */
    default_get_sd,                   /* get_sd */
    default_set_sd,                   /* set_sd */
    no_get_full_name,                 /* get_full_name */
    no_lookup_name,                   /* lookup_name */
    no_link_name,                     /* link_name */
    NULL,                             /* unlink_name */
    no_open_file,                     /* open_file */
    no_kernel_obj_list,               /* get_kernel_obj_list */
    no_object_mapping,                /* get_object_mapping */
    no_close_handle,                  /* close_handle */
    host_x11_destroy,                 /* destroy */
};

static void host_x11_fd_poll_event( struct fd *fd, int event );

static const struct fd_ops host_x11_fd_ops =
{
    NULL,                    /* get_poll_events */
    host_x11_fd_poll_event,  /* poll_event */
    NULL,                    /* get_fd_type */
    NULL,                    /* read */
    NULL,                    /* write */
    NULL,                    /* flush */
    NULL,                    /* get_file_info */
    NULL,                    /* get_volume_info */
    NULL,                    /* ioctl */
    NULL,                    /* cancel_async */
    NULL,                    /* queue_async */
    NULL,                    /* reselect_async */
};


typedef int (*window_data_cmp)(const struct window_data *, const void *);

/* lookup the window_data entry for which cmp(data, key) returns 0 */
static struct window_data *window_data_lookup( struct host_x11 *host, window_data_cmp cmp, const void *key )
{
    struct window_data *begin = host->windows, *end = begin + host->windows_count, *iter;
    for (iter = begin; iter < end; iter++) if (!cmp( iter, key )) return iter;
    return NULL;
}

static int compare_x11_win( const struct window_data *data, const void *value )
{
    const xcb_window_t *x11_win = value;
    return data->x11_win - *x11_win;
}

/* insert x11_win / win in the host windows mapping, return -1 if no memory */
static int window_data_insert( struct host_x11 *host, xcb_window_t x11_win,
                               user_handle_t window )
{
    struct window_data *found;
    unsigned int pos;

    TRACE( "host %p window %#x -> %#x\n", host, x11_win, window );

    if ((found = window_data_lookup( host, compare_x11_win, &x11_win )))
    {
        WARN( "host %p window %#x already present, updating\n", host, x11_win );
        found->window = window;
        return 0;
    }

    if (host->windows_count == host->windows_capacity)
    {
        size_t capacity = max( host->windows_capacity * 3 / 2, 256 );
        struct window_data *tmp;

        if (!(tmp = realloc( host->windows, capacity * sizeof(*found) )))
        {
            ERR( "host %p could not grow window array to capacity %zu\n", host, capacity );
            return -1;
        }

        host->windows = tmp;
        host->windows_capacity = capacity;
    }

    pos = host->windows_count++;
    host->windows[pos].window = window;
    host->windows[pos].x11_win = x11_win;

    return 0;
}

/* remove x11_win from the host window mapping if present */
static int window_data_remove( struct host_x11 *host, xcb_window_t x11_win )
{
    struct window_data *found;

    TRACE( "host %p window %#x\n", host, x11_win );

    if ((found = window_data_lookup( host, compare_x11_win, &x11_win )))
    {
        size_t count = host->windows + host->windows_count - found - 1;
        memmove( found, found + 1, count * sizeof(*found) );
        host->windows_count--;
    }

    if (host->windows_count < host->windows_capacity / 3)
    {
        size_t capacity = max( host->windows_count * 2, 1 );
        struct window_data *tmp;

        if (!(tmp = realloc( host->windows, capacity * sizeof(*found) )))
        {
            ERR( "host %p could not shrink window array to capacity %zu\n", host, capacity );
            return -1;
        }

        host->windows = tmp;
        host->windows_capacity = capacity;
    }

    return 0;
}


/* xcb_input_fp1616_t utility functions */

static double double_from_fp1616( xcb_input_fp1616_t v )
{
    return (double)v / (double)UINT16_MAX;
}

static int fp1616_round( xcb_input_fp1616_t v )
{
    return round( double_from_fp1616( v ) );
}


/* convert x / y coordinates from host root window to virtual screen */
static void root_to_virtual_screen( struct desktop *desktop, int *x, int *y )
{
    rectangle_t virtual_rect;
    get_top_window_rectangle( desktop, &virtual_rect );
    *x = *x + virtual_rect.left;
    *y = *y + virtual_rect.top;
}


/* send absolute mouse input received on an x11 window */
static void send_mouse_input( struct host_x11 *host, xcb_window_t x11_win, xcb_input_fp1616_t x,
                              xcb_input_fp1616_t y, int button_data, int button_flags )
{
    hw_input_t input = {.mouse = {.type = INPUT_MOUSE, .data = button_data, .flags = button_flags}};
    unsigned int origin = IMO_HARDWARE;
    struct window_data *data;
    struct desktop *desktop;

    if (!(data = window_data_lookup( host, compare_x11_win, &x11_win )))
    {
        ERR( "host %p failed to find window %#x\n", host, x11_win );
        return;
    }

    if (!(desktop = get_hardware_input_desktop( data->window ))) return;
    if (!set_input_desktop( desktop->winstation, desktop ))
    {
        release_object( desktop );
        return;
    }

    if ((current = get_foreground_thread( desktop, data->window )))
    {
        input.mouse.x = fp1616_round( x );
        input.mouse.y = fp1616_round( y );
        input.mouse.flags |= MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
        root_to_virtual_screen( desktop, &input.mouse.x, &input.mouse.y );

        TRACE( "host %p window %#x -> %#x pos (%+8.2f,%+8.2f) -> (%+5d,%+5d)\n", host, x11_win, data->window,
               double_from_fp1616( x ), double_from_fp1616( y ), input.mouse.x, input.mouse.y );

        queue_mouse_message( desktop, data->window, &input, origin, NULL );
        release_object( current );
        current = NULL;
    }

    release_object( desktop );
}


static void handle_xcb_input_enter( struct host_x11 *host, xcb_input_enter_event_t *event )
{
    TRACE( "host %p sequence %u time %u device %u\n", host, event->full_sequence, event->time, event->deviceid );

    /* TODO: Move the cursor where it's supposed to be if it's in the window */

    send_mouse_input( host, event->event, event->root_x, event->root_y, 0, 0 );
}

static void handle_xcb_input_leave( struct host_x11 *host, xcb_input_enter_event_t *event )
{
    TRACE( "host %p sequence %u time %u device %u\n", host, event->full_sequence, event->time, event->deviceid );

    /* TODO: Don't warp the cursor if it's outside our windows */

    send_mouse_input( host, event->event, event->root_x, event->root_y, 0, 0 );
}

static void handle_xcb_input_motion( struct host_x11 *host, xcb_input_motion_event_t *event )
{
    TRACE( "host %p sequence %u time %u device %u\n", host, event->full_sequence, event->time, event->deviceid );

    send_mouse_input( host, event->event, event->root_x, event->root_y, 0, 0 );
}


static void handle_xcb_ge_event( struct host_x11 *host, xcb_ge_event_t *event )
{
    switch (event->event_type)
    {
    case XCB_INPUT_MOTION: handle_xcb_input_motion( host, (xcb_input_motion_event_t *)event ); break;
    case XCB_INPUT_ENTER: handle_xcb_input_enter( host, (xcb_input_enter_event_t *)event ); break;
    case XCB_INPUT_LEAVE: handle_xcb_input_leave( host, (xcb_input_enter_event_t *)event ); break;
    default: WARN( "host %p unexpected event type %u\n", host, event->event_type ); break;
    }
}

static void handle_xcb_generic_error( struct host_x11 *host, xcb_generic_error_t *error )
{
    const char *label = xcb_event_get_error_label( error->error_code );
    ERR( "host %p got error %s sequence %u resource %#x major %u minor %u\n", host, debugstr_a(label),
         error->sequence, error->resource_id, error->minor_code, error->major_code );
}

static void host_x11_fd_poll_event( struct fd *fd, int events )
{
    struct object *obj = get_fd_user( fd );
    struct host_x11 *host = (struct host_x11 *)obj;
    xcb_generic_event_t *event;
    int err;

    assert( obj->ops == &host_x11_object_ops );

    if ((err = xcb_connection_has_error( host->xcb )))
    {
        ERR( "host %p lost xcb connection, error %d\n", host, err );
        set_fd_events( host->fd, -1 );
        return;
    }

    if (events & POLLOUT)
    {
        xcb_flush( host->xcb );
        host->needs_flush = FALSE;
    }

    if (events & POLLIN)
    {
        while ((event = xcb_poll_for_event( host->xcb )))
        {
            uint8_t event_code;

            if (!(event_code = XCB_EVENT_RESPONSE_TYPE( event )))
                handle_xcb_generic_error( host, (xcb_generic_error_t *)event );
            else if (event_code == XCB_GE_GENERIC)
                handle_xcb_ge_event( host, (xcb_ge_event_t *)event );
            else
            {
                const char *label = xcb_event_get_label( event->response_type );
                WARN( "host %p unexpected xcb event, type %s (%u) sequence %u\n", host,
                      debugstr_a(label), event->response_type, event->full_sequence );
            }

            free( event );
        }
    }

    events = POLLIN;
    if (host->needs_flush) events |= POLLOUT;
    set_fd_events( host->fd, events );
}

static void host_x11_dump( struct object *obj, int verbose )
{
    struct host_x11 *host = (struct host_x11 *)obj;
    assert( obj->ops == &host_x11_object_ops );

    fprintf( stderr, "X11 display %s\n", host->display );
}

static struct fd *host_x11_get_fd( struct object *obj )
{
    struct host_x11 *host = (struct host_x11 *)obj;
    assert( obj->ops == &host_x11_object_ops );

    return (struct fd *)grab_object( host->fd );
}

static void host_x11_destroy( struct object *obj )
{
    struct host_x11 *host = (struct host_x11 *)obj;
    assert( obj->ops == &host_x11_object_ops );

    free( host->windows );
    free( host->display );
    if (host->fd) release_object( host->fd );
    if (host->xcb) xcb_disconnect( host->xcb );
    list_remove( &host->entry );
}


struct xcb_input_mask
{
    xcb_input_event_mask_t head;
    xcb_input_xi_event_mask_t mask;
};

static int init_xinput( struct host_x11 *host )
{
    xcb_input_xi_query_version_cookie_t version_cookie;
    xcb_input_xi_query_version_reply_t *version_reply;
    xcb_generic_error_t *error;

    version_cookie = xcb_input_xi_query_version( host->xcb, XCB_INPUT_MAJOR_VERSION, XCB_INPUT_MINOR_VERSION );
    if (!(version_reply = xcb_input_xi_query_version_reply( host->xcb, version_cookie, &error )))
    {
        handle_xcb_generic_error( host, error );
        free( error );
        return -1;
    }

    TRACE( "host %p xinput version %u.%u enabled\n", host, version_reply->major_version, version_reply->minor_version );
    free( version_reply );

    return 0;
}

static struct object *create_host_x11( const char *display )
{
    struct host_x11 *host;
    xcb_screen_t *screen;
    int screen_num;

    LIST_FOR_EACH_ENTRY( host, &connections, struct host_x11, entry )
        if (!strcmp( host->display, display )) return grab_object( host );

    if (!(host = alloc_object( &host_x11_object_ops ))) return NULL;
    list_add_tail( &connections, &host->entry );

    host->fd = 0;
    host->display = NULL;
    host->xcb = 0;
    host->needs_flush = TRUE;

    host->root_window = 0;
    host->windows = NULL;
    host->windows_count = 0;
    host->windows_capacity = 0;

    if (!(host->display = strdup( display ))) goto failed;
    if (!(host->xcb = xcb_connect( display, &screen_num ))) goto failed;
    if (xcb_connection_has_error( host->xcb )) goto failed;
    if (!(host->fd = create_anonymous_fd( &host_x11_fd_ops, xcb_get_file_descriptor( host->xcb ),
                                          &host->obj, FILE_SYNCHRONOUS_IO_NONALERT )))
        goto failed;
    if (!(screen = xcb_aux_get_screen( host->xcb, screen_num )))
        goto failed;

    host->root_window = screen->root;
    if (init_xinput( host )) goto failed;

    TRACE( "host %p connected to X11 display %s\n", host, display );
    set_fd_events( host->fd, POLLIN | POLLOUT );
    return &host->obj;

failed:
    ERR( "failed to connect to X11 display %s\n", display );
    release_object( host );
    return NULL;
}


/* Connect a process to the host X11 server */
DECL_HANDLER( x11_connect )
{
    struct process *process = current->process;

    if (process->host) release_object( process->host );
    process->host = create_host_x11( get_req_data() );
}

/* Start listening for input on a X11 window */
DECL_HANDLER( x11_start_input )
{
    struct process *process = current->process;
    struct object *obj = process->host;

    if (!obj || obj->ops != &host_x11_object_ops )
        set_error( STATUS_INVALID_PARAMETER );
    else
    {
        struct host_x11 *host = (struct host_x11 *)obj;
        struct xcb_input_mask input_mask;
        xcb_void_cookie_t cookie;

        if (window_data_insert( host, req->x11_win, req->window )) return;

        input_mask.head.deviceid = XCB_INPUT_DEVICE_ALL_MASTER;
        input_mask.head.mask_len = sizeof(input_mask.mask) / sizeof(uint32_t);
        input_mask.mask = XCB_INPUT_XI_EVENT_MASK_ENTER | XCB_INPUT_XI_EVENT_MASK_LEAVE |
                          XCB_INPUT_XI_EVENT_MASK_MOTION;

        cookie = xcb_input_xi_select_events( host->xcb, req->x11_win, 1, &input_mask.head );
        if (!debug_level) xcb_discard_reply( host->xcb, cookie.sequence );

        if (!host->needs_flush) set_fd_events( host->fd, POLLIN | POLLOUT );
        host->needs_flush = TRUE;
    }
}

/* Stop listening for input on a X11 window */
DECL_HANDLER( x11_stop_input )
{
    struct process *process = current->process;
    struct object *obj = process->host;

    if (!obj || obj->ops != &host_x11_object_ops )
        set_error( STATUS_INVALID_PARAMETER );
    else
    {
        struct host_x11 *host = (struct host_x11 *)obj;
        window_data_remove( host, req->x11_win );
    }
}

#else /* ENABLE_SERVER_X11 */

/* Connect a process to the host X11 server */
DECL_HANDLER( x11_connect )
{
    set_error( STATUS_NOT_SUPPORTED );
}

/* Start listening for input on a X11 window */
DECL_HANDLER( x11_start_input )
{
    set_error( STATUS_NOT_SUPPORTED );
}

/* Stop listening for input on a X11 window */
DECL_HANDLER( x11_stop_input )
{
    set_error( STATUS_NOT_SUPPORTED );
}

#endif /* ENABLE_SERVER_X11 */
