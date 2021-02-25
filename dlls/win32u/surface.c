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

#include <stdarg.h>
#include <stdlib.h>
#include <limits.h>

#include "windef.h"
#include "winbase.h"
#include "winnt.h"
#include "winuser.h"
#include "wingdi.h"

#include "wine/rbtree.h"
#include "wine/debug.h"

#include "unixlib.h"
#include "win32u.h"

WINE_DEFAULT_DEBUG_CHANNEL(win32u);

struct hwnd_surface
{
    struct wine_rb_entry entry;
    LONG ref;
    HWND hwnd;
    BOOL resize_notify;
    BOOL reparent_notify;
    struct hwnd_surface *toplevel;
    struct unix_surface *unix_surface;
};

static RTL_CRITICAL_SECTION surfaces_cs;
static RTL_CRITICAL_SECTION_DEBUG surfaces_cs_debug =
{
    0, 0, &surfaces_cs,
    { &surfaces_cs_debug.ProcessLocksList, &surfaces_cs_debug.ProcessLocksList },
    0, 0, { (DWORD_PTR)(__FILE__ ": surfaces_cs") }
};
static RTL_CRITICAL_SECTION surfaces_cs = { &surfaces_cs_debug, -1, 0, 0, 0, 0 };

static int hwnd_surface_compare( const void *key, const struct wine_rb_entry *entry )
{
    HWND hwnd = (HWND)key;
    return (LONG_PTR)hwnd - (LONG_PTR)WINE_RB_ENTRY_VALUE( entry, struct hwnd_surface, entry )->hwnd;
}
static struct wine_rb_tree toplevel_surfaces = { hwnd_surface_compare, NULL };
static struct wine_rb_tree client_surfaces = { hwnd_surface_compare, NULL };

static struct hwnd_surface *toplevel_surface_create( HWND hwnd )
{
    struct hwnd_surface *toplevel;

    if (!(toplevel = malloc(sizeof(*toplevel)))) return NULL;
    toplevel->ref = 1;
    toplevel->hwnd = hwnd;
    toplevel->resize_notify = FALSE;
    toplevel->reparent_notify = FALSE;
    toplevel->toplevel = NULL;
    toplevel->unix_surface = unix_funcs->surface_create_toplevel( hwnd );

    TRACE( "created toplevel surface %p for hwnd %p.\n", toplevel, hwnd );

    return toplevel;
}

static struct hwnd_surface *client_surface_create( HWND hwnd )
{
    struct hwnd_surface *client;

    if (!(client = malloc(sizeof(*client)))) return NULL;
    client->ref = 1;
    client->hwnd = hwnd;
    client->resize_notify = FALSE;
    client->reparent_notify = FALSE;
    client->toplevel = NULL;
    client->unix_surface = unix_funcs->surface_create_client( hwnd );

    TRACE( "created client surface %p for hwnd %p.\n", client, hwnd );

    return client;
}

static void hwnd_surface_delete( struct hwnd_surface *surface )
{
    TRACE( "surface %p.\n", surface );

    unix_funcs->surface_delete( surface->unix_surface );

    free(surface);
}

static struct hwnd_surface *hwnd_surface_grab( struct hwnd_surface *surface )
{
    if (!surface) return NULL;
    InterlockedIncrement( &surface->ref );
    return surface;
}

static void hwnd_surface_release( struct hwnd_surface *surface )
{
    if (!InterlockedDecrement( &surface->ref )) hwnd_surface_delete( surface );
}

static void set_toplevel_surface_for_hwnd( HWND hwnd, struct hwnd_surface *toplevel )
{
    RtlEnterCriticalSection( &surfaces_cs );
    wine_rb_put( &toplevel_surfaces, hwnd, &toplevel->entry );
    RtlLeaveCriticalSection( &surfaces_cs );
}

