#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

#define COBJMACROS
#include <ntstatus.h>
#define WIN32_NO_STATUS
#include <windef.h>
#include <winbase.h>
#include <winreg.h>
#include <wingdi.h>
#include <winuser.h>
#include <winstring.h>
#include <setupapi.h>
#include <ddk/hidsdi.h>

#include <initguid.h>
#include <d3d9.h>
#include <dxgi1_6.h>
#include <dinput.h>
#include <xinput.h>
#include <roapi.h>
#include <setupapi.h>
#include <devpkey.h>
#include <devguid.h>
#include <ddk/d3dkmthk.h>

#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#include "windows.foundation.h"
#define WIDL_using_Windows_Devices_Haptics
#define WIDL_using_Windows_Gaming_Input
#include "windows.gaming.input.h"

#include <wine/hid.h>
#include <wine/debug.h>

#include "wintrace_hooks.h"

WINE_DEFAULT_DEBUG_CHANNEL(wintrace);

static BOOL verbose;

static HANDLE trace_file;
static DWORD trace_tls;
struct trace_data
{
    char buffer[4096];
    int length;
};

static BOOL WINAPI trace_init_global( INIT_ONCE *once, void *param, void **context )
{
    static const char *separator = "================================\n";
    WCHAR path[MAX_PATH];
    DWORD written;

    GetTempPathW( ARRAY_SIZE(path), path );
    wcscat( path, L"\\wintrace.log" );

    trace_tls = TlsAlloc();
    trace_file = CreateFileW( path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, 0, 0 );
    WriteFile( trace_file, separator, strlen( separator ), &written, NULL );

    return TRUE;
}

static void trace_init(void)
{
    static INIT_ONCE trace_init_once = INIT_ONCE_STATIC_INIT;
    InitOnceExecuteOnce( &trace_init_once, trace_init_global, NULL, NULL );
    TlsSetValue( trace_tls, calloc( 1, sizeof(struct trace_data) ) );
}

#define trace( fmt, ... ) trace_( __func__, fmt, ##__VA_ARGS__ )
static void CDECL trace_( const char *function, const char *msg, ... )
{
    static LARGE_INTEGER frequency;
    struct trace_data *data = TlsGetValue( trace_tls );
    LARGE_INTEGER counter, microsecs;
    va_list valist;
    DWORD written;

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
    WriteFile( trace_file, data->buffer, strlen( data->buffer ), &written, NULL );
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
    case HCBT_GETMESSAGE: return "getmessage";
    case HCBT_CALLWNDPROC: return "callwndproc";
    case HCBT_CALLWNDPROCRET: return "callwndprocret";
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

    return DefWindowProcW( hwnd, msg, wparam, lparam );
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
        ret = GetTouchInputInfo( handle, count, touch, sizeof(TOUCHINPUT) );

        trace( "hwnd %p, msg %s, wparam %#lx, lparam %#lx, ret %u\n", hwnd, debugstr_msg( msg ), wparam, lparam, ret );
        for (int i = 0; i < count; ++i) trace( " %s\n", debugstr_touchinput( touch + i ) );

        CloseTouchInputHandle( handle );
        break;
    }

    case WM_POINTERUPDATE:
    case WM_POINTERDOWN:
    case WM_POINTERUP:
        ret = GetPointerType( GET_POINTERID_WPARAM( wparam ), &type );

        trace( "hwnd %p, msg %s, wparam %#lx, lparam %#lx, ret %u, id %u, type %u, "
               "new %u, range %u, contact %u, primary %u, buttons %#x, x %u, y %u\n",
               hwnd, debugstr_msg( msg ), wparam, lparam, ret, GET_POINTERID_WPARAM( wparam ), type,
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

    return DefWindowProcW( hwnd, msg, wparam, lparam );
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

    return DefWindowProcW( hwnd, msg, wparam, lparam );
}

static LRESULT wm_keyboard( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    switch (msg)
    {
    case WM_CHAR:
        trace( "%s repeat %d, scan %x, char %s\n", debugstr_msg( msg ), LOWORD( lparam ),
               HIWORD( lparam ) & 0x1ff, debugstr_wn((WCHAR *)&wparam, 1) );
        break;
    case WM_KEYUP:
    case WM_KEYDOWN:
    case WM_SYSKEYUP:
    case WM_SYSKEYDOWN:
        trace( "%s repeat %d, scan %x, vkey %s\n", debugstr_msg( msg ), LOWORD( lparam ),
               HIWORD( lparam ) & 0x1ff, debugstr_vkey( wparam ) );
        break;
    }

    return DefWindowProcW( hwnd, msg, wparam, lparam );
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
    case WM_KEYUP:
    case WM_KEYDOWN:
    case WM_SYSKEYUP:
    case WM_SYSKEYDOWN:
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
        trace( "hwnd %p, msg %s, wparam %#lx, lparam %#lx\n", hwnd, debugstr_msg( msg ), wparam, lparam );
        return DefWindowProcW( hwnd, msg, wparam, lparam );
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
               GetWindowLongW( ULongToHandle( info->window ), GWL_STYLE ),
               GetWindowLongW( ULongToHandle( info->window ), GWL_EXSTYLE ) );
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
               GetWindowLongW( ULongToHandle( info->curr_active ), GWL_STYLE ),
               GetWindowLongW( ULongToHandle( info->curr_active ), GWL_EXSTYLE ),
               UlongToHandle( info->next_active ),
               GetWindowLongW( ULongToHandle( info->next_active ), GWL_STYLE ),
               GetWindowLongW( ULongToHandle( info->next_active ), GWL_EXSTYLE ), (BOOL)info->with_mouse );
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
               GetWindowLongW( ULongToHandle( info->curr_focus ), GWL_STYLE ),
               GetWindowLongW( ULongToHandle( info->curr_focus ), GWL_EXSTYLE ),
               UlongToHandle( info->next_focus ), GetWindowLongW( ULongToHandle( info->next_focus ), GWL_STYLE ),
               GetWindowLongW( ULongToHandle( info->next_focus ), GWL_EXSTYLE ) );
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
               GetWindowLongW( ULongToHandle( info->window ), GWL_STYLE ),
               GetWindowLongW( ULongToHandle( info->window ), GWL_EXSTYLE ), (LPARAM)info->operation );
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
               GetWindowLongW( ULongToHandle( info->window ), GWL_STYLE ),
               GetWindowLongW( ULongToHandle( info->window ), GWL_EXSTYLE ), debugstr_rect( &info->rect ) );
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

    return DefWindowProcW( hwnd, msg, wparam, lparam );
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

static DWORD CALLBACK hid_thread( void *arg )
{
    unsigned char buffer[1024];
    HANDLE device = arg;
    ULONG length;

    trace_init();

    while (ReadFile( device, buffer, sizeof(buffer), &length, NULL ))
    {
        trace( "device %p, length %x, data%s\n", device, length, verbose ? "" : debugstr_bytes( buffer, length ) );
        if (verbose) trace_hexdump( buffer, length );
    }

    return 0;
}

static inline const char *debugstr_hid_value_caps( struct hid_value_caps *caps )
{
    if (!caps) return "(null)";
    return wine_dbg_sprintf( "RId %d, Usg %02x:%02x-%02x Dat %02x-%02x, Str %d-%d, Des %d-%d, "
                             "Bits %02lx Flags %#lx, LCol %d LUsg %02x:%02x, BitSz %d, RCnt %d, Unit %lx E%+ld, Log %+ld-%+ld, Phy %+ld-%+ld",
                             caps->report_id, caps->usage_page, caps->usage_min, caps->usage_max, caps->data_index_min, caps->data_index_max,
                             caps->string_min, caps->string_max, caps->designator_min, caps->designator_max, caps->bit_field, caps->flags,
                             caps->link_collection, caps->link_usage_page, caps->link_usage, caps->bit_size, caps->report_count,
                             caps->units, caps->units_exp, caps->logical_min, caps->logical_max, caps->physical_min, caps->physical_max );
}

