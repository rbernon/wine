/*
 * Vulkan display driver loading
 *
 * Copyright (c) 2017 Roderick Colenbrander
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

#include <assert.h>
#include <dlfcn.h>
#include <pthread.h>
#ifdef HAVE_EGL_EGL_H
#include <EGL/egl.h>
#define EGL_EGLEXT_PROTOTYPES
#include <EGL/eglext.h>
#endif

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "ntgdi_private.h"
#include "win32u_private.h"
#include "ntuser_private.h"
#include "dibdrv/dibdrv.h"

#include "wine/wgl.h"
#include "wine/wgl_driver.h"

WINE_DEFAULT_DEBUG_CHANNEL(wgl);

static struct opengl_funcs *display_funcs;
static struct opengl_funcs *memory_funcs;

static void *egl_handle;
static struct opengl_funcs egl_funcs;

#define DECL_FUNCPTR(f) static typeof(f) *p_##f
#ifdef HAVE_EGL_EGL_H
DECL_FUNCPTR( eglGetProcAddress );
#endif /* HAVE_EGL_EGL_H */
#undef DECL_FUNCPTR

static int win32u_wglDescribePixelFormat( HDC hdc, int index, UINT size, PIXELFORMATDESCRIPTOR *desc )
{
    FIXME( "hdc %p, index %d, size %u, desc %p\n", hdc, index, size, desc );
    return 0;
}

static int win32u_wglGetPixelFormat( HDC hdc )
{
    FIXME( "hdc %p, stub!\n", hdc );
    return 0;
}

static BOOL win32u_wglSetPixelFormat( HDC hdc, int index, const PIXELFORMATDESCRIPTOR *desc )
{
    FIXME( "hdc %p, index %d, desc %p, stub!\n", hdc, index, desc );
    return FALSE;
}

static PROC win32u_wglGetProcAddress( const char *proc )
{
    TRACE( "name %s\n", debugstr_a(proc) );
    if (!strcmp( proc, "wglGetProcAddress" )) return (PROC)win32u_wglGetProcAddress;
    return (PROC)p_eglGetProcAddress( proc );
}

static struct wgl_context *win32u_wglCreateContext( HDC hdc )
{
    FIXME( "hdc %p\n", hdc );
    return NULL;
}

static BOOL win32u_wglDeleteContext( struct wgl_context *context )
{
    FIXME( "context %p\n", context );
    return FALSE;
}

static BOOL win32u_wglCopyContext( struct wgl_context *src, struct wgl_context *dst, UINT mask )
{
    FIXME( "src %p, dst %p, mask %#x, stub!\n", src, dst, mask );
    return FALSE;
}

static BOOL win32u_wglMakeCurrent( HDC hdc, struct wgl_context *context )
{
    FIXME( "hdc %p, context %p\n, stub!", hdc, context );
    return FALSE;
}

static BOOL win32u_wglShareLists( struct wgl_context *dst, struct wgl_context *src )
{
    FIXME( "dst %p, src %p, stub!\n", dst, src );
    return FALSE;
}

static BOOL win32u_wglSwapBuffers( HDC hdc )
{
    FIXME( "hdc %p\n", hdc );
    return FALSE;
}

static void egl_init(void)
{
#ifdef SONAME_LIBEGL
    /*setenv( "EGL_PLATFORM", "surfaceless", TRUE );*/
    /*setenv( "EGL_PLATFORM", "device", TRUE );*/
    /*setenv( "EGL_PLATFORM", "gbm", TRUE );*/
    setenv( "EGL_LOG_LEVEL", "debug", TRUE );

    if (!(egl_handle = dlopen( SONAME_LIBEGL, RTLD_NOW | RTLD_GLOBAL )))
    {
        ERR( "failed to load %s: %s\n", SONAME_LIBEGL, dlerror() );
        goto failed;
    }

#define LOAD_FUNCPTR( name )                                                                       \
    if (!(p_##name = dlsym( egl_handle, #name )))                                                  \
    {                                                                                              \
        ERR( "can't find EGL proc %s\n", #name );                                                  \
        goto failed;                                                                               \
    }
    LOAD_FUNCPTR( eglGetProcAddress )
#undef LOAD_FUNCPTR

    egl_funcs.wgl.p_wglCopyContext = win32u_wglCopyContext;
    egl_funcs.wgl.p_wglCreateContext = win32u_wglCreateContext;
    egl_funcs.wgl.p_wglDeleteContext = win32u_wglDeleteContext;
    egl_funcs.wgl.p_wglDescribePixelFormat = win32u_wglDescribePixelFormat;
    egl_funcs.wgl.p_wglGetPixelFormat = win32u_wglGetPixelFormat;
    egl_funcs.wgl.p_wglGetProcAddress = win32u_wglGetProcAddress;
    egl_funcs.wgl.p_wglMakeCurrent = win32u_wglMakeCurrent;
    egl_funcs.wgl.p_wglSetPixelFormat = win32u_wglSetPixelFormat;
    egl_funcs.wgl.p_wglShareLists = win32u_wglShareLists;
    egl_funcs.wgl.p_wglSwapBuffers = win32u_wglSwapBuffers;
    return;

failed:
    dlclose( egl_handle );
    egl_handle = NULL;
#else  /* SONAME_LIBEGL */
    ERR( "EGL support not compiled in\n" );
#endif /* SONAME_LIBEGL */
}

static void opengl_init(void)
{
    display_funcs = user_driver->pwine_get_wgl_driver( WINE_WGL_DRIVER_VERSION );
    if (display_funcs == (void *)-1) display_funcs = NULL;
    memory_funcs = dibdrv_get_wgl_driver();
    if (memory_funcs == (void *)-1) memory_funcs = NULL;

    if (!display_funcs || !memory_funcs) egl_init();
    if (!display_funcs && egl_handle) display_funcs = &egl_funcs;
    if (!memory_funcs && egl_handle) memory_funcs = &egl_funcs;
}

/***********************************************************************
 *      __wine_get_wgl_driver  (win32u.@)
 */
const struct opengl_funcs *__wine_get_wgl_driver( HDC hdc, UINT version )
{
    static pthread_once_t init_once = PTHREAD_ONCE_INIT;
    DWORD is_display, is_memdc;

    if (version != WINE_WGL_DRIVER_VERSION)
    {
        ERR( "version mismatch, opengl32 wants %u but dibdrv has %u\n",
             version, WINE_WGL_DRIVER_VERSION );
        return NULL;
    }

    pthread_once( &init_once, opengl_init );

    if (!NtGdiGetDCDword( hdc, NtGdiIsMemDC, &is_memdc )) return NULL;
    if (!NtGdiGetDCDword( hdc, NtGdiIsDisplay, &is_display )) return NULL;
    if (is_display && display_funcs) return display_funcs;
    if (is_memdc && memory_funcs) return memory_funcs;
    return (void *)-1;
}
