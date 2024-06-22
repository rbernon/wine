#ifndef ZINK_INSTANCE_H
#define ZINK_INSTANCE_H

#include "util/u_process.h"

#include <vulkan/vulkan_core.h>

#ifdef __APPLE__
#include "MoltenVK/mvk_vulkan.h"
// Source of MVK_VERSION
#include "MoltenVK/mvk_config.h"
#endif /* __APPLE__ */

struct pipe_screen;
struct zink_screen;

struct zink_instance_info {
   uint32_t loader_version;

   bool have_EXT_debug_utils;
   bool have_KHR_get_physical_device_properties2;
   bool have_KHR_external_memory_capabilities;
   bool have_KHR_external_semaphore_capabilities;
   bool have_MVK_moltenvk;
   bool have_KHR_surface;
   bool have_EXT_headless_surface;
   bool have_KHR_wayland_surface;
   bool have_KHR_xcb_surface;
   bool have_KHR_win32_surface;

   bool have_layer_KHRONOS_validation;
   bool have_layer_LUNARG_standard_validation;
};

bool
zink_create_instance(struct zink_screen *screen, bool display_dev);

void
zink_verify_instance_extensions(struct zink_screen *screen);

/* stub functions that get inserted into the dispatch table if they are not
 * properly loaded.
 */
void VKAPI_PTR zink_stub_CreateDebugUtilsMessengerEXT(void);
void VKAPI_PTR zink_stub_DestroyDebugUtilsMessengerEXT(void);
void VKAPI_PTR zink_stub_SubmitDebugUtilsMessageEXT(void);
void VKAPI_PTR zink_stub_GetPhysicalDeviceFeatures2KHR(void);
void VKAPI_PTR zink_stub_GetPhysicalDeviceProperties2KHR(void);
void VKAPI_PTR zink_stub_GetPhysicalDeviceFormatProperties2KHR(void);
void VKAPI_PTR zink_stub_GetPhysicalDeviceImageFormatProperties2KHR(void);
void VKAPI_PTR zink_stub_GetPhysicalDeviceQueueFamilyProperties2KHR(void);
void VKAPI_PTR zink_stub_GetPhysicalDeviceMemoryProperties2KHR(void);
void VKAPI_PTR zink_stub_GetPhysicalDeviceSparseImageFormatProperties2KHR(void);
void VKAPI_PTR zink_stub_GetPhysicalDeviceExternalBufferPropertiesKHR(void);
void VKAPI_PTR zink_stub_GetPhysicalDeviceExternalSemaphorePropertiesKHR(void);
void VKAPI_PTR zink_stub_DestroySurfaceKHR(void);
void VKAPI_PTR zink_stub_GetPhysicalDeviceSurfaceSupportKHR(void);
void VKAPI_PTR zink_stub_GetPhysicalDeviceSurfaceCapabilitiesKHR(void);
void VKAPI_PTR zink_stub_GetPhysicalDeviceSurfaceFormatsKHR(void);
void VKAPI_PTR zink_stub_GetPhysicalDeviceSurfacePresentModesKHR(void);
void VKAPI_PTR zink_stub_CreateHeadlessSurfaceEXT(void);
void VKAPI_PTR zink_stub_CreateWaylandSurfaceKHR(void);
void VKAPI_PTR zink_stub_GetPhysicalDeviceWaylandPresentationSupportKHR(void);
void VKAPI_PTR zink_stub_CreateXcbSurfaceKHR(void);
void VKAPI_PTR zink_stub_GetPhysicalDeviceXcbPresentationSupportKHR(void);
void VKAPI_PTR zink_stub_CreateWin32SurfaceKHR(void);
void VKAPI_PTR zink_stub_GetPhysicalDeviceWin32PresentationSupportKHR(void);

struct pipe_screen;
struct pipe_resource;

#endif
