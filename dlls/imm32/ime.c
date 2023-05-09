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

#include <stdarg.h>
#include <stddef.h>

#include "imm_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(imm);

struct ime_private
{
    BOOL in_composition;
    HFONT hfont;
};

struct ime_ui_private
{
    HWND edit;
};

static const char *debugstr_imn( WPARAM wparam )
{
    switch (wparam)
    {
    case IMN_OPENSTATUSWINDOW: return "IMN_OPENSTATUSWINDOW";
    case IMN_CLOSESTATUSWINDOW: return "IMN_CLOSESTATUSWINDOW";
    case IMN_OPENCANDIDATE: return "IMN_OPENCANDIDATE";
    case IMN_CHANGECANDIDATE: return "IMN_CHANGECANDIDATE";
    case IMN_CLOSECANDIDATE: return "IMN_CLOSECANDIDATE";
    case IMN_SETCONVERSIONMODE: return "IMN_SETCONVERSIONMODE";
    case IMN_SETSENTENCEMODE: return "IMN_SETSENTENCEMODE";
    case IMN_SETOPENSTATUS: return "IMN_SETOPENSTATUS";
    case IMN_SETCANDIDATEPOS: return "IMN_SETCANDIDATEPOS";
    case IMN_SETCOMPOSITIONFONT: return "IMN_SETCOMPOSITIONFONT";
    case IMN_SETCOMPOSITIONWINDOW: return "IMN_SETCOMPOSITIONWINDOW";
    case IMN_GUIDELINE: return "IMN_GUIDELINE";
    case IMN_SETSTATUSWINDOWPOS: return "IMN_SETSTATUSWINDOWPOS";
    case IMN_WINE_SET_COMP_STRING: return "IMN_WINE_SET_COMP_STRING";
    default: return wine_dbg_sprintf( "%#Ix", wparam );
    }
}

static const char *debugstr_imc( WPARAM wparam )
{
    switch (wparam)
    {
    case IMC_GETCANDIDATEPOS: return "IMC_GETCANDIDATEPOS";
    case IMC_SETCANDIDATEPOS: return "IMC_SETCANDIDATEPOS";
    case IMC_GETCOMPOSITIONFONT: return "IMC_GETCOMPOSITIONFONT";
    case IMC_SETCOMPOSITIONFONT: return "IMC_SETCOMPOSITIONFONT";
    case IMC_GETCOMPOSITIONWINDOW: return "IMC_GETCOMPOSITIONWINDOW";
    case IMC_SETCOMPOSITIONWINDOW: return "IMC_SETCOMPOSITIONWINDOW";
    case IMC_GETSTATUSWINDOWPOS: return "IMC_GETSTATUSWINDOWPOS";
    case IMC_SETSTATUSWINDOWPOS: return "IMC_SETSTATUSWINDOWPOS";
    case IMC_CLOSESTATUSWINDOW: return "IMC_CLOSESTATUSWINDOW";
    case IMC_OPENSTATUSWINDOW: return "IMC_OPENSTATUSWINDOW";
    default: return wine_dbg_sprintf( "%#Ix", wparam );
    }
}

static WCHAR *input_context_get_comp_str( INPUTCONTEXT *ctx, BOOL result, UINT *length )
{
    COMPOSITIONSTRING *string;
    WCHAR *text = NULL;
    UINT len, off;

    if (!(string = ImmLockIMCC( ctx->hCompStr ))) return NULL;
    len = result ? string->dwResultStrLen : string->dwCompStrLen;
    off = result ? string->dwResultStrOffset : string->dwCompStrOffset;

    if (len && off && (text = malloc( (len + 1) * sizeof(WCHAR) )))
    {
        memcpy( text, (BYTE *)string + off, len * sizeof(WCHAR) );
        text[len] = 0;
        *length = len;
    }

    ImmUnlockIMCC( ctx->hCompStr );
    return text;
}

