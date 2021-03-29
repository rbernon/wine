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

struct hwnd_surfaces
{
    struct wine_rb_entry entry;
    LONG ref;
    HWND hwnd;
    BOOL offscreen;
    BOOL resize_notify;
    BOOL reparent_notify;
    struct unix_surface *toplevel;
    struct unix_surface *clients[32];
    LPARAM client_ids[32];
    LONG client_count;
    struct hwnd_surfaces *root;
};

static CRITICAL_SECTION surfaces_cs;
static CRITICAL_SECTION_DEBUG surfaces_cs_debug =
{
    0, 0, &surfaces_cs,
    { &surfaces_cs_debug.ProcessLocksList, &surfaces_cs_debug.ProcessLocksList },
    0, 0, { (DWORD_PTR)(__FILE__ ": surfaces_cs") }
};
static CRITICAL_SECTION surfaces_cs = { &surfaces_cs_debug, -1, 0, 0, 0, 0 };

static int hwnd_surfaces_compare( const void *key, const struct wine_rb_entry *entry )
{
    return (LONG_PTR)(HWND)key - (LONG_PTR)WINE_RB_ENTRY_VALUE( entry, struct hwnd_surfaces, entry )->hwnd;
}
static struct wine_rb_tree hwnd_surfaces = { hwnd_surfaces_compare, NULL };

static struct hwnd_surfaces *hwnd_surfaces_create( HWND hwnd )
{
    struct hwnd_surfaces *surfaces;

    if (!(surfaces = malloc(sizeof(*surfaces)))) return NULL;
    surfaces->ref = 1;
    surfaces->hwnd = hwnd;
    surfaces->offscreen = FALSE;
    surfaces->resize_notify = FALSE;
    surfaces->reparent_notify = FALSE;
    surfaces->toplevel = NULL;
    surfaces->client_count = 0;
    surfaces->root = NULL;

    TRACE( "created surfaces %p for hwnd %p.\n", surfaces, hwnd );

    return surfaces;
}

static void hwnd_surfaces_delete( struct hwnd_surfaces *surfaces )
{
    TRACE( "surfaces %p.\n", surfaces );

    while (surfaces->client_count--) unix_funcs->surface_delete( surfaces->clients[surfaces->client_count] );
    if (surfaces->toplevel) unix_funcs->surface_delete( surfaces->toplevel );

    free(surfaces);
}

static struct hwnd_surfaces *hwnd_surfaces_grab( struct hwnd_surfaces *surfaces )
{
    if (!surfaces) return NULL;
    InterlockedIncrement( &surfaces->ref );
    return surfaces;
}

static void hwnd_surfaces_release( struct hwnd_surfaces *surfaces )
{
    if (!InterlockedDecrement( &surfaces->ref )) hwnd_surfaces_delete( surfaces );
}

static struct hwnd_surfaces *find_surfaces_for_hwnd( HWND hwnd )
{
    struct wine_rb_entry *entry;
    struct hwnd_surfaces *surfaces;

    if (!(entry = wine_rb_get( &hwnd_surfaces, hwnd ))) surfaces = NULL;
    else surfaces = WINE_RB_ENTRY_VALUE( entry, struct hwnd_surfaces, entry );

    return surfaces;
}

static struct hwnd_surfaces *create_surfaces_for_hwnd( HWND hwnd )
{
    struct hwnd_surfaces *surfaces;

    if ((surfaces = find_surfaces_for_hwnd( hwnd ))) return surfaces;
    if (!(surfaces = hwnd_surfaces_create( hwnd ))) return NULL;
    wine_rb_put( &hwnd_surfaces, hwnd, &surfaces->entry );

    return surfaces;
}

static struct hwnd_surfaces *find_root_surfaces_for_hwnd( HWND hwnd )
{
    struct hwnd_surfaces *surfaces = NULL;
    LPARAM id;

    if (!(hwnd = GetAncestor( hwnd, GA_ROOT ))) return NULL;
    if (!(surfaces = create_surfaces_for_hwnd( hwnd ))) return NULL;
    if (!surfaces->toplevel && (id = (LPARAM)GetPropA( hwnd, "__wine_x11_whole_window" )) &&
        (surfaces->toplevel = unix_funcs->surface_create_foreign( hwnd )))
        unix_funcs->surface_create_notify( surfaces->toplevel, id );
    if (!surfaces->toplevel) return NULL;

