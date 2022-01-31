/*
 * Unit test suite for math functions.
 *
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

#include <errno.h>
#include <math.h>
#include <fenv.h>

#include "wine/test.h"

#define STEP 256

static int (*CDECL p__set_FMA3_enable)( int flag );
static typeof(asinf) *p_asinf;
static typeof(fegetround) *p_fegetround;
static typeof(fesetround) *p_fesetround;

union float_value
{
    INT i;
    UINT u;
    float f;
};

static void test_asinf_snan( UINT min, UINT max, UINT mode )
{
    union float_value v, r;
    for (v.u = min; v.u != max; v.u += min( STEP, max - v.u ))
    {
        r.f = p_asinf( v.f );
        ok( r.u == v.u + 0x400000, "asinf %#08x returned %#08x\n", v.u, r.u );
    }
}

static void test_asinf_qnan( UINT min, UINT max, UINT mode )
{
    union float_value v, r;
    for (v.u = min; v.u != max; v.u += min( STEP, max - v.u ))
    {
        r.f = p_asinf( v.f );
        ok( r.u == v.u, "asinf %#08x returned %#08x\n", v.u, r.u );
    }
}

static void test_asinf_edom( UINT min, UINT max, UINT mode )
{
    union float_value v, r;
    for (v.u = min; v.u != max; v.u += min( STEP, max - v.u ))
    {
        r.f = p_asinf( v.f );
        ok( r.u == 0xffc00000, "asinf %#08x returned %#08x\n", v.u, r.u );
    }
}

static void test_asinf_pio2( UINT min, UINT max, UINT mode )
{
    union float_value v, r;
    for (v.u = min; v.u != max; v.u += min( STEP, max - v.u ))
    {
        r.f = p_asinf(v.f);
        ok( r.u == ((r.u & 0x80000000) | 0x3fc90fdb) /* +-pi/2 */,
            "asinf %#08x returned %#08x\n", v.u, r.u );
    }
}

static void test_asinf_tiny( UINT min, UINT max, UINT mode )
{
    union float_value v, r;
    for (v.u = min; v.u != max; v.u += min( STEP, max - v.u ))
    {
        r.f = p_asinf( v.f );
        ok( r.u == v.u, "asinf %#08x returned %#08x\n", v.u, r.u );
    }
}

static float asinf_R( float z )
{
    /* coefficients for R(x^2) */
    static const float p1 = 1.66666672e-01,
                 p2 = -5.11644611e-02,
                 p3 = -1.21124933e-02,
                 p4 = -3.58742251e-03,
                 q1 = -7.56982703e-01;
    float p, q;
    p = z * (p1 + z * (p2 + z * (p3 + z * p4)));
    q = 1.0f + z * q1;
    return p / q;
}

static float asinf_lowr( float x )
{
    float f = fabs( x );
    f = f + f * asinf_R( f * f );
    if (x < 0) return -f;
    return f;
}

static void test_asinf_lowr( UINT min, UINT max, UINT mode )
{
    union float_value v, o, r;
    for (v.u = o.u = min; v.u != max; v.u += min( STEP, max - v.u ), o.u = v.u)
    {
        r.f = p_asinf( v.f );
        o.u ^= 0x80000000;
        o.f = p_asinf( o.f );
        o.u ^= 0x80000000;
        ok( r.u == o.u, "asinf %#08x returned %#08x expected %#08x\n", v.u, r.u, o.u );
        o.f = asinf_lowr( v.f );
        ok( abs( r.i - o.i ) <= 1, "asinf %#08x returned %#08x expected %#08x\n", v.u, r.u, o.u );
    }
}

static float asinf_high( float x )
{
    static const float pio4_hi = 0.785398125648;
    static const float pio2_lo = 7.54978941586e-08;
    float s, z, f, c;

    z = (1 - fabsf( x )) * 0.5f;
    s = sqrtf( z );
    *(unsigned int *)&f = *(unsigned int *)&s & 0xffff0000;
    c = (z - f * f) / (s + f);
    f = pio4_hi - (2 * s * asinf_R( z ) - (pio2_lo - 2 * c) - (pio4_hi - 2 * f));
    if (x < 0) return -f;
    return f;
}

static void test_asinf_high( UINT min, UINT max, UINT mode )
{
    union float_value v, o, r;
    for (v.u = o.u = min; v.u != max; v.u += min( STEP, max - v.u ), o.u = v.u)
    {
        r.f = p_asinf( v.f );
        o.u ^= 0x80000000;
        o.f = p_asinf( o.f );
        o.u ^= 0x80000000;
        ok( r.u == o.u, "asinf %#08x returned %#08x\n", v.u, r.u );
        o.f = asinf_high( v.f );
        ok( abs( r.i - o.i ) <= 1, "asinf %#08x returned %#08x\n", v.u, r.u );
    }
}

