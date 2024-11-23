/* WinRT Windows.Gaming.Input implementation
 *
 * Copyright 2021 Rémi Bernon for CodeWeavers
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

static CRITICAL_SECTION controller_cs;
static CRITICAL_SECTION_DEBUG controller_cs_debug =
{
    0, 0, &controller_cs,
    { &controller_cs_debug.ProcessLocksList, &controller_cs_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": controller_cs") }
};
static CRITICAL_SECTION controller_cs = { &controller_cs_debug, -1, 0, 0, 0, 0 };

static IVector_RawGameController *controllers;
static struct list controller_added_handlers = LIST_INIT( controller_added_handlers );
static struct list controller_removed_handlers = LIST_INIT( controller_removed_handlers );

static HRESULT init_controllers(void)
{
    static const struct vector_iids iids =
    {
        .vector = &IID_IVector_RawGameController,
        .view = &IID_IVectorView_RawGameController,
        .iterable = &IID_IIterable_RawGameController,
        .iterator = &IID_IIterator_RawGameController,
    };
    HRESULT hr;

    EnterCriticalSection( &controller_cs );
    if (controllers) hr = S_OK;
    else hr = vector_create( &iids, (void **)&controllers );
    LeaveCriticalSection( &controller_cs );

    return hr;
}

struct controller
{
    IGameControllerImpl IGameControllerImpl_iface;
    IGameControllerInputSink IGameControllerInputSink_iface;
    IRawGameController IRawGameController_iface;
    IRawGameController2 IRawGameController2_iface;
    IGameController *IGameController_outer;
    LONG refcount;

    IGameControllerProvider *provider;
    IWineGameControllerProvider *wine_provider;
};

WIDL_impl_from_IGameControllerImpl( controller );

static void controller_destroy( struct controller *impl )
{
    if (impl->wine_provider) IWineGameControllerProvider_Release( impl->wine_provider );
    IGameControllerProvider_Release( impl->provider );
    free( impl );
}

WIDL_impl_IUnknown_QueryInterface( controller,
    IGameControllerImpl,
    IGameControllerInputSink,
    IRawGameController,
    IRawGameController2,
    END, FIXME
);
WIDL_impl_IUnknown_AddRef( controller, IGameControllerImpl );
WIDL_impl_IUnknown_Release( controller, IGameControllerImpl );

static HRESULT WINAPI controller_GetIids( IGameControllerImpl *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI controller_GetRuntimeClassName( IGameControllerImpl *iface, HSTRING *class_name )
{
    return WindowsCreateString( RuntimeClass_Windows_Gaming_Input_RawGameController,
                                ARRAY_SIZE(RuntimeClass_Windows_Gaming_Input_RawGameController),
                                class_name );
}

static HRESULT WINAPI controller_GetTrustLevel( IGameControllerImpl *iface, TrustLevel *trust_level )
{
    FIXME( "iface %p, trust_level %p stub!\n", iface, trust_level );
    return E_NOTIMPL;
}

static HRESULT WINAPI controller_Initialize( IGameControllerImpl *iface, IGameController *outer,
                                             IGameControllerProvider *provider )
{
    struct controller *impl = controller_from_IGameControllerImpl( iface );
    HRESULT hr;

    TRACE( "iface %p, outer %p, provider %p.\n", iface, outer, provider );

    impl->IGameController_outer = outer;
    IGameControllerProvider_AddRef( (impl->provider = provider) );

    hr = IGameControllerProvider_QueryInterface( provider, &IID_IWineGameControllerProvider,
                                                 (void **)&impl->wine_provider );
    if (FAILED(hr)) return hr;

    EnterCriticalSection( &controller_cs );
    if (SUCCEEDED(hr = init_controllers()))
        hr = IVector_RawGameController_Append( controllers, &impl->IRawGameController_iface );
    LeaveCriticalSection( &controller_cs );

    return hr;
}

WIDL_impl_IGameControllerImplVtbl( controller );

DEFINE_IINSPECTABLE_OUTER( controller_IGameControllerInputSink, IGameControllerInputSink, controller, IGameController_outer )

static HRESULT WINAPI controller_IGameControllerInputSink_OnInputResumed( IGameControllerInputSink *iface, UINT64 timestamp )
{
    FIXME( "iface %p, timestamp %I64u stub!\n", iface, timestamp );
    return E_NOTIMPL;
}

static HRESULT WINAPI controller_IGameControllerInputSink_OnInputSuspended( IGameControllerInputSink *iface, UINT64 timestamp )
{
    FIXME( "iface %p, timestamp %I64u stub!\n", iface, timestamp );
    return E_NOTIMPL;
}

WIDL_impl_IGameControllerInputSinkVtbl( controller_IGameControllerInputSink );

DEFINE_IINSPECTABLE_OUTER( controller_IRawGameController, IRawGameController, controller, IGameController_outer )

static HRESULT WINAPI controller_IRawGameController_get_AxisCount( IRawGameController *iface, INT32 *value )
{
    struct controller *impl = controller_from_IRawGameController( iface );
    return IWineGameControllerProvider_get_AxisCount( impl->wine_provider, value );
}

static HRESULT WINAPI controller_IRawGameController_get_ButtonCount( IRawGameController *iface, INT32 *value )
{
    struct controller *impl = controller_from_IRawGameController( iface );
    return IWineGameControllerProvider_get_ButtonCount( impl->wine_provider, value );
}

static HRESULT WINAPI controller_IRawGameController_get_ForceFeedbackMotors( IRawGameController *iface,
                                                                             IVectorView_ForceFeedbackMotor **value )
{
    static const struct vector_iids iids =
    {
        .vector = &IID_IVector_ForceFeedbackMotor,
        .view = &IID_IVectorView_ForceFeedbackMotor,
        .iterable = &IID_IIterable_ForceFeedbackMotor,
        .iterator = &IID_IIterator_ForceFeedbackMotor,
    };
    struct controller *impl = controller_from_IRawGameController( iface );
    IVector_ForceFeedbackMotor *vector;
    IForceFeedbackMotor *motor;
    HRESULT hr;

    TRACE( "iface %p, value %p\n", iface, value );

    if (FAILED(hr = vector_create( &iids, (void **)&vector ))) return hr;

    if (SUCCEEDED(IWineGameControllerProvider_get_ForceFeedbackMotor( impl->wine_provider, &motor )) && motor)
    {
        hr = IVector_ForceFeedbackMotor_Append( vector, motor );
        IForceFeedbackMotor_Release( motor );
    }

    if (SUCCEEDED(hr)) hr = IVector_ForceFeedbackMotor_GetView( vector, value );
    IVector_ForceFeedbackMotor_Release( vector );

    return hr;
}

static HRESULT WINAPI controller_IRawGameController_get_HardwareProductId( IRawGameController *iface, UINT16 *value )
{
    struct controller *impl = controller_from_IRawGameController( iface );
    return IGameControllerProvider_get_HardwareProductId( impl->provider, value );
}

static HRESULT WINAPI controller_IRawGameController_get_HardwareVendorId( IRawGameController *iface, UINT16 *value )
{
    struct controller *impl = controller_from_IRawGameController( iface );
    return IGameControllerProvider_get_HardwareVendorId( impl->provider, value );
}

static HRESULT WINAPI controller_IRawGameController_get_SwitchCount( IRawGameController *iface, INT32 *value )
{
    struct controller *impl = controller_from_IRawGameController( iface );
    return IWineGameControllerProvider_get_SwitchCount( impl->wine_provider, value );
}

static HRESULT WINAPI controller_IRawGameController_GetButtonLabel( IRawGameController *iface, INT32 index,
                                                                    enum GameControllerButtonLabel *value )
{
    FIXME( "iface %p, index %d, value %p stub!\n", iface, index, value );
    return E_NOTIMPL;
}

static HRESULT WINAPI
controller_IRawGameController_GetCurrentReading( IRawGameController *iface, UINT32 buttons_size, BOOLEAN *buttons,
                                                 UINT32 switches_size, enum GameControllerSwitchPosition *switches,
                                                 UINT32 axes_size, DOUBLE *axes, UINT64 *timestamp )
{
    struct controller *impl = controller_from_IRawGameController( iface );
    WineGameControllerState state;
    HRESULT hr;

    TRACE( "iface %p, buttons_size %u, buttons %p, switches_size %u, switches %p, axes_size %u, axes %p, timestamp %p.\n",
           iface, buttons_size, buttons, switches_size, switches, axes_size, axes, timestamp );

    if (FAILED(hr = IWineGameControllerProvider_get_State( impl->wine_provider, &state ))) return hr;

    memcpy( axes, state.axes, axes_size * sizeof(*axes) );
    memcpy( buttons, state.buttons, buttons_size * sizeof(*buttons) );
    memcpy( switches, state.switches, switches_size * sizeof(*switches) );
    *timestamp = state.timestamp;

    return hr;
}

static HRESULT WINAPI controller_IRawGameController_GetSwitchKind( IRawGameController *iface, INT32 index,
                                                                   enum GameControllerSwitchKind *value )
{
    FIXME( "iface %p, index %d, value %p stub!\n", iface, index, value );
    return E_NOTIMPL;
}

WIDL_impl_IRawGameControllerVtbl( controller_IRawGameController );

DEFINE_IINSPECTABLE_OUTER( controller_IRawGameController2, IRawGameController2, controller, IGameController_outer )

static HRESULT WINAPI
controller_IRawGameController2_get_SimpleHapticsControllers( IRawGameController2 *iface,
                                                             IVectorView_SimpleHapticsController **value )
{
    static const struct vector_iids iids =
    {
        .vector = &IID_IVector_SimpleHapticsController,
        .view = &IID_IVectorView_SimpleHapticsController,
        .iterable = &IID_IIterable_SimpleHapticsController,
        .iterator = &IID_IIterator_SimpleHapticsController,
    };
    IVector_SimpleHapticsController *vector;
    HRESULT hr;

    FIXME( "iface %p, value %p stub!\n", iface, value );

    if (SUCCEEDED(hr = vector_create( &iids, (void **)&vector )))
    {
        hr = IVector_SimpleHapticsController_GetView( vector, value );
        IVector_SimpleHapticsController_Release( vector );
    }

    return hr;
}

static HRESULT WINAPI controller_IRawGameController2_get_NonRoamableId( IRawGameController2 *iface, HSTRING *value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static HRESULT WINAPI controller_IRawGameController2_get_DisplayName( IRawGameController2 *iface, HSTRING *value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

WIDL_impl_IRawGameController2Vtbl( controller_IRawGameController2 );

struct controller_statics
{
    IActivationFactory IActivationFactory_iface;
    IRawGameControllerStatics IRawGameControllerStatics_iface;
    ICustomGameControllerFactory ICustomGameControllerFactory_iface;
    IAgileObject IAgileObject_iface;
    LONG ref;
};

WIDL_impl_from_IActivationFactory( controller_statics );
WIDL_impl_IUnknown_QueryInterface( controller_statics,
    IActivationFactory,
    IRawGameControllerStatics,
    ICustomGameControllerFactory,
    IAgileObject,
    END, FIXME
);
WIDL_impl_static_IUnknown_AddRef( controller_statics, IActivationFactory );
WIDL_impl_static_IUnknown_Release( controller_statics, IActivationFactory );

static HRESULT WINAPI controller_statics_GetIids( IActivationFactory *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI controller_statics_GetRuntimeClassName( IActivationFactory *iface, HSTRING *class_name )
{
    FIXME( "iface %p, class_name %p stub!\n", iface, class_name );
    return E_NOTIMPL;
}

static HRESULT WINAPI controller_statics_GetTrustLevel( IActivationFactory *iface, TrustLevel *trust_level )
{
    FIXME( "iface %p, trust_level %p stub!\n", iface, trust_level );
    return E_NOTIMPL;
}

static HRESULT WINAPI controller_statics_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    FIXME( "iface %p, instance %p stub!\n", iface, instance );
    return E_NOTIMPL;
}

WIDL_impl_IActivationFactoryVtbl( controller_statics );

DEFINE_IINSPECTABLE( controller_statics_IRawGameControllerStatics, IRawGameControllerStatics,
                     controller_statics, IActivationFactory_iface )

static HRESULT WINAPI
controller_statics_IRawGameControllerStatics_add_RawGameControllerAdded( IRawGameControllerStatics *iface,
                                                                         IEventHandler_RawGameController *handler,
                                                                         EventRegistrationToken *token )
{
    TRACE( "iface %p, handler %p, token %p.\n", iface, handler, token );
    if (!handler) return E_INVALIDARG;
    return event_handlers_append( &controller_added_handlers, (IEventHandler_IInspectable *)handler, token );
}

static HRESULT WINAPI
controller_statics_IRawGameControllerStatics_remove_RawGameControllerAdded( IRawGameControllerStatics *iface, EventRegistrationToken token )
{
    TRACE( "iface %p, token %#I64x.\n", iface, token.value );
    return event_handlers_remove( &controller_added_handlers, &token );
}

static HRESULT WINAPI
controller_statics_IRawGameControllerStatics_add_RawGameControllerRemoved( IRawGameControllerStatics *iface,
                                                                           IEventHandler_RawGameController *handler,
                                                                           EventRegistrationToken *token )
{
    TRACE( "iface %p, handler %p, token %p.\n", iface, handler, token );
    if (!handler) return E_INVALIDARG;
    return event_handlers_append( &controller_removed_handlers, (IEventHandler_IInspectable *)handler, token );
}

static HRESULT WINAPI
controller_statics_IRawGameControllerStatics_remove_RawGameControllerRemoved( IRawGameControllerStatics *iface, EventRegistrationToken token )
{
    TRACE( "iface %p, token %#I64x.\n", iface, token.value );
    return event_handlers_remove( &controller_removed_handlers, &token );
}

static HRESULT WINAPI
controller_statics_IRawGameControllerStatics_get_RawGameControllers( IRawGameControllerStatics *iface,
                                                                     IVectorView_RawGameController **value )
{
    HRESULT hr;

    TRACE( "iface %p, value %p.\n", iface, value );

    EnterCriticalSection( &controller_cs );
    if (SUCCEEDED(hr = init_controllers()))
        hr = IVector_RawGameController_GetView( controllers, value );
    LeaveCriticalSection( &controller_cs );

    return hr;
}

static HRESULT WINAPI
controller_statics_IRawGameControllerStatics_FromGameController( IRawGameControllerStatics *iface, IGameController *game_controller,
                                                                 IRawGameController **value )
{
    struct controller_statics *impl = controller_statics_from_IRawGameControllerStatics( iface );
    IGameController *controller;
    HRESULT hr;

    TRACE( "iface %p, game_controller %p, value %p.\n", iface, game_controller, value );

    *value = NULL;
    hr = IGameControllerFactoryManagerStatics2_TryGetFactoryControllerFromGameController( manager_factory, &impl->ICustomGameControllerFactory_iface,
                                                                                          game_controller, &controller );
    if (FAILED(hr) || !controller) return hr;

    hr = IGameController_QueryInterface( controller, &IID_IRawGameController, (void **)value );
    IGameController_Release( controller );

    return hr;
}

WIDL_impl_IRawGameControllerStaticsVtbl( controller_statics_IRawGameControllerStatics );

DEFINE_IINSPECTABLE( controller_statics_ICustomGameControllerFactory, ICustomGameControllerFactory,
                     controller_statics, IActivationFactory_iface )

static HRESULT WINAPI
controller_statics_ICustomGameControllerFactory_CreateGameController( ICustomGameControllerFactory *iface,
                                                                      IGameControllerProvider *provider, IInspectable **value )
{
    struct controller *impl;

    TRACE( "iface %p, provider %p, value %p.\n", iface, provider, value );

    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IGameControllerImpl_iface.lpVtbl = &controller_vtbl;
    impl->IGameControllerInputSink_iface.lpVtbl = &controller_IGameControllerInputSink_vtbl;
    impl->IRawGameController_iface.lpVtbl = &controller_IRawGameController_vtbl;
    impl->IRawGameController2_iface.lpVtbl = &controller_IRawGameController2_vtbl;
    impl->refcount = 1;

    TRACE( "created RawGameController %p\n", impl );

    *value = (IInspectable *)&impl->IGameControllerImpl_iface;
    return S_OK;
}

static HRESULT WINAPI
controller_statics_ICustomGameControllerFactory_OnGameControllerAdded( ICustomGameControllerFactory *iface, IGameController *value )
{
    IRawGameController *controller;
    HRESULT hr;

    TRACE( "iface %p, value %p.\n", iface, value );

    if (FAILED(hr = IGameController_QueryInterface( value, &IID_IRawGameController, (void **)&controller )))
        return hr;
    event_handlers_notify( &controller_added_handlers, (IInspectable *)controller );
    IRawGameController_Release( controller );

    return S_OK;
}

static HRESULT WINAPI
controller_statics_ICustomGameControllerFactory_OnGameControllerRemoved( ICustomGameControllerFactory *iface, IGameController *value )
{
    IRawGameController *controller;
    BOOLEAN found;
    UINT32 index;
    HRESULT hr;

    TRACE( "iface %p, value %p.\n", iface, value );

    if (FAILED(hr = IGameController_QueryInterface( value, &IID_IRawGameController, (void **)&controller )))
        return hr;

    EnterCriticalSection( &controller_cs );
    if (SUCCEEDED(hr = init_controllers()))
    {
        if (FAILED(hr = IVector_RawGameController_IndexOf( controllers, controller, &index, &found )) || !found)
            WARN( "Could not find controller %p, hr %#lx!\n", controller, hr );
        else
            hr = IVector_RawGameController_RemoveAt( controllers, index );
    }
    LeaveCriticalSection( &controller_cs );

    if (FAILED(hr))
        WARN( "Failed to remove controller %p, hr %#lx!\n", controller, hr );
    else if (found)
    {
        TRACE( "Removed controller %p.\n", controller );
        event_handlers_notify( &controller_removed_handlers, (IInspectable *)controller );
    }
    IRawGameController_Release( controller );

    return S_OK;
}

WIDL_impl_ICustomGameControllerFactoryVtbl( controller_statics_ICustomGameControllerFactory );

DEFINE_IAGILEOBJECT( controller_statics, IActivationFactory, &object->IActivationFactory_iface );

static struct controller_statics controller_statics =
{
    {&controller_statics_vtbl},
    {&controller_statics_IRawGameControllerStatics_vtbl},
    {&controller_statics_ICustomGameControllerFactory_vtbl},
    {&controller_statics_IAgileObject_vtbl},
    1,
};

ICustomGameControllerFactory *controller_factory = &controller_statics.ICustomGameControllerFactory_iface;