static inline const char *debugstr_hid_collection_node( struct hid_collection_node *node )
{
    if (!node) return "(null)";
    return wine_dbg_sprintf( "Usg %02x:%02x, Parent %u, Next %u, NbChild %u, Child %u, Type %02lx",
                             node->usage_page, node->usage, node->parent, node->next_sibling,
                             node->number_of_children, node->first_child, node->collection_type );
}

static BOOL hid_open( const WCHAR *path, HANDLE *device, PHIDP_PREPARSED_DATA *preparsed,
                      HIDD_ATTRIBUTES *attrs, HIDP_CAPS *caps )
{
    struct hid_preparsed_data *data = NULL;
    unsigned int i, end;
    HANDLE handle;

    handle = CreateFileW( path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                          NULL, OPEN_EXISTING, 0, NULL );
    if (handle == INVALID_HANDLE_VALUE) return FALSE;

    if (!HidD_GetPreparsedData( handle, (PHIDP_PREPARSED_DATA *)&data )) goto failed;
    if (!HidD_GetAttributes( handle, attrs )) goto failed;
    if (HidP_GetCaps( (PHIDP_PREPARSED_DATA)data, caps ) != HIDP_STATUS_SUCCESS) goto failed;

    trace( "%s attrs vid %#x, pid %#x, rev %#x\n", debugstr_w(path), attrs->VendorID, attrs->ProductID, attrs->VersionNumber );
    trace( "usage %02x:%02x input %u-(%u)-%u, report len %u output %u-(%u)-%u, report len %u "
           "feature %u-(%u)-%u, report len %u collections %u\n", data->usage_page, data->usage,
            data->input_caps_start, data->input_caps_count, data->input_caps_end, data->input_report_byte_length,
            data->output_caps_start, data->output_caps_count, data->output_caps_end, data->output_report_byte_length,
            data->feature_caps_start, data->feature_caps_count, data->feature_caps_end, data->feature_report_byte_length,
            data->number_link_collection_nodes );
    end = data->input_caps_count;
    for (i = 0; i < end; i++) trace( "input %d: %s\n", i, debugstr_hid_value_caps( HID_INPUT_VALUE_CAPS( data ) + i ) );
    end = data->output_caps_count;
    for (i = 0; i < end; i++) trace( "output %d: %s\n", i, debugstr_hid_value_caps( HID_OUTPUT_VALUE_CAPS( data ) + i ) );
    end = data->feature_caps_count;
    for (i = 0; i < end; i++) trace( "feature %d: %s\n", i, debugstr_hid_value_caps( HID_FEATURE_VALUE_CAPS( data ) + i ) );
    end = data->number_link_collection_nodes;
    for (i = 0; i < end; i++) trace( "collection %d: %s\n", i, debugstr_hid_collection_node( HID_COLLECTION_NODES( data ) + i ) );

    *device = handle;
    *preparsed = (PHIDP_PREPARSED_DATA)data;
    return TRUE;

failed:
    CloseHandle( handle );
    HidD_FreePreparsedData( (PHIDP_PREPARSED_DATA)data );
    return FALSE;
}

static BOOL CALLBACK trace_objects( const DIDEVICEOBJECTINSTANCEW *obj, void *args )
{
    trace( "type %s, ofs %#x, type %#x, flags %#x, ff_max %u, ff_res %u, coll %u, index %u, usage %04x:%04x, "
           "dim %#x, exp %#x, rid %u, name %s\n", debugstr_guid(&obj->guidType), obj->dwOfs, obj->dwType, obj->dwFlags,
           obj->dwFFMaxForce, obj->dwFFForceResolution, obj->wCollectionNumber, obj->wDesignatorIndex,
           obj->wUsagePage, obj->wUsage, obj->dwDimension, obj->wExponent, obj->wReportId, debugstr_w(obj->tszName) );
    return DIENUM_CONTINUE;
}

static DWORD CALLBACK dinput_thread( void *arg )
{
    DIDEVICEINSTANCEW instance = {.dwSize = sizeof(DIDEVICEINSTANCEW)};
    DIDEVCAPS caps = {.dwSize = sizeof(DIDEVCAPS)};
    IDirectInputDevice8W *device = arg;
    unsigned char buffer[1024];
    ULONG length, type;
    HANDLE event;
    HRESULT hr;

    trace_init();

    hr = IDirectInputDevice8_GetDeviceInfo( device, &instance );
    assert( hr == DI_OK );
    type = GET_DIDEVICE_TYPE( instance.dwDevType );
    if (type == DI8DEVTYPE_KEYBOARD) length = c_dfDIKeyboard.dwDataSize;
    else if (type == DI8DEVTYPE_MOUSE) length = c_dfDIMouse2.dwDataSize;
    else length = c_dfDIJoystick2.dwDataSize;

    trace( "usage %04x:%04x devtype %#x, instance %s %s, product %s %s, ff_driver %s\n", instance.wUsagePage, instance.wUsage,
           instance.dwDevType, debugstr_guid(&instance.guidInstance), debugstr_w(instance.tszInstanceName),
           debugstr_guid(&instance.guidInstance), debugstr_w(instance.tszInstanceName), debugstr_guid(&instance.guidFFDriver) );

    hr = IDirectInputDevice8_GetCapabilities( device, &caps );
    assert( hr == DI_OK );

    trace( "caps flags %#x, devtype %#x, axes %u, buttons %u, povs %u, ff_period %u, ff_min_time %u,"
           " firmware %u, hardware %u, ff_version %u\n",  caps.dwFlags, caps.dwDevType, caps.dwAxes,
           caps.dwButtons, caps.dwPOVs, caps.dwFFSamplePeriod, caps.dwFFMinTimeResolution, caps.dwFirmwareRevision,
           caps.dwHardwareRevision, caps.dwFFDriverVersion );

    hr = IDirectInputDevice8_EnumObjects( device, trace_objects, NULL, DIDFT_ALL );
    assert( hr == DI_OK );

    event = CreateEventW( NULL, FALSE, FALSE, NULL );
    assert( event );
    hr = IDirectInputDevice8_SetEventNotification( device, event );
    assert( hr == DI_OK );

    while (FAILED(hr = IDirectInputDevice8_Acquire( device ))) Sleep( 100 );

    while (WaitForSingleObject( event, INFINITE ) == 0)
    {
        hr = IDirectInputDevice8_GetDeviceState( device, length, buffer );
        while (FAILED(hr))
        {
            while (FAILED(hr = IDirectInputDevice8_Acquire( device ))) Sleep( 100 );
            hr = IDirectInputDevice8_GetDeviceState( device, length, buffer );
        }

        trace( "device %p, length %x, data%s\n", device, length, verbose ? "" : debugstr_bytes( buffer, length ) );
        if (verbose) trace_hexdump( buffer, length );
    }

    CloseHandle( event );
    IDirectInputDevice8_Release( device );

    return 0;
}

