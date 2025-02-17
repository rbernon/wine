/*
 * Enhanced MetaFile driver
 *
 * Copyright 1999 Huw D M Davies
 * Copyright 2021 Jacek Caban for CodeWeavers
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

#include "ntgdi_private.h"


typedef struct
{
    struct gdi_physdev dev;
    INT dev_caps[COLORMGMTCAPS + 1];
} EMFDRV_PDEVICE;

static inline EMFDRV_PDEVICE *get_emf_physdev( PHYSDEV dev )
{
    return CONTAINING_RECORD( dev, EMFDRV_PDEVICE, dev );
}

static HBITMAP EMFDC_SelectBitmap( PHYSDEV dev, HBITMAP hbitmap )
{
    return 0;
}

static HFONT EMFDC_SelectFont( PHYSDEV dev, HFONT font, UINT *aa_flags )
{
    *aa_flags = GGO_BITMAP;  /* no point in anti-aliasing on metafiles */

    dev = GET_NEXT_PHYSDEV( dev, pSelectFont );
    return dev->funcs->pSelectFont( dev, font, aa_flags );
}

static INT EMFDC_GetDeviceCaps( PHYSDEV dev, INT cap )
{
    EMFDRV_PDEVICE *physDev = get_emf_physdev( dev );

    if (cap >= 0 && cap < ARRAY_SIZE( physDev->dev_caps ))
        return physDev->dev_caps[cap];
    return 0;
}

static BOOL EMFDC_DeleteDC( PHYSDEV dev )
{
    EMFDRV_PDEVICE *physDev = get_emf_physdev( dev );
    free( physDev );
    return TRUE;
}

static const struct gdi_dc_funcs emfdrv_driver =
{
    NULL,                            /* pAbortDoc */
    NULL,                            /* pAbortPath */
    NULL,               /* pAlphaBlend */
    NULL,                            /* pAngleArc */
    NULL,                      /* pArc */
    NULL,                    /* pArcTo */
    NULL,                            /* pBeginPath */
    NULL,                            /* pBlendImage */
    NULL,                    /* pChord */
    NULL,                            /* pCloseFigure */
    NULL,                            /* pCreateCompatibleDC */
    NULL,                            /* pCreateDC */
    EMFDC_DeleteDC,                 /* pDeleteDC */
    NULL,                            /* pDeleteObject */
    NULL,                  /* pEllipse */
    NULL,                            /* pEndDoc */
    NULL,                            /* pEndPage */
    NULL,                            /* pEndPath */
    NULL,                            /* pEnumFonts */
    NULL,                            /* pExtEscape */
    NULL,                            /* pExtFloodFill */
    NULL,               /* pExtTextOut */
    NULL,                 /* pFillPath */
    NULL,                  /* pFillRgn */
    NULL,                            /* pFontIsLinked */
    NULL,                 /* pFrameRgn */
    NULL,                            /* pGetBoundsRect */
    NULL,                            /* pGetCharABCWidths */
    NULL,                            /* pGetCharABCWidthsI */
    NULL,                            /* pGetCharWidth */
    NULL,                            /* pGetCharWidthInfo */
    EMFDC_GetDeviceCaps,            /* pGetDeviceCaps */
    NULL,                            /* pGetDeviceGammaRamp */
    NULL,                            /* pGetFontData */
    NULL,                            /* pGetFontRealizationInfo */
    NULL,                            /* pGetFontUnicodeRanges */
    NULL,                            /* pGetGlyphIndices */
    NULL,                            /* pGetGlyphOutline */
    NULL,                            /* pGetICMProfile */
    NULL,                            /* pGetImage */
    NULL,                            /* pGetKerningPairs */
    NULL,                            /* pGetNearestColor */
    NULL,                            /* pGetOutlineTextMetrics */
    NULL,                            /* pGetPixel */
    NULL,                            /* pGetSystemPaletteEntries */
    NULL,                            /* pGetTextCharsetInfo */
    NULL,                            /* pGetTextExtentExPoint */
    NULL,                            /* pGetTextExtentExPointI */
    NULL,                            /* pGetTextFace */
    NULL,                            /* pGetTextMetrics */
    NULL,             /* pGradientFill */
    NULL,                /* pInvertRgn */
    NULL,                   /* pLineTo */
    NULL,                            /* pMoveTo */
    NULL,                            /* pPaintRgn */
    NULL,                   /* pPatBlt */
    NULL,                      /* pPie */
    NULL,               /* pPolyBezier */
    NULL,             /* pPolyBezierTo */
    NULL,                 /* pPolyDraw */
    NULL,              /* pPolyPolygon */
    NULL,             /* pPolyPolyline */
    NULL,               /* pPolylineTo */
    NULL,                            /* pPutImage */
    NULL,                            /* pRealizeDefaultPalette */
    NULL,                            /* pRealizePalette */
    NULL,                            /* pRectangle */
    NULL,                            /* pResetDC */
    NULL,                            /* pRoundRect */
    EMFDC_SelectBitmap,             /* pSelectBitmap */
    NULL,                            /* pSelectBrush */
    EMFDC_SelectFont,               /* pSelectFont */
    NULL,                            /* pSelectPen */
    NULL,                            /* pSetBkColor */
    NULL,                            /* pSetBoundsRect */
    NULL,                            /* pSetDCBrushColor*/
    NULL,                            /* pSetDCPenColor*/
    NULL,                            /* pSetDIBitsToDevice */
    NULL,                            /* pSetDeviceClipping */
    NULL,                            /* pSetDeviceGammaRamp */
    NULL,                            /* pSetPixel */
    NULL,                            /* pSetTextColor */
    NULL,                            /* pStartDoc */
    NULL,                            /* pStartPage */
    NULL,                            /* pStretchBlt */
    NULL,                            /* pStretchDIBits */
    NULL,                            /* pStrokeAndFillPath */
    NULL,                            /* pStrokePath */
    NULL,                            /* pUnrealizePalette */
    GDI_PRIORITY_GRAPHICS_DRV        /* priority */
};


