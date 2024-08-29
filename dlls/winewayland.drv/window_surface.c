/*
 * Wayland window surface implementation
 *
 * Copyright 2020 Alexandros Frantzis for Collabora Ltd
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
#pragma makedep unix
#endif

#include "config.h"

#include <limits.h>
#include <stdlib.h>
#include <unistd.h>

#include "waylanddrv.h"
#include "wine/debug.h"
#include "wine/server.h"

WINE_DEFAULT_DEBUG_CHANNEL(waylanddrv);

struct wayland_buffer
{
    struct wl_buffer *wl_buffer;
    BOOL busy;
};

struct wayland_buffer_queue
{
    struct wl_event_queue *wl_event_queue;
    UINT buffer_count;
    struct wayland_buffer buffers[];
};

struct wayland_window_surface
{
    struct window_surface header;
    struct wayland_buffer_queue *wayland_buffer_queue;
};

static struct wayland_window_surface *wayland_window_surface_cast(
    struct window_surface *window_surface)
{
    return (struct wayland_window_surface *)window_surface;
}

static void buffer_release(void *data, struct wl_buffer *buffer)
{
    BOOL *busy = data;
    if (busy) *busy = FALSE;
}

static const struct wl_buffer_listener buffer_listener = { buffer_release };

/**********************************************************************
 *          wayland_buffer_queue_destroy
 *
 * Destroys a buffer queue and any contained buffers.
 */
static void wayland_buffer_queue_destroy(struct wayland_buffer_queue *queue)
{
    UINT i;

    for (i = 0; i < queue->buffer_count; i++)
    {
        struct wl_proxy *wl_buffer = (struct wl_proxy *)queue->buffers[i].wl_buffer;
        /* Since this buffer may still be busy, attach it to the per-process
         * wl_event_queue to handle any future buffer release events. */
        wl_proxy_set_user_data(wl_buffer, NULL);
        wl_proxy_set_queue(wl_buffer, process_wayland.wl_event_queue);
    }

    if (queue->wl_event_queue)
    {
        /* Dispatch the event queue before destruction to process any
         * pending buffer release events. This is required after changing
         * the buffer proxy event queue in the previous step, to avoid
         * missing any events. */
        wl_display_dispatch_queue_pending(process_wayland.wl_display,
                                          queue->wl_event_queue);
        wl_event_queue_destroy(queue->wl_event_queue);
    }

    free(queue);
}


/**********************************************************************
 *          wayland_shm_buffer_create
 *
 * Creates a SHM buffer with the specified width, height and format.
 */
struct wayland_shm_buffer *shm_buffer_create(struct wl_shm_pool *pool, int offset,
                                             const BITMAPINFO *info)
{
    int width = info->bmiHeader.biWidth, height = abs(info->bmiHeader.biHeight);
    struct wayland_shm_buffer *shm_buffer;
    int stride;

    if (!(shm_buffer = calloc(1, sizeof(*shm_buffer))))
    {
        ERR("Failed to allocate space for SHM buffer\n");
        goto err;
    }

    shm_buffer->ref = 1;
    shm_buffer->width = width;
    shm_buffer->height = height;

    shm_buffer->damage_region = NtGdiCreateRectRgn(0, 0, width, height);
    if (!shm_buffer->damage_region)
    {
        ERR("Failed to create buffer damage region\n");
        goto err;
    }

    stride = info->bmiHeader.biSizeImage / height;
    shm_buffer->wl_buffer = wl_shm_pool_create_buffer(pool, offset, width, height,
                                                      stride, WL_SHM_FORMAT_XRGB8888);
    if (!shm_buffer->wl_buffer)
    {
        ERR("Failed to create SHM buffer %dx%d\n", width, height);
        goto err;
    }

    return shm_buffer;

err:
    if (shm_buffer) wayland_shm_buffer_unref(shm_buffer);
    return NULL;
}


/**********************************************************************
 *          wayland_buffer_queue_create
 *
 * Creates a buffer queue containing buffers with the specified width and height.
 */
