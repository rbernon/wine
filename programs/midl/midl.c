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
    int nocpp;
    int confirm;
    int nologo;
    int warning_level;
    int warning_error;
};

char *input_name = NULL;

int __cdecl main( int argc, char *argv[] )
{
    struct options options = {0};
    const char *input = NULL;
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

        if (!input) input = strdup( argv[i] );
        else
        {
            ERR( "duplicate input file: %s\n", debugstr_a(argv[i]) );
            return 1;
        }
    }

    if (options.confirm)
    {
        printf( "Current options:\n" );
        printf( "  input_idl:      %s\n", debugstr_a(input) );
        printf( "  syntax_check:   %d\n", options.syntax_check );
        printf( "  nocpp:          %d\n", options.nocpp );
        printf( "  confirm:        %d\n", options.confirm );
        printf( "  nologo:         %d\n", options.nologo );
        printf( "  warning_level:  %d\n", options.warning_level );
        printf( "  warning_error:  %d\n", options.warning_error );
        return 0;
    }

    if (!input)
    {
        fprintf( stderr, "error: No input file specified\n" );
        return 1;
    }
}
