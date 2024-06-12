/*
 * Graphics driver management functions
 *
 * Copyright 1994 Bob Amstadt
 * Copyright 1996, 2001 Alexandre Julliard
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

#if 0
#pragma makedep unix
#endif

#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>

#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"

#include "ntgdi_private.h"
#include "ntuser_private.h"

#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(cairo);

static const struct gdi_dc_funcs cairodrv_gdi_driver;

struct surface
{
    LONG ref;
    cairo_surface_t *source;
    cairo_t *cairo;
    pthread_mutex_t lock;
};

static struct surface *surface_create( cairo_surface_t *target )
{
    struct surface *surface;
    if (!(surface = calloc(1, sizeof(*surface)))) return NULL;
    surface->ref = 1;
    surface->cairo = cairo_create( target );
    pthread_mutex_init(&surface->lock, NULL);
    return surface;
}

static void surface_acquire( struct surface *surface )
{
    InterlockedIncrement(&surface->ref);
}

static void surface_release( struct surface *surface )
{
    if (!InterlockedDecrement(&surface->ref))
    {
        if (surface->source)
        {
            unsigned char *pixels = cairo_image_surface_get_data( surface->source );
            cairo_surface_destroy( surface->source );
            free( pixels );
        }

        pthread_mutex_destroy(&surface->lock);
        cairo_destroy(surface->cairo);
        free(surface);
    }
}

struct device
{
    struct gdi_physdev dev;
    struct surface *surface;
    RECT rect;

    LOGBRUSH brush;
    struct brush_pattern brush_pattern;

    LOGPEN pen;
    LOGBRUSH pen_brush;
    struct brush_pattern pen_brush_pattern;
};

static struct device *device_from_physdev( PHYSDEV dev )
{
    return CONTAINING_RECORD( dev, struct device, dev );
}

static const char *dbgstr_bitblt_coords( const struct bitblt_coords *coords )
{
    RECT log = {coords->log_x, coords->log_y, coords->log_x+coords->log_width, coords->log_y+coords->log_height};
    RECT phy = {coords->x, coords->y, coords->x+coords->width, coords->y+coords->height};
    return wine_dbg_sprintf("(log %s phy %s vis %s)", wine_dbgstr_rect(&log), wine_dbgstr_rect(&phy), wine_dbgstr_rect(&coords->visrect));
    return wine_dbgstr_rect(&coords->visrect);
};

static void debug_cairo_rect( cairo_t *cairo, const RECT *rect, const char *func )
{
    float r = rand() / (float)RAND_MAX, g = rand() / (float)RAND_MAX, b = rand() / (float)RAND_MAX;

if (0)
{
    cairo_set_source_rgb( cairo, 0, 0, 0 );
    cairo_paint_with_alpha( cairo, 0.1 );
}

    cairo_set_operator( cairo, CAIRO_OPERATOR_SOURCE );
    cairo_set_source_rgba( cairo, r, g, b, 0.5 );
    cairo_rectangle( cairo, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top );
    cairo_fill_preserve( cairo );
    cairo_set_source_rgba( cairo, 0.5 + r / 2.0, 0.5 + g / 2.0, 0.5 + b / 2.0, 0.5 );
    cairo_stroke( cairo );

    cairo_set_line_width( cairo, 0.5 );
    cairo_set_source_rgba( cairo, 0.75 + r / 4.0, 0.75 + g / 4.0, 0.75 + b / 4.0, 1.0 );
    cairo_move_to( cairo, rect->left + 1, rect->top + 10 );
    cairo_text_path( cairo, func );
    cairo_stroke( cairo );
}

static struct surface *device_lock_surface( struct device *device )
{
    struct surface *surface = device->surface;
    if (!device->surface) return NULL;

    surface_acquire( surface );
    pthread_mutex_lock( &surface->lock );

    cairo_identity_matrix( surface->cairo );
    cairo_translate( surface->cairo, device->rect.left, device->rect.top );
    return surface;
}

static void device_unlock_surface( struct device *device, struct surface *surface )
{
    pthread_mutex_unlock( &surface->lock );
    surface_release( surface );
}

struct cairodrv_window
{
    WND *locked_win;
    struct surface *surface;
};

static void cairodrv_window_init( HWND hwnd, struct cairodrv_window *window )
{
    struct cairodrv_window *previous = window;
    WND *win;

    if ((win = get_win_ptr( hwnd )) && win != WND_OTHER_PROCESS && win != WND_DESKTOP)
    {
        previous = win->cairodrv_window;
        win->cairodrv_window = window;
        release_win_ptr( win );
    }

    free( previous );
}

static struct cairodrv_window *cairodrv_window_lock( HWND hwnd )
{
    struct cairodrv_window *window;
    WND *win;

    if (!(win = get_win_ptr( hwnd )) || win == WND_OTHER_PROCESS || win == WND_DESKTOP) return NULL;
    if (!(window = win->cairodrv_window))
    {
        release_win_ptr( win );
        return NULL;
    }

    window->locked_win = win;
    return window;
}

static void cairodrv_window_unlock( struct cairodrv_window *window )
{
    WND *win = window->locked_win;
    window->locked_win = NULL;
    release_win_ptr( win );
}

static void cairodrv_window_destroy( struct cairodrv_window *window )
{
    surface_release( window->surface );
    free( window );
}

static cairo_format_t bpp_to_cairo_format( UINT bpp )
{
    switch (bpp)
    {
    case 1: return CAIRO_FORMAT_A1;
    case 8: return CAIRO_FORMAT_A8;
    case 16: return CAIRO_FORMAT_RGB16_565;
    case 24: return CAIRO_FORMAT_RGB24;
    case 32: return CAIRO_FORMAT_ARGB32;
    }
    return CAIRO_FORMAT_INVALID;
}

static inline INT get_rop2_from_rop(INT rop)
{
    return (((rop >> 18) & 0x0c) | ((rop >> 16) & 0x03)) + 1;
}

static cairo_surface_t *surface_from_pattern( const BITMAPINFO *info, const struct gdi_image_bits bits )
{
    UINT bpp, width, height, stride;
    unsigned char *pixels = bits.ptr;

    bpp = info->bmiHeader.biBitCount;
    width = info->bmiHeader.biWidth;
    height = abs(info->bmiHeader.biHeight);
    stride = info->bmiHeader.biSizeImage / height;
    if (info->bmiHeader.biHeight < 0) pixels -= info->bmiHeader.biSizeImage + stride;
TRACE("bpp %u, width %d, height %d, stride %d bottomup %u\n", bpp, width, height, stride, info->bmiHeader.biHeight < 0);

    return cairo_image_surface_create_for_data( pixels, bpp_to_cairo_format( bpp ), width, height, stride );
}

static void get_buffer_stride( const BITMAPINFO *src_info, const RECT *src_rect, UINT *dst_stride, RECT *dst_rect )
{
    static const SIZE_T align = sizeof(UINT32) - 1;
    UINT bpp = src_info->bmiHeader.biBitCount;

    dst_rect->top = 0;
    dst_rect->bottom = src_rect->bottom - src_rect->top;
    dst_rect->left = (src_rect->left * bpp - (src_rect->left * bpp / 8) * 8) / bpp;
    dst_rect->right = dst_rect->left + (src_rect->right - src_rect->left);

    *dst_stride = ((dst_rect->right * bpp + 7) / 8 + align) & ~align;
}

static void copy_image_bits( const struct gdi_image_bits *bits, const BITMAPINFO *src_info, const RECT *src_rect,
                             unsigned char *dst, UINT dst_stride, const RECT *dst_rect )
{
    UINT bpp = src_info->bmiHeader.biBitCount, height = dst_rect->bottom - dst_rect->top, src_stride, size;
    char *src = bits->ptr;

    src_stride = src_info->bmiHeader.biSizeImage / src_info->bmiHeader.biHeight;
    size = (src_rect->right * bpp + 7) / 8 - (src_rect->left * bpp) / 8;
    src += src_rect->top * src_stride + (src_rect->left * bpp) / 8;
    dst += dst_rect->top * dst_stride + (dst_rect->left * bpp) / 8;

    while (height--)
    {
        memcpy( dst, src, size );
        src += src_stride;
        dst += dst_stride;
    }
}

static void context_set_source( struct surface *context, BITMAPINFO *info, const struct gdi_image_bits *bits,
                                struct bitblt_coords *coords )
{
    UINT bpp, data_size, dst_stride;
    unsigned char *pixels;
    RECT dst_rect;

    assert(!IsRectEmpty( &coords->visrect ));

    bpp = info->bmiHeader.biBitCount;
    get_buffer_stride( info, &coords->visrect, &dst_stride, &dst_rect );
    data_size = dst_stride * dst_rect.bottom;

    if (!(pixels = malloc( data_size ))) return;
    copy_image_bits( bits, info, &coords->visrect, pixels, dst_stride, &dst_rect );

    if (context->source) cairo_surface_destroy( context->source );
    context->source = cairo_image_surface_create_for_data( pixels, bpp_to_cairo_format( bpp ), dst_rect.right - dst_rect.left,
                                                           dst_rect.bottom - dst_rect.top, dst_stride );
}

static void cairodrv_get_image( struct device *device, struct surface *surface, PHYSDEV dev, struct bitblt_coords *coords )
{
    char info_buffer[FIELD_OFFSET( BITMAPINFO, bmiColors[256] )];
    BITMAPINFO *info = (BITMAPINFO *)info_buffer;
    DC *dc = get_physdev_dc( dev );
    struct gdi_image_bits bits;
    UINT err;

    dev = GET_DC_PHYSDEV( dc, pGetImage );
    if ((err = dev->funcs->pGetImage( dev, info, &bits, coords ))) ERR( "failed to get image data, error %u\n", err );
    else context_set_source( surface, info, &bits, coords );

    if (bits.free) bits.free( &bits );
}

static void context_put_image( struct surface *context, const RECT *rect )
{
    cairo_set_source_surface( context->cairo, context->source, rect->left, rect->top );
    cairo_rectangle( context->cairo, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top );
    cairo_fill( context->cairo );
}

static BOOL cairodrv_AlphaBlend( PHYSDEV dst_dev, struct bitblt_coords *dst,
                               PHYSDEV src_dev, struct bitblt_coords *src, BLENDFUNCTION func )
{
    struct device *device = device_from_physdev( dst_dev );
    struct surface *surface = device_lock_surface( device );
    RECT dst_rect;

    if (*(UINT32 *)&func) /* FIXME */
        TRACE( "dst_dev %s, dst %s, src_dev %s, src %s, func %#x\n", dst_dev->funcs->name,
             dbgstr_bitblt_coords(dst), src_dev->funcs->name, dbgstr_bitblt_coords(src), *(UINT32 *)&func );
    else
        TRACE( "dst_dev %s, dst %s, src_dev %s, src %s, func %#x\n", dst_dev->funcs->name,
             dbgstr_bitblt_coords(dst), src_dev->funcs->name, dbgstr_bitblt_coords(src), *(UINT32 *)&func );

    cairodrv_get_image( device, surface, src_dev, src );

    dst_rect = dst->visrect;
    cairo_set_operator( surface->cairo, CAIRO_OPERATOR_SOURCE );
    context_put_image( surface, &dst_rect );

    if (0) debug_cairo_rect( surface->cairo, &dst->visrect, "ablend" );
    device_unlock_surface( device, surface );
    return TRUE;
}

