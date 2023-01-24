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
#include <stdlib.h>
#include <stdio.h>

#include "windef.h"
#include "winbase.h"

#include "typetree.h"
#include "parser.h"
#include "parser.tab.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(midl);

static char const usage[] =
{
    "Usage: midl.exe [options...] <input.idl>\n"
    "\n"
    "  Input options:\n"
    "    -I<dir>, -I <dir>     Add <dir> to include and import lookup path.\n"
    "\n"
    "  Output options:\n"
    "    -Zs, -syntax_check    Only check syntax, do not generate any output.\n"
    "\n"
    "  WinRT options:\n"
    "    -winrt                Enable WinRT mode with IDL 3.0 support.\n"
    "    -nomidl               Skip running MIDL again after WinRT mode.\n"
    "    -nomd                 Do not use WinMD metadata files.\n"
    "\n"
    "  Preprocessing options:\n"
    "    -nocpp                Disable C preprocessing entirely.\n"
    "\n"
    "  Other options:\n"
    "    -?, --help            Display this help message.\n"
    "    -confirm              Only print current options.\n"
    "    -nologo               Do not output MIDL version header.\n"
    "    -W[0|1|2|3|4]         Set warning message level.\n"
    "    -WX                   Enable warnings as errors.\n"
    ""
};

struct options
{
    int syntax_check;
    int winrt;
    int nomidl;
    int nomd;
    int nocpp;
    int confirm;
    int nologo;
    int warning_level;
    int warning_error;
};

int do_typelib = 0;
char typename_base[1] = {""};

char *find_input_file( const char *name, const char *parent )
{
    FIXME( "name %s, parent %s stub!\n", debugstr_a(name), debugstr_a(parent) );
    return strdup( name );
}

FILE *open_input_file( const char *path, char **temp_name )
{
    FILE *file;

    FIXME( "path %s, temp_name %p stub!\n", debugstr_a(path), temp_name );

    if (!(file = fopen( path, "r" )))
    {
        fprintf( stderr, "Unable to open %s\n", path );
        exit( 1 );
    }
    *temp_name = NULL;

    return file;
}

void close_input_file( FILE *file, char *temp_name )
{
    fclose( file );
    if (temp_name) unlink( temp_name );
    free( temp_name );
}

void add_importlib( const char *name, typelib_t *typelib )
{
    FIXME( "name %s, typelib %p stub!\n", debugstr_a(name), typelib );
}

int __cdecl main( int argc, char *argv[] )
{
    struct options options = {0};
    struct idl_ctx ctx = {0};
    int i;

    for (i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-' || argv[i][0] == '/')
        {
            const char *opt = argv[i] + 1;
            switch (*opt)
            {
            case 'I':
                if (!opt[1]) i++;
                FIXME( "-I option not implemented!\n" );
                continue;
            case 'W':
                if (opt[1] == 'X') options.warning_error = 1;
                else if (opt[1] >= '0' && opt[1] <= '4') options.warning_level = opt[1] - '0';
                else break;

                FIXME( "-W option not implemented!\n" );
                continue;

            default:
                if (!strcmp( opt, "?" ) || !strcmp( opt, "help" ))
                {
                    printf( usage );
                    return 0;
                }
                if (!strcmp( opt, "Zs" ) || !strcmp( opt, "syntax_check" ))
                {
                    FIXME( "-syntax_check option not implemented!\n" );
                    options.syntax_check = 1;
                    continue;
                }
                if (!strcmp( opt, "winrt" ))
                {
                    FIXME( "-winrt option not implemented!\n" );
                    options.winrt = 1;
                    continue;
                }
                if (!strcmp( opt, "nomidl" ))
                {
                    FIXME( "-nomidl option not implemented!\n" );
                    options.nomidl = 1;
                    continue;
                }
                if (!strcmp( opt, "nomd" ))
                {
                    FIXME( "-nomd option not implemented!\n" );
                    options.nomd = 1;
                    continue;
                }
                if (!strcmp( opt, "nocpp" ))
                {
                    FIXME( "-nocpp option not implemented!\n" );
                    options.nocpp = 1;
                    continue;
                }
                if (!strcmp( opt, "confirm" ))
                {
                    options.confirm = 1;
                    continue;
                }
                if (!strcmp( opt, "nologo" ))
                {
                    FIXME( "-nologo option not implemented!\n" );
                    options.nologo = 1;
                    continue;
                }
                break;
            }

            ERR( "unknown option %s\n", debugstr_a(opt) );
            if (options.warning_error) return 1;
            continue;
        }

        if (!ctx.input) ctx.input = strdup( argv[i] );
        else
        {
            ERR( "duplicate input file: %s\n", debugstr_a(argv[i]) );
            return 1;
        }
    }

    if (options.confirm)
    {
        printf( "Current options:\n" );
        printf( "  input_idl:      %s\n", debugstr_a(ctx.input) );
        printf( "  syntax_check:   %d\n", options.syntax_check );
        printf( "  winrt:          %d\n", options.winrt );
        printf( "  nomidl:         %d\n", options.nomidl );
        printf( "  nomd:           %d\n", options.nomd );
        printf( "  nocpp:          %d\n", options.nocpp );
        printf( "  confirm:        %d\n", options.confirm );
        printf( "  nologo:         %d\n", options.nologo );
        printf( "  warning_level:  %d\n", options.warning_level );
        printf( "  warning_error:  %d\n", options.warning_error );
        return 0;
    }

    if (!ctx.input)
    {
        fprintf( stderr, "error: No input file specified\n" );
        return 1;
    }

    winrt_mode = options.winrt;
    return idl_compile( &ctx, ctx.input, NULL );
}
