/*
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

#include "config.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "widl.h"
#include "utils.h"
#include "parser.h"
#include "header.h"
#include "expr.h"
#include "typetree.h"
#include "typelib.h"

static void indent( FILE *out, int delta )
{
    static int level = 0;
    int i;

    if (delta < 0) level += delta;
    else if (delta > 0) level += delta;
    else for (i = 0; i < level; i++) fprintf( out, "    " );
}

static void write_line( FILE *out, const char *fmt, ... )
{
    va_list ap;
    indent( out, 0 );
    va_start( ap, fmt );
    vfprintf( out, fmt, ap );
    va_end( ap );
    fprintf( out, "\n" );
}

static void write_widl_using_macros( FILE *out, const type_t *type )
{
    const struct uuid *uuid = get_attrp( type->attrs, ATTR_UUID );
    const char *name = type->short_name ? type->short_name : type->name;
    char *macro;

    if (!strcmp( type->name, type->c_name )) return;

    macro = format_namespace( type->namespace, "WIDL_using_", "_", NULL, NULL );
    write_line( out, "#ifdef %s", macro );

    if (uuid) write_line( out, "#define CLSID_%s CLSID_%s", name, type->c_name );
    write_line( out, "#define %sVtbl %sVtbl", name, type->c_name );
    write_line( out, "#define %s %s", name, type->c_name );

    write_line( out, "#endif /* %s */\n", macro );
    free( macro );
}

static int is_override_method( const type_t *iface, const var_t *func )
{
    const statement_t *stmt;
    type_t *base;

    if (!(base = type_iface_get_inherit( iface ))) return 0;

    do STATEMENTS_FOR_EACH_FUNC( stmt, type_iface_get_stmts( base ) )
    {
        char inherit_name[256];
        strcpy( inherit_name, get_name( stmt->u.var ) );
        if (!strcmp( inherit_name, get_name( func ) )) return 1;
    }
    while ((base = type_iface_get_inherit( base )));

    return 0;
}

static inline const typeref_list_t *klass_get_ifaces( const type_t *klass )
{
    static typeref_list_t empty_ifaces = LIST_INIT( empty_ifaces );
    const typeref_list_t *ifaces = NULL;

    if (klass->type_type == TYPE_COCLASS) ifaces = type_coclass_get_ifaces( klass );
    if (klass->type_type == TYPE_RUNTIMECLASS) ifaces = type_runtimeclass_get_ifaces( klass );
    if (klass->type_type == TYPE_STRUCT) ifaces = get_attrp( klass->attrs, ATTR_IIDIS );
    if (!ifaces) ifaces = &empty_ifaces;

    return ifaces;
}

static inline type_t *klass_get_default_iface( const type_t *klass )
{
    const typeref_list_t *ifaces = klass_get_ifaces( klass );
    const typeref_t *ref;

    LIST_FOR_EACH_ENTRY(ref, ifaces, typeref_t, entry)
        if (is_attr(ref->attrs, ATTR_DEFAULT))
            return ref->type;

    return LIST_ENTRY( list_head( ifaces ), typeref_t, entry )->type;
}

static inline void make_mock_iface_list( type_t *klass )
{
    typeref_list_t *ifaces;
    const var_t *var;
    typeref_t *ref;

    if (klass->type_type != TYPE_STRUCT) return;
    if (!(ifaces = malloc( sizeof(*ifaces) ))) return;
    list_init( ifaces );

    LIST_FOR_EACH_ENTRY( var, klass->details.structure->fields, var_t, entry )
    {
        if (!strstr(var->name, "_iface")) continue;
        if (!(ref = make_typeref( var->declspec.type ))) break;
        list_add_tail( ifaces, &ref->entry );
    }

    klass->attrs = append_attr( klass->attrs, make_attrp(ATTR_IIDIS, ifaces) );
}

static const type_t *find_common_base_iface( const type_t *klass, const type_t *top_iface, const type_t *iface )
{
    const typeref_list_t *ifaces = klass_get_ifaces( klass );
    const type_t *base;
    typeref_t *ref;

    LIST_FOR_EACH_ENTRY( ref, ifaces, typeref_t, entry )
    {
        base = ref->type;
        while (base && base != iface) base = type_iface_get_inherit( base );
        if (base == iface) return ref->type;
    }

    return NULL;
}

static const var_t *find_internal_member( const type_t *klass, const type_t *top_iface, const type_t *iface )
{
    const var_t *var;

    if (klass->type_type != TYPE_STRUCT) return NULL;
    LIST_FOR_EACH_ENTRY( var, klass->details.structure->fields, var_t, entry )
    {
        if (!strstr(var->name, "_internal")) continue;
        if (type_pointer_get_ref( var->declspec.type )->type == iface) return var;
    }

    return NULL;
}

static size_t strappend_ptr_asterisk( char **buf, size_t *len, size_t pos )
{
    size_t off = 0;
    if (pos && (*buf)[pos - 1] != '*') off += strappend( buf, len, pos + off, " " );
    off += strappend( buf, len, pos + off, "*" );
    return off;
}

