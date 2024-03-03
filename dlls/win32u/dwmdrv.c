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

static BOOL SKIP = FALSE;


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

    if (getenv( "WINENODWM" )) return TRUE;

    if (req_handle) NtClose( req_handle );
    if (reply_handle) NtClose( reply_handle );
    reply_handle = req_handle = 0;

    if (!(server_handle = create_request_pipe( &req_handle ))) return FALSE;

    for (;;)
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

static dwm_context_t dwm_gdi_context_create(void)
{
    struct dwm_req_gdi_context_create req = {.header = {.type = DWM_REQ_GDI_CONTEXT_CREATE, .req_size = sizeof(req)}};
    union dwm_reply reply;
    IO_STATUS_BLOCK io;
    UINT status;

    if ((status = NtWriteFile( req_handle, NULL, NULL, NULL, &io, &req, sizeof(union dwm_request), NULL, NULL )) ||
        io.Information != sizeof(union dwm_request))
        ERR( "Failed to write request, status %#x\n", status );
    if ((status = NtReadFile( reply_handle, NULL, NULL, NULL, &io, &reply, sizeof(reply), NULL, NULL )) ||
        io.Information != sizeof(reply) || (status = reply.header.status))
        ERR( "Failed to read reply, status %#x\n", status );

    TRACE( "created %#x\n", (UINT)reply.gdi_context_create.dwm_context );
    return reply.gdi_context_create.dwm_context;
}

static void dwm_gdi_context_destroy( dwm_context_t dwm_context )
{
    struct dwm_req_gdi_context_destroy req = {.header = {.type = DWM_REQ_GDI_CONTEXT_DESTROY, .req_size = sizeof(req)}};
    union dwm_reply reply;
    IO_STATUS_BLOCK io;
    UINT status;

    req.dwm_context = dwm_context;

    if ((status = NtWriteFile( req_handle, NULL, NULL, NULL, &io, &req, sizeof(union dwm_request), NULL, NULL )) ||
        io.Information != sizeof(union dwm_request))
        ERR( "Failed to write request, status %#x\n", status );
    if ((status = NtReadFile( reply_handle, NULL, NULL, NULL, &io, &reply, sizeof(reply), NULL, NULL )) ||
        io.Information != sizeof(reply) || (status = reply.header.status))
        ERR( "Failed to read reply, status %#x\n", status );
}

static dwm_window_t dwm_window_create( dwm_display_t display, HWND hwnd, UINT64 native )
{
    struct dwm_req_window_create req = {.header = {.type = DWM_REQ_WINDOW_CREATE, .req_size = sizeof(req)}};
    union dwm_reply reply;
    IO_STATUS_BLOCK io;
    UINT status;
    WND *win;

    req.dwm_display = display;
    req.hwnd = hwnd;
    req.native = native;

    if ((status = NtWriteFile( req_handle, NULL, NULL, NULL, &io, &req, sizeof(union dwm_request), NULL, NULL )) ||
        io.Information != sizeof(union dwm_request))
        ERR( "Failed to write request, status %#x\n", status );
    if ((status = NtReadFile( reply_handle, NULL, NULL, NULL, &io, &reply, sizeof(reply), NULL, NULL )) ||
        io.Information != sizeof(reply) || (status = reply.header.status))
        ERR( "Failed to read reply, status %#x\n", status );

    if ((win = get_win_ptr( hwnd )))
    {
        win->dwm_window = reply.window_create.dwm_window;
        release_win_ptr( win );
    }

    return reply.window_create.dwm_window;
}

static void dwm_window_destroy( HWND hwnd, dwm_window_t dwm_window )
{
    struct dwm_req_window_destroy req = {.header = {.type = DWM_REQ_WINDOW_DESTROY, .req_size = sizeof(req)}};
    union dwm_reply reply;
    IO_STATUS_BLOCK io;
    UINT status;
    WND *win;

    if ((win = get_win_ptr( hwnd )))
    {
        win->dwm_window = 0;
        release_win_ptr( win );
    }

    req.dwm_window = dwm_window;

    if ((status = NtWriteFile( req_handle, NULL, NULL, NULL, &io, &req, sizeof(union dwm_request), NULL, NULL )) ||
        io.Information != sizeof(union dwm_request))
        ERR( "Failed to write request, status %#x\n", status );
    if ((status = NtReadFile( reply_handle, NULL, NULL, NULL, &io, &reply, sizeof(reply), NULL, NULL )) ||
        io.Information != sizeof(reply) || (status = reply.header.status))
        ERR( "Failed to read reply, status %#x\n", status );
}

static const struct dwm_funcs dwm_funcs =
{
    dwm_client_connect,
    dwm_window_create,
    dwm_window_destroy,
};

const struct dwm_funcs *__wine_get_dwm_driver( UINT version )
{
    TRACE( "version %u\n", version );
    if (version != WINE_GDI_DRIVER_VERSION) return NULL;
    if (!dwm_server_connect()) return NULL;
    return &dwm_funcs;
}


enum opcode
{
    DWM_OP_SET_SOURCE,
    DWM_OP_SET_POINTS,

    DWM_OP_CLIP_SOURCE,

    DWM_OP_BLEND_IMAGE,
    DWM_OP_PUT_IMAGE,

    DWM_OP_RECTANGLE,
    DWM_OP_POLY_POLYLINE,
    DWM_OP_POLY_POLYGON,

    DWM_OP_PAT_BLT,
    DWM_OP_EXT_TEXT_OUT,

    DWM_OP_SELECT_BRUSH,
    DWM_OP_SELECT_FONT,
    DWM_OP_SELECT_PEN,

    DWM_OP_SET_BOUNDS_RECT,
    DWM_OP_SET_DEVICE_CLIPPING,
};

struct cmd
{
    UINT32 op;
    UINT32 size;
};


struct cmd_set_source
{
    struct cmd cmd;
    UINT32 bpp;
    UINT32 stride;
    RECT rect;
};

struct cmd_set_points
{
    struct cmd cmd;
    UINT32 count;
};


struct cmd_alpha_blend
{
    struct cmd cmd;
    RECT dst_rect;
    BLENDFUNCTION func;
};

struct cmd_blend_image
{
    struct cmd cmd;
    RECT dst_rect;
    BLENDFUNCTION func;
};

struct cmd_stretch_blt
{
    struct cmd cmd;
    RECT dst_rect;
    UINT32 rop;
};

struct cmd_put_image
{
    struct cmd cmd;
    RECT dst_rect;
    UINT32 rop;
};


struct cmd_move_to
{
    struct cmd cmd;
    UINT x;
    UINT y;
};

struct cmd_line_to
{
    struct cmd cmd;
    UINT x;
    UINT y;
};

