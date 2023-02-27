/*
 * Copyright 2023 Rémi Bernon for CodeWeavers
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

#include <stdarg.h>
#include <stddef.h>

#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#ifdef HAVE_X11_XKBLIB_H
#include <X11/XKBlib.h>
#endif

#include "x11drv.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(keyboard);
WINE_DECLARE_DEBUG_CHANNEL(winediag);

/* SHA1 algorithm
 *
 * Based on public domain SHA code by Steve Reid <steve@edmweb.com>
 * Copied and adapted from ntdll.A_SHAInit / ntdll.A_SHAUpdate / ntdll.A_SHAFinal
 */

struct sha1_context
{
    unsigned int state[5];
    unsigned int count[2];
    char buffer[64];
};

#ifdef WORDS_BIGENDIAN
#define DWORD2BE(x) (x)
#else
#define DWORD2BE(x) ((((x) >> 24) & 0xff) | (((x) >> 8) & 0xff00) | (((x) << 8) & 0xff0000) | (((x) << 24) & 0xff000000))
#endif

#define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))
#define blk0(i) (block[i] = DWORD2BE(block[i]))
#define blk1(i) (block[i&15] = rol(block[(i+13)&15]^block[(i+8)&15]^block[(i+2)&15]^block[i&15],1))
#define f1(x,y,z) (z^(x&(y^z)))
#define f2(x,y,z) (x^y^z)
#define f3(x,y,z) ((x&y)|(z&(x|y)))
#define f4(x,y,z) (x^y^z)
/* (R0+R1), R2, R3, R4 are the different operations used in SHA1 */
#define R0(v,w,x,y,z,i) z+=f1(w,x,y)+blk0(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R1(v,w,x,y,z,i) z+=f1(w,x,y)+blk1(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R2(v,w,x,y,z,i) z+=f2(w,x,y)+blk1(i)+0x6ED9EBA1+rol(v,5);w=rol(w,30);
#define R3(v,w,x,y,z,i) z+=f3(w,x,y)+blk1(i)+0x8F1BBCDC+rol(v,5);w=rol(w,30);
#define R4(v,w,x,y,z,i) z+=f4(w,x,y)+blk1(i)+0xCA62C1D6+rol(v,5);w=rol(w,30);

/* Hash a single 512-bit block. This is the core of the algorithm. */
static void sha1_transform( struct sha1_context *ctx )
{
    unsigned int a, b, c, d, e, *block = (unsigned int *)ctx->buffer;

    /* Copy ctx->state[] to working variables */
    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];

    /* 4 rounds of 20 operations each. Loop unrolled. */
    R0(a,b,c,d,e, 0); R0(e,a,b,c,d, 1); R0(d,e,a,b,c, 2); R0(c,d,e,a,b, 3);
    R0(b,c,d,e,a, 4); R0(a,b,c,d,e, 5); R0(e,a,b,c,d, 6); R0(d,e,a,b,c, 7);
    R0(c,d,e,a,b, 8); R0(b,c,d,e,a, 9); R0(a,b,c,d,e,10); R0(e,a,b,c,d,11);
    R0(d,e,a,b,c,12); R0(c,d,e,a,b,13); R0(b,c,d,e,a,14); R0(a,b,c,d,e,15);
    R1(e,a,b,c,d,16); R1(d,e,a,b,c,17); R1(c,d,e,a,b,18); R1(b,c,d,e,a,19);
    R2(a,b,c,d,e,20); R2(e,a,b,c,d,21); R2(d,e,a,b,c,22); R2(c,d,e,a,b,23);
    R2(b,c,d,e,a,24); R2(a,b,c,d,e,25); R2(e,a,b,c,d,26); R2(d,e,a,b,c,27);
    R2(c,d,e,a,b,28); R2(b,c,d,e,a,29); R2(a,b,c,d,e,30); R2(e,a,b,c,d,31);
    R2(d,e,a,b,c,32); R2(c,d,e,a,b,33); R2(b,c,d,e,a,34); R2(a,b,c,d,e,35);
    R2(e,a,b,c,d,36); R2(d,e,a,b,c,37); R2(c,d,e,a,b,38); R2(b,c,d,e,a,39);
    R3(a,b,c,d,e,40); R3(e,a,b,c,d,41); R3(d,e,a,b,c,42); R3(c,d,e,a,b,43);
    R3(b,c,d,e,a,44); R3(a,b,c,d,e,45); R3(e,a,b,c,d,46); R3(d,e,a,b,c,47);
    R3(c,d,e,a,b,48); R3(b,c,d,e,a,49); R3(a,b,c,d,e,50); R3(e,a,b,c,d,51);
    R3(d,e,a,b,c,52); R3(c,d,e,a,b,53); R3(b,c,d,e,a,54); R3(a,b,c,d,e,55);
    R3(e,a,b,c,d,56); R3(d,e,a,b,c,57); R3(c,d,e,a,b,58); R3(b,c,d,e,a,59);
    R4(a,b,c,d,e,60); R4(e,a,b,c,d,61); R4(d,e,a,b,c,62); R4(c,d,e,a,b,63);
    R4(b,c,d,e,a,64); R4(a,b,c,d,e,65); R4(e,a,b,c,d,66); R4(d,e,a,b,c,67);
    R4(c,d,e,a,b,68); R4(b,c,d,e,a,69); R4(a,b,c,d,e,70); R4(e,a,b,c,d,71);
    R4(d,e,a,b,c,72); R4(c,d,e,a,b,73); R4(b,c,d,e,a,74); R4(a,b,c,d,e,75);
    R4(e,a,b,c,d,76); R4(d,e,a,b,c,77); R4(c,d,e,a,b,78); R4(b,c,d,e,a,79);

    /* Add the working variables back into ctx->state[] */
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;

    /* Wipe variables */
    a = b = c = d = e = 0;
}

void sha1_init( struct sha1_context *ctx )
{
    /* SHA1 initialization constants */
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
    ctx->count[0] = 0;
    ctx->count[1] = 0;
}

void sha1_update( struct sha1_context *ctx, const char *data, size_t data_size )
{
    size_t buffer_size;

    buffer_size = ctx->count[1] & 63;
    ctx->count[1] += data_size;
    if (ctx->count[1] < data_size) ctx->count[0]++;
    ctx->count[0] += (data_size >> 29);

    if (buffer_size + data_size < 64) memcpy( &ctx->buffer[buffer_size], data, data_size );
    else
    {
        while (buffer_size + data_size >= 64)
        {
            memcpy( ctx->buffer + buffer_size, data, 64 - buffer_size );
            data += 64 - buffer_size;
            data_size -= 64 - buffer_size;
            sha1_transform( ctx );
            buffer_size = 0;
        }
        memcpy( ctx->buffer + buffer_size, data, data_size );
    }
}

void sha1_finalize( struct sha1_context *ctx, unsigned int result[5] )
{
    unsigned int *count, length_hi, length_lo, i;
    size_t pad_size, buffer_size;
    char pad[72];

    buffer_size = ctx->count[1] & 63;
    if (buffer_size >= 56) pad_size = 56 + 64 - buffer_size;
    else pad_size = 56 - buffer_size;

    length_hi = (ctx->count[0] << 3) | (ctx->count[1] >> (32 - 3));
    length_lo = (ctx->count[1] << 3);

    memset( pad + 1, 0, pad_size - 1 );
    pad[0] = 0x80;
    count = (unsigned int *)( pad + pad_size );
    count[0] = DWORD2BE( length_hi );
    count[1] = DWORD2BE( length_lo );
    sha1_update( ctx, pad, pad_size + 8 );

    for (i = 0; i < 5; i++) result[i] = DWORD2BE( ctx->state[i] );

    sha1_init( ctx );
}

struct keymap_hashes
{
    unsigned short data[8];
};

struct keymap
{
    LANGID langid;
    const char *layout;
    const char *description;
    struct keymap_hashes hashes;
};

