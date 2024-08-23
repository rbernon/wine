/* WinRT Windows.Gaming.Input implementation
 *
 * Copyright 2022 Bernhard Kölbl for CodeWeavers
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

#define Closed 4
#define HANDLER_NOT_SET ((void *)~(ULONG_PTR)0)

struct async_info
{
    IWineAsyncInfoImpl IWineAsyncInfoImpl_iface;
    IAsyncInfo IAsyncInfo_iface;
    IAgileObject IAgileObject_iface;
    IInspectable *outer;
    const WCHAR *class_name;
    LONG refcount;

    async_operation_callback callback;
    TP_WORK *async_run_work;
    IUnknown *invoker;
    IUnknown *param;

    CRITICAL_SECTION cs;
    IWineAsyncOperationCompletedHandler *handler;
    PROPVARIANT result;
    AsyncStatus status;
    HRESULT hr;
};

static void async_info_destroy( struct async_info *impl )
{
    if (impl->handler && impl->handler != HANDLER_NOT_SET) IWineAsyncOperationCompletedHandler_Release( impl->handler );
    IAsyncInfo_Close( &impl->IAsyncInfo_iface );
    if (impl->param) IUnknown_Release( impl->param );
    if (impl->invoker) IUnknown_Release( impl->invoker );
    impl->cs.DebugInfo->Spare[0] = 0;
    DeleteCriticalSection( &impl->cs );
    free( impl );
}

WIDL_impl_outer_IWineAsyncInfoImpl( async_info,
    IAgileObject,
    IAsyncInfo,
    END, FIXME
);

static HRESULT WINAPI async_info_put_Completed( IWineAsyncInfoImpl *iface, IWineAsyncOperationCompletedHandler *handler )
{
    struct async_info *impl = async_info_from_IWineAsyncInfoImpl( iface );
    HRESULT hr = S_OK;

    TRACE( "iface %p, handler %p.\n", iface, handler );

    EnterCriticalSection( &impl->cs );
    if (impl->status == Closed) hr = E_ILLEGAL_METHOD_CALL;
    else if (impl->handler != HANDLER_NOT_SET) hr = E_ILLEGAL_DELEGATE_ASSIGNMENT;
    else if ((impl->handler = handler))
    {
        IWineAsyncOperationCompletedHandler_AddRef( impl->handler );

        if (impl->status > Started)
        {
            IInspectable *operation = impl->outer;
            AsyncStatus status = impl->status;
            impl->handler = NULL; /* Prevent concurrent invoke. */
            LeaveCriticalSection( &impl->cs );

            IWineAsyncOperationCompletedHandler_Invoke( handler, operation, status );
            IWineAsyncOperationCompletedHandler_Release( handler );

            return S_OK;
        }
    }
    LeaveCriticalSection( &impl->cs );

    return hr;
}

static HRESULT WINAPI async_info_get_Completed( IWineAsyncInfoImpl *iface, IWineAsyncOperationCompletedHandler **handler )
{
    struct async_info *impl = async_info_from_IWineAsyncInfoImpl( iface );
    HRESULT hr = S_OK;

    TRACE( "iface %p, handler %p.\n", iface, handler );

    EnterCriticalSection( &impl->cs );
    if (impl->status == Closed) hr = E_ILLEGAL_METHOD_CALL;
    if (impl->handler == NULL || impl->handler == HANDLER_NOT_SET) *handler = NULL;
    else IWineAsyncOperationCompletedHandler_AddRef( (*handler = impl->handler) );
    LeaveCriticalSection( &impl->cs );

    return hr;
}

static HRESULT WINAPI async_info_get_Result( IWineAsyncInfoImpl *iface, PROPVARIANT *result )
{
    struct async_info *impl = async_info_from_IWineAsyncInfoImpl( iface );
    HRESULT hr = E_ILLEGAL_METHOD_CALL;

    TRACE( "iface %p, result %p.\n", iface, result );

    EnterCriticalSection( &impl->cs );
    if (impl->status == Completed || impl->status == Error)
    {
        PropVariantCopy( result, &impl->result );
        hr = impl->hr;
    }
    LeaveCriticalSection( &impl->cs );

    return hr;
}