static BOOL CALLBACK dinput_list( const DIDEVICEINSTANCEW *instance, void *context )
{
    IDirectInput8W *dinput = context;
    IDirectInputDevice8W *device;

    trace( "usage %04x:%04x devtype %#x, instance %s %s, product %s %s, ff_driver %s\n", instance->wUsagePage, instance->wUsage,
           instance->dwDevType, debugstr_guid(&instance->guidInstance), debugstr_w(instance->tszInstanceName),
           debugstr_guid(&instance->guidInstance), debugstr_w(instance->tszInstanceName), debugstr_guid(&instance->guidFFDriver) );

    if (SUCCEEDED(IDirectInput8_CreateDevice( dinput, &instance->guidInstance, &device, NULL )) ||
        SUCCEEDED(IDirectInput8_CreateDevice( dinput, &instance->guidProduct, &device, NULL )))
    {
        DIPROPGUIDANDPATH guid_path = {.diph = {.dwHeaderSize = sizeof(DIPROPHEADER), .dwSize = sizeof(DIPROPGUIDANDPATH)}};
        DIPROPDWORD vid_pid = {.diph = {.dwHeaderSize = sizeof(DIPROPHEADER), .dwSize = sizeof(DIPROPDWORD)}};
        HANDLE handle;

        if (SUCCEEDED(IDirectInputDevice8_GetProperty( device, DIPROP_VIDPID, &vid_pid.diph )))
            trace( "  vid/pid: %04x:%04x\n", vid_pid.dwData >> 16, vid_pid.dwData & 0xffff );
        if (SUCCEEDED(IDirectInputDevice8_GetProperty( device, DIPROP_GUIDANDPATH, &guid_path.diph )))
        {
            trace( "  guid: %s\n", debugstr_guid(&guid_path.guidClass) );
            trace( "  path: %s\n", debugstr_w(guid_path.wszPath) );
        }

        handle = CreateFileW( guid_path.wszPath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              NULL, OPEN_EXISTING, 0, NULL );
        trace( "    handle: %p\n", handle );
        CloseHandle( handle );

        IDirectInput8_Release( device );
    }

    return DIENUM_CONTINUE;
}

static BOOL CALLBACK dinput_open( const DIDEVICEINSTANCEW *instance, void *context )
{
    static const DIPROPDWORD buffer_size =
    {
        .diph =
        {
            .dwHeaderSize = sizeof(DIPROPHEADER),
            .dwSize = sizeof(DIPROPDWORD),
            .dwHow = DIPH_DEVICE,
            .dwObj = 0,
        },
        .dwData = UINT_MAX,
    };
    IDirectInput8W *dinput = context;
    IDirectInputDevice8W *device;
    HANDLE thread;
    HRESULT hr;

    trace( "usage %04x:%04x devtype %#x, instance %s %s, product %s %s, ff_driver %s\n", instance->wUsagePage, instance->wUsage,
           instance->dwDevType, debugstr_guid(&instance->guidInstance), debugstr_w(instance->tszInstanceName),
           debugstr_guid(&instance->guidInstance), debugstr_w(instance->tszInstanceName), debugstr_guid(&instance->guidFFDriver) );

    if (GET_DIDEVICE_TYPE(instance->dwDevType) == DI8DEVTYPE_MOUSE) return DIENUM_CONTINUE;
    if (GET_DIDEVICE_TYPE(instance->dwDevType) == DI8DEVTYPE_KEYBOARD) return DIENUM_CONTINUE;

    if (FAILED(IDirectInput8_CreateDevice( dinput, &instance->guidInstance, &device, NULL )) &&
        FAILED(IDirectInput8_CreateDevice( dinput, &instance->guidProduct, &device, NULL )))
        return DIENUM_CONTINUE;

    hr = IDirectInputDevice8_SetDataFormat( device, &c_dfDIJoystick2 );
    assert( hr == DI_OK );
    hr = IDirectInputDevice8_SetCooperativeLevel( device, 0, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND );
    assert( hr == DI_OK );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_BUFFERSIZE, &buffer_size.diph );
    assert( hr == DI_OK );

    thread = CreateThread( NULL, 0, dinput_thread, device, 0, NULL );
    assert( thread );
    CloseHandle( thread );

    return DIENUM_CONTINUE;
}

static DWORD CALLBACK xinput_thread( void *arg )
{
    XINPUT_CAPABILITIES caps, prev_caps[16];
    XINPUT_STATE state, prev_state[16];
    DWORD i, ret;

    trace_init();

    while (1)
    {
        for (i = 0; i < 16; ++i)
        {
            if ((ret = XInputGetCapabilities( i, 0, &caps ))) continue;
            if (!memcmp( &caps, &prev_caps[i], sizeof(caps) )) continue;
            prev_caps[i] = caps;
            trace( "%u: caps type %#x, sub %#x, flags %#x, buttons %#x, left %+d, right %+d, lx %+d, ly %+d, rx %+d, ry %+d, left %+d, right %+d\n", i,
                   caps.Type, caps.SubType, caps.Flags, caps.Gamepad.wButtons, caps.Gamepad.bLeftTrigger, caps.Gamepad.bRightTrigger, caps.Gamepad.sThumbLX,
                   caps.Gamepad.sThumbLY, caps.Gamepad.sThumbRX, caps.Gamepad.sThumbRY, caps.Vibration.wLeftMotorSpeed, caps.Vibration.wRightMotorSpeed );
        }

        for (i = 0; i < 16; ++i)
        {
            if ((ret = XInputGetState( i, &state ))) continue;
            if (!memcmp( &state, &prev_state[i], sizeof(state) )) continue;
            prev_state[i] = state;
            trace( "%u: state packet %#x, buttons %#x, left %+d, right %+d, lx %+d, ly %+d, rx %+d, ry %+d, left %+d, right %+d\n", i,
                   state.dwPacketNumber, state.Gamepad.wButtons, state.Gamepad.bLeftTrigger, state.Gamepad.bRightTrigger, state.Gamepad.sThumbLX,
                   state.Gamepad.sThumbLY, state.Gamepad.sThumbRX, state.Gamepad.sThumbRY );
        }

        Sleep(100);
    }

    return 0;
}

static const char *debugstr_adapter_flags( UINT flags )
{
    char buffer[1024] = {0};
    if (flags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) strcat( buffer, "DISPLAY_DEVICE_ATTACHED_TO_DESKTOP " );
    if (flags & DISPLAY_DEVICE_MULTI_DRIVER) strcat( buffer, "DISPLAY_DEVICE_MULTI_DRIVER " );
    if (flags & DISPLAY_DEVICE_PRIMARY_DEVICE) strcat( buffer, "DISPLAY_DEVICE_PRIMARY_DEVICE " );
    if (flags & DISPLAY_DEVICE_MIRRORING_DRIVER) strcat( buffer, "DISPLAY_DEVICE_MIRRORING_DRIVER " );
    if (flags & DISPLAY_DEVICE_VGA_COMPATIBLE) strcat( buffer, "DISPLAY_DEVICE_VGA_COMPATIBLE " );
    if (flags & DISPLAY_DEVICE_REMOVABLE) strcat( buffer, "DISPLAY_DEVICE_REMOVABLE " );
    if (flags & DISPLAY_DEVICE_ACC_DRIVER) strcat( buffer, "DISPLAY_DEVICE_ACC_DRIVER " );
    if (flags & DISPLAY_DEVICE_UNSAFE_MODES_ON) strcat( buffer, "DISPLAY_DEVICE_UNSAFE_MODES_ON " );
    if (flags & DISPLAY_DEVICE_TS_COMPATIBLE) strcat( buffer, "DISPLAY_DEVICE_TS_COMPATIBLE " );
    if (flags & DISPLAY_DEVICE_RDPUDD) strcat( buffer, "DISPLAY_DEVICE_RDPUDD " );
    if (flags & DISPLAY_DEVICE_DISCONNECT) strcat( buffer, "DISPLAY_DEVICE_DISCONNECT " );
    if (flags & DISPLAY_DEVICE_REMOTE) strcat( buffer, "DISPLAY_DEVICE_REMOTE " );
    if (flags & DISPLAY_DEVICE_MODESPRUNED) strcat( buffer, "DISPLAY_DEVICE_MODESPRUNED " );
    return wine_dbg_sprintf( "%s", buffer );
}