static void input_context_set_comp_str( INPUTCONTEXT *ctx, const WCHAR *str, UINT len )
{
    COMPOSITIONSTRING *compstr;
    HIMCC himcc;
    UINT size;
    BYTE *dst;

    TRACE( "ctx %p, str %s\n", ctx, debugstr_wn( str, len ) );

    size = sizeof(*compstr);
    size += len * sizeof(WCHAR); /* GCS_COMPSTR */
    size += len; /* GCS_COMPSTRATTR */
    size += 2 * sizeof(DWORD); /* GCS_COMPSTRCLAUSE */

    if (!(himcc = ImmReSizeIMCC( ctx->hCompStr, size )))
        WARN( "Failed to resize input context composition string\n" );
    else if (!(compstr = ImmLockIMCC( (ctx->hCompStr = himcc) )))
        WARN( "Failed to lock input context composition string\n" );
    else
    {
        memset( compstr, 0, sizeof(*compstr) );
        compstr->dwSize = sizeof(*compstr);

        if (len)
        {
            compstr->dwCursorPos = len;

            compstr->dwCompStrLen = len;
            compstr->dwCompStrOffset = compstr->dwSize;
            dst = (BYTE *)compstr + compstr->dwCompStrOffset;
            memcpy( dst, str, compstr->dwCompStrLen * sizeof(WCHAR) );
            compstr->dwSize += compstr->dwCompStrLen * sizeof(WCHAR);

            compstr->dwCompClauseLen = 2 * sizeof(DWORD);
            compstr->dwCompClauseOffset = compstr->dwSize;
            dst = (BYTE *)compstr + compstr->dwCompClauseOffset;
            *((DWORD *)dst + 0) = 0;
            *((DWORD *)dst + 1) = compstr->dwCompStrLen;
            compstr->dwSize += compstr->dwCompClauseLen;

            compstr->dwCompAttrLen = compstr->dwCompStrLen;
            compstr->dwCompAttrOffset = compstr->dwSize;
            dst = (BYTE *)compstr + compstr->dwCompAttrOffset;
            memset( dst, ATTR_INPUT, compstr->dwCompAttrLen );
            compstr->dwSize += compstr->dwCompAttrLen;
        }

        ImmUnlockIMCC( ctx->hCompStr );
    }
}

static HFONT input_context_select_ui_font( INPUTCONTEXT *ctx, HDC hdc )
{
    struct ime_private *priv;
    HFONT font = NULL;
    if (!(priv = ImmLockIMCC( ctx->hPrivate ))) return NULL;
    if (priv->hfont) font = SelectObject( hdc, priv->hfont );
    ImmUnlockIMCC( ctx->hPrivate );
    return font;
}

static void ime_send_message( HIMC himc, UINT message, WPARAM wparam, LPARAM lparam )
{
    INPUTCONTEXT *ctx;
    TRANSMSG *msgs;
    HIMCC himcc;

    if (!(ctx = ImmLockIMC( himc ))) return;
    if (!(himcc = ImmReSizeIMCC( ctx->hMsgBuf, (ctx->dwNumMsgBuf + 1) * sizeof(*msgs) )))
        WARN( "Failed to resize input context message buffer\n" );
    else if (!(msgs = ImmLockIMCC( (ctx->hMsgBuf = himcc) )))
        WARN( "Failed to lock input context message buffer\n" );
    else
    {
        TRANSMSG msg = {.message = message, .wParam = wparam, .lParam = lparam};
        msgs[ctx->dwNumMsgBuf++] = msg;
        ImmUnlockIMCC( ctx->hMsgBuf );
    }

    ImmUnlockIMC( himc );
    ImmGenerateMessage( himc );
}

static UINT ime_set_composition_status( HIMC himc, BOOL composition )
{
    struct ime_private *priv;
    INPUTCONTEXT *ctx;
    UINT msg = 0;

    if (!(ctx = ImmLockIMC( himc ))) return 0;
    if ((priv = ImmLockIMCC( ctx->hPrivate )))
    {
        if (!priv->in_composition && composition) msg = WM_IME_STARTCOMPOSITION;
        else if (priv->in_composition && !composition) msg = WM_IME_ENDCOMPOSITION;
        priv->in_composition = composition;
        ImmUnlockIMCC( ctx->hPrivate );
    }
    ImmUnlockIMC( himc );

    return msg;
}

