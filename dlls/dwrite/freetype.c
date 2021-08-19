/*
 *    FreeType integration
 *
 * Copyright 2014-2017 Nikolay Sivov for CodeWeavers
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

#include <ft2build.h>
#include FT_CACHE_H
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_TRUETYPE_TABLES_H

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "wine/debug.h"

#include "dwrite_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(dwrite);

static RTL_CRITICAL_SECTION freetype_cs;
static RTL_CRITICAL_SECTION_DEBUG critsect_debug =
{
    0, 0, &freetype_cs,
    { &critsect_debug.ProcessLocksList, &critsect_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": freetype_cs") }
};
static RTL_CRITICAL_SECTION freetype_cs = { &critsect_debug, -1, 0, 0, 0, 0 };

static FT_Library library = 0;
static FTC_Manager cache_manager = 0;
static FTC_ImageCache image_cache = 0;
typedef struct
{
    FT_Int major;
    FT_Int minor;
    FT_Int patch;
} FT_Version_t;

static void face_finalizer(void *object)
{
    FT_Face face = object;
    freetype_release_font_data((struct font_data_context *)face->generic.data);
}

static FT_Error face_requester(FTC_FaceID face_id, FT_Library library, FT_Pointer request_data, FT_Face *face)
{
    struct font_data_context *context;
    const void *data_ptr;
    FT_Error fterror;
    UINT64 data_size;
    UINT32 index;

    *face = NULL;

    if (!face_id)
    {
        WARN("NULL fontface requested.\n");
        return FT_Err_Ok;
    }

    if (freetype_get_font_data(face_id, &data_ptr, &data_size, &index, &context))
        return FT_Err_Ok;

    fterror = FT_New_Memory_Face(library, data_ptr, data_size, index, face);
    if (fterror == FT_Err_Ok)
    {
        (*face)->generic.data = context;
        (*face)->generic.finalizer = face_finalizer;
    }
    else
        freetype_release_font_data(context);

    return fterror;
}

void init_font_backend(void)
{
    FT_Version_t FT_Version;

    if (FT_Init_FreeType(&library) != 0) {
        ERR("Can't init FreeType library\n");
	   return;
    }
    FT_Library_Version(library, &FT_Version.major, &FT_Version.minor, &FT_Version.patch);

    /* init cache manager */
    if (FTC_Manager_New(library, 0, 0, 0, &face_requester, NULL, &cache_manager) != 0 ||
        FTC_ImageCache_New(cache_manager, &image_cache) != 0) {

        ERR("Failed to init FreeType cache\n");
        FTC_Manager_Done(cache_manager);
        FT_Done_FreeType(library);
        return;
    }

    TRACE("FreeType version is %d.%d.%d\n", FT_Version.major, FT_Version.minor, FT_Version.patch);
}

void freetype_notify_release(void *key)
{
    RtlEnterCriticalSection(&freetype_cs);
    FTC_Manager_RemoveFaceID(cache_manager, key);
    RtlLeaveCriticalSection(&freetype_cs);
}

void freetype_get_design_glyph_metrics(void *key, UINT16 upem, UINT16 ascent,
        unsigned int simulations, UINT16 glyph, DWRITE_GLYPH_METRICS *ret)
{
    FTC_ScalerRec scaler;
    FT_Size size;

    scaler.face_id = key;
    scaler.width = upem;
    scaler.height = upem;
    scaler.pixel = 1;
    scaler.x_res = 0;
    scaler.y_res = 0;

    RtlEnterCriticalSection(&freetype_cs);
    if (FTC_Manager_LookupSize(cache_manager, &scaler, &size) == 0) {
         if (FT_Load_Glyph(size->face, glyph, FT_LOAD_NO_SCALE) == 0) {
             FT_Glyph_Metrics *metrics = &size->face->glyph->metrics;

             ret->leftSideBearing = metrics->horiBearingX;
             ret->advanceWidth = metrics->horiAdvance;
             ret->rightSideBearing = metrics->horiAdvance - metrics->horiBearingX - metrics->width;

             ret->advanceHeight = metrics->vertAdvance;
             ret->verticalOriginY = ascent;
             ret->topSideBearing = ascent - metrics->horiBearingY;
             ret->bottomSideBearing = metrics->vertAdvance - metrics->height - ret->topSideBearing;

             /* Adjust in case of bold simulation, glyphs without contours are ignored. */
             if (simulations & DWRITE_FONT_SIMULATIONS_BOLD &&
                     size->face->glyph->format == FT_GLYPH_FORMAT_OUTLINE && size->face->glyph->outline.n_contours)
             {
                 if (ret->advanceWidth)
                     ret->advanceWidth += (upem + 49) / 50;
             }
         }
    }
    RtlLeaveCriticalSection(&freetype_cs);
}

