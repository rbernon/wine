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

struct input
{
    struct list entry;
    const WCHAR *name;
    const char *text;
};

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
    MIDL_WERROR = 4,
    MIDL_WINRT = 8,
};

static DWORD run_midl( const WCHAR *args, const WCHAR *cwd, enum midl_flags flags )
{
    STARTUPINFOW si = {.cb = sizeof(STARTUPINFOW)};
    PROCESS_INFORMATION pi = {0};
    WCHAR cmd[5 * MAX_PATH];
    DWORD res, exit_code;
    const WCHAR *suffix;
    BOOL ret;

    if (!strcmp( winetest_platform, "wine" )) suffix = L"";
    else suffix = (flags & MIDL_WINRT ? L"rt" : L"");

    swprintf( cmd, ARRAY_SIZE(cmd), L"\"%s\\midl%s.exe\" %s", midl_path, suffix, args );
    if (flags & MIDL_VERBOSE) trace( "Running %s\n", debugstr_w(cmd) );

    ret = CreateProcessW( NULL, cmd, NULL, NULL, FALSE, 0, NULL, cwd, &si, &pi );
    ok( ret, "CreateProcessW failed, error %lu\n", GetLastError() );
    res = WaitForSingleObject( pi.hProcess, 1000 );
    ok( !res, "WaitForSingleObject returned %#lx\n", res );
    ret = GetExitCodeProcess( pi.hProcess, &exit_code );
    ok( ret, "GetExitCodeProcess failed, error %lu\n", GetLastError() );
    CloseHandle( pi.hThread );
    CloseHandle( pi.hProcess );

    return exit_code;
}

static DWORD check_idl( struct list *inputs, enum midl_flags flags )
{
    WCHAR temp[MAX_PATH], args[5 * MAX_PATH], path[MAX_PATH], cwd[MAX_PATH];
    struct input *in, *main = LIST_ENTRY( list_head( inputs ), struct input, entry );
    DWORD res;

    if (flags & MIDL_VERSION) trace( "Using MIDL from %s\n", debugstr_w(midl_path) );

    GetTempPathW( ARRAY_SIZE(temp), temp );
    GetTempFileNameW( temp, L"midl", 0, temp );

    swprintf( cwd, ARRAY_SIZE(cwd), L"%s.dir", temp );
    CreateDirectoryW( cwd, NULL );

    LIST_FOR_EACH_ENTRY( in, inputs, struct input, entry )
    {
        swprintf( path, ARRAY_SIZE(path), L"%s\\%s", cwd, in->name );
        write_file( path, in->text );
    }

    swprintf( args, ARRAY_SIZE(args), L"%s -I. -nocpp -syntax_check", main->name );
    if (!(flags & MIDL_VERSION)) wcscat( args, L" -nologo" );
    if (flags & MIDL_WERROR) wcscat( args, L" -W4 -WX" );
    if (flags & MIDL_WINRT) wcscat( args, L" -winrt -nomd -nomidl" );

    res = run_midl( args, cwd, flags );

    LIST_FOR_EACH_ENTRY( in, inputs, struct input, entry )
    {
        swprintf( path, ARRAY_SIZE(path), L"%s\\%s", cwd, in->name );
        DeleteFileW( path );
    }

    RemoveDirectoryW( cwd );
    DeleteFileW( temp );

    return res;
}

static void test_cmdline(void)
{
    static const WCHAR *prefixes[] = {L"-", L"/"};
    WCHAR args[MAX_PATH];
    DWORD i, res;

    /* arg stating with either - or / is an option */
    for (i = 0; i < ARRAY_SIZE(prefixes); ++i)
    {
        swprintf( args, ARRAY_SIZE(args), L"-WX %sconfirm", prefixes[i] );
        res = run_midl( args, NULL, 0 );
        todo_wine
        ok( !res, "MIDL failed, error %#lx\n", res );

        swprintf( args, ARRAY_SIZE(args), L"%sWX -confirm", prefixes[i] );
        res = run_midl( args, NULL, 0 );
        todo_wine
        ok( !res, "MIDL failed, error %#lx\n", res );
    }

    /* there's no magic values for stdin or alike */
    res = run_midl( L"-WX --", NULL, 0 );
    ok( res, "MIDL succeeded\n" );
    res = run_midl( L"-WX -", NULL, 0 );
    ok( res, "MIDL succeeded\n" );
    res = run_midl( L"-WX /", NULL, 0 );
    ok( res, "MIDL succeeded\n" );

    /* -- isn't accepted as a special prefix */
    res = run_midl( L"-WX --confirm", NULL, 0 );
    ok( res, "MIDL succeeded\n" );

    /* unknown options are ignred unless WX is used */
    res = run_midl( L"--confirm -confirm", NULL, 0 );
    todo_wine
    ok( !res, "MIDL failed, error %#lx\n", res );
}

