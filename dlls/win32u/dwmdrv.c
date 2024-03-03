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

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dwmdrv);

enum opcode
{
    DWM_OP_SET_SOURCE,
    DWM_OP_SET_POINTS,

    DWM_OP_CLIP_SOURCE,

    DWM_OP_ALPHA_BLEND,
    DWM_OP_BLEND_IMAGE,
    DWM_OP_STRETCH_BLT,
    DWM_OP_PUT_IMAGE,

    DWM_OP_MOVE_TO,
    DWM_OP_LINE_TO,
    DWM_OP_RECTANGLE,
    DWM_OP_POLY_POLY_LINE,
    DWM_OP_POLY_POLYGON,

    DWM_OP_PAT_BLT,
    DWM_OP_EXT_TEXT_OUT,

    DWM_OP_SELECT_BRUSH,
    DWM_OP_SELECT_FONT,
    DWM_OP_SELECT_PEN,

    DWM_OP_SET_BOUNDS_RECT,
    DWM_OP_SET_DEVICE_CLIPPING,
};

struct cmd
{
    UINT32 op;
};


struct cmd_set_source
{
    struct cmd cmd;
    UINT32 bpp;
    UINT32 stride;
    RECT rect;
};

struct cmd_set_points
{
    struct cmd cmd;
    UINT32 count;
};


struct cmd_alpha_blend
{
    struct cmd cmd;
    RECT dst_rect;
    BLENDFUNCTION func;
};

struct cmd_blend_image
{
    struct cmd cmd;
    RECT dst_rect;
    BLENDFUNCTION func;
};

struct cmd_stretch_blt
{
    struct cmd cmd;
    RECT dst_rect;
    UINT32 rop;
};

struct cmd_put_image
{
    struct cmd cmd;
    RECT dst_rect;
    UINT32 rop;
};


struct cmd_move_to
{
    struct cmd cmd;
    UINT x;
    UINT y;
};

struct cmd_line_to
{
    struct cmd cmd;
    UINT x;
    UINT y;
};

struct cmd_rectangle
{
    struct cmd cmd;
    UINT left;
    UINT top;
    UINT right;
    UINT bottom;
};

struct cmd_poly_poly_line
{
    struct cmd cmd;
    UINT lines;
};

struct cmd_poly_polygon
{
    struct cmd cmd;
    UINT polygons;
};


struct cmd_pat_blt
{
    struct cmd cmd;
    RECT dst_rect;
    UINT32 rop;
};

struct cmd_ext_text_out
{
    struct cmd cmd;
    UINT x;
    UINT y;
    UINT flags;
    RECT rect;
    UINT count;
};


struct cmd_select_brush
{
    struct cmd cmd;
    HBRUSH brush;
};

struct cmd_select_font
{
    struct cmd cmd;
    HFONT font;
};

struct cmd_select_pen
{
    struct cmd cmd;
    HPEN pen;
};


struct cmd_set_bounds_rect
{
    struct cmd cmd;
    RECT rect;
    UINT32 flags;
};


struct device
{
    struct gdi_physdev dev;
};

static struct device *device_from_physdev( PHYSDEV dev )
{
    return CONTAINING_RECORD( dev, struct device, dev );
}

static const char *dbgstr_coords( const struct bitblt_coords *coords )
{
    RECT log = {coords->log_x, coords->log_y, coords->log_x + coords->log_width, coords->log_y + coords->log_height};
    RECT phy = {coords->x, coords->y, coords->x + coords->width, coords->y + coords->height};
    return wine_dbg_sprintf( "(log %s phy %s vis %s)", wine_dbgstr_rect(&log), wine_dbgstr_rect(&phy),
                             wine_dbgstr_rect(&coords->visrect) );
};

static void *device_start_cmd( struct device *device, enum opcode op, UINT size )
{
    return calloc( 1, size );
}

static void device_end_cmd( struct device *device, void *cmd )
{
    free( cmd );
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
                             BYTE *dst, UINT dst_stride, const RECT *dst_rect )
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

