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

/* interface between win32u and the user drivers */
struct opengl_driver_funcs
{
    EGLenum (*p_get_host_egl_platform)(void);

    void *(*p_egl_surface_create)(EGLDisplay, HWND, EGLSurface *);
    void (*p_egl_surface_destroy)(EGLDisplay, HWND, EGLSurface, void *);
    void (*p_egl_surface_attach)(HWND, EGLSurface, void *, const RECT *);
    void (*p_egl_surface_detach)(HWND, EGLSurface, void *, HDC *);
};

#endif /* __WINE_OPENGL_DRIVER_H */