static BOOL devcap_is_valid( int cap )
{
    if (cap >= 0 && cap <= ASPECTXY) return !(cap & 1);
    if (cap >= PHYSICALWIDTH && cap <= COLORMGMTCAPS) return TRUE;
    switch (cap)
    {
    case LOGPIXELSX:
    case LOGPIXELSY:
    case CAPS1:
    case SIZEPALETTE:
    case NUMRESERVED:
    case COLORRES:
        return TRUE;
    }
    return FALSE;
}

/**********************************************************************
 *           NtGdiCreateMetafileDC   (win32u.@)
 */
HDC WINAPI NtGdiCreateMetafileDC( HDC hdc )
{
    EMFDRV_PDEVICE *physDev;
    HDC ref_dc, ret;
    int cap;
    DC *dc;

    if (!(dc = alloc_dc_ptr( NTGDI_OBJ_ENHMETADC ))) return 0;

    physDev = malloc( sizeof(*physDev) );
    if (!physDev)
    {
        free_dc_ptr( dc );
        return 0;
    }

    push_dc_driver( &dc->physDev, &physDev->dev, &emfdrv_driver );

    if (hdc)  /* if no ref, use current display */
        ref_dc = hdc;
    else
        ref_dc = NtGdiOpenDCW( NULL, NULL, NULL, 0, TRUE, NULL, NULL, NULL );

    memset( physDev->dev_caps, 0, sizeof(physDev->dev_caps) );
    for (cap = 0; cap < ARRAY_SIZE( physDev->dev_caps ); cap++)
        if (devcap_is_valid( cap ))
            physDev->dev_caps[cap] = NtGdiGetDeviceCaps( ref_dc, cap );

    if (!hdc) NtGdiDeleteObjectApp( ref_dc );

    NtGdiSetVirtualResolution( dc->hSelf, 0, 0, 0, 0 );

    ret = dc->hSelf;
    release_dc_ptr( dc );
    return ret;
}
