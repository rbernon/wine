/*
 * Fallbacks for debugging functions when running on Windows
 *
 * Copyright 2019 Alexandre Julliard
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

#ifdef __WINE_PE_BUILD

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#define NONAMELESSUNION
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "ddk/wdm.h"

#include "wine/debug.h"
#include "wine/heap.h"

WINE_DECLARE_DEBUG_CHANNEL(pid);
WINE_DECLARE_DEBUG_CHANNEL(thread);
WINE_DECLARE_DEBUG_CHANNEL(timestamp);
WINE_DECLARE_DEBUG_CHANNEL(microsecs);
WINE_DECLARE_DEBUG_CHANNEL(address);

struct winedebug_mapping
{
    DWORD tls_index;
};

static HANDLE winedebug_section;
static struct winedebug_mapping *winedebug_mapping;

static NTSTATUS initialize_winedebug_section(void)
{
    OBJECT_ATTRIBUTES root_attrs, attrs;
    UNICODE_STRING root_name, name;
    WCHAR buffer[MAX_PATH];
    HANDLE root, section;
    LARGE_INTEGER size;
    NTSTATUS status;

    if (winedebug_section) return STATUS_SUCCESS;

    swprintf( buffer, ARRAY_SIZE(buffer), L"\\Sessions\\%u\\BaseNamedObjects",
              NtCurrentTeb()->Peb->SessionId );
    RtlInitUnicodeString( &root_name, buffer );
    InitializeObjectAttributes( &root_attrs, &root_name, 0, 0, NULL );
    status = NtOpenDirectoryObject( &root, DIRECTORY_CREATE_OBJECT | DIRECTORY_TRAVERSE, &root_attrs );
    assert( !status );

    swprintf( buffer, ARRAY_SIZE(buffer), L"Local\\winedebug_section_%08x",
              NtCurrentTeb()->ClientId.UniqueProcess );
    RtlInitUnicodeString( &name, buffer );
    InitializeObjectAttributes( &attrs, &name, OBJ_OPENIF, root, NULL );

    size.QuadPart = 0x1000;
    status = NtCreateSection( &section, STANDARD_RIGHTS_REQUIRED | SECTION_QUERY | SECTION_MAP_READ | SECTION_MAP_WRITE,
                              &attrs, &size, PAGE_READWRITE, SEC_COMMIT, 0 );
    assert( status == STATUS_SUCCESS || status == STATUS_OBJECT_NAME_EXISTS );
    NtClose( root );

    if (InterlockedCompareExchangePointer( &winedebug_section, section, NULL ))
        NtClose( section );

    swprintf( buffer, ARRAY_SIZE(buffer), L"winedebug_section %p, status %#x\n", winedebug_section, status );
    fwrite( buffer, 1, wcslen(buffer) * sizeof(WCHAR), stderr );

    return STATUS_SUCCESS;
}

static NTSTATUS initialize_winedebug_mapping(void)
{
    struct winedebug_mapping *mapping = NULL;
    SIZE_T size = 0x1000;
    NTSTATUS status;

    if (winedebug_mapping) return STATUS_SUCCESS;

    if ((status = initialize_winedebug_section())) return status;
    if ((status = NtMapViewOfSection( winedebug_section, GetCurrentProcess(), (void **)&mapping,
                                      0, 0, NULL, &size, ViewUnmap, 0, PAGE_READWRITE )))
        return status;

    mapping->tls_index = TlsAlloc();

    if (InterlockedCompareExchangePointer( (void **)&winedebug_mapping, mapping, NULL ))
    {
        TlsFree( mapping->tls_index );
        NtUnmapViewOfSection( GetCurrentProcess(), mapping );
    }

    return STATUS_SUCCESS;
}

struct debug_info
{
    unsigned int str_pos;       /* current position in strings buffer */
    unsigned int out_pos;       /* current position in output buffer */
    char         strings[1020]; /* buffer for temporary strings */
    char         output[1020];  /* current output line */
};

C_ASSERT( sizeof(struct debug_info) == 0x800 );

static inline struct debug_info *get_info(void)
{
    struct debug_info *info;
    NTSTATUS status;

    if ((status = initialize_winedebug_mapping())) return NULL;
    if ((info = TlsGetValue( winedebug_mapping->tls_index ))) return info;
    info = RtlAllocateHeap( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(struct debug_info) );
    TlsSetValue( winedebug_mapping->tls_index, info );
    return info;
}

static int (WINAPI *p__wine_dbg_write)( const char *str, unsigned int len );

static const char * const debug_classes[] = { "fixme", "err", "warn", "trace" };