static BOOL cairodrv_AngleArc( PHYSDEV dev, INT x, INT y, DWORD radius, FLOAT start, FLOAT sweep )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return TRUE;
}

static BOOL cairodrv_Arc( PHYSDEV dev, INT left, INT top, INT right, INT bottom,
                        INT xstart, INT ystart, INT xend, INT yend )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return TRUE;
}

static BOOL cairodrv_ArcTo( PHYSDEV dev, INT left, INT top, INT right, INT bottom, INT xstart, INT ystart, INT xend, INT yend )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return TRUE;
}

static DWORD cairodrv_BlendImage( PHYSDEV dev, BITMAPINFO *info, const struct gdi_image_bits *bits,
                                struct bitblt_coords *src, struct bitblt_coords *dst, BLENDFUNCTION func )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return 0;
}

static BOOL cairodrv_Chord( PHYSDEV dev, INT left, INT top, INT right, INT bottom,
                          INT xstart, INT ystart, INT xend, INT yend )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return TRUE;
}

static BOOL cairodrv_CreateCompatibleDC( PHYSDEV orig, PHYSDEV *dev )
{
    struct device *device;
    PHYSDEV next = *dev;

    TRACE( "orig %p, dev %p\n", orig, dev );
    TRACE( "  next %p hdc %p dev %s prio %d next %p\n", next, next->hdc, next->funcs->name, next->funcs->priority, next->next );
    next = next->next;
    TRACE( "  next %p hdc %p dev %s prio %d next %p\n", next, next->hdc, next->funcs->name, next->funcs->priority, next->next );

    if (!(device = calloc(1, sizeof(*device)))) return FALSE;
    push_dc_driver( dev, &device->dev, &cairodrv_gdi_driver );
    return TRUE;
}

