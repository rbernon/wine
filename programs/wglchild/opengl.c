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

#include "wglgears_private.h"

#include "wingdi.h"
#include "winuser.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "glad/glad.h"
#include "glad/glad_wgl.h"

struct window_data
{
    HWND hwnd;
    HGLRC hrc;

    GLboolean use_srgb;
    GLint gear1, gear2, gear3;
    GLfloat view_rotx, view_roty, view_rotz;
    GLfloat angle;
    GLboolean animate;

    double tRot0, tRate0;
    int frames;
};

static GLfloat srgb_to_linear( GLfloat c )
{
    if (c <= 0.04045f) return c / 12.92f;
    return powf( (c + 0.055f) / 1.055f, 2.4f );
}

/*
 *
 *  Draw a gear wheel.  You'll probably want to call this function when
 *  building a display list since we do a lot of trig here.
 *
 *  Input:  inner_radius - radius of hole at center
 *          outer_radius - radius at center of teeth
 *          width - width of gear
 *          teeth - number of teeth
 *          tooth_depth - depth of tooth
 */
static void gear( GLfloat inner_radius, GLfloat outer_radius, GLfloat width, GLint teeth, GLfloat tooth_depth )
{
    GLint i;
    GLfloat r0, r1, r2;
    GLfloat angle, da;
    GLfloat u, v, len;

    r0 = inner_radius;
    r1 = outer_radius - tooth_depth / 2.0;
    r2 = outer_radius + tooth_depth / 2.0;

    da = 2.0 * M_PI / teeth / 4.0;

    glShadeModel( GL_FLAT );

    glNormal3f( 0.0, 0.0, 1.0 );

    /* draw front face */
    glBegin( GL_QUAD_STRIP );
    for (i = 0; i <= teeth; i++)
    {
        angle = i * 2.0 * M_PI / teeth;
        glVertex3f( r0 * cos( angle ), r0 * sin( angle ), width * 0.5 );
        glVertex3f( r1 * cos( angle ), r1 * sin( angle ), width * 0.5 );
        if (i < teeth)
        {
            glVertex3f( r0 * cos( angle ), r0 * sin( angle ), width * 0.5 );
            glVertex3f( r1 * cos( angle + 3 * da ), r1 * sin( angle + 3 * da ), width * 0.5 );
        }
    }
    glEnd();

    /* draw front sides of teeth */
    glBegin( GL_QUADS );
    da = 2.0 * M_PI / teeth / 4.0;
    for (i = 0; i < teeth; i++)
    {
        angle = i * 2.0 * M_PI / teeth;

        glVertex3f( r1 * cos( angle ), r1 * sin( angle ), width * 0.5 );
        glVertex3f( r2 * cos( angle + da ), r2 * sin( angle + da ), width * 0.5 );
        glVertex3f( r2 * cos( angle + 2 * da ), r2 * sin( angle + 2 * da ), width * 0.5 );
        glVertex3f( r1 * cos( angle + 3 * da ), r1 * sin( angle + 3 * da ), width * 0.5 );
    }
    glEnd();

    glNormal3f( 0.0, 0.0, -1.0 );

    /* draw back face */
    glBegin( GL_QUAD_STRIP );
    for (i = 0; i <= teeth; i++)
    {
        angle = i * 2.0 * M_PI / teeth;
        glVertex3f( r1 * cos( angle ), r1 * sin( angle ), -width * 0.5 );
        glVertex3f( r0 * cos( angle ), r0 * sin( angle ), -width * 0.5 );
        if (i < teeth)
        {
            glVertex3f( r1 * cos( angle + 3 * da ), r1 * sin( angle + 3 * da ), -width * 0.5 );
            glVertex3f( r0 * cos( angle ), r0 * sin( angle ), -width * 0.5 );
        }
    }
    glEnd();

    /* draw back sides of teeth */
    glBegin( GL_QUADS );
    da = 2.0 * M_PI / teeth / 4.0;
    for (i = 0; i < teeth; i++)
    {
        angle = i * 2.0 * M_PI / teeth;

        glVertex3f( r1 * cos( angle + 3 * da ), r1 * sin( angle + 3 * da ), -width * 0.5 );
        glVertex3f( r2 * cos( angle + 2 * da ), r2 * sin( angle + 2 * da ), -width * 0.5 );
        glVertex3f( r2 * cos( angle + da ), r2 * sin( angle + da ), -width * 0.5 );
        glVertex3f( r1 * cos( angle ), r1 * sin( angle ), -width * 0.5 );
    }
    glEnd();

    /* draw outward faces of teeth */
    glBegin( GL_QUAD_STRIP );
    for (i = 0; i < teeth; i++)
    {
        angle = i * 2.0 * M_PI / teeth;

        glVertex3f( r1 * cos( angle ), r1 * sin( angle ), width * 0.5 );
        glVertex3f( r1 * cos( angle ), r1 * sin( angle ), -width * 0.5 );
        u = r2 * cos( angle + da ) - r1 * cos( angle );
        v = r2 * sin( angle + da ) - r1 * sin( angle );
        len = sqrt( u * u + v * v );
        u /= len;
        v /= len;
        glNormal3f( v, -u, 0.0 );
        glVertex3f( r2 * cos( angle + da ), r2 * sin( angle + da ), width * 0.5 );
        glVertex3f( r2 * cos( angle + da ), r2 * sin( angle + da ), -width * 0.5 );
        glNormal3f( cos( angle ), sin( angle ), 0.0 );
        glVertex3f( r2 * cos( angle + 2 * da ), r2 * sin( angle + 2 * da ), width * 0.5 );
        glVertex3f( r2 * cos( angle + 2 * da ), r2 * sin( angle + 2 * da ), -width * 0.5 );
        u = r1 * cos( angle + 3 * da ) - r2 * cos( angle + 2 * da );
        v = r1 * sin( angle + 3 * da ) - r2 * sin( angle + 2 * da );
        glNormal3f( v, -u, 0.0 );
        glVertex3f( r1 * cos( angle + 3 * da ), r1 * sin( angle + 3 * da ), width * 0.5 );
        glVertex3f( r1 * cos( angle + 3 * da ), r1 * sin( angle + 3 * da ), -width * 0.5 );
        glNormal3f( cos( angle ), sin( angle ), 0.0 );
    }

    glVertex3f( r1 * cos( 0 ), r1 * sin( 0 ), width * 0.5 );
    glVertex3f( r1 * cos( 0 ), r1 * sin( 0 ), -width * 0.5 );

    glEnd();

    glShadeModel( GL_SMOOTH );

    /* draw inside radius cylinder */
    glBegin( GL_QUAD_STRIP );
    for (i = 0; i <= teeth; i++)
    {
        angle = i * 2.0 * M_PI / teeth;
        glNormal3f( -cos( angle ), -sin( angle ), 0.0 );
        glVertex3f( r0 * cos( angle ), r0 * sin( angle ), -width * 0.5 );
        glVertex3f( r0 * cos( angle ), r0 * sin( angle ), width * 0.5 );
    }
    glEnd();
}

