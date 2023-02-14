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

#include <stddef.h>
#include <stdarg.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "wingdi.h"
#include "winuser.h"

#include "imm.h"
#include "immdev.h"

#include "unixlib.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(imm);

struct ime_context
{
    HIMC imc;
    INPUTCONTEXT *input;
    UINT64 handle;
};

static struct ime_context *ime_acquire_context( HIMC imc )
{
    struct ime_context *ctx = NULL;
    INPUTCONTEXT *input;

    if (!(input = ImmLockIMC( imc )))
        WARN( "Failed to lock IME context\n" );
    else if (!(ctx = ImmLockIMCC( input->hPrivate )))
    {
        WARN( "Failed to lock private IME data\n" );
        ImmUnlockIMC( imc );
    }
    else
    {
        ctx->imc = imc;
        ctx->input = input;
    }

    return ctx;
}

static void ime_release_context( struct ime_context *ctx )
{
    ImmUnlockIMCC( ctx->input->hPrivate );
    ImmUnlockIMC( ctx->imc );
}

static HANDLE ime_thread;

static DWORD CALLBACK ime_thread_proc( void *arg )
{
    NTSTATUS status;

    TRACE( "Starting IME thread\n" );

    SetThreadDescription( GetCurrentThread(), L"wine_wineime_worker" );

    status = UNIX_CALL( ime_main, NULL );
    if (status == STATUS_THREAD_IS_TERMINATING) TRACE( "Exiting IME thread\n" );
    else WARN( "Exiting IME thread with status %#lx\n", status );

    return status;
}

static LRESULT CALLBACK ime_ui_window_proc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    TRACE( "hwnd %p, msg %#x, wparam %#Ix, lparam %#Ix\n", hwnd, msg, wparam, lparam );

    if (ImmIsUIMessageW( 0, msg, wparam, lparam )) return 0;
    return DefWindowProcW( hwnd, msg, wparam, lparam );
}

static WNDCLASSEXW ime_ui_class =
{
    .cbSize = sizeof(WNDCLASSEXW),
    .style = CS_IME,
    .lpfnWndProc = ime_ui_window_proc,
    .cbWndExtra = 2 * sizeof(LONG_PTR),
    .lpszClassName = L"WineIME",
};

BOOL WINAPI ImeInquire( IMEINFO *info, WCHAR *ui_class, DWORD flags )
{
    NTSTATUS status;

    FIXME( "info %p, ui_class %s, flags %#lx semi-stub!\n", info, debugstr_w(ui_class), flags );

    if ((status = UNIX_CALL( ime_init, NULL )))
        WARN( "Failed to initialize IME, status %#lx\n", status );
    else if (!(ime_thread = CreateThread( NULL, 0, ime_thread_proc, NULL, 0, NULL )))
    {
        WARN( "Failed to spawn IME helper thread\n" );
        status = STATUS_UNSUCCESSFUL;
    }
    if (status) return FALSE;

    info->dwPrivateDataSize = sizeof(struct ime_context);
    info->fdwProperty = IME_PROP_UNICODE | IME_PROP_AT_CARET;
    info->fdwConversionCaps = IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE;
    info->fdwSentenceCaps = IME_SMODE_AUTOMATIC;
    info->fdwUICaps = UI_CAP_2700;
    info->fdwSCSCaps = 0;
    info->fdwSelectCaps = SELECT_CAP_CONVERSION;
    wcscpy( ui_class, ime_ui_class.lpszClassName );

    return TRUE;
}

BOOL WINAPI ImeDestroy( UINT force )
{
    TRACE( "force %u\n", force );

    if (ime_thread)
    {
        UNIX_CALL( ime_exit, NULL );
        WaitForSingleObject( ime_thread, INFINITE );
        CloseHandle( ime_thread );
        ime_thread = NULL;
    }

    return TRUE;
}

BOOL WINAPI ImeConfigure( HKL hkl, HWND hwnd, DWORD mode, void *data )
{
    FIXME( "hkl %p, hwnd %p, mode %lu, data %p stub!\n", hkl, hwnd, mode, data );
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return FALSE;
}

DWORD WINAPI ImeConversionList( HIMC himc, const WCHAR *source, CANDIDATELIST *dest, DWORD dest_len, UINT flag )
{
    FIXME( "himc %p, source %s, dest %p, dest_len %lu, flag %#x stub!\n", himc, debugstr_w(source),
           dest, dest_len, flag );
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return 0;
}

UINT WINAPI ImeEnumRegisterWord( REGISTERWORDENUMPROCW proc, const WCHAR *reading, DWORD style,
                                 const WCHAR *string, void *data )
{
    FIXME( "proc %p, reading %s, style %lu, string %s, data %p stub!\n", proc, debugstr_w(reading),
           style, debugstr_w(string), data );
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return 0;
}

LRESULT WINAPI ImeEscape( HIMC himc, UINT escape, void *data )
{
    FIXME( "himc %p, escape %u, data %p stub!\n", himc, escape, data );
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return 0;
}

UINT WINAPI ImeGetRegisterWordStyle( UINT item, STYLEBUFW *style_buf )
{
    FIXME( "item %u, style_buf %p stub!\n", item, style_buf );
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return 0;
}