static BOOL cairodrv_CreateDC( PHYSDEV *dev, const WCHAR *device_name, const WCHAR *output_name,
                             const DEVMODEW *devmode )
{
    struct device *device;
    PHYSDEV next = *dev;

    TRACE( "dev %p, name %s, output %s, devmode %p\n", dev, debugstr_w(device_name), debugstr_w(output_name), devmode );
    TRACE( "  next %p hdc %p dev %s prio %d next %p\n", next, next->hdc, next->funcs->name, next->funcs->priority, next->next );
    next = next->next;
    TRACE( "  next %p hdc %p dev %s prio %d next %p\n", next, next->hdc, next->funcs->name, next->funcs->priority, next->next );

    if (!(device = calloc(1, sizeof(*device)))) return FALSE;
    push_dc_driver( dev, &device->dev, &cairodrv_gdi_driver );
    return TRUE;
}

static BOOL cairodrv_DeleteDC( PHYSDEV dev )
{
    struct device *device = device_from_physdev( dev );
    TRACE( "device %p\n", device );
    if (device->surface) surface_release( device->surface );
    free( device );
    return TRUE;
}

static BOOL cairodrv_DeleteObject( PHYSDEV dev, HGDIOBJ obj )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return TRUE;
}

static BOOL cairodrv_Ellipse( PHYSDEV dev, INT left, INT top, INT right, INT bottom )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return TRUE;
}

static INT cairodrv_ExtEscape( PHYSDEV dev, INT escape, INT in_size, const void *in_data,
                             INT out_size, void *out_data )
{
    struct device *device = device_from_physdev( dev );
    TRACE( "device %p, escape %u, in_size %#x, in_data %p, out_size %#x, out_data %p\n",
           device, escape, in_size, in_data, out_size, out_data );
    return TRUE;
}

static BOOL cairodrv_ExtFloodFill( PHYSDEV dev, INT x, INT y, COLORREF color, UINT type )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return TRUE;
}

static BOOL cairodrv_ExtTextOut( PHYSDEV dev, INT x, INT y, UINT flags, const RECT *rect,
                               LPCWSTR str, UINT count, const INT *dx )
{
    struct device *device = device_from_physdev( dev );
    HWND hwnd = NtUserWindowFromDC(device->dev.hdc), parent = NtUserGetAncestor(hwnd, GA_ROOT);

    TRACE( "dev %s, x %d, y %d, flags %#x, rect %s, str %s, dx %p\n", dev->funcs->name,
           x, y, flags, wine_dbgstr_rect(rect), debugstr_wn(str, count), dx );
    TRACE( "device %p, hdc %p, hwnd %p, parent %p\n", device, device->dev.hdc, hwnd, parent );

    dev = GET_NEXT_PHYSDEV( dev, pExtTextOut );
    return dev->funcs->pExtTextOut( dev, x, y, flags, rect, str, count, dx );
}

static BOOL cairodrv_FillPath( PHYSDEV dev )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return TRUE;
}

static BOOL cairodrv_FillRgn( PHYSDEV dev, HRGN rgn, HBRUSH brush )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return TRUE;
}

static BOOL cairodrv_FrameRgn( PHYSDEV dev, HRGN rgn, HBRUSH brush, INT width, INT height )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return TRUE;
}

static UINT cairodrv_GetBoundsRect( PHYSDEV dev, RECT *rect, UINT flags )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return DCB_RESET;
}

static INT cairodrv_GetDeviceCaps( PHYSDEV dev, INT cap )
{
    struct device *device = device_from_physdev( dev );
    TRACE( "device %p\n", device );
    dev = GET_NEXT_PHYSDEV( dev, pGetDeviceCaps );
    return dev->funcs->pGetDeviceCaps( dev, cap );
}

static BOOL cairodrv_GetDeviceGammaRamp( PHYSDEV dev, void *ramp )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return FALSE;
}

static BOOL cairodrv_GetICMProfile( PHYSDEV dev, BOOL allow_default, LPDWORD size, LPWSTR filename )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return FALSE;
}

static DWORD cairodrv_GetImage( PHYSDEV dev, BITMAPINFO *info, struct gdi_image_bits *bits,
                               struct bitblt_coords *src )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return ERROR_NOT_SUPPORTED;
}

static COLORREF cairodrv_GetNearestColor( PHYSDEV dev, COLORREF color )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return TRUE;
}

static COLORREF cairodrv_GetPixel( PHYSDEV dev, INT x, INT y )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return TRUE;
}

static UINT cairodrv_GetSystemPaletteEntries( PHYSDEV dev, UINT start, UINT count, PALETTEENTRY *entries )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return 0;
}

static BOOL cairodrv_GradientFill( PHYSDEV dev, TRIVERTEX *vert_array, ULONG nvert,
                                  void * grad_array, ULONG ngrad, ULONG mode )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return TRUE;
}