    return surfaces;
}

static void hwnd_surfaces_update_root( struct hwnd_surfaces *surfaces )
{
    if (surfaces->root && surfaces->root != surfaces) hwnd_surfaces_release( surfaces->root );
    if (surfaces->toplevel) surfaces->root = surfaces;
    else
    {
        surfaces->root = hwnd_surfaces_grab( find_root_surfaces_for_hwnd( surfaces->hwnd ) );
        if (surfaces->root == surfaces) hwnd_surfaces_release( surfaces->root );
    }
}

void win32u_create_toplevel_surface( HWND hwnd )
{
    struct hwnd_surfaces *surfaces;

    TRACE( "hwnd %p.\n", hwnd );

    EnterCriticalSection( &surfaces_cs );
    if ((surfaces = create_surfaces_for_hwnd( hwnd )) && !surfaces->toplevel)
    {
        surfaces->toplevel = unix_funcs->surface_create_toplevel( hwnd );
        hwnd_surfaces_update_root( surfaces );
    }
    LeaveCriticalSection( &surfaces_cs );

    if (!surfaces) ERR( "failed to create surfaces for hwnd %p!\n", hwnd );
}

void win32u_create_toplevel_surface_notify( HWND hwnd, LPARAM param )
{
    struct hwnd_surfaces *surfaces;

    UINT flags = SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE
                 | SWP_DEFERERASE | SWP_NOSENDCHANGING | SWP_STATECHANGED;

    TRACE( "hwnd %p.\n", hwnd );

    EnterCriticalSection( &surfaces_cs );
    if ((surfaces = find_surfaces_for_hwnd( hwnd )) && surfaces->toplevel)
        unix_funcs->surface_create_notify( surfaces->toplevel, param );
    win32u_resize_hwnd_surfaces( hwnd );
    LeaveCriticalSection( &surfaces_cs );

    if (surfaces && surfaces->toplevel) SetWindowPos( hwnd, 0, 0, 0, 0, 0, flags );
}

void win32u_create_client_surface( HWND hwnd, LPARAM *id )
{
    struct hwnd_surfaces *surfaces;

    TRACE( "hwnd %p.\n", hwnd );

    EnterCriticalSection( &surfaces_cs );
    if ((surfaces = create_surfaces_for_hwnd( hwnd )))
    {
        hwnd_surfaces_update_root( surfaces );
        if (surfaces->client_count >= ARRAY_SIZE(surfaces->clients)) ERR( "too many client surfaces for hwnd %p\n", hwnd );
        else
        {
            surfaces->clients[surfaces->client_count] = unix_funcs->surface_create_client( hwnd, id );
            surfaces->client_ids[surfaces->client_count] = *id;
            surfaces->client_count++;
        }
    }
    LeaveCriticalSection( &surfaces_cs );
}

void win32u_create_client_surface_notify( HWND hwnd, LPARAM param )
{
    struct hwnd_surfaces *surfaces;
    ULONG i;

    TRACE( "hwnd %p.\n", hwnd );

    EnterCriticalSection( &surfaces_cs );
    if ((surfaces = find_surfaces_for_hwnd( hwnd )))
    {
        for (i = 0; i < surfaces->client_count; ++i) if (surfaces->client_ids[i] == param) break;
        if (i < surfaces->client_count) unix_funcs->surface_create_notify( surfaces->clients[i], param );
    }
    LeaveCriticalSection( &surfaces_cs );
}

void win32u_delete_toplevel_surface( HWND hwnd )
{
    struct hwnd_surfaces *surfaces;

    TRACE( "hwnd %p.\n", hwnd );

    EnterCriticalSection( &surfaces_cs );
    if ((surfaces = find_surfaces_for_hwnd( hwnd )) && surfaces->toplevel)
    {
        unix_funcs->surface_delete( surfaces->toplevel );
        surfaces->toplevel = NULL;
        hwnd_surfaces_update_root( surfaces );
    }
    LeaveCriticalSection( &surfaces_cs );
}

