#include "vk_enum_to_str.h"
#include "zink_instance.h"
#include "zink_screen.h"

bool
zink_create_instance(struct zink_screen *screen, bool display_dev)
{
   struct zink_instance_info *instance_info = &screen->instance_info;

   /* reserve one slot for MoltenVK */
   const char *layers[3] = {0};
   uint32_t num_layers = 0;
   
   const char *extensions[11] = {0};
   uint32_t num_extensions = 0;

   bool have_EXT_debug_utils = false;
   bool have_KHR_get_physical_device_properties2 = false;
   bool have_KHR_external_memory_capabilities = false;
   bool have_KHR_external_semaphore_capabilities = false;
   bool have_MVK_moltenvk = false;
   bool have_KHR_surface = false;
   bool have_EXT_headless_surface = false;
   bool have_KHR_wayland_surface = false;
   bool have_KHR_xcb_surface = false;
   bool have_KHR_win32_surface = false;

   bool have_layer_KHRONOS_validation = false;
   bool have_layer_LUNARG_standard_validation = false;

#if defined(MVK_VERSION)
   bool have_moltenvk_layer = false;
#endif

   GET_PROC_ADDR_INSTANCE_LOCAL(screen, NULL, EnumerateInstanceExtensionProperties);
   GET_PROC_ADDR_INSTANCE_LOCAL(screen, NULL, EnumerateInstanceLayerProperties);
   if (!vk_EnumerateInstanceExtensionProperties ||
       !vk_EnumerateInstanceLayerProperties)
      return false;

   // Build up the extensions from the reported ones but only for the unnamed layer
   uint32_t extension_count = 0;
   if (vk_EnumerateInstanceExtensionProperties(NULL, &extension_count, NULL) != VK_SUCCESS) {
       if (!screen->implicitly_loaded)
           mesa_loge("ZINK: vkEnumerateInstanceExtensionProperties failed");
   } else {
       VkExtensionProperties *extension_props = malloc(extension_count * sizeof(VkExtensionProperties));
       if (extension_props) {
           if (vk_EnumerateInstanceExtensionProperties(NULL, &extension_count, extension_props) != VK_SUCCESS) {
               if (!screen->implicitly_loaded)
                   mesa_loge("ZINK: vkEnumerateInstanceExtensionProperties failed");
           } else {
              for (uint32_t i = 0; i < extension_count; i++) {
                if (!strcmp(extension_props[i].extensionName, "VK_EXT_debug_utils")) {
                    have_EXT_debug_utils = true;
                }
                if (!strcmp(extension_props[i].extensionName, "VK_KHR_get_physical_device_properties2")) {
                    have_KHR_get_physical_device_properties2 = true;
                }
                if (!strcmp(extension_props[i].extensionName, "VK_KHR_external_memory_capabilities")) {
                    have_KHR_external_memory_capabilities = true;
                }
                if (!strcmp(extension_props[i].extensionName, "VK_KHR_external_semaphore_capabilities")) {
                    have_KHR_external_semaphore_capabilities = true;
                }
                if (!strcmp(extension_props[i].extensionName, "VK_MVK_moltenvk")) {
                    have_MVK_moltenvk = true;
                }
                if (!strcmp(extension_props[i].extensionName, "VK_KHR_surface")) {
                    have_KHR_surface = true;
                }
                if (!strcmp(extension_props[i].extensionName, "VK_EXT_headless_surface")) {
                    have_EXT_headless_surface = true;
                }
                if (!strcmp(extension_props[i].extensionName, "VK_KHR_wayland_surface")) {
                    have_KHR_wayland_surface = true;
                }
                if (!strcmp(extension_props[i].extensionName, "VK_KHR_xcb_surface")) {
                    have_KHR_xcb_surface = true;
                }
                if (!strcmp(extension_props[i].extensionName, "VK_KHR_win32_surface")) {
                    have_KHR_win32_surface = true;
                }
              }
           }
       free(extension_props);
       }
   }

    // Build up the layers from the reported ones
    uint32_t layer_count = 0;

    if (vk_EnumerateInstanceLayerProperties(&layer_count, NULL) != VK_SUCCESS) {
        if (!screen->implicitly_loaded)
           mesa_loge("ZINK: vkEnumerateInstanceLayerProperties failed");
    } else {
        VkLayerProperties *layer_props = malloc(layer_count * sizeof(VkLayerProperties));
        if (layer_props) {
            if (vk_EnumerateInstanceLayerProperties(&layer_count, layer_props) != VK_SUCCESS) {
                if (!screen->implicitly_loaded)
                    mesa_loge("ZINK: vkEnumerateInstanceLayerProperties failed");
            } else {
               for (uint32_t i = 0; i < layer_count; i++) {
                  if (!strcmp(layer_props[i].layerName, "VK_LAYER_KHRONOS_validation")) {
                     have_layer_KHRONOS_validation = true;
                  }
                  if (!strcmp(layer_props[i].layerName, "VK_LAYER_LUNARG_standard_validation")) {
                     have_layer_LUNARG_standard_validation = true;
                  }
#if defined(MVK_VERSION)
                  if (!strcmp(layer_props[i].layerName, "MoltenVK")) {
                     have_moltenvk_layer = true;
                     layers[num_layers++] = "MoltenVK";
                  }
#endif
               }
            }
        free(layer_props);
        }
    }

   if (have_EXT_debug_utils ) {
      instance_info->have_EXT_debug_utils = have_EXT_debug_utils;
      extensions[num_extensions++] = "VK_EXT_debug_utils";
   }
   if (have_KHR_get_physical_device_properties2 ) {
      instance_info->have_KHR_get_physical_device_properties2 = have_KHR_get_physical_device_properties2;
      extensions[num_extensions++] = "VK_KHR_get_physical_device_properties2";
   }
   if (have_KHR_external_memory_capabilities ) {
      instance_info->have_KHR_external_memory_capabilities = have_KHR_external_memory_capabilities;
      extensions[num_extensions++] = "VK_KHR_external_memory_capabilities";
   }
   if (have_KHR_external_semaphore_capabilities ) {
      instance_info->have_KHR_external_semaphore_capabilities = have_KHR_external_semaphore_capabilities;
      extensions[num_extensions++] = "VK_KHR_external_semaphore_capabilities";
   }
   if (have_MVK_moltenvk ) {
      instance_info->have_MVK_moltenvk = have_MVK_moltenvk;
      extensions[num_extensions++] = "VK_MVK_moltenvk";
   }
   if (have_KHR_surface ) {
      instance_info->have_KHR_surface = have_KHR_surface;
      extensions[num_extensions++] = "VK_KHR_surface";
   }
   if (have_EXT_headless_surface ) {
      instance_info->have_EXT_headless_surface = have_EXT_headless_surface;
      extensions[num_extensions++] = "VK_EXT_headless_surface";
   }
   if (have_KHR_wayland_surface && (!display_dev)) {
      instance_info->have_KHR_wayland_surface = have_KHR_wayland_surface;
      extensions[num_extensions++] = "VK_KHR_wayland_surface";
   }
   if (have_KHR_xcb_surface && (!display_dev)) {
      instance_info->have_KHR_xcb_surface = have_KHR_xcb_surface;
      extensions[num_extensions++] = "VK_KHR_xcb_surface";
   }
   if (have_KHR_win32_surface ) {
      instance_info->have_KHR_win32_surface = have_KHR_win32_surface;
      extensions[num_extensions++] = "VK_KHR_win32_surface";
   }

   if (have_layer_KHRONOS_validation && (zink_debug & ZINK_DEBUG_VALIDATION)) {
      layers[num_layers++] = "VK_LAYER_KHRONOS_validation";
      instance_info->have_layer_KHRONOS_validation = true;
   }
   if (have_layer_LUNARG_standard_validation && (zink_debug & ZINK_DEBUG_VALIDATION) && (!have_layer_KHRONOS_validation)) {
      layers[num_layers++] = "VK_LAYER_LUNARG_standard_validation";
      instance_info->have_layer_LUNARG_standard_validation = true;
   }

   VkApplicationInfo ai = {0};
   ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;

   const char *proc_name = util_get_process_name();
   if (!proc_name)
      proc_name = "unknown";

   ai.pApplicationName = proc_name;
   ai.pEngineName = "mesa zink";
   ai.apiVersion = instance_info->loader_version;

   VkInstanceCreateInfo ici = {0};
   ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
#ifdef __APPLE__
   ici.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif
   ici.pApplicationInfo = &ai;
   ici.ppEnabledExtensionNames = extensions;
   ici.enabledExtensionCount = num_extensions;
   ici.ppEnabledLayerNames = layers;
   ici.enabledLayerCount = num_layers;

   GET_PROC_ADDR_INSTANCE_LOCAL(screen, NULL, CreateInstance);
   assert(vk_CreateInstance);

   VkResult err = vk_CreateInstance(&ici, NULL, &screen->instance);
   if (err != VK_SUCCESS) {
      if (!screen->implicitly_loaded)
          mesa_loge("ZINK: vkCreateInstance failed (%s)", vk_Result_to_str(err));
      return false;
   }

   return true;
}

