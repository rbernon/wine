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

static CRITICAL_SECTION gamepad_cs;
static CRITICAL_SECTION_DEBUG gamepad_cs_debug =
{
    0, 0, &gamepad_cs,
    { &gamepad_cs_debug.ProcessLocksList, &gamepad_cs_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": gamepad_cs") }
};
static CRITICAL_SECTION gamepad_cs = { &gamepad_cs_debug, -1, 0, 0, 0, 0 };

static IVector_Gamepad *gamepads;
static struct list gamepad_added_handlers = LIST_INIT( gamepad_added_handlers );
static struct list gamepad_removed_handlers = LIST_INIT( gamepad_removed_handlers );

static HRESULT init_gamepads(void)
{
    static const struct vector_iids iids =
    {
        .vector = &IID_IVector_Gamepad,
        .view = &IID_IVectorView_Gamepad,
        .iterable = &IID_IIterable_Gamepad,
        .iterator = &IID_IIterator_Gamepad,
    };
    HRESULT hr;

    EnterCriticalSection( &gamepad_cs );
    if (gamepads) hr = S_OK;
    else hr = vector_create( &iids, (void **)&gamepads );
    LeaveCriticalSection( &gamepad_cs );

    return hr;
}

struct gamepad
{
    IGameControllerImpl IGameControllerImpl_iface;
    IGameControllerInputSink IGameControllerInputSink_iface;
    IGamepad IGamepad_iface;
    IGamepad2 IGamepad2_iface;
    IInspectable *outer;
    const WCHAR *class_name;
    LONG refcount;

    IGameControllerProvider *provider;
    IWineGameControllerProvider *wine_provider;

    struct WineGameControllerState initial_state;
    BOOL state_changed;
};

static void gamepad_destroy( struct gamepad *impl )
{
    if (impl->wine_provider) IWineGameControllerProvider_Release( impl->wine_provider );
    IGameControllerProvider_Release( impl->provider );
    free( impl );
}

WIDL_impl_outer_IGameControllerImpl( gamepad,
    IGameControllerInputSink,
    IGamepad,
    IGamepad2,
    END, FIXME
);

static HRESULT WINAPI gamepad_Initialize( IGameControllerImpl *iface, IGameController *outer,
                                             IGameControllerProvider *provider )
{
    struct gamepad *impl = gamepad_from_IGameControllerImpl( iface );
    HRESULT hr;

    TRACE( "iface %p, outer %p, provider %p.\n", iface, outer, provider );

    impl->outer = (IInspectable *)outer;
    IGameControllerProvider_AddRef( (impl->provider = provider) );

    hr = IGameControllerProvider_QueryInterface( provider, &IID_IWineGameControllerProvider,
                                                 (void **)&impl->wine_provider );

    if (SUCCEEDED(hr))
        hr = IWineGameControllerProvider_get_State( impl->wine_provider, &impl->initial_state );

    if (FAILED(hr)) return hr;

    EnterCriticalSection( &gamepad_cs );
    if (SUCCEEDED(hr = init_gamepads()))
        hr = IVector_Gamepad_Append( gamepads, &impl->IGamepad_iface );
    LeaveCriticalSection( &gamepad_cs );

    return hr;
}

WIDL_impl_IGameControllerImplVtbl( gamepad );

static HRESULT WINAPI gamepad_IGameControllerInputSink_OnInputResumed( IGameControllerInputSink *iface, UINT64 timestamp )
{
    FIXME( "iface %p, timestamp %I64u stub!\n", iface, timestamp );
    return E_NOTIMPL;
}

static HRESULT WINAPI gamepad_IGameControllerInputSink_OnInputSuspended( IGameControllerInputSink *iface, UINT64 timestamp )
{
    FIXME( "iface %p, timestamp %I64u stub!\n", iface, timestamp );
    return E_NOTIMPL;
}

WIDL_impl_IGameControllerInputSinkVtbl( gamepad_IGameControllerInputSink );

