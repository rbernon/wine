/*
 * Debugging functions
 *
 * Copyright 2000 Alexandre Julliard
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

#if 0
#pragma makedep unix
#endif

#include "config.h"
#include "wine/port.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "windef.h"
#include "winnt.h"
#include "winternl.h"
#include "unix_private.h"
#include "wine/debug.h"

WINE_DECLARE_DEBUG_CHANNEL(pid);
WINE_DECLARE_DEBUG_CHANNEL(timestamp);

struct debug_info
{
    unsigned int str_pos;       /* current position in strings buffer */
    unsigned int out_pos;       /* current position in output buffer */
    char         strings[1020]; /* buffer for temporary strings */
    char         output[1020];  /* current output line */
};

C_ASSERT( sizeof(struct debug_info) == 0x800 );

static BOOL init_done;
static struct debug_info initial_info;  /* debug info for initial thread */
static unsigned char default_flags = (1 << __WINE_DBCL_ERR) | (1 << __WINE_DBCL_FIXME);
static int nb_debug_options = -1;
static int options_size;
static struct __wine_debug_channel *debug_options;

static const char * const debug_classes[] = { "fixme", "err", "warn", "trace" };

/* get the debug info pointer for the current thread */
static inline struct debug_info *get_info(void)
{
    if (!init_done) return &initial_info;
#ifdef _WIN64
    return (struct debug_info *)((TEB32 *)((char *)NtCurrentTeb() + teb_offset) + 1);
#else
    return (struct debug_info *)(NtCurrentTeb() + 1);
#endif
}

/* add a string to the output buffer */
static int append_output( struct debug_info *info, const char *str, size_t len )
{
    if (len >= sizeof(info->output) - info->out_pos)
    {
       fprintf( stderr, "wine_dbg_output: debugstr buffer overflow (contents: '%s')\n", info->output );
       info->out_pos = 0;
       abort();
    }
    memcpy( info->output + info->out_pos, str, len );
    info->out_pos += len;
    return len;
}

/* add a new debug option at the end of the option list */
static void add_option( const char *name, unsigned char set, unsigned char clear )
{
    int min = 0, max = nb_debug_options - 1, pos, res;

    if (!name[0])  /* "all" option */
    {
        default_flags = (default_flags & ~clear) | set;
        return;
    }
    if (strlen(name) >= sizeof(debug_options[0].name)) return;

    while (min <= max)
    {
        pos = (min + max) / 2;
        res = strcmp( name, debug_options[pos].name );
        if (!res)
        {
            debug_options[pos].flags = (debug_options[pos].flags & ~clear) | set;
            return;
        }
        if (res < 0) max = pos - 1;
        else min = pos + 1;
    }
    if (nb_debug_options >= options_size)
    {
        options_size = max( options_size * 2, 16 );
        debug_options = realloc( debug_options, options_size * sizeof(debug_options[0]) );
    }

    pos = min;
    if (pos < nb_debug_options) memmove( &debug_options[pos + 1], &debug_options[pos],
                                         (nb_debug_options - pos) * sizeof(debug_options[0]) );
    strcpy( debug_options[pos].name, name );
    debug_options[pos].flags = (default_flags & ~clear) | set;
    nb_debug_options++;
}

/* parse a set of debugging option specifications and add them to the option list */
static void parse_options( const char *str )
{
    char *opt, *next, *options;
    unsigned int i;

    if (!(options = strdup(str))) return;
    for (opt = options; opt; opt = next)
    {
        const char *p;
        unsigned char set = 0, clear = 0;

        if ((next = strchr( opt, ',' ))) *next++ = 0;

        p = opt + strcspn( opt, "+-" );
        if (!p[0]) p = opt;  /* assume it's a debug channel name */

        if (p > opt)
        {
            for (i = 0; i < ARRAY_SIZE(debug_classes); i++)
            {
                int len = strlen(debug_classes[i]);
                if (len != (p - opt)) continue;
                if (!memcmp( opt, debug_classes[i], len ))  /* found it */
                {
                    if (*p == '+') set |= 1 << i;
                    else clear |= 1 << i;
                    break;
                }
            }
            if (i == ARRAY_SIZE(debug_classes)) /* bad class name, skip it */
                continue;
        }
        else
        {
            if (*p == '-') clear = ~0;
            else set = ~0;
        }
        if (*p == '+' || *p == '-') p++;
        if (!p[0]) continue;

        if (!strcmp( p, "all" ))
            default_flags = (default_flags & ~clear) | set;
        else
            add_option( p, set, clear );
    }
    free( options );
}