void
zink_verify_instance_extensions(struct zink_screen *screen)
{
   if (screen->instance_info.have_EXT_debug_utils) {
      if (!screen->vk.CreateDebugUtilsMessengerEXT) {
#ifndef NDEBUG
         screen->vk.CreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)zink_stub_CreateDebugUtilsMessengerEXT;
#else
         screen->vk.CreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)zink_stub_function_not_loaded;
#endif
      }
      if (!screen->vk.DestroyDebugUtilsMessengerEXT) {
#ifndef NDEBUG
         screen->vk.DestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)zink_stub_DestroyDebugUtilsMessengerEXT;
#else
         screen->vk.DestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)zink_stub_function_not_loaded;
#endif
      }
      if (!screen->vk.SubmitDebugUtilsMessageEXT) {
#ifndef NDEBUG
         screen->vk.SubmitDebugUtilsMessageEXT = (PFN_vkSubmitDebugUtilsMessageEXT)zink_stub_SubmitDebugUtilsMessageEXT;
#else
         screen->vk.SubmitDebugUtilsMessageEXT = (PFN_vkSubmitDebugUtilsMessageEXT)zink_stub_function_not_loaded;
#endif
      }
   }
   if (screen->instance_info.have_KHR_get_physical_device_properties2) {
      if (!screen->vk.GetPhysicalDeviceFeatures2KHR) {
#ifndef NDEBUG
         screen->vk.GetPhysicalDeviceFeatures2KHR = (PFN_vkGetPhysicalDeviceFeatures2KHR)zink_stub_GetPhysicalDeviceFeatures2KHR;
#else
         screen->vk.GetPhysicalDeviceFeatures2KHR = (PFN_vkGetPhysicalDeviceFeatures2KHR)zink_stub_function_not_loaded;
#endif
      }
      if (!screen->vk.GetPhysicalDeviceProperties2KHR) {
#ifndef NDEBUG
         screen->vk.GetPhysicalDeviceProperties2KHR = (PFN_vkGetPhysicalDeviceProperties2KHR)zink_stub_GetPhysicalDeviceProperties2KHR;
#else
         screen->vk.GetPhysicalDeviceProperties2KHR = (PFN_vkGetPhysicalDeviceProperties2KHR)zink_stub_function_not_loaded;
#endif
      }
      if (!screen->vk.GetPhysicalDeviceFormatProperties2KHR) {
#ifndef NDEBUG
         screen->vk.GetPhysicalDeviceFormatProperties2KHR = (PFN_vkGetPhysicalDeviceFormatProperties2KHR)zink_stub_GetPhysicalDeviceFormatProperties2KHR;
#else
         screen->vk.GetPhysicalDeviceFormatProperties2KHR = (PFN_vkGetPhysicalDeviceFormatProperties2KHR)zink_stub_function_not_loaded;
#endif
      }
      if (!screen->vk.GetPhysicalDeviceImageFormatProperties2KHR) {
#ifndef NDEBUG
         screen->vk.GetPhysicalDeviceImageFormatProperties2KHR = (PFN_vkGetPhysicalDeviceImageFormatProperties2KHR)zink_stub_GetPhysicalDeviceImageFormatProperties2KHR;
#else
         screen->vk.GetPhysicalDeviceImageFormatProperties2KHR = (PFN_vkGetPhysicalDeviceImageFormatProperties2KHR)zink_stub_function_not_loaded;
#endif
      }
      if (!screen->vk.GetPhysicalDeviceQueueFamilyProperties2KHR) {
#ifndef NDEBUG
         screen->vk.GetPhysicalDeviceQueueFamilyProperties2KHR = (PFN_vkGetPhysicalDeviceQueueFamilyProperties2KHR)zink_stub_GetPhysicalDeviceQueueFamilyProperties2KHR;
#else
         screen->vk.GetPhysicalDeviceQueueFamilyProperties2KHR = (PFN_vkGetPhysicalDeviceQueueFamilyProperties2KHR)zink_stub_function_not_loaded;
#endif
      }
      if (!screen->vk.GetPhysicalDeviceMemoryProperties2KHR) {
#ifndef NDEBUG
         screen->vk.GetPhysicalDeviceMemoryProperties2KHR = (PFN_vkGetPhysicalDeviceMemoryProperties2KHR)zink_stub_GetPhysicalDeviceMemoryProperties2KHR;
#else
         screen->vk.GetPhysicalDeviceMemoryProperties2KHR = (PFN_vkGetPhysicalDeviceMemoryProperties2KHR)zink_stub_function_not_loaded;
#endif
      }
      if (!screen->vk.GetPhysicalDeviceSparseImageFormatProperties2KHR) {
#ifndef NDEBUG
         screen->vk.GetPhysicalDeviceSparseImageFormatProperties2KHR = (PFN_vkGetPhysicalDeviceSparseImageFormatProperties2KHR)zink_stub_GetPhysicalDeviceSparseImageFormatProperties2KHR;
#else
         screen->vk.GetPhysicalDeviceSparseImageFormatProperties2KHR = (PFN_vkGetPhysicalDeviceSparseImageFormatProperties2KHR)zink_stub_function_not_loaded;
#endif
      }
   }
   if (screen->instance_info.have_KHR_external_memory_capabilities) {
      if (!screen->vk.GetPhysicalDeviceExternalBufferPropertiesKHR) {
#ifndef NDEBUG
         screen->vk.GetPhysicalDeviceExternalBufferPropertiesKHR = (PFN_vkGetPhysicalDeviceExternalBufferPropertiesKHR)zink_stub_GetPhysicalDeviceExternalBufferPropertiesKHR;
#else
         screen->vk.GetPhysicalDeviceExternalBufferPropertiesKHR = (PFN_vkGetPhysicalDeviceExternalBufferPropertiesKHR)zink_stub_function_not_loaded;
#endif
      }
   }
   if (screen->instance_info.have_KHR_external_semaphore_capabilities) {
      if (!screen->vk.GetPhysicalDeviceExternalSemaphorePropertiesKHR) {
#ifndef NDEBUG
         screen->vk.GetPhysicalDeviceExternalSemaphorePropertiesKHR = (PFN_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR)zink_stub_GetPhysicalDeviceExternalSemaphorePropertiesKHR;
#else
         screen->vk.GetPhysicalDeviceExternalSemaphorePropertiesKHR = (PFN_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR)zink_stub_function_not_loaded;
#endif
      }
   }
   if (screen->instance_info.have_KHR_surface) {
      if (!screen->vk.DestroySurfaceKHR) {
#ifndef NDEBUG
         screen->vk.DestroySurfaceKHR = (PFN_vkDestroySurfaceKHR)zink_stub_DestroySurfaceKHR;
#else
         screen->vk.DestroySurfaceKHR = (PFN_vkDestroySurfaceKHR)zink_stub_function_not_loaded;
#endif
      }
      if (!screen->vk.GetPhysicalDeviceSurfaceSupportKHR) {
#ifndef NDEBUG
         screen->vk.GetPhysicalDeviceSurfaceSupportKHR = (PFN_vkGetPhysicalDeviceSurfaceSupportKHR)zink_stub_GetPhysicalDeviceSurfaceSupportKHR;
#else
         screen->vk.GetPhysicalDeviceSurfaceSupportKHR = (PFN_vkGetPhysicalDeviceSurfaceSupportKHR)zink_stub_function_not_loaded;
#endif
      }
      if (!screen->vk.GetPhysicalDeviceSurfaceCapabilitiesKHR) {
#ifndef NDEBUG
         screen->vk.GetPhysicalDeviceSurfaceCapabilitiesKHR = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)zink_stub_GetPhysicalDeviceSurfaceCapabilitiesKHR;
#else
         screen->vk.GetPhysicalDeviceSurfaceCapabilitiesKHR = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)zink_stub_function_not_loaded;
