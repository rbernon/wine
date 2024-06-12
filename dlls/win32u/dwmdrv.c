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
#include <assert.h>

#include <unistd.h>
#include <fcntl.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"

#include "ntgdi_private.h"
#include "ntuser_private.h"

#include "wine/dwmapi.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dwm);

static HANDLE req_handle, reply_handle;

static const struct gdi_dc_funcs dwm_gdi_driver;

struct dwm_gdi_device
{
    struct gdi_physdev dev;
    dwm_context_t dwm_context;
};

static const char *dbgstr_bitblt_coords( const struct bitblt_coords *coords )
{
    RECT log = {coords->log_x, coords->log_y, coords->log_x+coords->log_width, coords->log_y+coords->log_height};
    RECT phy = {coords->x, coords->y, coords->x+coords->width, coords->y+coords->height};
    return wine_dbg_sprintf("(log %s phy %s vis %s)", wine_dbgstr_rect(&log), wine_dbgstr_rect(&phy), wine_dbgstr_rect(&coords->visrect));
    return wine_dbgstr_rect(&coords->visrect);
};

struct dwmdrv_window
{
    WND *locked_win;
    dwm_window_t dwm_window;
};

static void dwmdrv_window_init( HWND hwnd, struct dwmdrv_window *window )
{
    struct dwmdrv_window *previous = window;
    WND *win;

    if ((win = get_win_ptr( hwnd )) && win != WND_OTHER_PROCESS && win != WND_DESKTOP)
    {
        previous = win->dwmdrv_window;
        win->dwmdrv_window = window;
        release_win_ptr( win );
    }

    free( previous );
}

static struct dwmdrv_window *dwmdrv_window_lock( HWND hwnd )
{
    struct dwmdrv_window *window;
    WND *win;

    if (!(win = get_win_ptr( hwnd )) || win == WND_OTHER_PROCESS || win == WND_DESKTOP) return NULL;
    if (!(window = win->dwmdrv_window))
    {
        release_win_ptr( win );
        return NULL;
    }

    window->locked_win = win;
    return window;
}

static void dwmdrv_window_unlock( struct dwmdrv_window *window )
{
    WND *win = window->locked_win;
    window->locked_win = NULL;
    release_win_ptr( win );
}

static void dwmdrv_window_destroy( HWND hwnd )
{
    struct dwmdrv_window *window;

    if ((window = dwmdrv_window_lock( hwnd )))
    {
        window->locked_win->dwmdrv_window = NULL;
        dwmdrv_window_unlock( window );
        free( window );
    }
}

static dwm_context_t dwm_gdi_context_create(void)
{
    struct dwm_req_gdi_context_create req = {.header = {.type = DWM_REQ_GDI_CONTEXT_CREATE, .req_size = sizeof(req)}};
    union dwm_reply reply;
    IO_STATUS_BLOCK io;
    UINT status;

    if ((status = NtWriteFile( req_handle, NULL, NULL, NULL, &io, &req, sizeof(union dwm_request), NULL, NULL )) ||
        io.Information != sizeof(union dwm_request))
        ERR( "Failed to write request, status %#x\n", status );
    if ((status = NtReadFile( reply_handle, NULL, NULL, NULL, &io, &reply, sizeof(reply), NULL, NULL )) ||
        io.Information != sizeof(reply) || (status = reply.header.status))
        ERR( "Failed to read reply, status %#x\n", status );

    return reply.gdi_context_create.dwm_context;
}

static void dwm_gdi_context_destroy( dwm_context_t dwm_context )
{
    struct dwm_req_gdi_context_destroy req = {.header = {.type = DWM_REQ_GDI_CONTEXT_DESTROY, .req_size = sizeof(req)}};
    union dwm_reply reply;
    IO_STATUS_BLOCK io;
    UINT status;

    req.dwm_context = dwm_context;

    if ((status = NtWriteFile( req_handle, NULL, NULL, NULL, &io, &req, sizeof(union dwm_request), NULL, NULL )) ||
        io.Information != sizeof(union dwm_request))
        ERR( "Failed to write request, status %#x\n", status );
    if ((status = NtReadFile( reply_handle, NULL, NULL, NULL, &io, &reply, sizeof(reply), NULL, NULL )) ||
        io.Information != sizeof(reply) || (status = reply.header.status))
        ERR( "Failed to read reply, status %#x\n", status );
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
                             char *dst, UINT dst_stride, const RECT *dst_rect )
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

static void dwm_set_source( struct dwm_gdi_device *device, BITMAPINFO *info, const struct gdi_image_bits *bits,
                            struct bitblt_coords *coords )
{
    struct dwm_req_gdi_set_source req = {.header = {.type = DWM_REQ_GDI_SET_SOURCE, .req_size = sizeof(req)}};
    IO_STATUS_BLOCK io;
    char *pixels;
    UINT status;

    assert(!IsRectEmpty( &coords->visrect ));

    req.dwm_context = device->dwm_context;
    req.bpp = info->bmiHeader.biBitCount;
    get_buffer_stride( info, &coords->visrect, &req.stride, &req.rect );
    req.data_size = req.stride * req.rect.bottom;

    if (!(pixels = malloc( req.data_size ))) return;
    copy_image_bits( bits, info, &coords->visrect, pixels, req.stride, &req.rect );

    if ((status = NtWriteFile( req_handle, NULL, NULL, NULL, &io, &req, sizeof(union dwm_request), NULL, NULL )) ||
        io.Information != sizeof(union dwm_request))
        ERR( "Failed to write request, status %#x\n", status );
    if ((status = NtWriteFile( req_handle, NULL, NULL, NULL, &io, pixels, req.data_size, NULL, NULL )) ||
        io.Information != req.data_size)
        ERR( "Failed to write request, status %#x\n", status );