static struct wayland_buffer_queue *wayland_buffer_queue_create( int fd, UINT size, const BITMAPINFO *info, UINT count )
{
    int i, width = info->bmiHeader.biWidth, height = abs(info->bmiHeader.biHeight);
    struct wayland_buffer_queue *queue;
    struct wl_shm_pool *pool;

    if (!(queue = calloc(1, offsetof(struct wayland_buffer_queue, buffers[count])))) return NULL;
    if (!(queue->wl_event_queue = wl_display_create_queue(process_wayland.wl_display)))
    {
        ERR("Failed to create display buffer queue\n");
        free( queue );
        return NULL;
    }

    if (!(pool = wl_shm_create_pool(process_wayland.wl_shm, fd, size)))
    {
        ERR("Failed to create SHM pool fd=%d size=%d\n", fd, size);
        goto err;
    }

    for (i = 0; i < count; i++)
    {
        struct wayland_buffer *buffer = queue->buffers + i;
        buffer->wl_buffer = wl_shm_pool_create_buffer(pool, i * info->bmiHeader.biSizeImage, width, height,
                                                      info->bmiHeader.biSizeImage / height, WL_SHM_FORMAT_XRGB8888);
        if (!buffer->wl_buffer)
        {
            ERR("Failed to create SHM buffer %dx%d\n", width, height);
            break;
        }

        wl_proxy_set_queue((struct wl_proxy *)buffer->wl_buffer, queue->wl_event_queue);
        wl_buffer_add_listener(buffer->wl_buffer, &buffer_listener, &buffer->busy);
        queue->buffer_count++;
    }

    wl_shm_pool_destroy(pool);
    return queue;

err:
    wayland_buffer_queue_destroy(queue);
    return NULL;
}


/***********************************************************************
 *           wayland_window_surface_set_clip
 */
static void wayland_window_surface_set_clip(struct window_surface *window_surface,
                                            const RECT *rects, UINT count)
{
    /* TODO */
}

/**********************************************************************
 *          get_region_data
 */
RGNDATA *get_region_data(HRGN region)
{
    RGNDATA *data;
    DWORD size;

    if (!region) return NULL;
    if (!(size = NtGdiGetRegionData(region, 0, NULL))) return NULL;
    if (!(data = malloc(size))) return NULL;
    if (!NtGdiGetRegionData(region, size, data))
    {
        free(data);
        return NULL;
    }

    return data;
}

/***********************************************************************
 *           wayland_window_surface_flush
 */
static BOOL wayland_window_surface_flush(struct window_surface *window_surface, const RECT *rect, const RECT *dirty,
                                         const BITMAPINFO *color_info, const void *color_bits, BOOL shape_changed,
                                         const BITMAPINFO *shape_info, const void *shape_bits)
{
    return TRUE;
}

/***********************************************************************
 *           wayland_window_surface_destroy
 */
static void wayland_window_surface_destroy(struct window_surface *window_surface)
{
    struct wayland_window_surface *wws = wayland_window_surface_cast(window_surface);

    TRACE("surface=%p\n", wws);

    wayland_buffer_queue_destroy(wws->wayland_buffer_queue);
}

static void wayland_window_surface_create_images( struct window_surface *window_surface, int fd, UINT size,
                                                  const BITMAPINFO *info, UINT count )
{
    struct wayland_window_surface *wws = wayland_window_surface_cast(window_surface);

    TRACE("surface=%p, fd=%d, size=%u, info=%p, count=%u\n", wws, fd, size, info, count);

    wws->wayland_buffer_queue = wayland_buffer_queue_create( fd, size, info, count );
}