struct decompose_context
{
    struct dwrite_outline *outline;
    BOOL figure_started;
    BOOL move_to;     /* last call was 'move_to' */
    FT_Vector origin; /* 'pen' position from last call */
};

static inline void ft_vector_to_d2d_point(const FT_Vector *v, D2D1_POINT_2F *p)
{
    p->x = v->x / 64.0f;
    p->y = v->y / 64.0f;
}

static int dwrite_outline_push_tag(struct dwrite_outline *outline, unsigned char tag)
{
    if (!dwrite_array_reserve((void **)&outline->tags.values, &outline->tags.size, outline->tags.count + 1,
            sizeof(*outline->tags.values)))
    {
        return 1;
    }

    outline->tags.values[outline->tags.count++] = tag;

    return 0;
}

static int dwrite_outline_push_points(struct dwrite_outline *outline, const D2D1_POINT_2F *points, unsigned int count)
{
    if (!dwrite_array_reserve((void **)&outline->points.values, &outline->points.size, outline->points.count + count,
            sizeof(*outline->points.values)))
    {
        return 1;
    }

    memcpy(&outline->points.values[outline->points.count], points, sizeof(*points) * count);
    outline->points.count += count;

    return 0;
}

static int decompose_beginfigure(struct decompose_context *ctxt)
{
    D2D1_POINT_2F point;
    int ret;

    if (!ctxt->move_to)
        return 0;

    ft_vector_to_d2d_point(&ctxt->origin, &point);
    if ((ret = dwrite_outline_push_tag(ctxt->outline, OUTLINE_BEGIN_FIGURE))) return ret;
    if ((ret = dwrite_outline_push_points(ctxt->outline, &point, 1))) return ret;

    ctxt->figure_started = TRUE;
    ctxt->move_to = FALSE;

    return 0;
}

static int decompose_move_to(const FT_Vector *to, void *user)
{
    struct decompose_context *ctxt = (struct decompose_context *)user;
    int ret;

    if (ctxt->figure_started)
    {
        if ((ret = dwrite_outline_push_tag(ctxt->outline, OUTLINE_END_FIGURE))) return ret;
        ctxt->figure_started = FALSE;
    }

    ctxt->move_to = TRUE;
    ctxt->origin = *to;
    return 0;
}

static int decompose_line_to(const FT_Vector *to, void *user)
{
    struct decompose_context *ctxt = (struct decompose_context *)user;
    D2D1_POINT_2F point;
    int ret;

    /* Special case for empty contours, in a way freetype returns them. */
    if (ctxt->move_to && !memcmp(to, &ctxt->origin, sizeof(*to)))
        return 0;

    ft_vector_to_d2d_point(to, &point);

    if ((ret = decompose_beginfigure(ctxt))) return ret;
    if ((ret = dwrite_outline_push_points(ctxt->outline, &point, 1))) return ret;
    if ((ret = dwrite_outline_push_tag(ctxt->outline, OUTLINE_LINE))) return ret;

    ctxt->origin = *to;
    return 0;
}

