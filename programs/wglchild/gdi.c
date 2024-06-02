/*
 * Copyright 2023 Rémi Bernon for CodeWeavers
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

#include "wglgears_private.h"

#include "wingdi.h"
#include "winuser.h"

#include <stdlib.h>

struct window_data
{
    HWND hwnd;
    BOOL layered;
};

static void draw_pixels( HDC hdc, const RECT *rect )
{
    if (rect->bottom == 0) return;

    for (int i = 0; i < 1000; i++)
    {
        int x = rand() % rect->right;
        int y = rand() % rect->bottom;
        SetPixel( hdc, x, y, RGB( 255, 0, 0 ) );
    }
}

static void draw_rectangles( HDC hdc )
{
    HBRUSH brush1 = CreateSolidBrush( RGB( 121, 90, 0 ) );
    HBRUSH brush2 = CreateSolidBrush( RGB( 240, 63, 19 ) );
    HBRUSH brush3 = CreateSolidBrush( RGB( 240, 210, 18 ) );
    HBRUSH brush4 = CreateSolidBrush( RGB( 9, 189, 21 ) );

    SelectObject( hdc, brush1 );
    Rectangle( hdc, 30, 30, 100, 100 );
    SelectObject( hdc, brush2 );
    Rectangle( hdc, 110, 30, 180, 100 );
    SelectObject( hdc, brush3 );
    Rectangle( hdc, 30, 110, 100, 180 );
    SelectObject( hdc, brush4 );
    Rectangle( hdc, 110, 110, 180, 180 );

    DeleteObject( brush1 );
    DeleteObject( brush2 );
    DeleteObject( brush3 );
    DeleteObject( brush4 );
}

static void draw_window( HDC hdc, RECT *rect )
{
    HPEN pen = CreatePen( PS_NULL, 1, RGB( 0, 0, 0 ) );
    HBRUSH brush = CreateSolidBrush( RGB( 0, 0, 0 ) );
    HPEN old_pen = SelectObject( hdc, pen );
    HBRUSH old_brush = SelectObject( hdc, brush );

    Rectangle( hdc, rect->left, rect->top, rect->right, rect->bottom );
    draw_rectangles( hdc );
    draw_pixels( hdc, rect );

    SelectObject( hdc, old_pen );
    SelectObject( hdc, old_brush );
    DeleteObject( pen );
    DeleteObject( brush );
}

static void DrawRectangles( HWND hwnd )
{
    PAINTSTRUCT ps;
    RECT rect;

    HDC hdc = BeginPaint( hwnd, &ps );
    GetClientRect( hwnd, &rect );
    draw_window( hdc, &rect );
    EndPaint( hwnd, &ps );
}

void gdi_draw_layered( HWND hwnd )
{
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255 * 70 / 100, AC_SRC_ALPHA};
    POINT src_pos = {0}, dst_pos = {0};
    HDC hdc, screen_dc = GetDC( NULL );
    BITMAPINFO info = {0};
    RECT window_rect;
    UINT32 *pixels;
    HBITMAP bmp;
    SIZE size;
    UINT i;

    GetWindowRect( hwnd, &window_rect );
    dst_pos = *(POINT *)&window_rect.left;

    OffsetRect( &window_rect, -window_rect.left, -window_rect.top );
    size.cx = window_rect.right;
    size.cy = window_rect.bottom;

    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = size.cx;
    info.bmiHeader.biHeight = -size.cy;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    bmp = CreateDIBSection( 0, &info, DIB_RGB_COLORS, (void **)&pixels, NULL, 0 );
    hdc = CreateCompatibleDC( screen_dc );
    SelectObject( hdc, bmp );
    draw_window( hdc, &window_rect );
    for (i = 0; i < size.cx * size.cy; i++) pixels[i] |= 0xff000000;

    UpdateLayeredWindow( hwnd, screen_dc, &dst_pos, &size, hdc, &src_pos, RGB( 240, 63, 19 ),
                         &blend, ULW_ALPHA | ULW_COLORKEY );

    DeleteObject( bmp );
    DeleteDC( hdc );
    ReleaseDC( NULL, screen_dc );
}

static LRESULT CALLBACK gdi_window_proc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    struct window_data *data = (void *)GetWindowLongPtrW( hwnd, GWLP_USERDATA );
    switch (msg)
    {
    case WM_CLOSE:
        PostQuitMessage( 0 );
        return 0;

    case WM_PAINT:
        if (data && data->layered) gdi_draw_layered( hwnd );
        else DrawRectangles( hwnd );
        return 0;

#if WINVER >= 0x0605
    case WM_NCCREATE: EnableNonClientDpiScaling( hwnd ); break;
#endif
    }

    return default_window_proc( hwnd, msg, wparam, lparam );
}

static WNDCLASSW wc =
{
    .style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
    .lpfnWndProc = (WNDPROC)gdi_window_proc,
    .lpszClassName = L"GDI",
};

HWND gdi_create_window( const WCHAR *name, RECT *geometry, BOOL fullscreen, BOOL layered )
{
    DWORD ex_style = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE, style = WS_OVERLAPPEDWINDOW;
    RECT window_rect = *geometry;
    struct window_data *data;

    if (!(data = calloc( 1, sizeof(*data) ))) return NULL;

    if (!fullscreen) AdjustWindowRectEx( &window_rect, style, FALSE, ex_style );
    else style = WS_POPUP;

    if (fullscreen)
    {
        DEVMODEW devmode = {.dmSize = sizeof(DEVMODEW)};
        devmode.dmPelsWidth = geometry->right - geometry->left;
        devmode.dmPelsHeight = geometry->bottom - geometry->top;
        devmode.dmBitsPerPel = 24;
        devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
        ChangeDisplaySettingsW( &devmode, CDS_FULLSCREEN );
    }

    if (layered) ex_style |= WS_EX_LAYERED;
    data->hwnd = CreateWindowExW( ex_style, wc.lpszClassName, name, WS_CLIPSIBLINGS | WS_CLIPCHILDREN | style,
                                  geometry->left, geometry->top, window_rect.right - window_rect.left,
                                  window_rect.bottom - window_rect.top, NULL, NULL, NULL, NULL );
    SetWindowLongPtrW( data->hwnd, GWLP_USERDATA, (LONG_PTR)data );
    if ((data->layered = layered)) gdi_draw_layered( data->hwnd );

    return data->hwnd;
}

void gdi_destroy_window( HWND hwnd )
{
    struct window_data *data = (void *)GetWindowLongPtrW( hwnd, GWLP_USERDATA );

    if (!data) return;

    DestroyWindow( data->hwnd );
    free( data );
}

BOOL gdi_init(void)
{
    wc.hInstance = GetModuleHandleA( NULL );
    wc.hIcon = LoadIconW( NULL, MAKEINTRESOURCEW( IDI_WINLOGO ) );
    wc.hCursor = LoadCursorW( NULL, MAKEINTRESOURCEW( IDC_ARROW ) );
    RegisterClassW( &wc );

    return TRUE;
}