struct cmd_rectangle
{
    struct cmd cmd;
    UINT left;
    UINT top;
    UINT right;
    UINT bottom;
};

struct cmd_poly_polyline
{
    struct cmd cmd;
    UINT lines;
};

struct cmd_poly_polygon
{
    struct cmd cmd;
    UINT polygons;
};


struct cmd_pat_blt
{
    struct cmd cmd;
    RECT dst_rect;
    UINT32 rop;
};

struct cmd_ext_text_out
{
    struct cmd cmd;
    UINT x;
    UINT y;
    UINT flags;
    RECT rect;
    UINT count;
};


struct cmd_select_brush
{
    struct cmd cmd;
    HBRUSH brush;
};

struct cmd_select_font
{
    struct cmd cmd;
    HFONT font;
};

struct cmd_select_pen
{
    struct cmd cmd;
    HPEN pen;
};


struct cmd_set_bounds_rect
{
    struct cmd cmd;
    RECT rect;
    UINT32 flags;
};


struct device
{
    struct gdi_physdev dev;
    dwm_context_t dwm_context;

    struct cmd_set_source *cmd_source;
    struct cmd_set_points *cmd_points;

    DWORD flush_time;
    BYTE *cmd_buf_max;
    BYTE *cmd_buf_end;
    BYTE cmd_buf[];
};

static struct device *device_from_physdev( PHYSDEV dev )
{
    return CONTAINING_RECORD( dev, struct device, dev );
}

static const char *dbgstr_coords( const struct bitblt_coords *coords )
{
    RECT log = {coords->log_x, coords->log_y, coords->log_x + coords->log_width, coords->log_y + coords->log_height};
    RECT phy = {coords->x, coords->y, coords->x + coords->width, coords->y + coords->height};
    return wine_dbg_sprintf( "(log %s phy %s vis %s)", wine_dbgstr_rect(&log), wine_dbgstr_rect(&phy),
                             wine_dbgstr_rect(&coords->visrect) );
};

static void device_send_commands( struct device *device )
{
    struct dwm_req_gdi_commands req = {.header = {.type = DWM_REQ_GDI_COMMANDS, .req_size = sizeof(req)}};
    HWND hwnd = NtUserWindowFromDC( device->dev.hdc ), toplevel = NtUserGetAncestor( hwnd, GA_ROOT );
    DC *dc = get_physdev_dc( &device->dev );
    IO_STATUS_BLOCK io;
    UINT status;
    WND *win;

    if (!(req.data_size = device->cmd_buf_end - device->cmd_buf)) return;
    if (!(req.dwm_context = device->dwm_context)) return;

    if ((win = get_win_ptr( toplevel )))
    {
        req.dwm_window = win->dwm_window;
        release_win_ptr( win );
    }

    NtUserGetWindowRect( toplevel, &req.window_rect );
    NtUserGetWindowRect( hwnd, &req.context_rect );

if (0)
{
    ERR( "toplevel %p, window_rect %s, hwnd %p context_rect %s\n",
         toplevel, wine_dbgstr_rect(&req.window_rect),
         hwnd, wine_dbgstr_rect(&req.context_rect) );
    ERR( "  dc %p device %s vis %s wnd %s-(%d,%d) vport %s-(%d,%d)\n", dc, wine_dbgstr_rect(&dc->device_rect), wine_dbgstr_rect(&dc->attr->vis_rect),
         wine_dbgstr_point(&dc->attr->wnd_org), (UINT)dc->attr->wnd_org.x + (UINT)dc->attr->wnd_ext.cx, (UINT)dc->attr->wnd_org.y + (UINT)dc->attr->wnd_ext.cy,
         wine_dbgstr_point(&dc->attr->vport_org), (UINT)dc->attr->vport_org.x + (UINT)dc->attr->vport_ext.cx, (UINT)dc->attr->vport_org.y + (UINT)dc->attr->vport_ext.cy
         );

    ERR( "    flags %#x\n", dc->flags );
    ERR( "    hClipRgn %p\n", dc->hClipRgn );
    ERR( "    hMetaRgn %p\n", dc->hMetaRgn );
    ERR( "    hVisRgn %p\n", dc->hVisRgn );
    ERR( "    region %p\n", dc->region );
    ERR( "    hPen %p\n", dc->hPen );
    ERR( "    hBrush %p\n", dc->hBrush );
    ERR( "    hFont %p\n", dc->hFont );
    ERR( "    hBitmap %p\n", dc->hBitmap );
    ERR( "    hPalette %p\n", dc->hPalette );

    ERR( "    breakExtra %u\n", dc->breakExtra );
    ERR( "    breakRem %u\n", dc->breakRem );
    ERR( "    xformWorld2Wnd %f %f %f %f %f %f\n", dc->xformWorld2Wnd.eM11, dc->xformWorld2Wnd.eM12, dc->xformWorld2Wnd.eM21, dc->xformWorld2Wnd.eM22, dc->xformWorld2Wnd.eDx, dc->xformWorld2Wnd.eDy );
    ERR( "    xformWorld2Vport %f %f %f %f %f %f\n", dc->xformWorld2Vport.eM11, dc->xformWorld2Vport.eM12, dc->xformWorld2Vport.eM21, dc->xformWorld2Vport.eM22, dc->xformWorld2Vport.eDx, dc->xformWorld2Vport.eDy );
    ERR( "    xformVport2World %f %f %f %f %f %f\n", dc->xformVport2World.eM11, dc->xformVport2World.eM12, dc->xformVport2World.eM21, dc->xformVport2World.eM22, dc->xformVport2World.eDx, dc->xformVport2World.eDy );
    ERR( "    vport2WorldValid %u\n", dc->vport2WorldValid );
    ERR( "    bounds %s\n", wine_dbgstr_rect(&dc->bounds) );
}

if (1)
{
    OffsetRect( &req.context_rect, -dc->device_rect.left, -dc->device_rect.top );
    OffsetRect( &req.window_rect, -req.window_rect.left, -req.window_rect.top );
}
else
{
    OffsetRect( &req.window_rect, -req.window_rect.left, -req.window_rect.top );
    NtUserMapWindowPoints( 0, toplevel, (POINT *)&req.context_rect, 2 );
    intersect_rect( &req.context_rect, &req.window_rect, &req.context_rect );
}

    if ((status = NtWriteFile( req_handle, NULL, NULL, NULL, &io, &req, sizeof(union dwm_request), NULL, NULL )) ||
        io.Information != sizeof(union dwm_request))
        ERR( "Failed to write request, status %#x\n", status );
    if ((status = NtWriteFile( req_handle, NULL, NULL, NULL, &io, device->cmd_buf, req.data_size, NULL, NULL )) ||
        io.Information != req.data_size)
        ERR( "Failed to write request, status %#x\n", status );

    device->cmd_buf_end = device->cmd_buf;
    device->flush_time = NtGetTickCount();
}