static int decompose_conic_to(const FT_Vector *control, const FT_Vector *to, void *user)
{
    struct decompose_context *ctxt = (struct decompose_context *)user;
    D2D1_POINT_2F points[3];
    FT_Vector cubic[3];
    int ret;

    if ((ret = decompose_beginfigure(ctxt)))
        return ret;

    /* convert from quadratic to cubic */

    /*
       The parametric eqn for a cubic Bezier is, from PLRM:
       r(t) = at^3 + bt^2 + ct + r0
       with the control points:
       r1 = r0 + c/3
       r2 = r1 + (c + b)/3
       r3 = r0 + c + b + a

       A quadratic Bezier has the form:
       p(t) = (1-t)^2 p0 + 2(1-t)t p1 + t^2 p2

       So equating powers of t leads to:
       r1 = 2/3 p1 + 1/3 p0
       r2 = 2/3 p1 + 1/3 p2
       and of course r0 = p0, r3 = p2
    */

    /* r1 = 1/3 p0 + 2/3 p1
       r2 = 1/3 p2 + 2/3 p1 */
    cubic[0].x = (2 * control->x + 1) / 3;
    cubic[0].y = (2 * control->y + 1) / 3;
    cubic[1] = cubic[0];
    cubic[0].x += (ctxt->origin.x + 1) / 3;
    cubic[0].y += (ctxt->origin.y + 1) / 3;
    cubic[1].x += (to->x + 1) / 3;
    cubic[1].y += (to->y + 1) / 3;
    cubic[2] = *to;

    ft_vector_to_d2d_point(cubic, points);
    ft_vector_to_d2d_point(cubic + 1, points + 1);
    ft_vector_to_d2d_point(cubic + 2, points + 2);
    if ((ret = dwrite_outline_push_points(ctxt->outline, points, 3))) return ret;
    if ((ret = dwrite_outline_push_tag(ctxt->outline, OUTLINE_BEZIER))) return ret;
    ctxt->origin = *to;
    return 0;
}

static int decompose_cubic_to(const FT_Vector *control1, const FT_Vector *control2,
    const FT_Vector *to, void *user)
{
    struct decompose_context *ctxt = (struct decompose_context *)user;
    D2D1_POINT_2F points[3];
    int ret;

    if ((ret = decompose_beginfigure(ctxt)))
        return ret;

    ft_vector_to_d2d_point(control1, points);
    ft_vector_to_d2d_point(control2, points + 1);
    ft_vector_to_d2d_point(to, points + 2);
    ctxt->origin = *to;

    if ((ret = dwrite_outline_push_points(ctxt->outline, points, 3))) return ret;
    if ((ret = dwrite_outline_push_tag(ctxt->outline, OUTLINE_BEZIER))) return ret;

    return 0;
}

static int decompose_outline(FT_Outline *ft_outline, struct dwrite_outline *outline)
{
    static const FT_Outline_Funcs decompose_funcs =
    {
        decompose_move_to,
        decompose_line_to,
        decompose_conic_to,
        decompose_cubic_to,
        0,
        0
    };
    struct decompose_context context = { 0 };
    int ret;

    context.outline = outline;

    ret = FT_Outline_Decompose(ft_outline, &decompose_funcs, &context);

    if (!ret && context.figure_started)
        ret = dwrite_outline_push_tag(outline, OUTLINE_END_FIGURE);

    return ret;
}

static void embolden_glyph_outline(FT_Outline *outline, FLOAT emsize)
{
    FT_Pos strength;

    strength = FT_MulDiv(emsize, 1 << 6, 24);
    FT_Outline_EmboldenXY(outline, strength, 0);
}

static void embolden_glyph(FT_Glyph glyph, FLOAT emsize)
{
    FT_OutlineGlyph outline_glyph = (FT_OutlineGlyph)glyph;

    if (glyph->format != FT_GLYPH_FORMAT_OUTLINE)
        return;

    embolden_glyph_outline(&outline_glyph->outline, emsize);
}

