/* WinRT Windows.Gaming.Input implementation
 *
 * Copyright 2022 Rémi Bernon for CodeWeavers
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

#include "private.h"
#include "provider.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(input);

static CRITICAL_SECTION manager_cs;
static CRITICAL_SECTION_DEBUG manager_cs_debug =
{
    0, 0, &manager_cs,
    { &manager_cs_debug.ProcessLocksList, &manager_cs_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": manager_cs") }
};
static CRITICAL_SECTION manager_cs = { &manager_cs_debug, -1, 0, 0, 0, 0 };

static struct list controller_list = LIST_INIT( controller_list );

struct controller
{
    IGameController IGameController_iface;
    IGameControllerBatteryInfo IGameControllerBatteryInfo_iface;
    IAgileObject IAgileObject_iface;
    IInspectable *IInspectable_inner;
    const WCHAR *class_name;
    LONG refcount;

    struct list entry;
    IGameControllerProvider *provider;
    ICustomGameControllerFactory *factory;
};

static void controller_destroy( struct controller *impl )
{
    IInspectable_Release( impl->IInspectable_inner );
    ICustomGameControllerFactory_Release( impl->factory );
    IGameControllerProvider_Release( impl->provider );
    free( impl );
}

WIDL_impl_from_IGameController( controller );

static HRESULT WINAPI controller_QueryInterface( IGameController *iface, REFIID iid, void **out )
{
    struct controller *impl = controller_from_IGameController( iface );
    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );
    WIDL_impl_QueryInterface_IGameController( impl, iid, out, IGameController_iface );
    WIDL_impl_QueryInterface_IGameControllerBatteryInfo( impl, iid, out, IGameControllerBatteryInfo_iface );
    WIDL_impl_QueryInterface_IAgileObject( impl, iid, out, IAgileObject_iface );
    return IInspectable_QueryInterface( impl->IInspectable_inner, iid, out );
}

WIDL_impl_IUnknown_AddRef( controller, IGameController );
WIDL_impl_IUnknown_Release( controller, IGameController );

static HRESULT WINAPI controller_GetIids( IGameController *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI controller_GetRuntimeClassName( IGameController *iface, HSTRING *class_name )
{
    struct controller *impl = controller_from_IGameController( iface );
    return IInspectable_GetRuntimeClassName( impl->IInspectable_inner, class_name );
}

static HRESULT WINAPI controller_GetTrustLevel( IGameController *iface, TrustLevel *trust_level )
{
    struct controller *impl = controller_from_IGameController( iface );
    return IInspectable_GetTrustLevel( impl->IInspectable_inner, trust_level );
}

static HRESULT WINAPI controller_add_HeadsetConnected( IGameController *iface, ITypedEventHandler_IGameController_Headset *handler,
                                                       EventRegistrationToken *token )
{
    FIXME( "iface %p, handler %p, token %p stub!\n", iface, handler, token );
    return E_NOTIMPL;
}

static HRESULT WINAPI controller_remove_HeadsetConnected( IGameController *iface, EventRegistrationToken token )
{
    FIXME( "iface %p, token %I64x stub!\n", iface, token.value );
    return E_NOTIMPL;
}

static HRESULT WINAPI controller_add_HeadsetDisconnected( IGameController *iface, ITypedEventHandler_IGameController_Headset *handler,
                                                          EventRegistrationToken *token )
{
    FIXME( "iface %p, handler %p, token %p stub!\n", iface, handler, token );
    return E_NOTIMPL;
}

static HRESULT WINAPI controller_remove_HeadsetDisconnected( IGameController *iface, EventRegistrationToken token )
{
    FIXME( "iface %p, token %I64x stub!\n", iface, token.value );
    return E_NOTIMPL;
}

static HRESULT WINAPI controller_add_UserChanged( IGameController *iface,
                                                  ITypedEventHandler_IGameController_UserChangedEventArgs *handler,
                                                  EventRegistrationToken *token )
{
    FIXME( "iface %p, handler %p, token %p stub!\n", iface, handler, token );
    return E_NOTIMPL;
}

static HRESULT WINAPI controller_remove_UserChanged( IGameController *iface, EventRegistrationToken token )
{
    FIXME( "iface %p, token %I64x stub!\n", iface, token.value );
    return E_NOTIMPL;
}

static HRESULT WINAPI controller_get_Headset( IGameController *iface, IHeadset **value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static HRESULT WINAPI controller_get_IsWireless( IGameController *iface, boolean *value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static HRESULT WINAPI controller_get_User( IGameController *iface, __x_ABI_CWindows_CSystem_CIUser **value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

WIDL_impl_IGameControllerVtbl( controller );

WIDL_impl_base_IGameControllerBatteryInfo( controller, IGameController, &object->IGameController_iface );

static HRESULT WINAPI controller_IGameControllerBatteryInfo_TryGetBatteryReport( IGameControllerBatteryInfo *iface, IBatteryReport **value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

WIDL_impl_IGameControllerBatteryInfoVtbl( controller_IGameControllerBatteryInfo );

WIDL_impl_base_IAgileObject( controller, IGameController, &object->IGameController_iface );
WIDL_impl_IAgileObjectVtbl( controller_IAgileObject );

struct manager_statics
{
    IActivationFactory IActivationFactory_iface;
    IGameControllerFactoryManagerStatics IGameControllerFactoryManagerStatics_iface;
    IGameControllerFactoryManagerStatics2 IGameControllerFactoryManagerStatics2_iface;
    IAgileObject IAgileObject_iface;
    const WCHAR *class_name;
};

WIDL_impl_static_IActivationFactory( manager_statics,
    IGameControllerFactoryManagerStatics,
    IGameControllerFactoryManagerStatics2,
    IAgileObject,
    END, FIXME
);

static HRESULT WINAPI manager_statics_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    FIXME( "iface %p, instance %p stub!\n", iface, instance );
    return E_NOTIMPL;
}

WIDL_impl_IActivationFactoryVtbl( manager_statics );

static HRESULT WINAPI
manager_statics_IGameControllerFactoryManagerStatics_RegisterCustomFactoryForGipInterface( IGameControllerFactoryManagerStatics *iface,
                                              ICustomGameControllerFactory *factory,
                                              GUID interface_id )
{
    FIXME( "iface %p, factory %p, interface_id %s stub!\n", iface, factory, debugstr_guid(&interface_id) );
    return E_NOTIMPL;
}

static HRESULT WINAPI
manager_statics_IGameControllerFactoryManagerStatics_RegisterCustomFactoryForHardwareId( IGameControllerFactoryManagerStatics *iface,
                                            ICustomGameControllerFactory *factory,
                                            UINT16 vendor_id, UINT16 product_id )
{
    FIXME( "iface %p, factory %p, vendor_id %u, product_id %u stub!\n", iface, factory, vendor_id, product_id );
    return E_NOTIMPL;
}

static HRESULT WINAPI
manager_statics_IGameControllerFactoryManagerStatics_RegisterCustomFactoryForXusbType( IGameControllerFactoryManagerStatics *iface,
                                          ICustomGameControllerFactory *factory,
                                          XusbDeviceType type, XusbDeviceSubtype subtype )
{
    FIXME( "iface %p, factory %p, type %d, subtype %d stub!\n", iface, factory, type, subtype );
    return E_NOTIMPL;
}

WIDL_impl_IGameControllerFactoryManagerStaticsVtbl( manager_statics_IGameControllerFactoryManagerStatics );

static HRESULT WINAPI
manager_statics_IGameControllerFactoryManagerStatics2_TryGetFactoryControllerFromGameController( IGameControllerFactoryManagerStatics2 *iface,
                                                    ICustomGameControllerFactory *factory,
                                                    IGameController *controller, IGameController **value )
{
    struct controller *entry, *other;
    IGameController *tmp_controller;
    BOOL found = FALSE;

    TRACE( "iface %p, factory %p, controller %p, value %p.\n", iface, factory, controller, value );

    /* Spider Man Remastered passes a IRawGameController instead of IGameController, query the iface again */
    if (FAILED(IGameController_QueryInterface( controller, &IID_IGameController, (void **)&tmp_controller ))) goto done;

    EnterCriticalSection( &manager_cs );

    LIST_FOR_EACH_ENTRY( entry, &controller_list, struct controller, entry )
        if ((found = &entry->IGameController_iface == tmp_controller)) break;

    if (!found) WARN( "Failed to find controller %p\n", controller );
    else
    {
        LIST_FOR_EACH_ENTRY( other, &controller_list, struct controller, entry )
            if ((found = entry->provider == other->provider && other->factory == factory)) break;
        if (!found) WARN( "Failed to find controller %p, factory %p\n", controller, factory );
        else IGameController_AddRef( (*value = &other->IGameController_iface) );
    }

    LeaveCriticalSection( &manager_cs );

    IGameController_Release( tmp_controller );

