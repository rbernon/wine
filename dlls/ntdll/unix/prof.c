/*
 * Profiling functions
 *
 * Copyright 2019 Rémi Bernon for CodeWeavers
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

#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>

#ifdef WINE_UNIX_LIB
#include <time.h>
#endif

#include "wine/prof.h"
#include "wine/debug.h"

WINE_DECLARE_DEBUG_CHANNEL(prof);
WINE_DECLARE_DEBUG_CHANNEL(fprof);

static int trace_on = -1;
static int fprof_on = -1;

static size_t spike_threshold = 1; /* % of period time, spent in a single iteration */
static size_t cumul_threshold = 5; /* % of period time, spent in cumulated iterations */
static size_t period_ticks;
static size_t ticks_per_ns;

static inline size_t prof_ticks(void)
{
    unsigned int hi, lo;
    __asm__ __volatile__ ("mfence; rdtscp; lfence" : "=&d" (hi), "=&a" (lo) :: "%rcx", "memory");
    return (size_t)(((unsigned long long)hi << 32) | (unsigned long long)lo);
}

static __cdecl __attribute__((noinline)) void prof_initialize(void)
{
#ifdef WINE_UNIX_LIB
    struct timespec time_start, time_end;
#else
    LARGE_INTEGER time_start, time_end, frequency;
#endif
    size_t warmup_count = 5000, time_ns, ticks, tick_overhead, rdtsc_overhead = ~0;

#ifdef WINE_UNIX_LIB
    clock_gettime( CLOCK_MONOTONIC, &time_start );
#else
    NtQueryPerformanceCounter( &time_start, &frequency );
    if (!frequency.QuadPart) return;
#endif
    ticks = -prof_ticks();

    while (--warmup_count)
    {
        tick_overhead = -prof_ticks();
        tick_overhead += prof_ticks();
        if (rdtsc_overhead > tick_overhead / 2) rdtsc_overhead = tick_overhead / 2;
    }

#ifdef WINE_UNIX_LIB
    clock_gettime( CLOCK_MONOTONIC, &time_end );
#else
    NtQueryPerformanceCounter( &time_end, &frequency );
#endif
    ticks += prof_ticks();

#ifdef WINE_UNIX_LIB
    time_ns = (time_end.tv_sec - time_start.tv_sec) * 1000 * 1000 * 1000 + (time_end.tv_nsec - time_start.tv_nsec);
#else
    time_ns = (time_end.QuadPart - time_start.QuadPart) * 1000 * 1000 * 1000 / frequency.QuadPart +
              (time_end.QuadPart - time_start.QuadPart) / frequency.QuadPart;
#endif
    ticks_per_ns = (ticks - rdtsc_overhead + time_ns / 2) / time_ns;
    if (ticks_per_ns == 0) ticks_per_ns = 1;

    period_ticks = 1000000000ull / 30 * ticks_per_ns;
}

static char prof_data_pool[4096];
static size_t prof_data_pool_size;

struct __wine_prof_data *__cdecl __wine_prof_data_alloc( size_t size )
{
    size_t alloc_size = sizeof(struct __wine_prof_data) + size, pool_size;
    pool_size = __atomic_add_fetch( &prof_data_pool_size, alloc_size, __ATOMIC_SEQ_CST );
    if (pool_size <= ARRAY_SIZE(prof_data_pool)) return (struct __wine_prof_data *)(prof_data_pool + pool_size - size) - 1;
#ifndef WINE_UNIX_LIB
    return RtlAllocateHeap( GetProcessHeap(), HEAP_ZERO_MEMORY, alloc_size );
#else
    return calloc( 1, alloc_size );
#endif
}

size_t __cdecl __wine_prof_start( struct __wine_prof_data *data )
{
    size_t start_ticks;

    if (!data || !data->name || !trace_on) return 0;
    if (__builtin_expect( trace_on == -1, 0 ) && !(trace_on = TRACE_ON(prof))) return 0;
    if (__builtin_expect( period_ticks == 0, 0 )) prof_initialize();

    start_ticks = prof_ticks();
    if (__atomic_load_n( &data->print_ticks, __ATOMIC_ACQUIRE ) == 0)
        __atomic_compare_exchange_n( &data->print_ticks, &data->print_ticks, start_ticks, 0,
                                     __ATOMIC_RELEASE, __ATOMIC_ACQUIRE );
    return start_ticks;
}

void __cdecl __wine_prof_stop( struct __wine_prof_data *data, size_t start_ticks,
                               const char *file, int line, const char *func, void *retaddr )
{
    size_t stop_ticks, spent_ticks, total_ticks, cumul_ticks, print_ticks, limit_ticks;

    if (!data || !data->name || !trace_on) return;

    stop_ticks = prof_ticks();
    print_ticks = data->print_ticks;
    spent_ticks = stop_ticks - start_ticks;
    total_ticks = stop_ticks - print_ticks;
    cumul_ticks = __atomic_add_fetch( &data->cumul_ticks, spent_ticks, __ATOMIC_ACQUIRE );

    limit_ticks = period_ticks;
    if (data->limit_ns) limit_ticks = data->limit_ns * ticks_per_ns;

    if (spent_ticks * 100 >= limit_ticks * spike_threshold)
        wine_dbg_log( __WINE_DBCL_TRACE, &__wine_dbch_prof, file, line, func, retaddr,
                      "%s: time spike: %" PRIuPTR " (ns) %" PRIuPTR " (%%)\n", data->name,
                      spent_ticks / ticks_per_ns, spent_ticks * 100u / limit_ticks );

    if (total_ticks >= limit_ticks &&
        __atomic_compare_exchange_n( &data->print_ticks, &print_ticks, stop_ticks, 0, __ATOMIC_RELEASE, __ATOMIC_ACQUIRE ))
    {
        if (cumul_ticks * 100 >= total_ticks * cumul_threshold)
            wine_dbg_log( __WINE_DBCL_TRACE, &__wine_dbch_prof, file, line, func, retaddr,
                          "%s: time spent: %" PRIuPTR " (ns) %" PRIuPTR " (%%)\n", data->name,
                          cumul_ticks / ticks_per_ns, cumul_ticks * 100u / total_ticks );
        __atomic_fetch_sub( &data->cumul_ticks, cumul_ticks, __ATOMIC_RELEASE );
    }
}

void __cdecl __wine_prof_frame( struct __wine_prof_frame_data *data,
                                const char *file, int line, const char *function, void *retaddr )
{
    size_t time_ticks, i;

    if (!data || !data->name || !fprof_on) return;
    if (__builtin_expect(fprof_on == -1, 0) && !(fprof_on = TRACE_ON(fprof))) return;
    if (__builtin_expect(period_ticks == 0, 0)) prof_initialize();

    time_ticks = prof_ticks();

    if (!data->prev_ticks)
    {
        data->prev_ticks = time_ticks;
        data->print_ticks = time_ticks;
        return;
    }

    data->time_ticks[data->time_count++] = time_ticks - data->prev_ticks;
    data->prev_ticks = time_ticks;

    if ((time_ticks - data->print_ticks) / 1000 / 1000 < 5000 &&
        data->time_count < 1024)
        return;

    for (i = 0; i < data->time_count; ++i)
    {
        wine_dbg_log( __WINE_DBCL_TRACE, &__wine_dbch_fprof, file, line, function, retaddr,
                      "%s: %" PRIuPTR " %" PRIuPTR "\n", data->name, data->print_ticks / ticks_per_ns,
                      data->time_ticks[i] / ticks_per_ns);
        data->print_ticks += data->time_ticks[i];
    }

    data->time_count = 0;
    data->print_ticks = time_ticks;
}