static BOOL cairodrv_InvertRgn( PHYSDEV dev, HRGN rgn )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return TRUE;
}

static BOOL cairodrv_LineTo( PHYSDEV dev, INT x, INT y )
{
    struct device *device = device_from_physdev( dev );
    struct surface *surface = device_lock_surface( device );
    DC *dc = get_physdev_dc( dev );
    POINT point = {x, y};

    ERR( "dev %s, x %d, y %d\n", dev->funcs->name, x, y );

    lp_to_dp( dc, &point, 1 );
    cairo_line_to( surface->cairo, point.x, point.y );
    cairo_set_source_rgb( surface->cairo, 1, 0, 0 );
    cairo_stroke( surface->cairo );

    device_unlock_surface( device, surface );
    return TRUE;
}

static BOOL cairodrv_MoveTo( PHYSDEV dev, INT x, INT y )
{
    struct device *device = device_from_physdev( dev );
    struct surface *surface = device_lock_surface( device );
    DC *dc = get_physdev_dc( dev );
    POINT point = {x, y};

    ERR( "dev %s, x %d, y %d\n", dev->funcs->name, x, y );

    lp_to_dp( dc, &point, 1 );
    cairo_new_path( surface->cairo );
    cairo_move_to( surface->cairo, point.x, point.y );

    device_unlock_surface( device, surface );
    return TRUE;
}

static BOOL cairodrv_PaintRgn( PHYSDEV dev, HRGN rgn )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return TRUE;
}

static BOOL cairodrv_PatBlt( PHYSDEV dev, struct bitblt_coords *dst, DWORD rop )
{
    struct device *device = device_from_physdev( dev );
    struct surface *surface = device_lock_surface( device );
    BOOL ret = TRUE;
    RECT dst_rect;

    ERR( "dev %s, dst %s, rop %#x\n", dev->funcs->name, dbgstr_bitblt_coords(dst), (UINT)rop );

    dst_rect = dst->visrect;

    switch (rop)
    {
    case R2_NOP:
        FIXME("R2_NOP\n");
        break;
    case R2_NOT:
        FIXME("R2_NOT\n");
        cairo_set_operator( surface->cairo, CAIRO_OPERATOR_XOR );
        cairo_set_source_rgb( surface->cairo, 1, 1, 1 );
        break;
    case R2_WHITE:
        FIXME("R2_WHITE\n");
        cairo_set_operator( surface->cairo, CAIRO_OPERATOR_SOURCE );
        cairo_set_source_rgb( surface->cairo, 1, 1, 1 );
        break;
    case R2_BLACK:
        FIXME("R2_BLACK\n");
        cairo_set_operator( surface->cairo, CAIRO_OPERATOR_SOURCE );
        cairo_set_source_rgb( surface->cairo, 0, 0, 0 );
        break;
    case PATCOPY:
    case PATINVERT:
    case DSTINVERT:
    case BLACKNESS:
    case WHITENESS:
        switch (device->brush.lbStyle)
        {
        case BS_NULL:
            FIXME("BS_NULL\n");
            break;
        case BS_HATCHED:
            FIXME("BS_HATCHED\n");
            break;
        case BS_SOLID:
            cairo_set_operator( surface->cairo, CAIRO_OPERATOR_SOURCE );
            cairo_set_source_rgb( surface->cairo, ((device->brush.lbColor >> 16) & 0xff) / 255.0,
                                  ((device->brush.lbColor >> 8) & 0xff) / 255.0,
                                  ((device->brush.lbColor >> 0) & 0xff) / 255.0 );
            break;
        case BS_PATTERN:
        {
            cairo_surface_t *brush_surface;
            TRACE("BS_PATTERN %d\n", (UINT)device->brush_pattern.info->bmiHeader.biHeight);
            cairo_set_operator( surface->cairo, CAIRO_OPERATOR_SOURCE );
            brush_surface = surface_from_pattern( device->brush_pattern.info, device->brush_pattern.bits );
            cairo_set_source_surface( surface->cairo, brush_surface, dst_rect.left, dst_rect.top );
            cairo_surface_destroy( brush_surface );
            break;
        }
        case BS_DIBPATTERN:
            FIXME("BS_DIBPATTERN\n");
            break;
        default:
            FIXME("%#x\n", (UINT)device->brush.lbStyle);
            break;
        }
        break;
    }

    cairo_rectangle( surface->cairo, dst_rect.left, dst_rect.top,
                     dst_rect.right - dst_rect.left, dst_rect.bottom - dst_rect.top );
    cairo_fill( surface->cairo );

    if (1) debug_cairo_rect( surface->cairo, &dst_rect, "pat" );
    device_unlock_surface( device, surface );
    return ret;
}

static BOOL cairodrv_Pie( PHYSDEV dev, INT left, INT top, INT right, INT bottom,
                         INT xstart, INT ystart, INT xend, INT yend )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return TRUE;
}

static BOOL cairodrv_PolyBezier( PHYSDEV dev, const POINT *points, DWORD count )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return TRUE;
}

static BOOL cairodrv_PolyBezierTo( PHYSDEV dev, const POINT *points, DWORD count )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return TRUE;
}

static BOOL cairodrv_PolyDraw( PHYSDEV dev, const POINT *points, const BYTE *types, DWORD count )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return TRUE;
}

static BOOL cairodrv_PolyPolygon( PHYSDEV dev, const POINT *points, const INT *counts, UINT polygons )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return TRUE;
}

