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
#include "kbd.h"

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
#include <xcb/xkb.h>

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbregistry.h>
#include <xkbcommon/xkbcommon-x11.h>

#include <linux/input.h>

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

    xcb_input_device_id_t       pointer_id;       /* master pointer device id */
    xcb_input_valuator_class_t  x_class;          /* master pointer x axis valuator class */
    xcb_input_valuator_class_t  y_class;          /* master pointer y axis valuator class */
    uint64_t                    warp_sequence;    /* cursor warping request sequence */
    int                         is_clipping;      /* whether clipping is currently active */
    xcb_xfixes_barrier_t        clip_barrier[4];  /* barriers for cursor clipping */

    uint8_t                     xkb_event_code;   /* event code for Xkb events */
    xcb_input_device_id_t       keyboard_id;      /* master keyboard device id */
    int                         keyboard_grabbed; /* keyboard is grabbed by the window manager */
    struct layout              *active_layout;    /* the currently active keyboard layout */
    struct list                 layouts;          /* list of layouts from the Xkb keymap */
    xcb_window_t                keyboard_focus;   /* window with the X11 keyboard focus */
};

static void host_x11_dump( struct object *obj, int verbose );
static struct fd *host_x11_get_fd( struct object *obj );
static void host_x11_destroy( struct object *obj );
static const struct host_ops *host_x11_get_host_ops( struct object *obj );

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
    host_x11_get_host_ops,            /* get_host_ops */
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

static void host_x11_warp_cursor( struct object *obj, struct desktop *desktop );
static void host_x11_clip_cursor( struct object *obj, struct desktop *desktop, const rectangle_t *rect );

static const struct host_ops host_x11_host_ops =
{
    host_x11_warp_cursor,             /* warp_cursor */
    host_x11_clip_cursor,             /* clip_cursor */
};

static const struct host_ops *host_x11_get_host_ops( struct object *obj )
{
    return &host_x11_host_ops;
}

static void host_set_needs_flush( struct host_x11 *host )
{
    if (!host->needs_flush) set_fd_events( host->fd, POLLIN | POLLOUT );
    host->needs_flush = TRUE;
}


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


/* xcb_input_fp3232_t utility functions */

static double double_from_fp3232( xcb_input_fp3232_t v )
{
    return (double)v.integral + (double)v.frac / (double)UINT32_MAX;
}

static int fp3232_round( xcb_input_fp3232_t v )
{
    return round( double_from_fp3232( v ) );
}

static int64_t int64_from_fp3232( xcb_input_fp3232_t v )
{
    return (int64_t)v.integral << 32 | v.frac;
}

static xcb_input_fp3232_t fp3232_from_int64( int64_t v )
{
    xcb_input_fp3232_t fp = {.integral = v >> 32, .frac = (uint32_t)v};
    return fp;
}

static xcb_input_fp3232_t fp3232_sub( xcb_input_fp3232_t a, xcb_input_fp3232_t b )
{
    int64_t a64 = int64_from_fp3232( a ), b64 = int64_from_fp3232( b );
    return fp3232_from_int64( a64 - b64 );
}

/* scale an fp3232 value from its valuator range to the desired range */
static int fp3232_scale( xcb_input_fp3232_t v, xcb_input_valuator_class_t *klass, int32_t min, int32_t max )
{
    xcb_input_fp3232_t klass_range = fp3232_sub( klass->max, klass->min );
    double scale = klass_range.integral > 0 ? (max - min) / double_from_fp3232( klass_range ) : 1;
    return round( double_from_fp3232( fp3232_sub( v, klass->min ) ) * scale );
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


/* convert x / y coordinates from virtual screen to host root window */
static void virtual_screen_to_root( struct desktop *desktop, int *x, int *y )
{
    rectangle_t virtual_rect;
    get_top_window_rectangle( desktop, &virtual_rect );
    *x = *x - virtual_rect.left;
    *y = *y - virtual_rect.top;
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

        queue_mouse_message( desktop, data->window, &input, origin, NULL, 0 );
        release_object( current );
        current = NULL;
    }

    release_object( desktop );
}


/* send raw mouse input received on a window */
static void send_raw_mouse_input( struct host_x11 *host, xcb_input_fp3232_t x, xcb_input_fp3232_t y,
                                  int data, int flags )
{
    struct hw_msg_source source = {IMDT_UNAVAILABLE, IMO_HARDWARE};
    struct thread *foreground;
    struct desktop *desktop;
    int raw_x = 0, raw_y = 0;

    if (!(desktop = get_hardware_input_desktop( 0 ))) return;
    if (!set_input_desktop( desktop->winstation, desktop ))
    {
        release_object( desktop );
        return;
    }

    if (int64_from_fp3232( x ) || int64_from_fp3232( x ))
    {
        flags |= MOUSEEVENTF_MOVE;

        if (host->x_class.mode == XCB_INPUT_VALUATOR_MODE_ABSOLUTE)
        {
            flags |= MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
            raw_x  = fp3232_scale( x, &host->x_class, 0, UINT16_MAX );
            raw_y  = fp3232_scale( y, &host->y_class, 0, UINT16_MAX );
        }
        else
        {
            raw_x = fp3232_round( x );
            raw_y = fp3232_round( y );
        }
    }

    if ((foreground = get_foreground_thread( desktop, 0 )))
    {
        struct rawinput_message raw_msg = {0};
        raw_msg.foreground = foreground;
        raw_msg.source     = source;
        raw_msg.time       = get_tick_count();
        raw_msg.message    = WM_INPUT;
        raw_msg.flags      = flags;
        rawmouse_init( &raw_msg.rawinput, &raw_msg.data.mouse, raw_x, raw_y, flags, data, 0 );

        TRACE( "host %p %s raw (%+8.2f,%+8.2f) -> (%+5d,%+5d)\n", host, flags & MOUSEEVENTF_ABSOLUTE ? "abs" : "rel",
               double_from_fp3232( x ), double_from_fp3232( y ), raw_x, raw_y );

        dispatch_rawinput_message( desktop, &raw_msg );
        release_object( foreground );
    }

    release_object( desktop );
}


static void handle_xcb_input_button_press( struct host_x11 *host, xcb_input_button_press_event_t *event )
{
    static const UINT flags[] =
    {
        MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_MIDDLEDOWN, MOUSEEVENTF_RIGHTDOWN,
        MOUSEEVENTF_WHEEL, MOUSEEVENTF_WHEEL,
        MOUSEEVENTF_HWHEEL, MOUSEEVENTF_HWHEEL,
        MOUSEEVENTF_XDOWN, MOUSEEVENTF_XDOWN,
    };
    static const UINT data[ARRAY_SIZE(flags)] =
    {
        0, 0, 0,
        WHEEL_DELTA, -WHEEL_DELTA,
        -WHEEL_DELTA, WHEEL_DELTA,
        XBUTTON1, XBUTTON2,
    };
    xcb_input_fp3232_t fp3232_zero = {0};
    int button = event->detail - 1;

    TRACE( "host %p sequence %u time %u device %u\n", host, event->full_sequence, event->time, event->deviceid );

    if (button >= ARRAY_SIZE(flags)) return;
    send_raw_mouse_input( host, fp3232_zero, fp3232_zero, data[button], flags[button] );
    send_mouse_input( host, event->event, event->root_x, event->root_y, data[button], flags[button] );
}

static void handle_xcb_input_button_release( struct host_x11 *host, xcb_input_button_release_event_t *event )
{
    static const UINT flags[] =
    {
        MOUSEEVENTF_LEFTUP, MOUSEEVENTF_MIDDLEUP, MOUSEEVENTF_RIGHTUP,
        0, 0,
        0, 0,
        MOUSEEVENTF_XUP, MOUSEEVENTF_XUP,
    };
    static const UINT data[ARRAY_SIZE(flags)] =
    {
        0, 0, 0,
        0, 0,
        0, 0,
        XBUTTON1, XBUTTON2,
    };
    xcb_input_fp3232_t fp3232_zero = {0};
    int button = event->detail - 1;

    TRACE( "host %p sequence %u time %u device %u\n", host, event->full_sequence, event->time, event->deviceid );

    if (button >= ARRAY_SIZE(flags)) return;
    send_raw_mouse_input( host, fp3232_zero, fp3232_zero, data[button], flags[button] );
    send_mouse_input( host, event->event, event->root_x, event->root_y, data[button], flags[button] );
}

static void handle_xcb_input_enter( struct host_x11 *host, xcb_input_enter_event_t *event )
{
    TRACE( "host %p sequence %u time %u device %u\n", host, event->full_sequence, event->time, event->deviceid );

    /* TODO: Move the cursor where it's supposed to be if it's in the window */

    if (event->full_sequence == host->warp_sequence) return;
    send_mouse_input( host, event->event, event->root_x, event->root_y, 0, 0 );
}

static void handle_xcb_input_leave( struct host_x11 *host, xcb_input_enter_event_t *event )
{
    TRACE( "host %p sequence %u time %u device %u\n", host, event->full_sequence, event->time, event->deviceid );

    /* TODO: Don't warp the cursor if it's outside our windows */

    if (event->full_sequence == host->warp_sequence) return;
    send_mouse_input( host, event->event, event->root_x, event->root_y, 0, 0 );
}

static void handle_xcb_input_motion( struct host_x11 *host, xcb_input_motion_event_t *event )
{
    TRACE( "host %p sequence %u time %u device %u\n", host, event->full_sequence, event->time, event->deviceid );

    if (event->full_sequence == host->warp_sequence) return;
    send_mouse_input( host, event->event, event->root_x, event->root_y, 0, 0 );
}


/* xinput2 device valuator class handling */

static int host_classes_are_valid( struct host_x11 *host )
{
    return host->pointer_id != (xcb_input_device_id_t)-1 && host->x_class.mode == host->y_class.mode &&
           host->x_class.number != (uint16_t)-1 && host->y_class.number != (uint16_t)-1;
}

static void handle_valuator_class( struct host_x11 *host, xcb_input_valuator_class_t *klass )
{
    if (klass->number == 0) host->x_class = *klass;
    if (klass->number == 1) host->y_class = *klass;
}

static void handle_xcb_input_xi_query_device_reply( struct host_x11 *host, xcb_input_xi_query_device_reply_t *reply )
{
    xcb_input_xi_device_info_iterator_t iter;

    for (iter = xcb_input_xi_query_device_infos_iterator( reply ); iter.rem;
         xcb_input_xi_device_info_next( &iter ))
    {
        xcb_input_device_class_iterator_t class_iter;

        if (iter.data->type != XCB_INPUT_DEVICE_TYPE_MASTER_POINTER) continue;
        host->pointer_id = iter.data->deviceid;

        for (class_iter = xcb_input_xi_device_info_classes_iterator( iter.data ); class_iter.rem;
             xcb_input_device_class_next( &class_iter ))
        {
            if (class_iter.data->type != XCB_INPUT_DEVICE_CLASS_TYPE_VALUATOR) continue;
            handle_valuator_class( host, (xcb_input_valuator_class_t *)class_iter.data );

            if (host_classes_are_valid( host )) goto done;
        }
    }

done:
    /* TODO: Initialize the desktop cursor position once desktop is ready */

    TRACE( "host %p pointer device %u is at (%+8.2f,%+8.2f)\n", host, host->pointer_id,
           double_from_fp3232( host->x_class.value ), double_from_fp3232( host->y_class.value ) );
}