static size_t strappend_basic_type( char **buf, size_t *len, size_t pos, enum type_basic_type type, int sign )
{
    size_t off = 0;

    if (type != TYPE_BASIC_INT32 && type != TYPE_BASIC_INT64 &&
        type != TYPE_BASIC_LONG && type != TYPE_BASIC_HYPER)
    {
        if (sign < 0) off += strappend( buf, len, pos + off, "signed " );
        else if (sign > 0) off += strappend( buf, len, pos + off, "unsigned " );
    }

    switch (type)
    {
    case TYPE_BASIC_INT8: off += strappend( buf, len, pos + off, "small" ); break;
    case TYPE_BASIC_INT16: off += strappend( buf, len, pos + off, "short" ); break;
    case TYPE_BASIC_INT: off += strappend( buf, len, pos + off, "int" ); break;
    case TYPE_BASIC_INT3264: off += strappend( buf, len, pos + off, "__int3264" ); break;
    case TYPE_BASIC_BYTE: off += strappend( buf, len, pos + off, "byte" ); break;
    case TYPE_BASIC_CHAR: off += strappend( buf, len, pos + off, "char" ); break;
    case TYPE_BASIC_WCHAR: off += strappend( buf, len, pos + off, "wchar_t" ); break;
    case TYPE_BASIC_FLOAT: off += strappend( buf, len, pos + off, "float" ); break;
    case TYPE_BASIC_DOUBLE: off += strappend( buf, len, pos + off, "double" ); break;
    case TYPE_BASIC_ERROR_STATUS_T: off += strappend( buf, len, pos + off, "error_status_t" ); break;
    case TYPE_BASIC_HANDLE: off += strappend( buf, len, pos + off, "handle_t" ); break;

    case TYPE_BASIC_INT32:
        if (sign > 0) off += strappend( buf, len, pos + off, "UINT32" );
        else off += strappend( buf, len, pos + off, "INT32" );
        break;
    case TYPE_BASIC_LONG:
        if (sign > 0) off += strappend( buf, len, pos + off, "ULONG" );
        else off += strappend( buf, len, pos + off, "LONG" );
        break;
    case TYPE_BASIC_INT64:
        if (sign > 0) off += strappend( buf, len, pos + off, "UINT64" );
        else off += strappend( buf, len, pos + off, "INT64" );
        break;
    case TYPE_BASIC_HYPER:
        if (sign > 0) off += strappend( buf, len, pos + off, "MIDL_uhyper" );
        else off += strappend( buf, len, pos + off, "hyper" );
        break;
    }

    return off;
}

static size_t strappend_decl_spec( char **buf, size_t *len, size_t pos, const decl_spec_t *decl )
{
    const type_t *type = decl->type;
    enum type_type kind;
    size_t off = 0;

    switch ((kind = type_get_type_detect_alias( type )))
    {
    case TYPE_POINTER:
        off += strappend_decl_spec( buf, len, pos + off, type_pointer_get_ref( type ) );
        off += strappend_ptr_asterisk( buf, len, pos + off );
        if (decl->qualifier & TYPE_QUALIFIER_CONST) off += strappend( buf, len, pos + off, " const" );
        break;
    case TYPE_ARRAY:
        off += strappend_decl_spec( buf, len, pos + off, type_array_get_element( type ) );
        off += strappend_ptr_asterisk( buf, len, pos + off );
        if (decl->qualifier & TYPE_QUALIFIER_CONST) off += strappend( buf, len, pos + off, " const" );
        break;
    case TYPE_ENUM:
        if (decl->qualifier & TYPE_QUALIFIER_CONST) off += strappend( buf, len, pos + off, "const " );
        off += strappend( buf, len, pos + off, "enum %s", type_get_decl_name( type, NAME_C ) );
        break;
    case TYPE_UNION:
        if (decl->qualifier & TYPE_QUALIFIER_CONST) off += strappend( buf, len, pos + off, "const " );
        off += strappend( buf, len, pos + off, "union %s", type_get_decl_name( type, NAME_C ) );
        break;
    case TYPE_STRUCT:
    case TYPE_ENCAPSULATED_UNION:
        if (decl->qualifier & TYPE_QUALIFIER_CONST) off += strappend( buf, len, pos + off, "const " );
        off += strappend( buf, len, pos + off, "struct %s", type_get_decl_name( type, NAME_C ) );
        break;
    case TYPE_RUNTIMECLASS:
    case TYPE_DELEGATE:
    case TYPE_INTERFACE:
    case TYPE_MODULE:
    case TYPE_COCLASS:
        if (decl->qualifier & TYPE_QUALIFIER_CONST) off += strappend( buf, len, pos + off, "const " );
        if (kind == TYPE_RUNTIMECLASS) type = type_runtimeclass_get_default_iface( type, TRUE );
        if (kind == TYPE_DELEGATE) type = type_delegate_get_iface( type );
        off += strappend( buf, len, pos + off, "%s", type_get_name( type, NAME_C ) );
        break;
    case TYPE_ALIAS:
        if (decl->qualifier & TYPE_QUALIFIER_CONST) off += strappend( buf, len, pos + off, "const " );
        if (type->c_name) off += strappend( buf, len, pos + off, "%s", type->c_name );
        else if (type->name) off += strappend( buf, len, pos + off, "%s", type->name );
        else off += strappend_decl_spec( buf, len, pos + off, type_alias_get_aliasee( type ) );
        break;
    case TYPE_BASIC:
        if (decl->qualifier & TYPE_QUALIFIER_CONST) off += strappend( buf, len, pos + off, "const " );
        off += strappend_basic_type( buf, len, pos + off, type_basic_get_type( type ), type_basic_get_sign( type ) );
        break;
    case TYPE_VOID: off += strappend( buf, len, pos + off, "void" ); break;
    case TYPE_FUNCTION:
    case TYPE_BITFIELD:
    case TYPE_PARAMETERIZED_TYPE:
    case TYPE_PARAMETER:
    case TYPE_APICONTRACT:
        assert( 0 );
        break;
    }

    return off;
}