#endif
      }
      if (!screen->vk.GetPhysicalDeviceSurfaceFormatsKHR) {
#ifndef NDEBUG
         screen->vk.GetPhysicalDeviceSurfaceFormatsKHR = (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)zink_stub_GetPhysicalDeviceSurfaceFormatsKHR;
#else
         screen->vk.GetPhysicalDeviceSurfaceFormatsKHR = (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)zink_stub_function_not_loaded;
#endif
      }
      if (!screen->vk.GetPhysicalDeviceSurfacePresentModesKHR) {
#ifndef NDEBUG
         screen->vk.GetPhysicalDeviceSurfacePresentModesKHR = (PFN_vkGetPhysicalDeviceSurfacePresentModesKHR)zink_stub_GetPhysicalDeviceSurfacePresentModesKHR;
#else
         screen->vk.GetPhysicalDeviceSurfacePresentModesKHR = (PFN_vkGetPhysicalDeviceSurfacePresentModesKHR)zink_stub_function_not_loaded;
#endif
      }
   }
   if (screen->instance_info.have_EXT_headless_surface) {
      if (!screen->vk.CreateHeadlessSurfaceEXT) {
#ifndef NDEBUG
         screen->vk.CreateHeadlessSurfaceEXT = (PFN_vkCreateHeadlessSurfaceEXT)zink_stub_CreateHeadlessSurfaceEXT;
#else
         screen->vk.CreateHeadlessSurfaceEXT = (PFN_vkCreateHeadlessSurfaceEXT)zink_stub_function_not_loaded;
#endif
      }
   }
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
   if (screen->instance_info.have_KHR_wayland_surface) {
      if (!screen->vk.CreateWaylandSurfaceKHR) {
#ifndef NDEBUG
         screen->vk.CreateWaylandSurfaceKHR = (PFN_vkCreateWaylandSurfaceKHR)zink_stub_CreateWaylandSurfaceKHR;
#else
         screen->vk.CreateWaylandSurfaceKHR = (PFN_vkCreateWaylandSurfaceKHR)zink_stub_function_not_loaded;
#endif
      }
      if (!screen->vk.GetPhysicalDeviceWaylandPresentationSupportKHR) {
#ifndef NDEBUG
         screen->vk.GetPhysicalDeviceWaylandPresentationSupportKHR = (PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR)zink_stub_GetPhysicalDeviceWaylandPresentationSupportKHR;
#else
         screen->vk.GetPhysicalDeviceWaylandPresentationSupportKHR = (PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR)zink_stub_function_not_loaded;
#endif
      }
   }
