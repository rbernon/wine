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

#include "intrin.h"
#include "libbf.h"

#include "wine/asm.h"
#include "wine/test.h"

#define STEP 1

static int (*CDECL p__set_FMA3_enable)( int flag );
static typeof(asinf) *p_asinf;
static typeof(acosf) *p_acosf;
static typeof(sqrtf) *p_sqrtf;
static typeof(atanf) *p_atanf;
static typeof(atan2f) *p_atan2f;
static typeof(fegetround) *p_fegetround;
static typeof(fesetround) *p_fesetround;

union float_value
{
    INT i;
    UINT u;
    float f;
};

float CDECL sse2_sqrtf(float);
__ASM_GLOBAL_FUNC( sse2_sqrtf,
        "sqrtss %xmm0, %xmm0\n\t"
        "ret" )

static void test_asinf_nan( UINT min, UINT max, UINT mode )
{
    union float_value v, r;
    for (v.u = min; v.u != max; v.u += min( STEP, max - v.u ))
    {
        r.f = p_asinf( v.f );
        ok( r.u == (v.u | 0x400000), "asinf %#08x returned %#08x\n", v.u, r.u );
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

static void test_asinf_sign( UINT min, UINT max, UINT mode )
{
    union float_value v, o, r;
    for (v.u = o.u = min; v.u != max; v.u += min( STEP, max - v.u ), o.u = v.u)
    {
        r.f = p_asinf( v.f );
        o.u ^= 0x80000000;
        o.f = p_asinf( o.f );
        o.u ^= 0x80000000;
        ok( r.u == o.u, "asinf %#08x returned %#08x\n", v.u, r.u );
    }
}

static void test_asinf_tiny2( UINT min, UINT max, UINT mode )
{
    union float_value v, r;
    for (v.u = min; v.u != max; v.u += min( STEP, max - v.u ))
    {
        r.f = p_asinf( v.f );
        ok( abs( r.i - v.i ) <= 1, "asinf %#08x returned %#08x\n", v.u, r.u );
    }
}

static float asinf_R( float z )
{
    static const double Pn[] = {-3.5892746177188881e-03, -1.2124695893780329e-02,
                                -5.1102040203526725e-02,  1.6666666757062301e-01, 0};
    static const double Qn[] = {-2.4391734864756136e-04, -7.5660791231708657e-01, 1};
    double p = 0, q = 0, Z = z;
    int i;

    for (i = 0; i < ARRAY_SIZE(Pn); ++i) p = Pn[i] + Z * p;
    for (i = 0; i < ARRAY_SIZE(Qn); ++i) q = Qn[i] + Z * q;

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
        ok( r.u == o.u, "asinf %#08x returned %#08x\n", v.u, r.u );
        o.f = asinf_lowr( v.f );
        ok( abs( r.i - o.i ) <= 1, "asinf %#08x returned %#08x\n", v.u, r.u );
    }
}

static void test_asinf_part( UINT min, UINT max, UINT mode )
{
    union float_value v, r, p = {.u = min};
    p.f = p_asinf( p.f );
    for (v.u = min; v.u != max; v.u += min( STEP, max - v.u ))
    {
        r.f = p_asinf( v.f );
        ok( (r.u & 0x7f800000) == (p.u & 0x7f800000), "split %08x\n", v.u );
        p.u = r.u;
    }
}

static float asinf_high( float x )
{
    static const float pio4_hi = 0.785398125648;
    static const float pio2_lo = 7.54978941586e-08;
    float s, z, f, c;

    z = (1 - fabsf( x )) * 0.5f;
    s = sse2_sqrtf( z );
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

static void test_acosf_nan( UINT min, UINT max, UINT mode )
{
    union float_value v, r;
    for (v.u = min; v.u != max; v.u += min(STEP, max - v.u))
    {
        r.f = p_acosf(v.f);
        ok( r.u == (v.u | 0x400000), "acosf %#08x returned %#08x\n", v.u, r.u );
    }
}

static void test_acosf_edom( UINT min, UINT max, UINT mode )
{
    union float_value v, r;
    for (v.u = min; v.u != max; v.u += min(STEP, max - v.u))
    {
        r.f = p_acosf(v.f);
        ok( r.u == 0xffc00000, "acosf %#08x returned %#08x\n", v.u, r.u );
    }
}

static void test_acosf_pi( UINT min, UINT max, UINT mode )
{
    union float_value v, r;
    for (v.u = min; v.u != max; v.u += min(STEP, max - v.u))
    {
        r.f = p_acosf(v.f);
        ok( r.u == (v.u >> 31) * 0x40490fdb, "acosf %#08x returned %#08x\n", v.u, r.u );
    }
}

static void test_acosf_tiny( UINT min, UINT max, UINT mode )
{
    union float_value v, r;
    for (v.u = min; v.u != max; v.u += min(STEP, max - v.u))
    {
        r.f = p_acosf(v.f);
        ok( r.u == 0x3fc90fdb /* pi/2 */, "acosf %#08x returned %#08x\n", v.u, r.u );
    }
}

static float acosf_lowr( float x )
{
    static const double pio2_lo = 6.12323399573676603587e-17;
    static const double pio2_hi = 1.57079632679489655800e+00;
    return pio2_hi - (x - (pio2_lo - x * asinf_R(x * x)));
}

static void test_acosf_lowr( UINT min, UINT max, UINT mode )
{
    union float_value v, o, r;
    for (v.u = o.u = min; v.u != max; v.u += min(STEP, max - v.u), o.u = v.u)
    {
        r.f = p_acosf(v.f);
        o.f = p_asinf(v.f) + r.f;
        ok( o.u == 0x3fc90fdb || o.u == 0x3fc90fda || o.u == 0x3fc90fd9 || o.u == 0x3fc90fdc,
            "acosf %#08x returned %#08x\n", v.u, r.u );
        o.f = acosf_lowr( v.f );
        ok( abs( r.i - o.i ) <= 1, "acosf %#08x returned %#08x\n", v.u, r.u );
    }
}

static void test_acosf_part( UINT min, UINT max, UINT mode )
{
    union float_value v, r, p = {.u = min};
    p.f = p_acosf( p.f );
    for (v.u = min; v.u != max; v.u += min( STEP, max - v.u ))
    {
        r.f = p_acosf( v.f );
        ok( (r.u & 0x7f800000) == (p.u & 0x7f800000), "split %08x\n", v.u );
        p.u = r.u;
    }
}

static float acosf_hneg( float x )
{
    static const double pio2_lo = 6.12323399573676603587e-17;
    static const double pio2_hi = 1.57079632679489655800e+00;
    float z = (1 + x) * 0.5f;
    float s = sse2_sqrtf(z);
    return 2*(pio2_hi - (s + (asinf_R(z) * s - pio2_lo)));
}

static void test_acosf_hneg( UINT min, UINT max, UINT mode )
{
    union float_value v, o, r;
    for (v.u = o.u = min; v.u != max; v.u += min(STEP, max - v.u), o.u = v.u)
    {
        r.f = p_acosf(v.f);
        o.f = p_asinf(v.f) + r.f;
        ok( o.u == 0x3fc90fdb || o.u == 0x3fc90fda || o.u == 0x3fc90fd9 || o.u == 0x3fc90fdc,
            "acosf %#08x returned %#08x\n", v.u, r.u );
        o.f = acosf_hneg( v.f );
        ok( abs( r.i - o.i ) <= 1, "acosf %#08x returned %#08x\n", v.u, r.u );
    }
}

static float acosf_hpos( float x )
{
    float z = (1 - x) * 0.5f;
    float s = sse2_sqrtf(z);
    unsigned int hx = *(unsigned int*)&s & 0xffff0000;
    float df = *(float*)&hx;
    float c = (z - df * df) / (s + df);
    float w = asinf_R(z) * s + c;
    return 2 * (df + w);
}

static void test_acosf_hpos( UINT min, UINT max, UINT mode )
{
    union float_value v, o, r;
    for (v.u = o.u = min; v.u != max; v.u += min(STEP, max - v.u), o.u = v.u)
    {
        r.f = p_acosf(v.f);
        o.f = acosf_hpos( v.f );
        ok( abs( r.i - o.i ) <= 1, "acosf %#08x returned %#08x\n", v.u, r.u );
    }
}

static void test_sqrtf( UINT min, UINT max, UINT mode )
{
    union float_value v, o, r;
    for (v.u = o.u = min; v.u != max; v.u += min(STEP, max - v.u), o.u = v.u)
    {
        r.f = p_sqrtf(v.f);
        o.f = sse2_sqrtf(v.f);
        ok( o.u == r.u, "sqrtf %#08x returned %#08x expected %#08x\n", v.u, r.u, o.u );
    }
}

static void dump_asinf( UINT min, UINT max, UINT mode )
{
    union float_value v, *r;
    char buffer[256];
    FILE *out;

    r = malloc((max - min) * sizeof(*r));
    for (v.u = min; v.u != max; v.u++) r[v.u - min].f = p_asinf(v.f);

    sprintf( buffer, "asinf-%08x-%08x-%u-%s.dat", min, max, mode, strcmp( winetest_platform, "wine" ) ? "msvc" : "wine" );
    out = fopen(buffer, "wb");
    fwrite(r, max - min, sizeof(*r), out);
    fflush(out);
    fclose(out);
    free(r);
}

union float_value asinf_lower_bound(union float_value min, union float_value max, union float_value v, union float_value r)
{
    union float_value it;
    unsigned int step;
 
    while (max.u != min.u)
    {
        step = (max.u - min.u) / 2;
        it.u = min.u + step;
        it.f = v.f + v.f * it.f;
        if (it.u == r.u) max.u = min.u + step;
        else min.u = min.u + step + 1;
    }

    it.f = v.f + v.f * min.f;
    if (it.f != r.f) min.u += 1;
    return min;
}

static void dump_asinf_core_min( UINT min, UINT max, UINT mode )
{
    union float_value v, a, o, *r;
    char buffer[256];
    FILE *out;

    r = malloc((max - min) * sizeof(*r));
    sprintf(buffer, "asinf-%08x-%08x-%u-%s.dat", min, max, mode, strcmp( winetest_platform, "wine" ) ? "msvc" : "wine" );
    out = fopen(buffer, "rb");
    fread(r, max - min, sizeof(*r), out);
    fclose(out);

    for (v.u = min; v.u < max; v.u++)
    {
        o.u = 0;
        a.f = (r[v.u - min].f - v.f) / v.f;
        r[v.u - min] = asinf_lower_bound(o, a, v, r[v.u - min]);
    }

    sprintf(buffer, "asinf-core-min-%08x-%08x-%u.dat", min, max, mode);
    out = fopen(buffer, "wb");
    fwrite(r, max - min, sizeof(*r), out);
    fclose(out);
    free(r);
}

union float_value asinf_upper_bound(union float_value min, union float_value max, union float_value v, union float_value r)
{
    union float_value it;
    unsigned int step;
 
    while (max.u != min.u)
    {
        step = (max.u - min.u) / 2;
        it.u = max.u - step;
        it.f = v.f + v.f * it.f;
        if (it.u == r.u) min.u = max.u - step;
        else max.u = max.u - step - 1;
    }

    it.f = v.f + v.f * max.f;
    if (it.f != r.f) max.u -= 1;
    return max;
}

static void dump_asinf_core_max( UINT min, UINT max, UINT mode )
{
    union float_value v, a, o, *r;
    char buffer[256];
    FILE *out;

    r = malloc((max - min) * sizeof(*r));
    sprintf(buffer, "asinf-%08x-%08x-%u-%s.dat", min, max, mode, strcmp( winetest_platform, "wine" ) ? "msvc" : "wine" );
    out = fopen(buffer, "rb");
    fread(r, max - min, sizeof(*r), out);
    fclose(out);

    for (v.u = min; v.u < max; v.u++)
    {
        o.u = 0x7f800000;
        a.f = (r[v.u - min].f - v.f) / v.f;
        r[v.u - min] = asinf_upper_bound(a, o, v, r[v.u - min]);
    }

    sprintf(buffer, "asinf-core-max-%08x-%08x-%u.dat", min, max, mode);
    out = fopen(buffer, "wb");
    fwrite(r, max - min, sizeof(*r), out);
    fclose(out);
    free(r);
}

static void *bf_realloc_func(void *opaque, void *ptr, size_t size)
{
    return realloc(ptr, size);
}

static void dump_asin_bf( UINT min, UINT max, UINT mode )
{
    bf_flags_t bf_round = mode == 0 ? BF_RNDN : mode == 1 ? BF_RNDD : mode == 2 ? BF_RNDU : BF_RNDZ;
    bf_t bf_a, bf_v;
    union float_value v, *r;
    double *asin, tmp;
    char buffer[256];
    bf_context_t bf;
    float *asinf;
    FILE *out;
    int res;

    bf_context_init(&bf, bf_realloc_func, NULL);
    bf_init(&bf, &bf_a);
    bf_init(&bf, &bf_v);

    r = malloc((max - min) * sizeof(*r));
    asin = malloc((max - min) * sizeof(*asin));
    asinf = malloc((max - min) * sizeof(*asinf));
    for (v.u = min; v.u != max; v.u++)
    {
        res = bf_set_float64(&bf_v, v.f);
        ok(res == 0, "bf_set_float64 returned %d\n", res);
        bf_normalize_and_round(&bf_v, 24, bf_set_exp_bits(8) | bf_round);

        res = bf_asin(&bf_a, &bf_v, 112, bf_set_exp_bits(15) | bf_round);
        ok(res == BF_ST_INEXACT, "bf_asin returned %d\n", res);
        bf_get_float64(&bf_a, &asin[v.u - min], bf_round);
        asinf[v.u - min] = asin[v.u - min];

        bf_sub(&bf_a, &bf_a, &bf_v, BF_PREC_INF, bf_round);
        bf_div(&bf_a, &bf_a, &bf_v, 112, bf_set_exp_bits(15) | bf_round);
        bf_normalize_and_round(&bf_a, 24, bf_set_exp_bits(8) | bf_round);
        bf_mul(&bf_a, &bf_a, &bf_v, 24, bf_set_exp_bits(8) | bf_round);
        bf_add(&bf_a, &bf_a, &bf_v, 24, bf_set_exp_bits(8) | bf_round);

        bf_get_float64(&bf_a, &tmp, bf_round);
        r[v.u - min].f = tmp;
    }

    bf_delete(&bf_v);
    bf_delete(&bf_a);
    bf_context_end(&bf);

    sprintf(buffer, "asinf-%08x-%08x-%u-bf-asinf.dat", min, max, mode);
    out = fopen(buffer, "wb");
    fwrite(asinf, max - min, sizeof(*asinf), out);
    fclose(out);
    free(asinf);

    sprintf(buffer, "asinf-%08x-%08x-%u-bf-asin.dat", min, max, mode);
    out = fopen(buffer, "wb");
    fwrite(asin, max - min, sizeof(*asin), out);
    fclose(out);
    free(asin);

    sprintf(buffer, "asinf-%08x-%08x-%u-bf-maddx.dat", min, max, mode);
    out = fopen(buffer, "wb");
    fwrite(r, max - min, sizeof(*r), out);
    fclose(out);
    free(r);
}

static float asinf_bf_high( float x, float asin_r )
{
    static const float pio4_hi = 0.785398125648;
    static const float pio2_lo = 7.54978941586e-08;
    float s, z, f, c;

    z = (1 - fabsf( x )) * 0.5f;
    s = sse2_sqrtf( z );
    *(unsigned int *)&f = *(unsigned int *)&s & 0xffff0000;
    c = (z - f * f) / (s + f);
    f = pio4_hi - (2 * s * asin_r - (pio2_lo - 2 * c) - (pio4_hi - 2 * f));
    if (x < 0) return -f;
    return f;
}

static void dump_asin_bf_high( UINT min, UINT max, UINT mode )
{
    bf_flags_t bf_round = mode == 0 ? BF_RNDN : mode == 1 ? BF_RNDD : mode == 2 ? BF_RNDU : BF_RNDZ;
    bf_t bf_a, bf_v, bf_tmp, bf_1, bf_2, bf_pio2;
    union float_value v, *r;
    char buffer[256];
    bf_context_t bf;
    double *d, a;
    FILE *out;
    int res;

    bf_context_init(&bf, bf_realloc_func, NULL);
    bf_init(&bf, &bf_a);
    bf_init(&bf, &bf_v);
    bf_init(&bf, &bf_tmp);
    bf_init(&bf, &bf_1);
    bf_init(&bf, &bf_2);
    bf_init(&bf, &bf_pio2);

    res = bf_set_float64(&bf_1, 1);
    ok(res == 0, "bf_set_float64 returned %d\n", res);
    res = bf_set_float64(&bf_2, 2);
    ok(res == 0, "bf_set_float64 returned %d\n", res);
    res = bf_set_float64(&bf_pio2, M_PI_2);
    ok(res == 0, "bf_set_float64 returned %d\n", res);

    r = malloc((max - min) * sizeof(*r));
    d = malloc((max - min) * sizeof(*d));
    for (v.u = min; v.u != max; v.u++)
    {
        res = bf_set_float64(&bf_v, v.f);
        ok(res == 0, "bf_set_float64 returned %d\n", res);
        bf_normalize_and_round(&bf_v, 24, bf_set_exp_bits(8) | bf_round);

        res = bf_asin(&bf_a, &bf_v, 112, bf_set_exp_bits(15) | bf_round);
        ok(res == BF_ST_INEXACT, "bf_asin returned %d\n", res);
        bf_get_float64(&bf_a, &d[v.u - min], bf_round);

        bf_sub(&bf_v, &bf_1, &bf_v, BF_PREC_INF, bf_round);
        bf_div(&bf_v, &bf_v, &bf_2, 112, bf_set_exp_bits(15) | bf_round);
        bf_sqrt(&bf_tmp, &bf_v, 112, bf_set_exp_bits(15) | bf_round);
        bf_set(&bf_v, &bf_tmp);

        res = bf_asin(&bf_a, &bf_v, 112, bf_set_exp_bits(15) | bf_round);
        ok(res == BF_ST_INEXACT, "bf_asin returned %d\n", res);

        bf_sub(&bf_a, &bf_a, &bf_v, BF_PREC_INF, bf_round);
        bf_div(&bf_a, &bf_a, &bf_v, 112, bf_set_exp_bits(15) | bf_round);
        bf_normalize_and_round(&bf_a, 24, bf_set_exp_bits(8) | bf_round);

        bf_get_float64(&bf_a, &a, bf_round);
        a = asinf_bf_high( v.f, a );

        r[v.u - min].f = a;
    }

    bf_delete(&bf_pio2);
    bf_delete(&bf_2);
    bf_delete(&bf_1);
    bf_delete(&bf_tmp);
    bf_delete(&bf_v);
    bf_delete(&bf_a);
    bf_context_end(&bf);

    sprintf(buffer, "asinf-%08x-%08x-%u-bf32.dat", min, max, mode);
    out = fopen(buffer, "wb");
    fwrite(r, max - min, sizeof(*r), out);
    fclose(out);
    free(r);

    sprintf(buffer, "asinf-%08x-%08x-%u-bf64.dat", min, max, mode);
    out = fopen(buffer, "wb");
    fwrite(d, max - min, sizeof(*d), out);
    fclose(out);
    free(d);
}

static void dump_asinf_core_error( UINT min, UINT max, UINT mode )
{
    bf_flags_t bf_round = mode == 0 ? BF_RNDN : mode == 1 ? BF_RNDD : mode == 2 ? BF_RNDU : BF_RNDZ;
    bf_t bf_r, bf_a, bf_e, bf_v;
    union float_value v, *r;
    char buffer[256];
    bf_context_t bf;
    FILE *out;
    double *d;
    int res;

    bf_context_init(&bf, bf_realloc_func, NULL);
    bf_init(&bf, &bf_a);
    bf_init(&bf, &bf_e);
    bf_init(&bf, &bf_r);
    bf_init(&bf, &bf_v);

    r = malloc((max - min) * sizeof(*r));
    sprintf(buffer, "asinf-%08x-%08x-%u-%s.dat", min, max, mode, strcmp( winetest_platform, "wine" ) ? "msvc" : "wine" );
    out = fopen(buffer, "rb");
    fread(r, max - min, sizeof(*r), out);
    fclose(out);

    d = malloc((max - min) * sizeof(*d));
    for (v.u = min; v.u != max; v.u+=16)
    {
        res = bf_set_float64(&bf_v, v.f);
        ok(res == 0, "bf_set_float64 returned %d\n", res);
        bf_normalize_and_round(&bf_v, 24, bf_set_exp_bits(8) | bf_round);

        res = bf_asin(&bf_a, &bf_v, 112, bf_set_exp_bits(15) | bf_round);
        ok(res == BF_ST_INEXACT, "bf_asin returned %d\n", res);

        bf_sub(&bf_e, &bf_a, &bf_v, BF_PREC_INF, bf_round);
        /* bf_div(&bf_e, &bf_e, &bf_v, 112, bf_set_exp_bits(15) | bf_round); */
        bf_normalize_and_round(&bf_e, 24, bf_set_exp_bits(8) | bf_round);
        /* bf_mul(&bf_e, &bf_e, &bf_v, 24, bf_set_exp_bits(8) | bf_round); */
        bf_add(&bf_e, &bf_e, &bf_v, 24, bf_set_exp_bits(8) | bf_round);
        bf_sub(&bf_e, &bf_e, &bf_a, BF_PREC_INF, bf_round);
        bf_mul(&bf_e, &bf_e, &bf_e, BF_PREC_INF, bf_round);

        res = bf_set_float64(&bf_r, r[v.u - min].f);
        ok(res == 0, "bf_set_float32 returned %d\n", res);
        bf_normalize_and_round(&bf_r, 24, bf_set_exp_bits(8) | bf_round);
#if 0
        bf_mul(&bf_r, &bf_r, &bf_v, 24, bf_set_exp_bits(8) | bf_round);
        bf_add(&bf_r, &bf_r, &bf_v, 24, bf_set_exp_bits(8) | bf_round);
#endif

        bf_sub(&bf_r, &bf_r, &bf_a, BF_PREC_INF, bf_round);
        bf_mul(&bf_r, &bf_r, &bf_r, BF_PREC_INF, bf_round);
        /* bf_div(&bf_r, &bf_r, &bf_e, 112, bf_set_exp_bits(15) | bf_round); */

        bf_get_float64(&bf_r, &d[v.u - min], bf_round);
        if (!d[v.u - min]) d[v.u - min] = 1;
    }

    bf_delete(&bf_v);
    bf_delete(&bf_r);
    bf_delete(&bf_e);
    bf_delete(&bf_a);
    bf_context_end(&bf);

    sprintf(buffer, "asinf-core-err-%08x-%08x-%u.dat", min, max, mode);
    out = fopen(buffer, "wb");
    fwrite(d, max - min, sizeof(*d), out);
    fclose(out);
    free(d);
    free(r);
}

static void dump_acosf( UINT min, UINT max, UINT mode )
{
    union float_value v, *r;
    char buffer[256];
    FILE *out;

    r = malloc((max - min) * sizeof(*r));
    for (v.u = min; v.u != max; v.u++) r[v.u - min].f = p_acosf(v.f);

    sprintf(buffer, "acosf-%08x-%08x-%u-%s.dat", min, max, mode, strcmp( winetest_platform, "wine" ) ? "msvc" : "wine" );
    out = fopen(buffer, "wb");
    fwrite(r, max - min, sizeof(*r), out);
    fclose(out);
    free(r);
}

static void dump_acos_bf( UINT min, UINT max, UINT mode )
{
    bf_flags_t bf_round = mode == 0 ? BF_RNDN : mode == 1 ? BF_RNDD : mode == 2 ? BF_RNDU : BF_RNDZ;
    bf_t bf_a, bf_v, bf_pio2;
    union float_value v, *r;
    double *acos, tmp;
    char buffer[256];
    bf_context_t bf;
    float *acosf;
    FILE *out;
    int res;

    bf_context_init(&bf, bf_realloc_func, NULL);
    bf_init(&bf, &bf_a);
    bf_init(&bf, &bf_v);
    bf_init(&bf, &bf_pio2);

    bf_set_float64(&bf_pio2, M_PI_2);

    r = malloc((max - min) * sizeof(*r));
    acos = malloc((max - min) * sizeof(*acos));
    acosf = malloc((max - min) * sizeof(*acosf));
    for (v.u = min; v.u != max; v.u++)
    {
        res = bf_set_float64(&bf_v, v.f);
        ok(res == 0, "bf_set_float64 returned %d\n", res);
        bf_normalize_and_round(&bf_v, 24, bf_set_exp_bits(8) | bf_round);

        res = bf_acos(&bf_a, &bf_v, 112, bf_set_exp_bits(15) | bf_round);
        ok(res == BF_ST_INEXACT, "bf_acos returned %d\n", res);
        bf_get_float64(&bf_a, &acos[v.u - min], bf_round);
        acosf[v.u - min] = acos[v.u - min];

        bf_sub(&bf_a, &bf_pio2, &bf_a, BF_PREC_INF, bf_round);
        bf_sub(&bf_a, &bf_a, &bf_v, BF_PREC_INF, bf_round);
        bf_div(&bf_a, &bf_a, &bf_v, 112, bf_set_exp_bits(15) | bf_round);
        bf_normalize_and_round(&bf_a, 24, bf_set_exp_bits(8) | bf_round);
        bf_mul(&bf_a, &bf_a, &bf_v, 24, bf_set_exp_bits(8) | bf_round);
        bf_add(&bf_a, &bf_a, &bf_v, 24, bf_set_exp_bits(8) | bf_round);
        bf_sub(&bf_a, &bf_pio2, &bf_a, BF_PREC_INF, bf_round);

        bf_get_float64(&bf_a, &tmp, bf_round);
        r[v.u - min].f = tmp;
    }

    bf_delete(&bf_v);
    bf_delete(&bf_a);
    bf_context_end(&bf);

    sprintf(buffer, "acosf-%08x-%08x-%u-bf-acosf.dat", min, max, mode);
    out = fopen(buffer, "wb");
    fwrite(acosf, max - min, sizeof(*acosf), out);
    fclose(out);
    free(acosf);

    sprintf(buffer, "acosf-%08x-%08x-%u-bf-acos.dat", min, max, mode);
    out = fopen(buffer, "wb");
    fwrite(acos, max - min, sizeof(*acos), out);
    fclose(out);
    free(acos);

    sprintf(buffer, "acosf-%08x-%08x-%u-bf-maddx.dat", min, max, mode);
    out = fopen(buffer, "wb");
    fwrite(r, max - min, sizeof(*r), out);
    fclose(out);
    free(r);
}

static void test_atanf_nan( UINT min, UINT max, UINT mode )
{
    union float_value v, r;
    for (v.u = min; v.u != max; v.u += min(STEP, max - v.u))
    {
        r.f = p_atanf(v.f);
        ok( r.u == (v.u | 0x400000), "atanf %#08x returned %#08x\n", v.u, r.u );
    }
}

static void test_atanf_pio2( UINT min, UINT max, UINT mode )
{
    union float_value v, r;
    for (v.u = min; v.u != max; v.u += min(STEP, max - v.u))
    {
        r.f = p_atanf(v.f);
        ok( r.u == ((v.u & 0x80000000) | 0x3fc90fdb), "atanf %#08x returned %#08x\n", v.u, r.u );
    }
}

static void test_atan2f_0x_nan( UINT min, UINT max, UINT mode )
{
    union float_value v, r;
    for (v.u = min; v.u != max; v.u += min(STEP, max - v.u))
    {
        r.f = p_atan2f(0, v.f);
        ok( r.u == (v.u | 0x400000), "atan2f 0 %#08x returned %#08x\n", v.u, r.u );
    }
}

static void test_atan2f_0x_zero( UINT min, UINT max, UINT mode )
{
    union float_value v, r;
    for (v.u = min; v.u != max; v.u += min(STEP, max - v.u))
    {
        r.f = p_atan2f(0, v.f);
        ok( r.u == 0, "atan2f 0 %#08x returned %#08x\n", v.u, r.u );
    }
}

static void test_atan2f_0x_pi( UINT min, UINT max, UINT mode )
{
    union float_value v, r;
    for (v.u = min; v.u != max; v.u += min(STEP, max - v.u))
    {
        r.f = p_atan2f(0, v.f);
        ok( r.u == 0x40490fdb, "atan2f 0 %#08x returned %#08x\n", v.u, r.u );
    }
}

static void test_atan2f_y0_nan( UINT min, UINT max, UINT mode )
{
    union float_value v, r;
    for (v.u = min; v.u != max; v.u += min(STEP, max - v.u))
    {
        r.f = p_atan2f(0, v.f);
        ok( r.u == (v.u | 0x400000), "atan2f %#08x 0 returned %#08x\n", v.u, r.u );
    }
}

static void test_atan2f_y0_zero( UINT min, UINT max, UINT mode )
{
    union float_value v, r;
    for (v.u = min; v.u != max; v.u += min(STEP, max - v.u))
    {
        r.f = p_atan2f(v.f, 0);
        ok( r.u == v.u, "atan2f %#08x 0 returned %#08x\n", v.u, r.u );
    }
}

static void test_atan2f_y0_pio2( UINT min, UINT max, UINT mode )
{
    union float_value v, r;
    for (v.u = min; v.u != max; v.u += min(STEP, max - v.u))
    {
        r.f = p_atan2f(v.f, 0);
        ok( r.u == ((v.u & 0x80000000) | 0x3fc90fdb), "atan2f %#08x 0 returned %#08x\n", v.u, r.u );
    }
}

static void test_atan2f_1x_pio2( UINT min, UINT max, UINT mode )
{
    union float_value v, r;
    for (v.u = min; v.u != max; v.u += min(STEP, max - v.u))
    {
        r.f = p_atan2f(1.f, v.f);
        ok( r.u == 0x3fc90fdb, "atan2f 1 %#08x returned %#08x\n", v.u, r.u );
    }
}

static void test_atan2f_1x_huge( UINT min, UINT max, UINT mode )
{
    union float_value v, o, r;
    for (v.u = min; v.u != max; v.u += min(STEP, max - v.u))
    {
        r.f = p_atan2f(1.f, v.f);
        o.f = 1.f / v.f;
        ok( r.u == o.u, "atan2f 1 %#08x returned %#08x\n", v.u, r.u );
    }
}

static void test_atan2f_1x( UINT min, UINT max, UINT mode )
{
    union float_value v, o, r;
    for (v.u = min; v.u != max; v.u += min(STEP, max - v.u))
    {
        r.f = p_atan2f(1.f, v.f);
        o.f = 1.f / v.f;
        /* o.f = p_atanf(o.f); */
        ok( r.u != o.u, "atan2f 1 %#08x returned %#08x\n", v.u, r.u );
    }
}

static void test_atan2f_y1_tiny( UINT min, UINT max, UINT mode )
{
    union float_value v, r;
    for (v.u = min; v.u != max; v.u += min(STEP, max - v.u))
    {
        r.f = p_atan2f(v.f, 1.f);
        ok( r.u == v.u, "atan2f %#08x 1 returned %#08x\n", v.u, r.u );
    }
}

static void test_atan2f_y1_pio2( UINT min, UINT max, UINT mode )
{
    union float_value v, r;
    for (v.u = min; v.u != max; v.u += min(STEP, max - v.u))
    {
        r.f = p_atan2f(v.f, 1.f);
        ok( r.u == ((v.u & 0x80000000) | 0x3fc90fdb), "atan2f %#08x 1 returned %#08x\n", v.u, r.u );
    }
}

static void test_atan2f_y1( UINT min, UINT max, UINT mode )
{
    union float_value v, o, r;
    for (v.u = min; v.u != max; v.u += min(STEP, max - v.u))
    {
        r.f = p_atan2f(v.f, 1.f);
        o.f = p_atanf(v.f);
        ok( r.u == o.u, "atan2f 1 %#08x returned %#08x\n", v.u, r.u );
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
    UINT sse_mode = params.mode * 0x2000;

    SetEvent( params.event );

    winetest_push_context( "%s", mode_names[params.mode] );
    if (0) if (p__set_FMA3_enable) p__set_FMA3_enable( 0 );

    if (p_fegetround && p_fesetround)
    {
        ok( p_fegetround() == FE_TONEAREST, "fegetround returned %u\n", p_fegetround() );
        p_fesetround( fp_modes[params.mode] );
        ok( p_fegetround() == fp_modes[params.mode], "fegetround returned %u\n", p_fegetround() );
    }
    else
    {
        ok( _MM_GET_ROUNDING_MODE() == 0, "got SSE rounding mode %#x\n", _MM_GET_ROUNDING_MODE() );
        _MM_SET_ROUNDING_MODE( sse_mode );
    }
    ok( _MM_GET_ROUNDING_MODE() == sse_mode, "got SSE rounding mode %#x\n", _MM_GET_ROUNDING_MODE() );

    params.callback( params.min, params.max, params.mode );
    winetest_pop_context();

    return 0;
}

static void run_split( void (*callback)( UINT min, UINT max, UINT mode ),
                       const UINT *split, UINT count, UINT mode )
{
    struct run_params params = {.callback = callback, .max = split[0], .mode = mode};
    HANDLE thread[32];
    UINT i;

    for (i = 0; i < count; ++i)
    {
        params.min = params.max;
        params.max = split[i + 1];
        params.event = CreateEventW( NULL, FALSE, FALSE, NULL );
        thread[i] = CreateThread( NULL, 0, run_thread, &params, 0, NULL );
        WaitForSingleObject( params.event, INFINITE );
        CloseHandle( params.event );
    }

    WaitForMultipleObjects( count, thread, TRUE, INFINITE );
    while (count--) CloseHandle( thread[count] );
}

static void run( void (*callback)( UINT min, UINT max, UINT mode ),
                 UINT min, UINT max, UINT count, UINT mode )
{
    UINT i, split[32] = { min, max };
    for (i = 1; i < count; ++i)
    {
        split[i] = min + (max - min) / (count - i);
        split[i + 1] = max;
        min = split[i];
    }
    run_split( callback, split, count, mode );
}

START_TEST( math )
{
    static const UINT32 asinf_split[4][17] =
    {
        {
            0x38800000, 0x39000000, 0x39800000, 0x39ffffff,
            0x3a7ffffd, 0x3afffff5, 0x3b7fffd5, 0x3bffff55,
            0x3c7ffd55, 0x3cfff555, 0x3d7fd557, 0x3dff5577,
            0x3e7d5777, 0x3ef57744, 0x3f000000, 0x3f576aa5, 0x3f800000,
        },
        {
            0x38800000, 0x39000000, 0x39800000, 0x3a000000,
            0x3a7ffffe, 0x3afffff6, 0x3b7fffd6, 0x3bffff56,
            0x3c7ffd56, 0x3cfff556, 0x3d7fd558, 0x3dff5578,
            0x3e7d5777, 0x3ef57744, 0x3f000000, 0x3f576aa5, 0x3f800000,
        },
        {
            0x38800000, 0x38ffffff, 0x397fffff, 0x39ffffff,
            0x3a7ffffd, 0x3afffff5, 0x3b7fffd5, 0x3bffff55,
            0x3c7ffd55, 0x3cfff555, 0x3d7fd557, 0x3dff5577,
            0x3e7d5776, 0x3ef57743, 0x3f000000, 0x3f576aa5, 0x3f800000,
        },
        {
            0x38800000, 0x39000000, 0x39800000, 0x3a000000,
            0x3a7ffffe, 0x3afffff6, 0x3b7fffd6, 0x3bffff56,
            0x3c7ffd56, 0x3cfff556, 0x3d7fd558, 0x3dff5578,
            0x3e7d5777, 0x3ef57744, 0x3f000000, 0x3f576aa5, 0x3f800000,
        },
    };
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
        p_acosf = (void *)GetProcAddress( ucrtbase, "acosf" );
        ok( !!p_acosf, "acosf not found\n" );
        p_sqrtf = (void *)GetProcAddress( ucrtbase, "sqrtf" );
        ok( !!p_sqrtf, "sqrtf not found\n" );
        p_atanf = (void *)GetProcAddress( ucrtbase, "atanf" );
        ok( !!p_atanf, "atanf not found\n" );
        p_atan2f = (void *)GetProcAddress( ucrtbase, "atan2f" );
        ok( !!p_atan2f, "atan2f not found\n" );
    }
    ok( !!ucrtbase, "ucrtbase not found\n" );

    if (!p_sqrtf) skip( "sqrtf not found, skipping tests\n" );
    else for (mode = 1; mode < 2; ++mode)
    {
        /* if (0) run( test_sqrtf_snan, 0x7f800001, 0x7fc00000, 8, mode ); */
        /* if (0) run( test_sqrtf_qnan, 0x7fc00000, 0x80000000, 8, mode ); */
        /* if (0) run( test_sqrtf_snan, 0xff800001, 0xffc00000, 8, mode ); */
        /* if (0) run( test_sqrtf_qnan, 0xffc00000, 0x00000000, 8, mode ); */

        if (0) run( test_sqrtf, 0x00000000, 0x7f800000, 8, mode );
    }

    if (!p_asinf) skip( "asinf not found, skipping tests\n" );
    else for (mode = 0; mode < 4; ++mode)
    {
        if (0) run( test_asinf_tiny, 0x00000000, 0x38800000, 8, mode );
        if (0) run( test_asinf_pio2, 0x3f800000, 0x3f800001, 1, mode );
        if (0) run( test_asinf_edom, 0x3f800001, 0x7f800001, 8, mode );
        if (0) run( test_asinf_nan, 0x7f800001, 0x7fc00000, 8, mode ); /* SNaN */
        if (0) run( test_asinf_nan, 0x7fc00000, 0x80000000, 8, mode ); /* QNaN */

        if (0) run( test_asinf_tiny, 0x80000000, 0xb8800000, 8, mode );
        if (0) run( test_asinf_pio2, 0xbf800000, 0xbf800001, 1, mode );
        if (0) run( test_asinf_edom, 0xbf800001, 0xff800001, 8, mode );
        if (0) run( test_asinf_nan, 0xff800001, 0xffc00000, 8, mode ); /* SNaN */
        if (0) run( test_asinf_nan, 0xffc00000, 0x00000000, 8, mode ); /* QNaN */

        if (0) run( test_asinf_tiny2, 0x38800000, 0x3a389ba1, 1, mode );

        if (0) run( test_asinf_part, 0x38800000, 0x3f800000, 1, mode );
        if (0) run( test_asinf_part, 0xb8800000, 0xbf800000, 1, mode );

        if (0) run( test_asinf_sign, 0x38800000, 0x3f000000, 8, mode );
        if (0) run( test_asinf_sign, 0x3f000000, 0x3f800000, 8, mode );
        if (0) run( test_asinf_sign, 0xb8800000, 0xbf000000, 8, mode );
        if (0) run( test_asinf_sign, 0xbf000000, 0xbf800000, 8, mode );

        if (0) run( test_asinf_lowr, 0x38800000, 0x3f000000, 8, mode );
        if (0) run( test_asinf_high, 0x3f000000, 0x3f800000, 8, mode );
        if (0) run( test_asinf_lowr, 0xb8800000, 0xbf000000, 8, mode );
        if (0) run( test_asinf_high, 0xbf000000, 0xbf800000, 8, mode );

        if (1) run_split( dump_asinf, asinf_split[mode], 16, mode );
        if (0) run( dump_asinf, 0x38800000, 0x3f000000, 8, mode );
        if (0) run( dump_asinf, 0x3f000000, 0x3f800000, 8, mode );
        if (0) run( dump_asinf, 0xb8800000, 0xbf000000, 8, mode );
        if (0) run( dump_asinf, 0xbf000000, 0xbf800000, 8, mode );

        if (0) run_split( dump_asin_bf, asinf_split[mode], 16, mode );
        if (0) run( dump_asin_bf, 0x38800000, 0x3f000000, 8, mode );
        if (0) run( dump_asin_bf_high, 0x3f000000, 0x3f800000, 8, mode );
        if (0) run( dump_asinf_core_min, 0x38800000, 0x3f000000, 8, mode );
        if (0) run( dump_asinf_core_max, 0x38800000, 0x3f000000, 8, mode );
        if (0) run( dump_asinf_core_error, 0x38800000, 0x3f000000, 8, mode );
    }

    if (!p_acosf) skip( "acosf not found, skipping tests\n" );
    else for (mode = 0; mode < 4; ++mode)
    {
        if (0) run( test_acosf_tiny, 0x00000000, 0x32800000, 8, mode );
        if (0) run( test_acosf_pi, 0x3f800000, 0x3f800001, 1, mode );
        if (0) run( test_acosf_edom, 0x3f800001, 0x7f800001, 8, mode );
        if (0) run( test_acosf_nan, 0x7f800001, 0x7fc00000, 8, mode ); /* SNaN */
        if (0) run( test_acosf_nan, 0x7fc00000, 0x80000000, 8, mode ); /* QNaN */

        if (0) run( test_acosf_tiny, 0x80000000, 0xb2800000, 8, mode );
        if (0) run( test_acosf_pi, 0xbf800000, 0xbf800001, 1, mode );
        if (0) run( test_acosf_edom, 0xbf800001, 0xff800001, 8, mode );
        if (0) run( test_acosf_nan, 0xff800001, 0xffc00000, 8, mode ); /* SNaN */
        if (0) run( test_acosf_nan, 0xffc00000, 0x00000000, 8, mode ); /* QNaN */

        if (0) run( test_acosf_part, 0x32800000, 0x3f800000, 1, mode );
        if (0) run( test_acosf_part, 0xb2800000, 0xbf800000, 1, mode );

        if (0) run( test_acosf_lowr, 0x32800000, 0x3f000000, 8, mode );
        if (0) run( test_acosf_hpos, 0x3f000000, 0x3f800000, 8, mode );
        if (0) run( test_acosf_lowr, 0xb2800000, 0xbf000000, 8, mode );
        if (0) run( test_acosf_hneg, 0xbf000000, 0xbf800000, 8, mode );

        if (1) run_split( dump_acosf, asinf_split[mode], 16, mode );
        if (0) run( dump_acosf, 0x32800000, 0x3f000000, 8, mode );
        if (0) run( dump_acosf, 0x38800000, 0x3f000000, 8, mode );
        if (0) run( dump_acosf, 0x3f000000, 0x3f800000, 8, mode );
        if (0) run( dump_acosf, 0xb2800000, 0xbf000000, 1, mode );
        if (0) run( dump_acosf, 0xbf000000, 0xbf800000, 1, mode );

        if (0) run( dump_acos_bf, 0x32800000, 0x3f000000, 8, mode );
    }

    if (!p_atanf) skip( "atanf not found, skipping tests\n" );
    else for (mode = 0; mode < 4; ++mode)
    {
        if (0) run( test_atanf_nan, 0x7f800001, 0x7fc00000, 8, mode ); /* SNaN */
        if (0) run( test_atanf_nan, 0x7fc00000, 0x80000000, 8, mode ); /* QNaN */

        if (0) run( test_atanf_nan, 0xff800001, 0xffc00000, 8, mode ); /* SNaN */
        if (0) run( test_atanf_nan, 0xffc00000, 0x00000000, 8, mode ); /* QNaN */

        if (0) run( test_atanf_pio2, 0x5e9a0000, 0x7f800001, 1, mode );
        if (0) run( test_atanf_pio2, 0xde9a0000, 0xff800001, 1, mode );

#if 0
        if (0) run( test_atanf, 0x00000000, 0x7f800001, 8, mode );
        if (0) run( test_atanf, 0x80000000, 0xff800001, 8, mode );

        if (0) run( dump_atanf, 0x32800000, 0x3f000000, 1, mode );
        if (0) run( dump_atanf, 0x3f000000, 0x3f800000, 1, mode );
        if (0) run( dump_atanf, 0xb2800000, 0xbf000000, 1, mode );
        if (0) run( dump_atanf, 0xbf000000, 0xbf800000, 1, mode );
#endif
    }

    if (!p_atan2f) skip( "atan2f not found, skipping tests\n" );
    else for (mode = 0; mode < 4; ++mode)
    {
        if (0) run( test_atan2f_0x_nan, 0x7f800001, 0x7fc00000, 8, mode ); /* SNaN */
        if (0) run( test_atan2f_0x_nan, 0x7fc00000, 0x80000000, 8, mode ); /* QNaN */
        if (0) run( test_atan2f_0x_nan, 0xff800001, 0xffc00000, 8, mode ); /* SNaN */
        if (0) run( test_atan2f_0x_nan, 0xffc00000, 0x00000000, 8, mode ); /* QNaN */
        if (0) run( test_atan2f_y0_nan, 0x7f800001, 0x7fc00000, 8, mode ); /* SNaN */
        if (0) run( test_atan2f_y0_nan, 0x7fc00000, 0x80000000, 8, mode ); /* QNaN */
        if (0) run( test_atan2f_y0_nan, 0xff800001, 0xffc00000, 8, mode ); /* SNaN */
        if (0) run( test_atan2f_y0_nan, 0xffc00000, 0x00000000, 8, mode ); /* QNaN */

        if (0) run( test_atan2f_0x_zero, 0x00000000, 0x7f800001, 8, mode );
        if (0) run( test_atan2f_0x_pi, 0x80000000, 0xff800001, 8, mode );

        if (0) run( test_atan2f_y0_zero, 0x00000000, 0x00000001, 1, mode );
        if (0) run( test_atan2f_y0_zero, 0x80000000, 0x80000001, 1, mode );
        if (0) run( test_atan2f_y0_pio2, 0x00000001, 0x7f800001, 8, mode );
        if (0) run( test_atan2f_y0_pio2, 0x80000001, 0xff800001, 8, mode );

        if (0) run( test_atan2f_1x_pio2, 0x00000000, 0x32800000, 8, mode );
        if (0) run( test_atan2f_1x_pio2, 0x80000000, 0xb2800000, 8, mode );
        if (0) run( test_atan2f_y1_tiny, 0x00000000, 0x39000000, 1, mode );
        if (0) run( test_atan2f_y1_tiny, 0x80000000, 0xb9000000, 8, mode );

        if (0) run( test_atan2f_y1_pio2, 0x4d000000, 0x7f800001, 8, mode );
        if (0) run( test_atan2f_y1_pio2, 0xcd000000, 0xff800001, 8, mode );

        if (0) run( test_atan2f_1x_huge, 0x461c4000, 0x7f800001, 1, mode );
        if (0) run( test_atan2f_1x, 0x32800000, 0x461c4000, 1, mode );
        if (0) run( test_atan2f_1x, 0xb2800000, 0xff800001, 1, mode );

        if (0) run( test_atan2f_y1, 0x39000000, 0x4d000000, 8, mode );
        if (0) run( test_atan2f_y1, 0xb9000000, 0xcd000000, 8, mode );

#if 0
        if (0) run( test_atan2f_lowr, 0x32800000, 0x3f000000, 8, mode );
        if (0) run( test_atan2f_hpos, 0x3f000000, 0x3f800000, 8, mode );
        if (0) run( test_atan2f_lowr, 0xb2800000, 0xbf000000, 8, mode );
        if (0) run( test_atan2f_hneg, 0xbf000000, 0xbf800000, 8, mode );

        if (0) run( dump_atan2f, 0x32800000, 0x3f000000, 1, mode );
        if (0) run( dump_atan2f, 0x3f000000, 0x3f800000, 1, mode );
        if (0) run( dump_atan2f, 0xb2800000, 0xbf000000, 1, mode );
        if (0) run( dump_atan2f, 0xbf000000, 0xbf800000, 1, mode );
#endif
    }

    FreeLibrary( ucrtbase );
}
