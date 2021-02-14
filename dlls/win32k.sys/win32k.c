/*
 * Copyright 2021 Rémi Bernon for CodeWeavers
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

#include <stdarg.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winnt.h"
#include "winuser.h"

#include "wine/win32u.h"
#include "wine/server.h"
#include "wine/debug.h"

#include "unixlib.h"

WINE_DEFAULT_DEBUG_CHANNEL(win32k);

static struct unix_funcs *unix_funcs;

static void process_desktop_ioctls(HDESK desktop)
{
    char ioctl_buffer[512], *buf = ioctl_buffer;
    data_size_t out_size = 0, in_size, buf_size = sizeof(ioctl_buffer);
    unsigned int code;
    NTSTATUS status = STATUS_SUCCESS;

    for (;;)
    {
        if (status) out_size = 0;

        SERVER_START_REQ( get_desktop_ioctl )
        {
            req->handle = wine_server_obj_handle( desktop );
            req->status = status;
            wine_server_add_data( req, buf, out_size );
            wine_server_set_reply( req, buf, buf_size );
            status = wine_server_call( req );
            code = reply->code;
            out_size = reply->out_size;
            in_size = wine_server_reply_size( reply );
        }
        SERVER_END_REQ;

        if (status == STATUS_PENDING) break;
        if (status == STATUS_BUFFER_OVERFLOW)
        {
            if (buf != ioctl_buffer) buf = HeapReAlloc( GetProcessHeap(), 0, buf, out_size );
            else buf = HeapAlloc( GetProcessHeap(), 0, out_size );
            if (!buf) status = STATUS_NO_MEMORY;
            else status = STATUS_SUCCESS;
            buf_size = out_size;
            continue;
        }

        TRACE( "processing desktop %s ioctl (code=%08x) size %x\n",
               debugstr_win32u_ioctl( code ), code, in_size );

        out_size = 0;
        switch (code)
        {
        case IOCTL_WIN32U_CREATE_WINDOW:
        {
            struct win32u_create_window_input in;
            if (in_size != sizeof(in)) status = STATUS_INVALID_PARAMETER;
            else
            {
                memcpy( &in, buf, sizeof(in) );
                if (unix_funcs->create_window( UlongToHandle(in.hwnd) )) status = STATUS_SUCCESS;
                else status = STATUS_INVALID_PARAMETER;
            }
            break;
        }
        case IOCTL_WIN32U_DESTROY_WINDOW:
        {
            struct win32u_destroy_window_input in;
            if (in_size != sizeof(in)) status = STATUS_INVALID_PARAMETER;
            else
            {
                memcpy( &in, buf, sizeof(in) );
                unix_funcs->destroy_window( UlongToHandle(in.hwnd) );
                status = STATUS_SUCCESS;
            }
            break;
        }
        case IOCTL_WIN32U_WINDOW_POS_CHANGING:
        {
            struct win32u_window_pos_changing_input in;
            struct win32u_window_pos_changing_output out;
            struct window_surface *surface = NULL;
            void *driver_handle;
            if (in_size != sizeof(in)) status = STATUS_INVALID_PARAMETER;
            else
            {
                memcpy( &in, buf, sizeof(in) );
                unix_funcs->window_pos_changing( UlongToHandle(in.hwnd), UlongToHandle(in.insert_after),
                                                 in.swp_flags, &in.window_rect, &in.client_rect,
                                                 &out.visible_rect, &surface, &out.screen_rect,
                                                 &driver_handle );
                out.unix_handle = (UINT_PTR)driver_handle;
                if (surface) window_surface_release( surface );
                memcpy( buf, &out, sizeof(out) );
                out_size = sizeof(out);
                status = STATUS_SUCCESS;
            }
            break;
        }
        case IOCTL_WIN32U_WINDOW_POS_CHANGED:
        {
            struct win32u_window_pos_changed_input in;
            struct window_surface *surface = NULL;
            if (in_size != sizeof(in)) status = STATUS_INVALID_PARAMETER;
            else
            {
                memcpy( &in, buf, sizeof(in) );
                unix_funcs->window_pos_changed( UlongToHandle(in.hwnd), UlongToHandle(in.insert_after),
                                                in.swp_flags, &in.window_rect, &in.client_rect, &in.visible_rect,
                                                in.valid_rects_set ? &in.valid_rects : NULL, surface );
                status = STATUS_SUCCESS;
            }
            break;
        }
        case IOCTL_WIN32U_SHOW_WINDOW:
        {
            struct win32u_show_window_input in;
            struct win32u_show_window_output out;
            if (in_size != sizeof(in)) status = STATUS_INVALID_PARAMETER;
            else
            {
                memcpy( &in, buf, sizeof(in) );
                out.swp = unix_funcs->show_window( UlongToHandle(in.hwnd), in.cmd, &in.rect, in.swp );
                memcpy( buf, &out, sizeof(out) );
                out_size = sizeof(out);
                status = STATUS_SUCCESS;
            }
            break;
        }
        case IOCTL_WIN32U_SET_WINDOW_STYLE:
        {
            struct win32u_set_window_style_input in;
            if (in_size != sizeof(in)) status = STATUS_INVALID_PARAMETER;
            else
            {
                memcpy( &in, buf, sizeof(in) );
                unix_funcs->set_window_style( UlongToHandle(in.hwnd), in.offset, &in.style );
                status = STATUS_SUCCESS;
            }
            break;
        }
        case IOCTL_WIN32U_SET_FOCUS:
        {
            struct win32u_set_focus_input in;
            if (in_size != sizeof(in)) status = STATUS_INVALID_PARAMETER;
            else
            {
                memcpy( &in, buf, sizeof(in) );
                unix_funcs->set_focus( UlongToHandle(in.hwnd) );
                status = STATUS_SUCCESS;
            }
            break;
        }
        case IOCTL_WIN32U_SET_PARENT:
        {
            struct win32u_set_parent_input in;
            if (in_size != sizeof(in)) status = STATUS_INVALID_PARAMETER;
            else
            {
                memcpy( &in, buf, sizeof(in) );
                unix_funcs->set_parent( UlongToHandle(in.hwnd), UlongToHandle(in.parent),
                                        UlongToHandle(in.old_parent) );
                status = STATUS_SUCCESS;
            }
            break;
        }
        case IOCTL_WIN32U_TO_UNICODE:
        {
            struct win32u_to_unicode_input in;
            if (in_size != sizeof(in)) status = STATUS_INVALID_PARAMETER;
            else
            {
                memcpy( &in, buf, sizeof(in) );
                out_size = unix_funcs->to_unicode( in.vkey, in.scancode, in.keystate, (void *)buf,
                                                   buf_size, in.flags, UlongToHandle(in.hkl) );
                out_size *= sizeof(WCHAR);
                status = STATUS_SUCCESS;
            }
            break;
        }
        default:
        {
            FIXME( "unimplemented desktop %s ioctl (code=%08x) size %x\n",
                   debugstr_win32u_ioctl( code ), code, in_size );
            status = STATUS_NOT_IMPLEMENTED;
            break;
        }
        }
    }

    if (buf != ioctl_buffer) HeapFree( GetProcessHeap(), 0, buf );
}

DWORD CDECL win32k_MsgWaitForMultipleObjectsEx( DWORD count, const HANDLE *handles, DWORD timeout,
                                                DWORD mask, DWORD flags )
{
    HANDLE *wait_handles;
    DWORD res, wait_count = count;
    HDESK desktop = GetThreadDesktop( GetCurrentThreadId() );

    TRACE( "count %u, handles %p, timeout %u, mask %x, flags %x.\n",
           count, handles, timeout, mask, flags );

    wait_handles = HeapAlloc( GetProcessHeap(), 0, (count + 1) * sizeof(HANDLE) );
    if (handles && count) memcpy( wait_handles, handles, count * sizeof(HANDLE) );

    SERVER_START_REQ( get_desktop_wait_handle )
    {
        req->handle = wine_server_obj_handle( desktop );
        if (wine_server_call( req )) WARN( "Unable to get a desktop wait handle!\n" );
        else wait_handles[wait_count++] = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;

    do
    {
        res = unix_funcs->msg_wait_for_multiple_objects( wait_count, wait_handles, timeout, mask, flags );
        if (res == WAIT_OBJECT_0 + count) process_desktop_ioctls( desktop );
    }
    while (res == WAIT_OBJECT_0 + count);

    HeapFree( GetProcessHeap(), 0, wait_handles );
    return res;
}

BOOL WINAPI DllMain( HINSTANCE instance, DWORD reason, LPVOID reserved )
{
    TRACE( "instance %p, reason %d, reserved %p.\n", instance, reason, reserved );

    switch (reason)
    {
    case DLL_PROCESS_ATTACH: DisableThreadLibraryCalls( instance ); break;
    case DLL_PROCESS_DETACH: break;
    }

    if (__wine_init_unix_lib( instance, reason, NULL, &unix_funcs )) return FALSE;
    return TRUE;
}
