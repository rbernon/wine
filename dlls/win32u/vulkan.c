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

WINE_DEFAULT_DEBUG_CHANNEL(vulkan);

PFN_vkGetDeviceProcAddr p_vkGetDeviceProcAddr = NULL;
PFN_vkGetInstanceProcAddr p_vkGetInstanceProcAddr = NULL;

static void *vulkan_handle;
static struct vulkan_funcs vulkan_funcs;

#ifdef SONAME_LIBVULKAN

WINE_DECLARE_DEBUG_CHANNEL(fps);

static const struct vulkan_driver_funcs *driver_funcs;

static const UINT EXTERNAL_MEMORY_WIN32_BITS = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT |
                                               VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT |
                                               VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT |
                                               VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT |
                                               VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_HEAP_BIT |
                                               VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;

struct device_memory
{
    struct vulkan_device_memory obj;
    VkDeviceSize size;
    void *vm_map;
};

static inline struct device_memory *device_memory_from_handle( VkDeviceMemory handle )
{
    struct vulkan_device_memory *obj = vulkan_device_memory_from_handle( handle );
    return CONTAINING_RECORD( obj, struct device_memory, obj );
}

struct surface
{
    struct vulkan_surface obj;
    void *driver_private;
    HWND hwnd;

    struct list entry;
    struct rb_entry window_entry;
};

static struct surface *surface_from_handle( VkSurfaceKHR handle )
{
    struct vulkan_surface *obj = vulkan_surface_from_handle( handle );
    return CONTAINING_RECORD( obj, struct surface, obj );
}

struct swapchain
{
    struct vulkan_swapchain obj;
    struct surface *surface;
    VkExtent2D extents;
};

static struct swapchain *swapchain_from_handle( VkSwapchainKHR handle )
{
    struct vulkan_swapchain *obj = vulkan_swapchain_from_handle( handle );
    return CONTAINING_RECORD( obj, struct swapchain, obj );
}

struct semaphore
{
    struct vulkan_semaphore obj;
};

static inline struct semaphore *semaphore_from_handle( VkSemaphore handle )
{
    struct vulkan_semaphore *obj = vulkan_semaphore_from_handle( handle );
    return CONTAINING_RECORD( obj, struct semaphore, obj );
}

static inline const void *find_next_struct( const void *head, VkStructureType type )
{
    const VkBaseInStructure *header;
    for (header = head; header; header = header->pNext) if (header->sType == type) return header;
    return NULL;
}