static size_t strappend_fmt_basic_type( char **buf, size_t *len, size_t pos, enum type_basic_type type, int sign )
{
    size_t off = 0;
    switch (type)
    {
    case TYPE_BASIC_INT8: off += strappend( buf, len, pos + off, sign > 0 ? "%%hhu" : "%%hhd" ); break;
    case TYPE_BASIC_INT16: off += strappend( buf, len, pos + off, sign > 0 ? "%%hu" : "%%hd" ); break;
    case TYPE_BASIC_INT32: off += strappend( buf, len, pos + off, sign > 0 ? "%%u" : "%%d" ); break;
    case TYPE_BASIC_INT64: off += strappend( buf, len, pos + off, sign > 0 ? "%%I64u" : "%%I64d" ); break;
    case TYPE_BASIC_HYPER: off += strappend( buf, len, pos + off, sign > 0 ? "%%I64u" : "%%I64d" ); break;
    case TYPE_BASIC_LONG: off += strappend( buf, len, pos + off, sign > 0 ? "%%lu" : "%%ld" ); break;
    case TYPE_BASIC_INT: off += strappend( buf, len, pos + off, sign > 0 ? "%%u" : "%%d" ); break;
    case TYPE_BASIC_INT3264: off += strappend( buf, len, pos + off, "__int3264" ); break;
    case TYPE_BASIC_BYTE: off += strappend( buf, len, pos + off, "byte" ); break;
    case TYPE_BASIC_CHAR: off += strappend( buf, len, pos + off, "%%c" ); break;
    case TYPE_BASIC_WCHAR: off += strappend( buf, len, pos + off, "wchar_t" ); break;
    case TYPE_BASIC_FLOAT: off += strappend( buf, len, pos + off, "%%hf" ); break;
    case TYPE_BASIC_DOUBLE: off += strappend( buf, len, pos + off, "%%f" ); break;
    case TYPE_BASIC_ERROR_STATUS_T: off += strappend( buf, len, pos + off, "%%lu" ); break;
    case TYPE_BASIC_HANDLE: off += strappend( buf, len, pos + off, "%%p" ); break;
    }

    return off;
}

static size_t strappend_fmt_decl_spec( char **buf, size_t *len, size_t pos, const decl_spec_t *decl )
{
    const type_t *type = decl->type;
    enum type_type kind;
    size_t off = 0;

    switch ((kind = type_get_type_detect_alias( type )))
    {
    case TYPE_POINTER:
    case TYPE_ARRAY:
    case TYPE_UNION:
    case TYPE_STRUCT:
    case TYPE_ENCAPSULATED_UNION:
    case TYPE_RUNTIMECLASS:
    case TYPE_DELEGATE:
    case TYPE_INTERFACE:
    case TYPE_MODULE:
    case TYPE_COCLASS:
        off += strappend( buf, len, pos + off, "%%p" );
        break;
    case TYPE_ENUM:
        off += strappend( buf, len, pos + off, "%%#x" );
        break;
    case TYPE_ALIAS:
        off += strappend_fmt_decl_spec( buf, len, pos + off, type_alias_get_aliasee( type ) );
        break;
    case TYPE_BASIC:
        off += strappend_fmt_basic_type( buf, len, pos + off, type_basic_get_type( type ), type_basic_get_sign( type ) );
        break;
    case TYPE_VOID:
    case TYPE_FUNCTION:
    case TYPE_BITFIELD:
    case TYPE_PARAMETERIZED_TYPE:
    case TYPE_PARAMETER:
    case TYPE_APICONTRACT:
        assert( 0 );
        break;
    }

    return off;
}

static char *format_interface_method_fmt_args_spec( const type_t *klass, const type_t *top_iface, const var_t *func, int output )
{
    const var_list_t *args = type_function_get_args( func->declspec.type );
    struct list empty_args = LIST_INIT( empty_args );
    size_t len = 1, pos = 0;
    char *buf = xstrdup("");
    const var_t *arg;

    if (!args) args = &empty_args;

    LIST_FOR_EACH_ENTRY( arg, args, const var_t, entry )
    {
        if (output && type_get_type_detect_alias( arg->declspec.type ) != TYPE_POINTER) continue;
        if (output && type_get_type_detect_alias( type_pointer_get_ref( arg->declspec.type )->type ) == TYPE_VOID) continue;
        if (output && type_pointer_get_ref( arg->declspec.type )->qualifier & TYPE_QUALIFIER_CONST) continue;
        pos += strappend( &buf, &len, pos, ", %s ", arg->name );
        pos += strappend_fmt_decl_spec( &buf, &len, pos, output ? type_pointer_get_ref( arg->declspec.type ) : &arg->declspec );
    }

    return buf;
}

static char *format_interface_method_fmt_args_list( const type_t *klass, const type_t *top_iface, const var_t *func, int output )
{
    const var_list_t *args = type_function_get_args( func->declspec.type );
    struct list empty_args = LIST_INIT( empty_args );
    size_t len = 1, pos = 0;
    char *buf = xstrdup("");
    const var_t *arg;

    if (!args) args = &empty_args;

    LIST_FOR_EACH_ENTRY( arg, args, const var_t, entry )
    {
        if (output && type_get_type_detect_alias( arg->declspec.type ) != TYPE_POINTER) continue;
        if (output && type_get_type_detect_alias( type_pointer_get_ref( arg->declspec.type )->type ) == TYPE_VOID) continue;
        if (output && type_pointer_get_ref( arg->declspec.type )->qualifier & TYPE_QUALIFIER_CONST) continue;
        pos += strappend( &buf, &len, pos, output ? ", *%s" : ", %s", arg->name );
    }

    return buf;
}

