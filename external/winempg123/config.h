/*
 * Copyright 2021 Rémi Bernon for CodeWeavers
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

/* Number of bits in a file offset, on hosts where this is settable. */
#define _FILE_OFFSET_BITS 64
/* Define this to the size of native offset type in bits, used for LFS alias
   functions. */
#define LFS_ALIAS_BITS 32
/* Define to the native offset type (long or actually off_t). */
#define lfs_alias_t long

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1
/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1
/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1
/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1
/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1
/* Define to 1 if you have the <windows.h> header file. */
#define HAVE_WINDOWS_H 1

/* Define to 1 if you have the `strerror' function. */
#define HAVE_STRERROR 1

/* Define to use proper rounding. */
#define ACCURATE_ROUNDING 1
/* Define if .balign is present. */
#define ASMALIGN_BALIGN 1
/* Define if __attribute__((aligned(16))) shall be used */
#define CCALIGN 1

/* Define if frame index should be used. */
#define FRAME_INDEX 1
/* Define if gapless is enabled. */
#define GAPLESS 1
/* Define to indicate that float storage follows IEEE754. */
#define IEEE_FLOAT 1
/* size of the frame index seek table */
#define INDEX_SIZE 1000
/* Define for new Huffman decoding scheme. */
#define USE_NEW_HUFFTABLE 1

/* Define if FIFO support is enabled. */
#define FIFO 1
/* Define if IPV6 support is enabled. */
#define IPV6 1
/* Define if network support is enabled. */
#define NETWORK 1

/* Define to use Win32 named pipes */
#define WANT_WIN32_FIFO 1
/* Define to use Win32 sockets */
#define WANT_WIN32_SOCKETS 1
/* Define to use Unicode for Windows */
#define WANT_WIN32_UNICODE 1