    free( pixels );
}


static void dwm_get_image( struct dwm_gdi_device *device, PHYSDEV dev, struct bitblt_coords *coords )
{
    char info_buffer[FIELD_OFFSET( BITMAPINFO, bmiColors[256] )];
    BITMAPINFO *info = (BITMAPINFO *)info_buffer;
    DC *dc = get_physdev_dc( dev );
    struct gdi_image_bits bits;
    UINT err;

    dev = GET_DC_PHYSDEV( dc, pGetImage );
    if ((err = dev->funcs->pGetImage( dev, info, &bits, coords ))) ERR( "failed to get image data, error %u\n", err );
    else dwm_set_source( device, info, &bits, coords );

    if (bits.free) bits.free( &bits );
}

static BOOL dwmdrv_AlphaBlend( PHYSDEV dst_dev, struct bitblt_coords *dst,
                               PHYSDEV src_dev, struct bitblt_coords *src, BLENDFUNCTION func )
{
    struct dwm_req_gdi_put_image req = {.header = {.type = DWM_REQ_GDI_PUT_IMAGE, .req_size = sizeof(req)}};
    struct dwm_gdi_device *device = CONTAINING_RECORD( dst_dev, struct dwm_gdi_device, dev );
    HWND hwnd = NtUserWindowFromDC(device->dev.hdc), parent = NtUserGetAncestor(hwnd, GA_ROOT);
    struct dwmdrv_window *window;
    IO_STATUS_BLOCK io;
    UINT status;

    TRACE( "dst_dev %s, dst %s, src_dev %s, src %s, func %#x\n", dst_dev->funcs->name,
         dbgstr_bitblt_coords(dst), src_dev->funcs->name, dbgstr_bitblt_coords(src), *(UINT32 *)&func );
    TRACE( "hwnd %p parent %p\n", hwnd, parent );

    dwm_get_image( device, src_dev, src );

    req.dwm_context = device->dwm_context;
    req.rect = dst->visrect;
    NtUserMapWindowPoints( hwnd, parent, (POINT *)&req.rect, 2 );

    if ((window = dwmdrv_window_lock( parent )))
    {
        req.dwm_window = window->dwm_window;
        dwmdrv_window_unlock( window );
    }

    if ((status = NtWriteFile( req_handle, NULL, NULL, NULL, &io, &req, sizeof(union dwm_request), NULL, NULL )) ||
        io.Information != sizeof(union dwm_request))
        ERR( "Failed to write request, status %#x\n", status );

    return TRUE;
}

static BOOL dwmdrv_AngleArc( PHYSDEV dev, INT x, INT y, DWORD radius, FLOAT start, FLOAT sweep )
{
    ERR( "\n" );
    return TRUE;
}

static BOOL dwmdrv_Arc( PHYSDEV dev, INT left, INT top, INT right, INT bottom,
                        INT xstart, INT ystart, INT xend, INT yend )
{
    ERR( "\n" );
    return TRUE;
}

static BOOL dwmdrv_ArcTo( PHYSDEV dev, INT left, INT top, INT right, INT bottom, INT xstart, INT ystart, INT xend, INT yend )
{
    ERR( "\n" );
    return TRUE;
}

static DWORD dwmdrv_BlendImage( PHYSDEV dev, BITMAPINFO *info, const struct gdi_image_bits *bits,
                                struct bitblt_coords *src, struct bitblt_coords *dst, BLENDFUNCTION func )
{
    ERR( "\n" );
    return 0;
}

static BOOL dwmdrv_Chord( PHYSDEV dev, INT left, INT top, INT right, INT bottom,
                          INT xstart, INT ystart, INT xend, INT yend )
{
    ERR( "\n" );
    return TRUE;
}

static BOOL dwmdrv_CreateCompatibleDC( PHYSDEV orig, PHYSDEV *dev )
{
    struct dwm_gdi_device *device;
    PHYSDEV next = *dev;

    if (orig)  /* chain to x11drv first */
    {
        orig = GET_NEXT_PHYSDEV( orig, pCreateCompatibleDC );
        if (!orig->funcs->pCreateCompatibleDC( orig, dev )) return FALSE;
    }

    TRACE( "orig %p, dev %p\n", orig, dev );
    TRACE( "  next %p hdc %p dev %s prio %d next %p\n", next, next->hdc, next->funcs->name, next->funcs->priority, next->next );
    next = next->next;
    TRACE( "  next %p hdc %p dev %s prio %d next %p\n", next, next->hdc, next->funcs->name, next->funcs->priority, next->next );

    if (!(device = calloc(1, sizeof(*device)))) return FALSE;
    device->dwm_context = dwm_gdi_context_create();
    push_dc_driver( dev, &device->dev, &dwm_gdi_driver );
    return TRUE;
}

static BOOL dwmdrv_CreateDC( PHYSDEV *dev, const WCHAR *device_name, const WCHAR *output_name,
                             const DEVMODEW *devmode )
{
    struct dwm_gdi_device *device;
    PHYSDEV next = *dev;

    TRACE( "dev %p, name %s, output %s, devmode %p\n", dev, debugstr_w(device_name), debugstr_w(output_name), devmode );
    TRACE( "  next %p hdc %p dev %s prio %d next %p\n", next, next->hdc, next->funcs->name, next->funcs->priority, next->next );
    next = next->next;
    TRACE( "  next %p hdc %p dev %s prio %d next %p\n", next, next->hdc, next->funcs->name, next->funcs->priority, next->next );

    if (!(device = calloc(1, sizeof(*device)))) return FALSE;
    device->dwm_context = dwm_gdi_context_create();
    push_dc_driver( dev, &device->dev, &dwm_gdi_driver );
    return TRUE;
}

