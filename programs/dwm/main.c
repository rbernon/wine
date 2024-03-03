/*
 * Copyright 2022 Rémi Bernon for CodeWeavers
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
#include <stddef.h>
#include <assert.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"

#include "unixlib.h"

#include "wine/dwmapi.h"
#include "wine/server.h"
#include "wine/debug.h"

struct dwm_connection
{
    HANDLE request;
    HANDLE reply;
};

static DWORD WINAPI dwm_client_thread( void *args )
{
    dwm_display_t display = DWM_INVALID_DISPLAY;
    struct dwm_connection *connection = args;
    NTSTATUS status = STATUS_SUCCESS;
    DWORD req_size, reply_size;
    BOOL ret;

    status = dwm_client_loop( connection->request, connection->reply );

    for (;;)
    {
        union dwm_request req;
        union dwm_reply reply;

        if (!(ret = ReadFile( connection->request, &req, sizeof(req), &req_size, NULL )) ||
            req_size < sizeof(req.header) || req_size != req.header.req_size)
        {
            ERR( "failed to read request, ret %d, read %lu, size %u, error %lu\n",
                 ret, req_size, req.header.req_size, GetLastError() );
            break;
        }

        switch (req.header.type)
        {
        case DWM_REQ_CONNECT:
            if (req_size < sizeof(req.connect)) status = STATUS_BUFFER_TOO_SMALL;
            else if (req.connect.version != DWM_PROTOCOL_VERSION)
            {
                ERR( "client version mismatch, version %u != %u\n", req.connect.version, DWM_PROTOCOL_VERSION );
                status = STATUS_INVALID_PARAMETER;
            }
            else
            {
                if (display != DWM_INVALID_DISPLAY) dwm_disconnect( display );
                display = dwm_connect( req.connect.display_name );
            }
            break;

        case DWM_REQ_WINDOW_POS_CHANGING:
            if (req_size < sizeof(req.window_pos_changing)) status = STATUS_BUFFER_TOO_SMALL;
            else status = WINE_UNIX_CALL( unix_dwm_window_pos_changing, &req.window_pos_changing );
            break;
        case DWM_REQ_WINDOW_POS_CHANGED:
            if (req_size < sizeof(req.window_pos_changed)) status = STATUS_BUFFER_TOO_SMALL;
            else status = WINE_UNIX_CALL( unix_dwm_window_pos_changed, &req.window_pos_changed );
            break;
        case DWM_REQ_WINDOW_DESTROY:
            if (req_size < sizeof(req.window_destroy)) status = STATUS_BUFFER_TOO_SMALL;
            else status = WINE_UNIX_CALL( unix_dwm_window_destroy, &req.window_destroy );
            break;
        }

        if ((reply.header.status = status)) ERR( "got status %#lx\n", status );
        if (!(ret = WriteFile( connection->reply, &reply, sizeof(reply), &reply_size, NULL )) ||
            reply_size != sizeof(reply))
        {
            ERR( "failed to write reply, ret %d, read %lu, size %Iu, error %lu\n",
                 ret, reply_size, sizeof(reply), GetLastError() );
            break;
        }
    }

    if (display != DWM_INVALID_DISPLAY) dwm_disconnect( display );
    CloseHandle( connection->request );
    CloseHandle( connection->reply );
    free( connection );
    return status;
}

static BOOL dwm_server_loop(void)
{
    HANDLE control[2];
    NTSTATUS status;
    DWORD ret;

    TRACE( "\n" );

    SERVER_START_REQ( dwm_create_server )
    {
        req->access     = SYNCHRONIZE;
        req->attributes = 0;
        if (wine_server_call_err( req )) return FALSE;
        else control[0] = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;

    if ((status = NtSetInformationProcess( GetCurrentProcess(), ProcessWineMakeProcessSystem,
                                           &control[1], sizeof(HANDLE *) )))
    {
        CloseHandle( control[0] );
        SetLastError( RtlNtStatusToDosError( status ) );
        return FALSE;
    }

    LoadLibraryW( L"user32" );
    if ((status = __wine_init_unix_call())) ERR( "unixlib initialization failed, status %#lx\n", status );
    dwm_init();

    while (!(ret = WaitForMultipleObjects( 2, control, FALSE, INFINITE )))
    {
        struct dwm_connection *connection;
        HANDLE thread;

        if (!(connection = calloc( 1, sizeof(*connection) )))
        {
            SetLastError( ERROR_OUTOFMEMORY );
            break;
        }

        SERVER_START_REQ( dwm_server_accept )
        {
            req->server = wine_server_obj_handle( control[0] );
            if (!(ret = wine_server_call_err( req )))
            {
                connection->request = wine_server_ptr_handle( reply->req_handle );
                connection->reply = wine_server_ptr_handle( reply->reply_handle );
            }
        }
        SERVER_END_REQ;

        if (!ret && (thread = CreateThread( NULL, 0, dwm_client_thread, connection, 0, NULL )))
        {
            CloseHandle( thread );
            connection = NULL;
        }

        if (ret != STATUS_PENDING && connection)
        {
            if (connection->request) CloseHandle( connection->request );
            if (connection->reply) CloseHandle( connection->reply );
            free( connection );
            break;
        }

        free( connection );
    }

    CloseHandle( control[0] );
    CloseHandle( control[1] );
    return ret == ARRAY_SIZE(control) - 1;
}

int __cdecl wmain( int argc, WCHAR *argv[] )
{
    NTSTATUS status;

    TRACE( "\n" );

    if ((status = !dwm_server_loop())) ERR( "server loop failed, error %ld\n", GetLastError() );

    TRACE( "exiting\n" );
    return status;
}
