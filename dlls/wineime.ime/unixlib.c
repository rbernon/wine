/*
 * Copyright 2023 Rémi Bernon for CodeWeavers
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

#ifdef HAVE_IBUS_H
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wstrict-prototypes"
# include <ibus.h>
# pragma GCC diagnostic pop
#endif

#include "unixlib.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(imm);

#ifdef SONAME_LIBIBUS_1_0

static NTSTATUS ime_init( void *arg )
{
    FIXME( "stub!\n" );
    return STATUS_NOT_IMPLEMENTED;
}

#else

static NTSTATUS ime_init( void *arg )
{
    FIXME( "Not supported!\n" );
    return STATUS_NOT_SUPPORTED;
}

#endif /* SONAME_LIBIBUS_1_0 */

const unixlib_entry_t __wine_unix_call_funcs[] =
{
#define X( x ) [unix_ ## x] = x
    X( ime_init ),
#undef X
};