static BOOL dwmdrv_DeleteDC( PHYSDEV dev )
{
    struct dwm_gdi_device *device = CONTAINING_RECORD( dev, struct dwm_gdi_device, dev );
    TRACE( "device %p\n", device );
    dwm_gdi_context_destroy( device->dwm_context );
    free( device );
    return TRUE;
}

static BOOL dwmdrv_DeleteObject( PHYSDEV dev, HGDIOBJ obj )
{
    ERR( "\n" );
    return TRUE;
}

static BOOL dwmdrv_Ellipse( PHYSDEV dev, INT left, INT top, INT right, INT bottom )
{
    ERR( "\n" );
    return TRUE;
}

static INT dwmdrv_ExtEscape( PHYSDEV dev, INT escape, INT in_size, const void *in_data,
                             INT out_size, void *out_data )
{
    TRACE( "dev %s, escape %u, in_size %#x, in_data %p, out_size %#x, out_data %p\n",
         dev->funcs->name, escape, in_size, in_data, out_size, out_data );
    return TRUE;
}

static BOOL dwmdrv_ExtFloodFill( PHYSDEV dev, INT x, INT y, COLORREF color, UINT type )
{
    ERR( "\n" );
    return TRUE;
}

static BOOL dwmdrv_ExtTextOut( PHYSDEV dev, INT x, INT y, UINT flags, const RECT *rect,
                               LPCWSTR str, UINT count, const INT *dx )
{
    ERR( "\n" );
    return TRUE;
}

static BOOL dwmdrv_FillPath( PHYSDEV dev )
{
    ERR( "\n" );
    return TRUE;
}

static BOOL dwmdrv_FillRgn( PHYSDEV dev, HRGN rgn, HBRUSH brush )
{
    ERR( "\n" );
    return TRUE;
}

static BOOL dwmdrv_FrameRgn( PHYSDEV dev, HRGN rgn, HBRUSH brush, INT width, INT height )
{
    ERR( "\n" );
    return TRUE;
}

static UINT dwmdrv_GetBoundsRect( PHYSDEV dev, RECT *rect, UINT flags )
{
    ERR( "\n" );
    return DCB_RESET;
}

static INT dwmdrv_GetDeviceCaps( PHYSDEV dev, INT cap )
{
    dev = GET_NEXT_PHYSDEV( dev, pGetDeviceCaps );
    return dev->funcs->pGetDeviceCaps( dev, cap );
}

static BOOL dwmdrv_GetDeviceGammaRamp( PHYSDEV dev, void *ramp )
{
    ERR( "\n" );
    return FALSE;
}

static BOOL dwmdrv_GetICMProfile( PHYSDEV dev, BOOL allow_default, LPDWORD size, LPWSTR filename )
{
    ERR( "\n" );
    return FALSE;
}

static DWORD dwmdrv_GetImage( PHYSDEV dev, BITMAPINFO *info, struct gdi_image_bits *bits,
                               struct bitblt_coords *src )
{
    ERR( "\n" );
    return ERROR_NOT_SUPPORTED;
}

static COLORREF dwmdrv_GetNearestColor( PHYSDEV dev, COLORREF color )
{
    ERR( "\n" );
    return TRUE;
}

static COLORREF dwmdrv_GetPixel( PHYSDEV dev, INT x, INT y )
{
    ERR( "\n" );
    return TRUE;
}

static UINT dwmdrv_GetSystemPaletteEntries( PHYSDEV dev, UINT start, UINT count, PALETTEENTRY *entries )
{
    ERR( "\n" );
    return 0;
}

static BOOL dwmdrv_GradientFill( PHYSDEV dev, TRIVERTEX *vert_array, ULONG nvert,
                                  void * grad_array, ULONG ngrad, ULONG mode )
{
    ERR( "\n" );
    return TRUE;
}

static BOOL dwmdrv_InvertRgn( PHYSDEV dev, HRGN rgn )
{
    ERR( "\n" );
    return TRUE;
}

static BOOL dwmdrv_LineTo( PHYSDEV dev, INT x, INT y )
{
    ERR( "\n" );
    return TRUE;
}

static BOOL dwmdrv_MoveTo( PHYSDEV dev, INT x, INT y )
{
    ERR( "\n" );
    return TRUE;
}

static BOOL dwmdrv_PaintRgn( PHYSDEV dev, HRGN rgn )
{
    ERR( "\n" );
    return TRUE;
}

static BOOL dwmdrv_PatBlt( PHYSDEV dev, struct bitblt_coords *dst, DWORD rop )
{
    ERR( "dev %s, dst %s, rop %#x\n", dev->funcs->name, dbgstr_bitblt_coords(dst), (UINT)rop );
    return TRUE;
}

static BOOL dwmdrv_Pie( PHYSDEV dev, INT left, INT top, INT right, INT bottom,
                         INT xstart, INT ystart, INT xend, INT yend )
{
    ERR( "\n" );
    return TRUE;
}

static BOOL dwmdrv_PolyBezier( PHYSDEV dev, const POINT *points, DWORD count )
{
    ERR( "\n" );
    return TRUE;
}