static HRESULT WINAPI async_info_Start( IWineAsyncInfoImpl *iface )
{
    struct async_info *impl = async_info_from_IWineAsyncInfoImpl( iface );

    TRACE( "iface %p.\n", iface );

    /* keep the async alive in the callback */
    IInspectable_AddRef( impl->outer );
    SubmitThreadpoolWork( impl->async_run_work );

    return S_OK;
}

WIDL_impl_IWineAsyncInfoImplVtbl( async_info );

static HRESULT WINAPI async_info_IAsyncInfo_get_Id( IAsyncInfo *iface, UINT32 *id )
{
    struct async_info *impl = async_info_from_IAsyncInfo( iface );
    HRESULT hr = S_OK;

    TRACE( "iface %p, id %p.\n", iface, id );

    EnterCriticalSection( &impl->cs );
    if (impl->status == Closed) hr = E_ILLEGAL_METHOD_CALL;
    *id = 1;
    LeaveCriticalSection( &impl->cs );

    return hr;
}

static HRESULT WINAPI async_info_IAsyncInfo_get_Status( IAsyncInfo *iface, AsyncStatus *status )
{
    struct async_info *impl = async_info_from_IAsyncInfo( iface );
    HRESULT hr = S_OK;

    TRACE( "iface %p, status %p.\n", iface, status );

    EnterCriticalSection( &impl->cs );
    if (impl->status == Closed) hr = E_ILLEGAL_METHOD_CALL;
    *status = impl->status;
    LeaveCriticalSection( &impl->cs );

    return hr;
}

static HRESULT WINAPI async_info_IAsyncInfo_get_ErrorCode( IAsyncInfo *iface, HRESULT *error_code )
{
    struct async_info *impl = async_info_from_IAsyncInfo( iface );
    HRESULT hr = S_OK;

    TRACE( "iface %p, error_code %p.\n", iface, error_code );

    EnterCriticalSection( &impl->cs );
    if (impl->status == Closed) *error_code = hr = E_ILLEGAL_METHOD_CALL;
    else *error_code = impl->hr;
    LeaveCriticalSection( &impl->cs );

    return hr;
}

static HRESULT WINAPI async_info_IAsyncInfo_Cancel( IAsyncInfo *iface )
{
    struct async_info *impl = async_info_from_IAsyncInfo( iface );
    HRESULT hr = S_OK;

    TRACE( "iface %p.\n", iface );

    EnterCriticalSection( &impl->cs );
    if (impl->status == Closed) hr = E_ILLEGAL_METHOD_CALL;
    else if (impl->status == Started) impl->status = Canceled;
    LeaveCriticalSection( &impl->cs );

    return hr;
}

static HRESULT WINAPI async_info_IAsyncInfo_Close( IAsyncInfo *iface )
{
    struct async_info *impl = async_info_from_IAsyncInfo( iface );
    HRESULT hr = S_OK;

    TRACE( "iface %p.\n", iface );

    EnterCriticalSection( &impl->cs );
    if (impl->status == Started)
        hr = E_ILLEGAL_STATE_CHANGE;
    else if (impl->status != Closed)
    {
        CloseThreadpoolWork( impl->async_run_work );
        impl->async_run_work = NULL;
        impl->status = Closed;
    }
    LeaveCriticalSection( &impl->cs );

    return hr;
}

WIDL_impl_IAgileObjectVtbl( async_info_IAgileObject );
WIDL_impl_IAsyncInfoVtbl( async_info_IAsyncInfo );

