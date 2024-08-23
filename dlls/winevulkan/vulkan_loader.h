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

#ifndef __WINE_VULKAN_LOADER_H
#define __WINE_VULKAN_LOADER_H

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "ntuser.h"
#include "wine/asm.h"
#include "wine/debug.h"
#include "wine/vulkan.h"
#include "wine/vulkan_driver.h"
#include "wine/unixlib.h"
#include "wine/list.h"

#include "loader_thunks.h"

/* Magic value defined by Vulkan ICD / Loader spec */
#define VULKAN_ICD_MAGIC_VALUE 0x01CDC0DE

#define WINEVULKAN_QUIRK_GET_DEVICE_PROC_ADDR 0x00000001

struct VkPhysicalDevice_T
{
    struct vulkan_client_object obj;
};

struct VkInstance_T
{
    struct vulkan_client_object obj;
    uint32_t phys_dev_count;
    struct VkPhysicalDevice_T phys_devs[1];
};

struct VkQueue_T
{
    struct vulkan_client_object obj;
};

struct VkDevice_T
{
    struct vulkan_client_object obj;
    unsigned int quirks;
    struct VkQueue_T queues[1];
};

struct vk_command_pool
{
    struct vulkan_client_object obj;
    struct list command_buffers;
};

static inline struct vk_command_pool *command_pool_from_handle(VkCommandPool handle)
{
    return (struct vk_command_pool *)(uintptr_t)handle;
}

struct VkCommandBuffer_T
{
    struct vulkan_client_object obj;
    struct list pool_link;
};

struct vulkan_func
{
    const char *name;
    void *func;
};

void *wine_vk_get_device_proc_addr(const char *name);
void *wine_vk_get_phys_dev_proc_addr(const char *name);
void *wine_vk_get_instance_proc_addr(const char *name);

struct vk_callback_funcs
{
    UINT64 call_vulkan_debug_report_callback;
    UINT64 call_vulkan_debug_utils_callback;
};

/* debug callbacks params */

struct debug_utils_label
{
    UINT32 label_name_len;
    float color[4];
};

struct debug_utils_object
{
    UINT32 object_type;
    UINT64 object_handle;
    UINT32 object_name_len;
};

struct debug_device_address_binding
{
    UINT32 flags;
    UINT64 base_address;
    UINT64 size;
    UINT32 binding_type;
};

struct wine_vk_debug_utils_params
{
    struct dispatch_callback_params dispatch;
    UINT64 user_callback; /* client pointer */
    UINT64 user_data; /* client pointer */

    UINT32 severity;
    UINT32 message_types;
    UINT32 flags;
    UINT32 message_id_number;

    UINT32 message_id_name_len;
    UINT32 message_len;
    UINT32 queue_label_count;
    UINT32 cmd_buf_label_count;
    UINT32 object_count;

    UINT8 has_address_binding;
    struct debug_device_address_binding address_binding;
};

struct wine_vk_debug_report_params
{
    struct dispatch_callback_params dispatch;
    UINT64 user_callback; /* client pointer */
    UINT64 user_data; /* client pointer */

    UINT32 flags;
    UINT32 object_type;
    UINT64 object_handle;
    UINT64 location;
    UINT32 code;
    UINT32 layer_len;
    UINT32 message_len;
};

struct is_available_instance_function_params
{
    VkInstance instance;
    const char *name;
};

struct is_available_device_function_params
{
    VkDevice device;
    const char *name;
};

#define UNIX_CALL(code, params) WINE_UNIX_CALL(unix_ ## code, params)

#ifdef __x86_64__

#define VK_FAST_THUNK_MAX_ARGS 7

#define __VK_FAST_THUNK_ARGS_0
#define __VK_FAST_THUNK_ARGS_1 "movq %rcx,%r8\n\t"         __VK_FAST_THUNK_ARGS_0
#define __VK_FAST_THUNK_ARGS_2 "movq %rdx,%xmm0\n\t"       __VK_FAST_THUNK_ARGS_1
#define __VK_FAST_THUNK_ARGS_3 "movq %r8,%xmm1\n\t"        __VK_FAST_THUNK_ARGS_2
#define __VK_FAST_THUNK_ARGS_4 "movq %r9,%xmm2\n\t"        __VK_FAST_THUNK_ARGS_3
#define __VK_FAST_THUNK_ARGS_5 "movq 0x28(%rsp),%xmm3\n\t" __VK_FAST_THUNK_ARGS_4
#define __VK_FAST_THUNK_ARGS_6 "movq 0x30(%rsp),%xmm4\n\t" __VK_FAST_THUNK_ARGS_5
#define __VK_FAST_THUNK_ARGS_7 "movq 0x38(%rsp),%xmm5\n\t" __VK_FAST_THUNK_ARGS_6

#define VK_FAST_THUNK(name, code, args) \
    __ASM_GLOBAL_FUNC(name, __VK_FAST_THUNK_ARGS_ ## args \
                      "movq __wine_unixlib_handle(%rip),%rcx\n\t" \
                      "movq $" code ",%rdx\n\t" \
                      "jmp *__wine_unix_call_dispatcher(%rip)")

#define __VK_FAST_GET_ARG_0 "movq %%rdi,%0"
#define __VK_FAST_GET_ARG_1 "movq %%xmm0,%0"
#define __VK_FAST_GET_ARG_2 "movq %%xmm1,%0"
#define __VK_FAST_GET_ARG_3 "movq %%xmm2,%0"
#define __VK_FAST_GET_ARG_4 "movq %%xmm3,%0"
#define __VK_FAST_GET_ARG_5 "movq %%xmm4,%0"
#define __VK_FAST_GET_ARG_6 "movq %%xmm5,%0"

#define VK_FAST_THUNK_GET_ARG(n, type, param) do { \
        void *__reg; \
        __asm__ __volatile__(__VK_FAST_GET_ARG_ ## n : "=r"(__reg)); \
        (param) = (type)__reg; \
    } while(0)

#endif /* __x86_64__ */

#endif /* __WINE_VULKAN_LOADER_H */
