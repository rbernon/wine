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

struct wgl_context
{
    EGLContext *host_context;
    EGLDisplay *display;

    GLuint framebuffer;
    GLuint colorbuffer;
};

struct extension_buffer
{
    char buffer[4096];
    unsigned int length;
};

static void *egl_handle;
static struct opengl_funcs egl_funcs;
static struct extension_buffer egl_extensions;

struct egl_context
{
    EGLDisplay display;
    EGLConfig *configs;
    UINT pixel_format_count;
    PIXELFORMATDESCRIPTOR *pixel_formats;
} display_egl, memory_egl;

#define DECL_FUNCPTR(f) static typeof(f) *p_##f
#ifdef HAVE_EGL_EGL_H
DECL_FUNCPTR( eglCreateContext );
DECL_FUNCPTR( eglDestroyContext );
DECL_FUNCPTR( eglGetConfigAttrib );
DECL_FUNCPTR( eglGetPlatformDisplay );
DECL_FUNCPTR( eglGetConfigs );
DECL_FUNCPTR( eglGetDisplay );
DECL_FUNCPTR( eglGetProcAddress );
DECL_FUNCPTR( eglBindAPI );
DECL_FUNCPTR( eglInitialize );
DECL_FUNCPTR( eglMakeCurrent );
DECL_FUNCPTR( eglQueryString );
DECL_FUNCPTR( eglSwapBuffers );
DECL_FUNCPTR( eglQueryDevicesEXT );
DECL_FUNCPTR( eglQueryDeviceStringEXT );
#endif /* HAVE_EGL_EGL_H */
#undef DECL_FUNCPTR

#define DECL_FUNCPTR( ns, f ) static typeof(egl_funcs.ns.p_##f) p_##f
DECL_FUNCPTR( gl, glFinish );
DECL_FUNCPTR( gl, glFlush );
DECL_FUNCPTR( gl, glGetString );
DECL_FUNCPTR( ext, glBindFramebuffer );
#undef DECL_FUNCPTR

static BOOL init_extensions( struct extension_buffer *extensions, const char *str )
{
    if (!str) return FALSE;
    extensions->length = sprintf( extensions->buffer, " %s", str );
    return TRUE;
}

static BOOL has_extension( struct extension_buffer *extensions, const char *name )
{
    char lookup[256], *tmp;
    unsigned int len = sprintf( lookup, " %s", name );
    if (!(tmp = strstr( extensions->buffer, lookup ))) return FALSE;
    if (tmp[len] && tmp[len] != ' ') return FALSE;
    return TRUE;
}

