/*
 * sfnt2fon.  Bitmap-only ttf to Windows font file converter
 *
 * Copyright 2004 Huw Davies
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
#include <stdlib.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XI2.h>
#include <X11/extensions/XInput2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum atoms_ids
{
    FIRST_XATOM = XA_LAST_PREDEFINED + 1,
    XATOM_CLIPBOARD = FIRST_XATOM,
    XATOM_COMPOUND_TEXT,
    XATOM_INCR,
    XATOM_MANAGER,
    XATOM_MULTIPLE,
    XATOM_SELECTION_DATA,
    XATOM_TARGETS,
    XATOM_TEXT,
    XATOM_TIMESTAMP,
    XATOM_UTF8_STRING,
    XATOM_RAW_ASCENT,
    XATOM_RAW_DESCENT,
    XATOM_RAW_CAP_HEIGHT,
    XATOM_Rel_X,
    XATOM_Rel_Y,
    XATOM_WM_PROTOCOLS,
    XATOM_WM_DELETE_WINDOW,
    XATOM_WM_STATE,
    XATOM_WM_TAKE_FOCUS,
    XATOM_DndProtocol,
    XATOM_DndSelection,
    XATOM__ICC_PROFILE,
    XATOM__MOTIF_WM_HINTS,
    XATOM__NET_STARTUP_INFO_BEGIN,
    XATOM__NET_STARTUP_INFO,
    XATOM__NET_SUPPORTED,
    XATOM__NET_SYSTEM_TRAY_OPCODE,
    XATOM__NET_SYSTEM_TRAY_S0,
    XATOM__NET_SYSTEM_TRAY_VISUAL,
    XATOM__NET_WM_ICON,
    XATOM__NET_WM_MOVERESIZE,
    XATOM__NET_WM_NAME,
    XATOM__NET_WM_PID,
    XATOM__NET_WM_PING,
    XATOM__NET_WM_STATE,
    XATOM__NET_WM_STATE_ABOVE,
    XATOM__NET_WM_STATE_DEMANDS_ATTENTION,
    XATOM__NET_WM_STATE_FULLSCREEN,
    XATOM__NET_WM_STATE_MAXIMIZED_HORZ,
    XATOM__NET_WM_STATE_MAXIMIZED_VERT,
    XATOM__NET_WM_STATE_SKIP_PAGER,
    XATOM__NET_WM_STATE_SKIP_TASKBAR,
    XATOM__NET_WM_USER_TIME,
    XATOM__NET_WM_USER_TIME_WINDOW,
    XATOM__NET_WM_WINDOW_OPACITY,
    XATOM__NET_WM_WINDOW_TYPE,
    XATOM__NET_WM_WINDOW_TYPE_DIALOG,
    XATOM__NET_WM_WINDOW_TYPE_NORMAL,
    XATOM__NET_WM_WINDOW_TYPE_UTILITY,
    XATOM__NET_WORKAREA,
    XATOM__XEMBED,
    XATOM__XEMBED_INFO,
    XATOM_XdndAware,
    XATOM_XdndEnter,
    XATOM_XdndPosition,
    XATOM_XdndStatus,
    XATOM_XdndLeave,
    XATOM_XdndFinished,
    XATOM_XdndDrop,
    XATOM_XdndActionCopy,
    XATOM_XdndActionMove,
    XATOM_XdndActionLink,
    XATOM_XdndActionAsk,
    XATOM_XdndActionPrivate,
    XATOM_XdndSelection,
    XATOM_XdndTypeList,
    XATOM_HTML_Format,
    XATOM_WCF_DIF,
    XATOM_WCF_ENHMETAFILE,
    XATOM_WCF_HDROP,
    XATOM_WCF_PENDATA,
    XATOM_WCF_RIFF,
    XATOM_WCF_SYLK,
    XATOM_WCF_TIFF,
    XATOM_WCF_WAVE,
    XATOM_image_bmp,
    XATOM_image_gif,
    XATOM_image_jpeg,
    XATOM_image_png,
    XATOM_text_html,
    XATOM_text_plain,
    XATOM_text_rtf,
    XATOM_text_richtext,
    XATOM_text_uri_list,
    NB_XATOMS
};

static const char *const atoms_names[NB_XATOMS - FIRST_XATOM] =
{
    "CLIPBOARD",
    "COMPOUND_TEXT",
    "INCR",
    "MANAGER",
    "MULTIPLE",
    "SELECTION_DATA",
    "TARGETS",
    "TEXT",
    "TIMESTAMP",
    "UTF8_STRING",
    "RAW_ASCENT",
    "RAW_DESCENT",
    "RAW_CAP_HEIGHT",
    "Rel X",
    "Rel Y",
    "WM_PROTOCOLS",
    "WM_DELETE_WINDOW",
    "WM_STATE",
    "WM_TAKE_FOCUS",
    "DndProtocol",
    "DndSelection",
    "_ICC_PROFILE",
    "_MOTIF_WM_HINTS",
    "_NET_STARTUP_INFO_BEGIN",
    "_NET_STARTUP_INFO",
    "_NET_SUPPORTED",
    "_NET_SYSTEM_TRAY_OPCODE",
    "_NET_SYSTEM_TRAY_S0",
    "_NET_SYSTEM_TRAY_VISUAL",
    "_NET_WM_ICON",
    "_NET_WM_MOVERESIZE",
    "_NET_WM_NAME",
    "_NET_WM_PID",
    "_NET_WM_PING",
    "_NET_WM_STATE",
    "_NET_WM_STATE_ABOVE",
    "_NET_WM_STATE_DEMANDS_ATTENTION",
    "_NET_WM_STATE_FULLSCREEN",
    "_NET_WM_STATE_MAXIMIZED_HORZ",
    "_NET_WM_STATE_MAXIMIZED_VERT",
    "_NET_WM_STATE_SKIP_PAGER",
    "_NET_WM_STATE_SKIP_TASKBAR",
    "_NET_WM_USER_TIME",
    "_NET_WM_USER_TIME_WINDOW",
    "_NET_WM_WINDOW_OPACITY",
    "_NET_WM_WINDOW_TYPE",
    "_NET_WM_WINDOW_TYPE_DIALOG",
    "_NET_WM_WINDOW_TYPE_NORMAL",
    "_NET_WM_WINDOW_TYPE_UTILITY",
    "_NET_WORKAREA",
    "_XEMBED",
    "_XEMBED_INFO",
    "XdndAware",
    "XdndEnter",
    "XdndPosition",
    "XdndStatus",
    "XdndLeave",
    "XdndFinished",
    "XdndDrop",
    "XdndActionCopy",
    "XdndActionMove",
    "XdndActionLink",
    "XdndActionAsk",
    "XdndActionPrivate",
    "XdndSelection",
    "XdndTypeList",
    "HTML Format",
    "WCF_DIF",
    "WCF_ENHMETAFILE",
    "WCF_HDROP",
    "WCF_PENDATA",
    "WCF_RIFF",
    "WCF_SYLK",
    "WCF_TIFF",
    "WCF_WAVE",
    "image/bmp",
    "image/gif",
    "image/jpeg",
    "image/png",
    "text/html",
    "text/plain",
    "text/rtf",
    "text/richtext",
    "text/uri-list",
};

Atom atoms[NB_XATOMS - FIRST_XATOM];

Atom _NET_ACTIVE_WINDOW;

void set_active_window( Display *d, Window w )
{
    XEvent xev;

    xev.xclient.type = ClientMessage;
    xev.xclient.window = w;
    xev.xclient.message_type = _NET_ACTIVE_WINDOW;
    xev.xclient.serial = 0;
    xev.xclient.display = d;
    xev.xclient.send_event = True;
    xev.xclient.format = 32;

    xev.xclient.data.l[0] = 1; /* source: application */
    xev.xclient.data.l[1] = CurrentTime;
    xev.xclient.data.l[2] = 0;
    xev.xclient.data.l[3] = 0;
    xev.xclient.data.l[4] = 0;
    XSendEvent( d, DefaultRootWindow( d ), False, SubstructureRedirectMask | SubstructureNotifyMask, &xev );
}