int freetype_get_glyph_outline(void *key, float emSize, unsigned int simulations,
        UINT16 glyph, struct dwrite_outline *outline)
{
    FTC_ScalerRec scaler;
    FT_Size size;
    int ret;

    scaler.face_id = key;
    scaler.width  = emSize;
    scaler.height = emSize;
    scaler.pixel = 1;
    scaler.x_res = 0;
    scaler.y_res = 0;

    RtlEnterCriticalSection(&freetype_cs);
    if (!(ret = FTC_Manager_LookupSize(cache_manager, &scaler, &size)))
    {
        if (FT_Load_Glyph(size->face, glyph, FT_LOAD_NO_BITMAP) == 0)
        {
            FT_Outline *ft_outline = &size->face->glyph->outline;
            FT_Matrix m;

            if (simulations & DWRITE_FONT_SIMULATIONS_BOLD)
                embolden_glyph_outline(ft_outline, emSize);

            m.xx = 1 << 16;
            m.xy = simulations & DWRITE_FONT_SIMULATIONS_OBLIQUE ? (1 << 16) / 3 : 0;
            m.yx = 0;
            m.yy = -(1 << 16); /* flip Y axis */

            FT_Outline_Transform(ft_outline, &m);

            ret = decompose_outline(ft_outline, outline);
        }
    }
    RtlLeaveCriticalSection(&freetype_cs);

    return ret;
}

UINT16 freetype_get_glyph_count(void *key)
{
    UINT16 count = 0;
    FT_Face face;

    RtlEnterCriticalSection(&freetype_cs);
    if (FTC_Manager_LookupFace(cache_manager, key, &face) == 0)
        count = face->num_glyphs;
    RtlLeaveCriticalSection(&freetype_cs);

    return count;
}

static inline void ft_matrix_from_dwrite_matrix(const DWRITE_MATRIX *m, FT_Matrix *ft_matrix)
{
    ft_matrix->xx =  m->m11 * 0x10000;
    ft_matrix->xy = -m->m21 * 0x10000;
    ft_matrix->yx = -m->m12 * 0x10000;
    ft_matrix->yy =  m->m22 * 0x10000;
}

/* Should be used only while holding 'freetype_cs' */
static BOOL is_face_scalable(void *key)
{
    FT_Face face;
    if (FTC_Manager_LookupFace(cache_manager, key, &face) == 0)
        return FT_IS_SCALABLE(face);
    else
        return FALSE;
}

static BOOL get_glyph_transform(struct dwrite_glyphbitmap *bitmap, FT_Matrix *ret)
{
    FT_Matrix m;

    ret->xx = 1 << 16;
    ret->xy = 0;
    ret->yx = 0;
    ret->yy = 1 << 16;

    /* Some fonts provide mostly bitmaps and very few outlines, for example for .notdef.
       Disable transform if that's the case. */
    if (!is_face_scalable(bitmap->key) || (!bitmap->m && bitmap->simulations == 0))
        return FALSE;

    if (bitmap->simulations & DWRITE_FONT_SIMULATIONS_OBLIQUE) {
        m.xx =  1 << 16;
        m.xy = (1 << 16) / 3;
        m.yx =  0;
        m.yy =  1 << 16;
        FT_Matrix_Multiply(&m, ret);
    }

    if (bitmap->m) {
        ft_matrix_from_dwrite_matrix(bitmap->m, &m);
        FT_Matrix_Multiply(&m, ret);
    }

    return TRUE;
}