static void handle_xcb_input_device_changed( struct host_x11 *host, xcb_input_device_changed_event_t *event )
{
    xcb_input_device_class_iterator_t class_iter;

    if (event->deviceid != host->pointer_id) return;
    host->x_class.number = -1;
    host->y_class.number = -1;

    for (class_iter = xcb_input_device_changed_classes_iterator( event ); class_iter.rem;
         xcb_input_device_class_next( &class_iter ))
    {
        if (class_iter.data->type != XCB_INPUT_DEVICE_CLASS_TYPE_VALUATOR) continue;
        handle_valuator_class( host, (xcb_input_valuator_class_t *)class_iter.data );

        if (host_classes_are_valid( host )) break;
    }
}

/* extract x / y values from raw event valuators */
static void extract_axisvalues( struct host_x11 *host, uint32_t *mask_buf, int mask_len,
                                xcb_input_fp3232_t *values_buf, int values_len,
                                xcb_input_fp3232_t *x, xcb_input_fp3232_t *y )
{
    uint32_t bit, mask, i = 0, *mask_end = mask_buf + mask_len;
    xcb_input_fp3232_t *values_end = values_buf + values_len;

    while (mask_buf < mask_end && values_buf < values_end)
    {
        for (mask = *mask_buf++, bit = 0; bit < 8 * sizeof(mask); bit++, i++, mask >>= 1)
        {
            if (!(mask & 1)) continue;
            if (i == host->x_class.number) *x = *values_buf;
            if (i == host->y_class.number) *y = *values_buf;
            if (++values_buf == values_end) return;
        }
    }
}

static void handle_xcb_input_raw_motion( struct host_x11 *host, xcb_input_raw_motion_event_t *event )
{
    xcb_input_fp3232_t x = {0}, y = {0};

    if (!host_classes_are_valid( host )) return;

    extract_axisvalues( host, xcb_input_raw_button_press_valuator_mask( event ), xcb_input_raw_button_press_valuator_mask_length( event ),
                        xcb_input_raw_button_press_axisvalues_raw( event ), xcb_input_raw_button_press_axisvalues_raw_length( event ), &x, &y );
    send_raw_mouse_input( host, x, y, 0, 0 );
}


/* keyboard layout implementation */

struct layout
{
    struct list entry;

    LANGID lang;
    WORD index;
    /* "Layout Id", used by NtUserGetKeyboardLayoutName / LoadKeyboardLayoutW */
    WORD layout_id;

    xkb_group_index_t xkb_group;
    xkb_mod_mask_t shift_mask;
    xkb_mod_mask_t ctrl_mask;
    xkb_mod_mask_t alt_mask;
    xkb_mod_mask_t altgr_mask;
    xkb_mod_mask_t caps_mask;
    xkb_mod_mask_t num_mask;
    const USHORT *scan2vk;

    KBDTABLES tables;
    WCHAR key_names_buf[4096];
    VSC_LPWSTR key_names[0x100];
    VSC_LPWSTR key_names_ext[0x200];

    USHORT vsc2vk[0x100];
    VSC_VK vsc2vk_e0[0x100];
    VSC_VK vsc2vk_e1[0x100];

    VK_TO_WCHAR_TABLE vk_to_wchar_table[2];
    VK_TO_WCHARS8 vk_to_wchars8[0x100 * 2 /* SGCAPS */];
    VK_TO_BIT vk2bit[4];
    union
    {
        MODIFIERS modifiers;
        char modifiers_buf[offsetof(MODIFIERS, ModNumber[8])];
    };
};

C_ASSERT( sizeof(struct layout) <= 32 * 1024 );

static struct rxkb_context *rxkb_context;

#define EXTRA_SCAN2VK \
    T36 | KBDEXT, T37 | KBDMULTIVK, \
    T38, T39, T3A, T3B, T3C, T3D, T3E, T3F, \
    T40, T41, T42, T43, T44, T45 | KBDEXT | KBDMULTIVK, T46 | KBDMULTIVK, T47 | KBDNUMPAD | KBDSPECIAL, \
    T48 | KBDNUMPAD | KBDSPECIAL, T49 | KBDNUMPAD | KBDSPECIAL, T4A, T4B | KBDNUMPAD | KBDSPECIAL, \
    T4C | KBDNUMPAD | KBDSPECIAL, T4D | KBDNUMPAD | KBDSPECIAL, T4E, T4F | KBDNUMPAD | KBDSPECIAL, \
    T50 | KBDNUMPAD | KBDSPECIAL, T51 | KBDNUMPAD | KBDSPECIAL, T52 | KBDNUMPAD | KBDSPECIAL, \
    T53 | KBDNUMPAD | KBDSPECIAL, T54, T55, T56, T57, \
    T58, T59, T5A, T5B, T5C, T5D, T5E, T5F, \
    T60, T61, T62, T63, T64, T65, T66, T67, \
    T68, T69, T6A, T6B, T6C, T6D, T6E, T6F, \
    T70, T71, T72, T73, T74, T75, T76, T77, \
    T78, T79, T7A, T7B, T7C, T7D, T7E, \
    [0x110] = X10 | KBDEXT, [0x119] = X19 | KBDEXT, [0x11d] = X1D | KBDEXT, [0x120] = X20 | KBDEXT, \
    [0x121] = X21 | KBDEXT, [0x122] = X22 | KBDEXT, [0x124] = X24 | KBDEXT, [0x12e] = X2E | KBDEXT, \
    [0x130] = X30 | KBDEXT, [0x132] = X32 | KBDEXT, [0x135] = X35 | KBDEXT, [0x137] = X37 | KBDEXT, \
    [0x138] = X38 | KBDEXT, [0x147] = X47 | KBDEXT, [0x148] = X48 | KBDEXT, [0x149] = X49 | KBDEXT, \
    [0x14b] = X4B | KBDEXT, [0x14d] = X4D | KBDEXT, [0x14f] = X4F | KBDEXT, [0x150] = X50 | KBDEXT, \
    [0x151] = X51 | KBDEXT, [0x152] = X52 | KBDEXT, [0x153] = X53 | KBDEXT, [0x15b] = X5B | KBDEXT, \
    [0x15c] = X5C | KBDEXT, [0x15d] = X5D | KBDEXT, [0x15f] = X5F | KBDEXT, [0x165] = X65 | KBDEXT, \
    [0x166] = X66 | KBDEXT, [0x167] = X67 | KBDEXT, [0x168] = X68 | KBDEXT, [0x169] = X69 | KBDEXT, \
    [0x16a] = X6A | KBDEXT, [0x16b] = X6B | KBDEXT, [0x16c] = X6C | KBDEXT, [0x16d] = X6D | KBDEXT, \
    [0x11c] = X1C | KBDEXT, [0x146] = X46 | KBDEXT, [0x21d] = Y1D,

static const USHORT scan2vk_qwerty[0x280] =
{
    T00, T01, T02, T03, T04, T05, T06, T07, T08, T09, T0A, T0B, T0C, T0D, T0E,
    T0F, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T1A, T1B, T1C,
    T1D, T1E, T1F, T20, T21, T22, T23, T24, T25, T26, T27, T28, T29,
    T2A, T2B, T2C, T2D, T2E, T2F, T30, T31, T32, T33, T34, T35,
    EXTRA_SCAN2VK
};

static const USHORT scan2vk_azerty[0x280] =
{
    T00, T01, T02, T03, T04, T05, T06, T07, T08, T09, T0A, T0B, VK_OEM_4, T0D, T0E,
    T0F, 'A', 'Z', T12, T13, T14, T15, T16, T17, T18, T19, VK_OEM_6, VK_OEM_1, T1C,
    T1D, 'Q', T1F, T20, T21, T22, T23, T24, T25, T26, 'M', VK_OEM_3, VK_OEM_7,
    T2A, T2B, 'W', T2D, T2E, T2F, T30, T31, VK_OEM_COMMA, VK_OEM_PERIOD, VK_OEM_2, VK_OEM_8,
    EXTRA_SCAN2VK
};

static const USHORT scan2vk_qwertz[0x280] =
{
    T00, T01, T02, T03, T04, T05, T06, T07, T08, T09, T0A, T0B, VK_OEM_4, VK_OEM_6, T0E,
    T0F, T10, T11, T12, T13, T14, 'Z', T16, T17, T18, T19, VK_OEM_1, VK_OEM_3, T1C,
    T1D, T1E, T1F, T20, T21, T22, T23, T24, T25, T26, VK_OEM_7, VK_OEM_5, VK_OEM_2,
    T2A, VK_OEM_8, 'Y', T2D, T2E, T2F, T30, T31, T32, T33, T34, VK_OEM_MINUS,
    EXTRA_SCAN2VK
};

static const USHORT scan2vk_dvorak[0x280] =
{
    T00, T01, T02, T03, T04, T05, T06, T07, T08, T09, T0A, T0B, VK_OEM_4, VK_OEM_6, T0E,
    T0F, VK_OEM_7, VK_OEM_COMMA, VK_OEM_PERIOD, 'P', 'Y', 'F', 'G', 'C', 'R', 'L', VK_OEM_2, VK_OEM_PLUS, T1C,
    T1D, T1E, 'O', 'E', 'U', 'I', 'D', 'H', 'T', 'N', 'S', VK_OEM_MINUS, T29,
    T2A, T2B, VK_OEM_1, 'Q', 'J', 'K', 'X', 'B', 'M', 'W', 'V', 'Z',
    EXTRA_SCAN2VK
};