static void CALLBACK async_info_callback( TP_CALLBACK_INSTANCE *instance, void *iface, TP_WORK *work )
{
    struct async_info *impl = async_info_from_IWineAsyncInfoImpl( iface );
    IInspectable *operation = impl->outer;
    PROPVARIANT result;
    HRESULT hr;

    hr = impl->callback( impl->invoker, impl->param, &result );

    EnterCriticalSection( &impl->cs );
    if (impl->status != Closed) impl->status = FAILED(hr) ? Error : Completed;
    PropVariantCopy( &impl->result, &result );
    impl->hr = hr;

    if (impl->handler != NULL && impl->handler != HANDLER_NOT_SET)
    {
        IWineAsyncOperationCompletedHandler *handler = impl->handler;
        AsyncStatus status = impl->status;
        impl->handler = NULL; /* Prevent concurrent invoke. */
        LeaveCriticalSection( &impl->cs );

        IWineAsyncOperationCompletedHandler_Invoke( handler, operation, status );
        IWineAsyncOperationCompletedHandler_Release( handler );
    }
    else LeaveCriticalSection( &impl->cs );

    /* release refcount acquired in Start */
    IInspectable_Release( operation );

    PropVariantClear( &result );
}

static HRESULT async_info_create( IUnknown *invoker, IUnknown *param, async_operation_callback callback,
                                  IInspectable *outer, IWineAsyncInfoImpl **out )
{
    struct async_info *impl;
    HRESULT hr;

    if (!(impl = calloc( 1, sizeof(struct async_info) ))) return E_OUTOFMEMORY;
    impl->IWineAsyncInfoImpl_iface.lpVtbl = &async_info_vtbl;
    impl->IAsyncInfo_iface.lpVtbl = &async_info_IAsyncInfo_vtbl;
    impl->IAgileObject_iface.lpVtbl = &async_info_IAgileObject_vtbl;
    impl->outer = outer;
    impl->refcount = 1;

    impl->callback = callback;
    impl->handler = HANDLER_NOT_SET;
    impl->status = Started;
    if (!(impl->async_run_work = CreateThreadpoolWork( async_info_callback, &impl->IWineAsyncInfoImpl_iface, NULL )))
    {
        hr = HRESULT_FROM_WIN32( GetLastError() );
        free( impl );
        return hr;
    }

    if ((impl->invoker = invoker)) IUnknown_AddRef( impl->invoker );
    if ((impl->param = param)) IUnknown_AddRef( impl->param );

    InitializeCriticalSectionEx( &impl->cs, 0, RTL_CRITICAL_SECTION_FLAG_FORCE_DEBUG_INFO );
    impl->cs.DebugInfo->Spare[0] = (DWORD_PTR)( __FILE__ ": async_info.cs" );

    *out = &impl->IWineAsyncInfoImpl_iface;
    return S_OK;
}

struct async_bool
{
    IAsyncOperation_boolean IAsyncOperation_boolean_iface;
    IWineAsyncInfoImpl *IWineAsyncInfoImpl_inner;
    const WCHAR *class_name;
    LONG refcount;
};

static void async_bool_destroy( struct async_bool *impl )
{
    IWineAsyncInfoImpl_Release( impl->IWineAsyncInfoImpl_inner );
    free( impl );
}

WIDL_impl_from_IAsyncOperation_boolean( async_bool );

static HRESULT WINAPI async_bool_QueryInterface( IAsyncOperation_boolean *iface, REFIID iid, void **out )
{
    struct async_bool *impl = async_bool_from_IAsyncOperation_boolean( iface );
    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );
    WIDL_impl_QueryInterface_IAsyncOperation_boolean( impl, iid, out, IAsyncOperation_boolean_iface );
    return IWineAsyncInfoImpl_QueryInterface( impl->IWineAsyncInfoImpl_inner, iid, out );
}

WIDL_impl_IUnknown_AddRef( async_bool, IAsyncOperation_boolean );
WIDL_impl_IUnknown_Release( async_bool, IAsyncOperation_boolean );
WIDL_impl_IInspectable_methods( async_bool, IAsyncOperation_boolean );

