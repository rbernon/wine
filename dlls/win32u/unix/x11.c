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
MAKE_FUNCPTR(cairo_clip)
MAKE_FUNCPTR(cairo_create)
MAKE_FUNCPTR(cairo_destroy)
MAKE_FUNCPTR(cairo_fill)
MAKE_FUNCPTR(cairo_image_surface_get_data)
MAKE_FUNCPTR(cairo_image_surface_get_height)
MAKE_FUNCPTR(cairo_image_surface_get_stride)
MAKE_FUNCPTR(cairo_image_surface_get_width)
MAKE_FUNCPTR(cairo_move_to)
MAKE_FUNCPTR(cairo_paint_with_alpha)
MAKE_FUNCPTR(cairo_rectangle)
MAKE_FUNCPTR(cairo_set_source_rgba)
MAKE_FUNCPTR(cairo_set_source_surface)
MAKE_FUNCPTR(cairo_surface_create_similar_image)
MAKE_FUNCPTR(cairo_surface_destroy)
MAKE_FUNCPTR(cairo_surface_flush)
MAKE_FUNCPTR(cairo_surface_mark_dirty_rectangle)
MAKE_FUNCPTR(cairo_surface_reference)
MAKE_FUNCPTR(cairo_set_source_rgba)
MAKE_FUNCPTR(cairo_stroke)
MAKE_FUNCPTR(cairo_text_path)
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
    LOAD_FUNCPTR(cairo_clip)
    LOAD_FUNCPTR(cairo_create)
    LOAD_FUNCPTR(cairo_destroy)
    LOAD_FUNCPTR(cairo_fill)
    LOAD_FUNCPTR(cairo_image_surface_get_data)
    LOAD_FUNCPTR(cairo_image_surface_get_height)
    LOAD_FUNCPTR(cairo_image_surface_get_stride)
    LOAD_FUNCPTR(cairo_image_surface_get_width)
    LOAD_FUNCPTR(cairo_move_to)
    LOAD_FUNCPTR(cairo_paint_with_alpha)
    LOAD_FUNCPTR(cairo_rectangle)
    LOAD_FUNCPTR(cairo_set_source_rgba)
    LOAD_FUNCPTR(cairo_set_source_surface)
    LOAD_FUNCPTR(cairo_surface_create_similar_image)
    LOAD_FUNCPTR(cairo_surface_destroy)
    LOAD_FUNCPTR(cairo_surface_flush)
    LOAD_FUNCPTR(cairo_surface_mark_dirty_rectangle)
    LOAD_FUNCPTR(cairo_surface_reference)
    LOAD_FUNCPTR(cairo_set_source_rgba)
    LOAD_FUNCPTR(cairo_stroke)
    LOAD_FUNCPTR(cairo_text_path)
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

#ifdef SONAME_LIBXCOMPOSITE

#define MAKE_FUNCPTR(f) typeof(f) * p##f;
MAKE_FUNCPTR(XCompositeQueryExtension)
MAKE_FUNCPTR(XCompositeQueryVersion)
MAKE_FUNCPTR(XCompositeRedirectWindow)
MAKE_FUNCPTR(XCompositeUnredirectWindow)
#undef MAKE_FUNCPTR

static BOOL init_xcomposite(Display *display)
{
    void *xcomposite_handle;
    int event_base, error_base, major_version = 0, minor_version = 4;

    if (!(xcomposite_handle = dlopen(SONAME_LIBXCOMPOSITE, RTLD_NOW)))
    {
        ERR("dlopen(%s, RTLD_NOW) failed!\n", SONAME_LIBXCOMPOSITE);
        return FALSE;
    }

#define LOAD_FUNCPTR(f) \
    if ((p##f = dlsym(xcomposite_handle, #f)) == NULL) \
    { \
        ERR("dlsym(%s, %s) failed!\n", SONAME_LIBXCOMPOSITE, #f); \
        goto error; \
    }

    LOAD_FUNCPTR(XCompositeQueryExtension)
    LOAD_FUNCPTR(XCompositeQueryVersion)
    LOAD_FUNCPTR(XCompositeRedirectWindow)
    LOAD_FUNCPTR(XCompositeUnredirectWindow)
#undef LOAD_FUNCPTR

    if (!pXCompositeQueryExtension(display, &event_base, &error_base))
    {
        ERR("XCompositeQueryExtension(%p, %p, %p) failed!\n", display, &event_base, &error_base);
        goto error;
    }

    if (!pXCompositeQueryVersion(display, &major_version, &minor_version))
    {
        ERR("XCompositeQueryVersion(%p, %d, %d) failed!\n", display, major_version, minor_version);
        goto error;
    }

    TRACE("event_base %d, error_base %d, version %d.%d.\n", event_base, error_base, major_version, minor_version);
    return TRUE;

error:
    dlclose(xcomposite_handle);
    return FALSE;
}

#else

static BOOL init_xcomposite(Display *display)
{
    ERR("XComposite support not compiled in!\n");
    return FALSE;
}

#endif /* defined(SONAME_LIBXCOMPOSITE) */

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
    cairo_surface_create_drawable,
    cairo_surface_create_client,
    cairo_surface_create_notify,
    cairo_surface_delete,
    cairo_surface_present,
    cairo_surface_resize,
    cairo_surface_resize_notify,
    cairo_surface_set_offscreen,
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
        if (!init_xcomposite( gdi_display )) return STATUS_DLL_NOT_FOUND;
        break;
    case DLL_PROCESS_DETACH: break;
    }

    *(void **)ptr_out = &unix_funcs;
    return STATUS_SUCCESS;
}
