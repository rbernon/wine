/*
 * Wine debugging interface
 *
 * Copyright 1999 Patrik Stridvall
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
#pragma makedep install
#endif

#ifndef __WINE_WINE_DEBUG_H
#define __WINE_WINE_DEBUG_H

#include <stdarg.h>
#include <stdio.h>
#include <windef.h>
#include <winbase.h>
#ifndef GUID_DEFINED
#include <guiddef.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct _GUID;

/*
 * Internal definitions (do not use these directly)
 */

enum __wine_debug_class
{
    __WINE_DBCL_FIXME,
    __WINE_DBCL_ERR,
    __WINE_DBCL_WARN,
    __WINE_DBCL_TRACE,

    __WINE_DBCL_INIT = 7  /* lazy init flag */
};

struct __wine_debug_channel
{
    unsigned char flags;
    char name[15];
};

#ifndef WINE_NO_TRACE_MSGS
# define __WINE_GET_DEBUGGING_TRACE(dbch) ((dbch)->flags & (1 << __WINE_DBCL_TRACE))
#else
# define __WINE_GET_DEBUGGING_TRACE(dbch) 0
#endif

#ifndef WINE_NO_DEBUG_MSGS
# define __WINE_GET_DEBUGGING_WARN(dbch)  ((dbch)->flags & (1 << __WINE_DBCL_WARN))
# define __WINE_GET_DEBUGGING_FIXME(dbch) ((dbch)->flags & (1 << __WINE_DBCL_FIXME))
#else
# define __WINE_GET_DEBUGGING_WARN(dbch)  0
# define __WINE_GET_DEBUGGING_FIXME(dbch) 0
#endif

/* define error macro regardless of what is configured */
#define __WINE_GET_DEBUGGING_ERR(dbch)  ((dbch)->flags & (1 << __WINE_DBCL_ERR))

#define __WINE_GET_DEBUGGING(dbcl,dbch)  __WINE_GET_DEBUGGING##dbcl(dbch)