static void device_set_source_cmd( struct device *device, BITMAPINFO *info, struct bitblt_coords *src,
                                   const struct gdi_image_bits *bits )
{
    UINT bpp, data_size, stride;
    struct cmd_set_source *cmd;
    RECT rect;

    bpp = info->bmiHeader.biBitCount;
    get_buffer_stride( info, &src->visrect, &stride, &rect );
    data_size = stride * rect.bottom;

    if ((cmd = device_start_cmd( device, DWM_OP_SET_SOURCE, sizeof(*cmd) + data_size )))
    {
        cmd->bpp = bpp;
        cmd->rect = rect;
        cmd->stride = stride;
        copy_image_bits( bits, info, &src->visrect, (BYTE *)(cmd + 1), stride, &rect );
        device_end_cmd( device, cmd );
    }
}

static void device_set_source_cmd_from_dev( struct device *device, PHYSDEV src_dev, struct bitblt_coords *src )
{
    char info_buffer[FIELD_OFFSET( BITMAPINFO, bmiColors[256] )];
    BITMAPINFO *info = (BITMAPINFO *)info_buffer;
    DC *dc = get_physdev_dc( src_dev );
    struct gdi_image_bits bits;
    UINT err;

    src_dev = GET_DC_PHYSDEV( dc, pGetImage );
    if ((err = src_dev->funcs->pGetImage( src_dev, info, &bits, src ))) ERR( "failed to get image data, error %u\n", err );
    else device_set_source_cmd( device, info, src, &bits );

    if (bits.free) bits.free( &bits );
}


static void device_set_points_cmd( struct device *device, const POINT *points,
                                   const UINT *counts, UINT count, BOOL logical )
{
    DC *dc = get_physdev_dc( &device->dev );
    struct cmd_set_points *cmd;
    UINT i, total, data_size;

    for (i = total = 0; i < count; i++) total += counts[i];
    data_size = total * sizeof(*points);

    if ((cmd = device_start_cmd( device, DWM_OP_SET_POINTS, sizeof(*cmd) + data_size )))
    {
        memcpy( cmd + 1, points, data_size );
        if (logical) lp_to_dp( dc, (POINT *)(cmd + 1), total );
        device_end_cmd( device, cmd );
    }
}

static void device_set_points_cmd_from_hrgn( struct device *device, HRGN hrgn, enum opcode set_cmd )
{
    const WINEREGION *region;
    struct cmd *cmd;

    if ((region = get_wine_region( hrgn )))
    {
        UINT count = region->numRects * 2;
        device_set_points_cmd( device, (POINT *)region->rects, &count, 1, FALSE );
        release_wine_region( hrgn );
    }

    if ((cmd = device_start_cmd( device, set_cmd, sizeof(*cmd) )))
        device_end_cmd( device, cmd );
}


static BOOL drmdrv_AlphaBlend( PHYSDEV dst_dev, struct bitblt_coords *dst, PHYSDEV src_dev,
                               struct bitblt_coords *src, BLENDFUNCTION func )
{
    struct device *device = device_from_physdev( dst_dev );
    struct cmd_alpha_blend *cmd;

    TRACE( "device %p, dst %s, src_dev %s, src %s, func %#x\n", device, dbgstr_coords( dst ),
           src_dev->funcs->name, dbgstr_coords( src ), *(UINT32 *)&func );

    device_set_source_cmd_from_dev( device, src_dev, src );
    if ((cmd = device_start_cmd( device, DWM_OP_ALPHA_BLEND, sizeof(*cmd) )))
    {
        cmd->dst_rect = dst->visrect;
        cmd->func = func;
        device_end_cmd( device, cmd );
    }

    dst_dev = GET_NEXT_PHYSDEV( dst_dev, pAlphaBlend );
    return dst_dev->funcs->pAlphaBlend( dst_dev, dst, src_dev, src, func );
}

static BOOL drmdrv_AngleArc( PHYSDEV dev, INT x, INT y, DWORD radius, FLOAT start, FLOAT sweep )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static BOOL drmdrv_Arc( PHYSDEV dev, INT left, INT top, INT right, INT bottom, INT xstart,
                        INT ystart, INT xend, INT yend )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static BOOL drmdrv_ArcTo( PHYSDEV dev, INT left, INT top, INT right, INT bottom, INT xstart,
                          INT ystart, INT xend, INT yend )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static DWORD drmdrv_BlendImage( PHYSDEV dev, BITMAPINFO *info, const struct gdi_image_bits *bits,
                                struct bitblt_coords *src, struct bitblt_coords *dst, BLENDFUNCTION func )
{
    struct device *device = device_from_physdev( dev );
    struct cmd_blend_image *cmd;

