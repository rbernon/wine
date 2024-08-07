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

WINE_DEFAULT_DEBUG_CHANNEL(input);

struct ramp_effect
{
    IRampForceEffect IRampForceEffect_iface;
    IWineForceFeedbackEffectImpl *IWineForceFeedbackEffectImpl_inner;
    const WCHAR *class_name;
    LONG refcount;
};

static void ramp_effect_destroy( struct ramp_effect *impl )
{
    IWineForceFeedbackEffectImpl_Release( impl->IWineForceFeedbackEffectImpl_inner );
    free( impl );
}

WIDL_impl_from_IRampForceEffect( ramp_effect );

static HRESULT WINAPI ramp_effect_QueryInterface( IRampForceEffect *iface, REFIID iid, void **out )
{
    struct ramp_effect *impl = ramp_effect_from_IRampForceEffect( iface );
    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );
    WIDL_impl_QueryInterface_IRampForceEffect( impl, iid, out, IRampForceEffect_iface );
    return IWineForceFeedbackEffectImpl_QueryInterface( impl->IWineForceFeedbackEffectImpl_inner, iid, out );
}

WIDL_impl_IUnknown_AddRef( ramp_effect, IRampForceEffect );
WIDL_impl_IUnknown_Release( ramp_effect, IRampForceEffect );
WIDL_impl_IInspectable_methods( ramp_effect, IRampForceEffect );

static HRESULT WINAPI ramp_effect_SetParameters( IRampForceEffect *iface, Vector3 start_vector, Vector3 end_vector, TimeSpan duration )
{
    WineForceFeedbackEffectParameters params =
    {
        .ramp =
        {
            .type = WineForceFeedbackEffectType_Ramp,
            .start_vector = start_vector,
            .end_vector = end_vector,
            .duration = duration,
            .repeat_count = 1,
            .gain = 1.,
        },
    };
    struct ramp_effect *impl = ramp_effect_from_IRampForceEffect( iface );

    TRACE( "iface %p, start_vector %s, end_vector %s, duration %I64u.\n", iface,
           debugstr_vector3( &start_vector ), debugstr_vector3( &end_vector ), duration.Duration );

    return IWineForceFeedbackEffectImpl_put_Parameters( impl->IWineForceFeedbackEffectImpl_inner, params, NULL );
}

static HRESULT WINAPI ramp_effect_SetParametersWithEnvelope( IRampForceEffect *iface, Vector3 start_vector, Vector3 end_vector, FLOAT attack_gain,
                                                        FLOAT sustain_gain, FLOAT release_gain, TimeSpan start_delay,
                                                        TimeSpan attack_duration, TimeSpan sustain_duration,
                                                        TimeSpan release_duration, UINT32 repeat_count )
{
    WineForceFeedbackEffectParameters params =
    {
        .ramp =
        {
            .type = WineForceFeedbackEffectType_Ramp,
            .start_vector = start_vector,
            .end_vector = end_vector,
            .duration = {attack_duration.Duration + sustain_duration.Duration + release_duration.Duration},
            .start_delay = start_delay,
            .repeat_count = repeat_count,
            .gain = sustain_gain,
        },
    };
    WineForceFeedbackEffectEnvelope envelope =
    {
        .attack_gain = attack_gain,
        .release_gain = release_gain,
        .attack_duration = attack_duration,
        .release_duration = release_duration,
    };
    struct ramp_effect *impl = ramp_effect_from_IRampForceEffect( iface );

    TRACE( "iface %p, start_vector %s, end_vector %s, attack_gain %f, sustain_gain %f, release_gain %f, start_delay %I64u, attack_duration %I64u, "
           "sustain_duration %I64u, release_duration %I64u, repeat_count %u.\n", iface, debugstr_vector3( &start_vector ), debugstr_vector3( &end_vector ),
           attack_gain, sustain_gain, release_gain, start_delay.Duration, attack_duration.Duration, sustain_duration.Duration,
           release_duration.Duration, repeat_count );

    return IWineForceFeedbackEffectImpl_put_Parameters( impl->IWineForceFeedbackEffectImpl_inner, params, &envelope );
}

WIDL_impl_IRampForceEffectVtbl( ramp_effect );

struct ramp_factory
{
    IActivationFactory IActivationFactory_iface;
    IAgileObject IAgileObject_iface;
    const WCHAR *class_name;
};

WIDL_impl_static_IActivationFactory( ramp_factory,
    IAgileObject,
    END, FIXME
);

static HRESULT WINAPI ramp_factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    struct ramp_effect *impl;
    HRESULT hr;

    TRACE( "iface %p, instance %p.\n", iface, instance );

    if (!(impl = calloc( 1, sizeof(struct ramp_effect) ))) return E_OUTOFMEMORY;
    impl->IRampForceEffect_iface.lpVtbl = &ramp_effect_vtbl;
    impl->class_name = RuntimeClass_Windows_Gaming_Input_ForceFeedback_RampForceEffect;
    impl->refcount = 1;

    if (FAILED(hr = force_feedback_effect_create( WineForceFeedbackEffectType_Ramp, (IInspectable *)&impl->IRampForceEffect_iface,
                                                  &impl->IWineForceFeedbackEffectImpl_inner )))
    {
        free( impl );
        return hr;
    }

    *instance = (IInspectable *)&impl->IRampForceEffect_iface;
    TRACE( "created RampForceEffect %p\n", *instance );
    return S_OK;
}

WIDL_impl_IActivationFactoryVtbl( ramp_factory );
WIDL_impl_IAgileObjectVtbl( ramp_factory_IAgileObject );

static struct ramp_factory ramp_statics =
{
    {&ramp_factory_vtbl},
    {&ramp_factory_IAgileObject_vtbl},
    RuntimeClass_Windows_Gaming_Input_ForceFeedback_RampForceEffect,
};

IInspectable *ramp_effect_factory = (IInspectable *)&ramp_statics.IActivationFactory_iface;
