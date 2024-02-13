#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

#include <ntstatus.h>
#define WIN32_NO_STATUS
#include <windef.h>
#include <winbase.h>
#include <winreg.h>
#include <winuser.h>
#include <setupapi.h>
#include <ddk/hidsdi.h>

#include <initguid.h>
#include <dinput.h>
#include <xinput.h>

#include <wine/hid.h>
#include <wine/debug.h>

#include "wintrace_hooks.h"

WINE_DEFAULT_DEBUG_CHANNEL(wintrace);

static BOOL verbose;

static DWORD trace_tls;
struct trace_data
{
    char buffer[4096];
    int length;
};

static void trace_init(void)
{
    if (!trace_tls) trace_tls = TlsAlloc();
    TlsSetValue( trace_tls, calloc( 1, sizeof(struct trace_data) ) );
}

#define trace( fmt, ... ) trace_( __func__, fmt, ##__VA_ARGS__ )
static void CDECL trace_( const char *function, const char *msg, ... )
{
    static LARGE_INTEGER frequency;
    struct trace_data *data = TlsGetValue( trace_tls );
    LARGE_INTEGER counter, microsecs;
    va_list valist;

    if (data->length == 0)
    {
        if (!frequency.QuadPart) QueryPerformanceFrequency( &frequency );
        QueryPerformanceCounter( &counter );
        microsecs.QuadPart = counter.QuadPart * 1000000 / frequency.QuadPart;
        data->length = sprintf( data->buffer, "%3.6f:%04lx:%s:", microsecs.QuadPart / 1000000.,
                                GetCurrentThreadId(), function );
    }

    va_start( valist, msg );
    data->length += vsprintf( data->buffer + data->length, msg, valist );
    va_end( valist );

    if (data->buffer[data->length - 1] != '\n') return;
    MESSAGE( "%s", data->buffer );
    data->length = 0;
}

static const char *debugstr_hcbt( int code )
{
    switch (code)
    {
    case HCBT_MOVESIZE: return "movesize";
    case HCBT_MINMAX: return "minmax";
    case HCBT_QS: return "qs";
    case HCBT_CREATEWND: return "create";
    case HCBT_DESTROYWND: return "destroy";
    case HCBT_ACTIVATE: return "activate";
    case HCBT_CLICKSKIPPED: return "clickskip";
    case HCBT_KEYSKIPPED: return "keyskip";
    case HCBT_SYSCOMMAND: return "command";
    case HCBT_SETFOCUS: return "setfocus";
    }
    return wine_dbg_sprintf( "%#x", code );
}

static inline const char *debugstr_rect( const RECT *rect )
{
    if (!rect) return "(null)";
    return wine_dbg_sprintf( "%ldx%ld+%ldx%ld", rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top );
}

static const char *debugstr_guithreadinfo( const GUITHREADINFO *info )
{
    if (!info) return "(null)";
    return wine_dbg_sprintf( "active %p, focus %p", info->hwndActive, info->hwndFocus );
}

static inline const char *debugstr_vkey( UINT vkey )
{
    /* clang-format off */
    switch (vkey)
    {
    default: return wine_dbg_sprintf( "0x%02x", vkey );
#define X(vkey) case vkey: return #vkey;
#include "vkey.i"
#undef X
    }
    /* clang-format on */
}

static inline const char *debugstr_msg( UINT msg )
{
    static const char *const MSG_NAMES[] = {
#include "msg.i"
    };
    if (msg >= ARRAYSIZE( MSG_NAMES ) || !MSG_NAMES[msg]) return wine_dbg_sprintf( "msg %4x", msg );
    return wine_dbg_sprintf( "msg %4x %20s", msg, MSG_NAMES[msg] );
}

static const char *debugstr_bytes( void *data, size_t length )
{
    char buffer[1024], *buf = buffer;
    const unsigned char *ptr = (const unsigned char *)data, *end = ptr + min( 32, length );
    for (DWORD i = 0; ptr + i < end; ++i)
        buf += snprintf( buf, buffer + 1024 - buf, " %02x", ptr[i] );
    return wine_dbg_sprintf( "%s%s", buffer, length > 32 ? "..." : "" );
}

