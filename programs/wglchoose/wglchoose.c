/*
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
#include <stddef.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"

#include "glad/glad.h"
#include "glad/glad_wgl.h"

#include "wine/debug.h"

static const int all_attribs[] =
{
    WGL_DRAW_TO_WINDOW_ARB,
    WGL_DRAW_TO_BITMAP_ARB,
    WGL_ACCELERATION_ARB,
    WGL_NEED_PALETTE_ARB,
    WGL_NEED_SYSTEM_PALETTE_ARB,
    WGL_SWAP_LAYER_BUFFERS_ARB,
    WGL_SWAP_METHOD_ARB,
    WGL_NUMBER_OVERLAYS_ARB,
    WGL_NUMBER_UNDERLAYS_ARB,
    WGL_TRANSPARENT_ARB,
    WGL_TRANSPARENT_RED_VALUE_ARB,
    WGL_TRANSPARENT_GREEN_VALUE_ARB,
    WGL_TRANSPARENT_BLUE_VALUE_ARB,
    WGL_TRANSPARENT_ALPHA_VALUE_ARB,
    WGL_TRANSPARENT_INDEX_VALUE_ARB,
    WGL_SHARE_DEPTH_ARB,
    WGL_SHARE_STENCIL_ARB,
    WGL_SHARE_ACCUM_ARB,
    WGL_SUPPORT_GDI_ARB,
    WGL_SUPPORT_OPENGL_ARB,
    WGL_DOUBLE_BUFFER_ARB,
    WGL_STEREO_ARB,
    WGL_PIXEL_TYPE_ARB,
    WGL_COLOR_BITS_ARB,
    WGL_RED_BITS_ARB,
    WGL_RED_SHIFT_ARB,
    WGL_GREEN_BITS_ARB,
    WGL_GREEN_SHIFT_ARB,
    WGL_BLUE_BITS_ARB,
    WGL_BLUE_SHIFT_ARB,
    WGL_ALPHA_BITS_ARB,
    WGL_ALPHA_SHIFT_ARB,
    WGL_ACCUM_BITS_ARB,
    WGL_ACCUM_RED_BITS_ARB,
    WGL_ACCUM_GREEN_BITS_ARB,
    WGL_ACCUM_BLUE_BITS_ARB,
    WGL_ACCUM_ALPHA_BITS_ARB,
    WGL_DEPTH_BITS_ARB,
    WGL_STENCIL_BITS_ARB,
    WGL_AUX_BUFFERS_ARB,
    -1,
};

static int attrib_index( int attr )
{
    for (int i = 0; i < ARRAY_SIZE(all_attribs); i++) if (all_attribs[i] == attr) return i;
    return ARRAY_SIZE(all_attribs);
}

static void print_pixel_format( HDC hdc, int fmt )
{
    int values[ARRAY_SIZE(all_attribs)];
#define value(x) values[attrib_index(x)]

    wglGetPixelFormatAttribivARB( hdc, fmt, 0, ARRAY_SIZE(all_attribs) - 1, all_attribs, values );

    /*
    WGL_NEED_PALETTE_ARB, WGL_NEED_SYSTEM_PALETTE_ARB, WGL_SWAP_LAYER_BUFFERS_ARB, WGL_NUMBER_OVERLAYS_ARB, WGL_NUMBER_UNDERLAYS_ARB
    WGL_SHARE_DEPTH_ARB, WGL_SHARE_STENCIL_ARB, WGL_SHARE_ACCUM_ARB,
    WGL_RED_SHIFT_ARB, WGL_GREEN_SHIFT_ARB, WGL_BLUE_SHIFT_ARB, WGL_ALPHA_SHIFT_ARB,
    */

    printf( "%-4d %1s%1s %9s %-3s %-2s %-4s %-6s color %3d:%2d/%-2d,%2d/%-2d,%2d/%-2d,%2d/%-2d d%-2d s%-2d accum %3d:%2d,%2d,%2d,%2d aux%d %-6s %-9s\n", fmt,
        value(WGL_DRAW_TO_WINDOW_ARB) ? "w" : "",
        value(WGL_DRAW_TO_BITMAP_ARB) ? "b" : "",
        value(WGL_ACCELERATION_ARB) == WGL_NO_ACCELERATION_ARB ? "noaccel"
            : value(WGL_ACCELERATION_ARB) == WGL_GENERIC_ACCELERATION_ARB ? "genaccel"
            : value(WGL_ACCELERATION_ARB) == WGL_FULL_ACCELERATION_ARB ? "fullaccel"
            : "?",
        value(WGL_SUPPORT_GDI_ARB) ? "gdi" : "",
        value(WGL_SUPPORT_OPENGL_ARB) ? "gl" : "",
        value(WGL_PIXEL_TYPE_ARB) == WGL_TYPE_RGBA_ARB ? "rgba"
            : value(WGL_PIXEL_TYPE_ARB) == WGL_TYPE_COLORINDEX_ARB ? "cidx"
            : value(WGL_PIXEL_TYPE_ARB) == WGL_TYPE_RGBA_FLOAT_ARB ? "flt"
            : value(WGL_PIXEL_TYPE_ARB) == WGL_TYPE_RGBA_UNSIGNED_FLOAT_EXT ? "uflt"
            : "?",
        value(WGL_DOUBLE_BUFFER_ARB) ? "double" : "single",
        value(WGL_COLOR_BITS_ARB),
        value(WGL_RED_BITS_ARB),
        value(WGL_RED_SHIFT_ARB),
        value(WGL_GREEN_BITS_ARB),
        value(WGL_GREEN_SHIFT_ARB),
        value(WGL_BLUE_BITS_ARB),
        value(WGL_BLUE_SHIFT_ARB),
        value(WGL_ALPHA_BITS_ARB),
        value(WGL_ALPHA_SHIFT_ARB),
        value(WGL_DEPTH_BITS_ARB),
        value(WGL_STENCIL_BITS_ARB),
        value(WGL_ACCUM_BITS_ARB),
        value(WGL_ACCUM_RED_BITS_ARB),
        value(WGL_ACCUM_GREEN_BITS_ARB),
        value(WGL_ACCUM_BLUE_BITS_ARB),
        value(WGL_ACCUM_ALPHA_BITS_ARB),
        value(WGL_AUX_BUFFERS_ARB),
        value(WGL_STEREO_ARB) ? "stereo" : "  mono",
        value(WGL_SWAP_METHOD_ARB) == WGL_SWAP_COPY_ARB ? "swap:copy"
            : value(WGL_SWAP_METHOD_ARB) == WGL_SWAP_EXCHANGE_ARB ? "swap:xchg"
            : value(WGL_SWAP_METHOD_ARB) == WGL_SWAP_UNDEFINED_ARB ? "swap:undf"
            : "swap:?"
    );

