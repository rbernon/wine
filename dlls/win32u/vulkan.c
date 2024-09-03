/*
 * Vulkan display driver loading
 *
 * Copyright (c) 2017 Roderick Colenbrander
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

#include <dlfcn.h>
#include <pthread.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "win32u_private.h"
#include "ntuser_private.h"

#define VK_NO_PROTOTYPES
#define WINE_VK_HOST
#include "wine/vulkan.h"
#include "wine/vulkan_driver.h"

WINE_DEFAULT_DEBUG_CHANNEL(vulkan);
WINE_DECLARE_DEBUG_CHANNEL(fps);

PFN_vkGetDeviceProcAddr p_vkGetDeviceProcAddr = NULL;
PFN_vkGetInstanceProcAddr p_vkGetInstanceProcAddr = NULL;

static void *vulkan_handle;
static struct vulkan_funcs vulkan_funcs;

#ifdef SONAME_LIBVULKAN

static const struct vulkan_driver_funcs *driver_funcs;

struct vulkan_info_struct
{
    VkStructureType sType;
    struct vulkan_info_struct *pNext;
};

struct semaphore
{
    struct vulkan_object obj;
    D3DKMT_HANDLE d3dkmt_local;
    D3DKMT_HANDLE d3dkmt_global;
    HANDLE shared_handle;
};

static inline struct semaphore *semaphore_from_handle( VkSemaphore handle )
{
    return CONTAINING_RECORD( vulkan_semaphore_from_handle( handle ), struct semaphore, obj );
}

static inline void init_unicode_string( UNICODE_STRING *str, const WCHAR *data )
{
    str->Length = wcslen(data) * sizeof(WCHAR);
    str->MaximumLength = str->Length + sizeof(WCHAR);
    str->Buffer = (WCHAR *)data;
}

static VkResult win32u_vkCreateSemaphore( VkDevice device_handle, const VkSemaphoreCreateInfo *create_info,
                                          const VkAllocationCallbacks *allocator, VkSemaphore *handle )
{
    const struct vulkan_device_funcs *funcs = get_vulkan_device_funcs( device_handle );
    struct vulkan_device *device = vulkan_device_from_handle( device_handle );
    struct vulkan_info_struct *info = (struct vulkan_info_struct *)create_info;
    VkExportSemaphoreCreateInfo export_info = {0}, export_info_host = {0};
    VkSemaphoreCreateInfo create_info_host = *create_info;
    VkExportSemaphoreWin32HandleInfoKHR handle_info = {0};
    VkSemaphoreTypeCreateInfo type_info_host = {0};
    struct semaphore *semaphore;
    VkSemaphore host_semaphore;
    VkResult res;

    TRACE( "device_handle %p, create_info %p, allocator %p, handle %p\n", device_handle, create_info, allocator, handle );

    create_info_host.pNext = NULL;

    while ((info = info->pNext))
    {
        if (info->sType == VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO)
        {
            type_info_host = *(VkSemaphoreTypeCreateInfo *)info;
            type_info_host.pNext = create_info_host.pNext;
            create_info_host.pNext = &type_info_host;
        }
        else if (info->sType == VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO)
        {
            export_info = *(VkExportSemaphoreCreateInfo *)info;
            export_info_host = export_info;
            export_info_host.pNext = create_info_host.pNext;
            export_info_host.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT | VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
            create_info_host.pNext = &export_info_host;
        }
        else if (info->sType == VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR)
        {
            handle_info = *(VkExportSemaphoreWin32HandleInfoKHR *)info;
        }
        else
        {
            FIXME( "Unsupported struct type %#x in chain\n", info->sType );
        }
    }

    if (handle_info.sType) FIXME( "VkExportSemaphoreWin32HandleInfoKHR not implemented\n" );
    if (export_info.sType) FIXME( "VkExportSemaphoreCreateInfo not implemented\n" );

    if (!(semaphore = calloc( 1, sizeof(*semaphore) ))) return VK_ERROR_OUT_OF_HOST_MEMORY;
    if ((res = funcs->p_vkCreateSemaphore( device->obj.host.device, &create_info_host, NULL /* allocator */, &host_semaphore )))
    {
        free( semaphore );
        return res;
    }

    init_vulkan_object( &semaphore->obj, &device->obj, host_semaphore, NULL );
    add_vulkan_device_object( device_handle, &semaphore->obj );

    if (export_info.sType)
    {
        D3DKMT_CREATESYNCHRONIZATIONOBJECT2 create_params;
        VkSemaphoreGetFdInfoKHR get_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR};
        int fd;

        create_params.hDevice = 0 /* FIXME */;
        create_params.Info.Flags.Shared = 1;
        create_params.Info.Flags.NtSecuritySharing = !!handle_info.sType;
        create_params.Info.Type = 0;
        NtGdiDdDDICreateSynchronizationObject2( &create_params );

        get_info.semaphore = semaphore->obj.host.semaphore;
        get_info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
        funcs->p_vkGetSemaphoreFdKHR( device->obj.host.device, &get_info, &fd );

        if (handle_info.sType)
        {
            OBJECT_ATTRIBUTES attr;
            UNICODE_STRING str;

            init_unicode_string( &str, handle_info.name );
            InitializeObjectAttributes( &attr, &str, 0, 0, (void *)handle_info.pAttributes );
            NtGdiDdDDIShareObjects( 1, &create_params.hSyncObject, &attr, handle_info.dwAccess, &semaphore->shared_handle );
        }

        semaphore->d3dkmt_local = create_params.hSyncObject;
        semaphore->d3dkmt_global = create_params.Info.SharedHandle;
    }

    *handle = semaphore->obj.client.semaphore;
    return res;
}

