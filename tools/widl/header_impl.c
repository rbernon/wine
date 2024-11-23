/*
 * IDL Compiler
 *
 * Copyright 2002 Ove Kaaven
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

#include "config.h"

#include <stdarg.h>
#include <stdio.h>

#include "widl.h"
#include "utils.h"
#include "parser.h"
#include "header.h"
#include "expr.h"
#include "typetree.h"
#include "typelib.h"

static int indent;

static void write_import( const char *import )
{
    char *header = replace_extension( get_basename( import ), ".idl", "" );
    if (!strendswith( header, ".h" )) header = strmake( "%s_impl.h", header );
    put_str( indent, "#include <%s>\n", header );
    free( header );
}

static void write_imports( const statement_list_t *stmts )
{
    const statement_t *stmt;

    if (stmts) LIST_FOR_EACH_ENTRY( stmt, stmts, const statement_t, entry )
    {
        switch (stmt->type)
        {
        case STMT_IMPORT: write_import( stmt->u.str ); break;
        case STMT_LIBRARY: write_imports( stmt->u.lib->stmts ); break;
        case STMT_TYPE:
            if (type_get_type( stmt->u.type ) != TYPE_INTERFACE) break;
            write_imports( type_iface_get_stmts( stmt->u.type ) );
            break;
        default:
            break;
        }
    }
}

static void write_widl_using_macros( const type_t *iface )
{
    const char *name = iface->short_name ? iface->short_name : iface->name;
    char *macro;

    if (!strcmp( iface->name, iface->c_name )) return;

    macro = format_namespace( iface->namespace, "WIDL_using_", "_", NULL, NULL );
    put_str( indent, "#ifdef %s\n", macro );

    put_str( indent, "#define WIDL_impl_from_%s   WIDL_impl_from_%s\n", name, iface->c_name );
    put_str( indent, "#define WIDL_impl_QueryInterface_%s  WIDL_impl_QueryInterface_%s\n", name, iface->c_name );
    put_str( indent, "#define WIDL_impl_each_QueryInterface_%s WIDL_impl_each_QueryInterface_%s\n", name, iface->c_name );
    put_str( indent, "#define WIDL_impl_%sVtbl    WIDL_impl_%sVtbl\n", name, iface->c_name );

    put_str( indent, "#endif /* %s */\n\n", macro );
    free( macro );
}

static void write_widl_impl_macros_methods( const type_t *iface, const type_t *top_iface, const char *prefix )
{
    const statement_t *stmt;

    if (type_iface_get_inherit( iface )) write_widl_impl_macros_methods( type_iface_get_inherit( iface ), top_iface, prefix );

    STATEMENTS_FOR_EACH_FUNC( stmt, type_iface_get_stmts( iface ) )
    {
        const var_t *func = stmt->u.var;

        if (is_override_method( iface, top_iface, func )) continue;
        if (is_callas( func->attrs )) continue;

        put_str( indent, "        %s_%s, \\\n", prefix, get_name( func ) );
    }
}

static void write_widl_impl_macros( const type_t *iface )
{
    const struct uuid *uuid = get_attrp( iface->attrs, ATTR_UUID );
    const char *name = iface->short_name ? iface->short_name : iface->name;
    const type_t *base = type_iface_get_inherit( iface );

    if (uuid)
    {
        put_str( indent, "#define WIDL_impl_from_%s( type ) \\\n", iface->c_name );
        put_str( indent, "    static struct type *type ## _from_%s( %s *iface ) \\\n", name, iface->c_name );
        put_str( indent, "    { \\\n" );
        put_str( indent, "        return CONTAINING_RECORD( iface, struct type, %s_iface ); \\\n", name );
        put_str( indent, "    }\n" );
        put_str( indent, "\n" );

        put_str( indent, "#define WIDL_impl_QueryInterface_%s( object, iid, out, mem ) \\\n", iface->c_name );
        put_str( indent, "    do if ((object)->mem.lpVtbl) \\\n" );
        put_str( indent, "    { \\\n" );
        put_str( indent, "        if (0 " );
        for (base = iface; base; (base = type_iface_get_inherit( base )))
            put_str( indent, "%s|| IsEqualGUID( (iid), &IID_%s )", base == iface ? "" : " \\\n              ", base->c_name );
        put_str( indent, ") \\\n" );
        put_str( indent, "        { \\\n" );
        put_str( indent, "            %s_AddRef( &(object)->mem ); \\\n", iface->c_name );
        put_str( indent, "            *(out) = &(object)->mem; \\\n" );
        put_str( indent, "            return S_OK; \\\n" );
        put_str( indent, "        } \\\n" );
        put_str( indent, "    } while (0)\n" );

        put_str( indent, "#define WIDL_impl_each_QueryInterface_%s( object, iid, out, X, ... ) \\\n", iface->c_name );
        put_str( indent, "    WIDL_impl_QueryInterface_%s( object, iid, out, %s_iface ); \\\n", iface->c_name, name );
        put_str( indent, "    WIDL_impl_each_QueryInterface_ ## X( object, iid, out, __VA_ARGS__ )\n" );
        put_str( indent, "\n" );

        put_str( indent, "#define WIDL_impl_%sVtbl( pfx ) \\\n", iface->c_name );
        put_str( indent, "    static const %sVtbl %s_vtbl = \\\n", iface->c_name, "pfx ## " );
        put_str( indent, "    { \\\n" );
        write_widl_impl_macros_methods( iface, iface, "pfx ## " );
        put_str( indent, "    };\n" );
        put_str( indent, "\n" );
    }
}

