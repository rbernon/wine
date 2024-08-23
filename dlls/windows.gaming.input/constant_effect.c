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

struct constant_effect
{
    IConstantForceEffect IConstantForceEffect_iface;
    IWineForceFeedbackEffectImpl *IWineForceFeedbackEffectImpl_inner;
    const WCHAR *class_name;
    LONG refcount;
};

INTERFACE_IMPL_FROM( constant_effect, IConstantForceEffect );

static HRESULT WINAPI constant_effect_QueryInterface( IConstantForceEffect *iface, REFIID iid, void **out )
{
    struct constant_effect *impl = constant_effect_from_IConstantForceEffect( iface );
    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );
    QUERY_INTERFACE_IConstantForceEffect( impl, iid, out, IConstantForceEffect_iface )
    return IWineForceFeedbackEffectImpl_QueryInterface( impl->IWineForceFeedbackEffectImpl_inner, iid, out );
}

IUNKNOWN_IMPL_ADDREF( constant_effect, IConstantForceEffect );

static void constant_effect_destroy( struct constant_effect *impl )
{
    IWineForceFeedbackEffectImpl_Release( impl->IWineForceFeedbackEffectImpl_inner );
    free( impl );
}

IUNKNOWN_IMPL_RELEASE( constant_effect, IConstantForceEffect );
IINSPECTABLE_IMPL( constant_effect, IConstantForceEffect );

static HRESULT WINAPI constant_effect_SetParameters( IConstantForceEffect *iface, Vector3 direction, TimeSpan duration )
{
    WineForceFeedbackEffectParameters params =
    {
        .constant =
        {
            .type = WineForceFeedbackEffectType_Constant,
            .direction = direction,
            .duration = duration,
            .repeat_count = 1,
            .gain = 1.,
        },
    };
    struct constant_effect *impl = constant_effect_from_IConstantForceEffect( iface );

    TRACE( "iface %p, direction %s, duration %I64u.\n", iface, debugstr_vector3( &direction ), duration.Duration );

    return IWineForceFeedbackEffectImpl_put_Parameters( impl->IWineForceFeedbackEffectImpl_inner, params, NULL );
}

static HRESULT WINAPI constant_effect_SetParametersWithEnvelope( IConstantForceEffect *iface, Vector3 direction, FLOAT attack_gain,
                                                        FLOAT sustain_gain, FLOAT release_gain, TimeSpan start_delay,
                                                        TimeSpan attack_duration, TimeSpan sustain_duration,
                                                        TimeSpan release_duration, UINT32 repeat_count )
{
    WineForceFeedbackEffectParameters params =
    {
        .constant =
        {
            .type = WineForceFeedbackEffectType_Constant,
            .direction = direction,
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
    struct constant_effect *impl = constant_effect_from_IConstantForceEffect( iface );

    TRACE( "iface %p, direction %s, attack_gain %f, sustain_gain %f, release_gain %f, start_delay %I64u, attack_duration %I64u, "
           "sustain_duration %I64u, release_duration %I64u, repeat_count %u.\n", iface, debugstr_vector3( &direction ),
           attack_gain, sustain_gain, release_gain, start_delay.Duration, attack_duration.Duration, sustain_duration.Duration,
           release_duration.Duration, repeat_count );

    return IWineForceFeedbackEffectImpl_put_Parameters( impl->IWineForceFeedbackEffectImpl_inner, params, &envelope );
}

INTERFACE_VTBL_IConstantForceEffect( constant_effect );

struct constant_factory
{
    IActivationFactory IActivationFactory_iface;
    IAgileObject IAgileObject_iface;
    const WCHAR *class_name;
};

INTERFACE_IMPL_STATIC_IActivationFactory( constant_factory, IAgileObject, END, FIXME );

static HRESULT WINAPI constant_factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    struct constant_effect *impl;
    HRESULT hr;

    TRACE( "iface %p, instance %p.\n", iface, instance );

    if (!(impl = calloc( 1, sizeof(struct constant_effect) ))) return E_OUTOFMEMORY;
    impl->IConstantForceEffect_iface.lpVtbl = &constant_effect_vtbl;
    impl->class_name = RuntimeClass_Windows_Gaming_Input_ForceFeedback_ConstantForceEffect;
    impl->refcount = 1;

    if (FAILED(hr = force_feedback_effect_create( WineForceFeedbackEffectType_Constant, (IInspectable *)&impl->IConstantForceEffect_iface,
                                                  &impl->IWineForceFeedbackEffectImpl_inner )))
    {
        free( impl );
        return hr;
    }

    *instance = (IInspectable *)&impl->IConstantForceEffect_iface;
    TRACE( "created ConstantForceEffect %p\n", *instance );
    return S_OK;
}

INTERFACE_VTBL_IActivationFactory( constant_factory );
INTERFACE_VTBL_IAgileObject( constant_factory_IAgileObject );

static struct constant_factory constant_statics =
{
    {&constant_factory_vtbl},
    {&constant_factory_IAgileObject_vtbl},
    RuntimeClass_Windows_Gaming_Input_ForceFeedback_ConstantForceEffect,
};

IInspectable *constant_effect_factory = (IInspectable *)&constant_statics.IActivationFactory_iface;