#endif
#ifdef VK_USE_PLATFORM_XCB_KHR
   if (screen->instance_info.have_KHR_xcb_surface) {
      if (!screen->vk.CreateXcbSurfaceKHR) {
#ifndef NDEBUG
         screen->vk.CreateXcbSurfaceKHR = (PFN_vkCreateXcbSurfaceKHR)zink_stub_CreateXcbSurfaceKHR;
#else
         screen->vk.CreateXcbSurfaceKHR = (PFN_vkCreateXcbSurfaceKHR)zink_stub_function_not_loaded;
#endif
      }
      if (!screen->vk.GetPhysicalDeviceXcbPresentationSupportKHR) {
#ifndef NDEBUG
         screen->vk.GetPhysicalDeviceXcbPresentationSupportKHR = (PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR)zink_stub_GetPhysicalDeviceXcbPresentationSupportKHR;
#else
         screen->vk.GetPhysicalDeviceXcbPresentationSupportKHR = (PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR)zink_stub_function_not_loaded;
#endif
      }
   }
#endif
#ifdef VK_USE_PLATFORM_WIN32_KHR
   if (screen->instance_info.have_KHR_win32_surface) {
      if (!screen->vk.CreateWin32SurfaceKHR) {
#ifndef NDEBUG
         screen->vk.CreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)zink_stub_CreateWin32SurfaceKHR;
#else
         screen->vk.CreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)zink_stub_function_not_loaded;