static BOOL dwmdrv_PolyBezierTo( PHYSDEV dev, const POINT *points, DWORD count )
{
    ERR( "\n" );
    return TRUE;
}

static BOOL dwmdrv_PolyDraw( PHYSDEV dev, const POINT *points, const BYTE *types, DWORD count )
{
    ERR( "\n" );
    return TRUE;
}

static BOOL dwmdrv_PolyPolygon( PHYSDEV dev, const POINT *points, const INT *counts, UINT polygons )
{
    ERR( "\n" );
    return TRUE;
}

static BOOL dwmdrv_PolyPolyline( PHYSDEV dev, const POINT *points, const DWORD *counts, DWORD lines )
{
    ERR( "\n" );
    return TRUE;
}

static BOOL dwmdrv_PolylineTo( PHYSDEV dev, const POINT *points, INT count )
{
    ERR( "\n" );
    return TRUE;
}

static DWORD dwmdrv_PutImage( PHYSDEV dev, HRGN clip, BITMAPINFO *info, const struct gdi_image_bits *bits,
                              struct bitblt_coords *src, struct bitblt_coords *dst, DWORD rop )
{
    struct dwm_req_gdi_put_image req = {.header = {.type = DWM_REQ_GDI_PUT_IMAGE, .req_size = sizeof(req)}};
    struct dwm_gdi_device *device = CONTAINING_RECORD( dev, struct dwm_gdi_device, dev );
    HWND hwnd = NtUserWindowFromDC(device->dev.hdc), parent = NtUserGetAncestor(hwnd, GA_ROOT);
    struct dwmdrv_window *window;
    IO_STATUS_BLOCK io;
    UINT status;

    TRACE( "dev %s, clip %p, info %p, bits %p, src %s, dst %s, rop %#x\n", dev->funcs->name, clip, info, bits,
         dbgstr_bitblt_coords(src), dbgstr_bitblt_coords(dst), (UINT)rop );
    TRACE( "device %p, hdc %p, hwnd %p, parent %p\n", device, device->dev.hdc, hwnd, parent );

    dwm_set_source( device, info, bits, src );

    req.dwm_context = device->dwm_context;
    req.rect = dst->visrect;
    NtUserMapWindowPoints( hwnd, parent, (POINT *)&req.rect, 2 );

    if ((window = dwmdrv_window_lock( parent )))
    {
        req.dwm_window = window->dwm_window;
        dwmdrv_window_unlock( window );
    }

    if ((status = NtWriteFile( req_handle, NULL, NULL, NULL, &io, &req, sizeof(union dwm_request), NULL, NULL )) ||
        io.Information != sizeof(union dwm_request))
        ERR( "Failed to write request, status %#x\n", status );

    return ERROR_SUCCESS;
}

static UINT dwmdrv_RealizeDefaultPalette( PHYSDEV dev )
{
    TRACE( "\n" );
    return 0;
}

static UINT dwmdrv_RealizePalette( PHYSDEV dev, HPALETTE palette, BOOL primary )
{
    ERR( "\n" );
    return 0;
}

static BOOL dwmdrv_Rectangle( PHYSDEV dev, INT left, INT top, INT right, INT bottom )
{
    ERR( "\n" );
    return TRUE;
}

static BOOL dwmdrv_RoundRect( PHYSDEV dev, INT left, INT top, INT right, INT bottom,
                               INT ell_width, INT ell_height )
{
    ERR( "\n" );
    return TRUE;
}

static HBITMAP dwmdrv_SelectBitmap( PHYSDEV dev, HBITMAP bitmap )
{
    TRACE( "\n" );
    return bitmap;
}

static HBRUSH dwmdrv_SelectBrush( PHYSDEV dev, HBRUSH brush, const struct brush_pattern *pattern )
{
    TRACE( "\n" );
    return brush;
}

static HFONT dwmdrv_SelectFont( PHYSDEV dev, HFONT font, UINT *aa_flags )
{
    TRACE( "\n" );
    dev = GET_NEXT_PHYSDEV( dev, pSelectFont );
    return dev->funcs->pSelectFont( dev, font, aa_flags );
}

static HPEN dwmdrv_SelectPen( PHYSDEV dev, HPEN pen, const struct brush_pattern *pattern )
{
    TRACE( "\n" );
    return pen;
}

static UINT dwmdrv_SetBoundsRect( PHYSDEV dev, RECT *rect, UINT flags )
{
    TRACE( "device %p, rect %s, flags %#x\n", dev, wine_dbgstr_rect(rect), flags );
    return DCB_RESET;
}

static COLORREF dwmdrv_SetDCBrushColor( PHYSDEV dev, COLORREF color )
{
    ERR( "\n" );
    return color;
}

static COLORREF dwmdrv_SetDCPenColor( PHYSDEV dev, COLORREF color )
{
    ERR( "\n" );
    return color;
}

static INT dwmdrv_SetDIBitsToDevice( PHYSDEV dev, INT x_dst, INT y_dst, DWORD width, DWORD height,
                                      INT x_src, INT y_src, UINT start, UINT lines,
                                      const void *bits, BITMAPINFO *info, UINT coloruse )
{
    ERR( "\n" );
    return TRUE;
}

static void dwmdrv_SetDeviceClipping( PHYSDEV dev, HRGN rgn )
{
    TRACE( "\n" );
}

static BOOL dwmdrv_SetDeviceGammaRamp( PHYSDEV dev, void *ramp )
{
    ERR( "\n" );
    return FALSE;
}

static COLORREF dwmdrv_SetPixel( PHYSDEV dev, INT x, INT y, COLORREF color )
{
    ERR( "\n" );
    return color;
}