#define ok(x) if (!(x)) { assert(0 && !#x); abort(); }

int main( void )
{
    XWMHints *wm_hints;
    Atom protocols[3];
    int s, i = 0;
    Window w, w2;
    Display *d;
    long time;
    XEvent e;

    ok((d = XOpenDisplay( NULL )));

    XInternAtoms( d, (char **)atoms_names, NB_XATOMS - FIRST_XATOM, False, atoms );
    _NET_ACTIVE_WINDOW = XInternAtom( d, "_NET_ACTIVE_WINDOW", False );

    s = DefaultScreen( d );
    w = XCreateSimpleWindow( d, RootWindow( d, s ), 10, 10, 500, 500, 1, BlackPixel( d, s ), WhitePixel( d, s ) );
    w2 = XCreateSimpleWindow( d, RootWindow( d, s ), 10, 10, 500, 500, 1, BlackPixel( d, s ), WhitePixel( d, s ) );

    if ((wm_hints = XAllocWMHints()))
    {
        wm_hints->flags = InputHint | StateHint;
        wm_hints->input = True;
        wm_hints->initial_state = NormalState;
        XSetWMHints( d, w, wm_hints );
        XFree( wm_hints );
    }

    if ((wm_hints = XAllocWMHints()))
    {
        wm_hints->flags = InputHint | StateHint;
        wm_hints->input = True;
        wm_hints->initial_state = NormalState;
        XSetWMHints( d, w2, wm_hints );
        XFree( wm_hints );
    }

if (0)
{
    XChangeProperty( d, w, atoms[XATOM__NET_WM_USER_TIME_WINDOW - FIRST_XATOM], XA_WINDOW, 32, PropModeReplace,
                     (unsigned char *)&w, 1 );
    XChangeProperty( d, w2, atoms[XATOM__NET_WM_USER_TIME_WINDOW - FIRST_XATOM], XA_WINDOW, 32, PropModeReplace,
                     (unsigned char *)&w2, 1 );
}

if (0)
{
    protocols[i++] = atoms[XATOM_WM_DELETE_WINDOW - FIRST_XATOM];
    protocols[i++] = atoms[XATOM_WM_TAKE_FOCUS - FIRST_XATOM];
    XChangeProperty( d, w, atoms[XATOM_WM_PROTOCOLS - FIRST_XATOM], XA_ATOM, 32, PropModeReplace,
                     (unsigned char *)protocols, i );
}

    XSelectInput( d, w, KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | EnterWindowMask |
                  LeaveWindowMask | PointerMotionMask | Button1MotionMask | Button2MotionMask |
                  Button3MotionMask | Button4MotionMask | Button5MotionMask | ButtonMotionMask |
                  KeymapStateMask | ExposureMask | VisibilityChangeMask | StructureNotifyMask |
                  ResizeRedirectMask | SubstructureNotifyMask | SubstructureRedirectMask |
                  FocusChangeMask | PropertyChangeMask | ColormapChangeMask | OwnerGrabButtonMask );
    XSelectInput( d, w2, KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | EnterWindowMask |
                  LeaveWindowMask | PointerMotionMask | Button1MotionMask | Button2MotionMask |
                  Button3MotionMask | Button4MotionMask | Button5MotionMask | ButtonMotionMask |
                  KeymapStateMask | ExposureMask | VisibilityChangeMask | StructureNotifyMask |
                  ResizeRedirectMask | SubstructureNotifyMask | SubstructureRedirectMask |
                  FocusChangeMask | PropertyChangeMask | ColormapChangeMask | OwnerGrabButtonMask );
    XMapWindow( d, w );

    while (1)
    {
        XNextEvent( d, &e );
        if (e.type == KeyPress)
        {
            static int count;
fprintf(stderr, "key\n");

            time = 0;
            XChangeProperty( d, w2, atoms[XATOM__NET_WM_USER_TIME - FIRST_XATOM], XA_CARDINAL, 32, PropModeReplace,
                             (unsigned char *)&time, 1 );

            if (!(count++ % 2)) XMapWindow( d, w2 );
            else XUnmapWindow( d, w2 );
        }

        if (e.type == ClientMessage)
        {
        }
    }

    XDestroyWindow( d, w );
    XCloseDisplay( d );
    return 0;
}
