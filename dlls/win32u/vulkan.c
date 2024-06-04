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

void *(*p_vkGetDeviceProcAddr)(VkDevice, const char *) = NULL;
void *(*p_vkGetInstanceProcAddr)(VkInstance, const char *) = NULL;

static void *vulkan_handle;
static struct vulkan_funcs vulkan_funcs;

#ifdef SONAME_LIBVULKAN

static const struct vulkan_driver_funcs *driver_funcs;

/* list of surfaces attached to other processes / desktop windows */
static struct list offscreen_surfaces = LIST_INIT(offscreen_surfaces);
static pthread_mutex_t vulkan_mutex = PTHREAD_MUTEX_INITIALIZER;

static void (*p_vkDestroySurfaceKHR)(VkInstance, VkSurfaceKHR, const VkAllocationCallbacks *);
static VkResult (*p_vkQueuePresentKHR)(VkQueue, const VkPresentInfoKHR *);

static void (*p_vkGetPhysicalDeviceProperties2)(VkPhysicalDevice, VkPhysicalDeviceProperties2 *);
static void (*p_vkGetPhysicalDeviceProperties2KHR)(VkPhysicalDevice, VkPhysicalDeviceProperties2 *);

static const char *debugstr_luid( const LUID *luid )
{
    return wine_dbg_sprintf( "%08x:%08x", (UINT)luid->HighPart, (UINT)luid->LowPart );
}

struct surface
{
    struct list entry;
    VkSurfaceKHR host_surface;
    void *driver_private;
    HDC offscreen_dc;
    HRGN region;
    HWND hwnd;
};

static inline struct surface *surface_from_handle( VkSurfaceKHR handle )
{
    return (struct surface *)(uintptr_t)handle;
}

static inline VkSurfaceKHR surface_to_handle( struct surface *surface )
{
    return (VkSurfaceKHR)(uintptr_t)surface;
}

static VkResult win32u_vkCreateWin32SurfaceKHR( VkInstance instance, const VkWin32SurfaceCreateInfoKHR *info,
                                                const VkAllocationCallbacks *allocator, VkSurfaceKHR *handle )
{
    HWND toplevel = NtUserGetAncestor( info->hwnd, GA_ROOT );
    struct surface *surface;
    VkResult res;
    WND *win;

    TRACE( "instance %p, info %p, allocator %p, handle %p\n", instance, info, allocator, handle );
    if (allocator) FIXME( "Support for allocation callbacks not implemented yet\n" );

    if (!(surface = calloc( 1, sizeof(*surface) ))) return VK_ERROR_OUT_OF_HOST_MEMORY;
    if ((res = driver_funcs->p_vulkan_surface_create( info->hwnd, instance, &surface->host_surface, &surface->driver_private )))
    {
        free( surface );
        return res;
    }

    /* make sure the window has a pixel format selected to get consistent window surface updates */
    if (!win32u_get_window_pixel_format( info->hwnd )) win32u_set_window_pixel_format( info->hwnd, 1, TRUE );

    if (!(win = get_win_ptr( toplevel )) || win == WND_DESKTOP || win == WND_OTHER_PROCESS)
    {
        pthread_mutex_lock( &vulkan_mutex );
        list_add_tail( &offscreen_surfaces, &surface->entry );
        pthread_mutex_unlock( &vulkan_mutex );
        driver_funcs->p_vulkan_surface_detach( info->hwnd, surface->driver_private, &surface->offscreen_dc );
    }
    else
    {
        list_add_tail( &win->vulkan_surfaces, &surface->entry );
        release_win_ptr( win );
        if (toplevel != info->hwnd) driver_funcs->p_vulkan_surface_detach( info->hwnd, surface->driver_private, &surface->offscreen_dc );
    }

    surface->region = NtGdiCreateRectRgn( 0, 0, 0, 0 );
    surface->hwnd = info->hwnd;
    *handle = surface_to_handle( surface );
    return VK_SUCCESS;
}

static void win32u_vkDestroySurfaceKHR( VkInstance instance, VkSurfaceKHR handle, const VkAllocationCallbacks *allocator )
{
    struct surface *surface = surface_from_handle( handle );

    TRACE( "instance %p, handle 0x%s, allocator %p\n", instance, wine_dbgstr_longlong(handle), allocator );
    if (allocator) FIXME( "Support for allocation callbacks not implemented yet\n" );

    pthread_mutex_lock( &vulkan_mutex );
    list_remove( &surface->entry );
    pthread_mutex_unlock( &vulkan_mutex );

    if (surface->offscreen_dc) NtGdiDeleteObjectApp( surface->offscreen_dc );
    p_vkDestroySurfaceKHR( instance, surface->host_surface, NULL /* allocator */ );
    driver_funcs->p_vulkan_surface_destroy( surface->hwnd, surface->driver_private );
    NtGdiDeleteObjectApp( surface->region );
    free( surface );
}