static void ime_ui_paint( HIMC himc, HWND hwnd )
{
    PAINTSTRUCT ps;
    RECT rect;
    HDC hdc;
    INPUTCONTEXT *ctx;
    WCHAR *str;
    UINT len;

    if (!(ctx = ImmLockIMC( himc )))
    {
        ValidateRect( hwnd, NULL );
        return;
    }

    hdc = BeginPaint( hwnd, &ps );

    GetClientRect( hwnd, &rect );
    FillRect( hdc, &rect, (HBRUSH)(COLOR_WINDOW + 1) );

    if ((str = input_context_get_comp_str( ctx, FALSE, &len )))
    {
        HFONT font = input_context_select_ui_font( ctx, hdc );
        RECT target, rect = ctx->cfCompForm.rcArea;
        POINT offset = {10, 10};
        UINT flags = 0;

        /*
         * How this works based on tests on windows:
         * CFS_POINT: then we start our window at the point and grow it as large
         *    as it needs to be for the string.
         * CFS_RECT:  we still use the ptCurrentPos as a starting point and our
         *    window is only as large as we need for the string, but we do not
         *    grow such that our window exceeds the given rect.  Wrapping if
         *    needed and possible.   If our ptCurrentPos is outside of our rect
         *    then no window is displayed.
         * CFS_FORCE_POSITION: appears to behave just like CFS_POINT
         *    maybe because the default MSIME does not do any IME adjusting.
         */
        OffsetRect( &rect, -rect.left, -rect.top );
        if (ctx->cfCompForm.dwStyle == CFS_RECT) flags |= DT_WORDBREAK;
        DrawTextW( hdc, str, len, &rect, flags | DT_CALCRECT );

        if (ctx->cfCompForm.dwStyle != CFS_DEFAULT)
        {
            POINT pt = ctx->cfCompForm.ptCurrentPos;
            ClientToScreen( ctx->hWnd, &pt );
            OffsetRect( &rect, pt.x, pt.y );
            offset.x = offset.y = 0;

        }
        else
        {
            /* Windows places the default IME window in the bottom left */
            GetWindowRect( ctx->hWnd, &target );
            InflateRect( &rect, offset.x, offset.y );
            OffsetRect( &rect, target.left - rect.left, target.bottom - rect.top );
        }

        if (ctx->cfCompForm.dwStyle == CFS_DEFAULT)
        {
            /* make sure we are on the desktop */
            HMONITOR monitor = MonitorFromWindow( ctx->hWnd, MONITOR_DEFAULTTOPRIMARY );
            MONITORINFO info = {.cbSize = sizeof(info)};
            GetMonitorInfoW( monitor, &info );
            if (rect.bottom > info.rcWork.bottom) OffsetRect( &rect, 0, info.rcWork.bottom - rect.bottom );
            if (rect.left < 0) OffsetRect( &rect, -rect.left, 0 );
            if (rect.right > info.rcWork.right) OffsetRect( &rect, info.rcWork.right - rect.right, 0 );
        }

        if (font) SelectObject( hdc, font );
        free( str );
    }

    EndPaint( hwnd, &ps );
    ImmUnlockIMC( himc );

    SetWindowPos( hwnd, HWND_TOPMOST, rect.left, rect.top, rect.right - rect.left,
                  rect.bottom - rect.top, SWP_NOACTIVATE );
}

static void ime_set_open_status( INPUTCONTEXT *ctx, BOOL open )
{
    if (ctx->fOpen != open)
    {
        ctx->fOpen = open;
        SendMessageW( ctx->hWnd, WM_IME_NOTIFY, IMN_SETOPENSTATUS, 0 );
    }
}

static void ime_ui_update_window( INPUTCONTEXT *ctx, HWND hwnd )
{
    struct ime_ui_private *ui = (void *)GetWindowLongPtrW( hwnd, IMMGWL_PRIVATE );
    WCHAR *str;
    UINT len;

    if (!(str = input_context_get_comp_str( ctx, FALSE, &len )))
    {
        ShowWindow( hwnd, SW_HIDE );
        ime_set_open_status( ctx, FALSE );
        ctx->hWnd = GetFocus();
    }
    else
    {
        ctx->hWnd = GetFocus();
        SendMessageW( ui->edit, WM_SETTEXT, 0, (LPARAM)str );
        ShowWindow( hwnd, SW_SHOWNOACTIVATE );
        RedrawWindow( hwnd, NULL, NULL, RDW_ERASENOW | RDW_INVALIDATE );
        ime_set_open_status( ctx, TRUE );
    }
    free( str );
}