#define __WINE_IS_DEBUG_ON(dbcl,dbch) \
  (__WINE_GET_DEBUGGING##dbcl(dbch) && (__wine_dbg_get_channel_flags(dbch) & (1 << __WINE_DBCL##dbcl)))

#define __WINE_DPRINTF(dbcl,dbch) \
  do { if(__WINE_GET_DEBUGGING(dbcl,(dbch))) { \
       struct __wine_debug_channel * const __dbch = (dbch); \
       const enum __wine_debug_class __dbcl = __WINE_DBCL##dbcl; \
       __WINE_DBG_LOG

#define __WINE_DBG_LOG(...) \
   wine_dbg_log( __dbcl, __dbch, __func__, __VA_ARGS__); } } while(0)

#if (defined(__GNUC__) || defined(__clang__)) && (defined(__MINGW32__) || defined (_MSC_VER) || !defined(__WINE_USE_MSVCRT))
#define __WINE_PRINTF_ATTR(fmt,args) __attribute__((format (printf,fmt,args)))
#else
#define __WINE_PRINTF_ATTR(fmt,args)
#endif

#ifdef WINE_NO_TRACE_MSGS
#define WINE_TRACE(...) do { } while(0)
#define WINE_TRACE_(ch) WINE_TRACE
#endif

#ifdef WINE_NO_DEBUG_MSGS
#define WINE_WARN(...) do { } while(0)
#define WINE_WARN_(ch) WINE_WARN
#define WINE_FIXME(...) do { } while(0)
#define WINE_FIXME_(ch) WINE_FIXME
#endif

struct debug_info
{
    unsigned int str_pos;       /* current position in strings buffer */
    unsigned int out_pos;       /* current position in output buffer */
    char         strings[1020]; /* buffer for temporary strings */
    char         output[1020];  /* current output line */
};

C_ASSERT( sizeof(struct debug_info) == 0x800 );

NTSYSAPI int WINAPI __wine_dbg_write( const char *str, unsigned int len );
extern DECLSPEC_EXPORT int __cdecl __wine_dbg_init( struct __wine_debug_channel **options );
extern DECLSPEC_EXPORT struct debug_info *__cdecl __wine_dbg_get_info(void);
extern DECLSPEC_EXPORT unsigned char __cdecl __wine_dbg_get_channel_flags( struct __wine_debug_channel *channel );
extern DECLSPEC_EXPORT const char * __cdecl __wine_dbg_strdup( const char *str );
extern DECLSPEC_EXPORT int __cdecl __wine_dbg_output( const char *str );
extern DECLSPEC_EXPORT int __cdecl __wine_dbg_header( enum __wine_debug_class cls, struct __wine_debug_channel *channel,
                                                      const char *function );

/*
 * Exported definitions and macros
 */

/* These functions return a printable version of a string, including
   quotes.  The string will be valid for some time, but not indefinitely
   as strings are re-used.  */

#if defined(__x86_64__) && defined(__GNUC__) && defined(__WINE_USE_MSVCRT)
# define __wine_dbg_cdecl __cdecl
#else
# define __wine_dbg_cdecl
#endif

/* winecrtd/sprintf.c */
extern const char * __wine_dbg_cdecl wine_dbg_vsprintf( const char *format, va_list args ) __WINE_PRINTF_ATTR(1,0);
extern const char * __wine_dbg_cdecl wine_dbg_sprintf( const char *format, ... ) __WINE_PRINTF_ATTR(1,2);

/* winecrtd/printf.c */
extern int __wine_dbg_cdecl wine_dbg_vprintf( const char *format, va_list args ) __WINE_PRINTF_ATTR(1,0);
extern int __wine_dbg_cdecl wine_dbg_printf( const char *format, ... ) __WINE_PRINTF_ATTR(1,2);

/* winecrtd/log.c */
extern int __wine_dbg_cdecl wine_dbg_vlog( enum __wine_debug_class cls,
                                           struct __wine_debug_channel *channel, const char *func,
                                           const char *format, va_list args ) __WINE_PRINTF_ATTR(4,0);
extern int __wine_dbg_cdecl wine_dbg_log( enum __wine_debug_class cls,
                                          struct __wine_debug_channel *channel, const char *func,
                                          const char *format, ... ) __WINE_PRINTF_ATTR(4,5);
extern const char *wine_dbgstr_an( const char *str, int n );
extern const char *wine_dbgstr_wn( const WCHAR *str, int n );
static inline const char *wine_dbgstr_a( const char *s ) { return wine_dbgstr_an( s, -1 ); }
static inline const char *wine_dbgstr_w( const WCHAR *s ) { return wine_dbgstr_wn( s, -1 ); }

#if defined(__hstring_h__) && defined(__WINSTRING_H_)
static inline const char *wine_dbgstr_hstring( HSTRING hstr )
{
    UINT32 len;
    const WCHAR *str = WindowsGetStringRawBuffer( hstr, &len );
    return wine_dbgstr_wn( str, len );
}
#endif

extern const char *wine_dbgstr_guid( const GUID *id );
extern const char *wine_dbgstr_fourcc( unsigned int fourcc );
extern const char *wine_dbgstr_point( const POINT *pt );
extern const char *wine_dbgstr_rect( const RECT *rect );
extern const char *wine_dbgstr_longlong( ULONGLONG ll );

#if defined(__oaidl_h__) && defined(V_VT)

extern const char *wine_dbgstr_vt( VARTYPE vt );
extern const char *wine_dbgstr_variant( const VARIANT *v );

#endif /* defined(__oaidl_h__) && defined(V_VT) */

#ifndef WINE_TRACE
#define WINE_TRACE                 __WINE_DPRINTF(_TRACE,__wine_dbch___default)
#define WINE_TRACE_(ch)            __WINE_DPRINTF(_TRACE,&__wine_dbch_##ch)
#endif
#define WINE_TRACE_ON(ch)          __WINE_IS_DEBUG_ON(_TRACE,&__wine_dbch_##ch)

#ifndef WINE_WARN
#define WINE_WARN                  __WINE_DPRINTF(_WARN,__wine_dbch___default)
#define WINE_WARN_(ch)             __WINE_DPRINTF(_WARN,&__wine_dbch_##ch)
#endif
#define WINE_WARN_ON(ch)           __WINE_IS_DEBUG_ON(_WARN,&__wine_dbch_##ch)

#ifndef WINE_FIXME
#define WINE_FIXME                 __WINE_DPRINTF(_FIXME,__wine_dbch___default)
#define WINE_FIXME_(ch)            __WINE_DPRINTF(_FIXME,&__wine_dbch_##ch)
#endif
#define WINE_FIXME_ON(ch)          __WINE_IS_DEBUG_ON(_FIXME,&__wine_dbch_##ch)

#define WINE_ERR                   __WINE_DPRINTF(_ERR,__wine_dbch___default)
#define WINE_ERR_(ch)              __WINE_DPRINTF(_ERR,&__wine_dbch_##ch)
#define WINE_ERR_ON(ch)            __WINE_IS_DEBUG_ON(_ERR,&__wine_dbch_##ch)

#define WINE_DECLARE_DEBUG_CHANNEL(ch) \
    static struct __wine_debug_channel __wine_dbch_##ch = { 0xff, #ch }; \
    C_ASSERT(sizeof(#ch) <= sizeof(__wine_dbch_##ch.name))
#define WINE_DEFAULT_DEBUG_CHANNEL(ch) \
    static struct __wine_debug_channel __wine_dbch_##ch = { 0xff, #ch }; \
    C_ASSERT(sizeof(#ch) <= sizeof(__wine_dbch_##ch.name)); \
    static struct __wine_debug_channel * const __wine_dbch___default = &__wine_dbch_##ch

#define WINE_MESSAGE               wine_dbg_printf

#ifdef __WINESRC__
/* Wine uses shorter names that are very likely to conflict with other software */

static inline const char *debugstr_an( const char * s, int n ) { return wine_dbgstr_an( s, n ); }
static inline const char *debugstr_wn( const WCHAR *s, int n ) { return wine_dbgstr_wn( s, n ); }
static inline const char *debugstr_guid( const struct _GUID *id ) { return wine_dbgstr_guid(id); }
static inline const char *debugstr_fourcc( unsigned int cc ) { return wine_dbgstr_fourcc( cc ); }
static inline const char *debugstr_a( const char *s )  { return wine_dbgstr_an( s, -1 ); }
static inline const char *debugstr_w( const WCHAR *s ) { return wine_dbgstr_wn( s, -1 ); }

#if defined(__hstring_h__) && defined(__WINSTRING_H_)
static inline const char *debugstr_hstring( struct HSTRING__ *s ) { return wine_dbgstr_hstring( s ); }
#endif

#if defined(__oaidl_h__) && defined(V_VT)
static inline const char *debugstr_vt( VARTYPE vt ) { return wine_dbgstr_vt( vt ); }
static inline const char *debugstr_variant( const VARIANT *v ) { return wine_dbgstr_variant( v ); }
#endif

#define TRACE                      WINE_TRACE
#define TRACE_(ch)                 WINE_TRACE_(ch)
#define TRACE_ON(ch)               WINE_TRACE_ON(ch)

#define WARN                       WINE_WARN
#define WARN_(ch)                  WINE_WARN_(ch)
#define WARN_ON(ch)                WINE_WARN_ON(ch)

#define FIXME                      WINE_FIXME
#define FIXME_(ch)                 WINE_FIXME_(ch)
#define FIXME_ON(ch)               WINE_FIXME_ON(ch)

#undef ERR  /* Solaris got an 'ERR' define in <sys/reg.h> */
#define ERR                        WINE_ERR
#define ERR_(ch)                   WINE_ERR_(ch)
#define ERR_ON(ch)                 WINE_ERR_ON(ch)

#define MESSAGE                    WINE_MESSAGE

#endif /* __WINESRC__ */

#ifdef __cplusplus
}
#endif

#endif  /* __WINE_WINE_DEBUG_H */