static BOOL cairodrv_PolyPolyline( PHYSDEV dev, const POINT *points, const DWORD *counts, DWORD lines )
{
    struct device *device = device_from_physdev( dev );
    struct surface *surface = device_lock_surface( device );
    DC *dc = get_physdev_dc( dev );
    DWORD total = 0, max = 0, pos, i, j;
    POINT *device_points;

    TRACE( "dev %s, points %p, counts %p, lines %#x\n", dev->funcs->name, points, counts, (UINT)lines );

    for (i = 0; i < lines; i++)
    {
        if (counts[i] < 2) goto failed;
        if (counts[i] > max) max = counts[i];
        total += counts[i];
    }

    if (!(device_points = malloc( total * sizeof(*points) ))) goto failed;
    memcpy( device_points, points, total * sizeof(*points) );
    lp_to_dp( dc, device_points, total );

    cairo_new_path( surface->cairo );
    for (i = pos = 0; i < lines; pos += counts[i++])
    {
        cairo_move_to( surface->cairo, device_points[pos].x, device_points[pos].y );
        for (j = 1; j < counts[i]; j++)
            cairo_line_to( surface->cairo, device_points[pos + j].x, device_points[pos + j].y );
    }

    cairo_set_source_rgb( surface->cairo, 1, 0, 0 );
    cairo_stroke( surface->cairo );

    free( device_points );
    device_unlock_surface( device, surface );
    return TRUE;

failed:
    device_unlock_surface( device, surface );
    return FALSE;
}

static BOOL cairodrv_PolylineTo( PHYSDEV dev, const POINT *points, INT count )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return TRUE;
}

static DWORD cairodrv_PutImage( PHYSDEV dev, HRGN clip, BITMAPINFO *info, const struct gdi_image_bits *bits,
                              struct bitblt_coords *src, struct bitblt_coords *dst, DWORD rop )
{
    struct device *device = device_from_physdev( dev );
    struct surface *surface = device_lock_surface( device );
    HWND hwnd = NtUserWindowFromDC(device->dev.hdc), parent = NtUserGetAncestor(hwnd, GA_ROOT);
    RECT dst_rect;

if (rop != SRCCOPY && rop != SRCPAINT)
{
    ERR( "dev %s, clip %p, info %p, bits %p, src %s, dst %s, rop %#x\n", dev->funcs->name, clip, info, bits,
         dbgstr_bitblt_coords(src), dbgstr_bitblt_coords(dst), (UINT)rop );
    ERR( "device %p, hdc %p, hwnd %p, parent %p\n", device, device->dev.hdc, hwnd, parent );
}
else
{
    ERR( "dev %s, clip %p, info %p, bits %p, src %s, dst %s, rop %#x\n", dev->funcs->name, clip, info, bits,
         dbgstr_bitblt_coords(src), dbgstr_bitblt_coords(dst), (UINT)rop );
    ERR( "device %p, hdc %p, hwnd %p, parent %p\n", device, device->dev.hdc, hwnd, parent );
}

    context_set_source( surface, info, bits, src );

    dst_rect = dst->visrect;
    if (rop == SRCPAINT) cairo_set_operator( surface->cairo, CAIRO_OPERATOR_ADD );
    else cairo_set_operator( surface->cairo, CAIRO_OPERATOR_SOURCE );
    context_put_image( surface, &dst_rect );

    if (0) debug_cairo_rect( surface->cairo, &dst->visrect, "put" );
    device_unlock_surface( device, surface );
    return ERROR_SUCCESS;
}

static UINT cairodrv_RealizeDefaultPalette( PHYSDEV dev )
{
    struct device *device = device_from_physdev( dev );
    TRACE( "device %p\n", device );
    return 0;
}

static UINT cairodrv_RealizePalette( PHYSDEV dev, HPALETTE palette, BOOL primary )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return 0;
}

static BOOL cairodrv_Rectangle( PHYSDEV dev, INT left, INT top, INT right, INT bottom )
{
    struct device *device = device_from_physdev( dev );
    struct surface *surface = device_lock_surface( device );
    RECT rect = {left, top, right, bottom};
    DC *dc = get_physdev_dc( dev );

    ERR( "dev %s, left %d, top %d, right %d, bottom %d\n", dev->funcs->name, left, top, right, bottom );

    lp_to_dp( dc, (POINT *)&rect, 2 );
    cairo_rectangle( surface->cairo, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top );
    cairo_set_source_rgb( surface->cairo, 1, 0, 0 );
    cairo_fill( surface->cairo );

    device_unlock_surface( device, surface );
    return TRUE;

}

static BOOL cairodrv_RoundRect( PHYSDEV dev, INT left, INT top, INT right, INT bottom,
                               INT ell_width, INT ell_height )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return TRUE;
}

static HBITMAP cairodrv_SelectBitmap( PHYSDEV dev, HBITMAP bitmap )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p, bitmap %p\n", device, bitmap );
    dev = GET_NEXT_PHYSDEV( dev, pSelectBitmap );
    return dev->funcs->pSelectBitmap( dev, bitmap );
}

static HBRUSH cairodrv_SelectBrush( PHYSDEV dev, HBRUSH brush, const struct brush_pattern *pattern )
{
    struct device *device = device_from_physdev( dev );
    DC *dc = get_physdev_dc( dev );

    TRACE( "device %p, brush %p, pattern %p\n", dev->funcs->name, brush, pattern );

    NtGdiExtGetObjectW( brush, sizeof(device->brush), &device->brush );
    if (brush == GetStockObject( DC_BRUSH )) device->brush.lbColor = dc->attr->brush_color;
    if (pattern) device->brush_pattern = *pattern;

    dev = GET_NEXT_PHYSDEV( dev, pSelectBrush );
    return dev->funcs->pSelectBrush( dev, brush, pattern );
}

static HFONT cairodrv_SelectFont( PHYSDEV dev, HFONT font, UINT *aa_flags )
{
    struct device *device = device_from_physdev( dev );
    TRACE( "device %p\n", device );
    dev = GET_NEXT_PHYSDEV( dev, pSelectFont );
    return dev->funcs->pSelectFont( dev, font, aa_flags );
}

