/*
 * Input Context implementation
 *
 * Copyright 1998 Patrik Stridvall
 * Copyright 2002, 2003, 2007 CodeWeavers, Aric Stewart
 * Copyright 2022 Jacek Caban for CodeWeavers
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

#include <pthread.h>
#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "win32u_private.h"
#include "ntuser_private.h"
#include "immdev.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(imm);

struct ime_funcs
{
    BOOL (*p_inquire)(void);
    BOOL (*p_destroy)(void);
    BOOL (*p_context_create)( HIMC himc );
    BOOL (*p_context_delete)( HIMC himc );
    BOOL (*p_context_activate)( HIMC himc, BOOL active );
    BOOL (*p_process_key)( HIMC himc, UINT wparam, UINT lparam, const BYTE *state );
};

struct imc
{
    struct user_object obj;
    DWORD    thread_id;
    UINT_PTR client_ptr;
};

struct imm_thread_data
{
    struct list entry;
    DWORD thread_id;
    HWND  default_hwnd;
    BOOL  disable_ime;
    UINT  window_cnt;
};

static struct list thread_data_list = LIST_INIT( thread_data_list );
static pthread_mutex_t imm_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct ime_funcs *ime_funcs;
static BOOL disable_ime;

static struct imc *get_imc_ptr( HIMC handle )
{
    struct imc *imc = get_user_handle_ptr( handle, NTUSER_OBJ_IMC );
    if (imc && imc != OBJ_OTHER_PROCESS) return imc;
    WARN( "invalid handle %p\n", handle );
    RtlSetLastWin32Error( ERROR_INVALID_HANDLE );
    return NULL;
}

static void release_imc_ptr( struct imc *imc )
{
    release_user_handle_ptr( imc );
}

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

static pthread_mutex_t ibus_lock = PTHREAD_MUTEX_INITIALIZER;
static GHashTable *ibus_contexts;
static IBusBus *ibus_bus;

static POINT virtual_screen_to_root( INT x, INT y )
{
    RECT virtual = NtUserGetVirtualScreenRect();
    POINT pt;
    pt.x = x - virtual.left;
    pt.y = y - virtual.top;
    return pt;
}

/* sends a message to the IME UI window */
static LRESULT send_ime_ui_message( UINT msg, WPARAM wparam, LPARAM lparam )
{
    static const WCHAR WINE_IME_UI_CLASS[] = {'W','i','n','e',' ','I','M','E',0};
    WCHAR buffer[64];
    UNICODE_STRING name = {.Buffer = buffer, .MaximumLength = sizeof(buffer)};
    GUITHREADINFO info = {.cbSize = sizeof(GUITHREADINFO)};
    HWND hwnd;
    HIMC himc;

    TRACE( "msg %#x, wparam %#zx, lparam %#zx\n", msg, (size_t)wparam, (size_t)lparam );

    if (!NtUserGetGUIThreadInfo( 0, &info )) return 0;
    hwnd = info.hwndFocus;
    if (!(himc = NtUserGetWindowInputContext( hwnd ))) return 0;
    if (!(hwnd = (HWND)NtUserQueryInputContext( himc, NtUserInputContextUIHwnd ))) return 0;
    if (!NtUserGetClassName( hwnd, 0, &name ) || wcscmp( buffer, WINE_IME_UI_CLASS )) return 0;
    return NtUserMessageCall( hwnd, msg, wparam, lparam, NULL, NtUserSendDriverMessage, FALSE );
}

static void ibus_ctx_commit_text( IBusInputContext *ctx, IBusText *text, void *user )
{
    const gchar *str = ibus_text_get_text( text );
    UINT len = strlen( str );
    WCHAR *tmp;

    TRACE( "text %s\n", debugstr_a( str ) );

    send_ime_ui_message( WM_WINE_IME_SET_OPEN_STATUS, TRUE, 0 );
    send_ime_ui_message( WM_WINE_IME_SET_COMP_STATUS, TRUE, 0 );

    if (!(tmp = malloc( (len + 1) * sizeof(WCHAR) ))) return;
    ntdll_umbstowcs( str, len + 1, tmp, len + 1 );
    send_ime_ui_message( WM_WINE_IME_SET_COMP_TEXT, FALSE, (LPARAM)tmp );
    free( tmp );
}