static void init_window_data( struct window_data *data )
{
    static GLfloat pos[4] = {5.0, 5.0, 10.0, 0.0};
    static GLfloat red[4] = {0.8, 0.1, 0.0, 1.0};
    static GLfloat green[4] = {0.0, 0.8, 0.2, 1.0};
    static GLfloat blue[4] = {0.2, 0.2, 1.0, 1.0};
    int i;

    data->view_rotx = 20.0;
    data->view_roty = 30.0;
    data->animate = GL_TRUE;

    data->tRot0 = -1.0;
    data->tRate0 = -1.0;

    glLightfv( GL_LIGHT0, GL_POSITION, pos );
    glEnable( GL_CULL_FACE );
    glEnable( GL_LIGHTING );
    glEnable( GL_LIGHT0 );
    glEnable( GL_DEPTH_TEST );
    if (data->use_srgb)
    {
        for (i = 0; i < 3; ++i)
        {
            red[i] = srgb_to_linear( red[i] );
            green[i] = srgb_to_linear( green[i] );
            blue[i] = srgb_to_linear( blue[i] );
        }
        glEnable( GL_FRAMEBUFFER_SRGB );
    }

    /* make the gears */
    data->gear1 = glGenLists( 1 );
    glNewList( data->gear1, GL_COMPILE );
    glMaterialfv( GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red );
    gear( 1.0, 4.0, 1.0, 20, 0.7 );
    glEndList();

    data->gear2 = glGenLists( 1 );
    glNewList( data->gear2, GL_COMPILE );
    glMaterialfv( GL_FRONT, GL_AMBIENT_AND_DIFFUSE, green );
    gear( 0.5, 2.0, 2.0, 10, 0.7 );
    glEndList();

    data->gear3 = glGenLists( 1 );
    glNewList( data->gear3, GL_COMPILE );
    glMaterialfv( GL_FRONT, GL_AMBIENT_AND_DIFFUSE, blue );
    gear( 1.3, 2.0, 0.5, 10, 0.7 );
    glEndList();

    glEnable( GL_NORMALIZE );
}