static void init_pixel_formats( struct egl_context *egl, EGLConfig *configs, UINT config_count )
{
    static const struct
    {
        BYTE color_bits;
        BYTE red_bits, red_shift;
        BYTE green_bits, green_shift;
        BYTE blue_bits, blue_shift;
        BYTE alpha_bits, alpha_shift;
        BYTE accum_bits;
        BYTE depth_bits;
        BYTE stencil_bits;
    } generic_pixel_formats[] =
    {
        { 32,  8, 16, 8, 8,  8, 0,  8, 24,  16, 32, 8 },
        { 32,  8, 16, 8, 8,  8, 0,  8, 24,  16, 16, 8 },
        { 32,  8, 0,  8, 8,  8, 16, 8, 24,  16, 32, 8 },
        { 32,  8, 0,  8, 8,  8, 16, 8, 24,  16, 16, 8 },
        { 32,  8, 8,  8, 16, 8, 24, 8, 0,   16, 32, 8 },
        { 32,  8, 8,  8, 16, 8, 24, 8, 0,   16, 16, 8 },
        { 24,  8, 0,  8, 8,  8, 16, 0, 0,   16, 32, 8 },
        { 24,  8, 0,  8, 8,  8, 16, 0, 0,   16, 16, 8 },
        { 24,  8, 16, 8, 8,  8, 0,  0, 0,   16, 32, 8 },
        { 24,  8, 16, 8, 8,  8, 0,  0, 0,   16, 16, 8 },
        { 16,  5, 0,  6, 5,  5, 11, 0, 0,   16, 32, 8 },
        { 16,  5, 0,  6, 5,  5, 11, 0, 0,   16, 16, 8 },
    };
    PIXELFORMATDESCRIPTOR *desc;
    UINT i, count;
    EGLint value;

    count = config_count + ARRAY_SIZE(generic_pixel_formats);
    if (!(egl->pixel_formats = calloc( count, sizeof(*egl->pixel_formats) ))) return;
    desc = egl->pixel_formats;

    for (i = 0; i < config_count; i++, desc++)
    {
        desc->nSize = sizeof(*desc);
        desc->nVersion = 1;

        desc->dwFlags = PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_SUPPORT_COMPOSITION;
        desc->iPixelType = PFD_TYPE_RGBA;
        desc->iLayerType = PFD_MAIN_PLANE;

        if (p_eglGetConfigAttrib( egl->display, configs[i], EGL_RENDERABLE_TYPE, &value ))
        {
            if (value & EGL_OPENGL_BIT) desc->dwFlags |= PFD_SUPPORT_OPENGL;
        }
        if (p_eglGetConfigAttrib( egl->display, configs[i], EGL_SURFACE_TYPE, &value ))
        {
            if (value & EGL_PBUFFER_BIT) desc->dwFlags |= PFD_DRAW_TO_BITMAP;
            if (value & EGL_PIXMAP_BIT) desc->dwFlags |= PFD_DRAW_TO_BITMAP;
            if (value & EGL_WINDOW_BIT) desc->dwFlags |= PFD_DRAW_TO_WINDOW;
        }
        if (p_eglGetConfigAttrib( egl->display, configs[i], EGL_COLOR_BUFFER_TYPE, &value ))
        {
            if (value == EGL_RGB_BUFFER) desc->iPixelType = PFD_TYPE_RGBA;
            else desc->iPixelType = PFD_TYPE_COLORINDEX;
        }

        if (p_eglGetConfigAttrib( egl->display, configs[i], EGL_BUFFER_SIZE, &value )) desc->cColorBits = value;
        if (p_eglGetConfigAttrib( egl->display, configs[i], EGL_RED_SIZE, &value )) desc->cRedBits = value;
        if (p_eglGetConfigAttrib( egl->display, configs[i], EGL_GREEN_SIZE, &value )) desc->cGreenBits = value;
        if (p_eglGetConfigAttrib( egl->display, configs[i], EGL_BLUE_SIZE, &value )) desc->cBlueBits = value;
        if (p_eglGetConfigAttrib( egl->display, configs[i], EGL_ALPHA_SIZE, &value )) desc->cAlphaBits = value;
        if (p_eglGetConfigAttrib( egl->display, configs[i], EGL_DEPTH_SIZE, &value )) desc->cDepthBits = value;
        if (p_eglGetConfigAttrib( egl->display, configs[i], EGL_STENCIL_SIZE, &value )) desc->cStencilBits = value;

        desc->cAlphaShift = 0;
        desc->cBlueShift = desc->cAlphaShift + desc->cAlphaBits;
        desc->cGreenShift = desc->cBlueShift + desc->cBlueBits;
        desc->cRedShift = desc->cGreenShift + desc->cGreenBits;

        /* FIXME: set accum buffer bits? */

        if (!display_funcs) desc->dwFlags |= PFD_DRAW_TO_WINDOW;

        TRACE( "config %u color %u %u/%u/%u/%u depth %u stencil %u\n",
               i, desc->cColorBits, desc->cRedBits, desc->cGreenBits, desc->cBlueBits,
               desc->cAlphaBits, desc->cDepthBits, desc->cStencilBits );
    }

    for (i = 0; i < ARRAY_SIZE(generic_pixel_formats); i++, desc++)
    {
        desc->nSize            = sizeof(*desc);
        desc->nVersion         = 1;
        desc->dwFlags          = PFD_SUPPORT_GDI | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_BITMAP | PFD_GENERIC_FORMAT | PFD_GENERIC_ACCELERATED;
        desc->iPixelType       = PFD_TYPE_RGBA;
        desc->cColorBits       = generic_pixel_formats[i].color_bits;
        desc->cRedBits         = generic_pixel_formats[i].red_bits;
        desc->cRedShift        = generic_pixel_formats[i].red_shift;
        desc->cGreenBits       = generic_pixel_formats[i].green_bits;
        desc->cGreenShift      = generic_pixel_formats[i].green_shift;
        desc->cBlueBits        = generic_pixel_formats[i].blue_bits;
        desc->cBlueShift       = generic_pixel_formats[i].blue_shift;
        desc->cAlphaBits       = generic_pixel_formats[i].alpha_bits;
        desc->cAlphaShift      = generic_pixel_formats[i].alpha_shift;
        desc->cAccumBits       = generic_pixel_formats[i].accum_bits;
        desc->cAccumRedBits    = generic_pixel_formats[i].accum_bits / 4;
        desc->cAccumGreenBits  = generic_pixel_formats[i].accum_bits / 4;
        desc->cAccumBlueBits   = generic_pixel_formats[i].accum_bits / 4;
        desc->cAccumAlphaBits  = generic_pixel_formats[i].accum_bits / 4;
        desc->cDepthBits       = generic_pixel_formats[i].depth_bits;
        desc->cStencilBits     = generic_pixel_formats[i].stencil_bits;
        desc->cAuxBuffers      = 0;
        desc->iLayerType       = PFD_MAIN_PLANE;

        TRACE( "generic %u color %u %u/%u/%u/%u depth %u stencil %u\n",
               i, desc->cColorBits, desc->cRedBits, desc->cGreenBits, desc->cBlueBits,
               desc->cAlphaBits, desc->cDepthBits, desc->cStencilBits );
    }

    egl->pixel_format_count = desc - egl->pixel_formats;
}