static void ime_ui_composition( HIMC himc, HWND hwnd, LPARAM lparam )
{
    INPUTCONTEXT *ctx;
    if (lparam & GCS_RESULTSTR) return;
    if (!(ctx = ImmLockIMC( himc ))) return;
    ime_ui_update_window( ctx, hwnd );
    ImmUnlockIMC( himc );
}

static void ime_ui_start_composition( HIMC himc, HWND hwnd )
{
    INPUTCONTEXT *ctx;
    if (!(ctx = ImmLockIMC( himc ))) return;
    ime_ui_update_window( ctx, hwnd );
    ImmUnlockIMC( himc );
}

static UINT ime_set_comp_string( HIMC himc, LPARAM lparam )
{
    union
    {
        struct
        {
            UINT uMsgCount;
            TRANSMSG TransMsg[10];
        };
        TRANSMSGLIST list;
    } buffer = {.uMsgCount = ARRAY_SIZE(buffer.TransMsg)};
    INPUTCONTEXT *ctx;
    TRANSMSG *msgs;
    HIMCC himcc;
    UINT count;

    TRACE( "himc %p\n", himc );

    if (!(ctx = ImmLockIMC( himc ))) return 0;

    count = ImeToAsciiEx( VK_PROCESSKEY, lparam, NULL, &buffer.list, 0, himc );
    if (!count)
        TRACE( "ImeToAsciiEx returned no messages\n" );
    else if (count >= buffer.uMsgCount)
        WARN( "ImeToAsciiEx returned %#x messages\n", count );
    else if (!(himcc = ImmReSizeIMCC( ctx->hMsgBuf, (ctx->dwNumMsgBuf + count) * sizeof(*msgs) )))
        WARN( "Failed to resize input context message buffer\n" );
    else if (!(msgs = ImmLockIMCC( (ctx->hMsgBuf = himcc) )))
        WARN( "Failed to lock input context message buffer\n" );
    else
    {
        memcpy( msgs + ctx->dwNumMsgBuf, buffer.TransMsg, count * sizeof(*msgs) );
        ImmUnlockIMCC( ctx->hMsgBuf );
        ctx->dwNumMsgBuf += count;
    }

    ImmUnlockIMC( himc );
    ImmGenerateMessage( himc );

    return count;
}

static LRESULT ime_ui_notify( HIMC himc, HWND hwnd, WPARAM wparam, LPARAM lparam )
{
    struct ime_ui_private *ui = (void *)GetWindowLongPtrW( hwnd, IMMGWL_PRIVATE );

    TRACE( "himc %p, hwnd %p, wparam %s, lparam %#Ix\n", hwnd, himc, debugstr_imn(wparam), lparam );

    switch (wparam)
    {
    case IMN_CLOSESTATUSWINDOW:
        ShowWindow( hwnd, SW_HIDE );
        return 0;
    case IMN_OPENSTATUSWINDOW:
        ShowWindow( hwnd, SW_SHOWNOACTIVATE );
        return 0;
    case IMN_SETCOMPOSITIONFONT:
    {
        LOGFONTW font;
        HFONT hfont;

        ImmGetCompositionFontW( himc, &font );
        hfont = CreateFontIndirectW( &font );

        SendMessageW( ui->edit, WM_SETFONT, (WPARAM)hfont, FALSE );
        return 0;
    }
    case IMN_SETCOMPOSITIONWINDOW:
    {
        COMPOSITIONFORM form;
        ImmGetCompositionWindow( himc, &form );
        ERR("form %#lx, pt %s, rect %s\n", form.dwStyle, wine_dbgstr_point(&form.ptCurrentPos), wine_dbgstr_rect(&form.rcArea) );
        return 0;
    }
    case IMN_WINE_SET_COMP_STRING:
        return ime_set_comp_string( himc, lparam );
    default:
        return 0;
    }
}