/* keep in sync with winewayland and winex11 */
static WORD key2scan( UINT key )
{
    /* base keys can be mapped directly */
    if (key <= KEY_KPDOT) return key;

    /* map keys found in KBDTABLES definitions (Txx Xxx Yxx macros) */
    switch (key)
    {
    case 84 /* ISO_Level3_Shift */: return 0x005a; /* T5A / VK_OEM_WSCTRL */
    case KEY_SYSRQ: return 0x0054; /* T54 / VK_SNAPSHOT */
    case KEY_102ND: return 0x0056; /* T56 / VK_OEM_102 */
    case KEY_F11: return 0x0057; /* T57 / VK_F11 */
    case KEY_F12: return 0x0058; /* T58 / VK_F12 */
    case KEY_LINEFEED: return 0x0059; /* T59 / VK_CLEAR */
    case KEY_EXIT: return 0x005b; /* T5B / VK_OEM_FINISH */
    case KEY_OPEN: return 0x005c; /* T5C / VK_OEM_JUMP */
    /* FIXME: map a KEY to T5D / VK_EREOF */
    /* FIXME: map a KEY to T5E / VK_OEM_BACKTAB */
    case KEY_COMPOSE: return 0x005f; /* T5F / VK_OEM_AUTO */
    case KEY_SCALE: return 0x0062; /* T62 / VK_ZOOM */
    case KEY_HELP: return 0x0063; /* T63 / VK_HELP */
    case KEY_F13: return 0x0064; /* T64 / VK_F13 */
    case KEY_F14: return 0x0065; /* T65 / VK_F14 */
    case KEY_F15: return 0x0066; /* T66 / VK_F15 */
    case KEY_F16: return 0x0067; /* T67 / VK_F16 */
    case KEY_F17: return 0x0068; /* T68 / VK_F17 */
    case KEY_F18: return 0x0069; /* T69 / VK_F18 */
    case KEY_F19: return 0x006a; /* T6A / VK_F19 */
    case KEY_F20: return 0x006b; /* T6B / VK_F20 */
    case KEY_F21: return 0x006c; /* T6C / VK_F21 */
    case KEY_F22: return 0x006d; /* T6D / VK_F22 */
    case KEY_F23: return 0x006e; /* T6E / VK_F23 */
    /* FIXME: map a KEY to T6F / VK_OEM_PA3 */
    case KEY_COMPUTER: return 0x0071; /* T71 / VK_OEM_RESET */
    /* FIXME: map a KEY to T73 / VK_ABNT_C1 */
    case KEY_F24: return 0x0076; /* T76 / VK_F24 */
    case KEY_KPPLUSMINUS: return 0x007b; /* T7B / VK_OEM_PA1 */
    /* FIXME: map a KEY to T7C / VK_TAB */
    /* FIXME: map a KEY to T7E / VK_ABNT_C2 */
    /* FIXME: map a KEY to T7F / VK_OEM_PA2 */
    case KEY_PREVIOUSSONG: return 0x0110; /* X10 / VK_MEDIA_PREV_TRACK */
    case KEY_NEXTSONG: return 0x0119; /* X19 / VK_MEDIA_NEXT_TRACK */
    case KEY_KPENTER: return 0x011c; /* X1C / VK_RETURN */
    case KEY_RIGHTCTRL: return 0x011d; /* X1D / VK_RCONTROL */
    case KEY_MUTE: return 0x0120; /* X20 / VK_VOLUME_MUTE */
    case KEY_PROG2: return 0x0121; /* X21 / VK_LAUNCH_APP2 */
    case KEY_PLAYPAUSE: return 0x0122; /* X22 / VK_MEDIA_PLAY_PAUSE */
    case KEY_STOPCD: return 0x0124; /* X24 / VK_MEDIA_STOP */
    case KEY_VOLUMEDOWN: return 0x012e; /* X2E / VK_VOLUME_DOWN */
    case KEY_VOLUMEUP: return 0x0130; /* X30 / VK_VOLUME_UP */
    case KEY_HOMEPAGE: return 0x0132; /* X32 / VK_BROWSER_HOME */
    case KEY_KPSLASH: return 0x0135; /* X35 / VK_DIVIDE */
    case KEY_PRINT: return 0x0137; /* X37 / VK_SNAPSHOT */
    case KEY_RIGHTALT: return 0x0138; /* X38 / VK_RMENU */
    case KEY_CANCEL: return 0x0146; /* X46 / VK_CANCEL */
    case KEY_HOME: return 0x0147; /* X47 / VK_HOME */
    case KEY_UP: return 0x0148; /* X48 / VK_UP */
    case KEY_PAGEUP: return 0x0149; /* X49 / VK_PRIOR */
    case KEY_LEFT: return 0x014b; /* X4B / VK_LEFT */
    case KEY_RIGHT: return 0x014d; /* X4D / VK_RIGHT */
    case KEY_END: return 0x014f; /* X4F / VK_END */
    case KEY_DOWN: return 0x0150; /* X50 / VK_DOWN */
    case KEY_PAGEDOWN: return 0x0151; /* X51 / VK_NEXT */
    case KEY_INSERT: return 0x0152; /* X52 / VK_INSERT */
    case KEY_DELETE: return 0x0153; /* X53 / VK_DELETE */
    case KEY_LEFTMETA: return 0x015b; /* X5B / VK_LWIN */
    case KEY_RIGHTMETA: return 0x015c; /* X5C / VK_RWIN */
    case KEY_MENU: return 0x015d; /* X5D / VK_APPS */
    case KEY_POWER: return 0x015e; /* X5E / VK_POWER */
    case KEY_SLEEP: return 0x015f; /* X5F / VK_SLEEP */
    case KEY_FIND: return 0x0165; /* X65 / VK_BROWSER_SEARCH */
    case KEY_BOOKMARKS: return 0x0166; /* X66 / VK_BROWSER_FAVORITES */
    case KEY_REFRESH: return 0x0167; /* X67 / VK_BROWSER_REFRESH */
    case KEY_STOP: return 0x0168; /* X68 / VK_BROWSER_STOP */
    case KEY_FORWARD: return 0x0169; /* X69 / VK_BROWSER_FORWARD */
    case KEY_BACK: return 0x016a; /* X6A / VK_BROWSER_BACK */
    case KEY_PROG1: return 0x016b; /* X6B / VK_LAUNCH_APP1 */
    case KEY_MAIL: return 0x016c; /* X6C / VK_LAUNCH_MAIL */
    case KEY_MEDIA: return 0x016d; /* X6D / VK_LAUNCH_MEDIA_SELECT */
    case KEY_PAUSE: return 0x021d; /* Y1D / VK_PAUSE */
    }

    /* otherwise just make up some extended scancode */
    return 0x200 | (key & 0x7f);
}

