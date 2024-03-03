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

#include <stdarg.h>
#include <stddef.h>

#include "windef.h"
#include "winbase.h"

#include "wine/dwmapi.h"
#include "wine/unixlib.h"
#include "wine/debug.h"

struct dwm_client_loop_params
{
    HANDLE request;
    HANDLE reply;
};

struct dwm_connect_params
{
    const char *display_name;
    dwm_display_t dwm_display;
};

struct dwm_disconnect_params
{
    dwm_display_t dwm_display;
};

enum unix_funcs
{
    unix_dwm_init,
    unix_dwm_client_loop,
    unix_dwm_debug_loop,
    unix_dwm_funcs_count,
};

#ifndef WINE_UNIX_LIB

WINE_DEFAULT_DEBUG_CHANNEL(dwm);

static NTSTATUS dwm_init(void)
{
    return WINE_UNIX_CALL( unix_dwm_init, NULL );
}

static NTSTATUS dwm_client_loop( HANDLE request, HANDLE reply )
{
    struct dwm_client_loop_params params = {.request = request, .reply = reply};
    return WINE_UNIX_CALL( unix_dwm_client_loop, &params );
}

static NTSTATUS dwm_debug_loop(void)
{
    return WINE_UNIX_CALL( unix_dwm_debug_loop, NULL );
}

#endif