static void draw_window_data( struct window_data *data )
{
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    glPushMatrix();
    glRotatef( data->view_rotx, 1.0, 0.0, 0.0 );
    glRotatef( data->view_roty, 0.0, 1.0, 0.0 );
    glRotatef( data->view_rotz, 0.0, 0.0, 1.0 );

    glPushMatrix();
    glTranslatef( -3.0, -2.0, 0.0 );
    glRotatef( data->angle, 0.0, 0.0, 1.0 );
    glCallList( data->gear1 );
    glPopMatrix();

    glPushMatrix();
    glTranslatef( 3.1, -2.0, 0.0 );
    glRotatef( -2.0 * data->angle - 9.0, 0.0, 0.0, 1.0 );
    glCallList( data->gear2 );
    glPopMatrix();

    glPushMatrix();
    glTranslatef( -3.1, 4.2, 0.0 );
    glRotatef( -2.0 * data->angle - 25.0, 0.0, 0.0, 1.0 );
    glCallList( data->gear3 );
    glPopMatrix();

    glPopMatrix();
}

void opengl_draw_frame( HWND hwnd )
{
    struct window_data *data = (void *)GetWindowLongPtrW( hwnd, GWLP_USERDATA );
    double dt, time = current_time();
    HDC hdc = GetDC( hwnd );

    if (data) wglMakeCurrent( hdc, data->hrc );

    if (data->tRot0 < 0.0) data->tRot0 = time;
    dt = time - data->tRot0;
    data->tRot0 = time;

    if (data->animate)
    {
        /* advance rotation for next frame */
        data->angle += 70.0 * dt; /* 70 degrees per second */
        if (data->angle > 3600.0) data->angle -= 3600.0;
    }

    draw_window_data( data );
    SwapBuffers( hdc );
    DeleteDC( hdc );

    data->frames++;

    if (data->tRate0 < 0.0) data->tRate0 = time;
    if (time - data->tRate0 >= 5.0 && verbose)
    {
        GLfloat seconds = time - data->tRate0;
        GLfloat fps = data->frames / seconds;
        printf( "%d frames in %3.1f seconds = %6.3f FPS\n", data->frames, seconds, fps );
        fflush( stdout );
        data->tRate0 = time;
        data->frames = 0;
    }
}

/* new window size or exposure */
static void reshape( int width, int height )
{
    GLfloat h = (GLfloat)height / (GLfloat)width;

    glViewport( 0, 0, (GLint)width, (GLint)height );
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    glFrustum( -1.0, 1.0, -h, h, 5.0, 60.0 );
    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();
    glTranslatef( 0.0, 0.0, -40.0 );
}

static LRESULT CALLBACK opengl_window_proc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    struct window_data *data = (void *)GetWindowLongPtrW( hwnd, GWLP_USERDATA );

    switch (msg)
    {
    case WM_CLOSE:
        PostQuitMessage( 0 );
        return 0;

    case WM_SIZE:
        /* This can be reached before wglMakeCurrent */
        if (data)
        {
            HDC hdc = GetDC( hwnd );
            wglMakeCurrent( hdc, data->hrc );
            reshape( LOWORD(lparam), HIWORD(lparam) );
            DeleteDC( hdc );
        }
        return 0;

    case WM_KEYDOWN:
        if (!data) break;
        if (wparam == VK_LEFT) data->view_roty += 5.0;
        else if (wparam == VK_RIGHT) data->view_roty -= 5.0;
        else if (wparam == VK_UP) data->view_rotx += 5.0;
        else if (wparam == VK_DOWN) data->view_rotx -= 5.0;
        else if (wparam == VK_ESCAPE) PostQuitMessage( 0 );
        else if (wparam == 'A') data->animate = !data->animate;
        return 0;

#if WINVER >= 0x0605
    case WM_NCCREATE: EnableNonClientDpiScaling( hwnd ); break;
#endif
    }

    return DefWindowProcW( hwnd, msg, wparam, lparam );
}

static const PIXELFORMATDESCRIPTOR pfd =
{
    .nSize = sizeof(PIXELFORMATDESCRIPTOR),
    .nVersion = 1,
    .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
    .iPixelType = PFD_TYPE_RGBA,
    .cColorBits = 24,
    .cDepthBits = 16,
    .iLayerType = PFD_MAIN_PLANE,
};

static WNDCLASSW wc =
{
    .style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
    .lpfnWndProc = (WNDPROC)opengl_window_proc,
    .lpszClassName = L"OpenGL",
};

