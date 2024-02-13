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
#include "wine/asm.h"
#include "wine/debug.h"
#include "wine/vulkan.h"
#include "wine/unixlib.h"
#include "wine/list.h"

#include "loader_thunks.h"

/* Magic value defined by Vulkan ICD / Loader spec */
#define VULKAN_ICD_MAGIC_VALUE 0x01CDC0DE

#define WINEVULKAN_QUIRK_GET_DEVICE_PROC_ADDR 0x00000001

/* Base 'class' for our Vulkan dispatchable objects such as VkDevice and VkInstance.
 * This structure MUST be the first element of a dispatchable object as the ICD
 * loader depends on it. For now only contains loader_magic, but over time more common
 * functionality is expected.
 */
struct wine_vk_base
{
    /* Special section in each dispatchable object for use by the ICD loader for
     * storing dispatch tables. The start contains a magical value '0x01CDC0DE'.
     */
    UINT64 loader_magic;
    UINT64 unix_handle;
};

struct VkPhysicalDevice_T
{
    struct wine_vk_base base;
};

struct VkInstance_T
{
    struct wine_vk_base base;
    uint32_t phys_dev_count;
    struct VkPhysicalDevice_T phys_devs[1];
};

struct VkQueue_T
{
    struct wine_vk_base base;
};

struct VkDevice_T
{
    struct wine_vk_base base;
    unsigned int quirks;
    struct VkQueue_T queues[1];
};

struct vk_command_pool
{
    UINT64 unix_handle;
    struct list command_buffers;
};

static inline struct vk_command_pool *command_pool_from_handle(VkCommandPool handle)
{
    return (struct vk_command_pool *)(uintptr_t)handle;
}

struct VkCommandBuffer_T
{
    struct wine_vk_base base;
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

/* debug callbacks params */

struct wine_vk_debug_utils_params
{
    PFN_vkDebugUtilsMessengerCallbackEXT user_callback;
    void *user_data;

    VkDebugUtilsMessageSeverityFlagBitsEXT severity;
    VkDebugUtilsMessageTypeFlagsEXT message_types;
    VkDebugUtilsMessengerCallbackDataEXT data;
};

struct wine_vk_debug_report_params
{
    PFN_vkDebugReportCallbackEXT user_callback;
    void *user_data;

    VkDebugReportFlagsEXT flags;
    VkDebugReportObjectTypeEXT object_type;
    uint64_t object_handle;
    size_t location;
    int32_t code;
    const char *layer_prefix;
    const char *message;
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
#define __VK_FAST_THUNK_ARGS_1 __VK_FAST_THUNK_ARGS_0
#define __VK_FAST_THUNK_ARGS_2 __VK_FAST_THUNK_ARGS_1
#define __VK_FAST_THUNK_ARGS_3 __VK_FAST_THUNK_ARGS_2
#define __VK_FAST_THUNK_ARGS_4 __VK_FAST_THUNK_ARGS_3
#define __VK_FAST_THUNK_ARGS_5 "movq 0x28(%rsp),%r10\n\t"  __VK_FAST_THUNK_ARGS_4
#define __VK_FAST_THUNK_ARGS_6 "movq 0x30(%rsp),%r11\n\t"  __VK_FAST_THUNK_ARGS_5
#define __VK_FAST_THUNK_ARGS_7 "movq 0x38(%rsp),%xmm0\n\t" __VK_FAST_THUNK_ARGS_6

#define VK_FAST_THUNK( name, code, args ) \
    __ASM_GLOBAL_FUNC( name, __VK_FAST_THUNK_ARGS_ ## args \
                       "movq $" code ",%rax\n\t" \
                       "jmp vk_fast_thunk_dispatcher" )

#define __VK_FAST_GET_ARG_0 "movq %%rcx,%0"
#define __VK_FAST_GET_ARG_1 "movq %%rdx,%0"
#define __VK_FAST_GET_ARG_2 "movq %%r8,%0"
#define __VK_FAST_GET_ARG_3 "movq %%r9,%0"
#define __VK_FAST_GET_ARG_4 "movq %%r10,%0"
#define __VK_FAST_GET_ARG_5 "movq %%r11,%0"
#define __VK_FAST_GET_ARG_6 "movq %%xmm0,%0"

#define VK_FAST_THUNK_GET_ARG(n, type, param) do { \
        void *__reg; \
        __asm__ __volatile__(__VK_FAST_GET_ARG_ ## n : "=r"(__reg)); \
        (param) = (type)__reg; \
    } while(0)

#ifdef VK_DEFINE_DISPATCHER
__ASM_GLOBAL_FUNC( vk_fast_thunk_dispatcher,
                   "pushq %rsi\n\t"
                   "pushq %rdi\n\t"
                   "subq $0xa8,%rsp\n\t"
                   "movdqa %xmm6,0x00(%rsp)\n\t"
                   "movdqa %xmm7,0x10(%rsp)\n\t"
                   "movdqa %xmm8,0x20(%rsp)\n\t"
                   "movdqa %xmm9,0x30(%rsp)\n\t"
                   "movdqa %xmm10,0x40(%rsp)\n\t"
                   "movdqa %xmm11,0x50(%rsp)\n\t"
                   "movdqa %xmm12,0x60(%rsp)\n\t"
                   "movdqa %xmm13,0x70(%rsp)\n\t"
                   "movdqa %xmm14,0x80(%rsp)\n\t"
                   "movdqa %xmm15,0x90(%rsp)\n\t"
                   "movq %rsp,%rsi\n\t"
                   "movq %gs:0x328,%rsp\n\t"
                   "movq %rsi,0x88(%rsp)\n\t"
                   "movq __wine_unixlib_handle(%rip),%rsi\n\t"
                   "callq *(%rsi,%rax,8)\n\t"
                   "movq 0x88(%rsp),%rsp\n\t"
                   "movdqa 0x00(%rsp),%xmm6\n\t"
                   "movdqa 0x10(%rsp),%xmm7\n\t"
                   "movdqa 0x20(%rsp),%xmm8\n\t"
                   "movdqa 0x30(%rsp),%xmm9\n\t"
                   "movdqa 0x40(%rsp),%xmm10\n\t"
                   "movdqa 0x50(%rsp),%xmm11\n\t"
                   "movdqa 0x60(%rsp),%xmm12\n\t"
                   "movdqa 0x70(%rsp),%xmm13\n\t"
                   "movdqa 0x80(%rsp),%xmm14\n\t"
                   "movdqa 0x90(%rsp),%xmm15\n\t"
                   "addq $0xa8,%rsp\n\t"
                   "popq %rdi\n\t"
                   "popq %rsi\n\t"
                   "ret" )
#endif

#endif /* __x86_64__ */

#endif /* __WINE_VULKAN_LOADER_H */
