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

struct host_x11
{
    struct object               obj;              /* object header */
    struct list                 entry;            /* entry in the connections list */
    struct fd                  *fd;               /* file descriptor for this connection */
    char                       *display;          /* X display for this connection */
    xcb_connection_t           *xcb;              /* xcb connection to the host display */
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

    if (events & POLLIN)
    {
        while ((event = xcb_poll_for_event( host->xcb )))
        {
            uint8_t event_code;

            if (!(event_code = XCB_EVENT_RESPONSE_TYPE( event )))
                handle_xcb_generic_error( host, (xcb_generic_error_t *)event );
            else
            {
                const char *label = xcb_event_get_label( event->response_type );
                WARN( "host %p unexpected xcb event, type %s (%u) sequence %u\n", host,
                      debugstr_a(label), event->response_type, event->full_sequence );
            }

            free( event );
        }
    }

    set_fd_events( host->fd, POLLIN );
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

    free( host->display );
    if (host->fd) release_object( host->fd );
    if (host->xcb) xcb_disconnect( host->xcb );
    list_remove( &host->entry );
}


static struct object *create_host_x11( const char *display )
{
    struct host_x11 *host;

    LIST_FOR_EACH_ENTRY( host, &connections, struct host_x11, entry )
        if (!strcmp( host->display, display )) return grab_object( host );

    if (!(host = alloc_object( &host_x11_object_ops ))) return NULL;
    list_add_tail( &connections, &host->entry );

    host->fd = 0;
    host->display = NULL;
    host->xcb = 0;

    if (!(host->display = strdup( display ))) goto failed;
    if (!(host->xcb = xcb_connect( display, NULL ))) goto failed;
    if (xcb_connection_has_error( host->xcb )) goto failed;
    if (!(host->fd = create_anonymous_fd( &host_x11_fd_ops, xcb_get_file_descriptor( host->xcb ),
                                          &host->obj, FILE_SYNCHRONOUS_IO_NONALERT )))
        goto failed;

    TRACE( "host %p connected to X11 display %s\n", host, display );
    set_fd_events( host->fd, POLLIN );
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

#else /* ENABLE_SERVER_X11 */

/* Connect a process to the host X11 server */
DECL_HANDLER( x11_connect )
{
    set_error( STATUS_NOT_SUPPORTED );
}

#endif /* ENABLE_SERVER_X11 */