static BOOL dwmdrv_StretchBlt( PHYSDEV dst_dev, struct bitblt_coords *dst,
                               PHYSDEV src_dev, struct bitblt_coords *src, DWORD rop )
{
    struct dwm_req_gdi_put_image req = {.header = {.type = DWM_REQ_GDI_PUT_IMAGE, .req_size = sizeof(req)}};
    struct dwm_gdi_device *device = CONTAINING_RECORD( dst_dev, struct dwm_gdi_device, dev );
    HWND hwnd = NtUserWindowFromDC(device->dev.hdc), parent = NtUserGetAncestor(hwnd, GA_ROOT);
    struct dwmdrv_window *window;
    IO_STATUS_BLOCK io;
    UINT status;

    dwm_get_image( device, src_dev, src );

    req.dwm_context = device->dwm_context;
    req.rect = dst->visrect;
    NtUserMapWindowPoints( hwnd, parent, (POINT *)&req.rect, 2 );

    if ((window = dwmdrv_window_lock( parent )))
    {
        req.dwm_window = window->dwm_window;
        dwmdrv_window_unlock( window );
    }

    if ((status = NtWriteFile( req_handle, NULL, NULL, NULL, &io, &req, sizeof(union dwm_request), NULL, NULL )) ||
        io.Information != sizeof(union dwm_request))
        ERR( "Failed to write request, status %#x\n", status );

    return TRUE;
}

static INT  dwmdrv_StretchDIBits( PHYSDEV dev, INT dst_x, INT dst_y, INT dst_width, INT dst_height,
                                  INT src_x, INT src_y, INT src_width, INT src_height, const void *bits,
                                  BITMAPINFO *info, UINT coloruse, DWORD rop )
{
    TRACE( "dev %p, dst (%d,%d)-(%d,%d), src (%d,%d)-(%d,%d), bits %p, info %p, coloruse %u, rop %#x\n",
         dev, dst_x, dst_y, dst_width, dst_height, src_x, src_y, src_width, src_height, bits, info, coloruse, (UINT)rop );
    dev = GET_NEXT_PHYSDEV( dev, pStretchDIBits );
    return dev->funcs->pStretchDIBits( dev, dst_x, dst_y, dst_width, dst_height, src_x, src_y, src_width, src_height, bits, info, coloruse, rop );
}

static BOOL dwmdrv_StrokeAndFillPath( PHYSDEV dev )
{
    ERR( "\n" );
    return TRUE;
}

static BOOL dwmdrv_StrokePath( PHYSDEV dev )
{
    ERR( "\n" );
    return TRUE;
}

static BOOL dwmdrv_UnrealizePalette( HPALETTE palette )
{
    ERR( "\n" );
    return FALSE;
}