done:
    if (!found) *value = NULL;
    return S_OK;
}

WIDL_impl_IGameControllerFactoryManagerStatics2Vtbl( manager_statics_IGameControllerFactoryManagerStatics2 );
WIDL_impl_IAgileObjectVtbl( manager_statics_IAgileObject );

static struct manager_statics manager_statics =
{
    {&manager_statics_vtbl},
    {&manager_statics_IGameControllerFactoryManagerStatics_vtbl},
    {&manager_statics_IGameControllerFactoryManagerStatics2_vtbl},
    {&manager_statics_IAgileObject_vtbl},
    RuntimeClass_Windows_Gaming_Input_Custom_GameControllerFactoryManager,
};

IGameControllerFactoryManagerStatics2 *manager_factory = &manager_statics.IGameControllerFactoryManagerStatics2_iface;

static HRESULT controller_create( ICustomGameControllerFactory *factory, IGameControllerProvider *provider,
                                  struct controller **out )
{
    IGameControllerImpl *inner_impl;
    struct controller *impl;
    HRESULT hr;

    if (!(impl = malloc(sizeof(*impl)))) return E_OUTOFMEMORY;
    impl->IGameController_iface.lpVtbl = &controller_vtbl;
    impl->IGameControllerBatteryInfo_iface.lpVtbl = &controller_IGameControllerBatteryInfo_vtbl;
    impl->IAgileObject_iface.lpVtbl = &controller_IAgileObject_vtbl;
    impl->refcount = 1;

