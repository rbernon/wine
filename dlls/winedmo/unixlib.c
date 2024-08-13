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

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include "unix_private.h"

#include "wine/debug.h"

#define MAKE_UNSUPPORTED_ENTRY( name )                                                             \
    static NTSTATUS name( void *arg )                                                              \
    {                                                                                              \
        return STATUS_NOT_SUPPORTED;                                                               \
    }
MAKE_UNSUPPORTED_ENTRY( process_attach )
#undef MAKE_UNSUPPORTED_ENTRY

const unixlib_entry_t __wine_unix_call_funcs[] =
{
#define X( name ) [unix_##name] = name
    X( process_attach ),
};

C_ASSERT(ARRAY_SIZE(__wine_unix_call_funcs) == unix_funcs_count);

#ifdef _WIN64

const unixlib_entry_t __wine_unix_call_wow64_funcs[] =
{
#define X64( name ) [unix_##name] = wow64_##name
    X( process_attach ),
};

C_ASSERT(ARRAY_SIZE(__wine_unix_call_wow64_funcs) == unix_funcs_count);

#endif /* _WIN64 */
