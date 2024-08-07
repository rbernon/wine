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

static CRITICAL_SECTION racing_wheel_cs;
static CRITICAL_SECTION_DEBUG racing_wheel_cs_debug =
{
    0, 0, &racing_wheel_cs,
    { &racing_wheel_cs_debug.ProcessLocksList, &racing_wheel_cs_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": racing_wheel_cs") }
};
static CRITICAL_SECTION racing_wheel_cs = { &racing_wheel_cs_debug, -1, 0, 0, 0, 0 };

static IVector_RacingWheel *racing_wheels;
static struct list racing_wheel_added_handlers = LIST_INIT( racing_wheel_added_handlers );
static struct list racing_wheel_removed_handlers = LIST_INIT( racing_wheel_removed_handlers );

static HRESULT init_racing_wheels(void)
{
    static const struct vector_iids iids =
    {
        .vector = &IID_IVector_RacingWheel,
        .view = &IID_IVectorView_RacingWheel,
        .iterable = &IID_IIterable_RacingWheel,
        .iterator = &IID_IIterator_RacingWheel,
    };
    HRESULT hr;

    EnterCriticalSection( &racing_wheel_cs );
    if (racing_wheels) hr = S_OK;
    else hr = vector_create( &iids, (void **)&racing_wheels );
    LeaveCriticalSection( &racing_wheel_cs );

    return hr;
}

struct racing_wheel
{
    IGameControllerImpl IGameControllerImpl_iface;
    IGameControllerInputSink IGameControllerInputSink_iface;
    IRacingWheel IRacingWheel_iface;
    IInspectable *outer;
    const WCHAR *class_name;
    LONG refcount;

    IGameControllerProvider *provider;
    IWineGameControllerProvider *wine_provider;
};

static void racing_wheel_destroy( struct racing_wheel *impl )
{
    if (impl->wine_provider) IWineGameControllerProvider_Release( impl->wine_provider );
    IGameControllerProvider_Release( impl->provider );
    free( impl );
}

WIDL_impl_outer_IGameControllerImpl( racing_wheel,
    IGameControllerInputSink,
    IRacingWheel,
    END, FIXME
);

static HRESULT WINAPI racing_wheel_Initialize( IGameControllerImpl *iface, IGameController *outer,
                                             IGameControllerProvider *provider )
{
    struct racing_wheel *impl = racing_wheel_from_IGameControllerImpl( iface );
    HRESULT hr;

    TRACE( "iface %p, outer %p, provider %p.\n", iface, outer, provider );

    impl->outer = (IInspectable *)outer;
    IGameControllerProvider_AddRef( (impl->provider = provider) );

    hr = IGameControllerProvider_QueryInterface( provider, &IID_IWineGameControllerProvider,
                                                 (void **)&impl->wine_provider );
    if (FAILED(hr)) return hr;

    EnterCriticalSection( &racing_wheel_cs );
    if (SUCCEEDED(hr = init_racing_wheels()))
        hr = IVector_RacingWheel_Append( racing_wheels, &impl->IRacingWheel_iface );
    LeaveCriticalSection( &racing_wheel_cs );

    return hr;
}

WIDL_impl_IGameControllerImplVtbl( racing_wheel );

static HRESULT WINAPI racing_wheel_IGameControllerInputSink_OnInputResumed( IGameControllerInputSink *iface, UINT64 timestamp )
{
    FIXME( "iface %p, timestamp %I64u stub!\n", iface, timestamp );
    return E_NOTIMPL;
}

static HRESULT WINAPI racing_wheel_IGameControllerInputSink_OnInputSuspended( IGameControllerInputSink *iface, UINT64 timestamp )
{
    FIXME( "iface %p, timestamp %I64u stub!\n", iface, timestamp );
    return E_NOTIMPL;
}

WIDL_impl_IGameControllerInputSinkVtbl( racing_wheel_IGameControllerInputSink );