static char *format_interface_method_ret( const type_t *klass, const type_t *top_iface, const var_t *func )
{
    const decl_spec_t *ret = type_function_get_ret( func->declspec.type );
    size_t len = 0, pos = 0;
    char *buf = NULL;

    pos += strappend_decl_spec( &buf, &len, pos, ret );

    return buf;
}

static char *format_interface_method_args_decl( const type_t *klass, const type_t *top_iface, const var_t *func )
{
    const var_list_t *args = type_function_get_args( func->declspec.type );
    struct list empty_args = LIST_INIT( empty_args );
    size_t len = 1, pos = 0;
    char *buf = xstrdup("");
    const var_t *arg;

    if (!args) args = &empty_args;

    LIST_FOR_EACH_ENTRY( arg, args, const var_t, entry )
    {
        pos += strappend( &buf, &len, pos, ", " );
        pos += strappend_decl_spec( &buf, &len, pos, &arg->declspec );
        if (pos && buf[pos - 1] != '*') pos += strappend( &buf, &len, pos, " " );
        pos += strappend( &buf, &len, pos, "%s", arg->name );
    }

    return buf;
}

static char *format_interface_method_args_list( const type_t *klass, const type_t *top_iface, const var_t *func )
{
    const var_list_t *args = type_function_get_args( func->declspec.type );
    struct list empty_args = LIST_INIT( empty_args );
    size_t len = 1, pos = 0;
    char *buf = xstrdup("");
    const var_t *arg;

    if (!args) args = &empty_args;

    LIST_FOR_EACH_ENTRY( arg, args, const var_t, entry )
        pos += strappend( &buf, &len, pos, ", %s", arg->name );

    return buf;
}

static void write_interface_method_default_forward_impl( FILE *out, const type_t *klass, const type_t *top_iface, const var_t *func )
{
    const type_t *default_iface = klass_get_default_iface( klass );
    char *ret = format_interface_method_ret( klass, top_iface, func ), *args;

    args = format_interface_method_args_decl( klass, top_iface, func );
    write_line( out, "static %s WINAPI %s_%s_%s( %s *iface%s )", ret, klass->name, top_iface->name, get_name( func ), top_iface->c_name, args );
    free( args );

    args = format_interface_method_args_list( klass, top_iface, func );
    write_line( out, "{" );
    write_line( out, "    struct %s *klass = CONTAINING_RECORD( iface, struct %s, %s_iface );", klass->c_name, klass->c_name, top_iface->name );
    write_line( out, "    return %s_%s_%s( &klass->%s_iface%s );", klass->name, default_iface->name, get_name( func ), default_iface->name, args );
    write_line( out, "}\n" );
    free( args );
}

static void write_interface_method_member_forward_impl( FILE *out, const type_t *klass, const type_t *top_iface, const var_t *func, const var_t *mem )
{
    char *ret = format_interface_method_ret( klass, top_iface, func ), *args;

    args = format_interface_method_args_decl( klass, top_iface, func );
    write_line( out, "static %s WINAPI %s_%s_%s( %s *iface%s )", ret, klass->name, top_iface->name, get_name( func ), top_iface->c_name, args );
    free( args );

    args = format_interface_method_args_list( klass, top_iface, func );
    write_line( out, "{" );
    write_line( out, "    struct %s *klass = CONTAINING_RECORD( iface, struct %s, %s_iface );", klass->c_name, klass->c_name, top_iface->name );
    write_line( out, "    return %s_%s( klass->%s%s );", type_pointer_get_ref( mem->declspec.type )->type->name, get_name( func ), mem->name, args );
    write_line( out, "}\n" );
    free( args );
}

static char *format_interface_is_equal_guid( const type_t *klass, const type_t *top_iface )
{
    const type_t *iface = top_iface;
    size_t len = 0, pos = 0;
    char *buf = NULL;

    pos += strappend( &buf, &len, pos, "IsEqualGUID( iid, &IID_%s )", iface->c_name );
    while ((iface = type_iface_get_inherit( iface )))
    {
        if (find_common_base_iface( klass, top_iface, iface ) != top_iface) continue;
        pos += strappend( &buf, &len, pos, " || IsEqualGUID( iid, &IID_%s )", iface->c_name );
    }

    return buf;
}

static void write_klass_query_interface_impl( FILE *out, const type_t *klass )
{
    const typeref_list_t *ifaces = klass_get_ifaces( klass );
    const type_t *default_iface = klass_get_default_iface( klass );
    struct list empty_ifaces = LIST_INIT( empty_ifaces );
    typeref_t *ref;
    char *guids;

    write_line( out, "static HRESULT WINAPI %s_%s_QueryInterface( %s *iface, const IID *iid, void **out )", klass->name, default_iface->name, default_iface->c_name );
    write_line( out, "{" );
    write_line( out, "    struct %s *klass = CONTAINING_RECORD( iface, struct %s, %s_iface );\n", klass->c_name, klass->c_name, default_iface->name );
    write_line( out, "    TRACE( \"iface %%p, iid %%s, out %%p.\\n\", iface, debugstr_guid( iid ), out );\n" );

    LIST_FOR_EACH_ENTRY( ref, ifaces, typeref_t, entry )
    {
        guids = format_interface_is_equal_guid( klass, ref->type );
        write_line( out, "    if (%s)", guids);
        free( guids );

        write_line( out, "    {");
        write_line( out, "        %s_AddRef( &klass->%s_iface );", ref->type->c_name, ref->type->name );
        write_line( out, "        *out = &klass->%s_iface;", ref->type->name );
        write_line( out, "        return S_OK;");
        write_line( out, "    }\n");
    }

    write_line( out, "    *out = NULL;" );
    write_line( out, "    FIXME( \"%%s not implemented, returning E_NOINTERFACE.\\n\", debugstr_guid( iid ) );" );
    write_line( out, "    return E_NOINTERFACE;" );
    write_line( out, "}\n" );
}

