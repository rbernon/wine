/* Wine OpenXR ICD implementation
 *
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

#include "vulkan_loader.h"

WINE_DEFAULT_DEBUG_CHANNEL(openxr);

static BOOL WINAPI init_openxr( INIT_ONCE *once, void *param, void **context )
{
    return !UNIX_CALL( init_openxr, NULL );
}

static BOOL init_openxr_once( void )
{
    static INIT_ONCE init_once = INIT_ONCE_STATIC_INIT;
    return InitOnceExecuteOnce( &init_once, init_openxr, NULL, NULL );
}

static const struct vulkan_func global_proc_table[] =
{
    /* These functions must call wine_vk_init_once() before accessing vk_funcs. */
    {"xrGetInstanceProcAddr", &xrGetInstanceProcAddr},
    {"xrNegotiateLoaderRuntimeInterface", &xrNegotiateLoaderRuntimeInterface},
    {"xrEnumerateInstanceExtensionProperties", &xrEnumerateInstanceExtensionProperties},
    {"xrCreateInstance", &xrCreateInstance},
};

static void *get_global_proc_addr( const char *name )
{
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(global_proc_table); i++)
    {
        if (strcmp( name, global_proc_table[i].name ) == 0)
        {
            TRACE( "Found name=%s in global table\n", debugstr_a(name));
            return global_proc_table[i].func;
        }
    }

    return NULL;
}

static BOOL is_available_xr_instance_function( XrInstance instance, const char *name )
{
    struct is_available_xr_instance_function_params params = {.instance = instance, .name = name};
    return UNIX_CALL( is_available_xr_instance_function, &params );
}