static struct hwnd_surface *get_toplevel_surface_for_hwnd( HWND hwnd )
{
    struct wine_rb_entry *entry;
    struct hwnd_surface *toplevel;
    RtlEnterCriticalSection( &surfaces_cs );
    if (!(entry = wine_rb_get( &toplevel_surfaces, hwnd ))) toplevel = NULL;
    else toplevel = WINE_RB_ENTRY_VALUE( entry, struct hwnd_surface, entry );
    RtlLeaveCriticalSection( &surfaces_cs );
    return toplevel;
}

static void set_client_surface_for_hwnd( HWND hwnd, struct hwnd_surface *surface )
{
    RtlEnterCriticalSection( &surfaces_cs );
    wine_rb_put( &client_surfaces, hwnd, &surface->entry );
    RtlLeaveCriticalSection( &surfaces_cs );
}

static struct hwnd_surface *get_client_surface_for_hwnd( HWND hwnd )
{
    struct wine_rb_entry *entry;
    struct hwnd_surface *surface;
    RtlEnterCriticalSection( &surfaces_cs );
    if (!(entry = wine_rb_get( &client_surfaces, hwnd ))) surface = NULL;
    else surface = WINE_RB_ENTRY_VALUE( entry, struct hwnd_surface, entry );
    RtlLeaveCriticalSection( &surfaces_cs );
    return surface;
}

void win32u_create_toplevel_surface( HWND hwnd )
{
    struct hwnd_surface *toplevel;

    TRACE( "hwnd %p.\n", hwnd );

    RtlEnterCriticalSection( &surfaces_cs );
    if (!(toplevel = get_toplevel_surface_for_hwnd( hwnd )))
    {
        toplevel = toplevel_surface_create( hwnd );
        set_toplevel_surface_for_hwnd( hwnd, toplevel );
    }
    RtlLeaveCriticalSection( &surfaces_cs );
}

void win32u_create_toplevel_surface_notify( HWND hwnd, LPARAM param )
{
    struct hwnd_surface *toplevel;
    UINT flags = SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE | SWP_NOREDRAW
                 | SWP_DEFERERASE | SWP_NOSENDCHANGING | SWP_STATECHANGED;

    TRACE( "hwnd %p.\n", hwnd );

    RtlEnterCriticalSection( &surfaces_cs );
    if ((toplevel = get_toplevel_surface_for_hwnd( hwnd )))
        unix_funcs->surface_create_notify( toplevel->unix_surface, param );
    else
        ERR( "no toplevel surface for hwnd %p!\n", hwnd );
    RtlLeaveCriticalSection( &surfaces_cs );

    if (toplevel) SetWindowPos( hwnd, 0, 0, 0, 0, 0, flags );
}

void win32u_create_client_surface( HWND hwnd )
{
    struct hwnd_surface *client;

    TRACE( "hwnd %p.\n", hwnd );

    RtlEnterCriticalSection( &surfaces_cs );
    if (!(client = get_client_surface_for_hwnd( hwnd )))
    {
        client = client_surface_create( hwnd );
        set_client_surface_for_hwnd( hwnd, client );
    }
    RtlLeaveCriticalSection( &surfaces_cs );
}

void win32u_create_client_surface_notify( HWND hwnd, LPARAM param )
{
    struct hwnd_surface *client;

    TRACE( "hwnd %p.\n", hwnd );

    RtlEnterCriticalSection( &surfaces_cs );
    if ((client = get_client_surface_for_hwnd( hwnd )))
        unix_funcs->surface_create_notify( client->unix_surface, param );
    else
        ERR( "no client surface for hwnd %p!\n", hwnd );
    RtlLeaveCriticalSection( &surfaces_cs );
}

void win32u_delete_toplevel_surface( HWND hwnd )
{
    struct hwnd_surface *toplevel;

    TRACE( "hwnd %p.\n", hwnd );

    RtlEnterCriticalSection( &surfaces_cs );
    if ((toplevel = get_toplevel_surface_for_hwnd( hwnd )))
    {
        wine_rb_remove( &toplevel_surfaces, &toplevel->entry );
        hwnd_surface_release( toplevel );
    }
    RtlLeaveCriticalSection( &surfaces_cs );
}