    TRACE( "device %p, info %p, bits %p, src %s, dst %s, func %#x\n", device, info, bits,
           dbgstr_coords( src ), dbgstr_coords( dst ), *(UINT32 *)&func );

    device_set_source_cmd( device, info, src, bits );
    if ((cmd = device_start_cmd( device, DWM_OP_BLEND_IMAGE, sizeof(*cmd) )))
    {
        cmd->dst_rect = dst->visrect;
        cmd->func = func;
        device_end_cmd( device, cmd );
    }

    dev = GET_NEXT_PHYSDEV( dev, pBlendImage );
    return dev->funcs->pBlendImage( dev, info, bits, src, dst, func );
}

static BOOL drmdrv_Chord( PHYSDEV dev, INT left, INT top, INT right, INT bottom, INT xstart,
                          INT ystart, INT xend, INT yend )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static inline BOOL drmdrv_CreateCompatibleDC( PHYSDEV orig, PHYSDEV *dev )
{
    struct device *device;
    PHYSDEV next = *dev;

    TRACE( "orig %p, dev %p\n", orig, dev );
    TRACE( "  next %p hdc %p dev %s prio %d next %p\n", next, next->hdc, next->funcs->name,
           next->funcs->priority, next->next );
    next = next->next;
    TRACE( "  next %p hdc %p dev %s prio %d next %p\n", next, next->hdc, next->funcs->name,
           next->funcs->priority, next->next );

    if (!(device = calloc( 1, sizeof(*device) ))) return FALSE;
    push_dc_driver( dev, &device->dev, &dwm_driver );
    return TRUE;
}

static BOOL drmdrv_CreateDC( PHYSDEV *dev, const WCHAR *device_name, const WCHAR *output_name, const DEVMODEW *devmode )
{
    struct device *device;
    PHYSDEV next = *dev;

    TRACE( "dev %p, name %s, output %s, devmode %p\n", dev, debugstr_w( device_name ),
           debugstr_w( output_name ), devmode );
    TRACE( "  next %p hdc %p dev %s prio %d next %p\n", next, next->hdc, next->funcs->name,
           next->funcs->priority, next->next );
    next = next->next;
    TRACE( "  next %p hdc %p dev %s prio %d next %p\n", next, next->hdc, next->funcs->name,
           next->funcs->priority, next->next );

    if (!(device = calloc( 1, sizeof(*device) ))) return FALSE;
    push_dc_driver( dev, &device->dev, &dwm_driver );
    return TRUE;
}

static BOOL drmdrv_DeleteDC( PHYSDEV dev )
{
    struct device *device = device_from_physdev( dev );
    TRACE( "device %p\n", device );
    free( device );
    return TRUE;
}

static BOOL drmdrv_DeleteObject( PHYSDEV dev, HGDIOBJ obj )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static BOOL drmdrv_Ellipse( PHYSDEV dev, INT left, INT top, INT right, INT bottom )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static INT drmdrv_ExtEscape( PHYSDEV dev, INT escape, INT in_size, const void *in_data, INT out_size, void *out_data )
{
    struct device *device = device_from_physdev( dev );

    /* FIXME */
    TRACE( "device %p, escape %u, in_size %#x, in_data %p, out_size %#x, out_data %p\n", device,
           escape, in_size, in_data, out_size, out_data );

    dev = GET_NEXT_PHYSDEV( dev, pExtEscape );
    return dev->funcs->pExtEscape( dev, escape, in_size, in_data, out_size, out_data );
}

static BOOL drmdrv_ExtFloodFill( PHYSDEV dev, INT x, INT y, COLORREF color, UINT type )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static BOOL drmdrv_ExtTextOut( PHYSDEV dev, INT x, INT y, UINT flags, const RECT *rect, LPCWSTR str,
                               UINT count, const INT *dx )
{
    struct device *device = device_from_physdev( dev );
    UINT data_size = count * sizeof(WCHAR);
    struct cmd_ext_text_out *cmd;

    TRACE( "device %p, x %d, y %d, flags %#x, rect %s, str %s, dx %p\n", device, x, y, flags,
           wine_dbgstr_rect( rect ), debugstr_wn( str, count ), dx );

    if ((cmd = device_start_cmd( device, DWM_OP_EXT_TEXT_OUT, sizeof(*cmd) + data_size )))
    {
        /* FIXME: dx */
        cmd->x = x;
        cmd->y = y;
        cmd->flags = flags;
        cmd->rect = *rect;
        cmd->count = count;
        memcpy( cmd + 1, str, data_size );
        device_end_cmd( device, cmd );
    }

    dev = GET_NEXT_PHYSDEV( dev, pExtTextOut );
    return dev->funcs->pExtTextOut( dev, x, y, flags, rect, str, count, dx );
}