static void write_interface( const type_t *iface )
{
    put_str( indent, "/* %s */\n", iface->name );
    if (winrt_mode) write_widl_using_macros( iface );
    write_widl_impl_macros( iface );

    if (!strcmp( iface->name, "IUnknown" ))
    {
        put_str( indent, "#define WIDL_impl_each_QueryInterface( object, iid, out, X, ... ) WIDL_impl_each_QueryInterface_ ## X( object, iid, out, __VA_ARGS__ )\n" );
        put_str( indent, "#define WIDL_impl_each_QueryInterface_END( object, iid, out, X, ... ) \\\n" );
        put_str( indent, "        *out = NULL; \\\n" );
        put_str( indent, "        X( \"%%s not implemented, returning E_NOINTERFACE.\\n\", debugstr_guid(iid) ); \\\n" );
        put_str( indent, "        return E_NOINTERFACE;\n" );
        put_str( indent, "\n" );
        put_str( indent, "#define WIDL_impl_IUnknown_QueryInterface( type, name, ... ) WIDL_impl_IUnknown_QueryInterface_( type, name, type ## _from_ ## name, __VA_ARGS__ )\n" );
        put_str( indent, "#define WIDL_impl_IUnknown_QueryInterface_( type, name, impl_from, ... ) \\\n" );
        put_str( indent, "    static HRESULT WINAPI type ## _QueryInterface( name *iface, REFIID iid, void **out ) \\\n" );
        put_str( indent, "    { \\\n" );
        put_str( indent, "        struct type *object = impl_from( iface ); \\\n" );
        put_str( indent, "        TRACE( \"object %%p, iid %%s, out %%p.\\n\", object, debugstr_guid(iid), out ); \\\n" );
        put_str( indent, "        WIDL_impl_each_QueryInterface( object, iid, out, name, __VA_ARGS__ ); \\\n" );
        put_str( indent, "    }\n" );
        put_str( indent, "\n" );

        put_str( indent, "#define WIDL_impl_IUnknown_AddRef( type, name ) WIDL_impl_IUnknown_AddRef_( type, name, type ## _from_ ## name )\n" );
        put_str( indent, "#define WIDL_impl_IUnknown_AddRef_( type, name, impl_from ) \\\n" );
        put_str( indent, "    static ULONG WINAPI type ## _AddRef( name *iface ) \\\n" );
        put_str( indent, "    { \\\n" );
        put_str( indent, "        struct type *object = impl_from( iface ); \\\n" );
        put_str( indent, "        ULONG ref = InterlockedIncrement( &object->refcount ); \\\n" );
        put_str( indent, "        TRACE( \"object %%p increasing refcount to %%lu.\\n\", object, ref ); \\\n" );
        put_str( indent, "        return ref; \\\n" );
        put_str( indent, "    }\n" );
        put_str( indent, "#define WIDL_impl_static_IUnknown_AddRef( type, name ) \\\n" );
        put_str( indent, "    static ULONG WINAPI type ## _AddRef( name *iface ) \\\n" );
        put_str( indent, "    { \\\n" );
        put_str( indent, "        return 2; \\\n" );
        put_str( indent, "    }\n" );
        put_str( indent, "\n" );

        put_str( indent, "#define WIDL_impl_IUnknown_Release( type, name ) WIDL_impl_IUnknown_Release_( type, name, type ## _from_ ## name )\n" );
        put_str( indent, "#define WIDL_impl_IUnknown_Release_( type, name, impl_from ) \\\n" );
        put_str( indent, "    static ULONG WINAPI type ## _Release( name *iface ) \\\n" );
        put_str( indent, "    { \\\n" );
        put_str( indent, "        struct type *object = impl_from( iface ); \\\n" );
        put_str( indent, "        ULONG ref = InterlockedDecrement( &object->refcount ); \\\n" );
        put_str( indent, "        TRACE( \"object %%p decreasing refcount to %%lu.\\n\", object, ref); \\\n" );
        put_str( indent, "        if (!ref) \\\n" );
        put_str( indent, "        { \\\n" );
        put_str( indent, "            InterlockedIncrement( &object->refcount ); /* guard against re-entry when aggregated */ \\\n" );
        put_str( indent, "            type ## _destroy( object ); \\\n" );
        put_str( indent, "        } \\\n" );
        put_str( indent, "        return ref; \\\n" );
        put_str( indent, "    }\n" );
        put_str( indent, "#define WIDL_impl_static_IUnknown_Release( type, name ) \\\n" );
        put_str( indent, "    static ULONG WINAPI type ## _Release( name *iface ) \\\n" );
        put_str( indent, "    { \\\n" );
        put_str( indent, "        return 1; \\\n" );
        put_str( indent, "    }\n" );
        put_str( indent, "\n" );

        put_str( indent, "#define WIDL_impl_IUnknown_forwards( type, name, base, expr ) WIDL_impl_IUnknown_forwards_( type, name, base, expr, type ## _from_ ## name, type ## _ ## name )\n" );
        put_str( indent, "#define WIDL_impl_IUnknown_forwards_( type, name, base, expr, impl_from, prefix ) \\\n" );
        put_str( indent, "    static HRESULT WINAPI prefix ## _QueryInterface( name *iface, REFIID iid, void **out ) \\\n" );
        put_str( indent, "    { \\\n" );
        put_str( indent, "        struct type *object = impl_from( iface ); \\\n" );
        put_str( indent, "        return base ## _QueryInterface( (expr), iid, out ); \\\n" );
        put_str( indent, "    } \\\n" );
        put_str( indent, "    static ULONG WINAPI prefix ## _AddRef( name *iface ) \\\n" );
        put_str( indent, "    { \\\n" );
        put_str( indent, "        struct type *object = impl_from( iface ); \\\n" );
        put_str( indent, "        return base ## _AddRef( (expr) ); \\\n" );
        put_str( indent, "    } \\\n" );
        put_str( indent, "    static ULONG WINAPI prefix ## _Release( name *iface ) \\\n" );
        put_str( indent, "    { \\\n" );
        put_str( indent, "        struct type *object = impl_from( iface ); \\\n" );
        put_str( indent, "        return base ## _Release( (expr) ); \\\n" );
        put_str( indent, "    }\n" );
        put_str( indent, "\n" );
    }

    if (!strcmp( iface->name, "IInspectable" ))
    {
        put_str( indent, "#define WIDL_impl_IInspectable_forwards( type, name, base, expr ) WIDL_impl_IInspectable_forwards_( type, name, base, expr, type ## _from_ ## name, type ## _ ## name )\n" );
        put_str( indent, "#define WIDL_impl_IInspectable_forwards_( type, name, base, expr, impl_from, prefix ) \\\n" );
        put_str( indent, "    static HRESULT WINAPI prefix ## _GetIids( name *iface, ULONG *count, IID **iids ) \\\n" );
        put_str( indent, "    { \\\n" );
        put_str( indent, "        struct type *object = impl_from( iface ); \\\n" );
        put_str( indent, "        return base ## _GetIids( (expr), count, iids ); \\\n" );
        put_str( indent, "    } \\\n" );
        put_str( indent, "    static HRESULT WINAPI prefix ## _GetRuntimeClassName( name *iface, HSTRING *class_name ) \\\n" );
        put_str( indent, "    { \\\n" );
        put_str( indent, "        struct type *object = impl_from( iface ); \\\n" );
        put_str( indent, "        return base ## _GetRuntimeClassName( (expr), class_name ); \\\n" );
        put_str( indent, "    } \\\n" );
        put_str( indent, "    static HRESULT WINAPI prefix ## _GetTrustLevel( name *iface, TrustLevel *trust_level ) \\\n" );
        put_str( indent, "    { \\\n" );
        put_str( indent, "        struct type *object = impl_from( iface ); \\\n" );
        put_str( indent, "        return base ## _GetTrustLevel( (expr), trust_level ); \\\n" );
        put_str( indent, "    }\n" );
        put_str( indent, "\n" );
    }
}

