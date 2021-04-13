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

#include "win32u_unix.h"
#include "unixlib.h"
#include "x11drv.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(win32u);

#ifdef SONAME_LIBCAIRO

#ifdef HAVE_CAIRO_CAIRO_H
#define MAKE_FUNCPTR(f) typeof(f) *p_##f;
MAKE_FUNCPTR(cairo_surface_destroy)
#undef MAKE_FUNCPTR
#endif

#ifdef HAVE_CAIRO_CAIRO_XCB_H
#define MAKE_FUNCPTR(f) typeof(f) *p_##f;
MAKE_FUNCPTR(cairo_xcb_surface_create)
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
#ifdef HAVE_CAIRO_CAIRO_XCB_H
    LOAD_FUNCPTR(cairo_xcb_surface_create)
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

#ifdef SONAME_LIBXCB

#ifdef HAVE_XCB_XCB_H
#define MAKE_FUNCPTR(f) typeof(f) *p_##f;
MAKE_FUNCPTR(xcb_connect)
MAKE_FUNCPTR(xcb_connection_has_error)
MAKE_FUNCPTR(xcb_depth_next)
MAKE_FUNCPTR(xcb_depth_visuals_iterator)
MAKE_FUNCPTR(xcb_get_setup)
MAKE_FUNCPTR(xcb_get_geometry)
MAKE_FUNCPTR(xcb_get_geometry_reply)
MAKE_FUNCPTR(xcb_get_window_attributes)
MAKE_FUNCPTR(xcb_get_window_attributes_reply)
MAKE_FUNCPTR(xcb_screen_allowed_depths_iterator)
MAKE_FUNCPTR(xcb_screen_next)
MAKE_FUNCPTR(xcb_setup_roots_iterator)
MAKE_FUNCPTR(xcb_visualtype_next)
#undef MAKE_FUNCPTR
xcb_connection_t *xcb = NULL;
#endif

static BOOL init_xcb(void)
{
    void *libxcb;
    int screen = 0;

    if (!(libxcb = dlopen(SONAME_LIBXCB, RTLD_NOW)))
    {
        ERR("dlopen(%s, RTLD_NOW) failed!\n", SONAME_LIBXCB);
        return FALSE;
    }

#define LOAD_FUNCPTR(f) \
    if ((p_##f = dlsym(libxcb, #f)) == NULL) \
    { \
        ERR("dlsym(%s, %s) failed!\n", SONAME_LIBXCB, #f); \
        goto error; \
    }

#ifdef HAVE_XCB_XCB_H
    LOAD_FUNCPTR(xcb_connect)
    LOAD_FUNCPTR(xcb_connection_has_error)
    LOAD_FUNCPTR(xcb_depth_next)
    LOAD_FUNCPTR(xcb_depth_visuals_iterator)
    LOAD_FUNCPTR(xcb_get_setup)
    LOAD_FUNCPTR(xcb_get_geometry)
    LOAD_FUNCPTR(xcb_get_geometry_reply)
    LOAD_FUNCPTR(xcb_get_window_attributes)
    LOAD_FUNCPTR(xcb_get_window_attributes_reply)
    LOAD_FUNCPTR(xcb_screen_allowed_depths_iterator)
    LOAD_FUNCPTR(xcb_screen_next)
    LOAD_FUNCPTR(xcb_setup_roots_iterator)
    LOAD_FUNCPTR(xcb_visualtype_next)
#endif
#undef LOAD_FUNCPTR

    xcb = p_xcb_connect(NULL, &screen);
    if (p_xcb_connection_has_error(xcb))
    {
        ERR("failed to connect to X server using XCB!\n");
        goto error;
    }

    return TRUE;

error:
    xcb = NULL;
    dlclose(libxcb);
    return FALSE;
}

#else

static BOOL init_xcb(void)
{
    ERR("xcb support not compiled in!\n");
    return FALSE;
}

#endif

static struct unix_funcs unix_funcs = {
    cairo_surface_create_foreign,
    cairo_surface_delete,
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
        if (!init_xcb()) return STATUS_DLL_NOT_FOUND;
        break;
    case DLL_PROCESS_DETACH: break;
    }

    *(void **)ptr_out = &unix_funcs;
    return STATUS_SUCCESS;
}
