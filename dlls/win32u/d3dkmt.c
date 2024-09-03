/*
 * Copyright 2024 Rémi Bernon for CodeWeavers
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

#include <assert.h>
#include <pthread.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "ntgdi_private.h"
#include "win32u_private.h"
#include "ntuser_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(vulkan);

struct d3dkmt_object
{
    enum d3dkmt_type type;
    D3DKMT_HANDLE global;
    D3DKMT_HANDLE local;
    HANDLE handle;

    SIZE_T private_size;
    char private[256];
};

struct d3dkmt_resource
{
    struct d3dkmt_object obj;
    D3DKMT_HANDLE allocation;
};

struct d3dkmt_adapter
{
    D3DKMT_HANDLE handle;               /* Kernel mode graphics adapter handle */
    struct list entry;                  /* List entry */
    VkPhysicalDevice vk_device;         /* Vulkan physical device */
};

struct d3dkmt_device
{
    D3DKMT_HANDLE handle;               /* Kernel mode graphics device handle*/
    struct list entry;                  /* List entry */
};

struct d3dkmt_vidpn_source
{
    D3DKMT_VIDPNSOURCEOWNER_TYPE type;      /* VidPN source owner type */
    D3DDDI_VIDEO_PRESENT_SOURCE_ID id;      /* VidPN present source id */
    D3DKMT_HANDLE device;                   /* Kernel mode device context */
    struct list entry;                      /* List entry */
};

static pthread_mutex_t d3dkmt_lock = PTHREAD_MUTEX_INITIALIZER;
static struct list d3dkmt_adapters = LIST_INIT( d3dkmt_adapters );
static struct list d3dkmt_devices = LIST_INIT( d3dkmt_devices );
static struct list d3dkmt_vidpn_sources = LIST_INIT( d3dkmt_vidpn_sources );   /* VidPN source information list */

static struct d3dkmt_object **objects;
static unsigned int object_count, object_capacity, next_index;

static BOOL is_d3dkmt_global( D3DKMT_HANDLE handle )
{
    return (handle & 0xc0000000) && (handle & 0x3f) == 2;
}

/* return the position of the first object whose handle is not less than the
 * given local handle, d3dkmt_lock must be held. */
static unsigned int lookup_d3dkmt_object_pos( D3DKMT_HANDLE local )
{
    unsigned int begin = 0, end = object_count, mid;

    while (begin < end)
    {
        mid = begin + (end - begin) / 2;
        if (objects[mid]->local < local) begin = mid + 1;
        else end = mid;
    }

    return begin;
}

/* allocate a d3dkmt object with a local handle */
static D3DKMT_HANDLE alloc_object_handle( struct d3dkmt_object *object )
{
    D3DKMT_HANDLE handle = 0;
    unsigned int index;

    pthread_mutex_lock( &d3dkmt_lock );

    if (object_count >= object_capacity)
    {
        unsigned int capacity = max( 32, object_capacity * 3 / 2 );
        struct d3dkmt_object **tmp;
        assert( capacity > object_capacity );

        if (capacity >= 0xffff) goto done;
        if (!(tmp = realloc( objects, capacity * sizeof(*objects) ))) goto done;
        object_capacity = capacity;
        objects = tmp;
    }

    handle = object->local = ((next_index++ << 6) & ~0xc000003f) | 0x40000000;
    index = lookup_d3dkmt_object_pos( object->local );
    if (index < object_count) memmove( objects + index + 1, objects, (object_count - index) * sizeof(*objects) );
    objects[index] = object;
    object_count++;

done:
    pthread_mutex_unlock( &d3dkmt_lock );
    return handle;
}

/* free a d3dkmt local object handle */
static void free_object_handle( struct d3dkmt_object *object )
{
    unsigned int index;

    pthread_mutex_lock( &d3dkmt_lock );
    index = lookup_d3dkmt_object_pos( object->local );
    assert( index < object_count && objects[index] == object );
    object_count--;
    memmove( objects + index, objects + index + 1, (object_count - index) * sizeof(*objects) );
    pthread_mutex_unlock( &d3dkmt_lock );
}

/* return a pointer to a d3dkmt object from its local handle */
static void *open_d3dkmt_object( D3DKMT_HANDLE local, enum d3dkmt_type type )
{
    struct d3dkmt_object *object;
    unsigned int index;

    pthread_mutex_lock( &d3dkmt_lock );
    index = lookup_d3dkmt_object_pos( local );
    if (index >= object_count) object = NULL;
    else object = objects[index];
    pthread_mutex_unlock( &d3dkmt_lock );

    if (!object || object->local != local || (type != -1 && object->type != type)) return NULL;
    return object;
}

static NTSTATUS d3dkmt_object_alloc( UINT size, enum d3dkmt_type type, D3DKMT_HANDLE global, HANDLE handle,
                                     const void *private, SIZE_T private_size, void **obj )
{
    struct d3dkmt_object *object;

    if (!(object = calloc( 1, size )) || !alloc_object_handle( object ))
    {
        free( object );
        return STATUS_NO_MEMORY;
    }
    object->type = type;
    object->global = global;
    object->handle = handle;

    object->private_size = private_size;
    if (private_size) memcpy( object->private, private, private_size );

    *obj = object;
    return STATUS_SUCCESS;
}

static NTSTATUS d3dkmt_object_create( UINT size, enum d3dkmt_type type, BOOL shared,
                                      const void *private, SIZE_T private_size, void **obj )
{
    D3DKMT_HANDLE global = 0;
    HANDLE handle = 0;
    UINT status;

    if (!shared) status = STATUS_SUCCESS;
    else SERVER_START_REQ(create_d3dkmt_object)
    {
        req->type = type;
        wine_server_add_data( req, private, private_size );
        status = wine_server_call( req );
        handle = wine_server_ptr_handle( reply->handle );
        global = reply->global;
    }
    SERVER_END_REQ;
    if (status) return status;

    status = d3dkmt_object_alloc( size, type, global, handle, private, private_size, obj );
    if (status) NtClose( handle );
    return status;
}

static NTSTATUS d3dkmt_object_open_global( UINT size, enum d3dkmt_type type, D3DKMT_HANDLE global,
                                           void *private, UINT *private_size, void **obj )
{
    char buffer[1024];
    HANDLE handle;
    UINT status;

    if (!private && !*private_size)
    {
        private = buffer;
        *private_size = sizeof(buffer);
    }

    SERVER_START_REQ(open_d3dkmt_object)
    {
        req->type = type;
        wine_server_set_reply( req, private, *private_size );
        req->global = global;
        status = wine_server_call( req );
        handle = wine_server_ptr_handle( reply->handle );
        *private_size = reply->private_size;
    }
    SERVER_END_REQ;
    if (status) return status;

    if (!obj) NtClose( handle );
    else
    {
        status = d3dkmt_object_alloc( size, type, global, handle, private, *private_size, obj );
        if (status) NtClose( handle );
    }

    return status;
}

static NTSTATUS d3dkmt_object_open_shared( UINT size, enum d3dkmt_type type, HANDLE shared,
                                           void *private, UINT *private_size, void **obj )
{
    D3DKMT_HANDLE global;
    char buffer[1024];
    HANDLE handle;
    UINT status;

    if (!private && !*private_size)
    {
        private = buffer;
        *private_size = sizeof(buffer);
    }

    SERVER_START_REQ(get_shared_d3dkmt_object)
    {
        req->type = type;
        req->handle = wine_server_obj_handle( shared );
        wine_server_set_reply( req, private, *private_size );
        status = wine_server_call( req );
        handle = wine_server_ptr_handle( reply->handle );
        global = reply->global;
        *private_size = reply->private_size;
    }
    SERVER_END_REQ;
    if (status) return status;

    if (!obj) NtClose( handle );
    else
    {
        status = d3dkmt_object_alloc( size, type, global, handle, private, *private_size, obj );
        if (status) NtClose( handle );
    }

    return status;
}