    if (FAILED(hr = ICustomGameControllerFactory_CreateGameController( factory, provider, &impl->IInspectable_inner )))
        WARN( "Failed to create game controller, hr %#lx\n", hr );
    else if (FAILED(hr = IInspectable_QueryInterface( impl->IInspectable_inner, &IID_IGameControllerImpl, (void **)&inner_impl )))
        WARN( "Failed to find IGameControllerImpl iface, hr %#lx\n", hr );
    else
    {
        if (FAILED(hr = IGameControllerImpl_Initialize( inner_impl, &impl->IGameController_iface, provider )))
            WARN( "Failed to initialize game controller, hr %#lx\n", hr );
        IGameControllerImpl_Release( inner_impl );
    }

    if (FAILED(hr))
    {
        if (impl->IInspectable_inner) IInspectable_Release( impl->IInspectable_inner );
        free( impl );
        return hr;
    }

    ICustomGameControllerFactory_AddRef( (impl->factory = factory) );
    IGameControllerProvider_AddRef( (impl->provider = provider) );

    *out = impl;
    return S_OK;
}

void manager_on_provider_created( IGameControllerProvider *provider )
{
    IWineGameControllerProvider *wine_provider;
    struct list *entry, *next, *list;
    struct controller *controller;
    WineGameControllerType type;
    HRESULT hr;

    TRACE( "provider %p\n", provider );

    if (FAILED(IGameControllerProvider_QueryInterface( provider, &IID_IWineGameControllerProvider,
                                                       (void **)&wine_provider )))
    {
        FIXME( "IWineGameControllerProvider isn't implemented by provider %p\n", provider );
        return;
    }
    if (FAILED(hr = IWineGameControllerProvider_get_Type( wine_provider, &type )))
    {
        WARN( "Failed to get controller type, hr %#lx\n", hr );
        type = WineGameControllerType_Joystick;
    }
    IWineGameControllerProvider_Release( wine_provider );

    EnterCriticalSection( &manager_cs );

    if (list_empty( &controller_list )) list = &controller_list;
    else list = list_tail( &controller_list );

    if (SUCCEEDED(controller_create( controller_factory, provider, &controller )))
        list_add_tail( &controller_list, &controller->entry );

    switch (type)
    {
    case WineGameControllerType_Joystick: break;
    case WineGameControllerType_Gamepad:
        if (SUCCEEDED(controller_create( gamepad_factory, provider, &controller )))
            list_add_tail( &controller_list, &controller->entry );
        break;
    case WineGameControllerType_RacingWheel:
        if (SUCCEEDED(controller_create( racing_wheel_factory, provider, &controller )))
            list_add_tail( &controller_list, &controller->entry );
        break;
    }

    LIST_FOR_EACH_SAFE( entry, next, list )
    {
        controller = LIST_ENTRY( entry, struct controller, entry );
        hr = ICustomGameControllerFactory_OnGameControllerAdded( controller->factory,
                                                                 &controller->IGameController_iface );
        if (FAILED(hr)) WARN( "OnGameControllerAdded failed, hr %#lx\n", hr );
        if (next == &controller_list) break;
    }

    LeaveCriticalSection( &manager_cs );
}

void manager_on_provider_removed( IGameControllerProvider *provider )
{
    struct controller *controller, *next;

    TRACE( "provider %p\n", provider );

    EnterCriticalSection( &manager_cs );

    LIST_FOR_EACH_ENTRY( controller, &controller_list, struct controller, entry )
    {
        if (controller->provider != provider) continue;
        ICustomGameControllerFactory_OnGameControllerRemoved( controller->factory,
                                                              &controller->IGameController_iface );
    }

    LIST_FOR_EACH_ENTRY_SAFE( controller, next, &controller_list, struct controller, entry )
    {
        if (controller->provider != provider) continue;
        list_remove( &controller->entry );
        IGameController_Release( &controller->IGameController_iface );
    }

    LeaveCriticalSection( &manager_cs );
}