static void *device_start_cmd( struct device *device, enum opcode op, UINT size )
{
    struct cmd *cmd;

    if (NtGetTickCount() - device->flush_time > 50)
        device_send_commands( device );
    if (device->cmd_buf_end + size >= device->cmd_buf_max)
        device_send_commands( device );
    if (device->cmd_buf_end + size >= device->cmd_buf_max)
    {
        ERR( "device->cmd_buf_max - device->cmd_buf_end %zu < size %u\n",
             device->cmd_buf_max - device->cmd_buf_end, size );
        return NULL;
    }

    cmd = (struct cmd *)device->cmd_buf_end;
    cmd->op = op;
    cmd->size = size;
    return cmd;
}

static void device_end_cmd( struct device *device, struct cmd *cmd )
{
    device->cmd_buf_end += cmd->size;
}


static void get_buffer_stride( const BITMAPINFO *src_info, const RECT *src_rect, UINT *dst_stride, RECT *dst_rect )
{
    static const SIZE_T align = sizeof(UINT32) - 1;
    UINT bpp = src_info->bmiHeader.biBitCount;

    dst_rect->top = 0;
    dst_rect->bottom = src_rect->bottom - src_rect->top;
    dst_rect->left = (src_rect->left * bpp - (src_rect->left * bpp / 8) * 8) / bpp;
    dst_rect->right = dst_rect->left + (src_rect->right - src_rect->left);

    *dst_stride = ((dst_rect->right * bpp + 7) / 8 + align) & ~align;
}

static void copy_image_bits( const struct gdi_image_bits *bits, const BITMAPINFO *src_info, const RECT *src_rect,
                             BYTE *dst, UINT dst_stride, const RECT *dst_rect )
{
    UINT bpp = src_info->bmiHeader.biBitCount, height = dst_rect->bottom - dst_rect->top, src_stride, size;
    char *src = bits->ptr;

    src_stride = src_info->bmiHeader.biSizeImage / src_info->bmiHeader.biHeight;
    size = (src_rect->right * bpp + 7) / 8 - (src_rect->left * bpp) / 8;
    src += src_rect->top * src_stride + (src_rect->left * bpp) / 8;
    dst += dst_rect->top * dst_stride + (dst_rect->left * bpp) / 8;

    while (height--)
    {
        memcpy( dst, src, size );
        src += src_stride;
        dst += dst_stride;
    }
}

static void device_set_source_cmd( struct device *device, BITMAPINFO *info, struct bitblt_coords *src,
                                   const struct gdi_image_bits *bits )
{
    UINT bpp, data_size, stride;
    struct cmd_set_source *cmd;
    RECT rect;

    bpp = info->bmiHeader.biBitCount;
    get_buffer_stride( info, &src->visrect, &stride, &rect );
    data_size = stride * rect.bottom;

    if ((cmd = device_start_cmd( device, DWM_OP_SET_SOURCE, sizeof(*cmd) + data_size )))
    {
        cmd->bpp = bpp;
        cmd->rect = rect;
        cmd->stride = stride;
        copy_image_bits( bits, info, &src->visrect, (BYTE *)(cmd + 1), stride, &rect );
        device_end_cmd( device, &cmd->cmd );
    }
}


static void device_set_points_cmd( struct device *device, const POINT *points,
                                   const UINT *counts, UINT count, BOOL logical )
{
    DC *dc = get_physdev_dc( &device->dev );
    struct cmd_set_points *cmd;
    UINT i, total, data_size;

    for (i = total = 0; i < count; i++) total += counts[i];
    data_size = total * sizeof(*points);

    if ((cmd = device_start_cmd( device, DWM_OP_SET_POINTS, sizeof(*cmd) + data_size )))
    {
        memcpy( cmd + 1, points, data_size );
        if (logical) lp_to_dp( dc, (POINT *)(cmd + 1), total );
        device_end_cmd( device, &cmd->cmd );
    }
}

static void device_set_points_cmd_from_hrgn( struct device *device, HRGN hrgn, enum opcode set_cmd )
{
    const WINEREGION *region;
    struct cmd *cmd;

    if ((region = get_wine_region( hrgn )))
    {
        UINT count = region->numRects * 2;
        device_set_points_cmd( device, (POINT *)region->rects, &count, 1, FALSE );
        release_wine_region( hrgn );
    }

    if ((cmd = device_start_cmd( device, set_cmd, sizeof(*cmd) )))
        device_end_cmd( device, cmd );
}


static BOOL dwmdrv_AngleArc( PHYSDEV dev, INT x, INT y, DWORD radius, FLOAT start, FLOAT sweep )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static BOOL dwmdrv_Arc( PHYSDEV dev, INT left, INT top, INT right, INT bottom, INT xstart,
                        INT ystart, INT xend, INT yend )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static BOOL dwmdrv_ArcTo( PHYSDEV dev, INT left, INT top, INT right, INT bottom, INT xstart,
                          INT ystart, INT xend, INT yend )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static DWORD dwmdrv_BlendImage( PHYSDEV dev, BITMAPINFO *info, const struct gdi_image_bits *bits,
                                struct bitblt_coords *src, struct bitblt_coords *dst, BLENDFUNCTION func )
{
    struct device *device = device_from_physdev( dev );
    struct cmd_blend_image *cmd;
    DWORD err;

    ERR( "device %p, info %p, bits %p, src %s, dst %s, func %#x\n", device, info, bits,
           dbgstr_coords( src ), dbgstr_coords( dst ), *(UINT32 *)&func );
    
    dev = GET_NEXT_PHYSDEV( dev, pBlendImage );
    if ((err = dev->funcs->pBlendImage( dev, info, bits, src, dst, func ))) return err;

    device_set_source_cmd( device, info, src, bits );
    if ((cmd = device_start_cmd( device, DWM_OP_BLEND_IMAGE, sizeof(*cmd) )))
    {
        cmd->dst_rect = dst->visrect;
        cmd->func = func;
        device_end_cmd( device, &cmd->cmd );
    }

    return ERROR_SUCCESS;
}