void win32u_delete_hwnd_surfaces( HWND hwnd )
{
    struct hwnd_surface *client;

    TRACE( "hwnd %p.\n", hwnd );

    RtlEnterCriticalSection( &surfaces_cs );
    win32u_delete_toplevel_surface( hwnd );
    if ((client = get_client_surface_for_hwnd( hwnd )))
    {
        wine_rb_remove( &client_surfaces, &client->entry );
        if (client->toplevel) hwnd_surface_release( client->toplevel );
        hwnd_surface_delete( client );
    }
    RtlLeaveCriticalSection( &surfaces_cs );
}

static struct hwnd_surface *find_toplevel_surface_for_hwnd( HWND hwnd )
{
    struct hwnd_surface *toplevel = NULL;
    RtlEnterCriticalSection( &surfaces_cs );
    while (hwnd && !(toplevel = get_toplevel_surface_for_hwnd( hwnd )))
        hwnd = GetAncestor( hwnd, GA_PARENT );
    RtlLeaveCriticalSection( &surfaces_cs );
    return toplevel;
}

void win32u_resize_hwnd_surfaces( HWND hwnd )
{
    struct hwnd_surface *toplevel, *client;
    RECT client_rect;

    TRACE( "hwnd %p.\n", hwnd );

    GetWindowRect( hwnd, &client_rect );

    RtlEnterCriticalSection( &surfaces_cs );
    if ((toplevel = get_toplevel_surface_for_hwnd( hwnd )))
        unix_funcs->surface_resize_notify( toplevel->unix_surface, NULL, &client_rect );
    if ((client = get_client_surface_for_hwnd( hwnd )))
    {
        if (client->toplevel) hwnd_surface_release( client->toplevel );
        client->toplevel = hwnd_surface_grab( find_toplevel_surface_for_hwnd( hwnd ) );
        unix_funcs->surface_resize_notify( client->unix_surface, client->toplevel ? client->toplevel->unix_surface : NULL, &client_rect );
    }
    RtlLeaveCriticalSection( &surfaces_cs );
}

void win32u_resize_hwnd_surfaces_notify( HWND hwnd, BOOL enable )
{
    struct hwnd_surface *toplevel, *client;

    TRACE( "hwnd %p, enable %d.\n", hwnd, enable );

    RtlEnterCriticalSection( &surfaces_cs );
    if ((toplevel = get_toplevel_surface_for_hwnd( hwnd )))
        toplevel->resize_notify = enable;
    if ((client = get_client_surface_for_hwnd( hwnd )))
        client->resize_notify = enable;
    RtlLeaveCriticalSection( &surfaces_cs );
}

void win32u_reparent_hwnd_surfaces_notify( HWND hwnd, BOOL enable )
{
    struct hwnd_surface *toplevel, *client;

    TRACE( "hwnd %p, enable %d.\n", hwnd, enable );

    RtlEnterCriticalSection( &surfaces_cs );
    if ((toplevel = get_toplevel_surface_for_hwnd( hwnd )))
        toplevel->reparent_notify = enable;
    if ((client = get_client_surface_for_hwnd( hwnd )))
        client->reparent_notify = enable;
    RtlLeaveCriticalSection( &surfaces_cs );
}

void win32u_present_client_surface( HWND hwnd, HRGN region )
{
    struct hwnd_surface *toplevel, *client;
    RGNDATA *data = NULL;
    POINT target_pos;
    DWORD i, size, clip_rect_count = 0;
    RECT source_rect, *clip_rects = NULL;

    TRACE( "hwnd %p, region %p.\n", hwnd, region );

    GetClientRect( hwnd, &source_rect );

    if (!region)
    {
        clip_rect_count = 1;
        clip_rects = &source_rect;
    }
    else if ((size = GetRegionData( region, 0, NULL )) && (data = malloc( size )))
    {
        GetRegionData( region, size, data );
        clip_rect_count = data->rdh.nCount;
        clip_rects = (RECT *)data->Buffer;
    }

    RtlEnterCriticalSection( &surfaces_cs );
    if (clip_rect_count && (client = get_client_surface_for_hwnd( hwnd )) && (toplevel = client->toplevel))
    {
        target_pos = *(POINT *)&source_rect;
        MapWindowPoints( hwnd, toplevel->hwnd, &target_pos, 1 );
        for (i = 0; i < clip_rect_count; ++i)
            MapWindowPoints( hwnd, toplevel->hwnd, (POINT *)&clip_rects[i], 2 );
        unix_funcs->surface_present( toplevel->unix_surface, client->unix_surface, &target_pos, &source_rect, clip_rect_count, clip_rects );
    }
    RtlLeaveCriticalSection( &surfaces_cs );

    if (data) free( data );
}