static unsigned char default_flags = (1 << __WINE_DBCL_ERR) | (1 << __WINE_DBCL_FIXME);
static int nb_debug_options = -1;
static int options_size;
static struct __wine_debug_channel *debug_options;
static DWORD partial_line_tid;  /* id of the last thread to output a partial line */

static void load_func( void **func, const char *name, void *def )
{
    if (!*func)
    {
        DWORD err = GetLastError();
        HMODULE module = GetModuleHandleA( "ntdll.dll" );
        void *proc = GetProcAddress( module, name );
        InterlockedExchangePointer( func, proc ? proc : def );
        SetLastError( err );
    }
}
#define LOAD_FUNC(name) load_func( (void **)&p ## name, #name, fallback ## name )

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
        debug_options = heap_realloc( debug_options, options_size * sizeof(debug_options[0]) );
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

    if (!(options = _strdup(str))) return;
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

/* initialize all options at startup */
static void init_options(void)
{
    char *wine_debug = getenv("WINEDEBUG");
    NTSTATUS status;

    struct debug_info *info = get_info();
    assert( info );

    nb_debug_options = 0;
    if (wine_debug) parse_options( wine_debug );
}

/* add a string to the output buffer */
static int append_output( struct debug_info *info, const char *str, size_t len )
{
    if (len >= sizeof(info->output) - info->out_pos)
    {
        __wine_dbg_write( info->output, info->out_pos );
        info->out_pos = 0;
        ERR_(thread)( "debug buffer overflow:\n" );
        __wine_dbg_write( str, len );
        RtlRaiseStatus( STATUS_BUFFER_OVERFLOW );
    }
    memcpy( info->output + info->out_pos, str, len );
    info->out_pos += len;
    return len;
}

/***********************************************************************
 *      __wine_dbg_get_channel_flags  (NTDLL.@)
 *
 * Get the flags to use for a given channel, possibly setting them too in case of lazy init
 */
unsigned char __cdecl __wine_dbg_get_channel_flags( struct __wine_debug_channel *channel )
{
    int min, max, pos, res;
    unsigned char default_flags;

    if (!debug_options) init_options();

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
    default_flags = debug_options[nb_debug_options].flags;
    if (channel->flags & (1 << __WINE_DBCL_INIT)) channel->flags = default_flags;
    return default_flags;
}

/***********************************************************************
 *      __wine_dbg_strdup  (NTDLL.@)
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
 *      __wine_dbg_header  (NTDLL.@)
 */
int __cdecl __wine_dbg_header( enum __wine_debug_class cls, struct __wine_debug_channel *channel,
                               void *ret, void *addr, const char *function )
{
    static const char * const classes[] = { "fixme", "err", "warn", "trace" };
    struct debug_info *info = get_info();
    char *pos = info->output;

    if (!(__wine_dbg_get_channel_flags( channel ) & (1 << cls))) return -1;

    /* only print header if we are at the beginning of the line */
    if (info->out_pos) return 0;

    if (TRACE_ON(microsecs))
    {
        LARGE_INTEGER counter, frequency, microsecs;
        NtQueryPerformanceCounter(&counter, &frequency);
        microsecs.QuadPart = counter.QuadPart * 1000000 / frequency.QuadPart;
        pos += sprintf( pos, "%3u.%06u:", (unsigned int)(microsecs.QuadPart / 1000000), (unsigned int)(microsecs.QuadPart % 1000000) );
    }
    else if (TRACE_ON(timestamp))
    {
        ULONG ticks = NtGetTickCount();
        pos += sprintf( pos, "%3u.%03u:", ticks / 1000, ticks % 1000 );
    }
    if (TRACE_ON(pid)) pos += sprintf( pos, "%04x:%9lu:", GetCurrentProcessId(), (ULONG_PTR)NtCurrentTeb()->SystemReserved1[0] );
    pos += sprintf( pos, "%04x:%9lu:", GetCurrentThreadId(), (ULONG_PTR)NtCurrentTeb()->SystemReserved1[1] );
    if (TRACE_ON(address)) pos += sprintf( pos, "%p:%p:", ret, addr );
    if (function && cls < ARRAY_SIZE( classes ))
        pos += snprintf( pos, sizeof(info->output) - (pos - info->output), "%s:%s:%s ",
                         classes[cls], channel->name, function );
    info->out_pos = pos - info->output;
    return info->out_pos;
}

/***********************************************************************
 *      __wine_dbg_output  (NTDLL.@)
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

static int fallback__wine_dbg_write( const char *str, unsigned int len )
{
    return fwrite( str, 1, len, stderr );
}

int WINAPI __wine_dbg_write( const char *str, unsigned int len )
{
    LOAD_FUNC( __wine_dbg_write );
    return fallback__wine_dbg_write( str, len );
}

#endif  /* __WINE_PE_BUILD */
