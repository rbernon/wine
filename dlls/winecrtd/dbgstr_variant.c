/*
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

#include <stdarg.h>
#include <stddef.h>

#include "windef.h"
#include "winbase.h"
#include "oaidl.h"

#include "wine/debug.h"

const char *wine_dbgstr_vt( VARTYPE vt )
{
    static const char *const variant_types[] =
    {
        "VT_EMPTY","VT_NULL","VT_I2","VT_I4","VT_R4","VT_R8","VT_CY","VT_DATE",
        "VT_BSTR","VT_DISPATCH","VT_ERROR","VT_BOOL","VT_VARIANT","VT_UNKNOWN",
        "VT_DECIMAL","15","VT_I1","VT_UI1","VT_UI2","VT_UI4","VT_I8","VT_UI8",
        "VT_INT","VT_UINT","VT_VOID","VT_HRESULT","VT_PTR","VT_SAFEARRAY",
        "VT_CARRAY","VT_USERDEFINED","VT_LPSTR","VT_LPWSTR","32","33","34","35",
        "VT_RECORD","VT_INT_PTR","VT_UINT_PTR","39","40","41","42","43","44","45",
        "46","47","48","49","50","51","52","53","54","55","56","57","58","59","60",
        "61","62","63","VT_FILETIME","VT_BLOB","VT_STREAM","VT_STORAGE",
        "VT_STREAMED_OBJECT","VT_STORED_OBJECT","VT_BLOB_OBJECT","VT_CF","VT_CLSID",
        "VT_VERSIONED_STREAM"
    };

    static const char *const variant_flags[16] =
    {
        "",
        "|VT_VECTOR",
        "|VT_ARRAY",
        "|VT_VECTOR|VT_ARRAY",
        "|VT_BYREF",
        "|VT_VECTOR|VT_BYREF",
        "|VT_ARRAY|VT_BYREF",
        "|VT_VECTOR|VT_ARRAY|VT_BYREF",
        "|VT_RESERVED",
        "|VT_VECTOR|VT_RESERVED",
        "|VT_ARRAY|VT_RESERVED",
        "|VT_VECTOR|VT_ARRAY|VT_RESERVED",
        "|VT_BYREF|VT_RESERVED",
        "|VT_VECTOR|VT_BYREF|VT_RESERVED",
        "|VT_ARRAY|VT_BYREF|VT_RESERVED",
        "|VT_VECTOR|VT_ARRAY|VT_BYREF|VT_RESERVED",
    };

    if (vt & ~VT_TYPEMASK)
        return wine_dbg_sprintf( "%s%s", wine_dbgstr_vt(vt&VT_TYPEMASK), variant_flags[vt>>12] );

    if (vt < sizeof(variant_types)/sizeof(*variant_types))
        return variant_types[vt];

    if (vt == VT_BSTR_BLOB)
        return "VT_BSTR_BLOB";

    return wine_dbg_sprintf( "vt(invalid %x)", vt );
}

const char *wine_dbgstr_variant( const VARIANT *v )
{
    if (!v)
        return "(null)";

    if (V_VT(v) & VT_BYREF) {
        if (V_VT(v) == (VT_VARIANT|VT_BYREF))
            return wine_dbg_sprintf( "%p {VT_VARIANT|VT_BYREF: %s}", v, wine_dbgstr_variant(V_VARIANTREF(v)) );
        if (V_VT(v) == (VT_BSTR|VT_BYREF))
            return wine_dbg_sprintf( "%p {VT_BSTR|VT_BYREF: %s}", v, V_BSTRREF(v) ? wine_dbgstr_w(*V_BSTRREF(v)) : "(none)" );
        return wine_dbg_sprintf( "%p {%s %p}", v, wine_dbgstr_vt(V_VT(v)), V_BYREF(v) );
    }

    if (V_ISARRAY(v) || V_ISVECTOR(v))
        return wine_dbg_sprintf( "%p {%s %p}", v, wine_dbgstr_vt(V_VT(v)), V_ARRAY(v) );

    switch(V_VT(v)) {
    case VT_EMPTY:
        return wine_dbg_sprintf( "%p {VT_EMPTY}", v );
    case VT_NULL:
        return wine_dbg_sprintf( "%p {VT_NULL}", v );
    case VT_I2:
        return wine_dbg_sprintf( "%p {VT_I2: %d}", v, V_I2(v) );
    case VT_I4:
        return wine_dbg_sprintf( "%p {VT_I4: %d}", v, (int)V_I4(v) );
    case VT_R4:
        return wine_dbg_sprintf( "%p {VT_R4: %f}", v, V_R4(v) );
    case VT_R8:
        return wine_dbg_sprintf( "%p {VT_R8: %lf}", v, V_R8(v) );
    case VT_CY:
        return wine_dbg_sprintf( "%p {VT_CY: %s}", v, wine_dbgstr_longlong(V_CY(v).int64) );
    case VT_DATE:
        return wine_dbg_sprintf( "%p {VT_DATE: %lf}", v, V_DATE(v) );
    case VT_LPSTR:
        return wine_dbg_sprintf( "%p {VT_LPSTR: %s}", v, wine_dbgstr_a((const char *)V_BSTR(v)) );
    case VT_LPWSTR:
        return wine_dbg_sprintf( "%p {VT_LPWSTR: %s}", v, wine_dbgstr_w(V_BSTR(v)) );
    case VT_BSTR:
        return wine_dbg_sprintf( "%p {VT_BSTR: %s}", v, wine_dbgstr_w(V_BSTR(v)) );
    case VT_DISPATCH:
        return wine_dbg_sprintf( "%p {VT_DISPATCH: %p}", v, V_DISPATCH(v) );
    case VT_ERROR:
        return wine_dbg_sprintf( "%p {VT_ERROR: %08x}", v, (int)V_ERROR(v) );
    case VT_BOOL:
        return wine_dbg_sprintf( "%p {VT_BOOL: %x}", v, V_BOOL(v) );
    case VT_UNKNOWN:
        return wine_dbg_sprintf( "%p {VT_UNKNOWN: %p}", v, V_UNKNOWN(v) );
    case VT_I1:
        return wine_dbg_sprintf( "%p {VT_I1: %d}", v, V_I1(v) );
    case VT_UI1:
        return wine_dbg_sprintf( "%p {VT_UI1: %u}", v, V_UI1(v) );
    case VT_UI2:
        return wine_dbg_sprintf( "%p {VT_UI2: %u}", v, V_UI2(v) );
    case VT_UI4:
        return wine_dbg_sprintf( "%p {VT_UI4: %u}", v, (unsigned int)V_UI4(v) );
    case VT_I8:
        return wine_dbg_sprintf( "%p {VT_I8: %s}", v, wine_dbgstr_longlong(V_I8(v)) );
    case VT_UI8:
        return wine_dbg_sprintf( "%p {VT_UI8: %s}", v, wine_dbgstr_longlong(V_UI8(v)) );
    case VT_INT:
        return wine_dbg_sprintf( "%p {VT_INT: %d}", v, V_INT(v) );
    case VT_UINT:
        return wine_dbg_sprintf( "%p {VT_UINT: %u}", v, V_UINT(v) );
    case VT_VOID:
        return wine_dbg_sprintf( "%p {VT_VOID}", v );
    case VT_RECORD:
        return wine_dbg_sprintf( "%p {VT_RECORD: %p %p}", v, V_RECORD(v), V_RECORDINFO(v) );
    default:
        return wine_dbg_sprintf( "%p {vt %s}", v, wine_dbgstr_vt(V_VT(v)) );
    }
}