void win32u_delete_client_surface( HWND hwnd, LPARAM param )
{
    struct hwnd_surfaces *surfaces;
    ULONG i;

    TRACE( "hwnd %p.\n", hwnd );

    EnterCriticalSection( &surfaces_cs );
    if ((surfaces = find_surfaces_for_hwnd( hwnd )) && surfaces->client_count)
    {
        for (i = 0; i < surfaces->client_count; ++i) if (surfaces->client_ids[i] == param) break;
        if (i < surfaces->client_count)
        {
            unix_funcs->surface_delete( surfaces->clients[i] );
            surfaces->client_count--;
            memmove( surfaces->clients + i, surfaces->clients + i + 1, (surfaces->client_count - i) * sizeof(*surfaces->clients) );
            memmove( surfaces->client_ids + i, surfaces->client_ids + i + 1, (surfaces->client_count - i) * sizeof(*surfaces->client_ids) );
        }
    }
    LeaveCriticalSection( &surfaces_cs );
}

void win32u_delete_hwnd_surfaces( HWND hwnd )
{
    struct hwnd_surfaces *surfaces;

    TRACE( "hwnd %p.\n", hwnd );

    EnterCriticalSection( &surfaces_cs );
    if ((surfaces = find_surfaces_for_hwnd( hwnd )))
    {
        wine_rb_remove( &hwnd_surfaces, &surfaces->entry );
        hwnd_surfaces_release( surfaces );
    }
    LeaveCriticalSection( &surfaces_cs );
}

static void resize_toplevel_hwnd_surface( struct hwnd_surfaces *surfaces )
{
    RECT window_rect;

    GetWindowRect( surfaces->hwnd, &window_rect );

    if (surfaces->resize_notify) unix_funcs->surface_resize_notify( surfaces->toplevel, NULL, &window_rect );
    else unix_funcs->surface_resize( surfaces->toplevel, NULL, &window_rect );
}

static void resize_client_hwnd_surface( struct hwnd_surfaces *surfaces )
{
    RECT client_rect;
    ULONG i;

    GetClientRect( surfaces->hwnd, &client_rect );
    if (surfaces->root) MapWindowPoints( surfaces->hwnd, surfaces->root->hwnd, (POINT *)&client_rect, 2 );

    for (i = 0; i < surfaces->client_count; ++i)
        unix_funcs->surface_resize( surfaces->clients[i], surfaces->root ? surfaces->root->toplevel : NULL, &client_rect );
}

static BOOL CALLBACK resize_hwnd_surfaces( HWND hwnd, LPARAM lparam )
{
    struct hwnd_surfaces *surfaces;
    RECT client_rect;

    GetWindowRect( hwnd, &client_rect );

    EnterCriticalSection( &surfaces_cs );
    if ((surfaces = find_surfaces_for_hwnd( hwnd )))
    {
        hwnd_surfaces_update_root( surfaces );
        if (surfaces->toplevel) resize_toplevel_hwnd_surface( surfaces );
        if (surfaces->client_count) resize_client_hwnd_surface( surfaces );
    }
    LeaveCriticalSection( &surfaces_cs );

    return TRUE;
}

void win32u_resize_hwnd_surfaces( HWND hwnd )
{
    TRACE( "hwnd %p.\n", hwnd );

    EnterCriticalSection( &surfaces_cs );
    resize_hwnd_surfaces( hwnd, 0 );
    EnumChildWindows( hwnd, resize_hwnd_surfaces, 0 );
    LeaveCriticalSection( &surfaces_cs );
}

void win32u_resize_hwnd_surfaces_notify( HWND hwnd, BOOL enable )
{
    struct hwnd_surfaces *surfaces;

    TRACE( "hwnd %p, enable %d.\n", hwnd, enable );

    EnterCriticalSection( &surfaces_cs );
    if ((surfaces = find_surfaces_for_hwnd( hwnd )))
        surfaces->resize_notify = enable;
    LeaveCriticalSection( &surfaces_cs );
}