static VkResult win32u_vkQueuePresentKHR( VkQueue queue, const VkPresentInfoKHR *present_info, VkSurfaceKHR *surfaces )
{
    VkResult res;
    UINT i;

    TRACE( "queue %p, present_info %p\n", queue, present_info );

    res = p_vkQueuePresentKHR( queue, present_info );

    for (i = 0; i < present_info->swapchainCount; i++)
    {
        VkResult swapchain_res = present_info->pResults ? present_info->pResults[i] : res;
        struct surface *surface = surface_from_handle( surfaces[i] );

        driver_funcs->p_vulkan_surface_presented( surface->hwnd, swapchain_res );

        if (swapchain_res >= VK_SUCCESS && surface->offscreen_dc)
        {
            UINT width, height;
            RECT client_rect;
            HDC hdc_dst;

            NtUserGetClientRect( surface->hwnd, &client_rect, get_thread_dpi() );
            width = client_rect.right - client_rect.left;
            height = client_rect.bottom - client_rect.top;

            WARN("Copying vulkan child window %p rect %s\n", surface->hwnd, wine_dbgstr_rect(&client_rect));

            if ((hdc_dst = NtUserGetDCEx(surface->hwnd, surface->region, DCX_USESTYLE | DCX_CACHE)))
            {
                NtGdiStretchBlt(hdc_dst, client_rect.left, client_rect.top, width, height,
                                surface->offscreen_dc, 0, 0, width, height, SRCCOPY, 0);
                NtUserReleaseDC(surface->hwnd, hdc_dst);
            }
        }
    }

    return res;
}

/* wait until graphics driver is loaded by explorer */
static void wait_graphics_driver_ready(void)
{
    static BOOL ready = FALSE;

    if (!ready)
    {
        send_message( get_desktop_window(), WM_NULL, 0, 0 );
        ready = TRUE;
    }
}

static void *find_vulkan_struct( void *chain, VkStructureType type )
{
    VkBaseOutStructure *header;
    for (header = chain; header; header = header->pNext)
        if (header->sType == type) return header;
    return NULL;
}

static void set_physical_device_luid( VkPhysicalDeviceProperties2 *properties2 )
{
    VkPhysicalDeviceVulkan11Properties *vk11;
    VkPhysicalDeviceIDProperties *id;
    GUID vulkan_uuid;
    LUID luid;

    wait_graphics_driver_ready();

    TRACE( "device name %s\n", properties2->properties.deviceName );

    if ((id = find_vulkan_struct( properties2, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES )))
    {
        memcpy( &vulkan_uuid, id->deviceUUID, sizeof(vulkan_uuid) );
        id->deviceLUIDValid = get_luid_from_vulkan_uuid( &vulkan_uuid, &luid );
        memcpy( id->deviceLUID, &luid, sizeof(luid) );
        id->deviceNodeMask = !!id->deviceLUIDValid;

        ERR( "id %p UUID %s LUID %s valid %d mask %u\n", id, debugstr_guid( &vulkan_uuid ),
               debugstr_luid( &luid ), id->deviceLUIDValid, id->deviceNodeMask );
    }

    if ((vk11 = find_vulkan_struct( properties2, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES )))
    {
        memcpy( &vulkan_uuid, id->deviceUUID, sizeof(vulkan_uuid) );
        vk11->deviceLUIDValid = get_luid_from_vulkan_uuid( &vulkan_uuid, &luid );
        memcpy( vk11->deviceLUID, &luid, sizeof(luid) );
        vk11->deviceNodeMask = !!vk11->deviceLUIDValid;

        ERR( "vk11 %p UUID %s LUID %s valid %d mask %u\n", vk11, debugstr_guid( &vulkan_uuid ),
               debugstr_luid( &luid ), vk11->deviceLUIDValid, vk11->deviceNodeMask );
    }
}

void win32u_vkGetPhysicalDeviceProperties2( VkPhysicalDevice phys_dev, VkPhysicalDeviceProperties2 *properties2 )
{
    TRACE( "%p, %p\n", phys_dev, properties2 );
    p_vkGetPhysicalDeviceProperties2( phys_dev, properties2 );
    set_physical_device_luid( properties2 );
}