static BOOL dwmdrv_Chord( PHYSDEV dev, INT left, INT top, INT right, INT bottom, INT xstart,
                          INT ystart, INT xend, INT yend )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static inline BOOL dwmdrv_CreateCompatibleDC( PHYSDEV orig, PHYSDEV *dev )
{
    struct device *device;
    PHYSDEV next = *dev;
    SIZE_T size;

    TRACE( "orig %p, dev %p\n", orig, dev );
    TRACE( "  next %p hdc %p dev %s prio %d next %p\n", next, next->hdc, next->funcs->name,
           next->funcs->priority, next->next );
    next = next->next;
    TRACE( "  next %p hdc %p dev %s prio %d next %p\n", next, next->hdc, next->funcs->name,
           next->funcs->priority, next->next );

    size = offsetof(struct device, cmd_buf[0x4000 - sizeof(struct device)]);
    if (!(device = calloc( 1, size ))) return FALSE;
    if (!getenv( "WINENODWM" )) device->dwm_context = dwm_gdi_context_create();
    device->cmd_buf_max = (BYTE *)device + size;
    device->cmd_buf_end = device->cmd_buf;
    push_dc_driver( dev, &device->dev, &dwm_driver );

    TRACE( "device %p\n", device );
    return TRUE;
}

static BOOL dwmdrv_CreateDC( PHYSDEV *dev, const WCHAR *device_name, const WCHAR *output_name, const DEVMODEW *devmode )
{
    struct device *device;
    PHYSDEV next = *dev;
    SIZE_T size;

    TRACE( "  next %p hdc %p dev %s prio %d next %p\n", next, next->hdc, next->funcs->name,
           next->funcs->priority, next->next );
    next = next->next;
    TRACE( "  next %p hdc %p dev %s prio %d next %p\n", next, next->hdc, next->funcs->name,
           next->funcs->priority, next->next );

    size = offsetof(struct device, cmd_buf[0x4000 - sizeof(struct device)]);
    if (!(device = calloc( 1, size ))) return FALSE;
    if (!getenv( "WINENODWM" )) device->dwm_context = dwm_gdi_context_create();
    device->cmd_buf_max = (BYTE *)device + size;
    device->cmd_buf_end = device->cmd_buf;
    push_dc_driver( dev, &device->dev, &dwm_driver );

    TRACE( "device %p\n", device );
    return TRUE;
}

static BOOL dwmdrv_DeleteDC( PHYSDEV dev )
{
    struct device *device = device_from_physdev( dev );
    TRACE( "device %p\n", device );
    device_send_commands( device );
    if (!getenv( "WINENODWM" )) dwm_gdi_context_destroy( device->dwm_context );
    free( device );
    return TRUE;
}

static BOOL dwmdrv_DeleteObject( PHYSDEV dev, HGDIOBJ obj )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static BOOL dwmdrv_Ellipse( PHYSDEV dev, INT left, INT top, INT right, INT bottom )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

#if 0
static void context_create_source( struct context *context, struct surface_lock *lock,
                                  const struct cmd_set_source *cmd )
{
    D3DKMT_CREATEDCFROMMEMORY desc;
    NTSTATUS status;

    context->cmd_source = malloc( cmd->cmd.size );
    memcpy( context->cmd_source, cmd, cmd->cmd.size );

    desc.pMemory = context->cmd_source + 1;
    desc.Format = D3DDDIFMT_A8R8G8B8; /* format->ddi_format; */
    desc.Width = context->cmd_source->rect.right;
    desc.Height = context->cmd_source->rect.bottom;
    desc.Pitch = context->cmd_source->stride;
    desc.hDeviceDc = lock->hdc;
    desc.pColorTable = NULL;

    if (!(status = NtGdiDdDDICreateDCFromMemory( &desc )))
    {
        context->source_hbitmap = desc.hBitmap;
        context->source_hdc = desc.hDc;
    }
}

static void context_destroy_source( struct context *context )
{
    D3DKMT_DESTROYDCFROMMEMORY desc;
    if (!context->cmd_source) return;
    desc.hDc = context->source_hdc;
    desc.hBitmap = context->source_hbitmap;
    NtGdiDdDDIDestroyDCFromMemory( &desc );
    free( context->cmd_source );
    context->cmd_source = NULL;
}
#endif

static void device_get_source_cmd_info( struct device *device, BITMAPINFO *info, struct gdi_image_bits *bits )
{
    info->bmiHeader.biPlanes = 1;
    info->bmiHeader.biCompression = BI_RGB;
    info->bmiHeader.biBitCount = device->cmd_source->bpp;
    info->bmiHeader.biWidth = device->cmd_source->rect.right;
    info->bmiHeader.biHeight = device->cmd_source->rect.bottom;
    info->bmiHeader.biSizeImage = device->cmd_source->rect.bottom * device->cmd_source->stride;

    bits->free = NULL;
    bits->is_copy = FALSE;
    bits->ptr = device->cmd_source + 1;
}

static void bitblt_coords_from_rect( struct bitblt_coords *coords, const RECT *rect )
{
    coords->log_x = coords->x = rect->left;
    coords->log_y = coords->y = rect->top;
    coords->log_width = coords->width = rect->right - rect->left;
    coords->log_height = coords->height = rect->bottom - rect->top;
    coords->visrect = *rect;
}

