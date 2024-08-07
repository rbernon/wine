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

struct condition_effect
{
    IConditionForceEffect IConditionForceEffect_iface;
    IWineForceFeedbackEffectImpl *IWineForceFeedbackEffectImpl_inner;
    const WCHAR *class_name;
    LONG refcount;

    ConditionForceEffectKind kind;
};

static void condition_effect_destroy( struct condition_effect *impl )
{
    IWineForceFeedbackEffectImpl_Release( impl->IWineForceFeedbackEffectImpl_inner );
    free( impl );
}

WIDL_impl_from_IConditionForceEffect( condition_effect );

static HRESULT WINAPI condition_effect_QueryInterface( IConditionForceEffect *iface, REFIID iid, void **out )
{
    struct condition_effect *impl = condition_effect_from_IConditionForceEffect( iface );
    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );
    WIDL_impl_QueryInterface_IConditionForceEffect( impl, iid, out, IConditionForceEffect_iface );
    return IWineForceFeedbackEffectImpl_QueryInterface( impl->IWineForceFeedbackEffectImpl_inner, iid, out );
}

WIDL_impl_IUnknown_AddRef( condition_effect, IConditionForceEffect );
WIDL_impl_IUnknown_Release( condition_effect, IConditionForceEffect );
WIDL_impl_IInspectable_methods( condition_effect, IConditionForceEffect );

static HRESULT WINAPI condition_effect_get_Kind( IConditionForceEffect *iface, ConditionForceEffectKind *kind )
{
    struct condition_effect *impl = condition_effect_from_IConditionForceEffect( iface );
    TRACE( "iface %p, kind %p.\n", iface, kind );
    *kind = impl->kind;
    return S_OK;
}

static HRESULT WINAPI condition_effect_SetParameters( IConditionForceEffect *iface, Vector3 direction, FLOAT positive_coeff, FLOAT negative_coeff,
                                            FLOAT max_positive_magnitude, FLOAT max_negative_magnitude, FLOAT deadzone, FLOAT bias )
{
    struct condition_effect *impl = condition_effect_from_IConditionForceEffect( iface );
    WineForceFeedbackEffectParameters params =
    {
        .condition =
        {
            .type = WineForceFeedbackEffectType_Condition + impl->kind,
            .direction = direction,
            .positive_coeff = positive_coeff,
            .negative_coeff = negative_coeff,
            .max_positive_magnitude = max_positive_magnitude,
            .max_negative_magnitude = max_negative_magnitude,
            .deadzone = deadzone,
            .bias = bias,
        },
    };

    TRACE( "iface %p, direction %s, positive_coeff %f, negative_coeff %f, max_positive_magnitude %f, max_negative_magnitude %f, deadzone %f, bias %f.\n",
           iface, debugstr_vector3( &direction ), positive_coeff, negative_coeff, max_positive_magnitude, max_negative_magnitude, deadzone, bias );

    return IWineForceFeedbackEffectImpl_put_Parameters( impl->IWineForceFeedbackEffectImpl_inner, params, NULL );
}

WIDL_impl_IConditionForceEffectVtbl( condition_effect );

struct condition_factory
{
    IActivationFactory IActivationFactory_iface;
    IConditionForceEffectFactory IConditionForceEffectFactory_iface;
    IAgileObject IAgileObject_iface;
    const WCHAR *class_name;
};

WIDL_impl_static_IActivationFactory( condition_factory,
    IConditionForceEffectFactory,
    IAgileObject,
    END, FIXME
);

static HRESULT WINAPI condition_factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    FIXME( "iface %p, instance %p stub!\n", iface, instance );
    return E_NOTIMPL;
}

WIDL_impl_IActivationFactoryVtbl( condition_factory );

static HRESULT WINAPI condition_factory_IConditionForceEffectFactory_CreateInstance( IConditionForceEffectFactory *iface, enum ConditionForceEffectKind kind, IForceFeedbackEffect **out )
{
    enum WineForceFeedbackEffectType type = WineForceFeedbackEffectType_Condition + kind;
    struct condition_effect *impl;
    HRESULT hr;

    TRACE( "iface %p, kind %u, out %p.\n", iface, kind, out );

    if (!(impl = calloc( 1, sizeof(struct condition_effect) ))) return E_OUTOFMEMORY;
    impl->IConditionForceEffect_iface.lpVtbl = &condition_effect_vtbl;
    impl->class_name = RuntimeClass_Windows_Gaming_Input_ForceFeedback_ConditionForceEffect;
    impl->refcount = 1;
    impl->kind = kind;

    if (FAILED(hr = force_feedback_effect_create( type, (IInspectable *)&impl->IConditionForceEffect_iface, &impl->IWineForceFeedbackEffectImpl_inner )) ||
        FAILED(hr = IConditionForceEffect_QueryInterface( &impl->IConditionForceEffect_iface, &IID_IForceFeedbackEffect, (void **)out )))
    {
        if (impl->IWineForceFeedbackEffectImpl_inner) IWineForceFeedbackEffectImpl_Release( impl->IWineForceFeedbackEffectImpl_inner );
        free( impl );
        return hr;
    }

    IConditionForceEffect_Release( &impl->IConditionForceEffect_iface );
    TRACE( "created ConditionForceEffect %p\n", *out );
    return S_OK;
}

WIDL_impl_IConditionForceEffectFactoryVtbl( condition_factory_IConditionForceEffectFactory );
WIDL_impl_IAgileObjectVtbl( condition_factory_IAgileObject );

static struct condition_factory condition_statics =
{
    {&condition_factory_vtbl},
    {&condition_factory_IConditionForceEffectFactory_vtbl},
    {&condition_factory_IAgileObject_vtbl},
    RuntimeClass_Windows_Gaming_Input_ForceFeedback_ConditionForceEffect,
};

IInspectable *condition_effect_factory = (IInspectable *)&condition_statics.IActivationFactory_iface;