static struct egl_context *egl_context_from_hdc( HDC hdc )
{
    DWORD is_memdc;

    if (!NtGdiGetDCDword( hdc, NtGdiIsMemDC, &is_memdc ) || !is_memdc)
        return &display_egl;

    return &memory_egl;
}

static const char *win32u_wglGetExtensionsStringEXT(void)
{
    TRACE( "returning \"%s\"\n", egl_extensions.buffer + 1 );
    return egl_extensions.buffer + 1;
}

static const char *win32u_wglGetExtensionsStringARB( HDC hdc )
{
    TRACE( "hdc %p\n", hdc );
    return win32u_wglGetExtensionsStringEXT();
}

static int win32u_wglDescribePixelFormat( HDC hdc, int index, UINT size, PIXELFORMATDESCRIPTOR *desc )
{
    struct egl_context *egl = egl_context_from_hdc( hdc );

    FIXME( "hdc %p, index %d, size %#x, desc %p\n", hdc, index, size, desc );

    if (desc && size >= sizeof(*desc) && index <= egl->pixel_format_count)
        *desc = egl->pixel_formats[index - 1];
    return egl->pixel_format_count;
}

static int win32u_wglGetPixelFormat( HDC hdc )
{
    DWORD index;
    FIXME( "hdc %p\n", hdc );
    if (!NtGdiGetDCDword( hdc, NtGdiGetPixelFormat, &index )) return 0;
    return index;
}

static BOOL win32u_wglSetPixelFormat( HDC hdc, int index, const PIXELFORMATDESCRIPTOR *desc )
{
    struct egl_context *egl = egl_context_from_hdc( hdc );

    FIXME( "hdc %p, index %d, desc %p\n", hdc, index, desc );

    if (index <= 0 || index > egl->pixel_format_count) return FALSE;
    return NtGdiSetPixelFormat( hdc, index );
}