static void win32u_vkDestroySemaphore( VkDevice device_handle, VkSemaphore semaphore_handle, const VkAllocationCallbacks *allocator )
{
    const struct vulkan_device_funcs *funcs = get_vulkan_device_funcs( device_handle );
    struct vulkan_device *device = vulkan_device_from_handle( device_handle );
    struct semaphore *semaphore = semaphore_from_handle( semaphore_handle );

    TRACE( "device_handle %p, semaphore_handle %#jx, allocator %p\n", device_handle, semaphore_handle, allocator );

    if (semaphore->shared_handle) NtClose( semaphore->shared_handle );
    if (semaphore->d3dkmt_local)
    {
        D3DKMT_DESTROYSYNCHRONIZATIONOBJECT destroy_params = {.hSyncObject = semaphore->d3dkmt_local};
        NtGdiDdDDIDestroySynchronizationObject( &destroy_params );
    }

    funcs->p_vkDestroySemaphore( device->obj.host.device, semaphore->obj.host.semaphore, NULL /* allocator */ );
    remove_vulkan_device_object( device_handle, &semaphore->obj );

    free( semaphore );
}

static VkResult win32u_vkGetSemaphoreWin32HandleKHR( VkDevice device_handle, const VkSemaphoreGetWin32HandleInfoKHR *handle_info, HANDLE *handle )
{
    struct semaphore *semaphore = semaphore_from_handle( handle_info->semaphore );

    FIXME( "stub\n" );

    if (handle_info->handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT)
        *handle = UlongToHandle( semaphore->d3dkmt_global );
    if (handle_info->handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT)
    {
        *handle = semaphore->shared_handle;
        semaphore->shared_handle = 0;
    }

    return VK_SUCCESS;
}

static VkResult win32u_vkImportSemaphoreWin32HandleKHR( VkDevice device_handle, const VkImportSemaphoreWin32HandleInfoKHR *handle_info )
{
    const struct vulkan_device_funcs *funcs = get_vulkan_device_funcs( device_handle );
    struct vulkan_device *device = vulkan_device_from_handle( device_handle );
    struct semaphore *semaphore = semaphore_from_handle( handle_info->semaphore );
    VkImportSemaphoreFdInfoKHR import_info = {.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR};
    int fd = -1;

    FIXME( "stub\n" );

    if (semaphore->shared_handle) NtClose( semaphore->shared_handle );
    if (semaphore->d3dkmt_local)
    {
        D3DKMT_DESTROYSYNCHRONIZATIONOBJECT destroy_params = {.hSyncObject = semaphore->d3dkmt_local};
        NtGdiDdDDIDestroySynchronizationObject( &destroy_params );
    }

    if (handle_info->handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT)
    {
        D3DKMT_OPENSYNCHRONIZATIONOBJECT open_params = {.hSharedHandle = HandleToULong(handle_info->handle)};
        NtGdiDdDDIOpenSynchronizationObject( &open_params );

        semaphore->d3dkmt_global = open_params.hSharedHandle;
        semaphore->d3dkmt_local = open_params.hSyncObject;
    }
    if (handle_info->handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT)
    {
        semaphore->shared_handle = handle_info->handle;
    }

    import_info.semaphore = semaphore->obj.host.semaphore;
    import_info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
    import_info.flags = handle_info->flags;
    import_info.fd = fd;
    funcs->p_vkImportSemaphoreFdKHR( device->obj.host.device, &import_info );

    return VK_SUCCESS;
}

static void win32u_vkGetPhysicalDeviceExternalSemaphoreProperties( VkPhysicalDevice device_handle, const VkPhysicalDeviceExternalSemaphoreInfo *info,
                                                                   VkExternalSemaphoreProperties *properties )
{
    static const VkExternalSemaphoreHandleTypeFlagBits handle_types = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT |
                                                                      VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT |
                                                                      VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT;
    /* info -> VkSemaphoreTypeCreateInfo */
    if (info->handleType & handle_types)
    {
        properties->exportFromImportedHandleTypes = handle_types;
        properties->compatibleHandleTypes = handle_types;
        properties->externalSemaphoreFeatures = VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT |
                                                VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT;
    }
    else
    {
        properties->exportFromImportedHandleTypes = 0;
        properties->compatibleHandleTypes = 0;
        properties->externalSemaphoreFeatures = 0;
    }
}

static void win32u_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR( VkPhysicalDevice device_handle, const VkPhysicalDeviceExternalSemaphoreInfo *info,
                                                                      VkExternalSemaphoreProperties *properties )
{
    win32u_vkGetPhysicalDeviceExternalSemaphoreProperties( device_handle, info, properties );
}

struct surface
{
    struct vulkan_object obj;
    VkSurfaceKHR driver_surface;
    void *driver_private;
    struct list entry;
    HWND hwnd;

    struct rb_entry window_entry;
};

static inline struct surface *surface_from_handle( VkSurfaceKHR handle )
{
    return CONTAINING_RECORD( vulkan_surface_from_handle( handle ), struct surface, obj );
}

struct swapchain
{
    struct vulkan_object obj;
    struct surface *surface;
    VkExtent2D extents;
};

static inline struct swapchain *swapchain_from_handle( VkSwapchainKHR handle )
{
    return CONTAINING_RECORD( vulkan_swapchain_from_handle( handle ), struct swapchain, obj );
}