#endif
      }
      if (!screen->vk.GetPhysicalDeviceWin32PresentationSupportKHR) {
#ifndef NDEBUG
         screen->vk.GetPhysicalDeviceWin32PresentationSupportKHR = (PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR)zink_stub_GetPhysicalDeviceWin32PresentationSupportKHR;
#else
         screen->vk.GetPhysicalDeviceWin32PresentationSupportKHR = (PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR)zink_stub_function_not_loaded;
#endif
      }
   }
#endif
}

#ifndef NDEBUG
/* generated stub functions */


   
void VKAPI_PTR
zink_stub_CreateDebugUtilsMessengerEXT()
{
   mesa_loge("ZINK: vkCreateDebugUtilsMessengerEXT is not loaded properly!");
   abort();
}
   
void VKAPI_PTR
zink_stub_DestroyDebugUtilsMessengerEXT()
{
   mesa_loge("ZINK: vkDestroyDebugUtilsMessengerEXT is not loaded properly!");
   abort();
}
   
void VKAPI_PTR
zink_stub_SubmitDebugUtilsMessageEXT()
{
   mesa_loge("ZINK: vkSubmitDebugUtilsMessageEXT is not loaded properly!");
   abort();
}
   
void VKAPI_PTR
zink_stub_GetPhysicalDeviceFeatures2KHR()
{
   mesa_loge("ZINK: vkGetPhysicalDeviceFeatures2KHR is not loaded properly!");
   abort();
}
   
