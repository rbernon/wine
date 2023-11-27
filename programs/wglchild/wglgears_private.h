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

#include "stdarg.h"
#include "stddef.h"

#include "windef.h"
#include "winbase.h"

extern BOOL verbose;

extern double current_time(void);

extern void gdi_draw_layered( HWND hwnd );
extern HWND gdi_create_window( const WCHAR *name, RECT *geometry, BOOL fullscreen, BOOL layered );
extern void gdi_destroy_window( HWND hwnd );
extern BOOL gdi_init(void);

extern void opengl_draw_frame( HWND hwnd );
extern HWND opengl_create_window( const WCHAR *name, RECT *geometry, BOOL fullscreen, INT samples, BOOL use_srgb );
extern void opengl_destroy_window( HWND hwnd );
extern BOOL opengl_init(void);

extern void vulkan_draw_frame( HWND hwnd );
extern HWND vulkan_create_window( const WCHAR *name, RECT *geometry, BOOL fullscreen );
extern void vulkan_destroy_window( HWND hwnd );
extern BOOL vulkan_init(void);