void win32u_reparent_hwnd_surfaces_notify( HWND hwnd, BOOL enable )
{
    struct hwnd_surfaces *surfaces;

    TRACE( "hwnd %p, enable %d.\n", hwnd, enable );

    EnterCriticalSection( &surfaces_cs );
    if ((surfaces = find_surfaces_for_hwnd( hwnd )))
        surfaces->reparent_notify = enable;
    LeaveCriticalSection( &surfaces_cs );
}

void win32u_present_client_surface( HWND hwnd, HRGN region, LPARAM param )
{
    struct hwnd_surfaces *surfaces;
    RGNDATA *data = NULL;
    POINT target_pos;
    DWORD i, size, clip_rect_count = 0;
    RECT source_rect, *clip_rects = NULL;
    BOOL offscreen;

    TRACE( "hwnd %p, region %p.\n", hwnd, region );

    GetClientRect( hwnd, &source_rect );

    if (!region)
    {
        clip_rect_count = 1;
        clip_rects = &source_rect;
        offscreen = FALSE;
    }
    else if ((size = GetRegionData( region, 0, NULL )) && (data = malloc( size )))
    {
        GetRegionData( region, size, data );
        clip_rect_count = data->rdh.nCount;
        clip_rects = (RECT *)data->Buffer;
        if (!data->rdh.nCount) offscreen = TRUE;
        if (!EqualRect( &source_rect, (RECT *)data->Buffer )) offscreen = TRUE;
        else offscreen = FALSE;
    }
    else offscreen = TRUE;

    EnterCriticalSection( &surfaces_cs );
    if ((surfaces = find_surfaces_for_hwnd( hwnd )))
    {
        if (surfaces->client_count > 1) offscreen = TRUE;
        if (surfaces->offscreen != offscreen)
        {
            if (offscreen) WARN( "putting %p client surfaces offscreen, expect degraded performance\n", hwnd );
            else WARN( "putting %p client surfaces onscreen, expect normal performance\n", hwnd );

            surfaces->offscreen = offscreen;
            for (i = 0; i < surfaces->client_count; ++i) unix_funcs->surface_set_offscreen( surfaces->clients[i], offscreen );
        }
        if (offscreen && clip_rect_count && surfaces->root && surfaces->root->toplevel)
        {
            target_pos = *(POINT *)&source_rect;
            MapWindowPoints( hwnd, surfaces->root->hwnd, &target_pos, 1 );
            for (i = 0; i < clip_rect_count; ++i) MapWindowPoints( hwnd, surfaces->root->hwnd, (POINT *)&clip_rects[i], 2 );
            for (i = 0; i < surfaces->client_count; ++i) if (surfaces->client_ids[i] == param) break;
            if (i < surfaces->client_count) unix_funcs->surface_present( surfaces->root->toplevel, surfaces->clients[i], &target_pos, &source_rect, clip_rect_count, clip_rects );
        }
    }
    LeaveCriticalSection( &surfaces_cs );

    if (data) free( data );
}

static const struct window_surface_funcs win32u_window_surface_funcs;

struct win32u_window_surface
{
    struct window_surface base;
    CRITICAL_SECTION cs;
    BITMAP bitmap;
    RECT bounds;
    HRGN region;
    HWND hwnd;

    struct hwnd_surfaces *target;
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

    EnterCriticalSection( &impl->cs );
}