static BOOL drmdrv_FillPath( PHYSDEV dev )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static BOOL drmdrv_FillRgn( PHYSDEV dev, HRGN rgn, HBRUSH brush )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static BOOL drmdrv_FrameRgn( PHYSDEV dev, HRGN rgn, HBRUSH brush, INT width, INT height )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static UINT drmdrv_GetBoundsRect( PHYSDEV dev, RECT *rect, UINT flags )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return DCB_RESET;
}

static INT drmdrv_GetDeviceCaps( PHYSDEV dev, INT cap )
{
    struct device *device = device_from_physdev( dev );
    TRACE( "device %p\n", device );
    dev = GET_NEXT_PHYSDEV( dev, pGetDeviceCaps );
    return dev->funcs->pGetDeviceCaps( dev, cap );
}

static BOOL drmdrv_GetDeviceGammaRamp( PHYSDEV dev, void *ramp )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return FALSE;
}

static BOOL drmdrv_GetICMProfile( PHYSDEV dev, BOOL allow_default, LPDWORD size, LPWSTR filename )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return FALSE;
}

static DWORD drmdrv_GetImage( PHYSDEV dev, BITMAPINFO *info, struct gdi_image_bits *bits, struct bitblt_coords *src )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return ERROR_NOT_SUPPORTED;
}

static COLORREF drmdrv_GetNearestColor( PHYSDEV dev, COLORREF color )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static COLORREF drmdrv_GetPixel( PHYSDEV dev, INT x, INT y )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static UINT drmdrv_GetSystemPaletteEntries( PHYSDEV dev, UINT start, UINT count, PALETTEENTRY *entries )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return 0;
}

static BOOL drmdrv_GradientFill( PHYSDEV dev, TRIVERTEX *vert_array, ULONG nvert, void *grad_array,
                                 ULONG ngrad, ULONG mode )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static BOOL drmdrv_InvertRgn( PHYSDEV dev, HRGN rgn )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static BOOL drmdrv_LineTo( PHYSDEV dev, INT x, INT y )
{
    struct device *device = device_from_physdev( dev );
    struct cmd_line_to *cmd;

    TRACE( "device %p, x %d, y %d\n", device, x, y );

    if ((cmd = device_start_cmd( device, DWM_OP_LINE_TO, sizeof(*cmd) )))
    {
        cmd->x = x;
        cmd->y = y;
        device_end_cmd( device, cmd );
    }

    dev = GET_NEXT_PHYSDEV( dev, pLineTo );
    return dev->funcs->pLineTo( dev, x, y );
}

static BOOL drmdrv_MoveTo( PHYSDEV dev, INT x, INT y )
{
    struct device *device = device_from_physdev( dev );
    struct cmd_move_to *cmd;

    TRACE( "device %p, x %d, y %d\n", device, x, y );

    if ((cmd = device_start_cmd( device, DWM_OP_MOVE_TO, sizeof(*cmd) )))
    {
        cmd->x = x;
        cmd->y = y;
        device_end_cmd( device, cmd );
    }

    dev = GET_NEXT_PHYSDEV( dev, pMoveTo );
    return dev->funcs->pMoveTo( dev, x, y );
}

static BOOL drmdrv_PaintRgn( PHYSDEV dev, HRGN rgn )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static BOOL drmdrv_PatBlt( PHYSDEV dev, struct bitblt_coords *dst, DWORD rop )
{
    struct device *device = device_from_physdev( dev );
    struct cmd_pat_blt *cmd;

    TRACE( "device %p, dst %s, rop %#x\n", device, dbgstr_coords( dst ), (UINT)rop );

    if ((cmd = device_start_cmd( device, DWM_OP_PAT_BLT, sizeof(*cmd) )))
    {
        cmd->dst_rect = dst->visrect;
        cmd->rop = rop;
        device_end_cmd( device, cmd );
    }

    dev = GET_NEXT_PHYSDEV( dev, pPatBlt );
    return dev->funcs->pPatBlt( dev, dst, rop );
}