/*
        value(WGL_NEED_PALETTE_ARB),
        value(WGL_NEED_SYSTEM_PALETTE_ARB),
        value(WGL_SWAP_LAYER_BUFFERS_ARB),

        value(WGL_NUMBER_OVERLAYS_ARB),
        value(WGL_NUMBER_UNDERLAYS_ARB),
        value(WGL_TRANSPARENT_ARB),
        value(WGL_TRANSPARENT_RED_VALUE_ARB),
        value(WGL_TRANSPARENT_GREEN_VALUE_ARB),
        value(WGL_TRANSPARENT_BLUE_VALUE_ARB),
        value(WGL_TRANSPARENT_ALPHA_VALUE_ARB),
        value(WGL_TRANSPARENT_INDEX_VALUE_ARB),
*/

/*
    ERR( "%4d %d %d %4x %4x %d %d %d %d %d %d %d %4x %3d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %d %d\n", fmt,
        value(WGL_DRAW_TO_WINDOW_ARB),
        value(WGL_DRAW_TO_BITMAP_ARB),
        value(WGL_ACCELERATION_ARB),
        value(WGL_SWAP_METHOD_ARB),
        value(WGL_SHARE_DEPTH_ARB),
        value(WGL_SHARE_STENCIL_ARB),
        value(WGL_SHARE_ACCUM_ARB),
        value(WGL_SUPPORT_GDI_ARB),
        value(WGL_SUPPORT_OPENGL_ARB),
        value(WGL_DOUBLE_BUFFER_ARB),
        value(WGL_STEREO_ARB),
        value(WGL_PIXEL_TYPE_ARB),
        value(WGL_COLOR_BITS_ARB),
        value(WGL_RED_BITS_ARB),
        value(WGL_RED_SHIFT_ARB),
        value(WGL_GREEN_BITS_ARB),
        value(WGL_GREEN_SHIFT_ARB),
        value(WGL_BLUE_BITS_ARB),
        value(WGL_BLUE_SHIFT_ARB),
        value(WGL_ALPHA_BITS_ARB),
        value(WGL_ALPHA_SHIFT_ARB),
        value(WGL_ACCUM_BITS_ARB),
        value(WGL_ACCUM_RED_BITS_ARB),
        value(WGL_ACCUM_GREEN_BITS_ARB),
        value(WGL_ACCUM_BLUE_BITS_ARB),
        value(WGL_ACCUM_ALPHA_BITS_ARB),
        value(WGL_DEPTH_BITS_ARB),
        value(WGL_STENCIL_BITS_ARB),
        value(WGL_AUX_BUFFERS_ARB)
    );
*/