static void trace_hexdump( void *data, size_t length )
{
    const unsigned char *ptr = (void *)data, *end = ptr + length;
    unsigned int i, j;

    for (i = 0; ptr + i < end;)
    {
        /* clang-format off */
        char buffer[256], *buf = buffer;
        buf += sprintf( buf, "%08x ", i );
        for (j = 0; j < 8 && ptr + i + j < end; ++j)
            buf += sprintf( buf, " %02x", ptr[i + j] );
        for (; j < 8 && ptr + i + j >= end; ++j)
            buf += sprintf( buf, "   " );
        buf += sprintf( buf, " " );
        for (j = 8; j < 16 && ptr + i + j < end; ++j)
            buf += sprintf( buf, " %02x", ptr[i + j] );
        for (; j < 16 && ptr + i + j >= end; ++j)
            buf += sprintf( buf, "   " );
        buf += sprintf( buf, "  |" );
        for (j = 0; j < 16 && ptr + i < end; ++j, ++i)
            buf += sprintf( buf, "%c", ptr[i] >= ' ' && ptr[i] <= '~' ? ptr[i] : '.' );
        buf += sprintf( buf, "|" );
        /* clang-format on */

        trace( "%s\n", buffer );
    }
}

static LRESULT wm_input( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    static const char *const TYPE_NAMES[] = {"RIM_TYPEMOUSE", "RIM_TYPEKEYBOARD", "RIM_TYPEHID"};
    unsigned char buffer[4096];
    RAWINPUT *raw = (RAWINPUT *)buffer;
    UINT size = sizeof(buffer);

    UINT count = GetRawInputData( (HRAWINPUT)lparam, RID_INPUT, raw, &size, sizeof(RAWINPUTHEADER) );
    trace( "count %x, type %2x %s, size %x, device %p, wparam %lx, ", count, raw->header.dwType,
           TYPE_NAMES[raw->header.dwType], raw->header.dwSize, raw->header.hDevice, raw->header.wParam );

    switch (raw->header.dwType)
    {
    case RIM_TYPEMOUSE:
        trace( "mouse x %+3d, y %+3d, flags %x, bflags %x, bdata %x, buttons %x, extra %x\n",
               raw->data.mouse.lLastX, raw->data.mouse.lLastY, raw->data.mouse.usFlags,
               raw->data.mouse.usButtonFlags, raw->data.mouse.usButtonData,
               raw->data.mouse.ulRawButtons, raw->data.mouse.ulExtraInformation );
        break;
    case RIM_TYPEKEYBOARD:
        trace( "keyboard code %x, flags %x, reserved %x, vkey %s, message %x, extra %x\n",
               raw->data.keyboard.MakeCode, raw->data.keyboard.Flags, raw->data.keyboard.Reserved,
               debugstr_vkey( raw->data.keyboard.VKey ), raw->data.keyboard.Message,
               raw->data.keyboard.ExtraInformation );
        break;
    case RIM_TYPEHID:
        trace( "hid size %x, count %x, data%s\n", raw->data.hid.dwSizeHid, raw->data.hid.dwCount,
               verbose ? "" : debugstr_bytes( raw->data.hid.bRawData, raw->data.hid.dwSizeHid ) );
        if (verbose) trace_hexdump( raw->data.hid.bRawData, raw->data.hid.dwSizeHid );
        break;
    default:
        assert( 0 );
        break;
    }

    return DefWindowProcA( hwnd, msg, wparam, lparam );
}

static inline const char *debugstr_touchinput( TOUCHINPUT *data )
{
    if (!data) return wine_dbg_sprintf( "touchinput (null)" );
    return wine_dbg_sprintf( "touchinput x %lu, y %lu, hSource %p, dwID %lu, dwFlags %#lx, dwMask %#lx, "
                             "dwTime %lu, dwExtraInfo %p, cxContact %lu, cyContact %lu",
                             data->x, data->y, data->hSource, data->dwID, data->dwFlags, data->dwMask,
                             data->dwTime, (void *)data->dwExtraInfo, data->cxContact, data->cyContact );
}

