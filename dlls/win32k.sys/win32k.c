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

        if (out_size > buf_size)
        {
            if (buf != ioctl_buffer) buf = HeapReAlloc( GetProcessHeap(), 0, buf, out_size );
            else
            {
                buf = HeapAlloc( GetProcessHeap(), 0, out_size );
                memcpy( buf, ioctl_buffer, in_size );
            }
            if (!buf) status = STATUS_NO_MEMORY;
            buf_size = out_size;
        }

        if (status == STATUS_BUFFER_OVERFLOW) continue;
        if (status) break;

        TRACE( "processing desktop %s ioctl (code=%08x) size %x\n",
               debugstr_win32u_ioctl( code ), code, in_size );

        out_size = 0;
        switch (code)
        {
        default:
        {
            FIXME( "unimplemented desktop %s ioctl (code=%08x) size %x\n",
                   debugstr_win32u_ioctl( code ), code, in_size );
            status = STATUS_NOT_IMPLEMENTED;
            out_size = 0;
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
