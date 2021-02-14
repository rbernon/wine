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

#include <limits.h>

#include "wine/rbtree.h"

#include "x11.h"
#include "x11drv.h"

#define PERSISTENT_MAPPING

WINE_DEFAULT_DEBUG_CHANNEL(win32u);

static const struct window_surface_funcs x11_window_surface_funcs;

struct x11_window_surface
{
    struct window_surface base;
    struct wine_rb_entry entry;
    RTL_CRITICAL_SECTION cs;

    BITMAPINFOHEADER info;
    RECT bounds;
    HWND hwnd;
    Window window;

    cairo_rectangle_int_t cairo_rect;
    cairo_surface_t *window_surface;
    cairo_surface_t *image_surface;
    int recursion_count;
};

#if 0
static RTL_CRITICAL_SECTION hwnd_surfaces_cs;
static RTL_CRITICAL_SECTION_DEBUG hwnd_surfaces_cs_debug = {
    0,
    0,
    &hwnd_surfaces_cs,
    {&hwnd_surfaces_cs_debug.ProcessLocksList, &hwnd_surfaces_cs_debug.ProcessLocksList},
    0,
    0,
    {(DWORD_PTR)(__FILE__ ": hwnd_surfaces_cs")}};
static RTL_CRITICAL_SECTION hwnd_surfaces_cs = {&hwnd_surfaces_cs_debug, -1, 0, 0, 0, 0};

static int hwnd_surface_compare( const void *key, const struct wine_rb_entry *entry )
{
    HWND hwnd = (HWND)key;
    return hwnd - WINE_RB_ENTRY_VALUE( entry, struct x11_window_surface, entry )->hwnd;
}

static struct wine_rb_tree hwnd_surfaces = {hwnd_surface_compare, NULL};
#endif

static inline void cairo_rectangle_int_from_rect( const RECT *rect, cairo_rectangle_int_t *cairo )
{
    cairo->x = rect->left;
    cairo->y = rect->top;
    cairo->width = rect->right - rect->left;
    cairo->height = rect->bottom - rect->top;
}

const char *debugstr_surface( struct x11_window_surface *impl )
{
    return wine_dbg_sprintf(
        "base %p, rect %s, bounds %s, hwnd %p, window %lx, window_surface %p, image_surface %p, recursion_count %d",
        &impl->base, wine_dbgstr_rect( &impl->base.rect ), wine_dbgstr_rect( &impl->bounds ),
        impl->hwnd, impl->window, impl->window_surface, impl->image_surface, impl->recursion_count );
}

static struct x11_window_surface *impl_from_window_surface( struct window_surface *base )
{
    if (base->funcs != &x11_window_surface_funcs) return NULL;
    return CONTAINING_RECORD( base, struct x11_window_surface, base );
}

static void CDECL x11_window_surface_lock( struct window_surface *base )
{
    struct x11_window_surface *impl = impl_from_window_surface( base );

    if (IsRectEmpty( &impl->base.rect )) return;

    TRACE( "surface %s stub!\n", debugstr_surface( impl ) );

    RtlEnterCriticalSection( &impl->cs );
#ifndef PERSISTENT_MAPPING
    if (impl->window_surface && !impl->recursion_count++)
    {
        p_cairo_surface_flush( impl->window_surface );
        impl->image_surface = p_cairo_surface_map_to_image( impl->window_surface, &impl->cairo_rect );
    }
#endif
}

static void CDECL x11_window_surface_unlock( struct window_surface *base )
{
    struct x11_window_surface *impl = impl_from_window_surface( base );

    if (IsRectEmpty( &impl->base.rect )) return;

    TRACE( "surface %s stub!\n", debugstr_surface( impl ) );

#ifndef PERSISTENT_MAPPING
    if (impl->window_surface && !--impl->recursion_count)
    {
        if (!IsRectEmpty( &impl->bounds ))
        {
            cairo_rectangle_int_t cairo_rect;
            cairo_rectangle_int_from_rect( &impl->bounds, &cairo_rect );
            p_cairo_surface_mark_dirty_rectangle(
                impl->image_surface, cairo_rect.x, cairo_rect.y, cairo_rect.width, cairo_rect.height );
        }

        p_cairo_surface_unmap_image( impl->window_surface, impl->image_surface );
        impl->image_surface = NULL;

        reset_bounds( &impl->bounds );
    }
#endif
    RtlLeaveCriticalSection( &impl->cs );
}

