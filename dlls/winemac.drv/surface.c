/*
 * Mac driver window surface implementation
 *
 * Copyright 1993, 1994, 2011 Alexandre Julliard
 * Copyright 2006 Damjan Jovanovic
 * Copyright 2012, 2013 Ken Thomases for CodeWeavers, Inc.
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

#include "config.h"

#include "macdrv.h"
#include "winuser.h"

WINE_DEFAULT_DEBUG_CHANNEL(bitblt);

static inline int get_dib_stride(int width, int bpp)
{
    return ((width * bpp + 31) >> 3) & ~3;
}

static inline int get_dib_image_size(const BITMAPINFO *info)
{
    return get_dib_stride(info->bmiHeader.biWidth, info->bmiHeader.biBitCount)
        * abs(info->bmiHeader.biHeight);
}


struct macdrv_window_surface
{
    struct window_surface   header;
    macdrv_window           window;
    CGDataProviderRef       provider;
};

static struct macdrv_window_surface *get_mac_surface(struct window_surface *surface);

static CGDataProviderRef data_provider_create(size_t size, void **bits)
{
    CGDataProviderRef provider;
    CFMutableDataRef data;

    if (!(data = CFDataCreateMutable(kCFAllocatorDefault, size))) return NULL;
    CFDataSetLength(data, size);

    if ((provider = CGDataProviderCreateWithCFData(data)))
        *bits = CFDataGetMutableBytePtr(data);
    CFRelease(data);

    return provider;
}

/***********************************************************************
 *              macdrv_surface_set_clip
 */
static void macdrv_surface_set_clip(struct window_surface *window_surface, const RECT *rects, UINT count)
{
}

/***********************************************************************
 *              macdrv_surface_flush
 */
static BOOL macdrv_surface_flush(struct window_surface *window_surface, const RECT *rect, const RECT *dirty,
                                 const BITMAPINFO *color_info, const void *color_bits, BOOL shape_changed,
                                 const BITMAPINFO *shape_info, const void *shape_bits)
{
    struct macdrv_window_surface *surface = get_mac_surface(window_surface);
    CGImageAlphaInfo alpha_info = (window_surface->alpha_mask ? kCGImageAlphaPremultipliedFirst : kCGImageAlphaNoneSkipFirst);
    CGColorSpaceRef colorspace;
    CGImageRef image;

    colorspace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    image = CGImageCreate(color_info->bmiHeader.biWidth, abs(color_info->bmiHeader.biHeight), 8, 32,
                          color_info->bmiHeader.biSizeImage / abs(color_info->bmiHeader.biHeight), colorspace,
                          alpha_info | kCGBitmapByteOrder32Little, surface->provider, NULL, retina_on, kCGRenderingIntentDefault);
    CGColorSpaceRelease(colorspace);

    macdrv_window_set_color_image(surface->window, image, cgrect_from_rect(*rect), cgrect_from_rect(*dirty));
    CGImageRelease(image);

    if (shape_changed)
    {
        if (!shape_bits)
            macdrv_window_set_shape_image(surface->window, NULL);
        else
        {
            const BYTE *src = shape_bits;
            CGDataProviderRef provider;
            CGImageRef image;
            BYTE *dst;
            UINT i;

            if (!(provider = data_provider_create(shape_info->bmiHeader.biSizeImage, (void **)&dst))) return TRUE;
            for (i = 0; i < shape_info->bmiHeader.biSizeImage; i++) dst[i] = ~src[i]; /* CGImage mask bits are inverted */

            image = CGImageMaskCreate(shape_info->bmiHeader.biWidth, abs(shape_info->bmiHeader.biHeight), 1, 1,
                                      shape_info->bmiHeader.biSizeImage / abs(shape_info->bmiHeader.biHeight),
                                      provider, NULL, retina_on);
            CGDataProviderRelease(provider);

            macdrv_window_set_shape_image(surface->window, image);
            CGImageRelease(image);
        }
    }

    return TRUE;
}

/***********************************************************************
 *              macdrv_surface_destroy
 */