static void ibus_ctx_update_preedit_text( IBusInputContext *ctx, IBusText *text,
                                          guint cursor_pos, gboolean arg2, gpointer user )
{
    const gchar *str = ibus_text_get_text( text );
    UINT len = strlen( str );
    WCHAR *tmp;

    TRACE( "text %s cursor_pos %u arg2 %u\n", debugstr_a( str ), cursor_pos, arg2 );

    send_ime_ui_message( WM_WINE_IME_SET_OPEN_STATUS, TRUE, 0 );
    send_ime_ui_message( WM_WINE_IME_SET_COMP_STATUS, TRUE, 0 );

    if (!(tmp = malloc( (len + 1) * sizeof(WCHAR) ))) return;
    ntdll_umbstowcs( str, len + 1, tmp, len + 1 );
    send_ime_ui_message( WM_WINE_IME_SET_COMP_TEXT, TRUE, (LPARAM)tmp );
    free( tmp );

    send_ime_ui_message( WM_WINE_IME_SET_CURSOR_POS, cursor_pos, 0 );
}

static void ibus_ctx_hide_preedit_text( IBusInputContext *ctx, void *user )
{
    TRACE( "\n" );
    send_ime_ui_message( WM_WINE_IME_SET_OPEN_STATUS, TRUE, 0 );
    send_ime_ui_message( WM_WINE_IME_SET_COMP_STATUS, TRUE, 0 );
    send_ime_ui_message( WM_WINE_IME_SET_COMP_TEXT, FALSE, (LPARAM)L"" );
}

static BOOL ime_inquire_ibus(void)
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
    if (!(ibus_contexts = g_hash_table_new( NULL, NULL )))
    {
        ERR( "Failed to create ibus context table\n" );
        goto error;
    }

    TRACE( "created IBusBus %p\n", ibus_bus );
    return TRUE;

error:
    if (ibus_bus) g_object_unref( ibus_bus );
    ibus_bus = NULL;
    return FALSE;
}

static BOOL ime_destroy_ibus(void)
{
    TRACE( "\n" );

    if (ibus_bus) g_object_unref( ibus_bus );
    ibus_bus = NULL;

    ibus_quit();

    return TRUE;
}

static BOOL ime_context_create_ibus( HIMC himc )
{
    IBusInputContext *ctx;

    TRACE( "himc %p\n", himc );

    if (!(ctx = ibus_bus_create_input_context( ibus_bus, "WineIME" )))
    {
        ERR( "Failed to create IBus input context.\n" );
        return FALSE;
    }

    g_signal_connect( ctx, "commit-text", G_CALLBACK( ibus_ctx_commit_text ), ctx );
    g_signal_connect( ctx, "update-preedit-text", G_CALLBACK( ibus_ctx_update_preedit_text ), ctx );
    g_signal_connect( ctx, "hide-preedit-text", G_CALLBACK( ibus_ctx_hide_preedit_text ), ctx );

    ibus_input_context_set_content_type( ctx, IBUS_INPUT_PURPOSE_FREE_FORM, IBUS_INPUT_HINT_NONE );
    ibus_input_context_set_capabilities( ctx, IBUS_CAP_FOCUS | IBUS_CAP_PREEDIT_TEXT | IBUS_CAP_SYNC_PROCESS_KEY );

    TRACE( "created IBusInputContext %p\n", ctx );

    pthread_mutex_lock( &ibus_lock );
    if (!g_hash_table_insert( ibus_contexts, himc, ctx )) WARN( "Failed to insert context %p\n", ctx );
    pthread_mutex_unlock( &ibus_lock );

    return TRUE;
}

static BOOL ime_context_delete_ibus( HIMC himc )
{
    IBusInputContext *ctx;

    TRACE( "himc %p\n", himc );

    pthread_mutex_lock( &ibus_lock );
    if ((ctx = g_hash_table_lookup( ibus_contexts, himc ))) g_object_unref( ctx );
    pthread_mutex_unlock( &ibus_lock );

    return TRUE;
}

static BOOL ime_context_activate_ibus( HIMC himc, BOOL active )
{
    IBusInputContext *ctx;

    TRACE( "himc %p, active %u\n", himc, active );

    pthread_mutex_lock( &ibus_lock );
    if ((ctx = g_hash_table_lookup( ibus_contexts, himc ))) g_object_ref( ctx );
    pthread_mutex_unlock( &ibus_lock );

    if (ctx)
    {
        if (active) ibus_input_context_focus_in( ctx );
        else ibus_input_context_focus_out( ctx );
        g_object_unref( ctx );
    }

    return TRUE;
}