static void d3dkmt_object_destroy( struct d3dkmt_object *object )
{
    free_object_handle( object );
    if (object->handle) NtClose( object->handle );
    free( object );
}

static VkInstance d3dkmt_vk_instance; /* Vulkan instance for D3DKMT functions */
static PFN_vkGetPhysicalDeviceMemoryProperties2KHR pvkGetPhysicalDeviceMemoryProperties2KHR;
static PFN_vkGetPhysicalDeviceMemoryProperties pvkGetPhysicalDeviceMemoryProperties;
static PFN_vkGetPhysicalDeviceProperties2KHR pvkGetPhysicalDeviceProperties2KHR;
static PFN_vkEnumeratePhysicalDevices pvkEnumeratePhysicalDevices;

static void d3dkmt_init_vulkan(void)
{
    static const char *extensions[] =
    {
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
    };
    VkInstanceCreateInfo create_info =
    {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .enabledExtensionCount = ARRAY_SIZE( extensions ),
        .ppEnabledExtensionNames = extensions,
    };
    PFN_vkDestroyInstance p_vkDestroyInstance;
    PFN_vkCreateInstance p_vkCreateInstance;
    VkResult vr;

    if (!vulkan_init())
    {
        WARN( "Failed to open the Vulkan driver\n" );
        return;
    }

    p_vkCreateInstance = (PFN_vkCreateInstance)p_vkGetInstanceProcAddr( NULL, "vkCreateInstance" );
    if ((vr = p_vkCreateInstance( &create_info, NULL, &d3dkmt_vk_instance )))
    {
        WARN( "Failed to create a Vulkan instance, vr %d.\n", vr );
        return;
    }

    p_vkDestroyInstance = (PFN_vkDestroyInstance)p_vkGetInstanceProcAddr( d3dkmt_vk_instance, "vkDestroyInstance" );
#define LOAD_VK_FUNC( f )                                                                      \
    if (!(p##f = (void *)p_vkGetInstanceProcAddr( d3dkmt_vk_instance, #f )))                   \
    {                                                                                          \
        WARN( "Failed to load " #f ".\n" );                                                    \
        p_vkDestroyInstance( d3dkmt_vk_instance, NULL );                                       \
        d3dkmt_vk_instance = NULL;                                                             \
        return;                                                                                \
    }
    LOAD_VK_FUNC( vkEnumeratePhysicalDevices )
    LOAD_VK_FUNC( vkGetPhysicalDeviceProperties2KHR )
    LOAD_VK_FUNC( vkGetPhysicalDeviceMemoryProperties )
    LOAD_VK_FUNC( vkGetPhysicalDeviceMemoryProperties2KHR )
#undef LOAD_VK_FUNC
}

static BOOL d3dkmt_use_vulkan(void)
{
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once( &once, d3dkmt_init_vulkan );
    return !!d3dkmt_vk_instance;
}

/* d3dkmt_lock must be held */
static struct d3dkmt_adapter *find_adapter_from_handle( D3DKMT_HANDLE handle )
{
    struct d3dkmt_adapter *adapter;
    LIST_FOR_EACH_ENTRY( adapter, &d3dkmt_adapters, struct d3dkmt_adapter, entry )
        if (adapter->handle == handle) return adapter;
    return NULL;
}

/******************************************************************************
 *           NtGdiDdDDIOpenAdapterFromHdc    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIOpenAdapterFromHdc( D3DKMT_OPENADAPTERFROMHDC *desc )
{
    FIXME( "(%p): stub\n", desc );
    return STATUS_NO_MEMORY;
}

/******************************************************************************
 *           NtGdiDdDDIEscape    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIEscape( const D3DKMT_ESCAPE *desc )
{
    FIXME( "(%p): stub\n", desc );
    return STATUS_NO_MEMORY;
}

/******************************************************************************
 *           NtGdiDdDDICloseAdapter    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDICloseAdapter( const D3DKMT_CLOSEADAPTER *desc )
{
    NTSTATUS status = STATUS_INVALID_PARAMETER;
    struct d3dkmt_adapter *adapter;

    TRACE( "(%p)\n", desc );

    if (!desc || !desc->hAdapter) return STATUS_INVALID_PARAMETER;

    pthread_mutex_lock( &d3dkmt_lock );
    if ((adapter = find_adapter_from_handle( desc->hAdapter )))
    {
        list_remove( &adapter->entry );
        status = STATUS_SUCCESS;
    }
    pthread_mutex_unlock( &d3dkmt_lock );

    free( adapter );
    return status;
}

/******************************************************************************
 *           NtGdiDdDDIOpenAdapterFromDeviceName    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIOpenAdapterFromDeviceName( D3DKMT_OPENADAPTERFROMDEVICENAME *desc )
{
    D3DKMT_OPENADAPTERFROMLUID desc_luid;
    NTSTATUS status;

    FIXME( "desc %p stub.\n", desc );

    if (!desc || !desc->pDeviceName) return STATUS_INVALID_PARAMETER;

    memset( &desc_luid, 0, sizeof(desc_luid) );
    if ((status = NtGdiDdDDIOpenAdapterFromLuid( &desc_luid ))) return status;

    desc->AdapterLuid = desc_luid.AdapterLuid;
    desc->hAdapter = desc_luid.hAdapter;
    return STATUS_SUCCESS;
}

static UINT get_vulkan_physical_devices( VkPhysicalDevice **devices )
{
    UINT device_count;
    VkResult vr;

    if ((vr = pvkEnumeratePhysicalDevices( d3dkmt_vk_instance, &device_count, NULL )))
    {
        WARN( "vkEnumeratePhysicalDevices returned %d\n", vr );
        return 0;
    }

    if (!device_count || !(*devices = malloc( device_count * sizeof(**devices) ))) return 0;

    if ((vr = pvkEnumeratePhysicalDevices( d3dkmt_vk_instance, &device_count, *devices )))
    {
        WARN( "vkEnumeratePhysicalDevices returned %d\n", vr );
        free( *devices );
        return 0;
    }

    return device_count;
}

static VkPhysicalDevice get_vulkan_physical_device( const GUID *uuid )
{
    VkPhysicalDevice *devices, device;
    UINT device_count, i;

    if (!(device_count = get_vulkan_physical_devices( &devices ))) return VK_NULL_HANDLE;

    for (i = 0, device = VK_NULL_HANDLE; i < device_count; ++i)
    {
        VkPhysicalDeviceIDProperties id = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
        VkPhysicalDeviceProperties2 properties2 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &id};

        pvkGetPhysicalDeviceProperties2KHR( devices[i], &properties2 );
        if (IsEqualGUID( uuid, id.deviceUUID ))
        {
            device = devices[i];
            break;
        }
    }

    free( devices );
    return device;
}

/******************************************************************************
 *           NtGdiDdDDIOpenAdapterFromLuid    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIOpenAdapterFromLuid( D3DKMT_OPENADAPTERFROMLUID *desc )
{
    static D3DKMT_HANDLE handle_start = 0;
    struct d3dkmt_adapter *adapter;
    GUID uuid = {0};

    if (!(adapter = calloc( 1, sizeof(*adapter) ))) return STATUS_NO_MEMORY;

    if (!d3dkmt_use_vulkan())
        WARN( "Vulkan is unavailable.\n" );
    else if (!get_vulkan_uuid_from_luid( &desc->AdapterLuid, &uuid ))
        WARN( "Failed to find Vulkan device with LUID %08x:%08x.\n",
              (int)desc->AdapterLuid.HighPart, (int)desc->AdapterLuid.LowPart );
    else if (!(adapter->vk_device = get_vulkan_physical_device( &uuid )))
        WARN( "Failed to find vulkan device with GUID %s\n", debugstr_guid( &uuid ) );

    pthread_mutex_lock( &d3dkmt_lock );
    desc->hAdapter = adapter->handle = ++handle_start;
    list_add_tail( &d3dkmt_adapters, &adapter->entry );
    pthread_mutex_unlock( &d3dkmt_lock );

    return STATUS_SUCCESS;
}

/******************************************************************************
 *           NtGdiDdDDICreateDevice    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDICreateDevice( D3DKMT_CREATEDEVICE *desc )
{
    static D3DKMT_HANDLE handle_start = 0;
    struct d3dkmt_device *device;
    BOOL found = FALSE;

    TRACE( "(%p)\n", desc );

    if (!desc) return STATUS_INVALID_PARAMETER;

    pthread_mutex_lock( &d3dkmt_lock );
    found = !!find_adapter_from_handle( desc->hAdapter );
    pthread_mutex_unlock( &d3dkmt_lock );

    if (!found) return STATUS_INVALID_PARAMETER;

    if (desc->Flags.LegacyMode || desc->Flags.RequestVSync || desc->Flags.DisableGpuTimeout)
        FIXME( "Flags unsupported.\n" );

    device = calloc( 1, sizeof(*device) );
    if (!device) return STATUS_NO_MEMORY;

    pthread_mutex_lock( &d3dkmt_lock );
    device->handle = ++handle_start;
    list_add_tail( &d3dkmt_devices, &device->entry );
    pthread_mutex_unlock( &d3dkmt_lock );

    desc->hDevice = device->handle;
    return STATUS_SUCCESS;
}

/******************************************************************************
 *           NtGdiDdDDIDestroyDevice    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIDestroyDevice( const D3DKMT_DESTROYDEVICE *desc )
{
    D3DKMT_SETVIDPNSOURCEOWNER set_owner_desc = {0};
    struct d3dkmt_device *device, *found = NULL;

    TRACE( "(%p)\n", desc );

    if (!desc || !desc->hDevice) return STATUS_INVALID_PARAMETER;

    pthread_mutex_lock( &d3dkmt_lock );
    LIST_FOR_EACH_ENTRY( device, &d3dkmt_devices, struct d3dkmt_device, entry )
    {
        if (device->handle == desc->hDevice)
        {
            list_remove( &device->entry );
            found = device;
            break;
        }
    }
    pthread_mutex_unlock( &d3dkmt_lock );

    if (!found) return STATUS_INVALID_PARAMETER;

    set_owner_desc.hDevice = desc->hDevice;
    NtGdiDdDDISetVidPnSourceOwner( &set_owner_desc );
    free( found );
    return STATUS_SUCCESS;
}

/******************************************************************************
 *           NtGdiDdDDIQueryAdapterInfo    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIQueryAdapterInfo( D3DKMT_QUERYADAPTERINFO *desc )
{
    TRACE( "(%p).\n", desc );

    if (!desc || !desc->hAdapter || !desc->pPrivateDriverData)
        return STATUS_INVALID_PARAMETER;

    switch (desc->Type)
    {
    case KMTQAITYPE_CHECKDRIVERUPDATESTATUS:
    {
        BOOL *value = desc->pPrivateDriverData;

        if (desc->PrivateDriverDataSize < sizeof(*value))
            return STATUS_INVALID_PARAMETER;

        *value = FALSE;
        return STATUS_SUCCESS;
    }
    case KMTQAITYPE_DRIVERVERSION:
    {
        D3DKMT_DRIVERVERSION *value = desc->pPrivateDriverData;

        if (desc->PrivateDriverDataSize < sizeof(*value))
            return STATUS_INVALID_PARAMETER;

        *value = KMT_DRIVERVERSION_WDDM_1_3;
        return STATUS_SUCCESS;
    }
    default:
    {
        FIXME( "type %d not handled.\n", desc->Type );
        return STATUS_NOT_IMPLEMENTED;
    }
    }
}

/******************************************************************************
 *           NtGdiDdDDIQueryStatistics    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIQueryStatistics( D3DKMT_QUERYSTATISTICS *stats )
{
    FIXME( "(%p): stub\n", stats );
    return STATUS_SUCCESS;
}

/******************************************************************************
 *           NtGdiDdDDIQueryVideoMemoryInfo    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIQueryVideoMemoryInfo( D3DKMT_QUERYVIDEOMEMORYINFO *desc )
{
    VkPhysicalDeviceMemoryBudgetPropertiesEXT budget;
    VkPhysicalDeviceMemoryProperties2 properties2;
    struct d3dkmt_adapter *adapter;
    OBJECT_BASIC_INFORMATION info;
    NTSTATUS status;
    unsigned int i;

    TRACE( "(%p)\n", desc );

    if (!desc || !desc->hAdapter ||
        (desc->MemorySegmentGroup != D3DKMT_MEMORY_SEGMENT_GROUP_LOCAL &&
         desc->MemorySegmentGroup != D3DKMT_MEMORY_SEGMENT_GROUP_NON_LOCAL))
        return STATUS_INVALID_PARAMETER;

    /* FIXME: Wine currently doesn't support linked adapters */
    if (desc->PhysicalAdapterIndex > 0) return STATUS_INVALID_PARAMETER;

    status = NtQueryObject( desc->hProcess ? desc->hProcess : GetCurrentProcess(),
                            ObjectBasicInformation, &info, sizeof(info), NULL );
    if (status != STATUS_SUCCESS) return status;
    if (!(info.GrantedAccess & PROCESS_QUERY_INFORMATION)) return STATUS_ACCESS_DENIED;

    desc->Budget = 0;
    desc->CurrentUsage = 0;
    desc->CurrentReservation = 0;
    desc->AvailableForReservation = 0;

    pthread_mutex_lock( &d3dkmt_lock );
    if ((adapter = find_adapter_from_handle( desc->hAdapter )) && adapter->vk_device)
    {
        memset( &budget, 0, sizeof(budget) );
        budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
        properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
        properties2.pNext = &budget;
        pvkGetPhysicalDeviceMemoryProperties2KHR( adapter->vk_device, &properties2 );
        for (i = 0; i < properties2.memoryProperties.memoryHeapCount; ++i)
        {
            if ((desc->MemorySegmentGroup == D3DKMT_MEMORY_SEGMENT_GROUP_LOCAL &&
                 properties2.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) ||
                (desc->MemorySegmentGroup == D3DKMT_MEMORY_SEGMENT_GROUP_NON_LOCAL &&
                 !(properties2.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)))
            {
                desc->Budget += budget.heapBudget[i];
                desc->CurrentUsage += budget.heapUsage[i];
            }
        }
        desc->AvailableForReservation = desc->Budget / 2;
    }
    pthread_mutex_unlock( &d3dkmt_lock );

    return adapter ? STATUS_SUCCESS : STATUS_INVALID_PARAMETER;
}

/******************************************************************************
 *           NtGdiDdDDISetQueuedLimit    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDISetQueuedLimit( D3DKMT_SETQUEUEDLIMIT *desc )
{
    FIXME( "(%p): stub\n", desc );
    return STATUS_NOT_IMPLEMENTED;
}

/******************************************************************************
 *           NtGdiDdDDISetVidPnSourceOwner    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDISetVidPnSourceOwner( const D3DKMT_SETVIDPNSOURCEOWNER *desc )
{
    struct d3dkmt_vidpn_source *source, *source2;
    BOOL found;
    UINT i;

    TRACE( "(%p)\n", desc );

    if (!desc || !desc->hDevice || (desc->VidPnSourceCount && (!desc->pType || !desc->pVidPnSourceId)))
        return STATUS_INVALID_PARAMETER;

    pthread_mutex_lock( &d3dkmt_lock );

    /* Check parameters */
    for (i = 0; i < desc->VidPnSourceCount; ++i)
    {
        LIST_FOR_EACH_ENTRY( source, &d3dkmt_vidpn_sources, struct d3dkmt_vidpn_source, entry )
        {
            if (source->id == desc->pVidPnSourceId[i])
            {
                /* Same device */
                if (source->device == desc->hDevice)
                {
                    if ((source->type == D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE &&
                         (desc->pType[i] == D3DKMT_VIDPNSOURCEOWNER_SHARED ||
                          desc->pType[i] == D3DKMT_VIDPNSOURCEOWNER_EMULATED)) ||
                        (source->type == D3DKMT_VIDPNSOURCEOWNER_EMULATED &&
                         desc->pType[i] == D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE))
                    {
                        pthread_mutex_unlock( &d3dkmt_lock );
                        return STATUS_INVALID_PARAMETER;
                    }
                }
                /* Different devices */
                else
                {
                    if ((source->type == D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE || source->type == D3DKMT_VIDPNSOURCEOWNER_EMULATED) &&
                        (desc->pType[i] == D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE ||
                         desc->pType[i] == D3DKMT_VIDPNSOURCEOWNER_EMULATED))
                    {
                        pthread_mutex_unlock( &d3dkmt_lock );
                        return STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE;
                    }
                }
            }
        }

        /* On Windows, it seems that all video present sources are owned by DMM clients, so any attempt to set
         * D3DKMT_VIDPNSOURCEOWNER_SHARED come back STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE */
        if (desc->pType[i] == D3DKMT_VIDPNSOURCEOWNER_SHARED)
        {
            pthread_mutex_unlock( &d3dkmt_lock );
            return STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE;
        }

        /* FIXME: D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVEGDI unsupported */
        if (desc->pType[i] == D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVEGDI || desc->pType[i] > D3DKMT_VIDPNSOURCEOWNER_EMULATED)
        {
            pthread_mutex_unlock( &d3dkmt_lock );
            return STATUS_INVALID_PARAMETER;
        }
    }

    /* Remove owner */
    if (!desc->VidPnSourceCount && !desc->pType && !desc->pVidPnSourceId)
    {
        LIST_FOR_EACH_ENTRY_SAFE( source, source2, &d3dkmt_vidpn_sources, struct d3dkmt_vidpn_source, entry )
        {
            if (source->device == desc->hDevice)
            {
                list_remove( &source->entry );
                free( source );
            }
        }

        pthread_mutex_unlock( &d3dkmt_lock );
        return STATUS_SUCCESS;
    }

    /* Add owner */
    for (i = 0; i < desc->VidPnSourceCount; ++i)
    {
        found = FALSE;
        LIST_FOR_EACH_ENTRY( source, &d3dkmt_vidpn_sources, struct d3dkmt_vidpn_source, entry )
        {
            if (source->device == desc->hDevice && source->id == desc->pVidPnSourceId[i])
            {
                found = TRUE;
                break;
            }
        }

        if (found) source->type = desc->pType[i];
        else
        {
            source = malloc( sizeof(*source) );
            if (!source)
            {
                pthread_mutex_unlock( &d3dkmt_lock );
                return STATUS_NO_MEMORY;
            }

            source->id = desc->pVidPnSourceId[i];
            source->type = desc->pType[i];
            source->device = desc->hDevice;
            list_add_tail( &d3dkmt_vidpn_sources, &source->entry );
        }
    }

    pthread_mutex_unlock( &d3dkmt_lock );
    return STATUS_SUCCESS;
}

/******************************************************************************
 *           NtGdiDdDDICheckVidPnExclusiveOwnership    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDICheckVidPnExclusiveOwnership( const D3DKMT_CHECKVIDPNEXCLUSIVEOWNERSHIP *desc )
{
    struct d3dkmt_vidpn_source *source;

    TRACE( "(%p)\n", desc );

    if (!desc || !desc->hAdapter) return STATUS_INVALID_PARAMETER;

    pthread_mutex_lock( &d3dkmt_lock );

    LIST_FOR_EACH_ENTRY( source, &d3dkmt_vidpn_sources, struct d3dkmt_vidpn_source, entry )
    {
        if (source->id == desc->VidPnSourceId && source->type == D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE)
        {
            pthread_mutex_unlock( &d3dkmt_lock );
            return STATUS_GRAPHICS_PRESENT_OCCLUDED;
        }
    }

    pthread_mutex_unlock( &d3dkmt_lock );
    return STATUS_SUCCESS;
}

struct vk_physdev_info
{
    VkPhysicalDeviceProperties2 properties2;
    VkPhysicalDeviceIDProperties id;
    VkPhysicalDeviceMemoryProperties mem_properties;
};

static int compare_vulkan_physical_devices( const void *v1, const void *v2 )
{
    static const int device_type_rank[6] = { 100, 1, 0, 2, 3, 200 };
    const struct vk_physdev_info *d1 = v1, *d2 = v2;
    int rank1, rank2;

    rank1 = device_type_rank[ min( d1->properties2.properties.deviceType, ARRAY_SIZE(device_type_rank) - 1) ];
    rank2 = device_type_rank[ min( d2->properties2.properties.deviceType, ARRAY_SIZE(device_type_rank) - 1) ];
    if (rank1 != rank2) return rank1 - rank2;

    return memcmp( &d1->id.deviceUUID, &d2->id.deviceUUID, sizeof(d1->id.deviceUUID) );
}

BOOL get_vulkan_gpus( struct list *gpus )
{
    struct vk_physdev_info *devinfo;
    VkPhysicalDevice *devices;
    UINT i, j, count;

    if (!d3dkmt_use_vulkan()) return FALSE;
    if (!(count = get_vulkan_physical_devices( &devices ))) return FALSE;

    if (!(devinfo = calloc( count, sizeof(*devinfo) )))
    {
        free( devices );
        return FALSE;
    }
    for (i = 0; i < count; ++i)
    {
        devinfo[i].id.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
        devinfo[i].properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        devinfo[i].properties2.pNext = &devinfo[i].id;
        pvkGetPhysicalDeviceProperties2KHR( devices[i], &devinfo[i].properties2 );
        pvkGetPhysicalDeviceMemoryProperties( devices[i], &devinfo[i].mem_properties );
    }
    qsort( devinfo, count, sizeof(*devinfo), compare_vulkan_physical_devices );

    for (i = 0; i < count; ++i)
    {
        struct vulkan_gpu *gpu;

        if (!(gpu = calloc( 1, sizeof(*gpu) ))) break;
        memcpy( &gpu->uuid, devinfo[i].id.deviceUUID, sizeof(gpu->uuid) );
        gpu->name = strdup( devinfo[i].properties2.properties.deviceName );
        gpu->pci_id.vendor = devinfo[i].properties2.properties.vendorID;
        gpu->pci_id.device = devinfo[i].properties2.properties.deviceID;

        for (j = 0; j < devinfo[i].mem_properties.memoryHeapCount; j++)
        {
            if (devinfo[i].mem_properties.memoryHeaps[j].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                gpu->memory += devinfo[i].mem_properties.memoryHeaps[j].size;
        }

        list_add_tail( gpus, &gpu->entry );
    }

    free( devinfo );
    free( devices );
    return TRUE;
}

void free_vulkan_gpu( struct vulkan_gpu *gpu )
{
    free( gpu->name );
    free( gpu );
}

/******************************************************************************
 *           NtGdiDdDDIShareObjects    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIShareObjects( UINT count, const D3DKMT_HANDLE *handles, OBJECT_ATTRIBUTES *attr,
                                        UINT access, HANDLE *handle )
{
    D3DKMT_HANDLE resource = 0, keyed_mutex = 0, sync_object = 0;
    struct object_attributes *objattr;
    struct d3dkmt_object *object;
    data_size_t len;
    UINT status;

    FIXME( "count %u, handles %p, attr %p, access %#x, handle %p stub!\n", count, handles, attr, access, handle );

    if (count == 1)
    {
        if (!(object = open_d3dkmt_object( handles[0], -1 ))) goto failed;
        if (object->type == D3DKMT_TYPE_RESOURCE) resource = object->global;
        else if (object->type == D3DKMT_TYPE_SYNC_OBJECT) sync_object = object->global;
        else goto failed;
    }
    else if (count == 3)
    {
        if (!(object = open_d3dkmt_object( handles[0], -1 ))) goto failed;
        if (object->type == D3DKMT_TYPE_RESOURCE) resource = object->global;
        else if (object->type == D3DKMT_TYPE_KEYED_MUTEX) keyed_mutex = object->global;
        else if (object->type == D3DKMT_TYPE_SYNC_OBJECT) sync_object = object->global;
        else goto failed;

        if (!(object = open_d3dkmt_object( handles[1], -1 ))) goto failed;
        if (object->type == D3DKMT_TYPE_RESOURCE) resource = object->global;
        else if (object->type == D3DKMT_TYPE_KEYED_MUTEX) keyed_mutex = object->global;
        else if (object->type == D3DKMT_TYPE_SYNC_OBJECT) sync_object = object->global;
        else goto failed;

        if (!(object = open_d3dkmt_object( handles[2], -1 ))) goto failed;
        if (object->type == D3DKMT_TYPE_RESOURCE) resource = object->global;
        else if (object->type == D3DKMT_TYPE_KEYED_MUTEX) keyed_mutex = object->global;
        else if (object->type == D3DKMT_TYPE_SYNC_OBJECT) sync_object = object->global;
        else goto failed;

        if (!resource || !keyed_mutex || !sync_object) goto failed;
    }
    else goto failed;

    if ((status = wine_server_object_attr( attr, &objattr, &len ))) return status;

    SERVER_START_REQ( share_d3dkmt_objects )
    {
        req->access = access;
        req->resource = resource;
        req->keyed_mutex = keyed_mutex;
        req->sync_object = sync_object;
        wine_server_add_data( req, objattr, len );
        status = wine_server_call( req );
        *handle = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;

    free( objattr );
    return status;

failed:
    FIXME( "Unsupported object count / types / handles\n" );
    return STATUS_INVALID_PARAMETER;
}


/******************************************************************************
 *           NtGdiDdDDICreateAllocation2    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDICreateAllocation2( D3DKMT_CREATEALLOCATION *params )
{
    D3DKMT_CREATESTANDARDALLOCATION *standard;
    struct d3dkmt_resource *resource = NULL;
    D3DDDI_ALLOCATIONINFO *alloc_info;
    struct d3dkmt_object *allocation;
    UINT status = STATUS_SUCCESS;
    BOOL global_shared;

    FIXME( "params %p semi-stub!\n", params );

    if (!params) return STATUS_INVALID_PARAMETER;
    if (!params->hDevice) return STATUS_INVALID_PARAMETER;
    if (!params->Flags.StandardAllocation) return STATUS_INVALID_PARAMETER;
    if (params->PrivateDriverDataSize) return STATUS_INVALID_PARAMETER;

    if (params->NumAllocations != 1) return STATUS_INVALID_PARAMETER;
    if (!(alloc_info = params->pAllocationInfo)) return STATUS_INVALID_PARAMETER;

    if (!(standard = params->pStandardAllocation)) return STATUS_INVALID_PARAMETER;
    if (standard->Type != D3DKMT_STANDARDALLOCATIONTYPE_EXISTINGHEAP) return STATUS_INVALID_PARAMETER;
    if (standard->ExistingHeapData.Size & 0xfff) return STATUS_INVALID_PARAMETER;
    if (!params->Flags.ExistingSysMem) return STATUS_INVALID_PARAMETER;
    if (!alloc_info->pSystemMem) return STATUS_INVALID_PARAMETER;

    if (params->Flags.CreateResource)
    {
        global_shared = params->Flags.CreateShared && !params->Flags.NtSecuritySharing;
        if (params->hResource && !(resource = open_d3dkmt_object( params->hResource, D3DKMT_TYPE_RESOURCE )))
            return STATUS_INVALID_HANDLE;
        else if (!(status = d3dkmt_object_create( sizeof(*resource), D3DKMT_TYPE_RESOURCE, global_shared,
                                                  params->pPrivateRuntimeData, params->PrivateRuntimeDataSize, (void **)&resource )))
        {
            params->hGlobalShare = resource->obj.global;
            params->hResource = resource->obj.local;
        }
    }
    else
    {
        if (params->Flags.CreateShared) return STATUS_INVALID_PARAMETER;
        if (params->hResource && !(resource = open_d3dkmt_object( params->hResource, D3DKMT_TYPE_RESOURCE )))
            return STATUS_INVALID_HANDLE;
        if (params->hResource && resource) return STATUS_INVALID_PARAMETER;
        params->hGlobalShare = 0;
    }

    if (!(status = d3dkmt_object_create( sizeof(*allocation), D3DKMT_TYPE_RESOURCE, FALSE,
                                         NULL, 0, (void **)&allocation )))
    {
        if (resource) resource->allocation = allocation->local;
        alloc_info->hAllocation = allocation->local;
    }

    return status ? status : STATUS_SUCCESS;
}

/******************************************************************************
 *           NtGdiDdDDICreateAllocation    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDICreateAllocation( D3DKMT_CREATEALLOCATION *params )
{
    FIXME( "params %p stub!\n", params );
    return NtGdiDdDDICreateAllocation2( params );
}

/******************************************************************************
 *           NtGdiDdDDIDestroyAllocation2    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIDestroyAllocation2( const D3DKMT_DESTROYALLOCATION2 *params )
{
    struct d3dkmt_object *allocation;
    D3DKMT_HANDLE alloc_handle = 0;
    UINT i;

    FIXME( "params %p stub!\n", params );

    if (!params) return STATUS_INVALID_PARAMETER;
    if (params->AllocationCount && !params->phAllocationList) return STATUS_INVALID_PARAMETER;

    if (params->hResource)
    {
        struct d3dkmt_resource *resource;
        if (!(resource = open_d3dkmt_object( params->hResource, D3DKMT_TYPE_RESOURCE )))
            return STATUS_INVALID_PARAMETER;
        alloc_handle = resource->allocation;
        d3dkmt_object_destroy( &resource->obj );
    }

    for (i = 0; i < params->AllocationCount; i++)
    {
        if (!(allocation = open_d3dkmt_object( params->phAllocationList[i], D3DKMT_TYPE_RESOURCE )))
            return STATUS_INVALID_PARAMETER;
        d3dkmt_object_destroy( allocation );
    }

    if (alloc_handle && (allocation = open_d3dkmt_object( alloc_handle, D3DKMT_TYPE_RESOURCE )))
        d3dkmt_object_destroy( allocation );

    return STATUS_SUCCESS;
}

/******************************************************************************
 *           NtGdiDdDDIDestroyAllocation    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIDestroyAllocation( const D3DKMT_DESTROYALLOCATION *params )
{
    D3DKMT_DESTROYALLOCATION2 params2 = {0};

    FIXME( "params %p stub!\n", params );

    if (!params) return STATUS_INVALID_PARAMETER;

    params2.hDevice = params->hDevice;
    params2.hResource = params->hResource;
    params2.phAllocationList = params->phAllocationList;
    params2.AllocationCount = params->AllocationCount;
    return NtGdiDdDDIDestroyAllocation2( &params2 );
}

typedef struct _D3DKMT_CHECKSHAREDRESOURCEACCESS
{
    D3DKMT_HANDLE hResource;
    UINT ClientPid;
} D3DKMT_CHECKSHAREDRESOURCEACCESS;

/******************************************************************************
 *           NtGdiDdDDICheckSharedResourceAccess    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDICheckSharedResourceAccess( const D3DKMT_CHECKSHAREDRESOURCEACCESS *params )
{
    FIXME( "params %p stub!\n", params );
    return STATUS_NOT_IMPLEMENTED;
}

typedef struct _D3DKMT_CONFIGURESHAREDRESOURCE
{
    D3DKMT_HANDLE hDevice;
    D3DKMT_HANDLE hResource;
    BOOLEAN IsDwm;
    HANDLE hProcess;
    BOOLEAN AllowAccess;
} D3DKMT_CONFIGURESHAREDRESOURCE;

/******************************************************************************
 *           NtGdiDdDDIConfigureSharedResource    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIConfigureSharedResource( const D3DKMT_CONFIGURESHAREDRESOURCE *params )
{
    FIXME( "params %p stub!\n", params );
    return STATUS_NOT_IMPLEMENTED;
}

typedef struct _D3DDDI_GETRESOURCEPRESENTPRIVATEDRIVERDATA
{
    D3DKMT_HANDLE hResource;
    UINT PrivateDriverDataSize;
    void *pPrivateDriverData;
} D3DDDI_GETRESOURCEPRESENTPRIVATEDRIVERDATA;

/******************************************************************************
 *           NtGdiDdDDIGetResourcePresentPrivateDriverData    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIGetResourcePresentPrivateDriverData( D3DDDI_GETRESOURCEPRESENTPRIVATEDRIVERDATA *params )
{
    FIXME( "params %p stub!\n", params );
    params->PrivateDriverDataSize = 0;
    return STATUS_NOT_IMPLEMENTED;
}

typedef struct _D3DKMT_GETSHAREDRESOURCEADAPTERLUID
{
    D3DKMT_HANDLE hGlobalShare;
    HANDLE hNtHandle;
    LUID AdapterLuid;
} D3DKMT_GETSHAREDRESOURCEADAPTERLUID;

/******************************************************************************
 *           NtGdiDdDDIGetSharedResourceAdapterLuid    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIGetSharedResourceAdapterLuid( D3DKMT_GETSHAREDRESOURCEADAPTERLUID *params )
{
    FIXME( "params %p stub!\n", params );
    params->AdapterLuid.HighPart = 0;
    params->AdapterLuid.LowPart = 0;
    return STATUS_NOT_IMPLEMENTED;
}

/******************************************************************************
 *           NtGdiDdDDIOpenResource    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIOpenResource( D3DKMT_OPENRESOURCE *params )
{
    D3DDDI_OPENALLOCATIONINFO *alloc_info;
    struct d3dkmt_resource *resource;
    struct d3dkmt_object *allocation;
    UINT status;

    FIXME( "params %p stub!\n", params );

    if (!params) return STATUS_INVALID_PARAMETER;
    if (!params->hDevice) return STATUS_INVALID_PARAMETER;
    if (!params->NumAllocations) return STATUS_INVALID_PARAMETER;
    if (!(alloc_info = params->pOpenAllocationInfo)) return STATUS_INVALID_PARAMETER;

    if (!(status = d3dkmt_object_open_global( sizeof(*resource), D3DKMT_TYPE_RESOURCE, params->hGlobalShare,
                                              params->pPrivateRuntimeData, &params->PrivateRuntimeDataSize, (void **)&resource )))
    {
        if (!(status = d3dkmt_object_create( sizeof(*allocation), D3DKMT_TYPE_RESOURCE, FALSE,
                                             NULL, 0, (void **)&allocation )))
        {
            resource->allocation = allocation->local;
            alloc_info->hAllocation = allocation->local;
            alloc_info->PrivateDriverDataSize = 0;
        }

        params->hResource = resource->obj.local;
        params->TotalPrivateDriverDataBufferSize = 0;
    }

    return status;
}

/******************************************************************************
 *           NtGdiDdDDIOpenResource2    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIOpenResource2( D3DKMT_OPENRESOURCE *params )
{
    D3DDDI_OPENALLOCATIONINFO2 *alloc_info;
    struct d3dkmt_resource *resource;
    struct d3dkmt_object *allocation;
    UINT status;

    FIXME( "params %p stub!\n", params );

    if (!params) return STATUS_INVALID_PARAMETER;
    if (!params->hDevice) return STATUS_INVALID_PARAMETER;
    if (!params->NumAllocations) return STATUS_INVALID_PARAMETER;
    if (!(alloc_info = params->pOpenAllocationInfo2)) return STATUS_INVALID_PARAMETER;

    if (!(status = d3dkmt_object_open_global( sizeof(*resource), D3DKMT_TYPE_RESOURCE, params->hGlobalShare,
                                              params->pPrivateRuntimeData, &params->PrivateRuntimeDataSize, (void **)&resource )))
    {
        if (!(status = d3dkmt_object_create( sizeof(*allocation), D3DKMT_TYPE_RESOURCE, FALSE,
                                             NULL, 0, (void **)&allocation )))
        {
            resource->allocation = allocation->local;
            alloc_info->hAllocation = allocation->local;
            alloc_info->PrivateDriverDataSize = 0;
        }

        params->hResource = resource->obj.local;
        params->TotalPrivateDriverDataBufferSize = 0;
    }

    return status;
}

/******************************************************************************
 *           NtGdiDdDDIOpenResourceFromNtHandle    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIOpenResourceFromNtHandle( D3DKMT_OPENRESOURCEFROMNTHANDLE *params )
{
    struct d3dkmt_object *resource = NULL, *keyed_mutex = NULL, *sync_object = NULL;
    UINT status, dummy = 0;

    FIXME( "params %p stub!\n", params );

    if ((status = d3dkmt_object_open_shared( sizeof(*resource), D3DKMT_TYPE_RESOURCE, params->hNtHandle,
                                             params->pPrivateRuntimeData, &params->PrivateRuntimeDataSize, (void **)&resource )))
        goto failed;
    if ((status = d3dkmt_object_open_shared( sizeof(*keyed_mutex), D3DKMT_TYPE_KEYED_MUTEX, params->hNtHandle,
                                             params->pKeyedMutexPrivateRuntimeData, &params->KeyedMutexPrivateRuntimeDataSize, (void **)&keyed_mutex )))
        goto failed;
    if ((status = d3dkmt_object_open_shared( sizeof(*sync_object), D3DKMT_TYPE_SYNC_OBJECT, params->hNtHandle,
                                             NULL, &dummy, (void **)&sync_object )))
        goto failed;

    params->TotalPrivateDriverDataBufferSize = 0;
    params->hResource = resource->local;
    params->hKeyedMutex = keyed_mutex->local;
    params->hSyncObject = sync_object->local;
    return status;

failed:
    if (sync_object) d3dkmt_object_destroy( sync_object );
    if (keyed_mutex) d3dkmt_object_destroy( keyed_mutex );
    if (resource) d3dkmt_object_destroy( resource );
    return status;
}

/******************************************************************************
 *           NtGdiDdDDIQueryResourceInfo    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIQueryResourceInfo( D3DKMT_QUERYRESOURCEINFO *params )
{
    UINT status;

    FIXME( "params %p stub!\n", params );

    if (!params) return STATUS_INVALID_PARAMETER;
    if (!params->hDevice) return STATUS_INVALID_PARAMETER;

    if ((status = d3dkmt_object_open_global( 0, D3DKMT_TYPE_RESOURCE, params->hGlobalShare,
                                             params->pPrivateRuntimeData, &params->PrivateRuntimeDataSize, NULL )))
        return status;
    params->TotalPrivateDriverDataSize = 96;
    params->NumAllocations = 1;

    return STATUS_SUCCESS;
}

/******************************************************************************
 *           NtGdiDdDDIQueryResourceInfoFromNtHandle    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIQueryResourceInfoFromNtHandle( D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE *params )
{
    FIXME( "params %p stub!\n", params );
    return STATUS_NOT_IMPLEMENTED;
}

typedef struct _D3DDDICB_LOCKFLAGS
{
    union
    {
        struct
        {
            UINT ReadOnly : 1;
            UINT WriteOnly : 1;
            UINT DonotWait : 1;
            UINT IgnoreSync : 1;
            UINT LockEntire : 1;
            UINT DonotEvict : 1;
            UINT AcquireAperture : 1;
            UINT Discard : 1;
            UINT NoExistingReference : 1;
            UINT UseAlternateVA : 1;
            UINT IgnoreReadSync : 1;
            UINT Reserved : 21;
        };
        UINT Value;
    };
} D3DDDICB_LOCKFLAGS;

typedef struct _D3DKMT_LOCK
{
    D3DKMT_HANDLE hDevice;
    D3DKMT_HANDLE hAllocation;
    UINT PrivateDriverData;
    UINT NumPages;
    const UINT *pPages;
    VOID *pData;
    D3DDDICB_LOCKFLAGS Flags;
    D3DGPU_VIRTUAL_ADDRESS GpuVirtualAddress;
} D3DKMT_LOCK;

typedef struct _D3DKMT_UNLOCK
{
    D3DKMT_HANDLE hDevice;
    UINT NumAllocations;
    const D3DKMT_HANDLE *phAllocations;
} D3DKMT_UNLOCK;

NTSTATUS WINAPI NtGdiDdDDILock( D3DKMT_LOCK *params )
{
    FIXME( "params %p stub!\n", params );
    params->pData = NULL;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS WINAPI NtGdiDdDDIUnlock( const D3DKMT_UNLOCK *params )
{
    FIXME( "params %p stub!\n", params );
    return STATUS_NOT_IMPLEMENTED;
}

typedef struct _D3DDDICB_LOCK2FLAGS
{
    union
    {
        struct
        {
            UINT Reserved : 32;
        };
        UINT Value;
    };
} D3DDDICB_LOCK2FLAGS;

typedef struct _D3DKMT_LOCK2
{
    D3DKMT_HANDLE hDevice;
    D3DKMT_HANDLE hAllocation;
    D3DDDICB_LOCK2FLAGS Flags;
    void *pData;
} D3DKMT_LOCK2;

typedef struct _D3DKMT_UNLOCK2
{
    D3DKMT_HANDLE hDevice;
    D3DKMT_HANDLE hAllocation;
} D3DKMT_UNLOCK2;

NTSTATUS WINAPI NtGdiDdDDILock2( D3DKMT_LOCK2 *params )
{
    FIXME( "params %p stub!\n", params );
    params->pData = NULL;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS WINAPI NtGdiDdDDIUnlock2( const D3DKMT_UNLOCK2 *params )
{
    FIXME( "params %p stub!\n", params );
    return STATUS_NOT_IMPLEMENTED;
}


/******************************************************************************
 *           NtGdiDdDDICreateKeyedMutex2    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDICreateKeyedMutex2( D3DKMT_CREATEKEYEDMUTEX2 *params )
{
    struct d3dkmt_object *keyed_mutex;
    UINT status;

    FIXME( "params %p stub!\n", params );

    if (!params) return STATUS_INVALID_PARAMETER;

/* params->Flags.NtSecuritySharing */
    if (!(status = d3dkmt_object_create( sizeof(*keyed_mutex), D3DKMT_TYPE_KEYED_MUTEX, !params->Flags.NtSecuritySharing,
                                         params->pPrivateRuntimeData, params->PrivateRuntimeDataSize,
                                         (void **)&keyed_mutex )))
    {
        params->hSharedHandle = keyed_mutex->global;
        params->hKeyedMutex = keyed_mutex->local;
    }

    return status;
}

