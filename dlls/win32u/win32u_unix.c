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

#ifdef HAVE_CAIRO_CAIRO_XCB_H
#define MAKE_FUNCPTR(f) typeof(f) *p_##f;
MAKE_FUNCPTR(cairo_xcb_surface_create)
MAKE_FUNCPTR(cairo_xcb_surface_set_size)
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
#ifdef HAVE_CAIRO_CAIRO_XCB_H
    LOAD_FUNCPTR(cairo_xcb_surface_create)
    LOAD_FUNCPTR(cairo_xcb_surface_set_size)
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

#ifdef SONAME_LIBXCB_COMPOSITE

#define MAKE_FUNCPTR(f) typeof(f) *p_##f;
MAKE_FUNCPTR(xcb_composite_query_version)
MAKE_FUNCPTR(xcb_composite_query_version_reply)
MAKE_FUNCPTR(xcb_composite_redirect_window_checked)
MAKE_FUNCPTR(xcb_composite_unredirect_window_checked)
MAKE_FUNCPTR(xcb_composite_name_window_pixmap_checked)
#undef MAKE_FUNCPTR
BOOL has_xcb_composite = FALSE;

static BOOL init_xcb_composite(void)
{
    xcb_composite_query_version_reply_t *xcb_composite_version;
    xcb_query_extension_reply_t *xcb_composite;
    void *libxcb_composite;

    if (!(libxcb_composite = dlopen(SONAME_LIBXCB_COMPOSITE, RTLD_NOW)))
    {
        ERR("dlopen(%s, RTLD_NOW) failed!\n", SONAME_LIBXCB_COMPOSITE);
        return FALSE;
    }

#define LOAD_FUNCPTR(f) \
    if ((p_##f = dlsym(libxcb_composite, #f)) == NULL) \
    { \
        ERR("dlsym(%s, %s) failed!\n", SONAME_LIBXCB_COMPOSITE, #f); \
        goto error; \
    }

    LOAD_FUNCPTR(xcb_composite_query_version)
    LOAD_FUNCPTR(xcb_composite_query_version_reply)
    LOAD_FUNCPTR(xcb_composite_redirect_window_checked)
    LOAD_FUNCPTR(xcb_composite_unredirect_window_checked)
    LOAD_FUNCPTR(xcb_composite_name_window_pixmap_checked)
#undef LOAD_FUNCPTR

    if ((xcb_composite = p_xcb_query_extension_reply(xcb, p_xcb_query_extension(xcb, strlen("Composite"), "Composite"), NULL)))
    {
        has_xcb_composite = xcb_composite->present;
        free(xcb_composite);
    }
    if (has_xcb_composite && (xcb_composite_version = p_xcb_composite_query_version_reply(xcb, p_xcb_composite_query_version(xcb, 0, 4), NULL)))
    {
        TRACE("found Composite extension version %d.%d\n", xcb_composite_version->major_version, xcb_composite_version->minor_version);
        free(xcb_composite_version);
    }
    if (!has_xcb_composite)
    {
        ERR("Composite extension not available\n");
        goto error;
    }

    return TRUE;

error:
    has_xcb_composite = FALSE;
    dlclose(libxcb_composite);
    return FALSE;
}

#else

static BOOL init_xcb_composite(void)
{
    ERR("Composite support not compiled in!\n");
    return FALSE;
}

#endif

#ifdef SONAME_LIBXCB_XFIXES

#define MAKE_FUNCPTR(f) typeof(f) *p_##f;
MAKE_FUNCPTR(xcb_xfixes_query_version)
MAKE_FUNCPTR(xcb_xfixes_query_version_reply)
MAKE_FUNCPTR(xcb_xfixes_create_region_checked)
MAKE_FUNCPTR(xcb_xfixes_set_region_checked)
MAKE_FUNCPTR(xcb_xfixes_destroy_region_checked)
#undef MAKE_FUNCPTR
BOOL has_xcb_xfixes = FALSE;

static BOOL init_xcb_xfixes(void)
{
    xcb_xfixes_query_version_reply_t *xcb_xfixes_version;
    xcb_query_extension_reply_t *xcb_xfixes;
    void *libxcb_xfixes;

    if (!(libxcb_xfixes = dlopen(SONAME_LIBXCB_XFIXES, RTLD_NOW)))
    {
        ERR("dlopen(%s, RTLD_NOW) failed!\n", SONAME_LIBXCB_XFIXES);
        return FALSE;
    }

#define LOAD_FUNCPTR(f) \
    if ((p_##f = dlsym(libxcb_xfixes, #f)) == NULL) \
    { \
        ERR("dlsym(%s, %s) failed!\n", SONAME_LIBXCB_XFIXES, #f); \
        goto error; \
    }

    LOAD_FUNCPTR(xcb_xfixes_query_version)
    LOAD_FUNCPTR(xcb_xfixes_query_version_reply)
    LOAD_FUNCPTR(xcb_xfixes_create_region_checked)
    LOAD_FUNCPTR(xcb_xfixes_set_region_checked)
    LOAD_FUNCPTR(xcb_xfixes_destroy_region_checked)
#undef LOAD_FUNCPTR

    if ((xcb_xfixes = p_xcb_query_extension_reply(xcb, p_xcb_query_extension(xcb, strlen("XFIXES"), "XFIXES"), NULL)))
    {
        has_xcb_xfixes = xcb_xfixes->present;
        free(xcb_xfixes);
    }
    if (has_xcb_xfixes && (xcb_xfixes_version = p_xcb_xfixes_query_version_reply(xcb, p_xcb_xfixes_query_version(xcb, 5, 0), NULL)))
    {
        TRACE("found XFIXES extension version %d.%d\n", xcb_xfixes_version->major_version, xcb_xfixes_version->minor_version);
        free(xcb_xfixes_version);
    }
    if (!has_xcb_xfixes)
    {
        ERR("XFIXES extension not available\n");
        goto error;
    }

    return TRUE;

error:
    has_xcb_xfixes = FALSE;
    dlclose(libxcb_xfixes);
    return FALSE;
}

#else

static BOOL init_xcb_xfixes(void)
{
    ERR("XFIXES support not compiled in!\n");
    return FALSE;
}

#endif

#ifdef SONAME_LIBXCB_PRESENT

#define MAKE_FUNCPTR(f) typeof(f) *p_##f;
MAKE_FUNCPTR(xcb_present_query_version)
MAKE_FUNCPTR(xcb_present_query_version_reply)
MAKE_FUNCPTR(xcb_present_pixmap_checked)
#undef MAKE_FUNCPTR
BOOL has_xcb_present = FALSE;

static BOOL init_xcb_present(void)
{
    xcb_present_query_version_reply_t *xcb_present_version;
    xcb_query_extension_reply_t *xcb_present;
    void *libxcb_present;

    if (!(libxcb_present = dlopen(SONAME_LIBXCB_PRESENT, RTLD_NOW)))
    {
        ERR("dlopen(%s, RTLD_NOW) failed!\n", SONAME_LIBXCB_PRESENT);
        return FALSE;
    }

#define LOAD_FUNCPTR(f) \
    if ((p_##f = dlsym(libxcb_present, #f)) == NULL) \
    { \
        ERR("dlsym(%s, %s) failed!\n", SONAME_LIBXCB_PRESENT, #f); \
        goto error; \
    }

    LOAD_FUNCPTR(xcb_present_query_version)
    LOAD_FUNCPTR(xcb_present_query_version_reply)
    LOAD_FUNCPTR(xcb_present_pixmap_checked)
#undef LOAD_FUNCPTR

    if ((xcb_present = p_xcb_query_extension_reply(xcb, p_xcb_query_extension(xcb, strlen("Present"), "Present"), NULL)))
    {
        has_xcb_present = xcb_present->present;
        free(xcb_present);
    }
    if (has_xcb_present && (xcb_present_version = p_xcb_present_query_version_reply(xcb, p_xcb_present_query_version(xcb, 1, 0), NULL)))
    {
        TRACE("found Present extension version %d.%d\n", xcb_present_version->major_version, xcb_present_version->minor_version);
        free(xcb_present_version);
    }
    if (!has_xcb_present)
    {
        ERR("Present extension not available\n");
        goto error;
    }

    return TRUE;

error:
    has_xcb_present = FALSE;
    dlclose(libxcb_present);
    return FALSE;
}

#else

static BOOL init_xcb_present(void)
{
    ERR("Present support not compiled in!\n");
    return FALSE;
}

#endif

#ifdef HAVE_XCB_PRESENT_H
#define MAKE_FUNCPTR(f) extern typeof(f) *p_##f DECLSPEC_HIDDEN;
MAKE_FUNCPTR(xcb_present_query_version)
MAKE_FUNCPTR(xcb_present_query_version_reply)
MAKE_FUNCPTR(xcb_present_pixmap_checked)
#undef MAKE_FUNCPTR
#endif

#ifdef SONAME_LIBXCB

#ifdef HAVE_XCB_XCB_H
#define MAKE_FUNCPTR(f) typeof(f) *p_##f;
MAKE_FUNCPTR(xcb_configure_window_checked)
MAKE_FUNCPTR(xcb_connect)
MAKE_FUNCPTR(xcb_generate_id)
MAKE_FUNCPTR(xcb_connection_has_error)
MAKE_FUNCPTR(xcb_create_window_checked)
MAKE_FUNCPTR(xcb_destroy_window_checked)
MAKE_FUNCPTR(xcb_create_colormap_checked)
MAKE_FUNCPTR(xcb_depth_next)
MAKE_FUNCPTR(xcb_depth_visuals_iterator)
MAKE_FUNCPTR(xcb_flush)
MAKE_FUNCPTR(xcb_get_setup)
MAKE_FUNCPTR(xcb_get_window_attributes)
MAKE_FUNCPTR(xcb_get_window_attributes_reply)
MAKE_FUNCPTR(xcb_get_geometry)
MAKE_FUNCPTR(xcb_get_geometry_reply)
MAKE_FUNCPTR(xcb_reparent_window_checked)
MAKE_FUNCPTR(xcb_unmap_window_checked)
MAKE_FUNCPTR(xcb_map_window_checked)
MAKE_FUNCPTR(xcb_request_check)
MAKE_FUNCPTR(xcb_screen_allowed_depths_iterator)
MAKE_FUNCPTR(xcb_screen_next)
MAKE_FUNCPTR(xcb_setup_roots_iterator)
MAKE_FUNCPTR(xcb_visualtype_next)
MAKE_FUNCPTR(xcb_query_extension)
MAKE_FUNCPTR(xcb_query_extension_reply)
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
    LOAD_FUNCPTR(xcb_configure_window_checked)
    LOAD_FUNCPTR(xcb_connect)
    LOAD_FUNCPTR(xcb_generate_id)
    LOAD_FUNCPTR(xcb_connection_has_error)
    LOAD_FUNCPTR(xcb_create_window_checked)
    LOAD_FUNCPTR(xcb_destroy_window_checked)
    LOAD_FUNCPTR(xcb_create_colormap_checked)
    LOAD_FUNCPTR(xcb_depth_next)
    LOAD_FUNCPTR(xcb_depth_visuals_iterator)
    LOAD_FUNCPTR(xcb_flush)
    LOAD_FUNCPTR(xcb_get_setup)
    LOAD_FUNCPTR(xcb_get_window_attributes)
    LOAD_FUNCPTR(xcb_get_window_attributes_reply)
    LOAD_FUNCPTR(xcb_get_geometry)
    LOAD_FUNCPTR(xcb_get_geometry_reply)
    LOAD_FUNCPTR(xcb_reparent_window_checked)
    LOAD_FUNCPTR(xcb_unmap_window_checked)
    LOAD_FUNCPTR(xcb_map_window_checked)
    LOAD_FUNCPTR(xcb_request_check)
    LOAD_FUNCPTR(xcb_screen_allowed_depths_iterator)
    LOAD_FUNCPTR(xcb_screen_next)
    LOAD_FUNCPTR(xcb_setup_roots_iterator)
    LOAD_FUNCPTR(xcb_visualtype_next)
    LOAD_FUNCPTR(xcb_query_extension)
    LOAD_FUNCPTR(xcb_query_extension_reply)
#endif
#undef LOAD_FUNCPTR

    xcb = p_xcb_connect(NULL, &screen);
    if (p_xcb_connection_has_error(xcb))
    {
        ERR("failed to connect to X server using XCB!\n");
        goto error;
    }

    if (!init_xcb_composite()) goto error;
    if (!init_xcb_xfixes()) goto error;
    if (!init_xcb_present()) goto error;

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
    cairo_surface_create_toplevel,
    cairo_surface_create_foreign,
    cairo_surface_create_drawable,
    cairo_surface_create_foreign,
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
        if (!init_xcb()) return STATUS_DLL_NOT_FOUND;
        break;
    case DLL_PROCESS_DETACH: break;
    }

    *(void **)ptr_out = &unix_funcs;
    return STATUS_SUCCESS;
}