static PROC win32u_wglGetProcAddress( const char *proc )
{
    TRACE( "%s\n", debugstr_a( proc ) );
    if (!strcmp( proc, "wglGetProcAddress" )) return (PROC)win32u_wglGetProcAddress;
    if (!strcmp( proc, "wglGetExtensionsStringEXT" )) return (PROC)win32u_wglGetExtensionsStringEXT;
    if (!strcmp( proc, "wglGetExtensionsStringARB" )) return (PROC)win32u_wglGetExtensionsStringARB;
    return (PROC)p_eglGetProcAddress( proc );
}

static struct wgl_context *win32u_wglCreateContext( HDC hdc )
{
    struct egl_context *egl = egl_context_from_hdc( hdc );
    struct wgl_context *context;

    FIXME( "hdc %p\n", hdc );

    if (!p_eglBindAPI( EGL_OPENGL_API )) return NULL;

    if (!win32u_wglGetPixelFormat( hdc ))
    {
        RtlSetLastWin32Error( ERROR_INVALID_PIXEL_FORMAT );
        return NULL;
    }

    if (!(context = calloc( 1, sizeof(*context) ))) return NULL;
    if (!(context->host_context = p_eglCreateContext( egl->display, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, NULL )))
    {
        free( context );
        return NULL;
    }
    context->display = egl->display;

    TRACE( "created context %p, host context %p\n", context, context->host_context );
    return context;
}

static BOOL win32u_wglDeleteContext( struct wgl_context *context )
{
    FIXME( "context %p\n", context );
    p_eglDestroyContext( context->display, context->host_context );
    free( context );
    return TRUE;
}

static BOOL win32u_wglCopyContext( struct wgl_context *src, struct wgl_context *dst, UINT mask )
{
    FIXME( "src %p, dst %p, mask %#x, stub!\n", src, dst, mask );
    if (!p_eglBindAPI( EGL_OPENGL_API )) return FALSE;
    return FALSE;
}

static BOOL win32u_wglMakeCurrent( HDC hdc, struct wgl_context *context )
{
    struct egl_context *egl = egl_context_from_hdc( hdc );
    HBITMAP handle;
    BITMAP bm;

    FIXME( "hdc %p, context %p\n, stub!", hdc, context );

    if (!p_eglBindAPI( EGL_OPENGL_API )) return FALSE;

    NtCurrentTeb()->glContext = context;
    if (!context) return p_eglMakeCurrent( egl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
    if (!p_eglMakeCurrent( egl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, context->host_context )) return FALSE;

    if (egl == &memory_egl && !context->framebuffer)
    {
        if ((handle = NtGdiGetDCObject( hdc, NTGDI_OBJ_SURF )))
            NtGdiExtGetObjectW( handle, sizeof(BITMAP), &bm );

        egl_funcs.gl.p_glGenTextures( 1, &context->colorbuffer );
        assert( !egl_funcs.gl.p_glGetError() );
        egl_funcs.gl.p_glBindTexture( GL_TEXTURE_2D, context->colorbuffer );
        assert( !egl_funcs.gl.p_glGetError() );
        egl_funcs.gl.p_glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, bm.bmWidth, bm.bmHeight,
                                     0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
        assert( !egl_funcs.gl.p_glGetError() );

        egl_funcs.ext.p_glGenFramebuffers( 1, &context->framebuffer );
        assert( !egl_funcs.gl.p_glGetError() );
        egl_funcs.ext.p_glBindFramebuffer( GL_FRAMEBUFFER, context->framebuffer );
        assert( !egl_funcs.gl.p_glGetError() );
        egl_funcs.ext.p_glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                                context->colorbuffer, 0 );
        assert( !egl_funcs.gl.p_glGetError() );

        egl_funcs.gl.p_glViewport( 0, 0, bm.bmWidth, bm.bmHeight );
        assert( !egl_funcs.gl.p_glGetError() );
    }

    return TRUE;
}