static const struct window_surface_funcs win32u_window_surface_funcs;

struct win32u_window_surface
{
    struct window_surface base;
    RTL_CRITICAL_SECTION cs;
    BITMAP bitmap;
    RECT bounds;
    HRGN region;

    struct hwnd_surface *toplevel;
    struct unix_surface *surface;
};

static inline void reset_bounds( RECT *bounds )
{
    bounds->left = bounds->top = INT_MAX;
    bounds->right = bounds->bottom = INT_MIN;
}

static struct win32u_window_surface *impl_from_window_surface( struct window_surface *base )
{
    if (base->funcs != &win32u_window_surface_funcs) return NULL;
    return CONTAINING_RECORD( base, struct win32u_window_surface, base );
}

static void CDECL win32u_window_surface_lock( struct window_surface *base )
{
    struct win32u_window_surface *impl = impl_from_window_surface( base );

    if (IsRectEmpty( &impl->base.rect )) return;

    TRACE( "surface %p.\n", base );

    RtlEnterCriticalSection( &impl->cs );
}

static void CDECL win32u_window_surface_unlock( struct window_surface *base )
{
    struct win32u_window_surface *impl = impl_from_window_surface( base );

    if (IsRectEmpty( &impl->base.rect )) return;

    TRACE( "surface %p.\n", base );

    RtlLeaveCriticalSection( &impl->cs );
}

static void *CDECL win32u_window_surface_get_info( struct window_surface *base, BITMAPINFO *info )
{
    struct win32u_window_surface *impl = impl_from_window_surface( base );

    TRACE( "surface %p.\n", base );

    info->bmiHeader.biSize = sizeof(info->bmiHeader);
    info->bmiHeader.biPlanes = impl->bitmap.bmPlanes;
    info->bmiHeader.biBitCount = impl->bitmap.bmBitsPixel;
    info->bmiHeader.biCompression = BI_RGB;
    info->bmiHeader.biWidth = impl->bitmap.bmWidth;
    info->bmiHeader.biHeight = -impl->bitmap.bmHeight;
    info->bmiHeader.biSizeImage = impl->bitmap.bmWidthBytes * impl->bitmap.bmHeight;

    return impl->bitmap.bmBits;
}

static RECT *CDECL win32u_window_surface_get_bounds( struct window_surface *base )
{
    struct win32u_window_surface *impl = impl_from_window_surface( base );

    TRACE( "surface %p.\n", base );

    return &impl->bounds;
}

static void CDECL win32u_window_surface_set_region( struct window_surface *base, HRGN region )
{
    struct win32u_window_surface *impl = impl_from_window_surface( base );

    TRACE( "surface %p.\n", base );

    base->funcs->lock(base);
    if (region)
    {
        if (!impl->region) impl->region = CreateRectRgn(0, 0, 0, 0);
        CombineRgn(impl->region, region, 0, RGN_COPY);
    }
    else
    {
        if (impl->region) DeleteObject(impl->region);
        impl->region = 0;
    }
    base->funcs->unlock(base);
}

