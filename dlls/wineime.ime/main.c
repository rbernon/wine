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
    COMPOSITIONSTRING *composition;
    UINT64 handle;
};

static struct ime_context *ime_acquire_context( HIMC imc )
{
    struct ime_context *ctx = NULL;
    COMPOSITIONSTRING *composition;
    INPUTCONTEXT *input;

    if (!(input = ImmLockIMC( imc )))
        WARN( "Failed to lock IME context\n" );
    else if (!(composition = ImmLockIMCC( input->hCompStr )))
    {
        WARN( "Failed to lock IME composition string\n" );
        ImmUnlockIMC( imc );
    }
    else if (!(ctx = ImmLockIMCC( input->hPrivate )))
    {
        WARN( "Failed to lock private IME data\n" );
        ImmUnlockIMC( input->hCompStr );
        ImmUnlockIMC( imc );
    }
    else
    {
        ctx->imc = imc;
        ctx->input = input;
        ctx->composition = composition;
    }

    return ctx;
}

static void ime_release_context( struct ime_context *ctx )
{
    if (ctx->composition) ImmUnlockIMCC( ctx->input->hCompStr );
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

static const char *debugstr_composition_string( COMPOSITIONSTRING *str )
{
    char buffer[1024], *end = buffer + sizeof(buffer), *buf = buffer;

    if (!str) return "(null)";

    buf += snprintf( buf, end - buf, "str %p (%#lx), ", str, str->dwSize );

    if (str->dwCompReadAttrOffset)
        buf += snprintf( buf, end - buf, "comp read attr %#lx (%#lx), ", str->dwCompReadAttrOffset,
                         str->dwCompReadAttrLen );
    if (str->dwCompReadClauseOffset)
        buf += snprintf( buf, end - buf, "comp read clause %#lx (%#lx), ",
                         str->dwCompReadClauseOffset, str->dwCompReadClauseLen );
    if (str->dwCompReadStrOffset)
        buf += snprintf( buf, end - buf, "comp read str %#lx (%s), ", str->dwCompReadStrOffset,
                         debugstr_wn((WCHAR *)( (BYTE *)str + str->dwCompReadStrOffset ), str->dwCompReadStrLen) );

    if (str->dwCompAttrOffset)
        buf += snprintf( buf, end - buf, "comp attr %#lx (%#lx), ", str->dwCompAttrOffset, str->dwCompAttrLen );
    if (str->dwCompClauseOffset)
        buf += snprintf( buf, end - buf, "comp clause %#lx (%#lx), ", str->dwCompClauseOffset, str->dwCompClauseLen );
    if (str->dwCompStrOffset)
        buf += snprintf( buf, end - buf, "comp str %#lx (%s), ", str->dwCompStrOffset,
                         debugstr_wn((WCHAR *)( (BYTE *)str + str->dwCompStrOffset ), str->dwCompStrLen) );

    buf += snprintf( buf, end - buf, "cursor %#lx, delta %#lx, ", str->dwCursorPos, str->dwDeltaStart );

    if (str->dwResultReadClauseOffset)
        buf += snprintf( buf, end - buf, "result read clause %#lx (%#lx), ",
                         str->dwResultReadClauseOffset, str->dwResultReadClauseLen );
    if (str->dwResultReadStrOffset)
        buf += snprintf( buf, end - buf, "result read str %#lx (%s), ", str->dwResultReadStrOffset,
                         debugstr_wn((WCHAR *)( (BYTE *)str + str->dwResultReadStrOffset ), str->dwResultReadStrLen) );

    if (str->dwResultClauseOffset)
        buf += snprintf( buf, end - buf, "result clause %#lx (%#lx), ", str->dwResultClauseOffset,
                         str->dwResultClauseLen );
    if (str->dwResultStrOffset)
        buf += snprintf( buf, end - buf, "result str %#lx (%s), ", str->dwResultStrOffset,
                         debugstr_wn((WCHAR *)( (BYTE *)str + str->dwResultStrOffset ), str->dwResultStrLen) );

    buf += snprintf( buf, end - buf, "private %#lx (%#lx)", str->dwPrivateOffset, str->dwPrivateSize );

    return wine_dbg_sprintf( "%s", buffer );
}

static COMPOSITIONSTRING *composition_string_realloc( struct ime_context *ctx, SIZE_T grow )
{
    INPUTCONTEXT *input = ctx->input;
    COMPOSITIONSTRING *str = NULL;
    HIMCC tmp;

    if (!(tmp = ImmCreateIMCC( ctx->composition->dwSize + grow )))
        WARN( "Failed to resize IME composition string\n" );
    else if (!(str = ImmLockIMCC( tmp )))
        WARN( "Failed to lock IME composition string\n" );
    else
        memcpy( str, ctx->composition, ctx->composition->dwSize );

    ImmUnlockIMCC( input->hCompStr );
    ImmDestroyIMCC( input->hCompStr );
    input->hCompStr = tmp;
    ctx->composition = str;

    return ctx->composition;
}

static void *composition_string_insert( COMPOSITIONSTRING *str, DWORD offset, DWORD old_len, DWORD new_len )
{
    BYTE *dst = (BYTE *)str + offset, *new_end = (BYTE *)dst + new_len, *old_end = (BYTE *)dst + old_len;

    if (str->dwCompReadAttrOffset > offset) str->dwCompReadAttrOffset += new_len - old_len;
    if (str->dwCompReadClauseOffset > offset) str->dwCompReadClauseOffset += new_len - old_len;
    if (str->dwCompReadStrOffset > offset) str->dwCompReadStrOffset += new_len - old_len;
    if (str->dwCompAttrOffset > offset) str->dwCompAttrOffset += new_len - old_len;
    if (str->dwCompClauseOffset > offset) str->dwCompClauseOffset += new_len - old_len;
    if (str->dwCompStrOffset > offset) str->dwCompStrOffset += new_len - old_len;
    if (str->dwResultReadClauseOffset > offset) str->dwResultReadClauseOffset += new_len - old_len;
    if (str->dwResultReadStrOffset > offset) str->dwResultReadStrOffset += new_len - old_len;
    if (str->dwResultClauseOffset > offset) str->dwResultClauseOffset += new_len - old_len;
    if (str->dwResultStrOffset > offset) str->dwResultStrOffset += new_len - old_len;
    if (str->dwPrivateOffset > offset) str->dwPrivateOffset += new_len - old_len;

    memmove( new_end, old_end, str->dwSize - (old_end - (BYTE *)str) );

    str->dwSize += new_len - old_len;
    return dst;
}

static BOOL ime_update_comp_string( struct ime_context *ctx, const WCHAR *preedit_str, SIZE_T preedit_len )
{
    COMPOSITIONSTRING *str = ctx->composition;
    DWORD grow = 0;
    BYTE *dst;

    if (str->dwCompStrLen < preedit_len) grow += (preedit_len - str->dwCompStrLen) * sizeof(WCHAR);
    if (str->dwCompClauseLen < 2 * sizeof(DWORD)) grow += 2 * sizeof(DWORD) - str->dwCompClauseLen;
    if (str->dwCompAttrLen < preedit_len) grow += preedit_len - str->dwCompAttrLen;
    if (grow && !(str = composition_string_realloc( ctx, grow ))) return FALSE;

    if (!str->dwCompStrOffset) str->dwCompStrOffset = str->dwSize;
    dst = composition_string_insert( str, str->dwCompStrOffset, str->dwCompStrLen * sizeof(WCHAR), preedit_len * sizeof(WCHAR) );
    if (preedit_str) memcpy( dst, preedit_str, preedit_len * sizeof(WCHAR) );
    if (!(str->dwCompStrLen = preedit_len)) str->dwCompStrOffset = 0;

    if (!str->dwCompClauseOffset) str->dwCompClauseOffset = str->dwSize;
    dst = composition_string_insert( str, str->dwCompClauseOffset, str->dwCompClauseLen, 2 * sizeof(DWORD) );
    *(DWORD *)(dst + 0 * sizeof(DWORD)) = 0;
    *(DWORD *)(dst + 1 * sizeof(DWORD)) = preedit_len;
    str->dwCompClauseLen = 2 * sizeof(DWORD);

    if (!str->dwCompAttrOffset) str->dwCompAttrOffset = str->dwSize;
    dst = composition_string_insert( str, str->dwCompAttrOffset, str->dwCompAttrLen, preedit_len );
    memset( dst, ATTR_INPUT, preedit_len );
    if (!(str->dwCompAttrLen = preedit_len)) str->dwCompAttrOffset = 0;

    str->dwDeltaStart = 0;
    return TRUE;
}

static BOOL ime_update_result_string( struct ime_context *ctx, const WCHAR *result_str, SIZE_T result_len )
{
    COMPOSITIONSTRING *str = ctx->composition;
    DWORD grow = 0;
    BYTE *dst;

    if (str->dwResultStrLen < result_len) grow += (result_len - str->dwResultStrLen) * sizeof(WCHAR);
    if (str->dwResultClauseLen < 2 * sizeof(DWORD)) grow += 2 * sizeof(DWORD) - str->dwResultClauseLen;
    if (grow && !(str = composition_string_realloc( ctx, grow ))) return FALSE;

    if (!str->dwResultStrOffset) str->dwResultStrOffset = str->dwSize;
    dst = composition_string_insert( str, str->dwResultStrOffset, str->dwResultStrLen * sizeof(WCHAR), result_len * sizeof(WCHAR) );
    if (result_str) memcpy( dst, result_str, result_len * sizeof(WCHAR) );
    if (!(str->dwResultStrLen = result_len)) str->dwResultStrOffset = 0;

    if (!str->dwResultClauseOffset) str->dwResultClauseOffset = str->dwSize;
    dst = composition_string_insert( str, str->dwResultClauseOffset, str->dwResultClauseLen, 2 * sizeof(DWORD) );
    *(DWORD *)(dst + 0 * sizeof(DWORD)) = 0;
    *(DWORD *)(dst + 1 * sizeof(DWORD)) = result_len;
    str->dwResultClauseLen = 2 * sizeof(DWORD);

    str->dwDeltaStart = 0;
    return TRUE;
}

static void ime_send_message( struct ime_context *ctx, UINT msg, WPARAM wparam, LPARAM lparam )
{
    TRANSMSG *messages, message = {.message = msg, .wParam = wparam, .lParam = lparam};
    INPUTCONTEXT *input = ctx->input;
    DWORD count = input->dwNumMsgBuf + 1;
    HIMCC tmp = input->hMsgBuf;

    if (!(tmp = ImmReSizeIMCC( input->hMsgBuf, count * sizeof(message) )))
    {
        WARN( "Failed to resize input context message buffer\n" );
        return;
    }

    input->hMsgBuf = tmp;
    if (!(messages = ImmLockIMCC( input->hMsgBuf )))
        WARN( "Failed to lock input context message buffer\n" );
    else
    {
        messages[input->dwNumMsgBuf++] = message;
        ImmUnlockIMCC( input->hMsgBuf );
        ImmGenerateMessage( ctx->imc );
    }
}

static LRESULT CALLBACK ime_ui_window_proc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    TRACE( "hwnd %p, msg %#x, wparam %#Ix, lparam %#Ix\n", hwnd, msg, wparam, lparam );

    if (msg == WM_IME_NOTIFY && wparam == IMN_PRIVATE)
    {
        struct ime_context *ctx;
        struct ime_event *event;
        NTSTATUS status;
        HIMC himc;

        if (wparam != IMN_PRIVATE) return 0;
        if (!(himc = (HIMC)GetWindowLongPtrW( hwnd, IMMGWL_IMC ))) return 0;
        himc = *(HIMC *)himc; /* FIXME */

        if (!(ctx = ime_acquire_context( himc ))) return 0;

        if (!(event = malloc( lparam ))) goto error;
        event->handle = ctx->handle;
        event->buffer_len = lparam;

        if ((status = UNIX_CALL( ime_get_event, event )))
            WARN( "Failed to get IME event, status %#lx\n", status );
        else if (event->index == GCS_RESULT)
        {
            TRACE( "index %#x, cursor_pos %u, result %s\n", event->index, event->cursor_pos,
                   debugstr_wn( event->buffer, event->buffer_len ) );

            if (!ctx->composition->dwCompStrLen) ime_send_message( ctx, WM_IME_STARTCOMPOSITION, 0, 0 );
            if (!ime_update_result_string( ctx, event->buffer, event->buffer_len )) goto error;
            if (!ime_update_comp_string( ctx, NULL, 0 )) goto error;
            ctx->composition->dwCursorPos = event->cursor_pos;

            TRACE( "Received result %s\n", debugstr_composition_string( ctx->composition ) );
            ime_send_message( ctx, WM_IME_COMPOSITION, event->buffer[0], GCS_RESULT | GCS_COMP | GCS_CURSORPOS );
            ime_send_message( ctx, WM_IME_ENDCOMPOSITION, 0, 0 );
        }
        else if (event->index == GCS_COMP)
        {
            TRACE( "index %#x, cursor_pos %u, comp %s\n", event->index, event->cursor_pos,
                   debugstr_wn( event->buffer, event->buffer_len ) );

            if (!ctx->composition->dwCompStrLen) ime_send_message( ctx, WM_IME_STARTCOMPOSITION, 0, 0 );
            if (!ime_update_comp_string( ctx, event->buffer, event->buffer_len )) goto error;
            if (!ime_update_result_string( ctx, NULL, 0 )) goto error;
            ctx->composition->dwCursorPos = event->cursor_pos;

            TRACE( "Received preedit %s\n", debugstr_composition_string( ctx->composition ) );
            ime_send_message( ctx, WM_IME_COMPOSITION, 0, GCS_COMP | GCS_CURSORPOS );
            if (!ctx->composition->dwCompStrLen) ime_send_message( ctx, WM_IME_ENDCOMPOSITION, 0, 0 );
        }
        else
        {
            FIXME( "index %#x, cursor_pos %u, comp %s\n", event->index, event->cursor_pos,
                   debugstr_wn( event->buffer, event->buffer_len ) );
        }

    error:
        ime_release_context( ctx );
        free( event );
        return 0;
    }

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
    TRACE( "vkey %u, scan_code %u, key_state %p, list %p, state %u, himc %p\n", vkey,
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