static LRESULT ime_ui_control( HIMC himc, HWND hwnd, WPARAM wparam, LPARAM lparam )
{
    INPUTCONTEXT *ctx;

    TRACE( "himc %p, hwnd %p, wparam %s, lparam %#Ix\n", hwnd, himc, debugstr_imn(wparam), lparam );

    if (!(ctx = ImmLockIMC( himc ))) return 0;

    switch (wparam)
    {
    case IMC_CLOSESTATUSWINDOW:
        if (!IsWindowVisible( hwnd )) return 0;
        return SendMessageW( ctx->hWnd, WM_IME_NOTIFY, IMN_CLOSESTATUSWINDOW, 0 );

    case IMC_OPENSTATUSWINDOW:
        if (IsWindowVisible( hwnd )) return 0;
        return SendMessageW( ctx->hWnd, WM_IME_NOTIFY, IMN_OPENSTATUSWINDOW, 0 );

    case IMC_GETCANDIDATEPOS:
    case IMC_GETCOMPOSITIONFONT:
    case IMC_GETCOMPOSITIONWINDOW:
    case IMC_GETSTATUSWINDOWPOS:
    case IMC_SETCANDIDATEPOS:
    case IMC_SETCOMPOSITIONFONT:
    case IMC_SETCOMPOSITIONWINDOW:
    case IMC_SETSTATUSWINDOWPOS:
    default:
        FIXME( "hwnd %p, himc %p, wparam %s, lparam %#Ix stub!\n", hwnd, himc, debugstr_imc(wparam), lparam );
        break;
    }

    ImmUnlockIMC( himc );

    return 0;
}

static LRESULT WINAPI ime_ui_window_proc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    struct ime_ui_private *ui = (void *)GetWindowLongPtrW( hwnd, IMMGWL_PRIVATE );
    HIMC himc = (HIMC)GetWindowLongPtrW( hwnd, IMMGWL_IMC );

    ERR( "hwnd %p, himc %p, msg %s, wparam %#Ix, lparam %#Ix\n",
           hwnd, himc, debugstr_wm_ime(msg), wparam, lparam );

    switch (msg)
    {
    case WM_CREATE:
        if (!(ui = calloc( 1, sizeof(*ui) ))) return FALSE;
        ui->edit = CreateWindowExW( 0, L"edit", NULL, WS_CHILD | WS_VISIBLE | ES_AUTOVSCROLL | ES_MULTILINE | ES_NOHIDESEL | ES_READONLY,
                                    0, 0, 0, 0, hwnd, 0, imm32_module, NULL );
        SendMessageW( ui->edit, EM_LIMITTEXT, 0, 0 );
        SetWindowLongPtrW( hwnd, IMMGWL_PRIVATE, (LPARAM)ui );
        return TRUE;
    case WM_PAINT:
        ime_ui_paint( himc, hwnd );
        return FALSE;
    case WM_SIZE:
        MoveWindow( ui->edit, 0, 0, LOWORD(lparam), HIWORD(lparam), TRUE );
        return 0;
    case WM_SETFOCUS:
        if (wparam) SetFocus( (HWND)wparam );
        else FIXME( "Received focus, should never have focus\n" );
        break;
    case WM_IME_COMPOSITION:
        ime_ui_composition( himc, hwnd, lparam );
        break;
    case WM_IME_STARTCOMPOSITION:
        ime_ui_start_composition( himc, hwnd );
        break;
    case WM_IME_ENDCOMPOSITION:
        ShowWindow( hwnd, SW_HIDE );
        break;
    case WM_IME_NOTIFY:
        return ime_ui_notify( himc, hwnd, wparam, lparam );
    case WM_IME_CONTROL:
        return ime_ui_control( himc, hwnd, wparam, lparam );
    }

    return DefWindowProcW( hwnd, msg, wparam, lparam );
}

static WNDCLASSEXW ime_ui_class =
{
    .cbSize = sizeof(WNDCLASSEXW),
    .style = CS_GLOBALCLASS | CS_IME | CS_HREDRAW | CS_VREDRAW,
    .lpfnWndProc = ime_ui_window_proc,
    .cbWndExtra = 2 * sizeof(LONG_PTR),
    .lpszClassName = L"Wine IME",
    .hbrBackground = (HBRUSH)(COLOR_WINDOW + 1),
};