/* print the usage message */
static void debug_usage(void)
{
    static const char usage[] =
        "Syntax of the WINEDEBUG variable:\n"
        "  WINEDEBUG=[class]+xxx,[class]-yyy,...\n\n"
        "Example: WINEDEBUG=+relay,warn-heap\n"
        "    turns on relay traces, disable heap warnings\n"
        "Available message classes: err, warn, fixme, trace\n";
    write( 2, usage, sizeof(usage) - 1 );
    exit(1);
}

/* initialize all options at startup */
static void init_options(void)
{
    char *wine_debug = getenv("WINEDEBUG");
    struct stat st1, st2;

    nb_debug_options = 0;

    /* check for stderr pointing to /dev/null */
    if (!fstat( 2, &st1 ) && S_ISCHR(st1.st_mode) &&
        !stat( "/dev/null", &st2 ) && S_ISCHR(st2.st_mode) &&
        st1.st_rdev == st2.st_rdev)
    {
        default_flags = 0;
        return;
    }
    if (!wine_debug) return;
    if (!strcmp( wine_debug, "help" )) debug_usage();
    parse_options( wine_debug );
}

/***********************************************************************
 *		__wine_dbg_get_channel_flags  (NTDLL.@)
 *
 * Get the flags to use for a given channel, possibly setting them too in case of lazy init
 */
unsigned char __cdecl __wine_dbg_get_channel_flags( struct __wine_debug_channel *channel )
{
    int min, max, pos, res;

    if (nb_debug_options == -1) init_options();

    min = 0;
    max = nb_debug_options - 1;
    while (min <= max)
    {
        pos = (min + max) / 2;
        res = strcmp( channel->name, debug_options[pos].name );
        if (!res) return debug_options[pos].flags;
        if (res < 0) max = pos - 1;
        else min = pos + 1;
    }
    /* no option for this channel */
    if (channel->flags & (1 << __WINE_DBCL_INIT)) channel->flags = default_flags;
    return default_flags;
}

/***********************************************************************
 *		__wine_dbg_strdup  (NTDLL.@)
 */
const char * __cdecl __wine_dbg_strdup( const char *str )
{
    struct debug_info *info = get_info();
    unsigned int pos = info->str_pos;
    size_t n = strlen( str ) + 1;

    assert( n <= sizeof(info->strings) );
    if (pos + n > sizeof(info->strings)) pos = 0;
    info->str_pos = pos + n;
    return memcpy( info->strings + pos, str, n );
}

/***********************************************************************
 *		__wine_dbg_write  (NTDLL.@)
 */
int WINAPI __wine_dbg_write( const char *str, unsigned int len )
{
    return write( 2, str, len );
}

/***********************************************************************
 *		__wine_dbg_output  (NTDLL.@)
 */
int __cdecl __wine_dbg_output( const char *str )
{
    struct debug_info *info = get_info();
    const char *end = strrchr( str, '\n' );
    int ret = 0;

    if (end)
    {
        ret += append_output( info, str, end + 1 - str );
        __wine_dbg_write( info->output, info->out_pos );
        info->out_pos = 0;
        str = end + 1;
    }
    if (*str) ret += append_output( info, str, strlen( str ));
    return ret;
}

/***********************************************************************
 *		__wine_dbg_header  (NTDLL.@)
 */