static inline const char *debugstr_vulkan_object( const struct vulkan_object *obj )
{
    return wine_dbg_sprintf( "%p %#jx/%#jx, parent %p", obj, obj->host.handle, obj->client.handle, obj->parent );
}

static int window_surface_compare( const void *key, const struct rb_entry *entry )
{
    const struct surface *surface = RB_ENTRY_VALUE( entry, struct surface, window_entry );
    HWND key_hwnd = (HWND)key;

    if (key_hwnd < surface->hwnd) return -1;
    if (key_hwnd > surface->hwnd) return 1;
    return 0;
}

static pthread_mutex_t window_surfaces_lock = PTHREAD_MUTEX_INITIALIZER;
static struct rb_tree window_surfaces = {.compare = window_surface_compare};

static void window_surfaces_insert( struct surface *surface )
{
    struct surface *previous;
    struct rb_entry *ptr;

    pthread_mutex_lock( &window_surfaces_lock );

    if (!(ptr = rb_get( &window_surfaces, surface->hwnd )))
        rb_put( &window_surfaces, surface->hwnd, &surface->window_entry );
    else
    {
        previous = RB_ENTRY_VALUE( ptr, struct surface, window_entry );
        rb_replace( &window_surfaces, &previous->window_entry, &surface->window_entry );
        previous->hwnd = 0; /* make sure previous surface becomes invalid */
    }

    pthread_mutex_unlock( &window_surfaces_lock );
}

static void window_surfaces_remove( struct surface *surface )
{
    pthread_mutex_lock( &window_surfaces_lock );
    if (surface->hwnd) rb_remove( &window_surfaces, &surface->window_entry );
    pthread_mutex_unlock( &window_surfaces_lock );
}

static VkResult win32u_vkCreateWin32SurfaceKHR( VkInstance instance_handle, const VkWin32SurfaceCreateInfoKHR *info,
                                                const VkAllocationCallbacks *allocator, VkSurfaceKHR *handle )
{
    struct vulkan_instance *instance = vulkan_instance_from_handle( instance_handle );
    struct surface *surface;
    VkSurfaceKHR host_surface;
    HWND dummy = NULL;
    VkResult res;
    WND *win;

    TRACE( "instance_handle %p, info %p, allocator %p, handle %p\n", instance_handle, info, allocator, handle );
    if (allocator) FIXME( "Support for allocation callbacks not implemented yet\n" );

    if (!(surface = calloc( 1, sizeof(*surface) ))) return VK_ERROR_OUT_OF_HOST_MEMORY;

    /* Windows allows surfaces to be created with no HWND, they return VK_ERROR_SURFACE_LOST_KHR later */
    if (!(surface->hwnd = info->hwnd))
    {
        static const WCHAR staticW[] = {'s', 't', 'a', 't', 'i', 'c', 0};
        UNICODE_STRING static_us = RTL_CONSTANT_STRING( staticW );
        dummy = NtUserCreateWindowEx( 0, &static_us, &static_us, &static_us, WS_POPUP, 0, 0, 0, 0,
                                      NULL, NULL, NULL, NULL, 0, NULL, 0, FALSE );
        WARN( "Created dummy window %p for null surface window\n", dummy );
        surface->hwnd = dummy;
    }

    res = driver_funcs->p_vulkan_surface_create( surface->hwnd, instance->obj.host.instance,
                                                 &host_surface, &surface->driver_private );
    if (res != VK_SUCCESS)
    {
        if (dummy) NtUserDestroyWindow( dummy );
        free( surface );
        return res;
    }

    if (!(win = get_win_ptr( info->hwnd )) || win == WND_DESKTOP || win == WND_OTHER_PROCESS)
        list_init( &surface->entry );
    else
    {
        list_add_tail( &win->vulkan_surfaces, &surface->entry );
        release_win_ptr( win );
    }

    init_vulkan_object( &surface->obj, &instance->obj, host_surface, NULL );
    add_vulkan_object( instance, &surface->obj );
    TRACE( "created surface %s\n", debugstr_vulkan_object( &surface->obj ) );

    if (dummy) NtUserDestroyWindow( dummy );
    window_surfaces_insert( surface );

    *handle = surface->obj.client.surface;
    return VK_SUCCESS;
}

static void win32u_vkDestroySurfaceKHR( VkInstance instance_handle, VkSurfaceKHR surface_handle, const VkAllocationCallbacks *allocator )
{
    struct vulkan_instance *instance = vulkan_instance_from_handle( instance_handle );
    const struct vulkan_instance_funcs *funcs = &instance->funcs;
    struct surface *surface = surface_from_handle( surface_handle );
    WND *win;

    if (!surface) return;

    TRACE( "instance_handle %p, surface_handle %#jx, allocator %p\n", instance_handle, surface_handle, allocator );
    if (allocator) FIXME( "Support for allocation callbacks not implemented yet\n" );

    if ((win = get_win_ptr( surface->hwnd )) && win != WND_DESKTOP && win != WND_OTHER_PROCESS)
    {
        list_remove( &surface->entry );
        release_win_ptr( win );
    }

    funcs->p_vkDestroySurfaceKHR( instance->obj.host.instance, surface->obj.host.surface, NULL /* allocator */ );
    driver_funcs->p_vulkan_surface_destroy( surface->hwnd, surface->driver_private );

    remove_vulkan_object( instance, &surface->obj );
    window_surfaces_remove( surface );

    free( surface );
}