static BOOL ime_process_key_ibus( HIMC himc, UINT wparam, UINT lparam, const BYTE *state )
{
    GUITHREADINFO info = {.cbSize = sizeof(GUITHREADINFO)};
    WORD scan = HIWORD(lparam) & 0x1ff, vkey = LOWORD(wparam);
    BOOL press = !(lparam >> 31);
    IBusInputContext *ctx;
    guint32 keyval = 0;
    WCHAR wchr[2];
    RECT root;

    TRACE( "himc %p, wparam %#x, lparam %#x, state %p\n", himc, wparam, lparam, state );

    pthread_mutex_lock( &ibus_lock );
    if ((ctx = g_hash_table_lookup( ibus_contexts, himc ))) g_object_ref( ctx );
    pthread_mutex_unlock( &ibus_lock );

    if (!ctx) return FALSE;
    if (!press) return FALSE;

    if (!NtUserGetGUIThreadInfo( 0, &info )) memset( &info.rcCaret, 0, sizeof(info.rcCaret) );
    else NtUserMapWindowPoints( info.hwndCaret, HWND_DESKTOP, (POINT *)&info.rcCaret, 2 );

    NtUserToUnicodeEx( vkey, scan, state, wchr, ARRAY_SIZE(wchr), 0, NtUserGetKeyboardLayout( 0 ) );

    if (vkey <= ARRAY_SIZE(vk2ibus)) keyval = vk2ibus[vkey];
    if (!keyval) keyval = ibus_unicode_to_keyval( wchr[0] );

    *(POINT *)&root.left = virtual_screen_to_root( info.rcCaret.left, info.rcCaret.top );
    *(POINT *)&root.right = virtual_screen_to_root( info.rcCaret.right, info.rcCaret.bottom );

    ibus_input_context_set_cursor_location( ctx, root.left, root.top, root.right - root.left, root.bottom - root.top );
    return ibus_input_context_process_key_event( ctx, keyval, scan, 0 );
}

struct ime_funcs ime_funcs_ibus =
{
    .p_inquire = ime_inquire_ibus,
    .p_destroy = ime_destroy_ibus,
    .p_context_create = ime_context_create_ibus,
    .p_context_delete = ime_context_delete_ibus,
    .p_context_activate = ime_context_activate_ibus,
    .p_process_key = ime_process_key_ibus,
};

void ime_thread_ibus(void)
{
    ibus_main();
}

#else /* SONAME_LIBIBUS_1_0 */

struct ime_funcs ime_funcs_ibus = {0};

void ime_thread_ibus(void)
{
}

#endif /* SONAME_LIBIBUS_1_0 */

/******************************************************************************
 *           NtUserCreateInputContext   (win32u.@)
 */
HIMC WINAPI NtUserCreateInputContext( UINT_PTR client_ptr )
{
    struct imc *imc;
    HIMC handle;

    if (!(imc = malloc( sizeof(*imc) ))) return 0;
    imc->client_ptr = client_ptr;
    imc->thread_id = GetCurrentThreadId();
    if (!(handle = alloc_user_handle( &imc->obj, NTUSER_OBJ_IMC )))
    {
        free( imc );
        return 0;
    }

    TRACE( "%lx returning %p\n", (long)client_ptr, handle );
    return handle;
}

/******************************************************************************
 *           NtUserDestroyInputContext   (win32u.@)
 */
BOOL WINAPI NtUserDestroyInputContext( HIMC handle )
{
    struct imc *imc;

    TRACE( "%p\n", handle );

    if (!(imc = free_user_handle( handle, NTUSER_OBJ_IMC ))) return FALSE;
    if (imc == OBJ_OTHER_PROCESS)
    {
        FIXME( "other process handle %p\n", handle );
        return FALSE;
    }
    free( imc );
    return TRUE;
}

/******************************************************************************
 *           NtUserUpdateInputContext   (win32u.@)
 */