static LANGID langid_from_xkb_layout( const char *layout, size_t layout_len )
{
#define MAKEINDEX(c0, c1) (MAKEWORD(c0, c1) - MAKEWORD('a', 'a'))
    static const LANGID langids[] =
    {
        [MAKEINDEX('a','f')] = MAKELANGID(LANG_DARI, SUBLANG_DEFAULT),
        [MAKEINDEX('a','l')] = MAKELANGID(LANG_ALBANIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('a','m')] = MAKELANGID(LANG_ARMENIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('a','t')] = MAKELANGID(LANG_GERMAN, SUBLANG_GERMAN_AUSTRIAN),
        [MAKEINDEX('a','z')] = MAKELANGID(LANG_AZERBAIJANI, SUBLANG_DEFAULT),
        [MAKEINDEX('a','u')] = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_AUS),
        [MAKEINDEX('b','a')] = MAKELANGID(LANG_BOSNIAN, SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_CYRILLIC),
        [MAKEINDEX('b','d')] = MAKELANGID(LANG_BANGLA, SUBLANG_DEFAULT),
        [MAKEINDEX('b','e')] = MAKELANGID(LANG_FRENCH, SUBLANG_FRENCH_BELGIAN),
        [MAKEINDEX('b','g')] = MAKELANGID(LANG_BULGARIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('b','r')] = MAKELANGID(LANG_PORTUGUESE, 2),
        [MAKEINDEX('b','t')] = MAKELANGID(LANG_TIBETAN, 3),
        [MAKEINDEX('b','w')] = MAKELANGID(LANG_TSWANA, SUBLANG_TSWANA_BOTSWANA),
        [MAKEINDEX('b','y')] = MAKELANGID(LANG_BELARUSIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('c','a')] = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_CAN),
        [MAKEINDEX('c','d')] = MAKELANGID(LANG_FRENCH, SUBLANG_CUSTOM_UNSPECIFIED),
        [MAKEINDEX('c','h')] = MAKELANGID(LANG_GERMAN, SUBLANG_GERMAN_SWISS),
        [MAKEINDEX('c','m')] = MAKELANGID(LANG_FRENCH, 11),
        [MAKEINDEX('c','n')] = MAKELANGID(LANG_CHINESE, SUBLANG_DEFAULT),
        [MAKEINDEX('c','z')] = MAKELANGID(LANG_CZECH, SUBLANG_DEFAULT),
        [MAKEINDEX('d','e')] = MAKELANGID(LANG_GERMAN, SUBLANG_DEFAULT),
        [MAKEINDEX('d','k')] = MAKELANGID(LANG_DANISH, SUBLANG_DEFAULT),
        [MAKEINDEX('d','z')] = MAKELANGID(LANG_TAMAZIGHT, SUBLANG_TAMAZIGHT_ALGERIA_LATIN),
        [MAKEINDEX('e','e')] = MAKELANGID(LANG_ESTONIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('e','s')] = MAKELANGID(LANG_SPANISH, SUBLANG_DEFAULT),
        [MAKEINDEX('e','t')] = MAKELANGID(LANG_AMHARIC, SUBLANG_DEFAULT),
        [MAKEINDEX('f','i')] = MAKELANGID(LANG_FINNISH, SUBLANG_DEFAULT),
        [MAKEINDEX('f','o')] = MAKELANGID(LANG_FAEROESE, SUBLANG_DEFAULT),
        [MAKEINDEX('f','r')] = MAKELANGID(LANG_FRENCH, SUBLANG_DEFAULT),
        [MAKEINDEX('g','b')] = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_UK),
        [MAKEINDEX('g','e')] = MAKELANGID(LANG_GEORGIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('g','h')] = MAKELANGID(LANG_ENGLISH, SUBLANG_CUSTOM_UNSPECIFIED),
        [MAKEINDEX('g','n')] = MAKELANGID(LANG_NEUTRAL, SUBLANG_CUSTOM_DEFAULT),
        [MAKEINDEX('g','r')] = MAKELANGID(LANG_GREEK, SUBLANG_DEFAULT),
        [MAKEINDEX('h','r')] = MAKELANGID(LANG_CROATIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('h','u')] = MAKELANGID(LANG_HUNGARIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('i','d')] = MAKELANGID(LANG_INDONESIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('i','e')] = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_EIRE),
        [MAKEINDEX('i','l')] = MAKELANGID(LANG_HEBREW, SUBLANG_DEFAULT),
        [MAKEINDEX('i','n')] = MAKELANGID(LANG_HINDI, SUBLANG_DEFAULT),
        [MAKEINDEX('i','q')] = MAKELANGID(LANG_ARABIC, SUBLANG_ARABIC_IRAQ),
        [MAKEINDEX('i','r')] = MAKELANGID(LANG_PERSIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('i','s')] = MAKELANGID(LANG_ICELANDIC, SUBLANG_DEFAULT),
        [MAKEINDEX('i','t')] = MAKELANGID(LANG_ITALIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('j','p')] = MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT),
        [MAKEINDEX('k','e')] = MAKELANGID(LANG_NEUTRAL, SUBLANG_CUSTOM_DEFAULT),
        [MAKEINDEX('k','g')] = MAKELANGID(LANG_KYRGYZ, SUBLANG_DEFAULT),
        [MAKEINDEX('k','h')] = MAKELANGID(LANG_KHMER, SUBLANG_DEFAULT),
        [MAKEINDEX('k','r')] = MAKELANGID(LANG_KOREAN, SUBLANG_DEFAULT),
        [MAKEINDEX('k','z')] = MAKELANGID(LANG_KAZAK, SUBLANG_DEFAULT),
        [MAKEINDEX('l','a')] = MAKELANGID(LANG_LAO, SUBLANG_DEFAULT),
        [MAKEINDEX('l','k')] = MAKELANGID(LANG_SINHALESE, SUBLANG_DEFAULT),
        [MAKEINDEX('l','t')] = MAKELANGID(LANG_LITHUANIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('l','v')] = MAKELANGID(LANG_LATVIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('m','a')] = MAKELANGID(LANG_ARABIC, SUBLANG_ARABIC_MOROCCO),
        [MAKEINDEX('m','d')] = MAKELANGID(LANG_ROMANIAN, SUBLANG_CUSTOM_UNSPECIFIED),
        [MAKEINDEX('m','e')] = MAKELANGID(LANG_SERBIAN, SUBLANG_SERBIAN_MONTENEGRO_LATIN),
        [MAKEINDEX('m','k')] = MAKELANGID(LANG_MACEDONIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('m','l')] = MAKELANGID(LANG_NEUTRAL, SUBLANG_CUSTOM_DEFAULT),
        [MAKEINDEX('m','m')] = MAKELANGID(0x55 /*LANG_BURMESE*/, SUBLANG_DEFAULT),
        [MAKEINDEX('m','n')] = MAKELANGID(LANG_MONGOLIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('m','t')] = MAKELANGID(LANG_MALTESE, SUBLANG_DEFAULT),
        [MAKEINDEX('m','v')] = MAKELANGID(LANG_DIVEHI, SUBLANG_DEFAULT),
        [MAKEINDEX('m','y')] = MAKELANGID(LANG_MALAY, SUBLANG_DEFAULT),
        [MAKEINDEX('n','g')] = MAKELANGID(LANG_ENGLISH, SUBLANG_CUSTOM_UNSPECIFIED),
        [MAKEINDEX('n','l')] = MAKELANGID(LANG_DUTCH, SUBLANG_DEFAULT),
        [MAKEINDEX('n','o')] = MAKELANGID(LANG_NORWEGIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('n','p')] = MAKELANGID(LANG_NEPALI, SUBLANG_DEFAULT),
        [MAKEINDEX('p','h')] = MAKELANGID(LANG_FILIPINO, SUBLANG_DEFAULT),
        [MAKEINDEX('p','k')] = MAKELANGID(LANG_URDU, SUBLANG_DEFAULT),
        [MAKEINDEX('p','l')] = MAKELANGID(LANG_POLISH, SUBLANG_DEFAULT),
        [MAKEINDEX('p','t')] = MAKELANGID(LANG_PORTUGUESE, SUBLANG_DEFAULT),
        [MAKEINDEX('r','o')] = MAKELANGID(LANG_ROMANIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('r','s')] = MAKELANGID(LANG_SERBIAN, SUBLANG_SERBIAN_LATIN),
        [MAKEINDEX('r','u')] = MAKELANGID(LANG_RUSSIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('s','e')] = MAKELANGID(LANG_SWEDISH, SUBLANG_DEFAULT),
        [MAKEINDEX('s','i')] = MAKELANGID(LANG_SLOVENIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('s','k')] = MAKELANGID(LANG_SLOVAK, SUBLANG_DEFAULT),
        [MAKEINDEX('s','n')] = MAKELANGID(LANG_WOLOF, SUBLANG_DEFAULT),
        [MAKEINDEX('s','y')] = MAKELANGID(LANG_SYRIAC, SUBLANG_DEFAULT),
        [MAKEINDEX('t','g')] = MAKELANGID(LANG_FRENCH, SUBLANG_CUSTOM_UNSPECIFIED),
        [MAKEINDEX('t','h')] = MAKELANGID(LANG_THAI, SUBLANG_DEFAULT),
        [MAKEINDEX('t','j')] = MAKELANGID(LANG_TAJIK, SUBLANG_DEFAULT),
        [MAKEINDEX('t','m')] = MAKELANGID(LANG_TURKMEN, SUBLANG_DEFAULT),
        [MAKEINDEX('t','r')] = MAKELANGID(LANG_TURKISH, SUBLANG_DEFAULT),
        [MAKEINDEX('t','w')] = MAKELANGID(LANG_CHINESE, SUBLANG_CUSTOM_UNSPECIFIED),
        [MAKEINDEX('t','z')] = MAKELANGID(LANG_SWAHILI, SUBLANG_CUSTOM_UNSPECIFIED),
        [MAKEINDEX('u','a')] = MAKELANGID(LANG_UKRAINIAN, SUBLANG_DEFAULT),
        [MAKEINDEX('u','s')] = MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
        [MAKEINDEX('u','z')] = MAKELANGID(LANG_UZBEK, 2),
        [MAKEINDEX('v','n')] = MAKELANGID(LANG_VIETNAMESE, SUBLANG_DEFAULT),
        [MAKEINDEX('z','a')] = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_SOUTH_AFRICA),
    };
    LANGID langid;

    if (layout_len == 2 && (langid = langids[MAKEINDEX(layout[0], layout[1])])) return langid;
    if (layout_len == 3 && !memcmp( layout, "ara", layout_len )) return MAKELANGID(LANG_ARABIC, SUBLANG_DEFAULT);
    if (layout_len == 3 && !memcmp( layout, "epo", layout_len )) return MAKELANGID(LANG_NEUTRAL, SUBLANG_CUSTOM_DEFAULT);
    if (layout_len == 3 && !memcmp( layout, "mao", layout_len )) return MAKELANGID(LANG_MAORI, SUBLANG_DEFAULT);
    if (layout_len == 4 && !memcmp( layout, "brai", layout_len )) return MAKELANGID(LANG_NEUTRAL, SUBLANG_CUSTOM_DEFAULT);
    if (layout_len == 5 && !memcmp( layout, "latam", layout_len )) return MAKELANGID(LANG_SPANISH, SUBLANG_CUSTOM_UNSPECIFIED);
#undef MAKEINDEX

    FIXME( "Unknown layout language %s\n", debugstr_a(layout) );
    return MAKELANGID(LANG_NEUTRAL, SUBLANG_CUSTOM_UNSPECIFIED);
};

static unsigned int get_layout_hkl( struct layout *layout, LCID locale )
{
    if (!layout->layout_id) return MAKELONG(locale, layout->lang);
    else return MAKELONG(locale, 0xf000 | layout->layout_id);
}