static HPEN cairodrv_SelectPen( PHYSDEV dev, HPEN pen, const struct brush_pattern *pattern )
{
    struct device *device = device_from_physdev( dev );
    DC *dc = get_physdev_dc( dev );
    EXTLOGPEN *elp;
    INT size;

    TRACE( "dev %s, pen %p, pattern %p\n", dev->funcs->name, pen, pattern );

    if (!NtGdiExtGetObjectW( pen, sizeof(device->pen), &device->pen ))
    {
        /* must be an extended pen */
        if (!(size = NtGdiExtGetObjectW( pen, 0, NULL ))) return 0;
        elp = malloc( size );

        NtGdiExtGetObjectW( pen, size, elp );
        device->pen.lopnStyle = elp->elpPenStyle;
        device->pen.lopnWidth.x = elp->elpWidth;
        /* cosmetic ext pens are always 1-pixel wide */
        if (!(device->pen.lopnStyle & PS_GEOMETRIC)) device->pen.lopnWidth.x = 0;

        device->pen_brush.lbStyle = elp->elpBrushStyle;
        device->pen_brush.lbColor = elp->elpColor;
        device->pen_brush.lbHatch = elp->elpHatch;
    }
    else
    {
        device->pen_brush.lbStyle = BS_SOLID;
        device->pen_brush.lbColor = device->pen.lopnColor;
        device->pen_brush.lbHatch = 0;
    }

    if (pen == GetStockObject( DC_PEN )) device->pen_brush.lbColor = dc->attr->pen_color;
    if (pattern) device->pen_brush_pattern = *pattern;

    dev = GET_NEXT_PHYSDEV( dev, pSelectPen );
    return dev->funcs->pSelectPen( dev, pen, pattern );
}

static UINT cairodrv_SetBoundsRect( PHYSDEV dev, RECT *rect, UINT flags )
{
    struct device *device = device_from_physdev( dev );

    if (flags & DCB_ENABLE) ERR( "device %p, rect %s, flags %#x\n", device, wine_dbgstr_rect(rect), flags );
    else TRACE( "device %p, rect %s, flags %#x\n", device, wine_dbgstr_rect(rect), flags );

/*
    if (flags & DCB_DISABLE) device->bounds = NULL;
    else if (flags & DCB_ENABLE) device->bounds = rect;
*/
    return DCB_RESET;  /* we don't have device-specific bounds */
}

static COLORREF cairodrv_SetDCBrushColor( PHYSDEV dev, COLORREF color )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return color;
}

static COLORREF cairodrv_SetDCPenColor( PHYSDEV dev, COLORREF color )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return color;
}

static INT cairodrv_SetDIBitsToDevice( PHYSDEV dev, INT x_dst, INT y_dst, DWORD width, DWORD height,
                                      INT x_src, INT y_src, UINT start, UINT lines,
                                      const void *bits, BITMAPINFO *info, UINT coloruse )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return TRUE;
}

static void cairodrv_SetDeviceClipping( PHYSDEV dev, HRGN clip )
{
    struct device *device = device_from_physdev( dev );
    struct surface *surface = device_lock_surface( device );
    const WINEREGION *region;
    UINT i;

    TRACE( "clip %p\n", clip );

    if (!surface) return;

    if ((region = get_wine_region( clip )))
    {
        cairo_reset_clip( surface->cairo );
        for (i = 0; i < region->numRects; i++)
        {
            TRACE("  %s\n", wine_dbgstr_rect(region->rects + i));
            cairo_rectangle( surface->cairo, region->rects[i].left, region->rects[i].top,
                             region->rects[i].right - region->rects[i].left,
                             region->rects[i].bottom - region->rects[i].top );
        }
        cairo_clip( surface->cairo );

        for (i = 0; i < region->numRects; i++)
            if (0) debug_cairo_rect( surface->cairo, &region->rects[i], "clip" );
        release_wine_region( clip );
    }

    device_unlock_surface( device, surface );
}

static BOOL cairodrv_SetDeviceGammaRamp( PHYSDEV dev, void *ramp )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return FALSE;
}

static COLORREF cairodrv_SetPixel( PHYSDEV dev, INT x, INT y, COLORREF color )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return color;
}

static BOOL cairodrv_StretchBlt( PHYSDEV dst_dev, struct bitblt_coords *dst,
                               PHYSDEV src_dev, struct bitblt_coords *src, DWORD rop )
{
    struct device *device = device_from_physdev( dst_dev );
    struct surface *surface = device_lock_surface( device );
    RECT dst_rect;

    if (rop != SRCCOPY && rop != SRCPAINT && rop != SRCAND)
        ERR( "dst_dev %s, dst %s, src_dev %s, src %s, rop %#x\n", dst_dev->funcs->name,
             dbgstr_bitblt_coords(dst), src_dev->funcs->name, dbgstr_bitblt_coords(src), (UINT)rop );
    else
        TRACE( "dst_dev %s, dst %s, src_dev %s, src %s, rop %#x\n", dst_dev->funcs->name,
             dbgstr_bitblt_coords(dst), src_dev->funcs->name, dbgstr_bitblt_coords(src), (UINT)rop );

    goto done;
    cairodrv_get_image( device, surface, src_dev, src );

    dst_rect = dst->visrect;
    if (rop == SRCPAINT) cairo_set_operator( surface->cairo, CAIRO_OPERATOR_ADD );
    else if (rop == SRCAND) cairo_set_operator( surface->cairo, CAIRO_OPERATOR_ATOP );
    else cairo_set_operator( surface->cairo, CAIRO_OPERATOR_SOURCE );
    context_put_image( surface, &dst_rect );

done:
    if (0) debug_cairo_rect( surface->cairo, &dst->visrect, "stretch" );
    device_unlock_surface( device, surface );
    return TRUE;
}

static INT  cairodrv_StretchDIBits( PHYSDEV dev, INT dst_x, INT dst_y, INT dst_width, INT dst_height,
                                  INT src_x, INT src_y, INT src_width, INT src_height, const void *bits,
                                  BITMAPINFO *info, UINT coloruse, DWORD rop )
{
    struct device *device = device_from_physdev( dev );
    TRACE( "device %p, dst (%d,%d)-(%d,%d), src (%d,%d)-(%d,%d), bits %p, info %p, coloruse %u, rop %#x\n",
           device, dst_x, dst_y, dst_width, dst_height, src_x, src_y, src_width, src_height, bits, info, coloruse, (UINT)rop );
    dev = GET_NEXT_PHYSDEV( dev, pStretchDIBits );
    return dev->funcs->pStretchDIBits( dev, dst_x, dst_y, dst_width, dst_height, src_x, src_y, src_width, src_height, bits, info, coloruse, rop );
}