static BOOL drmdrv_Pie( PHYSDEV dev, INT left, INT top, INT right, INT bottom, INT xstart,
                        INT ystart, INT xend, INT yend )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static BOOL drmdrv_PolyBezier( PHYSDEV dev, const POINT *points, DWORD count )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static BOOL drmdrv_PolyBezierTo( PHYSDEV dev, const POINT *points, DWORD count )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static BOOL drmdrv_PolyDraw( PHYSDEV dev, const POINT *points, const BYTE *types, DWORD count )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static BOOL drmdrv_PolyPolygon( PHYSDEV dev, const POINT *points, const INT *counts, UINT polygons )
{
    struct device *device = device_from_physdev( dev );
    UINT data_size = polygons * sizeof(*counts);
    struct cmd_poly_polygon *cmd;

    TRACE( "device %p, points %p, counts %p, polygons %u\n", device, points, counts, polygons );

    device_set_points_cmd( device, points, (const UINT *)counts, polygons, TRUE );

    if ((cmd = device_start_cmd( device, DWM_OP_POLY_POLYGON, sizeof(*cmd) + data_size )))
    {
        cmd->polygons = polygons;
        memcpy( cmd + 1, counts, data_size );
        device_end_cmd( device, cmd );
    }

    dev = GET_NEXT_PHYSDEV( dev, pPolyPolygon );
    return dev->funcs->pPolyPolygon( dev, points, counts, polygons );
}

static BOOL drmdrv_PolyPolyline( PHYSDEV dev, const POINT *points, const DWORD *counts, DWORD lines )
{
    struct device *device = device_from_physdev( dev );
    UINT data_size = lines * sizeof(*counts);
    struct cmd_poly_poly_line *cmd;

    TRACE( "device %p, points %p, counts %p, lines %u\n", device, points, counts, (UINT)lines );

    device_set_points_cmd( device, points, (const UINT *)counts, lines, TRUE );

    if ((cmd = device_start_cmd( device, DWM_OP_POLY_POLY_LINE, sizeof(*cmd) + data_size )))
    {
        cmd->lines = lines;
        memcpy( cmd + 1, counts, data_size );
        device_end_cmd( device, cmd );
    }

    dev = GET_NEXT_PHYSDEV( dev, pPolyPolyline );
    return dev->funcs->pPolyPolyline( dev, points, counts, lines );
}

static BOOL drmdrv_PolylineTo( PHYSDEV dev, const POINT *points, INT count )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static DWORD drmdrv_PutImage( PHYSDEV dev, HRGN clip, BITMAPINFO *info, const struct gdi_image_bits *bits,
                              struct bitblt_coords *src, struct bitblt_coords *dst, DWORD rop )
{
    struct device *device = device_from_physdev( dev );
    struct cmd_put_image *cmd;

    TRACE( "device %p, clip %p, info %p, bits %p, src %s, dst %s, rop %#x\n", device, clip, info,
           bits, dbgstr_coords( src ), dbgstr_coords( dst ), (UINT)rop );

    device_set_source_cmd( device, info, src, bits );
    device_set_points_cmd_from_hrgn( device, clip, DWM_OP_CLIP_SOURCE );
    if ((cmd = device_start_cmd( device, DWM_OP_PUT_IMAGE, sizeof(*cmd) )))
    {
        cmd->dst_rect = dst->visrect;
        cmd->rop = rop;
        device_end_cmd( device, cmd );
    }

    dev = GET_NEXT_PHYSDEV( dev, pPutImage );
    return dev->funcs->pPutImage( dev, clip, info, bits, src, dst, rop );
}

static UINT drmdrv_RealizeDefaultPalette( PHYSDEV dev )
{
    struct device *device = device_from_physdev( dev );

    /* FIXME */
    TRACE( "device %p\n", device );

    dev = GET_NEXT_PHYSDEV( dev, pRealizeDefaultPalette );
    return dev->funcs->pRealizeDefaultPalette( dev );
}

