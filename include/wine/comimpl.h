/*
 * Wine COM implementation helpers
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
#pragma makedep install
#endif

#ifndef __WINE_WINE_COMIMPL_H
#define __WINE_WINE_COMIMPL_H

#include <stdarg.h>
#include <stdio.h>

#include <windef.h>
#include <winbase.h>

#define QUERY_INTERFACES( object, iid, out, X, ... ) QUERY_INTERFACES_ ## X( object, iid, out, __VA_ARGS__ )
#define QUERY_INTERFACES_END( object, iid, out, X, ... ) \
        *out = NULL; \
        X( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid) ); \
        return E_NOINTERFACE;

#define INTERFACES_FWD( type, base, expr, X, ... ) INTERFACES_FWD_ ## X( type, base, expr, __VA_ARGS__ )
#define INTERFACES_FWD_END( type, base, expr, ... )

#define INTERFACE_IMPL_FROM( type, name ) INTERFACE_IMPL_FROM_( type, name, type ## _from_ ## name, name ## _iface )
#define INTERFACE_IMPL_FROM_( type, name, impl_from, iface_mem ) \
    static struct type *impl_from( name *iface ) \
    { \
        return CONTAINING_RECORD( iface, struct type, iface_mem ); \
    }

#define IUNKNOWN_IMPL_QUERY_INTERFACE( type, name, ... ) IUNKNOWN_IMPL_QUERY_INTERFACE_( type, name, type ## _from_ ## name, __VA_ARGS__ )
#define IUNKNOWN_IMPL_QUERY_INTERFACE_( type, name, impl_from, ... ) \
    static HRESULT WINAPI type ## _QueryInterface( name *iface, REFIID iid, void **out ) \
    { \
        struct type *object = impl_from( iface ); \
        TRACE( "object %p, iid %s, out %p.\n", object, debugstr_guid(iid), out ); \
        QUERY_INTERFACES( object, iid, out, name, __VA_ARGS__ ); \
    }

#define IUNKNOWN_IMPL_ADDREF( type, name ) IUNKNOWN_IMPL_ADDREF_( type, name, type ## _from_ ## name )
#define IUNKNOWN_IMPL_ADDREF_( type, name, impl_from ) \
    static ULONG WINAPI type ## _AddRef( name *iface ) \
    { \
        struct type *object = impl_from( iface ); \
        ULONG ref = InterlockedIncrement( &object->refcount ); \
        TRACE( "object %p increasing refcount to %lu.\n", object, ref ); \
        return ref; \
    }
#define IUNKNOWN_IMPL_STATIC_ADDREF( type, name ) \
    static ULONG WINAPI type ## _AddRef( name *iface ) \
    { \
        return 2; \
    }

#define IUNKNOWN_IMPL_RELEASE( type, name ) IUNKNOWN_IMPL_RELEASE_( type, name, type ## _from_ ## name )
#define IUNKNOWN_IMPL_RELEASE_( type, name, impl_from ) \
    static ULONG WINAPI type ## _Release( name *iface ) \
    { \
        struct type *object = impl_from( iface ); \
        ULONG ref = InterlockedDecrement( &object->refcount ); \
        TRACE( "object %p decreasing refcount to %lu.\n", object, ref); \
        if (!ref) \
        { \
            InterlockedIncrement( &object->refcount ); /* guard against re-entry when aggregated */ \
            type ## _destroy( object ); \
        } \
        return ref; \
    }
#define IUNKNOWN_IMPL_STATIC_RELEASE( type, name ) \
    static ULONG WINAPI type ## _Release( name *iface ) \
    { \
        return 1; \
    }

#define IUNKNOWN_IMPL( type, name, ... ) \
    IUNKNOWN_IMPL_QUERY_INTERFACE_( type, name, type ## _from_ ## name, __VA_ARGS__ ) \
    IUNKNOWN_IMPL_ADDREF_( type, name, type ## _from_ ## name ) \
    IUNKNOWN_IMPL_RELEASE_( type, name, type ## _from_ ## name )

#define IUNKNOWN_IMPL_STATIC( type, name, ... ) \
    IUNKNOWN_IMPL_QUERY_INTERFACE_( type, name, type ## _from_ ## name, __VA_ARGS__ ) \
    IUNKNOWN_IMPL_STATIC_ADDREF( type, name ) \
    IUNKNOWN_IMPL_STATIC_RELEASE( type, name )

#define IUNKNOWN_FWD( type, name, base, expr ) IUNKNOWN_FWD_( type, name, base, expr, type ## _from_ ## name, type ## _ ## name )
#define IUNKNOWN_FWD_( type, name, base, expr, impl_from, prefix ) \
    static HRESULT WINAPI prefix ## _QueryInterface( name *iface, REFIID iid, void **out ) \
    { \
        struct type *object = impl_from( iface ); \
        return base ## _QueryInterface( (expr), iid, out ); \
    } \
    static ULONG WINAPI prefix ## _AddRef( name *iface ) \
    { \
        struct type *object = impl_from( iface ); \
        return base ## _AddRef( (expr) ); \
    } \
    static ULONG WINAPI prefix ## _Release( name *iface ) \
    { \
        struct type *object = impl_from( iface ); \
        return base ## _Release( (expr) ); \
    }

#define IINSPECTABLE_IMPL( type, name ) IINSPECTABLE_IMPL_( type, name, type ## _from_ ## name )
#define IINSPECTABLE_IMPL_( type, name, impl_from ) \
    static HRESULT WINAPI type ## _GetIids( name *iface, ULONG *count, IID **iids ) \
    { \
        struct type *object = impl_from( iface ); \
        FIXME( "object %p, count %p, iids %p, stub!\n", object, count, iids ); \
        return E_NOTIMPL; \
    } \
    static HRESULT WINAPI type ## _GetRuntimeClassName( name *iface, HSTRING *class_name ) \
    { \
        struct type *object = impl_from( iface ); \
        if (!object->class_name) \
        { \
            FIXME( "object %p, class_name %p, stub!\n", object, class_name ); \
            return E_NOTIMPL; \
        } \
        TRACE( "object %p, class_name %p.\n", object, class_name ); \
        return WindowsCreateString( object->class_name, wcslen( object->class_name ), class_name ); \
    } \
    static HRESULT WINAPI type ## _GetTrustLevel( name *iface, TrustLevel *trust_level ) \
    { \
        struct type *object = impl_from( iface ); \
        FIXME( "object %p, trust_level %p, stub!\n", object, trust_level ); \
        return E_NOTIMPL; \
    }

#define IINSPECTABLE_FWD( type, name, base, expr ) IINSPECTABLE_FWD_( type, name, base, expr, type ## _from_ ## name, type ## _ ## name )
#define IINSPECTABLE_FWD_( type, name, base, expr, impl_from, prefix ) \
    static HRESULT WINAPI prefix ## _GetIids( name *iface, ULONG *count, IID **iids ) \
    { \
        struct type *object = impl_from( iface ); \
        return base ## _GetIids( (expr), count, iids ); \
    } \
    static HRESULT WINAPI prefix ## _GetRuntimeClassName( name *iface, HSTRING *class_name ) \
    { \
        struct type *object = impl_from( iface ); \
        return base ## _GetRuntimeClassName( (expr), class_name ); \
    } \
    static HRESULT WINAPI prefix ## _GetTrustLevel( name *iface, TrustLevel *trust_level ) \
    { \
        struct type *object = impl_from( iface ); \
        return base ## _GetTrustLevel( (expr), trust_level ); \
    }

#endif  /* __WINE_WINE_COMIMPL_H */