static void write_klass_add_ref_impl( FILE *out, const type_t *klass )
{
    const type_t *default_iface = klass_get_default_iface( klass );

    write_line( out, "static ULONG WINAPI %s_%s_AddRef( %s *iface )", klass->name, default_iface->name, default_iface->c_name );
    write_line( out, "{" );
    write_line( out, "    struct %s *klass = CONTAINING_RECORD( iface, struct %s, %s_iface );", klass->c_name, klass->c_name, default_iface->name );
    write_line( out, "    ULONG ref = InterlockedIncrement( &klass->refcount );" );
    write_line( out, "    TRACE( \"iface %%p increasing refcount to %%lu.\\n\", iface, ref );" );
    write_line( out, "    return ref;" );
    write_line( out, "}\n" );
}

static void write_klass_release_impl( FILE *out, const type_t *klass )
{
    const type_t *default_iface = klass_get_default_iface( klass );

    write_line( out, "static ULONG WINAPI %s_%s_Release( %s *iface )", klass->name, default_iface->name, default_iface->c_name );
    write_line( out, "{" );
    write_line( out, "    struct %s *klass = CONTAINING_RECORD( iface, struct %s, %s_iface );", klass->c_name, klass->c_name, default_iface->name );
    write_line( out, "    ULONG ref = InterlockedDecrement( &klass->refcount );" );
    write_line( out, "    TRACE( \"iface %%p decreasing refcount to %%lu.\\n\", iface, ref );" );
    write_line( out, "    if (!ref) klass->vtbl->Release( klass );" );
    write_line( out, "    return ref;" );
    write_line( out, "}\n" );
}

static void write_klass_get_iids_impl( FILE *out, const type_t *klass )
{
    const type_t *default_iface = klass_get_default_iface( klass );

    write_line( out, "static HRESULT WINAPI %s_%s_GetIids( %s *iface, ULONG *count, IID **iids )", klass->name, default_iface->name, default_iface->c_name );
    write_line( out, "{" );
    write_line( out, "    FIXME( \"iface %%p, count %%p, iids %%p stub!\\n\", iface, count, iids );" );
    write_line( out, "    return E_NOTIMPL;" );
    write_line( out, "}\n" );
}

static void write_klass_get_runtime_class_name_impl( FILE *out, const type_t *klass )
{
    const type_t *default_iface = klass_get_default_iface( klass );
    char *name = format_namespace( klass->namespace, "RuntimeClass_", "_",  klass->name, NULL);

    write_line( out, "static HRESULT WINAPI %s_%s_GetRuntimeClassName( %s *iface, HSTRING *name )", klass->name, default_iface->name, default_iface->c_name );
    write_line( out, "{" );
    write_line( out, "    TRACE( \"iface %%p, name %%p.\\n\", iface, name );" );
    write_line( out, "    return WindowsCreateString( %s, ARRAY_SIZE(%s), name );", name, name );
    write_line( out, "}\n" );

    free( name );
}

static void write_klass_get_trust_level_impl( FILE *out, const type_t *klass )
{
    const type_t *default_iface = klass_get_default_iface( klass );

    write_line( out, "static HRESULT WINAPI %s_%s_GetTrustLevel( %s *iface, TrustLevel *level )", klass->name, default_iface->name, default_iface->c_name );
    write_line( out, "{" );
    write_line( out, "    FIXME( \"iface %%p, level %%p stub!\\n\", iface, level );" );
    write_line( out, "    return E_NOTIMPL;" );
    write_line( out, "}\n" );
}

static void write_interface_method_impl( FILE *out, const type_t *klass, const type_t *top_iface, const type_t *iface, const var_t *func )
{
    char *ret, *args, *fmt;

    if (find_common_base_iface( klass, top_iface, iface ) != top_iface) return write_interface_method_default_forward_impl( out, klass, top_iface, func );

    if (!strcmp(iface->name, "IUnknown"))
    {
        if (!strcmp(func->name, "QueryInterface")) return write_klass_query_interface_impl( out, klass );
        if (!strcmp(func->name, "AddRef")) return write_klass_add_ref_impl( out, klass );
        if (!strcmp(func->name, "Release")) return write_klass_release_impl( out, klass );
        assert( 0 );
        return;
    }
    if (!strcmp(iface->name, "IInspectable"))
    {
        if (!strcmp(func->name, "GetIids")) return write_klass_get_iids_impl( out, klass );
        if (!strcmp(func->name, "GetRuntimeClassName")) return write_klass_get_runtime_class_name_impl( out, klass );
        if (!strcmp(func->name, "GetTrustLevel")) return write_klass_get_trust_level_impl( out, klass );
        assert( 0 );
        return;
    }

    ret = format_interface_method_ret( klass, top_iface, func );
    args = format_interface_method_args_decl( klass, top_iface, func );
    write_line( out, "static %s WINAPI %s_%s_%s( %s *iface%s )", ret, klass->name, top_iface->name, get_name( func ), top_iface->c_name, args );
    free( args );
    free( ret );

    write_line( out, "{" );
    write_line( out, "    struct %s *klass = CONTAINING_RECORD( iface, struct %s, %s_iface );", klass->c_name, klass->c_name, top_iface->name );
    write_line( out, "    HRESULT hr;\n" );

    fmt = format_interface_method_fmt_args_spec( klass, top_iface, func, 0 );
    args = format_interface_method_fmt_args_list( klass, top_iface, func, 0 );
    write_line( out, "    if (!klass->vtbl->%s)", get_name( func ) );
    write_line( out, "    {" );
    write_line( out, "        FIXME( \"klass %%p%s stub!\\n\", klass%s );", fmt, args );
    write_line( out, "        return E_NOTIMPL;" );
    write_line( out, "    }\n" );
    write_line( out, "    TRACE( \"klass %%p%s\\n\", klass%s );", fmt, args );
    free( fmt );
    free( args );

    args = format_interface_method_args_list( klass, top_iface, func );
    write_line( out, "    hr = klass->vtbl->%s( klass%s );", get_name( func ), args );
    free( args );

    fmt = format_interface_method_fmt_args_spec( klass, top_iface, func, 1 );
    args = format_interface_method_fmt_args_list( klass, top_iface, func, 1 );
    write_line( out, "    TRACE( \"klass %%p%s, return %%#lx\\n\", klass%s, hr );", fmt, args );
    free( fmt );
    free( args );

    write_line( out, "    return hr;" );
    write_line( out, "}\n" );
}