static void write_interfaces( const statement_list_t *stmts )
{
    const statement_t *stmt;

    LIST_FOR_EACH_ENTRY( stmt, stmts, const statement_t, entry )
    {
        const type_t *type = stmt->u.type;
        enum type_type kind;

        if (stmt->type != STMT_TYPE) continue;
        switch ((kind = type_get_type( type )))
        {
        case TYPE_INTERFACE: write_interface( type ); break;
        default: break;
        }
    }
}

void write_header_impl( const statement_list_t *stmts )
{
    char *impl_name;
    FILE *file;

    if (!do_header) return;

    init_output_buffer();
    put_str( indent, "/*** Autogenerated by WIDL %s from %s - Do not edit ***/\n\n", PACKAGE_VERSION, input_name );
    put_str( indent, "#ifndef __%s_impl__\n", header_token );
    put_str( indent, "#define __%s_impl__\n\n", header_token );
    put_str( indent, "#include <%s>\n\n", get_basename( header_name ) );

    write_imports( stmts );
    put_str( indent, "\n" );

    write_interfaces( stmts );
    put_str( indent, "#endif /* __%s_impl__ */\n", header_token );

    impl_name = replace_extension( header_name, ".h", "_impl.h" );
    if (!(file = fopen( impl_name, "w" ))) error( "Could not open %s for output\n", impl_name );
    if (fwrite( output_buffer, 1, output_buffer_pos, file ) != output_buffer_pos) error( "Failed to write to %s\n", impl_name );
    if (fclose( file )) error( "Failed to write to %s\n", impl_name );
    free( impl_name );
}
