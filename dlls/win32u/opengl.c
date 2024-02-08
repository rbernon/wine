/*
 * Vulkan display driver loading
 *
 * Copyright (c) 2017 Roderick Colenbrander
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

#include <pthread.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "ntgdi_private.h"
#include "win32u_private.h"
#include "ntuser_private.h"

#include "wine/wgl.h"
#include "wine/wgl_driver.h"

WINE_DEFAULT_DEBUG_CHANNEL(wgl);

static struct opengl_funcs *display_funcs;
static struct opengl_funcs *memory_funcs;

static void opengl_init(void)
{
    display_funcs = user_driver->pwine_get_wgl_driver( WINE_WGL_DRIVER_VERSION );
    if (display_funcs == (void *)-1) display_funcs = NULL;
    memory_funcs = dibdrv_get_wgl_driver();
    if (memory_funcs == (void *)-1) memory_funcs = NULL;
}

/***********************************************************************
 *      __wine_get_wgl_driver  (win32u.@)
 */
const struct opengl_funcs *__wine_get_wgl_driver( HDC hdc, UINT version )
{
    static pthread_once_t init_once = PTHREAD_ONCE_INIT;
    DWORD is_display, is_memdc;

    if (version != WINE_WGL_DRIVER_VERSION)
    {
        ERR( "version mismatch, opengl32 wants %u but dibdrv has %u\n",
             version, WINE_WGL_DRIVER_VERSION );
        return NULL;
    }

    pthread_once( &init_once, opengl_init );

    if (!NtGdiGetDCDword( hdc, NtGdiIsMemDC, &is_memdc )) return NULL;
    if (!NtGdiGetDCDword( hdc, NtGdiIsDisplay, &is_display )) return NULL;
    if (is_display && display_funcs) return display_funcs;
    if (is_memdc && memory_funcs) return memory_funcs;
    return (void *)-1;
}