static HRESULT WINAPI gamepad_IGamepad_get_Vibration( IGamepad *iface, struct GamepadVibration *value )
{
    struct gamepad *impl = gamepad_from_IGamepad( iface );
    struct WineGameControllerVibration vibration;
    HRESULT hr;

    TRACE( "iface %p, value %p.\n", iface, value );

    if (FAILED(hr = IWineGameControllerProvider_get_Vibration( impl->wine_provider, &vibration ))) return hr;

    value->LeftMotor = vibration.rumble / 65535.;
    value->RightMotor = vibration.buzz / 65535.;
    value->LeftTrigger = vibration.left / 65535.;
    value->RightTrigger = vibration.right / 65535.;

    return S_OK;
}

static HRESULT WINAPI gamepad_IGamepad_put_Vibration( IGamepad *iface, struct GamepadVibration value )
{
    struct gamepad *impl = gamepad_from_IGamepad( iface );
    struct WineGameControllerVibration vibration =
    {
        .rumble = value.LeftMotor * 65535.,
        .buzz = value.RightMotor * 65535.,
        .left = value.LeftTrigger * 65535.,
        .right = value.RightTrigger * 65535.,
    };

    TRACE( "iface %p, value %p.\n", iface, &value );

    return IWineGameControllerProvider_put_Vibration( impl->wine_provider, vibration );
}

static HRESULT WINAPI gamepad_IGamepad_GetCurrentReading( IGamepad *iface, struct GamepadReading *value )
{
    struct gamepad *impl = gamepad_from_IGamepad( iface );
    struct WineGameControllerState state;
    HRESULT hr;

    TRACE( "iface %p, value %p.\n", iface, value );

    if (FAILED(hr = IWineGameControllerProvider_get_State( impl->wine_provider, &state ))) return hr;

    memset(value, 0, sizeof(*value));
    if (impl->state_changed ||
        memcmp( impl->initial_state.axes, state.axes, sizeof(state) - offsetof(struct WineGameControllerState, axes)) )
    {
        impl->state_changed = TRUE;
        if (state.buttons[0]) value->Buttons |= GamepadButtons_A;
        if (state.buttons[1]) value->Buttons |= GamepadButtons_B;
        if (state.buttons[2]) value->Buttons |= GamepadButtons_X;
        if (state.buttons[3]) value->Buttons |= GamepadButtons_Y;
        if (state.buttons[4]) value->Buttons |= GamepadButtons_LeftShoulder;
        if (state.buttons[5]) value->Buttons |= GamepadButtons_RightShoulder;
        if (state.buttons[6]) value->Buttons |= GamepadButtons_View;
        if (state.buttons[7]) value->Buttons |= GamepadButtons_Menu;
        if (state.buttons[8]) value->Buttons |= GamepadButtons_LeftThumbstick;
        if (state.buttons[9]) value->Buttons |= GamepadButtons_RightThumbstick;

        switch (state.switches[0])
        {
        case GameControllerSwitchPosition_Up:
        case GameControllerSwitchPosition_UpRight:
        case GameControllerSwitchPosition_UpLeft:
            value->Buttons |= GamepadButtons_DPadUp;
            break;
        case GameControllerSwitchPosition_Down:
        case GameControllerSwitchPosition_DownRight:
        case GameControllerSwitchPosition_DownLeft:
            value->Buttons |= GamepadButtons_DPadDown;
            break;
        default:
            break;
        }

        switch (state.switches[0])
        {
        case GameControllerSwitchPosition_Right:
        case GameControllerSwitchPosition_UpRight:
        case GameControllerSwitchPosition_DownRight:
            value->Buttons |= GamepadButtons_DPadRight;
            break;
        case GameControllerSwitchPosition_Left:
        case GameControllerSwitchPosition_UpLeft:
        case GameControllerSwitchPosition_DownLeft:
            value->Buttons |= GamepadButtons_DPadLeft;
            break;
        default:
            break;
        }

        value->LeftThumbstickX = 2. * state.axes[0] - 1.;
        value->LeftThumbstickY = 1. - 2. * state.axes[1];
        value->LeftTrigger = state.axes[2];
        value->RightThumbstickX = 2. * state.axes[3] - 1.;
        value->RightThumbstickY = 1. - 2. * state.axes[4];
        value->RightTrigger = state.axes[5];

        value->Timestamp = state.timestamp;
    }

    return hr;
}

