/*
 * DllMainCRTStartup default entry point
 *
 * Copyright 2019 Jacek Caban for CodeWeavers
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

#include <stdarg.h>
#include <stdio.h>
#include "windef.h"
#include "winbase.h"

#include "corecrt_startup.h"

#if defined(_MSC_VER)
#define _CRTALLOC(x) __declspec(allocate(x))
#elif defined(__GNUC__)
#define _CRTALLOC(x) __attribute__((section(x)))
#else
#define _CRTALLOC(x) static /* unsupported */
#endif

_CRTALLOC(".CRT$XIA") void (*__xi_a)(void) = 0;
_CRTALLOC(".CRT$XIZ") void (*__xi_z)(void) = 0;
_CRTALLOC(".CRT$XCA") void (*__xc_a)(void) = 0;
_CRTALLOC(".CRT$XCZ") void (*__xc_z)(void) = 0;

#if defined(__GNUC__)
extern void (*__CTOR_LIST__[])(void) __attribute__((weak));
extern void (*__DTOR_LIST__[])(void) __attribute__((weak));
#endif

static CRITICAL_SECTION onexit_cs;
static CRITICAL_SECTION_DEBUG onexit_cs_debug =
{
    0, 0, &onexit_cs,
    { &onexit_cs_debug.ProcessLocksList, &onexit_cs_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": onexit_cs") }
};
static CRITICAL_SECTION onexit_cs = { &onexit_cs_debug, -1, 0, 0, 0, 0 };
static _onexit_t *onexit_begin, *onexit_end;

static void _CRT_INIT(void)
{
    void (**ctor)(void);
    if ((ctor = &__xi_a)) while (++ctor != &__xi_z && *ctor) (*ctor)();
    if ((ctor = &__xc_a)) while (++ctor != &__xc_z && *ctor) (*ctor)();
#if defined(__GNUC__)
    if ((ctor = __CTOR_LIST__) && *ctor == (void *)-1) while (++ctor != __DTOR_LIST__ && *ctor) (*ctor)();
#endif
}

static _onexit_t dll_onexit( _onexit_t func, _onexit_t **start, _onexit_t **end )
{
    int len = (*end - *start);
    _onexit_t *tmp;

    if (!func || ++len <= 0) return NULL;
    if (!(tmp = HeapReAlloc( GetProcessHeap(), 0, *start, len * sizeof(*tmp) ))) return NULL;
    *start = tmp;
    *end = tmp + len;
    tmp[len - 1] = func;

    return func;
}

int __cdecl atexit( void (__cdecl *func)(void) )
{
    int ret;
    EnterCriticalSection( &onexit_cs );
    ret = dll_onexit( (_onexit_t)func, &onexit_begin, &onexit_end ) == (_onexit_t)func;
    LeaveCriticalSection( &onexit_cs );
    return ret;
}

BOOL WINAPI DllMainCRTStartup( HINSTANCE inst, DWORD reason, void *reserved )
{
    BOOL ret;

    if (reason == DLL_PROCESS_ATTACH) _CRT_INIT();

    ret = DllMain( inst, reason, reserved );

    if (reason == DLL_PROCESS_DETACH)
    {
        while (onexit_end-- > onexit_begin) (**onexit_end)();
        HeapFree( GetProcessHeap(), 0, onexit_begin );
    }

    return ret;
}

BOOL WINAPI _DllMainCRTStartup( HINSTANCE inst, DWORD reason, void *reserved )
{
    return DllMainCRTStartup( inst, reason, reserved );
}

#endif
