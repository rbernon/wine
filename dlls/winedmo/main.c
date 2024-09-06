/*
 * Copyright 2024 Rémi Bernon for CodeWeavers
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

#include <stddef.h>
#include <stdarg.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"

#include "wine/debug.h"
#include "unixlib.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmo);


static NTSTATUS WINAPI seek_callback( void *args, ULONG size )
{
    struct seek_callback_params *params = args;
    FIXME( "context %#I64x, offset %#I64x, stub!\n", params->context, params->offset );
    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS WINAPI read_callback( void *args, ULONG size )
{
    struct read_callback_params *params = args;
    FIXME( "context %#I64x, size %#x, stub!\n", params->context, params->size );
    return STATUS_NOT_IMPLEMENTED;
}


BOOL WINAPI DllMain( HINSTANCE instance, DWORD reason, void *reserved )
{
    TRACE( "instance %p, reason %lu, reserved %p\n", instance, reason, reserved );

    if (reason == DLL_PROCESS_ATTACH)
    {
        struct process_attach_params params =
        {
            .seek_callback = (UINT_PTR)seek_callback,
            .read_callback = (UINT_PTR)read_callback,
        };
        NTSTATUS status;

        DisableThreadLibraryCalls( instance );

        status = __wine_init_unix_call();
        if (!status) status = UNIX_CALL( process_attach, &params );
        if (status) WARN( "Failed to init unixlib, status %#lx\n", status );
    }

    return TRUE;
}


NTSTATUS CDECL winedmo_demuxer_check( const char *mime_type )
{
    struct demuxer_check_params params = {0};
    NTSTATUS status;

    TRACE( "mime_type %s\n", debugstr_a(mime_type) );
    lstrcpynA( params.mime_type, mime_type, sizeof(params.mime_type) );

    if ((status = UNIX_CALL( demuxer_check, &params ))) WARN( "returning %#lx\n", status );
    return status;
}

NTSTATUS CDECL winedmo_demuxer_create( struct winedmo_demuxer *demuxer )
{
    struct demuxer_create_params params = {0};
    NTSTATUS status;

    TRACE( "demuxer %p\n", demuxer );

    if ((status = UNIX_CALL( demuxer_create, &params )))
    {
        WARN( "demuxer_create failed, status %#lx\n", status );
        return status;
    }

    *demuxer = params.demuxer;
    TRACE( "created %#I64x\n", demuxer->handle );
    return STATUS_SUCCESS;
}

NTSTATUS CDECL winedmo_demuxer_destroy( struct winedmo_demuxer *demuxer )
{
    struct demuxer_destroy_params params = {.demuxer = *demuxer};
    NTSTATUS status;

    if (!demuxer->handle) return STATUS_SUCCESS;

    TRACE( "demuxer %#I64x\n", demuxer->handle );

    demuxer->handle = 0;
    status = UNIX_CALL( demuxer_destroy, &params );
    if (status) WARN( "demuxer_destroy failed, status %#lx\n", status );

    return status;
}