void VKAPI_PTR
zink_stub_GetPhysicalDeviceProperties2KHR()
{
   mesa_loge("ZINK: vkGetPhysicalDeviceProperties2KHR is not loaded properly!");
   abort();
}
   
void VKAPI_PTR
zink_stub_GetPhysicalDeviceFormatProperties2KHR()
{
   mesa_loge("ZINK: vkGetPhysicalDeviceFormatProperties2KHR is not loaded properly!");
   abort();
}
   
void VKAPI_PTR
zink_stub_GetPhysicalDeviceImageFormatProperties2KHR()
{
   mesa_loge("ZINK: vkGetPhysicalDeviceImageFormatProperties2KHR is not loaded properly!");
   abort();
}
   
void VKAPI_PTR
zink_stub_GetPhysicalDeviceQueueFamilyProperties2KHR()
{
   mesa_loge("ZINK: vkGetPhysicalDeviceQueueFamilyProperties2KHR is not loaded properly!");
   abort();
}
   
void VKAPI_PTR
zink_stub_GetPhysicalDeviceMemoryProperties2KHR()
{
   mesa_loge("ZINK: vkGetPhysicalDeviceMemoryProperties2KHR is not loaded properly!");
   abort();
}
   
void VKAPI_PTR
zink_stub_GetPhysicalDeviceSparseImageFormatProperties2KHR()
{
   mesa_loge("ZINK: vkGetPhysicalDeviceSparseImageFormatProperties2KHR is not loaded properly!");
   abort();
}
   
