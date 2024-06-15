/*
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

#include <stdarg.h>
#include <stddef.h>
#include <assert.h>

#include "windef.h"
#include "winbase.h"

void *CDECL operator new( size_t size ) { return malloc( size ); }
void *CDECL operator new( size_t n, void *ptr ) { return ptr; }
void *CDECL operator new[]( size_t size ) { return malloc( size ); }
void CDECL operator delete( void *ptr ) noexcept { free( ptr ); }
void CDECL operator delete( void *ptr, size_t size ) { free( ptr ); }
void CDECL operator delete[]( void *ptr ) noexcept { free( ptr ); }

namespace std
{

void CDECL __throw_bad_alloc(void) { assert( 0 ); }
void CDECL __throw_bad_array_new_length(void) { assert( 0 ); }
void CDECL __throw_length_error( char const *message ) { assert( 0 ); }

} /* namespace std */