BOOL WINAPI ImeInquire( IMEINFO *info, WCHAR *ui_class, DWORD flags )
{
    TRACE( "info %p, ui_class %p, flags %#lx\n", info, ui_class, flags );

    ime_ui_class.hInstance = imm32_module;
    ime_ui_class.hCursor = LoadCursorW( NULL, (LPWSTR)IDC_ARROW );
    ime_ui_class.hIcon = LoadIconW( NULL, (LPWSTR)IDI_APPLICATION );
    RegisterClassExW( &ime_ui_class );

    wcscpy( ui_class, ime_ui_class.lpszClassName );
    memset( info, 0, sizeof(*info) );
    info->dwPrivateDataSize = sizeof(struct ime_private);
    info->fdwProperty = IME_PROP_UNICODE | IME_PROP_AT_CARET;
    info->fdwConversionCaps = IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE;
    info->fdwSentenceCaps = IME_SMODE_AUTOMATIC;
    info->fdwUICaps = UI_CAP_2700;
    /* Tell App we cannot accept ImeSetCompositionString calls */
    info->fdwSCSCaps = 0;
    info->fdwSelectCaps = SELECT_CAP_CONVERSION;

    return TRUE;
}

BOOL WINAPI ImeDestroy( UINT force )
{
    TRACE( "force %u\n", force );
    UnregisterClassW( ime_ui_class.lpszClassName, imm32_module );
    return TRUE;
}

BOOL WINAPI ImeSelect( HIMC himc, BOOL select )
{
    struct ime_private *priv;
    INPUTCONTEXT *ctx;

    TRACE( "himc %p, select %u\n", himc, select );

    if (!himc || !select) return TRUE;
    if (!(ctx = ImmLockIMC( himc ))) return FALSE;

    ImmSetOpenStatus( himc, FALSE );

    if ((priv = ImmLockIMCC( ctx->hPrivate )))
    {
        memset( priv, 0, sizeof(*priv) );
        ImmUnlockIMCC( ctx->hPrivate );
    }

    ImmUnlockIMC( himc );
    return TRUE;
}

BOOL WINAPI ImeSetActiveContext( HIMC himc, BOOL flag )
{
    TRACE( "himc %p, flag %#x stub!\n", himc, flag );
    return TRUE;
}

BOOL WINAPI ImeProcessKey( HIMC himc, UINT vkey, LPARAM lparam, BYTE *state )
{
    struct ime_driver_call_params params = {.himc = himc, .state = state};
    INPUTCONTEXT *ctx;
    LRESULT ret;

    TRACE( "himc %p, vkey %#x, lparam %#Ix, state %p\n", himc, vkey, lparam, state );

    if (!(ctx = ImmLockIMC( himc ))) return FALSE;
    ret = NtUserMessageCall( ctx->hWnd, WINE_IME_PROCESS_KEY, vkey, lparam, &params,
                             NtUserImeDriverCall, FALSE );
    ImmUnlockIMC( himc );

    return ret;
}