static const char *debugstr_monitor_flags( UINT flags )
{
    char buffer[1024] = {0};
    if (flags & DISPLAY_DEVICE_ATTACHED) strcat( buffer, "DISPLAY_DEVICE_ATTACHED " );
    if (flags & DISPLAY_DEVICE_ACTIVE) strcat( buffer, "DISPLAY_DEVICE_ACTIVE " );
    return wine_dbg_sprintf( "%s", buffer );
}

static BOOL CALLBACK enum_monitor( HMONITOR handle, HDC hdc, RECT *rect, LPARAM lparam )
{
    MONITORINFO info = {.cbSize = sizeof(MONITORINFO)};
    UINT dpi_x, dpi_y;

    GetMonitorInfoW( handle, &info );

    trace( "%p %s %#x\n", handle, wine_dbgstr_rect(rect), info.dwFlags );
    trace( "  - monitor: %s\n", wine_dbgstr_rect(&info.rcMonitor) );
    trace( "  - work: %s\n", wine_dbgstr_rect(&info.rcWork) );

    GetDpiForMonitorInternal( handle, 0, &dpi_x, &dpi_y );
    trace( "  - dpi: %d %d\n", dpi_x, dpi_y );

    return TRUE;
}

static const char *debugstr_devmodew(const DEVMODEW *devmode)
{
    char position[32] = {0};

    if (devmode->dmFields & DM_POSITION)
    {
        snprintf(position, sizeof(position), " at (%d,%d)",
                 (int)devmode->dmPosition.x, (int)devmode->dmPosition.y);
    }

    return wine_dbg_sprintf("(%4u,%4u) %2ubits %2uHz rotated %3u degrees %sstretched %sinterlaced%s",
                            (unsigned int)devmode->dmPelsWidth,
                            (unsigned int)devmode->dmPelsHeight,
                            (unsigned int)devmode->dmBitsPerPel,
                            (unsigned int)devmode->dmDisplayFrequency,
                            (unsigned int)devmode->dmDisplayOrientation * 90,
                            devmode->dmDisplayFixedOutput == DMDFO_STRETCH ? "" : "un",
                            devmode->dmDisplayFlags & DM_INTERLACED ? "" : "non-",
                            position);
}

static void list_devmode( const WCHAR *device )
{
    DEVMODEW mode = {.dmSize = sizeof(DEVMODEW)};
    UINT i;

    for (i = 0; EnumDisplaySettingsExW( device, i, &mode, 0 ); ++i)
        trace( "    %3u: %s\n", i, debugstr_devmodew( &mode ) );

    if (EnumDisplaySettingsExW( device, ENUM_REGISTRY_SETTINGS, &mode, 0 ))
        trace( "    reg: %s\n", debugstr_devmodew( &mode ) );
    if (EnumDisplaySettingsExW( device, ENUM_CURRENT_SETTINGS, &mode, 0 ))
        trace( "    cur: %s\n", debugstr_devmodew( &mode ) );
}

static void set_display_settings(void)
{
    DEVMODEW mode = {.dmSize = sizeof(DEVMODEW)};

    mode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_POSITION;
    mode.dmBitsPerPel = 32;
    mode.dmPelsWidth = 800;
    mode.dmPelsHeight = 600;

    ChangeDisplaySettingsExW( NULL, &mode, 0, CDS_UPDATEREGISTRY | CDS_NORESET, NULL );
    ChangeDisplaySettingsExW( NULL, NULL, NULL, 0, NULL );
}

static void list_display_dxgi(void)
{
    IDXGIFactory *factory;
    IDXGIAdapter *adapter;
    IDXGIOutput *output;
    DXGI_ADAPTER_DESC adapter_desc;
    HRESULT hr;

    hr = CreateDXGIFactory( &IID_IDXGIFactory, (void **)&factory );
    assert( hr == S_OK );

    for (int i = 0; (hr = IDXGIFactory_EnumAdapters( factory, i, &adapter )) == S_OK; ++i)
    {
        memset( &adapter_desc, 0, sizeof(adapter_desc) );
        hr = IDXGIAdapter_GetDesc( adapter, &adapter_desc );
        assert( hr == S_OK );

        trace( "%u %s\n", i, debugstr_w( adapter_desc.Description ) );
        trace( "  vid/dev/sub/rev: %#x/%#x/%#x/%#x\n", adapter_desc.VendorId, adapter_desc.DeviceId, adapter_desc.SubSysId, adapter_desc.Revision );
        trace( "  vram/sys/shared: %I64d/%I64d/%I64d (MiB)\n", adapter_desc.DedicatedVideoMemory / 1024 / 1024, adapter_desc.DedicatedSystemMemory / 1024 / 1024, adapter_desc.SharedSystemMemory / 1024 / 1024 );
        trace( "  luid: %#x:%#x\n", adapter_desc.AdapterLuid.HighPart, adapter_desc.AdapterLuid.LowPart );

        for (int j = 0; (hr = IDXGIAdapter_EnumOutputs( adapter, j, &output )) == S_OK; ++j)
        {
            DXGI_OUTPUT_DESC output_desc = {0};
            DXGI_MODE_DESC *modes;
            UINT mode_count = 0;

            hr = IDXGIOutput_GetDesc( output, &output_desc );
            assert( hr == S_OK );

            trace( "  %u %s\n", j, debugstr_w( output_desc.DeviceName ) );
            trace( "    DesktopCoordinates: %s\n", wine_dbgstr_rect(&output_desc.DesktopCoordinates) );
            trace( "    AttachedToDesktop: %u\n", output_desc.AttachedToDesktop );
            trace( "    Rotation: %u\n", output_desc.Rotation );
            trace( "    Monitor: %#x\n", output_desc.Monitor );

            hr = IDXGIOutput_GetDisplayModeList( output, DXGI_FORMAT_R8G8B8A8_UNORM, 0, &mode_count, NULL );
            assert( hr == S_OK );

            modes = calloc( mode_count, sizeof(*modes) );
            assert( modes != NULL );

            hr = IDXGIOutput_GetDisplayModeList( output, DXGI_FORMAT_R8G8B8A8_UNORM, 0, &mode_count, modes );
            assert( hr == S_OK );

            for (int k = 0; k < mode_count; ++k)
                trace( "      %ux%u@%uHz\n", modes[k].Width, modes[k].Height,
                       modes[k].RefreshRate.Numerator / modes[k].RefreshRate.Denominator );

            free( modes );
            IDXGIOutput_Release( output );
        }

        IDXGIAdapter_Release( adapter );
    }

    IDXGIFactory_Release( factory );
}