BOOL WINAPI ImeProcessKey( HIMC himc, UINT vkey, LPARAM key_data, BYTE *key_state )
{
    struct ime_process_key_params params = {.vkey = vkey, .scan = HIWORD(key_data) & 0x1ff};
    struct ime_context *ctx;
    RECT client;

    TRACE( "himc %p, vkey %u, key_data %#Ix, key_state %p\n", himc, vkey, key_data, key_state );

    if (!(ctx = ime_acquire_context( himc ))) return FALSE;

    ToUnicode( vkey, params.scan, key_state, params.wchr, ARRAY_SIZE(params.wchr), 0 );

    GetClientRect( GetFocus(), &client );
    MapWindowPoints( GetFocus(), HWND_DESKTOP, (POINT *)&client, 2 );

    GetCaretPos( &params.pos );
    params.pos.x += client.left;
    params.pos.y += client.top;

    params.handle = ctx->handle;
    UNIX_CALL( ime_process_key, &params );

    ime_release_context( ctx );
    return params.ret;
}

BOOL WINAPI ImeRegisterWord( const WCHAR *reading, DWORD style, const WCHAR *string )
{
    FIXME( "reading %s, style %lu, string %s stub!\n", debugstr_w(reading), style, debugstr_w(string) );
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return FALSE;
}

BOOL WINAPI ImeSelect( HIMC himc, BOOL select )
{
    struct ime_context *ctx;

    TRACE( "himc %p, select %d\n", himc, select );

    if (!(ctx = ime_acquire_context( himc ))) return FALSE;

    if (select)
    {
        struct ime_create_context_params params = {0};
        NTSTATUS status = UNIX_CALL( ime_create_context, &params );
        if (status) WARN( "Failed to create context, status %#lx\n", status );
        else ctx->handle = params.handle;
    }
    else
    {
        struct ime_delete_context_params params = {.handle = ctx->handle};
        NTSTATUS status = UNIX_CALL( ime_delete_context, &params );
        if (status) WARN( "Failed to delete context, status %#lx\n", status );
        else ctx->handle = 0;
    }

    ime_release_context( ctx );
    return TRUE;
}

BOOL WINAPI ImeSetActiveContext( HIMC himc, BOOL flag )
{
    struct ime_activate_context_params params = {.active = flag};
    struct ime_context *ctx;
    NTSTATUS status;

    TRACE( "himc %p, flag %#x\n", himc, flag );

    if (!(ctx = ime_acquire_context( himc ))) return FALSE;

    params.handle = ctx->handle;
    if ((status = UNIX_CALL( ime_activate_context, &params )))
        WARN( "Failed to create context, status %#lx\n", status );
    else
        ImmSetOpenStatus( himc, flag );

    ime_release_context( ctx );
    return !status;
}

BOOL WINAPI ImeSetCompositionString( HIMC himc, DWORD index, const void *comp, DWORD comp_len,
                                     const void *read, DWORD read_len )
{
    FIXME( "himc %p, index %lu, comp %p, comp_len %lu, read %p, read_len %lu stub!\n", himc, index,
           comp, comp_len, read, read_len );
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return FALSE;
}

UINT WINAPI ImeToAsciiEx( UINT vkey, UINT scan_code, BYTE *key_state, TRANSMSGLIST *list, UINT state, HIMC himc )
{
    FIXME( "vkey %u, scan_code %u, key_state %p, list %p, state %u, himc %p stub!\n", vkey,
           scan_code, key_state, list, state, himc );
    return 0;
}

BOOL WINAPI ImeUnregisterWord( const WCHAR *reading, DWORD style, const WCHAR *string )
{
    FIXME( "reading %s, style %lu, string %s stub!\n", debugstr_w(reading), style, debugstr_w(string) );
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return FALSE;
}

BOOL WINAPI NotifyIME( HIMC himc, DWORD action, DWORD index, DWORD value )
{
    FIXME( "himc %p, action %lu, index %lu, value %lu stub!\n", himc, action, index, value );
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return FALSE;
}

DWORD WINAPI ImeGetImeMenuItems( HIMC himc, DWORD flags, DWORD type, IMEMENUITEMINFOW *parent,
                                 IMEMENUITEMINFOW *menu, DWORD size )
{
    FIXME( "himc %p, flags %#lx, type %#lx, parent %p, menu %p, size %#lx\n", himc, flags, type, parent, menu, size );
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return 0;
}

BOOL WINAPI DllMain( HINSTANCE instance, DWORD reason, LPVOID reserved )
{
    NTSTATUS status;

    TRACE( "instance %p, reason %lu, reserved %p.\n", instance, reason, reserved );

    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls( instance );
        if ((status = __wine_init_unix_call()))
        {
            ERR( "Failed to load unixlib, status %#lx\n", status );
            return FALSE;
        }

        ime_ui_class.hInstance = instance;
        RegisterClassExW( &ime_ui_class );
        break;

    case DLL_PROCESS_DETACH:
        UnregisterClassW( ime_ui_class.lpszClassName, instance );
        break;
    }

    return TRUE;
}