BOOL WINAPI NtUserUpdateInputContext( HIMC handle, UINT attr, UINT_PTR value )
{
    struct imc *imc;
    BOOL ret = TRUE;

    TRACE( "%p %u %lx\n", handle, attr, (long)value );

    if (!(imc = get_imc_ptr( handle ))) return FALSE;

    switch (attr)
    {
    case NtUserInputContextClientPtr:
        imc->client_ptr = value;
        break;

    default:
        FIXME( "unknown attr %u\n", attr );
        ret = FALSE;
    };

    release_imc_ptr( imc );
    return ret;
}

/******************************************************************************
 *           NtUserQueryInputContext   (win32u.@)
 */
UINT_PTR WINAPI NtUserQueryInputContext( HIMC handle, UINT attr )
{
    struct imc *imc;
    UINT_PTR ret;

    if (!(imc = get_imc_ptr( handle ))) return FALSE;

    switch (attr)
    {
    case NtUserInputContextClientPtr:
        ret = imc->client_ptr;
        break;

    case NtUserInputContextThreadId:
        ret = imc->thread_id;
        break;

    default:
        FIXME( "unknown attr %u\n", attr );
        ret = 0;
    };

    release_imc_ptr( imc );
    return ret;
}

/******************************************************************************
 *           NtUserAssociateInputContext   (win32u.@)
 */
UINT WINAPI NtUserAssociateInputContext( HWND hwnd, HIMC ctx, ULONG flags )
{
    WND *win;
    UINT ret = AICR_OK;

    TRACE( "%p %p %x\n", hwnd, ctx, (int)flags );

    switch (flags)
    {
    case 0:
    case IACE_IGNORENOCONTEXT:
    case IACE_DEFAULT:
        break;

    default:
        FIXME( "unknown flags 0x%x\n", (int)flags );
        return AICR_FAILED;
    }

    if (flags == IACE_DEFAULT)
    {
        if (!(ctx = get_default_input_context())) return AICR_FAILED;
    }
    else if (ctx)
    {
        if (NtUserQueryInputContext( ctx, NtUserInputContextThreadId ) != GetCurrentThreadId())
            return AICR_FAILED;
    }

    if (!(win = get_win_ptr( hwnd )) || win == WND_OTHER_PROCESS || win == WND_DESKTOP)
        return AICR_FAILED;

    if (ctx && win->tid != GetCurrentThreadId()) ret = AICR_FAILED;
    else if (flags != IACE_IGNORENOCONTEXT || win->imc)
    {
        if (win->imc != ctx && get_focus() == hwnd) ret = AICR_FOCUS_CHANGED;
        win->imc = ctx;
    }

    release_win_ptr( win );
    return ret;
}

HIMC get_default_input_context(void)
{
    struct ntuser_thread_info *thread_info = NtUserGetThreadInfo();
    if (!thread_info->default_imc)
        thread_info->default_imc = HandleToUlong( NtUserCreateInputContext( 0 ));
    return UlongToHandle( thread_info->default_imc );
}

HIMC get_window_input_context( HWND hwnd )
{
    WND *win;
    HIMC ret;

    if (!(win = get_win_ptr( hwnd )) || win == WND_OTHER_PROCESS || win == WND_DESKTOP)
    {
        RtlSetLastWin32Error( ERROR_INVALID_WINDOW_HANDLE );
        return 0;
    }

    ret = win->imc;
    release_win_ptr( win );
    return ret;
}

static HWND detach_default_window( struct imm_thread_data *thread_data )
{
    HWND hwnd = thread_data->default_hwnd;
    thread_data->default_hwnd = NULL;
    thread_data->window_cnt = 0;
    return hwnd;
}

static struct imm_thread_data *get_imm_thread_data(void)
{
    struct user_thread_info *thread_info = get_user_thread_info();
    if (!thread_info->imm_thread_data)
    {
        struct imm_thread_data *data;
        if (!(data = calloc( 1, sizeof( *data )))) return NULL;
        data->thread_id = GetCurrentThreadId();

        pthread_mutex_lock( &imm_mutex );
        list_add_tail( &thread_data_list, &data->entry );
        pthread_mutex_unlock( &imm_mutex );

        thread_info->imm_thread_data = data;
    }
    return thread_info->imm_thread_data;
}