static void list_display_d3d9(void)
{
    UINT adapter_count;
    IDirect3D9 *d3d9;
    D3DCAPS9 caps;

    d3d9 = Direct3DCreate9( D3D_SDK_VERSION );
    assert( !!d3d9 );

    adapter_count = IDirect3D9_GetAdapterCount( d3d9 );

    for (int i = 0; i < adapter_count; ++i)
    {
        HMONITOR monitor = IDirect3D9_GetAdapterMonitor( d3d9, i );
        IDirect3D9_GetDeviceCaps( d3d9, i, D3DDEVTYPE_HAL, &caps );
        trace( "%u monitor: %p\n", i, monitor );
        trace( "  AdapterOrdinal: %d\n", caps.AdapterOrdinal );
    }

    IDirect3D9_Release( d3d9 );
}

DEFINE_DEVPROPKEY( DEVPROPKEY_GPU_LUID, 0x60b193cb, 0x5276, 0x4d0f, 0x96, 0xfc, 0xf1, 0x73, 0xab,
                   0xad, 0x3e, 0xc6, 2 );
DEFINE_DEVPROPKEY( DEVPROPKEY_MONITOR_GPU_LUID, 0xca085853, 0x16ce, 0x48aa, 0xb1, 0x14, 0xde, 0x9c,
                   0x72, 0x33, 0x42, 0x23, 1 );
DEFINE_DEVPROPKEY( DEVPROPKEY_MONITOR_OUTPUT_ID, 0xca085853, 0x16ce, 0x48aa, 0xb1, 0x14, 0xde, 0x9c,
                   0x72, 0x33, 0x42, 0x23, 2 );

static void list_display_setupapi(void)
{
    HDEVINFO devinfo;
    SP_DEVINFO_DATA device_data = {sizeof(device_data)};
    DWORD type;
    DWORD i = 0;
    LUID luid;
    INT id;
    BOOL ret;

    trace( "GPU:\n" );
    devinfo = SetupDiGetClassDevsW( &GUID_DEVCLASS_DISPLAY, L"PCI", NULL, 0 );
    while (SetupDiEnumDeviceInfo( devinfo, i++, &device_data ))
    {
        BOOL present = FALSE;
        ret = SetupDiGetDevicePropertyW( devinfo, &device_data, &DEVPKEY_Device_IsPresent, &type,
                                         (BYTE *)&present, sizeof(present), NULL, 0 );
        assert( ret );
        if (!present) continue;

        ret = SetupDiGetDevicePropertyW( devinfo, &device_data, &DEVPROPKEY_GPU_LUID, &type,
                                         (BYTE *)&luid, sizeof(luid), NULL, 0 );
        assert( ret );
        trace( "%u luid: %#x:%#x type: %#lx\n", i - 1, luid.HighPart, luid.LowPart, type );
    }
    SetupDiDestroyDeviceInfoList( devinfo );

    trace( "Monitor:\n" );
    devinfo = SetupDiGetClassDevsW( &GUID_DEVCLASS_MONITOR, L"DISPLAY", NULL, 0 );
    i = 0;
    while (SetupDiEnumDeviceInfo( devinfo, i++, &device_data ))
    {
if (0)
{
        BOOL present = FALSE;
        ret = SetupDiGetDevicePropertyW( devinfo, &device_data, &DEVPKEY_Device_IsPresent, &type,
                                         (BYTE *)&present, sizeof(present), NULL, 0 );
        assert( ret );
        if (!present) continue;
}

        ret = SetupDiGetDevicePropertyW( devinfo, &device_data, &DEVPROPKEY_MONITOR_GPU_LUID, &type,
                                         (BYTE *)&luid, sizeof(luid), NULL, 0 );
        assert( ret );
        trace( "%u luid: %#x:%#x type: %#lx\n", i - 1, luid.HighPart, luid.LowPart, type );

        ret = SetupDiGetDevicePropertyW( devinfo, &device_data, &DEVPROPKEY_MONITOR_OUTPUT_ID,
                                         &type, (BYTE *)&id, sizeof(id), NULL, 0 );
        assert( ret );
        trace( "  id: %#x type: %#lx \n", id, type );
    }
    SetupDiDestroyDeviceInfoList( devinfo );
}

static void list_display_d3dkmt(void)
{
    DISPLAY_DEVICEW display_device = {sizeof(display_device)};
    D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME open_desc;
    D3DKMT_CLOSEADAPTER close_desc;
    NTSTATUS status;

    for (int i = 0; EnumDisplayDevicesW( NULL, i, &display_device, 0 ); i++)
    {
        if (!(display_device.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)) continue;

        lstrcpyW( open_desc.DeviceName, display_device.DeviceName );
        status = D3DKMTOpenAdapterFromGdiDisplayName( &open_desc );
        assert( !status );
        trace( "%u luid: %#x:%#x vidsourceid %d\n", i, open_desc.AdapterLuid.HighPart,
                open_desc.AdapterLuid.LowPart, open_desc.VidPnSourceId );
        close_desc.hAdapter = open_desc.hAdapter;
        D3DKMTCloseAdapter( &close_desc );
    }
}

static void list_display(void)
{
    DISPLAY_DEVICEW adapter = {.cb = sizeof(adapter)};
    UINT i, j;

    trace( "primary modes:\n" );
    list_devmode( NULL );

    for (i = 0; EnumDisplayDevicesW( NULL, i, &adapter, 0 ); ++i)
    {
        DISPLAY_DEVICEW monitor = {.cb = sizeof(monitor)};

        trace( "%u %s %s %#x\n", i, debugstr_w(adapter.DeviceName), debugstr_w(adapter.DeviceString), adapter.StateFlags);
        trace( "  - flags: %s\n", debugstr_adapter_flags(adapter.StateFlags) );
        trace( "  - id: %s\n", debugstr_w(adapter.DeviceID) );
        trace( "  - key: %s\n", debugstr_w(adapter.DeviceKey) );

        EnumDisplayDevicesW( NULL, i, &adapter, EDD_GET_DEVICE_INTERFACE_NAME );
        trace( "    - name: %s\n", debugstr_w(adapter.DeviceName) );
        trace( "    - string: %s\n", debugstr_w(adapter.DeviceString) );
        trace( "    - id: %s\n", debugstr_w(adapter.DeviceID) );
        trace( "    - key: %s\n", debugstr_w(adapter.DeviceKey) );
        EnumDisplayDevicesW( NULL, i, &adapter, 0 );

        trace( "  - monitors:\n" );
        for (j = 0; EnumDisplayDevicesW( adapter.DeviceName, j, &monitor, 0 ); ++j)
        {
            trace( "    %u %s %s %#x\n", j, debugstr_w(monitor.DeviceName), debugstr_w(monitor.DeviceString), monitor.StateFlags);
            trace( "      - flags: %s\n", debugstr_monitor_flags(monitor.StateFlags) );
            trace( "      - id: %s\n", debugstr_w(monitor.DeviceID) );
            trace( "      - key: %s\n", debugstr_w(monitor.DeviceKey) );

            EnumDisplayDevicesW( adapter.DeviceName, j, &monitor, EDD_GET_DEVICE_INTERFACE_NAME );
            trace( "          - name: %s\n", debugstr_w(monitor.DeviceName) );
            trace( "          - string: %s\n", debugstr_w(monitor.DeviceString) );
            trace( "          - id: %s\n", debugstr_w(monitor.DeviceID) );
            trace( "          - key: %s\n", debugstr_w(monitor.DeviceKey) );
        }

        trace( "  - modes:\n" );
        list_devmode( adapter.DeviceName );
    }

{
    LARGE_INTEGER time, freq, total = {0};
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&time);
    total.QuadPart -= time.QuadPart;

    for (int i = 0; i < 10000; i++) EnumDisplayDevicesW( NULL, 0, &adapter, 0 );

    QueryPerformanceCounter(&time);
    total.QuadPart += time.QuadPart;
    total.QuadPart *= 10000000 / freq.QuadPart;
    trace("EnumDisplayDevicesW NULL %I64u ns\n", total.QuadPart / 10000);
}

