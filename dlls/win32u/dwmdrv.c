/*
 * Graphics driver management functions
 *
 * Copyright 1994 Bob Amstadt
 * Copyright 1996, 2001 Alexandre Julliard
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

#include <stdarg.h>
#include <stddef.h>
#include <assert.h>

#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"

#include "ntgdi_private.h"
#include "ntuser_private.h"

#include "wine/dwmapi.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dwmdrv);

static HANDLE req_handle, reply_handle;


#ifdef _WIN64
static inline TEB64 *NtCurrentTeb64(void) { return NULL; }
#else
static inline TEB64 *NtCurrentTeb64(void) { return (TEB64 *)NtCurrentTeb()->GdiBatchCount; }
#endif

static void dwm_server_start(void)
{
    WCHAR appnameW[MAX_PATH], cmdlineW[MAX_PATH], system_dirW[MAX_PATH];
    RTL_USER_PROCESS_PARAMETERS params = { sizeof(params), sizeof(params) };
    ULONG_PTR buffer[offsetof( PS_ATTRIBUTE_LIST, Attributes[2] ) / sizeof(ULONG_PTR)];
    PS_ATTRIBUTE_LIST *ps_attr = (PS_ATTRIBUTE_LIST *)buffer;
    TEB64 *teb64 = NtCurrentTeb64();
    PEB *peb = NtCurrentTeb()->Peb;
    PS_CREATE_INFO create_info;
    WCHAR desktop[MAX_PATH];
    HANDLE process, thread;
    unsigned int status;
    BOOL redirect;

    asciiz_to_unicode( appnameW, "\\??\\C:\\windows\\system32\\dwm.exe" );
    asciiz_to_unicode( cmdlineW, "\"C:\\windows\\system32\\dwm.exe\"" );
    asciiz_to_unicode( system_dirW, "C:\\windows\\system32" );

    params.Flags           = PROCESS_PARAMS_FLAG_NORMALIZED;
    params.Environment     = peb->ProcessParameters->Environment;
    params.EnvironmentSize = peb->ProcessParameters->EnvironmentSize;
    params.hStdError       = peb->ProcessParameters->hStdError;
    RtlInitUnicodeString( &params.CurrentDirectory.DosPath, system_dirW );
    RtlInitUnicodeString( &params.ImagePathName, appnameW + 4 );
    RtlInitUnicodeString( &params.CommandLine, cmdlineW );
    RtlInitUnicodeString( &params.WindowTitle, appnameW + 4 );
    RtlInitUnicodeString( &params.Desktop, desktop );

    ps_attr->Attributes[0].Attribute    = PS_ATTRIBUTE_IMAGE_NAME;
    ps_attr->Attributes[0].Size         = sizeof(appnameW) - sizeof(WCHAR);
    ps_attr->Attributes[0].ValuePtr     = (WCHAR *)appnameW;
    ps_attr->Attributes[0].ReturnLength = NULL;

    ps_attr->Attributes[1].Attribute    = PS_ATTRIBUTE_TOKEN;
    ps_attr->Attributes[1].Size         = sizeof(HANDLE);
    ps_attr->Attributes[1].ValuePtr     = GetCurrentThreadEffectiveToken();
    ps_attr->Attributes[1].ReturnLength = NULL;

    ps_attr->TotalLength = offsetof( PS_ATTRIBUTE_LIST, Attributes[2] );

    if ((redirect = teb64 && !teb64->TlsSlots[WOW64_TLS_FILESYSREDIR]))
        teb64->TlsSlots[WOW64_TLS_FILESYSREDIR] = TRUE;
    status = NtCreateUserProcess( &process, &thread, PROCESS_ALL_ACCESS, THREAD_ALL_ACCESS,
                                  NULL, NULL, 0, THREAD_CREATE_FLAGS_CREATE_SUSPENDED, &params,
                                  &create_info, ps_attr );
    if (redirect) teb64->TlsSlots[WOW64_TLS_FILESYSREDIR] = FALSE;

    if (status)
        ERR( "failed to start dwm, status %#x\n", status );
    else
    {
        NtResumeThread( thread, NULL );
        TRACE( "started dwm\n" );
        NtClose( thread );
        NtClose( process );
    }
}

static HANDLE create_request_pipe( HANDLE *req_handle )
{
    HANDLE server_handle = 0;
    UINT status;
    int fds[2];

    if (pipe( fds ) < 0) return 0;
    fcntl( fds[0], F_SETFD, FD_CLOEXEC );
    fcntl( fds[1], F_SETFD, FD_CLOEXEC );

    if ((status = wine_server_fd_to_handle( fds[0], GENERIC_READ | SYNCHRONIZE, 0, &server_handle )) ||
        (status = wine_server_fd_to_handle( fds[1], GENERIC_WRITE | SYNCHRONIZE, 0, req_handle )))
    {
        ERR( "Failed to create handles for DWM pipes, status %#x\n", status );
        if (server_handle) NtClose( server_handle );
        server_handle = 0;
    }

    close( fds[0] );
    close( fds[1] );
    return server_handle;
}

static dwm_display_t dwm_connect( const char *display_type, const char *display_name )
{
    struct dwm_req_connect req = {.header = {.type = DWM_REQ_CONNECT, .req_size = sizeof(req)}};
    union dwm_reply reply;
    IO_STATUS_BLOCK io;
    UINT status;

    strcpy( req.display_type, display_type );
    strcpy( req.display_name, display_name );
    req.version = DWM_PROTOCOL_VERSION;

    if ((status = NtWriteFile( req_handle, NULL, NULL, NULL, &io, &req, sizeof(union dwm_request), NULL, NULL )) ||
        io.Information != sizeof(union dwm_request))
    {
        ERR( "Failed to write HELLO, status %#x\n", status );
        return DWM_INVALID_DISPLAY;
    }
    if ((status = NtReadFile( reply_handle, NULL, NULL, NULL, &io, &reply, sizeof(reply), NULL, NULL )) ||
        io.Information != sizeof(reply) || (status = reply.header.status))
    {
        ERR( "Failed to read HELLO, status %#x\n", status );
        return DWM_INVALID_DISPLAY;
    }

    return reply.connect.dwm_display;
}

static inline BOOL dwm_server_connect(void)
{
    HANDLE server_handle;
    UINT retry = 2;

    if (getenv( "WINENODWM" )) return TRUE;

    if (req_handle) NtClose( req_handle );
    if (reply_handle) NtClose( reply_handle );
    reply_handle = req_handle = 0;

    if (!(server_handle = create_request_pipe( &req_handle ))) return FALSE;

    while (retry--)
    {
        SERVER_START_REQ( dwm_connect )
        {
            req->server_handle = wine_server_obj_handle( server_handle );
            if (wine_server_call_err( req )) reply_handle = 0;
            else reply_handle = wine_server_ptr_handle( reply->handle );
        }
        SERVER_END_REQ;

        if (reply_handle) break;
        dwm_server_start();
    }

    NtClose( server_handle );
    return TRUE;
}

static dwm_display_t dwm_client_connect( const char *type, const char *name )
{
    return dwm_connect( type, name );
}

struct dwmdrv_surface
{
    struct window_surface base;
    pthread_mutex_t mutex;

    HWND hwnd;
    RECT old_window_rect;
    RECT bounds;
    HRGN region;

    BITMAPINFOHEADER info;
    BYTE bits[];
};

static void *dwmdrv_surface_lock( struct window_surface *iface )
{
    struct dwmdrv_surface *surface = CONTAINING_RECORD( iface, struct dwmdrv_surface, base );
    pthread_mutex_lock( &surface->mutex );
    return surface + 1;
}

static void dwmdrv_surface_unlock( struct window_surface *iface )
{
    struct dwmdrv_surface *surface = CONTAINING_RECORD( iface, struct dwmdrv_surface, base );
    pthread_mutex_unlock( &surface->mutex );
}

static void dwmdrv_surface_get_info( struct window_surface *iface, BITMAPINFO *info )
{
    struct dwmdrv_surface *surface = CONTAINING_RECORD( iface, struct dwmdrv_surface, base );
    info->bmiHeader = surface->info;
}

static void dwmdrv_surface_set_clip( struct window_surface *window_surface, const RECT *rects, UINT count )
{
}

static void dwmdrv_surface_set_key( struct window_surface *window_surface, COLORREF color_key )
{
}

static void dwmdrv_surface_set_shape( struct window_surface *window_surface, const RECT *rects, UINT count )
{
}

static BOOL dwmdrv_surface_flush( struct window_surface *iface )
{
    struct dwmdrv_surface *surface = CONTAINING_RECORD( iface, struct dwmdrv_surface, base );
    struct dwm_req_surface_flush req = {.header = {.type = DWM_REQ_SURFACE_FLUSH, .req_size = sizeof(req)}};
    static const UINT32 align = sizeof(UINT32);
    IO_STATUS_BLOCK io;
    UINT status;

    req.hwnd = surface->hwnd;
    req.rect = surface->base.rect;
    if (!intersect_rect( &req.rect, &req.rect, &surface->bounds )) SetRectEmpty( &req.rect );
    req.stride = ((32 * (req.rect.right - req.rect.left) + 7) / 8 + align) & ~align;
    req.old_window_rect = surface->old_window_rect;
    NtUserGetWindowRect( surface->hwnd, &req.new_window_rect );
    surface->old_window_rect = req.new_window_rect;

    if ((status = NtWriteFile( req_handle, NULL, NULL, NULL, &io, &req, sizeof(union dwm_request), NULL, NULL )) ||
        io.Information != sizeof(union dwm_request))
        ERR( "Failed to write HELLO, status %#x\n", status );

    if (!IsRectEmpty(&req.rect))
    {
        UINT width = surface->base.rect.right - surface->base.rect.left;
        BYTE *pixels;

        pixels = surface->bits + (req.rect.top * width + req.rect.left) * 4;
        while (req.rect.top != req.rect.bottom)
        {
            if ((status = NtWriteFile( req_handle, NULL, NULL, NULL, &io, pixels, req.stride, NULL, NULL )) ||
                io.Information != req.stride)
                ERR( "Failed to write HELLO, status %#x\n", status );
            pixels += width * 4;
            req.rect.top++;
        }
    }

    return TRUE;
}

static void dwmdrv_surface_destroy( struct window_surface *surface )
{
    free( surface );
}

static const struct window_surface_funcs dwmdrv_surface_funcs =
{
    dwmdrv_surface_lock,
    dwmdrv_surface_unlock,
    dwmdrv_surface_get_info,
    dwmdrv_surface_set_clip,
    dwmdrv_surface_set_key,
    dwmdrv_surface_set_shape,
    dwmdrv_surface_flush,
    dwmdrv_surface_destroy,
};

struct window_surface *dwmdrv_surface_create( HWND hwnd, const RECT *rect )
{
    UINT width = rect->right - rect->left, height = rect->bottom - rect->top, size;
    struct dwmdrv_surface *surface;
    pthread_mutexattr_t mutexattr;

    TRACE("hwnd %p rect %s\n", hwnd, wine_dbgstr_rect(rect));

    size = width * height * 4;
    if (!(surface = calloc(1, offsetof(struct dwmdrv_surface, bits[size])))) return NULL;
    surface->info.biSize = sizeof(surface->info);
    surface->info.biClrUsed = 0;
    surface->info.biBitCount = 32;
    surface->info.biCompression = BI_RGB;
    surface->info.biWidth = width;
    surface->info.biHeight = -height; /* top-down */
    surface->info.biPlanes = 1;
    surface->info.biSizeImage = width * height * 4;

    pthread_mutexattr_init(&mutexattr);
    pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&surface->mutex, &mutexattr);
    pthread_mutexattr_destroy(&mutexattr);

    surface->base.funcs = &dwmdrv_surface_funcs;
    surface->base.rect = *rect;
    surface->base.ref = 1;
    surface->hwnd = hwnd;
    reset_bounds(&surface->bounds);

    TRACE("created %p, hwnd %p, rect, %s, bits [%p,%p)\n", surface, hwnd, wine_dbgstr_rect(rect),
          surface->bits, surface->bits + surface->info.biSizeImage);

    return &surface->base;
}

static const struct dwm_funcs dwm_funcs =
{
    dwm_client_connect,
    dwmdrv_surface_create,
};

const struct dwm_funcs *__wine_get_dwm_driver( UINT version )
{
    TRACE( "version %u\n", version );
    if (getenv( "WINENODWM" )) return NULL;
    if (version != WINE_GDI_DRIVER_VERSION) return NULL;
    if (!dwm_server_connect()) return NULL;
    return &dwm_funcs;
}

void dwm_init(void)
{
    if (getenv( "WINENODWM" )) return;
    if (!dwm_server_connect()) WARN( "Failed to connect to dwm server\n" );
}