static BOOL cairodrv_StrokeAndFillPath( PHYSDEV dev )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return TRUE;
}

static BOOL cairodrv_StrokePath( PHYSDEV dev )
{
    struct device *device = device_from_physdev( dev );
    ERR( "device %p\n", device );
    return TRUE;
}

static BOOL cairodrv_UnrealizePalette( HPALETTE palette )
{
    ERR( "palette %p\n", palette );
    return FALSE;
}

static void cairodrv_SetWindowRegion( PHYSDEV dev, HWND hwnd, HWND toplevel, const RECT *window_rect,
                                      const RECT *toplevel_rect )
{
    struct device *device = device_from_physdev( dev );
    struct cairodrv_window *window;

    TRACE( "dev %p, hwnd %p, toplevel %p, window_rect %s, toplevel_rect %s\n", dev, hwnd, toplevel,
           wine_dbgstr_rect(window_rect), wine_dbgstr_rect(toplevel_rect) );

    if ((window = cairodrv_window_lock( toplevel )))
    {
        struct surface *surface = window->surface;
        surface_acquire( window->surface );
        cairodrv_window_unlock( window );

        if (device->surface) surface_release( device->surface );
        device->surface = surface;
        device->rect = *window_rect;
        OffsetRect( &device->rect, -toplevel_rect->left, -toplevel_rect->top );
    }
}

static const struct gdi_dc_funcs cairodrv_gdi_driver =
{
    /*.pAbortDoc = cairodrv_AbortDoc,*/
    /*.pAbortPath = cairodrv_AbortPath,*/
    .pAlphaBlend = cairodrv_AlphaBlend,
    .pAngleArc = cairodrv_AngleArc,
    .pArc = cairodrv_Arc,
    .pArcTo = cairodrv_ArcTo,
    /*.pBeginPath = cairodrv_BeginPath,*/
    .pBlendImage = cairodrv_BlendImage,
    .pChord = cairodrv_Chord,
    /*.pCloseFigure = cairodrv_CloseFigure,*/
    .pCreateCompatibleDC = cairodrv_CreateCompatibleDC,
    .pCreateDC = cairodrv_CreateDC,
    .pDeleteDC = cairodrv_DeleteDC,
    .pDeleteObject = cairodrv_DeleteObject,
    .pEllipse = cairodrv_Ellipse,
    /*.pEndDoc = cairodrv_EndDoc,*/
    /*.pEndPage = cairodrv_EndPage,*/
    /*.pEndPath = cairodrv_EndPath,*/
    /*.pEnumFonts = cairodrv_EnumFonts,*/
    .pExtEscape = cairodrv_ExtEscape,
    .pExtFloodFill = cairodrv_ExtFloodFill,
    .pExtTextOut = cairodrv_ExtTextOut,
    .pFillPath = cairodrv_FillPath,
    .pFillRgn = cairodrv_FillRgn,
    /*.pFontIsLinked = cairodrv_FontIsLinked,*/
    .pFrameRgn = cairodrv_FrameRgn,
    .pGetBoundsRect = cairodrv_GetBoundsRect,
    /*.pGetCharABCWidths = cairodrv_GetCharABCWidths,*/
    /*.pGetCharABCWidthsI = cairodrv_GetCharABCWidthsI,*/
    /*.pGetCharWidth = cairodrv_GetCharWidth,*/
    /*.pGetCharWidthInfo = cairodrv_GetCharWidthInfo,*/
    .pGetDeviceCaps = cairodrv_GetDeviceCaps,
    .pGetDeviceGammaRamp = cairodrv_GetDeviceGammaRamp,
    /*.pGetFontData = cairodrv_GetFontData,*/
    /*.pGetFontRealizationInfo = cairodrv_GetFontRealizationInfo,*/
    /*.pGetFontUnicodeRanges = cairodrv_GetFontUnicodeRanges,*/
    /*.pGetGlyphIndices = cairodrv_GetGlyphIndices,*/
    /*.pGetGlyphOutline = cairodrv_GetGlyphOutline,*/
    .pGetICMProfile = cairodrv_GetICMProfile,
    .pGetImage = cairodrv_GetImage,
    /*.pGetKerningPairs = cairodrv_GetKerningPairs,*/
    .pGetNearestColor = cairodrv_GetNearestColor,
    /*.pGetOutlineTextMetrics = cairodrv_GetOutlineTextMetrics,*/
    .pGetPixel = cairodrv_GetPixel,
    .pGetSystemPaletteEntries = cairodrv_GetSystemPaletteEntries,
    /*.pGetTextCharsetInfo = cairodrv_GetTextCharsetInfo,*/
    /*.pGetTextExtentExPoint = cairodrv_GetTextExtentExPoint,*/
    /*.pGetTextExtentExPointI = cairodrv_GetTextExtentExPointI,*/
    /*.pGetTextFace = cairodrv_GetTextFace,*/
    /*.pGetTextMetrics = cairodrv_GetTextMetrics,*/
    .pGradientFill = cairodrv_GradientFill,
    .pInvertRgn = cairodrv_InvertRgn,
    .pLineTo = cairodrv_LineTo,
    .pMoveTo = cairodrv_MoveTo,
    .pPaintRgn = cairodrv_PaintRgn,
    .pPatBlt = cairodrv_PatBlt,
    .pPie = cairodrv_Pie,
    .pPolyBezier = cairodrv_PolyBezier,
    .pPolyBezierTo = cairodrv_PolyBezierTo,
    .pPolyDraw = cairodrv_PolyDraw,
    .pPolyPolygon = cairodrv_PolyPolygon,
    .pPolyPolyline = cairodrv_PolyPolyline,
    .pPolylineTo = cairodrv_PolylineTo,
    .pPutImage = cairodrv_PutImage,
    .pRealizeDefaultPalette = cairodrv_RealizeDefaultPalette,
    .pRealizePalette = cairodrv_RealizePalette,
    .pRectangle = cairodrv_Rectangle,
    /*.pResetDC = cairodrv_ResetDC,*/
    .pRoundRect = cairodrv_RoundRect,
    .pSelectBitmap = cairodrv_SelectBitmap,
    .pSelectBrush = cairodrv_SelectBrush,
    .pSelectFont = cairodrv_SelectFont,
    .pSelectPen = cairodrv_SelectPen,
    /*.pSetBkColor = cairodrv_SetBkColor,*/
    .pSetBoundsRect = cairodrv_SetBoundsRect,
    .pSetDCBrushColor = cairodrv_SetDCBrushColor,
    .pSetDCPenColor = cairodrv_SetDCPenColor,
    .pSetDIBitsToDevice = cairodrv_SetDIBitsToDevice,
    .pSetDeviceClipping = cairodrv_SetDeviceClipping,
    .pSetDeviceGammaRamp = cairodrv_SetDeviceGammaRamp,
    .pSetPixel = cairodrv_SetPixel,
    /*.pSetTextColor = cairodrv_SetTextColor,*/
    /*.pStartDoc = cairodrv_StartDoc,*/
    /*.pStartPage = cairodrv_StartPage,*/
    .pStretchBlt = cairodrv_StretchBlt,
    .pStretchDIBits = cairodrv_StretchDIBits,
    .pStrokeAndFillPath = cairodrv_StrokeAndFillPath,
    .pStrokePath = cairodrv_StrokePath,
    .pUnrealizePalette = cairodrv_UnrealizePalette,
    .pSetWindowRegion = cairodrv_SetWindowRegion,

    .priority = GDI_PRIORITY_GRAPHICS_DRV,
    .name = "cairodrv",
};