{
    LARGE_INTEGER time, freq, total = {0};
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&time);
    total.QuadPart -= time.QuadPart;

    for (int i = 0; i < 10000; i++) EnumDisplayDevicesW( L"\\\\.\\DISPLAY1", 0, &adapter, 0 );

    QueryPerformanceCounter(&time);
    total.QuadPart += time.QuadPart;
    total.QuadPart *= 10000000 / freq.QuadPart;
    trace("EnumDisplayDevicesW DISPLAY1 %I64u ns\n", total.QuadPart / 10000);
}

    trace( "primary (%d,%d)\n", GetSystemMetrics( SM_CXSCREEN ), GetSystemMetrics( SM_CYSCREEN ) );
    trace( "virtual (%d,%d)-(%d,%d)\n", GetSystemMetrics( SM_XVIRTUALSCREEN ), GetSystemMetrics( SM_YVIRTUALSCREEN ),
           GetSystemMetrics( SM_CXVIRTUALSCREEN ), GetSystemMetrics( SM_CYVIRTUALSCREEN ) );
    trace( "monitors %u\n", GetSystemMetrics( SM_CMONITORS ) );

{
    HDC dc = GetDC( NULL );
    UINT dpi_x = GetDeviceCaps( dc, LOGPIXELSX );
    UINT dpi_y = GetDeviceCaps( dc, LOGPIXELSY );
    UINT res_x = GetDeviceCaps( dc, HORZRES );
    UINT res_y = GetDeviceCaps( dc, VERTRES );
    trace( "res (%d,%d) dpi (%d,%d)\n", res_x, res_y, dpi_x, dpi_y );
    DeleteDC( dc );
}

    /* HKCU\Control Panel\Desktop\Win8DpiScaling */
    /* HKCU\Control Panel\Desktop\DpiScalingVer */
    /* HKCU\Control Panel\Desktop\DesktopDPIOverride */
    /* HKCU\Control Panel\Desktop\LogPixels */
    /* HKCU\Control Panel\Desktop\WindowMetrics\AppliedDPI */
    /* HKCU\Control Panel\Desktop\PerMonitorSettings\NOEDID_XXX\DpiValue */

/*
    get_scale_req.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_DPI_SCALE;
    get_scale_req.header.size = sizeof(get_scale_req);
    get_scale_req.header.adapterId = open_adapter_gdi_desc.AdapterLuid;
    get_scale_req.header.id = open_adapter_gdi_desc.VidPnSourceId;
    DisplayConfigGetDeviceInfo
    DisplayConfigSetDeviceInfo

    GetDpiForMonitorInternal( ... MDT_EFFECTIVE_DPI   = 0, )
    GetDpiForMonitorInternal( ... MDT_ANGULAR_DPI     = 1, )
    GetDpiForMonitorInternal( ... MDT_RAW_DPI         = 2, )
    GetDpiForMonitorInternal( ... MDT_DEFAULT         = MDT_EFFECTIVE_DPI, )
*/

    trace( "system dpi %u\n", GetDpiForSystem() );

{
    DPI_AWARENESS_CONTEXT old = SetThreadDpiAwarenessContext( DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 );
    EnumDisplayMonitors( 0, NULL, enum_monitor, 0 );
    SetThreadDpiAwarenessContext( old );
}

    trace( "dxgi\n" );
    list_display_dxgi();
    trace( "d3d9\n" );
    list_display_d3d9();
    trace( "d3dkmt\n" );
    list_display_d3dkmt();
    trace( "setupapi\n" );
    list_display_setupapi();
}