void win32u_vkGetPhysicalDeviceProperties2KHR( VkPhysicalDevice phys_dev, VkPhysicalDeviceProperties2 *properties2 )
{
    TRACE( "%p, %p\n", phys_dev, properties2 );
    p_vkGetPhysicalDeviceProperties2KHR( phys_dev, properties2 );
    set_physical_device_luid( properties2 );
}

static void *win32u_vkGetDeviceProcAddr( VkDevice device, const char *name )
{
    TRACE( "device %p, name %s\n", device, debugstr_a(name) );

    if (!strcmp( name, "vkGetDeviceProcAddr" )) return vulkan_funcs.p_vkGetDeviceProcAddr;
    if (!strcmp( name, "vkQueuePresentKHR" )) return vulkan_funcs.p_vkQueuePresentKHR;

    return p_vkGetDeviceProcAddr( device, name );
}

static void *win32u_vkGetInstanceProcAddr( VkInstance instance, const char *name )
{
    TRACE( "instance %p, name %s\n", instance, debugstr_a(name) );

    if (!instance) return p_vkGetInstanceProcAddr( instance, name );

    if (!strcmp( name, "vkCreateWin32SurfaceKHR" )) return vulkan_funcs.p_vkCreateWin32SurfaceKHR;
    if (!strcmp( name, "vkDestroySurfaceKHR" )) return vulkan_funcs.p_vkDestroySurfaceKHR;
    if (!strcmp( name, "vkGetInstanceProcAddr" )) return vulkan_funcs.p_vkGetInstanceProcAddr;
    if (!strcmp( name, "vkGetPhysicalDeviceProperties2" ))
    {
        if (!p_vkGetPhysicalDeviceProperties2) return NULL;
        return vulkan_funcs.p_vkGetPhysicalDeviceProperties2;
    }
    if (!strcmp( name, "vkGetPhysicalDeviceProperties2KHR" ))
    {
        if (!p_vkGetPhysicalDeviceProperties2KHR) return NULL;
        return vulkan_funcs.p_vkGetPhysicalDeviceProperties2KHR;
    }
    if (!strcmp( name, "vkGetPhysicalDeviceWin32PresentationSupportKHR" )) return vulkan_funcs.p_vkGetPhysicalDeviceWin32PresentationSupportKHR;

    /* vkGetInstanceProcAddr also loads any children of instance, so device functions as well. */
    if (!strcmp( name, "vkGetDeviceProcAddr" )) return vulkan_funcs.p_vkGetDeviceProcAddr;
    if (!strcmp( name, "vkQueuePresentKHR" )) return vulkan_funcs.p_vkQueuePresentKHR;

    return p_vkGetInstanceProcAddr( instance, name );
}

static VkSurfaceKHR win32u_wine_get_host_surface( VkSurfaceKHR handle )
{
    struct surface *surface = surface_from_handle( handle );
    return surface->host_surface;
}

static struct vulkan_funcs vulkan_funcs =
{
    .p_vkCreateWin32SurfaceKHR = win32u_vkCreateWin32SurfaceKHR,
    .p_vkDestroySurfaceKHR = win32u_vkDestroySurfaceKHR,
    .p_vkQueuePresentKHR = win32u_vkQueuePresentKHR,
    .p_vkGetPhysicalDeviceProperties2 = win32u_vkGetPhysicalDeviceProperties2,
    .p_vkGetPhysicalDeviceProperties2KHR = win32u_vkGetPhysicalDeviceProperties2KHR,
    .p_vkGetDeviceProcAddr = win32u_vkGetDeviceProcAddr,
    .p_vkGetInstanceProcAddr = win32u_vkGetInstanceProcAddr,
    .p_wine_get_host_surface = win32u_wine_get_host_surface,
};

static VkResult nulldrv_vulkan_surface_create( HWND hwnd, VkInstance instance, VkSurfaceKHR *surface, void **private )
{
    FIXME( "stub!\n" );
    return VK_ERROR_INCOMPATIBLE_DRIVER;
}

static void nulldrv_vulkan_surface_destroy( HWND hwnd, void *private )
{
}

static void nulldrv_vulkan_surface_attach( HWND hwnd, void *private )
{
}

static void nulldrv_vulkan_surface_detach( HWND hwnd, void *private, HDC *hdc )
{
}