static UINT device_execute_cmd( PHYSDEV dev, const void *ptr )
{
    struct device *device = device_from_physdev( dev );
    const struct cmd *header = ptr;
    UINT err = 0;

    switch (header->op)
    {
    case DWM_OP_SET_SOURCE:
    {
        const struct cmd_set_source *cmd = ptr;

        ERR( "device %p, rect %s, bpp %u, stride %u\n", device, wine_dbgstr_rect(&cmd->rect), cmd->bpp, cmd->stride );

        free( device->cmd_source );
        device->cmd_source = malloc( cmd->cmd.size );
        memcpy( device->cmd_source, cmd, cmd->cmd.size );
        break;
    }

    case DWM_OP_SET_POINTS:
    {
        const struct cmd_set_points *cmd = ptr;
        free( device->cmd_points );
        device->cmd_points = malloc( cmd->cmd.size );
        memcpy( device->cmd_points, cmd, cmd->cmd.size );
        break;
    }

    case DWM_OP_CLIP_SOURCE:
    {
        /* FIXME( "DWM_OP_CLIP_SOURCE\n"); */
        break;
    }

    case DWM_OP_BLEND_IMAGE:
    {
        const struct cmd_blend_image *cmd = ptr;
        struct bitblt_coords src, dst;
        struct gdi_image_bits bits;
        BITMAPINFO info;

        if (!device->cmd_source) break;
        bitblt_coords_from_rect( &dst, &cmd->dst_rect );
        bitblt_coords_from_rect( &src, &device->cmd_source->rect );
        device_get_source_cmd_info( device, &info, &bits );

        ERR( "device %p, src %s, rect %s, func %#x\n", device, dbgstr_coords(&src), dbgstr_coords(&dst), *(UINT32 *)&cmd->func );

        dev = GET_NEXT_PHYSDEV( dev, pBlendImage );
        if ((err = dev->funcs->pBlendImage( dev, &info, &bits, &src, &dst, cmd->func )))
            ERR( "error %u\n", err );
        break;
    }
    case DWM_OP_PUT_IMAGE:
    {
        const struct cmd_put_image *cmd = ptr;
        struct bitblt_coords src, dst;
        struct gdi_image_bits bits;
        BITMAPINFO info;

        if (!device->cmd_source) break;
        bitblt_coords_from_rect( &dst, &cmd->dst_rect );
        bitblt_coords_from_rect( &src, &device->cmd_source->rect );
        device_get_source_cmd_info( device, &info, &bits );

        ERR( "device %p, src %s, rect %s, rop %#x\n", device, dbgstr_coords(&src), dbgstr_coords(&dst), cmd->rop );

        dev = GET_NEXT_PHYSDEV( dev, pPutImage );
        if ((err = dev->funcs->pPutImage( dev, 0, &info, &bits, &src, &dst, cmd->rop )))
            ERR( "error %u\n", err );
        break;
    }

    case DWM_OP_RECTANGLE:
    {
        const struct cmd_rectangle *cmd = ptr;

        TRACE( "device %p, left %d, top %d, right %d, bottom %d\n", device, cmd->left, cmd->top, cmd->right, cmd->bottom );

        dev = GET_NEXT_PHYSDEV( dev, pRectangle );
        if ((err = dev->funcs->pRectangle( dev, cmd->left, cmd->top, cmd->right, cmd->bottom )))
            ERR( "error %u\n", err );
        break;
    }

    case DWM_OP_POLY_POLYLINE:
    {
        const struct cmd_poly_polyline *cmd = ptr;

        TRACE( "device %p, lines %d\n", device, cmd->lines );

        dev = GET_NEXT_PHYSDEV( dev, pPolyPolyline );
        if ((err = dev->funcs->pPolyPolyline( dev, (POINT *)(device->cmd_points + 1), (ULONG *)(cmd + 1), cmd->lines )))
            ERR( "error %u\n", err );
        break;
    }
    case DWM_OP_POLY_POLYGON:
    {
        const struct cmd_poly_polygon *cmd = ptr;

        TRACE( "device %p, polygons %d\n", device, cmd->polygons );

        dev = GET_NEXT_PHYSDEV( dev, pPolyPolygon );
        if ((err = dev->funcs->pPolyPolygon( dev, (POINT *)(device->cmd_points + 1), (INT *)(cmd + 1), cmd->polygons )))
            ERR( "error %u\n", err );
        break;
    }

    case DWM_OP_PAT_BLT:
    {
        const struct cmd_pat_blt *cmd = ptr;
        struct bitblt_coords dst;

        bitblt_coords_from_rect( &dst, &cmd->dst_rect );

        TRACE( "device %p, dst %s\n", device, dbgstr_coords(&dst) );

        dev = GET_NEXT_PHYSDEV( dev, pPatBlt );
        if ((err = dev->funcs->pPatBlt( dev, &dst, cmd->rop )))
            ERR( "error %u\n", err );
        break;
    }

    case DWM_OP_EXT_TEXT_OUT:
    {
        const struct cmd_ext_text_out *cmd = ptr;
        const WCHAR *str = (const WCHAR *)(cmd + 1);
        INT dx;

        TRACE( "device %p, x %d, y %d, flags %#x, rect %s, str %s\n", device, cmd->x, cmd->y, cmd->flags, wine_dbgstr_rect(&cmd->rect), debugstr_wn(str, cmd->count) );

        dev = GET_NEXT_PHYSDEV( dev, pExtTextOut );
        dev->funcs->pExtTextOut( dev, cmd->x, cmd->y, cmd->flags, &cmd->rect, str, cmd->count, &dx );
        break;
    }

    case DWM_OP_SELECT_BRUSH: /* FIXME( "DWM_OP_SELECT_BRUSH\n"); */ break;
    case DWM_OP_SELECT_FONT: /* FIXME( "DWM_OP_SELECT_FONT\n"); */ break;
    case DWM_OP_SELECT_PEN: /* FIXME( "DWM_OP_SELECT_PEN\n"); */ break;

    case DWM_OP_SET_BOUNDS_RECT: /* FIXME( "DWM_OP_SET_BOUNDS_RECT\n"); */ break;
    case DWM_OP_SET_DEVICE_CLIPPING: /* FIXME( "DWM_OP_SET_DEVICE_CLIPPING\n"); */ break;
    }

    return header->size;
}

static INT dwmdrv_ExtEscape( PHYSDEV dev, INT escape, INT in_size, const void *in_data, INT out_size, void *out_data )
{
    struct device *device = device_from_physdev( dev );

    /* FIXME */
    TRACE( "device %p, escape %u, in_size %#x, in_data %p, out_size %#x, out_data %p\n", device,
           escape, in_size, in_data, out_size, out_data );

    if (escape == DWM_EXT_ESCAPE)
    {
        const char *ptr = in_data, *end = ptr + in_size;
        while (ptr < end) ptr += device_execute_cmd( dev, ptr );
    }

    if (SKIP) return 0;
    dev = GET_NEXT_PHYSDEV( dev, pExtEscape );
    return dev->funcs->pExtEscape( dev, escape, in_size, in_data, out_size, out_data );
}

static BOOL dwmdrv_ExtFloodFill( PHYSDEV dev, INT x, INT y, COLORREF color, UINT type )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static BOOL dwmdrv_ExtTextOut( PHYSDEV dev, INT x, INT y, UINT flags, const RECT *rect, LPCWSTR str,
                               UINT count, const INT *dx )
{
    struct device *device = device_from_physdev( dev );
    UINT data_size = count * sizeof(WCHAR);
    struct cmd_ext_text_out *cmd;

    TRACE( "device %p, x %d, y %d, flags %#x, rect %s, str %s, dx %p\n", device, x, y, flags,
           wine_dbgstr_rect( rect ), debugstr_wn( str, count ), dx );

    if ((cmd = device_start_cmd( device, DWM_OP_EXT_TEXT_OUT, sizeof(*cmd) + data_size )))
    {
        /* FIXME: dx */
        cmd->x = x;
        cmd->y = y;
        cmd->flags = flags;
        cmd->rect = *rect;
        cmd->count = count;
        memcpy( cmd + 1, str, data_size );
        device_end_cmd( device, &cmd->cmd );
    }

    if (SKIP) return TRUE;
    dev = GET_NEXT_PHYSDEV( dev, pExtTextOut );
    return dev->funcs->pExtTextOut( dev, x, y, flags, rect, str, count, dx );
}

static BOOL dwmdrv_FillPath( PHYSDEV dev )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static BOOL dwmdrv_FillRgn( PHYSDEV dev, HRGN rgn, HBRUSH brush )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static BOOL dwmdrv_FrameRgn( PHYSDEV dev, HRGN rgn, HBRUSH brush, INT width, INT height )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static UINT dwmdrv_GetBoundsRect( PHYSDEV dev, RECT *rect, UINT flags )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return DCB_RESET;
}