UINT WINAPI ImeToAsciiEx( UINT vkey, UINT vsc, BYTE *state, TRANSMSGLIST *msgs, UINT flags, HIMC himc )
{
    COMPOSITIONSTRING *compstr;
    UINT size, count = 0;
    INPUTCONTEXT *ctx;
    NTSTATUS status;

    TRACE( "vkey %#x, vsc %#x, state %p, msgs %p, flags %#x, himc %p\n",
           vkey, vsc, state, msgs, flags, himc );

    if (!(ctx = ImmLockIMC( himc ))) return 0;
    if (!(compstr = ImmLockIMCC( ctx->hCompStr ))) goto done;
    size = max( compstr->dwSize, sizeof(COMPOSITIONSTRING) );

    do
    {
        struct ime_driver_call_params params = {.himc = himc, .state = state};
        HIMCC himcc;

        ImmUnlockIMCC( ctx->hCompStr );
        if (!(himcc = ImmReSizeIMCC( ctx->hCompStr, size ))) goto done;
        if (!(compstr = ImmLockIMCC( (ctx->hCompStr = himcc) ))) goto done;

        params.compstr = compstr;
        status = NtUserMessageCall( ctx->hWnd, WINE_IME_TO_ASCII_EX, vkey, vsc, &params,
                                    NtUserImeDriverCall, FALSE );
        size = compstr->dwSize;
    } while (status == STATUS_BUFFER_TOO_SMALL);

    if (status) WARN( "WINE_IME_TO_ASCII_EX returned status %#lx\n", status );
    else
    {
        TRANSMSG status_msg = {0};

        if (compstr->dwResultStrOffset)
        {
            const WCHAR *result = (WCHAR *)((BYTE *)compstr + compstr->dwResultStrOffset);
            TRANSMSG msg = {.message = WM_IME_COMPOSITION, .wParam = result[0], .lParam = GCS_RESULTSTR};

            if ((status_msg.message = ime_set_composition_status( himc, FALSE )))
                msgs->TransMsg[count++] = status_msg;

            if (compstr->dwResultClauseOffset) msg.lParam |= GCS_RESULTCLAUSE;
            msgs->TransMsg[count++] = msg;
        }

        if ((status_msg.message = ime_set_composition_status( himc, !!compstr->dwCompStrOffset )))
            msgs->TransMsg[count++] = status_msg;

        if (compstr->dwCompStrOffset)
        {
            const WCHAR *comp = (WCHAR *)((BYTE *)compstr + compstr->dwCompStrOffset);
            TRANSMSG msg = {.message = WM_IME_COMPOSITION, .wParam = comp[0], .lParam = GCS_COMPSTR | GCS_CURSORPOS | GCS_DELTASTART};
            if (compstr->dwCompAttrOffset) msg.lParam |= GCS_COMPATTR;
            if (compstr->dwCompClauseOffset) msg.lParam |= GCS_COMPCLAUSE;
            else msg.lParam |= CS_INSERTCHAR|CS_NOMOVECARET;
            msgs->TransMsg[count++] = msg;
        }
    }

    ImmUnlockIMCC( ctx->hCompStr );

done:
    if (count >= msgs->uMsgCount) FIXME( "More than %u messages queued, messages possibly lost\n", msgs->uMsgCount );
    else TRACE( "Returning %u messages queued\n", count );
    ImmUnlockIMC( himc );
    return count;
}

BOOL WINAPI ImeConfigure( HKL hkl, HWND hwnd, DWORD mode, void *data )
{
    FIXME( "hkl %p, hwnd %p, mode %lu, data %p stub!\n", hkl, hwnd, mode, data );
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return FALSE;
}

DWORD WINAPI ImeConversionList( HIMC himc, const WCHAR *source, CANDIDATELIST *dest, DWORD dest_len, UINT flag )
{
    FIXME( "himc %p, source %s, dest %p, dest_len %lu, flag %#x stub!\n",
           himc, debugstr_w(source), dest, dest_len, flag );
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return 0;
}

BOOL WINAPI ImeSetCompositionString( HIMC himc, DWORD index, const void *comp, DWORD comp_len,
                                     const void *read, DWORD read_len )
{
    INPUTCONTEXT *ctx;

    FIXME( "himc %p, index %lu, comp %p, comp_len %lu, read %p, read_len %lu semi-stub!\n",
            himc, index, comp, comp_len, read, read_len );
    if (read && read_len) FIXME( "Read string unimplemented\n" );
    if (index != SCS_SETSTR && index != SCS_CHANGECLAUSE && index != SCS_CHANGEATTR) return FALSE;

    if (!(ctx = ImmLockIMC( himc ))) return FALSE;

    if (index != SCS_SETSTR)
        FIXME( "index %#lx not implemented\n", index );
    else
    {
        UINT msg, flags = GCS_COMPSTR | GCS_COMPCLAUSE | GCS_COMPATTR | GCS_DELTASTART | GCS_CURSORPOS;
        WCHAR wparam = comp && comp_len >= sizeof(WCHAR) ? *(WCHAR *)comp : 0;
        input_context_set_comp_str( ctx, comp, comp_len / sizeof(WCHAR) );
        if ((msg = ime_set_composition_status( himc, TRUE ))) ime_send_message( himc, msg, 0, 0 );
        ime_send_message( himc, WM_IME_COMPOSITION, wparam, flags );
    }

    ImmUnlockIMC( himc );

    return TRUE;
}

