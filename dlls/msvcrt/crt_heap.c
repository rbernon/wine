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

#if 0
#pragma makedep implib
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <process.h>

#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "wine/asm.h"

struct nothrow_t;
typedef size_t align_val_t;

#ifdef _WIN64

extern void* __cdecl operator_new(size_t size) asm("??2@YAPEAX_K@Z");
extern void* __cdecl operator_new_array(size_t size) asm("??_U@YAPEAX_K@Z");
extern void __cdecl operator_delete(void* ptr) asm("??3@YAXPEAX@Z");
extern void __cdecl operator_delete_array(void* ptr) asm("??_V@YAXPEAX@Z");

void __cdecl operator_delete_size(void* ptr, size_t size) asm("??3@YAXPEAX_K@Z");
void __cdecl operator_delete_array_size(void* ptr, size_t size) asm("??_V@YAXPEAX_K@Z");

void* __cdecl operator_new_align(size_t size, align_val_t align) asm("??2@YAPEAX_KW4align_val_t@std@@@Z");
void* __cdecl operator_new_array_align(size_t size, align_val_t align) asm("??_U@YAPEAX_KW4align_val_t@std@@@Z");

void __cdecl operator_delete_align(void* ptr, align_val_t align) asm("??3@YAXPEAXW4align_val_t@std@@@Z");
void __cdecl operator_delete_array_align(void* ptr, align_val_t align) asm("??_V@YAXPEAXW4align_val_t@std@@@Z");

void __cdecl operator_delete_size_align(void* ptr, size_t size, align_val_t align) asm("??3@YAXPEAX_KW4align_val_t@std@@@Z");
void __cdecl operator_delete_array_size_align(void* ptr, size_t size, align_val_t align) asm("??_V@YAXPEAX_KW4align_val_t@std@@@Z");

void* __cdecl operator_new_nothrow(size_t size, struct nothrow_t const* nothrow) asm("??2@YAPEAX_KAEBUnothrow_t@std@@@Z");
void* __cdecl operator_new_array_nothrow(size_t size, struct nothrow_t const* nothrow) asm("??_U@YAPEAX_KAEBUnothrow_t@std@@@Z");

void __cdecl operator_delete_nothrow(void* ptr, struct nothrow_t const* nothrow) asm("??3@YAXPEAXAEBUnothrow_t@std@@@Z");
void __cdecl operator_delete_array_nothrow(void* ptr, struct nothrow_t const* nothrow) asm("??_V@YAXPEAXAEBUnothrow_t@std@@@Z");

void* __cdecl operator_new_align_nothrow(size_t size, align_val_t align, struct nothrow_t const* nothrow) asm("??2@YAPEAX_KW4align_val_t@std@@AEBUnothrow_t@1@@Z");
void* __cdecl operator_new_array_align_nothrow(size_t size, align_val_t align, struct nothrow_t const* nothrow) asm("??_U@YAPEAX_KW4align_val_t@std@@AEBUnothrow_t@1@@Z");

void __cdecl operator_delete_align_nothrow(void* ptr, align_val_t align, struct nothrow_t const* nothrow) asm("??3@YAXPEAXW4align_val_t@std@@AEBUnothrow_t@1@@Z");
void __cdecl operator_delete_array_align_nothrow(void* ptr, align_val_t align, struct nothrow_t const* nothrow) asm("??_V@YAXPEAXW4align_val_t@std@@AEBUnothrow_t@1@@Z");

#else /* _WIN64 */

extern void* __cdecl operator_new(size_t size) asm("??2@YAPAXI@Z");
extern void* __cdecl operator_new_array(size_t size) asm("??_U@YAPAXI@Z");
extern void __cdecl operator_delete(void* ptr) asm("??3@YAXPAX@Z");
extern void __cdecl operator_delete_array(void* ptr) asm("??_V@YAXPAX@Z");

void __cdecl operator_delete_size(void* ptr, size_t size) asm("??3@YAXPAXI@Z");
void __cdecl operator_delete_array_size(void* ptr, size_t size) asm("??_V@YAXPAXI@Z");

void* __cdecl operator_new_align(size_t size, align_val_t align) asm("??2@YAPAXIW4align_val_t@std@@@Z");
void* __cdecl operator_new_array_align(size_t size, align_val_t align) asm("??_U@YAPAXIW4align_val_t@std@@@Z");