#undef value
}

static void print_all_pixel_formats( HDC hdc )
{
    int num_formats_attrib = WGL_NUMBER_PIXEL_FORMATS_ARB;
    int num_formats = 0;

    wglGetPixelFormatAttribivARB( hdc, 0, 0, 1, &num_formats_attrib, &num_formats );
    for (int fmt = 1; fmt <= num_formats; fmt++)
        print_pixel_format( hdc, fmt );
}

struct filter
{
    int buf, r, g, b, a, d, s, accum;
    int win, bitmap, db, accel, opengl, gdi, pixel;
    int aux, swap;
};

static void choose_pixel_format( HDC hdc, struct filter attribs )
{
    int wgl_attribs[64] = {0}, *attr = wgl_attribs, choose_fmt, formats[512];
    PIXELFORMATDESCRIPTOR desc = {.nSize = sizeof(desc), .nVersion = 1};
    unsigned int num_formats = 0;

    if (attribs.opengl == 1) desc.dwFlags |= PFD_SUPPORT_OPENGL;
    if (attribs.gdi == 1) desc.dwFlags |= PFD_SUPPORT_GDI;
    if (attribs.win == 1) desc.dwFlags |= PFD_DRAW_TO_WINDOW;
    if (attribs.bitmap == 1) desc.dwFlags |= PFD_DRAW_TO_BITMAP;
    if (attribs.db < 0) desc.dwFlags |= PFD_DOUBLEBUFFER_DONTCARE;
    else if (attribs.db == 1) desc.dwFlags |= PFD_DOUBLEBUFFER;
    if (attribs.d < 0) desc.dwFlags |= PFD_DEPTH_DONTCARE;
    if (attribs.pixel == WGL_TYPE_COLORINDEX_ARB) desc.iPixelType = PFD_TYPE_COLORINDEX;
    else desc.iPixelType = PFD_TYPE_RGBA;

    if (attribs.buf >= 0) desc.cColorBits = attribs.buf;
    if (attribs.a >= 0) desc.cAlphaBits = attribs.a;
    if (attribs.d >= 0) desc.cDepthBits = attribs.d;
    if (attribs.s >= 0) desc.cStencilBits = attribs.s;
    if (attribs.aux >= 0) desc.cAuxBuffers = attribs.aux;
    if (attribs.accum >= 0) desc.cAccumBits = attribs.accum;

    choose_fmt = ChoosePixelFormat( hdc, &desc );
    printf( "--- ChoosePixelFormat ---\n" );
    print_pixel_format( hdc, choose_fmt );
    printf( "--- wglChoosePixelFormatARB ---\n" );

#define ADD_ATTRIB(attrib, name) \
    if (attribs.attrib >= 0) \
    {  \
printf( #name ": %d\n", attribs.attrib ); \
      attr[0] = name; \
      attr[1] = attribs.attrib; \
      attr += 2; \
    }
    ADD_ATTRIB(win, WGL_DRAW_TO_WINDOW_ARB);
    ADD_ATTRIB(bitmap, WGL_DRAW_TO_BITMAP_ARB);
    ADD_ATTRIB(db, WGL_DOUBLE_BUFFER_ARB);
    ADD_ATTRIB(accel, WGL_ACCELERATION_ARB);
    ADD_ATTRIB(opengl, WGL_SUPPORT_OPENGL_ARB);
    ADD_ATTRIB(gdi, WGL_SUPPORT_GDI_ARB);
    ADD_ATTRIB(buf, WGL_COLOR_BITS_ARB);
    ADD_ATTRIB(pixel, WGL_PIXEL_TYPE_ARB);
    ADD_ATTRIB(r, WGL_RED_BITS_ARB);
    ADD_ATTRIB(g, WGL_GREEN_BITS_ARB);
    ADD_ATTRIB(b, WGL_BLUE_BITS_ARB);
    ADD_ATTRIB(a, WGL_ALPHA_BITS_ARB);
    ADD_ATTRIB(d, WGL_DEPTH_BITS_ARB);
    ADD_ATTRIB(s, WGL_STENCIL_BITS_ARB);
    ADD_ATTRIB(aux, WGL_AUX_BUFFERS_ARB);
    ADD_ATTRIB(swap, WGL_SWAP_METHOD_ARB);
#undef ADD_ATTRIB

    wglChoosePixelFormatARB( hdc, wgl_attribs, NULL, 512, formats, &num_formats );
    for (unsigned int i = 0; i < num_formats; ++i)
        print_pixel_format( hdc, formats[i] );
}

int main(int argc, char *argv[])
{
    PIXELFORMATDESCRIPTOR desc = {.nSize = sizeof(desc), .nVersion = 1, .dwFlags = PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_DRAW_TO_WINDOW};
    struct filter filter;
    HGLRC ctx;
    HWND hwnd;
    BOOL ret;
    int fmt;
    HDC hdc;

    hwnd = CreateWindowW( L"static", L"static", WS_POPUP | WS_VISIBLE, 0, 0, 100, 100, 0, 0, NULL, NULL );
    hdc = GetDC( hwnd );

    desc.iPixelType = PFD_TYPE_RGBA;
    desc.cColorBits = 32;
    desc.iLayerType = PFD_MAIN_PLANE;
    fmt = ChoosePixelFormat( hdc, &desc );
    printf( "ChoosePixelFormat -> %u\n", fmt );
    SetPixelFormat( hdc, fmt, &desc );

    ctx = wglCreateContext( hdc );
    printf( "wglCreateContext -> %p\n", ctx );
    ret = wglMakeCurrent( hdc, ctx );
    printf( "wglMakeCurrent -> %u\n", ret );

    gladLoadGL();
    gladLoadWGL( hdc );

    printf("===== wglchoose  =====\n");
    print_all_pixel_formats( hdc );

    memset( &filter, 0xff, sizeof(filter) );
    printf("===== wglchoose nil =====\n");
    choose_pixel_format( hdc, filter );

    memset( &filter, 0xff, sizeof(filter) );
    filter.d = 0;
    printf("===== wglchoose d=0 =====\n");
    choose_pixel_format( hdc, filter );
    memset( &filter, 0xff, sizeof(filter) );
    filter.d = 1;
    printf("===== wglchoose d=1 =====\n");
    choose_pixel_format( hdc, filter );
    memset( &filter, 0xff, sizeof(filter) );
    filter.d = 16;
    printf("===== wglchoose d=16 =====\n");
    choose_pixel_format( hdc, filter );

    memset( &filter, 0xff, sizeof(filter) );
    filter.s = 0;
    printf("===== wglchoose s=0 =====\n");
    choose_pixel_format( hdc, filter );
    memset( &filter, 0xff, sizeof(filter) );
    filter.s = 1;
    printf("===== wglchoose s=1 =====\n");
    choose_pixel_format( hdc, filter );
    memset( &filter, 0xff, sizeof(filter) );
    filter.s = 8;
    printf("===== wglchoose s=8 =====\n");
    choose_pixel_format( hdc, filter );

    memset( &filter, 0xff, sizeof(filter) );
    filter.buf = 16;
    printf("===== wglchoose buf=16 =====\n");
    choose_pixel_format( hdc, filter );
    filter.a = 0;
    printf("===== wglchoose buf=16:a=0 =====\n");
    choose_pixel_format( hdc, filter );
    filter.r = 5;
    filter.g = 6;
    filter.b = 5;
    filter.a = 1;
    printf("===== wglchoose buf=16:r=5:g=5:b=5:a=1 =====\n");
    choose_pixel_format( hdc, filter );
    filter.a = 0;
    printf("===== wglchoose buf=16:r=5:g=6:b=5:a=0 =====\n");
    choose_pixel_format( hdc, filter );

    memset( &filter, 0xff, sizeof(filter) );
    filter.buf = 32;
    printf("===== wglchoose buf=32 =====\n");
    choose_pixel_format( hdc, filter );
    filter.a = 0;
    printf("===== wglchoose buf=32:a=0 =====\n");
    choose_pixel_format( hdc, filter );
    filter.a = -1;
    filter.d = 0;
    filter.s = 0;
    printf("===== wglchoose buf=32:d=0:s=0 =====\n");
    choose_pixel_format( hdc, filter );
    filter.d = 1;
    printf("===== wglchoose buf=32:d=1:s=0 =====\n");
    choose_pixel_format( hdc, filter );
    filter.d = 0;
    filter.s = 1;
    printf("===== wglchoose buf=32:d=0:s=1 =====\n");
    choose_pixel_format( hdc, filter );
    filter.a = 1;
    filter.d = 1;
    printf("===== wglchoose buf=32:a=1:d=1:s=1 =====\n");
    choose_pixel_format( hdc, filter );

    memset( &filter, 0xff, sizeof(filter) );
    filter.buf = 24;
    filter.a = 0;
    printf("===== wglchoose buf=24:a=0 =====\n");
    choose_pixel_format( hdc, filter );

    memset( &filter, 0xff, sizeof(filter) );
    printf("===== wglchoose pixel=rgba:win:double:opengl:buf=32 =====\n");
    filter.pixel = WGL_TYPE_RGBA_ARB;
    filter.win = 1;
    filter.db = 1;
    filter.opengl = 1;
    filter.buf = 32;
    choose_pixel_format( hdc, filter );
    filter.a = 0;
    printf("===== wglchoose pixel=rgba:win:double:opengl:buf=32:a=0 =====\n");
    choose_pixel_format( hdc, filter );
    filter.a = -1;
    filter.d = 0;
    filter.s = 0;
    printf("===== wglchoose pixel=rgba:win:double:opengl:buf=32:d=0:s=0 =====\n");
    choose_pixel_format( hdc, filter );
    filter.a = 1;
    filter.d = 1;
    filter.s = 1;
    printf("===== wglchoose pixel=rgba:win:double:opengl:buf=32:a=1:d=1:s=1 =====\n");
    choose_pixel_format( hdc, filter );
    filter.buf = 16;
    filter.a = 0;
    filter.d = -1;
    filter.s = -1;
    printf("===== wglchoose pixel=rgba:win:double:opengl:buf=16:a=0 =====\n");
    choose_pixel_format( hdc, filter );
    filter.r = 5;
    filter.g = 6;
    filter.b = 5;
    filter.a = 1;
    printf("===== wglchoose pixel=rgba:win:double:opengl:buf=16:r=5:g=5:b=5:a=1 =====\n");
    choose_pixel_format( hdc, filter );
    filter.a = 0;
    printf("===== wglchoose pixel=rgba:win:double:opengl:buf=16:r=5:g=6:b=5:a=0 =====\n");
    choose_pixel_format( hdc, filter );

    memset( &filter, 0xff, sizeof(filter) );
    filter.buf = 32;
    filter.r = 10;
    filter.g = 10;
    filter.b = 10;
    printf("===== wglchoose buf=32:r=10:g=10:b=10 =====\n");
    choose_pixel_format( hdc, filter );

    return 0;
}
