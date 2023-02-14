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

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winuser.rh"

#include "ntuser.h"
#include "unixlib.h"

#include "wine/debug.h"
#include "wine/server.h"

WINE_DEFAULT_DEBUG_CHANNEL(imm);

#ifdef SONAME_LIBIBUS_1_0

static guint32 vk2ibus[] =
{
    [VK_CANCEL] = IBUS_Cancel,
    [VK_BACK] = IBUS_BackSpace,
    [VK_TAB] = IBUS_Tab,
    [VK_CLEAR] = IBUS_Clear,
    [VK_RETURN] = IBUS_Return,
    [VK_SHIFT] = IBUS_Shift_L,
    [VK_CONTROL] = IBUS_Control_L,
    [VK_MENU] = IBUS_Menu,
    [VK_PAUSE] = IBUS_Pause,
    [VK_CAPITAL] = IBUS_Caps_Lock,
    [VK_ESCAPE] = IBUS_Escape,
    [VK_MODECHANGE] = IBUS_Mode_switch,
    [VK_SPACE] = IBUS_space,
    [VK_PRIOR] = IBUS_Prior,
    [VK_NEXT] = IBUS_Next,
    [VK_END] = IBUS_End,
    [VK_HOME] = IBUS_Home,
    [VK_LEFT] = IBUS_Left,
    [VK_UP] = IBUS_Up,
    [VK_RIGHT] = IBUS_Right,
    [VK_DOWN] = IBUS_Down,
    [VK_SELECT] = IBUS_Select,
    [VK_PRINT] = IBUS_Print,
    [VK_EXECUTE] = IBUS_Execute,
    [VK_INSERT] = IBUS_Insert,
    [VK_DELETE] = IBUS_Delete,
    [VK_HELP] = IBUS_Help,
    [VK_LWIN] = IBUS_Super_L,
    [VK_RWIN] = IBUS_Super_R,
    [VK_NUMPAD0] = IBUS_KP_0,
    [VK_NUMPAD1] = IBUS_KP_1,
    [VK_NUMPAD2] = IBUS_KP_2,
    [VK_NUMPAD3] = IBUS_KP_3,
    [VK_NUMPAD4] = IBUS_KP_4,
    [VK_NUMPAD5] = IBUS_KP_5,
    [VK_NUMPAD6] = IBUS_KP_6,
    [VK_NUMPAD7] = IBUS_KP_7,
    [VK_NUMPAD8] = IBUS_KP_8,
    [VK_NUMPAD9] = IBUS_KP_9,
    [VK_MULTIPLY] = IBUS_KP_Multiply,
    [VK_ADD] = IBUS_KP_Add,
    [VK_SEPARATOR] = IBUS_KP_Separator,
    [VK_SUBTRACT] = IBUS_KP_Subtract,
    [VK_DECIMAL] = IBUS_KP_Decimal,
    [VK_DIVIDE] = IBUS_KP_Divide,
    [VK_F1] = IBUS_F1,
    [VK_F2] = IBUS_F2,
    [VK_F3] = IBUS_F3,
    [VK_F4] = IBUS_F4,
    [VK_F5] = IBUS_F5,
    [VK_F6] = IBUS_F6,
    [VK_F7] = IBUS_F7,
    [VK_F8] = IBUS_F8,
    [VK_F9] = IBUS_F9,
    [VK_F10] = IBUS_F10,
    [VK_F11] = IBUS_F11,
    [VK_F12] = IBUS_F12,
    [VK_F13] = IBUS_F13,
    [VK_F14] = IBUS_F14,
    [VK_F15] = IBUS_F15,
    [VK_F16] = IBUS_F16,
    [VK_F17] = IBUS_F17,
    [VK_F18] = IBUS_F18,
    [VK_F19] = IBUS_F19,
    [VK_F20] = IBUS_F20,
    [VK_F21] = IBUS_F21,
    [VK_F22] = IBUS_F22,
    [VK_F23] = IBUS_F23,
    [VK_F24] = IBUS_F24,
    [VK_NUMLOCK] = IBUS_Num_Lock,
    [VK_SCROLL] = IBUS_Scroll_Lock,
    [VK_LSHIFT] = IBUS_Shift_L,
    [VK_RSHIFT] = IBUS_Shift_R,
    [VK_LCONTROL] = IBUS_Control_L,
    [VK_RCONTROL] = IBUS_Control_R,
    [VK_LMENU] = IBUS_Alt_L,
    [VK_RMENU] = IBUS_Alt_R,
};

static IBusBus *ibus_bus;

POINT virtual_screen_to_root( INT x, INT y )
{
    RECT virtual = NtUserGetVirtualScreenRect();
    POINT pt;
    pt.x = x - virtual.left;
    pt.y = y - virtual.top;
    return pt;
}

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

static NTSTATUS ime_process_key( void *arg )
{
    struct ime_process_key_params *params = arg;
    IBusInputContext *ctx = (void *)(UINT_PTR)params->handle;
    POINT pos = virtual_screen_to_root( params->pos.x, params->pos.y );
    SIZE size = {16, 16};
    guint32 keyval = 0;

    if (params->vkey <= ARRAY_SIZE(vk2ibus)) keyval = vk2ibus[params->vkey];
    if (!keyval) keyval = ibus_unicode_to_keyval( params->wchr[0] );

    TRACE( "scan %#x, vkey %#x, wchr %s keyval %#x, pos %s\n",
           params->scan, params->vkey, debugstr_w(params->wchr), keyval,
           wine_dbgstr_point( &params->pos ) );

    SERVER_START_REQ( set_caret_info )
    {
        if (!wine_server_call_err( req ))
        {
            size.cx = reply->old_rect.right - reply->old_rect.left;
            size.cy = reply->old_rect.bottom - reply->old_rect.top;
        }
    }
    SERVER_END_REQ;

    ibus_input_context_set_cursor_location( ctx, pos.x, pos.y, size.cx, size.cy );
    params->ret = ibus_input_context_process_key_event( ctx, keyval, params->scan, 0 );

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

static NTSTATUS ime_process_key( void *arg )
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
    X( ime_process_key ),
#undef X
};
