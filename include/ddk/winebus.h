/*
 * Mount point manager definitions
 *
 * Copyright 2007 Alexandre Julliard
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

#ifndef __WINE_WINEBUS_H
#define __WINE_WINEBUS_H

#include "ifdef.h"

#define WINEBUSCONTROLTYPE  ((ULONG)'b')

#if defined(_MSC_VER) || defined(__MINGW32__)
#define WINEBUS_DEVICE_NAME     L"\\Device\\WINEBUS"
#define WINEBUS_DOS_DEVICE_NAME L"\\\\.\\WINEBUS"
#else
static const WCHAR WINEBUS_DEVICE_NAME[] = {'\\','D','e','v','i','c','e','\\','W','I','N','E','B','U','S',0};
static const WCHAR WINEBUS_DOS_DEVICE_NAME[] = {'\\','\\','.','\\','W','I','N','E','B','U','S',0};
#endif

#define IOCTL_WINEBUS_X11_OPEN_DISPLAY    CTL_CODE(WINEBUSCONTROLTYPE, 0, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_WINEBUS_X11_CLOSE_DISPLAY   CTL_CODE(WINEBUSCONTROLTYPE, 1, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_WINEBUS_WINDOW_CREATED      CTL_CODE(WINEBUSCONTROLTYPE, 0x100, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_WINEBUS_WINDOW_DESTROYED    CTL_CODE(WINEBUSCONTROLTYPE, 0x101, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#endif /* __WINE_WINEBUS_H */