BOOL WINAPI NotifyIME( HIMC himc, DWORD action, DWORD index, DWORD value )
{
    struct ime_private *priv;
    INPUTCONTEXT *ctx;
    UINT msg;

    TRACE( "himc %p, action %#lx, index %#lx, value %#lx stub!\n", himc, action, index, value );

    if (!(ctx = ImmLockIMC( himc ))) return FALSE;

    switch (action)
    {
    case NI_CONTEXTUPDATED:
        switch (value)
        {
        case IMC_SETCOMPOSITIONFONT:
            if ((priv = ImmLockIMCC( ctx->hPrivate )))
            {
                if (priv->hfont) DeleteObject( priv->hfont );
                priv->hfont = CreateFontIndirectW( &ctx->lfFont.W );
                ImmUnlockIMCC( ctx->hPrivate );
            }
            break;
        case IMC_SETOPENSTATUS:
            if (!ctx->fOpen) ImmNotifyIME( himc, NI_COMPOSITIONSTR, CPS_COMPLETE, 0 );
            NtUserNotifyIMEStatus( ctx->hWnd, ctx->fOpen );
            break;
        }
        break;

    case NI_COMPOSITIONSTR:
        switch (index)
        {
        case CPS_COMPLETE:
        {
            COMPOSITIONSTRING *compstr;

            if (!(compstr = ImmLockIMCC( ctx->hCompStr )))
                WARN( "Failed to lock input context composition string\n" );
            else
            {
                WCHAR wchr = *(WCHAR *)((BYTE *)compstr + compstr->dwCompStrOffset);
                COMPOSITIONSTRING tmp = *compstr;
                UINT flags = 0;

                memset( compstr, 0, sizeof(*compstr) );
                compstr->dwSize = tmp.dwSize;
                compstr->dwResultStrLen = tmp.dwCompStrLen;
                compstr->dwResultStrOffset = tmp.dwCompStrOffset;
                compstr->dwResultClauseLen = tmp.dwCompClauseLen;
                compstr->dwResultClauseOffset = tmp.dwCompClauseOffset;
                ImmUnlockIMCC( ctx->hCompStr );

                if (tmp.dwCompStrLen) flags |= GCS_RESULTSTR;
                if (tmp.dwCompClauseLen) flags |= GCS_RESULTCLAUSE;
                if (flags) ime_send_message( himc, WM_IME_COMPOSITION, wchr, flags );
            }

            /* fallthrough */
        }
        case CPS_CANCEL:
            input_context_set_comp_str( ctx, NULL, 0 );
            if ((msg = ime_set_composition_status( himc, FALSE )))
                ime_send_message( himc, msg, 0, 0 );
            break;
        default:
            FIXME( "himc %p, action %#lx, index %#lx, value %#lx stub!\n", himc, action, index, value );
            break;
        }
        break;

    default:
        FIXME( "himc %p, action %#lx, index %#lx, value %#lx stub!\n", himc, action, index, value );
        break;
    }

    ImmUnlockIMC( himc );
    return TRUE;
}

LRESULT WINAPI ImeEscape( HIMC himc, UINT escape, void *data )
{
    FIXME( "himc %p, escape %#x, data %p stub!\n", himc, escape, data );
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return 0;
}

DWORD WINAPI ImeGetImeMenuItems( HIMC himc, DWORD flags, DWORD type, IMEMENUITEMINFOW *parent,
                                 IMEMENUITEMINFOW *menu, DWORD size )
{
    FIXME( "himc %p, flags %#lx, type %lu, parent %p, menu %p, size %#lx stub!\n",
           himc, flags, type, parent, menu, size );
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return 0;
}

BOOL WINAPI ImeRegisterWord( const WCHAR *reading, DWORD style, const WCHAR *string )
{
    FIXME( "reading %s, style %lu, string %s stub!\n", debugstr_w(reading), style, debugstr_w(string) );
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return FALSE;
}

UINT WINAPI ImeGetRegisterWordStyle( UINT item, STYLEBUFW *style )
{
    FIXME( "item %u, style %p stub!\n", item, style );
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return 0;
}

BOOL WINAPI ImeUnregisterWord( const WCHAR *reading, DWORD style, const WCHAR *string )
{
    FIXME( "reading %s, style %lu, string %s stub!\n", debugstr_w(reading), style, debugstr_w(string) );
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return FALSE;
}

UINT WINAPI ImeEnumRegisterWord( REGISTERWORDENUMPROCW proc, const WCHAR *reading, DWORD style,
                                 const WCHAR *string, void *data )
{
    FIXME( "proc %p, reading %s, style %lu, string %s, data %p stub!\n",
           proc, debugstr_w(reading), style, debugstr_w(string), data );
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return 0;
}
