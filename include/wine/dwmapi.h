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
typedef enum { DWM_INVALID_WINDOW = (UINT64)-1 } dwm_window_t;
typedef enum { DWM_INVALID_CONTEXT = (UINT64)-1 } dwm_context_t;

#define DWM_EXT_ESCAPE 0xfeedc0de

enum dwm_req_type
{
    DWM_REQ_CONNECT,
    DWM_REQ_WINDOW_CREATE,
    DWM_REQ_WINDOW_UPDATE,
    DWM_REQ_WINDOW_POS_CHANGING,
    DWM_REQ_WINDOW_POS_CHANGED,
    DWM_REQ_WINDOW_DESTROY,
    DWM_REQ_GDI_CONTEXT_CREATE,
    DWM_REQ_GDI_CONTEXT_DESTROY,
    DWM_REQ_GDI_SET_SOURCE,
    DWM_REQ_GDI_PUT_IMAGE,
    DWM_REQ_GDI_STRETCH_BLT,
    DWM_REQ_GDI_COMMANDS,
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

struct dwm_req_window_create
{
    struct dwm_req_header header;
    dwm_display_t dwm_display;
    HWND hwnd;
    UINT64 native;
};

struct dwm_req_window_update
{
    struct dwm_req_header header;
    dwm_window_t dwm_window;
    RECT window_rect;
};

struct dwm_req_window_pos_changing
{
    struct dwm_req_header header;
};

struct dwm_req_window_pos_changed
{
    struct dwm_req_header header;
};

struct dwm_req_window_destroy
{
    struct dwm_req_header header;
    dwm_window_t dwm_window;
};

struct dwm_req_gdi_context_create
{
    struct dwm_req_header header;
};

struct dwm_req_gdi_context_destroy
{
    struct dwm_req_header header;
    dwm_context_t dwm_context;
};

struct dwm_req_gdi_set_source
{
    struct dwm_req_header header;
    dwm_context_t dwm_context;
    UINT32 bpp;
    UINT32 stride;
    RECT rect;
    UINT32 data_size;
};

struct dwm_req_gdi_put_image
{
    struct dwm_req_header header;
    dwm_context_t dwm_context;
    dwm_window_t dwm_window;
    RECT rect;
};

struct dwm_req_gdi_stretch_blt
{
    struct dwm_req_header header;
    dwm_window_t dwm_window;
    UINT32 data_size;
};

struct dwm_req_gdi_commands
{
    struct dwm_req_header header;
    dwm_context_t dwm_context;
    dwm_window_t dwm_window;
    RECT context_rect;
    RECT window_rect;
    UINT32 data_size;
};

union dwm_request
{
    struct dwm_req_header header;
    struct dwm_req_connect connect;
    struct dwm_req_window_create window_create;
    struct dwm_req_window_update window_update;
    struct dwm_req_window_pos_changing window_pos_changing;
    struct dwm_req_window_pos_changed window_pos_changed;
    struct dwm_req_window_destroy window_destroy;
    struct dwm_req_gdi_context_create gdi_context_create;
    struct dwm_req_gdi_context_destroy gdi_context_destroy;
    struct dwm_req_gdi_set_source gdi_set_source;
    struct dwm_req_gdi_put_image gdi_put_image;
    struct dwm_req_gdi_stretch_blt gdi_stretch_blt;
    struct dwm_req_gdi_commands gdi_commands;
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

struct dwm_reply_window_create
{
    struct dwm_reply_header header;
    dwm_window_t dwm_window;
};

struct dwm_reply_gdi_context_create
{
    struct dwm_reply_header header;
    dwm_context_t dwm_context;
};

union dwm_reply
{
    struct dwm_reply_header header;
    struct dwm_reply_connect connect;
    struct dwm_reply_window_create window_create;
    struct dwm_reply_gdi_context_create gdi_context_create;
};

#define DWM_PROTOCOL_VERSION 1

#endif  /* __WINE_WINE_DWMAPI_H */