WIDL_impl_IGamepadVtbl( gamepad_IGamepad );

static HRESULT WINAPI gamepad_IGamepad2_GetButtonLabel( IGamepad2 *iface, GamepadButtons button, GameControllerButtonLabel *value )
{
    FIXME( "iface %p, button %#x, value %p stub!\n", iface, button, value );
    *value = GameControllerButtonLabel_None;
    return S_OK;
}

WIDL_impl_IGamepad2Vtbl( gamepad_IGamepad2 );

struct gamepad_statics
{
    IActivationFactory IActivationFactory_iface;
    IGamepadStatics IGamepadStatics_iface;
    IGamepadStatics2 IGamepadStatics2_iface;
    ICustomGameControllerFactory ICustomGameControllerFactory_iface;
    IAgileObject IAgileObject_iface;
    const WCHAR *class_name;
};

WIDL_impl_static_IActivationFactory( gamepad_statics,
    IGamepadStatics,
    IGamepadStatics2,
    ICustomGameControllerFactory,
    IAgileObject,
    END, FIXME
);

static HRESULT WINAPI gamepad_statics_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    FIXME( "iface %p, instance %p stub!\n", iface, instance );
    return E_NOTIMPL;
}

WIDL_impl_IActivationFactoryVtbl( gamepad_statics );

static HRESULT WINAPI gamepad_statics_IGamepadStatics_add_GamepadAdded( IGamepadStatics *iface, IEventHandler_Gamepad *handler,
                                                EventRegistrationToken *token )
{
    TRACE( "iface %p, handler %p, token %p.\n", iface, handler, token );
    if (!handler) return E_INVALIDARG;
    return event_handlers_append( &gamepad_added_handlers, (IEventHandler_IInspectable *)handler, token );
}

static HRESULT WINAPI gamepad_statics_IGamepadStatics_remove_GamepadAdded( IGamepadStatics *iface, EventRegistrationToken token )
{
    TRACE( "iface %p, token %#I64x.\n", iface, token.value );
    return event_handlers_remove( &gamepad_added_handlers, &token );
}

static HRESULT WINAPI gamepad_statics_IGamepadStatics_add_GamepadRemoved( IGamepadStatics *iface, IEventHandler_Gamepad *handler,
                                                  EventRegistrationToken *token )
{
    TRACE( "iface %p, handler %p, token %p.\n", iface, handler, token );
    if (!handler) return E_INVALIDARG;
    return event_handlers_append( &gamepad_removed_handlers, (IEventHandler_IInspectable *)handler, token );
}

static HRESULT WINAPI gamepad_statics_IGamepadStatics_remove_GamepadRemoved( IGamepadStatics *iface, EventRegistrationToken token )
{
    TRACE( "iface %p, token %#I64x.\n", iface, token.value );
    return event_handlers_remove( &gamepad_removed_handlers, &token );
}

static HRESULT WINAPI gamepad_statics_IGamepadStatics_get_Gamepads( IGamepadStatics *iface, IVectorView_Gamepad **value )
{
    HRESULT hr;

    TRACE( "iface %p, value %p.\n", iface, value );

    EnterCriticalSection( &gamepad_cs );
    if (SUCCEEDED(hr = init_gamepads()))
        hr = IVector_Gamepad_GetView( gamepads, value );
    LeaveCriticalSection( &gamepad_cs );

    return hr;
}

WIDL_impl_IGamepadStaticsVtbl( gamepad_statics_IGamepadStatics );

static HRESULT WINAPI gamepad_statics_IGamepadStatics2_FromGameController( IGamepadStatics2 *iface, IGameController *game_controller, IGamepad **value )
{
    struct gamepad_statics *impl = gamepad_statics_from_IGamepadStatics2( iface );
    IGameController *controller;
    HRESULT hr;

    TRACE( "iface %p, game_controller %p, value %p.\n", iface, game_controller, value );

    *value = NULL;
    hr = IGameControllerFactoryManagerStatics2_TryGetFactoryControllerFromGameController( manager_factory, &impl->ICustomGameControllerFactory_iface,
                                                                                          game_controller, &controller );
    if (FAILED(hr) || !controller) return hr;

    hr = IGameController_QueryInterface( controller, &IID_IGamepad, (void **)value );
    IGameController_Release( controller );
    return hr;
}