int WINAPI wWinMain( HINSTANCE instance, HINSTANCE previous, WCHAR *cmdline, int cmdshow )
{
    HHOOK cbt_hook, getmessage_hook, callwndproc_hook, callwndprocret_hook, mouse_hook = NULL, keyboard_hook = NULL;
    LRESULT (*CALLBACK hook_proc)( int code, WPARAM wparam, LPARAM lparam );
    WNDCLASSEXW class = {
        .cbSize = sizeof(WNDCLASSEXW),
        .lpfnWndProc = wintrace_wndproc,
        .lpszClassName = L"wintrace",
        .hInstance = instance,
    };
    HMODULE wintrace_hooks = NULL;
    HWND hwnd;
    BOOL ret;
    MSG msg;

    TRACE("instance %p, previous %p, cmdline %s, cmdshow %d.\n", instance, previous, debugstr_w(cmdline), cmdshow);

    trace_init();
    RegisterClassExW( &class );

    if (wcsstr( cmdline, L"list-hid" ))
    {
        unsigned char buffer[offsetof( SP_DEVICE_INTERFACE_DETAIL_DATA_W, DevicePath[MAX_PATH] )];
        WCHAR device_name[MAX_PATH] = {0}, device_serial[MAX_PATH] = {0};
        SP_DEVICE_INTERFACE_DETAIL_DATA_W *detail = (void *)buffer;
        SP_DEVICE_INTERFACE_DATA iface = {sizeof(iface)};
        SP_DEVINFO_DATA devinfo = {sizeof(devinfo)};
        PHIDP_PREPARSED_DATA preparsed;
        HIDD_ATTRIBUTES attrs;
        HIDP_CAPS caps;
        HANDLE device;
        HDEVINFO set;
        UINT32 i = 0;
        GUID hid;

        HidD_GetHidGuid( &hid );
        set = SetupDiGetClassDevsW( &hid, NULL, NULL, DIGCF_DEVICEINTERFACE );
        assert( set && set != INVALID_HANDLE_VALUE );

        while (SetupDiEnumDeviceInterfaces( set, NULL, &hid, i++, &iface ))
        {
            detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
            if (!SetupDiGetDeviceInterfaceDetailW( set, &iface, detail, sizeof(buffer), NULL, &devinfo ))
                continue;
            if (!wcsstr( detail->DevicePath, L"vid_1209" )) trace( "hid[%u]: %s\n", i - 1, debugstr_w(detail->DevicePath) );
            if (!hid_open( detail->DevicePath, &device, &preparsed, &attrs, &caps )) continue;

            HidD_GetProductString( device, device_name, sizeof(device_name) );
            HidD_GetSerialNumberString( device, device_serial, sizeof(device_serial) );

            trace( "    ids: %04x:%04x rev %u\n", attrs.VendorID, attrs.ProductID, attrs.VersionNumber );
            trace( "    name: %s\n", debugstr_w(device_name) );
            trace( "    serial: %s\n", debugstr_w(device_serial) );

            HidD_FreePreparsedData( preparsed );
            CloseHandle( device );
        }

        SetupDiDestroyDeviceInfoList( set );
    }

    if (wcsstr( cmdline, L"list-raw" ))
    {
        RID_DEVICE_INFO info = {.cbSize = sizeof(info)};
        UINT i, size, count, device_count;
        RAWINPUTDEVICELIST devices[64];
        WCHAR device_path[MAX_PATH];

        count = ARRAY_SIZE(devices);
        device_count = GetRawInputDeviceList( devices, &count, sizeof(*devices) );
        trace( "device_count %u count %u\n", device_count, count );

        for (i = 0; i < device_count; i++)
        {
            HANDLE handle;

            size = sizeof(device_path);
            ret = GetRawInputDeviceInfoW( devices[i].hDevice, RIDI_DEVICENAME, device_path, &size );
            size = sizeof(info);
            ret = GetRawInputDeviceInfoW( devices[i].hDevice, RIDI_DEVICEINFO, &info, &size );

            if (info.dwType != RIM_TYPEHID) continue;

            trace( "raw[%u]: %u %04x:%04x rev %u usage %04x:%04x\n", i, info.dwType, info.hid.dwVendorId, info.hid.dwProductId,
                   info.hid.dwVersionNumber, info.hid.usUsagePage, info.hid.usUsage );
            trace( "    path: %s\n", debugstr_w(device_path) );

            handle = CreateFileW( device_path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  NULL, OPEN_EXISTING, 0, NULL );
            trace( "    handle: %p\n", handle );
            CloseHandle( handle );
        }
    }

    if (wcsstr( cmdline, L"list-dinput" ))
    {
        IDirectInput8W *dinput;
        HRESULT hr;

        hr = DirectInput8Create( instance, DIRECTINPUT_VERSION, &IID_IDirectInput8W, (void *)&dinput, NULL );
        assert( hr == DI_OK );
        hr = IDirectInput8_EnumDevices( dinput, 0, dinput_list, dinput, DIEDFL_ATTACHEDONLY );
        assert( hr == DI_OK );
        IDirectInput8_Release( dinput );
    }

    if (wcsstr( cmdline, L"list-xinput" ))
    {
        typedef struct
        {
            XINPUT_CAPABILITIES Capabilities;
            WORD VendorId;
            WORD ProductId;
            WORD VersionNumber;
            WORD unk1;
            DWORD unk2;
        } XINPUT_CAPABILITIES_EX;
        DWORD (*WINAPI pXInputGetCapabilitiesEx)(DWORD unk, DWORD index, DWORD flags, XINPUT_CAPABILITIES_EX *caps);
        DWORD (*WINAPI pXInputGetState)(DWORD index, XINPUT_STATE *state);

        HMODULE xinput1_4 = LoadLibraryW( L"xinput1_4" );
        UINT i;

        trace("xinput1_4 %p\n", xinput1_4);
        pXInputGetState = (void *)GetProcAddress( xinput1_4, "XInputGetState" );

        if ((pXInputGetCapabilitiesEx = (void *)GetProcAddress( xinput1_4, (const char *)108 )))
        {
            trace("pXInputGetCapabilitiesEx %p\n", pXInputGetCapabilitiesEx);

            for (i = 0; i < XUSER_MAX_COUNT; i++)
            {
                XINPUT_CAPABILITIES_EX caps = {0};
                XINPUT_STATE state = {0};

                ret = pXInputGetState( i, &state );
                ret = pXInputGetCapabilitiesEx( 1, i, 0, &caps );
                trace( "xinput[%u]: %d %04x:%04x rev %u unk %u %u\n", i, ret, caps.VendorId, caps.ProductId, caps.VersionNumber, caps.unk1, caps.unk2 );
            }
        }
    }

    if (wcsstr( cmdline, L"list-wgi" ))
    {
        static const WCHAR *controller_class_name = RuntimeClass_Windows_Gaming_Input_RawGameController;
        IRawGameControllerStatics *controller_statics;
        IVectorView_RawGameController *controllers;
        UINT i, size;
        HSTRING str;
        HRESULT hr;

        hr = RoInitialize( RO_INIT_MULTITHREADED );
        assert( hr == S_OK );
        hr = WindowsCreateString( controller_class_name, wcslen( controller_class_name ), &str );
        assert( hr == S_OK );
        hr = RoGetActivationFactory( str, &IID_IRawGameControllerStatics, (void **)&controller_statics );
        assert( hr == S_OK );
        WindowsDeleteString( str );

        hr = IRawGameControllerStatics_get_RawGameControllers( controller_statics, &controllers );
        assert( hr == S_OK );

        hr = IVectorView_RawGameController_get_Size( controllers, &size );
        assert( hr == S_OK );
        for (i = 0; i < size; i++)
        {
            IRawGameController2 *controller2;
            IRawGameController *controller;
            UINT16 vid, pid;
            HSTRING name;

            hr = IVectorView_RawGameController_GetAt( controllers, i, &controller );
            assert( hr == S_OK );

            hr = IRawGameController_get_HardwareVendorId( controller, &vid );
            assert( hr == S_OK );
            hr = IRawGameController_get_HardwareProductId( controller, &pid );
            assert( hr == S_OK );


            hr = IRawGameController_QueryInterface( controller, &IID_IRawGameController2, (void **)&controller2 );
            assert( hr == S_OK );
            hr = IRawGameController2_get_DisplayName( controller2, &name );
            assert( hr == S_OK );
            trace( "wgi[%u]: %04x:%04x %s\n", i, vid, pid, debugstr_hstring(name) );
            WindowsDeleteString( name );

            hr = IRawGameController2_get_NonRoamableId( controller2, &name );
            assert( hr == S_OK );
            trace( "    id: %s\n", debugstr_hstring(name) );
            WindowsDeleteString( name );

            IRawGameController_Release( controller );
        }

        IVectorView_RawGameController_Release( controllers );
        IRawGameControllerStatics_Release( controller_statics );
    }

    if (wcsstr( cmdline, L"set-display" )) set_display_settings();
    if (wcsstr( cmdline, L"list-display" )) list_display();

    if (wcsstr( cmdline, L"list-" )) exit( 0 );

    hwnd = CreateWindowW( L"wintrace", L"wintrace", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                          200, 200, 0, 0, instance, NULL );
    if (!wcsstr( cmdline, L"windowed" ))
    {
        HMONITOR hmonitor = MonitorFromWindow( hwnd, MONITOR_DEFAULTTOPRIMARY );
        MONITORINFO mi = {.cbSize = sizeof(MONITORINFO)};

        GetMonitorInfoW( hmonitor, &mi );
        /* SetWindowLongW( hwnd, GWL_STYLE, GetWindowLongW( hwnd, GWL_STYLE ) & ~WS_OVERLAPPEDWINDOW ); */
        SetWindowPos( hwnd, 0, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                      mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_FRAMECHANGED );
    }
    ShowWindow( hwnd, cmdshow );

    if (wcsstr( cmdline, L"verbose" )) verbose = TRUE;

    if (!wcsstr( cmdline, L"hook:cbt" )) cbt_hook = NULL;
    else
    {
        wintrace_hooks = LoadLibraryW( L"wintrace_hooks" );
        hook_proc = (void *)GetProcAddress( wintrace_hooks, "hook_cbt" );
        cbt_hook = SetWindowsHookExW( WH_CBT, hook_proc, wintrace_hooks, 0 );
        assert( cbt_hook );
    }

    if (!wcsstr( cmdline, L"hook:getmessage" )) getmessage_hook = NULL;
    else
    {
        wintrace_hooks = LoadLibraryW( L"wintrace_hooks" );
        hook_proc = (void *)GetProcAddress( wintrace_hooks, "hook_getmessage" );
        getmessage_hook = SetWindowsHookExW( WH_GETMESSAGE, hook_proc, wintrace_hooks, 0 );
        assert( getmessage_hook );
    }

    if (!wcsstr( cmdline, L"hook:callwndproc" )) callwndproc_hook = callwndprocret_hook = NULL;
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

    if (wcsstr( cmdline, L"hook:ll" ))
    {
        mouse_hook = SetWindowsHookExW( WH_MOUSE_LL, ll_hook, NULL, 0 );
        assert( mouse_hook );
        keyboard_hook = SetWindowsHookExW( WH_KEYBOARD_LL, ll_hook, NULL, 0 );
        assert( keyboard_hook );
    }

    if (wcsstr( cmdline, L"hid" ))
    {
        unsigned char buffer[offsetof( SP_DEVICE_INTERFACE_DETAIL_DATA_W, DevicePath[MAX_PATH] )];
        SP_DEVICE_INTERFACE_DETAIL_DATA_W *detail = (void *)buffer;
        SP_DEVICE_INTERFACE_DATA iface = {sizeof(iface)};
        SP_DEVINFO_DATA devinfo = {sizeof(devinfo)};
        PHIDP_PREPARSED_DATA preparsed;
        HIDD_ATTRIBUTES attrs;
        HANDLE device, thread;
        HIDP_CAPS caps;
        HDEVINFO set;
        UINT32 i = 0;
        GUID hid;

        HidD_GetHidGuid( &hid );
        set = SetupDiGetClassDevsW( &hid, NULL, NULL, DIGCF_DEVICEINTERFACE );
        assert( set && set != INVALID_HANDLE_VALUE );

        while (SetupDiEnumDeviceInterfaces( set, NULL, &hid, i++, &iface ))
        {
            detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
            if (!SetupDiGetDeviceInterfaceDetailW( set, &iface, detail, sizeof(buffer), NULL, &devinfo ))
                continue;
            if (!hid_open( detail->DevicePath, &device, &preparsed, &attrs, &caps )) continue;

            thread = CreateThread( NULL, 0, hid_thread, device, 0, NULL );
            assert( thread );
            CloseHandle( thread );

            HidD_FreePreparsedData( preparsed );
        }

        SetupDiDestroyDeviceInfoList( set );
    }

    if (wcsstr( cmdline, L"raw" ))
    {
        RAWINPUTDEVICE dev[4];

        dev[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
        dev[0].usUsage = HID_USAGE_GENERIC_MOUSE;
        dev[0].dwFlags = (wcsstr( cmdline, L"nolegacy" ) ? RIDEV_NOLEGACY : 0) |
                         (wcsstr( cmdline, L"sink" ) ? RIDEV_INPUTSINK : 0) |
                         (wcsstr( cmdline, L"notify" ) ? RIDEV_DEVNOTIFY : 0);
        dev[0].hwndTarget = hwnd;

        dev[1].usUsagePage = HID_USAGE_PAGE_GENERIC;
        dev[1].usUsage = HID_USAGE_GENERIC_KEYBOARD;
        dev[1].dwFlags = (wcsstr( cmdline, L"nolegacy" ) ? RIDEV_NOLEGACY : 0) |
                         (wcsstr( cmdline, L"sink" ) ? RIDEV_INPUTSINK : 0) |
                         (wcsstr( cmdline, L"notify" ) ? RIDEV_DEVNOTIFY : 0);
        dev[1].hwndTarget = hwnd;

        dev[2].usUsagePage = HID_USAGE_PAGE_GENERIC;
        dev[2].usUsage = HID_USAGE_GENERIC_JOYSTICK;
        dev[2].dwFlags = (wcsstr( cmdline, L"sink" ) ? RIDEV_INPUTSINK : 0) |
                         (wcsstr( cmdline, L"notify" ) ? RIDEV_DEVNOTIFY : 0);
        dev[2].hwndTarget = hwnd;

        dev[3].usUsagePage = HID_USAGE_PAGE_GENERIC;
        dev[3].usUsage = HID_USAGE_GENERIC_GAMEPAD;
        dev[3].dwFlags = (wcsstr( cmdline, L"sink" ) ? RIDEV_INPUTSINK : 0) |
                         (wcsstr( cmdline, L"notify" ) ? RIDEV_DEVNOTIFY : 0);
        dev[3].hwndTarget = hwnd;

        ret = RegisterRawInputDevices( dev, ARRAYSIZE( dev ), sizeof(dev[0]) );
        assert( ret );
    }

    if (wcsstr( cmdline, L"dinput" ))
    {
        static const DIPROPDWORD buffer_size =
        {
            .diph =
            {
                .dwHeaderSize = sizeof(DIPROPHEADER),
                .dwSize = sizeof(DIPROPDWORD),
                .dwHow = DIPH_DEVICE,
                .dwObj = 0,
            },
            .dwData = UINT_MAX,
        };
        IDirectInputDevice8W *device;
        IDirectInput8W *dinput;
        HANDLE thread;
        HRESULT hr;

        hr = DirectInput8Create( instance, DIRECTINPUT_VERSION, &IID_IDirectInput8W, (void *)&dinput, NULL );
        assert( hr == DI_OK );

        hr = IDirectInput8_CreateDevice( dinput, &GUID_SysMouse, &device, NULL );
        assert( hr == DI_OK );
        hr = IDirectInputDevice8_SetDataFormat( device, &c_dfDIMouse2 );
        assert( hr == DI_OK );
        hr = IDirectInputDevice8_SetCooperativeLevel( device, hwnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND );
        assert( hr == DI_OK );
        hr = IDirectInputDevice8_SetProperty( device, DIPROP_BUFFERSIZE, &buffer_size.diph );
        assert( hr == DI_OK );
        thread = CreateThread( NULL, 0, dinput_thread, device, 0, NULL );
        assert( thread );
        CloseHandle( thread );

        hr = IDirectInput8_CreateDevice( dinput, &GUID_SysKeyboard, &device, NULL );
        assert( hr == DI_OK );
        hr = IDirectInputDevice8_SetDataFormat( device, &c_dfDIKeyboard );
        assert( hr == DI_OK );
        hr = IDirectInputDevice8_SetCooperativeLevel( device, hwnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND );
        assert( hr == DI_OK );
        hr = IDirectInputDevice8_SetProperty( device, DIPROP_BUFFERSIZE, &buffer_size.diph );
        assert( hr == DI_OK );
        thread = CreateThread( NULL, 0, dinput_thread, device, 0, NULL );
        assert( thread );
        CloseHandle( thread );

        hr = IDirectInput8_EnumDevices( dinput, 0, dinput_open, dinput, DIEDFL_ATTACHEDONLY );
        assert( hr == DI_OK );

        IDirectInput8_Release( dinput );
    }

    if (wcsstr( cmdline, L"xinput" ))
    {
        HANDLE thread = CreateThread( NULL, 0, xinput_thread, NULL, 0, NULL );
        assert( thread );
        CloseHandle( thread );
    }

    if (wcsstr( cmdline, L"touch" )) RegisterTouchWindow( hwnd, 0 );

    trace( "desktop %p, foreground %p\n", GetDesktopWindow(), GetForegroundWindow() );

    while (GetMessageW( &msg, 0, 0, 0 ))
    {
        TranslateMessage( &msg );
        DispatchMessageW( &msg );
    }

    if (keyboard_hook) UnhookWindowsHookEx( keyboard_hook );
    if (mouse_hook) UnhookWindowsHookEx( mouse_hook );
    if (callwndprocret_hook) UnhookWindowsHookEx( callwndprocret_hook );
    if (callwndproc_hook) UnhookWindowsHookEx( callwndproc_hook );
    if (getmessage_hook) UnhookWindowsHookEx( getmessage_hook );
    if (cbt_hook) UnhookWindowsHookEx( cbt_hook );
    if (wintrace_hooks) FreeLibrary( wintrace_hooks );

    DestroyWindow( hwnd );
    UnregisterClassW( L"wintrace", instance );
    return 0;
}
