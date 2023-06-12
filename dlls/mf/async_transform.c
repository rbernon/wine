/*
 * Copyright 2023 Rémi Bernon for CodeWeavers
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

#include <stddef.h>
#include <stdarg.h>

#define COBJMACROS
#include "windef.h"
#include "winbase.h"

#include "mf_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(mfplat);

struct async_transform
{
    IMFTransform IMFTransform_iface;
    IMFShutdown IMFShutdown_iface;
    LONG refcount;

    CRITICAL_SECTION cs;
    IMFTransform *transform;
};

static struct async_transform *impl_from_IMFTransform(IMFTransform *iface)
{
    return CONTAINING_RECORD(iface, struct async_transform, IMFTransform_iface);
}

static HRESULT WINAPI transform_QueryInterface(IMFTransform *iface, REFIID iid, void **out)
{
    struct async_transform *impl = impl_from_IMFTransform(iface);

    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown)
            || IsEqualGUID(iid, &IID_IMFTransform))
        *out = &impl->IMFTransform_iface;
    else if (IsEqualGUID(iid, &IID_IMFShutdown))
        *out = &impl->IMFShutdown_iface;
    else
    {
        *out = NULL;
        WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static ULONG WINAPI transform_AddRef(IMFTransform *iface)
{
    struct async_transform *impl = impl_from_IMFTransform(iface);
    ULONG refcount = InterlockedIncrement(&impl->refcount);
    TRACE("iface %p increasing refcount to %lu.\n", impl, refcount);
    return refcount;
}

static ULONG WINAPI transform_Release(IMFTransform *iface)
{
    struct async_transform *impl = impl_from_IMFTransform(iface);
    ULONG refcount = InterlockedDecrement(&impl->refcount);

    TRACE("iface %p decreasing refcount to %lu.\n", impl, refcount);

    if (!refcount)
    {
        IMFShutdown_Shutdown(&impl->IMFShutdown_iface);
        impl->cs.DebugInfo->Spare[0] = 0;
        DeleteCriticalSection(&impl->cs);
        free(impl);
    }

    return refcount;
}

static HRESULT WINAPI transform_GetStreamLimits(IMFTransform *iface, DWORD *input_minimum,
        DWORD *input_maximum, DWORD *output_minimum, DWORD *output_maximum)
{
    struct async_transform *impl = impl_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, input_minimum %p, input_maximum %p, output_minimum %p, output_maximum %p.\n",
            iface, input_minimum, input_maximum, output_minimum, output_maximum);

    EnterCriticalSection(&impl->cs);
    if (!impl->transform)
        hr = MF_E_SHUTDOWN;
    else
        hr = IMFTransform_GetStreamLimits(impl->transform, input_minimum, input_maximum, output_minimum, output_maximum);
    LeaveCriticalSection(&impl->cs);
    return hr;
}

static HRESULT WINAPI transform_GetStreamCount(IMFTransform *iface, DWORD *inputs, DWORD *outputs)
{
    struct async_transform *impl = impl_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, inputs %p, outputs %p.\n", iface, inputs, outputs);

    EnterCriticalSection(&impl->cs);
    if (!impl->transform)
        hr = MF_E_SHUTDOWN;
    else
        hr = IMFTransform_GetStreamCount(impl->transform, inputs, outputs);
    LeaveCriticalSection(&impl->cs);
    return hr;
}

static HRESULT WINAPI transform_GetStreamIDs(IMFTransform *iface, DWORD input_size, DWORD *inputs,
        DWORD output_size, DWORD *outputs)
{
    struct async_transform *impl = impl_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, input_size %lu, inputs %p, output_size %lu, outputs %p.\n", iface,
            input_size, inputs, output_size, outputs);

    EnterCriticalSection(&impl->cs);
    if (!impl->transform)
        hr = MF_E_SHUTDOWN;
    else
        hr = IMFTransform_GetStreamIDs(impl->transform, input_size, inputs, output_size, outputs);
    LeaveCriticalSection(&impl->cs);
    return hr;
}

static HRESULT WINAPI transform_GetInputStreamInfo(IMFTransform *iface, DWORD id, MFT_INPUT_STREAM_INFO *info)
{
    struct async_transform *impl = impl_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, id %#lx, info %p.\n", iface, id, info);

    EnterCriticalSection(&impl->cs);
    if (!impl->transform)
        hr = MF_E_SHUTDOWN;
    else
        hr = IMFTransform_GetInputStreamInfo(impl->transform, id, info);
    LeaveCriticalSection(&impl->cs);
    return hr;
}

static HRESULT WINAPI transform_GetOutputStreamInfo(IMFTransform *iface, DWORD id, MFT_OUTPUT_STREAM_INFO *info)
{
    struct async_transform *impl = impl_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, id %#lx, info %p.\n", iface, id, info);

    EnterCriticalSection(&impl->cs);
    if (!impl->transform)
        hr = MF_E_SHUTDOWN;
    else
        hr = IMFTransform_GetOutputStreamInfo(impl->transform, id, info);
    LeaveCriticalSection(&impl->cs);
    return hr;
}

static HRESULT WINAPI transform_GetAttributes(IMFTransform *iface, IMFAttributes **attributes)
{
    struct async_transform *impl = impl_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, attributes %p.\n", iface, attributes);

    EnterCriticalSection(&impl->cs);
    if (!impl->transform)
        hr = MF_E_SHUTDOWN;
    else
        hr = IMFTransform_GetAttributes(impl->transform, attributes);
    LeaveCriticalSection(&impl->cs);
    return hr;
}

static HRESULT WINAPI transform_GetInputStreamAttributes(IMFTransform *iface, DWORD id, IMFAttributes **attributes)
{
    struct async_transform *impl = impl_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, id %#lx, attributes %p.\n", iface, id, attributes);

    EnterCriticalSection(&impl->cs);
    if (!impl->transform)
        hr = MF_E_SHUTDOWN;
    else
        hr = IMFTransform_GetInputStreamAttributes(impl->transform, id, attributes);
    LeaveCriticalSection(&impl->cs);
    return hr;
}

static HRESULT WINAPI transform_GetOutputStreamAttributes(IMFTransform *iface, DWORD id, IMFAttributes **attributes)
{
    struct async_transform *impl = impl_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, id %#lx, attributes %p.\n", iface, id, attributes);

    EnterCriticalSection(&impl->cs);
    if (!impl->transform)
        hr = MF_E_SHUTDOWN;
    else
        hr = IMFTransform_GetOutputStreamAttributes(impl->transform, id, attributes);
    LeaveCriticalSection(&impl->cs);
    return hr;
}

static HRESULT WINAPI transform_DeleteInputStream(IMFTransform *iface, DWORD id)
{
    struct async_transform *impl = impl_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, id %#lx.\n", iface, id);

    EnterCriticalSection(&impl->cs);
    if (!impl->transform)
        hr = MF_E_SHUTDOWN;
    else
        hr = IMFTransform_DeleteInputStream(impl->transform, id);
    LeaveCriticalSection(&impl->cs);
    return hr;
}

static HRESULT WINAPI transform_AddInputStreams(IMFTransform *iface, DWORD streams, DWORD *ids)
{
    struct async_transform *impl = impl_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, streams %lu, ids %p.\n", iface, streams, ids);

    EnterCriticalSection(&impl->cs);
    if (!impl->transform)
        hr = MF_E_SHUTDOWN;
    else
        hr = IMFTransform_AddInputStreams(impl->transform, streams, ids);
    LeaveCriticalSection(&impl->cs);
    return hr;
}

static HRESULT WINAPI transform_GetInputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    struct async_transform *impl = impl_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, id %#lx, index %#lx, type %p.\n", iface, id, index, type);

    EnterCriticalSection(&impl->cs);
    if (!impl->transform)
        hr = MF_E_SHUTDOWN;
    else
        hr = IMFTransform_GetInputAvailableType(impl->transform, id, index, type);
    LeaveCriticalSection(&impl->cs);
    return hr;
}

static HRESULT WINAPI transform_GetOutputAvailableType(IMFTransform *iface, DWORD id,
        DWORD index, IMFMediaType **type)
{
    struct async_transform *impl = impl_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, id %#lx, index %#lx, type %p.\n", iface, id, index, type);

    EnterCriticalSection(&impl->cs);
    if (!impl->transform)
        hr = MF_E_SHUTDOWN;
    else
        hr = IMFTransform_GetOutputAvailableType(impl->transform, id, index, type);
    LeaveCriticalSection(&impl->cs);
    return hr;
}

static HRESULT WINAPI transform_SetInputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    struct async_transform *impl = impl_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, id %#lx, type %p, flags %#lx.\n", iface, id, type, flags);

    EnterCriticalSection(&impl->cs);
    if (!impl->transform)
        hr = MF_E_SHUTDOWN;
    else
        hr = IMFTransform_SetInputType(impl->transform, id, type, flags);
    LeaveCriticalSection(&impl->cs);
    return hr;
}

static HRESULT WINAPI transform_SetOutputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    struct async_transform *impl = impl_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, id %#lx, type %p, flags %#lx.\n", iface, id, type, flags);

    EnterCriticalSection(&impl->cs);
    if (!impl->transform)
        hr = MF_E_SHUTDOWN;
    else
        hr = IMFTransform_SetOutputType(impl->transform, id, type, flags);
    LeaveCriticalSection(&impl->cs);
    return hr;
}

static HRESULT WINAPI transform_GetInputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **type)
{
    struct async_transform *impl = impl_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, id %#lx, type %p.\n", iface, id, type);

    EnterCriticalSection(&impl->cs);
    if (!impl->transform)
        hr = MF_E_SHUTDOWN;
    else
        hr = IMFTransform_GetInputCurrentType(impl->transform, id, type);
    LeaveCriticalSection(&impl->cs);
    return hr;
}

static HRESULT WINAPI transform_GetOutputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **type)
{
    struct async_transform *impl = impl_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, id %#lx, type %p.\n", iface, id, type);

    EnterCriticalSection(&impl->cs);
    if (!impl->transform)
        hr = MF_E_SHUTDOWN;
    else
        hr = IMFTransform_GetOutputCurrentType(impl->transform, id, type);
    LeaveCriticalSection(&impl->cs);
    return hr;
}

static HRESULT WINAPI transform_GetInputStatus(IMFTransform *iface, DWORD id, DWORD *flags)
{
    struct async_transform *impl = impl_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, id %#lx, flags %p.\n", iface, id, flags);

    EnterCriticalSection(&impl->cs);
    if (!impl->transform)
        hr = MF_E_SHUTDOWN;
    else
        hr = IMFTransform_GetInputStatus(impl->transform, id, flags);
    LeaveCriticalSection(&impl->cs);
    return hr;
}

static HRESULT WINAPI transform_GetOutputStatus(IMFTransform *iface, DWORD *flags)
{
    struct async_transform *impl = impl_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, flags %p.\n", iface, flags);

    EnterCriticalSection(&impl->cs);
    if (!impl->transform)
        hr = MF_E_SHUTDOWN;
    else
        hr = IMFTransform_GetOutputStatus(impl->transform, flags);
    LeaveCriticalSection(&impl->cs);
    return hr;
}

static HRESULT WINAPI transform_SetOutputBounds(IMFTransform *iface, LONGLONG lower, LONGLONG upper)
{
    struct async_transform *impl = impl_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, lower %I64d, upper %I64d.\n", iface, lower, upper);

    EnterCriticalSection(&impl->cs);
    if (!impl->transform)
        hr = MF_E_SHUTDOWN;
    else
        hr = IMFTransform_SetOutputBounds(impl->transform, lower, upper);
    LeaveCriticalSection(&impl->cs);
    return hr;
}

static HRESULT WINAPI transform_ProcessEvent(IMFTransform *iface, DWORD id, IMFMediaEvent *event)
{
    struct async_transform *impl = impl_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, id %#lx, event %p.\n", iface, id, event);

    EnterCriticalSection(&impl->cs);
    if (!impl->transform)
        hr = MF_E_SHUTDOWN;
    else
        hr = IMFTransform_ProcessEvent(impl->transform, id, event);
    LeaveCriticalSection(&impl->cs);
    return hr;
}

static HRESULT WINAPI transform_ProcessMessage(IMFTransform *iface, MFT_MESSAGE_TYPE message, ULONG_PTR param)
{
    struct async_transform *impl = impl_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, message %#x, param %Ix.\n", iface, message, param);

    EnterCriticalSection(&impl->cs);
    if (!impl->transform)
        hr = MF_E_SHUTDOWN;
    else
        hr = IMFTransform_ProcessMessage(impl->transform, message, param);
    LeaveCriticalSection(&impl->cs);
    return hr;
}

static HRESULT WINAPI transform_ProcessInput(IMFTransform *iface, DWORD id, IMFSample *sample, DWORD flags)
{
    struct async_transform *impl = impl_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, id %#lx, sample %p, flags %#lx.\n", iface, id, sample, flags);

    EnterCriticalSection(&impl->cs);
    if (!impl->transform)
        hr = MF_E_SHUTDOWN;
    else
        hr = IMFTransform_ProcessInput(impl->transform, id, sample, flags);
    LeaveCriticalSection(&impl->cs);
    return hr;
}

static HRESULT WINAPI transform_ProcessOutput(IMFTransform *iface, DWORD flags, DWORD count,
        MFT_OUTPUT_DATA_BUFFER *samples, DWORD *status)
{
    struct async_transform *impl = impl_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, flags %#lx, count %lu, samples %p, status %p.\n", iface, flags, count, samples, status);

    EnterCriticalSection(&impl->cs);
    if (!impl->transform)
        hr = MF_E_SHUTDOWN;
    else
        hr = IMFTransform_ProcessOutput(impl->transform, flags, count, samples, status);
    LeaveCriticalSection(&impl->cs);
    return hr;
}

static const IMFTransformVtbl transform_vtbl =
{
    transform_QueryInterface,
    transform_AddRef,
    transform_Release,
    transform_GetStreamLimits,
    transform_GetStreamCount,
    transform_GetStreamIDs,
    transform_GetInputStreamInfo,
    transform_GetOutputStreamInfo,
    transform_GetAttributes,
    transform_GetInputStreamAttributes,
    transform_GetOutputStreamAttributes,
    transform_DeleteInputStream,
    transform_AddInputStreams,
    transform_GetInputAvailableType,
    transform_GetOutputAvailableType,
    transform_SetInputType,
    transform_SetOutputType,
    transform_GetInputCurrentType,
    transform_GetOutputCurrentType,
    transform_GetInputStatus,
    transform_GetOutputStatus,
    transform_SetOutputBounds,
    transform_ProcessEvent,
    transform_ProcessMessage,
    transform_ProcessInput,
    transform_ProcessOutput,
};

static struct async_transform *impl_from_IMFShutdown(IMFShutdown *iface)
{
    return CONTAINING_RECORD(iface, struct async_transform, IMFShutdown_iface);
}

static HRESULT WINAPI shutdown_QueryInterface(IMFShutdown *iface, REFIID iid, void **out)
{
    struct async_transform *impl = impl_from_IMFShutdown(iface);
    return IMFTransform_QueryInterface(&impl->IMFTransform_iface, iid, out);
}

static ULONG WINAPI shutdown_AddRef(IMFShutdown *iface)
{
    struct async_transform *impl = impl_from_IMFShutdown(iface);
    return IMFTransform_AddRef(&impl->IMFTransform_iface);
}

static ULONG WINAPI shutdown_Release(IMFShutdown *iface)
{
    struct async_transform *impl = impl_from_IMFShutdown(iface);
    return IMFTransform_Release(&impl->IMFTransform_iface);
}

static HRESULT WINAPI shutdown_Shutdown(IMFShutdown *iface)
{
    struct async_transform *impl = impl_from_IMFShutdown(iface);
    HRESULT hr;

    TRACE("iface %p.\n", iface);

    EnterCriticalSection(&impl->cs);
    if (!impl->transform)
        hr = MF_E_SHUTDOWN;
    else
    {
        IMFTransform_Release(impl->transform);
        impl->transform = NULL;
        hr = S_OK;
    }
    LeaveCriticalSection(&impl->cs);

    return hr;
}

static HRESULT WINAPI shutdown_GetShutdownStatus(IMFShutdown *iface, MFSHUTDOWN_STATUS *status)
{
    struct async_transform *impl = impl_from_IMFShutdown(iface);
    HRESULT hr;

    TRACE("iface %p, status %p.\n", iface, status);

    if (!status)
        return E_INVALIDARG;

    EnterCriticalSection(&impl->cs);
    if (impl->transform)
        hr = MF_E_INVALIDREQUEST;
    else
    {
        *status = MFSHUTDOWN_COMPLETED;
        hr = S_OK;
    }
    LeaveCriticalSection(&impl->cs);

    return hr;
}

static const IMFShutdownVtbl shutdown_vtbl =
{
    shutdown_QueryInterface,
    shutdown_AddRef,
    shutdown_Release,
    shutdown_Shutdown,
    shutdown_GetShutdownStatus,
};

HRESULT async_transform_create(IMFTransform *transform, IMFTransform **out)
{
    struct async_transform *impl;

    if (!(impl = calloc(1, sizeof(*impl))))
        return E_OUTOFMEMORY;
    impl->IMFTransform_iface.lpVtbl = &transform_vtbl;
    impl->IMFShutdown_iface.lpVtbl = &shutdown_vtbl;
    impl->refcount = 1;

    InitializeCriticalSection(&impl->cs);
    impl->cs.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": cs");

    impl->transform = transform;
    IMFTransform_AddRef(impl->transform);

    *out = &impl->IMFTransform_iface;
    TRACE("Created async transform %p for transform %p\n", *out, transform);
    return S_OK;
}