static INT dwmdrv_GetDeviceCaps( PHYSDEV dev, INT cap )
{
    struct device *device = device_from_physdev( dev );
    TRACE( "device %p\n", device );
    if (SKIP) return 0;
    dev = GET_NEXT_PHYSDEV( dev, pGetDeviceCaps );
    return dev->funcs->pGetDeviceCaps( dev, cap );
}

static BOOL dwmdrv_GetDeviceGammaRamp( PHYSDEV dev, void *ramp )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return FALSE;
}

static BOOL dwmdrv_GetICMProfile( PHYSDEV dev, BOOL allow_default, LPDWORD size, LPWSTR filename )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return FALSE;
}

static DWORD dwmdrv_GetImage( PHYSDEV dev, BITMAPINFO *info, struct gdi_image_bits *bits, struct bitblt_coords *src )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return ERROR_NOT_SUPPORTED;
}

static COLORREF dwmdrv_GetNearestColor( PHYSDEV dev, COLORREF color )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static COLORREF dwmdrv_GetPixel( PHYSDEV dev, INT x, INT y )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static UINT dwmdrv_GetSystemPaletteEntries( PHYSDEV dev, UINT start, UINT count, PALETTEENTRY *entries )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return 0;
}

static BOOL dwmdrv_GradientFill( PHYSDEV dev, TRIVERTEX *vert_array, ULONG nvert, void *grad_array,
                                 ULONG ngrad, ULONG mode )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static BOOL dwmdrv_InvertRgn( PHYSDEV dev, HRGN rgn )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static BOOL dwmdrv_LineTo( PHYSDEV dev, INT x, INT y )
{
    struct device *device = device_from_physdev( dev );
    DC *dc = get_physdev_dc( &device->dev );
    UINT counts = 2, data_size = sizeof(counts);
    POINT points[2] = {dc->attr->cur_pos, {x, y}};
    struct cmd_poly_polyline *cmd;

    TRACE( "device %p, x %d, y %d\n", device, x, y );

    device_set_points_cmd( device, points, &counts, 1, TRUE );

    if ((cmd = device_start_cmd( device, DWM_OP_POLY_POLYLINE, sizeof(*cmd) + data_size )))
    {
        cmd->lines = 1;
        memcpy( cmd + 1, &counts, data_size );
        device_end_cmd( device, &cmd->cmd );
    }

    if (SKIP) return TRUE;
    dev = GET_NEXT_PHYSDEV( dev, pLineTo );
    return dev->funcs->pLineTo( dev, x, y );
}

static BOOL dwmdrv_PaintRgn( PHYSDEV dev, HRGN rgn )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static BOOL dwmdrv_PatBlt( PHYSDEV dev, struct bitblt_coords *dst, DWORD rop )
{
    struct device *device = device_from_physdev( dev );
    struct cmd_pat_blt *cmd;

    ERR( "device %p, dst %s, rop %#x\n", device, dbgstr_coords( dst ), (UINT)rop );

    if ((cmd = device_start_cmd( device, DWM_OP_PAT_BLT, sizeof(*cmd) )))
    {
        cmd->dst_rect = dst->visrect;
        cmd->rop = rop;
        device_end_cmd( device, &cmd->cmd );
    }

    dev = GET_NEXT_PHYSDEV( dev, pPatBlt );
    return dev->funcs->pPatBlt( dev, dst, rop );
}

static BOOL dwmdrv_Pie( PHYSDEV dev, INT left, INT top, INT right, INT bottom, INT xstart,
                        INT ystart, INT xend, INT yend )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static BOOL dwmdrv_PolyBezier( PHYSDEV dev, const POINT *points, DWORD count )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static BOOL dwmdrv_PolyBezierTo( PHYSDEV dev, const POINT *points, DWORD count )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static BOOL dwmdrv_PolyDraw( PHYSDEV dev, const POINT *points, const BYTE *types, DWORD count )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static BOOL dwmdrv_PolyPolygon( PHYSDEV dev, const POINT *points, const INT *counts, UINT polygons )
{
    struct device *device = device_from_physdev( dev );
    UINT data_size = polygons * sizeof(*counts);
    struct cmd_poly_polygon *cmd;

    TRACE( "device %p, points %p, counts %p, polygons %u\n", device, points, counts, polygons );

    device_set_points_cmd( device, points, (const UINT *)counts, polygons, TRUE );

    if ((cmd = device_start_cmd( device, DWM_OP_POLY_POLYGON, sizeof(*cmd) + data_size )))
    {
        cmd->polygons = polygons;
        memcpy( cmd + 1, counts, data_size );
        device_end_cmd( device, &cmd->cmd );
    }

    if (SKIP) return TRUE;
    dev = GET_NEXT_PHYSDEV( dev, pPolyPolygon );
    return dev->funcs->pPolyPolygon( dev, points, counts, polygons );
}

static BOOL dwmdrv_PolyPolyline( PHYSDEV dev, const POINT *points, const DWORD *counts, DWORD lines )
{
    struct device *device = device_from_physdev( dev );
    UINT data_size = lines * sizeof(*counts);
    struct cmd_poly_polyline *cmd;

    TRACE( "device %p, points %p, counts %p, lines %u\n", device, points, counts, (UINT)lines );

    device_set_points_cmd( device, points, (const UINT *)counts, lines, TRUE );

    if ((cmd = device_start_cmd( device, DWM_OP_POLY_POLYLINE, sizeof(*cmd) + data_size )))
    {
        cmd->lines = lines;
        memcpy( cmd + 1, counts, data_size );
        device_end_cmd( device, &cmd->cmd );
    }

    if (SKIP) return TRUE;
    dev = GET_NEXT_PHYSDEV( dev, pPolyPolyline );
    return dev->funcs->pPolyPolyline( dev, points, counts, lines );
}

static BOOL dwmdrv_PolylineTo( PHYSDEV dev, const POINT *points, INT count )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static DWORD dwmdrv_PutImage( PHYSDEV dev, HRGN clip, BITMAPINFO *info, const struct gdi_image_bits *bits,
                              struct bitblt_coords *src, struct bitblt_coords *dst, DWORD rop )
{
    struct device *device = device_from_physdev( dev );
    struct cmd_put_image *cmd;
    DWORD err;

    ERR( "device %p, clip %p, info %p, bits %p, src %s, dst %s, rop %#x\n", device, clip, info,
           bits, dbgstr_coords( src ), dbgstr_coords( dst ), (UINT)rop );

