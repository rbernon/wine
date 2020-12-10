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
MAKE_FUNCPTR(cairo_surface_create_similar_image)
MAKE_FUNCPTR(cairo_surface_map_to_image)
MAKE_FUNCPTR(cairo_surface_unmap_image)
MAKE_FUNCPTR(cairo_surface_destroy)
MAKE_FUNCPTR(cairo_surface_flush)
MAKE_FUNCPTR(cairo_surface_write_to_png_stream)
MAKE_FUNCPTR(cairo_surface_mark_dirty)
MAKE_FUNCPTR(cairo_surface_mark_dirty_rectangle)
MAKE_FUNCPTR(cairo_image_surface_get_data)
MAKE_FUNCPTR(cairo_image_surface_get_width)
MAKE_FUNCPTR(cairo_image_surface_get_height)
MAKE_FUNCPTR(cairo_image_surface_get_stride)
#undef MAKE_FUNCPTR
#endif

#ifdef HAVE_CAIRO_CAIRO_XLIB_H
#define MAKE_FUNCPTR(f) typeof(f) *p_##f;
MAKE_FUNCPTR(cairo_xlib_surface_create)
MAKE_FUNCPTR(cairo_xlib_surface_set_drawable)
MAKE_FUNCPTR(cairo_xlib_surface_set_size)
#undef MAKE_FUNCPTR
#endif

#ifdef HAVE_CAIRO_CAIRO_XLIB_XRENDER_H
#define MAKE_FUNCPTR(f) typeof(f) *p_##f;
MAKE_FUNCPTR(cairo_xlib_surface_create_with_xrender_format)
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
    LOAD_FUNCPTR(cairo_surface_create_similar_image)
    LOAD_FUNCPTR(cairo_surface_map_to_image)
    LOAD_FUNCPTR(cairo_surface_unmap_image)
    LOAD_FUNCPTR(cairo_surface_destroy)
    LOAD_FUNCPTR(cairo_surface_flush)
    LOAD_FUNCPTR(cairo_surface_write_to_png_stream)
    LOAD_FUNCPTR(cairo_surface_mark_dirty)
    LOAD_FUNCPTR(cairo_surface_mark_dirty_rectangle)
    LOAD_FUNCPTR(cairo_image_surface_get_data)
    LOAD_FUNCPTR(cairo_image_surface_get_width)
    LOAD_FUNCPTR(cairo_image_surface_get_height)
    LOAD_FUNCPTR(cairo_image_surface_get_stride)
#endif
#ifdef HAVE_CAIRO_CAIRO_XLIB_H
    LOAD_FUNCPTR(cairo_xlib_surface_create)
    LOAD_FUNCPTR(cairo_xlib_surface_set_drawable)
    LOAD_FUNCPTR(cairo_xlib_surface_set_size)
#endif
#ifdef HAVE_CAIRO_CAIRO_XLIB_XRENDER_H
    LOAD_FUNCPTR(cairo_xlib_surface_create_with_xrender_format)
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

#if 0
    if (!pXInitThreads())
    {
        ERR("XInitThreads() failed!\n");
        goto error;
    }

    if (!(display = pXOpenDisplay( NULL )))
    {
        ERR("XOpenDisplay(NULL) failed!");
        goto error;
    }

    gdi_display = display;
    root_window = DefaultRootWindow(display);
#endif

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
        if (!init_cairo()) return STATUS_DLL_NOT_FOUND;
        if (!init_xlib()) return STATUS_DLL_NOT_FOUND;
        break;
    case DLL_PROCESS_DETACH: break;
    }

    *(void **)ptr_out = &unix_funcs;
    return STATUS_SUCCESS;
}