static const struct keymap all_keymaps[] =
{
    {.langid = 0x0409, .hashes = {{0x4b1d,0x4b8f,0xcb10,0x3118,0xe988,0x21d4,0x3554,0xaca9}}, .layout = "us", .description = "English (US)"},
    /* same as: au "English (Australian)" */
    /* same as: cm "English (Cameroon)" */
    /* same as: cn "Chinese" */
    /* same as: id "Indonesian (Latin)" */
    /* same as: kr "Korean" */
    {.langid = 0x0407, .hashes = {{0x13b1,0x33f9,0xcb10,0xeb68,0x8cf2,0x7140,0x981d,0x0256}}, .layout = "de", .description = "German"},
    /* same as: at "German (Austria)" */
    {.langid = 0x040c, .hashes = {{0x819d,0x1aff,0x7a2a,0x21e7,0x6000,0x3f44,0x5521,0x364e}}, .layout = "fr", .description = "French"},
    {.langid = 0x048c, .hashes = {{0x056e,0xda81,0x7ce6,0xc43f,0x5f40,0xe9fa,0x41fc,0xbf16}}, .layout = "af", .description = "Dari"},
    {.langid = 0x041c, .hashes = {{0x7e1e,0x33f9,0xcb10,0xeb68,0xf6d5,0xb65e,0xe8f9,0xf8cd}}, .layout = "al", .description = "Albanian"},
    {.langid = 0x042b, .hashes = {{0x1c5d,0xb758,0x803a,0x5068,0xba59,0x3b2e,0xdda7,0x9cab}}, .layout = "am", .description = "Armenian"},
    {.langid = 0x0401, .hashes = {{0xe848,0xfe39,0x385b,0x2be2,0xcc30,0x9c4b,0x6801,0x8e79}}, .layout = "ara", .description = "Arabic"},
    /* same as: iq "Iraqi" */
    /* same as: sy "Arabic (Syria)" */
    {.langid = 0x042c, .hashes = {{0x1df5,0x2ffe,0xcb10,0x3118,0xd28b,0xc221,0xb29f,0x4e10}}, .layout = "az", .description = "Azerbaijani"},
    {.langid = 0x201a, .hashes = {{0x417f,0x33f9,0xcb10,0xeb68,0x3809,0x75c6,0xa8e2,0x05f8}}, .layout = "ba", .description = "Bosnian"},
    {.langid = 0x0445, .hashes = {{0x157c,0x8f5a,0xd906,0x3002,0xb3d0,0xba72,0x2330,0xa0b5}}, .layout = "bd", .description = "Bangla"},
    {.langid = 0x080c, .hashes = {{0xd1f7,0x1aff,0x7a2a,0x21e7,0x02e1,0xf48f,0x2b18,0x2511}}, .layout = "be", .description = "Belgian"},
    {.langid = 0x0402, .hashes = {{0x117b,0x52a8,0x41c2,0xc880,0xb488,0x020f,0xae5a,0x6c57}}, .layout = "bg", .description = "Bulgarian"},
    {.langid = 0x0816, .hashes = {{0x68eb,0x4b8f,0xcb10,0x3118,0x4f40,0x4c28,0xfd0d,0x21e8}}, .layout = "br", .description = "Portuguese (Brazil)"},
    {.langid = 0x0c00, .hashes = {{0x0db8,0x0db8,0x9686,0xbf40,0x7fc1,0x7fc1,0x7fc1,0xf81b}}, .layout = "brai", .description = "Braille"},
    {.langid = 0x0c51, .hashes = {{0xa4b1,0x5bd0,0x1ca4,0x80a4,0xa6a1,0x63c7,0xcfa1,0x107d}}, .layout = "bt", .description = "Dzongkha"},
    {.langid = 0x0832, .hashes = {{0x4b1d,0x4b8f,0xcb10,0x3118,0x447e,0x21d4,0x151d,0xaca9}}, .layout = "bw", .description = "Tswana"},
    /* same as: ke "Swahili (Kenya)" */
    {.langid = 0x0423, .hashes = {{0x88d9,0x4862,0x9778,0xc190,0x74b5,0xa202,0xdc98,0x3acb}}, .layout = "by", .description = "Belarusian"},
    {.langid = 0x1009, .hashes = {{0x0953,0x4b8f,0xcb10,0x3118,0xa4f1,0xf8dc,0x841f,0xf2bb}}, .layout = "ca", .description = "French (Canada)"},
    {.langid = 0x100c, .hashes = {{0x643f,0x1aff,0x7a2a,0x21e7,0x8fa6,0x7513,0xc524,0xb309}}, .layout = "cd", .description = "French (Democratic Republic of the Congo)"},
    {.langid = 0x0807, .hashes = {{0x4577,0x33f9,0xcb10,0xeb68,0x2610,0xe2ad,0xe20b,0xa905}}, .layout = "ch", .description = "German (Switzerland)"},
    {.langid = 0x0405, .hashes = {{0x62f7,0x33f9,0xcb10,0xeb68,0xc217,0x9ceb,0x253e,0x62b8}}, .layout = "cz", .description = "Czech"},
    {.langid = 0x0406, .hashes = {{0x1339,0x4b8f,0xcb10,0x3118,0x1e91,0x18b5,0x1385,0x780c}}, .layout = "dk", .description = "Danish"},
    {.langid = 0x085f, .hashes = {{0x819d,0x1aff,0x7a2a,0x21e7,0xa3de,0xa98c,0x7a6b,0xe78a}}, .layout = "dz", .description = "Berber (Algeria, Latin)"},
    {.langid = 0x0425, .hashes = {{0x1339,0x4b8f,0xcb10,0x3118,0x138d,0xc370,0x2034,0xc988}}, .layout = "ee", .description = "Estonian"},
    {.langid = 0x0c00, .hashes = {{0x4b1d,0xb62e,0xcb10,0x44d1,0x0be2,0x3d84,0x19b3,0xcea5}}, .layout = "epo", .description = "Esperanto"},
    {.langid = 0x040a, .hashes = {{0x3398,0x4b8f,0xcb10,0x3118,0xcab4,0xa6e4,0x2e80,0xa905}}, .layout = "es", .description = "Spanish"},
    {.langid = 0x045e, .hashes = {{0x00b2,0xa691,0x21fb,0x879b,0x96e0,0x6a3a,0x922f,0x14cd}}, .layout = "et", .description = "Amharic"},
    {.langid = 0x040b, .hashes = {{0x1339,0x4b8f,0xcb10,0x3118,0x4bd2,0xb042,0x812e,0x171d}}, .layout = "fi", .description = "Finnish"},
    {.langid = 0x0438, .hashes = {{0x1339,0x4b8f,0xcb10,0x3118,0x1e91,0x18b5,0x1385,0x9f72}}, .layout = "fo", .description = "Faroese"},
    {.langid = 0x0809, .hashes = {{0x2557,0x4b8f,0xcb10,0x3118,0xeebc,0x4020,0x4b0a,0xe30a}}, .layout = "gb", .description = "English (UK)"},
    {.langid = 0x0437, .hashes = {{0x4b1d,0xa2f3,0xa93d,0xfb6d,0x45c9,0xcef4,0xfa5c,0x77e6}}, .layout = "ge", .description = "Georgian"},
    {.langid = 0x1009, .hashes = {{0x7a34,0x4b8f,0xcb10,0x3118,0x3289,0x21d4,0x3554,0xaca9}}, .layout = "gh", .description = "English (Ghana)"},
    {.langid = 0x0c00, .hashes = {{0x67f2,0xf196,0x19c4,0x33be,0xf5a8,0x1b86,0xde6f,0xdfad}}, .layout = "gn", .description = "N'Ko (AZERTY)"},
    {.langid = 0x0408, .hashes = {{0x4b1d,0xf09c,0xac6d,0xc250,0x2370,0xef21,0xd15e,0xb610}}, .layout = "gr", .description = "Greek"},
    {.langid = 0x041a, .hashes = {{0x417f,0x33f9,0xcb10,0xeb68,0x197f,0x75c6,0xba2a,0x2bff}}, .layout = "hr", .description = "Croatian"},
    {.langid = 0x040e, .hashes = {{0x33e6,0x33f9,0xcb10,0xeb68,0x77e1,0xb139,0x23e9,0xc497}}, .layout = "hu", .description = "Hungarian"},
    {.langid = 0x1809, .hashes = {{0x2557,0x4b8f,0xcb10,0x3118,0x907a,0xb287,0xf1cb,0x4ce5}}, .layout = "ie", .description = "Irish"},
    {.langid = 0x040d, .hashes = {{0xe848,0xc9fe,0xe9cb,0x9b3f,0xc7a1,0x3207,0xb3b0,0x0172}}, .layout = "il", .description = "Hebrew"},
    {.langid = 0x0439, .hashes = {{0xa7b4,0x3ccc,0x114b,0xc04f,0x07bf,0x7e76,0x7764,0xc571}}, .layout = "in", .description = "Indian"},
    {.langid = 0x0429, .hashes = {{0x056e,0xda81,0x7ce6,0xc43f,0x1bad,0x4b2e,0x794f,0xfa7d}}, .layout = "ir", .description = "Persian"},
    {.langid = 0x040f, .hashes = {{0x417f,0x4b8f,0xcb10,0x3118,0x8cf2,0x66c2,0xcdd7,0x4e8c}}, .layout = "is", .description = "Icelandic"},
    {.langid = 0x0410, .hashes = {{0x87ee,0x4b8f,0xcb10,0x3118,0x9f8f,0xe271,0xa3bc,0xa754}}, .layout = "it", .description = "Italian"},
    {.langid = 0x0411, .hashes = {{0xabbf,0x4b8f,0xcb10,0x3118,0xdd26,0x588c,0xd2fc,0xdc34}}, .layout = "jp", .description = "Japanese"},
    {.langid = 0x0421, .hashes = {{0x0db8,0x0db8,0x9686,0xbf40,0x8c1a,0x7fc1,0x576c,0xe8ec}}, .layout = "jv", .description = "Indonesian (Javanese)"},
    {.langid = 0x0440, .hashes = {{0x88d9,0x9ff8,0x9778,0x2dc4,0xca02,0x08b7,0xdc98,0x9680}}, .layout = "kg", .description = "Kyrgyz"},
    {.langid = 0x0453, .hashes = {{0xb877,0x5dc0,0xa484,0x13fd,0x5dfb,0x6578,0x3726,0x0580}}, .layout = "kh", .description = "Khmer (Cambodia)"},
    {.langid = 0x043f, .hashes = {{0x34d3,0x9ff8,0x9778,0x2dc4,0x63f0,0x08b7,0xdc98,0x8331}}, .layout = "kz", .description = "Kazakh"},
    {.langid = 0x0454, .hashes = {{0x5f1a,0xb8c5,0x4227,0x9059,0x4371,0x805f,0xcd18,0xacbe}}, .layout = "la", .description = "Lao"},
    {.langid = 0x100a, .hashes = {{0x417f,0x4b8f,0xcb10,0x3118,0x006d,0x735a,0x3a00,0x4e70}}, .layout = "latam", .description = "Spanish (Latin American)"},
    {.langid = 0x045b, .hashes = {{0x4b1d,0x819f,0x348c,0xedac,0xe988,0x5134,0x213f,0x6ca9}}, .layout = "lk", .description = "Sinhala (phonetic)"},
    {.langid = 0x0427, .hashes = {{0xae16,0x4b8f,0xcb10,0x3118,0xd7e1,0xd1f8,0x7475,0xe30a}}, .layout = "lt", .description = "Lithuanian"},
    {.langid = 0x0426, .hashes = {{0x4b1d,0x4b8f,0xcb10,0x3118,0x8bb0,0xe505,0xa1a1,0x47c1}}, .layout = "lv", .description = "Latvian"},
    {.langid = 0x1801, .hashes = {{0x819d,0xfe39,0x385b,0x2be2,0xcc30,0x9c4b,0x6801,0x8e79}}, .layout = "ma", .description = "Arabic (Morocco)"},
    {.langid = 0x0481, .hashes = {{0x4b1d,0x4b8f,0xcb10,0x3118,0xc637,0x9957,0x34a4,0xe30a}}, .layout = "mao", .description = "Maori"},
    {.langid = 0x1018, .hashes = {{0x4b1d,0x4b8f,0xcb10,0x3118,0x65cb,0x573a,0x9c5c,0x9e78}}, .layout = "md", .description = "Moldavian"},
    /* same as: ro "Romanian" */
    {.langid = 0x2c1a, .hashes = {{0x417f,0x33f9,0xcb10,0xeb68,0x3809,0x3a3c,0x0623,0x05f8}}, .layout = "me", .description = "Montenegrin"},
    {.langid = 0x042f, .hashes = {{0x1c13,0xccfd,0xa750,0x956e,0x8ace,0x072a,0x2ec1,0x1345}}, .layout = "mk", .description = "Macedonian"},
    {.langid = 0x0c00, .hashes = {{0x819d,0x1aff,0x7a2a,0x21e7,0x6000,0x4c01,0x80b9,0x92a8}}, .layout = "ml", .description = "Bambara"},
    {.langid = 0x0455, .hashes = {{0xc7dd,0x04d7,0x5d48,0xc20e,0xe988,0x21d4,0x3554,0xaca9}}, .layout = "mm", .description = "Burmese"},
    {.langid = 0x0450, .hashes = {{0x6706,0xf027,0x05db,0x1896,0x78ce,0x5059,0x067e,0xdc5a}}, .layout = "mn", .description = "Mongolian"},
    {.langid = 0x043a, .hashes = {{0x76da,0x4b8f,0xcb10,0x3118,0xbe9b,0xf807,0x6339,0xe30a}}, .layout = "mt", .description = "Maltese"},
    {.langid = 0x0465, .hashes = {{0x4b1d,0x500f,0x46ef,0x41f6,0xe988,0x9baa,0x0a52,0x2906}}, .layout = "mv", .description = "Dhivehi"},
    {.langid = 0x043e, .hashes = {{0x53f4,0x151a,0x34d6,0xd868,0x8c1c,0x11b5,0x3abd,0xaf1b}}, .layout = "my", .description = "Malay (Jawi, Arabic Keyboard)"},
    {.langid = 0x1009, .hashes = {{0xabfe,0x4b8f,0xcb10,0x3118,0x3289,0x21d4,0x3554,0xaca9}}, .layout = "ng", .description = "English (Nigeria)"},
    {.langid = 0x0413, .hashes = {{0x2598,0x4b8f,0xcb10,0x3118,0xacc4,0xf0de,0x57ff,0x54d4}}, .layout = "nl", .description = "Dutch"},
    {.langid = 0x1000, .hashes = {{0x1339,0x4b8f,0xcb10,0x3118,0xf758,0x89ca,0x16e6,0xbaa0}}, .layout = "no", .description = "Norwegian"},
    {.langid = 0x0461, .hashes = {{0x6ec5,0x2982,0x8c1d,0xd2f4,0xa210,0x8d0f,0x71f6,0xbeb3}}, .layout = "np", .description = "Nepali"},
    {.langid = 0x0464, .hashes = {{0x4b1d,0x4b8f,0xcb10,0x3118,0xd4eb,0x266e,0xcced,0xe4e5}}, .layout = "ph", .description = "Filipino"},
    {.langid = 0x0420, .hashes = {{0xe848,0x736f,0xab3b,0xe59a,0xba6f,0x4b73,0xf543,0xb95e}}, .layout = "pk", .description = "Urdu (Pakistan)"},
    {.langid = 0x0415, .hashes = {{0x4b1d,0x4b8f,0xcb10,0x3118,0x10bf,0x7da4,0x84e1,0x0e56}}, .layout = "pl", .description = "Polish"},
    {.langid = 0x0416, .hashes = {{0x417f,0x4b8f,0xcb10,0x3118,0xafd5,0x70dd,0x2b18,0xe30a}}, .layout = "pt", .description = "Portuguese"},
    {.langid = 0x081a, .hashes = {{0x417f,0x8ace,0xa750,0x626f,0x3806,0x2b36,0x1e4d,0x54ac}}, .layout = "rs", .description = "Serbian"},
    {.langid = 0x0419, .hashes = {{0x88d9,0x9ff8,0x9778,0x2dc4,0x74b5,0x08b7,0xdc98,0xf678}}, .layout = "ru", .description = "Russian"},
    {.langid = 0x041d, .hashes = {{0x1339,0x4b8f,0xcb10,0x3118,0x198d,0x18b5,0x60cd,0x48a0}}, .layout = "se", .description = "Swedish"},
    {.langid = 0x0424, .hashes = {{0x417f,0x33f9,0xcb10,0xeb68,0x3809,0x75c6,0x8598,0x05f8}}, .layout = "si", .description = "Slovenian"},
    {.langid = 0x041b, .hashes = {{0x39be,0x33f9,0xcb10,0xeb68,0xc217,0x9ceb,0x253e,0x62b8}}, .layout = "sk", .description = "Slovak"},
    {.langid = 0x0488, .hashes = {{0x819d,0x1aff,0x7a2a,0x21e7,0x2f03,0x35f3,0x3b3e,0xcc28}}, .layout = "sn", .description = "Wolof"},
    {.langid = 0x100c, .hashes = {{0x819d,0x1aff,0x7a2a,0x21e7,0xd095,0x3c61,0xab8e,0x53b2}}, .layout = "tg", .description = "French (Togo)"},
    {.langid = 0x041e, .hashes = {{0x4849,0x024c,0x1d59,0x4198,0xd4c7,0xabf2,0x81fa,0x54a5}}, .layout = "th", .description = "Thai"},
    {.langid = 0x0428, .hashes = {{0x88d9,0x1a8e,0x63e8,0x1f51,0xafee,0xa12a,0x0f7a,0xa9a5}}, .layout = "tj", .description = "Tajik"},
    {.langid = 0x0442, .hashes = {{0x4b1d,0x8a6d,0xcb10,0x7587,0xe988,0x21d4,0x3554,0xaca9}}, .layout = "tm", .description = "Turkmen"},
    {.langid = 0x041f, .hashes = {{0x38ae,0x64cc,0xcb10,0x3118,0xb735,0x07a1,0x18a1,0x3c83}}, .layout = "tr", .description = "Turkish"},
    {.langid = 0x1004, .hashes = {{0x4b1d,0x4b8f,0xcb10,0x3118,0x83a7,0xece5,0xa332,0x8bc2}}, .layout = "tw", .description = "Taiwanese"},
    {.langid = 0x1041, .hashes = {{0x343f,0x5b62,0x5c7b,0xb48b,0x6edd,0x8450,0x01fc,0x9bcf}}, .layout = "tz", .description = "Swahili (Tanzania)"},
    {.langid = 0x0422, .hashes = {{0x88d9,0x9ff8,0xba8f,0x2dc4,0x6572,0x351c,0x010e,0x1f16}}, .layout = "ua", .description = "Ukrainian"},
    {.langid = 0x0843, .hashes = {{0x88d9,0x4862,0x3cea,0x2dc4,0xe29e,0x20dd,0x42e1,0xb8be}}, .layout = "uz", .description = "Uzbek"},
    {.langid = 0x042a, .hashes = {{0x41f1,0x4b8f,0xcb10,0x3118,0x6a57,0x0923,0x3554,0xaca9}}, .layout = "vn", .description = "Vietnamese"},
    {.langid = 0x1c09, .hashes = {{0x4b1d,0x4b8f,0xcb10,0x3118,0x1f64,0x4980,0xb470,0x39da}}, .layout = "za", .description = "English (South Africa)"},
    {.langid = 0x0409, .hashes = {{0x5b42,0xad06,0x9bb8,0x710b,0x5913,0x095b,0x59fb,0x39bd}}, .layout = "us:chr", .description = "Cherokee"},
    {.langid = 0x0409, .hashes = {{0x0b7e,0x89c3,0x6acf,0x6486,0x3d66,0x029a,0x1dd8,0xecaa}}, .layout = "us:dvp", .description = "English (programmer Dvorak)"},
    {.langid = 0x0409, .hashes = {{0x4b1d,0x4b8f,0xcb10,0x3118,0xf7f1,0x0cc2,0xe1f2,0x877d}}, .layout = "us:mac", .description = "English (Macintosh)"},
    {.langid = 0x0409, .hashes = {{0x4b1d,0x4a48,0x3404,0x263e,0xe988,0xd283,0x53d7,0x7af9}}, .layout = "us:rus", .description = "Russian (US, phonetic)"},
    {.langid = 0x0409, .hashes = {{0x68eb,0x4b8f,0xcb10,0x3118,0x2f61,0xe297,0xece7,0xb282}}, .layout = "us:intl", .description = "English (US, intl., with dead keys)"},
    {.langid = 0x0409, .hashes = {{0x4b1d,0x4b8f,0xcb10,0x3118,0xda35,0x1d34,0x698a,0xaa88}}, .layout = "us:olpc2", .description = "English (the divide/multiply toggle the layout)"},
    {.langid = 0x0409, .hashes = {{0x4b1d,0x010b,0x6acf,0xc21e,0x538a,0x1146,0xf148,0xecaa}}, .layout = "us:dvorak", .description = "English (Dvorak)"},
    {.langid = 0x0409, .hashes = {{0x4b1d,0x54cd,0xf364,0x1b53,0xe988,0xd916,0xb55f,0x6ee5}}, .layout = "us:norman", .description = "English (Norman)"},
    {.langid = 0x0409, .hashes = {{0x4b1d,0x56d0,0x2c62,0xba0b,0x9a90,0xf4dc,0xc040,0xe2bf}}, .layout = "us:colemak", .description = "English (Colemak)"},
    {.langid = 0x0409, .hashes = {{0x4b1d,0x3706,0x287a,0xb9e3,0xe988,0xa8e9,0xc5eb,0x4df9}}, .layout = "us:workman", .description = "English (Workman)"},
    {.langid = 0x0409, .hashes = {{0x68eb,0x4b8f,0xcb10,0x3118,0x9090,0xd778,0x9263,0xc862}}, .layout = "us:alt-intl", .description = "English (US, alt. intl.)"},
    {.langid = 0x0409, .hashes = {{0x14b3,0xf2d0,0xff75,0x1a09,0x9c6c,0x9188,0x1918,0x32de}}, .layout = "us:dvorak-l", .description = "English (Dvorak, left-handed)"},
    {.langid = 0x0409, .hashes = {{0xe74d,0x45f4,0x9dda,0xb9ed,0xe92a,0x2179,0x0a42,0xad24}}, .layout = "us:dvorak-r", .description = "English (Dvorak, right-handed)"},
    {.langid = 0x0409, .hashes = {{0x4b1d,0x4b8f,0xcb10,0x3118,0x85cf,0xd4b8,0x8d57,0x88f6}}, .layout = "us:symbolic", .description = "English (US, Symbolic)"},
    {.langid = 0x0409, .hashes = {{0x4b1d,0x4b8f,0xcb10,0x3118,0xb8e3,0x3974,0xa92f,0x1523}}, .layout = "us:altgr-intl", .description = "English (intl., with AltGr dead keys)"},
    {.langid = 0x0409, .hashes = {{0x4b1d,0xb068,0x84d7,0x2073,0x9a90,0xf4dc,0xc040,0x7904}}, .layout = "us:colemak_dh", .description = "English (Colemak-DH)"},
    {.langid = 0x0409, .hashes = {{0x4b1d,0x010b,0x6acf,0xc21e,0x1f86,0x56c6,0x6341,0x42d7}}, .layout = "us:dvorak-mac", .description = "English (Dvorak, Macintosh)"},
    {.langid = 0x0409, .hashes = {{0x68eb,0x8232,0x6acf,0xc21e,0x33b2,0x3f86,0xdad3,0x9361}}, .layout = "us:dvorak-intl", .description = "English (Dvorak, intl., with dead keys)"},
    {.langid = 0x0409, .hashes = {{0x68eb,0x3706,0x287a,0xb9e3,0x2f61,0x90bc,0x0fb2,0xbfe5}}, .layout = "us:workman-intl", .description = "English (Workman, intl., with dead keys)"},
    {.langid = 0x0409, .hashes = {{0xca00,0x82bc,0x6acf,0xc21e,0xb285,0x1faa,0xf148,0xecaa}}, .layout = "us:dvorak-classic", .description = "English (classic Dvorak)"},
    {.langid = 0x0409, .hashes = {{0x4b1d,0x010b,0x6acf,0xc21e,0x134a,0x4edc,0xe14d,0xfdda}}, .layout = "us:dvorak-alt-intl", .description = "English (Dvorak, alt. intl.)"},
    {.langid = 0x0407, .hashes = {{0x13b1,0x33f9,0xcb10,0xeb68,0x5c2a,0xf71c,0x554e,0xe5b7}}, .layout = "de:e1", .description = "German (E1)"},
    {.langid = 0x0407, .hashes = {{0x13b1,0xbc63,0x3404,0xe386,0x8cf2,0xd68f,0xdfc4,0xdba3}}, .layout = "de:ru", .description = "Russian (Germany, phonetic)"},
    {.langid = 0x0407, .hashes = {{0x4b1d,0x4b8f,0xcb10,0x3118,0x7ad1,0xad68,0xfd47,0xbe22}}, .layout = "de:us", .description = "German (US)"},
    {.langid = 0x0407, .hashes = {{0xd31f,0x63cc,0xbcd6,0x9eb4,0x3076,0x3d7d,0x506a,0x8f5e}}, .layout = "de:neo", .description = "German (Neo 2)"},
    {.langid = 0x0407, .hashes = {{0x13b1,0xa42e,0x8861,0x81f5,0x4157,0x8601,0x40df,0x92c7}}, .layout = "de:dvorak", .description = "German (Dvorak)"},
    {.langid = 0x0407, .hashes = {{0x13b1,0x4b8f,0xcb10,0x3118,0x8cf2,0x2352,0x981d,0xd706}}, .layout = "de:qwerty", .description = "German (QWERTY)"},
    {.langid = 0x040c, .hashes = {{0x4b1d,0x4b8f,0xcb10,0x3118,0x98cf,0xe666,0xc242,0xa8f7}}, .layout = "fr:us", .description = "French (US)"},
    {.langid = 0x040c, .hashes = {{0x819d,0x8edc,0xcb10,0xa235,0xcc5a,0x1df6,0x8bda,0x4344}}, .layout = "fr:bre", .description = "French (Breton)"},
    {.langid = 0x040c, .hashes = {{0x05d8,0x3a77,0x6909,0x41a6,0xe2ce,0x4688,0x0bd2,0x44a0}}, .layout = "fr:geo", .description = "Georgian (France, AZERTY Tskapo)"},
    {.langid = 0x040c, .hashes = {{0xd1f7,0x1aff,0x7a2a,0x21e7,0x3c02,0x52b6,0xa059,0x111f}}, .layout = "fr:mac", .description = "French (Macintosh)"},
    {.langid = 0x040c, .hashes = {{0x9db8,0xaccc,0x4de6,0xbadd,0x9400,0x57d2,0x4ce5,0xa2b5}}, .layout = "fr:bepo", .description = "French (BEPO)"},
    {.langid = 0x040c, .hashes = {{0x2772,0x1aff,0x7a2a,0x8b95,0x417a,0x59b7,0xe437,0x3246}}, .layout = "fr:afnor", .description = "French (AZERTY, AFNOR)"},
    {.langid = 0x040c, .hashes = {{0xd082,0x3dff,0xf7b3,0xce05,0x5ac6,0x47d6,0x208b,0xa78e}}, .layout = "fr:dvorak", .description = "French (Dvorak)"},
    {.langid = 0x040c, .hashes = {{0x819d,0x1aff,0x7a2a,0x21e7,0x8d55,0x9d5b,0x84a5,0x3f06}}, .layout = "fr:latin9", .description = "French (legacy, alt.)"},
    {.langid = 0x040c, .hashes = {{0x9db8,0xaccc,0x4de6,0xb1af,0x3e73,0x33cf,0xbeb9,0x36c7}}, .layout = "fr:bepo_afnor", .description = "French (BEPO, AFNOR)"},
    {.langid = 0x040c, .hashes = {{0x819d,0x1aff,0x7a2a,0x21e7,0x07bc,0xb589,0x5211,0xf3f9}}, .layout = "fr:nodeadkeys", .description = "French (no dead keys)"},
    {.langid = 0x040c, .hashes = {{0x819d,0x1aff,0x7a2a,0x21e7,0x2260,0x324d,0x6550,0xec6e}}, .layout = "fr:oss_latin9", .description = "French (alt., Latin-9 only)"},
    {.langid = 0x040c, .hashes = {{0x819d,0x1aff,0x7a2a,0x21e7,0x5310,0xd060,0x8e10,0x54d2}}, .layout = "fr:oss_nodeadkeys", .description = "French (alt., no dead keys)"},
    {.langid = 0x048c, .hashes = {{0x241c,0x93bc,0xf23a,0xe09f,0x5f40,0x1470,0x9ede,0x4c4b}}, .layout = "af:ps", .description = "Pashto"},
    {.langid = 0x048c, .hashes = {{0x056e,0xda81,0xb0b3,0x7d53,0x5f40,0x5d88,0xf80d,0x17a5}}, .layout = "af:uz", .description = "Uzbek (Afghanistan)"},
    {.langid = 0x048c, .hashes = {{0xc905,0x62cd,0x5815,0xfffb,0x02f7,0x15aa,0x32d7,0xbf16}}, .layout = "af:fa-olpc", .description = "Dari (Afghanistan, OLPC)"},
    {.langid = 0x048c, .hashes = {{0x8936,0x09bf,0x635f,0x2bf2,0x1a3b,0x038f,0xd86e,0xf29a}}, .layout = "af:ps-olpc", .description = "Pashto (Afghanistan, OLPC)"},
    {.langid = 0x048c, .hashes = {{0xc905,0x62cd,0x8fa5,0xfd4b,0x02f7,0x946c,0x6fac,0x17a5}}, .layout = "af:uz-olpc", .description = "Uzbek (Afghanistan, OLPC)"},
    {.langid = 0x041c, .hashes = {{0x4b1d,0x4b8f,0xcb10,0x3118,0x5ac6,0x2b31,0x9279,0xff79}}, .layout = "al:plisi", .description = "Albanian (Plisi)"},
    {.langid = 0x041c, .hashes = {{0x4b1d,0x0db8,0x9686,0xbf40,0xe988,0x60cb,0x53d7,0x7af9}}, .layout = "al:veqilharxhi", .description = "Albanian (Veqilharxhi)"},
    {.langid = 0x042b, .hashes = {{0xa29f,0x3ef8,0x8c2a,0x6e94,0xbfbd,0xcb3f,0xa7a0,0x5db1}}, .layout = "am:eastern", .description = "Armenian (eastern)"},
    {.langid = 0x042b, .hashes = {{0xa29f,0x4192,0x5115,0x537e,0xbfbd,0x1615,0x9e8e,0x013c}}, .layout = "am:western", .description = "Armenian (western)"},
    {.langid = 0x042b, .hashes = {{0xbae6,0xc837,0xec03,0x0e60,0x4f92,0x7bc7,0x5016,0x09d3}}, .layout = "am:phonetic", .description = "Armenian (phonetic)"},
    {.langid = 0x042b, .hashes = {{0xa29f,0x3ef8,0x286a,0x26dd,0xbfbd,0xcb3f,0x9df8,0x339c}}, .layout = "am:eastern-alt", .description = "Armenian (alt. eastern)"},
    {.langid = 0x042b, .hashes = {{0x7720,0x642b,0xec03,0x0e60,0x2cb4,0x1755,0x5016,0x09d3}}, .layout = "am:phonetic-alt", .description = "Armenian (alt. phonetic)"},
    {.langid = 0x0401, .hashes = {{0x53f4,0xb04e,0x21ad,0xe4ac,0x5481,0x9c4b,0x8d7d,0x8e79}}, .layout = "ara:mac", .description = "Arabic (Macintosh)"},
    {.langid = 0x0401, .hashes = {{0x0cb6,0xcdb1,0x21b2,0x8b2a,0x5481,0x9c4b,0x8d7d,0x8e79}}, .layout = "ara:olpc", .description = "Arabic (OLPC)"},
    {.langid = 0x0401, .hashes = {{0x4011,0x9dbc,0x9d9a,0x9bbc,0x3e48,0xc649,0x590b,0xfb89}}, .layout = "ara:buckwalter", .description = "Arabic (Buckwalter)"},
    {.langid = 0x0c07, .hashes = {{0x13b1,0x33f9,0xcb10,0xeb68,0xae31,0xbc41,0x3461,0x4b7d}}, .layout = "at:mac", .description = "German (Austria, Macintosh)"},
    {.langid = 0x0c07, .hashes = {{0x13b1,0x33f9,0xcb10,0xeb68,0xcd50,0x2f12,0xf4a6,0xa9a3}}, .layout = "at:nodeadkeys", .description = "German (Austria, no dead keys)"},
    {.langid = 0x042c, .hashes = {{0x8586,0xc6a7,0x9778,0x9133,0x4b1c,0x1fc6,0xabca,0x9bb3}}, .layout = "az:cyrillic", .description = "Azerbaijani (Cyrillic)"},
    {.langid = 0x201a, .hashes = {{0x417f,0xf4a6,0xcb10,0xeb30,0x3809,0x9689,0xa8e2,0x4788}}, .layout = "ba:unicode", .description = "Bosnian (with Bosnian digraphs)"},
    {.langid = 0x0445, .hashes = {{0x6137,0x79a3,0xc591,0x95bf,0xe632,0x2e65,0x3972,0x451c}}, .layout = "bd:probhat", .description = "Bangla (Probhat)"},
    {.langid = 0x080c, .hashes = {{0xd1f7,0x1aff,0x7a2a,0x21e7,0xabba,0x88c6,0x03cf,0xc30e}}, .layout = "be:oss", .description = "Belgian (alt.)"},
    {.langid = 0x080c, .hashes = {{0xd1f7,0x1aff,0x7a2a,0x21e7,0x22d5,0x48c4,0x6f7b,0x47b9}}, .layout = "be:nodeadkeys", .description = "Belgian (no dead keys)"},
    {.langid = 0x0402, .hashes = {{0x117b,0xda7a,0x41c2,0x2ba2,0x5fe4,0xc1c5,0x6a93,0x379f}}, .layout = "bg:bekl", .description = "Bulgarian (enhanced)"},
    {.langid = 0x0402, .hashes = {{0x93c8,0x1b60,0x3404,0xd16a,0xf4b3,0xa870,0x64ed,0xc917}}, .layout = "bg:phonetic", .description = "Bulgarian (traditional phonetic)"},
    {.langid = 0x0402, .hashes = {{0x93c8,0xb7c0,0x3404,0x468e,0xf4b3,0x2b4e,0xe37d,0xf619}}, .layout = "bg:bas_phonetic", .description = "Bulgarian (new phonetic)"},
    {.langid = 0x0816, .hashes = {{0x68eb,0x82bc,0x6acf,0xc21e,0xba5c,0x8a4c,0xde44,0x2e58}}, .layout = "br:dvorak", .description = "Portuguese (Brazil, Dvorak)"},
    {.langid = 0x0816, .hashes = {{0x68eb,0x850e,0x441f,0xbc29,0xba5c,0x3b1a,0x1ea9,0x0952}}, .layout = "br:nativo", .description = "Portuguese (Brazil, Nativo)"},
    {.langid = 0x0816, .hashes = {{0x68eb,0x3071,0x441f,0x7fca,0xba5c,0x9615,0x1ea9,0x524f}}, .layout = "br:nativo-epo", .description = "Esperanto (Brazil, Nativo)"},
    {.langid = 0x0816, .hashes = {{0x3dc2,0x4b8f,0xcb10,0x3118,0x1abe,0x4c28,0x95a9,0xb63b}}, .layout = "br:nodeadkeys", .description = "Portuguese (Brazil, no dead keys)"},
    {.langid = 0x1009, .hashes = {{0xbcb9,0x96c8,0x1903,0x5d26,0x0ea1,0x3bc6,0xad72,0x5f3d}}, .layout = "ca:ike", .description = "Inuktitut"},
    {.langid = 0x1009, .hashes = {{0x6a68,0x4b8f,0xcb10,0x3118,0x90a6,0x6618,0x6d40,0xc867}}, .layout = "ca:multix", .description = "Canadian (intl.)"},
    {.langid = 0x1009, .hashes = {{0x0953,0x6b38,0x6acf,0xc21e,0xde06,0x126e,0xa9da,0x28bb}}, .layout = "ca:fr-dvorak", .description = "French (Canada, Dvorak)"},
    {.langid = 0x1009, .hashes = {{0x6f6b,0x4b8f,0xcb10,0x3118,0x0f68,0xe476,0x8b92,0x4850}}, .layout = "ca:fr-legacy", .description = "French (Canada, legacy)"},
    {.langid = 0x1009, .hashes = {{0xdfdd,0xf364,0x7570,0x5438,0x7264,0x55c4,0xef86,0xb9eb}}, .layout = "ca:multi-2gr", .description = "Canadian (intl., 2nd part)"},
    {.langid = 0x0807, .hashes = {{0x4577,0x33f9,0xcb10,0xeb68,0xd57f,0xf17e,0xca20,0xc7b5}}, .layout = "ch:fr_mac", .description = "French (Switzerland, Macintosh)"},
    {.langid = 0x2c0c, .hashes = {{0xf9a1,0x1aff,0x7a2a,0x21e7,0xc20f,0xd656,0x87fd,0xc405}}, .layout = "cm:azerty", .description = "Cameroon (AZERTY, intl.)"},
    {.langid = 0x2c0c, .hashes = {{0x490f,0x418f,0x6acf,0x697d,0x0e16,0xbaef,0xed79,0x859b}}, .layout = "cm:dvorak", .description = "Cameroon (Dvorak, intl.)"},
    {.langid = 0x2c0c, .hashes = {{0x4b1d,0x2dce,0x68a8,0x3118,0x3455,0xf21c,0xd497,0x283d}}, .layout = "cm:mmuock", .description = "Mmuock"},
    {.langid = 0x2c0c, .hashes = {{0x4b1d,0x4b8f,0xcb10,0x3118,0x3e2d,0xae45,0xec32,0xc508}}, .layout = "cm:qwerty", .description = "Cameroon Multilingual (QWERTY, intl.)"},
    {.langid = 0x0404, .hashes = {{0xe848,0xbf32,0xd83a,0x8c52,0x25fc,0x1273,0x90ee,0x5340}}, .layout = "cn:ug", .description = "Uyghur"},
    {.langid = 0x0404, .hashes = {{0x8044,0x9c4b,0xd984,0x820c,0xf56d,0xd0d0,0xdc78,0xe5b1}}, .layout = "cn:tib", .description = "Tibetan"},
    {.langid = 0x0404, .hashes = {{0x40b0,0x196e,0x620c,0x5f6d,0xbca4,0x5787,0xe075,0x105a}}, .layout = "cn:mon_trad", .description = "Mongolian (Bichig)"},
    {.langid = 0x0404, .hashes = {{0x7b94,0x5bc5,0x17ac,0x4128,0xc076,0xcbc9,0x5a30,0x4fcd}}, .layout = "cn:mon_trad_todo", .description = "Mongolian (Todo)"},
    {.langid = 0x0404, .hashes = {{0x2aa9,0xf059,0xfef0,0x382a,0x3f6d,0x3fc0,0x2b11,0x5c12}}, .layout = "cn:mon_trad_xibe", .description = "Mongolian (Xibe)"},
    {.langid = 0x0404, .hashes = {{0xa19b,0x5f2c,0xf216,0x2263,0x2c27,0xae54,0xa23a,0x01b2}}, .layout = "cn:mon_todo_galik", .description = "Mongolian (Todo Galik)"},
    {.langid = 0x0404, .hashes = {{0x82a0,0x87e9,0xd90d,0x5027,0xc344,0xfca1,0xa73a,0x40d4}}, .layout = "cn:mon_trad_galik", .description = "Mongolian (Galik)"},
    {.langid = 0x0404, .hashes = {{0xef24,0xed27,0x03ad,0x382a,0x54ba,0x0635,0xe28a,0x34f9}}, .layout = "cn:mon_trad_manchu", .description = "Mongolian (Manchu)"},
    {.langid = 0x0404, .hashes = {{0xad2e,0xb5df,0x624f,0xa70e,0x37bd,0x0c3d,0xe26d,0xb315}}, .layout = "cn:mon_manchu_galik", .description = "Mongolian (Manchu Galik)"},
    {.langid = 0x0405, .hashes = {{0xfce8,0xd552,0x3404,0x4bff,0x7f54,0xdd95,0xae47,0x7147}}, .layout = "cz:rus", .description = "Russian (Czech, phonetic)"},
    {.langid = 0x0405, .hashes = {{0x0db8,0x25dc,0xa5e3,0x4c08,0x7fc1,0x8ea5,0xef12,0x1621}}, .layout = "cz:ucw", .description = "Czech (UCW, only accented letters)"},
    {.langid = 0x0405, .hashes = {{0x62f7,0x4b8f,0xcb10,0x3118,0xb473,0xd3c3,0xaeb7,0x62b8}}, .layout = "cz:qwerty", .description = "Czech (QWERTY)"},
    {.langid = 0x0405, .hashes = {{0x62f7,0x4b8f,0xcb10,0x3118,0xe438,0x6fcd,0x92fd,0xfaff}}, .layout = "cz:qwerty-mac", .description = "Czech (QWERTY, Macintosh)"},
    {.langid = 0x0406, .hashes = {{0x1339,0x212f,0x6acf,0x58e8,0xe7d2,0xcf74,0x8c0c,0x0040}}, .layout = "dk:dvorak", .description = "Danish (Dvorak)"},
    {.langid = 0x0406, .hashes = {{0x1339,0x4b8f,0xcb10,0x3118,0x7eec,0xf28b,0xfc75,0xfb36}}, .layout = "dk:nodeadkeys", .description = "Danish (no dead keys)"},
    {.langid = 0x085f, .hashes = {{0x819d,0xa3b0,0x4448,0x70c2,0xa3de,0x7bb1,0xb308,0x420b}}, .layout = "dz:ber", .description = "Berber (Algeria, Tifinagh)"},
    {.langid = 0x085f, .hashes = {{0xfbd3,0x4b8f,0xcb10,0x3118,0x6dac,0x4efe,0xf858,0x4765}}, .layout = "dz:qwerty-gb-deadkeys", .description = "Kabyle (QWERTY, UK, with dead keys)"},
    {.langid = 0x0425, .hashes = {{0x4b1d,0x4b8f,0xcb10,0x3118,0x9cf6,0x15d8,0x3a6b,0x4420}}, .layout = "ee:us", .description = "Estonian (US)"},
    {.langid = 0x0c00, .hashes = {{0x6b82,0x3b97,0xcb10,0x44d1,0x7fc1,0xe15e,0x061b,0xe008}}, .layout = "epo:legacy", .description = "Esperanto (legacy)"},
    {.langid = 0x040a, .hashes = {{0x3398,0x11e5,0x986f,0x6696,0x5b78,0x9bf0,0x6530,0x7065}}, .layout = "es:dvorak", .description = "Spanish (Dvorak)"},
    {.langid = 0x040b, .hashes = {{0x1339,0x4b8f,0xcb10,0x3118,0x043f,0x5902,0xc7bc,0x311f}}, .layout = "fi:mac", .description = "Finnish (Macintosh)"},
    {.langid = 0x040b, .hashes = {{0x417f,0x4bbb,0xcb10,0x7125,0x8266,0x8856,0xfa5a,0x4f6e}}, .layout = "fi:smi", .description = "Northern Saami (Finland)"},
    {.langid = 0x040b, .hashes = {{0x1339,0x4b8f,0xcb10,0x3118,0xe4dc,0xa876,0xb77c,0xe92e}}, .layout = "fi:classic", .description = "Finnish (classic)"},
    {.langid = 0x040b, .hashes = {{0x1339,0x4b8f,0xcb10,0x3118,0x8a50,0x8eb5,0x0662,0x0b5f}}, .layout = "fi:nodeadkeys", .description = "Finnish (classic, no dead keys)"},
    {.langid = 0x0809, .hashes = {{0x2557,0x4b8f,0xcb10,0x3118,0x253b,0x2a12,0x2994,0x1842}}, .layout = "gb:extd", .description = "English (UK, extended, Windows)"},
    {.langid = 0x0809, .hashes = {{0x2557,0xe7b2,0x6acf,0xc21e,0x2c62,0x4edc,0x3ddb,0x1447}}, .layout = "gb:dvorak", .description = "English (UK, Dvorak)"},
    {.langid = 0x0809, .hashes = {{0x2557,0x56d0,0x2c62,0xba0b,0x5fb3,0xf4dc,0xc7a3,0xb6d7}}, .layout = "gb:colemak", .description = "English (UK, Colemak)"},
    {.langid = 0x0809, .hashes = {{0x7679,0x4b8f,0xcb10,0x3118,0xd39a,0x638e,0x994b,0xd581}}, .layout = "gb:mac_intl", .description = "English (UK, Macintosh, intl.)"},
    {.langid = 0x0809, .hashes = {{0x2557,0x82bc,0x6acf,0xc21e,0x62be,0x973e,0x0afe,0x1447}}, .layout = "gb:dvorakukp", .description = "English (UK, Dvorak, with UK punctuation)"},
    {.langid = 0x0809, .hashes = {{0x2557,0xb068,0x84d7,0x5971,0x5fb3,0xf4dc,0xc7a3,0x3b46}}, .layout = "gb:colemak_dh", .description = "English (UK, Colemak-DH)"},
    {.langid = 0x0437, .hashes = {{0x7fd6,0x9ff8,0x9778,0x2dc4,0xf85c,0x08b7,0xf322,0x0aaf}}, .layout = "ge:os", .description = "Ossetian (Georgia)"},
    {.langid = 0x0437, .hashes = {{0xdd9f,0x8b1c,0x2b4c,0xa7ca,0xe61a,0x3a11,0x1682,0x1d54}}, .layout = "ge:ru", .description = "Russian (Georgia)"},
    {.langid = 0x0437, .hashes = {{0x18b5,0xed50,0xc786,0xd9a7,0xc190,0x1636,0x4501,0x8d6f}}, .layout = "ge:mess", .description = "Georgian (MESS)"},
    {.langid = 0x0437, .hashes = {{0x979b,0x7125,0xf351,0x5d8e,0x8615,0x8635,0xc2e8,0x1f3f}}, .layout = "ge:ergonomic", .description = "Georgian (ergonomic)"},
    {.langid = 0x1009, .hashes = {{0x4b1d,0x4e31,0x6f2a,0x672b,0x529a,0x7383,0x71f7,0x99b1}}, .layout = "gh:avn", .description = "Avatime"},
    {.langid = 0x1009, .hashes = {{0x7a34,0x06b3,0x0f9c,0x7da2,0x3289,0xef0b,0x258b,0xe892}}, .layout = "gh:ewe", .description = "Ewe"},
    {.langid = 0x1009, .hashes = {{0x7a34,0x49fc,0xdde1,0x9456,0x3289,0xd778,0x3554,0xaca9}}, .layout = "gh:fula", .description = "Fula"},
    {.langid = 0x1009, .hashes = {{0x4b1d,0x4e31,0xcb10,0x71b3,0x2963,0x2934,0x9e27,0xbc3e}}, .layout = "gh:gillbt", .description = "English (Ghana, GILLBT)"},
    {.langid = 0x1009, .hashes = {{0x4b1d,0x4b8f,0xcb10,0x3118,0x3fb4,0x2710,0x0502,0x75da}}, .layout = "gh:generic", .description = "English (Ghana, multilingual)"},
    {.langid = 0x0408, .hashes = {{0x4b1d,0xf09c,0xac6d,0xc250,0xe988,0xb24f,0x4160,0xc20f}}, .layout = "gr:simple", .description = "Greek (simple)"},
    {.langid = 0x0408, .hashes = {{0x4b1d,0xf09c,0xac6d,0xc250,0x8986,0x214c,0x7c6b,0xba86}}, .layout = "gr:extended", .description = "Greek (extended)"},
    {.langid = 0x0408, .hashes = {{0x4b1d,0xf09c,0xac6d,0xc250,0xcafd,0x26de,0xa496,0x1519}}, .layout = "gr:polytonic", .description = "Greek (polytonic)"},
    {.langid = 0x040e, .hashes = {{0x33e6,0x4b8f,0xcb10,0x3118,0x6a79,0xb139,0x5cd8,0xc497}}, .layout = "hu:101_qwerty_comma_nodead", .description = "Hungarian (QWERTY, 101-key, comma, no dead keys)"},
    {.langid = 0x0421, .hashes = {{0xe51c,0x09a2,0xeb3b,0xd772,0xeadf,0x4b1a,0x247a,0xcbc4}}, .layout = "id:phonetic", .description = "Indonesian (Arab Pegon, phonetic)"},
    {.langid = 0x0421, .hashes = {{0xe51c,0x09a2,0xeb3b,0xd772,0x4a81,0xe266,0xd87e,0x6387}}, .layout = "id:phoneticx", .description = "Indonesian (Arab Pegon, extended phonetic)"},
    {.langid = 0x1809, .hashes = {{0x0db8,0x7879,0x49ad,0x026f,0x7fc1,0x4e88,0x7c72,0x1b78}}, .layout = "ie:ogam", .description = "Ogham"},
    {.langid = 0x1809, .hashes = {{0x2557,0x4b8f,0xcb10,0x3118,0x76f7,0x4e88,0x7c72,0x1b78}}, .layout = "ie:ogam_is434", .description = "Ogham (IS434)"},
    {.langid = 0x1809, .hashes = {{0x2557,0x4b8f,0xcb10,0x3118,0x68fc,0x7c96,0x13c3,0xf23c}}, .layout = "ie:UnicodeExpert", .description = "Irish (UnicodeExpert)"},
    {.langid = 0x040d, .hashes = {{0xe848,0x1b61,0xd967,0x3b08,0xcf31,0x32d0,0x1a0a,0x0f72}}, .layout = "il:lyx", .description = "Hebrew (lyx)"},
    {.langid = 0x040d, .hashes = {{0x85e2,0xc61e,0xcb56,0xd9b8,0xab91,0x0ca9,0xefbc,0x561d}}, .layout = "il:biblical", .description = "Hebrew (Biblical, Tiro)"},
    {.langid = 0x040d, .hashes = {{0x4b1d,0x9f5c,0x8aa5,0x90a8,0xc7a1,0x3207,0xb3b0,0x0172}}, .layout = "il:phonetic", .description = "Hebrew (phonetic)"},
    {.langid = 0x0439, .hashes = {{0x98a8,0xdc94,0x0039,0x7ea8,0x6b80,0x42d8,0xa629,0xcf84}}, .layout = "in:ben", .description = "Bangla (India)"},
    {.langid = 0x0439, .hashes = {{0xdff5,0x8bd3,0xc004,0x2569,0x3467,0x985c,0xd952,0x6d59}}, .layout = "in:guj", .description = "Gujarati"},
    {.langid = 0x0439, .hashes = {{0xf0f5,0x6a31,0xf8d7,0x7b3a,0x7445,0x39ef,0xad6c,0xd6f1}}, .layout = "in:kan", .description = "Kannada"},
    {.langid = 0x0439, .hashes = {{0x5c39,0xa3e1,0x4af4,0xb4d2,0x41bf,0x1424,0xf748,0xa915}}, .layout = "in:mal", .description = "Malayalam"},
    {.langid = 0x0439, .hashes = {{0x0543,0x3654,0x9c5e,0x4d71,0x6178,0x185a,0xaeb6,0x51a6}}, .layout = "in:ori", .description = "Oriya"},
    {.langid = 0x0439, .hashes = {{0xc027,0x1af3,0xcabe,0x2cb8,0xe27f,0x8042,0xfb34,0x455e}}, .layout = "in:tam", .description = "Tamil (InScript)"},
    {.langid = 0x0439, .hashes = {{0x1322,0x34bb,0x9303,0x3d65,0x5279,0x9bbf,0xc10a,0xca3a}}, .layout = "in:tel", .description = "Telugu"},
    {.langid = 0x0439, .hashes = {{0xbef4,0x80ba,0xa26e,0x18e7,0xdb9e,0x9926,0x5668,0x8e94}}, .layout = "in:guru", .description = "Punjabi (Gurmukhi)"},
    {.langid = 0x0439, .hashes = {{0x8c37,0xa7cb,0x13a1,0x17df,0x9675,0xf359,0xec6a,0xd790}}, .layout = "in:iipa", .description = "Indic IPA"},
    {.langid = 0x0439, .hashes = {{0xf477,0xd6e9,0x853f,0x5818,0xfcb5,0x8fd2,0x0fdb,0x4dfe}}, .layout = "in:olck", .description = "Ol Chiki"},
    {.langid = 0x0439, .hashes = {{0x6ec5,0xf800,0xf987,0x1428,0xd095,0xcfea,0x22d9,0x46b2}}, .layout = "in:hin-wx", .description = "Hindi (Wx)"},
    {.langid = 0x0439, .hashes = {{0xaab7,0x9310,0x0d01,0xa137,0x7a9f,0xdbb7,0xd3a8,0xfaaf}}, .layout = "in:jhelum", .description = "Punjabi (Gurmukhi Jhelum)"},
    {.langid = 0x0439, .hashes = {{0x9905,0xfc52,0x3d13,0xc615,0x3d84,0x78d4,0xeccc,0x977e}}, .layout = "in:ori-wx", .description = "Oriya (Wx)"},
    {.langid = 0x0439, .hashes = {{0xf73d,0x3ccc,0x114b,0x4533,0xb7a9,0xae72,0x7f1d,0xce23}}, .layout = "in:marathi", .description = "Marathi (enhanced InScript)"},
    {.langid = 0x0439, .hashes = {{0x4b1d,0xddcd,0x0f00,0x068b,0x2d1a,0x5db4,0x6b1f,0xd2bf}}, .layout = "in:bolnagri", .description = "Hindi (Bolnagri)"},
    {.langid = 0x0439, .hashes = {{0x4b1d,0x2708,0x9cf4,0x4341,0x8eef,0x4eab,0x0a52,0x6f27}}, .layout = "in:hin-kagapa", .description = "Hindi (KaGaPa, phonetic)"},
    {.langid = 0x0439, .hashes = {{0x4b1d,0xaa5c,0x74e0,0xb977,0x0f39,0x2be8,0x6d67,0x23b2}}, .layout = "in:kan-kagapa", .description = "Kannada (KaGaPa, phonetic)"},
    {.langid = 0x0439, .hashes = {{0x4b1d,0x7028,0xcd6f,0xde4b,0xdafe,0x13fa,0xaa93,0x4178}}, .layout = "in:tel-kagapa", .description = "Telugu (KaGaPa, phonetic)"},
    {.langid = 0x0439, .hashes = {{0x875e,0x644d,0x03fc,0x8826,0xbaa7,0x341a,0xf223,0x01a8}}, .layout = "in:tel-sarala", .description = "Telugu (Sarala)"},
    {.langid = 0x0439, .hashes = {{0xcfca,0x3208,0x0fc7,0x460b,0x0f71,0x8a05,0xf2c8,0xf048}}, .layout = "in:ben_bornona", .description = "Bangla (India, Bornona)"},
    {.langid = 0x0439, .hashes = {{0x4b1d,0x9728,0x51dd,0x445c,0x7afb,0x6df3,0x6440,0x8e87}}, .layout = "in:mal_lalitha", .description = "Malayalam (Lalitha)"},
    {.langid = 0x0439, .hashes = {{0x8235,0x5ac0,0xea48,0x20a6,0xff13,0x6277,0x9c51,0x2224}}, .layout = "in:urd-winkeys", .description = "Urdu (Windows)"},
    {.langid = 0x0439, .hashes = {{0xc0f8,0xdc94,0x0039,0x26a7,0x50de,0x42d8,0x45ad,0xc438}}, .layout = "in:ben_inscript", .description = "Bangla (India, Baishakhi InScript)"},
    {.langid = 0x0439, .hashes = {{0x850d,0x4d0a,0x4af4,0x8bd8,0x7942,0xa0c1,0xf748,0xe602}}, .layout = "in:mal_enhanced", .description = "Malayalam (enhanced InScript, with rupee)"},
    {.langid = 0x0439, .hashes = {{0x9905,0xb9b4,0x98d5,0x38fc,0x98fd,0x8c1d,0xeb28,0xf954}}, .layout = "in:ori-bolnagri", .description = "Oriya (Bolnagri)"},
    {.langid = 0x0439, .hashes = {{0xceb7,0x0d52,0x85ad,0x0345,0x4133,0x70c5,0x63bb,0x4db0}}, .layout = "in:tam_tamilnet", .description = "Tamil (TamilNet '99)"},
    {.langid = 0x0439, .hashes = {{0x157c,0x9219,0xbba9,0xa82d,0x1770,0x20e4,0xa3d0,0xd844}}, .layout = "in:ben_baishakhi", .description = "Bangla (India, Baishakhi)"},
    {.langid = 0x0439, .hashes = {{0xfeab,0xc4fc,0x88b1,0x911c,0x11fd,0x2f36,0x7233,0xf4cc}}, .layout = "in:ben_gitanjali", .description = "Bangla (India, Gitanjali)"},
    {.langid = 0x0439, .hashes = {{0xfa39,0x2066,0xab3b,0xe59a,0x9aa7,0x03eb,0xe90f,0x829e}}, .layout = "in:urd-phonetic3", .description = "Urdu (alt. phonetic)"},
    {.langid = 0x0439, .hashes = {{0xb957,0xa5a4,0x9862,0x2ce4,0xc3b0,0xa855,0x38b3,0x991e}}, .layout = "in:tam_tamilnet_TAB", .description = "Tamil (TamilNet '99, TAB encoding)"},
    {.langid = 0x0439, .hashes = {{0xc2e5,0x5306,0xc09e,0xf04f,0x4764,0x655b,0x3b48,0x8ce0}}, .layout = "in:tam_tamilnet_TSCII", .description = "Tamil (TamilNet '99, TSCII encoding)"},
    {.langid = 0x0429, .hashes = {{0xbdae,0x93dd,0x0c2c,0xedf2,0x7933,0xf69f,0x8d06,0x3a1d}}, .layout = "ir:ku_f", .description = "Kurdish (Iran, F)"},
    {.langid = 0x0429, .hashes = {{0x4b1d,0x4b8f,0xcb10,0x3118,0x568a,0xdcef,0xcfae,0x7e46}}, .layout = "ir:ku_alt", .description = "Kurdish (Iran, Latin Alt-Q)"},
    {.langid = 0x0429, .hashes = {{0x307f,0x56c0,0x7112,0x61c4,0x2303,0xb381,0x5df2,0x974b}}, .layout = "ir:ku_ara", .description = "Kurdish (Iran, Arabic-Latin)"},
    {.langid = 0x040f, .hashes = {{0x417f,0x4b8f,0xcb10,0x3118,0xb4a6,0x2747,0x0a84,0xbcae}}, .layout = "is:mac", .description = "Icelandic (Macintosh)"},
    {.langid = 0x040f, .hashes = {{0x417f,0x4b8f,0xcb10,0x3118,0x44bd,0x217a,0xc22b,0xf5cf}}, .layout = "is:mac_legacy", .description = "Icelandic (Macintosh, legacy)"},
    {.langid = 0x0410, .hashes = {{0x4b1d,0x4b8f,0xcb10,0x3118,0x8429,0xbc6b,0x1534,0x4f61}}, .layout = "it:us", .description = "Italian (US)"},
    {.langid = 0x0410, .hashes = {{0x87ee,0x4b8f,0xcb10,0x3118,0x220d,0x9ad3,0x18bc,0x99ab}}, .layout = "it:fur", .description = "Friulian (Italy)"},
    {.langid = 0x0410, .hashes = {{0x87ee,0xa2f3,0xa93d,0xfb6d,0x9f8f,0x59ca,0x83c3,0x9c00}}, .layout = "it:geo", .description = "Georgian (Italy)"},
    {.langid = 0x0410, .hashes = {{0x87ee,0x4b8f,0xcb10,0x3118,0xdf57,0x573e,0x4bf8,0xc68d}}, .layout = "it:ibm", .description = "Italian (IBM 142)"},
    {.langid = 0x0410, .hashes = {{0x58e9,0xfb3a,0xcb10,0x21e7,0xf7c9,0xb755,0x8b0e,0x65ac}}, .layout = "it:mac", .description = "Italian (Macintosh)"},
    {.langid = 0x0411, .hashes = {{0x7916,0xcf43,0x7f61,0xda51,0x4e61,0xca5e,0x35aa,0x5ef1}}, .layout = "jp:kana", .description = "Japanese (Kana)"},
    {.langid = 0x0411, .hashes = {{0x81f3,0x5874,0x6acf,0x425b,0xa6bd,0x9a5c,0xd183,0x2bb6}}, .layout = "jp:dvorak", .description = "Japanese (Dvorak)"},
    {.langid = 0x0c00, .hashes = {{0x4b1d,0x4e87,0xcb10,0x05f0,0x7750,0x03da,0x942e,0xe30a}}, .layout = "ke:kik", .description = "Kikuyu"},
    {.langid = 0x0440, .hashes = {{0x0c38,0xdc62,0x4e89,0xab61,0x5654,0xe691,0x7e62,0x5aa6}}, .layout = "kg:phonetic", .description = "Kyrgyz (phonetic)"},
    {.langid = 0x043f, .hashes = {{0x34d3,0x9ff8,0x9778,0x2dc4,0xe988,0x720e,0xc23b,0xf2d0}}, .layout = "kz:ext", .description = "Kazakh (extended)"},
    {.langid = 0x043f, .hashes = {{0x3656,0x4b8f,0xcb10,0xe15c,0x5492,0xb54d,0x0c1f,0x22d8}}, .layout = "kz:latin", .description = "Kazakh (Latin)"},
    {.langid = 0x043f, .hashes = {{0x88d9,0xbdc4,0x142b,0x3510,0x30b4,0x179f,0x71c9,0xd4f4}}, .layout = "kz:kazrus", .description = "Kazakh (with Russian)"},
    {.langid = 0x043f, .hashes = {{0x88d9,0x9ff8,0x9778,0x2dc4,0x30b4,0x5334,0x1eab,0xd567}}, .layout = "kz:ruskaz", .description = "Russian (Kazakhstan, with Kazakh)"},
    {.langid = 0x0454, .hashes = {{0x4699,0xae1b,0x4227,0xd5ee,0x0653,0x2e79,0xecb0,0x5d50}}, .layout = "la:stea", .description = "Lao (STEA)"},
    {.langid = 0x100a, .hashes = {{0x417f,0x11e5,0x986f,0x6696,0x006d,0x5c1a,0x3022,0xad08}}, .layout = "latam:dvorak", .description = "Spanish (Latin American, Dvorak)"},
    {.langid = 0x100a, .hashes = {{0x417f,0x671b,0x2c62,0xba0b,0x006d,0xea9f,0x84a0,0x17fb}}, .layout = "latam:colemak", .description = "Spanish (Latin American, Colemak)"},
    {.langid = 0x100a, .hashes = {{0x417f,0x4b8f,0xcb10,0x3118,0x4101,0x16cc,0x0c64,0x9986}}, .layout = "latam:nodeadkeys", .description = "Spanish (Latin American, no dead keys)"},
    {.langid = 0x045b, .hashes = {{0x4b1d,0xec84,0xff72,0xe550,0xc9a7,0xf903,0x35d8,0xff66}}, .layout = "lk:us", .description = "Sinhala (US)"},
    {.langid = 0x0427, .hashes = {{0xc0fa,0x8e0a,0xcb10,0x37c9,0xe8cd,0xa8ff,0x9b4d,0x8e29}}, .layout = "lt:ibm", .description = "Lithuanian (IBM LST 1205-92)"},
    {.langid = 0x0427, .hashes = {{0x5c76,0x8e0a,0xb330,0x37c9,0x2679,0x0fe5,0xbb7c,0xdf9a}}, .layout = "lt:std", .description = "Lithuanian (standard)"},
    {.langid = 0x0427, .hashes = {{0xb7f8,0xd675,0xf9a6,0x5054,0x5637,0xf18c,0x0267,0x3e45}}, .layout = "lt:lekp", .description = "Lithuanian (LEKP)"},
    {.langid = 0x0427, .hashes = {{0x9668,0x4b1b,0xeb98,0xc761,0x2614,0xd28c,0x0ea7,0x1889}}, .layout = "lt:ratise", .description = "Lithuanian (Ratise)"},
    {.langid = 0x0426, .hashes = {{0x4b1d,0x1ece,0x4168,0xa776,0x8bb0,0x0ec1,0x17c8,0x5342}}, .layout = "lv:modern", .description = "Latvian (modern)"},
    {.langid = 0x0426, .hashes = {{0x495e,0x0764,0xcb10,0x2f75,0x8294,0x5718,0x9144,0x3179}}, .layout = "lv:adapted", .description = "Latvian (adapted)"},
    {.langid = 0x0426, .hashes = {{0x4b1d,0xae61,0x3f4b,0xeaf8,0xd311,0x01c9,0x97f1,0xa8cb}}, .layout = "lv:ergonomic", .description = "Latvian (ergonomic, ŪGJRMV)"},
    {.langid = 0x1801, .hashes = {{0x244d,0xe9b1,0x19dd,0xfc8d,0x55d1,0x53c4,0x5416,0x4379}}, .layout = "ma:tifinagh", .description = "Berber (Morocco, Tifinagh)"},
    {.langid = 0x1801, .hashes = {{0x95cb,0xcfbf,0x3662,0xcdd9,0xd095,0xb400,0xabae,0x3ff8}}, .layout = "ma:tifinagh-alt", .description = "Berber (Morocco, Tifinagh alt.)"},
    {.langid = 0x1801, .hashes = {{0xe1d0,0x1e2a,0x5027,0xbd9a,0x8b24,0x2f5a,0x0fdb,0x74ea}}, .layout = "ma:tifinagh-extended", .description = "Berber (Morocco, Tifinagh extended)"},
    {.langid = 0x1801, .hashes = {{0x244d,0x94fc,0xdff8,0x4145,0x55d1,0x84de,0x218e,0xf770}}, .layout = "ma:tifinagh-phonetic", .description = "Berber (Morocco, Tifinagh phonetic)"},
    {.langid = 0x1801, .hashes = {{0x95cb,0x20e2,0xeb30,0xfb9e,0xd095,0x6696,0x101c,0xa999}}, .layout = "ma:tifinagh-alt-phonetic", .description = "Berber (Morocco, Tifinagh phonetic, alt.)"},
    {.langid = 0x1801, .hashes = {{0xe1d0,0x0e97,0x58c7,0xaa0f,0x8b24,0x09c8,0xe0a9,0x6607}}, .layout = "ma:tifinagh-extended-phonetic", .description = "Berber (Morocco, Tifinagh extended phonetic)"},
    {.langid = 0x1018, .hashes = {{0x730d,0xaf07,0xcb10,0x0f9b,0x12a0,0x21d4,0x2a5b,0x20ce}}, .layout = "md:gag", .description = "Moldavian (Gagauz)"},
    {.langid = 0x2c1a, .hashes = {{0x417f,0x4b8f,0xcb10,0x3118,0x3809,0x75c6,0x0623,0x4082}}, .layout = "me:latinyz", .description = "Montenegrin (Latin, QWERTY)"},
    {.langid = 0x0455, .hashes = {{0xf177,0x6b98,0x0736,0x2c2d,0xe988,0x21d4,0x3554,0xfda2}}, .layout = "mm:mnw", .description = "Mon"},
    {.langid = 0x0455, .hashes = {{0xae37,0xbfe2,0xe4df,0x12c3,0x27c1,0x21d4,0x3554,0xaca9}}, .layout = "mm:shn", .description = "Shan"},
    {.langid = 0x0455, .hashes = {{0x589d,0x9256,0xeb58,0x3d4a,0x02d0,0x0c43,0x7864,0xbd8d}}, .layout = "mm:mnw-a1", .description = "Mon (A1)"},
    {.langid = 0x0455, .hashes = {{0xc0c6,0xb951,0x3e2f,0x5714,0xdac1,0x1624,0x46fd,0x5292}}, .layout = "mm:zawgyi", .description = "Burmese Zawgyi"},
    {.langid = 0x043a, .hashes = {{0xa6a0,0x4b8f,0xcb10,0x3118,0x17d7,0xf807,0xe79a,0x40b2}}, .layout = "mt:us", .description = "Maltese (US)"},
    {.langid = 0x043a, .hashes = {{0x2557,0x4b8f,0xcb10,0x3118,0x7c7a,0x6be4,0x8270,0x865f}}, .layout = "mt:alt-gb", .description = "Maltese (UK, with AltGr overrides)"},
    {.langid = 0x043a, .hashes = {{0x4b1d,0x4b8f,0xcb10,0x3118,0x5a87,0x6ecc,0x9a87,0xf85a}}, .layout = "mt:alt-us", .description = "Maltese (US, with AltGr overrides)"},
    {.langid = 0x043e, .hashes = {{0x8b7f,0x6950,0x2d80,0x4a7a,0x8c1c,0x1f0b,0xd38f,0xe692}}, .layout = "my:phonetic", .description = "Malay (Jawi, phonetic)"},
    {.langid = 0x0413, .hashes = {{0x2598,0x4b8f,0xcb10,0x3118,0xaba4,0x10f8,0xea61,0x6dc5}}, .layout = "nl:std", .description = "Dutch (standard)"},
    {.langid = 0x1000, .hashes = {{0x1339,0x4b8f,0xcb10,0x3118,0x37bd,0x735a,0xfaef,0xe30a}}, .layout = "no:mac", .description = "Norwegian (Macintosh)"},
    {.langid = 0x1000, .hashes = {{0x1339,0x0739,0x2c62,0xba0b,0xf758,0x0bfb,0xd242,0x1a1d}}, .layout = "no:colemak", .description = "Norwegian (Colemak)"},
    {.langid = 0x1000, .hashes = {{0x1339,0x4b8f,0xcb10,0x3118,0xecc4,0x8ee7,0xfa5c,0x2586}}, .layout = "no:nodeadkeys", .description = "Norwegian (no dead keys)"},
    {.langid = 0x0464, .hashes = {{0x4b1d,0x010b,0x6acf,0xc21e,0xc384,0xb5d0,0xfafe,0x98ba}}, .layout = "ph:dvorak", .description = "Filipino (Dvorak, Latin)"},
    {.langid = 0x0464, .hashes = {{0x4b1d,0x56d0,0x2c62,0xba0b,0xd4eb,0x35a1,0x36d8,0x5f9a}}, .layout = "ph:colemak", .description = "Filipino (Colemak, Latin)"},
    {.langid = 0x0464, .hashes = {{0x0db8,0x82f5,0xbe12,0x475c,0x7fc1,0x7fc1,0x7fc1,0x5c04}}, .layout = "ph:qwerty-bay", .description = "Filipino (QWERTY, Baybayin)"},
    {.langid = 0x0464, .hashes = {{0x4b1d,0x6b27,0x053d,0x39f6,0xc384,0x82ca,0xc9c6,0x4974}}, .layout = "ph:capewell-dvorak", .description = "Filipino (Capewell-Dvorak, Latin)"},
    {.langid = 0x0464, .hashes = {{0x4b1d,0x10c0,0x1be6,0x3118,0xd4eb,0xb8af,0xfb2e,0xe4e5}}, .layout = "ph:capewell-qwerf2k6", .description = "Filipino (Capewell-QWERF 2006, Latin)"},
    {.langid = 0x0420, .hashes = {{0xc8af,0x885f,0x50c2,0x9594,0xb31e,0xe473,0xcf7b,0x4f8f}}, .layout = "pk:ara", .description = "Arabic (Pakistan)"},
    {.langid = 0x0420, .hashes = {{0x2920,0xe6a3,0xb994,0x31be,0xb363,0x1a0d,0x3032,0x8431}}, .layout = "pk:snd", .description = "Sindhi"},
    {.langid = 0x0415, .hashes = {{0x0b7e,0x89c3,0x6acf,0x6486,0x396d,0x06a2,0x6eb7,0xd34b}}, .layout = "pl:dvp", .description = "Polish (programmer Dvorak)"},
    {.langid = 0x0415, .hashes = {{0x417f,0x33f9,0xcb10,0xeb68,0x17b7,0xff72,0x67f7,0x593f}}, .layout = "pl:qwertz", .description = "Polish (QWERTZ)"},
    {.langid = 0x0415, .hashes = {{0x4b1d,0x010b,0x6acf,0xc21e,0x2b2b,0x7841,0x6ac8,0x15fc}}, .layout = "pl:dvorak_quotes", .description = "Polish (Dvorak, with Polish quotes on quotemark key)"},
    {.langid = 0x0415, .hashes = {{0x4b1d,0x28a9,0xd17a,0x51a8,0x538a,0xb4e2,0xa65e,0x579d}}, .layout = "pl:ru_phonetic_dvorak", .description = "Russian (Poland, phonetic Dvorak)"},
    {.langid = 0x0416, .hashes = {{0x417f,0x8b2c,0x441f,0xbc29,0x78c6,0xe471,0x0ac5,0xb020}}, .layout = "pt:nativo", .description = "Portuguese (Nativo)"},
    {.langid = 0x0416, .hashes = {{0x417f,0x354a,0x441f,0x7fca,0x78c6,0x5b8e,0x0ac5,0x997a}}, .layout = "pt:nativo-epo", .description = "Esperanto (Portugal, Nativo)"},
    {.langid = 0x0416, .hashes = {{0x417f,0x4b8f,0xcb10,0x3118,0x49ed,0x4542,0xc8f8,0xc846}}, .layout = "pt:nodeadkeys", .description = "Portuguese (no dead keys)"},
    {.langid = 0x0418, .hashes = {{0x1339,0x33f9,0xcb10,0xeb68,0xcb6b,0x88e7,0x8c34,0x35a9}}, .layout = "ro:winkeys", .description = "Romanian (Windows)"},
    {.langid = 0x081a, .hashes = {{0x417f,0x1e9f,0x2996,0xb1ad,0x3806,0x2b36,0xf2ff,0x54ac}}, .layout = "rs:rue", .description = "Pannonian Rusyn"},
    {.langid = 0x0419, .hashes = {{0x200e,0x9ff8,0x9778,0x2dc4,0xb2c0,0x08b7,0xd0c3,0xc839}}, .layout = "ru:dos", .description = "Russian (DOS)"},
    {.langid = 0x0419, .hashes = {{0x6809,0x9ff8,0x9778,0x2dc4,0xad3c,0x08b7,0xae6e,0x9630}}, .layout = "ru:mac", .description = "Russian (Macintosh)"},
    {.langid = 0x0419, .hashes = {{0x88d9,0x9ff8,0x9778,0x2dc4,0x3c36,0x9141,0x5063,0x2c17}}, .layout = "ru:srp", .description = "Serbian (Russia)"},
    {.langid = 0x0419, .hashes = {{0x36a5,0x9ff8,0x9778,0x2dc4,0x8c1c,0x9e98,0x66a9,0x7231}}, .layout = "ru:xal", .description = "Kalmyk"},
    {.langid = 0x0419, .hashes = {{0x44e6,0x4a48,0x3404,0x263e,0x28c7,0xb7a5,0x6f07,0x9c46}}, .layout = "ru:phonetic", .description = "Russian (phonetic)"},
    {.langid = 0x0419, .hashes = {{0x67b3,0x9ff8,0x9778,0x2dc4,0x6830,0x08b7,0xb7f3,0x9bd9}}, .layout = "ru:typewriter", .description = "Russian (typewriter)"},
    {.langid = 0x0419, .hashes = {{0x819d,0xe00d,0xb3ee,0x3c12,0x6000,0xbbed,0xfcdb,0x7dcb}}, .layout = "ru:phonetic_azerty", .description = "Russian (phonetic, AZERTY)"},
    {.langid = 0x0419, .hashes = {{0x44e6,0x28a9,0xd17a,0xcb13,0x0d9b,0x1599,0x2e36,0x9031}}, .layout = "ru:phonetic_dvorak", .description = "Russian (phonetic, Dvorak)"},
    {.langid = 0x0419, .hashes = {{0x44e6,0x4a48,0x10de,0x1915,0xddca,0xb7a5,0x58ae,0x01db}}, .layout = "ru:phonetic_winkeys", .description = "Russian (phonetic, Windows)"},
    {.langid = 0x0419, .hashes = {{0x44e6,0xbbdc,0x10de,0x65b8,0xddca,0xc14e,0x58ae,0xa15f}}, .layout = "ru:phonetic_YAZHERTY", .description = "Russian (phonetic, YAZHERTY)"},
    {.langid = 0x0419, .hashes = {{0xe7ec,0x9ff8,0x9778,0x2dc4,0x5cb3,0x08b7,0x058e,0xfa40}}, .layout = "ru:typewriter-legacy", .description = "Russian (typewriter, legacy)"},
    {.langid = 0x041d, .hashes = {{0x1339,0x4a48,0x3404,0x263e,0xe7cf,0xb3ff,0x53db,0x2e37}}, .layout = "se:rus", .description = "Russian (Sweden, phonetic)"},
    {.langid = 0x041d, .hashes = {{0x1339,0xb5b3,0x6acf,0x5203,0x198d,0xfb7c,0xfcf4,0xc1a0}}, .layout = "se:dvorak", .description = "Swedish (Dvorak)"},
    {.langid = 0x041d, .hashes = {{0x1339,0x212f,0x6acf,0x92e9,0x198d,0x449e,0x6538,0x24f6}}, .layout = "se:svdvorak", .description = "Swedish (Svdvorak)"},
    {.langid = 0x041d, .hashes = {{0x7e1e,0xd995,0x6acf,0x92e9,0x7b2f,0xe7ae,0xf7ed,0x64dd}}, .layout = "se:us_dvorak", .description = "Swedish (Dvorak, intl.)"},
    {.langid = 0x045a, .hashes = {{0x8616,0x6319,0x0dcd,0x603d,0x34c9,0xf7cf,0x0beb,0x3418}}, .layout = "sy:syc", .description = "Syriac"},
    {.langid = 0x045a, .hashes = {{0x8616,0x1079,0xded8,0xc267,0x34c9,0x074c,0xe1df,0xd053}}, .layout = "sy:syc_phonetic", .description = "Syriac (phonetic)"},
    {.langid = 0x041e, .hashes = {{0x24e1,0x2bf8,0x1399,0x7003,0x9287,0x5c39,0x47aa,0x515b}}, .layout = "th:pat", .description = "Thai (Pattachote)"},
    {.langid = 0x041e, .hashes = {{0x8b80,0x024c,0x1d59,0x4198,0xc893,0xffc8,0x121b,0xe070}}, .layout = "th:tis", .description = "Thai (TIS-820.2538)"},
    {.langid = 0x041f, .hashes = {{0xbdae,0x4eab,0x11f4,0x0354,0x7933,0x5858,0x541f,0xaf08}}, .layout = "tr:f", .description = "Turkish (F)"},
    {.langid = 0x041f, .hashes = {{0x056e,0xc8bc,0x0442,0xa43b,0xb525,0x7fc1,0x7fc1,0x5c04}}, .layout = "tr:ot", .description = "Ottoman (Q)"},
    {.langid = 0x041f, .hashes = {{0x5f73,0x0db8,0x9686,0xbf40,0x0e90,0x2db8,0xfedb,0xae86}}, .layout = "tr:otk", .description = "Old Turkic"},
    {.langid = 0x041f, .hashes = {{0xa886,0x64cc,0xcb10,0x3118,0x4732,0xfca0,0x3ed9,0x05bf}}, .layout = "tr:intl", .description = "Turkish (intl., with dead keys)"},
    {.langid = 0x0422, .hashes = {{0x5c95,0x9ff8,0x0d22,0xc190,0x7fe6,0x99bf,0x115e,0xf9a3}}, .layout = "ua:rstu", .description = "Ukrainian (standard RSTU)"},
    {.langid = 0x0422, .hashes = {{0x6809,0x9ff8,0x0d22,0xc190,0x0af1,0x351c,0x03d5,0xcb50}}, .layout = "ua:macOS", .description = "Ukrainian (macOS)"},
    {.langid = 0x0422, .hashes = {{0x5c95,0x9ff8,0x9778,0x2dc4,0x7fe6,0x08b7,0xc8c8,0x7654}}, .layout = "ua:rstu_ru", .description = "Russian (Ukraine, standard RSTU)"},
    {.langid = 0x0422, .hashes = {{0x88d9,0x9ff8,0xba8f,0x2dc4,0x3c36,0x72cd,0x09f3,0x103e}}, .layout = "ua:winkeys", .description = "Ukrainian (Windows)"},
    {.langid = 0x0422, .hashes = {{0x7fd6,0xcdb7,0x3404,0x263e,0xf85c,0x958f,0x9957,0x6eac}}, .layout = "ua:phonetic", .description = "Ukrainian (phonetic)"},
    {.langid = 0x0422, .hashes = {{0x34ff,0xcf2c,0xd3c4,0x65b8,0xb93c,0x9886,0xd9b2,0xde0d}}, .layout = "ua:homophonic", .description = "Ukrainian (homophonic)"},
    {.langid = 0x0422, .hashes = {{0xe7ec,0x9ff8,0x0d22,0xc190,0x5cb3,0x99bf,0xb891,0x9f09}}, .layout = "ua:typewriter", .description = "Ukrainian (typewriter)"},
    {.langid = 0x042a, .hashes = {{0xd417,0x1aff,0x7a2a,0x4ac2,0x47d1,0xc4ad,0x5d11,0xfd67}}, .layout = "vn:fr", .description = "Vietnamese (French)"},
    {.langid = 0x042a, .hashes = {{0x4b1d,0x4b8f,0xcb10,0x3118,0x42ce,0xfb7b,0xd421,0x7bb0}}, .layout = "vn:us", .description = "Vietnamese (US)"},
};