    dev = GET_NEXT_PHYSDEV( dev, pPutImage );
    if ((err = dev->funcs->pPutImage( dev, clip, info, bits, src, dst, rop ))) return err;

    device_set_source_cmd( device, info, src, bits );
    device_set_points_cmd_from_hrgn( device, clip, DWM_OP_CLIP_SOURCE );
    if ((cmd = device_start_cmd( device, DWM_OP_PUT_IMAGE, sizeof(*cmd) )))
    {
        cmd->dst_rect = dst->visrect;
        cmd->rop = rop;
        device_end_cmd( device, &cmd->cmd );
    }

    return ERROR_SUCCESS;
}

static UINT dwmdrv_RealizeDefaultPalette( PHYSDEV dev )
{
    struct device *device = device_from_physdev( dev );

    /* FIXME */
    TRACE( "device %p\n", device );

    if (SKIP) return 0;
    dev = GET_NEXT_PHYSDEV( dev, pRealizeDefaultPalette );
    return dev->funcs->pRealizeDefaultPalette( dev );
}

static UINT dwmdrv_RealizePalette( PHYSDEV dev, HPALETTE palette, BOOL primary )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return 0;
}

static BOOL dwmdrv_Rectangle( PHYSDEV dev, INT left, INT top, INT right, INT bottom )
{
    struct device *device = device_from_physdev( dev );
    struct cmd_rectangle *cmd;

    ERR( "device %p, left %d, top %d, right %d, bottom %d\n", device, left, top, right, bottom );

    if ((cmd = device_start_cmd( device, DWM_OP_RECTANGLE, sizeof(*cmd) )))
    {
        cmd->left = left;
        cmd->top = top;
        cmd->right = right;
        cmd->bottom = bottom;
        device_end_cmd( device, &cmd->cmd );
    }

    if (SKIP) return TRUE;
    dev = GET_NEXT_PHYSDEV( dev, pRectangle );
    return dev->funcs->pRectangle( dev, left, top, right, bottom );
}

static BOOL dwmdrv_RoundRect( PHYSDEV dev, INT left, INT top, INT right, INT bottom, INT ell_width, INT ell_height )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static HBRUSH dwmdrv_SelectBrush( PHYSDEV dev, HBRUSH brush, const struct brush_pattern *pattern )
{
    struct device *device = device_from_physdev( dev );
    struct cmd_select_brush *cmd;

    TRACE( "device %p, brush %p, pattern %p\n", device, brush, pattern );

    if ((cmd = device_start_cmd( device, DWM_OP_SELECT_BRUSH, sizeof(*cmd) )))
    {
        /*if (pattern) FIXME( "device %p, brush %p, pattern %p\n", device, brush, pattern );*/
        cmd->brush = brush;
        device_end_cmd( device, &cmd->cmd );
    }

    if (SKIP) return brush;
    dev = GET_NEXT_PHYSDEV( dev, pSelectBrush );
    return dev->funcs->pSelectBrush( dev, brush, pattern );
}

static HFONT dwmdrv_SelectFont( PHYSDEV dev, HFONT font, UINT *aa_flags )
{
    struct device *device = device_from_physdev( dev );
    struct cmd_select_font *cmd;

    TRACE( "device %p, font %p, aa_flags %p\n", device, font, aa_flags );

    if ((cmd = device_start_cmd( device, DWM_OP_SELECT_FONT, sizeof(*cmd) )))
    {
        /* if (aa_flags) FIXME( "device %p, font %p, aa_flags %p\n", device, font, aa_flags ); */
        cmd->font = font;
        device_end_cmd( device, &cmd->cmd );
    }

    if (SKIP) return font;
    dev = GET_NEXT_PHYSDEV( dev, pSelectFont );
    return dev->funcs->pSelectFont( dev, font, aa_flags );
}

static HPEN dwmdrv_SelectPen( PHYSDEV dev, HPEN pen, const struct brush_pattern *pattern )
{
    struct device *device = device_from_physdev( dev );
    struct cmd_select_pen *cmd;

    TRACE( "device %p, pen %p, pattern %p\n", device, pen, pattern );

    if ((cmd = device_start_cmd( device, DWM_OP_SELECT_PEN, sizeof(*cmd) )))
    {
        if (pattern) FIXME( "device %p, pen %p, pattern %p\n", device, pen, pattern );
        cmd->pen = pen;
        device_end_cmd( device, &cmd->cmd );
    }

    if (SKIP) return pen;
    dev = GET_NEXT_PHYSDEV( dev, pSelectPen );
    return dev->funcs->pSelectPen( dev, pen, pattern );
}

static UINT dwmdrv_SetBoundsRect( PHYSDEV dev, RECT *rect, UINT flags )
{
    struct device *device = device_from_physdev( dev );
    struct cmd_set_bounds_rect *cmd;

    TRACE( "device %p, rect %s, flags %#x\n", device, wine_dbgstr_rect( rect ), flags );

    if ((cmd = device_start_cmd( device, DWM_OP_SET_BOUNDS_RECT, sizeof(*cmd) )))
    {
        cmd->rect = *rect;
        cmd->flags = flags;
        device_end_cmd( device, &cmd->cmd );
    }

    if (SKIP) return 0;
    dev = GET_NEXT_PHYSDEV( dev, pSetBoundsRect );
    return dev->funcs->pSetBoundsRect( dev, rect, flags );
}

static COLORREF dwmdrv_SetDCBrushColor( PHYSDEV dev, COLORREF color )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return color;
}

static COLORREF dwmdrv_SetDCPenColor( PHYSDEV dev, COLORREF color )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return color;
}

static INT dwmdrv_SetDIBitsToDevice( PHYSDEV dev, INT x_dst, INT y_dst, DWORD width, DWORD height,
                                     INT x_src, INT y_src, UINT start, UINT lines, const void *bits,
                                     BITMAPINFO *info, UINT coloruse )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static void dwmdrv_SetDeviceClipping( PHYSDEV dev, HRGN clip )
{
    struct device *device = device_from_physdev( dev );

    TRACE( "device %p, clip %p\n", device, clip );

    device_set_points_cmd_from_hrgn( device, clip, DWM_OP_SET_DEVICE_CLIPPING );

    if (SKIP) return;
    dev = GET_NEXT_PHYSDEV( dev, pSetDeviceClipping );
    return dev->funcs->pSetDeviceClipping( dev, clip );
}

static BOOL dwmdrv_SetDeviceGammaRamp( PHYSDEV dev, void *ramp )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return FALSE;
}

static COLORREF dwmdrv_SetPixel( PHYSDEV dev, INT x, INT y, COLORREF color )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return color;
}

static BOOL dwmdrv_StrokeAndFillPath( PHYSDEV dev )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static BOOL dwmdrv_StrokePath( PHYSDEV dev )
{
    struct device *device = device_from_physdev( dev );
    FIXME( "device %p\n", device );
    return TRUE;
}