static HRESULT WINAPI async_bool_put_Completed( IAsyncOperation_boolean *iface, IAsyncOperationCompletedHandler_boolean *bool_handler )
{
    IWineAsyncOperationCompletedHandler *handler = (IWineAsyncOperationCompletedHandler *)bool_handler;
    struct async_bool *impl = async_bool_from_IAsyncOperation_boolean( iface );
    TRACE( "iface %p, handler %p.\n", iface, handler );
    return IWineAsyncInfoImpl_put_Completed( impl->IWineAsyncInfoImpl_inner, handler );
}

static HRESULT WINAPI async_bool_get_Completed( IAsyncOperation_boolean *iface, IAsyncOperationCompletedHandler_boolean **bool_handler )
{
    IWineAsyncOperationCompletedHandler **handler = (IWineAsyncOperationCompletedHandler **)bool_handler;
    struct async_bool *impl = async_bool_from_IAsyncOperation_boolean( iface );
    TRACE( "iface %p, handler %p.\n", iface, handler );
    return IWineAsyncInfoImpl_get_Completed( impl->IWineAsyncInfoImpl_inner, handler );
}

static HRESULT WINAPI async_bool_GetResults( IAsyncOperation_boolean *iface, BOOLEAN *results )
{
    struct async_bool *impl = async_bool_from_IAsyncOperation_boolean( iface );
    PROPVARIANT result = {.vt = VT_BOOL};
    HRESULT hr;

    TRACE( "iface %p, results %p.\n", iface, results );

    hr = IWineAsyncInfoImpl_get_Result( impl->IWineAsyncInfoImpl_inner, &result );

    *results = result.boolVal;
    PropVariantClear( &result );
    return hr;
}

WIDL_impl_IAsyncOperation_booleanVtbl( async_bool );

HRESULT async_operation_boolean_create( IUnknown *invoker, IUnknown *param, async_operation_callback callback,
                                        IAsyncOperation_boolean **out )
{
    struct async_bool *impl;
    HRESULT hr;

    *out = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IAsyncOperation_boolean_iface.lpVtbl = &async_bool_vtbl;
    impl->class_name = L"Windows.Foundation.IAsyncOperation`1<Boolean>";
    impl->refcount = 1;

    if (FAILED(hr = async_info_create( invoker, param, callback, (IInspectable *)&impl->IAsyncOperation_boolean_iface, &impl->IWineAsyncInfoImpl_inner )) ||
        FAILED(hr = IWineAsyncInfoImpl_Start( impl->IWineAsyncInfoImpl_inner )))
    {
        if (impl->IWineAsyncInfoImpl_inner) IWineAsyncInfoImpl_Release( impl->IWineAsyncInfoImpl_inner );
        free( impl );
        return hr;
    }

    *out = &impl->IAsyncOperation_boolean_iface;
    TRACE( "created IAsyncOperation_boolean %p\n", *out );
    return S_OK;
}

struct async_result
{
    IAsyncOperation_ForceFeedbackLoadEffectResult IAsyncOperation_ForceFeedbackLoadEffectResult_iface;
    IWineAsyncInfoImpl *IWineAsyncInfoImpl_inner;
    const WCHAR *class_name;
    LONG refcount;
};

static void async_result_destroy( struct async_result *impl )
{
    IWineAsyncInfoImpl_Release( impl->IWineAsyncInfoImpl_inner );
    free( impl );
}

WIDL_impl_from_IAsyncOperation_ForceFeedbackLoadEffectResult( async_result );

static HRESULT WINAPI async_result_QueryInterface( IAsyncOperation_ForceFeedbackLoadEffectResult *iface, REFIID iid, void **out )
{
    struct async_result *impl = async_result_from_IAsyncOperation_ForceFeedbackLoadEffectResult( iface );
    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );
    WIDL_impl_QueryInterface_IAsyncOperation_ForceFeedbackLoadEffectResult( impl, iid, out, IAsyncOperation_ForceFeedbackLoadEffectResult_iface );
    return IWineAsyncInfoImpl_QueryInterface( impl->IWineAsyncInfoImpl_inner, iid, out );
}