/******************************************************************************
 *           NtGdiDdDDICreateKeyedMutex    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDICreateKeyedMutex( D3DKMT_CREATEKEYEDMUTEX *params )
{
    D3DKMT_CREATEKEYEDMUTEX2 params2 = {0};
    UINT status;

    FIXME( "params %p stub!\n", params );

    if (!params) return STATUS_INVALID_PARAMETER;

    params2.InitialValue = params->InitialValue;
    status = NtGdiDdDDICreateKeyedMutex2( &params2 );
    params->hSharedHandle = params2.hSharedHandle;
    params->hKeyedMutex = params2.hKeyedMutex;
    return status;
}

/******************************************************************************
 *           NtGdiDdDDIDestroyKeyedMutex    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIDestroyKeyedMutex( const D3DKMT_DESTROYKEYEDMUTEX *params )
{
    struct d3dkmt_object *keyed_mutex;

    FIXME( "params %p stub!\n", params );

    if (!(keyed_mutex = open_d3dkmt_object( params->hKeyedMutex, D3DKMT_TYPE_KEYED_MUTEX )))
        return STATUS_INVALID_PARAMETER;
    d3dkmt_object_destroy( keyed_mutex );

    return STATUS_SUCCESS;
}

/******************************************************************************
 *           NtGdiDdDDIOpenKeyedMutex2    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIOpenKeyedMutex2( D3DKMT_OPENKEYEDMUTEX2 *params )
{
    struct d3dkmt_object *keyed_mutex;
    UINT status, private_size;

    FIXME( "params %p stub!\n", params );

    if (!params) return STATUS_INVALID_PARAMETER;
    if (!is_d3dkmt_global(params->hSharedHandle)) return STATUS_INVALID_PARAMETER;
    if (params->PrivateRuntimeDataSize && !params->pPrivateRuntimeData) return STATUS_INVALID_PARAMETER;

    private_size = params->PrivateRuntimeDataSize;
    if (!(status = d3dkmt_object_open_global( sizeof(*keyed_mutex), D3DKMT_TYPE_KEYED_MUTEX, params->hSharedHandle,
                                              params->pPrivateRuntimeData, &private_size, (void **)&keyed_mutex )))
        params->hKeyedMutex = keyed_mutex->local;

    return status;
}

/******************************************************************************
 *           NtGdiDdDDIOpenKeyedMutex    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIOpenKeyedMutex( D3DKMT_OPENKEYEDMUTEX *params )
{
    D3DKMT_OPENKEYEDMUTEX2 params2 = {0};
    UINT status;

    FIXME( "params %p stub!\n", params );

    if (!params) return STATUS_INVALID_PARAMETER;

    params2.hSharedHandle = params->hSharedHandle;
    status = NtGdiDdDDIOpenKeyedMutex2( &params2 );
    params->hKeyedMutex = params2.hKeyedMutex;
    return status;
}

/******************************************************************************
 *           NtGdiDdDDIOpenKeyedMutexFromNtHandle    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIOpenKeyedMutexFromNtHandle( D3DKMT_OPENKEYEDMUTEXFROMNTHANDLE *params )
{
    struct d3dkmt_object *keyed_mutex;
    UINT status;

    FIXME( "params %p stub!\n", params );

    if (!(status = d3dkmt_object_open_shared( sizeof(*keyed_mutex), D3DKMT_TYPE_KEYED_MUTEX, params->hNtHandle,
                                              params->pPrivateRuntimeData, &params->PrivateRuntimeDataSize, (void **)&keyed_mutex )))
        params->hKeyedMutex = keyed_mutex->local;
    return status;
}

/******************************************************************************
 *           NtGdiDdDDIAcquireKeyedMutex    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIAcquireKeyedMutex( D3DKMT_ACQUIREKEYEDMUTEX *params )
{
    FIXME( "params %p stub!\n", params );
    params->FenceValue = 0;
    return STATUS_NOT_IMPLEMENTED;
}

/******************************************************************************
 *           NtGdiDdDDIAcquireKeyedMutex2    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIAcquireKeyedMutex2( D3DKMT_ACQUIREKEYEDMUTEX2 *params )
{
    FIXME( "params %p stub!\n", params );
    params->FenceValue = 0;
    return STATUS_NOT_IMPLEMENTED;
}

/******************************************************************************
 *           NtGdiDdDDIReleaseKeyedMutex    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIReleaseKeyedMutex( D3DKMT_RELEASEKEYEDMUTEX *params )
{
    FIXME( "params %p stub!\n", params );
    return STATUS_NOT_IMPLEMENTED;
}

/******************************************************************************
 *           NtGdiDdDDIReleaseKeyedMutex2    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIReleaseKeyedMutex2( D3DKMT_RELEASEKEYEDMUTEX2 *params )
{
    FIXME( "params %p stub!\n", params );
    return STATUS_NOT_IMPLEMENTED;
}


/******************************************************************************
 *           NtGdiDdDDICreateSynchronizationObject2    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDICreateSynchronizationObject2( D3DKMT_CREATESYNCHRONIZATIONOBJECT2 *params )
{
    struct d3dkmt_object *sync_object;
    BOOL global_shared;
    UINT status;

    FIXME( "params %p stub!\n", params );

    if (!params) return STATUS_INVALID_PARAMETER;
    if (!params->hDevice) return STATUS_INVALID_PARAMETER;
    if (params->Info.Type < D3DDDI_SYNCHRONIZATION_MUTEX ||
        params->Info.Type > D3DDDI_MONITORED_FENCE)
    {
        FIXME( "Type %u not implemented!\n", params->Info.Type );
        return STATUS_INVALID_PARAMETER;
    }

    if (params->Info.Type == D3DDDI_CPU_NOTIFICATION && !params->Info.CPUNotification.Event)
        return STATUS_INVALID_HANDLE;
    if (params->Info.Flags.NtSecuritySharing && !params->Info.Flags.Shared)
        return STATUS_INVALID_PARAMETER;

    global_shared = params->Info.Flags.Shared && !params->Info.Flags.NtSecuritySharing;
    if (!(status = d3dkmt_object_create( sizeof(*sync_object), D3DKMT_TYPE_SYNC_OBJECT, global_shared,
                                         NULL, 0, (void **)&sync_object )))
    {
        if (global_shared) params->Info.SharedHandle = sync_object->global;
        params->hSyncObject = sync_object->local;
    }

    return status;
}

/******************************************************************************
 *           NtGdiDdDDICreateSynchronizationObject    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDICreateSynchronizationObject( D3DKMT_CREATESYNCHRONIZATIONOBJECT *params )
{
    D3DKMT_CREATESYNCHRONIZATIONOBJECT2 params2 = {0};
    UINT status;

    FIXME( "params %p stub!\n", params );

    if (!params) return STATUS_INVALID_PARAMETER;
    if (params->Info.Type != D3DDDI_SYNCHRONIZATION_MUTEX &&
        params->Info.Type != D3DDDI_SEMAPHORE)
        return STATUS_INVALID_PARAMETER;

    params2.hDevice = params->hDevice;
    params2.Info.Type = params->Info.Type;
    params2.Info.Flags.Shared = 1;
    memcpy( &params2.Info.Reserved, &params->Info.Reserved, sizeof(params->Info.Reserved) );
    status = NtGdiDdDDICreateSynchronizationObject2( &params2 );
    params->hSyncObject = params2.hSyncObject;
    return status;
}

/******************************************************************************
 *           NtGdiDdDDIOpenSyncObjectFromNtHandle2    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIOpenSyncObjectFromNtHandle2( D3DKMT_OPENSYNCOBJECTFROMNTHANDLE2 *params )
{
    struct d3dkmt_object *sync_object;
    UINT status, dummy = 0;

    FIXME( "params %p stub!\n", params );

    if (!(status = d3dkmt_object_open_shared( sizeof(*sync_object), D3DKMT_TYPE_SYNC_OBJECT, params->hNtHandle,
                                              NULL, &dummy, (void **)&sync_object )))
    {
        params->hSyncObject = sync_object->local;
        params->MonitoredFence.FenceValueCPUVirtualAddress = 0;
        params->MonitoredFence.FenceValueGPUVirtualAddress = 0;
    }

    return status;
}

/******************************************************************************
 *           NtGdiDdDDIOpenSyncObjectFromNtHandle    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIOpenSyncObjectFromNtHandle( D3DKMT_OPENSYNCOBJECTFROMNTHANDLE *params )
{
    D3DKMT_OPENSYNCOBJECTFROMNTHANDLE2 params2 = {0};
    UINT status;

    FIXME( "params %p stub!\n", params );

    if (!params) return STATUS_INVALID_PARAMETER;

    params2.hNtHandle = params->hNtHandle;
    status = NtGdiDdDDIOpenSyncObjectFromNtHandle2( &params2 );
    params->hSyncObject = params2.hSyncObject;
    return status;
}

/******************************************************************************
 *           NtGdiDdDDIOpenSyncObjectNtHandleFromName    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIOpenSyncObjectNtHandleFromName( D3DKMT_OPENSYNCOBJECTNTHANDLEFROMNAME *params )
{
    FIXME( "params %p stub!\n", params );
    return STATUS_NOT_IMPLEMENTED;
}

/******************************************************************************
 *           NtGdiDdDDIOpenSynchronizationObject    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIOpenSynchronizationObject( D3DKMT_OPENSYNCHRONIZATIONOBJECT *params )
{
    struct d3dkmt_object *sync_object;
    UINT status, dummy = 0;

    FIXME( "params %p stub!\n", params );

    if (!(status = d3dkmt_object_open_global( sizeof(*sync_object), D3DKMT_TYPE_SYNC_OBJECT, params->hSharedHandle,
                                              NULL, &dummy, (void **)&sync_object )))
        params->hSyncObject = sync_object->local;
    return status;
}

/******************************************************************************
 *           NtGdiDdDDIDestroySynchronizationObject    (win32u.@)
 */