void freetype_get_glyph_bbox(struct dwrite_glyphbitmap *bitmap)
{
    FTC_ImageTypeRec imagetype;
    FT_BBox bbox = { 0 };
    BOOL needs_transform;
    FT_Glyph glyph;
    FT_Matrix m;

    RtlEnterCriticalSection(&freetype_cs);

    needs_transform = get_glyph_transform(bitmap, &m);

    imagetype.face_id = bitmap->key;
    imagetype.width = 0;
    imagetype.height = bitmap->emsize;
    imagetype.flags = needs_transform ? FT_LOAD_NO_BITMAP : FT_LOAD_DEFAULT;

    if (FTC_ImageCache_Lookup(image_cache, &imagetype, bitmap->glyph, &glyph, NULL) == 0) {
        if (needs_transform) {
            FT_Glyph glyph_copy;

            if (FT_Glyph_Copy(glyph, &glyph_copy) == 0) {
                if (bitmap->simulations & DWRITE_FONT_SIMULATIONS_BOLD)
                    embolden_glyph(glyph_copy, bitmap->emsize);

                /* Includes oblique and user transform. */
                FT_Glyph_Transform(glyph_copy, &m, NULL);
                FT_Glyph_Get_CBox(glyph_copy, FT_GLYPH_BBOX_PIXELS, &bbox);
                FT_Done_Glyph(glyph_copy);
            }
        }
        else
            FT_Glyph_Get_CBox(glyph, FT_GLYPH_BBOX_PIXELS, &bbox);
    }

    RtlLeaveCriticalSection(&freetype_cs);

    /* flip Y axis */
    SetRect(&bitmap->bbox, bbox.xMin, -bbox.yMax, bbox.xMax, -bbox.yMin);
}

static BOOL freetype_get_aliased_glyph_bitmap(struct dwrite_glyphbitmap *bitmap, FT_Glyph glyph)
{
    const RECT *bbox = &bitmap->bbox;
    int width = bbox->right - bbox->left;
    int height = bbox->bottom - bbox->top;

    if (glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
        FT_OutlineGlyph outline = (FT_OutlineGlyph)glyph;
        const FT_Outline *src = &outline->outline;
        FT_Bitmap ft_bitmap;
        FT_Outline copy;

        ft_bitmap.width = width;
        ft_bitmap.rows = height;
        ft_bitmap.pitch = bitmap->pitch;
        ft_bitmap.pixel_mode = FT_PIXEL_MODE_MONO;
        ft_bitmap.buffer = bitmap->buf;

        /* Note: FreeType will only set 'black' bits for us. */
        if (FT_Outline_New(library, src->n_points, src->n_contours, &copy) == 0) {
            FT_Outline_Copy(src, &copy);
            FT_Outline_Translate(&copy, -bbox->left << 6, bbox->bottom << 6);
            FT_Outline_Get_Bitmap(library, &copy, &ft_bitmap);
            FT_Outline_Done(library, &copy);
        }
    }
    else if (glyph->format == FT_GLYPH_FORMAT_BITMAP) {
        FT_Bitmap *ft_bitmap = &((FT_BitmapGlyph)glyph)->bitmap;
        BYTE *src = ft_bitmap->buffer, *dst = bitmap->buf;
        int w = min(bitmap->pitch, (ft_bitmap->width + 7) >> 3);
        int h = min(height, ft_bitmap->rows);

        while (h--) {
            memcpy(dst, src, w);
            src += ft_bitmap->pitch;
            dst += bitmap->pitch;
        }
    }
    else
        FIXME("format %x not handled\n", glyph->format);

    return TRUE;
}

