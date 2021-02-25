/*
 * Copyright 2020 Rémi Bernon for CodeWeavers
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

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winnt.h"
#include "winuser.h"
#include "wingdi.h"

#include "wine/gdi_driver.h"
#include "wine/debug.h"

#include "unixlib.h"
#include "win32u.h"

WINE_DEFAULT_DEBUG_CHANNEL(win32u);

struct unix_funcs *unix_funcs;
static const struct gdi_dc_funcs gdi_dc_funcs;

void CDECL X11DRV_WindowPosChanging( HWND hwnd, HWND insert_after, UINT swp_flags,
                                     const RECT *window_rect, const RECT *client_rect,
                                     RECT *visible_rect, struct window_surface **surface );

void CDECL win32u_WindowPosChanging( HWND hwnd, HWND insert_after, UINT swp_flags,
                                     const RECT *window_rect, const RECT *client_rect,
                                     RECT *visible_rect, struct window_surface **surface )
{
    struct window_surface *x11drv_surface = NULL;
    HWND parent;

    TRACE( "hwnd %p, insert_after %p, swp_flags %x, window_rect %s, client_rect %s, "
           "visible_rect %p, surface %p.\n", hwnd, insert_after, swp_flags,
           wine_dbgstr_rect( window_rect ), wine_dbgstr_rect( client_rect ),
           visible_rect, surface );

    /* create a unix / window surface for top-level windows */
    if ((parent = GetAncestor( hwnd, GA_PARENT )) && parent == GetDesktopWindow())
        win32u_create_toplevel_surface( hwnd );
    else
        win32u_delete_toplevel_surface( hwnd );

    X11DRV_WindowPosChanging( hwnd, insert_after, swp_flags, window_rect, client_rect,
                              visible_rect, &x11drv_surface );
    if (x11drv_surface) window_surface_release( x11drv_surface );

    win32u_update_window_surface( hwnd, visible_rect, surface );
}

void CDECL X11DRV_WindowPosChanged( HWND hwnd, HWND insert_after, UINT swp_flags,
                                    const RECT *window_rect, const RECT *client_rect,
                                    const RECT *visible_rect, const RECT *valid_rects,
                                    struct window_surface *surface );

void CDECL win32u_WindowPosChanged( HWND hwnd, HWND insert_after, UINT swp_flags,
                                    const RECT *window_rect, const RECT *client_rect,
                                    const RECT *visible_rect, const RECT *valid_rects,
                                    struct window_surface *surface )
{
    TRACE( "hwnd %p, insert_after %p, swp_flags %x, window_rect %s, client_rect %s, "
           "visible_rect %s, valid_rects %s, surface %p.\n", hwnd, insert_after, swp_flags,
           wine_dbgstr_rect( window_rect ), wine_dbgstr_rect( client_rect ),
           wine_dbgstr_rect( visible_rect ), wine_dbgstr_rect( valid_rects ), surface );

    X11DRV_WindowPosChanged( hwnd, insert_after, swp_flags, window_rect, client_rect,
                             visible_rect, valid_rects, NULL );

    win32u_resize_hwnd_surfaces( hwnd );
}

void CDECL X11DRV_SetParent( HWND hwnd, HWND parent, HWND old_parent );

void CDECL win32u_SetParent( HWND hwnd, HWND parent, HWND old_parent )
{
    TRACE( "hwnd %p, parent %p, old_parent %p.\n", hwnd, parent, old_parent );

    /* create a unix / window surface for top-level windows */
    if (parent == GetDesktopWindow())
        win32u_create_toplevel_surface( hwnd );
    else
        win32u_delete_toplevel_surface( hwnd );

    X11DRV_SetParent( hwnd, parent, old_parent );
}

void CDECL X11DRV_DestroyWindow( HWND hwnd );

void CDECL win32u_DestroyWindow( HWND hwnd )
{
    TRACE( "hwnd %p.\n", hwnd );

    win32u_delete_toplevel_surface( hwnd );

    X11DRV_DestroyWindow( hwnd );
}

LRESULT CDECL X11DRV_WindowMessage( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp );

LRESULT CDECL win32u_WindowMessage( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    TRACE( "hwnd %p, msg %x, wp %lx, lp %lx.\n", hwnd, msg, wp, lp );

    switch (msg)
    {
    case WM_X11DRV_NOTIFY_HWND_SURFACE_CREATED:
        if (wp) win32u_create_toplevel_surface_notify( hwnd, lp );
        else win32u_create_client_surface_notify( hwnd, lp );
        break;
    case WM_X11DRV_NOTIFY_HWND_SURFACE_RESIZE:
        win32u_resize_hwnd_surfaces_notify( hwnd, (BOOL)lp );
        break;
    case WM_X11DRV_NOTIFY_HWND_SURFACE_REPARENT:
        win32u_reparent_hwnd_surfaces_notify( hwnd, (BOOL)lp );
        break;
    }

    return X11DRV_WindowMessage( hwnd, msg, wp, lp );
}

