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

#ifndef __WINE_WIN32U_UNIX_X11_H
#define __WINE_WIN32U_UNIX_X11_H

#ifndef __WINE_CONFIG_H
# error You must include config.h to use this header
#endif

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_X11_XLIB_H
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#endif

#undef Status  /* avoid conflict with wintrnl.h */
typedef int Status;

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "winternl.h"
#include "winnt.h"
#include "wingdi.h"

#include "wine/gdi_driver.h"
#include "wine/debug.h"

#ifdef HAVE_X11_XLIB_H
#define MAKE_FUNCPTR(f) extern typeof(f) * p##f DECLSPEC_HIDDEN;
MAKE_FUNCPTR(XInitThreads)
MAKE_FUNCPTR(XOpenDisplay)
#undef MAKE_FUNCPTR
#endif

extern struct window_surface* CDECL x11_create_window_surface(void) DECLSPEC_HIDDEN;

#endif