static BOOL freetype_get_aa_glyph_bitmap(struct dwrite_glyphbitmap *bitmap, FT_Glyph glyph)
{
    const RECT *bbox = &bitmap->bbox;
    int width = bbox->right - bbox->left;
    int height = bbox->bottom - bbox->top;
    BOOL ret = FALSE;

    if (glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
        FT_OutlineGlyph outline = (FT_OutlineGlyph)glyph;
        const FT_Outline *src = &outline->outline;
        FT_Bitmap ft_bitmap;
        FT_Outline copy;

        ft_bitmap.width = width;
        ft_bitmap.rows = height;
        ft_bitmap.pitch = bitmap->pitch;
        ft_bitmap.pixel_mode = FT_PIXEL_MODE_GRAY;
        ft_bitmap.buffer = bitmap->buf;

        /* Note: FreeType will only set 'black' bits for us. */
        if (FT_Outline_New(library, src->n_points, src->n_contours, &copy) == 0) {
            FT_Outline_Copy(src, &copy);
            FT_Outline_Translate(&copy, -bbox->left << 6, bbox->bottom << 6);
            FT_Outline_Get_Bitmap(library, &copy, &ft_bitmap);
            FT_Outline_Done(library, &copy);
        }
    }
    else if (glyph->format == FT_GLYPH_FORMAT_BITMAP) {
        FT_Bitmap *ft_bitmap = &((FT_BitmapGlyph)glyph)->bitmap;
        BYTE *src = ft_bitmap->buffer, *dst = bitmap->buf;
        int w = min(bitmap->pitch, (ft_bitmap->width + 7) >> 3);
        int h = min(height, ft_bitmap->rows);

        while (h--) {
            memcpy(dst, src, w);
            src += ft_bitmap->pitch;
            dst += bitmap->pitch;
        }

        ret = TRUE;
    }
    else
        FIXME("format %x not handled\n", glyph->format);

    return ret;
}

BOOL freetype_get_glyph_bitmap(struct dwrite_glyphbitmap *bitmap)
{
    FTC_ImageTypeRec imagetype;
    BOOL needs_transform;
    BOOL ret = FALSE;
    FT_Glyph glyph;
    FT_Matrix m;

    RtlEnterCriticalSection(&freetype_cs);

    needs_transform = get_glyph_transform(bitmap, &m);

    imagetype.face_id = bitmap->key;
    imagetype.width = 0;
    imagetype.height = bitmap->emsize;
    imagetype.flags = needs_transform ? FT_LOAD_NO_BITMAP : FT_LOAD_DEFAULT;

    if (FTC_ImageCache_Lookup(image_cache, &imagetype, bitmap->glyph, &glyph, NULL) == 0) {
        FT_Glyph glyph_copy;

        if (needs_transform) {
            if (FT_Glyph_Copy(glyph, &glyph_copy) == 0) {
                if (bitmap->simulations & DWRITE_FONT_SIMULATIONS_BOLD)
                    embolden_glyph(glyph_copy, bitmap->emsize);

                /* Includes oblique and user transform. */
                FT_Glyph_Transform(glyph_copy, &m, NULL);
                glyph = glyph_copy;
            }
        }
        else
            glyph_copy = NULL;

        if (bitmap->aliased)
            ret = freetype_get_aliased_glyph_bitmap(bitmap, glyph);
        else
            ret = freetype_get_aa_glyph_bitmap(bitmap, glyph);

        if (glyph_copy)
            FT_Done_Glyph(glyph_copy);
    }

    RtlLeaveCriticalSection(&freetype_cs);

    return ret;
}

INT32 freetype_get_glyph_advance(void *key, float emSize, UINT16 index,
        DWRITE_MEASURING_MODE mode, BOOL *has_contours)
{
    FTC_ImageTypeRec imagetype;
    FT_Glyph glyph;
    INT32 advance;

    imagetype.face_id = key;
    imagetype.width = 0;
    imagetype.height = emSize;
    imagetype.flags = FT_LOAD_DEFAULT;
    if (mode == DWRITE_MEASURING_MODE_NATURAL)
        imagetype.flags |= FT_LOAD_NO_HINTING;

    RtlEnterCriticalSection(&freetype_cs);
    if (FTC_ImageCache_Lookup(image_cache, &imagetype, index, &glyph, NULL) == 0) {
        *has_contours = glyph->format == FT_GLYPH_FORMAT_OUTLINE && ((FT_OutlineGlyph)glyph)->outline.n_contours;
        advance = glyph->advance.x >> 16;
    }
    else {
        *has_contours = FALSE;
        advance = 0;
    }
    RtlLeaveCriticalSection(&freetype_cs);

    return advance;
}

void release_font_backend(void)
{
    FTC_Manager_Done(cache_manager);
    FT_Done_FreeType(library);
}