static void add_xkb_layout( struct host_x11 *host, const char *xkb_layout, struct xkb_keymap *xkb_keymap,
                            xkb_layout_index_t xkb_group, LANGID lang )
{
    static WORD next_layout_id = 1;

    unsigned int i, mod, keyc, len, names_len, min_keycode, max_keycode;
    struct xkb_state *xkb_state = xkb_state_new( xkb_keymap );
    VSC_LPWSTR *names_entry, *names_ext_entry;
    VSC_VK *vsc2vk_e0_entry, *vsc2vk_e1_entry;
    VK_TO_WCHARS8 *vk2wchars_entry;
    struct layout *layout;
    WCHAR *names_str;
    WORD index = 0;

    min_keycode = xkb_keymap_min_keycode( xkb_keymap );
    max_keycode = xkb_keymap_max_keycode( xkb_keymap );

    TRACE( "host %p xkb_layout %s xkb_keymap %p xkb_group %u lang %04x\n",
           host, xkb_layout, xkb_keymap, xkb_group, lang );

    LIST_FOR_EACH_ENTRY( layout, &host->layouts, struct layout, entry )
        if (layout->lang == lang) index++;

    for (names_len = 0, keyc = min_keycode; keyc <= max_keycode; keyc++)
    {
        const xkb_keysym_t *keysym;
        if (!xkb_keymap_key_get_syms_by_level( xkb_keymap, keyc, xkb_group, 0, &keysym )) continue;
        names_len += xkb_keysym_get_name( *keysym, NULL, 0 ) + 1;
    }

    names_len *= sizeof(WCHAR);
    if (!(layout = calloc( 1, sizeof(*layout) + names_len )))
    {
        ERR( "host %p failed to allocate memory for layout entry\n", host );
        return;
    }

    layout->xkb_group = xkb_group;
    layout->lang = lang;
    layout->index = index;
    if (index) layout->layout_id = next_layout_id++;
    names_str = layout->key_names_buf;

    switch (lang)
    {
    case MAKELANGID(LANG_FRENCH, SUBLANG_DEFAULT): layout->scan2vk = scan2vk_azerty; break;
    case MAKELANGID(LANG_GERMAN, SUBLANG_DEFAULT): layout->scan2vk = scan2vk_qwertz; break;
    case MAKELANGID(LANG_GERMAN, SUBLANG_GERMAN_SWISS): layout->scan2vk = scan2vk_qwertz; break;
    default: layout->scan2vk = scan2vk_qwerty; break;
    }
    if (strstr( xkb_layout, "dvorak" )) layout->scan2vk = scan2vk_dvorak;

    layout->tables.pKeyNames = layout->key_names;
    layout->tables.pKeyNamesExt = layout->key_names_ext;
    layout->tables.bMaxVSCtoVK = 0xff;
    layout->tables.pusVSCtoVK = layout->vsc2vk;
    layout->tables.pVSCtoVK_E0 = layout->vsc2vk_e0;
    layout->tables.pVSCtoVK_E1 = layout->vsc2vk_e1;
    layout->tables.pCharModifiers = &layout->modifiers;
    layout->tables.pVkToWcharTable = layout->vk_to_wchar_table;
    layout->tables.fLocaleFlags = MAKELONG(KLLF_ALTGR, KBD_VERSION);

    layout->vk_to_wchar_table[0].pVkToWchars = (VK_TO_WCHARS1 *)layout->vk_to_wchars8;
    layout->vk_to_wchar_table[0].cbSize = sizeof(*layout->vk_to_wchars8);
    layout->vk_to_wchar_table[0].nModifications = 8;

    layout->vk2bit[0].Vk = VK_SHIFT;
    layout->vk2bit[0].ModBits = KBDSHIFT;
    layout->vk2bit[1].Vk = VK_CONTROL;
    layout->vk2bit[1].ModBits = KBDCTRL;
    layout->vk2bit[2].Vk = VK_MENU;
    layout->vk2bit[2].ModBits = KBDALT;

    layout->modifiers.pVkToBit = layout->vk2bit;
    for (mod = 0; mod <= (KBDSHIFT | KBDCTRL | KBDALT); ++mod)
    {
        BYTE num = 0;
        if (mod & KBDSHIFT) num |= 1 << 0;
        if (mod & KBDCTRL)  num |= 1 << 1;
        if (mod & KBDALT)   num |= 1 << 2;
        layout->modifiers.ModNumber[mod] = num;
    }
    layout->modifiers.wMaxModBits = 7;

    names_entry = layout->tables.pKeyNames;
    names_ext_entry = layout->tables.pKeyNamesExt;
    vsc2vk_e0_entry = layout->tables.pVSCtoVK_E0;
    vsc2vk_e1_entry = layout->tables.pVSCtoVK_E1;
    vk2wchars_entry = layout->vk_to_wchars8;

    for (keyc = min_keycode; keyc <= max_keycode; keyc++)
    {
        WORD scan = key2scan( keyc - 8 );
        const xkb_keysym_t *keysym;
        VSC_LPWSTR *entry;
        char name[256];

        if (!xkb_keymap_key_get_syms_by_level( xkb_keymap, keyc, xkb_group, 0, &keysym )) continue;
        len = xkb_keysym_get_name( *keysym, name, sizeof(name) );
        if (names_str + len + 1 >= layout->key_names_buf + ARRAY_SIZE(layout->key_names_buf)) break;

        if (!(scan & 0xff) || !len) continue;
        if (!(scan & 0x300)) entry = names_entry++;
        else entry = names_ext_entry++;

        entry->vsc = (BYTE)scan;
        entry->pwsz = names_str;
        for (i = 0; i < len; i++) entry->pwsz[i] = name[i];
        names_str += len + 1;

        TRACE( "host %p keyc %#04x, scan %#04x -> name %s\n", host, keyc, scan, debugstr_a(name) );
    }

    for (keyc = min_keycode; keyc <= max_keycode; keyc++)
    {
        WORD scan = key2scan( keyc - 8 ), vkey = layout->scan2vk[scan];
        VSC_VK *entry = NULL;

        if (!(scan & 0xff) || !vkey) continue;
        if (scan & 0x100) entry = vsc2vk_e0_entry++;
        else if (scan & 0x200) entry = vsc2vk_e1_entry++;
        else layout->tables.pusVSCtoVK[scan & 0xff] = vkey;

        if (entry)
        {
            entry->Vsc = scan & 0xff;
            entry->Vk = vkey;
        }

        TRACE( "host %p keyc %#04x, scan %#05x -> vkey %#06x\n", host, keyc, scan, vkey );
    }

    layout->shift_mask = 1 << xkb_keymap_mod_get_index( xkb_keymap, XKB_MOD_NAME_SHIFT );
    layout->ctrl_mask = 1 << xkb_keymap_mod_get_index( xkb_keymap, XKB_MOD_NAME_CTRL );
    layout->alt_mask = 1 << xkb_keymap_mod_get_index( xkb_keymap, XKB_MOD_NAME_ALT );
    layout->altgr_mask = 1 << xkb_keymap_mod_get_index( xkb_keymap, "Mod5" );
    layout->caps_mask = 1 << xkb_keymap_mod_get_index( xkb_keymap, XKB_MOD_NAME_CAPS );
    layout->num_mask = 1 << xkb_keymap_mod_get_index( xkb_keymap, XKB_MOD_NAME_NUM );

    for (keyc = min_keycode; keyc <= max_keycode; keyc++)
    {
        WORD scan = key2scan( keyc - 8 ), vkey = layout->scan2vk[scan];
        VK_TO_WCHARS8 vkey2wch = {.VirtualKey = vkey}, caps_vkey2wch = vkey2wch;
        BOOL found = FALSE, caps_found = FALSE;
        uint32_t caps_ret, shift_ret;
        unsigned int mod;

        for (mod = 0; mod < 8; ++mod)
        {
            xkb_mod_mask_t mod_mask = 0;
            uint32_t ret;

            if (mod & (1 << 0)) mod_mask |= layout->shift_mask;
            if (mod & (1 << 1)) mod_mask |= layout->ctrl_mask;
            /* Windows uses VK_CTRL + VK_MENU for AltGr, we cannot combine Ctrl and Alt */
            if (mod & (1 << 2)) mod_mask = (mod_mask & ~layout->ctrl_mask) | layout->altgr_mask;

            xkb_state_update_mask( xkb_state, 0, 0, mod_mask, 0, 0, xkb_group );

            if (mod_mask & layout->ctrl_mask) vkey2wch.wch[mod] = WCH_NONE; /* on Windows CTRL+key behave specifically */
            else if (!(ret = xkb_state_key_get_utf32( xkb_state, keyc ))) vkey2wch.wch[mod] = WCH_NONE;
            else vkey2wch.wch[mod] = ret;

            if (vkey2wch.wch[mod] != WCH_NONE) found = TRUE;

            xkb_state_update_mask( xkb_state, 0, 0, mod_mask | layout->caps_mask, 0, 0, xkb_group );

            if (mod_mask & layout->ctrl_mask) caps_vkey2wch.wch[mod] = WCH_NONE; /* on Windows CTRL+key behave specifically */
            else if (!(ret = xkb_state_key_get_utf32( xkb_state, keyc ))) caps_vkey2wch.wch[mod] = WCH_NONE;
            else if (ret == vkey2wch.wch[mod]) caps_vkey2wch.wch[mod] = WCH_NONE;
            else caps_vkey2wch.wch[mod] = ret;

            if (caps_vkey2wch.wch[mod] != WCH_NONE) caps_found = TRUE;
        }

        if (!found) continue;

        if (caps_found)
        {
            TRACE( "host %p vkey %#06x + CAPS -> {\\%x,\\%x,\\%x,\\%x,\\%x,\\%x,\\%x,\\%x}\n",
                   host, caps_vkey2wch.VirtualKey, caps_vkey2wch.wch[0], caps_vkey2wch.wch[1],
                   caps_vkey2wch.wch[2], caps_vkey2wch.wch[3], caps_vkey2wch.wch[4],
                   caps_vkey2wch.wch[5], caps_vkey2wch.wch[6], caps_vkey2wch.wch[7] );
            caps_vkey2wch.Attributes = SGCAPS;
            *vk2wchars_entry++ = caps_vkey2wch;
        }
        else
        {
            xkb_state_update_mask( xkb_state, 0, 0, layout->caps_mask, 0, 0, xkb_group );
            caps_ret = xkb_state_key_get_utf32( xkb_state, keyc );
            xkb_state_update_mask( xkb_state, 0, 0, layout->shift_mask, 0, 0, xkb_group );
            shift_ret = xkb_state_key_get_utf32( xkb_state, keyc );
            if (caps_ret && caps_ret == shift_ret) vkey2wch.Attributes |= CAPLOK;
        }

        TRACE( "host %p vkey %#06x -> {\\%x,\\%x,\\%x,\\%x,\\%x,\\%x,\\%x,\\%x}\n", host, vkey2wch.VirtualKey,
               caps_vkey2wch.wch[0], caps_vkey2wch.wch[1], caps_vkey2wch.wch[2], caps_vkey2wch.wch[3],
               caps_vkey2wch.wch[4], caps_vkey2wch.wch[5], caps_vkey2wch.wch[6], caps_vkey2wch.wch[7] );
        *vk2wchars_entry++ = vkey2wch;
    }

    xkb_state_unref( xkb_state );

    TRACE( "Created layout entry=%p index=%04x lang=%04x id=%04x\n", layout, layout->index,
           layout->lang, layout->layout_id );
    list_add_tail( &host->layouts, &layout->entry );

    /* make the pointer relative to the tables to make sending to the client easier */
#define KBDTABLES_FIXUP_POINTER( base, ptr ) (ptr) = (void *)((char *)(ptr) - (char *)(base))

    for (names_entry = layout->tables.pKeyNames; names_entry->vsc; names_entry++)
        KBDTABLES_FIXUP_POINTER( &layout->tables, names_entry->pwsz );
    for (names_entry = layout->tables.pKeyNamesExt; names_entry->vsc; names_entry++)
        KBDTABLES_FIXUP_POINTER( &layout->tables, names_entry->pwsz );

    KBDTABLES_FIXUP_POINTER( &layout->tables, layout->tables.pVkToWcharTable[0].pVkToWchars );
    KBDTABLES_FIXUP_POINTER( &layout->tables, layout->tables.pCharModifiers->pVkToBit );

    KBDTABLES_FIXUP_POINTER( &layout->tables, layout->tables.pKeyNames );
    KBDTABLES_FIXUP_POINTER( &layout->tables, layout->tables.pKeyNamesExt );
    KBDTABLES_FIXUP_POINTER( &layout->tables, layout->tables.pusVSCtoVK );
    KBDTABLES_FIXUP_POINTER( &layout->tables, layout->tables.pVSCtoVK_E0 );
    KBDTABLES_FIXUP_POINTER( &layout->tables, layout->tables.pVSCtoVK_E1 );
    KBDTABLES_FIXUP_POINTER( &layout->tables, layout->tables.pCharModifiers );
    KBDTABLES_FIXUP_POINTER( &layout->tables, layout->tables.pVkToWcharTable );

#undef KBDTABLES_FIXUP_POINTER
}

static void set_current_xkb_group( struct host_x11 *host, xkb_layout_index_t xkb_group )
{
    struct window_data *data;
    struct layout *layout;

    LIST_FOR_EACH_ENTRY( layout, &host->layouts, struct layout, entry )
        if (layout->xkb_group == xkb_group) break;
    if (&layout->entry == &host->layouts)
    {
        ERR( "host %p failed to find layout for group %d\n", host, xkb_group );
        layout = host->active_layout;
    }

    if (layout == host->active_layout) return;
    host->active_layout = layout;

    if (!(data = window_data_lookup( host, compare_x11_win, &host->keyboard_focus )))
    {
        ERR( "host %p failed to find window %#x\n", host, host->keyboard_focus );
        return;
    }

    TRACE( "host %p changing keyboard layout to %#x\n", host, get_layout_hkl( layout, layout->lang ) );
    post_message( data->window, WM_INPUTLANGCHANGEREQUEST, 0 /*FIXME*/,
                  get_layout_hkl( layout, layout->lang ) );
}

static BOOL find_xkb_layout_variant( const char *name, const char **layout, const char **variant )
{
    struct rxkb_layout *iter;

    for (iter = rxkb_layout_first( rxkb_context ); iter; iter = rxkb_layout_next( iter ))
    {
        if (!strcmp( name, rxkb_layout_get_description( iter ) ))
        {
            *layout = rxkb_layout_get_name( iter );
            *variant = rxkb_layout_get_variant( iter );
            return TRUE;
        }
    }

    return FALSE;
}

struct set_desktop_keystate_params
{
    unsigned int vkey;
    int locked;
};

