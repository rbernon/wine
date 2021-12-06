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

LRESULT CALLBACK hcbt_hook( int code, WPARAM wparam, LPARAM lparam );

#endif /* __WINE_WINHOOKS_H */
