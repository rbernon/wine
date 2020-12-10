/*
 * Copyright 2020 Rémi Bernon for CodeWeavers
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

#ifndef __WINE_WINE_WIN32U_H
#define __WINE_WINE_WIN32U_H

#include <windef.h>
#include <winuser.h>
#include <winioctl.h>

#define IOCTL_WIN32U_CREATE_WINDOW        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, 0)
#define IOCTL_WIN32U_DESTROY_WINDOW       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, 0)
#define IOCTL_WIN32U_WINDOW_POS_CHANGING  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, 0)
#define IOCTL_WIN32U_WINDOW_POS_CHANGED   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, 0)
#define IOCTL_WIN32U_SHOW_WINDOW          CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, 0)
#define IOCTL_WIN32U_SET_WINDOW_STYLE     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, 0)
#define IOCTL_WIN32U_SET_FOCUS            CTL_CODE(FILE_DEVICE_UNKNOWN, 0x806, METHOD_BUFFERED, 0)
#define IOCTL_WIN32U_SET_PARENT           CTL_CODE(FILE_DEVICE_UNKNOWN, 0x807, METHOD_BUFFERED, 0)
#define IOCTL_WIN32U_TO_UNICODE           CTL_CODE(FILE_DEVICE_UNKNOWN, 0x808, METHOD_BUFFERED, 0)

#define WIN32U_DUMMY_DESKTOP_SURFACE (~(UINT64)0)

/* IOCTL_WIN32U_CREATE_WINDOW params */
struct win32u_create_window_input
{
    ULONG hwnd;
};

/* IOCTL_WIN32U_DESTROY_WINDOW params */
struct win32u_destroy_window_input
{
    ULONG hwnd;
};

/* IOCTL_WIN32U_WINDOW_POS_CHANGING params */
struct win32u_window_pos_changing_input
{
    ULONG  hwnd;
    ULONG  insert_after;
    UINT   swp_flags;
    RECT   window_rect;
    RECT   client_rect;
    UINT64 desktop_surface;
};

struct win32u_window_pos_changing_output
{
    RECT   visible_rect;
    UINT64 desktop_surface;
};

/* IOCTL_WIN32U_WINDOW_POS_CHANGED params */
struct win32u_window_pos_changed_input
{
    ULONG  hwnd;
    ULONG  insert_after;
    UINT   swp_flags;
    RECT   window_rect;
    RECT   client_rect;
    RECT   visible_rect;
    BOOL   valid_rects_set;
    RECT   valid_rects;
    UINT64 desktop_surface;
};

/* IOCTL_WIN32U_SHOW_WINDOW params */
struct win32u_show_window_input
{
    ULONG  hwnd;
    INT    cmd;
    RECT   rect;
    UINT   swp;
};

struct win32u_show_window_output
{
    UINT swp;
};

/* IOCTL_WIN32U_SET_WINDOW_STYLE params */
struct win32u_set_window_style_input
{
    ULONG       hwnd;
    INT         offset;
    STYLESTRUCT style;
};

/* IOCTL_WIN32U_SET_FOCUS params */
struct win32u_set_focus_input
{
    ULONG hwnd;
};

/* IOCTL_WIN32U_SET_PARENT params */
struct win32u_set_parent_input
{
    ULONG hwnd;
    ULONG parent;
    ULONG old_parent;
};

/* IOCTL_WIN32U_TO_UNICODE params */
struct win32u_to_unicode_input
{
    UINT  vkey;
    UINT  scancode;
    BYTE  keystate[256];
    UINT  flags;
    ULONG hkl;
};

#endif