static UINT drmdrv_RealizePalette( PHYSDEV dev, HPALETTE palette, BOOL primary )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return 0;
}

static BOOL drmdrv_Rectangle( PHYSDEV dev, INT left, INT top, INT right, INT bottom )
{
    struct device *device = device_from_physdev( dev );
    struct cmd_rectangle *cmd;

    TRACE( "device %p, left %d, top %d, right %d, bottom %d\n", device, left, top, right, bottom );

    if ((cmd = device_start_cmd( device, DWM_OP_RECTANGLE, sizeof(*cmd) )))
    {
        cmd->left = left;
        cmd->top = top;
        cmd->right = right;
        cmd->bottom = bottom;
        device_end_cmd( device, cmd );
    }

    dev = GET_NEXT_PHYSDEV( dev, pRectangle );
    return dev->funcs->pRectangle( dev, left, top, right, bottom );
}

static BOOL drmdrv_RoundRect( PHYSDEV dev, INT left, INT top, INT right, INT bottom, INT ell_width, INT ell_height )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static HBRUSH drmdrv_SelectBrush( PHYSDEV dev, HBRUSH brush, const struct brush_pattern *pattern )
{
    struct device *device = device_from_physdev( dev );
    struct cmd_select_brush *cmd;

    TRACE( "device %p, brush %p, pattern %p\n", device, brush, pattern );

    if ((cmd = device_start_cmd( device, DWM_OP_SELECT_BRUSH, sizeof(*cmd) )))
    {
        /*if (pattern) FIXME( "device %p, brush %p, pattern %p\n", device, brush, pattern );*/
        cmd->brush = brush;
        device_end_cmd( device, cmd );
    }

    dev = GET_NEXT_PHYSDEV( dev, pSelectBrush );
    return dev->funcs->pSelectBrush( dev, brush, pattern );
}

static HFONT drmdrv_SelectFont( PHYSDEV dev, HFONT font, UINT *aa_flags )
{
    struct device *device = device_from_physdev( dev );
    struct cmd_select_font *cmd;

    TRACE( "device %p, font %p, aa_flags %p\n", device, font, aa_flags );

    if ((cmd = device_start_cmd( device, DWM_OP_SELECT_FONT, sizeof(*cmd) )))
    {
        /* if (aa_flags) FIXME( "device %p, font %p, aa_flags %p\n", device, font, aa_flags ); */
        cmd->font = font;
        device_end_cmd( device, cmd );
    }

    dev = GET_NEXT_PHYSDEV( dev, pSelectFont );
    return dev->funcs->pSelectFont( dev, font, aa_flags );
}

static HPEN drmdrv_SelectPen( PHYSDEV dev, HPEN pen, const struct brush_pattern *pattern )
{
    struct device *device = device_from_physdev( dev );
    struct cmd_select_pen *cmd;

    TRACE( "device %p, pen %p, pattern %p\n", device, pen, pattern );

    if ((cmd = device_start_cmd( device, DWM_OP_SELECT_PEN, sizeof(*cmd) )))
    {
        if (pattern) FIXME( "device %p, pen %p, pattern %p\n", device, pen, pattern );
        cmd->pen = pen;
        device_end_cmd( device, cmd );
    }

    dev = GET_NEXT_PHYSDEV( dev, pSelectPen );
    return dev->funcs->pSelectPen( dev, pen, pattern );
}

static UINT drmdrv_SetBoundsRect( PHYSDEV dev, RECT *rect, UINT flags )
{
    struct device *device = device_from_physdev( dev );
    struct cmd_set_bounds_rect *cmd;

    TRACE( "device %p, rect %s, flags %#x\n", device, wine_dbgstr_rect( rect ), flags );

    if ((cmd = device_start_cmd( device, DWM_OP_SET_BOUNDS_RECT, sizeof(*cmd) )))
    {
        cmd->rect = *rect;
        cmd->flags = flags;
        device_end_cmd( device, cmd );
    }

    dev = GET_NEXT_PHYSDEV( dev, pSetBoundsRect );
    return dev->funcs->pSetBoundsRect( dev, rect, flags );
}

static COLORREF drmdrv_SetDCBrushColor( PHYSDEV dev, COLORREF color )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return color;
}

static COLORREF drmdrv_SetDCPenColor( PHYSDEV dev, COLORREF color )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return color;
}

