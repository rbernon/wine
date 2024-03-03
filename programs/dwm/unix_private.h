/*
 * Copyright 2022 Rémi Bernon for CodeWeavers
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

#include <pthread.h>
#include <cairo/cairo.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"

struct display;
struct window;

struct display_ops
{
    void (*poll)(struct display *,int);
    void (*destroy)(struct display *);
    struct window *(*window_create)(struct display *,HWND,UINT64);
    void (*window_update)(struct window *, const RECT *window_rect);
    void (*window_destroy)(struct window *);
    cairo_surface_t *(*window_surface)(struct window *, const RECT *window_rect);
};

struct display
{
    const struct display_ops *ops;
    LONG ref;
};

struct window
{
    struct display *display;
    LONG ref;
    pthread_mutex_t lock;
};

static inline struct display *display_acquire( struct display *display )
{
    InterlockedIncrement( &display->ref );
    return display;
}

static inline void display_release( struct display *display )
{
    if (!InterlockedDecrement( &display->ref ))
        display->ops->destroy( display );
}

static inline struct window *window_acquire( struct window *window )
{
    InterlockedIncrement( &window->ref );
    return window;
}

static inline void window_release( struct window *window )
{
    struct display *display = window->display;

    if (!InterlockedDecrement( &window->ref ))
    {
        display->ops->window_destroy( window );
        display_release( display );
    }
}

extern struct display *x11_display_create( const char *name );
