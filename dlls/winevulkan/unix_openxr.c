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

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include "vulkan_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(openxr);

static PFN_xrEnumerateInstanceExtensionProperties p_xrEnumerateInstanceExtensionProperties;
static PFN_xrCreateInstance p_xrCreateInstance;
static PFN_xrDestroyInstance p_xrDestroyInstance;
static PFN_xrGetInstanceProcAddr p_xrGetInstanceProcAddr;

static void openxr_object_init_ptr( struct vulkan_object *obj, UINT64 host_handle, struct openxr_client_object *client )
{
    obj->host_handle = host_handle;
    obj->client_handle = (UINT_PTR)client;
    client->unix_handle = (UINT_PTR)obj;
}

NTSTATUS init_openxr( void *arg )
{
    return STATUS_SUCCESS;
}


XrResult wine_xrEnumerateInstanceExtensionProperties( const char *layer_name, uint32_t capacity,
                                                      uint32_t *count, XrExtensionProperties *properties )
{
    XrExtensionProperties *host_properties;
    uint32_t i, j, host_count;
    XrResult res;

    TRACE( "layer_name %s, capacity %u, count %p, properties %p\n", debugstr_a(layer_name), capacity, count, properties );

    if ((res = p_xrEnumerateInstanceExtensionProperties( layer_name, 0, &host_count, NULL ))) return res;
    if (!(host_properties = malloc( host_count * sizeof(*host_properties) ))) return XR_ERROR_OUT_OF_MEMORY;
    if ((res = p_xrEnumerateInstanceExtensionProperties( layer_name, host_count, &host_count, host_properties ))) goto done;

    for (i = 0, j = 0; i < host_count && j < capacity; i++)
    {
        XrExtensionProperties property = host_properties[i];
        property.pNext = NULL;

        if (!strcmp( property.extensionName, "XR_KHR_vulkan_enable" ))
        {
            static const XrExtensionProperties d3d11_enable =
            {
                .sType = XR_TYPE_EXTENSION_PROPERTIES,
                .extensionName = XR_KHR_D3D11_ENABLE_EXTENSION_NAME,
                .extensionVersion = XR_KHR_D3D11_enable_SPEC_VERSION,
            };
            static const XrExtensionProperties d3d12_enable =
            {
                .sType = XR_TYPE_EXTENSION_PROPERTIES,
                .extensionName = XR_KHR_D3D12_ENABLE_EXTENSION_NAME,
                .extensionVersion = XR_KHR_D3D12_enable_SPEC_VERSION,
            };
            if (j++ < capacity && properties) properties[j - 1] = d3d11_enable;
            if (j++ < capacity && properties) properties[j - 1] = d3d12_enable;
        }

        if (!strcmp( property.extensionName, "XR_KHR_convert_timespec_time" ))
        {
            strcpy( property.extensionName, XR_KHR_WIN32_CONVERT_PERFORMANCE_COUNTER_TIME_EXTENSION_NAME );
            property.extensionVersion = XR_KHR_win32_convert_performance_counter_time_SPEC_VERSION;
        }

        if (j++ < capacity && properties) properties[j - 1] = property;
    }

    if (i == host_count) *count = j;
    else *count = host_count + 2;

done:
    free( host_properties );
    return res;
}