HWND opengl_create_window( const WCHAR *name, RECT *geometry, BOOL fullscreen, INT samples, BOOL use_srgb )
{
    DWORD ex_style = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE, style = WS_OVERLAPPEDWINDOW;
    RECT window_rect = *geometry;
    struct window_data *data;
    INT format;
    HDC hdc;

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

    data->hwnd = CreateWindowExW( ex_style, wc.lpszClassName, name, WS_CLIPSIBLINGS | WS_CLIPCHILDREN | style,
                                  geometry->left, geometry->top, window_rect.right - window_rect.left,
                                  window_rect.bottom - window_rect.top, NULL, NULL, NULL, NULL );
    hdc = GetDC( data->hwnd );

    if (!(format = ChoosePixelFormat( hdc, &pfd ))) goto failed;

    if ((data->use_srgb = use_srgb) || samples > 0)
    {
        static const float float_attribs[] = {0};
        GLint int_attribs[64] =
        {
            WGL_SUPPORT_OPENGL_ARB, TRUE,
            WGL_DRAW_TO_WINDOW_ARB, TRUE,
            WGL_COLOR_BITS_ARB, 24,
            WGL_DEPTH_BITS_ARB, 24,
            WGL_DOUBLE_BUFFER_ARB, TRUE,
            WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, TRUE,
        };
        PIXELFORMATDESCRIPTOR newPfd;
        UINT numFormats;
        int i = 10;

        /* We can't query/use extension functions until after we've
         * created and bound a rendering context (done above).
         *
         * We can only set the pixel format of the window once, so we need to
         * create a new device context in order to use the pixel format returned
         * from wglChoosePixelFormatARB, and then create a new window.
         */
        assert( GLAD_WGL_ARB_create_context );

        if (data->use_srgb)
        {
            int_attribs[i++] = WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB;
            int_attribs[i++] = TRUE;
        }
        if (samples > 0)
        {
            int_attribs[i++] = WGL_SAMPLE_BUFFERS_ARB;
            int_attribs[i++] = 1;
            int_attribs[i++] = WGL_SAMPLES_ARB;
            int_attribs[i++] = samples;
        }
        int_attribs[i++] = 0;

        if (!wglChoosePixelFormatARB( hdc, int_attribs, float_attribs, 1, &format, &numFormats ) || !numFormats)
            goto failed;
        DescribePixelFormat( hdc, format, sizeof(pfd), &newPfd );
    }

    SetPixelFormat( hdc, format, &pfd );
    data->hrc = wglCreateContext( hdc );
    wglMakeCurrent( hdc, data->hrc );

    init_window_data( data );
    SetWindowLongPtrW( data->hwnd, GWLP_USERDATA, (LONG_PTR)data );
    DeleteDC( hdc );

    return data->hwnd;

failed:
    wglMakeCurrent( hdc, NULL );
    DeleteDC( hdc );

    DestroyWindow( data->hwnd );
    free( data );
    return 0;
}

void opengl_destroy_window( HWND hwnd )
{
    struct window_data *data = (void *)GetWindowLongPtrW( hwnd, GWLP_USERDATA );
    HDC hdc;

    if (!data) return;

    hdc = GetDC( hwnd );
    wglMakeCurrent( hdc, NULL );
    wglDeleteContext( data->hrc );
    DeleteDC( hdc );

    DestroyWindow( data->hwnd );
    free( data );
}

BOOL opengl_init(void)
{
    INT format;
    HGLRC hrc;
    HWND hwnd;
    HDC hdc;

    wc.hInstance = GetModuleHandleA( NULL );
    wc.hIcon = LoadIconW( NULL, MAKEINTRESOURCEW( IDI_WINLOGO ) );
    wc.hCursor = LoadCursorW( NULL, MAKEINTRESOURCEW( IDC_ARROW ) );
    RegisterClassW( &wc );

    hwnd = CreateWindowW( L"static", L"static", WS_POPUP, 0, 0, 1, 1, NULL, NULL, wc.hInstance, NULL );

    hdc = GetDC( hwnd );
    if (!(format = ChoosePixelFormat( hdc, &pfd ))) return FALSE;

    SetPixelFormat( hdc, format, &pfd );
    hrc = wglCreateContext( hdc );
    wglMakeCurrent( hdc, hrc );

    gladLoadWGL( hdc );
    gladLoadGL();

    wglMakeCurrent( hdc, NULL );
    wglDeleteContext( hrc );
    DeleteDC( hdc );
    DestroyWindow( hwnd );

    return TRUE;
}
