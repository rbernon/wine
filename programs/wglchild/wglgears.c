/*
 * Copyright (C) 1999-2001  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * This is a port of the infamous "gears" demo to straight GLX (i.e. no GLUT)
 * Port by Brian Paul  23 March 2001
 *
 * Command line options:
 *    -info      print GL implementation information
 *
 * Modified from X11/GLX to Win32/WGL by Ben Skeggs
 * 25th October 2004
 */

#include <assert.h>
#include <limits.h>
#include <windows.h>
#include "glad/glad.h"
#include "glad/glad_wgl.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <mmsystem.h>

#include "wglgears_private.h"

BOOL verbose = FALSE;
static HWND vulkan_parent;
static HWND vulkan_child;
static HWND opengl_parent;
static HWND opengl_child;
static HWND gdi_child;

static void usage(void)
{
    printf( "Usage:\n" );
    printf( "  -srgb              run in sRGB mode\n" );
    printf( "  -samples N         run in multisample mode with at least N samples\n" );
    printf( "  -fullscreen        run in fullscreen mode\n" );
    printf( "  -info              display OpenGL renderer info\n" );
    printf( "  -geometry WxH+X+Y  window geometry\n" );
}

/* return current time (in seconds) */
double current_time(void)
{
    return timeGetTime() / 1000.0;
}

static void event_loop(void)
{
    MSG msg;

    TIMECAPS tc;
    timeGetDevCaps( &tc, sizeof(tc) );
    timeBeginPeriod( tc.wPeriodMin );

    while (1)
    {
        if (PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE ))
        {
            if (msg.message == WM_QUIT) break;
            TranslateMessage( &msg );
            DispatchMessageW( &msg );
        }

        if (opengl_parent) opengl_draw_frame( opengl_parent );
        if (opengl_child) opengl_draw_frame( opengl_child );
        if (vulkan_parent) vulkan_draw_frame( vulkan_parent );
        if (vulkan_child) vulkan_draw_frame( vulkan_child );
    }

    timeEndPeriod( tc.wPeriodMin );
}

static void parse_geometry( const char *str, RECT *rect )
{
    long tw, th, tx, ty;
    char *end;
    if (*str == '=') str++;

    tw = LONG_MAX;
    if (isdigit( *str ))
    {
        tw = strtol( str, &end, 10 );
        if (str == end) return;
        str = end;
    }

    th = LONG_MAX;
    if (tolower( *str ) == 'x')
    {
        str++;
        th = strtol( str, &end, 10 );
        if (str == end) return;
        str = end;
    }

    tx = LONG_MAX;
    if (*str == '+' || *str == '-')
    {
        tx = strtol( str, &end, 10 );
        if (str == end) return;
        str = end;
    }

    ty = LONG_MAX;
    if (*str == '+' || *str == '-')
    {
        ty = strtol( str, &end, 10 );
        if (str == end) return;
        str = end;
    }

    if (tx < INT_MAX) rect->left = tx;
    if (ty < INT_MAX) rect->top = ty;
    if (tw < LONG_MAX) rect->right = rect->left + tw;
    else rect->right = rect->left + 300;
    if (th < LONG_MAX) rect->bottom = rect->top + th;
    else rect->bottom = rect->top + 300;
}

int main( int argc, char *argv[] )
{
    RECT geometry = {.right = 300, .bottom = 300};
    BOOL use_srgb = FALSE, fullscreen = FALSE;
    INT i, samples = 0;

    for (i = 1; i < argc; i++)
    {
        if (strcmp( argv[i], "-info" ) == 0)
        {
            verbose = TRUE;
        }
        else if (strcmp( argv[i], "-srgb" ) == 0)
        {
            use_srgb = GL_TRUE;
        }
        else if (i < argc - 1 && strcmp( argv[i], "-samples" ) == 0)
        {
            samples = strtod( argv[i + 1], NULL );
            ++i;
        }
        else if (strcmp( argv[i], "-fullscreen" ) == 0)
        {
            fullscreen = GL_TRUE;
        }
        else if (strcmp( argv[i], "-geometry" ) == 0)
        {
            parse_geometry( argv[i + 1], &geometry );
            i++;
        }
        else
        {
            usage();
            return -1;
        }
    }

#if WINVER >= 0x0605
    SetProcessDpiAwarenessContext( DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE );
#endif

    if (fullscreen) SetRect( &geometry, 0, 0, GetSystemMetrics( SM_CXSCREEN ), GetSystemMetrics( SM_CYSCREEN ) );

    gdi_init();
    opengl_init();
    vulkan_init();

    vulkan_parent = vulkan_create_window( L"Vulkan (parent)", &geometry, fullscreen );
    ShowWindow( vulkan_parent, SW_SHOW );

    InflateRect( &geometry, 200, 100 );
    OffsetRect( &geometry, -geometry.left, -geometry.top );

    opengl_parent = opengl_create_window( L"OpenGL (parent)", &geometry, fullscreen, samples, use_srgb );
    ShowWindow( opengl_parent, SW_SHOW );

    InflateRect( &geometry, -200, -100 );
    OffsetRect( &geometry, -geometry.left, -geometry.top );
    opengl_child = opengl_create_window( L"OpenGL (child)", &geometry, fullscreen, samples, use_srgb );
    SetParent( opengl_child, opengl_parent );
    ShowWindow( opengl_child, SW_SHOW );

    OffsetRect( &geometry, geometry.right, 0 );
    gdi_child = gdi_create_window( L"GDI (child)", &geometry, fullscreen, FALSE );
    SetParent( gdi_child, opengl_parent );
    ShowWindow( gdi_child, SW_SHOW );

    OffsetRect( &geometry, -geometry.left, geometry.bottom );
    vulkan_child = vulkan_create_window( L"Vulkan (child)", &geometry, fullscreen );
    SetParent( vulkan_child, opengl_parent );
    ShowWindow( vulkan_child, SW_SHOW );

    if (verbose)
    {
        printf( "GL_RENDERER   = %s\n", (char *)glGetString( GL_RENDERER ) );
        printf( "GL_VERSION    = %s\n", (char *)glGetString( GL_VERSION ) );
        printf( "GL_VENDOR     = %s\n", (char *)glGetString( GL_VENDOR ) );
        printf( "GL_EXTENSIONS = %s\n", (char *)glGetString( GL_EXTENSIONS ) );
    }

    event_loop();

    if (gdi_child) gdi_destroy_window( gdi_child );
    if (opengl_parent) opengl_destroy_window( opengl_parent );
    if (opengl_child) opengl_destroy_window( opengl_child );
    if (vulkan_parent) vulkan_destroy_window( vulkan_parent );
    if (vulkan_child) vulkan_destroy_window( vulkan_child );

    return EXIT_SUCCESS;
}