static void CDECL win32u_window_surface_flush( struct window_surface *base )
{
    struct win32u_window_surface *impl = impl_from_window_surface( base );
    RGNDATA *data = NULL;
    DWORD size, clip_rect_count = 0;
    RECT target_rect, source_rect, *clip_rects = NULL;

    if (IsRectEmpty( &impl->base.rect )) return;

    TRACE( "surface %p.\n", base );

    RtlEnterCriticalSection( &impl->cs );

    if (!IsRectEmpty( &impl->bounds ))
    {
        target_rect = source_rect = impl->bounds;
        OffsetRect( &target_rect, impl->base.rect.left, impl->base.rect.top );

        if (!impl->region)
        {
            clip_rect_count = 1;
            clip_rects = &target_rect;
        }
        else if ((size = GetRegionData( impl->region, 0, NULL )) && (data = malloc( size )))
        {
            GetRegionData( impl->region, size, data );
            clip_rect_count = data->rdh.nCount;
            clip_rects = (RECT *)data->Buffer;
        }

        if (clip_rect_count)
            unix_funcs->surface_present( impl->toplevel->unix_surface, impl->surface, (POINT *)&target_rect.left,
                                         &source_rect, clip_rect_count, clip_rects );

        if (data) free( data );
    }

    reset_bounds( &impl->bounds );

    RtlLeaveCriticalSection( &impl->cs );
}

static void CDECL win32u_window_surface_destroy( struct window_surface *base )
{
    struct win32u_window_surface *impl = impl_from_window_surface( base );

    TRACE( "surface %p.\n", base );

    if (impl->region) DeleteObject(impl->region);
    unix_funcs->surface_delete( impl->surface );

    free(impl);
}

static const struct window_surface_funcs win32u_window_surface_funcs =
{
    win32u_window_surface_lock,
    win32u_window_surface_unlock,
    win32u_window_surface_get_info,
    win32u_window_surface_get_bounds,
    win32u_window_surface_set_region,
    win32u_window_surface_flush,
    win32u_window_surface_destroy,
};

static void create_window_surface( struct hwnd_surface *toplevel, const RECT *visible_rect, struct window_surface **window_surface )
{
    struct win32u_window_surface *impl = NULL;
    struct window_surface *iface;
    RECT surface_rect = *visible_rect;

    TRACE( "toplevel %p, visible_rect %s, window_surface %p.\n", toplevel, wine_dbgstr_rect( visible_rect ), window_surface );

    OffsetRect( &surface_rect, -surface_rect.left, -surface_rect.top );

    /* check that old surface is a win32u_window_surface, or release it */
    if ((iface = *window_surface) && !(impl = impl_from_window_surface( iface ))) window_surface_release( iface );

    /* if the underlying surface or rect didn't change, keep the same surface */
    if (impl && impl->toplevel == toplevel && EqualRect( &surface_rect, &impl->base.rect )) return;

    /* create a new window surface */
    *window_surface = NULL;
    if (impl) window_surface_release( &impl->base );
    if (!(impl = calloc(1, sizeof(*impl)))) return;

    impl->base.funcs = &win32u_window_surface_funcs;
    impl->base.ref = 1;
    impl->base.rect = surface_rect;
    list_init( &impl->base.entry );
    RtlInitializeCriticalSection( &impl->cs );
    impl->toplevel = hwnd_surface_grab( toplevel );
    impl->bitmap.bmWidth = surface_rect.right - surface_rect.left;
    impl->bitmap.bmHeight = surface_rect.bottom - surface_rect.top;
    impl->surface = unix_funcs->surface_create_drawable( toplevel->unix_surface, &impl->bitmap );
    impl->base.rect.right = impl->base.rect.left + impl->bitmap.bmWidth;
    impl->base.rect.bottom = impl->base.rect.top + impl->bitmap.bmHeight;
    impl->region = 0;
    reset_bounds( &impl->bounds );

    TRACE( "created window surface %p\n", &impl->base );

    *window_surface = &impl->base;
}

void win32u_update_window_surface( HWND hwnd, const RECT *visible_rect, struct window_surface **window_surface )
{
    struct hwnd_surface *toplevel;

    TRACE( "hwnd %p, visible_rect %s, window_surface %p.\n", hwnd, wine_dbgstr_rect( visible_rect ), window_surface );

    RtlEnterCriticalSection( &surfaces_cs );
    if ((toplevel = get_toplevel_surface_for_hwnd( hwnd )))
        create_window_surface( toplevel, visible_rect, window_surface );
    RtlLeaveCriticalSection( &surfaces_cs );
}