static const char *dump_keymap_hashes( struct keymap_hashes *hashes )
{
    return wine_dbg_sprintf( "{%#06x,%#06x,%#06x,%#06x,%#06x,%#06x,%#06x,%#06x}",
                             hashes->data[0], hashes->data[1], hashes->data[2], hashes->data[3],
                             hashes->data[4], hashes->data[5], hashes->data[6], hashes->data[7] );
}

static unsigned int compute_keycode_range_hash( Display *display, int state, unsigned char first, unsigned char last )
{
    unsigned short keysyms[0x200] = {0};
    unsigned int dummy, sha1[5];
    struct sha1_context ctx;
    unsigned char keyc, mod;
    KeySym keysym;

    for (keyc = first; keyc <= last; keyc++)
    {
        for (mod = 0; mod < 2; ++mod)
        {
            if (!XkbLookupKeySym( display, keyc, state | mod, &dummy, &keysym )) continue;
            if ((keysym >> 9) >= 0x7f && (keysym & ~0x7fff) != 0x1000000) continue;
            keysyms[(keyc - first) * 2 + mod] = keysym;
        }
    }

    sha1_init( &ctx );
    sha1_update( &ctx, (char *)keysyms, (last - first + 1) * 2 * sizeof(*keysyms) );
    sha1_finalize( &ctx, sha1 );
    return sha1[0];
}