int __cdecl __wine_dbg_header( enum __wine_debug_class cls, struct __wine_debug_channel *channel,
                               const char *function )
{
    static const char * const classes[] = { "fixme", "err", "warn", "trace" };
    struct debug_info *info = get_info();
    char *pos = info->output;

    if (!(__wine_dbg_get_channel_flags( channel ) & (1 << cls))) return -1;

    /* only print header if we are at the beginning of the line */
    if (info->out_pos) return 0;

    if (init_done)
    {
        if (TRACE_ON(timestamp))
        {
            ULONG ticks = NtGetTickCount();
            pos += sprintf( pos, "%3u.%03u:", ticks / 1000, ticks % 1000 );
        }
        if (TRACE_ON(pid)) pos += sprintf( pos, "%04x:", GetCurrentProcessId() );
        pos += sprintf( pos, "%04x:", GetCurrentThreadId() );
    }
    if (function && cls < ARRAY_SIZE( classes ))
        pos += snprintf( pos, sizeof(info->output) - (pos - info->output), "%s:%s:%s ",
                         classes[cls], channel->name, function );
    info->out_pos = pos - info->output;
    return info->out_pos;
}

static size_t sprintf_dbgstr_an( char *buffer, size_t length, const char *str, int n )
{
    static const char hex[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
    char *dst = buffer;

    if (!str) { if (length >= 7) strcpy(buffer, "(null)"); return 6; }
    if (!((ULONG_PTR)str >> 16)) return snprintf( buffer, length, "#%04x", LOWORD(str) );
    if (n == -1) for (n = 0; str[n]; n++) ;
    *dst++ = '"';
    while (n-- > 0 && dst <= buffer + length - 9)
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
    return dst - buffer;
}

static size_t sprintf_dbgstr_wn( char *buffer, size_t length, const WCHAR *str, int n )
{
    static const char hex[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
    char *dst = buffer;

    if (!str) { if (length >= 7) strcpy(buffer, "(null)"); return 6; }
    if (!((ULONG_PTR)str >> 16)) return snprintf( buffer, length, "#%04x", LOWORD(str) );
    if (n == -1) for (n = 0; str[n]; n++) ;
    *dst++ = 'L';
    *dst++ = '"';
    while (n-- > 0 && dst <= buffer + length - 10)
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
    return dst - buffer;
}

static int __cdecl wine_dbg_vsnprintf( char *buffer, size_t length, const char *format, __ms_va_list args )
{
    char fmtbuf[1024];
    char *buf = buffer, *end = buffer + length;
    char *fmt = fmtbuf, *tmp = fmt;
    char old, *spec, *width = NULL, *prec = NULL;
    int ret, w, p;

    assert( strlen( format ) < sizeof(fmtbuf) );
    memcpy( fmtbuf, format, strlen( format ) + 1 );

    while (buf < end && *fmt)
    {
        if (!(tmp = strchr( tmp + 1, '%' ))) tmp = fmt + strlen( fmt );
        else if (fmt[0] == '%' && tmp == fmt + 1) continue;
        old = *tmp;
        *tmp = 0;

        if (fmt[0] != '%') spec = tmp;
        else spec = fmt + 1 + strcspn( fmt + 1, "AacCdeEfFgGinopsSuxXZ%" );

        if (fmt[0] != '%') prec = width = NULL;
        else if (fmt[1] == '-' || fmt[1] == '+' || fmt[1] == ' ' || fmt[1] == '#' || fmt[1] == '0') width = fmt + 2;
        else width = fmt + 1;

        if (!width) w = -1;
        else if (*width == '*') w = va_arg( args, int );
        else if (!(w = atoi( width ))) w = -1;

        if (fmt[0] != '%' || !(prec = strchr( fmt, '.' )) || ++prec >= spec) p = INT_MAX;
        else if (*prec == '*') p = va_arg( args, int );
        else if (!(p = atoi( prec ))) p = INT_MAX;

#define append_checked( b, l, x )                                                                  \
    do { if ((ret = (x)) >= 0 && ret < (l)) b += ret;                                              \
         else if (ret < 0) return ret;                                                             \
         else return b - buffer + ret; } while (0)

        /* dispatch width / precision arguments for all possible %*.*<spec> format specifiers */
#define snprintf_dispatch( b, l, f, a ) \
        append_checked( b, l, (width && *width == '*' ? (prec && *prec == '*' ? snprintf( b, l, f, w, p, a ) \
                                                                              : snprintf( b, l, f, w, a )) \
                                                      : (prec && *prec == '*' ? snprintf( b, l, f, p, a ) \
                                                                              : snprintf( b, l, f, a ))))
#define snprintf_checked( b, l, ... ) append_checked( b, l, snprintf( b, l, ## __VA_ARGS__ ) )

        switch (*spec)
        {
        case 'c':
        case 'C':
            if (spec[-1] == 'l' || spec[-1] == 'w' || (spec[0] == 'C' && spec[-1] != 'h'))
            {
                unsigned int wc = va_arg( args, unsigned int );
                if (wc >= ' ' && wc <= '~') snprintf_checked( buf, end - buf, "%c", wc );
                else snprintf_checked( buf, end - buf, "\\U%04x", wc );
                snprintf_checked( buf, end - buf, spec + 1 );
            }
            else
            {
                snprintf_checked( buf, end - buf, "%c", va_arg( args, int ) );
                snprintf_checked( buf, end - buf, spec + 1 );
            }
            break;
        case 'd':
        case 'i':
        case 'o':
        case 'u':
        case 'x':
        case 'X':
            if (spec[-1] == '4' && spec[-2] == '6' && spec[-3] == 'I')
            {
                spec[-3] = 'j';
                spec[-2] = spec[0];
                spec[-1] = 0;
                snprintf_dispatch( buf, end - buf, fmt, va_arg( args, uintmax_t ) );
                snprintf_checked( buf, end - buf, spec + 1 );
                break;
            }
            if (spec[-1] == '2' && spec[-2] == '3' && spec[-3] == 'I')
            {
                spec[-3] = spec[0];
                spec[-2] = 0;
                snprintf_dispatch( buf, end - buf, fmt, va_arg( args, unsigned int ) );
                snprintf_checked( buf, end - buf, spec + 1 );
                break;
            }

            if (spec[-1] == 'I') spec[-1] = 'z';
            if (spec[-1] == 'j')
                snprintf_dispatch( buf, end - buf, fmt, va_arg( args, uintmax_t ) );
            else if (spec[-1] == 'z')
                snprintf_dispatch( buf, end - buf, fmt, va_arg( args, size_t ) );
            else if (spec[-1] == 't')
                snprintf_dispatch( buf, end - buf, fmt, va_arg( args, ptrdiff_t ) );
            else if (spec[-1] == 'l' && spec[-2] == 'l')
                snprintf_dispatch( buf, end - buf, fmt, va_arg( args, long long int ) );
            else if (spec[-1] == 'l')
                snprintf_dispatch( buf, end - buf, fmt, va_arg( args, long int ) );
            else
                snprintf_dispatch( buf, end - buf, fmt, va_arg( args, int ) );
            break;
        case 's':
        case 'S':
            if (spec[-1] == 'l' || spec[-1] == 'w' || (spec[0] == 'S' && spec[-1] != 'h'))
            {
                WCHAR *wstr = va_arg( args, WCHAR * );
                while (*wstr && p--)
                {
                    if (*wstr >= ' ' && *wstr <= '~') snprintf_checked( buf, end - buf, "%c", *wstr++ );
                    else snprintf_checked( buf, end - buf, "\\U%04x", *wstr++ );
                }
                snprintf_checked( buf, end - buf, spec + 1 );
            }
            else
            {
                char *str = va_arg( args, char * );
                if (spec[-1] != 'l' && spec[-1] != 'w')
                    snprintf_dispatch( buf, end - buf, fmt, str );
                else
                {
                    spec[-1] = 's';
                    spec[0] = 0;
                    snprintf_dispatch( buf, end - buf, fmt, str );
                    snprintf_checked( buf, end - buf, spec + 1 );
                }
            }
            break;
        case 'Z':
            if (spec[-1] == 'l' || spec[-1] == 'w')
            {
                UNICODE_STRING *ptr = va_arg( args, UNICODE_STRING * );
                WCHAR *wstr = ptr->Buffer;
                USHORT len = ptr->Length;
                while (len--)
                {
                    if (*wstr >= ' ' && *wstr <= '~') snprintf_checked( buf, end - buf, "%c", *wstr++ );
                    else snprintf_checked( buf, end - buf, "\\U%04x", *wstr++ );
                }
                snprintf_checked( buf, end - buf, spec + 1 );
            }
            else
            {
                ANSI_STRING *ptr = va_arg( args, ANSI_STRING * );
                char *str = ptr->Buffer;
                USHORT len = ptr->Length;
                snprintf_checked( buf, end - buf, "%.*s", len, str );
                snprintf_checked( buf, end - buf, spec + 1 );
            }
            break;
        case 'p':
            if (!strncmp( spec, "p(astr)", 7 )) /* debugstr_a / debugstr_an */
            {
                append_checked( buf, end - buf, sprintf_dbgstr_an( buf, end - buf, va_arg( args, const char * ), w ) );
                snprintf_checked( buf, end - buf, spec + 7 );
            }
            else if (!strncmp( spec, "p(wstr)", 7 )) /* debugstr_w / debugstr_wn */
            {
                append_checked( buf, end - buf, sprintf_dbgstr_wn( buf, end - buf, va_arg( args, const WCHAR * ), w ) );
                snprintf_checked( buf, end - buf, spec + 7 );
            }
            else snprintf_dispatch( buf, end - buf, fmt, va_arg( args, void * ) );
            break;
        case 'A':
        case 'a':
        case 'e':
        case 'E':
        case 'f':
        case 'F':
        case 'g':
        case 'G':
            if (spec[-1] == 'l') spec[-1] = 'L';
            if (spec[-1] == 'L') snprintf_dispatch( buf, end - buf, fmt, va_arg( args, long double ) );
            else snprintf_dispatch( buf, end - buf, fmt, va_arg( args, double ) );
            break;
        case '%':
        case '\0':
            snprintf_checked( buf, end - buf, fmt );
            break;
        case 'n':
        default:
            fprintf( stderr, "wine_dbg_vsnprintf: unsupported format string: %s\n", fmt );
            break;
        }

#undef snprintf_checked
#undef snprintf_dispatch
#undef append_checked

        *tmp = old;
        fmt = tmp;
    }

    return buf - buffer;
}

/***********************************************************************
 *      __wine_dbg_vprintf  (NTDLL.@)
 */
int __cdecl __wine_dbg_vprintf( const char *format, __ms_va_list args )
{
    char buffer[1024];
    wine_dbg_vsnprintf( buffer, sizeof(buffer), format, args );
    return __wine_dbg_output( buffer );
}


/***********************************************************************
 *      __wine_dbg_vsprintf  (NTDLL.@)
 */
const char * __cdecl __wine_dbg_vsprintf( const char *format, __ms_va_list args )
{
    char buffer[200];
    wine_dbg_vsnprintf( buffer, sizeof(buffer), format, args );
    return __wine_dbg_strdup( buffer );
}


/***********************************************************************
 *		dbg_init
 */
void dbg_init(void)
{
    struct __wine_debug_channel *options, default_option = { default_flags };

    setbuf( stdout, NULL );
    setbuf( stderr, NULL );

    if (nb_debug_options == -1) init_options();

    options = (struct __wine_debug_channel *)((char *)peb + (is_win64 ? 2 : 1) * page_size);
    memcpy( options, debug_options, nb_debug_options * sizeof(*options) );
    free( debug_options );
    debug_options = options;
    options[nb_debug_options] = default_option;
    init_done = TRUE;
}
