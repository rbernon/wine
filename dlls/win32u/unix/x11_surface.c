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

WINE_DEFAULT_DEBUG_CHANNEL(win32u);

struct x11_window_surface
{
    struct window_surface base;
};

static struct x11_window_surface *impl_from_window_surface( struct window_surface *base )
{
    return CONTAINING_RECORD( base, struct x11_window_surface, base );
}

static void CDECL x11_window_surface_lock( struct window_surface *base )
{
    FIXME( "base %p stub!\n", base );
}

static void CDECL x11_window_surface_unlock( struct window_surface *base )
{
    FIXME( "base %p stub!\n", base );
}

static void *CDECL x11_window_surface_get_info( struct window_surface *base, BITMAPINFO *info )
{
    static DWORD dummy_data;

    FIXME( "base %p stub!\n", base );

    info->bmiHeader.biSize = sizeof(info->bmiHeader);
    info->bmiHeader.biWidth = base->rect.right;
    info->bmiHeader.biHeight = base->rect.bottom;
    info->bmiHeader.biPlanes = 1;
    info->bmiHeader.biBitCount = 32;
    info->bmiHeader.biCompression = BI_RGB;
    info->bmiHeader.biSizeImage = 0;
    info->bmiHeader.biXPelsPerMeter = 0;
    info->bmiHeader.biYPelsPerMeter = 0;
    info->bmiHeader.biClrUsed = 0;
    info->bmiHeader.biClrImportant = 0;

    return &dummy_data;
}

static RECT *CDECL x11_window_surface_get_bounds( struct window_surface *base )
{
    static RECT dummy_bounds;
    FIXME( "base %p stub!\n", base );
    return &dummy_bounds;
}

static void CDECL x11_window_surface_set_region( struct window_surface *base, HRGN region )
{
    FIXME( "base %p stub!\n", base );
}

static void CDECL x11_window_surface_flush( struct window_surface *base )
{
    FIXME( "base %p stub!\n", base );
}

static void CDECL x11_window_surface_destroy( struct window_surface *base )
{
    struct x11_window_surface *impl = impl_from_window_surface( base );

    FIXME( "base %p stub!\n", base );

    RtlFreeHeap( GetProcessHeap(), 0, impl );
}

static const struct window_surface_funcs x11_window_surface_funcs = {
    x11_window_surface_lock,       x11_window_surface_unlock,     x11_window_surface_get_info,
    x11_window_surface_get_bounds, x11_window_surface_set_region, x11_window_surface_flush,
    x11_window_surface_destroy};

static inline void get_surface_rect( const RECT *visible_rect, RECT *surface_rect )
{
    *surface_rect = *visible_rect;
    OffsetRect( surface_rect, -visible_rect->left, -visible_rect->top );
    surface_rect->left &= ~31;
    surface_rect->top &= ~31;
    surface_rect->right = max( surface_rect->left + 32, (surface_rect->right + 31) & ~31 );
    surface_rect->bottom = max( surface_rect->top + 32, (surface_rect->bottom + 31) & ~31 );
}

struct window_surface *CDECL x11_create_window_surface( const RECT *visible_rect )
{
    struct x11_window_surface *impl;

    TRACE( "visible_rect %s.\n", wine_dbgstr_rect( visible_rect ) );

    if (!(impl = RtlAllocateHeap( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*impl) ))) return NULL;

    impl->base.funcs = &x11_window_surface_funcs;
    impl->base.ref = 1;
    list_init( &impl->base.entry );
    get_surface_rect( visible_rect, &impl->base.rect );

    return &impl->base;
}

struct window_surface *CDECL x11_resize_window_surface( struct window_surface *base, const RECT *visible_rect )
{
    RECT surface_rect;

    TRACE( "base %p, visible_rect %s.\n", base, wine_dbgstr_rect( visible_rect ) );

    get_surface_rect( visible_rect, &surface_rect );
    if (base->funcs == &x11_window_surface_funcs && EqualRect( &surface_rect, &base->rect ))
        return base;

    window_surface_release( base );
    return x11_create_window_surface( visible_rect );
}