static void compute_keymap_hashes( Display *display, int group, struct keymap_hashes *hashes )
{
    unsigned int altgr_mod = 0, dummy;
    int mod, keyc, keyc_min, keyc_max;
    XModifierKeymap *mod_keymap;
    KeySym keysym;

    XDisplayKeycodes( display, &keyc_min, &keyc_max );
    if (keyc_min + 0x35 >= keyc_max) fprintf( stderr, "Unsupported keycode min %u, max %u\n", keyc_min, keyc_max );

    mod_keymap = XGetModifierMapping( display );
    for (mod = 0; mod < 8 * mod_keymap->max_keypermod; mod++)
    {
        int xmod = 1 << (mod / mod_keymap->max_keypermod);
        if (!(keyc = mod_keymap->modifiermap[mod])) continue;
        if (!XkbLookupKeySym( display, keyc, group, &dummy, &keysym )) continue;
        if (keysym == XK_ISO_Level3_Shift) altgr_mod = xmod;
    }
    XFreeModifiermap( mod_keymap );

    hashes->data[0] = compute_keycode_range_hash( display, group, keyc_min + 0x02, keyc_min + 0x0b );
    hashes->data[1] = compute_keycode_range_hash( display, group, keyc_min + 0x10, keyc_min + 0x19 );
    hashes->data[2] = compute_keycode_range_hash( display, group, keyc_min + 0x1e, keyc_min + 0x26 );
    hashes->data[3] = compute_keycode_range_hash( display, group, keyc_min + 0x2c, keyc_min + 0x32 );

    if (altgr_mod <= 1) altgr_mod = Mod5Mask;
    hashes->data[4] = compute_keycode_range_hash( display, group | altgr_mod, keyc_min + 0x02, keyc_min + 0x0d );
    hashes->data[5] = compute_keycode_range_hash( display, group | altgr_mod, keyc_min + 0x10, keyc_min + 0x1b );
    hashes->data[6] = compute_keycode_range_hash( display, group | altgr_mod, keyc_min + 0x1e, keyc_min + 0x29 );
    hashes->data[7] = compute_keycode_range_hash( display, group | altgr_mod, keyc_min + 0x2b, keyc_min + 0x35 );
}

