/*
 * Copyright 2024 Rémi Bernon for CodeWeavers
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

/* pull isprint from CRT or ntdll */
#define _CRTIMP
/* pull IsBadStringPtrA/W from kernel or ntdll */
#define WINBASEAPI

#include <stdarg.h>
#include <stddef.h>

#include "windef.h"
#include "winbase.h"

#include "wine/debug.h"

const char *wine_dbgstr_an( const char *str, int n )
{
    static const char hex[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
    char buffer[300], *dst = buffer;

    if (!str) return "(null)";
    if (!((ULONG_PTR)str >> 16)) return wine_dbg_sprintf( "#%04x", LOWORD(str) );
#ifdef __WINE_PE_BUILD
    if (IsBadStringPtrA( str, n )) return "(invalid)";
#endif
    if (n == -1) for (n = 0; str[n]; n++) ;
    *dst++ = '"';
    while (n-- > 0 && dst <= buffer + sizeof(buffer) - 9)
    {
        unsigned char c = *str++;
        switch (c)
        {
        case '\n': *dst++ = '\\'; *dst++ = 'n'; break;
        case '\r': *dst++ = '\\'; *dst++ = 'r'; break;
        case '\t': *dst++ = '\\'; *dst++ = 't'; break;
        case '"':  *dst++ = '\\'; *dst++ = '"'; break;
        case '\\': *dst++ = '\\'; *dst++ = '\\'; break;
        default:
            if (c < ' ' || c >= 127)
            {
                *dst++ = '\\';
                *dst++ = 'x';
                *dst++ = hex[(c >> 4) & 0x0f];
                *dst++ = hex[c & 0x0f];
            }
            else *dst++ = c;
        }
    }
    *dst++ = '"';
    if (n > 0)
    {
        *dst++ = '.';
        *dst++ = '.';
        *dst++ = '.';
    }
    *dst = 0;
    return __wine_dbg_strdup( buffer );
}

const char *wine_dbgstr_wn( const WCHAR *str, int n )
{
    static const char hex[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
    char buffer[300], *dst = buffer;

    if (!str) return "(null)";
    if (!((ULONG_PTR)str >> 16)) return wine_dbg_sprintf( "#%04x", LOWORD(str) );
#ifdef __WINE_PE_BUILD
    if (IsBadStringPtrW( str, n )) return "(invalid)";
#endif
    if (n == -1) for (n = 0; str[n]; n++) ;
    *dst++ = 'L';
    *dst++ = '"';
    while (n-- > 0 && dst <= buffer + sizeof(buffer) - 10)
    {
        WCHAR c = *str++;
        switch (c)
        {
        case '\n': *dst++ = '\\'; *dst++ = 'n'; break;
        case '\r': *dst++ = '\\'; *dst++ = 'r'; break;
        case '\t': *dst++ = '\\'; *dst++ = 't'; break;
        case '"':  *dst++ = '\\'; *dst++ = '"'; break;
        case '\\': *dst++ = '\\'; *dst++ = '\\'; break;
        default:
            if (c < ' ' || c >= 127)
            {
                *dst++ = '\\';
                *dst++ = hex[(c >> 12) & 0x0f];
                *dst++ = hex[(c >> 8) & 0x0f];
                *dst++ = hex[(c >> 4) & 0x0f];
                *dst++ = hex[c & 0x0f];
            }
            else *dst++ = (char)c;
        }
    }
    *dst++ = '"';
    if (n > 0)
    {
        *dst++ = '.';
        *dst++ = '.';
        *dst++ = '.';
    }
    *dst = 0;
    return __wine_dbg_strdup( buffer );
}

const char *wine_dbgstr_guid( const GUID *id )
{
    if (!id) return "(null)";
    if (!((ULONG_PTR)id >> 16)) return wine_dbg_sprintf( "<guid-0x%04hx>", (WORD)(ULONG_PTR)id );
    return wine_dbg_sprintf( "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
                             (unsigned int)id->Data1, id->Data2, id->Data3,
                             id->Data4[0], id->Data4[1], id->Data4[2], id->Data4[3],
                             id->Data4[4], id->Data4[5], id->Data4[6], id->Data4[7] );
}

const char *wine_dbgstr_fourcc( unsigned int fourcc )
{
    char str[4] = { (char)fourcc, (char)(fourcc >> 8), (char)(fourcc >> 16), (char)(fourcc >> 24) };
    if (!fourcc)
        return "''";
    if (isprint( str[0] ) && isprint( str[1] ) && isprint( str[2] ) && isprint( str[3] ))
        return wine_dbg_sprintf( "'%.4s'", str );
    return wine_dbg_sprintf( "0x%08x", fourcc );
}

const char *wine_dbgstr_point( const POINT *pt )
{
    if (!pt) return "(null)";
    return wine_dbg_sprintf( "(%d,%d)", (int)pt->x, (int)pt->y );
}

const char *wine_dbgstr_rect( const RECT *rect )
{
    if (!rect) return "(null)";
    return wine_dbg_sprintf( "(%d,%d)-(%d,%d)", (int)rect->left, (int)rect->top,
                             (int)rect->right, (int)rect->bottom );
}

const char *wine_dbgstr_longlong( ULONGLONG ll )
{
    if (sizeof(ll) > sizeof(unsigned long) && ll >> 32)
        return wine_dbg_sprintf( "%lx%08lx", (unsigned long)(ll >> 32), (unsigned long)ll );
    else return wine_dbg_sprintf( "%lx", (unsigned long)ll );
}
