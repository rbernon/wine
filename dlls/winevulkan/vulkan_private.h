/* Wine Vulkan ICD private data structures
 *
 * Copyright 2017 Roderick Colenbrander
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

#ifndef __WINE_VULKAN_PRIVATE_H
#define __WINE_VULKAN_PRIVATE_H

#define WINE_VK_HOST
#define VK_NO_PROTOTYPES

#include <pthread.h>

#include "vulkan_loader.h"
#include "wine/rbtree.h"
#include "wine/vulkan_driver.h"

extern const struct vulkan_funcs *vk_funcs;

static inline const char *debugstr_vulkan_object( const struct vulkan_object *obj )
{
    return wine_dbg_sprintf( "%p %#jx/%#jx, parent %p", obj, obj->host.handle, obj->client.handle, obj->parent );
}

struct wine_cmd_buffer
{
    struct vulkan_object obj;
};

static inline struct wine_cmd_buffer *wine_cmd_buffer_from_handle(VkCommandBuffer handle)
{
    return (struct wine_cmd_buffer *)(uintptr_t)handle->base.unix_handle;
}

struct wine_queue
{
    struct vulkan_object obj;
    uint32_t family_index;
    uint32_t queue_index;
    VkDeviceQueueCreateFlags flags;
};

static inline struct wine_queue *wine_queue_from_handle(VkQueue handle)
{
    return (struct wine_queue *)(uintptr_t)handle->base.unix_handle;
}

struct wine_device
{
    struct vulkan_device device;
    uint32_t queue_count;
    struct wine_queue queues[];
};

C_ASSERT(sizeof(struct wine_device) == offsetof(struct wine_device, queues[0]));

static inline struct wine_device *wine_device_from_handle(VkDevice handle)
{
    return CONTAINING_RECORD(vulkan_device_from_handle(handle), struct wine_device, device.obj);
}

struct wine_debug_utils_messenger;

struct wine_debug_report_callback
{
    struct vulkan_object obj;
    UINT64 user_callback; /* client pointer */
    UINT64 user_data; /* client pointer */
};

struct wine_phys_dev
{
    struct vulkan_object obj;
    VkPhysicalDeviceMemoryProperties memory_properties;
    VkExtensionProperties *extensions;
    uint32_t extension_count;

    uint32_t external_memory_align;
    uint32_t map_placed_align;
};

static inline struct wine_phys_dev *wine_phys_dev_from_handle(VkPhysicalDevice handle)
{
    return CONTAINING_RECORD(vulkan_physical_device_from_handle(handle), struct wine_phys_dev, obj);
}

struct wine_debug_report_callback;

struct wine_instance
{
    struct vulkan_instance instance;

    VkBool32 enable_win32_surface;
    VkBool32 enable_wrapper_list;

    struct wine_debug_utils_messenger *utils_messengers;
    uint32_t utils_messenger_count;

    struct wine_debug_report_callback default_callback;

    unsigned int quirks;

    /* We cache devices as we need to wrap them as they are dispatchable objects. */
    uint32_t phys_dev_count;
    struct wine_phys_dev phys_devs[];
};

C_ASSERT(sizeof(struct wine_instance) == offsetof(struct wine_instance, phys_devs[0]));

struct wine_cmd_pool
{
    struct vulkan_object obj;
};

static inline struct wine_cmd_pool *wine_cmd_pool_from_handle(VkCommandPool handle)
{
    struct vk_command_pool *client_ptr = command_pool_from_handle(handle);
    return (struct wine_cmd_pool *)(uintptr_t)client_ptr->base.unix_handle;
}

struct wine_device_memory
{
    struct vulkan_object obj;
    VkDeviceSize size;
    void *vm_map;
};

static inline struct wine_device_memory *wine_device_memory_from_handle(VkDeviceMemory handle)
{
    return (struct wine_device_memory *)(uintptr_t)handle;
}

struct wine_debug_utils_messenger
{
    struct vulkan_object obj;
    UINT64 user_callback; /* client pointer */
    UINT64 user_data; /* client pointer */
};

static inline struct wine_debug_utils_messenger *wine_debug_utils_messenger_from_handle(
        VkDebugUtilsMessengerEXT handle)
{
    return (struct wine_debug_utils_messenger *)(uintptr_t)handle;
}

static inline struct wine_debug_report_callback *wine_debug_report_callback_from_handle(
        VkDebugReportCallbackEXT handle)
{
    return (struct wine_debug_report_callback *)(uintptr_t)handle;
}

BOOL wine_vk_device_extension_supported(const char *name);
BOOL wine_vk_instance_extension_supported(const char *name);
BOOL wine_vk_is_host_surface_extension(const char *name);

BOOL wine_vk_is_type_wrapped(VkObjectType type);

NTSTATUS init_vulkan(void *args);

NTSTATUS vk_is_available_instance_function(void *arg);
NTSTATUS vk_is_available_device_function(void *arg);
NTSTATUS vk_is_available_instance_function32(void *arg);
NTSTATUS vk_is_available_device_function32(void *arg);

struct conversion_context
{
    char buffer[2048];
    uint32_t used;
    struct list alloc_entries;
};

static inline void init_conversion_context(struct conversion_context *pool)
{
    pool->used = 0;
    list_init(&pool->alloc_entries);
}

static inline void free_conversion_context(struct conversion_context *pool)
{
    struct list *entry, *next;
    LIST_FOR_EACH_SAFE(entry, next, &pool->alloc_entries)
        free(entry);
}

static inline void *conversion_context_alloc(struct conversion_context *pool, size_t size)
{
    if (pool->used + size <= sizeof(pool->buffer))
    {
        void *ret = pool->buffer + pool->used;
        pool->used += (size + sizeof(UINT64) - 1) & ~(sizeof(UINT64) - 1);
        return ret;
    }
    else
    {
        struct list *entry;
        if (!(entry = malloc(sizeof(*entry) + size)))
            return NULL;
        list_add_tail(&pool->alloc_entries, entry);
        return entry + 1;
    }
}

struct wine_deferred_operation
{
    struct vulkan_object obj;
    struct conversion_context ctx; /* to keep params alive. */
};

static inline struct wine_deferred_operation *wine_deferred_operation_from_handle(
        VkDeferredOperationKHR handle)
{
    return (struct wine_deferred_operation *)(uintptr_t)handle;
}

typedef UINT32 PTR32;

typedef struct
{
    VkStructureType sType;
    PTR32 pNext;
} VkBaseInStructure32;

typedef struct
{
    VkStructureType sType;
    PTR32 pNext;
} VkBaseOutStructure32;

static inline void *find_next_struct32(void *s, VkStructureType t)
{
    VkBaseOutStructure32 *header;

    for (header = s; header; header = UlongToPtr(header->pNext))
    {
        if (header->sType == t)
            return header;
    }

    return NULL;
}

static inline void *find_next_struct(const void *s, VkStructureType t)
{
    VkBaseOutStructure *header;

    for (header = (VkBaseOutStructure *)s; header; header = header->pNext)
    {
        if (header->sType == t)
            return header;
    }

    return NULL;
}

#endif /* __WINE_VULKAN_PRIVATE_H */