NTSTATUS WINAPI NtGdiDdDDIDestroySynchronizationObject( const D3DKMT_DESTROYSYNCHRONIZATIONOBJECT *params )
{
    struct d3dkmt_object *sync_object;

    FIXME( "params %p stub!\n", params );

    if (!(sync_object = open_d3dkmt_object( params->hSyncObject, D3DKMT_TYPE_SYNC_OBJECT )))
        return STATUS_INVALID_PARAMETER;
    d3dkmt_object_destroy( sync_object );

    return STATUS_SUCCESS;
}

NTSTATUS d3dkmt_set_object_fd( D3DKMT_HANDLE local, const struct d3dkmt_desc *desc, int fd )
{
    struct d3dkmt_object *object;
    HANDLE handle = 0;
    UINT status;

    if (!(object = open_d3dkmt_object( local, -1 ))) return STATUS_INVALID_HANDLE;

    status = wine_server_fd_to_handle( fd, 0, OBJ_INHERIT, &handle );
    if (!status) SERVER_START_REQ( set_d3dkmt_object_fd )
    {
        req->type = object->type;
        req->global = object->global;
        req->handle = wine_server_obj_handle( handle );
        req->desc = *desc;
        status = wine_server_call( req );
    }
    SERVER_END_REQ;
    if (handle) NtClose( handle );

    if (status) ERR("got status %#x\n", status);
    return status;
}

NTSTATUS d3dkmt_get_object_fd( D3DKMT_HANDLE local, struct d3dkmt_desc *desc, int *fd )
{
    struct d3dkmt_object *object;
    HANDLE handle;
    UINT status;

    if (!(object = open_d3dkmt_object( local, -1 ))) return STATUS_INVALID_HANDLE;

    SERVER_START_REQ( get_d3dkmt_object_fd )
    {
        req->type = object->type;
        req->global = object->global;
        status = wine_server_call( req );
        handle = status ? 0 : wine_server_ptr_handle( reply->handle );
        *desc = reply->desc;
    }
    SERVER_END_REQ;
    if (!status) status = wine_server_handle_to_fd( handle, 0, fd, NULL );
    if (handle) NtClose( handle );

    if (status) ERR("got status %#x\n", status);
    return status;
}
