/*
 * Copyright 2017-2018 Roderick Colenbrander
 * Copyright 2022 Jacek Caban for CodeWeavers
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

#ifndef __WINE_OPENGL_DRIVER_H
#define __WINE_OPENGL_DRIVER_H

#include <stdarg.h>
#include <stddef.h>

#include "windef.h"
#include "winbase.h"

#include "winuser.h"

struct opengl_surface;

/* interface between win32u and the user drivers */
struct opengl_driver_funcs
{
    struct opengl_surface *(*p_opengl_surface_create)(HWND);
    void (*p_opengl_surface_destroy)(HWND, struct opengl_surface *);
    void (*p_opengl_surface_attach)(HWND, struct opengl_surface *, const RECT *);
    void (*p_opengl_surface_detach)(HWND, struct opengl_surface *, HDC *);
};

#endif /* __WINE_OPENGL_DRIVER_H */