static void test_idl_parsing(void)
{
    struct input src = {.name = L"main.idl"}, unknwn = {.name = L"unknwn.idl"}, inspectable = {.name = L"inspectable.idl"};
    struct list in = LIST_INIT( in );
    DWORD res;

    list_add_tail( &in, &src.entry );

    /* check basic success */
    src.text = "interface I;";
    res = check_idl( &in, MIDL_WERROR | MIDL_VERBOSE | MIDL_VERSION );
    todo_wine
    ok( !res, "MIDL failed, error %#lx\n", res );

    /* check basic failure */
    src.text = "interface {}";
    res = check_idl( &in, 0 );
    ok( res, "MIDL succeeded\n" );

    /* winrt mode degrades automatically */
    src.text = "interface I {}; coclass C { [default] interface I; }";
    res = check_idl( &in, MIDL_WERROR | MIDL_WINRT );
    todo_wine
    ok( !res, "MIDL failed, error %#lx\n", res );

    /* legacy mode doesn't upgrade */
    src.text = "namespace N {}";
    res = check_idl( &in, 0 );
    ok( res, "MIDL succeeded\n" );
    res = check_idl( &in, MIDL_WERROR | MIDL_WINRT );
    ok( !res, "MIDL failed, error %#lx\n", res );

    /* colon is sometimes optional */
    src.text = "[local]interface I1 {}\n"
               "[local]interface I2 {};\n"
               "[uuid(00000000-0000-0000-0000-000000000000)]coclass C1{interface I1;interface I2;};\n"
               "[uuid(00000000-0000-0000-0000-000000000001)]coclass C2{interface I1;interface I2;}\n";
    res = check_idl( &in, MIDL_WERROR );
    todo_wine
    ok( !res, "MIDL failed, error %#lx\n", res );

    /* attribute syntax is flexible */
    src.text = "[hidden,][,][,,local,][,,][,helpstring(\"\")]"
               "interface I{}";
    res = check_idl( &in, MIDL_WERROR );
    ok( !res, "MIDL failed, error %#lx\n", res );

    /* allowed but ignored on forward definitions */
    src.text = "[local][local]interface I{}";
    res = check_idl( &in, MIDL_WERROR );
    todo_wine
    ok( res, "MIDL succeeded\n" );
    src.text = "[local][local]interface I;";
    res = check_idl( &in, MIDL_WERROR );
    todo_wine
    ok( !res, "MIDL failed, error %#lx\n", res );

    /* warning: object needs IUnknown */
    src.text = "[uuid(00000000-0000-0000-0000-000000000000),object]\n"
               "interface I{}\n";
    res = check_idl( &in, 0 );
    todo_wine
    ok( !res, "MIDL failed, error %#lx\n", res );
    res = check_idl( &in, MIDL_WERROR );
    todo_wine
    ok( res, "MIDL succeeded\n" );

    /* needs a unique uuid */
    src.text = "[uuid(00000000-0000-0000-0000-000000000000),object]\n"
               "interface IUnknown{}\n"
               "[uuid(00000000-0000-0000-0000-000000000000),object]\n"
               "interface I:IUnknown{}\n";
    res = check_idl( &in, 0 );
    todo_wine
    ok( res, "MIDL succeeded\n" );

    src.text = "[uuid(00000000-0000-0000-0000-000000000000),object]\n"
               "interface IUnknown{}\n"
               "[uuid(00000000-0000-0000-0000-000000000001),object]\n"
               "interface I:IUnknown{}\n"
               "[uuid(00000000-0000-0000-0000-000000000002),object]\n"
               "interface J:I{}\n";
    res = check_idl( &in, 0 );
    todo_wine
    ok( !res, "MIDL failed, error %#lx\n", res );

    unknwn.text = "[uuid(00000000-0000-0000-0000-000000000000),object]"
                  "interface IUnknown{}";
    list_add_tail( &in, &unknwn.entry );

    inspectable.text = "import\"unknwn.idl\";"
                       "[uuid(00000000-0000-0000-0000-000000000001),object]"
                       "interface IInspectable:IUnknown{}";
    list_add_tail( &in, &inspectable.entry );

    /* winrt requires IInspectable for all */
    src.text = "import\"inspectable.idl\";\n"
               "namespace N {\n"
               "[uuid(00000000-0000-0000-0000-000000000002),version(1)]\n"
               "interface I:IInspectable{}\n"
               "}\n";
    res = check_idl( &in, MIDL_WINRT | MIDL_WERROR );
    ok( !res, "MIDL failed, error %#lx\n", res );
    src.text = "import\"inspectable.idl\";\n"
               "namespace N {\n"
               "[uuid(00000000-0000-0000-0000-000000000002),version(1)]\n"
               "interface I:IInspectable{}\n"
               "[uuid(00000000-0000-0000-0000-000000000003),version(1)]\n"
               "interface J:I{}\n"
               "}\n";
    res = check_idl( &in, MIDL_WINRT );
    todo_wine
    ok( res, "MIDL succeeded\n" );

    /* warning: cannot mix non-winrt interface definition */
    src.text = "import\"inspectable.idl\";\n"
               "[uuid(00000000-0000-0000-0000-000000000002),object]\n"
               "interface I:IUnknown{}\n"
               "namespace N {\n"
               "[uuid(00000000-0000-0000-0000-000000000003),version(1)]\n"
               "interface J:IInspectable{}\n"
               "}\n";
    res = check_idl( &in, MIDL_WINRT );
    ok( !res, "MIDL failed, error %#lx\n", res );
    res = check_idl( &in, MIDL_WINRT | MIDL_WERROR );
    todo_wine
    ok( res, "MIDL succeeded\n" );
}

START_TEST( midl )
{
    if (!midl_test_init())
    {
        skip( "midl.exe not found, skipping tests\n" );
        return;
    }

    test_cmdline();
    test_idl_parsing();
}