#define WIN32U_ESCAPE 6789

void CDECL win32u_GetDC(HDC hdc, HWND hwnd, HWND top, const RECT *win_rect,
                        const RECT *top_rect, DWORD flags)
{
    TRACE( "hdc %p, hwnd %p, top %p, win_rect %s, top_rect %s, flags %#x.\n",
           hdc, hwnd, top, wine_dbgstr_rect( win_rect ), wine_dbgstr_rect( top_rect ), flags );
}

void CDECL win32u_ReleaseDC( HWND hwnd, HDC hdc )
{
    TRACE( "hwnd %p, hdc %p.\n", hwnd, hdc );
}

static BOOL CDECL win32u_CreateCompatibleDC(PHYSDEV orig, PHYSDEV *dev)
{
    struct gdi_physdev *phys_dev;

    TRACE( "orig %p, dev %p.\n", orig, dev );

    if (!(phys_dev = malloc(sizeof(*phys_dev)))) return FALSE;
    push_dc_driver( dev, phys_dev, &gdi_dc_funcs );
    return TRUE;
}

static BOOL CDECL win32u_CreateDC(PHYSDEV *dev, const WCHAR *driver, const WCHAR *device, const WCHAR *output,
                                 const DEVMODEW* init_data)
{
    struct gdi_physdev *phys_dev;

    TRACE( "dev %p, driver %s, device %s, output %s, init_data %p.\n",
           dev, debugstr_w( driver ), debugstr_w( device ), debugstr_w( output ), init_data );

    if (!(phys_dev = malloc(sizeof(*phys_dev)))) return FALSE;
    push_dc_driver( dev, phys_dev, &gdi_dc_funcs );
    return TRUE;
}

static BOOL CDECL win32u_DeleteDC(PHYSDEV dev)
{
    TRACE( "dev %p.\n", dev );

    free(dev);

    return TRUE;
}

static INT CDECL win32u_ExtEscape(PHYSDEV dev, INT escape, INT in_count, const void *in_data,
                                  INT out_count, LPVOID out_data)
{
    const enum x11drv_escape_codes *code = in_data;
    HWND hwnd = WindowFromDC( dev->hdc );

    TRACE( "dev %p, escape %d, in_count %d, in_data %p, out_count %d, out_data %p.\n",
           dev, escape, in_count, in_data, out_count, out_data );

    if (escape != WIN32U_ESCAPE) return 0;
    if (!in_data || in_count < sizeof(*code)) return -1;

    switch (*code)
    {
    case X11DRV_FLUSH_VK_DRAWABLE:
    case X11DRV_FLUSH_GL_DRAWABLE:
        win32u_present_client_surface( hwnd, dev->clip_region );
        return 1;
    default:
        FIXME("%x\n", *code);
        break;
    }

    return 0;
}

static void CDECL win32u_SetDeviceClipping( PHYSDEV dev, HRGN rgn )
{
    TRACE( "dev %p, rgn %p.\n", dev, rgn );
    dev->clip_region = rgn;
}

extern NTSTATUS CDECL X11DRV_D3DKMTCheckVidPnExclusiveOwnership( const D3DKMT_CHECKVIDPNEXCLUSIVEOWNERSHIP *desc ) DECLSPEC_HIDDEN;
extern NTSTATUS CDECL X11DRV_D3DKMTSetVidPnSourceOwner( const D3DKMT_SETVIDPNSOURCEOWNER *desc ) DECLSPEC_HIDDEN;

struct opengl_funcs * CDECL X11DRV_wine_get_wgl_driver( PHYSDEV dev, UINT version ) DECLSPEC_HIDDEN;

static struct opengl_funcs * CDECL win32u_wine_get_wgl_driver( PHYSDEV dev, UINT version )
{
    TRACE( "dev %p, version %u.\n", dev, version );
    return X11DRV_wine_get_wgl_driver( dev, version );
}

const struct vulkan_funcs * CDECL X11DRV_wine_get_vulkan_driver( PHYSDEV dev, UINT version ) DECLSPEC_HIDDEN;

static const struct vulkan_funcs * CDECL win32u_wine_get_vulkan_driver( PHYSDEV dev, UINT version )
{
    TRACE( "dev %p, version %u.\n", dev, version );
    return X11DRV_wine_get_vulkan_driver( dev, version );
}