static void cairodrv_funcs_window_create( HWND hwnd, void *surface )
{
    struct cairodrv_window *window;

    TRACE( "hwnd %p, surface %p\n", hwnd, surface );

    if ((window = cairodrv_window_lock( hwnd )))
    {
        struct surface *old = window->surface;
        window->surface = surface_create( surface );
        cairodrv_window_unlock( window );
        if (old) surface_release( old );
    }
    else if ((window = calloc( 1, sizeof(*window) )))
    {
        window->surface = surface_create( surface );
        cairodrv_window_init( hwnd, window );
    }
}

static void cairodrv_funcs_window_destroy( HWND hwnd )
{
    struct cairodrv_window *window;

    TRACE( "hwnd %p\n", hwnd );

    if ((window = cairodrv_window_lock( hwnd )))
    {
        window->locked_win->cairodrv_window = NULL;
        cairodrv_window_unlock( window );
        cairodrv_window_destroy( window );
    }
}

static void cairodrv_surface_lock( struct window_surface *window_surface )
{
    TRACE( "window_surface %p\n", window_surface );
}

static void cairodrv_surface_unlock( struct window_surface *window_surface )
{
    TRACE( "window_surface %p\n", window_surface );
}

static void *cairodrv_surface_get_bitmap_info( struct window_surface *window_surface, BITMAPINFO *info )
{
    static DWORD cairodrv_data;
    ERR( "window_surface %p\n", window_surface );
    info->bmiHeader.biSize          = sizeof( info->bmiHeader );
    info->bmiHeader.biWidth         = window_surface->rect.right;
    info->bmiHeader.biHeight        = window_surface->rect.bottom;
    info->bmiHeader.biPlanes        = 1;
    info->bmiHeader.biBitCount      = 32;
    info->bmiHeader.biCompression   = BI_RGB;
    info->bmiHeader.biSizeImage     = 0;
    info->bmiHeader.biXPelsPerMeter = 0;
    info->bmiHeader.biYPelsPerMeter = 0;
    info->bmiHeader.biClrUsed       = 0;
    info->bmiHeader.biClrImportant  = 0;
    return &cairodrv_data;
}

static RECT *cairodrv_surface_get_bounds( struct window_surface *window_surface )
{
    static RECT cairodrv_bounds;
    ERR( "window_surface %p\n", window_surface );
reset_bounds(&cairodrv_bounds);
    return &cairodrv_bounds;
}

static void cairodrv_surface_set_region( struct window_surface *window_surface, HRGN region )
{
    ERR( "window_surface %p\n", window_surface );
}

static void cairodrv_surface_flush( struct window_surface *window_surface )
{
    TRACE( "window_surface %p\n", window_surface );
}

static void cairodrv_surface_destroy( struct window_surface *window_surface )
{
    ERR( "window_surface %p\n", window_surface );
    free( window_surface );
}

static const struct window_surface_funcs cairodrv_surface_funcs =
{
    cairodrv_surface_lock,
    cairodrv_surface_unlock,
    cairodrv_surface_get_bitmap_info,
    cairodrv_surface_get_bounds,
    cairodrv_surface_set_region,
    cairodrv_surface_flush,
    cairodrv_surface_destroy
};

static struct window_surface *cairodrv_funcs_surface_create( HWND hwnd, const RECT *rect, COLORREF key, BOOL alpha )
{
    struct window_surface *window_surface;
    ERR("hwnd %p, rect %s, key %#x, alpha %u\n", hwnd, wine_dbgstr_rect(rect), (UINT)key, alpha);
    if (!(window_surface = calloc(1, sizeof(*window_surface)))) return NULL;
    window_surface->funcs = &cairodrv_surface_funcs;
    window_surface->rect = *rect;
    window_surface->ref = 1;
    return window_surface;
}

static const struct cairodrv_funcs cairodrv_funcs =
{
    &cairodrv_gdi_driver,
    cairodrv_funcs_window_create,
    cairodrv_funcs_window_destroy,
    cairodrv_funcs_surface_create,
};

const struct cairodrv_funcs *__wine_get_cairo_driver( UINT version )
{
    TRACE( "version %u\n", version );
    if (version != WINE_GDI_DRIVER_VERSION) return NULL;
    return &cairodrv_funcs;
}