static const struct keymap *find_known_keymap( struct keymap_hashes *hashes, unsigned int *score )
{
    unsigned int i, j, count, best_count = 0;
    const struct keymap *best_keymap = NULL;

    *score = 9;
    for (i = 0; i < ARRAY_SIZE(all_keymaps); ++i)
    {
        if (!memcmp( hashes, &all_keymaps[i].hashes, sizeof(all_keymaps[i].hashes) )) break;
        for (count = 0, j = 0; j < ARRAY_SIZE(all_keymaps[i].hashes.data); ++j) count += all_keymaps[i].hashes.data[j] == hashes->data[j];
        if (best_count >= count) continue;
        best_keymap = all_keymaps + i;
        best_count = count;
    }

    if (i < ARRAY_SIZE(all_keymaps)) return all_keymaps + i;
    *score = best_count;
    return best_keymap;
}

LANGID x11drv_keyboard_detect( Display *display, int group )
{
    struct keymap_hashes hashes;
    const struct keymap *keymap;
    unsigned int score;

    compute_keymap_hashes( display, group, &hashes );

    if ((keymap = find_known_keymap( &hashes, &score )) && score == 9)
    {
        ERR( "Detected keyboard layout %s %s perfect match.\n", debugstr_a(keymap->layout),
               debugstr_a(keymap->description) );
        return keymap->langid;
    }
    if (keymap)
    {
        ERR( "Detected keyboard layout %s %s with %u matches.\n", debugstr_a(keymap->layout),
               debugstr_a(keymap->description), score );
        return keymap->langid;
    }

    ERR_(winediag)( "Keyboard layout detection failed, keymap hashes: %s\n", dump_keymap_hashes( &hashes ) );
    return 0x0400;
}
