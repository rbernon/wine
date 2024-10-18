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

#ifndef __WINE_VULKAN_DRIVER_H
#define __WINE_VULKAN_DRIVER_H

#include <stdarg.h>
#include <stddef.h>

#include <windef.h>
#include <winbase.h>

/* Base 'class' for our Vulkan dispatchable objects such as VkDevice and VkInstance.
 * This structure MUST be the first element of a dispatchable object as the ICD
 * loader depends on it. For now only contains loader_magic, but over time more common
 * functionality is expected.
 */
struct vulkan_client_object
{
    /* Special section in each dispatchable object for use by the ICD loader for
     * storing dispatch tables. The start contains a magical value '0x01CDC0DE'.
     */
    UINT64 loader_magic;
    UINT64 unix_handle;
};

#ifdef WINE_UNIX_LIB

#include <pthread.h>

#define WINE_VK_HOST
#include "wine/vulkan.h"
#include "wine/vulkan_thunks.h"
#include "wine/rbtree.h"
#include "wine/debug.h"

/* Wine internal vulkan driver version, needs to be bumped upon vulkan_funcs changes. */
#define WINE_VULKAN_DRIVER_VERSION 35

struct vulkan_object
{
    union
    {
        UINT64 handle;
        VkCommandBuffer command_buffer;
        VkCommandPool command_pool;
        VkDebugReportCallbackEXT debug_report_callback;
        VkDebugUtilsMessengerEXT debug_utils_messenger;
        VkDeferredOperationKHR deferred_operation;
        VkDevice device;
        VkDeviceMemory device_memory;
        VkInstance instance;
        VkPhysicalDevice physical_device;
        VkQueue queue;
        VkSurfaceKHR surface;
        VkSwapchainKHR swapchain;
    } host;
    union
    {
        UINT64 handle;
        struct vulkan_client_object *object;
        VkCommandBuffer command_buffer;
        VkCommandPool command_pool;
        VkDebugReportCallbackEXT debug_report_callback;
        VkDebugUtilsMessengerEXT debug_utils_messenger;
        VkDeferredOperationKHR deferred_operation;
        VkDevice device;
        VkDeviceMemory device_memory;
        VkInstance instance;
        VkPhysicalDevice physical_device;
        VkQueue queue;
        VkSurfaceKHR surface;
        VkSwapchainKHR swapchain;
    } client;
    struct vulkan_object *parent;
    struct rb_entry entry;
};

static inline void init_vulkan_object( struct vulkan_object *obj, struct vulkan_object *parent,
                                       UINT64 host_handle, struct vulkan_client_object *client )
{
    obj->host.handle = (UINT_PTR)host_handle;
    obj->client.handle = client ? (UINT_PTR)client : (UINT_PTR)obj;
    obj->parent = parent;
    if (client) client->unix_handle = (UINT_PTR)obj;
}

static inline void init_vulkan_object_ptr( struct vulkan_object *obj, struct vulkan_object *parent,
                                           void *host_handle, struct vulkan_client_object *client )
{
    init_vulkan_object( obj, parent, (UINT_PTR)host_handle, client );
}

struct vulkan_instance
{
    struct vulkan_object obj;
    struct vulkan_instance_funcs funcs;
    pthread_rwlock_t objects_lock;
    struct rb_tree objects;
};

static inline struct vulkan_instance *vulkan_instance_from_handle( VkInstance handle )
{
    struct vulkan_client_object *client = (struct vulkan_client_object *)handle;
    return (struct vulkan_instance *)(UINT_PTR)client->unix_handle;
}

static inline const struct vulkan_instance_funcs *get_vulkan_instance_funcs( VkInstance handle )
{
    return &vulkan_instance_from_handle(handle)->funcs;
}
static inline const struct vulkan_instance_funcs *get_vulkan_parent_instance_funcs( struct vulkan_object *obj )
{
    struct vulkan_instance *instance = CONTAINING_RECORD( obj->parent, struct vulkan_instance, obj );
    return &instance->funcs;
}

static inline void add_vulkan_object( struct vulkan_instance *instance, struct vulkan_object *obj )
{
    if (instance->objects.compare)
    {
        pthread_rwlock_wrlock( &instance->objects_lock );
        rb_put( &instance->objects, &obj->host.handle, &obj->entry );
        pthread_rwlock_unlock( &instance->objects_lock );
    }
}

static inline void remove_vulkan_object( struct vulkan_instance *instance, struct vulkan_object *obj )
{
    if (instance->objects.compare)
    {
        pthread_rwlock_wrlock( &instance->objects_lock );
        rb_remove( &instance->objects, &obj->entry );
        pthread_rwlock_unlock( &instance->objects_lock );
    }
}