static BOOL win32u_wglShareLists( struct wgl_context *dst, struct wgl_context *src )
{
    FIXME( "dst %p, src %p, stub!\n", dst, src );
    return FALSE;
}

static BOOL win32u_wglSwapBuffers( HDC hdc )
{
    struct wgl_context *context = NtCurrentTeb()->glContext;

    FIXME( "hdc %p\n", hdc );

    if (!context || context->framebuffer) return TRUE;
    return p_eglSwapBuffers( context->display, EGL_NO_SURFACE );
}

static void read_pixels_to_memory_dc( HDC hdc )
{
    HBITMAP bitmap;
    BITMAPOBJ *bmp;
    dib_info dib;

    bitmap = NtGdiGetDCObject( hdc, NTGDI_OBJ_SURF );
    bmp = GDI_GetObjPtr( bitmap, NTGDI_OBJ_BITMAP );
    if (!bmp) return;

    if (init_dib_info_from_bitmapobj( &dib, bmp ))
    {
        int width, height;
        char *bits;

        width = dib.rect.right - dib.rect.left;
        height = dib.rect.bottom - dib.rect.top;
        if (dib.stride < 0) bits = (char *)dib.bits.ptr + (dib.rect.bottom - 1) * dib.stride;
        else bits = (char *)dib.bits.ptr + dib.rect.top * dib.stride;
        bits += dib.rect.left * dib.bit_count / 8;

        ERR( "hdc %p bpp %u bits %p size %ux%u stride %d\n", hdc, dib.bit_count, bits, width, height, dib.stride );

        egl_funcs.gl.p_glReadPixels( 0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE, bits );
        assert( !egl_funcs.gl.p_glGetError() );
    }

    GDI_ReleaseObj( bitmap );
}

static void win32u_glFinish(void)
{
    struct wgl_context *context = NtCurrentTeb()->glContext;
    HDC hdc = NtCurrentTeb()->glReserved1[0];
    DWORD is_memdc;

    ERR( "\n" );

    p_glFinish();

    if (context && NtGdiGetDCDword( hdc, NtGdiIsMemDC, &is_memdc ) && is_memdc)
        read_pixels_to_memory_dc( hdc );
}

static void win32u_glFlush(void)
{
    struct wgl_context *context = NtCurrentTeb()->glContext;
    HDC hdc = NtCurrentTeb()->glReserved1[0];
    DWORD is_memdc;

    ERR( "\n" );

    p_glFlush();

    if (context && NtGdiGetDCDword( hdc, NtGdiIsMemDC, &is_memdc ) && is_memdc)
        read_pixels_to_memory_dc( hdc );
}

static const GLubyte *win32u_glGetString( GLenum name )
{
    TRACE( "name %#x\n", name );
    return p_glGetString( name );
}

static void win32u_glBindFramebuffer( GLenum target, GLuint framebuffer )
{
    struct wgl_context *context = NtCurrentTeb()->glContext;
    if (context && !framebuffer) framebuffer = context->framebuffer;
    p_glBindFramebuffer( target, framebuffer );
}

