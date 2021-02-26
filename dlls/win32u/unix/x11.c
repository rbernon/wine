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

#ifdef SONAME_LIBCAIRO

#ifdef HAVE_CAIRO_CAIRO_H
#define MAKE_FUNCPTR(f) typeof(f) *p_##f;
MAKE_FUNCPTR(cairo_surface_destroy)
#undef MAKE_FUNCPTR
#endif

#ifdef HAVE_CAIRO_CAIRO_XLIB_H
#define MAKE_FUNCPTR(f) typeof(f) *p_##f;
MAKE_FUNCPTR(cairo_xlib_surface_create)
MAKE_FUNCPTR(cairo_xlib_surface_get_drawable)
MAKE_FUNCPTR(cairo_xlib_surface_get_display)
MAKE_FUNCPTR(cairo_xlib_surface_set_size)
#undef MAKE_FUNCPTR
#endif

static BOOL init_cairo(void)
{
    void *libcairo;

    if (!(libcairo = dlopen( SONAME_LIBCAIRO, RTLD_NOW )))
    {
        ERR( "dlopen(%s, RTLD_NOW) failed!\n", SONAME_LIBCAIRO );
        return FALSE;
    }

#define LOAD_FUNCPTR( f )                                                                          \
    if ((p_##f = dlsym( libcairo, #f )) == NULL)                                                   \
    {                                                                                              \
        ERR( "dlsym(%s, %s) failed!\n", SONAME_LIBCAIRO, #f );                                     \
        goto error;                                                                                \
    }

#ifdef HAVE_CAIRO_CAIRO_H
    LOAD_FUNCPTR(cairo_surface_destroy)
#endif
#ifdef HAVE_CAIRO_CAIRO_XLIB_H
    LOAD_FUNCPTR(cairo_xlib_surface_create)
    LOAD_FUNCPTR(cairo_xlib_surface_get_drawable)
    LOAD_FUNCPTR(cairo_xlib_surface_get_display)
    LOAD_FUNCPTR(cairo_xlib_surface_set_size)
#endif
#undef LOAD_FUNCPTR

    return TRUE;

error:
    dlclose( libcairo );
    return FALSE;
}

#else

static BOOL init_cairo(void)
{
    ERR( "Cairo 2D support not compiled in!\n" );
    return FALSE;
}

#endif /* defined(SONAME_LIBCAIRO) */

#ifdef SONAME_LIBX11

#define MAKE_FUNCPTR(f) typeof(f) *p##f;
MAKE_FUNCPTR(XGetWindowAttributes)
MAKE_FUNCPTR(XConfigureWindow)
MAKE_FUNCPTR(XReparentWindow)
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

    LOAD_FUNCPTR(XGetWindowAttributes)
    LOAD_FUNCPTR(XConfigureWindow)
    LOAD_FUNCPTR(XReparentWindow)
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
    cairo_surface_create_toplevel,
    cairo_surface_create_notify,
    cairo_surface_delete,
    cairo_surface_resize_notify,
};

NTSTATUS CDECL __wine_init_unix_lib( HMODULE module, DWORD reason, const void *ptr_in, void *ptr_out )
{
    TRACE( "module %p, reason %x, ptr_in %p, ptr_out %p.\n", module, reason, ptr_in, ptr_out );

    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        x11drv_module = module;
        if (!x11drv_process_attach()) return STATUS_DLL_NOT_FOUND;
        if (!init_cairo()) return STATUS_DLL_NOT_FOUND;
        if (!init_xlib()) return STATUS_DLL_NOT_FOUND;
        break;
    case DLL_PROCESS_DETACH: break;
    }

    *(void **)ptr_out = &unix_funcs;
    return STATUS_SUCCESS;
}