static void *CDECL x11_window_surface_get_info( struct window_surface *base, BITMAPINFO *info )
{
    struct x11_window_surface *impl = impl_from_window_surface( base );
    cairo_surface_t *surface;
    void *bits = NULL;

    TRACE( "surface %s stub!\n", debugstr_surface( impl ) );

    RtlEnterCriticalSection( &impl->cs );
    if (impl->window_surface && !impl->info.biSizeImage)
    {
#ifdef PERSISTENT_MAPPING
        surface = impl->image_surface;
#else
        p_cairo_surface_flush( impl->window_surface );
        if (impl->image_surface) surface = impl->image_surface;
        else surface = p_cairo_surface_map_to_image( impl->window_surface, &impl->cairo_rect );
#endif

        impl->info.biSize = sizeof(impl->info);
        impl->info.biPlanes = 1;
        impl->info.biBitCount = 32;
        impl->info.biCompression = BI_RGB;
        impl->info.biWidth = p_cairo_image_surface_get_width( surface );
        impl->info.biHeight = -p_cairo_image_surface_get_height( surface );
        impl->info.biSizeImage =
            p_cairo_image_surface_get_height( surface ) * p_cairo_image_surface_get_stride( surface );

#ifndef PERSISTENT_MAPPING
        if (!impl->image_surface) p_cairo_surface_unmap_image( impl->window_surface, surface );
        else p_cairo_surface_flush( impl->image_surface );
#endif
    }

    bits = impl->image_surface ? p_cairo_image_surface_get_data( impl->image_surface ) : NULL;
    RtlLeaveCriticalSection( &impl->cs );

    info->bmiHeader = impl->info;
    return bits;
}

static RECT *CDECL x11_window_surface_get_bounds( struct window_surface *base )
{
    struct x11_window_surface *impl = impl_from_window_surface( base );

    TRACE( "surface %s stub!\n", debugstr_surface( impl ) );

    return &impl->bounds;
}

static void CDECL x11_window_surface_set_region( struct window_surface *base, HRGN region )
{
    FIXME( "base %p stub!\n", base );
}

static void CDECL x11_window_surface_flush( struct window_surface *base )
{
    struct x11_window_surface *impl = impl_from_window_surface( base );

    if (IsRectEmpty( &impl->base.rect )) return;

    TRACE( "surface %s stub!\n", debugstr_surface( impl ) );

    RtlEnterCriticalSection( &impl->cs );
#ifndef PERSISTENT_MAPPING
    if (impl->window_surface) p_cairo_surface_flush( impl->window_surface );
#else
    if (impl->window_surface && !IsRectEmpty( &impl->bounds ))
    {
        cairo_rectangle_int_t cairo_rect;
        cairo_rectangle_int_from_rect( &impl->bounds, &cairo_rect );
        p_cairo_surface_mark_dirty_rectangle(
            impl->image_surface, cairo_rect.x, cairo_rect.y, cairo_rect.width, cairo_rect.height );

        p_cairo_surface_unmap_image( impl->window_surface, impl->image_surface );
        p_cairo_surface_flush( impl->window_surface );
        impl->image_surface = p_cairo_surface_map_to_image( impl->window_surface, &impl->cairo_rect );
        reset_bounds( &impl->bounds );
    }
#endif
    RtlLeaveCriticalSection( &impl->cs );
}

static void CDECL x11_window_surface_destroy( struct window_surface *base )
{
    struct x11_window_surface *impl = impl_from_window_surface( base );

    TRACE( "surface %s stub!\n", debugstr_surface( impl ) );

#if 0
    RtlEnterCriticalSection(&hwnd_surfaces_cs);
    wine_rb_remove(&hwnd_surfaces, &impl->entry);
    RtlLeaveCriticalSection(&hwnd_surfaces_cs);
#endif

    if (impl->image_surface)
        p_cairo_surface_unmap_image( impl->window_surface, impl->image_surface );
    if (impl->window_surface) p_cairo_surface_destroy( impl->window_surface );
    RtlFreeHeap( GetProcessHeap(), 0, impl );
}

static const struct window_surface_funcs x11_window_surface_funcs = {
    x11_window_surface_lock,       x11_window_surface_unlock,     x11_window_surface_get_info,
    x11_window_surface_get_bounds, x11_window_surface_set_region, x11_window_surface_flush,
    x11_window_surface_destroy};