static void macdrv_surface_destroy(struct window_surface *window_surface)
{
    struct macdrv_window_surface *surface = get_mac_surface(window_surface);

    TRACE("freeing %p\n", surface);
    CGDataProviderRelease(surface->provider);
}

static void unmap_image_data( void *info, const void *data, size_t size )
{
    munmap( info, size );
}

static void macdrv_surface_create_images( struct window_surface *window_surface, int fd, UINT size,
                                          const BITMAPINFO *info, UINT count )
{
    struct macdrv_window_surface *surface = get_mac_surface(window_surface);
    void *data = mmap( NULL, size, PROT_READ, MAP_PRIVATE, fd, 0 );
    DWORD window_background;

    window_background = macdrv_window_background_color();
    memset_pattern4( data, &window_background, info->bmiHeader.biSizeImage );

    surface->provider = CGDataProviderCreateWithData( data, data, info->bmiHeader.biSizeImage, unmap_image_data );
}

static UINT macdrv_surface_acquire_image( struct window_surface *window_surface )
{
    return 0; /* always return the first image */
}

static void macdrv_surface_present_image( struct window_surface *window_surface, UINT index, const RECT *dirty )
{
    /* nothing to do, this is handled in flush instead */
}

static const struct window_surface_funcs macdrv_surface_funcs =
{
    .set_clip = macdrv_surface_set_clip,
    .flush = macdrv_surface_flush,
    .destroy = macdrv_surface_destroy,
    .create_images = macdrv_surface_create_images,
    .acquire_image = macdrv_surface_acquire_image,
    .present_image = macdrv_surface_present_image,
};

static struct macdrv_window_surface *get_mac_surface(struct window_surface *surface)
{
    if (!surface || surface->funcs != &macdrv_surface_funcs) return NULL;
    return (struct macdrv_window_surface *)surface;
}

/***********************************************************************
 *              create_surface
 */
static struct window_surface *create_surface(HWND hwnd, macdrv_window window, const RECT *rect)
{
    struct macdrv_window_surface *surface;
    int width = rect->right - rect->left, height = rect->bottom - rect->top;
    char buffer[FIELD_OFFSET(BITMAPINFO, bmiColors[256])];
    BITMAPINFO *info = (BITMAPINFO *)buffer;
    struct window_surface *window_surface;

    memset(info, 0, sizeof(*info));
    info->bmiHeader.biSize        = sizeof(info->bmiHeader);
    info->bmiHeader.biWidth       = width;
    info->bmiHeader.biHeight      = -height; /* top-down */
    info->bmiHeader.biPlanes      = 1;
    info->bmiHeader.biBitCount    = 32;
    info->bmiHeader.biSizeImage   = get_dib_image_size(info);
    info->bmiHeader.biCompression = BI_RGB;

    if ((window_surface = window_surface_create(sizeof(*surface), &macdrv_surface_funcs, hwnd, rect, info, 0)))
    {
        surface = get_mac_surface(window_surface);
        surface->window = window;
    }

    return window_surface;
}


/***********************************************************************
 *              CreateWindowSurface   (MACDRV.@)
 */
BOOL macdrv_CreateWindowSurface(HWND hwnd, BOOL layered, float scale, const RECT *surface_rect,
                                struct window_surface **surface)
{
    struct window_surface *previous;
    struct macdrv_win_data *data;

    TRACE("hwnd %p, layered %u, surface_rect %s, surface %p\n", hwnd, layered, wine_dbgstr_rect(surface_rect), surface);

    if (scale != 1.0) return FALSE; /* let win32u scale for us */
    if ((previous = *surface) && previous->funcs == &macdrv_surface_funcs) return TRUE;
    if (!(data = get_win_data(hwnd))) return TRUE; /* use default surface */
    if (previous) window_surface_release(previous);

    if (layered)
    {
        data->layered = TRUE;
        data->ulw_layered = TRUE;
    }

    *surface = create_surface(hwnd, data->cocoa_window, surface_rect);

    release_win_data(data);
    return TRUE;
}
