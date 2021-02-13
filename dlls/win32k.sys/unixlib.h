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

#ifndef __WIN32K_UNIXLIB_H
#define __WIN32K_UNIXLIB_H

#include "windef.h"
#include "wingdi.h"

#include "wine/gdi_driver.h"

struct unix_funcs
{
    DWORD (CDECL *msg_wait_for_multiple_objects)( DWORD count, const HANDLE *handles, DWORD timeout,
                                                  DWORD mask, DWORD flags );
};

extern NTSTATUS CDECL __wine_init_unix_lib( HMODULE module, DWORD reason, const void *ptr_in, void *ptr_out ) DECLSPEC_HIDDEN;

#endif /* __WIN32K_UNIXLIB_H */