static BOOL extents_equals( const VkExtent2D *extents, const RECT *rect )
{
    return extents->width == rect->right - rect->left && extents->height == rect->bottom - rect->top;
}

static VkResult win32u_vkQueuePresentKHR( VkQueue queue_handle, const VkPresentInfoKHR *present_info )
{
    VkSwapchainKHR swapchains_buffer[16], *swapchains = swapchains_buffer;
    struct vulkan_object *queue = vulkan_queue_from_handle( queue_handle );
    const struct vulkan_device_funcs *funcs = get_vulkan_parent_device_funcs( queue );
    VkPresentInfoKHR present_info_host = *present_info;
    VkResult res;
    UINT i;

    TRACE( "queue_handle %p, present_info %p\n", queue_handle, present_info );

    if (present_info->swapchainCount > ARRAY_SIZE( swapchains_buffer ) &&
        !(swapchains = malloc( present_info->swapchainCount * sizeof(*swapchains) )))
    {
        free( swapchains );
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    for (i = 0; i < present_info->swapchainCount; i++)
    {
        struct swapchain *swapchain = swapchain_from_handle( present_info->pSwapchains[i] );
        swapchains[i] = swapchain->obj.host.swapchain;
    }
    present_info_host.pSwapchains = swapchains;

    res = funcs->p_vkQueuePresentKHR( queue->host.queue, &present_info_host );

    for (i = 0; i < present_info->swapchainCount; i++)
    {
        struct swapchain *swapchain = swapchain_from_handle( present_info->pSwapchains[i] );
        VkResult swapchain_res = present_info->pResults ? present_info->pResults[i] : res;
        struct surface *surface = swapchain->surface;
        RECT client_rect;

        driver_funcs->p_vulkan_surface_presented( surface->hwnd, swapchain_res );

        if (swapchain_res < VK_SUCCESS) continue;
        if (!NtUserGetClientRect( surface->hwnd, &client_rect, get_win_monitor_dpi( surface->hwnd ) ))
        {
            WARN( "Swapchain window %p is invalid, returning VK_ERROR_OUT_OF_DATE_KHR\n", surface->hwnd );
            if (present_info->pResults) present_info->pResults[i] = VK_ERROR_OUT_OF_DATE_KHR;
            if (res >= VK_SUCCESS) res = VK_ERROR_OUT_OF_DATE_KHR;
        }
        else if (swapchain_res != VK_SUCCESS)
            WARN( "Present returned status %d for swapchain %p\n", swapchain_res, swapchain );
        else if (!extents_equals( &swapchain->extents, &client_rect ))
        {
            WARN( "Swapchain size %dx%d does not match client rect %s, returning VK_SUBOPTIMAL_KHR\n",
                  swapchain->extents.width, swapchain->extents.height, wine_dbgstr_rect( &client_rect ) );
            if (present_info->pResults) present_info->pResults[i] = VK_SUBOPTIMAL_KHR;
            if (res == VK_SUCCESS) res = VK_SUBOPTIMAL_KHR;
        }
    }

    if (swapchains != swapchains_buffer) free( swapchains );

    if (TRACE_ON(fps))
    {
        static unsigned long frames, frames_total;
        static long prev_time, start_time;
        DWORD time;

        time = NtGetTickCount();
        frames++;
        frames_total++;

        if (time - prev_time > 1500)
        {
            TRACE_(fps)("%p @ approx %.2ffps, total %.2ffps\n", queue,
                        1000.0 * frames / (time - prev_time),
                        1000.0 * frames_total / (time - start_time));
            prev_time = time;
            frames = 0;

            if (!start_time) start_time = time;
        }
    }

    return res;
}

static VkResult win32u_vkAcquireNextImage2KHR( VkDevice device_handle, const VkAcquireNextImageInfoKHR *acquire_info, uint32_t *image_index )
{
    struct swapchain *swapchain = swapchain_from_handle( acquire_info->swapchain );
    struct vulkan_device *device = vulkan_device_from_handle( device_handle );
    VkAcquireNextImageInfoKHR acquire_info_host = *acquire_info;
    struct surface *surface = swapchain->surface;
    RECT client_rect;
    VkResult res;

    acquire_info_host.swapchain = swapchain->obj.host.swapchain;
    res = device->funcs.p_vkAcquireNextImage2KHR( device->obj.host.device, &acquire_info_host, image_index );

    if (res == VK_SUCCESS && NtUserGetClientRect( surface->hwnd, &client_rect, get_win_monitor_dpi( surface->hwnd ) ) &&
        !extents_equals( &swapchain->extents, &client_rect ))
    {
        WARN( "Swapchain size %dx%d does not match client rect %s, returning VK_SUBOPTIMAL_KHR\n",
              swapchain->extents.width, swapchain->extents.height, wine_dbgstr_rect( &client_rect ) );
        return VK_SUBOPTIMAL_KHR;
    }

    return res;
}

static VkResult win32u_vkAcquireNextImageKHR( VkDevice device_handle, VkSwapchainKHR swapchain_handle, uint64_t timeout,
                                              VkSemaphore semaphore, VkFence fence, uint32_t *image_index )
{
    struct swapchain *swapchain = swapchain_from_handle( swapchain_handle );
    struct vulkan_device *device = vulkan_device_from_handle( device_handle );
    struct surface *surface = swapchain->surface;
    RECT client_rect;
    VkResult res;

    res = device->funcs.p_vkAcquireNextImageKHR( device->obj.host.device, swapchain->obj.host.swapchain,
                                                 timeout, semaphore, fence, image_index );

    if (res == VK_SUCCESS && NtUserGetClientRect( surface->hwnd, &client_rect, get_win_monitor_dpi( surface->hwnd ) ) &&
        !extents_equals( &swapchain->extents, &client_rect ))
    {
        WARN( "Swapchain size %dx%d does not match client rect %s, returning VK_SUBOPTIMAL_KHR\n",
              swapchain->extents.width, swapchain->extents.height, wine_dbgstr_rect( &client_rect ) );
        return VK_SUBOPTIMAL_KHR;
    }

    return res;
}

static VkResult win32u_vkCreateSwapchainKHR( VkDevice device_handle, const VkSwapchainCreateInfoKHR *create_info,
                                             const VkAllocationCallbacks *allocator, VkSwapchainKHR *swapchain_handle )
{
    struct swapchain *object, *old_swapchain = swapchain_from_handle( create_info->oldSwapchain );
    struct surface *surface = surface_from_handle( create_info->surface );
    struct vulkan_device *device = vulkan_device_from_handle( device_handle );
    struct vulkan_object *physical_device = device->obj.parent;
    struct vulkan_instance *instance = CONTAINING_RECORD( physical_device->parent, struct vulkan_instance, obj );
    const struct vulkan_instance_funcs *funcs = get_vulkan_parent_instance_funcs( physical_device );
    VkSwapchainCreateInfoKHR create_info_host = *create_info;
    VkSurfaceCapabilitiesKHR capabilities;
    VkSwapchainKHR host_swapchain;
    VkResult res;

    if (!NtUserIsWindow( surface->hwnd ))
    {
        ERR( "surface %p, hwnd %p is invalid!\n", surface, surface->hwnd );
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (surface) create_info_host.surface = surface->obj.host.surface;
    if (old_swapchain) create_info_host.oldSwapchain = old_swapchain->obj.host.swapchain;

    /* Windows allows client rect to be empty, but host Vulkan often doesn't, adjust extents back to the host capabilities */
    res = funcs->p_vkGetPhysicalDeviceSurfaceCapabilitiesKHR( physical_device->host.physical_device,
                                                              surface->obj.host.surface, &capabilities );
    if (res != VK_SUCCESS) return res;

    create_info_host.imageExtent.width = max( create_info_host.imageExtent.width, capabilities.minImageExtent.width );
    create_info_host.imageExtent.height = max( create_info_host.imageExtent.height, capabilities.minImageExtent.height );

    if (!(object = calloc( 1, sizeof(*object) ))) return VK_ERROR_OUT_OF_HOST_MEMORY;
    res = device->funcs.p_vkCreateSwapchainKHR( device->obj.host.device, &create_info_host, NULL, &host_swapchain );
    if (res != VK_SUCCESS)
    {
        free( object );
        return res;
    }

    init_vulkan_object( &object->obj, &device->obj, host_swapchain, NULL );
    object->surface = surface;
    object->extents = create_info->imageExtent;

    add_vulkan_object( instance, &object->obj );
    TRACE( "created swapchain %s\n", debugstr_vulkan_object( &object->obj ) );

    *swapchain_handle = object->obj.client.swapchain;
    return VK_SUCCESS;
}

static void win32u_vkDestroySwapchainKHR( VkDevice device_handle, VkSwapchainKHR swapchain_handle,
                                          const VkAllocationCallbacks *allocator )
{
    struct vulkan_device *device = vulkan_device_from_handle( device_handle );
    struct swapchain *swapchain = swapchain_from_handle( swapchain_handle );

    if (allocator) FIXME( "Support for allocation callbacks not implemented yet\n" );
    if (!swapchain) return;

    device->funcs.p_vkDestroySwapchainKHR( device->obj.host.device, swapchain->obj.host.swapchain, NULL );
    remove_vulkan_device_object( device_handle, &swapchain->obj );

    free( swapchain );
}

static void adjust_surface_capabilities( struct surface *surface, VkSurfaceCapabilitiesKHR *capabilities )
{
    RECT client_rect;

    /* Many Windows games, for example Strange Brigade, No Man's Sky, Path of Exile
     * and World War Z, do not expect that maxImageCount can be set to 0.
     * A value of 0 means that there is no limit on the number of images.
     * Nvidia reports 8 on Windows, AMD 16.
     * https://vulkan.gpuinfo.org/displayreport.php?id=9122#surface
     * https://vulkan.gpuinfo.org/displayreport.php?id=9121#surface
     */
    if (!capabilities->maxImageCount) capabilities->maxImageCount = max( capabilities->minImageCount, 16 );

    /* Update the image extents to match what the Win32 WSI would provide. */
    /* FIXME: handle DPI scaling, somehow */
    NtUserGetClientRect( surface->hwnd, &client_rect, get_win_monitor_dpi( surface->hwnd ) );
    capabilities->minImageExtent.width = client_rect.right - client_rect.left;
    capabilities->minImageExtent.height = client_rect.bottom - client_rect.top;
    capabilities->maxImageExtent.width = client_rect.right - client_rect.left;
    capabilities->maxImageExtent.height = client_rect.bottom - client_rect.top;
    capabilities->currentExtent.width = client_rect.right - client_rect.left;
    capabilities->currentExtent.height = client_rect.bottom - client_rect.top;
}

static VkResult win32u_vkGetPhysicalDeviceSurfaceCapabilitiesKHR( VkPhysicalDevice device_handle, VkSurfaceKHR surface_handle,
                                                                  VkSurfaceCapabilitiesKHR *capabilities )
{
    struct vulkan_object *physical_device = vulkan_physical_device_from_handle( device_handle );
    const struct vulkan_instance_funcs *funcs = get_vulkan_parent_instance_funcs( physical_device );
    struct surface *surface = surface_from_handle( surface_handle );
    VkResult res;

    if (!NtUserIsWindow( surface->hwnd )) return VK_ERROR_SURFACE_LOST_KHR;
    res = funcs->p_vkGetPhysicalDeviceSurfaceCapabilitiesKHR( physical_device->host.physical_device,
                                                              surface->obj.host.surface, capabilities );
    if (res == VK_SUCCESS) adjust_surface_capabilities( surface, capabilities );
    return res;
}

static VkResult win32u_vkGetPhysicalDeviceSurfaceCapabilities2KHR( VkPhysicalDevice device_handle,
                                                                   const VkPhysicalDeviceSurfaceInfo2KHR *surface_info,
                                                                   VkSurfaceCapabilities2KHR *capabilities )
{
    struct vulkan_object *physical_device = vulkan_physical_device_from_handle( device_handle );
    const struct vulkan_instance_funcs *funcs = get_vulkan_parent_instance_funcs( physical_device );
    struct surface *surface = surface_from_handle( surface_info->surface );
    VkPhysicalDeviceSurfaceInfo2KHR surface_info_host = *surface_info;
    VkResult res;

    if (!funcs->p_vkGetPhysicalDeviceSurfaceCapabilities2KHR)
    {
        /* Until the loader version exporting this function is common, emulate it using the older non-2 version. */
        if (surface_info->pNext || capabilities->pNext) FIXME( "Emulating vkGetPhysicalDeviceSurfaceCapabilities2KHR, ignoring pNext.\n" );
        return win32u_vkGetPhysicalDeviceSurfaceCapabilitiesKHR( device_handle, surface_info->surface,
                                                                 &capabilities->surfaceCapabilities );
    }

    surface_info_host.surface = surface->obj.host.surface;

    if (!NtUserIsWindow( surface->hwnd )) return VK_ERROR_SURFACE_LOST_KHR;
    res = funcs->p_vkGetPhysicalDeviceSurfaceCapabilities2KHR( physical_device->host.physical_device,
                                                               &surface_info_host, capabilities );
    if (res == VK_SUCCESS) adjust_surface_capabilities( surface, &capabilities->surfaceCapabilities );
    return res;
}

static VkResult win32u_vkGetPhysicalDevicePresentRectanglesKHR( VkPhysicalDevice device_handle, VkSurfaceKHR surface_handle,
                                                                uint32_t *rect_count, VkRect2D *rects )
{
    struct vulkan_object *physical_device = vulkan_physical_device_from_handle( device_handle );
    const struct vulkan_instance_funcs *funcs = get_vulkan_parent_instance_funcs( physical_device );
    struct surface *surface = surface_from_handle( surface_handle );

    if (!NtUserIsWindow( surface->hwnd ))
    {
        if (rects && !*rect_count) return VK_INCOMPLETE;
        if (rects) memset( rects, 0, sizeof(VkRect2D) );
        *rect_count = 1;
        return VK_SUCCESS;
    }

    return funcs->p_vkGetPhysicalDevicePresentRectanglesKHR( physical_device->host.physical_device,
                                                             surface->obj.host.surface, rect_count, rects );
}

static VkResult win32u_vkGetPhysicalDeviceSurfaceFormatsKHR( VkPhysicalDevice device_handle, VkSurfaceKHR surface_handle,
                                                             uint32_t *format_count, VkSurfaceFormatKHR *formats )
{
    struct vulkan_object *physical_device = vulkan_physical_device_from_handle( device_handle );
    const struct vulkan_instance_funcs *funcs = get_vulkan_parent_instance_funcs( physical_device );
    struct surface *surface = surface_from_handle( surface_handle );

    return funcs->p_vkGetPhysicalDeviceSurfaceFormatsKHR( physical_device->host.physical_device,
                                                          surface->obj.host.surface, format_count, formats );
}

static VkResult win32u_vkGetPhysicalDeviceSurfaceFormats2KHR( VkPhysicalDevice device_handle,
                                                              const VkPhysicalDeviceSurfaceInfo2KHR *surface_info,
                                                              uint32_t *format_count, VkSurfaceFormat2KHR *formats )
{
    struct vulkan_object *physical_device = vulkan_physical_device_from_handle( device_handle );
    const struct vulkan_instance_funcs *funcs = get_vulkan_parent_instance_funcs( physical_device );
    struct surface *surface = surface_from_handle( surface_info->surface );
    VkPhysicalDeviceSurfaceInfo2KHR surface_info_host = *surface_info;
    VkResult res;

    if (!funcs->p_vkGetPhysicalDeviceSurfaceFormats2KHR)
    {
        VkSurfaceFormatKHR *surface_formats;
        UINT i;

        /* Until the loader version exporting this function is common, emulate it using the older non-2 version. */
        if (surface_info->pNext) FIXME( "Emulating vkGetPhysicalDeviceSurfaceFormats2KHR, ignoring pNext.\n" );

        if (!formats) return win32u_vkGetPhysicalDeviceSurfaceFormatsKHR( device_handle, surface_info->surface, format_count, NULL );

        surface_formats = calloc( *format_count, sizeof(*surface_formats) );
        if (!surface_formats) return VK_ERROR_OUT_OF_HOST_MEMORY;

        res = win32u_vkGetPhysicalDeviceSurfaceFormatsKHR( device_handle, surface_info->surface, format_count, surface_formats );
        if (res == VK_SUCCESS || res == VK_INCOMPLETE)
        {
            for (i = 0; i < *format_count; i++) formats[i].surfaceFormat = surface_formats[i];
        }

        free( surface_formats );
        return res;
    }

    surface_info_host.surface = surface->obj.host.surface;
    return funcs->p_vkGetPhysicalDeviceSurfaceFormats2KHR( physical_device->host.physical_device,
                                                           &surface_info_host, format_count, formats );
}

static VkBool32 win32u_vkGetPhysicalDeviceWin32PresentationSupportKHR( VkPhysicalDevice device_handle, uint32_t queue )
{
    struct vulkan_object *physical_device = vulkan_physical_device_from_handle( device_handle );
    return driver_funcs->p_vkGetPhysicalDeviceWin32PresentationSupportKHR( physical_device->host.physical_device, queue );
}

static PFN_vkVoidFunction win32u_vkGetDeviceProcAddr( VkDevice device, const char *name )
{
    TRACE( "device %p, name %s\n", device, debugstr_a(name) );

    if (!strcmp( name, "vkGetDeviceProcAddr" )) return (PFN_vkVoidFunction)vulkan_funcs.p_vkGetDeviceProcAddr;

    return p_vkGetDeviceProcAddr( device, name );
}

static PFN_vkVoidFunction win32u_vkGetInstanceProcAddr( VkInstance instance, const char *name )
{
    TRACE( "instance %p, name %s\n", instance, debugstr_a(name) );

    if (!instance) return p_vkGetInstanceProcAddr( instance, name );

    if (!strcmp( name, "vkGetInstanceProcAddr" )) return (PFN_vkVoidFunction)vulkan_funcs.p_vkGetInstanceProcAddr;
    /* vkGetInstanceProcAddr also loads any children of instance, so device functions as well. */
    if (!strcmp( name, "vkGetDeviceProcAddr" )) return (PFN_vkVoidFunction)vulkan_funcs.p_vkGetDeviceProcAddr;

    return p_vkGetInstanceProcAddr( instance, name );
}

static struct vulkan_funcs vulkan_funcs =
{
    .p_vkAcquireNextImage2KHR = win32u_vkAcquireNextImage2KHR,
    .p_vkAcquireNextImageKHR = win32u_vkAcquireNextImageKHR,
    .p_vkCreateSemaphore = win32u_vkCreateSemaphore,
    .p_vkCreateSwapchainKHR = win32u_vkCreateSwapchainKHR,
    .p_vkCreateWin32SurfaceKHR = win32u_vkCreateWin32SurfaceKHR,
    .p_vkDestroySemaphore = win32u_vkDestroySemaphore,
    .p_vkDestroySurfaceKHR = win32u_vkDestroySurfaceKHR,
    .p_vkDestroySwapchainKHR = win32u_vkDestroySwapchainKHR,
    .p_vkGetDeviceProcAddr = win32u_vkGetDeviceProcAddr,
    .p_vkGetInstanceProcAddr = win32u_vkGetInstanceProcAddr,
    .p_vkGetPhysicalDeviceExternalSemaphoreProperties = win32u_vkGetPhysicalDeviceExternalSemaphoreProperties,
    .p_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR = win32u_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR,
    .p_vkGetPhysicalDevicePresentRectanglesKHR = win32u_vkGetPhysicalDevicePresentRectanglesKHR,
    .p_vkGetPhysicalDeviceSurfaceCapabilities2KHR = win32u_vkGetPhysicalDeviceSurfaceCapabilities2KHR,
    .p_vkGetPhysicalDeviceSurfaceCapabilitiesKHR = win32u_vkGetPhysicalDeviceSurfaceCapabilitiesKHR,
    .p_vkGetPhysicalDeviceSurfaceFormats2KHR = win32u_vkGetPhysicalDeviceSurfaceFormats2KHR,
    .p_vkGetPhysicalDeviceWin32PresentationSupportKHR = win32u_vkGetPhysicalDeviceWin32PresentationSupportKHR,
    .p_vkGetSemaphoreWin32HandleKHR = win32u_vkGetSemaphoreWin32HandleKHR,
    .p_vkImportSemaphoreWin32HandleKHR = win32u_vkImportSemaphoreWin32HandleKHR,
    .p_vkQueuePresentKHR = win32u_vkQueuePresentKHR,
};

static VkResult nulldrv_vulkan_surface_create( HWND hwnd, VkInstance instance, VkSurfaceKHR *surface, void **private )
{
    FIXME( "stub!\n" );
    return VK_ERROR_INCOMPATIBLE_DRIVER;
}

static void nulldrv_vulkan_surface_destroy( HWND hwnd, void *private )
{
}

static void nulldrv_vulkan_surface_detach( HWND hwnd, void *private )
{
}

static void nulldrv_vulkan_surface_presented( HWND hwnd, void *private, VkResult result )
{
}

static VkBool32 nulldrv_vkGetPhysicalDeviceWin32PresentationSupportKHR( VkPhysicalDevice device, uint32_t queue )
{
    return VK_TRUE;
}

static const char *nulldrv_get_host_surface_extension(void)
{
    return "VK_WINE_nulldrv_surface";
}

static const struct vulkan_driver_funcs nulldrv_funcs =
{
    .p_vulkan_surface_create = nulldrv_vulkan_surface_create,
    .p_vulkan_surface_destroy = nulldrv_vulkan_surface_destroy,
    .p_vulkan_surface_detach = nulldrv_vulkan_surface_detach,
    .p_vulkan_surface_presented = nulldrv_vulkan_surface_presented,
    .p_vkGetPhysicalDeviceWin32PresentationSupportKHR = nulldrv_vkGetPhysicalDeviceWin32PresentationSupportKHR,
    .p_get_host_surface_extension = nulldrv_get_host_surface_extension,
};

static void vulkan_driver_init(void)
{
    UINT status;

    if ((status = user_driver->pVulkanInit( WINE_VULKAN_DRIVER_VERSION, vulkan_handle, &driver_funcs )) &&
        status != STATUS_NOT_IMPLEMENTED)
    {
        ERR( "Failed to initialize the driver vulkan functions, status %#x\n", status );
        return;
    }

    if (status == STATUS_NOT_IMPLEMENTED)
        driver_funcs = &nulldrv_funcs;
    else
        vulkan_funcs.p_get_host_surface_extension = driver_funcs->p_get_host_surface_extension;
}

static void vulkan_driver_load(void)
{
    static pthread_once_t init_once = PTHREAD_ONCE_INIT;
    pthread_once( &init_once, vulkan_driver_init );
}

static VkResult lazydrv_vulkan_surface_create( HWND hwnd, VkInstance instance, VkSurfaceKHR *surface, void **private )
{
    vulkan_driver_load();
    return driver_funcs->p_vulkan_surface_create( hwnd, instance, surface, private );
}

static void lazydrv_vulkan_surface_destroy( HWND hwnd, void *private )
{
    vulkan_driver_load();
    return driver_funcs->p_vulkan_surface_destroy( hwnd, private );
}

static void lazydrv_vulkan_surface_detach( HWND hwnd, void *private )
{
    vulkan_driver_load();
    return driver_funcs->p_vulkan_surface_detach( hwnd, private );
}

static void lazydrv_vulkan_surface_presented( HWND hwnd, void *private, VkResult result )
{
    vulkan_driver_load();
    driver_funcs->p_vulkan_surface_presented( hwnd, private, result );
}

static VkBool32 lazydrv_vkGetPhysicalDeviceWin32PresentationSupportKHR( VkPhysicalDevice device, uint32_t queue )
{
    vulkan_driver_load();
    return driver_funcs->p_vkGetPhysicalDeviceWin32PresentationSupportKHR( device, queue );
}

static const char *lazydrv_get_host_surface_extension(void)
{
    vulkan_driver_load();
    return driver_funcs->p_get_host_surface_extension();
}

static const struct vulkan_driver_funcs lazydrv_funcs =
{
    .p_vulkan_surface_create = lazydrv_vulkan_surface_create,
    .p_vulkan_surface_destroy = lazydrv_vulkan_surface_destroy,
    .p_vulkan_surface_detach = lazydrv_vulkan_surface_detach,
    .p_vulkan_surface_presented = lazydrv_vulkan_surface_presented,
    .p_vkGetPhysicalDeviceWin32PresentationSupportKHR = lazydrv_vkGetPhysicalDeviceWin32PresentationSupportKHR,
};

static void vulkan_init_once(void)
{
    if (!(vulkan_handle = dlopen( SONAME_LIBVULKAN, RTLD_NOW )))
    {
        ERR( "Failed to load %s\n", SONAME_LIBVULKAN );
        return;
    }

#define LOAD_FUNCPTR( f )                                                                          \
    if (!(p_##f = dlsym( vulkan_handle, #f )))                                                     \
    {                                                                                              \
        ERR( "Failed to find " #f "\n" );                                                          \
        dlclose( vulkan_handle );                                                                  \
        vulkan_handle = NULL;                                                                      \
        return;                                                                                    \
    }

    LOAD_FUNCPTR( vkGetDeviceProcAddr );
    LOAD_FUNCPTR( vkGetInstanceProcAddr );
#undef LOAD_FUNCPTR

    driver_funcs = &lazydrv_funcs;
    vulkan_funcs.p_get_host_surface_extension = lazydrv_get_host_surface_extension;
}

void vulkan_detach_surfaces( struct list *surfaces )
{
    struct surface *surface, *next;

    LIST_FOR_EACH_ENTRY_SAFE( surface, next, surfaces, struct surface, entry )
    {
        driver_funcs->p_vulkan_surface_detach( surface->hwnd, surface->driver_private );
        list_remove( &surface->entry );
        list_init( &surface->entry );
    }
}

#else /* SONAME_LIBVULKAN */

void vulkan_detach_surfaces( struct list *surfaces )
{
}

static void vulkan_init_once(void)
{
    ERR( "Wine was built without Vulkan support.\n" );
}

#endif /* SONAME_LIBVULKAN */

BOOL vulkan_init(void)
{
    static pthread_once_t init_once = PTHREAD_ONCE_INIT;
    pthread_once( &init_once, vulkan_init_once );
    return !!vulkan_handle;
}

/***********************************************************************
 *      __wine_get_vulkan_driver  (win32u.so)
 */
const struct vulkan_funcs *__wine_get_vulkan_driver( UINT version )
{
    if (version != WINE_VULKAN_DRIVER_VERSION)
    {
        ERR( "version mismatch, vulkan wants %u but win32u has %u\n", version, WINE_VULKAN_DRIVER_VERSION );
        return NULL;
    }

    if (!vulkan_init()) return NULL;
    return &vulkan_funcs;
}