static INT drmdrv_SetDIBitsToDevice( PHYSDEV dev, INT x_dst, INT y_dst, DWORD width, DWORD height,
                                     INT x_src, INT y_src, UINT start, UINT lines, const void *bits,
                                     BITMAPINFO *info, UINT coloruse )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static void drmdrv_SetDeviceClipping( PHYSDEV dev, HRGN clip )
{
    struct device *device = device_from_physdev( dev );

    TRACE( "device %p, clip %p\n", device, clip );

    device_set_points_cmd_from_hrgn( device, clip, DWM_OP_SET_DEVICE_CLIPPING );

    dev = GET_NEXT_PHYSDEV( dev, pSetDeviceClipping );
    return dev->funcs->pSetDeviceClipping( dev, clip );
}

static BOOL drmdrv_SetDeviceGammaRamp( PHYSDEV dev, void *ramp )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return FALSE;
}

static COLORREF drmdrv_SetPixel( PHYSDEV dev, INT x, INT y, COLORREF color )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return color;
}

static BOOL drmdrv_StretchBlt( PHYSDEV dst_dev, struct bitblt_coords *dst, PHYSDEV src_dev,
                               struct bitblt_coords *src, DWORD rop )
{
    struct device *device = device_from_physdev( dst_dev );
    struct cmd_stretch_blt *cmd;

    TRACE( "device %p, dst %s, src_dev %s, src %s, rop %#x\n", device, dbgstr_coords( dst ),
           src_dev->funcs->name, dbgstr_coords( src ), (UINT)rop );

    device_set_source_cmd_from_dev( device, src_dev, src );
    if ((cmd = device_start_cmd( device, DWM_OP_STRETCH_BLT, sizeof(*cmd) )))
    {
        cmd->dst_rect = dst->visrect;
        cmd->rop = rop;
        device_end_cmd( device, cmd );
    }

    dst_dev = GET_NEXT_PHYSDEV( dst_dev, pStretchBlt );
    return dst_dev->funcs->pStretchBlt( dst_dev, dst, src_dev, src, rop );
}

static BOOL drmdrv_StrokeAndFillPath( PHYSDEV dev )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static BOOL drmdrv_StrokePath( PHYSDEV dev )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static BOOL drmdrv_UnrealizePalette( HPALETTE palette )
{
    FIXME( "palette %p\n", palette );
    return FALSE;
}