static const struct gdi_dc_funcs dwm_gdi_driver =
{
    /*.pAbortDoc = dwmdrv_AbortDoc,*/
    /*.pAbortPath = dwmdrv_AbortPath,*/
    .pAlphaBlend = dwmdrv_AlphaBlend,
    .pAngleArc = dwmdrv_AngleArc,
    .pArc = dwmdrv_Arc,
    .pArcTo = dwmdrv_ArcTo,
    /*.pBeginPath = dwmdrv_BeginPath,*/
    .pBlendImage = dwmdrv_BlendImage,
    .pChord = dwmdrv_Chord,
    /*.pCloseFigure = dwmdrv_CloseFigure,*/
    .pCreateCompatibleDC = dwmdrv_CreateCompatibleDC,
    .pCreateDC = dwmdrv_CreateDC,
    .pDeleteDC = dwmdrv_DeleteDC,
    .pDeleteObject = dwmdrv_DeleteObject,
    .pEllipse = dwmdrv_Ellipse,
    /*.pEndDoc = dwmdrv_EndDoc,*/
    /*.pEndPage = dwmdrv_EndPage,*/
    /*.pEndPath = dwmdrv_EndPath,*/
    /*.pEnumFonts = dwmdrv_EnumFonts,*/
    .pExtEscape = dwmdrv_ExtEscape,
    .pExtFloodFill = dwmdrv_ExtFloodFill,
    .pExtTextOut = dwmdrv_ExtTextOut,
    .pFillPath = dwmdrv_FillPath,
    .pFillRgn = dwmdrv_FillRgn,
    /*.pFontIsLinked = dwmdrv_FontIsLinked,*/
    .pFrameRgn = dwmdrv_FrameRgn,
    .pGetBoundsRect = dwmdrv_GetBoundsRect,
    /*.pGetCharABCWidths = dwmdrv_GetCharABCWidths,*/
    /*.pGetCharABCWidthsI = dwmdrv_GetCharABCWidthsI,*/
    /*.pGetCharWidth = dwmdrv_GetCharWidth,*/
    /*.pGetCharWidthInfo = dwmdrv_GetCharWidthInfo,*/
    .pGetDeviceCaps = dwmdrv_GetDeviceCaps,
    .pGetDeviceGammaRamp = dwmdrv_GetDeviceGammaRamp,
    /*.pGetFontData = dwmdrv_GetFontData,*/
    /*.pGetFontRealizationInfo = dwmdrv_GetFontRealizationInfo,*/
    /*.pGetFontUnicodeRanges = dwmdrv_GetFontUnicodeRanges,*/
    /*.pGetGlyphIndices = dwmdrv_GetGlyphIndices,*/
    /*.pGetGlyphOutline = dwmdrv_GetGlyphOutline,*/
    .pGetICMProfile = dwmdrv_GetICMProfile,
    .pGetImage = dwmdrv_GetImage,
    /*.pGetKerningPairs = dwmdrv_GetKerningPairs,*/
    .pGetNearestColor = dwmdrv_GetNearestColor,
    /*.pGetOutlineTextMetrics = dwmdrv_GetOutlineTextMetrics,*/
    .pGetPixel = dwmdrv_GetPixel,
    .pGetSystemPaletteEntries = dwmdrv_GetSystemPaletteEntries,
    /*.pGetTextCharsetInfo = dwmdrv_GetTextCharsetInfo,*/
    /*.pGetTextExtentExPoint = dwmdrv_GetTextExtentExPoint,*/
    /*.pGetTextExtentExPointI = dwmdrv_GetTextExtentExPointI,*/
    /*.pGetTextFace = dwmdrv_GetTextFace,*/
    /*.pGetTextMetrics = dwmdrv_GetTextMetrics,*/
    .pGradientFill = dwmdrv_GradientFill,
    .pInvertRgn = dwmdrv_InvertRgn,
    .pLineTo = dwmdrv_LineTo,
    .pMoveTo = dwmdrv_MoveTo,
    .pPaintRgn = dwmdrv_PaintRgn,
    .pPatBlt = dwmdrv_PatBlt,
    .pPie = dwmdrv_Pie,
    .pPolyBezier = dwmdrv_PolyBezier,
    .pPolyBezierTo = dwmdrv_PolyBezierTo,
    .pPolyDraw = dwmdrv_PolyDraw,
    .pPolyPolygon = dwmdrv_PolyPolygon,
    .pPolyPolyline = dwmdrv_PolyPolyline,
    .pPolylineTo = dwmdrv_PolylineTo,
    .pPutImage = dwmdrv_PutImage,
    .pRealizeDefaultPalette = dwmdrv_RealizeDefaultPalette,
    .pRealizePalette = dwmdrv_RealizePalette,
    .pRectangle = dwmdrv_Rectangle,
    /*.pResetDC = dwmdrv_ResetDC,*/
    .pRoundRect = dwmdrv_RoundRect,
    .pSelectBitmap = dwmdrv_SelectBitmap,
    .pSelectBrush = dwmdrv_SelectBrush,
    .pSelectFont = dwmdrv_SelectFont,
    .pSelectPen = dwmdrv_SelectPen,
    /*.pSetBkColor = dwmdrv_SetBkColor,*/
    .pSetBoundsRect = dwmdrv_SetBoundsRect,
    .pSetDCBrushColor = dwmdrv_SetDCBrushColor,
    .pSetDCPenColor = dwmdrv_SetDCPenColor,
    .pSetDIBitsToDevice = dwmdrv_SetDIBitsToDevice,
    .pSetDeviceClipping = dwmdrv_SetDeviceClipping,
    .pSetDeviceGammaRamp = dwmdrv_SetDeviceGammaRamp,
    .pSetPixel = dwmdrv_SetPixel,
    /*.pSetTextColor = dwmdrv_SetTextColor,*/
    /*.pStartDoc = dwmdrv_StartDoc,*/
    /*.pStartPage = dwmdrv_StartPage,*/
    .pStretchBlt = dwmdrv_StretchBlt,
    .pStretchDIBits = dwmdrv_StretchDIBits,
    .pStrokeAndFillPath = dwmdrv_StrokeAndFillPath,
    .pStrokePath = dwmdrv_StrokePath,
    .pUnrealizePalette = dwmdrv_UnrealizePalette,

    .priority = GDI_PRIORITY_GRAPHICS_DRV + 20,         /* priority */
    .name = "dwmdrv",
};

static dwm_window_t dwm_window_create( dwm_display_t display, HWND hwnd, UINT64 native )
{
    struct dwm_req_window_create req = {.header = {.type = DWM_REQ_WINDOW_CREATE, .req_size = sizeof(req)}};
    struct dwmdrv_window *window;
    union dwm_reply reply;
    IO_STATUS_BLOCK io;
    UINT status;

    req.dwm_display = display;
    req.hwnd = hwnd;
    req.native = native;

    if ((status = NtWriteFile( req_handle, NULL, NULL, NULL, &io, &req, sizeof(union dwm_request), NULL, NULL )) ||
        io.Information != sizeof(union dwm_request))
        ERR( "Failed to write request, status %#x\n", status );
    if ((status = NtReadFile( reply_handle, NULL, NULL, NULL, &io, &reply, sizeof(reply), NULL, NULL )) ||
        io.Information != sizeof(reply) || (status = reply.header.status))
        ERR( "Failed to read reply, status %#x\n", status );

    if ((window = dwmdrv_window_lock( hwnd )))
    {
        window->dwm_window = reply.window_create.dwm_window;
        dwmdrv_window_unlock( window );
    }
    else
    {
        if (!(window = calloc( 1, sizeof(*window) ))) return FALSE;
        window->dwm_window = reply.window_create.dwm_window;
        dwmdrv_window_init( hwnd, window );
    }

    return reply.window_create.dwm_window;
}

static void dwm_window_update( dwm_window_t dwm_window, const RECT *window_rect )
{
    struct dwm_req_window_update req = {.header = {.type = DWM_REQ_WINDOW_UPDATE, .req_size = sizeof(req)}};
    union dwm_reply reply;
    IO_STATUS_BLOCK io;
    UINT status;

    req.dwm_window = dwm_window;
    req.window_rect = *window_rect;

    if ((status = NtWriteFile( req_handle, NULL, NULL, NULL, &io, &req, sizeof(union dwm_request), NULL, NULL )) ||
        io.Information != sizeof(union dwm_request))
        ERR( "Failed to write request, status %#x\n", status );
    if ((status = NtReadFile( reply_handle, NULL, NULL, NULL, &io, &reply, sizeof(reply), NULL, NULL )) ||
        io.Information != sizeof(reply) || (status = reply.header.status))
        ERR( "Failed to read reply, status %#x\n", status );
}