WIDL_impl_IGamepadStatics2Vtbl( gamepad_statics_IGamepadStatics2 );

static HRESULT WINAPI gamepad_statics_ICustomGameControllerFactory_CreateGameController( ICustomGameControllerFactory *iface, IGameControllerProvider *provider,
                                                               IInspectable **value )
{
    struct gamepad *impl;

    TRACE( "iface %p, provider %p, value %p.\n", iface, provider, value );

    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IGameControllerImpl_iface.lpVtbl = &gamepad_vtbl;
    impl->IGameControllerInputSink_iface.lpVtbl = &gamepad_IGameControllerInputSink_vtbl;
    impl->IGamepad_iface.lpVtbl = &gamepad_IGamepad_vtbl;
    impl->IGamepad2_iface.lpVtbl = &gamepad_IGamepad2_vtbl;
    impl->class_name = RuntimeClass_Windows_Gaming_Input_Gamepad;
    impl->refcount = 1;

    TRACE( "created Gamepad %p\n", impl );

    *value = (IInspectable *)&impl->IGameControllerImpl_iface;
    return S_OK;
}

static HRESULT WINAPI gamepad_statics_ICustomGameControllerFactory_OnGameControllerAdded( ICustomGameControllerFactory *iface, IGameController *value )
{
    IGamepad *gamepad;
    HRESULT hr;

    TRACE( "iface %p, value %p.\n", iface, value );

    if (FAILED(hr = IGameController_QueryInterface( value, &IID_IGamepad, (void **)&gamepad )))
        return hr;
    event_handlers_notify( &gamepad_added_handlers, (IInspectable *)gamepad );
    IGamepad_Release( gamepad );

    return S_OK;
}

static HRESULT WINAPI gamepad_statics_ICustomGameControllerFactory_OnGameControllerRemoved( ICustomGameControllerFactory *iface, IGameController *value )
{
    IGamepad *gamepad;
    BOOLEAN found;
    UINT32 index;
    HRESULT hr;

    TRACE( "iface %p, value %p.\n", iface, value );

    if (FAILED(hr = IGameController_QueryInterface( value, &IID_IGamepad, (void **)&gamepad )))
        return hr;

    EnterCriticalSection( &gamepad_cs );
    if (SUCCEEDED(hr = init_gamepads()))
    {
        if (FAILED(hr = IVector_Gamepad_IndexOf( gamepads, gamepad, &index, &found )) || !found)
            WARN( "Could not find gamepad %p, hr %#lx!\n", gamepad, hr );
        else
            hr = IVector_Gamepad_RemoveAt( gamepads, index );
    }
    LeaveCriticalSection( &gamepad_cs );

    if (FAILED(hr))
        WARN( "Failed to remove gamepad %p, hr %#lx!\n", gamepad, hr );
    else if (found)
    {
        TRACE( "Removed gamepad %p.\n", gamepad );
        event_handlers_notify( &gamepad_removed_handlers, (IInspectable *)gamepad );
    }
    IGamepad_Release( gamepad );

    return S_OK;
}

WIDL_impl_ICustomGameControllerFactoryVtbl( gamepad_statics_ICustomGameControllerFactory );
WIDL_impl_IAgileObjectVtbl( gamepad_statics_IAgileObject );

static struct gamepad_statics gamepad_statics =
{
    {&gamepad_statics_vtbl},
    {&gamepad_statics_IGamepadStatics_vtbl},
    {&gamepad_statics_IGamepadStatics2_vtbl},
    {&gamepad_statics_ICustomGameControllerFactory_vtbl},
    {&gamepad_statics_IAgileObject_vtbl},
    RuntimeClass_Windows_Gaming_Input_Gamepad,
};

ICustomGameControllerFactory *gamepad_factory = &gamepad_statics.ICustomGameControllerFactory_iface;
