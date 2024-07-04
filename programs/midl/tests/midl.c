/*
 * Copyright 2022 Rémi Bernon for CodeWeavers
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

#include <windef.h>
#include <winbase.h>
#include <winreg.h>

#include "wine/test.h"
#include "wine/list.h"

static WCHAR midl_path[MAX_PATH];

static BOOL midl_test_init(void)
{
    const char *env = getenv( "WINEDLLOVERRIDES" );
    WCHAR path[MAX_PATH], version[MAX_PATH], *tmp;
    DWORD i, res, type, size;
    HKEY key;

    res = RegOpenKeyW( HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows Kits\\Installed Roots", &key );
    if (!res) for (i = 0; !RegEnumKeyW( key, i, version, sizeof(version) ); i++)
    {
        size = sizeof(path);
        res = RegQueryValueExW( key, L"KitsRoot10", NULL, &type, (BYTE *)path, &size );
        ok( !res, "got res %#lx\n", res );
        swprintf( midl_path, MAX_PATH, L"%sbin\\%s\\x86\\midl.exe", path, version );
        if (GetFileAttributesW( midl_path ) != INVALID_FILE_ATTRIBUTES) goto found;
    }

    wcscpy( midl_path, L"c:\\Program Files (x86)\\Windows Kits\\8.1\\bin\\x86\\midl.exe" );
    if (GetFileAttributesW( midl_path ) != INVALID_FILE_ATTRIBUTES) goto found;
    wcscpy( midl_path, L"c:\\Program Files (x86)\\Microsoft SDKs\\Windows\v7.1a\\bin\\midl.exe" );
    if (GetFileAttributesW( midl_path ) != INVALID_FILE_ATTRIBUTES) goto found;

    wcscpy( midl_path, L"z:\\home\\rbernon\\Code\\windows-kits\\10\\bin\\10.0.20348.0\\x64\\midl.exe" );
    if (strstr( env, "midl.exe=n" ) && GetFileAttributesW( midl_path ) != INVALID_FILE_ATTRIBUTES) goto found;

    wcscpy( midl_path, L"c:\\Windows\\system32\\midl.exe" );
    if (GetFileAttributesW( midl_path ) != INVALID_FILE_ATTRIBUTES) goto found;

    wcscpy( midl_path, L"z:\\home\\rbernon\\Code\\windows-kits\\10\\bin\\10.0.20348.0\\x64\\midl.exe" );
    if (GetFileAttributesW( midl_path ) != INVALID_FILE_ATTRIBUTES) goto found;

    RegCloseKey( key );
    return FALSE;

found:
    RegCloseKey( key );
    if ((tmp = wcsrchr( midl_path, '\\' ))) *tmp = 0;
    return TRUE;
}

static void write_file( const WCHAR *filename, const char *source )
{
    HANDLE file;
    DWORD size;
    BOOL ret;

    file = CreateFileW( filename, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
    ok( file != INVALID_HANDLE_VALUE, "CreateFileW failed, error %lu\n", GetLastError() );

    ret = WriteFile( file, source, strlen( source ), &size, NULL );
    ok( ret, "WriteFile failed, error %lu\n", GetLastError() );
    ok( size == strlen( source ), "got size %lu\n", size );

    CloseHandle( file );
}

enum midl_flags
{
    MIDL_VERBOSE = 1,
    MIDL_VERSION = 2,
};

static DWORD check_idl( const char *source, enum midl_flags flags )
{
    WCHAR temp[MAX_PATH], cmd[5 * MAX_PATH], path[MAX_PATH], dir[MAX_PATH];
    STARTUPINFOW si = {.cb = sizeof(STARTUPINFOW)};
    PROCESS_INFORMATION pi = {0};
    DWORD res, exit_code;
    BOOL ret;

    if (flags & MIDL_VERSION) trace( "Using MIDL from %s\n", debugstr_w(midl_path) );

    GetTempPathW( ARRAY_SIZE(temp), temp );
    GetTempFileNameW( temp, L"midl", 0, temp );

    swprintf( dir, ARRAY_SIZE(dir), L"%s.dir", temp );
    CreateDirectoryW( dir, NULL );

    swprintf( path, ARRAY_SIZE(path), L"%s\\main.idl", dir );
    write_file( path, source );

    swprintf( path, ARRAY_SIZE(path), L"%s\\midl.exe", midl_path );
    swprintf( cmd, ARRAY_SIZE(cmd), L"\"%s\" main.idl -nocpp -syntax_check", path );
    if (!(flags & MIDL_VERSION)) wcscat( cmd, L" -nologo" );

    if (flags & MIDL_VERBOSE) trace( "Running %s\n", debugstr_w(cmd) );
    ret = CreateProcessW( NULL, cmd, NULL, NULL, FALSE, 0, NULL, dir, &si, &pi );
    ok( ret, "CreateProcessW failed, error %lu\n", GetLastError() );

    res = WaitForSingleObject( pi.hProcess, 1000 );
    ok( !res, "WaitForSingleObject returned %#lx\n", res );
    ret = GetExitCodeProcess( pi.hProcess, &exit_code );
    ok( ret, "GetExitCodeProcess failed, error %lu\n", GetLastError() );

    CloseHandle( pi.hThread );
    CloseHandle( pi.hProcess );

    swprintf( path, ARRAY_SIZE(path), L"%s\\main.idl", dir );
    DeleteFileW( path );

    RemoveDirectoryW( dir );
    DeleteFileW( temp );

    return exit_code;
}

static void test_idl_parsing(void)
{
    const char *src;
    DWORD res;

    /* check basic success */
    src = "interface I;";
    res = check_idl( src, MIDL_VERBOSE | MIDL_VERSION );
    todo_wine
    ok( !res, "MIDL failed, error %#lx\n", res );

    /* check basic failure */
    src = "interface {}";
    res = check_idl( src, 0 );
    ok( res, "MIDL succeeded\n" );

    /* colon is sometimes optional */
    src = "[local]interface I1 {}\n"
          "[local]interface I2 {};\n"
          "[uuid(00000000-0000-0000-0000-000000000000)]coclass C1{interface I1;interface I2;};\n"
          "[uuid(00000000-0000-0000-0000-000000000001)]coclass C2{interface I1;interface I2;}\n";
    res = check_idl( src, 0 );
    todo_wine
    ok( !res, "MIDL failed, error %#lx\n", res );

    /* attribute syntax is flexible */
    src = "[hidden,][,][,,local,][,,][,helpstring(\"\")]"
          "interface I{}";
    res = check_idl( src, 0 );
    todo_wine
    ok( !res, "MIDL failed, error %#lx\n", res );

    /* needs a unique uuid */
    src = "[uuid(00000000-0000-0000-0000-000000000000),object]\n"
          "interface IUnknown{}\n"
          "[uuid(00000000-0000-0000-0000-000000000000),object]\n"
          "interface I:IUnknown{}\n";
    res = check_idl( src, 0 );
    todo_wine
    ok( res, "MIDL succeeded\n" );

    src = "[uuid(00000000-0000-0000-0000-000000000000),object]\n"
          "interface IUnknown{}\n"
          "[uuid(00000000-0000-0000-0000-000000000001),object]\n"
          "interface I:IUnknown{}\n"
          "[uuid(00000000-0000-0000-0000-000000000002),object]\n"
          "interface J:I{}\n";
    res = check_idl( src, 0 );
    todo_wine
    ok( !res, "MIDL failed, error %#lx\n", res );
}

START_TEST( midl )
{
    if (!midl_test_init())
    {
        skip( "midl.exe not found, skipping tests\n" );
        return;
    }

    test_idl_parsing();
}