static BOOL dwmdrv_UnrealizePalette( HPALETTE palette )
{
    FIXME( "palette %p\n", palette );
    return FALSE;
}

const struct gdi_dc_funcs dwm_driver =
{
    /*.pAbortDoc = dwmdrv_AbortDoc,*/
    /*.pAbortPath = dwmdrv_AbortPath,*/
    /*.pAlphaBlend = dwmdrv_AlphaBlend,*/
    .pAngleArc = dwmdrv_AngleArc,
    .pArc = dwmdrv_Arc,
    .pArcTo = dwmdrv_ArcTo,
    /*.pBeginPath = dwmdrv_BeginPath,*/
    .pBlendImage = dwmdrv_BlendImage,
    .pChord = dwmdrv_Chord,
    /*.pCloseFigure = dwmdrv_CloseFigure,*/
    /*.pCreateCompatibleDC = dwmdrv_CreateCompatibleDC,*/
    .pCreateDC = dwmdrv_CreateDC,
    .pDeleteDC = dwmdrv_DeleteDC,
    .pDeleteObject = dwmdrv_DeleteObject,
    .pEllipse = dwmdrv_Ellipse,
    /*.pEndDoc = dwmdrv_EndDoc,*/
    /*.pEndPage = dwmdrv_EndPage,*/
    /*.pEndPath = dwmdrv_EndPath,*/
    /*.pEnumFonts = dwmdrv_EnumFonts,*/
    .pExtEscape = dwmdrv_ExtEscape,
    .pExtFloodFill = dwmdrv_ExtFloodFill,
    .pExtTextOut = dwmdrv_ExtTextOut,
    .pFillPath = dwmdrv_FillPath,
    .pFillRgn = dwmdrv_FillRgn,
    /*.pFontIsLinked = dwmdrv_FontIsLinked,*/
    .pFrameRgn = dwmdrv_FrameRgn,
    .pGetBoundsRect = dwmdrv_GetBoundsRect,
    /*.pGetCharABCWidths = dwmdrv_GetCharABCWidths,*/
    /*.pGetCharABCWidthsI = dwmdrv_GetCharABCWidthsI,*/
    /*.pGetCharWidth = dwmdrv_GetCharWidth,*/
    /*.pGetCharWidthInfo = dwmdrv_GetCharWidthInfo,*/
    .pGetDeviceCaps = dwmdrv_GetDeviceCaps,
    .pGetDeviceGammaRamp = dwmdrv_GetDeviceGammaRamp,
    /*.pGetFontData = dwmdrv_GetFontData,*/
    /*.pGetFontRealizationInfo = dwmdrv_GetFontRealizationInfo,*/
    /*.pGetFontUnicodeRanges = dwmdrv_GetFontUnicodeRanges,*/
    /*.pGetGlyphIndices = dwmdrv_GetGlyphIndices,*/
    /*.pGetGlyphOutline = dwmdrv_GetGlyphOutline,*/
    .pGetICMProfile = dwmdrv_GetICMProfile,
    .pGetImage = dwmdrv_GetImage,
    /*.pGetKerningPairs = dwmdrv_GetKerningPairs,*/
    .pGetNearestColor = dwmdrv_GetNearestColor,
    /*.pGetOutlineTextMetrics = dwmdrv_GetOutlineTextMetrics,*/
    .pGetPixel = dwmdrv_GetPixel,
    .pGetSystemPaletteEntries = dwmdrv_GetSystemPaletteEntries,
    /*.pGetTextCharsetInfo = dwmdrv_GetTextCharsetInfo,*/
    /*.pGetTextExtentExPoint = dwmdrv_GetTextExtentExPoint,*/
    /*.pGetTextExtentExPointI = dwmdrv_GetTextExtentExPointI,*/
    /*.pGetTextFace = dwmdrv_GetTextFace,*/
    /*.pGetTextMetrics = dwmdrv_GetTextMetrics,*/
    .pGradientFill = dwmdrv_GradientFill,
    .pInvertRgn = dwmdrv_InvertRgn,
    .pLineTo = dwmdrv_LineTo,
    /*.pMoveTo = dwmdrv_MoveTo,*/
    .pPaintRgn = dwmdrv_PaintRgn,
    .pPatBlt = dwmdrv_PatBlt,
    .pPie = dwmdrv_Pie,
    .pPolyBezier = dwmdrv_PolyBezier,
    .pPolyBezierTo = dwmdrv_PolyBezierTo,
    .pPolyDraw = dwmdrv_PolyDraw,
    .pPolyPolygon = dwmdrv_PolyPolygon,
    .pPolyPolyline = dwmdrv_PolyPolyline,
    .pPolylineTo = dwmdrv_PolylineTo,
    .pPutImage = dwmdrv_PutImage,
    .pRealizeDefaultPalette = dwmdrv_RealizeDefaultPalette,
    .pRealizePalette = dwmdrv_RealizePalette,
    .pRectangle = dwmdrv_Rectangle,
    /*.pResetDC = dwmdrv_ResetDC,*/
    .pRoundRect = dwmdrv_RoundRect,
    /*.pSelectBitmap = dwmdrv_SelectBitmap,*/
    .pSelectBrush = dwmdrv_SelectBrush,
    .pSelectFont = dwmdrv_SelectFont,
    .pSelectPen = dwmdrv_SelectPen,
    /*.pSetBkColor = dwmdrv_SetBkColor,*/
    .pSetBoundsRect = dwmdrv_SetBoundsRect,
    .pSetDCBrushColor = dwmdrv_SetDCBrushColor,
    .pSetDCPenColor = dwmdrv_SetDCPenColor,
    .pSetDIBitsToDevice = dwmdrv_SetDIBitsToDevice,
    .pSetDeviceClipping = dwmdrv_SetDeviceClipping,
    .pSetDeviceGammaRamp = dwmdrv_SetDeviceGammaRamp,
    .pSetPixel = dwmdrv_SetPixel,
    /*.pSetTextColor = dwmdrv_SetTextColor,*/
    /*.pStartDoc = dwmdrv_StartDoc,*/
    /*.pStartPage = dwmdrv_StartPage,*/
    /*.pStretchBlt = dwmdrv_StretchBlt,*/
    /*.pStretchDIBits = dwmdrv_StretchDIBits,*/
    .pStrokeAndFillPath = dwmdrv_StrokeAndFillPath,
    .pStrokePath = dwmdrv_StrokePath,
    .pUnrealizePalette = dwmdrv_UnrealizePalette,

    .priority = GDI_PRIORITY_DIB_DRV + 50,
    .name = "dwmdrv",
};