static inline struct vulkan_object *vulkan_physical_device_from_handle( VkPhysicalDevice handle )
{
    struct vulkan_client_object *client = (struct vulkan_client_object *)handle;
    return (struct vulkan_object *)(UINT_PTR)client->unix_handle;
}

struct vulkan_device
{
    struct vulkan_object obj;
    struct vulkan_device_funcs funcs;
};

static inline struct vulkan_device *vulkan_device_from_handle( VkDevice handle )
{
    struct vulkan_client_object *client = (struct vulkan_client_object *)handle;
    return (struct vulkan_device *)(UINT_PTR)client->unix_handle;
}
static inline struct vulkan_object *vulkan_queue_from_handle( VkQueue handle )
{
    struct vulkan_client_object *client = (struct vulkan_client_object *)handle;
    return (struct vulkan_object *)(UINT_PTR)client->unix_handle;
}

static inline const struct vulkan_device_funcs *get_vulkan_device_funcs( VkDevice handle )
{
    return &vulkan_device_from_handle(handle)->funcs;
}
static inline const struct vulkan_device_funcs *get_vulkan_parent_device_funcs( struct vulkan_object *obj )
{
    struct vulkan_device *device = CONTAINING_RECORD( obj->parent, struct vulkan_device, obj );
    return &device->funcs;
}

static inline void add_vulkan_device_object( VkDevice handle, struct vulkan_object *obj )
{
    struct vulkan_device *device = vulkan_device_from_handle( handle );
    struct vulkan_object *physical_device = device->obj.parent;
    struct vulkan_instance *instance = CONTAINING_RECORD( physical_device->parent, struct vulkan_instance, obj );
    add_vulkan_object( instance, obj );
}

static inline void remove_vulkan_device_object( VkDevice handle, struct vulkan_object *obj )
{
    struct vulkan_device *device = vulkan_device_from_handle( handle );
    struct vulkan_object *physical_device = device->obj.parent;
    struct vulkan_instance *instance = CONTAINING_RECORD( physical_device->parent, struct vulkan_instance, obj );
    remove_vulkan_object( instance, obj );
}

static inline struct vulkan_object *vulkan_surface_from_handle( VkSurfaceKHR handle )
{
    return (struct vulkan_object *)(uintptr_t)handle;
}

static inline struct vulkan_object *vulkan_swapchain_from_handle( VkSwapchainKHR handle )
{
    return (struct vulkan_object *)(uintptr_t)handle;
}

struct vulkan_funcs
{
    /* Vulkan global functions. These are the only calls at this point a graphics driver
     * needs to provide. Other function calls will be provided indirectly by dispatch
     * tables part of dispatchable Vulkan objects such as VkInstance or vkDevice.
     */
    PFN_vkAcquireNextImage2KHR p_vkAcquireNextImage2KHR;
    PFN_vkAcquireNextImageKHR p_vkAcquireNextImageKHR;
    PFN_vkCreateSwapchainKHR p_vkCreateSwapchainKHR;
    PFN_vkCreateWin32SurfaceKHR p_vkCreateWin32SurfaceKHR;
    PFN_vkDestroySurfaceKHR p_vkDestroySurfaceKHR;
    PFN_vkDestroySwapchainKHR p_vkDestroySwapchainKHR;
    PFN_vkGetDeviceProcAddr p_vkGetDeviceProcAddr;
    PFN_vkGetInstanceProcAddr p_vkGetInstanceProcAddr;
    PFN_vkGetPhysicalDevicePresentRectanglesKHR p_vkGetPhysicalDevicePresentRectanglesKHR;
    PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR p_vkGetPhysicalDeviceSurfaceCapabilities2KHR;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR p_vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
    PFN_vkGetPhysicalDeviceSurfaceFormats2KHR p_vkGetPhysicalDeviceSurfaceFormats2KHR;
    PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR p_vkGetPhysicalDeviceWin32PresentationSupportKHR;
    PFN_vkQueuePresentKHR p_vkQueuePresentKHR;

    /* winevulkan specific functions */
    const char *(*p_get_host_surface_extension)(void);
};

/* interface between win32u and the user drivers */
struct vulkan_driver_funcs
{
    VkResult (*p_vulkan_surface_create)(HWND, VkInstance, VkSurfaceKHR *, void **);
    void (*p_vulkan_surface_destroy)(HWND, void *);
    void (*p_vulkan_surface_detach)(HWND, void *);
    void (*p_vulkan_surface_update)(HWND, void *);
    void (*p_vulkan_surface_presented)(HWND, void *, VkResult);

    VkBool32 (*p_vkGetPhysicalDeviceWin32PresentationSupportKHR)(VkPhysicalDevice, uint32_t);
    const char *(*p_get_host_surface_extension)(void);
};

#endif /* WINE_UNIX_LIB */

#endif /* __WINE_VULKAN_DRIVER_H */
