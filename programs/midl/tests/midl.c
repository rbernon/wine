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
#include <assert.h>

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

struct output
{
    struct list entry;
    const WCHAR *flag;
    const WCHAR *name;
    unsigned char *data;
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

static void read_file( const WCHAR *filename, unsigned char **data )
{
    DWORD size = 0, total = 0;
    HANDLE file;

    file = CreateFileW( filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
    ok( file != INVALID_HANDLE_VALUE, "CreateFileW failed, error %lu\n", GetLastError() );
    do
    {
        *data = realloc( *data, total + 0x1000 );
        ReadFile( file, *data + total, 0x1000, &size, NULL );
        total += size;
    } while (size == 0x1000);

    CloseHandle( file );
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

static DWORD check_idl( struct list *inputs, struct list *outputs, enum midl_flags flags )
{
    WCHAR temp[MAX_PATH], args[5 * MAX_PATH], path[MAX_PATH], cwd[MAX_PATH];
    struct input *in, *main = LIST_ENTRY( list_head( inputs ), struct input, entry );
    struct output *out;
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

    swprintf( args, ARRAY_SIZE(args), L"%s -I. -nocpp", main->name );
    if (list_empty( outputs )) wcscat( args, L" -syntax_check" );
    if (!(flags & MIDL_VERSION)) wcscat( args, L" -nologo" );
    if (flags & MIDL_WERROR) wcscat( args, L" -W4 -WX" );
    if (flags & MIDL_WINRT) wcscat( args, L" -winrt -nomd -nomidl" );

    LIST_FOR_EACH_ENTRY( out, outputs, struct output, entry )
    {
        wcscat( args, L" " );
        wcscat( args, out->flag );
        wcscat( args, L" " );
        wcscat( args, out->name );
    }

    res = run_midl( args, cwd, flags );

    LIST_FOR_EACH_ENTRY( in, inputs, struct input, entry )
    {
        swprintf( path, ARRAY_SIZE(path), L"%s\\%s", cwd, in->name );
        DeleteFileW( path );
    }

    LIST_FOR_EACH_ENTRY( out, outputs, struct output, entry )
    {
        swprintf( path, ARRAY_SIZE(path), L"%s\\%s", cwd, out->name );
        read_file( path, &out->data );
        DeleteFileW( path );
    }

    RemoveDirectoryW( cwd );
    DeleteFileW( temp );

    return res;
}

static void strip_comments( char *text )
{
    char *end, *next = strstr( text, "/*" );
    int len = next - text;

    while (next && *next && (end = strstr( next, "*/" )))
    {
        if (!(next = strstr( end, "/*" ))) next = end + strlen( end );
        memmove( text + len, end + 2, next - end - 2 );
        len += next - end - 2;
    }

    text[len] = 0;
}

static void unescape_lines( char *text )
{
    char *end, *next = strstr( text, "\\\r\n" );
    int len = next - text;

    while (next && *next && (end = next + 3))
    {
        if (!(next = strstr( end, "\\\r\n" ))) next = end + strlen( end );
        memmove( text + len, end, next - end );
        len += next - end;
    }

    text[len] = 0;
}

static void squash_blank_lines( char *text )
{
    static const char *lookup = "\r\n";
    char *pos, *end, *next = strpbrk( text, lookup );
    int len = next - text;

    while (next > text && (next[-1] == ' ' || next[-1] == '\t')) next--;
    while ((pos = next) && *pos && (end = next + strspn( next, lookup )) > next)
    {
        if (!(pos = next = strpbrk( end, lookup ))) pos = end + strlen( end );
        while (pos > text && (pos[-1] == ' ' || pos[-1] == '\t')) pos--;
        memmove( text + len, end, pos - end );
        len += pos - end;
        text[len++] = '\n';
    }

    text[len] = 0;
}

static void post_process_source( char *text )
{
    strip_comments( text );
    unescape_lines( text );
    squash_blank_lines( text );
    squash_blank_lines( text );
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
    struct output header = {.name = L"main.h", .flag = L"-h" /* strcmp( winetest_platform, "wine" ) ? L"-h" : L"-H" */};
    struct list in = LIST_INIT( in ), out = LIST_INIT( out );
    DWORD res;

    list_add_tail( &in, &src.entry );

    /* check basic success */
    src.text = "interface I;";
    res = check_idl( &in, &out, MIDL_WERROR | MIDL_VERBOSE | MIDL_VERSION );
    todo_wine
    ok( !res, "MIDL failed, error %#lx\n", res );

    /* check basic failure */
    src.text = "interface {}";
    res = check_idl( &in, &out, 0 );
    ok( res, "MIDL succeeded\n" );

    /* winrt mode degrades automatically */
    src.text = "interface I {}; coclass C { [default] interface I; }";
    res = check_idl( &in, &out, MIDL_WERROR | MIDL_WINRT );
    todo_wine
    ok( !res, "MIDL failed, error %#lx\n", res );

    /* legacy mode doesn't upgrade */
    src.text = "namespace N {}";
    res = check_idl( &in, &out, 0 );
    ok( res, "MIDL succeeded\n" );
    res = check_idl( &in, &out, MIDL_WERROR | MIDL_WINRT );
    ok( !res, "MIDL failed, error %#lx\n", res );

    /* colon is sometimes optional */
    src.text = "[local]interface I1 {}\n"
               "[local]interface I2 {};\n"
               "[uuid(00000000-0000-0000-0000-000000000000)]coclass C1{interface I1;interface I2;};\n"
               "[uuid(00000000-0000-0000-0000-000000000001)]coclass C2{interface I1;interface I2;}\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    todo_wine
    ok( !res, "MIDL failed, error %#lx\n", res );

    /* attribute syntax is flexible */
    src.text = "[hidden,][,][,,local,][,,][,helpstring(\"\")]"
               "interface I{}";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( !res, "MIDL failed, error %#lx\n", res );

    /* allowed but ignored on forward definitions */
    src.text = "[local][local]interface I{}";
    res = check_idl( &in, &out, MIDL_WERROR );
    todo_wine
    ok( res, "MIDL succeeded\n" );
    src.text = "[local][local]interface I;";
    res = check_idl( &in, &out, MIDL_WERROR );
    todo_wine
    ok( !res, "MIDL failed, error %#lx\n", res );

    /* warning: object needs IUnknown */
    src.text = "[uuid(00000000-0000-0000-0000-000000000000),object]\n"
               "interface I{}\n";
    res = check_idl( &in, &out, 0 );
    todo_wine
    ok( !res, "MIDL failed, error %#lx\n", res );
    res = check_idl( &in, &out, MIDL_WERROR );
    todo_wine
    ok( res, "MIDL succeeded\n" );

    /* error: interface with method needs uuid */
    src.text = "typedef unsigned int HRESULT;\n"
               "interface I{HRESULT bla();}\n";
    res = check_idl( &in, &out, 0 );
    todo_wine
    ok( res, "MIDL succeeded\n" );

    /* warning: local doesn't needs uuid */
    src.text = "typedef unsigned int HRESULT;\n"
               "[local]interface I{}\n";
    res = check_idl( &in, &out, 0 );
    ok( !res, "MIDL failed, error %#lx\n", res );

    /* warning: local doesn't needs uuid */
    src.text = "typedef unsigned int HRESULT;\n"
               "[local]interface I{HRESULT bla();}\n";
    res = check_idl( &in, &out, 0 );
    ok( !res, "MIDL failed, error %#lx\n", res );

    /* warning: local doesn't needs uuid */
    src.text = "typedef unsigned int HRESULT;\n"
               "[local,uuid(00000000-0000-0000-0000-000000000000)]interface I{HRESULT bla();}\n";
    res = check_idl( &in, &out, 0 );
    ok( !res, "MIDL failed, error %#lx\n", res );

    /* warning: object needs uuid */
    src.text = "[uuid(00000000-0000-0000-0000-000000000000),object]\n"
               "interface IUnknown{}\n"
               "[object]interface I:IUnknown{}\n";
    res = check_idl( &in, &out, 0 );
    todo_wine
    ok( res, "MIDL succeeded\n" );

    /* warning: object needs uuid */
    src.text = "[uuid(00000000-0000-0000-0000-000000000000),object]\n"
               "interface IUnknown{}\n"
               "[object,local]interface I:IUnknown{}\n";
    res = check_idl( &in, &out, 0 );
    todo_wine
    ok( res, "MIDL succeeded\n" );

    /* needs a unique uuid */
    src.text = "[uuid(00000000-0000-0000-0000-000000000000),object]\n"
               "interface IUnknown{}\n"
               "[uuid(00000000-0000-0000-0000-000000000000),object]\n"
               "interface I:IUnknown{}\n";
    res = check_idl( &in, &out, 0 );
    todo_wine
    ok( res, "MIDL succeeded\n" );

    src.text = "[uuid(00000000-0000-0000-0000-000000000000),object]\n"
               "interface IUnknown{}\n"
               "[uuid(00000000-0000-0000-0000-000000000001),object]\n"
               "interface I:IUnknown{}\n"
               "[uuid(00000000-0000-0000-0000-000000000002),object]\n"
               "interface J:I{}\n";
    res = check_idl( &in, &out, 0 );
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
    res = check_idl( &in, &out, MIDL_WINRT | MIDL_WERROR );
    ok( !res, "MIDL failed, error %#lx\n", res );
    src.text = "import\"inspectable.idl\";\n"
               "namespace N {\n"
               "[uuid(00000000-0000-0000-0000-000000000002),version(1)]\n"
               "interface I:IInspectable{}\n"
               "[uuid(00000000-0000-0000-0000-000000000003),version(1)]\n"
               "interface J:I{}\n"
               "}\n";
    res = check_idl( &in, &out, MIDL_WINRT );
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
    res = check_idl( &in, &out, MIDL_WINRT );
    ok( !res, "MIDL failed, error %#lx\n", res );
    res = check_idl( &in, &out, MIDL_WINRT | MIDL_WERROR );
    todo_wine
    ok( res, "MIDL succeeded\n" );

    /* check header generation */
    src.text = "import\"unknwn.idl\";\n"
               "typedef unsigned long HRESULT;\n"
               "[uuid(00000000-0000-0000-0000-000000000002),object]\n"
               "interface I:IUnknown{HRESULT Foo(void);}\n";
    list_add_tail( &out, &header.entry );
    res = check_idl( &in, &out, MIDL_WERROR | MIDL_VERBOSE | MIDL_VERSION );
    ok( !res, "MIDL failed, error %#lx\n", res );

    if (0) post_process_source( (char *)header.data );
    printf( "%s:\n", debugstr_w(header.name) );
    printf( "%s", header.data );

    free( header.data );
}

static void test_attributes(void)
{
    struct input oaidl =
    {
        .name = L"oaidl.idl",
        .text = "interface ITypeLib;\n",
    };
    struct input unknwn =
    {
        .name = L"unknwn.idl",
        .text = "[uuid(00000000-0000-0000-0000-000000000000),object]"
                "interface IUnknown{}",
    };
    struct input inspectable =
    {
        .name = L"inspectable.idl",
        .text = "import\"unknwn.idl\";"
                "[uuid(00000000-0000-0000-0000-000000000001),object]"
                "interface IInspectable:IUnknown{}",
    };
    struct input src = {.name = L"main.idl"};
    struct list in = LIST_INIT( in ), out = LIST_INIT( out );
    DWORD res;

    list_add_tail( &in, &src.entry );
    list_add_tail( &in, &oaidl.entry );
    list_add_tail( &in, &unknwn.entry );
    list_add_tail( &in, &inspectable.entry );

    src.text = "struct A[helpstring(\"\")];\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "struct [helpstring(\"\")]A;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );

    src.text = "static [helpstring(\"\")]struct A *a;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "static struct [helpstring(\"\")]A *a;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "static struct A [helpstring(\"\")]*a;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "static struct A *[helpstring(\"\")]a;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "static struct A *a[helpstring(\"\")];\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );

    src.text = "typedef const [helpstring(\"\")]struct A *B;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "typedef const struct [helpstring(\"\")]A *B;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "typedef const struct A [helpstring(\"\")]*B;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "typedef const struct A *[helpstring(\"\")]B;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "typedef const struct A *B[helpstring(\"\")];\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );

    /* error duplicate attribute */
    src.text = "[helpstring(\"\")]typedef [helpstring(\"\")]struct A *B;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );

    src.text = "import \"unknwn.idl\";\n"
               "[helpstring(\"\")] enum A;\n"
               "[helpstring(\"\")] union A;\n"
               "[helpstring(\"\")] struct A;\n"
               "[helpstring(\"\")] static enum A *a;\n"
               "[helpstring(\"\")] static union A *b;\n"
               "[helpstring(\"\")] static struct A *c;\n"
               "[helpstring(\"\")] typedef [version(1)] const enum A *B;\n"
               "[helpstring(\"\")] typedef [version(1)] const union A *C;\n"
               "[helpstring(\"\")] typedef [version(1)] const struct A *D;\n"
               "[helpstring(\"\")] typedef [version(1)] A const *E;\n"
               "[helpstring(\"\"),object,uuid(00000000-0000-0000-0000-000000000001)]"
               "interface A;\n"
               "[helpstring(\"\"),object,uuid(00000000-0000-0000-0000-000000000002)]"
               "interface A;\n" /* different redeclaration is fine */
               "[helpstring(\"\"),object,uuid(00000000-0000-0000-0000-000000000003)]\n"
               "interface A:IUnknown{};\n"; /* different definition is fine */
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( !res, "got %#lx\n", res );

    src.text = "[helpstring(\"\")]dispinterface A;\n"
               "[helpstring(\"\")]coclass A;\n"
               "[helpstring(\"\")]module A;\n"
               "[uuid(00000000-0000-0000-0000-000000000000)]"
               "library A{};\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( !res, "got %#lx\n", res );

    src.text = "import \"inspectable.idl\";\n"
               "[helpstring(\"\")]namespace N{\n"
               "[contractversion(1)]apicontract A{};\n"
               "[version(1)]delegate A;\n"
               "[version(1)]runtimeclass A;\n"
               "}\n";
    res = check_idl( &in, &out, MIDL_WERROR | MIDL_VERBOSE | MIDL_WINRT );
    ok( !res, "got %#lx\n", res );
}

static void test_declarations(void)
{
    struct input oaidl =
    {
        .name = L"oaidl.idl",
        .text = "interface ITypeLib;\n",
    };
    struct input unknwn =
    {
        .name = L"unknwn.idl",
        .text = "[uuid(00000000-0000-0000-0000-000000000000),object]"
                "interface IUnknown{}",
    };
    struct input inspectable =
    {
        .name = L"inspectable.idl",
        .text = "import\"unknwn.idl\";"
                "[uuid(00000000-0000-0000-0000-000000000001),object]"
                "interface IInspectable:IUnknown{}",
    };
    struct output header = {.name = L"main.h", .flag = L"-h" /* strcmp( winetest_platform, "wine" ) ? L"-h" : L"-H" */};
    struct input src = {.name = L"main.idl"};
    struct list in = LIST_INIT( in ), out = LIST_INIT( out );
    DWORD res;

    list_add_tail( &in, &src.entry );
    list_add_tail( &in, &oaidl.entry );
    list_add_tail( &in, &unknwn.entry );
    list_add_tail( &in, &inspectable.entry );

    src.text = "typedef interface A *B;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "typedef interface A {} *B;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "typedef dispinterface A *B;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "typedef dispinterface A {} *B;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "typedef coclass A *B;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "typedef coclass A {} *B;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "typedef runtimeclass A *B;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "typedef runtimeclass A {} *B;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );

    src.text = "static A *a;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "static interface A *a;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "static interface A {} *a;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "static dispinterface A *a;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "static dispinterface A {} *a;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "static coclass A *a;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "static coclass A {} *a;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "static runtimeclass A *a;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "static runtimeclass A {} *a;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );

    src.text = "struct A {int a;}; struct A {int a;};\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "struct A {int a;}; union A {int a;};\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "struct A {int a;}; enum A {V};\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "typedef A B; typedef A B;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "typedef A B; [local] interface B {};\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );

    src.text = "import \"unknwn.idl\";\n"
               "typedef A B;\n"
               "static B *a;\n"
               "typedef enum A *C;\n"
               "typedef enum B {V} *D;\n"
               "typedef union A *E;\n"
               "typedef union C {int a;} *F;\n"
               "typedef struct A *G;\n"
               "typedef struct D {int a;} *H;\n"
               "typedef I *J;\n"
               "[object,uuid(00000000-0000-0000-0000-000000000001)]\n"
               "interface I : IUnknown {};"
               "\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( !res, "got %#lx\n", res );

    src.text = "[local]interface IDummy {}\n"
               "\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( !res, "got %#lx\n", res );

    list_add_tail( &out, &header.entry );

    src.text = "typedef small sm;\n"
               "typedef signed small ssm;\n"
               "typedef unsigned small usm;\n"
               "typedef small int smi;\n"
               "typedef signed small int ssmi;\n"
               "typedef unsigned small int usmi;\n"
               "\n"
               "typedef short sh;\n"
               "typedef signed short ssh;\n"
               "typedef unsigned short ush;\n"
               "typedef short int shi;\n"
               "typedef signed short int sshi;\n"
               "typedef unsigned short int ushi;\n"
               "\n"
               "typedef long l;\n"
               "typedef signed long sl;\n"
               "typedef long signed ls;\n"
               "typedef unsigned long ul;\n"
               "typedef long int li;\n"
               "typedef int long il;\n"
               "typedef signed long int sli;\n"
               "typedef long signed int lsi;\n"
               "typedef int signed long isl;\n"
               "typedef int long signed ils;\n"
               "typedef unsigned long int uli;\n"
               "\n"
               "typedef long long ll;\n"
               "typedef signed long long sll;\n"
               "typedef unsigned long long ull;\n"
               "typedef long long int lli;\n"
               "typedef signed long long int slli;\n"
               "typedef unsigned long long int ulli;\n"
               "typedef int unsigned long long iull;\n"
               "\n"
               "typedef signed s;\n"
               "typedef unsigned u;\n"
               "typedef int i;\n"
               "typedef signed int si;\n"
               "typedef unsigned int ui;\n"
               "\n"
               "typedef char c;\n"
               "typedef signed char sc;\n"
               "typedef unsigned char uc;\n"
               "\n"
               "typedef hyper hy;\n"
               "typedef signed hyper shy;\n"
               "typedef unsigned hyper uhy;\n"
               "typedef hyper int hyi;\n"
               "typedef signed hyper int shyi;\n"
               "typedef unsigned hyper int uhyi;\n"
               "\n"
               "typedef void v;\n"
               "typedef float f;\n"
               "typedef double d;\n"
               "typedef long double ld;\n"
               "typedef byte by;\n"
               "typedef boolean bo;\n"
               "typedef __int32 i32;\n"
               "typedef __int3264 i3264;\n"
               "typedef __int64 i64;\n"
               "\n"
               "typedef handle_t ht;\n"
               "typedef error_status_t est;\n"
               "typedef wchar_t wt;\n"
               "\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( !res, "got %#lx\n", res );

    if (0) post_process_source( (char *)header.data );
    printf( "%s:\n", debugstr_w(header.name) );
    printf( "%s", header.data );

    src.text = "typedef int handle_t;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "typedef int error_status_t;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "typedef int wchar_t;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );

    src.text = "typedef UINT32 t;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "typedef HANDLE t;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
    src.text = "typedef GUID t;\n";
    res = check_idl( &in, &out, MIDL_WERROR );
    ok( res, "got %#lx\n", res );
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
    test_attributes();
    test_declarations();
}
