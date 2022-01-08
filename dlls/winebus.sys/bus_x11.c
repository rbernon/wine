/*  Bus like function for X11 devices
 *
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

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <dlfcn.h>

#include <pthread.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "winioctl.h"

#ifdef HAVE_X11_XLIB_H
#include <X11/Xlib.h>
#endif
#ifdef HAVE_X11_EXTENSIONS_XINPUT2_H
#include <X11/extensions/XInput2.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <poll.h>

#include "wine/debug.h"

#include "unix_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(x11);

#if defined(SONAME_LIBX11) && defined(SONAME_LIBXI) && defined(HAVE_X11_EXTENSIONS_XINPUT2_H)

#define MAKE_FUNCPTR(f) static typeof(f) * p##f
MAKE_FUNCPTR( XCloseDisplay );
MAKE_FUNCPTR( XInitThreads );
MAKE_FUNCPTR( XNextEvent );
MAKE_FUNCPTR( XOpenDisplay );
MAKE_FUNCPTR( XPending );
MAKE_FUNCPTR( XQueryExtension );
MAKE_FUNCPTR( XChangeWindowAttributes );
MAKE_FUNCPTR( XSelectInput );

MAKE_FUNCPTR( XIFreeDeviceInfo );
MAKE_FUNCPTR( XIGetClientPointer );
MAKE_FUNCPTR( XIQueryDevice );
MAKE_FUNCPTR( XIQueryVersion );
MAKE_FUNCPTR( XISelectEvents );
#undef MAKE_FUNCPTR

static struct x11_bus_options bus_options;
static int bus_control[2];

static void *x11_handle;
static Display *display;

static void *xi_handle;
static int xi_opcode;
static int xi_event;
static int xi_error;
static int xi_major = 2;
static int xi_minor = 1;

static struct list event_queue = LIST_INIT(event_queue);

static char const *debugstr_xevent( XEvent *event )
{
    switch (event->type)
    {
    case ButtonPress: return "type ButtonPress ...";
    case ButtonRelease: return "type ButtonRelease ...";
    case CirculateNotify: return "type CirculateNotify ...";
    case CirculateRequest: return "type CirculateRequest ...";
    case ClientMessage: return "type ClientMessage ...";
    case ColormapNotify: return "type ColormapNotify ...";
    case ConfigureNotify: return "type ConfigureNotify ...";
    case ConfigureRequest: return "type ConfigureRequest ...";
    case CreateNotify: return "type CreateNotify ...";
    case DestroyNotify: return "type DestroyNotify ...";
    case EnterNotify: return "type EnterNotify ...";
    case Expose: return "type Expose ...";
    case FocusIn: return "type FocusIn ...";
    case FocusOut: return "type FocusOut ...";
    case GenericEvent: return "type GenericEvent ...";
    case GraphicsExpose: return "type GraphicsExpose ...";
    case GravityNotify: return "type GravityNotify ...";
    case KeymapNotify: return "type KeymapNotify ...";
    case KeyPress: return "type KeyPress ...";
    case KeyRelease: return "type KeyRelease ...";
    case LeaveNotify: return "type LeaveNotify ...";
    case MapNotify: return "type MapNotify ...";
    case MappingNotify: return "type MappingNotify ...";
    case MapRequest: return "type MapRequest ...";
    case MotionNotify: return "type MotionNotify ...";
    case NoExpose: return "type NoExpose ...";
    case PropertyNotify: return "type PropertyNotify ...";
    case ReparentNotify: return "type ReparentNotify ...";
    case ResizeRequest: return "type ResizeRequest ...";
    case SelectionClear: return "type SelectionClear ...";
    case SelectionNotify: return "type SelectionNotify ...";
    case SelectionRequest: return "type SelectionRequest ...";
    case UnmapNotify: return "type UnmapNotify ...";
    case VisibilityNotify: return "type VisibilityNotify ...";
    default: return wine_dbg_sprintf("type %u", event->type);
    }
}

static void x11_process_event( XEvent *event )
{
    ERR( "event %s\n", debugstr_xevent(event) );

    if (event->type == CreateNotify)
    {
        pXSelectInput( display, event->xcreatewindow.window, PointerMotionMask );
    }

    if (0 && event->type == CreateNotify)
    {
        unsigned char mask_bits[XIMaskLen(XI_LASTEVENT)];
        XIEventMask mask;

        mask.mask     = mask_bits;
        mask.mask_len = sizeof(mask_bits);
        mask.deviceid = XIAllMasterDevices;
        memset( mask_bits, 0, sizeof(mask_bits) );
        XISetMask( mask_bits, XI_DeviceChanged );
        XISetMask( mask_bits, XI_Motion );
        XISetMask( mask_bits, XI_Enter );
        XISetMask( mask_bits, XI_Leave );
        XISetMask( mask_bits, XI_ButtonPress );
        XISetMask( mask_bits, XI_ButtonRelease );

        pXISelectEvents( display, event->xcreatewindow.window, &mask, 1 );
    }
}

NTSTATUS x11_bus_init( void *args )
{
    TRACE("args %p\n", args);

    bus_options = *(struct x11_bus_options *)args;

    if (pipe(bus_control) != 0)
    {
        ERR( "Failed to create control pipe!\n" );
        return STATUS_UNSUCCESSFUL;
    }

    if (!(x11_handle = dlopen( SONAME_LIBX11, RTLD_NOW )))
    {
        ERR( "Could not load %s!\n", SONAME_LIBX11 );
        goto failed;
    }
#define LOAD_FUNCPTR(f)                          \
    if ((p##f = dlsym( x11_handle, #f )) == NULL)  \
    {                                            \
        ERR( "Could not find symbol %s!\n", #f );  \
        goto failed;                             \
    }
    LOAD_FUNCPTR( XCloseDisplay );
    LOAD_FUNCPTR( XInitThreads );
    LOAD_FUNCPTR( XNextEvent );
    LOAD_FUNCPTR( XOpenDisplay );
    LOAD_FUNCPTR( XPending );
    LOAD_FUNCPTR( XQueryExtension );
    LOAD_FUNCPTR( XChangeWindowAttributes );
    LOAD_FUNCPTR( XSelectInput );
#undef LOAD_FUNCPTR

    if (!(xi_handle = dlopen( SONAME_LIBXI, RTLD_NOW )))
    {
        ERR( "Could not load %s!\n", SONAME_LIBXI );
        goto failed;
    }
#define LOAD_FUNCPTR(f)                          \
    if ((p##f = dlsym( xi_handle, #f )) == NULL)  \
    {                                            \
        ERR( "Could not find symbol %s!\n", #f );  \
        goto failed;                             \
    }
    LOAD_FUNCPTR( XIFreeDeviceInfo );
    LOAD_FUNCPTR( XIGetClientPointer );
    LOAD_FUNCPTR( XIQueryDevice );
    LOAD_FUNCPTR( XIQueryVersion );
    LOAD_FUNCPTR( XISelectEvents );
#undef LOAD_FUNCPTR

    if (!pXInitThreads()) ERR( "XInitThreads failed, trouble ahead!\n" );
    if (!(display = pXOpenDisplay( NULL )))
    {
        ERR( "Failed to open X11 display!\n" );
        goto failed;
    }

    pXSelectInput( display, DefaultRootWindow( display ), SubstructureNotifyMask );

    if (!pXQueryExtension( display, "XInputExtension", &xi_opcode, &xi_event, &xi_error ))
    {
        ERR( "Failed to query XInput extension!\n" );
        goto failed;
    }
    if (pXIQueryVersion( display, &xi_major, &xi_minor ) != Success)
    {
        ERR( "Failed to query XInput extension version %d.%d!\n", xi_major, xi_minor );
        goto failed;
    }

    TRACE( "Initialized XInput extension version %d.%d.\n", xi_major, xi_minor );
    return STATUS_SUCCESS;

failed:
    if (display) pXCloseDisplay( display );
    if (xi_handle) dlclose( xi_handle );
    xi_handle = NULL;
    if (x11_handle) dlclose( x11_handle );
    x11_handle = NULL;
    close( bus_control[0] );
    close( bus_control[1] );
    return STATUS_UNSUCCESSFUL;
}

NTSTATUS x11_bus_wait( void *args )
{
    struct bus_event *result = args;
    struct pollfd pfd[2];
    char ctrl = 0;
    XEvent event;

    /* cleanup previously returned event */
    bus_event_cleanup( result );

    ERR( "args %p\n", args );

    pfd[0].fd = bus_control[0];
    pfd[1].fd = ConnectionNumber( display );

    do
    {
        if (bus_event_queue_pop( &event_queue, result )) return STATUS_PENDING;

        pfd[0].events = POLLIN;
        pfd[0].revents = 0;
        pfd[1].events = POLLIN;
        pfd[1].revents = 0;

        while (poll(pfd, 2, -1) <= 0) {}

        if (pfd[0].revents) read(bus_control[0], &ctrl, 1);
        while (pXPending( display ))
        {
            pXNextEvent( display, &event );
            x11_process_event( &event );
        }
    } while (ctrl != 'q');

    ERR( "X11 main loop exiting\n" );
    bus_event_queue_destroy( &event_queue );

    dlclose( xi_handle );
    xi_handle = NULL;
    dlclose( x11_handle );
    x11_handle = NULL;
    close( bus_control[0] );
    close( bus_control[1] );
    return STATUS_SUCCESS;
}

NTSTATUS x11_bus_stop( void *args )
{
    if (x11_handle) write( bus_control[1], "q", 1 );
    return STATUS_SUCCESS;
}

#else

NTSTATUS x11_bus_init( void *args )
{
    WARN( "X11 support not compiled in!\n" );
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS x11_bus_wait( void *args )
{
    WARN( "X11 support not compiled in!\n" );
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS x11_bus_stop( void *args )
{
    WARN( "X11 support not compiled in!\n" );
    return STATUS_NOT_IMPLEMENTED;
}

#endif