static void CDECL win32u_window_surface_unlock( struct window_surface *base )
{
    struct win32u_window_surface *impl = impl_from_window_surface( base );

    if (IsRectEmpty( &impl->base.rect )) return;

    LeaveCriticalSection( &impl->cs );
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
    DWORD i, size, clip_rect_count = 0;
    RECT target_rect, source_rect, clip_rect, *clip_rects = NULL;

    if (IsRectEmpty( &impl->base.rect )) return;

    TRACE( "surface %p.\n", base );

    EnterCriticalSection( &impl->cs );

    if (!IsRectEmpty( &impl->bounds ))
    {
        clip_rect = source_rect = impl->bounds;
        if (impl->hwnd == impl->target->hwnd)
            GetClientRect( impl->hwnd, &target_rect );
        else
        {
            GetWindowRect( impl->hwnd, &target_rect );
            MapWindowPoints( NULL, impl->target->hwnd, (POINT *)&target_rect, 2 );
        }

        if (impl->region && (size = GetRegionData( impl->region, 0, NULL )) && (data = malloc( size )))
        {
            GetRegionData( impl->region, size, data );
            clip_rect_count = data->rdh.nCount;
            clip_rects = (RECT *)data->Buffer;
        }

        if (clip_rect_count == 0)
        {
            clip_rect_count = 1;
            clip_rects = &clip_rect;
        }

        for (i = 0; i < clip_rect_count; ++i) OffsetRect( clip_rects + i, target_rect.left, target_rect.top );
        OffsetRect( &target_rect, source_rect.left, source_rect.top );

        if (clip_rect_count)
        {
            EnterCriticalSection( &surfaces_cs );
            unix_funcs->surface_present( impl->target->toplevel, impl->surface, (POINT *)&target_rect.left,
                                         &source_rect, clip_rect_count, clip_rects );
            LeaveCriticalSection( &surfaces_cs );
        }

        if (data) free( data );
    }

    reset_bounds( &impl->bounds );

    LeaveCriticalSection( &impl->cs );
}

static void CDECL win32u_window_surface_destroy( struct window_surface *base )
{
    struct win32u_window_surface *impl = impl_from_window_surface( base );

    TRACE( "surface %p.\n", base );

    if (impl->region) DeleteObject(impl->region);
    unix_funcs->surface_delete( impl->surface );
    hwnd_surfaces_release( impl->target );

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

static void create_window_surface( struct hwnd_surfaces *target, HWND hwnd, const RECT *visible_rect, struct window_surface **window_surface )
{
    struct win32u_window_surface *impl = NULL;
    struct window_surface *iface;
    RECT surface_rect = *visible_rect;

    TRACE( "target %p, visible_rect %s, window_surface %p (%p).\n", target, wine_dbgstr_rect( visible_rect ), window_surface, window_surface ? *window_surface : NULL );

    OffsetRect( &surface_rect, -surface_rect.left, -surface_rect.top );

    /* check that old surface is a win32u_window_surface, or release it */
    if ((iface = *window_surface) && !(impl = impl_from_window_surface( iface ))) window_surface_release( iface );

    /* if the underlying surface or rect didn't change, keep the same surface */
    if (impl && impl->target == target && impl->hwnd == hwnd && EqualRect( &surface_rect, &impl->base.rect )) return;

    /* create a new window surface */
    *window_surface = NULL;
    if (impl) window_surface_release( &impl->base );
    if (!(impl = calloc(1, sizeof(*impl)))) return;

    impl->base.funcs = &win32u_window_surface_funcs;
    impl->base.ref = 1;
    impl->base.rect = surface_rect;
    list_init( &impl->base.entry );
    InitializeCriticalSection( &impl->cs );
    impl->target = hwnd_surfaces_grab( target );
    impl->bitmap.bmWidth = surface_rect.right - surface_rect.left;
    impl->bitmap.bmHeight = surface_rect.bottom - surface_rect.top;
    impl->surface = unix_funcs->surface_create_drawable( target->toplevel, &impl->bitmap );
    impl->base.rect.right = impl->base.rect.left + impl->bitmap.bmWidth;
    impl->base.rect.bottom = impl->base.rect.top + impl->bitmap.bmHeight;
    impl->hwnd = hwnd;
    impl->region = 0;
    reset_bounds( &impl->bounds );

    TRACE( "created window surface %p\n", &impl->base );

    *window_surface = &impl->base;
}

void win32u_update_window_surface( HWND hwnd, const RECT *visible_rect, struct window_surface **window_surface )
{
    struct hwnd_surfaces *surfaces;
    DWORD pid;

    TRACE( "hwnd %p, visible_rect %s, window_surface %p.\n", hwnd, wine_dbgstr_rect( visible_rect ), window_surface );

    EnterCriticalSection( &surfaces_cs );
    if ((surfaces = find_root_surfaces_for_hwnd( hwnd )) && (surfaces->hwnd == hwnd || (GetWindowThreadProcessId( surfaces->hwnd, &pid ) && pid != GetCurrentProcessId())))
        create_window_surface( surfaces, hwnd, visible_rect, window_surface );
    LeaveCriticalSection( &surfaces_cs );
}