static void write_interface_methods_impl( FILE *out, const type_t *klass, const type_t *top_iface, const type_t *iface )
{
    const statement_t *stmt;
    type_t *base;

    if ((base = type_iface_get_inherit( iface ))) write_interface_methods_impl( out, klass, top_iface, base );

    STATEMENTS_FOR_EACH_FUNC( stmt, type_iface_get_stmts( iface ) )
    {
        const var_t *func = stmt->u.var;
        if (is_callas( func->attrs ) || is_override_method( iface, func )) continue;
        write_interface_method_impl( out, klass, top_iface, iface, func );
    }
}

static void write_interface_method_vtbl( FILE *out, const type_t *klass, const type_t *top_iface, const type_t *iface, const var_t *func )
{
    char *ret, *args;

    if (find_common_base_iface( klass, top_iface, iface ) != top_iface) return;
    if (!strcmp(iface->name, "IUnknown"))
    {
        if (!strcmp(func->name, "Release")) write_line( out, "ULONG (WINAPI *Release)( %s *klass );", klass->c_name );
        return;
    }
    if (!strcmp(iface->name, "IInspectable")) return;

    ret = format_interface_method_ret( klass, top_iface, func );
    args = format_interface_method_args_decl( klass, top_iface, func );
    write_line( out, "%s (WINAPI *%s)( %s *klass%s );", ret, get_name( func ), klass->c_name, args );
    free( args );
    free( ret );
}

static void write_interface_methods_vtbl( FILE *out, const type_t *klass, const type_t *top_iface, const type_t *iface )
{
    const statement_t *stmt;
    type_t *base;

    if ((base = type_iface_get_inherit( iface ))) write_interface_methods_vtbl( out, klass, top_iface, base );

    STATEMENTS_FOR_EACH_FUNC( stmt, type_iface_get_stmts( iface ) )
    {
        const var_t *func = stmt->u.var;
        if (is_callas( func->attrs ) || is_override_method( iface, func )) continue;
        write_interface_method_vtbl( out, klass, top_iface, iface, func );
    }
}

static void write_interface_vtbl_method( FILE *out, const type_t *klass, const type_t *top_iface, const var_t *func )
{
    write_line( out, "%s_%s_%s,", klass->name, top_iface->name, get_name( func ) );
}

static void write_interface_vtbl_methods( FILE *out, const type_t *klass, const type_t *top_iface, const type_t *iface )
{
    const statement_t *stmt;
    type_t *base;

    if ((base = type_iface_get_inherit( iface ))) write_interface_vtbl_methods( out, klass, top_iface, base );

    write_line( out, "/*** %s methods ***/", iface->name );

    STATEMENTS_FOR_EACH_FUNC( stmt, type_iface_get_stmts( iface ) )
    {
        const var_t *func = stmt->u.var;
        if (is_callas( func->attrs ) || is_override_method( iface, func )) continue;
        write_interface_vtbl_method( out, klass, top_iface, func );
    }
}

static void write_interface_vtbl( FILE *out, const type_t *klass, const type_t *top_iface )
{
    write_line( out, "static const %sVtbl %s_%s_vtbl =", top_iface->c_name, klass->name, top_iface->name );
    write_line( out, "{" );
    indent( out, +1 );

    write_interface_vtbl_methods( out, klass, top_iface, top_iface );

    indent( out, -1 );
    write_line( out, "};\n" );
}

static void write_interface_member( FILE *out, const type_t *type )
{
    write_line( out, "%s %s_iface;", type->c_name, type->name );
}