static void egl_init(void)
{
#ifdef SONAME_LIBEGL
    struct extension_buffer extensions;
    EGLDeviceEXT *devices = NULL;
    GLint i, count;

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
    LOAD_FUNCPTR( eglCreateContext )
    LOAD_FUNCPTR( eglDestroyContext )
    LOAD_FUNCPTR( eglGetConfigAttrib )
    LOAD_FUNCPTR( eglGetPlatformDisplay )
    LOAD_FUNCPTR( eglGetConfigs )
    LOAD_FUNCPTR( eglGetProcAddress )
    LOAD_FUNCPTR( eglGetDisplay )
    LOAD_FUNCPTR( eglInitialize )
    LOAD_FUNCPTR( eglBindAPI )
    LOAD_FUNCPTR( eglMakeCurrent )
    LOAD_FUNCPTR( eglQueryString )
    LOAD_FUNCPTR( eglSwapBuffers )
#undef LOAD_FUNCPTR

    if (!init_extensions( &extensions, p_eglQueryString( EGL_NO_DISPLAY, EGL_EXTENSIONS )))
    {
        ERR( "can't list client extensions\n" );
        goto failed;
    }
    ERR( "client extensions %s\n", extensions.buffer + 1 );

#define CHECK_EXTENSION( category, name )                                                          \
    if (!has_extension( &extensions, name ))                                                       \
    {                                                                                              \
        ERR( "missing " category " extension %s\n", name );                                        \
        goto failed;                                                                               \
    }
    CHECK_EXTENSION( "client", "EGL_EXT_client_extensions" );

    /* use eglGetProcAddress for core GL functions */
    CHECK_EXTENSION( "client", "EGL_KHR_client_get_all_proc_addresses" );

    CHECK_EXTENSION( "client", "EGL_EXT_device_base" );
    CHECK_EXTENSION( "client", "EGL_EXT_device_query" );
    CHECK_EXTENSION( "client", "EGL_EXT_device_enumeration" );
    CHECK_EXTENSION( "client", "EGL_EXT_platform_base" );
    CHECK_EXTENSION( "client", "EGL_EXT_platform_device" );

#define LOAD_FUNCPTR( name )                                                                       \
    if (!(p_##name = (void *)p_eglGetProcAddress( #name )))                                        \
    {                                                                                              \
        ERR( "can't find symbol %s\n", #name );                                                    \
        goto failed;                                                                               \
    }
    LOAD_FUNCPTR( eglQueryDevicesEXT )
    LOAD_FUNCPTR( eglQueryDeviceStringEXT )
#undef LOAD_FUNCPTR

#define USE_GL_FUNC( name )                                                                        \
    if (!(egl_funcs.gl.p_##name = (void *)p_eglGetProcAddress( #name )))                           \
    {                                                                                              \
        ERR( "can't find GL proc %s\n", #name );                                                   \
        goto failed;                                                                               \
    }
    ALL_WGL_FUNCS
#undef USE_GL_FUNC

#define USE_EXT_FUNC( name )                                                                       \
    if (!(egl_funcs.ext.p_##name = (void *)p_eglGetProcAddress( #name )))                          \
    {                                                                                              \
        ERR( "can't find GL proc %s\n", #name );                                                   \
        goto failed;                                                                               \
    }
    USE_EXT_FUNC( glGenFramebuffers )
    USE_EXT_FUNC( glBindFramebuffer )
    USE_EXT_FUNC( glFramebufferTexture2D )
#undef USE_EXT_FUNC

#define OVERIDE_FUNC( ns, name )                                                                   \
    p_##name = egl_funcs.ns.p_##name;                                                              \
    egl_funcs.ns.p_##name = win32u_##name;
    OVERIDE_FUNC( gl, glFinish );
    OVERIDE_FUNC( gl, glFlush );
    OVERIDE_FUNC( gl, glGetString );
    OVERIDE_FUNC( ext, glBindFramebuffer );
#undef OVERIDE_FUNC

    if (!p_eglQueryDevicesEXT( 0, NULL, &count ) || !count) goto failed;
    if (!(devices = malloc( count * sizeof(*devices) ))) goto failed;
    if (!p_eglQueryDevicesEXT( count, devices, &count )) goto failed;

    ERR( "found %u EGL devices\n", count );
    for (i = 0; i < count; ++i)
    {
        EGLDisplay display;
        GLint major, minor;
        EGLConfig *configs;

        if (!init_extensions( &extensions, p_eglQueryDeviceStringEXT( devices[i], EGL_EXTENSIONS ))) continue;
        ERR( "device %u extensions %s\n", i, extensions.buffer + 1 );

        if (!(display = p_eglGetPlatformDisplay( EGL_PLATFORM_DEVICE_EXT, devices[i], NULL ))) continue;
        if (!p_eglInitialize( display, &major, &minor )) continue;

        if (!init_extensions( &extensions, p_eglQueryString( display, EGL_EXTENSIONS )))
        {
            ERR( "can't list display extensions\n" );
            goto failed;
        }

        ERR( "display extensions %s\n", extensions.buffer + 1 );
        CHECK_EXTENSION( "display", "EGL_KHR_no_config_context" );
        CHECK_EXTENSION( "display", "EGL_KHR_surfaceless_context" );

        if (!p_eglGetConfigs( display, NULL, 0, &count )) goto failed;
        if (!(configs = malloc( count * sizeof(*configs) ))) goto failed;
        if (!(p_eglGetConfigs( display, configs, count, &count ))) goto failed;

        memory_egl.display = display;
        memory_egl.configs = configs;
        init_pixel_formats( &memory_egl, configs, count );
        break;
    }
#undef CHECK_EXTENSION

    if (i == count) goto failed;
    free( devices );

    if ((display_egl.display = p_eglGetDisplay( EGL_DEFAULT_DISPLAY )))
    {
        EGLConfig *configs;
        GLint major, minor;

        if (!p_eglInitialize( display_egl.display, &major, &minor )) goto failed;
        if (!init_extensions( &extensions, p_eglQueryString( display_egl.display, EGL_EXTENSIONS )))
        {
            ERR( "can't list display extensions\n" );
            goto failed;
        }

        ERR( "display extensions %s\n", extensions.buffer + 1 );

        if (!p_eglGetConfigs( display_egl.display, NULL, 0, &count )) goto failed;
        if (!(configs = malloc( count * sizeof(*configs) ))) goto failed;
        if (!(p_eglGetConfigs( display_egl.display, configs, count, &count ))) goto failed;

        display_egl.configs = configs;
        init_pixel_formats( &display_egl, configs, count );
    }

    /* check extensions:
    EGL_KHR_create_context?
    EGL_KHR_gl_renderbuffer_image
    */

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

/*
    p_wglQueryCurrentRendererIntegerWINE
    p_wglQueryCurrentRendererStringWINE
    p_wglQueryRendererIntegerWINE
    p_wglQueryRendererStringWINE

    p_wglChoosePixelFormatARB
    p_wglGetPixelFormatAttribfvARB
    p_wglGetPixelFormatAttribivARB
    p_wglSetPixelFormatWINE

    p_wglCreatePbufferARB
    p_wglDestroyPbufferARB
    p_wglGetPbufferDCARB
    p_wglQueryPbufferARB
    p_wglReleasePbufferDCARB
    p_wglSetPbufferAttribARB

    p_wglAllocateMemoryNV
    p_wglFreeMemoryNV

    p_wglBindTexImageARB
    p_wglCreateContextAttribsARB
    p_wglGetCurrentReadDCARB
    p_wglGetExtensionsStringARB
    p_wglGetExtensionsStringEXT
    p_wglGetSwapIntervalEXT
    p_wglMakeContextCurrentARB
    p_wglReleaseTexImageARB
    p_wglSwapIntervalEXT
*/
    return;

failed:
    free( display_egl.pixel_formats );
    free( memory_egl.pixel_formats );
    free( devices );
    dlclose( egl_handle );
    egl_handle = NULL;
#else  /* SONAME_LIBEGL */
    ERR( "EGL support not compiled in\n" );
#endif /* SONAME_LIBEGL */
}

static void opengl_init(void)
{
    if (0) display_funcs = user_driver->pwine_get_wgl_driver( WINE_WGL_DRIVER_VERSION );
    if (display_funcs == (void *)-1) display_funcs = NULL;
    if (0) memory_funcs = dibdrv_get_wgl_driver();
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