XrResult wine_xrCreateInstance( const XrInstanceCreateInfo *create_info, XrInstance *ret )
{
    XrInstance host_instance, client_instance = *ret;
    struct openxr_instance *instance;
    XrResult res;

    TRACE( "create_info %p, client_instance %p\n", create_info, client_instance );

    if (!(instance = calloc( 1, sizeof(*instance) ))) return XR_ERROR_OUT_OF_MEMORY;
    if ((res = p_xrCreateInstance( create_info, &host_instance )))
    {
        free( instance );
        return res;
    }

    openxr_object_init_ptr( &instance->obj, (UINT_PTR)host_instance, &client_instance->obj );
#define USE_XR_FUNC( name )                                                                        \
    if ((res = p_xrGetInstanceProcAddr( instance->host.instance, #name,                            \
                                        (PFN_xrVoidFunction *)&instance->p_##name )))              \
        goto failed;
    ALL_XR_INSTANCE_FUNCS
#undef USE_XR_FUNC

    return res;

failed:
    p_xrDestroyInstance( host_instance );
    free( instance );
    return res;
}

XrResult wine_xrDestroyInstance( XrInstance client_instance )
{
    struct openxr_instance *instance = openxr_instance_from_handle( client_instance );

    TRACE( "instance %p\n", instance );

    instance->p_xrDestroyInstance( instance->host.instance );
    free( instance );

    return XR_SUCCESS;
}

XrResult wine_xrCreateVulkanDeviceKHR( XrInstance client_instance, const XrVulkanDeviceCreateInfoKHR *create_info,
                                       VkDevice *vulkan_device, VkResult *vulkan_result )
{
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XrResult wine_xrCreateVulkanInstanceKHR( XrInstance client_instance, const XrVulkanInstanceCreateInfoKHR *create_info,
                                         VkInstance *vulkan_instance, VkResult *vulkan_result )
{
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XrResult wine_xrGetVulkanGraphicsDeviceKHR( XrInstance client_instance, XrSystemId system_id, VkInstance vulkan_instance,
                                            VkPhysicalDevice *vulkan_physical_device )
{
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XrResult wine_xrGetVulkanGraphicsDevice2KHR( XrInstance client_instance, const XrVulkanGraphicsDeviceGetInfoKHR *get_info,
                                             VkPhysicalDevice *vulkan_physical_device )
{
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}


XrResult wine_xrCreateSession( XrInstance client_instance, const XrSessionCreateInfo *create_info, XrSession *ret )
{
    struct openxr_instance *instance = openxr_instance_from_handle( client_instance );
    XrSession host_session, client_session = *ret;
    struct openxr_session *session;
    XrResult res;

    if (!(session = calloc( 1, sizeof(*session) ))) return XR_ERROR_OUT_OF_MEMORY;
    if ((res = instance->p_xrCreateSession( instance->host.instance, create_info, &host_session )))
    {
        free( session );
        return res;
    }

    openxr_object_init_ptr( &session->obj, (UINT_PTR)host_session, &client_session->obj );
    session->instance = instance;
    return res;
}

XrResult wine_xrDestroySession( XrSession client_session )
{
    struct openxr_session *session = openxr_session_from_handle( client_session );
    struct openxr_instance *instance = session->instance;

    instance->p_xrDestroySession( session->host.session );
    free( session );

    return XR_SUCCESS;
}


XrResult wine_xrCreateSwapchain( XrSession client_session, const XrSwapchainCreateInfo *create_info, XrSwapchain *ret )
{
    struct openxr_session *session = openxr_session_from_handle( client_session );
    struct openxr_instance *instance = session->instance;
    XrSwapchain host_swapchain, client_swapchain = *ret;
    struct openxr_swapchain *swapchain;
    XrResult res;

    if (!(swapchain = calloc( 1, sizeof(*swapchain) ))) return XR_ERROR_OUT_OF_MEMORY;
    if ((res = instance->p_xrCreateSwapchain( session->host.session, create_info, &host_swapchain )))
    {
        free( swapchain );
        return res;
    }

    openxr_object_init_ptr( &swapchain->obj, (UINT_PTR)host_swapchain, &client_swapchain->obj );
    swapchain->instance = instance;
    return res;
}

XrResult wine_xrDestroySwapchain( XrSwapchain client_swapchain )
{
    struct openxr_swapchain *swapchain = openxr_swapchain_from_handle( client_swapchain );
    struct openxr_instance *instance = swapchain->instance;

    instance->p_xrDestroySwapchain( swapchain->host.swapchain );
    free( swapchain );

    return XR_SUCCESS;
}


XrResult wine_xrCreateActionSpace( XrSession client_session, const XrActionSpaceCreateInfo *create_info, XrSpace *ret )
{
    struct openxr_session *session = openxr_session_from_handle( client_session );
    struct openxr_instance *instance = session->instance;
    XrSpace host_space, client_space = *ret;
    struct openxr_space *space;
    XrResult res;

    if (!(space = calloc( 1, sizeof(*space) ))) return XR_ERROR_OUT_OF_MEMORY;
    if ((res = instance->p_xrCreateActionSpace( session->host.session, create_info, &host_space )))
    {
        free( space );
        return res;
    }

    openxr_object_init_ptr( &space->obj, (UINT_PTR)host_space, &client_space->obj );
    space->instance = instance;
    return res;
}

XrResult wine_xrCreateReferenceSpace( XrSession client_session, const XrReferenceSpaceCreateInfo *create_info,
                                      XrSpace *ret )
{
    struct openxr_session *session = openxr_session_from_handle( client_session );
    struct openxr_instance *instance = session->instance;
    XrSpace host_space, client_space = *ret;
    struct openxr_space *space;
    XrResult res;

    if (!(space = calloc( 1, sizeof(*space) ))) return XR_ERROR_OUT_OF_MEMORY;
    if ((res = instance->p_xrCreateReferenceSpace( session->host.session, create_info, &host_space )))
    {
        free( space );
        return res;
    }

    openxr_object_init_ptr( &space->obj, (UINT_PTR)host_space, &client_space->obj );
    space->instance = instance;
    return res;
}

XrResult wine_xrDestroySpace( XrSpace client_space )
{
    struct openxr_space *space = openxr_space_from_handle( client_space );
    struct openxr_instance *instance = space->instance;

    instance->p_xrDestroySpace( space->host.space );
    free( space );

    return XR_SUCCESS;
}


XrResult wine_xrCreateActionSet( XrInstance client_instance, const XrActionSetCreateInfo *create_info, XrActionSet *ret )
{
    struct openxr_instance *instance = openxr_instance_from_handle( client_instance );
    XrActionSet host_action_set, client_action_set = *ret;
    struct openxr_action_set *action_set;
    XrResult res;

    if (!(action_set = calloc( 1, sizeof(*action_set) ))) return XR_ERROR_OUT_OF_MEMORY;
    if ((res = instance->p_xrCreateActionSet( instance->host.instance, create_info, &host_action_set )))
    {
        free( action_set );
        return res;
    }

    openxr_object_init_ptr( &action_set->obj, (UINT_PTR)host_action_set, &client_action_set->obj );
    action_set->instance = instance;
    return res;
}

XrResult wine_xrDestroyActionSet( XrActionSet client_action_set )
{
    struct openxr_action_set *action_set = openxr_action_set_from_handle( client_action_set );
    struct openxr_instance *instance = action_set->instance;

    instance->p_xrDestroyActionSet( action_set->host.action_set );
    free( action_set );

    return XR_SUCCESS;
}


XrResult wine_xrCreateAction( XrActionSet client_action_set, const XrActionCreateInfo *create_info, XrAction *ret )
{
    struct openxr_action_set *action_set = openxr_action_set_from_handle( client_action_set );
    struct openxr_instance *instance = action_set->instance;
    XrAction host_action, client_action = *ret;
    struct openxr_action *action;
    XrResult res;

    if (!(action = calloc( 1, sizeof(*action) ))) return XR_ERROR_OUT_OF_MEMORY;
    if ((res = instance->p_xrCreateAction( action_set->host.action_set, create_info, &host_action )))
    {
        free( action );
        return res;
    }

    openxr_object_init_ptr( &action->obj, (UINT_PTR)host_action, &client_action->obj );
    action->instance = instance;
    return res;
}

XrResult wine_xrDestroyAction( XrAction client_action )
{
    struct openxr_action *action = openxr_action_from_handle( client_action );
    struct openxr_instance *instance = action->instance;

    instance->p_xrDestroyAction( action->host.action );
    free( action );

    return XR_SUCCESS;
}


#ifdef _WIN64

NTSTATUS xr_is_available_instance_function( void *arg )
{
    struct is_available_xr_instance_function_params *params = arg;
    struct openxr_instance *instance = openxr_instance_from_handle( params->instance );
    PFN_xrVoidFunction func = NULL;
    XrResult res = instance->p_xrGetInstanceProcAddr( instance->host.instance, params->name, &func );
    return !res && !!func;
}

#endif /* _WIN64 */

NTSTATUS xr_is_available_instance_function32( void *arg )
{
    struct
    {
        UINT32 instance;
        UINT32 name;
    } *params = arg;
    struct openxr_instance *instance = openxr_instance_from_handle( UlongToPtr( params->instance ) );
    PFN_xrVoidFunction func = NULL;
    XrResult res = instance->p_xrGetInstanceProcAddr( instance->host.instance, UlongToPtr( params->name ), &func );
    return !res && !!func;
}