static LRESULT wm_touch( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    unsigned char buffer[4096];
    TOUCHINPUT *touch = (TOUCHINPUT *)buffer;
    POINTER_INPUT_TYPE type;
    HTOUCHINPUT handle;
    UINT count, ret;

    switch (msg)
    {
    case WM_TOUCH:
    {
        count = LOWORD( wparam );
        handle = (HTOUCHINPUT)lparam;
        GetTouchInputInfo( handle, count, touch, sizeof(TOUCHINPUT) );

        trace( "%s", debugstr_msg( msg ) );
        for (int i = 0; i < count; ++i) trace( " %s\n", debugstr_touchinput( touch + i ) );

        CloseTouchInputHandle( handle );
        break;
    }

    case WM_POINTERUPDATE:
    case WM_POINTERDOWN:
    case WM_POINTERUP:
        ret = GetPointerType( GET_POINTERID_WPARAM( wparam ), &type );
        ERR( "GetPointerType ret %u\n", ret );

        trace( "%s id %u, type %u, new %u, range %u, contact %u, primary %u, buttons %#x, x %u, y %u\n",
               debugstr_msg( msg ), GET_POINTERID_WPARAM( wparam ), type,
               IS_POINTER_NEW_WPARAM( wparam ), IS_POINTER_INRANGE_WPARAM( wparam ),
               IS_POINTER_INCONTACT_WPARAM( wparam ), IS_POINTER_PRIMARY_WPARAM( wparam ),
               (IS_POINTER_FIRSTBUTTON_WPARAM( wparam ) << 0) |
                   (IS_POINTER_SECONDBUTTON_WPARAM( wparam ) << 1) |
                   (IS_POINTER_THIRDBUTTON_WPARAM( wparam ) << 2) |
                   (IS_POINTER_FOURTHBUTTON_WPARAM( wparam ) << 3) |
                   (IS_POINTER_FIFTHBUTTON_WPARAM( wparam ) << 4),
               LOWORD( lparam ), HIWORD( lparam ) );

        break;
    }

    return DefWindowProcA( hwnd, msg, wparam, lparam );
}

static LRESULT wm_mouse( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    switch (msg)
    {
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
        trace( "%s button %lx, x %u, y %u\n", debugstr_msg( msg ), wparam, LOWORD( lparam ), HIWORD( lparam ) );
        break;
    }

    return DefWindowProcA( hwnd, msg, wparam, lparam );
}

static LRESULT CALLBACK keyboard_subclass(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, UINT_PTR id, DWORD_PTR ref)
{
    char buffer[16] = {0};
    BYTE state[256];
    DWORD ret;

    switch (msg)
    {
    case WM_CHAR:
        trace( "%s repeat %d, scan %x, char %s\n", debugstr_msg( msg ), LOWORD( lparam ),
               HIWORD( lparam ) & 0x1ff, debugstr_wn((WCHAR *)&wparam, 1) );
        break;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        trace( "%s repeat %d, scan %x, vkey %s\n", debugstr_msg( msg ), LOWORD( lparam ),
               HIWORD( lparam ) & 0x1ff, debugstr_vkey( wparam ) );

        GetKeyboardState( state );
        ret = ToAscii( wparam, HIWORD(lparam), state, (LPWORD)buffer, 0 );

        trace( "ret %u, buffer %s\n", ret, debugstr_a(buffer) );
        break;
    }

    return DefSubclassProc(hwnd, msg, wparam, lparam);
}

static LRESULT wm_keyboard( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    char buffer[16] = {0};
    BYTE state[256];
    DWORD ret;

    switch (msg)
    {
    case WM_CHAR:
        trace( "%s repeat %d, scan %x, char %s\n", debugstr_msg( msg ), LOWORD( lparam ),
               HIWORD( lparam ) & 0x1ff, debugstr_wn((WCHAR *)&wparam, 1) );
        break;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        trace( "%s repeat %d, scan %x, vkey %s\n", debugstr_msg( msg ), LOWORD( lparam ),
               HIWORD( lparam ) & 0x1ff, debugstr_vkey( wparam ) );

        GetKeyboardState( state );
        ret = ToAscii( wparam, HIWORD(lparam), state, (LPWORD)buffer, 0 );

        trace( "ret %u, buffer %s\n", ret, debugstr_a(buffer) );
        break;
    }

    return DefWindowProcA( hwnd, msg, wparam, lparam );
}

