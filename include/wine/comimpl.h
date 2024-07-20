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

#define INTERFACE_IMPL_FROM( type, name ) INTERFACE_IMPL_FROM_( type, name, type ## _from_ ## name, name ## _iface )
#define INTERFACE_IMPL_FROM_( type, name, impl_from, iface_mem ) \
    static struct type *impl_from( name *iface ) \
    { \
        return CONTAINING_RECORD( iface, struct type, iface_mem ); \
    }

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
