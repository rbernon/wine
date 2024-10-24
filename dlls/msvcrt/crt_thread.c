/*
 * mainCRTStartup default entry point
 *
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

#if 0
#pragma makedep implib
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <process.h>

#include "windef.h"
#include "winbase.h"
#include "winternl.h"

/*
    // Pseudo code for the test:
    // if (TSS > _Init_thread_epoch) {
    //   _Init_thread_header(&TSS);
    //   if (TSS == -1) {
    //     ... initialize the object ...;
    //     _Init_thread_footer(&TSS);
    //   }
    // }
    //
    // The algorithm is almost identical to what can be found in the appendix
    // found in N2325.
*/

static CRITICAL_SECTION init_thread_cs;
static CRITICAL_SECTION_DEBUG init_thread_debug =
{
    0, 0, &init_thread_cs,
    { &init_thread_debug.ProcessLocksList, &init_thread_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": init_thread_cs") }
};
static CRITICAL_SECTION init_thread_cs = { &init_thread_debug, -1, 0, 0, 0, 0 };
static CONDITION_VARIABLE init_thread_cv;

void _Init_thread_lock(void)
{
    EnterCriticalSection( &init_thread_cs );
}

void _Init_thread_unlock(void)
{
    LeaveCriticalSection( &init_thread_cs );
}

void _Init_thread_wait( int *once )
{
    while (*once == -1) SleepConditionVariableCS( &init_thread_cv, &init_thread_cs, INFINITE );
}

void _Init_thread_notify(void)
{
    WakeAllConditionVariable( &init_thread_cv );
}

int _Init_global_epoch = INT_MIN;
__thread int _Init_thread_epoch = INT_MIN;

void _Init_thread_header( int *once )
{
    _Init_thread_lock();
    _Init_thread_wait( once );

    if (!*once)
    {
        *once = -1;
        _Init_thread_unlock();
        return;
    }

    _Init_thread_epoch = _Init_global_epoch;
    _Init_thread_unlock();
}

void _Init_thread_footer( int *once )
{
    _Init_thread_lock();
    _Init_global_epoch++;
    *once = _Init_global_epoch;
    _Init_thread_epoch = _Init_global_epoch;
    _Init_thread_unlock();
    _Init_thread_notify();
}

void _Init_thread_abort( int *once )
{
    _Init_thread_lock();
    *once = 0;
    _Init_thread_unlock();
    _Init_thread_notify();
}
