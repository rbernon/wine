/*
 * Copyright (C) 2024 Rémi Bernon for CodeWeavers
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

#ifndef __WINE_WINE_DWMAPI_H
#define __WINE_WINE_DWMAPI_H

#include <stdarg.h>
#include <stddef.h>

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "wingdi.h"

typedef enum { DWM_INVALID_DISPLAY = (UINT64)-1 } dwm_display_t;

enum dwm_req_type
{
    DWM_REQ_CONNECT,
    DWM_REQ_SURFACE_FLUSH,
};

struct dwm_req_header
{
    UINT32 type;
    UINT32 req_size;
};

struct dwm_req_connect
{
    struct dwm_req_header header;
    char display_type[16];
    char display_name[MAX_PATH];
    UINT32 version;
};

struct dwm_req_surface_flush
{
    struct dwm_req_header header;
    HWND hwnd;
    RECT old_window_rect;
    RECT new_window_rect;
    RECT rect;
    UINT stride;
};

union dwm_request
{
    struct dwm_req_header header;
    struct dwm_req_connect connect;
    struct dwm_req_surface_flush surface_flush;
};

struct dwm_reply_header
{
    UINT32 status;
};

struct dwm_reply_connect
{
    struct dwm_reply_header header;
    dwm_display_t dwm_display;
};

union dwm_reply
{
    struct dwm_reply_header header;
    struct dwm_reply_connect connect;
};

#define DWM_PROTOCOL_VERSION 1

#endif  /* __WINE_WINE_DWMAPI_H */