WIDL_impl_IUnknown_AddRef( async_result, IAsyncOperation_ForceFeedbackLoadEffectResult );
WIDL_impl_IUnknown_Release( async_result, IAsyncOperation_ForceFeedbackLoadEffectResult );
WIDL_impl_IInspectable_methods( async_result, IAsyncOperation_ForceFeedbackLoadEffectResult );

static HRESULT WINAPI async_result_put_Completed( IAsyncOperation_ForceFeedbackLoadEffectResult *iface, IAsyncOperationCompletedHandler_ForceFeedbackLoadEffectResult *handler )
{
    struct async_result *impl = async_result_from_IAsyncOperation_ForceFeedbackLoadEffectResult( iface );
    TRACE( "iface %p, handler %p.\n", iface, handler );
    return IWineAsyncInfoImpl_put_Completed( impl->IWineAsyncInfoImpl_inner, (IWineAsyncOperationCompletedHandler *)handler );
}

static HRESULT WINAPI async_result_get_Completed( IAsyncOperation_ForceFeedbackLoadEffectResult *iface, IAsyncOperationCompletedHandler_ForceFeedbackLoadEffectResult **handler )
{
    struct async_result *impl = async_result_from_IAsyncOperation_ForceFeedbackLoadEffectResult( iface );
    TRACE( "iface %p, handler %p.\n", iface, handler );
    return IWineAsyncInfoImpl_get_Completed( impl->IWineAsyncInfoImpl_inner, (IWineAsyncOperationCompletedHandler **)handler );
}

static HRESULT WINAPI async_result_GetResults( IAsyncOperation_ForceFeedbackLoadEffectResult *iface, ForceFeedbackLoadEffectResult *results )
{
    struct async_result *impl = async_result_from_IAsyncOperation_ForceFeedbackLoadEffectResult( iface );
    PROPVARIANT result = {.vt = VT_UI4};
    HRESULT hr;

    TRACE( "iface %p, results %p.\n", iface, results );

    hr = IWineAsyncInfoImpl_get_Result( impl->IWineAsyncInfoImpl_inner, &result );

    *results = result.ulVal;
    PropVariantClear( &result );
    return hr;
}

WIDL_impl_IAsyncOperation_ForceFeedbackLoadEffectResultVtbl( async_result );

HRESULT async_operation_effect_result_create( IUnknown *invoker, IUnknown *param, async_operation_callback callback,
                                              IAsyncOperation_ForceFeedbackLoadEffectResult **out )
{
    struct async_result *impl;
    HRESULT hr;

    *out = NULL;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->IAsyncOperation_ForceFeedbackLoadEffectResult_iface.lpVtbl = &async_result_vtbl;
    impl->class_name = L"Windows.Foundation.IAsyncOperation`1<Windows.Gaming.Input.ForceFeedback.ForceFeedbackLoadEffectResult>";
    impl->refcount = 1;

    if (FAILED(hr = async_info_create( invoker, param, callback, (IInspectable *)&impl->IAsyncOperation_ForceFeedbackLoadEffectResult_iface, &impl->IWineAsyncInfoImpl_inner )) ||
        FAILED(hr = IWineAsyncInfoImpl_Start( impl->IWineAsyncInfoImpl_inner )))
    {
        if (impl->IWineAsyncInfoImpl_inner) IWineAsyncInfoImpl_Release( impl->IWineAsyncInfoImpl_inner );
        free( impl );
        return hr;
    }

    *out = &impl->IAsyncOperation_ForceFeedbackLoadEffectResult_iface;
    TRACE( "created IAsyncOperation_ForceFeedbackLoadEffectResult %p\n", *out );
    return S_OK;
}