static void nulldrv_vulkan_surface_presented( HWND hwnd, VkResult result )
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
    .p_vulkan_surface_attach = nulldrv_vulkan_surface_attach,
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
    {
        vulkan_funcs.p_vkGetPhysicalDeviceWin32PresentationSupportKHR = driver_funcs->p_vkGetPhysicalDeviceWin32PresentationSupportKHR;
        vulkan_funcs.p_get_host_surface_extension = driver_funcs->p_get_host_surface_extension;
    }
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

static void lazydrv_vulkan_surface_attach( HWND hwnd, void *private )
{
    vulkan_driver_load();
    return driver_funcs->p_vulkan_surface_attach( hwnd, private );
}

static void lazydrv_vulkan_surface_detach( HWND hwnd, void *private, HDC *hdc )
{
    vulkan_driver_load();
    return driver_funcs->p_vulkan_surface_detach( hwnd, private, hdc );
}

static void lazydrv_vulkan_surface_presented( HWND hwnd, VkResult result )
{
    vulkan_driver_load();
    return driver_funcs->p_vulkan_surface_presented( hwnd, result );
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
    .p_vulkan_surface_attach = lazydrv_vulkan_surface_attach,
    .p_vulkan_surface_detach = lazydrv_vulkan_surface_detach,
    .p_vulkan_surface_presented = lazydrv_vulkan_surface_presented,
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

    LOAD_FUNCPTR( vkDestroySurfaceKHR );
    LOAD_FUNCPTR( vkQueuePresentKHR );
    LOAD_FUNCPTR( vkGetDeviceProcAddr );
    LOAD_FUNCPTR( vkGetInstanceProcAddr );
#undef LOAD_FUNCPTR

#define LOAD_FUNCPTR_OPT( f ) \
    if (!(p_##f = dlsym( vulkan_handle, #f ))) WARN( "Failed to find " #f "\n" );
    LOAD_FUNCPTR_OPT( vkGetPhysicalDeviceProperties2 );
    LOAD_FUNCPTR_OPT( vkGetPhysicalDeviceProperties2KHR );
    if (!p_vkGetPhysicalDeviceProperties2) p_vkGetPhysicalDeviceProperties2 = p_vkGetPhysicalDeviceProperties2KHR;
    if (!p_vkGetPhysicalDeviceProperties2KHR) p_vkGetPhysicalDeviceProperties2KHR = p_vkGetPhysicalDeviceProperties2;
#undef LOAD_FUNCPTR_OPT

    driver_funcs = &lazydrv_funcs;
    vulkan_funcs.p_vkGetPhysicalDeviceWin32PresentationSupportKHR = lazydrv_vkGetPhysicalDeviceWin32PresentationSupportKHR;
    vulkan_funcs.p_get_host_surface_extension = lazydrv_get_host_surface_extension;
}

void vulkan_detach_surfaces( struct list *surfaces )
{
    struct surface *surface;

    LIST_FOR_EACH_ENTRY( surface, surfaces, struct surface, entry )
    {
        if (surface->offscreen_dc) continue;
        driver_funcs->p_vulkan_surface_detach( surface->hwnd, surface->driver_private, &surface->offscreen_dc );
    }

    pthread_mutex_lock( &vulkan_mutex );
    list_move_tail( &offscreen_surfaces, surfaces );
    pthread_mutex_unlock( &vulkan_mutex );
}

static void append_window_surfaces( HWND toplevel, struct list *surfaces )
{
    WND *win;

    if (!(win = get_win_ptr( toplevel )) || win == WND_DESKTOP || win == WND_OTHER_PROCESS)
    {
        pthread_mutex_lock( &vulkan_mutex );
        list_move_tail( &offscreen_surfaces, surfaces );
        pthread_mutex_unlock( &vulkan_mutex );
    }
    else
    {
        list_move_tail( &win->vulkan_surfaces, surfaces );
        release_win_ptr( win );
    }
}

static void enum_window_surfaces( HWND toplevel, HWND hwnd, struct list *surfaces )
{
    struct list tmp_surfaces = LIST_INIT(tmp_surfaces);
    struct surface *surface, *next;
    WND *win;

    if (!(win = get_win_ptr( toplevel )) || win == WND_DESKTOP || win == WND_OTHER_PROCESS)
    {
        pthread_mutex_lock( &vulkan_mutex );
        list_move_tail( &tmp_surfaces, &offscreen_surfaces );
        pthread_mutex_unlock( &vulkan_mutex );
    }
    else
    {
        list_move_tail( &tmp_surfaces, &win->vulkan_surfaces );
        release_win_ptr( win );
    }

    LIST_FOR_EACH_ENTRY_SAFE( surface, next, &tmp_surfaces, struct surface, entry )
    {
        if (surface->hwnd != hwnd && !NtUserIsChild( hwnd, surface->hwnd )) continue;
        list_remove( &surface->entry );
        list_add_tail( surfaces, &surface->entry );
    }

    append_window_surfaces( toplevel, &tmp_surfaces );
}

void vulkan_set_parent( HWND hwnd, HWND new_parent, HWND old_parent )
{
    struct list surfaces = LIST_INIT(surfaces);
    HWND new_toplevel, old_toplevel;
    struct surface *surface;

    TRACE( "hwnd %p new_parent %p old_parent %p\n", hwnd, new_parent, old_parent );

    if (new_parent == NtUserGetDesktopWindow()) new_toplevel = hwnd;
    else new_toplevel = NtUserGetAncestor( new_parent, GA_ROOT );
    if (old_parent == NtUserGetDesktopWindow()) old_toplevel = hwnd;
    else old_toplevel = NtUserGetAncestor( old_parent, GA_ROOT );
    if (old_toplevel == new_toplevel) return;

    enum_window_surfaces( old_toplevel, hwnd, &surfaces );

    /* surfaces will be re-attached as needed from surface region updates */
    LIST_FOR_EACH_ENTRY( surface, &surfaces, struct surface, entry )
    {
        if (surface->offscreen_dc) continue;
        driver_funcs->p_vulkan_surface_detach( surface->hwnd, surface->driver_private, &surface->offscreen_dc );
    }

    append_window_surfaces( new_toplevel, &surfaces );
}

void vulkan_set_region( HWND toplevel, HRGN region )
{
    struct list surfaces = LIST_INIT(surfaces);
    struct surface *surface;

    enum_window_surfaces( toplevel, toplevel, &surfaces );

    LIST_FOR_EACH_ENTRY( surface, &surfaces, struct surface, entry )
    {
        RECT client_rect;
        BOOL is_clipped;

        NtUserGetClientRect( surface->hwnd, &client_rect, get_thread_dpi() );
        NtUserMapWindowPoints( surface->hwnd, toplevel, (POINT *)&client_rect, 2, get_thread_dpi() );
        is_clipped = NtGdiRectInRegion( region, &client_rect );

        if (is_clipped && !surface->offscreen_dc)
        {
            TRACE( "surface %p is now clipped\n", surface->hwnd );
            driver_funcs->p_vulkan_surface_detach( surface->hwnd, surface->driver_private, &surface->offscreen_dc );
            NtGdiCombineRgn( surface->region, region, 0, RGN_COPY );
        }
        else if (!is_clipped && surface->offscreen_dc)
        {
            TRACE( "surface %p is now unclipped\n", surface->hwnd );
            driver_funcs->p_vulkan_surface_attach( surface->hwnd, surface->driver_private );
            NtGdiDeleteObjectApp( surface->offscreen_dc );
            surface->offscreen_dc = NULL;
        }
    }

    append_window_surfaces( toplevel, &surfaces );
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

static VkInstance d3dkmt_instance; /* Vulkan instance for D3DKMT functions */
static PFN_vkGetPhysicalDeviceMemoryProperties2KHR p_vkGetPhysicalDeviceMemoryProperties2KHR;
static PFN_vkGetPhysicalDeviceMemoryProperties p_vkGetPhysicalDeviceMemoryProperties;
static PFN_vkGetPhysicalDeviceProperties2KHR p_vkGetPhysicalDeviceProperties2KHR;
static PFN_vkEnumeratePhysicalDevices p_vkEnumeratePhysicalDevices;

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
        ERR( "Failed to open the Vulkan driver\n" );
        return;
    }

    p_vkCreateInstance = p_vkGetInstanceProcAddr( NULL, "vkCreateInstance" );
    if ((vr = p_vkCreateInstance( &create_info, NULL, &d3dkmt_instance )))
    {
        ERR( "Failed to create a Vulkan instance, vr %d.\n", vr );
        d3dkmt_instance = NULL;
        return;
    }

    p_vkDestroyInstance = p_vkGetInstanceProcAddr( d3dkmt_instance, "vkDestroyInstance" );
#define LOAD_VK_FUNC( f )                                                                      \
    if (!(p_##f = (void *)p_vkGetInstanceProcAddr( d3dkmt_instance, #f )))                   \
    {                                                                                          \
        ERR( "Failed to load " #f ".\n" );                                                    \
        p_vkDestroyInstance( d3dkmt_instance, NULL );                                       \
        d3dkmt_instance = NULL;                                                                   \
        return;                                                                                \
    }
    LOAD_VK_FUNC( vkEnumeratePhysicalDevices )
    LOAD_VK_FUNC( vkGetPhysicalDeviceProperties2KHR )
    LOAD_VK_FUNC( vkGetPhysicalDeviceMemoryProperties )
    LOAD_VK_FUNC( vkGetPhysicalDeviceMemoryProperties2KHR )
#undef LOAD_VK_FUNC
}

BOOL d3dkmt_use_vulkan(void)
{
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once( &once, d3dkmt_init_vulkan );
    return !!d3dkmt_instance;
}

static UINT get_vulkan_physical_devices( VkPhysicalDevice **devices )
{
    UINT device_count;
    VkResult vr;

    if ((vr = p_vkEnumeratePhysicalDevices( d3dkmt_instance, &device_count, NULL )))
    {
        WARN( "vkEnumeratePhysicalDevices returned %d\n", vr );
        return 0;
    }

    if (!device_count || !(*devices = malloc( device_count * sizeof(**devices) ))) return 0;

    if ((vr = p_vkEnumeratePhysicalDevices( d3dkmt_instance, &device_count, *devices )))
    {
        WARN( "vkEnumeratePhysicalDevices returned %d\n", vr );
        free( *devices );
        return 0;
    }

    return device_count;
}

VkPhysicalDevice get_vulkan_physical_device( const GUID *uuid )
{
    VkPhysicalDevice *devices, device;
    UINT device_count, i;

    if (!(device_count = get_vulkan_physical_devices( &devices ))) return VK_NULL_HANDLE;

    for (i = 0, device = VK_NULL_HANDLE; i < device_count; ++i)
    {
        VkPhysicalDeviceIDProperties id = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
        VkPhysicalDeviceProperties2 properties2 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &id};

        p_vkGetPhysicalDeviceProperties2KHR( devices[i], &properties2 );
        if (IsEqualGUID( uuid, id.deviceUUID ))
        {
            device = devices[i];
            break;
        }
    }

    free( devices );
    return device;
}

BOOL get_vulkan_gpus( struct list *gpus )
{
    VkPhysicalDevice *devices;
    UINT i, j, count;

ERR("\n");
    if (!d3dkmt_use_vulkan()) return FALSE;
ERR("\n");
    if (!(count = get_vulkan_physical_devices( &devices ))) return FALSE;

ERR("\n");
    for (i = 0; i < count; ++i)
    {
        VkPhysicalDeviceIDProperties id = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
        VkPhysicalDeviceProperties2 properties2 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &id};
        VkPhysicalDeviceMemoryProperties mem_properties;
        struct vulkan_gpu *gpu;

        if (!(gpu = calloc( 1, sizeof(*gpu) ))) break;
        p_vkGetPhysicalDeviceProperties2KHR( devices[i], &properties2 );
        memcpy( &gpu->uuid, id.deviceUUID, sizeof(gpu->uuid) );
        gpu->name = strdup( properties2.properties.deviceName );
        gpu->pci_id.vendor = properties2.properties.vendorID;
        gpu->pci_id.device = properties2.properties.deviceID;

        p_vkGetPhysicalDeviceMemoryProperties( devices[i], &mem_properties );
        for (j = 0; j < mem_properties.memoryHeapCount; j++)
        {
            if (mem_properties.memoryHeaps[j].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                gpu->memory += mem_properties.memoryHeaps[j].size;
        }

ERR("adding %s\n", debugstr_a(gpu->name));
        list_add_tail( gpus, &gpu->entry );
    }

    free( devices );
    return TRUE;
}

void free_vulkan_gpu( struct vulkan_gpu *gpu )
{
    free( gpu->name );
    free( gpu );
}

void get_vulkan_physical_device_memory( VkPhysicalDevice device, D3DKMT_QUERYVIDEOMEMORYINFO *desc )
{
    VkPhysicalDeviceMemoryBudgetPropertiesEXT budget = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT};
    VkPhysicalDeviceMemoryProperties2 properties2 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
    UINT i;

    desc->Budget = 0;
    desc->CurrentUsage = 0;
    desc->CurrentReservation = 0;
    desc->AvailableForReservation = 0;

    properties2.pNext = &budget;
    p_vkGetPhysicalDeviceMemoryProperties2KHR( device, &properties2 );

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