static const struct gdi_dc_funcs gdi_dc_funcs =
{
    NULL,                               /* pAbortDoc */
    NULL,                               /* pAbortPath */
    NULL,                               /* pAlphaBlend */
    NULL,                               /* pAngleArc */
    NULL /* X11DRV_Arc */,                         /* pArc */
    NULL,                               /* pArcTo */
    NULL,                               /* pBeginPath */
    NULL,                               /* pBlendImage */
    NULL /* X11DRV_Chord */,                       /* pChord */
    NULL,                               /* pCloseFigure */
    win32u_CreateCompatibleDC /* X11DRV_CreateCompatibleDC */,          /* pCreateCompatibleDC */
    win32u_CreateDC /* X11DRV_CreateDC */,                    /* pCreateDC */
    win32u_DeleteDC /* X11DRV_DeleteDC */,                    /* pDeleteDC */
    NULL,                               /* pDeleteObject */
    NULL,                               /* pDeviceCapabilities */
    NULL /* X11DRV_Ellipse */,                     /* pEllipse */
    NULL,                               /* pEndDoc */
    NULL,                               /* pEndPage */
    NULL,                               /* pEndPath */
    NULL,                               /* pEnumFonts */
    NULL /* X11DRV_EnumICMProfiles */,             /* pEnumICMProfiles */
    NULL,                               /* pExcludeClipRect */
    NULL,                               /* pExtDeviceMode */
    win32u_ExtEscape,                   /* pExtEscape */
    NULL /* X11DRV_ExtFloodFill */,                /* pExtFloodFill */
    NULL,                               /* pExtSelectClipRgn */
    NULL,                               /* pExtTextOut */
    NULL /* X11DRV_FillPath */,                    /* pFillPath */
    NULL,                               /* pFillRgn */
    NULL,                               /* pFlattenPath */
    NULL,                               /* pFontIsLinked */
    NULL,                               /* pFrameRgn */
    NULL,                               /* pGdiComment */
    NULL,                               /* pGetBoundsRect */
    NULL,                               /* pGetCharABCWidths */
    NULL,                               /* pGetCharABCWidthsI */
    NULL,                               /* pGetCharWidth */
    NULL,                               /* pGetCharWidthInfo */
    NULL /* X11DRV_GetDeviceCaps */,               /* pGetDeviceCaps */
    NULL /* X11DRV_GetDeviceGammaRamp */,          /* pGetDeviceGammaRamp */
    NULL,                               /* pGetFontData */
    NULL,                               /* pGetFontRealizationInfo */
    NULL,                               /* pGetFontUnicodeRanges */
    NULL,                               /* pGetGlyphIndices */
    NULL,                               /* pGetGlyphOutline */
    NULL /* X11DRV_GetICMProfile */,               /* pGetICMProfile */
    NULL /* X11DRV_GetImage */,                    /* pGetImage */
    NULL,                               /* pGetKerningPairs */
    NULL /* X11DRV_GetNearestColor */,             /* pGetNearestColor */
    NULL,                               /* pGetOutlineTextMetrics */
    NULL,                               /* pGetPixel */
    NULL /* X11DRV_GetSystemPaletteEntries */,     /* pGetSystemPaletteEntries */
    NULL,                               /* pGetTextCharsetInfo */
    NULL,                               /* pGetTextExtentExPoint */
    NULL,                               /* pGetTextExtentExPointI */
    NULL,                               /* pGetTextFace */
    NULL,                               /* pGetTextMetrics */
    NULL /* X11DRV_GradientFill */,                /* pGradientFill */
    NULL,                               /* pIntersectClipRect */
    NULL,                               /* pInvertRgn */
    NULL /* X11DRV_LineTo */,                      /* pLineTo */
    NULL,                               /* pModifyWorldTransform */
    NULL,                               /* pMoveTo */
    NULL,                               /* pOffsetClipRgn */
    NULL,                               /* pOffsetViewportOrg */
    NULL,                               /* pOffsetWindowOrg */
    NULL /* X11DRV_PaintRgn */,                    /* pPaintRgn */
    NULL /* X11DRV_PatBlt */,                      /* pPatBlt */
    NULL /* X11DRV_Pie */,                         /* pPie */
    NULL,                               /* pPolyBezier */
    NULL,                               /* pPolyBezierTo */
    NULL,                               /* pPolyDraw */
    NULL /* X11DRV_PolyPolygon */,                 /* pPolyPolygon */
    NULL /* X11DRV_PolyPolyline */,                /* pPolyPolyline */
    NULL /* X11DRV_Polygon */,                     /* pPolygon */
    NULL,                               /* pPolyline */
    NULL,                               /* pPolylineTo */
    NULL /* X11DRV_PutImage */,                    /* pPutImage */
    NULL /* X11DRV_RealizeDefaultPalette */,       /* pRealizeDefaultPalette */
    NULL /* X11DRV_RealizePalette */,              /* pRealizePalette */
    NULL /* X11DRV_Rectangle */,                   /* pRectangle */
    NULL,                               /* pResetDC */
    NULL,                               /* pRestoreDC */
    NULL /* X11DRV_RoundRect */,                   /* pRoundRect */
    NULL,                               /* pSaveDC */
    NULL,                               /* pScaleViewportExt */
    NULL,                               /* pScaleWindowExt */
    NULL,                               /* pSelectBitmap */
    NULL /* X11DRV_SelectBrush */,      /* pSelectBrush */
    NULL,                               /* pSelectClipPath */
    NULL /* X11DRV_SelectFont */,       /* pSelectFont */
    NULL,                               /* pSelectPalette */
    NULL /* X11DRV_SelectPen */,        /* pSelectPen */
    NULL,                               /* pSetArcDirection */
    NULL,                               /* pSetBkColor */
    NULL,                               /* pSetBkMode */
    NULL /* X11DRV_SetBoundsRect */,    /* pSetBoundsRect */
    NULL /* X11DRV_SetDCBrushColor */,             /* pSetDCBrushColor */
    NULL /* X11DRV_SetDCPenColor */,               /* pSetDCPenColor */
    NULL,                               /* pSetDIBitsToDevice */
    win32u_SetDeviceClipping,           /* pSetDeviceClipping */
    NULL /* X11DRV_SetDeviceGammaRamp */,          /* pSetDeviceGammaRamp */
    NULL,                               /* pSetLayout */
    NULL,                               /* pSetMapMode */
    NULL,                               /* pSetMapperFlags */
    NULL /* X11DRV_SetPixel */,                    /* pSetPixel */
    NULL,                               /* pSetPolyFillMode */
    NULL,                               /* pSetROP2 */
    NULL,                               /* pSetRelAbs */
    NULL,                               /* pSetStretchBltMode */
    NULL,                               /* pSetTextAlign */
    NULL,                               /* pSetTextCharacterExtra */
    NULL,                               /* pSetTextColor */
    NULL,                               /* pSetTextJustification */
    NULL,                               /* pSetViewportExt */
    NULL,                               /* pSetViewportOrg */
    NULL,                               /* pSetWindowExt */
    NULL,                               /* pSetWindowOrg */
    NULL,                               /* pSetWorldTransform */
    NULL,                               /* pStartDoc */
    NULL,                               /* pStartPage */
    NULL /* X11DRV_StretchBlt */,                  /* pStretchBlt */
    NULL,                               /* pStretchDIBits */
    NULL /* X11DRV_StrokeAndFillPath */,           /* pStrokeAndFillPath */
    NULL /* X11DRV_StrokePath */,                  /* pStrokePath */
    NULL /* X11DRV_UnrealizePalette */,            /* pUnrealizePalette */
    NULL,                               /* pWidenPath */
    X11DRV_D3DKMTCheckVidPnExclusiveOwnership, /* pD3DKMTCheckVidPnExclusiveOwnership */
    X11DRV_D3DKMTSetVidPnSourceOwner,   /* pD3DKMTSetVidPnSourceOwner */
    win32u_wine_get_wgl_driver /* X11DRV_wine_get_wgl_driver */,         /* wine_get_wgl_driver */
    win32u_wine_get_vulkan_driver /* X11DRV_wine_get_vulkan_driver */,      /* wine_get_vulkan_driver */
    GDI_PRIORITY_GRAPHICS_DRV           /* priority */
};

/******************************************************************************
 *      win32u_get_gdi_driver
 */
const struct gdi_dc_funcs *CDECL win32u_get_gdi_driver( unsigned int version )
{
    TRACE( "version %u.\n", version );

    if (version != WINE_GDI_DRIVER_VERSION)
    {
        ERR( "version mismatch, gdi32 wants %u but winex11 has %u\n", version, WINE_GDI_DRIVER_VERSION );
        return NULL;
    }
    return &gdi_dc_funcs;
}

BOOL WINAPI DllMain( HINSTANCE instance, DWORD reason, LPVOID reserved )
{
    TRACE( "instance %p, reason %d, reserved %p.\n", instance, reason, reserved );

    switch (reason)
    {
    case DLL_PROCESS_ATTACH: DisableThreadLibraryCalls( instance ); break;
    case DLL_PROCESS_DETACH: break;
    }

    if (__wine_init_unix_lib( instance, reason, NULL, &unix_funcs )) return FALSE;
    return TRUE;
}