static void write_class_impl( FILE *out, const type_t *klass )
{
    const typeref_list_t *ifaces = klass_get_ifaces( klass );
    struct list empty_ifaces = LIST_INIT( empty_ifaces );
    typeref_t *ref;

    if (winrt_mode) write_widl_using_macros( out, klass );

    write_line( out, "typedef struct %sVtbl %sVtbl;", klass->c_name, klass->c_name );
    write_line( out, "typedef struct %s %s;\n", klass->c_name, klass->c_name );

    write_line( out, "struct %sVtbl", klass->c_name );
    write_line( out, "{" );
    indent( out, +1 );
    LIST_FOR_EACH_ENTRY( ref, ifaces, typeref_t, entry )
        write_interface_methods_vtbl( out, klass, ref->type, ref->type );
    indent( out, -1 );
    write_line( out, "};\n" );

    write_line( out, "struct %s", klass->c_name );
    write_line( out, "{" );
    indent( out, +1 );
    write_line( out, "%sVtbl *vtbl;", klass->c_name );
    LIST_FOR_EACH_ENTRY( ref, ifaces, typeref_t, entry )
        write_interface_member( out, ref->type );
    write_line( out, "LONG refcount;" );
    indent( out, -1 );
    write_line( out, "};\n" );

    LIST_FOR_EACH_ENTRY( ref, ifaces, typeref_t, entry )
    {
        write_interface_methods_impl( out, klass, ref->type, ref->type );
        write_interface_vtbl( out, klass, ref->type );
    }

    write_line( out, "void %s_init( %s *klass, %sVtbl *vtbl )", klass->name, klass->c_name, klass->c_name );
    write_line( out, "{" );

    LIST_FOR_EACH_ENTRY( ref, ifaces, typeref_t, entry )
        write_line( out, "    klass->%s_iface.lpVtbl = &%s_%s_vtbl;", ref->type->name, klass->name, ref->type->name );
    write_line( out, "    klass->refcount = 1;" );
    write_line( out, "    klass->vtbl = vtbl;" );

    write_line( out, "}\n" );
}

static void write_impl_stmts( FILE *out, const statement_list_t *stmts )
{
    const statement_t *stmt;

    if (!stmts) return;

    LIST_FOR_EACH_ENTRY( stmt, stmts, const statement_t, entry )
    {
        if (stmt->type != STMT_TYPE) continue;
        if (type_get_type( stmt->u.type ) == TYPE_COCLASS || type_get_type( stmt->u.type ) == TYPE_RUNTIMECLASS)
            write_class_impl( out, stmt->u.type );
        if (type_get_type( stmt->u.type ) == TYPE_STRUCT)
        {
            make_mock_iface_list( stmt->u.type );
            write_class_impl( out, stmt->u.type );
        }
    }
}

static void write_import(FILE *out, const char *filename)
{
  char *name = replace_extension( get_basename(filename), ".idl", "" );

  if (!strendswith( name, ".h" )) name = strmake( "%s.h", name );
  write_line(out, "#include <%s>", name);
  free(name);
}

static void write_imports(FILE *header, const statement_list_t *stmts)
{
  const statement_t *stmt;
  if (!stmts) return;
LIST_FOR_EACH_ENTRY( stmt, stmts, const statement_t, entry )
  {
    switch (stmt->type)
    {
      case STMT_TYPE:
        if (type_get_type(stmt->u.type) == TYPE_INTERFACE)
          write_imports(header, type_iface_get_stmts(stmt->u.type));
        break;
      case STMT_IMPORT:
        write_import(header, stmt->u.str);
        break;
      case STMT_LIBRARY:
        write_imports(header, stmt->u.lib->stmts);
        break;
      case STMT_TYPEREF:
      case STMT_IMPORTLIB:
      case STMT_TYPEDEF:
      case STMT_MODULE:
      case STMT_CPPQUOTE:
      case STMT_PRAGMA:
      case STMT_DECLARATION:
        break;
    }
  }
}

void write_impl( const statement_list_t *stmts )
{
    FILE *out;

    if (!do_impl) return;

    if (!(out = fopen( impl_name, "w" )))
    {
        error( "Could not open %s for output\n", impl_name );
        return;
    }
    write_line( out, "/*** Autogenerated by WIDL %s from %s - Do not edit ***/\n", PACKAGE_VERSION, input_name );

    write_line( out, "#include <stdarg.h>" );
    write_line( out, "#include <stddef.h>\n" );

    write_line( out, "#define COBJMACROS" );
    write_line( out, "#include \"windef.h\"" );
    write_line( out, "#include \"winbase.h\"" );
    write_line( out, "#include \"winstring.h\"" );
    write_line( out, "#include \"objbase.h\"\n" );

    write_imports( out, stmts );
    /* write_line( out, "#include \"%s\"\n", header_name ); */
    write_line( out, "#include \"wine/debug.h\"\n" );
    write_line( out, "WINE_DEFAULT_DEBUG_CHANNEL(comimpl);\n" );

    write_impl_stmts( out, stmts );

    fclose( out );
}

static void write_klass_query_interface_mock( FILE *out, const type_t *klass )
{
    const typeref_list_t *ifaces = klass_get_ifaces( klass );
    const type_t *default_iface = klass_get_default_iface( klass );
    struct list empty_ifaces = LIST_INIT( empty_ifaces );
    typeref_t *ref;
    char *guids;

    write_line( out, "static HRESULT WINAPI %s_%s_QueryInterface( %s *iface, const IID *iid, void **out )", klass->name, default_iface->name, default_iface->c_name );
    write_line( out, "{" );
    write_line( out, "    struct %s *klass = CONTAINING_RECORD( iface, struct %s, %s_iface );\n", klass->c_name, klass->c_name, default_iface->name );
    write_line( out, "    ok( 0, \"iface %%p, iid %%s, out %%p.\\n\", iface, debugstr_guid( iid ), out );\n" );

    LIST_FOR_EACH_ENTRY( ref, ifaces, typeref_t, entry )
    {
        guids = format_interface_is_equal_guid( klass, ref->type );
        write_line( out, "    if (%s)", guids);
        free( guids );

        write_line( out, "    {");
        write_line( out, "        %s_AddRef( (*out = &klass->%s_iface) );", ref->type->c_name, ref->type->name );
        write_line( out, "        return S_OK;");
        write_line( out, "    }\n");
    }

    write_line( out, "    *out = NULL;" );
    write_line( out, "    trace( \"%%s not implemented, returning E_NOINTERFACE.\\n\", debugstr_guid( iid ) );" );
    write_line( out, "    return E_NOINTERFACE;" );
    write_line( out, "}\n" );
}