static int set_desktop_keystate( struct process *process, void *arg )
{
    struct set_desktop_keystate_params *params = arg;
    struct desktop *desktop;

    if ((desktop = get_desktop_obj( process, process->desktop, 0 )))
    {
        if (params->locked) desktop->keystate[params->vkey] = 0x01;
        else desktop->keystate[params->vkey] = 0x00;
        release_object( desktop );
    }

    return 0;
}

static void update_mod_lock_state( struct host_x11 *host, struct xkb_state *state, const char *name, UINT vkey )
{
    struct set_desktop_keystate_params params = {.vkey = vkey};
    params.locked = xkb_state_mod_name_is_active( state, name, XKB_STATE_MODS_LOCKED );
ERR("name %s vkey %#x msg %#x\n", name, vkey, params.locked);
    enum_processes_for_host( &host->obj, set_desktop_keystate, &params );
}

static void update_keyboard_lock_state( struct host_x11 *host )
{
    struct xkb_context *context;
    struct xkb_keymap *keymap;
    struct xkb_state *state;
    xcb_xkb_group_t group;

    if (!(context = xkb_context_new( XKB_CONTEXT_NO_FLAGS )))
        return;
    if (!(keymap = xkb_x11_keymap_new_from_device( context, host->xcb, host->keyboard_id,
                                                   XKB_KEYMAP_COMPILE_NO_FLAGS )))
    {
        xkb_context_unref( context );
        return;
    }
    if (!(state = xkb_x11_state_new_from_device( keymap, host->xcb, host->keyboard_id )))
    {
        xkb_keymap_unref( keymap );
        xkb_context_unref( context );
        return;
    }

    for (group = 0; group < xkb_keymap_num_layouts( keymap ); group++)
    {
        if (xkb_state_layout_index_is_active( state, group, XKB_STATE_LAYOUT_EFFECTIVE ))
            break;
    }

    set_current_xkb_group( host, group );
    update_mod_lock_state( host, state, XKB_MOD_NAME_CAPS, VK_CAPITAL );
    update_mod_lock_state( host, state, XKB_MOD_NAME_NUM, VK_NUMLOCK );

    xkb_state_unref( state );
    xkb_keymap_unref( keymap );
    xkb_context_unref( context );
}

static BOOL update_keyboard_layout_tables( struct host_x11 *host )
{
    struct layout *layout, *next;
    struct xkb_context *context;
    struct xkb_keymap *keymap;
    xcb_xkb_group_t group;

    if (!(context = xkb_context_new( XKB_CONTEXT_NO_FLAGS )))
        return FALSE;
    if (!(keymap = xkb_x11_keymap_new_from_device( context, host->xcb, host->keyboard_id,
                                                   XKB_KEYMAP_COMPILE_NO_FLAGS )))
    {
        xkb_context_unref( context );
        return FALSE;
    }

    host->active_layout = NULL;
    LIST_FOR_EACH_ENTRY_SAFE( layout, next, &host->layouts, struct layout, entry )
    {
        list_remove( &layout->entry );
        free( layout );
    }

    for (group = 0; group < xkb_keymap_num_layouts( keymap ); group++)
    {
        const char *layout_name = xkb_keymap_layout_get_name( keymap, group );
        const char *layout, *variant = NULL;
        int layout_len, variant_len = 0;
        char buffer[1024];
        LANGID lang;

        if (!find_xkb_layout_variant( layout_name, &layout, &variant )) layout = "us";
        if (variant) variant_len = strlen( variant );
        layout_len = strlen( layout );

        TRACE( "host %p found layout %u name %s -> %s:%s\n", host, group,
               layout_name, layout, variant ? variant : "" );

        lang = langid_from_xkb_layout( layout, layout_len );
        snprintf( buffer, ARRAY_SIZE(buffer), "%.*s:%.*s", layout_len, layout, variant_len, variant );
        add_xkb_layout( host, buffer, keymap, group, lang );
    }

    xkb_keymap_unref( keymap );
    xkb_context_unref( context );

    return TRUE;
}


/* send keyboard input received on an x11 window */
static void send_keyboard_input( struct host_x11 *host, struct window_data *data,
                                 unsigned int scan, int pressed )
{
    hw_input_t input = {.kbd = {.type = INPUT_KEYBOARD}};
    unsigned int origin = IMO_HARDWARE;
    struct desktop *desktop;

    if (!(desktop = get_hardware_input_desktop( data->window ))) return;
    if (!set_input_desktop( desktop->winstation, desktop ))
    {
        release_object( desktop );
        return;
    }

    if ((current = get_foreground_thread( desktop, data->window )))
    {
        const USHORT *scan2vk = host->active_layout ? host->active_layout->scan2vk : NULL;

        if (!pressed) input.kbd.flags |= KEYEVENTF_KEYUP;
        input.kbd.scan = scan & 0xff;
        input.kbd.vkey = scan2vk ? scan2vk[scan] : VK_NONAME;
        if (scan & 0x300) input.kbd.flags |= KEYEVENTF_EXTENDEDKEY;

        if (!(input.kbd.vkey & KBDNUMPAD) || !(desktop->keystate[VK_NUMLOCK] & 1))
            input.kbd.vkey &= 0xff;
        else switch (input.kbd.vkey & 0xff)
        {
        case VK_DELETE: input.kbd.vkey = VK_DECIMAL; break;
        case VK_INSERT: input.kbd.vkey = VK_NUMPAD0; break;
        case VK_END: input.kbd.vkey = VK_NUMPAD1; break;
        case VK_DOWN: input.kbd.vkey = VK_NUMPAD2; break;
        case VK_NEXT: input.kbd.vkey = VK_NUMPAD3; break;
        case VK_LEFT: input.kbd.vkey = VK_NUMPAD4; break;
        case VK_CLEAR: input.kbd.vkey = VK_NUMPAD5; break;
        case VK_RIGHT: input.kbd.vkey = VK_NUMPAD6; break;
        case VK_HOME: input.kbd.vkey = VK_NUMPAD7; break;
        case VK_UP: input.kbd.vkey = VK_NUMPAD8; break;
        case VK_PRIOR: input.kbd.vkey = VK_NUMPAD9; break;
        }

        TRACE( "host %p window %#x -> %#x scan %#x vkey %#x flags %#x\n", host, data->x11_win, data->window,
               input.kbd.scan, input.kbd.vkey, input.kbd.flags );

        queue_keyboard_message( desktop, data->window, &input, origin, NULL );
        release_object( current );
        current = NULL;
    }

    release_object( desktop );
}

static void handle_xcb_xkb_new_keyboard_notify( struct host_x11 *host, xcb_xkb_new_keyboard_notify_event_t *event )
{
    TRACE( "host %p sequence %u time %u device %u\n", host, event->sequence, event->time, event->oldDeviceID );

    if (event->oldDeviceID != host->keyboard_id) return;
    host->keyboard_id = event->deviceID;

    update_keyboard_layout_tables( host );
    update_keyboard_lock_state( host );
}


struct update_desktop_keystate_params
{
    unsigned int msg;
    lparam_t wparam;
};

static int update_desktop_keystate( struct process *process, void *arg )
{
    struct update_desktop_keystate_params *params = arg;
    struct desktop *desktop;

    if ((desktop = get_desktop_obj( process, process->desktop, 0 )))
    {
        update_input_key_state( desktop, desktop->keystate, params->msg, params->wparam );
        release_object( desktop );
    }

    return 0;
}

/* send raw keyboard input received globally */
static void send_raw_keyboard_input( struct host_x11 *host, unsigned int scan, int pressed )
{
    struct hw_msg_source source = {IMDT_UNAVAILABLE, IMO_HARDWARE};
    struct update_desktop_keystate_params state_params;
    struct thread *foreground;
    struct desktop *desktop;

    if (!(desktop = get_hardware_input_desktop( 0 ))) return;
    if (!set_input_desktop( desktop->winstation, desktop ))
    {
        release_object( desktop );
        return;
    }

    if ((foreground = get_foreground_thread( desktop, 0 )))
    {
        unsigned short vkey = host->active_layout->scan2vk[scan];
        struct rawinput_message raw_msg = {0};
        raw_msg.foreground = foreground;
        raw_msg.source     = source;
        raw_msg.time       = get_tick_count();
        raw_msg.message    = WM_INPUT;
        raw_msg.flags      = 0;
        rawkeyboard_init( &raw_msg.rawinput, &raw_msg.data.keyboard, scan, vkey,
                          0, pressed ? WM_KEYDOWN : WM_KEYUP, 0 );

        ERR( "host %p raw scan %#x vkey %#x pressed %u\n", host, scan, vkey, pressed );

        /* FIXME */
        if (0) enum_processes( update_desktop_keystate, &state_params );

        dispatch_rawinput_message( desktop, &raw_msg );
        release_object( foreground );
    }

    release_object( desktop );
}

static void handle_xcb_xkb_state_notify( struct host_x11 *host, xcb_xkb_state_notify_event_t *event )
{
    unsigned int key = event->keycode - 8;
    int pressed;

    TRACE( "host %p sequence %u time %u device %u\n", host, event->sequence, event->time, event->deviceID );

    if (event->deviceID != host->keyboard_id) return;

    set_current_xkb_group( host, event->group );

    pressed = event->eventType == XCB_KEY_PRESS;
    /* NOTE: Windows normally sends VK_CONTROL + VK_MENU only if the layout has KLLF_ALTGR */
    if (key == KEY_RIGHTALT) send_raw_keyboard_input( host, key2scan( KEY_RIGHTCTRL ), pressed );
    send_raw_keyboard_input( host, key2scan( key ), pressed );
}

static void handle_xcb_xkb_map_notify( struct host_x11 *host, xcb_xkb_map_notify_event_t *event )
{
    TRACE( "host %p sequence %u time %u device %u\n", host, event->sequence, event->time, event->deviceID );

    if (event->deviceID != host->keyboard_id) return;

    update_keyboard_layout_tables( host );
    update_keyboard_lock_state( host );
}

static void handle_xcb_xkb_event( struct host_x11 *host, xcb_generic_event_t *event )
{
    switch (event->pad0)
    {
    case XCB_XKB_NEW_KEYBOARD_NOTIFY:
        handle_xcb_xkb_new_keyboard_notify( host, (xcb_xkb_new_keyboard_notify_event_t *)event );
        break;
    case XCB_XKB_MAP_NOTIFY:
        handle_xcb_xkb_map_notify( host, (xcb_xkb_map_notify_event_t *)event );
        break;
    case XCB_XKB_STATE_NOTIFY:
        handle_xcb_xkb_state_notify( host, (xcb_xkb_state_notify_event_t *)event );
        break;
    default:
        WARN( "host %p unexpected event type %u\n", host, event->pad0 );
        break;
    }
}