BOOL register_imm_window( HWND hwnd )
{
    struct imm_thread_data *thread_data;

    TRACE( "(%p)\n", hwnd );

    if (disable_ime || !needs_ime_window( hwnd ))
        return FALSE;

    thread_data = get_imm_thread_data();
    if (!thread_data || thread_data->disable_ime)
        return FALSE;

    TRACE( "window_cnt=%u, default_hwnd=%p\n", thread_data->window_cnt + 1, thread_data->default_hwnd );

    /* Create default IME window */
    if (!thread_data->window_cnt++)
    {
        static const WCHAR imeW[] = {'I','M','E',0};
        static const WCHAR default_imeW[] = {'D','e','f','a','u','l','t',' ','I','M','E',0};
        UNICODE_STRING class_name = RTL_CONSTANT_STRING( imeW );
        UNICODE_STRING name = RTL_CONSTANT_STRING( default_imeW );

        thread_data->default_hwnd = NtUserCreateWindowEx( 0, &class_name, &class_name, &name,
                                                          WS_POPUP | WS_DISABLED | WS_CLIPSIBLINGS,
                                                          0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, FALSE );
    }

    return TRUE;
}

void unregister_imm_window( HWND hwnd )
{
    struct imm_thread_data *thread_data = get_user_thread_info()->imm_thread_data;

    if (!thread_data) return;
    if (thread_data->default_hwnd == hwnd)
    {
        detach_default_window( thread_data );
        return;
    }

    if (!(win_set_flags( hwnd, 0, WIN_HAS_IME_WIN ) & WIN_HAS_IME_WIN)) return;

    /* destroy default IME window */
    TRACE( "unregister IME window for %p\n", hwnd );
    if (!--thread_data->window_cnt)
    {
        HWND destroy_hwnd = detach_default_window( thread_data );
        if (destroy_hwnd) NtUserDestroyWindow( destroy_hwnd );
    }
}

/***********************************************************************
 *           NtUserDisableThreadIme (win32u.@)
 */
BOOL WINAPI NtUserDisableThreadIme( DWORD thread_id )
{
    struct imm_thread_data *thread_data;

    if (thread_id == -1)
    {
        disable_ime = TRUE;

        pthread_mutex_lock( &imm_mutex );
        LIST_FOR_EACH_ENTRY( thread_data, &thread_data_list, struct imm_thread_data, entry )
        {
            if (thread_data->thread_id == GetCurrentThreadId()) continue;
            if (!thread_data->default_hwnd) continue;
            NtUserMessageCall( thread_data->default_hwnd, WM_WINE_DESTROYWINDOW, 0, 0,
                               0, NtUserSendNotifyMessage, FALSE );
        }
        pthread_mutex_unlock( &imm_mutex );
    }
    else if (!thread_id || thread_id == GetCurrentThreadId())
    {
        if (!(thread_data = get_imm_thread_data())) return FALSE;
        thread_data->disable_ime = TRUE;
    }
    else return FALSE;

    if ((thread_data = get_user_thread_info()->imm_thread_data))
    {
        HWND destroy_hwnd = detach_default_window( thread_data );
        NtUserDestroyWindow( destroy_hwnd );
    }
    return TRUE;
}

HWND get_default_ime_window( HWND hwnd )
{
    struct imm_thread_data *thread_data;
    HWND ret = 0;

    if (hwnd)
    {
        DWORD thread_id;

        if (!(thread_id = get_window_thread( hwnd, NULL ))) return 0;

        pthread_mutex_lock( &imm_mutex );
        LIST_FOR_EACH_ENTRY( thread_data, &thread_data_list, struct imm_thread_data, entry )
        {
            if (thread_data->thread_id != thread_id) continue;
            ret = thread_data->default_hwnd;
            break;
        }
        pthread_mutex_unlock( &imm_mutex );
    }
    else if ((thread_data = get_user_thread_info()->imm_thread_data))
    {
        ret = thread_data->default_hwnd;
    }

    TRACE( "default for %p is %p\n", hwnd, ret );
    return ret;
}

void cleanup_imm_thread(void)
{
    struct user_thread_info *thread_info = get_user_thread_info();

    if (thread_info->imm_thread_data)
    {
        pthread_mutex_lock( &imm_mutex );
        list_remove( &thread_info->imm_thread_data->entry );
        pthread_mutex_unlock( &imm_mutex );
        free( thread_info->imm_thread_data );
        thread_info->imm_thread_data = NULL;
    }

    NtUserDestroyInputContext( UlongToHandle( thread_info->client_info.default_imc ));
}

