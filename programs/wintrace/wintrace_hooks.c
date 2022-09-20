#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <windef.h>
#include <winbase.h>
#include <winuser.h>

#include <wine/debug.h>

#include "wintrace_hooks.h"

static HWND wintrace;

LRESULT CALLBACK hook_cbt( int code, WPARAM wparam, LPARAM lparam )
{
    WCHAR buffer[MAX_PATH];

    if (!IsWindow( wintrace )) wintrace = FindWindowW( L"wintrace", 0 );

    switch (code)
    {
    case HCBT_CREATEWND:
    {
        CBT_CREATEWNDW *data = (CBT_CREATEWNDW *)lparam;
        struct hcbt_create_window_data info =
        {
            .create_params = (UINT_PTR)data->lpcs->lpCreateParams,
            .instance = (UINT_PTR)data->lpcs->hInstance,
            .menu = (UINT_PTR)data->lpcs->hMenu,
            .window = wparam,
            .parent_window = (UINT_PTR)data->lpcs->hwndParent,
            .insert_after = (UINT_PTR)data->hwndInsertAfter,
            .rect.left = data->lpcs->x,
            .rect.top = data->lpcs->y,
            .rect.right = data->lpcs->x + data->lpcs->cx,
            .rect.bottom = data->lpcs->y + data->lpcs->cy,
            .style = data->lpcs->style,
            .exstyle = data->lpcs->dwExStyle,
        };
        COPYDATASTRUCT copy = {.dwData = GetCurrentThreadId(), .cbData = sizeof(info), .lpData = &info};

        if (!IS_INTRESOURCE(data->lpcs->lpszName)) lstrcpynW( info.window_name, data->lpcs->lpszName, 32 );
        if (!IS_INTRESOURCE(data->lpcs->lpszClass)) lstrcpynW( info.class_name, data->lpcs->lpszClass, 32 );
        GetModuleFileNameW( GetModuleHandleW( NULL ), buffer, MAX_PATH );
        lstrcpynW( info.module_name, wcsrchr( buffer, '\\' ) + 1, 32 );
        SendMessageW( wintrace, WM_COPYDATA, code, (LPARAM)&copy );
        break;
    }

    case HCBT_DESTROYWND:
    {
        struct hcbt_destroy_window_data info = {.window = wparam};
        COPYDATASTRUCT copy = {.dwData = GetCurrentThreadId(), .cbData = sizeof(info), .lpData = &info};
        GetModuleFileNameW( GetModuleHandleW( NULL ), buffer, MAX_PATH );
        lstrcpynW( info.module_name, wcsrchr( buffer, '\\' ) + 1, 32 );
        SendMessageW( wintrace, WM_COPYDATA, code, (LPARAM)&copy );
        break;
    }

    case HCBT_ACTIVATE:
    {
        CBTACTIVATESTRUCT *data = (CBTACTIVATESTRUCT *)lparam;
        struct hcbt_activate_data info =
        {
            .next_active = wparam,
            .curr_active = (UINT_PTR)data->hWndActive,
            .with_mouse = data->fMouse
        };
        COPYDATASTRUCT copy = {.dwData = GetCurrentThreadId(), .cbData = sizeof(info), .lpData = &info};
        GetModuleFileNameW( GetModuleHandleW( NULL ), buffer, MAX_PATH );
        lstrcpynW( info.module_name, wcsrchr( buffer, '\\' ) + 1, 32 );
        SendMessageW( wintrace, WM_COPYDATA, code, (LPARAM)&copy );
        break;
    }

    case HCBT_SETFOCUS:
    {
        struct hcbt_set_focus_data info = {.next_focus = wparam, .curr_focus = lparam};
        COPYDATASTRUCT copy = {.dwData = GetCurrentThreadId(), .cbData = sizeof(info), .lpData = &info};
        GetModuleFileNameW( GetModuleHandleW( NULL ), buffer, MAX_PATH );
        lstrcpynW( info.module_name, wcsrchr( buffer, '\\' ) + 1, 32 );
        SendMessageW( wintrace, WM_COPYDATA, code, (LPARAM)&copy );
        break;
    }

    case HCBT_MINMAX:
    {
        struct hcbt_min_max_data info = {.window = wparam, .operation = lparam};
        COPYDATASTRUCT copy = {.dwData = GetCurrentThreadId(), .cbData = sizeof(info), .lpData = &info};
        GetModuleFileNameW( GetModuleHandleW( NULL ), buffer, MAX_PATH );
        lstrcpynW( info.module_name, wcsrchr( buffer, '\\' ) + 1, 32 );
        SendMessageW( wintrace, WM_COPYDATA, code, (LPARAM)&copy );
        break;
    }

    case HCBT_MOVESIZE:
    {
        struct hcbt_move_size_data info = {.window = wparam, .rect = *(RECT *)lparam};
        COPYDATASTRUCT copy = {.dwData = GetCurrentThreadId(), .cbData = sizeof(info), .lpData = &info};
        GetModuleFileNameW( GetModuleHandleW( NULL ), buffer, MAX_PATH );
        lstrcpynW( info.module_name, wcsrchr( buffer, '\\' ) + 1, 32 );
        SendMessageW( wintrace, WM_COPYDATA, code, (LPARAM)&copy );
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
        break;

    case HCBT_KEYSKIPPED:
        /*
        Specifies the virtual-key code.

        Specifies the repeat count, scan code, key-transition code,
        previous key state, and context code. The HCBT_KEYSKIPPED value
        is sent to a CBTProc hook procedure only if a WH_KEYBOARD hook
        is installed. For more information, see WM_KEYUP or
        WM_KEYDOWN.
        */
        break;

    case HCBT_QS:
        /*
        Is undefined and must be zero.

        Is undefined and must be zero.
        */
        break;

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
        break;

    default:
        break;
    }

    return CallNextHookEx( 0, code, wparam, lparam );
}

LRESULT CALLBACK hook_getmessage( int code, WPARAM wparam, LPARAM lparam )
{
    MSG *params = (MSG *)lparam;
    WCHAR buffer[MAX_PATH];

    if (!IsWindow( wintrace )) wintrace = FindWindowW( L"wintrace", 0 );

    if (code == HC_ACTION)
    {
        struct hcbt_get_message_data info = {.window = PtrToUlong(params->hwnd), .message = params->message, .wparam = params->wParam, .lparam = params->lParam};
        COPYDATASTRUCT copy = {.dwData = GetCurrentThreadId(), .cbData = sizeof(info), .lpData = &info};
        GetModuleFileNameW( GetModuleHandleW( NULL ), buffer, MAX_PATH );
        lstrcpynW( info.module_name, wcsrchr( buffer, '\\' ) + 1, 32 );
        if (wcscmp( info.module_name, L"wintrace.exe" ) && wcscmp( info.module_name, L"wintrace_a.exe" )) SendMessageW( wintrace, WM_COPYDATA, HCBT_GETMESSAGE, (LPARAM)&copy );
    }

    return CallNextHookEx( 0, code, wparam, lparam );
}

LRESULT CALLBACK hook_callwndproc( int code, WPARAM wparam, LPARAM lparam )
{
    CWPSTRUCT *params = (CWPSTRUCT *)lparam;
    WCHAR buffer[MAX_PATH];

    if (!IsWindow( wintrace )) wintrace = FindWindowW( L"wintrace", 0 );

    if (code == HC_ACTION)
    {
        struct hcbt_call_window_proc_data info = {.window = PtrToUlong(params->hwnd), .message = params->message, .wparam = params->wParam, .lparam = params->lParam};
        COPYDATASTRUCT copy = {.dwData = GetCurrentThreadId(), .cbData = sizeof(info), .lpData = &info};
        GetModuleFileNameW( GetModuleHandleW( NULL ), buffer, MAX_PATH );
        lstrcpynW( info.module_name, wcsrchr( buffer, '\\' ) + 1, 32 );
        if (wcscmp( info.module_name, L"wintrace.exe" ) && wcscmp( info.module_name, L"wintrace_a.exe" )) SendMessageW( wintrace, WM_COPYDATA, HCBT_CALLWNDPROC, (LPARAM)&copy );
    }

    return CallNextHookEx( 0, code, wparam, lparam );
}

LRESULT CALLBACK hook_callwndprocret( int code, WPARAM wparam, LPARAM lparam )
{
    CWPRETSTRUCT *params = (CWPRETSTRUCT *)lparam;
    WCHAR buffer[MAX_PATH];

    if (!IsWindow( wintrace )) wintrace = FindWindowW( L"wintrace", 0 );

    if (code == HC_ACTION)
    {
        struct hcbt_call_window_proc_ret_data info = {.window = PtrToUlong(params->hwnd), .message = params->message, .wparam = params->wParam, .lparam = params->lParam, .lresult = params->lResult};
        COPYDATASTRUCT copy = {.dwData = GetCurrentThreadId(), .cbData = sizeof(info), .lpData = &info};
        GetModuleFileNameW( GetModuleHandleW( NULL ), buffer, MAX_PATH );
        lstrcpynW( info.module_name, wcsrchr( buffer, '\\' ) + 1, 32 );
        if (wcscmp( info.module_name, L"wintrace.exe" ) && wcscmp( info.module_name, L"wintrace_a.exe" )) SendMessageW( wintrace, WM_COPYDATA, HCBT_CALLWNDPROCRET, (LPARAM)&copy );
    }

    return CallNextHookEx( 0, code, wparam, lparam );
}