static void handle_xcb_input_focus_in( struct host_x11 *host, xcb_input_focus_in_event_t *event )
{
    struct window_data *data;

    TRACE( "host %p sequence %u time %u device %u\n", host, event->full_sequence, event->time, event->deviceid );

    if (event->detail >= XCB_INPUT_NOTIFY_DETAIL_POINTER) return;

    host->keyboard_grabbed = event->mode == XCB_INPUT_NOTIFY_MODE_GRAB ||
                             event->mode == XCB_INPUT_NOTIFY_MODE_WHILE_GRABBED;
    /* ignore wm specific NotifyUngrab / NotifyGrab events w.r.t focus */
    if (event->mode == XCB_INPUT_NOTIFY_MODE_GRAB || event->mode == XCB_INPUT_NOTIFY_MODE_UNGRAB) return;

    if (!(data = window_data_lookup( host, compare_x11_win, &event->event )))
    {
        ERR( "host %p failed to find window %#x\n", host, event->event );
        host->keyboard_focus = 0;
        return;
    }

    host->keyboard_focus = event->event;
    update_keyboard_lock_state( host );
    post_message( data->window, WM_INPUTLANGCHANGEREQUEST, 0 /*FIXME*/,
                  get_layout_hkl( host->active_layout, host->active_layout->lang ) );
}

static void handle_xcb_input_focus_out( struct host_x11 *host, xcb_input_focus_out_event_t *event )
{
    TRACE( "host %p sequence %u time %u device %u\n", host, event->full_sequence, event->time, event->deviceid );

    if (event->detail >= XCB_INPUT_NOTIFY_DETAIL_POINTER) return;

    host->keyboard_grabbed = event->mode == XCB_INPUT_NOTIFY_MODE_GRAB ||
                             event->mode == XCB_INPUT_NOTIFY_MODE_WHILE_GRABBED;
    /* ignore wm specific NotifyUngrab / NotifyGrab events w.r.t focus */
    if (event->mode == XCB_INPUT_NOTIFY_MODE_GRAB || event->mode == XCB_INPUT_NOTIFY_MODE_UNGRAB) return;

    if (host->keyboard_focus != event->event) return;
    host->keyboard_focus = 0;
}

static void handle_xcb_input_key_press( struct host_x11 *host, xcb_input_key_press_event_t *event )
{
    unsigned int key = event->detail - 8;
    struct window_data *data;

    TRACE( "host %p sequence %u time %u device %u\n", host, event->full_sequence, event->time, event->deviceid );

    if (!(data = window_data_lookup( host, compare_x11_win, &event->event )))
    {
        ERR( "host %p failed to find window %#x\n", host, event->event );
        return;
    }

    /* NOTE: Windows normally sends VK_CONTROL + VK_MENU only if the layout has KLLF_ALTGR */
    if (key == KEY_RIGHTALT) send_keyboard_input( host, data, key2scan( KEY_RIGHTCTRL ), 1 );
    send_keyboard_input( host, data, key2scan( key ), 1 );
}

static void handle_xcb_input_key_release( struct host_x11 *host, xcb_input_key_release_event_t *event )
{
    unsigned int key = event->detail - 8;
    struct window_data *data;

    TRACE( "host %p sequence %u time %u device %u\n", host, event->full_sequence, event->time, event->deviceid );

    if (!(data = window_data_lookup( host, compare_x11_win, &event->event )))
    {
        ERR( "host %p failed to find window %#x\n", host, event->event );
        return;
    }

    /* NOTE: Windows normally sends VK_CONTROL + VK_MENU only if the layout has KLLF_ALTGR */
    if (key == KEY_RIGHTALT) send_keyboard_input( host, data, key2scan( KEY_RIGHTCTRL ), 0 );
    send_keyboard_input( host, data, key2scan( key ), 0 );
}


/* send absolute touch input received on an x11 window */
static void send_touch_input( struct host_x11 *host, xcb_window_t x11_win, xcb_input_fp1616_t x,
                              xcb_input_fp1616_t y, unsigned int msg, unsigned int id, unsigned int flags )
{
    hw_input_t input = {.hw = {.type = INPUT_HARDWARE}};
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
        int virtual_x, virtual_y;
        rectangle_t virtual_rect;

        get_top_window_rectangle( desktop, &virtual_rect );
        virtual_x = fp1616_round( x ) * UINT16_MAX / (virtual_rect.right - virtual_rect.left);
        virtual_y = fp1616_round( y ) * UINT16_MAX / (virtual_rect.bottom - virtual_rect.top);

        input.hw.wparam = MAKELONG( id, POINTER_MESSAGE_FLAG_INRANGE | POINTER_MESSAGE_FLAG_INCONTACT | flags );
        input.hw.lparam = MAKELPARAM( virtual_x, virtual_y );

        TRACE( "host %p window %#x -> %#x pos (%+8.2f,%+8.2f) -> (%+5d,%+5d)\n", host, x11_win, data->window,
               double_from_fp1616( x ), double_from_fp1616( y ), virtual_x, virtual_y );

        queue_custom_hardware_message( desktop, data->window, origin, &input );
        release_object( current );
        current = NULL;
    }

    release_object( desktop );
}

static void handle_xcb_input_touch_event( struct host_x11 *host, xcb_input_touch_begin_event_t *event )
{
    TRACE( "host %p sequence %u time %u device %u\n", host, event->full_sequence, event->time, event->deviceid );

    switch (event->event_type)
    {
    case XCB_INPUT_TOUCH_BEGIN:
        send_touch_input( host, event->event, event->root_x, event->root_y, WM_POINTERDOWN, event->detail, POINTER_MESSAGE_FLAG_NEW );
        break;
    case XCB_INPUT_TOUCH_UPDATE:
        send_touch_input( host, event->event, event->root_x, event->root_y, WM_POINTERUPDATE, event->detail, 0 );
        break;
    case XCB_INPUT_TOUCH_END:
        send_touch_input( host, event->event, event->root_x, event->root_y, WM_POINTERUP, event->detail, 0 );
        break;
    }
}


static void handle_xcb_ge_event( struct host_x11 *host, xcb_ge_event_t *event )
{
    switch (event->event_type)
    {
    case XCB_INPUT_FOCUS_IN: handle_xcb_input_focus_in( host, (xcb_input_focus_in_event_t *)event ); break;
    case XCB_INPUT_FOCUS_OUT: handle_xcb_input_focus_out( host, (xcb_input_focus_out_event_t *)event ); break;
    case XCB_INPUT_KEY_PRESS: handle_xcb_input_key_press( host, (xcb_input_key_press_event_t *)event ); break;
    case XCB_INPUT_KEY_RELEASE: handle_xcb_input_key_release( host, (xcb_input_key_release_event_t *)event ); break;

    case XCB_INPUT_BUTTON_PRESS: handle_xcb_input_button_press( host, (xcb_input_button_press_event_t *)event ); break;
    case XCB_INPUT_BUTTON_RELEASE: handle_xcb_input_button_release( host, (xcb_input_button_release_event_t *)event ); break;
    case XCB_INPUT_MOTION: handle_xcb_input_motion( host, (xcb_input_motion_event_t *)event ); break;
    case XCB_INPUT_ENTER: handle_xcb_input_enter( host, (xcb_input_enter_event_t *)event ); break;
    case XCB_INPUT_LEAVE: handle_xcb_input_leave( host, (xcb_input_enter_event_t *)event ); break;

    case XCB_INPUT_DEVICE_CHANGED: handle_xcb_input_device_changed( host, (xcb_input_device_changed_event_t *)event ); break;
    case XCB_INPUT_RAW_MOTION: handle_xcb_input_raw_motion( host, (xcb_input_raw_motion_event_t *)event ); break;

    case XCB_INPUT_TOUCH_BEGIN: handle_xcb_input_touch_event( host, (xcb_input_touch_begin_event_t *)event ); break;
    case XCB_INPUT_TOUCH_UPDATE: handle_xcb_input_touch_event( host, (xcb_input_touch_update_event_t *)event ); break;
    case XCB_INPUT_TOUCH_END: handle_xcb_input_touch_event( host, (xcb_input_touch_end_event_t *)event ); break;

    default: WARN( "host %p unexpected event type %u\n", host, event->event_type ); break;
    }
}

static void handle_xcb_generic_error( struct host_x11 *host, xcb_generic_error_t *error )
{
    const char *label = xcb_event_get_error_label( error->error_code );
    ERR( "host %p got error %s sequence %u resource %#x major %u minor %u\n", host, debugstr_a(label),
         error->sequence, error->resource_id, error->minor_code, error->major_code );
}


/* warp the host cursor to the current desktop cursor position */
static void host_x11_warp_cursor( struct object *obj, struct desktop *desktop )
{
    int x = desktop->shared->cursor.x, y = desktop->shared->cursor.y;
    struct host_x11 *host = (struct host_x11 *)obj;
    xcb_input_fp1616_t warp_x, warp_y;
    xcb_void_cookie_t cookie;

    assert( obj->ops == &host_x11_object_ops );

    virtual_screen_to_root( desktop, &x, &y );
    warp_x = x << 16;
    warp_y = y << 16;

    cookie = xcb_input_xi_warp_pointer( host->xcb, XCB_NONE, host->root_window, 0, 0, 0, 0,
                                        warp_x, warp_y, host->pointer_id );
    if (!debug_level) xcb_discard_reply( host->xcb, cookie.sequence );

    TRACE( "host %p warping to (%+5d,%+5d) -> (%+8.2f,%+8.2f) cookie %u\n", host, x, y,
           double_from_fp1616( warp_x ), double_from_fp1616( warp_y ), cookie.sequence );

    host->warp_sequence = cookie.sequence;
    host_set_needs_flush( host );
}


