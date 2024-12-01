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

#include <pthread.h>

#include "vulkan_loader.h"
#include "vulkan_thunks.h"

#include "wine/rbtree.h"

extern const struct vulkan_funcs *vk_funcs;

struct wine_cmd_buffer
{
    VULKAN_OBJECT_HEADER( VkCommandBuffer, command_buffer );
    struct vulkan_device *device;
};

static inline struct wine_cmd_buffer *wine_cmd_buffer_from_handle(VkCommandBuffer handle)
{
    struct vulkan_client_object *client = (struct vulkan_client_object *)handle;
    return (struct wine_cmd_buffer *)(UINT_PTR)client->unix_handle;
}

struct wine_queue
{
    struct vulkan_queue obj;

    uint32_t family_index;
    uint32_t queue_index;
    VkDeviceQueueCreateFlags flags;
};

struct wine_device
{
    struct vulkan_device obj;
    uint64_t queue_count;
    struct wine_queue queues[];
};

C_ASSERT(sizeof(struct wine_device) == offsetof(struct wine_device, queues[0]));

struct wine_debug_utils_messenger;

struct wine_debug_report_callback
{
    VULKAN_OBJECT_HEADER( VkDebugReportCallbackEXT, debug_callback );
    struct vulkan_instance *instance;

    UINT64 user_callback; /* client pointer */
    UINT64 user_data; /* client pointer */
};

struct wine_debug_report_callback;

struct wine_instance
{
    struct vulkan_instance obj;

    VkBool32 enable_win32_surface;

    struct wine_debug_utils_messenger *utils_messengers;
    uint32_t utils_messenger_count;

    struct wine_debug_report_callback default_callback;

    unsigned int quirks;

    struct rb_tree objects;
    pthread_rwlock_t objects_lock;
};

struct wine_cmd_pool
{
    VULKAN_OBJECT_HEADER( VkCommandPool, command_pool );
};

static inline struct wine_cmd_pool *wine_cmd_pool_from_handle(VkCommandPool handle)
{
    struct vulkan_client_object *client = &command_pool_from_handle(handle)->obj;
    return (struct wine_cmd_pool *)(UINT_PTR)client->unix_handle;
}

struct wine_device_memory
{
    VULKAN_OBJECT_HEADER( VkDeviceMemory, device_memory );
    VkDeviceSize size;
    void *vm_map;
};

static inline struct wine_device_memory *wine_device_memory_from_handle(VkDeviceMemory handle)
{
    return (struct wine_device_memory *)(uintptr_t)handle;
}

struct wine_debug_utils_messenger
{
    VULKAN_OBJECT_HEADER( VkDebugUtilsMessengerEXT, debug_messenger );
    struct vulkan_instance *instance;

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
NTSTATUS init_openxr(void *args);

NTSTATUS vk_is_available_instance_function(void *arg);
NTSTATUS vk_is_available_device_function(void *arg);
NTSTATUS xr_is_available_instance_function(void *arg);
NTSTATUS vk_is_available_instance_function32(void *arg);
NTSTATUS vk_is_available_device_function32(void *arg);
NTSTATUS xr_is_available_instance_function32(void *arg);

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
    VULKAN_OBJECT_HEADER( VkDeferredOperationKHR, deferred_operation );
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

static inline void *find_next_vk_struct32(void *s, VkStructureType t)
{
    VkBaseOutStructure32 *header;

    for (header = s; header; header = UlongToPtr(header->pNext))
    {
        if (header->sType == t)
            return header;
    }

    return NULL;
}

static inline void *find_next_vk_struct(const void *s, VkStructureType t)
{
    VkBaseOutStructure *header;

    for (header = (VkBaseOutStructure *)s; header; header = header->pNext)
    {
        if (header->sType == t)
            return header;
    }

    return NULL;
}

struct openxr_instance
{
    VULKAN_OBJECT_HEADER( XrInstance, instance );
#define USE_XR_FUNC( x ) PFN_##x p_##x;
    ALL_XR_INSTANCE_FUNCS
#undef USE_XR_FUNC
};

static inline struct openxr_instance *openxr_instance_from_handle( XrInstance handle )
{
    struct openxr_client_object *client = (struct openxr_client_object *)handle;
    return (struct openxr_instance *)(UINT_PTR)client->unix_handle;
}

struct openxr_session
{
    VULKAN_OBJECT_HEADER( XrSession, session );
    struct openxr_instance *instance;
};

static inline struct openxr_session *openxr_session_from_handle( XrSession handle )
{
    struct openxr_client_object *client = (struct openxr_client_object *)handle;
    return (struct openxr_session *)(UINT_PTR)client->unix_handle;
}

struct openxr_swapchain
{
    VULKAN_OBJECT_HEADER( XrSwapchain, swapchain );
    struct openxr_instance *instance;
};

static inline struct openxr_swapchain *openxr_swapchain_from_handle( XrSwapchain handle )
{
    struct openxr_client_object *client = (struct openxr_client_object *)handle;
    return (struct openxr_swapchain *)(UINT_PTR)client->unix_handle;
}

struct openxr_space
{
    VULKAN_OBJECT_HEADER( XrSpace, space );
    struct openxr_instance *instance;
};

static inline struct openxr_space *openxr_space_from_handle( XrSpace handle )
{
    struct openxr_client_object *client = (struct openxr_client_object *)handle;
    return (struct openxr_space *)(UINT_PTR)client->unix_handle;
}

struct openxr_action_set
{
    VULKAN_OBJECT_HEADER( XrActionSet, action_set );
    struct openxr_instance *instance;
};

static inline struct openxr_action_set *openxr_action_set_from_handle( XrActionSet handle )
{
    struct openxr_client_object *client = (struct openxr_client_object *)handle;
    return (struct openxr_action_set *)(UINT_PTR)client->unix_handle;
}

struct openxr_action
{
    VULKAN_OBJECT_HEADER( XrAction, action );
    struct openxr_instance *instance;
};

static inline struct openxr_action *openxr_action_from_handle( XrAction handle )
{
    struct openxr_client_object *client = (struct openxr_client_object *)handle;
    return (struct openxr_action *)(UINT_PTR)client->unix_handle;
}

typedef struct
{
    XrStructureType sType;
    PTR32 pNext;
} XrBaseInStructure32;

typedef struct
{
    XrStructureType sType;
    PTR32 pNext;
} XrBaseOutStructure32;

static inline void *find_next_xr_struct32( void *s, XrStructureType t )
{
    XrBaseOutStructure32 *header;

    for (header = s; header; header = UlongToPtr( header->pNext ))
    {
        if (header->sType == t) return header;
    }

    return NULL;
}

static inline void *find_next_xr_struct( const void *s, XrStructureType t )
{
    XrBaseOutStructure *header;

    for (header = (XrBaseOutStructure *)s; header; header = header->pNext)
    {
        if (header->sType == t) return header;
    }

    return NULL;
}

#endif /* __WINE_VULKAN_PRIVATE_H */