static VkResult allocate_external_host_memory( struct vulkan_device *device, VkMemoryAllocateInfo *alloc_info,
                                               VkImportMemoryHostPointerInfoEXT *import_info )
{
    struct vulkan_physical_device *physical_device = device->physical_device;
    VkMemoryHostPointerPropertiesEXT props =
    {
        .sType = VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT,
    };
    uint32_t i, mem_flags, align = physical_device->external_memory_align - 1;
    SIZE_T alloc_size = alloc_info->allocationSize;
    static int once;
    void *mapping;
    VkResult res;

    if (!once++) FIXME( "Using VK_EXT_external_memory_host\n" );

    mem_flags = physical_device->memory_properties.memoryTypes[alloc_info->memoryTypeIndex].propertyFlags;

    if (NtAllocateVirtualMemory( GetCurrentProcess(), &mapping, zero_bits, &alloc_size, MEM_COMMIT, PAGE_READWRITE ))
    {
        ERR( "NtAllocateVirtualMemory failed\n" );
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    if ((res = device->p_vkGetMemoryHostPointerPropertiesEXT( device->host.device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT,
                                                              mapping, &props )))
    {
        ERR( "vkGetMemoryHostPointerPropertiesEXT failed: %d\n", res );
        return res;
    }

    if (!(props.memoryTypeBits & (1u << alloc_info->memoryTypeIndex)))
    {
        /* If requested memory type is not allowed to use external memory, try to find a supported compatible type. */
        uint32_t mask = mem_flags & ~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        for (i = 0; i < physical_device->memory_properties.memoryTypeCount; i++)
        {
            if (!(props.memoryTypeBits & (1u << i))) continue;
            if ((physical_device->memory_properties.memoryTypes[i].propertyFlags & mask) != mask) continue;

            TRACE( "Memory type not compatible with host memory, using %u instead\n", i );
            alloc_info->memoryTypeIndex = i;
            break;
        }
        if (i == physical_device->memory_properties.memoryTypeCount)
        {
            FIXME( "Not found compatible memory type\n" );
            alloc_size = 0;
            NtFreeVirtualMemory( GetCurrentProcess(), &mapping, &alloc_size, MEM_RELEASE );
        }
    }

    if (props.memoryTypeBits & (1u << alloc_info->memoryTypeIndex))
    {
        import_info->sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT;
        import_info->handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
        import_info->pHostPointer = mapping;
        import_info->pNext = alloc_info->pNext;
        alloc_info->pNext = import_info;
        alloc_info->allocationSize = (alloc_info->allocationSize + align) & ~align;
    }

    return VK_SUCCESS;
}

static VkResult win32u_vkAllocateMemory( VkDevice client_device, const VkMemoryAllocateInfo *alloc_info,
                                         const VkAllocationCallbacks *allocator, VkDeviceMemory *ret )
{
    struct vulkan_device *device = vulkan_device_from_handle( client_device );
    struct vulkan_physical_device *physical_device = device->physical_device;
    struct vulkan_instance *instance = device->physical_device->instance;
    VkImportMemoryHostPointerInfoEXT host_pointer_info;
    VkDeviceMemory host_device_memory;
    struct device_memory *memory;
    VkBaseOutStructure **next;
    uint32_t mem_flags;
    void *mapping = NULL;
    VkResult res;

    for (next = (VkBaseOutStructure **)&alloc_info->pNext; *next; next = &(*next)->pNext)
    {
        switch ((*next)->sType)
        {
        case VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_MEMORY_ALLOCATE_INFO_NV: break;
        case VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO:
        {
            VkExportMemoryAllocateInfo *export_info = (VkExportMemoryAllocateInfo *)*next;
            if (export_info->handleTypes & EXTERNAL_MEMORY_WIN32_BITS)
            {
                FIXME( "VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO not implemented!\n" );
                *next = (*next)->pNext;
            }
            break;
        }
        case VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR:
            FIXME( "VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR not implemented!\n" );
            *next = (*next)->pNext;
            break;
        case VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR:
            FIXME( "VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR not implemented!\n" );
            *next = (*next)->pNext;
            break;
        case VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR:
            ERR( "VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR not supported!\n" );
            *next = (*next)->pNext;
            break;
        case VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO: break;
        case VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO: break;
        case VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT:
            host_pointer_info = *(VkImportMemoryHostPointerInfoEXT *)*next;
            break;
        case VK_STRUCTURE_TYPE_MEMORY_PRIORITY_ALLOCATE_INFO_EXT: break;
        case VK_STRUCTURE_TYPE_MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO: break;
        default: FIXME( "Unhandled sType %u.\n", (*next)->sType ); break;
        }
    }

    /* For host visible memory, we try to use VK_EXT_external_memory_host on wow64 to ensure that mapped pointer is 32-bit. */
    mem_flags = physical_device->memory_properties.memoryTypes[alloc_info->memoryTypeIndex].propertyFlags;
    if (physical_device->external_memory_align && (mem_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) && !host_pointer_info.sType &&
        (res = allocate_external_host_memory( device, (VkMemoryAllocateInfo *)alloc_info, &host_pointer_info )))
        return res;

    if (!(memory = malloc( sizeof(*memory) ))) return VK_ERROR_OUT_OF_HOST_MEMORY;

    if ((res = device->host_vkAllocateMemory( device->host.device, alloc_info, NULL, &host_device_memory )))
    {
        free( memory );
        return res;
    }

    vulkan_object_init( &memory->obj.obj, host_device_memory, NULL );
    memory->size = alloc_info->allocationSize;
    memory->vm_map = mapping;
    instance->p_insert_object( instance, &memory->obj.obj );

    *ret = memory->obj.client.device_memory;
    return VK_SUCCESS;
}

static void win32u_vkFreeMemory( VkDevice client_device, VkDeviceMemory client_device_memory, const VkAllocationCallbacks *allocator )
{
    struct vulkan_device *device = vulkan_device_from_handle( client_device );
    struct vulkan_physical_device *physical_device = device->physical_device;
    struct vulkan_instance *instance = device->physical_device->instance;
    struct device_memory *memory;

    if (!client_device_memory) return;
    memory = device_memory_from_handle( client_device_memory );

    if (memory->vm_map && !physical_device->external_memory_align)
    {
        const VkMemoryUnmapInfoKHR info =
        {
            .sType = VK_STRUCTURE_TYPE_MEMORY_UNMAP_INFO_KHR,
            .memory = memory->obj.host.device_memory,
            .flags = VK_MEMORY_UNMAP_RESERVE_BIT_EXT,
        };
        device->host_vkUnmapMemory2KHR( device->host.device, &info );
    }

    device->host_vkFreeMemory( device->host.device, memory->obj.host.device_memory, NULL );
    instance->p_remove_object( instance, &memory->obj.obj );

    if (memory->vm_map)
    {
        SIZE_T alloc_size = 0;
        NtFreeVirtualMemory( GetCurrentProcess(), &memory->vm_map, &alloc_size, MEM_RELEASE );
    }

    free( memory );
}

static VkResult win32u_vkGetMemoryWin32HandleKHR( VkDevice client_device, const VkMemoryGetWin32HandleInfoKHR *handle_info, HANDLE *handle )
{
    struct vulkan_device *device = vulkan_device_from_handle( client_device );

    FIXME( "device %p, handle_info %p, handle %p stub!\n", device, handle_info, handle );

    return VK_ERROR_INCOMPATIBLE_DRIVER;
}

static VkResult win32u_vkGetMemoryWin32HandlePropertiesKHR( VkDevice client_device, VkExternalMemoryHandleTypeFlagBits handle_type, HANDLE handle,
                                                            VkMemoryWin32HandlePropertiesKHR *handle_properties )
{
    struct vulkan_device *device = vulkan_device_from_handle( client_device );

    FIXME( "device %p, handle_type %#x, handle %p, handle_properties %p stub!\n", device, handle_type, handle, handle_properties );

    return VK_ERROR_INCOMPATIBLE_DRIVER;
}

static VkResult win32u_vkMapMemory2KHR( VkDevice client_device, const VkMemoryMapInfoKHR *map_info, void **data )
{
    struct vulkan_device *device = vulkan_device_from_handle( client_device );
    struct vulkan_physical_device *physical_device = device->physical_device;
    struct device_memory *memory = device_memory_from_handle( map_info->memory );
    VkMemoryMapInfoKHR info = *map_info;
    VkMemoryMapPlacedInfoEXT placed_info =
    {
        .sType = VK_STRUCTURE_TYPE_MEMORY_MAP_PLACED_INFO_EXT,
    };
    VkResult res;

    info.memory = memory->obj.host.device_memory;
    if (memory->vm_map)
    {
        *data = (char *)memory->vm_map + info.offset;
        TRACE( "returning %p\n", *data );
        return VK_SUCCESS;
    }

    if (physical_device->map_placed_align)
    {
        SIZE_T alloc_size = memory->size;

        placed_info.pNext = info.pNext;
        info.pNext = &placed_info;
        info.offset = 0;
        info.size = VK_WHOLE_SIZE;
        info.flags |= VK_MEMORY_MAP_PLACED_BIT_EXT;

        if (NtAllocateVirtualMemory( GetCurrentProcess(), &placed_info.pPlacedAddress, zero_bits,
                                     &alloc_size, MEM_COMMIT, PAGE_READWRITE ))
        {
            ERR( "NtAllocateVirtualMemory failed\n" );
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    if (device->host_vkMapMemory2KHR)
    {
        res = device->host_vkMapMemory2KHR( device->host.device, &info, data );
    }
    else
    {
        if (info.pNext) FIXME( "struct extension chain not implemented!\n" );
        res = device->host_vkMapMemory( device->host.device, info.memory, info.offset, info.size, info.flags, data );
    }

    if (placed_info.pPlacedAddress)
    {
        if (res != VK_SUCCESS)
        {
            SIZE_T alloc_size = 0;
            ERR( "vkMapMemory2EXT failed: %d\n", res );
            NtFreeVirtualMemory( GetCurrentProcess(), &placed_info.pPlacedAddress, &alloc_size, MEM_RELEASE );
            return res;
        }
        memory->vm_map = placed_info.pPlacedAddress;
        *data = (char *)memory->vm_map + map_info->offset;
        TRACE( "Using placed mapping %p\n", memory->vm_map );
    }

#ifdef _WIN64
    if (NtCurrentTeb()->WowTebOffset && res == VK_SUCCESS && (UINT_PTR)*data >> 32)
    {
        FIXME( "returned mapping %p does not fit 32-bit pointer\n", *data );
        device->host_vkUnmapMemory( device->host.device, memory->obj.host.device_memory );
        *data = NULL;
        res = VK_ERROR_OUT_OF_HOST_MEMORY;
    }
#endif

    return res;
}

static VkResult win32u_vkMapMemory( VkDevice client_device, VkDeviceMemory client_device_memory, VkDeviceSize offset,
                                    VkDeviceSize size, VkMemoryMapFlags flags, void **data )
{
    const VkMemoryMapInfoKHR info =
    {
        .sType = VK_STRUCTURE_TYPE_MEMORY_MAP_INFO_KHR,
        .flags = flags,
        .memory = client_device_memory,
        .offset = offset,
        .size = size,
    };

    return win32u_vkMapMemory2KHR( client_device, &info, data );
}

static VkResult win32u_vkUnmapMemory2KHR( VkDevice client_device, const VkMemoryUnmapInfoKHR *unmap_info )
{
    struct vulkan_device *device = vulkan_device_from_handle( client_device );
    struct vulkan_physical_device *physical_device = device->physical_device;
    struct device_memory *memory = device_memory_from_handle( unmap_info->memory );
    VkMemoryUnmapInfoKHR info;
    VkResult res;

    if (memory->vm_map && physical_device->external_memory_align) return VK_SUCCESS;

    if (!device->host_vkUnmapMemory2KHR)
    {
        if (unmap_info->pNext || memory->vm_map) FIXME( "Not implemented\n" );
        device->host_vkUnmapMemory( device->host.device, memory->obj.host.device_memory );
        return VK_SUCCESS;
    }

    info = *unmap_info;
    info.memory = memory->obj.host.device_memory;
    if (memory->vm_map) info.flags |= VK_MEMORY_UNMAP_RESERVE_BIT_EXT;

    if (!(res = device->host_vkUnmapMemory2KHR( device->host.device, &info )) && memory->vm_map)
    {
        SIZE_T size = 0;
        NtFreeVirtualMemory( GetCurrentProcess(), &memory->vm_map, &size, MEM_RELEASE );
        memory->vm_map = NULL;
    }
    return res;
}

static void win32u_vkUnmapMemory( VkDevice client_device, VkDeviceMemory client_device_memory )
{
    const VkMemoryUnmapInfoKHR info =
    {
        .sType = VK_STRUCTURE_TYPE_MEMORY_UNMAP_INFO_KHR,
        .memory = client_device_memory,
    };

    win32u_vkUnmapMemory2KHR( client_device, &info );
}

static VkResult win32u_vkCreateBuffer( VkDevice client_device, const VkBufferCreateInfo *create_info,
                                       const VkAllocationCallbacks *allocator, VkBuffer *buffer )
{
    struct vulkan_device *device = vulkan_device_from_handle( client_device );
    struct vulkan_physical_device *physical_device = device->physical_device;
    VkExternalMemoryBufferCreateInfo host_external_info, *external_info = NULL;
    VkBaseOutStructure **next;

    for (next = (VkBaseOutStructure **)&create_info->pNext; *next; next = &(*next)->pNext)
    {
        switch ((*next)->sType)
        {
        case VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO_KHR: break;
        case VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_BUFFER_CREATE_INFO_NV: break;
        case VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO:
            external_info = (VkExternalMemoryBufferCreateInfo *)*next;
            if (external_info->handleTypes & EXTERNAL_MEMORY_WIN32_BITS)
            {
                FIXME( "VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO not implemented!\n" );
                *next = (*next)->pNext;
            }
            break;
        case VK_STRUCTURE_TYPE_BUFFER_OPAQUE_CAPTURE_ADDRESS_CREATE_INFO: break;
        case VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_CREATE_INFO_EXT: break;
        case VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR: break;
        case VK_STRUCTURE_TYPE_OPAQUE_CAPTURE_DESCRIPTOR_DATA_CREATE_INFO_EXT: break;
        default: FIXME( "Unhandled sType %u.\n", (*next)->sType ); break;
        }
    }

    if (physical_device->external_memory_align && !external_info)
    {
        host_external_info.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
        host_external_info.pNext = create_info->pNext;
        host_external_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
        ((VkBufferCreateInfo *)create_info)->pNext = &host_external_info; /* cast away const, it has been copied in the thunks */
    }

    return device->host_vkCreateBuffer( device->host.device, create_info, NULL, buffer );
}

static void win32u_vkGetDeviceBufferMemoryRequirements( VkDevice client_device, const VkDeviceBufferMemoryRequirements *buffer_requirements,
                                                        VkMemoryRequirements2 *memory_requirements )
{
    struct vulkan_device *device = vulkan_device_from_handle( client_device );
    VkExternalMemoryBufferCreateInfo *external_info;
    VkBaseOutStructure **next;

    TRACE( "device %p, buffer_requirements %p, memory_requirements %p\n", device, buffer_requirements, memory_requirements );

    for (next = (VkBaseOutStructure **)&buffer_requirements->pCreateInfo->pNext; *next; next = &(*next)->pNext)
    {
        switch ((*next)->sType)
        {
        case VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO_KHR: break;
        case VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_BUFFER_CREATE_INFO_NV: break;
        case VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO:
            external_info = (VkExternalMemoryBufferCreateInfo *)*next;
            if (external_info->handleTypes & EXTERNAL_MEMORY_WIN32_BITS)
            {
                FIXME( "VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO not implemented!\n" );
                *next = (*next)->pNext;
            }
            break;
        case VK_STRUCTURE_TYPE_BUFFER_OPAQUE_CAPTURE_ADDRESS_CREATE_INFO: break;
        case VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_CREATE_INFO_EXT: break;
        case VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR: break;
        case VK_STRUCTURE_TYPE_OPAQUE_CAPTURE_DESCRIPTOR_DATA_CREATE_INFO_EXT: break;
        default: FIXME( "Unhandled sType %u.\n", (*next)->sType ); break;
        }
    }

    device->host_vkGetDeviceBufferMemoryRequirements( device->host.device, buffer_requirements, memory_requirements );
}

static void win32u_vkGetPhysicalDeviceExternalBufferProperties( VkPhysicalDevice client_physical_device, const VkPhysicalDeviceExternalBufferInfo *buffer_info,
                                                                VkExternalBufferProperties *buffer_properies )
{
    struct vulkan_physical_device *physical_device = vulkan_physical_device_from_handle( client_physical_device );
    struct vulkan_instance *instance = physical_device->instance;

    TRACE( "physical_device %p, buffer_info %p, buffer_properies %p\n", physical_device, buffer_info, buffer_properies );

    if (buffer_info->handleType & EXTERNAL_MEMORY_WIN32_BITS)
    {
        FIXME( "VkPhysicalDeviceExternalBufferInfo Win32 handleType not implemented!\n" );
        ((VkPhysicalDeviceExternalBufferInfo *)buffer_info)->handleType = 0; /* cast away const, it has been copied in the thunks */
    }

    return instance->host_vkGetPhysicalDeviceExternalBufferProperties( physical_device->host.physical_device, buffer_info, buffer_properies );
}

static void win32u_vkGetPhysicalDeviceExternalBufferPropertiesKHR( VkPhysicalDevice client_physical_device, const VkPhysicalDeviceExternalBufferInfo *buffer_info,
                                                                   VkExternalBufferProperties *buffer_properies )
{
    win32u_vkGetPhysicalDeviceExternalBufferProperties( client_physical_device, buffer_info, buffer_properies );
}

static VkResult win32u_vkCreateImage( VkDevice client_device, const VkImageCreateInfo *create_info,
                                      const VkAllocationCallbacks *allocator, VkImage *image )
{
    struct vulkan_device *device = vulkan_device_from_handle( client_device );
    struct vulkan_physical_device *physical_device = device->physical_device;
    VkExternalMemoryImageCreateInfo host_external_info, *external_info = NULL;
    VkBaseOutStructure **next;

    for (next = (VkBaseOutStructure **)&create_info->pNext; *next; next = &(*next)->pNext)
    {
        switch ((*next)->sType)
        {
        case VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_IMAGE_CREATE_INFO_NV: break;
        case VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO:
            external_info = (VkExternalMemoryImageCreateInfo *)*next;
            if (external_info->handleTypes & EXTERNAL_MEMORY_WIN32_BITS)
            {
                FIXME( "VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO not implemented!\n" );
                *next = (*next)->pNext;
            }
            break;
        case VK_STRUCTURE_TYPE_IMAGE_SWAPCHAIN_CREATE_INFO_KHR: break;
        case VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO: break;
        case VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT: break;
        case VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT: break;
        case VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO: break;
        case VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR: break;
        case VK_STRUCTURE_TYPE_OPAQUE_CAPTURE_DESCRIPTOR_DATA_CREATE_INFO_EXT: break;
        case VK_STRUCTURE_TYPE_IMAGE_COMPRESSION_CONTROL_EXT: break;
        case VK_STRUCTURE_TYPE_OPTICAL_FLOW_IMAGE_FORMAT_INFO_NV: break;
        case VK_STRUCTURE_TYPE_IMAGE_ALIGNMENT_CONTROL_CREATE_INFO_MESA: break;
        default: FIXME( "Unhandled sType %u.\n", (*next)->sType ); break;
        }
    }

    if (physical_device->external_memory_align && !external_info)
    {
        host_external_info.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
        host_external_info.pNext = create_info->pNext;
        host_external_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
        ((VkBufferCreateInfo *)create_info)->pNext = &host_external_info; /* cast away const, it has been copied in the thunks */
    }

    return device->host_vkCreateImage( device->host.device, create_info, NULL, image );
}

static void win32u_vkGetDeviceImageMemoryRequirements( VkDevice client_device, const VkDeviceImageMemoryRequirements *image_requirements,
                                                       VkMemoryRequirements2 *memory_requirements )
{
    struct vulkan_device *device = vulkan_device_from_handle( client_device );
    VkExternalMemoryImageCreateInfo *external_info;
    VkBaseOutStructure **next;

    TRACE( "device %p, image_requirements %p, memory_requirements %p\n", device, image_requirements, memory_requirements );

    for (next = (VkBaseOutStructure **)&image_requirements->pCreateInfo->pNext; *next; next = &(*next)->pNext)
    {
        switch ((*next)->sType)
        {
        case VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_IMAGE_CREATE_INFO_NV: break;
        case VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO:
            external_info = (VkExternalMemoryImageCreateInfo *)*next;
            if (external_info->handleTypes & EXTERNAL_MEMORY_WIN32_BITS)
            {
                FIXME( "VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO not implemented!\n" );
                *next = (*next)->pNext;
            }
            break;
        case VK_STRUCTURE_TYPE_IMAGE_SWAPCHAIN_CREATE_INFO_KHR: break;
        case VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO: break;
        case VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT: break;
        case VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT: break;
        case VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO: break;
        case VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR: break;
        case VK_STRUCTURE_TYPE_OPAQUE_CAPTURE_DESCRIPTOR_DATA_CREATE_INFO_EXT: break;
        case VK_STRUCTURE_TYPE_IMAGE_COMPRESSION_CONTROL_EXT: break;
        case VK_STRUCTURE_TYPE_OPTICAL_FLOW_IMAGE_FORMAT_INFO_NV: break;
        case VK_STRUCTURE_TYPE_IMAGE_ALIGNMENT_CONTROL_CREATE_INFO_MESA: break;
        default: FIXME( "Unhandled sType %u.\n", (*next)->sType ); break;
        }
    }

    device->host_vkGetDeviceImageMemoryRequirements( device->host.device, image_requirements, memory_requirements );
}

static VkResult win32u_vkGetPhysicalDeviceImageFormatProperties2( VkPhysicalDevice client_physical_device, const VkPhysicalDeviceImageFormatInfo2 *format_info,
                                                                  VkImageFormatProperties2 *format_properies )
{
    struct vulkan_physical_device *physical_device = vulkan_physical_device_from_handle( client_physical_device );
    struct vulkan_instance *instance = physical_device->instance;
    VkPhysicalDeviceExternalImageFormatInfo *external_info;
    VkBaseOutStructure **next;

    TRACE( "physical_device %p, format_info %p, format_properies %p\n", physical_device, format_info, format_properies );

    for (next = (VkBaseOutStructure **)&format_info->pNext; *next; next = &(*next)->pNext)
    {
        switch ((*next)->sType)
        {
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO:;
            external_info = (VkPhysicalDeviceExternalImageFormatInfo *)*next;
            if (external_info->handleType & EXTERNAL_MEMORY_WIN32_BITS)
            {
                FIXME( "VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO not implemented!\n" );
                *next = (*next)->pNext;
            }
            break;
        case VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO: break;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT: break;
        case VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO: break;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_IMAGE_FORMAT_INFO_EXT: break;
        case VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR: break;
        case VK_STRUCTURE_TYPE_IMAGE_COMPRESSION_CONTROL_EXT: break;
        case VK_STRUCTURE_TYPE_OPTICAL_FLOW_IMAGE_FORMAT_INFO_NV: break;
        default: FIXME( "Unhandled sType %u.\n", (*next)->sType ); break;
        }
    }

    return instance->host_vkGetPhysicalDeviceImageFormatProperties2( physical_device->host.physical_device, format_info, format_properies );
}

static VkResult win32u_vkGetPhysicalDeviceImageFormatProperties2KHR( VkPhysicalDevice client_physical_device, const VkPhysicalDeviceImageFormatInfo2 *format_info,
                                                                     VkImageFormatProperties2 *format_properies )
{
    return win32u_vkGetPhysicalDeviceImageFormatProperties2( client_physical_device, format_info, format_properies );
}

static VkResult win32u_vkCreateWin32SurfaceKHR( VkInstance client_instance, const VkWin32SurfaceCreateInfoKHR *create_info,
                                                const VkAllocationCallbacks *allocator, VkSurfaceKHR *ret )
{
    struct vulkan_instance *instance = vulkan_instance_from_handle( client_instance );
    VkSurfaceKHR host_surface;
    struct surface *surface;
    HWND dummy = NULL;
    VkResult res;
    WND *win;

    TRACE( "client_instance %p, create_info %p, allocator %p, ret %p\n", client_instance, create_info, allocator, ret );
    if (allocator) FIXME( "Support for allocation callbacks not implemented yet\n" );

    if (!(surface = calloc( 1, sizeof(*surface) ))) return VK_ERROR_OUT_OF_HOST_MEMORY;

    /* Windows allows surfaces to be created with no HWND, they return VK_ERROR_SURFACE_LOST_KHR later */
    if (!(surface->hwnd = create_info->hwnd))
    {
        static const WCHAR staticW[] = {'s','t','a','t','i','c',0};
        UNICODE_STRING static_us = RTL_CONSTANT_STRING( staticW );
        dummy = NtUserCreateWindowEx( 0, &static_us, &static_us, &static_us, WS_POPUP, 0, 0, 0, 0,
                                      NULL, NULL, NULL, NULL, 0, NULL, 0, FALSE );
        WARN( "Created dummy window %p for null surface window\n", dummy );
        surface->hwnd = dummy;
    }

    if ((res = driver_funcs->p_vulkan_surface_create( surface->hwnd, instance->host.instance,
                                                      &host_surface, &surface->driver_private )))
    {
        if (dummy) NtUserDestroyWindow( dummy );
        free( surface );
        return res;
    }

    if (!(win = get_win_ptr( surface->hwnd )) || win == WND_DESKTOP || win == WND_OTHER_PROCESS)
        list_init( &surface->entry );
    else
    {
        list_add_tail( &win->vulkan_surfaces, &surface->entry );
        release_win_ptr( win );
    }

    vulkan_object_init( &surface->obj.obj, host_surface );
    surface->obj.instance = instance;
    instance->p_insert_object( instance, &surface->obj.obj );

    if (dummy) NtUserDestroyWindow( dummy );

    *ret = surface->obj.client.surface;
    return VK_SUCCESS;
}

static void win32u_vkDestroySurfaceKHR( VkInstance client_instance, VkSurfaceKHR client_surface,
                                        const VkAllocationCallbacks *allocator )
{
    struct vulkan_instance *instance = vulkan_instance_from_handle( client_instance );
    struct surface *surface = surface_from_handle( client_surface );
    WND *win;

    if (!surface) return;

    TRACE( "instance %p, handle 0x%s, allocator %p\n", instance, wine_dbgstr_longlong( client_surface ), allocator );
    if (allocator) FIXME( "Support for allocation callbacks not implemented yet\n" );

    if ((win = get_win_ptr( surface->hwnd )) && win != WND_DESKTOP && win != WND_OTHER_PROCESS)
    {
        list_remove( &surface->entry );
        release_win_ptr( win );
    }

    instance->p_vkDestroySurfaceKHR( instance->host.instance, surface->obj.host.surface, NULL /* allocator */ );
    driver_funcs->p_vulkan_surface_destroy( surface->hwnd, surface->driver_private );

    instance->p_remove_object( instance, &surface->obj.obj );

    free( surface );
}

static void adjust_surface_capabilities( struct vulkan_instance *instance, struct surface *surface,
                                         VkSurfaceCapabilitiesKHR *capabilities )
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
    NtUserGetClientRect( surface->hwnd, &client_rect, NtUserGetDpiForWindow( surface->hwnd ) );
    capabilities->minImageExtent.width = client_rect.right - client_rect.left;
    capabilities->minImageExtent.height = client_rect.bottom - client_rect.top;
    capabilities->maxImageExtent.width = client_rect.right - client_rect.left;
    capabilities->maxImageExtent.height = client_rect.bottom - client_rect.top;
    capabilities->currentExtent.width = client_rect.right - client_rect.left;
    capabilities->currentExtent.height = client_rect.bottom - client_rect.top;
}

static VkResult win32u_vkGetPhysicalDeviceSurfaceCapabilitiesKHR( VkPhysicalDevice client_physical_device, VkSurfaceKHR client_surface,
                                                                  VkSurfaceCapabilitiesKHR *capabilities )
{
    struct vulkan_physical_device *physical_device = vulkan_physical_device_from_handle( client_physical_device );
    struct surface *surface = surface_from_handle( client_surface );
    struct vulkan_instance *instance = physical_device->instance;
    VkResult res;

    if (!NtUserIsWindow( surface->hwnd )) return VK_ERROR_SURFACE_LOST_KHR;
    res = instance->p_vkGetPhysicalDeviceSurfaceCapabilitiesKHR( physical_device->host.physical_device,
                                                       surface->obj.host.surface, capabilities );
    if (!res) adjust_surface_capabilities( instance, surface, capabilities );
    return res;
}

static VkResult win32u_vkGetPhysicalDeviceSurfaceCapabilities2KHR( VkPhysicalDevice client_physical_device, const VkPhysicalDeviceSurfaceInfo2KHR *surface_info,
                                                                   VkSurfaceCapabilities2KHR *capabilities )
{
    struct vulkan_physical_device *physical_device = vulkan_physical_device_from_handle( client_physical_device );
    struct surface *surface = surface_from_handle( surface_info->surface );
    VkPhysicalDeviceSurfaceInfo2KHR surface_info_host = *surface_info;
    struct vulkan_instance *instance = physical_device->instance;
    VkResult res;

    if (!instance->p_vkGetPhysicalDeviceSurfaceCapabilities2KHR)
    {
        /* Until the loader version exporting this function is common, emulate it using the older non-2 version. */
        if (surface_info->pNext || capabilities->pNext) FIXME( "Emulating vkGetPhysicalDeviceSurfaceCapabilities2KHR, ignoring pNext.\n" );
        return win32u_vkGetPhysicalDeviceSurfaceCapabilitiesKHR( client_physical_device, surface_info->surface,
                                                                 &capabilities->surfaceCapabilities );
    }

    surface_info_host.surface = surface->obj.host.surface;

    if (!NtUserIsWindow( surface->hwnd )) return VK_ERROR_SURFACE_LOST_KHR;
    res = instance->p_vkGetPhysicalDeviceSurfaceCapabilities2KHR( physical_device->host.physical_device,
                                                                     &surface_info_host, capabilities );
    if (!res) adjust_surface_capabilities( instance, surface, &capabilities->surfaceCapabilities );
    return res;
}

static VkResult win32u_vkGetPhysicalDevicePresentRectanglesKHR( VkPhysicalDevice client_physical_device, VkSurfaceKHR client_surface,
                                                                uint32_t *rect_count, VkRect2D *rects )
{
    struct vulkan_physical_device *physical_device = vulkan_physical_device_from_handle( client_physical_device );
    struct surface *surface = surface_from_handle( client_surface );
    struct vulkan_instance *instance = physical_device->instance;

    if (!NtUserIsWindow( surface->hwnd ))
    {
        if (rects && !*rect_count) return VK_INCOMPLETE;
        if (rects) memset( rects, 0, sizeof(VkRect2D) );
        *rect_count = 1;
        return VK_SUCCESS;
    }

    return instance->p_vkGetPhysicalDevicePresentRectanglesKHR( physical_device->host.physical_device,
                                                                   surface->obj.host.surface, rect_count, rects );
}

static VkResult win32u_vkGetPhysicalDeviceSurfaceFormatsKHR( VkPhysicalDevice client_physical_device, VkSurfaceKHR client_surface,
                                                             uint32_t *format_count, VkSurfaceFormatKHR *formats )
{
    struct vulkan_physical_device *physical_device = vulkan_physical_device_from_handle( client_physical_device );
    struct surface *surface = surface_from_handle( client_surface );
    struct vulkan_instance *instance = physical_device->instance;

    return instance->p_vkGetPhysicalDeviceSurfaceFormatsKHR( physical_device->host.physical_device,
                                                                surface->obj.host.surface, format_count, formats );
}

static VkResult win32u_vkGetPhysicalDeviceSurfaceFormats2KHR( VkPhysicalDevice client_physical_device, const VkPhysicalDeviceSurfaceInfo2KHR *surface_info,
                                                              uint32_t *format_count, VkSurfaceFormat2KHR *formats )
{
    struct vulkan_physical_device *physical_device = vulkan_physical_device_from_handle( client_physical_device );
    struct surface *surface = surface_from_handle( surface_info->surface );
    VkPhysicalDeviceSurfaceInfo2KHR surface_info_host = *surface_info;
    struct vulkan_instance *instance = physical_device->instance;
    VkResult res;

    if (!instance->p_vkGetPhysicalDeviceSurfaceFormats2KHR)
    {
        VkSurfaceFormatKHR *surface_formats;
        UINT i;

        /* Until the loader version exporting this function is common, emulate it using the older non-2 version. */
        if (surface_info->pNext) FIXME( "Emulating vkGetPhysicalDeviceSurfaceFormats2KHR, ignoring pNext.\n" );
        if (!formats) return win32u_vkGetPhysicalDeviceSurfaceFormatsKHR( client_physical_device, surface_info->surface, format_count, NULL );

        surface_formats = calloc( *format_count, sizeof(*surface_formats) );
        if (!surface_formats) return VK_ERROR_OUT_OF_HOST_MEMORY;

        res = win32u_vkGetPhysicalDeviceSurfaceFormatsKHR( client_physical_device, surface_info->surface, format_count, surface_formats );
        if (!res || res == VK_INCOMPLETE) for (i = 0; i < *format_count; i++) formats[i].surfaceFormat = surface_formats[i];

        free( surface_formats );
        return res;
    }

    surface_info_host.surface = surface->obj.host.surface;

    return instance->p_vkGetPhysicalDeviceSurfaceFormats2KHR( physical_device->host.physical_device,
                                                                 &surface_info_host, format_count, formats );
}

static VkBool32 win32u_vkGetPhysicalDeviceWin32PresentationSupportKHR( VkPhysicalDevice client_physical_device, uint32_t queue )
{
    struct vulkan_physical_device *physical_device = vulkan_physical_device_from_handle( client_physical_device );
    return driver_funcs->p_vkGetPhysicalDeviceWin32PresentationSupportKHR( physical_device->host.physical_device, queue );
}

static VkResult win32u_vkCreateSwapchainKHR( VkDevice client_device, const VkSwapchainCreateInfoKHR *create_info,
                                             const VkAllocationCallbacks *allocator, VkSwapchainKHR *ret )
{
    struct swapchain *swapchain, *old_swapchain = swapchain_from_handle( create_info->oldSwapchain );
    struct surface *surface = surface_from_handle( create_info->surface );
    struct vulkan_device *device = vulkan_device_from_handle( client_device );
    struct vulkan_physical_device *physical_device = device->physical_device;
    struct vulkan_instance *instance = physical_device->instance;
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

    /* update the host surface to commit any pending size change */
    driver_funcs->p_vulkan_surface_update( surface->hwnd, surface->driver_private, TRUE );

    /* Windows allows client rect to be empty, but host Vulkan often doesn't, adjust extents back to the host capabilities */
    res = instance->p_vkGetPhysicalDeviceSurfaceCapabilitiesKHR( physical_device->host.physical_device, surface->obj.host.surface, &capabilities );
    if (res) return res;

    create_info_host.imageExtent.width = max( create_info_host.imageExtent.width, capabilities.minImageExtent.width );
    create_info_host.imageExtent.height = max( create_info_host.imageExtent.height, capabilities.minImageExtent.height );

    if (!(swapchain = calloc( 1, sizeof(*swapchain) ))) return VK_ERROR_OUT_OF_HOST_MEMORY;

    if ((res = device->p_vkCreateSwapchainKHR( device->host.device, &create_info_host, NULL, &host_swapchain )))
    {
        free( swapchain );
        return res;
    }

    vulkan_object_init( &swapchain->obj.obj, host_swapchain );
    swapchain->surface = surface;
    swapchain->extents = create_info->imageExtent;
    instance->p_insert_object( instance, &swapchain->obj.obj );

    *ret = swapchain->obj.client.swapchain;
    return VK_SUCCESS;
}

void win32u_vkDestroySwapchainKHR( VkDevice client_device, VkSwapchainKHR client_swapchain,
                                   const VkAllocationCallbacks *allocator )
{
    struct vulkan_device *device = vulkan_device_from_handle( client_device );
    struct vulkan_instance *instance = device->physical_device->instance;
    struct swapchain *swapchain = swapchain_from_handle( client_swapchain );

    if (allocator) FIXME( "Support for allocation callbacks not implemented yet\n" );
    if (!swapchain) return;

    device->p_vkDestroySwapchainKHR( device->host.device, swapchain->obj.host.swapchain, NULL );
    instance->p_remove_object( instance, &swapchain->obj.obj );

    free( swapchain );
}

static BOOL extents_equals( const VkExtent2D *extents, const RECT *rect )
{
    return extents->width == rect->right - rect->left && extents->height == rect->bottom - rect->top;
}

static VkResult win32u_vkAcquireNextImage2KHR( VkDevice client_device, const VkAcquireNextImageInfoKHR *acquire_info,
                                               uint32_t *image_index )
{
    struct vulkan_semaphore *semaphore = acquire_info->semaphore ? vulkan_semaphore_from_handle( acquire_info->semaphore ) : NULL;
    struct swapchain *swapchain = swapchain_from_handle( acquire_info->swapchain );
    struct vulkan_device *device = vulkan_device_from_handle( client_device );
    VkAcquireNextImageInfoKHR acquire_info_host = *acquire_info;
    struct surface *surface = swapchain->surface;
    RECT client_rect;
    VkResult res;

    acquire_info_host.swapchain = swapchain->obj.host.swapchain;
    acquire_info_host.semaphore = semaphore ? semaphore->host.semaphore : 0;
    res = device->p_vkAcquireNextImage2KHR( device->host.device, &acquire_info_host, image_index );

    if (!res && NtUserGetClientRect( surface->hwnd, &client_rect, NtUserGetDpiForWindow( surface->hwnd ) ) &&
        !extents_equals( &swapchain->extents, &client_rect ))
    {
        WARN( "Swapchain size %dx%d does not match client rect %s, returning VK_SUBOPTIMAL_KHR\n",
              swapchain->extents.width, swapchain->extents.height, wine_dbgstr_rect( &client_rect ) );
        return VK_SUBOPTIMAL_KHR;
    }

    return res;
}

static VkResult win32u_vkAcquireNextImageKHR( VkDevice client_device, VkSwapchainKHR client_swapchain, uint64_t timeout,
                                              VkSemaphore client_semaphore, VkFence fence, uint32_t *image_index )
{
    struct vulkan_semaphore *semaphore = client_semaphore ? vulkan_semaphore_from_handle( client_semaphore ) : NULL;
    struct swapchain *swapchain = swapchain_from_handle( client_swapchain );
    struct vulkan_device *device = vulkan_device_from_handle( client_device );
    struct surface *surface = swapchain->surface;
    RECT client_rect;
    VkResult res;

    res = device->p_vkAcquireNextImageKHR( device->host.device, swapchain->obj.host.swapchain, timeout,
                                              semaphore ? semaphore->host.semaphore : 0, fence, image_index );

    if (!res && NtUserGetClientRect( surface->hwnd, &client_rect, NtUserGetDpiForWindow( surface->hwnd ) ) &&
        !extents_equals( &swapchain->extents, &client_rect ))
    {
        WARN( "Swapchain size %dx%d does not match client rect %s, returning VK_SUBOPTIMAL_KHR\n",
              swapchain->extents.width, swapchain->extents.height, wine_dbgstr_rect( &client_rect ) );
        return VK_SUBOPTIMAL_KHR;
    }

    return res;
}

static VkResult win32u_vkQueuePresentKHR( VkQueue client_queue, const VkPresentInfoKHR *present_info )
{
    VkSwapchainKHR swapchains_buffer[16], *client_swapchains = swapchains_buffer;
    struct vulkan_queue *queue = vulkan_queue_from_handle( client_queue );
    VkPresentInfoKHR present_info_host = *present_info;
    struct vulkan_device *device = queue->device;
    VkResult res;
    UINT i;

    TRACE( "queue %p, present_info %p\n", queue, present_info );

    if (present_info->swapchainCount > ARRAY_SIZE(swapchains_buffer) &&
        !(client_swapchains = malloc( present_info->swapchainCount * sizeof(*client_swapchains) )))
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    memcpy( client_swapchains, present_info->pSwapchains, present_info->swapchainCount * sizeof(*client_swapchains) );

    for (i = 0; i < present_info->swapchainCount; i++)
    {
        VkSwapchainKHR *swapchains = (VkSwapchainKHR *)present_info->pSwapchains; /* cast away const, it has been copied in the thunks */
        struct swapchain *swapchain = swapchain_from_handle( swapchains[i] );
        swapchains[i] = swapchain->obj.host.swapchain;
    }

    for (i = 0; i < present_info->waitSemaphoreCount; i++)
    {
        VkSemaphore *semaphores = (VkSemaphore *)present_info_host.pWaitSemaphores; /* cast away const, it has been copied in the thunks */
        struct vulkan_semaphore *semaphore = vulkan_semaphore_from_handle( semaphores[i] );
        semaphores[i] = semaphore->host.semaphore;
    }

    res = device->p_vkQueuePresentKHR( queue->host.queue, present_info );

    for (i = 0; i < present_info->swapchainCount; i++)
    {
        struct swapchain *swapchain = swapchain_from_handle( client_swapchains[i] );
        VkResult swapchain_res = present_info->pResults ? present_info->pResults[i] : res;
        struct surface *surface = swapchain->surface;
        RECT client_rect;

        if (surface->hwnd)
            driver_funcs->p_vulkan_surface_presented( surface->hwnd, surface->driver_private, swapchain_res );

        if (swapchain_res < VK_SUCCESS) continue;
        if (!NtUserGetClientRect( surface->hwnd, &client_rect, NtUserGetDpiForWindow( surface->hwnd ) ))
        {
            WARN( "Swapchain window %p is invalid, returning VK_ERROR_OUT_OF_DATE_KHR\n", surface->hwnd );
            if (present_info->pResults) present_info->pResults[i] = VK_ERROR_OUT_OF_DATE_KHR;
            if (res >= VK_SUCCESS) res = VK_ERROR_OUT_OF_DATE_KHR;
        }
        else if (swapchain_res)
            WARN( "Present returned status %d for swapchain %p\n", swapchain_res, swapchain );
        else if (!extents_equals( &swapchain->extents, &client_rect ))
        {
            WARN( "Swapchain size %dx%d does not match client rect %s, returning VK_SUBOPTIMAL_KHR\n",
                  swapchain->extents.width, swapchain->extents.height, wine_dbgstr_rect( &client_rect ) );
            if (present_info->pResults) present_info->pResults[i] = VK_SUBOPTIMAL_KHR;
            if (!res) res = VK_SUBOPTIMAL_KHR;
        }
    }

    if (client_swapchains != swapchains_buffer) free( client_swapchains );

    if (TRACE_ON( fps ))
    {
        static unsigned long frames, frames_total;
        static long prev_time, start_time;
        DWORD time;

        time = NtGetTickCount();
        frames++;
        frames_total++;

        if (time - prev_time > 1500)
        {
            TRACE_(fps)( "%p @ approx %.2ffps, total %.2ffps\n", queue, 1000.0 * frames / (time - prev_time),
                         1000.0 * frames_total / (time - start_time) );
            prev_time = time;
            frames = 0;

            if (!start_time) start_time = time;
        }
    }

    return res;
}

static VkResult win32u_vkQueueSubmit( VkQueue client_queue, uint32_t count, const VkSubmitInfo *submits, VkFence fence )
{
    struct vulkan_queue *queue = vulkan_queue_from_handle( client_queue );
    struct vulkan_device *device = queue->device;
    uint32_t i, j;

    for (i = 0; i < count; i++)
    {
        VkSubmitInfo *submit = (VkSubmitInfo *)submits + i; /* cast away const, it has been copied in the thunks */
        VkBaseOutStructure **next;

        for (j = 0; j < submit->waitSemaphoreCount; j++)
        {
            VkSemaphore *semaphores = (VkSemaphore *)submit->pWaitSemaphores; /* cast away const, it has been copied in the thunks */
            struct vulkan_semaphore *semaphore = vulkan_semaphore_from_handle( semaphores[j] );
            semaphores[j] = semaphore->host.semaphore;
        }

        for (j = 0; j < submit->commandBufferCount; j++)
        {
            VkCommandBuffer *command_buffers = (VkCommandBuffer *)submit->pCommandBuffers; /* cast away const, it has been copied in the thunks */
            struct vulkan_command_buffer *command_buffer = vulkan_command_buffer_from_handle( command_buffers[j] );
            command_buffers[j] = command_buffer->host.command_buffer;
        }

        for (j = 0; j < submit->signalSemaphoreCount; j++)
        {
            VkSemaphore *semaphores = (VkSemaphore *)submit->pSignalSemaphores; /* cast away const, it has been copied in the thunks */
            struct vulkan_semaphore *semaphore = vulkan_semaphore_from_handle( semaphores[j] );
            semaphores[j] = semaphore->host.semaphore;
        }

        for (next = (VkBaseOutStructure **)&submit->pNext; *next; next = &(*next)->pNext)
        {
            switch ((*next)->sType)
            {
            case VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR:
                FIXME( "VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR not implemented!\n" );
                *next = (*next)->pNext;
                break;
            case VK_STRUCTURE_TYPE_D3D12_FENCE_SUBMIT_INFO_KHR:
                FIXME( "VK_STRUCTURE_TYPE_D3D12_FENCE_SUBMIT_INFO_KHR not implemented!\n" );
                *next = (*next)->pNext;
                break;
            case VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO: break;
            case VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO: break;
            case VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO: break;
            case VK_STRUCTURE_TYPE_PERFORMANCE_QUERY_SUBMIT_INFO_KHR: break;
            case VK_STRUCTURE_TYPE_FRAME_BOUNDARY_EXT: break;
            case VK_STRUCTURE_TYPE_LATENCY_SUBMISSION_PRESENT_ID_NV: break;
            default: FIXME( "Unhandled sType %u.\n", (*next)->sType ); break;
            }
        }
    }

    return device->host_vkQueueSubmit( queue->host.queue, count, submits, fence );
}

static VkResult win32u_vkQueueSubmit2( VkQueue client_queue, uint32_t count, const VkSubmitInfo2 *submits, VkFence fence )
{
    struct vulkan_queue *queue = vulkan_queue_from_handle( client_queue );
    struct vulkan_device *device = queue->device;
    uint32_t i, j;

    for (i = 0; i < count; i++)
    {
        VkSubmitInfo2 *submit = (VkSubmitInfo2 *)submits + i; /* cast away const, it has been copied in the thunks */
        VkBaseOutStructure **next;

        for (j = 0; j < submit->waitSemaphoreInfoCount; j++)
        {
            VkSemaphoreSubmitInfo *semaphore_infos = (VkSemaphoreSubmitInfo *)submit->pWaitSemaphoreInfos; /* cast away const, it has been copied in the thunks */
            struct vulkan_semaphore *semaphore = vulkan_semaphore_from_handle( semaphore_infos[j].semaphore );
            semaphore_infos[j].semaphore = semaphore->host.semaphore;
            if (semaphore_infos->pNext) FIXME( "Unhandled struct chain\n" );
        }

        for (j = 0; j < submit->commandBufferInfoCount; j++)
        {
            VkCommandBufferSubmitInfo *command_buffer_infos = (VkCommandBufferSubmitInfo *)submit->pCommandBufferInfos; /* cast away const, it has been copied in the thunks */
            struct vulkan_command_buffer *command_buffer = vulkan_command_buffer_from_handle( command_buffer_infos[j].commandBuffer );
            command_buffer_infos[j].commandBuffer = command_buffer->host.command_buffer;
            if (command_buffer_infos->pNext) FIXME( "Unhandled struct chain\n" );
        }

        for (j = 0; j < submit->signalSemaphoreInfoCount; j++)
        {
            VkSemaphoreSubmitInfo *semaphore_infos = (VkSemaphoreSubmitInfo *)submit->pSignalSemaphoreInfos; /* cast away const, it has been copied in the thunks */
            struct vulkan_semaphore *semaphore = vulkan_semaphore_from_handle( semaphore_infos[j].semaphore );
            semaphore_infos[j].semaphore = semaphore->host.semaphore;
            if (semaphore_infos->pNext) FIXME( "Unhandled struct chain\n" );
        }

        for (next = (VkBaseOutStructure **)&submit->pNext; *next; next = &(*next)->pNext)
        {
            switch ((*next)->sType)
            {
            case VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR:
                FIXME( "VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR not implemented!\n" );
                *next = (*next)->pNext;
                break;
            case VK_STRUCTURE_TYPE_PERFORMANCE_QUERY_SUBMIT_INFO_KHR: break;
            case VK_STRUCTURE_TYPE_FRAME_BOUNDARY_EXT: break;
            case VK_STRUCTURE_TYPE_LATENCY_SUBMISSION_PRESENT_ID_NV: break;
            default: FIXME( "Unhandled sType %u.\n", (*next)->sType ); break;
            }
        }
    }

    return device->host_vkQueueSubmit2( queue->host.queue, count, submits, fence );
}

static VkResult win32u_vkQueueSubmit2KHR( VkQueue client_queue, uint32_t count, const VkSubmitInfo2 *submits, VkFence fence )
{
    return win32u_vkQueueSubmit2( client_queue, count, submits, fence );
}

static VkResult win32u_vkCreateSemaphore( VkDevice client_device, const VkSemaphoreCreateInfo *create_info,
                                          const VkAllocationCallbacks *allocator, VkSemaphore *ret )
{
    struct vulkan_device *device = vulkan_device_from_handle( client_device );
    struct vulkan_instance *instance = device->physical_device->instance;
    struct semaphore *semaphore;
    VkSemaphore host_semaphore;
    VkBaseOutStructure **next;
    VkResult res;

    TRACE( "device %p, create_info %p, allocator %p, ret %p\n", device, create_info, allocator, ret );

    for (next = (VkBaseOutStructure **)&create_info->pNext; *next; next = &(*next)->pNext)
    {
        switch ((*next)->sType)
        {
        case VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO:
            FIXME( "VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO not implemented!\n" );
            *next = (*next)->pNext;
            break;
        case VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR:
            FIXME( "VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR not implemented!\n" );
            *next = (*next)->pNext;
            break;
        case VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO: break;
        case VK_STRUCTURE_TYPE_QUERY_LOW_LATENCY_SUPPORT_NV: break;
        default: FIXME( "Unhandled sType %u.\n", (*next)->sType ); break;
        }
    }

    if (!(semaphore = calloc( 1, sizeof(*semaphore) ))) return VK_ERROR_OUT_OF_HOST_MEMORY;

    if ((res = device->host_vkCreateSemaphore( device->host.device, create_info, NULL /* allocator */, &host_semaphore )))
    {
        free( semaphore );
        return res;
    }

    vulkan_object_init( &semaphore->obj.obj, host_semaphore, NULL );
    instance->p_insert_object( instance, &semaphore->obj.obj );

    *ret = semaphore->obj.client.semaphore;
    return res;
}

static void win32u_vkDestroySemaphore( VkDevice client_device, VkSemaphore client_semaphore, const VkAllocationCallbacks *allocator )
{
    struct vulkan_device *device = vulkan_device_from_handle( client_device );
    struct semaphore *semaphore = semaphore_from_handle( client_semaphore );
    struct vulkan_instance *instance = device->physical_device->instance;

    TRACE( "device %p, semaphore %p, allocator %p\n", device, semaphore, allocator );

    if (!client_semaphore) return;

    device->host_vkDestroySemaphore( device->host.device, semaphore->obj.host.semaphore, NULL /* allocator */ );
    instance->p_remove_object( instance, &semaphore->obj.obj );

    free( semaphore );
}

static VkResult win32u_vkGetSemaphoreWin32HandleKHR( VkDevice client_device, const VkSemaphoreGetWin32HandleInfoKHR *handle_info, HANDLE *handle )
{
    struct vulkan_device *device = vulkan_device_from_handle( client_device );

    FIXME( "device %p, handle_info %p, handle %p stub!\n", device, handle_info, handle );

    return VK_ERROR_INCOMPATIBLE_DRIVER;
}

static VkResult win32u_vkImportSemaphoreWin32HandleKHR( VkDevice client_device, const VkImportSemaphoreWin32HandleInfoKHR *handle_info )
{
    struct vulkan_device *device = vulkan_device_from_handle( client_device );

    FIXME( "device %p, handle_info %p stub!\n", device, handle_info );

    return VK_ERROR_INCOMPATIBLE_DRIVER;
}

static void win32u_vkGetPhysicalDeviceExternalSemaphoreProperties( VkPhysicalDevice client_physical_device, const VkPhysicalDeviceExternalSemaphoreInfo *semaphore_info,
                                                                   VkExternalSemaphoreProperties *semaphore_properties )
{
    struct vulkan_physical_device *physical_device = vulkan_physical_device_from_handle( client_physical_device );
    struct vulkan_instance *instance = physical_device->instance;

    TRACE( "physical_device %p, semaphore_info %p, semaphore_properties %p\n", physical_device, semaphore_info, semaphore_properties );

    instance->host_vkGetPhysicalDeviceExternalSemaphoreProperties( physical_device->host.physical_device, semaphore_info, semaphore_properties );
}

static void win32u_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR( VkPhysicalDevice client_physical_device, const VkPhysicalDeviceExternalSemaphoreInfo *semaphore_info,
                                                                      VkExternalSemaphoreProperties *semaphore_properties )
{
    win32u_vkGetPhysicalDeviceExternalSemaphoreProperties( client_physical_device, semaphore_info, semaphore_properties );
}

static VkResult win32u_vkCreateFence( VkDevice client_device, const VkFenceCreateInfo *create_info, const VkAllocationCallbacks *allocator, VkFence *ret )
{
    struct vulkan_device *device = vulkan_device_from_handle( client_device );
    VkBaseOutStructure **next;

    TRACE( "device %p, create_info %p, allocator %p, ret %p\n", device, create_info, allocator, ret );

    for (next = (VkBaseOutStructure **)&create_info->pNext; *next; next = &(*next)->pNext)
    {
        switch ((*next)->sType)
        {
        case VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO:
            FIXME( "VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO not implemented.\n" );
            *next = (*next)->pNext;
            break;
        case VK_STRUCTURE_TYPE_EXPORT_FENCE_WIN32_HANDLE_INFO_KHR:
            FIXME( "VK_STRUCTURE_TYPE_EXPORT_FENCE_WIN32_HANDLE_INFO_KHR not implemented.\n" );
            *next = (*next)->pNext;
            break;
        default: FIXME( "Unhandled sType %u.\n", (*next)->sType ); break;
        }
    }

    return device->host_vkCreateFence( device->host.device, create_info, NULL /* allocator */, ret );
}

static void win32u_vkDestroyFence( VkDevice client_device, VkFence fence, const VkAllocationCallbacks *allocator )
{
    struct vulkan_device *device = vulkan_device_from_handle( client_device );

    TRACE( "device %p, fence %s, allocator %p\n", device, wine_dbgstr_longlong(fence), allocator );

    return device->host_vkDestroyFence( device->host.device, fence, allocator );
}

static VkResult win32u_vkGetFenceWin32HandleKHR( VkDevice client_device, const VkFenceGetWin32HandleInfoKHR *handle_info, HANDLE *handle )
{
    struct vulkan_device *device = vulkan_device_from_handle( client_device );

    FIXME( "device %p, handle_info %p, handle %p stub!\n", device, handle_info, handle );

    return VK_ERROR_INCOMPATIBLE_DRIVER;
}

static VkResult win32u_vkImportFenceWin32HandleKHR( VkDevice client_device, const VkImportFenceWin32HandleInfoKHR *handle_info )
{
    struct vulkan_device *device = vulkan_device_from_handle( client_device );

    FIXME( "device %p, handle_info %p stub!\n", device, handle_info );

    return VK_ERROR_INCOMPATIBLE_DRIVER;
}

static void win32u_vkGetPhysicalDeviceExternalFenceProperties( VkPhysicalDevice client_physical_device, const VkPhysicalDeviceExternalFenceInfo *fence_info,
                                                               VkExternalFenceProperties *fence_properties )
{
    struct vulkan_physical_device *physical_device = vulkan_physical_device_from_handle( client_physical_device );
    struct vulkan_instance *instance = physical_device->instance;

    TRACE( "physical_device %p, fence_info %p, fence_properties %p\n", physical_device, fence_info, fence_properties );

    instance->host_vkGetPhysicalDeviceExternalFenceProperties( physical_device->host.physical_device, fence_info, fence_properties );
}

static void win32u_vkGetPhysicalDeviceExternalFencePropertiesKHR( VkPhysicalDevice client_physical_device, const VkPhysicalDeviceExternalFenceInfo *fence_info,
                                                                  VkExternalFenceProperties *fence_properties )
{
    return win32u_vkGetPhysicalDeviceExternalFenceProperties( client_physical_device, fence_info, fence_properties );
}

static const char *win32u_get_host_surface_extension(void)
{
    return driver_funcs->p_get_host_surface_extension();
}

static struct vulkan_funcs vulkan_funcs =
{
    .p_vkCreateWin32SurfaceKHR = win32u_vkCreateWin32SurfaceKHR,
    .p_vkDestroySurfaceKHR = win32u_vkDestroySurfaceKHR,
    .p_vkGetPhysicalDeviceSurfaceCapabilitiesKHR = win32u_vkGetPhysicalDeviceSurfaceCapabilitiesKHR,
    .p_vkGetPhysicalDeviceSurfaceCapabilities2KHR = win32u_vkGetPhysicalDeviceSurfaceCapabilities2KHR,
    .p_vkGetPhysicalDevicePresentRectanglesKHR = win32u_vkGetPhysicalDevicePresentRectanglesKHR,
    .p_vkGetPhysicalDeviceSurfaceFormatsKHR = win32u_vkGetPhysicalDeviceSurfaceFormatsKHR,
    .p_vkGetPhysicalDeviceSurfaceFormats2KHR = win32u_vkGetPhysicalDeviceSurfaceFormats2KHR,
    .p_vkGetPhysicalDeviceWin32PresentationSupportKHR = win32u_vkGetPhysicalDeviceWin32PresentationSupportKHR,
    .p_vkCreateSwapchainKHR = win32u_vkCreateSwapchainKHR,
    .p_vkDestroySwapchainKHR = win32u_vkDestroySwapchainKHR,
    .p_vkAcquireNextImage2KHR = win32u_vkAcquireNextImage2KHR,
    .p_vkAcquireNextImageKHR = win32u_vkAcquireNextImageKHR,
    .p_vkQueuePresentKHR = win32u_vkQueuePresentKHR,
    .p_get_host_surface_extension = win32u_get_host_surface_extension,
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

static void nulldrv_vulkan_surface_update( HWND hwnd, void *private, BOOL size_only )
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
    .p_vulkan_surface_update = nulldrv_vulkan_surface_update,
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

    if (status == STATUS_NOT_IMPLEMENTED) driver_funcs = &nulldrv_funcs;
    else vulkan_funcs.p_get_host_surface_extension = driver_funcs->p_get_host_surface_extension;
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

static void lazydrv_vulkan_surface_update( HWND hwnd, void *private, BOOL size_only )
{
    vulkan_driver_load();
    return driver_funcs->p_vulkan_surface_update( hwnd, private, size_only );
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
    .p_vulkan_surface_update = lazydrv_vulkan_surface_update,
    .p_vulkan_surface_presented = lazydrv_vulkan_surface_presented,
    .p_vkGetPhysicalDeviceWin32PresentationSupportKHR = lazydrv_vkGetPhysicalDeviceWin32PresentationSupportKHR,
    .p_get_host_surface_extension = lazydrv_get_host_surface_extension,
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
    vulkan_funcs.p_vkGetInstanceProcAddr = p_vkGetInstanceProcAddr;
    vulkan_funcs.p_vkGetDeviceProcAddr = p_vkGetDeviceProcAddr;
}

void vulkan_update_surfaces( HWND hwnd )
{
    struct surface *surface;
    struct rb_entry *ptr;

    pthread_mutex_lock( &window_surfaces_lock );

    if ((ptr = rb_get( &window_surfaces, hwnd )))
    {
        surface = RB_ENTRY_VALUE( ptr, struct surface, window_entry );
        driver_funcs->p_vulkan_surface_update( surface->hwnd, surface->driver_private );
    }

    pthread_mutex_unlock( &window_surfaces_lock );
}

void vulkan_detach_surfaces( struct list *surfaces )
{
    struct surface *surface, *next;

    LIST_FOR_EACH_ENTRY_SAFE( surface, next, surfaces, struct surface, entry )
    {
        driver_funcs->p_vulkan_surface_detach( surface->hwnd, surface->driver_private );
        list_remove( &surface->entry );
        list_init( &surface->entry );
        surface->hwnd = NULL;
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