void VKAPI_PTR
zink_stub_GetPhysicalDeviceExternalBufferPropertiesKHR()
{
   mesa_loge("ZINK: vkGetPhysicalDeviceExternalBufferPropertiesKHR is not loaded properly!");
   abort();
}
   
void VKAPI_PTR
zink_stub_GetPhysicalDeviceExternalSemaphorePropertiesKHR()
{
   mesa_loge("ZINK: vkGetPhysicalDeviceExternalSemaphorePropertiesKHR is not loaded properly!");
   abort();
}
   
void VKAPI_PTR
zink_stub_DestroySurfaceKHR()
{
   mesa_loge("ZINK: vkDestroySurfaceKHR is not loaded properly!");
   abort();
}
   
void VKAPI_PTR
zink_stub_GetPhysicalDeviceSurfaceSupportKHR()
{
   mesa_loge("ZINK: vkGetPhysicalDeviceSurfaceSupportKHR is not loaded properly!");
   abort();
}
   
void VKAPI_PTR
zink_stub_GetPhysicalDeviceSurfaceCapabilitiesKHR()
{
   mesa_loge("ZINK: vkGetPhysicalDeviceSurfaceCapabilitiesKHR is not loaded properly!");
   abort();
}
   
void VKAPI_PTR
zink_stub_GetPhysicalDeviceSurfaceFormatsKHR()
{
   mesa_loge("ZINK: vkGetPhysicalDeviceSurfaceFormatsKHR is not loaded properly!");
   abort();
}
   
void VKAPI_PTR
zink_stub_GetPhysicalDeviceSurfacePresentModesKHR()
{
   mesa_loge("ZINK: vkGetPhysicalDeviceSurfacePresentModesKHR is not loaded properly!");
   abort();
}
   
void VKAPI_PTR
zink_stub_CreateHeadlessSurfaceEXT()
{
   mesa_loge("ZINK: vkCreateHeadlessSurfaceEXT is not loaded properly!");
   abort();
}
   
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
void VKAPI_PTR
zink_stub_CreateWaylandSurfaceKHR()
{
   mesa_loge("ZINK: vkCreateWaylandSurfaceKHR is not loaded properly!");
   abort();
}
#endif
   
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
void VKAPI_PTR
zink_stub_GetPhysicalDeviceWaylandPresentationSupportKHR()
{
   mesa_loge("ZINK: vkGetPhysicalDeviceWaylandPresentationSupportKHR is not loaded properly!");
   abort();
}
#endif
   
#ifdef VK_USE_PLATFORM_XCB_KHR
void VKAPI_PTR
zink_stub_CreateXcbSurfaceKHR()
{
   mesa_loge("ZINK: vkCreateXcbSurfaceKHR is not loaded properly!");
   abort();
}
#endif
   
#ifdef VK_USE_PLATFORM_XCB_KHR
void VKAPI_PTR
zink_stub_GetPhysicalDeviceXcbPresentationSupportKHR()
{
   mesa_loge("ZINK: vkGetPhysicalDeviceXcbPresentationSupportKHR is not loaded properly!");
   abort();
}
#endif
   
#ifdef VK_USE_PLATFORM_WIN32_KHR
void VKAPI_PTR
zink_stub_CreateWin32SurfaceKHR()
{
   mesa_loge("ZINK: vkCreateWin32SurfaceKHR is not loaded properly!");
   abort();
}
#endif
   
#ifdef VK_USE_PLATFORM_WIN32_KHR
void VKAPI_PTR
zink_stub_GetPhysicalDeviceWin32PresentationSupportKHR()
{
   mesa_loge("ZINK: vkGetPhysicalDeviceWin32PresentationSupportKHR is not loaded properly!");
   abort();
}
#endif

#endif