static HRESULT WINAPI racing_wheel_IRacingWheel_get_HasClutch( IRacingWheel *iface, boolean *value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static HRESULT WINAPI racing_wheel_IRacingWheel_get_HasHandbrake( IRacingWheel *iface, boolean *value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static HRESULT WINAPI racing_wheel_IRacingWheel_get_HasPatternShifter( IRacingWheel *iface, boolean *value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static HRESULT WINAPI racing_wheel_IRacingWheel_get_MaxPatternShifterGear( IRacingWheel *iface, INT32 *value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static HRESULT WINAPI racing_wheel_IRacingWheel_get_MaxWheelAngle( IRacingWheel *iface, DOUBLE *value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static HRESULT WINAPI racing_wheel_IRacingWheel_get_WheelMotor( IRacingWheel *iface, IForceFeedbackMotor **value )
{
    struct racing_wheel *impl = racing_wheel_from_IRacingWheel( iface );

    TRACE( "iface %p, value %p\n", iface, value );

    return IWineGameControllerProvider_get_ForceFeedbackMotor( impl->wine_provider, value );
}

static HRESULT WINAPI racing_wheel_IRacingWheel_GetButtonLabel( IRacingWheel *iface, enum RacingWheelButtons button,
                                                   enum GameControllerButtonLabel *value )
{
    FIXME( "iface %p, button %d, value %p stub!\n", iface, button, value );
    return E_NOTIMPL;
}

static HRESULT WINAPI racing_wheel_IRacingWheel_GetCurrentReading( IRacingWheel *iface, struct RacingWheelReading *value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

WIDL_impl_IRacingWheelVtbl( racing_wheel_IRacingWheel );

struct racing_wheel_statics
{
    IActivationFactory IActivationFactory_iface;
    IRacingWheelStatics IRacingWheelStatics_iface;
    IRacingWheelStatics2 IRacingWheelStatics2_iface;
    ICustomGameControllerFactory ICustomGameControllerFactory_iface;
    IAgileObject IAgileObject_iface;
    const WCHAR *class_name;
};

WIDL_impl_static_IActivationFactory( racing_wheel_statics,
    IRacingWheelStatics,
    IRacingWheelStatics2,
    ICustomGameControllerFactory,
    IAgileObject,
    END, FIXME
);

static HRESULT WINAPI racing_wheel_statics_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    FIXME( "iface %p, instance %p stub!\n", iface, instance );
    return E_NOTIMPL;
}

WIDL_impl_IActivationFactoryVtbl( racing_wheel_statics );

static HRESULT WINAPI racing_wheel_statics_IRacingWheelStatics_add_RacingWheelAdded( IRacingWheelStatics *iface, IEventHandler_RacingWheel *handler,
                                                    EventRegistrationToken *token )
{
    TRACE( "iface %p, handler %p, token %p.\n", iface, handler, token );
    if (!handler) return E_INVALIDARG;
    return event_handlers_append( &racing_wheel_added_handlers, (IEventHandler_IInspectable *)handler, token );
}

static HRESULT WINAPI racing_wheel_statics_IRacingWheelStatics_remove_RacingWheelAdded( IRacingWheelStatics *iface, EventRegistrationToken token )
{
    TRACE( "iface %p, token %#I64x.\n", iface, token.value );
    return event_handlers_remove( &racing_wheel_added_handlers, &token );
}

static HRESULT WINAPI racing_wheel_statics_IRacingWheelStatics_add_RacingWheelRemoved( IRacingWheelStatics *iface, IEventHandler_RacingWheel *handler,
                                                      EventRegistrationToken *token )
{
    TRACE( "iface %p, handler %p, token %p.\n", iface, handler, token );
    if (!handler) return E_INVALIDARG;
    return event_handlers_append( &racing_wheel_removed_handlers, (IEventHandler_IInspectable *)handler, token );
}

static HRESULT WINAPI racing_wheel_statics_IRacingWheelStatics_remove_RacingWheelRemoved( IRacingWheelStatics *iface, EventRegistrationToken token )
{
    TRACE( "iface %p, token %#I64x.\n", iface, token.value );
    return event_handlers_remove( &racing_wheel_removed_handlers, &token );
}

static HRESULT WINAPI racing_wheel_statics_IRacingWheelStatics_get_RacingWheels( IRacingWheelStatics *iface, IVectorView_RacingWheel **value )
{
    HRESULT hr;

    TRACE( "iface %p, value %p.\n", iface, value );

    EnterCriticalSection( &racing_wheel_cs );
    if (SUCCEEDED(hr = init_racing_wheels())) hr = IVector_RacingWheel_GetView( racing_wheels, value );
    LeaveCriticalSection( &racing_wheel_cs );

    return hr;
}

WIDL_impl_IRacingWheelStaticsVtbl( racing_wheel_statics_IRacingWheelStatics );

static HRESULT WINAPI racing_wheel_statics_IRacingWheelStatics2_FromGameController( IRacingWheelStatics2 *iface, IGameController *game_controller, IRacingWheel **value )
{
    struct racing_wheel_statics *impl = racing_wheel_statics_from_IRacingWheelStatics2( iface );
    IGameController *controller;
    HRESULT hr;

    TRACE( "iface %p, game_controller %p, value %p.\n", iface, game_controller, value );

    *value = NULL;
    hr = IGameControllerFactoryManagerStatics2_TryGetFactoryControllerFromGameController( manager_factory, &impl->ICustomGameControllerFactory_iface,
                                                                                          game_controller, &controller );
    if (FAILED(hr) || !controller) return hr;

    hr = IGameController_QueryInterface( controller, &IID_IRacingWheel, (void **)value );
    IGameController_Release( controller );
    return hr;
}

WIDL_impl_IRacingWheelStatics2Vtbl( racing_wheel_statics_IRacingWheelStatics2 );

static HRESULT WINAPI racing_wheel_statics_ICustomGameControllerFactory_CreateGameController( ICustomGameControllerFactory *iface, IGameControllerProvider *provider,
                                                               IInspectable **value )
{
    struct racing_wheel *impl;

    TRACE( "iface %p, provider %p, value %p.\n", iface, provider, value );

    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IGameControllerImpl_iface.lpVtbl = &racing_wheel_vtbl;
    impl->IGameControllerInputSink_iface.lpVtbl = &racing_wheel_IGameControllerInputSink_vtbl;
    impl->IRacingWheel_iface.lpVtbl = &racing_wheel_IRacingWheel_vtbl;
    impl->class_name = RuntimeClass_Windows_Gaming_Input_RacingWheel;
    impl->refcount = 1;

    TRACE( "created RacingWheel %p\n", impl );

    *value = (IInspectable *)&impl->IGameControllerImpl_iface;
    return S_OK;
}

static HRESULT WINAPI racing_wheel_statics_ICustomGameControllerFactory_OnGameControllerAdded( ICustomGameControllerFactory *iface, IGameController *value )
{
    IRacingWheel *racing_wheel;
    HRESULT hr;

    TRACE( "iface %p, value %p.\n", iface, value );

    if (FAILED(hr = IGameController_QueryInterface( value, &IID_IRacingWheel, (void **)&racing_wheel )))
        return hr;
    event_handlers_notify( &racing_wheel_added_handlers, (IInspectable *)racing_wheel );
    IRacingWheel_Release( racing_wheel );

    return S_OK;
}

static HRESULT WINAPI racing_wheel_statics_ICustomGameControllerFactory_OnGameControllerRemoved( ICustomGameControllerFactory *iface, IGameController *value )
{
    IRacingWheel *racing_wheel;
    BOOLEAN found;
    UINT32 index;
    HRESULT hr;

    TRACE( "iface %p, value %p.\n", iface, value );

    if (FAILED(hr = IGameController_QueryInterface( value, &IID_IRacingWheel, (void **)&racing_wheel )))
        return hr;

    EnterCriticalSection( &racing_wheel_cs );
    if (SUCCEEDED(hr = init_racing_wheels()))
    {
        if (FAILED(hr = IVector_RacingWheel_IndexOf( racing_wheels, racing_wheel, &index, &found )) || !found)
            WARN( "Could not find RacingWheel %p, hr %#lx!\n", racing_wheel, hr );
        else
            hr = IVector_RacingWheel_RemoveAt( racing_wheels, index );
    }
    LeaveCriticalSection( &racing_wheel_cs );

    if (FAILED(hr))
        WARN( "Failed to remove RacingWheel %p, hr %#lx!\n", racing_wheel, hr );
    else if (found)
    {
        TRACE( "Removed RacingWheel %p.\n", racing_wheel );
        event_handlers_notify( &racing_wheel_removed_handlers, (IInspectable *)racing_wheel );
    }
    IRacingWheel_Release( racing_wheel );

    return S_OK;
}

WIDL_impl_ICustomGameControllerFactoryVtbl( racing_wheel_statics_ICustomGameControllerFactory );
WIDL_impl_IAgileObjectVtbl( racing_wheel_statics_IAgileObject );

static struct racing_wheel_statics racing_wheel_statics =
{
    {&racing_wheel_statics_vtbl},
    {&racing_wheel_statics_IRacingWheelStatics_vtbl},
    {&racing_wheel_statics_IRacingWheelStatics2_vtbl},
    {&racing_wheel_statics_ICustomGameControllerFactory_vtbl},
    {&racing_wheel_statics_IAgileObject_vtbl},
    RuntimeClass_Windows_Gaming_Input_RacingWheel,
};

ICustomGameControllerFactory *racing_wheel_factory = &racing_wheel_statics.ICustomGameControllerFactory_iface;
