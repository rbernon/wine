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

#include "windef.h"
#include "winbase.h"

#include "wine/debug.h"
#include "unixlib.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmo);

BOOL WINAPI DllMain( HINSTANCE instance, DWORD reason, void *reserved )
{
    TRACE( "instance %p, reason %lu, reserved %p\n", instance, reason, reserved );

    if (reason == DLL_PROCESS_ATTACH)
    {
        NTSTATUS status;
        DisableThreadLibraryCalls( instance );

        status = __wine_init_unix_call();
        if (!status) status = UNIX_CALL( process_attach, NULL );
        if (status) WARN( "Failed to init unixlib, status %#lx\n", status );
    }

    return TRUE;
}