static inline void get_surface_rect( const RECT *screen_rect, const RECT *visible_rect, RECT *surface_rect )
{
    *surface_rect = *screen_rect;
    if (!IntersectRect( surface_rect, surface_rect, visible_rect ))
        SetRect( surface_rect, 0, 0, 0, 0 );
    else OffsetRect( surface_rect, -visible_rect->left, -visible_rect->top );
}

struct window_surface *CDECL x11_create_window_surface( const RECT *screen_rect, const RECT *visible_rect,
                                                        HWND hwnd, UINT64 unix_handle )
{
    struct x11_window_surface *impl;
#if 0
    struct wine_rb_entry *entry;
#endif
    XWindowAttributes attr;

    TRACE( "screen_rect %s, visible_rect %s, hwnd %p, unix_handle %lx.\n",
           wine_dbgstr_rect( screen_rect ), wine_dbgstr_rect( visible_rect ), hwnd, (Window)unix_handle );

#if 0
    RtlEnterCriticalSection(&hwnd_surfaces_cs);
    if ((entry = wine_rb_get(&hwnd_surfaces, hwnd)))
    {
        impl = WINE_RB_ENTRY_VALUE(entry, struct x11_window_surface, entry);
        window_surface_add_ref(&impl->base);
    }
    else
#endif
    if ((impl = RtlAllocateHeap( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*impl) )))
    {
        impl->base.funcs = &x11_window_surface_funcs;
        impl->base.ref = 1;
        list_init( &impl->base.entry );
        RtlInitializeCriticalSection( &impl->cs );

        reset_bounds( &impl->bounds );
        get_surface_rect( screen_rect, visible_rect, &impl->base.rect );
        cairo_rectangle_int_from_rect( &impl->base.rect, &impl->cairo_rect );
        impl->hwnd = hwnd;
        impl->window = unix_handle;

        if (impl->window && !IsRectEmpty( &impl->base.rect ) &&
            pXGetWindowAttributes( gdi_display, impl->window, &attr ))
        {
            impl->window_surface = p_cairo_xlib_surface_create(
                gdi_display, impl->window, attr.visual, attr.width, attr.height );
            /*p_cairo_xlib_surface_set_size(impl->window_surface, attr.width, attr.height);*/
#ifdef PERSISTENT_MAPPING
            impl->image_surface = p_cairo_surface_map_to_image( impl->window_surface, &impl->cairo_rect );
#endif
        }
    }
#if 0
        wine_rb_put(&hwnd_surfaces, hwnd, &impl->entry);
    }
    RtlLeaveCriticalSection(&hwnd_surfaces_cs);
#endif

    TRACE( "created surface %s.\n", debugstr_surface( impl ) );
    return impl ? &impl->base : NULL;
}

struct window_surface *CDECL x11_resize_window_surface( struct window_surface *base, const RECT *screen_rect,
                                                        const RECT *visible_rect, HWND hwnd, UINT64 unix_handle )
{
    struct x11_window_surface *impl = impl_from_window_surface( base );
    XWindowAttributes attr;
    RECT surface_rect;

    TRACE( "base %p, screen_rect %s, visible_rect %s, hwnd %p, unix_handle %lx.\n", base,
           wine_dbgstr_rect( screen_rect ), wine_dbgstr_rect( visible_rect ), hwnd, (Window)unix_handle );

    get_surface_rect( screen_rect, visible_rect, &surface_rect );
    if (impl && impl->hwnd == hwnd && impl->window == unix_handle && EqualRect( &surface_rect, &base->rect ))
    {
        RtlEnterCriticalSection( &impl->cs );
        if (!impl->window_surface && impl->window && !IsRectEmpty( &impl->base.rect ) &&
            pXGetWindowAttributes( gdi_display, impl->window, &attr ))
        {
            impl->window_surface = p_cairo_xlib_surface_create(
                gdi_display, impl->window, attr.visual, attr.width, attr.height );
            /*p_cairo_xlib_surface_set_size(impl->window_surface, attr.width, attr.height);*/
#ifdef PERSISTENT_MAPPING
            impl->image_surface = p_cairo_surface_map_to_image( impl->window_surface, &impl->cairo_rect );
#endif
        }
        RtlLeaveCriticalSection( &impl->cs );
        return base;
    }

    window_surface_release( base );
    return x11_create_window_surface( screen_rect, visible_rect, hwnd, unix_handle );
}