static void write_klass_add_ref_mock( FILE *out, const type_t *klass )
{
    const type_t *default_iface = klass_get_default_iface( klass );
    write_line( out, "static ULONG WINAPI %s_%s_AddRef( %s *iface )", klass->name, default_iface->name, default_iface->c_name );
    write_line( out, "{" );
    write_line( out, "    return 2;" );
    write_line( out, "}\n" );
}

static void write_klass_release_mock( FILE *out, const type_t *klass )
{
    const type_t *default_iface = klass_get_default_iface( klass );
    write_line( out, "static ULONG WINAPI %s_%s_Release( %s *iface )", klass->name, default_iface->name, default_iface->c_name );
    write_line( out, "{" );
    write_line( out, "    return 1;" );
    write_line( out, "}\n" );
}

static void write_interface_method_mock( FILE *out, const type_t *klass, const type_t *top_iface, const type_t *iface, const var_t *func )
{
    char *ret, *args;
    const var_t *mem;

    if (find_common_base_iface( klass, top_iface, iface ) != top_iface) return write_interface_method_default_forward_impl( out, klass, top_iface, func );
    if ((mem = find_internal_member( klass, top_iface, iface ))) return write_interface_method_member_forward_impl( out, klass, top_iface, func, mem );

    if (!strcmp(iface->name, "IUnknown"))
    {
        if (!strcmp(func->name, "QueryInterface")) return write_klass_query_interface_mock( out, klass );
        if (!strcmp(func->name, "AddRef")) return write_klass_add_ref_mock( out, klass );
        if (!strcmp(func->name, "Release")) return write_klass_release_mock( out, klass );
        assert( 0 );
        return;
    }

    ret = format_interface_method_ret( klass, top_iface, func );
    args = format_interface_method_args_decl( klass, top_iface, func );
    write_line( out, "static %s WINAPI %s_%s_%s( %s *iface%s )", ret, klass->name, top_iface->name, get_name( func ), top_iface->c_name, args );
    free( args );
    free( ret );

    write_line( out, "{" );
    write_line( out, "    struct %s *klass = CONTAINING_RECORD( iface, struct %s, %s_iface );", klass->c_name, klass->c_name, top_iface->name );
    write_line( out, "    ok( 0, \"klass %%p\\n\", klass );" );
    write_line( out, "    return E_NOTIMPL;" );
    write_line( out, "}\n" );
}

static void write_interface_methods_mock( FILE *out, const type_t *klass, const type_t *top_iface, const type_t *iface )
{
    const statement_t *stmt;
    type_t *base;

    if ((base = type_iface_get_inherit( iface ))) write_interface_methods_mock( out, klass, top_iface, base );

    STATEMENTS_FOR_EACH_FUNC( stmt, type_iface_get_stmts( iface ) )
    {
        const var_t *func = stmt->u.var;
        if (is_callas( func->attrs ) || is_override_method( iface, func )) continue;
        write_interface_method_mock( out, klass, top_iface, iface, func );
    }
}

static void write_class_mock( FILE *out, const type_t *klass )
{
    const typeref_list_t *ifaces = klass_get_ifaces( klass );
    struct list empty_ifaces = LIST_INIT( empty_ifaces );
    typeref_t *ref;

    if (winrt_mode) write_widl_using_macros( out, klass );

    write_line( out, "struct %s", klass->c_name );
    write_line( out, "{" );
    indent( out, +1 );
    LIST_FOR_EACH_ENTRY( ref, ifaces, typeref_t, entry )
        write_interface_member( out, ref->type );
    indent( out, -1 );
    write_line( out, "};\n" );

    LIST_FOR_EACH_ENTRY( ref, ifaces, typeref_t, entry )
    {
        write_interface_methods_mock( out, klass, ref->type, ref->type );
        write_interface_vtbl( out, klass, ref->type );
    }

    write_line( out, "void %s_init( struct %s *klass )", klass->name, klass->c_name );
    write_line( out, "{" );
    LIST_FOR_EACH_ENTRY( ref, ifaces, typeref_t, entry )
        write_line( out, "    klass->%s_iface.lpVtbl = &%s_%s_vtbl;", ref->type->name, klass->name, ref->type->name );
    write_line( out, "}\n" );
}

static void write_mock_stmts( FILE *out, const statement_list_t *stmts )
{
    const statement_t *stmt;

    if (!stmts) return;

    LIST_FOR_EACH_ENTRY( stmt, stmts, const statement_t, entry )
    {
        if (stmt->type != STMT_TYPE) continue;
        if (type_get_type( stmt->u.type ) == TYPE_STRUCT)
        {
            make_mock_iface_list( stmt->u.type );
            write_class_mock( out, stmt->u.type );
        }
    }
}

void write_mock( const statement_list_t *stmts )
{
    FILE *out;

    if (!do_header) return;

    if (!(out = fopen( header_name, "w" )))
    {
        error( "Could not open %s for output\n", header_name );
        return;
    }
    write_line( out, "/*** Autogenerated by WIDL %s from %s - Do not edit ***/\n", PACKAGE_VERSION, input_name );

    write_line( out, "#include <stdarg.h>" );
    write_line( out, "#include <stddef.h>\n" );

    write_line( out, "#define COBJMACROS" );
    write_line( out, "#include \"windef.h\"" );
    write_line( out, "#include \"winbase.h\"" );
    write_line( out, "#include \"winstring.h\"" );
    write_line( out, "#include \"objbase.h\"\n" );

    write_imports( out, stmts );
    write_line( out, "" );

    write_mock_stmts( out, stmts );

    fclose( out );
}