XrResult WINAPI xrGetInstanceProcAddr( XrInstance instance, const char *name, PFN_xrVoidFunction *function )
{
    TRACE( "%p, %s\n", instance, debugstr_a(name));

    *function = NULL;
    if (!name) return XR_ERROR_FUNCTION_UNSUPPORTED;
    if ((*function = get_global_proc_addr( name ))) return XR_SUCCESS;
    if (instance)
    {
        if (!is_available_xr_instance_function( instance, name )) return XR_ERROR_FUNCTION_UNSUPPORTED;
        if ((*function = wine_xr_get_instance_proc_addr( name ))) return XR_SUCCESS;
    }

    WARN( "Unsupported device or instance function: %s.\n", debugstr_a(name));
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XrResult WINAPI xrNegotiateLoaderRuntimeInterface( const XrNegotiateLoaderInfo *info, XrNegotiateRuntimeRequest *request )
{
    TRACE( "%p %p\n", info, request );

    if (!info) return XR_ERROR_INITIALIZATION_FAILED;
    if (info->structType != XR_LOADER_INTERFACE_STRUCT_LOADER_INFO) return XR_ERROR_INITIALIZATION_FAILED;
    if (info->structVersion != XR_LOADER_INFO_STRUCT_VERSION) return XR_ERROR_INITIALIZATION_FAILED;
    if (info->structSize != sizeof(*info)) return XR_ERROR_INITIALIZATION_FAILED;

    if (!request) return XR_ERROR_INITIALIZATION_FAILED;
    if (request->structType != XR_LOADER_INTERFACE_STRUCT_RUNTIME_REQUEST) return XR_ERROR_INITIALIZATION_FAILED;
    if (request->structVersion != XR_RUNTIME_INFO_STRUCT_VERSION) return XR_ERROR_INITIALIZATION_FAILED;
    if (request->structSize != sizeof(*request)) return XR_ERROR_INITIALIZATION_FAILED;

    if (info->minInterfaceVersion > XR_CURRENT_LOADER_RUNTIME_VERSION ||
        info->maxInterfaceVersion < XR_CURRENT_LOADER_RUNTIME_VERSION)
        return XR_ERROR_INITIALIZATION_FAILED;
    if (info->minApiVersion > XR_CURRENT_API_VERSION ||
        info->maxApiVersion < XR_CURRENT_API_VERSION)
        return XR_ERROR_INITIALIZATION_FAILED;

    if (!init_openxr_once()) return XR_ERROR_INITIALIZATION_FAILED;

    request->runtimeInterfaceVersion = XR_CURRENT_LOADER_RUNTIME_VERSION;
    request->getInstanceProcAddr = (PFN_xrGetInstanceProcAddr)&xrGetInstanceProcAddr;
    request->runtimeApiVersion = XR_CURRENT_API_VERSION;
    return XR_SUCCESS;
}

XrResult WINAPI xrCreateApiLayerInstance( const XrInstanceCreateInfo *create_info, const XrApiLayerCreateInfo *layer_info,
                                          XrInstance *instance )
{
    ERR( "Unexpected call\n" );
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XrResult WINAPI xrEnumerateApiLayerProperties( uint32_t capacity, uint32_t *count, XrApiLayerProperties *properties )
{
    ERR( "Unexpected call\n" );
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XrResult WINAPI xrNegotiateLoaderApiLayerInterface( const XrNegotiateLoaderInfo *loader_info, const char *layer_name,
                                                    XrNegotiateApiLayerRequest *request )
{
    ERR( "Unexpected call\n" );
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}


XrResult WINAPI xrCreateInstance( const XrInstanceCreateInfo *create_info, XrInstance *instance )
{
    struct xrCreateInstance_params params;
    NTSTATUS status;

    if (!(*instance = calloc( 1, sizeof(**instance) ))) return XR_ERROR_OUT_OF_MEMORY;

    params.createInfo = create_info;
    params.instance = instance;
    status = UNIX_CALL( xrCreateInstance, &params );
    assert( !status && "xrCreateInstance" );

    if (params.result)
    {
        free( *instance );
        *instance = NULL;
    }
    return params.result;
}

XrResult WINAPI xrDestroyInstance( XrInstance instance )
{
    struct xrDestroyInstance_params params;
    NTSTATUS status;
    if (!(params.instance = instance)) return XR_SUCCESS;
    status = UNIX_CALL( xrDestroyInstance, &params );
    assert( !status && "xrDestroyInstance" );
    free( instance );
    return params.result;
}

XrResult WINAPI xrConvertTimeToWin32PerformanceCounterKHR( XrInstance instance, XrTime time, LARGE_INTEGER *counter )
{
    struct xrConvertTimeToWin32PerformanceCounterKHR_params params;
    NTSTATUS status;
    params.instance = instance;
    params.time = time;
    params.performanceCounter = counter;
    status = UNIX_CALL( xrConvertTimeToWin32PerformanceCounterKHR, &params );
    assert( !status && "xrConvertTimeToWin32PerformanceCounterKHR" );
    return params.result;
}

XrResult WINAPI xrConvertWin32PerformanceCounterToTimeKHR( XrInstance instance, const LARGE_INTEGER *counter, XrTime *time )
{
    struct xrConvertWin32PerformanceCounterToTimeKHR_params params;
    NTSTATUS status;
    params.instance = instance;
    params.performanceCounter = counter;
    params.time = time;
    status = UNIX_CALL( xrConvertWin32PerformanceCounterToTimeKHR, &params );
    assert( !status && "xrConvertWin32PerformanceCounterToTimeKHR" );
    return params.result;
}

XrResult WINAPI xrCreateVulkanDeviceKHR( XrInstance instance, const XrVulkanDeviceCreateInfoKHR *create_info,
                                         VkDevice *vulkan_device, VkResult *vulkan_result )
{
    struct xrCreateVulkanDeviceKHR_params params;
    NTSTATUS status;
    params.instance = instance;
    params.createInfo = create_info;
    params.vulkanDevice = vulkan_device;
    params.vulkanResult = vulkan_result;
    status = UNIX_CALL( xrCreateVulkanDeviceKHR, &params );
    assert( !status && "xrCreateVulkanDeviceKHR" );
    return params.result;
}

XrResult WINAPI xrCreateVulkanInstanceKHR( XrInstance instance, const XrVulkanInstanceCreateInfoKHR *create_info,
                                           VkInstance *vulkan_instance, VkResult *vulkan_result )
{
    struct xrCreateVulkanInstanceKHR_params params;
    NTSTATUS status;
    params.instance = instance;
    params.createInfo = create_info;
    params.vulkanInstance = vulkan_instance;
    params.vulkanResult = vulkan_result;
    status = UNIX_CALL( xrCreateVulkanInstanceKHR, &params );
    assert( !status && "xrCreateVulkanInstanceKHR" );
    return params.result;
}

XrResult WINAPI xrGetSystem( XrInstance instance, const XrSystemGetInfo *get_info, XrSystemId *system_id )
{
    struct xrGetSystem_params params;
    NTSTATUS status;
    params.instance = instance;
    params.getInfo = get_info;
    params.systemId = system_id;
    status = UNIX_CALL( xrGetSystem, &params );
    assert( !status && "xrGetSystem" );
    return params.result;
}

XrResult WINAPI xrGetD3D11GraphicsRequirementsKHR( XrInstance instance, XrSystemId system_id,
                                                   XrGraphicsRequirementsD3D11KHR *requirements )
{
    struct xrGetD3D11GraphicsRequirementsKHR_params params;
    NTSTATUS status;
    params.instance = instance;
    params.systemId = system_id;
    params.graphicsRequirements = requirements;
    status = UNIX_CALL( xrGetD3D11GraphicsRequirementsKHR, &params );
    assert( !status && "xrGetD3D11GraphicsRequirementsKHR" );
    return params.result;
}

XrResult WINAPI xrGetD3D12GraphicsRequirementsKHR( XrInstance instance, XrSystemId system_id,
                                                   XrGraphicsRequirementsD3D12KHR *requirements )
{
    struct xrGetD3D12GraphicsRequirementsKHR_params params;
    NTSTATUS status;
    params.instance = instance;
    params.systemId = system_id;
    params.graphicsRequirements = requirements;
    status = UNIX_CALL( xrGetD3D12GraphicsRequirementsKHR, &params );
    assert( !status && "xrGetD3D12GraphicsRequirementsKHR" );
    return params.result;
}

XrResult WINAPI xrGetVulkanDeviceExtensionsKHR( XrInstance instance, XrSystemId system_id,
                                                uint32_t capacity, uint32_t *count, char *buffer )
{
    struct xrGetVulkanDeviceExtensionsKHR_params params;
    NTSTATUS status;
    params.instance = instance;
    params.systemId = system_id;
    params.bufferCapacityInput = capacity;
    params.bufferCountOutput = count;
    params.buffer = buffer;
    status = UNIX_CALL( xrGetVulkanDeviceExtensionsKHR, &params );
    assert( !status && "xrGetVulkanDeviceExtensionsKHR" );
    return params.result;
}

XrResult WINAPI xrGetVulkanGraphicsDevice2KHR( XrInstance instance, const XrVulkanGraphicsDeviceGetInfoKHR *get_info,
                                               VkPhysicalDevice *vulkan_physical_device )
{
    struct xrGetVulkanGraphicsDevice2KHR_params params;
    NTSTATUS status;
    params.instance = instance;
    params.getInfo = get_info;
    params.vulkanPhysicalDevice = vulkan_physical_device;
    status = UNIX_CALL( xrGetVulkanGraphicsDevice2KHR, &params );
    assert( !status && "xrGetVulkanGraphicsDevice2KHR" );
    return params.result;
}

XrResult WINAPI xrGetVulkanGraphicsDeviceKHR( XrInstance instance, XrSystemId system_id, VkInstance vulkan_instance,
                                              VkPhysicalDevice *vulkan_physical_device )
{
    struct xrGetVulkanGraphicsDeviceKHR_params params;
    NTSTATUS status;
    params.instance = instance;
    params.systemId = system_id;
    params.vkInstance = vulkan_instance;
    params.vkPhysicalDevice = vulkan_physical_device;
    status = UNIX_CALL( xrGetVulkanGraphicsDeviceKHR, &params );
    assert( !status && "xrGetVulkanGraphicsDeviceKHR" );
    return params.result;
}

XrResult WINAPI xrGetVulkanInstanceExtensionsKHR( XrInstance instance, XrSystemId system_id,
                                                  uint32_t capacity, uint32_t *count, char *buffer )
{
    struct xrGetVulkanInstanceExtensionsKHR_params params;
    NTSTATUS status;
    params.instance = instance;
    params.systemId = system_id;
    params.bufferCapacityInput = capacity;
    params.bufferCountOutput = count;
    params.buffer = buffer;
    status = UNIX_CALL( xrGetVulkanInstanceExtensionsKHR, &params );
    assert( !status && "xrGetVulkanInstanceExtensionsKHR" );
    return params.result;
}

XrResult WINAPI xrPollEvent( XrInstance instance, XrEventDataBuffer *event_data )
{
    struct xrPollEvent_params params;
    NTSTATUS status;
    params.instance = instance;
    params.eventData = event_data;
    status = UNIX_CALL( xrPollEvent, &params );
    assert( !status && "xrPollEvent" );
    return params.result;
}


XrResult WINAPI xrCreateSession( XrInstance instance, const XrSessionCreateInfo *create_info, XrSession *session )
{
    struct xrCreateSession_params params;
    NTSTATUS status;

    if (!(*session = calloc( 1, sizeof(**session) ))) return XR_ERROR_OUT_OF_MEMORY;

    params.instance = instance;
    params.createInfo = create_info;
    params.session = session;
    status = UNIX_CALL( xrCreateSession, &params );
    assert( !status && "xrCreateSession" );

    if (params.result)
    {
        free( *session );
        *session = NULL;
    }
    return params.result;
}

XrResult WINAPI xrDestroySession( XrSession session )
{
    struct xrDestroySession_params params;
    NTSTATUS status;
    if (!(params.session = session)) return XR_SUCCESS;
    status = UNIX_CALL( xrDestroySession, &params );
    assert( !status && "xrDestroySession" );
    free( session );
    return params.result;
}

XrResult WINAPI xrEnumerateSwapchainFormats( XrSession session, uint32_t capacity, uint32_t *count, int64_t *formats )
{
    struct xrEnumerateSwapchainFormats_params params;
    NTSTATUS status;
    params.session = session;
    params.formatCapacityInput = capacity;
    params.formatCountOutput = count;
    params.formats = formats;
    status = UNIX_CALL( xrEnumerateSwapchainFormats, &params );
    assert( !status && "xrEnumerateSwapchainFormats" );
    return params.result;
}

XrResult WINAPI xrBeginFrame( XrSession session, const XrFrameBeginInfo *begin_info )
{
    struct xrBeginFrame_params params;
    NTSTATUS status;
    params.session = session;
    params.frameBeginInfo = begin_info;
    status = UNIX_CALL( xrBeginFrame, &params );
    assert( !status && "xrBeginFrame" );
    return params.result;
}

XrResult WINAPI xrEndFrame( XrSession session, const XrFrameEndInfo *end_info )
{
    struct xrEndFrame_params params;
    NTSTATUS status;
    params.session = session;
    params.frameEndInfo = end_info;
    status = UNIX_CALL( xrEndFrame, &params );
    assert( !status && "xrEndFrame" );
    return params.result;
}


XrResult WINAPI xrCreateSwapchain( XrSession session, const XrSwapchainCreateInfo *create_info, XrSwapchain *swapchain )
{
    struct xrCreateSwapchain_params params;
    NTSTATUS status;

    if (!(*swapchain = calloc( 1, sizeof(**swapchain) ))) return XR_ERROR_OUT_OF_MEMORY;

    params.session = session;
    params.createInfo = create_info;
    params.swapchain = swapchain;
    status = UNIX_CALL( xrCreateSwapchain, &params );
    assert( !status && "xrCreateSwapchain" );

    if (params.result)
    {
        free( *swapchain );
        *swapchain = NULL;
    }
    return params.result;
}

XrResult WINAPI xrDestroySwapchain( XrSwapchain swapchain )
{
    struct xrDestroySwapchain_params params;
    NTSTATUS status;
    if (!(params.swapchain = swapchain)) return XR_SUCCESS;
    status = UNIX_CALL( xrDestroySwapchain, &params );
    assert( !status && "xrDestroySwapchain" );
    free( swapchain );
    return params.result;
}

XrResult WINAPI xrAcquireSwapchainImage( XrSwapchain swapchain, const XrSwapchainImageAcquireInfo *acquire_info,
                                         uint32_t *index )
{
    struct xrAcquireSwapchainImage_params params;
    NTSTATUS status;
    params.swapchain = swapchain;
    params.acquireInfo = acquire_info;
    params.index = index;
    status = UNIX_CALL( xrAcquireSwapchainImage, &params );
    assert( !status && "xrAcquireSwapchainImage" );
    return params.result;
}

XrResult WINAPI xrEnumerateSwapchainImages( XrSwapchain swapchain, uint32_t capacity,
                                            uint32_t *count, XrSwapchainImageBaseHeader *images )
{
    struct xrEnumerateSwapchainImages_params params;
    NTSTATUS status;
    params.swapchain = swapchain;
    params.imageCapacityInput = capacity;
    params.imageCountOutput = count;
    params.images = images;
    status = UNIX_CALL( xrEnumerateSwapchainImages, &params );
    assert( !status && "xrEnumerateSwapchainImages" );
    return params.result;
}

XrResult WINAPI xrReleaseSwapchainImage( XrSwapchain swapchain, const XrSwapchainImageReleaseInfo *release_info )
{
    struct xrReleaseSwapchainImage_params params;
    NTSTATUS status;
    params.swapchain = swapchain;
    params.releaseInfo = release_info;
    status = UNIX_CALL( xrReleaseSwapchainImage, &params );
    assert( !status && "xrReleaseSwapchainImage" );
    return params.result;
}


XrResult WINAPI xrCreateActionSpace( XrSession session, const XrActionSpaceCreateInfo *create_info, XrSpace *space )
{
    struct xrCreateActionSpace_params params;
    NTSTATUS status;

    if (!(*space = calloc( 1, sizeof(**space) ))) return XR_ERROR_OUT_OF_MEMORY;

    params.session = session;
    params.createInfo = create_info;
    params.space = space;
    status = UNIX_CALL( xrCreateActionSpace, &params );
    assert( !status && "xrCreateActionSpace" );

    if (params.result)
    {
        free( *space );
        *space = NULL;
    }
    return params.result;
}

XrResult WINAPI xrCreateReferenceSpace( XrSession session, const XrReferenceSpaceCreateInfo *create_info, XrSpace *space )
{
    struct xrCreateReferenceSpace_params params;
    NTSTATUS status;

    if (!(*space = calloc( 1, sizeof(**space) ))) return XR_ERROR_OUT_OF_MEMORY;

    params.session = session;
    params.createInfo = create_info;
    params.space = space;
    status = UNIX_CALL( xrCreateReferenceSpace, &params );
    assert( !status && "xrCreateReferenceSpace" );

    if (params.result)
    {
        free( *space );
        *space = NULL;
    }
    return params.result;
}

XrResult WINAPI xrDestroySpace( XrSpace space )
{
    struct xrDestroySpace_params params;
    NTSTATUS status;
    if (!(params.space = space)) return XR_SUCCESS;
    status = UNIX_CALL( xrDestroySpace, &params );
    assert( !status && "xrDestroySpace" );
    free( space );
    return params.result;
}


XrResult WINAPI xrCreateActionSet( XrInstance instance, const XrActionSetCreateInfo *create_info, XrActionSet *action_set )
{
    struct xrCreateActionSet_params params;
    NTSTATUS status;

    if (!(*action_set = calloc( 1, sizeof(**action_set) ))) return XR_ERROR_OUT_OF_MEMORY;

    params.instance = instance;
    params.createInfo = create_info;
    params.actionSet = action_set;
    status = UNIX_CALL( xrCreateActionSet, &params );
    assert( !status && "xrCreateActionSet" );

    if (params.result)
    {
        free( *action_set );
        *action_set = NULL;
    }
    return params.result;
}

XrResult WINAPI xrDestroyActionSet( XrActionSet action_set )
{
    struct xrDestroyActionSet_params params;
    NTSTATUS status;
    if (!(params.actionSet = action_set)) return XR_SUCCESS;
    status = UNIX_CALL( xrDestroyActionSet, &params );
    assert( !status && "xrDestroyActionSet" );
    free( action_set );
    return params.result;
}


XrResult WINAPI xrCreateAction( XrActionSet action_set, const XrActionCreateInfo *create_info, XrAction *action )
{
    struct xrCreateAction_params params;
    NTSTATUS status;

    if (!(*action = calloc( 1, sizeof(**action) ))) return XR_ERROR_OUT_OF_MEMORY;

    params.actionSet = action_set;
    params.createInfo = create_info;
    params.action = action;
    status = UNIX_CALL( xrCreateAction, &params );
    assert( !status && "xrCreateAction" );

    if (params.result)
    {
        free( *action );
        *action = NULL;
    }
    return params.result;
}

XrResult WINAPI xrDestroyAction( XrAction action )
{
    struct xrDestroyAction_params params;
    NTSTATUS status;
    if (!(params.action = action)) return XR_SUCCESS;
    status = UNIX_CALL( xrDestroyAction, &params );
    assert( !status && "xrDestroyAction" );
    free( action );
    return params.result;
}
