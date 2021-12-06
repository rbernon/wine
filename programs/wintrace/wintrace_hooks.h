#ifndef __WINE_WINHOOKS_H
#define __WINE_WINHOOKS_H

#include <windef.h>
#include <winbase.h>
#include <winuser.h>

struct hcbt_create_window_data
{
    UINT64 create_params;
    UINT64 instance;
    UINT64 menu;
    UINT64 window;
    UINT64 parent_window;
    UINT64 insert_after;
    RECT rect;
    DWORD style;
    DWORD exstyle;
    WCHAR window_name[32];
    WCHAR class_name[32];
    WCHAR module_name[32];
};

struct hcbt_destroy_window_data
{
    UINT64 window;
    WCHAR module_name[32];
};

struct hcbt_activate_data
{
    UINT64 curr_active;
    UINT64 next_active;
    UINT64 with_mouse;
    WCHAR module_name[32];
};

struct hcbt_set_focus_data
{
    UINT64 curr_focus;
    UINT64 next_focus;
    WCHAR module_name[32];
};

struct hcbt_min_max_data
{
    UINT64 window;
    UINT64 operation;
    WCHAR module_name[32];
};

struct hcbt_move_size_data
{
    UINT64 window;
    RECT rect;
    WCHAR module_name[32];
};

#define HCBT_GETMESSAGE     0x10000
#define HCBT_CALLWNDPROC    0x10002
#define HCBT_CALLWNDPROCRET 0x10001

struct hcbt_get_message_data
{
    UINT64 window;
    UINT64 message;
    UINT64 lparam;
    UINT64 wparam;
    WCHAR module_name[32];
};

struct hcbt_call_window_proc_data
{
    UINT64 window;
    UINT64 message;
    UINT64 lparam;
    UINT64 wparam;
    WCHAR module_name[32];
};

struct hcbt_call_window_proc_ret_data
{
    UINT64 window;
    UINT64 message;
    UINT64 lparam;
    UINT64 wparam;
    UINT64 lresult;
    WCHAR module_name[32];
};

#endif /* __WINE_WINHOOKS_H */