static void dwm_window_destroy( dwm_window_t dwm_window )
{
    struct dwm_req_window_destroy req = {.header = {.type = DWM_REQ_WINDOW_DESTROY, .req_size = sizeof(req)}};
    union dwm_reply reply;
    IO_STATUS_BLOCK io;
    UINT status;

    req.dwm_window = dwm_window;

    if ((status = NtWriteFile( req_handle, NULL, NULL, NULL, &io, &req, sizeof(union dwm_request), NULL, NULL )) ||
        io.Information != sizeof(union dwm_request))
        ERR( "Failed to write request, status %#x\n", status );
    if ((status = NtReadFile( reply_handle, NULL, NULL, NULL, &io, &reply, sizeof(reply), NULL, NULL )) ||
        io.Information != sizeof(reply) || (status = reply.header.status))
        ERR( "Failed to read reply, status %#x\n", status );
}

static BOOL dwmdrv_WindowPosChanging( HWND hwnd, HWND insert_after, UINT swp_flags,
                                      const RECT *window_rect, const RECT *client_rect,
                                      RECT *visible_rect, struct window_surface **surface )
{
    struct dwmdrv_window *window;
    dwm_window_t dwm_window;

    TRACE( "hwnd %p\n", hwnd );

    if ((window = dwmdrv_window_lock( hwnd )))
    {
        dwm_window = window->dwm_window;
        dwmdrv_window_unlock( window );
    }
    else
    {
        if (!(window = calloc( 1, sizeof(*window) ))) return FALSE;
        dwm_window = window->dwm_window = dwm_window_create( 0, hwnd, 0 );
        dwmdrv_window_init( hwnd, window );
    }

    dwm_window_update( dwm_window, window_rect );

    return TRUE;
}

static void dwmdrv_WindowPosChanged( HWND hwnd, HWND insert_after, UINT swp_flags,
                                     const RECT *window_rect, const RECT *client_rect,
                                     const RECT *visible_rect, const RECT *valid_rects,
                                     struct window_surface *surface )
{
    struct dwmdrv_window *window;
    dwm_window_t dwm_window;

    TRACE( "hwnd %p\n", hwnd );

    if ((window = dwmdrv_window_lock( hwnd )))
    {
        dwm_window = window->dwm_window;
        dwmdrv_window_unlock( window );
    }
    else
    {
        if (!(window = calloc( 1, sizeof(*window) ))) return;
        dwm_window = window->dwm_window = dwm_window_create( 0, hwnd, 0 );
        dwmdrv_window_init( hwnd, window );
    }

    dwm_window_update( dwm_window, window_rect );
}

static void dwmdrv_DestroyWindow( HWND hwnd )
{
    struct dwmdrv_window *window;
    dwm_window_t dwm_window;

    TRACE( "hwnd %p\n", hwnd );

    if (!(window = dwmdrv_window_lock( hwnd ))) return;
    dwm_window = window->dwm_window;
    dwmdrv_window_unlock( window );

    dwmdrv_window_destroy( hwnd );
    dwm_window_destroy( dwm_window );
}

static const struct user_driver_funcs dwm_user_driver =
{
    .pWindowPosChanging = dwmdrv_WindowPosChanging,
    .pWindowPosChanged = dwmdrv_WindowPosChanged,
    .pDestroyWindow = dwmdrv_DestroyWindow,
};

#ifdef _WIN64
static inline TEB64 *NtCurrentTeb64(void) { return NULL; }
#else
static inline TEB64 *NtCurrentTeb64(void) { return (TEB64 *)NtCurrentTeb()->GdiBatchCount; }
#endif