const struct gdi_dc_funcs dwm_driver =
{
    /*.pAbortDoc = drmdrv_AbortDoc,*/
    /*.pAbortPath = drmdrv_AbortPath,*/
    .pAlphaBlend = drmdrv_AlphaBlend,
    .pAngleArc = drmdrv_AngleArc,
    .pArc = drmdrv_Arc,
    .pArcTo = drmdrv_ArcTo,
    /*.pBeginPath = drmdrv_BeginPath,*/
    .pBlendImage = drmdrv_BlendImage,
    .pChord = drmdrv_Chord,
    /*.pCloseFigure = drmdrv_CloseFigure,*/
    /*.pCreateCompatibleDC = drmdrv_CreateCompatibleDC,*/
    .pCreateDC = drmdrv_CreateDC,
    .pDeleteDC = drmdrv_DeleteDC,
    .pDeleteObject = drmdrv_DeleteObject,
    .pEllipse = drmdrv_Ellipse,
    /*.pEndDoc = drmdrv_EndDoc,*/
    /*.pEndPage = drmdrv_EndPage,*/
    /*.pEndPath = drmdrv_EndPath,*/
    /*.pEnumFonts = drmdrv_EnumFonts,*/
    .pExtEscape = drmdrv_ExtEscape,
    .pExtFloodFill = drmdrv_ExtFloodFill,
    .pExtTextOut = drmdrv_ExtTextOut,
    .pFillPath = drmdrv_FillPath,
    .pFillRgn = drmdrv_FillRgn,
    /*.pFontIsLinked = drmdrv_FontIsLinked,*/
    .pFrameRgn = drmdrv_FrameRgn,
    .pGetBoundsRect = drmdrv_GetBoundsRect,
    /*.pGetCharABCWidths = drmdrv_GetCharABCWidths,*/
    /*.pGetCharABCWidthsI = drmdrv_GetCharABCWidthsI,*/
    /*.pGetCharWidth = drmdrv_GetCharWidth,*/
    /*.pGetCharWidthInfo = drmdrv_GetCharWidthInfo,*/
    .pGetDeviceCaps = drmdrv_GetDeviceCaps,
    .pGetDeviceGammaRamp = drmdrv_GetDeviceGammaRamp,
    /*.pGetFontData = drmdrv_GetFontData,*/
    /*.pGetFontRealizationInfo = drmdrv_GetFontRealizationInfo,*/
    /*.pGetFontUnicodeRanges = drmdrv_GetFontUnicodeRanges,*/
    /*.pGetGlyphIndices = drmdrv_GetGlyphIndices,*/
    /*.pGetGlyphOutline = drmdrv_GetGlyphOutline,*/
    .pGetICMProfile = drmdrv_GetICMProfile,
    .pGetImage = drmdrv_GetImage,
    /*.pGetKerningPairs = drmdrv_GetKerningPairs,*/
    .pGetNearestColor = drmdrv_GetNearestColor,
    /*.pGetOutlineTextMetrics = drmdrv_GetOutlineTextMetrics,*/
    .pGetPixel = drmdrv_GetPixel,
    .pGetSystemPaletteEntries = drmdrv_GetSystemPaletteEntries,
    /*.pGetTextCharsetInfo = drmdrv_GetTextCharsetInfo,*/
    /*.pGetTextExtentExPoint = drmdrv_GetTextExtentExPoint,*/
    /*.pGetTextExtentExPointI = drmdrv_GetTextExtentExPointI,*/
    /*.pGetTextFace = drmdrv_GetTextFace,*/
    /*.pGetTextMetrics = drmdrv_GetTextMetrics,*/
    .pGradientFill = drmdrv_GradientFill,
    .pInvertRgn = drmdrv_InvertRgn,
    .pLineTo = drmdrv_LineTo,
    .pMoveTo = drmdrv_MoveTo,
    .pPaintRgn = drmdrv_PaintRgn,
    .pPatBlt = drmdrv_PatBlt,
    .pPie = drmdrv_Pie,
    .pPolyBezier = drmdrv_PolyBezier,
    .pPolyBezierTo = drmdrv_PolyBezierTo,
    .pPolyDraw = drmdrv_PolyDraw,
    .pPolyPolygon = drmdrv_PolyPolygon,
    .pPolyPolyline = drmdrv_PolyPolyline,
    .pPolylineTo = drmdrv_PolylineTo,
    .pPutImage = drmdrv_PutImage,
    .pRealizeDefaultPalette = drmdrv_RealizeDefaultPalette,
    .pRealizePalette = drmdrv_RealizePalette,
    .pRectangle = drmdrv_Rectangle,
    /*.pResetDC = drmdrv_ResetDC,*/
    .pRoundRect = drmdrv_RoundRect,
    /*.pSelectBitmap = drmdrv_SelectBitmap,*/
    .pSelectBrush = drmdrv_SelectBrush,
    .pSelectFont = drmdrv_SelectFont,
    .pSelectPen = drmdrv_SelectPen,
    /*.pSetBkColor = drmdrv_SetBkColor,*/
    .pSetBoundsRect = drmdrv_SetBoundsRect,
    .pSetDCBrushColor = drmdrv_SetDCBrushColor,
    .pSetDCPenColor = drmdrv_SetDCPenColor,
    .pSetDIBitsToDevice = drmdrv_SetDIBitsToDevice,
    .pSetDeviceClipping = drmdrv_SetDeviceClipping,
    .pSetDeviceGammaRamp = drmdrv_SetDeviceGammaRamp,
    .pSetPixel = drmdrv_SetPixel,
    /*.pSetTextColor = drmdrv_SetTextColor,*/
    /*.pStartDoc = drmdrv_StartDoc,*/
    /*.pStartPage = drmdrv_StartPage,*/
    .pStretchBlt = drmdrv_StretchBlt,
    /*.pStretchDIBits = drmdrv_StretchDIBits,*/
    .pStrokeAndFillPath = drmdrv_StrokeAndFillPath,
    .pStrokePath = drmdrv_StrokePath,
    .pUnrealizePalette = drmdrv_UnrealizePalette,

    .priority = GDI_PRIORITY_DIB_DRV + 50,
    .name = "dwmdrv",
};