static LRESULT WINAPI wintrace_wndproc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    GUITHREADINFO thread_info = {.cbSize = sizeof(GUITHREADINFO)};

    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage( 0 );
        break;
    case WM_INPUT:
        return wm_input( hwnd, msg, wparam, lparam );
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
        return wm_mouse( hwnd, msg, wparam, lparam );
    case WM_CHAR:
    case WM_KEYDOWN:
    case WM_SYSCOMMAND:
        return wm_keyboard( hwnd, msg, wparam, lparam );
    case WM_TOUCH:
    case WM_POINTERUPDATE:
    case WM_POINTERDOWN:
    case WM_POINTERUP:
        return wm_touch( hwnd, msg, wparam, lparam );
    }

    trace( "foreground %p, ", GetForegroundWindow() );
    if (msg != WM_COPYDATA)
    {
        trace( "hwnd %p, msg %#x, wparam %#lx, lparam %#lx\n", hwnd, msg, wparam, lparam );
        return DefWindowProcA( hwnd, msg, wparam, lparam );
    }

    switch (wparam)
    {
    case HCBT_CREATEWND:
    {
        COPYDATASTRUCT *data = (COPYDATASTRUCT *)lparam;
        struct hcbt_create_window_data *info = data->lpData;

        GetGUIThreadInfo( data->dwData, &thread_info );

        trace( "code %9s, thread %04lx, gui %s, module %16s, window %p, parent_window %p, "
               "style %08x:%08x, rect %s, name %s, class %s\n",
               debugstr_hcbt( wparam ), data->dwData, debugstr_guithreadinfo( &thread_info ),
               debugstr_w(info->module_name), UlongToHandle( info->window ),
               UlongToHandle( info->parent_window ), info->style, info->exstyle,
               debugstr_rect( &info->rect ), debugstr_w(info->window_name), debugstr_w(info->class_name) );
        break;
    }

    case HCBT_DESTROYWND:
    {
        COPYDATASTRUCT *data = (COPYDATASTRUCT *)lparam;
        struct hcbt_destroy_window_data *info = data->lpData;

        GetGUIThreadInfo( data->dwData, &thread_info );

        trace( "code %9s, thread %04lx, gui %s, module %16s, window %p, style %08x:%08x\n",
               debugstr_hcbt( wparam ), data->dwData, debugstr_guithreadinfo( &thread_info ),
               debugstr_w(info->module_name), UlongToHandle( info->window ),
               GetWindowLongA( ULongToHandle( info->window ), GWL_STYLE ),
               GetWindowLongA( ULongToHandle( info->window ), GWL_EXSTYLE ) );
        break;
    }

    case HCBT_ACTIVATE:
    {
        COPYDATASTRUCT *data = (COPYDATASTRUCT *)lparam;
        struct hcbt_activate_data *info = data->lpData;

        GetGUIThreadInfo( data->dwData, &thread_info );

        trace( "code %9s, thread %04lx, gui %s, module %16s, curr_active %p, style %08x:%08x, "
               "next_active %p, style %08x:%08x, mouse %u\n",
               debugstr_hcbt( wparam ), data->dwData, debugstr_guithreadinfo( &thread_info ),
               debugstr_w(info->module_name), UlongToHandle( info->curr_active ),
               GetWindowLongA( ULongToHandle( info->curr_active ), GWL_STYLE ),
               GetWindowLongA( ULongToHandle( info->curr_active ), GWL_EXSTYLE ),
               UlongToHandle( info->next_active ),
               GetWindowLongA( ULongToHandle( info->next_active ), GWL_STYLE ),
               GetWindowLongA( ULongToHandle( info->next_active ), GWL_EXSTYLE ), (BOOL)info->with_mouse );
        break;
    }

    case HCBT_SETFOCUS:
    {
        COPYDATASTRUCT *data = (COPYDATASTRUCT *)lparam;
        struct hcbt_set_focus_data *info = data->lpData;

        GetGUIThreadInfo( data->dwData, &thread_info );

        trace( "code %9s, thread %04lx, gui %s, module %16s, curr_focus %p, style %08x:%08x, "
               "next_focus %p, style %08x:%08x\n",
               debugstr_hcbt( wparam ), data->dwData, debugstr_guithreadinfo( &thread_info ),
               debugstr_w(info->module_name), UlongToHandle( info->curr_focus ),
               GetWindowLongA( ULongToHandle( info->curr_focus ), GWL_STYLE ),
               GetWindowLongA( ULongToHandle( info->curr_focus ), GWL_EXSTYLE ),
               UlongToHandle( info->next_focus ), GetWindowLongA( ULongToHandle( info->next_focus ), GWL_STYLE ),
               GetWindowLongA( ULongToHandle( info->next_focus ), GWL_EXSTYLE ) );
        break;
    }

    case HCBT_MINMAX:
    {
        COPYDATASTRUCT *data = (COPYDATASTRUCT *)lparam;
        struct hcbt_min_max_data *info = data->lpData;

        GetGUIThreadInfo( data->dwData, &thread_info );

        trace( "code %9s, thread %04lx, gui %s, module %16s, window %p, style %08x:%08x, operation "
               "%#lx\n",
               debugstr_hcbt( wparam ), data->dwData, debugstr_guithreadinfo( &thread_info ),
               debugstr_w(info->module_name), UlongToHandle( info->window ),
               GetWindowLongA( ULongToHandle( info->window ), GWL_STYLE ),
               GetWindowLongA( ULongToHandle( info->window ), GWL_EXSTYLE ), (LPARAM)info->operation );
        break;
    }

    case HCBT_MOVESIZE:
    {
        COPYDATASTRUCT *data = (COPYDATASTRUCT *)lparam;
        struct hcbt_move_size_data *info = data->lpData;

        GetGUIThreadInfo( data->dwData, &thread_info );

        trace( "code %9s, thread %04lx, gui %s, module %16s, window %p, style %08x:%08x, rect %s\n",
               debugstr_hcbt( wparam ), data->dwData, debugstr_guithreadinfo( &thread_info ),
               debugstr_w(info->module_name), UlongToHandle( info->window ),
               GetWindowLongA( ULongToHandle( info->window ), GWL_STYLE ),
               GetWindowLongA( ULongToHandle( info->window ), GWL_EXSTYLE ), debugstr_rect( &info->rect ) );
        break;
    }

    case HCBT_CLICKSKIPPED:
        /*
        Specifies the mouse message removed from the system message
        queue.

        Specifies a long pointer to a MOUSEHOOKSTRUCT structure
        containing the hit-test code and the handle to the window for
        which the mouse message is intended.

        The HCBT_CLICKSKIPPED value is sent to a CBTProc hook procedure
        only if a WH_MOUSE hook is installed. For a list of hit-test
        codes, see WM_NCHITTEST.
        */

    case HCBT_KEYSKIPPED:
        /*
        Specifies the virtual-key code.

        Specifies the repeat count, scan code, key-transition code,
        previous key state, and context code. The HCBT_KEYSKIPPED value
        is sent to a CBTProc hook procedure only if a WH_KEYBOARD hook
        is installed. For more information, see WM_KEYUP or
        WM_KEYDOWN.
        */

    case HCBT_QS:
        /*
        Is undefined and must be zero.

        Is undefined and must be zero.
        */

    case HCBT_SYSCOMMAND:
        /*
        Specifies a system-command value (SC_) specifying the system
        command. For more information about system-command values, see
        WM_SYSCOMMAND.

        Contains the same data as the lParam value of a WM_SYSCOMMAND
        message: If a system menu command is chosen with the mouse, the
        low-order word contains the x-coordinate of the cursor, in
        screen coordinates, and the high-order word contains the
        y-coordinate; otherwise, the parameter is not used.
        */

    case HCBT_GETMESSAGE:
    {
        COPYDATASTRUCT *data = (COPYDATASTRUCT *)lparam;
        struct hcbt_get_message_data *info = data->lpData;

        GetGUIThreadInfo( data->dwData, &thread_info );

        trace( "code %9s, thread %04lx, gui %s, module %16s, window %p, wparam %#I64x, lparam %#I64x\n",
               debugstr_hcbt( wparam ), data->dwData, debugstr_guithreadinfo( &thread_info ),
               debugstr_w(info->module_name), UlongToHandle( info->window ), info->message, info->wparam, info->lparam );
        break;
    }

    case HCBT_CALLWNDPROC:
    {
        COPYDATASTRUCT *data = (COPYDATASTRUCT *)lparam;
        struct hcbt_call_window_proc_data *info = data->lpData;

        GetGUIThreadInfo( data->dwData, &thread_info );

        trace( "code %9s, thread %04lx, gui %s, module %16s, window %p, wparam %#I64x, lparam %#I64x\n",
               debugstr_hcbt( wparam ), data->dwData, debugstr_guithreadinfo( &thread_info ),
               debugstr_w(info->module_name), UlongToHandle( info->window ), info->message, info->wparam, info->lparam );
        break;
    }

    case HCBT_CALLWNDPROCRET:
    {
        COPYDATASTRUCT *data = (COPYDATASTRUCT *)lparam;
        struct hcbt_call_window_proc_ret_data *info = data->lpData;

        GetGUIThreadInfo( data->dwData, &thread_info );

        trace( "code %9s, thread %04lx, gui %s, module %16s, window %p, wparam %#I64x, lparam %#I64x, ret %#I64x\n",
               debugstr_hcbt( wparam ), data->dwData, debugstr_guithreadinfo( &thread_info ),
               debugstr_w(info->module_name), UlongToHandle( info->window ), info->message, info->wparam, info->lparam, info->lresult );
        break;
    }

    default:
        trace( "code %9s, lparam %#lx\n", debugstr_hcbt( wparam ), lparam );
        break;
    }

    return DefWindowProcA( hwnd, msg, wparam, lparam );
}

