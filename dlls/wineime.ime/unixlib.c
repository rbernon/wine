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

static IBusBus *ibus_bus;

static NTSTATUS ime_init( void *arg )
{
    ibus_init();

    if (!(ibus_bus = ibus_bus_new()))
    {
        ERR( "Failed to create IBus bus.\n" );
        goto error;
    }
    if (!(ibus_bus_is_connected( ibus_bus )))
    {
        ERR( "Failed to connect to ibus-daemon.\n" );
        goto error;
    }

    return STATUS_SUCCESS;

error:
    if (ibus_bus) g_object_unref( ibus_bus );
    ibus_bus = NULL;
    return STATUS_UNSUCCESSFUL;
}

static NTSTATUS ime_exit( void *arg )
{
    TRACE( "\n" );

    if (ibus_bus) g_object_unref( ibus_bus );
    ibus_bus = NULL;

    ibus_quit();

    return STATUS_SUCCESS;
}

static NTSTATUS ime_main( void *arg )
{
    ibus_main();
    return STATUS_THREAD_IS_TERMINATING;
}

static NTSTATUS ime_create_context( void *arg )
{
    struct ime_create_context_params *params = arg;
    IBusInputContext *ctx;

    if (!(ctx = ibus_bus_create_input_context( ibus_bus, "WineIME" )))
    {
        ERR( "Failed to create IBus input context.\n" );
        return STATUS_UNSUCCESSFUL;
    }

    ibus_input_context_set_content_type( ctx, IBUS_INPUT_PURPOSE_FREE_FORM, IBUS_INPUT_HINT_NONE );
    ibus_input_context_set_capabilities( ctx, IBUS_CAP_FOCUS | IBUS_CAP_PREEDIT_TEXT | IBUS_CAP_SYNC_PROCESS_KEY );

    TRACE( "created IBusInputContext %p\n", ctx );

    params->handle = (UINT_PTR)(void *)ctx;
    return STATUS_SUCCESS;
}

static NTSTATUS ime_delete_context( void *arg )
{
    struct ime_delete_context_params *params = arg;
    IBusInputContext *ctx = (void *)(UINT_PTR)params->handle;

    TRACE( "ctx %p\n", ctx );

    g_object_unref( ctx );

    return STATUS_SUCCESS;
}

static NTSTATUS ime_activate_context( void *arg )
{
    struct ime_activate_context_params *params = arg;
    IBusInputContext *ctx = (void *)(UINT_PTR)params->handle;

    TRACE( "ctx %p, active %u\n", ctx, params->active );

    if (params->active) ibus_input_context_focus_in( ctx );
    else ibus_input_context_focus_out( ctx );

    return STATUS_SUCCESS;
}

#else

static NTSTATUS ime_init( void *arg )
{
    FIXME( "Not supported!\n" );
    return STATUS_NOT_SUPPORTED;
}

static NTSTATUS ime_exit( void *arg )
{
    FIXME( "Not supported!\n" );
    return STATUS_NOT_SUPPORTED;
}

static NTSTATUS ime_main( void *arg )
{
    FIXME( "Not supported!\n" );
    return STATUS_NOT_SUPPORTED;
}

static NTSTATUS ime_create_context( void *arg )
{
    FIXME( "Not supported!\n" );
    return STATUS_NOT_SUPPORTED;
}

static NTSTATUS ime_delete_context( void *arg )
{
    FIXME( "Not supported!\n" );
    return STATUS_NOT_SUPPORTED;
}

#endif /* SONAME_LIBIBUS_1_0 */

const unixlib_entry_t __wine_unix_call_funcs[] =
{
#define X( x ) [unix_ ## x] = x
    X( ime_init ),
    X( ime_exit ),
    X( ime_main ),
    X( ime_create_context ),
    X( ime_delete_context ),
    X( ime_activate_context ),
#undef X
};
