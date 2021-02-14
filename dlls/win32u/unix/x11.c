/*
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

#include "config.h"
#include "wine/port.h"

#include "x11.h"
#include "unixlib.h"

#include "x11drv.h"

WINE_DEFAULT_DEBUG_CHANNEL(win32u);

#ifdef SONAME_LIBX11

#define MAKE_FUNCPTR(f) typeof(f) *p##f;
MAKE_FUNCPTR(XCreateColormap)
MAKE_FUNCPTR(XCreatePixmap)
MAKE_FUNCPTR(XCreateWindow)
MAKE_FUNCPTR(XDestroyWindow)
MAKE_FUNCPTR(XFlush)
MAKE_FUNCPTR(XFree)
MAKE_FUNCPTR(XFreeColormap)
MAKE_FUNCPTR(XFreePixmap)
MAKE_FUNCPTR(XGetWindowAttributes)
MAKE_FUNCPTR(XInitThreads)
MAKE_FUNCPTR(XMapWindow)
MAKE_FUNCPTR(XOpenDisplay)
MAKE_FUNCPTR(XQueryExtension)
MAKE_FUNCPTR(XSync)
#undef MAKE_FUNCPTR

static BOOL init_xlib(void)
{
    void *xlib_handle;

    if (!(xlib_handle = dlopen( SONAME_LIBX11, RTLD_NOW )))
    {
        ERR( "dlopen(%s, RTLD_NOW) failed!\n", SONAME_LIBX11 );
        return FALSE;
    }

#define LOAD_FUNCPTR( f )                                                                          \
    if ((p##f = dlsym( xlib_handle, #f )) == NULL)                                                 \
    {                                                                                              \
        ERR( "dlsym(%s, %s) failed!\n", SONAME_LIBX11, #f );                                       \
        goto error;                                                                                \
    }

    LOAD_FUNCPTR(XCreateColormap)
    LOAD_FUNCPTR(XCreatePixmap)
    LOAD_FUNCPTR(XCreateWindow)
    LOAD_FUNCPTR(XDestroyWindow)
    LOAD_FUNCPTR(XFlush)
    LOAD_FUNCPTR(XFree)
    LOAD_FUNCPTR(XFreeColormap)
    LOAD_FUNCPTR(XFreePixmap)
    LOAD_FUNCPTR(XGetWindowAttributes)
    LOAD_FUNCPTR(XInitThreads)
    LOAD_FUNCPTR(XMapWindow)
    LOAD_FUNCPTR(XOpenDisplay)
    LOAD_FUNCPTR(XQueryExtension)
    LOAD_FUNCPTR(XSync)
#undef LOAD_FUNCPTR

    return TRUE;

error:
    dlclose( xlib_handle );
    return FALSE;
}

#else

static BOOL init_xlib(void)
{
    ERR( "Xlib support not compiled in!\n" );
    return FALSE;
}

#endif

static struct unix_funcs unix_funcs = {
    x11_create_window_surface,
    x11_resize_window_surface,
};

NTSTATUS CDECL __wine_init_unix_lib( HMODULE module, DWORD reason, const void *ptr_in, void *ptr_out )
{
    TRACE( "module %p, reason %x, ptr_in %p, ptr_out %p.\n", module, reason, ptr_in, ptr_out );

    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        x11drv_module = module;
        if (!x11drv_process_attach()) return STATUS_DLL_NOT_FOUND;
        if (!init_xlib()) return STATUS_DLL_NOT_FOUND;
        break;
    case DLL_PROCESS_DETACH: break;
    }

    *(void **)ptr_out = &unix_funcs;
    return STATUS_SUCCESS;
}