static LRESULT CALLBACK ll_hook( int code, WPARAM wparam, LPARAM lparam )
{
    KBDLLHOOKSTRUCT *kbdll = (KBDLLHOOKSTRUCT *)lparam;
    MSLLHOOKSTRUCT *msll = (MSLLHOOKSTRUCT *)lparam;
    POINT pt;

    trace( "code %#x, wparam %lx, lparam %lx\n", code, wparam, lparam );

    if (code != HC_ACTION) return CallNextHookEx( 0, code, wparam, lparam );

    switch (wparam)
    {
    case WM_SYSKEYDOWN:
        trace( "WM_SYSKEYDOWN %x %x %x %d\n", kbdll->vkCode, kbdll->scanCode, kbdll->flags, kbdll->time );
        break;
    case WM_SYSKEYUP:
        trace( "WM_SYSKEYUP %x %x %x %d\n", kbdll->vkCode, kbdll->scanCode, kbdll->flags, kbdll->time );
        break;
    case WM_KEYDOWN:
        trace( "WM_KEYDOWN %x %x %x %d\n", kbdll->vkCode, kbdll->scanCode, kbdll->flags, kbdll->time );
        break;
    case WM_KEYUP:
        trace( "WM_KEYUP %x %x %x %d\n", kbdll->vkCode, kbdll->scanCode, kbdll->flags, kbdll->time );
        break;
    case WM_MOUSEMOVE:
        GetCursorPos( &pt );
        if (msll->pt.x == pt.x && msll->pt.y == pt.y) break;
        trace( "WM_MOUSEMOVE %d %d\n", msll->pt.x - pt.x, msll->pt.y - pt.y );
        break;
    }

    return CallNextHookEx( 0, code, wparam, lparam );
}