void __cdecl operator_delete_align(void* ptr, align_val_t align) asm("??3@YAXPAXW4align_val_t@std@@@Z");
void __cdecl operator_delete_array_align(void* ptr, align_val_t align) asm("??_V@YAXPAXW4align_val_t@std@@@Z");

void __cdecl operator_delete_size_align(void* ptr, size_t size, align_val_t align) asm("??3@YAXPAXIW4align_val_t@std@@@Z");
void __cdecl operator_delete_array_size_align(void* ptr, size_t size, align_val_t align) asm("??_V@YAXPAXIW4align_val_t@std@@@Z");

void* __cdecl operator_new_nothrow(size_t size, struct nothrow_t const* nothrow) asm("??2@YAPAXIABUnothrow_t@std@@@Z");
void* __cdecl operator_new_array_nothrow(size_t size, struct nothrow_t const* nothrow) asm("??_U@YAPAXIABUnothrow_t@std@@@Z");

void __cdecl operator_delete_nothrow(void* ptr, struct nothrow_t const* nothrow) asm("??3@YAXPAXABUnothrow_t@std@@@Z");
void __cdecl operator_delete_array_nothrow(void* ptr, struct nothrow_t const* nothrow) asm("??_V@YAXPAXABUnothrow_t@std@@@Z");

void* __cdecl operator_new_align_nothrow(size_t size, align_val_t align, struct nothrow_t const* nothrow) asm("??2@YAPAXIW4align_val_t@std@@ABUnothrow_t@1@@Z");
void* __cdecl operator_new_array_align_nothrow(size_t size, align_val_t align, struct nothrow_t const* nothrow) asm("??_U@YAPAXIW4align_val_t@std@@ABUnothrow_t@1@@Z");

void __cdecl operator_delete_align_nothrow(void* ptr, align_val_t align, struct nothrow_t const* nothrow) asm("??3@YAXPAXW4align_val_t@std@@ABUnothrow_t@1@@Z");
void __cdecl operator_delete_array_align_nothrow(void* ptr, align_val_t align, struct nothrow_t const* nothrow) asm("??_V@YAXPAXW4align_val_t@std@@ABUnothrow_t@1@@Z");

#endif /* _WIN64 */

void __cdecl operator_delete_size(void* ptr, size_t size) { operator_delete(ptr); }
void __cdecl operator_delete_array_size(void* ptr, size_t size) { operator_delete_array(ptr); }

void* __cdecl operator_new_align(size_t size, align_val_t align) { return _aligned_malloc(size, align); }
void* __cdecl operator_new_array_align(size_t size, align_val_t align) { return _aligned_malloc(size, align); }

void __cdecl operator_delete_align(void* ptr, align_val_t align) { return _aligned_free(ptr); }
void __cdecl operator_delete_array_align(void* ptr, align_val_t align) { return _aligned_free(ptr); }

void __cdecl operator_delete_size_align(void* ptr, size_t size, align_val_t align) { return operator_delete_align(ptr, align); }
void __cdecl operator_delete_array_size_align(void* ptr, size_t size, align_val_t align) { return operator_delete_array_align(ptr, align); }

void* __cdecl operator_new_nothrow(size_t size, struct nothrow_t const* nothrow) { return operator_new(size); }
void* __cdecl operator_new_array_nothrow(size_t size, struct nothrow_t const* nothrow) { return operator_new_array(size); }

void __cdecl operator_delete_nothrow(void* ptr, struct nothrow_t const* nothrow) { operator_delete(ptr); }
void __cdecl operator_delete_array_nothrow(void* ptr, struct nothrow_t const* nothrow) { operator_delete_array(ptr); }

void* __cdecl operator_new_align_nothrow(size_t size, align_val_t align, struct nothrow_t const* nothrow) { return operator_new_align(size, align); }
void* __cdecl operator_new_array_align_nothrow(size_t size, align_val_t align, struct nothrow_t const* nothrow) { return operator_new_array_align(size, align); }

void __cdecl operator_delete_align_nothrow(void* ptr, align_val_t align, struct nothrow_t const* nothrow) { operator_delete_align(ptr, align); }
void __cdecl operator_delete_array_align_nothrow(void* ptr, align_val_t align, struct nothrow_t const* nothrow) { operator_delete_array_align(ptr, align); }