/* confine the host cursor in the given desktop rectangle / release the cursor from its confinement */
static void host_x11_clip_cursor( struct object *obj, struct desktop *desktop, const rectangle_t *rect )
{
    struct host_x11 *host = (struct host_x11 *)obj;
    xcb_void_cookie_t cookie;

    assert( obj->ops == &host_x11_object_ops );

    if (host->is_clipping)
    {
        cookie = xcb_xfixes_delete_pointer_barrier( host->xcb, host->clip_barrier[0] );
        if (!debug_level) xcb_discard_reply( host->xcb, cookie.sequence );
        cookie = xcb_xfixes_delete_pointer_barrier( host->xcb, host->clip_barrier[1] );
        if (!debug_level) xcb_discard_reply( host->xcb, cookie.sequence );
        cookie = xcb_xfixes_delete_pointer_barrier( host->xcb, host->clip_barrier[2] );
        if (!debug_level) xcb_discard_reply( host->xcb, cookie.sequence );
        cookie = xcb_xfixes_delete_pointer_barrier( host->xcb, host->clip_barrier[3] );
        if (!debug_level) xcb_discard_reply( host->xcb, cookie.sequence );

        host->is_clipping = 0;
        host_set_needs_flush( host );
    }

    if (!rect) TRACE( "host %p releasing cursor\n", host );
    else
    {
        rectangle_t host_rect = *rect;

        virtual_screen_to_root( desktop, &host_rect.left, &host_rect.top );
        virtual_screen_to_root( desktop, &host_rect.right, &host_rect.bottom );

        TRACE( "host %p clipping cursor to (%+5d,%+5d)x(%+5d,%+5d) -> (%+5d,%+5d)x(%+5d,%+5d)\n", host,
               rect->left, rect->top, rect->right, rect->bottom, host_rect.left, host_rect.top,
               host_rect.right, host_rect.bottom );

        cookie = xcb_xfixes_create_pointer_barrier( host->xcb, host->clip_barrier[0], host->root_window,
                                                    0, host_rect.top, UINT16_MAX, host_rect.top,
                                                    XCB_XFIXES_BARRIER_DIRECTIONS_POSITIVE_Y, 0, NULL );
        if (!debug_level) xcb_discard_reply( host->xcb, cookie.sequence );
        cookie = xcb_xfixes_create_pointer_barrier( host->xcb, host->clip_barrier[1], host->root_window,
                                                    0, host_rect.bottom, UINT16_MAX, host_rect.bottom,
                                                    XCB_XFIXES_BARRIER_DIRECTIONS_NEGATIVE_Y, 0, NULL );
        if (!debug_level) xcb_discard_reply( host->xcb, cookie.sequence );
        cookie = xcb_xfixes_create_pointer_barrier( host->xcb, host->clip_barrier[2], host->root_window,
                                                    host_rect.left, 0, host_rect.left, UINT16_MAX,
                                                    XCB_XFIXES_BARRIER_DIRECTIONS_POSITIVE_X, 0, NULL );
        if (!debug_level) xcb_discard_reply( host->xcb, cookie.sequence );
        cookie = xcb_xfixes_create_pointer_barrier( host->xcb, host->clip_barrier[3], host->root_window,
                                                    host_rect.right, 0, host_rect.right, UINT16_MAX,
                                                    XCB_XFIXES_BARRIER_DIRECTIONS_NEGATIVE_X, 0, NULL );
        if (!debug_level) xcb_discard_reply( host->xcb, cookie.sequence );

        host->is_clipping = 1;
        host_set_needs_flush( host );
    }
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
            else if (event_code == host->xkb_event_code)
                handle_xcb_xkb_event( host, event );
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
    xcb_input_xi_query_device_cookie_t device_cookie;
    xcb_input_xi_query_device_reply_t *device_reply;
    struct xcb_input_mask input_mask;
    xcb_generic_error_t *error;
    xcb_void_cookie_t cookie;

    version_cookie = xcb_input_xi_query_version( host->xcb, XCB_INPUT_MAJOR_VERSION, XCB_INPUT_MINOR_VERSION );
    if (!(version_reply = xcb_input_xi_query_version_reply( host->xcb, version_cookie, &error )))
    {
        handle_xcb_generic_error( host, error );
        free( error );
        return -1;
    }

    TRACE( "host %p xinput version %u.%u enabled\n", host, version_reply->major_version, version_reply->minor_version );
    free( version_reply );

    input_mask.head.deviceid = XCB_INPUT_DEVICE_ALL_MASTER;
    input_mask.head.mask_len = sizeof(input_mask.mask) / sizeof(uint32_t);
    input_mask.mask = XCB_INPUT_XI_EVENT_MASK_DEVICE_CHANGED | XCB_INPUT_XI_EVENT_MASK_RAW_MOTION;
    cookie = xcb_input_xi_select_events( host->xcb, host->root_window, 1, &input_mask.head );
    if (!debug_level) xcb_discard_reply( host->xcb, cookie.sequence );

    device_cookie = xcb_input_xi_query_device( host->xcb, XCB_INPUT_DEVICE_ALL_MASTER );
    if (!(device_reply = xcb_input_xi_query_device_reply( host->xcb, device_cookie, &error )))
    {
        handle_xcb_generic_error( host, error );
        free( error );
        return -1;
    }

    handle_xcb_input_xi_query_device_reply( host, device_reply );
    free( device_reply );

    return 0;
}

static int init_xfixes( struct host_x11 *host )
{
    xcb_xfixes_query_version_cookie_t version_cookie;
    xcb_xfixes_query_version_reply_t *version_reply;
    xcb_generic_error_t *error;

    version_cookie = xcb_xfixes_query_version( host->xcb, XCB_XFIXES_MAJOR_VERSION, XCB_XFIXES_MINOR_VERSION );
    if (!(version_reply = xcb_xfixes_query_version_reply( host->xcb, version_cookie, &error )))
    {
        handle_xcb_generic_error( host, error );
        free( error );
        return -1;
    }

    TRACE( "host %p xfixes version %u.%u enabled\n", host, version_reply->major_version, version_reply->minor_version );
    free( version_reply );

    host->clip_barrier[0] = xcb_generate_id( host->xcb );
    host->clip_barrier[1] = xcb_generate_id( host->xcb );
    host->clip_barrier[2] = xcb_generate_id( host->xcb );
    host->clip_barrier[3] = xcb_generate_id( host->xcb );

    return 0;
}

static int init_xkb( struct host_x11 *host )
{
    uint16_t major_version, minor_version, xkb_event_mask;
    xcb_void_cookie_t cookie;

    if (!rxkb_context && (!(rxkb_context = rxkb_context_new( RXKB_CONTEXT_NO_FLAGS )) ||
                          !rxkb_context_parse_default_ruleset( rxkb_context )))
    {
        ERR( "failed to parse default xkb ruleset\n" );
        return -1;
    }

    if (!xkb_x11_setup_xkb_extension( host->xcb, XCB_XKB_MAJOR_VERSION, XCB_XKB_MINOR_VERSION, XKB_X11_SETUP_XKB_EXTENSION_NO_FLAGS,
                                      &major_version, &minor_version, &host->xkb_event_code, NULL ))
        return -1;
    TRACE( "host %p xkb version %u.%u event code %u enabled\n", host, major_version, minor_version, host->xkb_event_code );

    xkb_event_mask = XCB_XKB_EVENT_TYPE_STATE_NOTIFY | XCB_XKB_EVENT_TYPE_MAP_NOTIFY | XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY;
    cookie = xcb_xkb_select_events( host->xcb, XCB_XKB_ID_USE_CORE_KBD, xkb_event_mask, 0, xkb_event_mask, 0xff, 0xff, NULL );
    if (!debug_level) xcb_discard_reply( host->xcb, cookie.sequence );

    host->keyboard_id = xkb_x11_get_core_keyboard_device_id( host->xcb );
    TRACE( "host %p keyboard device %u\n", host, host->keyboard_id );

    if (!update_keyboard_layout_tables( host )) return -1;
    update_keyboard_lock_state( host );
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

    host->pointer_id = -1;
    host->x_class.mode = XCB_INPUT_VALUATOR_MODE_ABSOLUTE;
    host->x_class.number = -1;
    host->y_class.mode = XCB_INPUT_VALUATOR_MODE_ABSOLUTE;
    host->y_class.number = -1;
    host->warp_sequence = -1;
    host->is_clipping = 0;
    host->clip_barrier[0] = 0;
    host->clip_barrier[1] = 0;
    host->clip_barrier[2] = 0;
    host->clip_barrier[3] = 0;

    host->xkb_event_code = -1;
    host->keyboard_id = -1;
    host->active_layout = NULL;
    list_init( &host->layouts );
    host->keyboard_focus = 0;

    if (!(host->display = strdup( display ))) goto failed;
    if (!(host->xcb = xcb_connect( display, &screen_num ))) goto failed;
    if (xcb_connection_has_error( host->xcb )) goto failed;
    if (!(host->fd = create_anonymous_fd( &host_x11_fd_ops, xcb_get_file_descriptor( host->xcb ),
                                          &host->obj, FILE_SYNCHRONOUS_IO_NONALERT )))
        goto failed;
    if (!(screen = xcb_aux_get_screen( host->xcb, screen_num )))
        goto failed;

    host->root_window = screen->root;
    if (init_xfixes( host )) goto failed;
    if (init_xinput( host )) goto failed;
    if (init_xkb( host )) goto failed;

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
        input_mask.mask = XCB_INPUT_XI_EVENT_MASK_BUTTON_PRESS | XCB_INPUT_XI_EVENT_MASK_BUTTON_RELEASE |
                          XCB_INPUT_XI_EVENT_MASK_FOCUS_IN | XCB_INPUT_XI_EVENT_MASK_FOCUS_OUT |
                          XCB_INPUT_XI_EVENT_MASK_KEY_PRESS | XCB_INPUT_XI_EVENT_MASK_KEY_RELEASE |
                          XCB_INPUT_XI_EVENT_MASK_TOUCH_BEGIN | XCB_INPUT_XI_EVENT_MASK_TOUCH_END |
                          XCB_INPUT_XI_EVENT_MASK_TOUCH_UPDATE |
                          XCB_INPUT_XI_EVENT_MASK_ENTER | XCB_INPUT_XI_EVENT_MASK_LEAVE |
                          XCB_INPUT_XI_EVENT_MASK_MOTION;

        cookie = xcb_input_xi_select_events( host->xcb, req->x11_win, 1, &input_mask.head );
        if (!debug_level) xcb_discard_reply( host->xcb, cookie.sequence );

        host_set_needs_flush( host );
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

/* Retrieve the KDBTABLES for a X11 host layout */
DECL_HANDLER( x11_kbdtables )
{
    struct process *process = current->process;
    struct object *obj = process->host;

    if (!obj || obj->ops != &host_x11_object_ops )
        set_error( STATUS_INVALID_PARAMETER );
    else
    {
        struct host_x11 *host = (struct host_x11 *)obj;
        struct layout *layout;

        LIST_FOR_EACH_ENTRY( layout, &host->layouts, struct layout, entry )
        {
            if (HIWORD(get_layout_hkl( layout, layout->lang )) == HIWORD(req->layout))
                break;
        }

        if (&layout->entry == &host->layouts) set_error( STATUS_NOT_FOUND );
        else
        {
            reply->group_mask = layout->xkb_group * 0x2000;
            reply->shift_mask = layout->shift_mask;
            reply->ctrl_mask = layout->ctrl_mask;
            reply->alt_mask = layout->alt_mask;
            reply->altgr_mask = layout->altgr_mask;
            reply->caps_mask = layout->caps_mask;
            reply->num_mask = layout->num_mask;
            set_reply_data( &layout->tables, (char *)(layout + 1) - (char *)&layout->tables );
        }
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

/* Retrieve the KDBTABLES for a X11 host layout */
DECL_HANDLER( x11_kbdtables )
{
    set_error( STATUS_NOT_SUPPORTED );
}

#endif /* ENABLE_SERVER_X11 */