int WINAPI WinMain( HINSTANCE instance, HINSTANCE previous, char *cmdline, int cmdshow )
{
    HHOOK cbt_hook, getmessage_hook, callwndproc_hook, callwndprocret_hook, mouse_hook = NULL, keyboard_hook = NULL;
    LRESULT (*CALLBACK hook_proc)( int code, WPARAM wparam, LPARAM lparam );
    WNDCLASSEXA class =
    {
        .cbSize = sizeof(WNDCLASSEXA),
        .lpfnWndProc = wintrace_wndproc,
        .lpszClassName = "wintrace",
        .hInstance = instance,
    };
    HMODULE wintrace_hooks = NULL;
    HWND hwnd;
    MSG msg;

    TRACE("instance %p, previous %p, cmdline %s, cmdshow %d.\n", instance, previous, debugstr_a(cmdline), cmdshow);

    trace_init();
    RegisterClassExA( &class );

    hwnd = CreateWindowA( "wintrace", "wintrace", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                          CW_USEDEFAULT, CW_USEDEFAULT, 200, 200, 0, 0, instance, NULL );
    SetWindowSubclass( hwnd, keyboard_subclass, 0, 0 );

    if (strstr( cmdline, "verbose" )) verbose = TRUE;

    if (!strstr( cmdline, "hook:cbt" )) cbt_hook = NULL;
    else
    {
        wintrace_hooks = LoadLibraryW( L"wintrace_hooks" );
        hook_proc = (void *)GetProcAddress( wintrace_hooks, "hook_cbt" );
        cbt_hook = SetWindowsHookExW( WH_CBT, hook_proc, wintrace_hooks, 0 );
        assert( cbt_hook );
    }

    if (!strstr( cmdline, "hook:getmessage" )) getmessage_hook = NULL;
    else
    {
        wintrace_hooks = LoadLibraryW( L"wintrace_hooks" );
        hook_proc = (void *)GetProcAddress( wintrace_hooks, "hook_getmessage" );
        getmessage_hook = SetWindowsHookExW( WH_GETMESSAGE, hook_proc, wintrace_hooks, 0 );
        assert( getmessage_hook );
    }

    if (!strstr( cmdline, "hook:callwndproc" )) callwndproc_hook = callwndprocret_hook = NULL;
    else
    {
        wintrace_hooks = LoadLibraryW( L"wintrace_hooks" );
        hook_proc = (void *)GetProcAddress( wintrace_hooks, "hook_callwndproc" );
        callwndproc_hook = SetWindowsHookExW( WH_CALLWNDPROC, hook_proc, wintrace_hooks, 0 );
        assert( callwndproc_hook );
        hook_proc = (void *)GetProcAddress( wintrace_hooks, "hook_callwndprocret" );
        callwndprocret_hook = SetWindowsHookExW( WH_CALLWNDPROCRET, hook_proc, wintrace_hooks, 0 );
        assert( callwndprocret_hook );
    }

    if (strstr( cmdline, "hook:ll" ))
    {
        mouse_hook = SetWindowsHookExW( WH_MOUSE_LL, ll_hook, NULL, 0 );
        assert( mouse_hook );
        keyboard_hook = SetWindowsHookExW( WH_KEYBOARD_LL, ll_hook, NULL, 0 );
        assert( keyboard_hook );
    }

    trace( "desktop %p, foreground %p\n", GetDesktopWindow(), GetForegroundWindow() );

    while (GetMessageA( &msg, 0, 0, 0 ))
    {
        TranslateMessage( &msg );
        DispatchMessageA( &msg );
    }

    if (keyboard_hook) UnhookWindowsHookEx( keyboard_hook );
    if (mouse_hook) UnhookWindowsHookEx( mouse_hook );
    if (callwndprocret_hook) UnhookWindowsHookEx( callwndprocret_hook );
    if (callwndproc_hook) UnhookWindowsHookEx( callwndproc_hook );
    if (getmessage_hook) UnhookWindowsHookEx( getmessage_hook );
    if (cbt_hook) UnhookWindowsHookEx( cbt_hook );
    if (wintrace_hooks) FreeLibrary( wintrace_hooks );

    DestroyWindow( hwnd );
    UnregisterClassA( "wintrace", instance );
    return 0;
}