/*****************************************************************************
 *           NtUserBuildHimcList (win32u.@)
 */
NTSTATUS WINAPI NtUserBuildHimcList( UINT thread_id, UINT count, HIMC *buffer, UINT *size )
{
    HANDLE handle = 0;
    struct imc *imc;

    TRACE( "thread_id %#x, count %u, buffer %p, size %p\n", thread_id, count, buffer, size );

    if (!buffer) return STATUS_UNSUCCESSFUL;
    if (!thread_id) thread_id = GetCurrentThreadId();

    *size = 0;
    user_lock();
    while (count && (imc = next_process_user_handle_ptr( &handle, NTUSER_OBJ_IMC )))
    {
        if (thread_id != -1 && imc->thread_id != thread_id) continue;
        buffer[(*size)++] = handle;
        count--;
    }
    user_unlock();

    return STATUS_SUCCESS;
}

LRESULT ime_driver_call( HWND hwnd, enum wine_ime_call call, WPARAM wparam, LPARAM lparam,
                         struct ime_driver_call_params *params )
{
    switch (call)
    {
    case WINE_IME_INQUIRE:
    {
        BOOL *needs_thread = (BOOL *)lparam;

        if (ime_funcs_ibus.p_inquire && ime_funcs_ibus.p_inquire())
        {
            TRACE( "using IBus IME backend\n" );
            ime_funcs = &ime_funcs_ibus;
            *needs_thread = TRUE;
        }
        else
        {
            TRACE( "using user driver IME backend\n" );
            *needs_thread = FALSE;
        }

        return TRUE;
    }

    case WINE_IME_DESTROY:
        if (ime_funcs) ime_funcs->p_destroy();
        ime_funcs = NULL;
        return 0;

    case WINE_IME_THREAD:
        if (ime_funcs == &ime_funcs_ibus) ime_thread_ibus();
        return 0;

    case WINE_IME_CONTEXT_CREATE:
        if (!ime_funcs || !ime_funcs->p_context_create) return TRUE;
        return ime_funcs->p_context_create( (HIMC)wparam );

    case WINE_IME_CONTEXT_DELETE:
        if (!ime_funcs || !ime_funcs->p_context_delete) return TRUE;
        return ime_funcs->p_context_delete( (HIMC)wparam );

    case WINE_IME_CONTEXT_ACTIVATE:
        if (!ime_funcs || !ime_funcs->p_context_activate) return TRUE;
        return ime_funcs->p_context_activate( (HIMC)wparam, lparam );

    case WINE_IME_PROCESS_KEY:
        if (!ime_funcs || !ime_funcs->p_process_key) return user_driver->pImeProcessKey( params->himc, wparam, lparam, params->state );
        return ime_funcs->p_process_key( params->himc, wparam, lparam, params->state );
    case WINE_IME_TO_ASCII_EX:
        return user_driver->pImeToAsciiEx( wparam, lparam, params->state, params->compstr, params->himc );
    default:
        ERR( "Unknown IME driver call %#x\n", call );
        return 0;
    }
}

/*****************************************************************************
 *           NtUserNotifyIMEStatus (win32u.@)
 */
void WINAPI NtUserNotifyIMEStatus( HWND hwnd, UINT status )
{
    user_driver->pNotifyIMEStatus( hwnd, status );
}

BOOL WINAPI DECLSPEC_HIDDEN ImmProcessKey( HWND hwnd, HKL hkl, UINT vkey, LPARAM key_data, DWORD unknown )
{
    struct imm_process_key_params params =
        { .hwnd = hwnd, .hkl = hkl, .vkey = vkey, .key_data = key_data };
    void *ret_ptr;
    ULONG ret_len;
    return KeUserModeCallback( NtUserImmProcessKey, &params, sizeof(params), &ret_ptr, &ret_len );
}

BOOL WINAPI DECLSPEC_HIDDEN ImmTranslateMessage( HWND hwnd, UINT msg, WPARAM wparam, LPARAM key_data )
{
    struct imm_translate_message_params params =
        { .hwnd = hwnd, .msg = msg, .wparam = wparam, .key_data = key_data };
    void *ret_ptr;
    ULONG ret_len;
    return KeUserModeCallback( NtUserImmTranslateMessage, &params, sizeof(params),
                               &ret_ptr, &ret_len );
}