struct run_params
{
    void (*callback)( UINT min, UINT max, UINT mode );
    HANDLE event;
    UINT min;
    UINT max;
    UINT mode;
};

static DWORD CALLBACK run_thread( void *args )
{
    static const UINT fp_modes[] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
    static const char *mode_names[] = {"nearest", "down", "up", "toward_zero"};

    struct run_params params = *(struct run_params *)args;

    SetEvent( params.event );

    winetest_push_context( "%s", mode_names[params.mode] );
    if (p__set_FMA3_enable) p__set_FMA3_enable( 0 );

    if (p_fegetround && p_fesetround)
    {
        ok( p_fegetround() == FE_TONEAREST, "fegetround returned %u\n", p_fegetround() );
        p_fesetround( fp_modes[params.mode] );
        ok( p_fegetround() == fp_modes[params.mode], "fegetround returned %u\n", p_fegetround() );
    }

    params.callback( params.min, params.max, params.mode );
    winetest_pop_context();

    return 0;
}

static void run( void (*callback)( UINT min, UINT max, UINT mode ),
                 UINT min, UINT max, UINT count, UINT mode )
{
    struct run_params params = {.callback = callback, .max = min, .mode = mode};
    HANDLE thread[16];

    for (UINT i = 0; i < count; ++i)
    {
        params.min = params.max;
        params.max = params.min + (max - params.min) / (count - i);
        params.event = CreateEventW( NULL, FALSE, FALSE, NULL );
        thread[i] = CreateThread( NULL, 0, run_thread, &params, 0, NULL );
        WaitForSingleObject( params.event, INFINITE );
        CloseHandle( params.event );
    }

    WaitForMultipleObjects( count, thread, TRUE, INFINITE );
    while (count--) CloseHandle( thread[count] );
}

START_TEST( math )
{
    HMODULE ucrtbase;
    UINT mode;

    if ((ucrtbase = LoadLibraryW( L"ucrtbase" )))
    {
        p__set_FMA3_enable = (void *)GetProcAddress( ucrtbase, "_set_FMA3_enable" );
        ok( !!p__set_FMA3_enable, "_set_FMA3_enable not found\n" );
        p_fegetround = (void *)GetProcAddress( ucrtbase, "fegetround" );
        ok( !!p_fegetround, "fegetround not found\n" );
        p_fesetround = (void *)GetProcAddress( ucrtbase, "fesetround" );
        ok( !!p_fesetround, "fesetround not found\n" );
        p_asinf = (void *)GetProcAddress( ucrtbase, "asinf" );
        ok( !!p_asinf, "asinf not found\n" );
    }
    ok( !!ucrtbase, "ucrtbase not found\n" );

    if (!p_asinf) skip( "asinf not found, skipping tests\n" );
    else for (mode = 0; mode < 4; ++mode)
    {
        if (1) run( test_asinf_tiny, 0x00000000, 0x38800000, 8, mode );
        if (1) run( test_asinf_pio2, 0x3f800000, 0x3f800001, 1, mode );
        if (1) run( test_asinf_edom, 0x3f800001, 0x7f800001, 8, mode );
        if (1) run( test_asinf_snan, 0x7f800001, 0x7fc00000, 8, mode );
        if (1) run( test_asinf_qnan, 0x7fc00000, 0x80000000, 8, mode );

        if (1) run( test_asinf_tiny, 0x80000000, 0xb8800000, 8, mode );
        if (1) run( test_asinf_pio2, 0xbf800000, 0xbf800001, 1, mode );
        if (1) run( test_asinf_edom, 0xbf800001, 0xff800001, 8, mode );
        if (1) run( test_asinf_snan, 0xff800001, 0xffc00000, 8, mode );
        if (1) run( test_asinf_qnan, 0xffc00000, 0x00000000, 8, mode );

        if (1) run( test_asinf_lowr, 0x38800000, 0x3f000000, 8, mode );
        if (1) run( test_asinf_high, 0x3f000000, 0x3f800000, 8, mode );
        if (1) run( test_asinf_lowr, 0xb8800000, 0xbf000000, 8, mode );
        if (1) run( test_asinf_high, 0xbf000000, 0xbf800000, 8, mode );
    }

    FreeLibrary( ucrtbase );
}