static UINT wayland_window_surface_acquire_image( struct window_surface *window_surface )
{
    struct wayland_window_surface *wws = wayland_window_surface_cast(window_surface);
    struct wayland_buffer_queue *queue = wws->wayland_buffer_queue;
    UINT i;

    TRACE("surface=%p\n", wws);

    for (;;)
    {
        /* Dispatch any pending buffer release events. */
        wl_display_dispatch_queue_pending(process_wayland.wl_display, queue->wl_event_queue);

        /* Search through our buffers to find an available one. */
        for (i = 0; i < queue->buffer_count; i++) if (!queue->buffers[i].busy) return i;

        /* We don't have any buffers available, so block waiting for a buffer release event. */
        if (wl_display_dispatch_queue(process_wayland.wl_display, queue->wl_event_queue) == -1)
        {
            ERR("Failed to dispatch buffer queue events\n");
            return -1;
        }
    }
}

static void wayland_window_surface_present_image( struct window_surface *window_surface, UINT index, const RECT *dirty )
{
    struct wayland_window_surface *wws = wayland_window_surface_cast(window_surface);
    struct wayland_buffer_queue *queue = wws->wayland_buffer_queue;
    struct wayland_buffer *buffer = queue->buffers + index;
    struct wayland_surface *wayland_surface;
    struct wayland_win_data *data;

    TRACE("surface=%p, index=%u, dirty=%s\n", wws, index, wine_dbgstr_rect(dirty));

    if (!(data = wayland_win_data_get(window_surface->hwnd))) return;

    if ((wayland_surface = data->wayland_surface) && wayland_surface_reconfigure(wayland_surface, data->client_surface))
    {
        buffer->busy = TRUE;
        wayland_surface_present(wayland_surface, buffer->wl_buffer, &window_surface->rect, dirty);
        wl_surface_commit(wayland_surface->wl_surface);
    }

    wayland_win_data_release(data);

    wl_display_flush(process_wayland.wl_display);
}

static const struct window_surface_funcs wayland_window_surface_funcs =
{
    .set_clip = wayland_window_surface_set_clip,
    .flush = wayland_window_surface_flush,
    .destroy = wayland_window_surface_destroy,
    .create_images = wayland_window_surface_create_images,
    .acquire_image = wayland_window_surface_acquire_image,
    .present_image = wayland_window_surface_present_image,
};

/***********************************************************************
 *           wayland_window_surface_create
 */
static struct window_surface *wayland_window_surface_create(HWND hwnd, const RECT *rect)
{
    char buffer[FIELD_OFFSET(BITMAPINFO, bmiColors[256])];
    BITMAPINFO *info = (BITMAPINFO *)buffer;
    struct wayland_window_surface *wws;
    int width = rect->right - rect->left;
    int height = rect->bottom - rect->top;

    TRACE("hwnd %p rect %s\n", hwnd, wine_dbgstr_rect(rect));

    memset(info, 0, sizeof(*info));
    info->bmiHeader.biSize        = sizeof(info->bmiHeader);
    info->bmiHeader.biWidth       = width;
    info->bmiHeader.biHeight      = -height; /* top-down */
    info->bmiHeader.biPlanes      = 1;
    info->bmiHeader.biBitCount    = 32;
    info->bmiHeader.biSizeImage   = width * height * 4;
    info->bmiHeader.biCompression = BI_RGB;

    return window_surface_create(sizeof(*wws), &wayland_window_surface_funcs, hwnd, rect, info, 0);
}

/***********************************************************************
 *           WAYLAND_CreateWindowSurface
 */
BOOL WAYLAND_CreateWindowSurface(HWND hwnd, BOOL layered, float scale, const RECT *surface_rect,
                                 struct window_surface **surface)
{
    struct window_surface *previous;
    struct wayland_win_data *data;

    TRACE("hwnd %p, layered %u, surface_rect %s, surface %p\n", hwnd, layered, wine_dbgstr_rect(surface_rect), surface);

    if (scale != 1.0) return FALSE; /* let win32u scale for us */
    if ((previous = *surface) && previous->funcs == &wayland_window_surface_funcs) return TRUE;
    if (!(data = wayland_win_data_get(hwnd))) return TRUE; /* use default surface */
    if (previous) window_surface_release(previous);

    *surface = wayland_window_surface_create(data->hwnd, surface_rect);

    wayland_win_data_release(data);
    return TRUE;
}