static void dwm_server_start(void)
{
    WCHAR appnameW[MAX_PATH], cmdlineW[MAX_PATH], system_dirW[MAX_PATH];
    RTL_USER_PROCESS_PARAMETERS params = { sizeof(params), sizeof(params) };
    ULONG_PTR buffer[offsetof( PS_ATTRIBUTE_LIST, Attributes[2] ) / sizeof(ULONG_PTR)];
    PS_ATTRIBUTE_LIST *ps_attr = (PS_ATTRIBUTE_LIST *)buffer;
    TEB64 *teb64 = NtCurrentTeb64();
    PEB *peb = NtCurrentTeb()->Peb;
    PS_CREATE_INFO create_info;
    WCHAR desktop[MAX_PATH];
    HANDLE process, thread;
    unsigned int status;
    BOOL redirect;

    asciiz_to_unicode( appnameW, "\\??\\C:\\windows\\system32\\dwm.exe" );
    asciiz_to_unicode( cmdlineW, "\"C:\\windows\\system32\\dwm.exe\"" );
    asciiz_to_unicode( system_dirW, "C:\\windows\\system32" );

    params.Flags           = PROCESS_PARAMS_FLAG_NORMALIZED;
    params.Environment     = peb->ProcessParameters->Environment;
    params.EnvironmentSize = peb->ProcessParameters->EnvironmentSize;
    params.hStdError       = peb->ProcessParameters->hStdError;
    RtlInitUnicodeString( &params.CurrentDirectory.DosPath, system_dirW );
    RtlInitUnicodeString( &params.ImagePathName, appnameW + 4 );
    RtlInitUnicodeString( &params.CommandLine, cmdlineW );
    RtlInitUnicodeString( &params.WindowTitle, appnameW + 4 );
    RtlInitUnicodeString( &params.Desktop, desktop );

    ps_attr->Attributes[0].Attribute    = PS_ATTRIBUTE_IMAGE_NAME;
    ps_attr->Attributes[0].Size         = sizeof(appnameW) - sizeof(WCHAR);
    ps_attr->Attributes[0].ValuePtr     = (WCHAR *)appnameW;
    ps_attr->Attributes[0].ReturnLength = NULL;

    ps_attr->Attributes[1].Attribute    = PS_ATTRIBUTE_TOKEN;
    ps_attr->Attributes[1].Size         = sizeof(HANDLE);
    ps_attr->Attributes[1].ValuePtr     = GetCurrentThreadEffectiveToken();
    ps_attr->Attributes[1].ReturnLength = NULL;

    ps_attr->TotalLength = offsetof( PS_ATTRIBUTE_LIST, Attributes[2] );

    if ((redirect = teb64 && !teb64->TlsSlots[WOW64_TLS_FILESYSREDIR]))
        teb64->TlsSlots[WOW64_TLS_FILESYSREDIR] = TRUE;
    status = NtCreateUserProcess( &process, &thread, PROCESS_ALL_ACCESS, THREAD_ALL_ACCESS,
                                  NULL, NULL, 0, THREAD_CREATE_FLAGS_CREATE_SUSPENDED, &params,
                                  &create_info, ps_attr );
    if (redirect) teb64->TlsSlots[WOW64_TLS_FILESYSREDIR] = FALSE;

    if (status)
        ERR( "failed to start dwm, status %#x\n", status );
    else
    {
        NtResumeThread( thread, NULL );
        TRACE( "started dwm\n" );
        NtClose( thread );
        NtClose( process );
    }
}

static HANDLE create_request_pipe( HANDLE *req_handle )
{
    HANDLE server_handle = 0;
    UINT status;
    int fds[2];

    if (pipe( fds ) < 0) return 0;
    fcntl( fds[0], F_SETFD, FD_CLOEXEC );
    fcntl( fds[1], F_SETFD, FD_CLOEXEC );

    if ((status = wine_server_fd_to_handle( fds[0], GENERIC_READ | SYNCHRONIZE, 0, &server_handle )) ||
        (status = wine_server_fd_to_handle( fds[1], GENERIC_WRITE | SYNCHRONIZE, 0, req_handle )))
    {
        ERR( "Failed to create handles for DWM pipes, status %#x\n", status );
        if (server_handle) NtClose( server_handle );
        server_handle = 0;
    }

    close( fds[0] );
    close( fds[1] );
    return server_handle;
}

static dwm_display_t dwm_connect( const char *display_type, const char *display_name )
{
    struct dwm_req_connect req = {.header = {.type = DWM_REQ_CONNECT, .req_size = sizeof(req)}};
    union dwm_reply reply;
    IO_STATUS_BLOCK io;
    UINT status;

    strcpy( req.display_type, display_type );
    strcpy( req.display_name, display_name );
    req.version = DWM_PROTOCOL_VERSION;

    if ((status = NtWriteFile( req_handle, NULL, NULL, NULL, &io, &req, sizeof(union dwm_request), NULL, NULL )) ||
        io.Information != sizeof(union dwm_request))
    {
        ERR( "Failed to write HELLO, status %#x\n", status );
        return DWM_INVALID_DISPLAY;
    }
    if ((status = NtReadFile( reply_handle, NULL, NULL, NULL, &io, &reply, sizeof(reply), NULL, NULL )) ||
        io.Information != sizeof(reply) || (status = reply.header.status))
    {
        ERR( "Failed to read HELLO, status %#x\n", status );
        return DWM_INVALID_DISPLAY;
    }

    return reply.connect.dwm_display;
}

static BOOL dwm_server_connect(void)
{
    HANDLE server_handle;

    if (req_handle) NtClose( req_handle );
    if (reply_handle) NtClose( reply_handle );
    reply_handle = req_handle = 0;

    if (!(server_handle = create_request_pipe( &req_handle ))) return FALSE;

    for (;;)
    {
        SERVER_START_REQ( dwm_connect )
        {
            req->server_handle = wine_server_obj_handle( server_handle );
            if (wine_server_call_err( req )) reply_handle = 0;
            else reply_handle = wine_server_ptr_handle( reply->handle );
        }
        SERVER_END_REQ;

        if (reply_handle) break;
        dwm_server_start();
    }

    NtClose( server_handle );

    return TRUE;
}

static dwm_display_t dwm_client_connect( const char *display_type, const char *display_name )
{
    return dwm_connect( display_type, display_name ? display_name : "" );
}

static const struct dwm_funcs dwm_funcs =
{
    &dwm_gdi_driver,
    dwm_client_connect,
    dwm_window_create,
    dwm_window_destroy,
};

const struct dwm_funcs *__wine_get_dwm_driver( UINT version )
{
    TRACE( "version %u\n", version );
    if (version != WINE_GDI_DRIVER_VERSION) return NULL;
    if (!dwm_server_connect()) return NULL;
    return &dwm_funcs;
}

BOOL load_dwm_driver(void)
{
    TRACE( "\n" );

    if (!dwm_server_connect()) return FALSE;
    __wine_set_user_driver( &dwm_user_driver, WINE_GDI_DRIVER_VERSION );
    return TRUE;
}
