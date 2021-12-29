/*
 * Copyright 2017 Hugh McMaster
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

#include <stdio.h>
#include "reg.h"

static HANDLE get_file_handle(WCHAR *filename)
{
    HANDLE file = CreateFileW(filename, GENERIC_READ, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (file == INVALID_HANDLE_VALUE)
    {
        output_message(STRING_CANCELLED);
        exit(0);
    }

    return file;
}

static BOOL set_privileges(LPCSTR privilege, BOOL set)
{
    TOKEN_PRIVILEGES tp;
    HANDLE token;
    LUID luid;
    BOOL ret = FALSE;

    if(!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token)) return FALSE;
    if(!LookupPrivilegeValueA(NULL, privilege, &luid)) goto done;

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    if (set) tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    else tp.Privileges[0].Attributes = 0;

    AdjustTokenPrivileges(token, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL);
    if (GetLastError() != ERROR_SUCCESS) goto done;

    ret = TRUE;

done:
    CloseHandle(token);
    return ret;
}

int reg_load(int argc, WCHAR *argvW[])
{
    HKEY root, hkey;
    REGSAM sam = 0;
    HANDLE file;
    WCHAR *path;
    int i, ret;

    if (argc < 4) goto invalid;

    if (!parse_registry_key(argvW[2], &root, &path))
        return 1;

    for (i = 4; i < argc; i++)
    {
        WCHAR *str;

        if (argvW[i][0] != '/' && argvW[i][0] != '-')
            goto invalid;

        str = &argvW[i][1];

        if (!lstrcmpiW(str, L"reg:32"))
        {
            if (sam & KEY_WOW64_32KEY) goto invalid;
            sam |= KEY_WOW64_32KEY;
            continue;
        }
        else if (!lstrcmpiW(str, L"reg:64"))
        {
            if (sam & KEY_WOW64_64KEY) goto invalid;
            sam |= KEY_WOW64_64KEY;
            continue;
        }
        else
            goto invalid;
    }

    if (sam == (KEY_WOW64_32KEY|KEY_WOW64_64KEY))
        goto invalid;

    set_privileges(SE_RESTORE_NAME, TRUE);

    if (RegOpenKeyExW(root, path, 0, KEY_WRITE|sam, &hkey))
    {
        output_message(STRING_KEY_NONEXIST);
        return 1;
    }

    file = get_file_handle(argvW[3]);
    CloseHandle(file);

    ret = RegLoadKeyW(hkey, argvW[3], 0);
    RegCloseKey(hkey);

    return ret;

invalid:
    output_message(STRING_INVALID_SYNTAX);
    output_message(STRING_FUNC_HELP, wcsupr(argvW[1]));
    return 1;
}
