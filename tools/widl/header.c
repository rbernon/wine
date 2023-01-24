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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "widl.h"
#include "utils.h"
#include "parser.h"
#include "header.h"
#include "typetree.h"
#include "typelib.h"

static int indentation = 0;
static int is_object_interface = 0;
static const char *default_callconv;
user_type_list_t user_type_list = LIST_INIT(user_type_list);
context_handle_list_t context_handle_list = LIST_INIT(context_handle_list);
generic_handle_list_t generic_handle_list = LIST_INIT(generic_handle_list);

static void put_declspec_full( const decl_spec_t *ds, int is_field, int declonly, const char *name,
                               enum name_type name_type, int (*put_str)( FILE *, const char *, ... ), FILE *file );
static void put_args( const var_list_t *args, const char *name, int method, int do_indent,
                      enum name_type name_type, int (*put_str)( FILE *, const char *, ... ), FILE *file );

static void indent(FILE *h, int delta)
{
  int c;
  if (delta < 0) indentation += delta;
  for (c=0; c<indentation; c++) if (h) fprintf(h, "    ");
  if (delta > 0) indentation += delta;
}

static void write_line(FILE *f, int delta, const char *fmt, ...)
{
    va_list ap;
    indent(f, delta);
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fprintf(f, "\n");
}

static int is_override_method( const type_t *iface, const type_t *child, const var_t *func )
{
    if (iface == child) return 0;

    do
    {
        const statement_t *stmt;
        STATEMENTS_FOR_EACH_FUNC( stmt, type_iface_get_stmts( child ) )
        {
            const var_t *funccmp = stmt->u.var;

            if (!is_callas( func->attrs ))
            {
                char inherit_name[256];
                /* compare full name including property prefix */
                strcpy( inherit_name, get_name( funccmp ) );
                if (!strcmp( inherit_name, get_name( func ) )) return 1;
            }
        }
    } while ((child = type_iface_get_inherit( child )) && child != iface);

    return 0;
}

void *get_attrp(const attr_list_t *list, enum attr_type t)
{
    const attr_t *attr;
    if (list) LIST_FOR_EACH_ENTRY( attr, list, const attr_t, entry )
        if (attr->type == t) return attr->u.pval;
    return NULL;
}

unsigned int get_attrv(const attr_list_t *list, enum attr_type t)
{
    const attr_t *attr;
    if (list) LIST_FOR_EACH_ENTRY( attr, list, const attr_t, entry )
        if (attr->type == t) return attr->u.ival;
    return 0;
}

static char *format_parameterized_type_args(const type_t *type, const char *prefix, const char *suffix)
{
    struct strbuf str = {0};
    typeref_list_t *params;
    typeref_t *ref;

    params = type->details.parameterized.params;
    if (params) LIST_FOR_EACH_ENTRY(ref, params, typeref_t, entry)
    {
        assert(ref->type->type_type != TYPE_POINTER);
        strappend( &str, "%s%s%s", prefix, ref->type->name, suffix );
        if (list_next( params, &ref->entry )) strappend( &str, ", " );
    }

    if (!str.buf) return xstrdup( "" );
    return str.buf;
}

static char *format_apicontract_macro( const type_t *type )
{
    char *name = format_namespace( type->namespace, "", "_", type->name, NULL );
    int i;
    for (i = strlen( name ); i > 0; --i) name[i - 1] = toupper( name[i - 1] );
    return name;
}

static void put_apicontract_guard_start( const expr_t *expr, int (*put_str)( FILE *, const char *, ... ), FILE *file )
{
    const type_t *type;
    char *name;
    int ver;
    if (!winrt_mode) return;
    type = expr->u.args[0]->u.decl->type;
    ver = expr->u.args[1]->u.lval;
    name = format_apicontract_macro( type );
    put_str( file, "#if %s_VERSION >= %#x\n", name, ver );
    free( name );
}

static void put_apicontract_guard_end( const expr_t *expr, int (*put_str)( FILE *, const char *, ... ), FILE *file )
{
    const type_t *type;
    char *name;
    int ver;
    if (!winrt_mode) return;
    type = expr->u.args[0]->u.decl->type;
    ver = expr->u.args[1]->u.lval;
    name = format_apicontract_macro( type );
    put_str( file, "#endif /* %s_VERSION >= %#x */\n", name, ver );
    free( name );
}

static void write_widl_using_method_macros( FILE *header, const type_t *iface, const type_t *top_iface )
{
    const statement_t *stmt;
    const char *name = top_iface->short_name ? top_iface->short_name : top_iface->name;

    if (type_iface_get_inherit( iface ))
        write_widl_using_method_macros( header, type_iface_get_inherit( iface ), top_iface );

    STATEMENTS_FOR_EACH_FUNC( stmt, type_iface_get_stmts( iface ) )
    {
        const var_t *func = stmt->u.var;
        const char *func_name;

        if (is_override_method( iface, top_iface, func )) continue;
        if (is_callas( func->attrs )) continue;

        func_name = get_name( func );
        fprintf( header, "#define %s_%s %s_%s\n", name, func_name, top_iface->c_name, func_name );
    }
}

static void write_widl_using_macros( FILE *header, type_t *iface )
{
    const struct uuid *uuid = get_attrp( iface->attrs, ATTR_UUID );
    const char *name = iface->short_name ? iface->short_name : iface->name;
    char *macro;

    if (!strcmp( iface->name, iface->c_name )) return;

    macro = format_namespace( iface->namespace, "WIDL_using_", "_", NULL, NULL );
    fprintf( header, "#ifdef %s\n", macro );

    if (uuid) fprintf( header, "#define IID_%s IID_%s\n", name, iface->c_name );
    if (iface->type_type == TYPE_INTERFACE)
        fprintf( header, "#define %sVtbl %sVtbl\n", name, iface->c_name );
    fprintf( header, "#define %s %s\n", name, iface->c_name );

    if (iface->type_type == TYPE_INTERFACE) write_widl_using_method_macros( header, iface, iface );

    fprintf( header, "#endif /* %s */\n", macro );
    free( macro );
}

static void write_guid(FILE *f, const char *guid_prefix, const char *name, const struct uuid *uuid)
{
  if (!uuid) return;
  fprintf(f, "DEFINE_GUID(%s_%s, 0x%08x, 0x%04x, 0x%04x, 0x%02x,0x%02x, 0x%02x,"
        "0x%02x,0x%02x,0x%02x,0x%02x,0x%02x);\n",
        guid_prefix, name, uuid->Data1, uuid->Data2, uuid->Data3, uuid->Data4[0],
        uuid->Data4[1], uuid->Data4[2], uuid->Data4[3], uuid->Data4[4], uuid->Data4[5],
        uuid->Data4[6], uuid->Data4[7]);
}

static void put_guid( const char *guid_prefix, const char *name, const struct uuid *uuid )
{
    if (!uuid) return;
    put_line( "DEFINE_GUID(%s_%s, 0x%08x, 0x%04x, 0x%04x, 0x%02x,0x%02x, 0x%02x,"
              "0x%02x,0x%02x,0x%02x,0x%02x,0x%02x);",
              guid_prefix, name, uuid->Data1, uuid->Data2, uuid->Data3, uuid->Data4[0],
              uuid->Data4[1], uuid->Data4[2], uuid->Data4[3], uuid->Data4[4], uuid->Data4[5],
              uuid->Data4[6], uuid->Data4[7] );
}

static void write_uuid_decl(FILE *f, type_t *type, const struct uuid *uuid)
{
  fprintf(f, "#ifdef __CRT_UUID_DECL\n");
  fprintf(f, "__CRT_UUID_DECL(%s, 0x%08x, 0x%04x, 0x%04x, 0x%02x,0x%02x, 0x%02x,"
        "0x%02x,0x%02x,0x%02x,0x%02x,0x%02x)\n",
        type->c_name, uuid->Data1, uuid->Data2, uuid->Data3, uuid->Data4[0], uuid->Data4[1],
        uuid->Data4[2], uuid->Data4[3], uuid->Data4[4], uuid->Data4[5], uuid->Data4[6],
        uuid->Data4[7]);
  fprintf(f, "#endif\n");
}

static void put_uuid_decl( type_t *type, const struct uuid *uuid )
{
    put_line( "#ifdef __CRT_UUID_DECL" );
    put_line( "__CRT_UUID_DECL(%s, 0x%08x, 0x%04x, 0x%04x, 0x%02x,0x%02x, 0x%02x,"
              "0x%02x,0x%02x,0x%02x,0x%02x,0x%02x)",
              type->c_name, uuid->Data1, uuid->Data2, uuid->Data3, uuid->Data4[0], uuid->Data4[1],
              uuid->Data4[2], uuid->Data4[3], uuid->Data4[4], uuid->Data4[5], uuid->Data4[6], uuid->Data4[7] );
    put_line( "#endif" );
}

static const char *uuid_string(const struct uuid *uuid)
{
  static char buf[37];

  sprintf(buf, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuid->Data1, uuid->Data2, uuid->Data3, uuid->Data4[0], uuid->Data4[1], uuid->Data4[2],
        uuid->Data4[3], uuid->Data4[4], uuid->Data4[5], uuid->Data4[6], uuid->Data4[7]);

  return buf;
}

static void put_namespace_start( struct namespace *namespace )
{
    if (!is_global_namespace( namespace ))
    {
        put_namespace_start( namespace->parent );
        put_line( "namespace %s {", namespace->name );
    }
    else if (use_abi_namespace) put_line( "namespace ABI {" );
}

static void put_namespace_end( struct namespace *namespace )
{
    if (!is_global_namespace( namespace ))
    {
        put_line( "}" );
        put_namespace_end( namespace->parent );
    }
    else if (use_abi_namespace) put_line( "}" );
}

static void write_namespace_start(FILE *header, struct namespace *namespace)
{
    if(is_global_namespace(namespace)) {
        if(use_abi_namespace)
            write_line(header, 1, "namespace ABI {");
        return;
    }

    write_namespace_start(header, namespace->parent);
    write_line(header, 1, "namespace %s {", namespace->name);
}

static void write_namespace_end(FILE *header, struct namespace *namespace)
{
    if(is_global_namespace(namespace)) {
        if(use_abi_namespace)
            write_line(header, -1, "}");
        return;
    }

    write_line(header, -1, "}");
    write_namespace_end(header, namespace->parent);
}

const char *get_name(const var_t *v)
{
    static char *buffer;
    free( buffer );
    if (is_attr( v->attrs, ATTR_EVENTADD ))
        return buffer = strmake( "add_%s", v->name );
    if (is_attr( v->attrs, ATTR_EVENTREMOVE ))
        return buffer = strmake( "remove_%s", v->name );
    if (is_attr( v->attrs, ATTR_PROPGET ))
        return buffer = strmake( "get_%s", v->name );
    if (is_attr( v->attrs, ATTR_PROPPUT ))
        return buffer = strmake( "put_%s", v->name );
    if (is_attr( v->attrs, ATTR_PROPPUTREF ))
        return buffer = strmake( "putref_%s", v->name );
    buffer = NULL;
    return v->name;
}

static void put_fields( var_list_t *fields, enum name_type name_type )
{
    unsigned nameless_struct_cnt = 0, nameless_struct_i = 0, nameless_union_cnt = 0, nameless_union_i = 0;
    const char *name;
    char buf[32];
    var_t *field;

    if (!fields) return;

    LIST_FOR_EACH_ENTRY( field, fields, var_t, entry )
    {
        if (!field->declspec.type) continue;

        switch (type_get_type_detect_alias( field->declspec.type ))
        {
        case TYPE_STRUCT:
        case TYPE_ENCAPSULATED_UNION: nameless_struct_cnt++; break;
        case TYPE_UNION: nameless_union_cnt++; break;
        default:;
        }
    }

    LIST_FOR_EACH_ENTRY( field, fields, var_t, entry )
    {
        expr_t *contract = get_attrp( field->attrs, ATTR_CONTRACT );
        if (!field->declspec.type) continue;
        if (contract) put_apicontract_guard_start( contract, put_str, file );

        indent( file, 0 );
        name = field->name;

        switch (type_get_type_detect_alias( field->declspec.type ))
        {
        case TYPE_STRUCT:
        case TYPE_ENCAPSULATED_UNION:
            if (!field->name)
            {
                put_str( file, "__C89_NAMELESS " );
                if (nameless_struct_cnt == 1)
                    name = "__C89_NAMELESSSTRUCTNAME";
                else if (nameless_struct_i < 5 /* # of supporting macros */)
                {
                    sprintf( buf, "__C89_NAMELESSSTRUCTNAME%d", ++nameless_struct_i );
                    name = buf;
                }
            }
            break;
        case TYPE_UNION:
            if (!field->name)
            {
                put_str( file, "__C89_NAMELESS " );
                if (nameless_union_cnt == 1)
                    name = "__C89_NAMELESSUNIONNAME";
                else if (nameless_union_i < 8 /* # of supporting macros */)
                {
                    sprintf( buf, "__C89_NAMELESSUNIONNAME%d", ++nameless_union_i );
                    name = buf;
                }
            }
            break;
        default: break;
        }
        put_declspec_full( &field->declspec, TRUE, field->declonly, name, name_type, put_str, file );
        put_str( file, ";\n" );
        if (contract) put_apicontract_guard_end( contract, put_str, file );
    }
}

static void put_enums( var_list_t *enums, const char *enum_name,
                       int (*put_str)( FILE *, const char *, ... ), FILE *file )
{
    var_t *value;
    if (!enums) return;
    LIST_FOR_EACH_ENTRY( value, enums, var_t, entry )
    {
        expr_t *contract = get_attrp( value->attrs, ATTR_CONTRACT );
        if (contract) put_apicontract_guard_start( contract, put_str, file );
        if (value->name)
        {
            indent( file, 0 );
            if (!enum_name) put_str( file, "%s", get_name( value ) );
            else put_str( file, "%s_%s", enum_name, get_name( value ) );
            if (value->eval)
            {
                put_str( file, " = " );
                put_expr( value->eval, 0, 1, NULL, NULL, "", put_str, file );
            }
        }
        if (list_next( enums, &value->entry )) put_str( file, ",\n" );
        else put_str( file, "\n" );
        if (contract) put_apicontract_guard_end( contract, put_str, file );
    }
}

int needs_space_after(type_t *t)
{
  return (type_is_alias(t) ||
          (!is_ptr(t) && (!is_array(t) || !type_array_is_decl_as_ptr(t) || t->name)));
}

static int decl_needs_parens(const type_t *t)
{
    if (type_is_alias(t))
        return FALSE;
    if (is_array(t) && !type_array_is_decl_as_ptr(t))
        return TRUE;
    return is_func(t);
}

static void put_pointer_left( type_t *ref )
{
    if (needs_space_after( ref )) put_str( " " );
    if (decl_needs_parens( ref )) put_str( "(" );
    if (type_get_type_detect_alias(ref) == TYPE_FUNCTION)
    {
        const char *callconv = get_attrp(ref->attrs, ATTR_CALLCONV);
        if (!callconv && is_object_interface) callconv = "STDMETHODCALLTYPE";
        if (callconv) put_str( "%s ", callconv );
    }
    put_str( "*" );
}

static void put_basic_type( type_t *type )
{
    if (type_basic_get_type( type ) != TYPE_BASIC_INT32 &&
        type_basic_get_type( type ) != TYPE_BASIC_INT64 &&
        type_basic_get_type( type ) != TYPE_BASIC_LONG &&
        type_basic_get_type( type ) != TYPE_BASIC_HYPER)
    {
        if (type_basic_get_sign( type ) < 0) put_str( file, "signed " );
        else if (type_basic_get_sign( type ) > 0) put_str( file, "unsigned " );
    }

    switch (type_basic_get_type( type ))
    {
    case TYPE_BASIC_INT8:           put_str( file, "small" ); break;
    case TYPE_BASIC_INT16:          put_str( file, "short" ); break;
    case TYPE_BASIC_INT3264:        put_str( file, "__int3264" ); break;
    case TYPE_BASIC_INT:            put_str( file, "int" ); break;
    case TYPE_BASIC_BYTE:           put_str( file, "byte" ); break;
    case TYPE_BASIC_CHAR:           put_str( file, "char" ); break;
    case TYPE_BASIC_WCHAR:          put_str( file, "wchar_t" ); break;
    case TYPE_BASIC_FLOAT:          put_str( file, "float" ); break;
    case TYPE_BASIC_DOUBLE:         put_str( file, "double" ); break;
    case TYPE_BASIC_HANDLE:         put_str( file, "handle_t" ); break;
    case TYPE_BASIC_ERROR_STATUS_T: put_str( file, "error_status_t" ); break;
    case TYPE_BASIC_INT32:
        if (type_basic_get_sign( type ) > 0) put_str( file, "UINT32" );
        else put_str( file, "INT32" );
        break;
    case TYPE_BASIC_LONG:
        if (type_basic_get_sign( type ) > 0) put_str( file, "ULONG" );
        else put_str( file, "LONG" );
        break;
    case TYPE_BASIC_INT64:
        if (type_basic_get_sign( type ) > 0) put_str( file, "UINT64" );
        else put_str( file, "INT64" );
        break;
    case TYPE_BASIC_HYPER:
        if (type_basic_get_sign( type ) > 0) put_str( file, "MIDL_uhyper" );
        else put_str( file, "hyper" );
        break;
    }
}

static void put_enum_type( type_t *type, enum name_type name_type )
{
    if (type->written)
    {
        const char *name = type_get_name( type, name_type );
        if (winrt_mode && name_type == NAME_DEFAULT && name) put_str( "%s", name );
        else put_str( "enum %s", name ? name : "" );
    }
    else
    {
        const char *decl_name = type_get_decl_name( type, name_type );
        var_list_t *values = type_enum_get_values( type );

        assert( type->defined );
        type->written = TRUE;

        put_str( "enum " );
        if (decl_name) put_str( "%s ", decl_name );
        put_line( "{" );
        put_enums( values, is_global_namespace( type->namespace ) ? NULL : type->name );
        put_str( "}" );
    }
}

static void put_struct_type( type_t *type, enum name_type name_type )
{
    if (type->written)
    {
        const char *name = type_get_name( type, name_type );
        if (winrt_mode && name_type == NAME_DEFAULT && name) put_str( "%s", name );
        else put_str( "struct %s", name ? name : "" );
    }
    else
    {
        const char *decl_name = type_get_decl_name( type, name_type );
        var_list_t *fields;

        if (type_get_type( type ) == TYPE_STRUCT) fields = type_struct_get_fields( type );
        else fields = type_encapsulated_union_get_fields( type );

        assert( type->defined );
        type->written = TRUE;

        put_str( "struct " );
        if (decl_name) put_str( "%s ", decl_name );
        put_line( "{" );
        put_fields( fields, name_type );
        put_str( "}" );
    }
}

static void put_union_type( type_t *type, enum name_type name_type )
{
    if (type->written)
    {
        const char *name = type_get_name( type, name_type );
        if (winrt_mode && name_type == NAME_DEFAULT && name) put_str( "%s", name );
        else put_str( "union %s", name ? name : "" );
    }
    else
    {
        const char *decl_name = type_get_decl_name( type, name_type );

        assert( type->defined );
        type->written = TRUE;

        put_str( "union " );
        if (decl_name) put_str( "%s ", decl_name );
        put_line( "{" );
        put_fields( type_union_get_cases( type ), name_type );
        put_str( "}" );
    }
}

static void put_type_left( const decl_spec_t *declspec, enum name_type name_type, int declonly, int write_callconv )
{
    const int is_inline = declspec->func_specifier & FUNCTION_SPECIFIER_INLINE;
    const int is_const = declspec->qualifier & TYPE_QUALIFIER_CONST;
    type_t *t = declspec->type;
    const char *decl_name;
    char *args;

    if (!file) return;

    decl_name = type_get_decl_name( t, name_type );

    if (ds->func_specifier & FUNCTION_SPECIFIER_INLINE)
        put_str( file, "inline " );

    if ((ds->qualifier & TYPE_QUALIFIER_CONST) && (type_is_alias( t ) || !is_ptr( t )))
        put_str( file, "const " );

    switch (type_get_type_detect_alias( t ))
    {
    case TYPE_ENUM:
        if (declonly) put_str( file, "enum %s", decl_name ? decl_name : "" );
        else put_enum_type( t, name_type, put_str, file );
        break;
    case TYPE_STRUCT:
    case TYPE_ENCAPSULATED_UNION:
        if (declonly) put_str( file, "struct %s", decl_name ? decl_name : "" );
        else put_struct_type( t, name_type, put_str, file );
        break;
    case TYPE_UNION:
        if (declonly) put_str( file, "union %s", decl_name ? decl_name : "" );
        else put_union_type( t, name_type, put_str, file );
        break;
    case TYPE_POINTER:
    {
        put_type_left( type_pointer_get_ref( t ), name_type, declonly, FALSE, put_str, file );
        put_pointer_left( type_pointer_get_ref_type( t ), put_str, file );
        if (ds->qualifier & TYPE_QUALIFIER_CONST) put_str( file, "const " );
        break;
    }
    case TYPE_ARRAY:
        if (t->name && type_array_is_decl_as_ptr( t )) put_str( file, "%s", t->name );
        else
        {
            put_type_left( type_array_get_element( t ), name_type, declonly, !type_array_is_decl_as_ptr( t ), put_str, file );
            if (type_array_is_decl_as_ptr( t )) put_pointer_left( type_array_get_element_type( t ), put_str, file );
        }
        break;
    case TYPE_FUNCTION:
        put_type_left( type_function_get_ret( t ), name_type, declonly, TRUE, put_str, file );

        /* A pointer to a function has to write the calling convention inside
         * the parentheses. There's no way to handle that here, so we have to
         * use an extra parameter to tell us whether to write the calling
         * convention or not. */
        if (write_callconv)
        {
            const char *callconv = get_attrp( t->attrs, ATTR_CALLCONV );
            if (!callconv && is_object_interface) callconv = "STDMETHODCALLTYPE";
            if (callconv) put_str( file, " %s ", callconv );
        }
        break;
    case TYPE_BASIC: put_basic_type( t, put_str, file ); break;
    case TYPE_INTERFACE:
    case TYPE_MODULE:
    case TYPE_COCLASS:
        put_str( file, "%s", type_get_name( t, name_type ) );
        break;
    case TYPE_RUNTIMECLASS:
        put_str( file, "%s", type_get_name( type_runtimeclass_get_default_iface( t, TRUE ), name_type ) );
        break;
    case TYPE_DELEGATE:
        put_str( file, "%s", type_get_name( type_delegate_get_iface( t ), name_type ) );
        break;
    case TYPE_VOID: put_str( file, "void" ); break;
    case TYPE_BITFIELD:
    {
        const decl_spec_t ds = {.type = type_bitfield_get_field( t )};
        put_type_left( &ds, name_type, declonly, TRUE, put_str, file );
        break;
    }
    case TYPE_ALIAS:
    {
        const decl_spec_t *ds = type_alias_get_aliasee( t );
        int in_namespace = ds && ds->type && ds->type->namespace && !is_global_namespace( ds->type->namespace );
        if (!in_namespace) put_str( file, "%s", t->name );
        else put_type_left( ds, name_type, declonly, write_callconv, put_str, file );
        break;
    }
    case TYPE_PARAMETERIZED_TYPE:
    {
        type_t *iface = type_parameterized_type_get_real_type( t );
        if (type_get_type( iface ) == TYPE_DELEGATE) iface = type_delegate_get_iface( iface );
        args = format_parameterized_type_args( t, "", "_logical" );
        put_str( file, "%s<%s>", iface->name, args );
        free( args );
        break;
    }
    case TYPE_PARAMETER:
        put_str( file, "%s_abi", t->name );
        break;
    case TYPE_APICONTRACT:
        /* shouldn't be here */
        assert( 0 );
        break;
    }
}

static void put_type_right( type_t *type, int is_field, int (*put_str)( FILE *, const char *, ... ), FILE *file )
{
    if (!file) return;
    if (type_is_alias( type )) return;

    switch (type_get_type( type ))
    {
    case TYPE_ARRAY:
    {
        type_t *elem = type_array_get_element_type( type );
        if (type_array_is_decl_as_ptr( type ))
        {
            if (decl_needs_parens( elem )) put_str( file, ")" );
        }
        else
        {
            if (is_conformant_array( type )) put_str( file, "[%s]", is_field ? "1" : "" );
            else put_str( file, "[%u]", type_array_get_dim( type ) );
        }
        put_type_right( elem, FALSE, put_str, file );
        break;
    }
    case TYPE_FUNCTION:
    {
        const var_list_t *args = type_function_get_args( type );
        put_str( file, "(" );
        if (args) put_args( args, NULL, 0, FALSE, NAME_DEFAULT, put_str, file );
        else put_str( file, "void" );
        put_str( file, ")" );
        put_type_right( type_function_get_rettype( type ), FALSE, put_str, file );
        break;
    }
    case TYPE_POINTER:
    {
        type_t *ref = type_pointer_get_ref_type( type );
        if (decl_needs_parens( ref )) put_str( file, ")" );
        put_type_right( ref, FALSE, put_str, file );
        break;
    }
    case TYPE_BITFIELD:
        put_str( file, " : %u", type_bitfield_get_bits( type )->cval );
        break;
    case TYPE_VOID:
    case TYPE_BASIC:
    case TYPE_ENUM:
    case TYPE_STRUCT:
    case TYPE_ENCAPSULATED_UNION:
    case TYPE_UNION:
    case TYPE_ALIAS:
    case TYPE_MODULE:
    case TYPE_COCLASS:
    case TYPE_INTERFACE:
    case TYPE_RUNTIMECLASS:
    case TYPE_DELEGATE:
    case TYPE_PARAMETERIZED_TYPE:
    case TYPE_PARAMETER: break;
    case TYPE_APICONTRACT:
        /* not supposed to be here */
        assert( 0 );
        break;
    }
}

static void put_declspec_full( const decl_spec_t *ds, int is_field, int declonly, const char *name,
                               enum name_type name_type, int (*put_str)( FILE *, const char *, ... ), FILE *file )
{
    type_t *t = ds->type;

    if (t) put_type_left( ds, name_type, declonly, TRUE, put_str, file );
    if (name) put_str( file, "%s%s", !t || needs_space_after( t ) ? " " : "", name );
    if (t) put_type_right( t, is_field, put_str, file );
}

static void write_type_definition(FILE *f, type_t *t, int declonly)
{
    int in_namespace = t->namespace && !is_global_namespace(t->namespace);
    int save_written = t->written;
    decl_spec_t ds = {.type = t};
    expr_t *contract = get_attrp(t->attrs, ATTR_CONTRACT);

    if (contract) put_apicontract_guard_start( contract, fprintf, f );
    if(in_namespace) {
        fprintf(f, "#ifdef __cplusplus\n");
        fprintf(f, "} /* extern \"C\" */\n");
        write_namespace_start(f, t->namespace);
    }
    indent(f, 0);
    put_type_left( &ds, NAME_DEFAULT, declonly, TRUE, fprintf, f );
    fprintf(f, ";\n");
    if(in_namespace) {
        t->written = save_written;
        write_namespace_end(f, t->namespace);
        fprintf(f, "extern \"C\" {\n");
        fprintf(f, "#else\n");
        put_type_left( &ds, NAME_C, declonly, TRUE, fprintf, f );
        fprintf(f, ";\n");
        if (winrt_mode) write_widl_using_macros(f, t);
        fprintf(f, "#endif\n\n");
    }
    if (contract) put_apicontract_guard_end( contract, fprintf, f );
}

static void write_declspec_full( FILE *h, const decl_spec_t *ds, int is_field, int declonly,
                                 const char *name, enum name_type name_type )
{
    if (!h) return;
    put_declspec_full( ds, is_field, declonly, name, NAME_DEFAULT, fprintf, h );
}

void write_declspec( FILE *h, const decl_spec_t *ds, const char *name )
{
    write_declspec_full( h, ds, FALSE, TRUE, name, NAME_DEFAULT );
}

void put_declspec( const decl_spec_t *ds, const char *name )
{
    put_declspec_full( ds, FALSE, TRUE, name, NAME_DEFAULT );
}

void write_expr( FILE *out, const expr_t *expr, int brackets, int toplevel, const char *toplevel_prefix,
                 const type_t *cont_type, const char *local_var_prefix )
{
    switch (expr->type)
    {
    case EXPR_VOID: break;
    case EXPR_NUM: fprintf( out, "%u", expr->u.lval ); break;
    case EXPR_HEXNUM: fprintf( out, "0x%x", expr->u.lval ); break;
    case EXPR_DOUBLE: fprintf( out, "%#.15g", expr->u.dval ); break;
    case EXPR_TRUEFALSE:
        if (!expr->u.lval) fprintf( out, "FALSE" );
        else fprintf( out, "TRUE" );
        break;
    case EXPR_IDENTIFIER:
        if (toplevel && toplevel_prefix && cont_type)
        {
            int found_in_cont_type;
            expr_find_identifier( expr->u.sval, cont_type, &found_in_cont_type );
            if (found_in_cont_type)
            {
                fprintf( out, "%s%s", toplevel_prefix, expr->u.sval );
                break;
            }
        }
        fprintf( out, "%s%s", local_var_prefix, expr->u.sval );
        break;
    case EXPR_STRLIT: fprintf( out, "\"%s\"", expr->u.sval ); break;
    case EXPR_WSTRLIT: fprintf( out, "L\"%s\"", expr->u.sval ); break;
    case EXPR_CHARCONST: fprintf( out, "'%s'", expr->u.sval ); break;
    case EXPR_LOGNOT:
        fprintf( out, "!" );
        write_expr( out, expr->ref, 1, toplevel, toplevel_prefix, cont_type, local_var_prefix );
        break;
    case EXPR_NOT:
        fprintf( out, "~" );
        write_expr( out, expr->ref, 1, toplevel, toplevel_prefix, cont_type, local_var_prefix );
        break;
    case EXPR_POS:
        fprintf( out, "+" );
        write_expr( out, expr->ref, 1, toplevel, toplevel_prefix, cont_type, local_var_prefix );
        break;
    case EXPR_NEG:
        fprintf( out, "-" );
        write_expr( out, expr->ref, 1, toplevel, toplevel_prefix, cont_type, local_var_prefix );
        break;
    case EXPR_ADDRESSOF:
        fprintf( out, "&" );
        write_expr( out, expr->ref, 1, toplevel, toplevel_prefix, cont_type, local_var_prefix );
        break;
    case EXPR_PPTR:
        fprintf( out, "*" );
        write_expr( out, expr->ref, 1, toplevel, toplevel_prefix, cont_type, local_var_prefix );
        break;
    case EXPR_CAST:
        fprintf( out, "(" );
        write_type_decl( out, &expr->u.tref, NULL );
        fprintf( out, ")" );
        write_expr( out, expr->ref, 1, toplevel, toplevel_prefix, cont_type, local_var_prefix );
        break;
    case EXPR_SIZEOF:
        fprintf( out, "sizeof(" );
        write_type_decl( out, &expr->u.tref, NULL );
        fprintf( out, ")" );
        break;
    case EXPR_SHL:
    case EXPR_SHR:
    case EXPR_MOD:
    case EXPR_MUL:
    case EXPR_DIV:
    case EXPR_ADD:
    case EXPR_SUB:
    case EXPR_AND:
    case EXPR_OR:
    case EXPR_LOGOR:
    case EXPR_LOGAND:
    case EXPR_XOR:
    case EXPR_EQUALITY:
    case EXPR_INEQUALITY:
    case EXPR_GTR:
    case EXPR_LESS:
    case EXPR_GTREQL:
    case EXPR_LESSEQL:
        if (brackets) fprintf( out, "(" );
        write_expr( out, expr->ref, 1, toplevel, toplevel_prefix, cont_type, local_var_prefix );
        switch (expr->type)
        {
        case EXPR_SHL: fprintf( out, " << " ); break;
        case EXPR_SHR: fprintf( out, " >> " ); break;
        case EXPR_MOD: fprintf( out, " %% " ); break;
        case EXPR_MUL: fprintf( out, " * " ); break;
        case EXPR_DIV: fprintf( out, " / " ); break;
        case EXPR_ADD: fprintf( out, " + " ); break;
        case EXPR_SUB: fprintf( out, " - " ); break;
        case EXPR_AND: fprintf( out, " & " ); break;
        case EXPR_OR: fprintf( out, " | " ); break;
        case EXPR_LOGOR: fprintf( out, " || " ); break;
        case EXPR_LOGAND: fprintf( out, " && " ); break;
        case EXPR_XOR: fprintf( out, " ^ " ); break;
        case EXPR_EQUALITY: fprintf( out, " == " ); break;
        case EXPR_INEQUALITY: fprintf( out, " != " ); break;
        case EXPR_GTR: fprintf( out, " > " ); break;
        case EXPR_LESS: fprintf( out, " < " ); break;
        case EXPR_GTREQL: fprintf( out, " >= " ); break;
        case EXPR_LESSEQL: fprintf( out, " <= " ); break;
        default: break;
        }
        write_expr( out, expr->u.ext, 1, toplevel, toplevel_prefix, cont_type, local_var_prefix );
        if (brackets) fprintf( out, ")" );
        break;
    case EXPR_MEMBER:
        if (brackets) fprintf( out, "(" );
        if (expr->ref->type == EXPR_PPTR)
        {
            write_expr( out, expr->ref->ref, 1, toplevel, toplevel_prefix, cont_type, local_var_prefix );
            fprintf( out, "->" );
        }
        else
        {
            write_expr( out, expr->ref, 1, toplevel, toplevel_prefix, cont_type, local_var_prefix );
            fprintf( out, "." );
        }
        write_expr( out, expr->u.ext, 1, 0, toplevel_prefix, cont_type, "" );
        if (brackets) fprintf( out, ")" );
        break;
    case EXPR_COND:
        if (brackets) fprintf( out, "(" );
        write_expr( out, expr->ref, 1, toplevel, toplevel_prefix, cont_type, local_var_prefix );
        fprintf( out, " ? " );
        write_expr( out, expr->u.ext, 1, toplevel, toplevel_prefix, cont_type, local_var_prefix );
        fprintf( out, " : " );
        write_expr( out, expr->ext2, 1, toplevel, toplevel_prefix, cont_type, local_var_prefix );
        if (brackets) fprintf( out, ")" );
        break;
    case EXPR_ARRAY:
        if (brackets) fprintf( out, "(" );
        write_expr( out, expr->ref, 1, toplevel, toplevel_prefix, cont_type, local_var_prefix );
        fprintf( out, "[" );
        write_expr( out, expr->u.ext, 1, 1, toplevel_prefix, cont_type, local_var_prefix );
        fprintf( out, "]" );
        if (brackets) fprintf( out, ")" );
        break;
    }
}

static int user_type_registered(const char *name)
{
  user_type_t *ut;
  LIST_FOR_EACH_ENTRY(ut, &user_type_list, user_type_t, entry)
    if (!strcmp(name, ut->name))
      return 1;
  return 0;
}

static int context_handle_registered(const char *name)
{
  context_handle_t *ch;
  LIST_FOR_EACH_ENTRY(ch, &context_handle_list, context_handle_t, entry)
    if (!strcmp(name, ch->name))
      return 1;
  return 0;
}

static int generic_handle_registered(const char *name)
{
  generic_handle_t *gh;
  LIST_FOR_EACH_ENTRY(gh, &generic_handle_list, generic_handle_t, entry)
    if (!strcmp(name, gh->name))
      return 1;
  return 0;
}

unsigned int get_context_handle_offset( const type_t *type )
{
    context_handle_t *ch;
    unsigned int index = 0;

    while (!is_attr( type->attrs, ATTR_CONTEXTHANDLE ))
    {
        if (type_is_alias( type )) type = type_alias_get_aliasee_type( type );
        else if (is_ptr( type )) type = type_pointer_get_ref_type( type );
        else error( "internal error: %s is not a context handle\n", type->name );
    }
    LIST_FOR_EACH_ENTRY( ch, &context_handle_list, context_handle_t, entry )
    {
        if (!strcmp( type->name, ch->name )) return index;
        index++;
    }
    error( "internal error: %s is not registered as a context handle\n", type->name );
    return index;
}

unsigned int get_generic_handle_offset( const type_t *type )
{
    generic_handle_t *gh;
    unsigned int index = 0;

    while (!is_attr( type->attrs, ATTR_HANDLE ))
    {
        if (type_is_alias( type )) type = type_alias_get_aliasee_type( type );
        else if (is_ptr( type )) type = type_pointer_get_ref_type( type );
        else error( "internal error: %s is not a generic handle\n", type->name );
    }
    LIST_FOR_EACH_ENTRY( gh, &generic_handle_list, generic_handle_t, entry )
    {
        if (!strcmp( type->name, gh->name )) return index;
        index++;
    }
    error( "internal error: %s is not registered as a generic handle\n", type->name );
    return index;
}

/* check for types which require additional prototypes to be generated in the
 * header */
void check_for_additional_prototype_types(type_t *type)
{
  if (!type) return;
  for (;;) {
    const char *name = type->name;
    if (type->user_types_registered) break;
    type->user_types_registered = 1;
    if (is_attr(type->attrs, ATTR_CONTEXTHANDLE)) {
      if (!context_handle_registered(name))
      {
        context_handle_t *ch = xmalloc(sizeof(*ch));
        ch->name = xstrdup(name);
        list_add_tail(&context_handle_list, &ch->entry);
      }
      /* don't carry on parsing fields within this type */
      break;
    }
    if ((type_get_type(type) != TYPE_BASIC ||
         type_basic_get_type(type) != TYPE_BASIC_HANDLE) &&
        is_attr(type->attrs, ATTR_HANDLE)) {
      if (!generic_handle_registered(name))
      {
        generic_handle_t *gh = xmalloc(sizeof(*gh));
        gh->name = xstrdup(name);
        list_add_tail(&generic_handle_list, &gh->entry);
      }
      /* don't carry on parsing fields within this type */
      break;
    }
    if (is_attr(type->attrs, ATTR_WIREMARSHAL)) {
      if (!user_type_registered(name))
      {
        user_type_t *ut = xmalloc(sizeof *ut);
        ut->name = xstrdup(name);
        list_add_tail(&user_type_list, &ut->entry);
      }
      /* don't carry on parsing fields within this type as we are already
       * using a wire marshaled type */
      break;
    }
    else if (type_is_defined(type))
    {
      var_list_t *vars;
      const var_t *v;
      switch (type_get_type_detect_alias(type))
      {
      case TYPE_ENUM:
        vars = type_enum_get_values(type);
        break;
      case TYPE_STRUCT:
        vars = type_struct_get_fields(type);
        break;
      case TYPE_UNION:
        vars = type_union_get_cases(type);
        break;
      default:
        vars = NULL;
        break;
      }
      if (vars) LIST_FOR_EACH_ENTRY( v, vars, const var_t, entry )
        check_for_additional_prototype_types(v->declspec.type);
    }

    if (type_is_alias(type))
      type = type_alias_get_aliasee_type(type);
    else if (is_ptr(type))
      type = type_pointer_get_ref_type(type);
    else if (is_array(type))
      type = type_array_get_element_type(type);
    else
      break;
  }
}

static int put_serialize_function_decl( FILE *header, const type_t *type )
{
    put_serialize_functions( type, NULL );
    return 1;
}

static int serializable_exists(FILE *header, const type_t *type)
{
    return 0;
}

static int for_each_serializable(const statement_list_t *stmts, FILE *header,
                                 int (*proc)(FILE*, const type_t*))
{
    statement_t *stmt, *iface_stmt;
    statement_list_t *iface_stmts;
    typeref_t *ref;

    if (stmts) LIST_FOR_EACH_ENTRY( stmt, stmts, statement_t, entry )
    {
        if (stmt->type != STMT_TYPE || type_get_type(stmt->u.type) != TYPE_INTERFACE)
            continue;

        iface_stmts = type_iface_get_stmts(stmt->u.type);
        if (iface_stmts) LIST_FOR_EACH_ENTRY( iface_stmt, iface_stmts, statement_t, entry )
        {
            if (iface_stmt->type != STMT_TYPEDEF) continue;
            if (iface_stmt->u.type_list) LIST_FOR_EACH_ENTRY(ref, iface_stmt->u.type_list, typeref_t, entry)
            {
                if (!is_attr(ref->type->attrs, ATTR_ENCODE)
                    && !is_attr(ref->type->attrs, ATTR_DECODE))
                    continue;
                if (!proc(header, ref->type))
                    return 0;
            }
        }
    }

    return 1;
}

static void put_user_types(void)
{
    const user_type_t *type;
    LIST_FOR_EACH_ENTRY( type, &user_type_list, user_type_t, entry )
    {
        const char *name = type->name;
        put_line( "ULONG           __RPC_USER %s_UserSize     (ULONG *, ULONG, %s *);", name, name );
        put_line( "unsigned char * __RPC_USER %s_UserMarshal  (ULONG *, unsigned char *, %s *);", name, name );
        put_line( "unsigned char * __RPC_USER %s_UserUnmarshal(ULONG *, unsigned char *, %s *);", name, name );
        put_line( "void            __RPC_USER %s_UserFree     (ULONG *, %s *);", name, name );
    }
}

static void put_context_handle_rundowns(void)
{
    const context_handle_t *handle;
    LIST_FOR_EACH_ENTRY( handle, &context_handle_list, context_handle_t, entry )
    {
        const char *name = handle->name;
        put_line( "void __RPC_USER %s_rundown(%s);", name, name );
    }
}

static void put_generic_handle_routines(void)
{
    const generic_handle_t *handle;
    LIST_FOR_EACH_ENTRY( handle, &generic_handle_list, generic_handle_t, entry )
    {
        const char *name = handle->name;
        put_line( "handle_t __RPC_USER %s_bind(%s);", name, name );
        put_line( "void __RPC_USER %s_unbind(%s, handle_t);", name, name );
    }
}

static void write_typedef(FILE *header, type_t *type, int declonly)
{
    type_t *t = type_alias_get_aliasee_type(type);
    if (winrt_mode && t->namespace && !is_global_namespace(t->namespace))
    {
        fprintf(header, "#ifndef __cplusplus\n");
        fprintf(header, "typedef ");
        write_declspec_full( header, type_alias_get_aliasee( type ), FALSE, declonly, type->c_name, NAME_C );
        fprintf(header, ";\n");
        if (type_get_type_detect_alias(t) != TYPE_ENUM)
        {
            fprintf(header, "#else /* __cplusplus */\n");
            write_namespace_start(header, t->namespace);
            indent(header, 0);
            fprintf(header, "typedef ");
            write_declspec_full( header, type_alias_get_aliasee( type ), FALSE, TRUE, type->name, NAME_DEFAULT );
            fprintf(header, ";\n");
            write_namespace_end(header, t->namespace);
        }
        fprintf(header, "#endif /* __cplusplus */\n\n");
    }
    else
    {
        fprintf(header, "typedef ");
        write_declspec_full( header, type_alias_get_aliasee( type ), FALSE, declonly, type->name, NAME_DEFAULT );
        fprintf(header, ";\n");
    }
}

int is_const_decl(const var_t *var)
{
  const decl_spec_t *t;
  /* strangely, MIDL accepts a const attribute on any pointer in the
  * declaration to mean that data isn't being instantiated. this appears
  * to be a bug, but there is no benefit to being incompatible with MIDL,
  * so we'll do the same thing */
  for (t = &var->declspec; ; )
  {
    if (t->qualifier & TYPE_QUALIFIER_CONST)
      return TRUE;
    else if (is_ptr(t->type))
      t = type_pointer_get_ref(t->type);
    else break;
  }
  return FALSE;
}

static void write_declaration(FILE *header, const var_t *v)
{
  if (is_const_decl(v) && v->eval)
  {
    fprintf(header, "#define %s (", v->name);
    write_expr(header, v->eval, 0, 1, NULL, NULL, "");
    fprintf(header, ")\n\n");
  }
  else
  {
    switch (v->declspec.stgclass)
    {
      case STG_NONE:
      case STG_REGISTER: /* ignored */
        break;
      case STG_STATIC:
        fprintf(header, "static ");
        break;
      case STG_EXTERN:
        fprintf(header, "extern ");
        break;
    }
    write_declspec_full( header, &v->declspec, FALSE, v->declonly, v->name, NAME_DEFAULT );
    fprintf(header, ";\n\n");
  }
}

static void write_library(FILE *header, const typelib_t *typelib)
{
  const struct uuid *uuid = get_attrp(typelib->attrs, ATTR_UUID);
  fprintf(header, "\n");
  write_guid(header, "LIBID", typelib->name, uuid);
  fprintf(header, "\n");
}


const type_t* get_explicit_generic_handle_type(const var_t* var)
{
    const type_t *t;
    for (t = var->declspec.type;
         is_ptr(t) || type_is_alias(t);
         t = type_is_alias(t) ? type_alias_get_aliasee_type(t) : type_pointer_get_ref_type(t))
        if ((type_get_type_detect_alias(t) != TYPE_BASIC || type_basic_get_type(t) != TYPE_BASIC_HANDLE) &&
            is_attr(t->attrs, ATTR_HANDLE))
            return t;
    return NULL;
}

const var_t *get_func_handle_var( const type_t *iface, const var_t *func,
                                  unsigned char *explicit_fc, unsigned char *implicit_fc )
{
    const var_t *var;
    const var_list_t *args = type_function_get_args( func->declspec.type );

    *explicit_fc = *implicit_fc = 0;
    if (args) LIST_FOR_EACH_ENTRY( var, args, const var_t, entry )
    {
        if (!is_attr( var->attrs, ATTR_IN ) && is_attr( var->attrs, ATTR_OUT )) continue;
        if (type_get_type( var->declspec.type ) == TYPE_BASIC && type_basic_get_type( var->declspec.type ) == TYPE_BASIC_HANDLE)
        {
            *explicit_fc = FC_BIND_PRIMITIVE;
            return var;
        }
        if (get_explicit_generic_handle_type( var ))
        {
            *explicit_fc = FC_BIND_GENERIC;
            return var;
        }
        if (is_context_handle( var->declspec.type ))
        {
            *explicit_fc = FC_BIND_CONTEXT;
            return var;
        }
    }

    if ((var = get_attrp( iface->attrs, ATTR_IMPLICIT_HANDLE )))
    {
        if (type_get_type( var->declspec.type ) == TYPE_BASIC &&
            type_basic_get_type( var->declspec.type ) == TYPE_BASIC_HANDLE)
            *implicit_fc = FC_BIND_PRIMITIVE;
        else
            *implicit_fc = FC_BIND_GENERIC;
        return var;
    }

    *implicit_fc = FC_AUTO_HANDLE;
    return NULL;
}

int has_out_arg_or_return(const var_t *func)
{
    const var_t *var;

    if (!is_void(type_function_get_rettype(func->declspec.type)))
        return 1;

    if (!type_function_get_args(func->declspec.type))
        return 0;

    LIST_FOR_EACH_ENTRY( var, type_function_get_args(func->declspec.type), const var_t, entry )
        if (is_attr(var->attrs, ATTR_OUT))
            return 1;

    return 0;
}


/********** INTERFACES **********/

int is_object(const type_t *iface)
{
    const attr_t *attr;
    if (type_is_defined(iface) && (type_get_type(iface) == TYPE_DELEGATE || type_iface_get_inherit(iface)))
        return 1;
    if (iface->attrs) LIST_FOR_EACH_ENTRY( attr, iface->attrs, const attr_t, entry )
        if (attr->type == ATTR_OBJECT || attr->type == ATTR_ODL) return 1;
    return 0;
}

int is_local(const attr_list_t *a)
{
  return is_attr(a, ATTR_LOCAL);
}

const var_t *is_callas(const attr_list_t *a)
{
  return get_attrp(a, ATTR_CALLAS);
}

static int is_inherited_method(const type_t *iface, const var_t *func)
{
  while ((iface = type_iface_get_inherit(iface)))
  {
    const statement_t *stmt;
    STATEMENTS_FOR_EACH_FUNC(stmt, type_iface_get_stmts(iface))
    {
      const var_t *funccmp = stmt->u.var;

      if (!is_callas(func->attrs))
      {
         char inherit_name[256];
         /* compare full name including property prefix */
         strcpy(inherit_name, get_name(funccmp));
         if (!strcmp(inherit_name, get_name(func))) return 1;
      }
    }
  }

  return 0;
}

static int is_aggregate_return(const var_t *func)
{
  enum type_type type = type_get_type(type_function_get_rettype(func->declspec.type));
  return type == TYPE_STRUCT || type == TYPE_UNION ||
         type == TYPE_COCLASS || type == TYPE_INTERFACE ||
         type == TYPE_RUNTIMECLASS;
}

static char *get_vtbl_entry_name(const type_t *iface, const var_t *func)
{
  static char buff[255];
  if (is_inherited_method(iface, func))
    sprintf(buff, "%s_%s", iface->name, get_name(func));
  else
    sprintf(buff, "%s", get_name(func));
  return buff;
}

static void put_method_macro(FILE *header, const type_t *iface, const type_t *child, const char *name)
{
    const statement_t *stmt;
    int first_iface = 1;

    if (type_iface_get_inherit( iface )) put_method_macro( header, type_iface_get_inherit( iface ), child, name );

    STATEMENTS_FOR_EACH_FUNC( stmt, type_iface_get_stmts( iface ) )
    {
        const var_t *arg, *func = stmt->u.var;
        var_list_t *args;

        if (first_iface)
        {
            put_line( "/*** %s methods ***/", iface->name );
            first_iface = 0;
        }

        if (is_override_method( iface, child, func )) continue;
        if (is_callas( func->attrs )) continue;

        put_str( "#define %s_%s(This", name, get_name( func ) );
        if ((args = type_function_get_args( func->declspec.type )))
            LIST_FOR_EACH_ENTRY( arg, args, const var_t, entry )
                put_str( ",%s", arg->name );
        put_str( ") " );

        if (is_aggregate_return( func ))
        {
            put_line( "%s_%s_define_WIDL_C_INLINE_WRAPPERS_for_aggregate_return_support",
                      name, get_name( func ) );
            continue;
        }

        put_str( "(This)->lpVtbl->%s(This", get_vtbl_entry_name( iface, func ) );
        if ((args = type_function_get_args( func->declspec.type )))
            LIST_FOR_EACH_ENTRY( arg, args, const var_t, entry )
                put_str( ",%s", arg->name );
        put_line( ")" );
    }
}

static void put_args( const var_list_t *args, const char *name, int method, int do_indent,
                      enum name_type name_type, int (*put_str)( FILE *, const char *, ... ), FILE *file )
{
    const var_t *arg;
    int count = 0;

    if (do_indent)
    {
        indentation++;
        indent( file, 0 );
    }
    if (method == 1)
    {
        put_str( file, "%s* This", name );
        count++;
    }
    if (args) LIST_FOR_EACH_ENTRY( arg, args, const var_t, entry )
    {
        if (count)
        {
            if (do_indent)
            {
                put_str( file, ",\n" );
                indent( file, 0 );
            }
            else put_str( file, "," );
        }
        /* In theory we should be writing the definition using write_declspec_full(...,
         * arg->declonly), but that causes redefinition in e.g. proxy files. In fact MIDL
         * disallows defining UDTs inside of an argument list. */
        put_declspec_full( &arg->declspec, FALSE, TRUE, arg->name, name_type, put_str, file );
        if (method == 2)
        {
            const expr_t *expr = get_attrp( arg->attrs, ATTR_DEFAULTVALUE );
            if (expr)
            {
                const var_t *tail_arg;

                /* Output default value only if all following arguments also have default value. */
                LIST_FOR_EACH_ENTRY_REV( tail_arg, args, const var_t, entry )
                {
                    if (tail_arg == arg)
                    {
                        expr_t bstr;

                        /* Fixup the expression type for a BSTR like midl does. */
                        if (get_type_vt( arg->declspec.type ) == VT_BSTR && expr->type == EXPR_STRLIT)
                        {
                            bstr = *expr;
                            bstr.type = EXPR_WSTRLIT;
                            expr = &bstr;
                        }

                        put_str( file, " = " );
                        put_expr( expr, 0, 1, NULL, NULL, "", put_str, file );
                        break;
                    }
                    if (!get_attrp( tail_arg->attrs, ATTR_DEFAULTVALUE )) break;
                }
            }
        }
        count++;
    }
    if (do_indent) indentation--;
}

void write_args( FILE *file, const var_list_t *args, const char *name, int method, int do_indent, enum name_type name_type )
{
    put_args( args, name, method, do_indent, name_type, fprintf, file );
}

void write_args( FILE *h, const var_list_t *arg, const char *name, int obj, int do_indent, enum name_type name_type )
{
    init_output_buffer();
    put_args( arg, name, obj, do_indent, name_type );
    fputs( (char *)output_buffer, h );
    free( output_buffer );
}

static void put_cpp_method_def( FILE *header, const type_t *iface )
{
    const statement_t *stmt;

    STATEMENTS_FOR_EACH_FUNC( stmt, type_iface_get_stmts( iface ) )
    {
        const var_t *func = stmt->u.var;
        const decl_spec_t *ret = type_function_get_ret( func->declspec.type );
        const char *callconv = get_attrp( func->declspec.type->attrs, ATTR_CALLCONV );
        const var_list_t *args = type_function_get_args( func->declspec.type );
        const var_t *arg;

        if (is_callas( func->attrs )) continue;
        if (!callconv) callconv = "STDMETHODCALLTYPE";

        if (is_aggregate_return( func ))
        {
            put_line( "#ifdef WIDL_EXPLICIT_AGGREGATE_RETURNS" );

            indent( header, 0 );
            put_str( "virtual " );
            put_declspec( header, ret, NULL );
            put_line( "* %s %s(", callconv, get_name( func ) );
            ++indentation;
            indent( header, 0 );
            put_declspec( header, ret, NULL );
            put_str( " *__ret" );
            --indentation;
            if (args)
            {
                put_line( "," );
                put_args( header, args, iface->name, 2, TRUE, NAME_DEFAULT );
            }
            put_line( ") = 0;" );

            indent( header, 0 );
            put_declspec( header, ret, NULL );
            put_line( " %s %s(", callconv, get_name( func ) );
            put_args( header, args, iface->name, 2, TRUE, NAME_DEFAULT );
            put_line( ")" );
            indent( header, 0 );
            put_line( "{" );
            ++indentation;
            indent( header, 0 );
            put_declspec( header, ret, NULL );
            put_line( " __ret;" );
            indent( header, 0 );
            put_str( "return *%s(&__ret", get_name( func ) );
            if (args) LIST_FOR_EACH_ENTRY( arg, args, const var_t, entry )
                put_str( ", %s", arg->name );
            put_line( ");" );
            --indentation;
            indent( header, 0 );
            put_line( "}" );

            put_line( "#else" );
        }

        indent( header, 0 );
        put_str( "virtual " );
        put_declspec( header, ret, NULL );
        put_line( " %s %s(", callconv, get_name( func ) );
        put_args( header, args, iface->name, 2, TRUE, NAME_DEFAULT );
        put_line( ") = 0;" );

        if (is_aggregate_return( func )) put_line( "#endif" );
        put_str( "\n" );
    }
}

static void put_inline_wrappers(FILE *header, const type_t *iface, const type_t *child, const char *name)
{
    const statement_t *stmt;
    int first_iface = 1;
    const type_t *base;

    if ((base = type_iface_get_inherit( iface ))) put_inline_wrappers( header, base, child, name );

    STATEMENTS_FOR_EACH_FUNC( stmt, type_iface_get_stmts( iface ) )
    {
        const var_t *arg, *func = stmt->u.var;
        const var_list_t *args = type_function_get_args( func->declspec.type );

        if (first_iface)
        {
            put_line( "/*** %s methods ***/", iface->name );
            first_iface = 0;
        }

        if (is_override_method( iface, child, func )) continue;
        if (is_callas( func->attrs )) continue;

        put_str( "static __WIDL_INLINE " );
        put_declspec( header, type_function_get_ret( func->declspec.type ), NULL );
        put_str( " %s_%s(", name, get_name( func ) );
        put_args( header, args, name, 1, FALSE, NAME_C );
        put_line( ") {" );
        ++indentation;

        if (!is_aggregate_return( func ))
        {
            indent( header, 0 );
            put_str( "%sThis->lpVtbl->%s(This",
                     is_void( type_function_get_rettype( func->declspec.type ) ) ? "" : "return ",
                     get_vtbl_entry_name( iface, func ) );
        }
        else
        {
            indent( header, 0 );
            put_declspec( header, type_function_get_ret( func->declspec.type ), NULL );
            put_line( " __ret;" );
            indent( header, 0 );
            put_str( "return *This->lpVtbl->%s(This,&__ret", get_vtbl_entry_name( iface, func ) );
        }
        if (args) LIST_FOR_EACH_ENTRY( arg, args, const var_t, entry )
            put_str( ",%s", arg->name );
        put_line( ");" );

        --indentation;
        put_line( "}" );
    }
}

static void write_inline_wrappers(FILE *header, const type_t *iface, const type_t *child, const char *name)
{
    init_output_buffer();
    put_inline_wrappers(header, iface, child, name);
    fputs( (char *)output_buffer, header );
    free( output_buffer );
}

static void do_put_c_method_def(FILE *header, const type_t *iface, const char *name)
{
    const statement_t *stmt;
    int first_iface = 1;

    if (type_iface_get_inherit( iface ))
        do_put_c_method_def( header, type_iface_get_inherit( iface ), name );

    STATEMENTS_FOR_EACH_FUNC( stmt, type_iface_get_stmts( iface ) )
    {
        const var_t *func = stmt->u.var;
        const char *callconv = get_attrp( func->declspec.type->attrs, ATTR_CALLCONV );

        if (first_iface)
        {
            indent( header, 0 );
            fprintf( header, "/*** %s methods ***/\n", iface->name );
            first_iface = 0;
        }

        if (is_callas( func->attrs )) continue;

        if (!callconv) callconv = "STDMETHODCALLTYPE";
        indent( header, 0 );
        put_declspec( header, type_function_get_ret( func->declspec.type ), NULL );
        if (is_aggregate_return( func )) put_str( " *" );
        if (!is_inherited_method( iface, func )) put_line( " (%s *%s)(", callconv, get_name( func ) );
        else put_line( " (%s *%s_%s)(", callconv, iface->name, func->name );
        ++indentation;
        indent( header, 0 );
        put_str( "%s *This", name );
        if (is_aggregate_return( func ))
        {
            put_line( "," );
            indent( header, 0 );
            put_declspec( header, type_function_get_ret( func->declspec.type ), NULL );
            put_str( " *__ret" );
        }
        --indentation;
        if (type_function_get_args( func->declspec.type ))
        {
            put_line( "," );
            write_args( header, type_function_get_args( func->declspec.type ), name, 0, TRUE, NAME_C );
        }
        put_line( ");" );
        put_line( "" );
    }
}

static void put_c_method_def( FILE *header, const type_t *iface )
{
    do_put_c_method_def( header, iface, iface->c_name );
}

static void put_c_disp_method_def( FILE *header, const type_t *iface )
{
    do_put_c_method_def( header, type_iface_get_inherit( iface ), iface->c_name );
}

static void put_method_proto(FILE *header, const type_t *iface)
{
    const statement_t *stmt;

    STATEMENTS_FOR_EACH_FUNC( stmt, type_iface_get_stmts( iface ) )
    {
        const var_t *func = stmt->u.var;
        const char *callconv;

        if (!is_callas( func->attrs )) continue;

        callconv = get_attrp( func->declspec.type->attrs, ATTR_CALLCONV );
        if (!callconv) callconv = "STDMETHODCALLTYPE";

        /* proxy prototype */
        put_declspec( header, type_function_get_ret( func->declspec.type ), NULL );
        put_line( " %s %s_%s_Proxy(", callconv, iface->name, get_name( func ) );
        put_args( header, type_function_get_args( func->declspec.type ), iface->name, 1, TRUE, NAME_DEFAULT );
        put_line( ");" );

        /* stub prototype */
        put_line( "void __RPC_STUB %s_%s_Stub(", iface->name, get_name( func ) );
        put_line( "    IRpcStubBuffer* This," );
        put_line( "    IRpcChannelBuffer* pRpcChannelBuffer," );
        put_line( "    PRPC_MESSAGE pRpcMessage," );
        put_line( "    DWORD* pdwStubPhase);" );
    }
}

static void write_method_proto(FILE *header, const type_t *iface)
{
    init_output_buffer();
    *output_buffer = 0;
    put_method_proto(header, iface);
    fputs( (char *)output_buffer, header );
    free( output_buffer );
}

static void write_locals(FILE *fp, const type_t *iface, int body)
{
  static const char comment[]
    = "/* WIDL-generated stub.  You must provide an implementation for this.  */";
  const statement_t *stmt;

  if (!is_object(iface))
    return;

  STATEMENTS_FOR_EACH_FUNC(stmt, type_iface_get_stmts(iface)) {
    const var_t *func = stmt->u.var;
    const var_t *cas = is_callas(func->attrs);

    if (cas) {
      const statement_t *stmt2 = NULL;
      STATEMENTS_FOR_EACH_FUNC(stmt2, type_iface_get_stmts(iface))
        if (!strcmp(get_name(stmt2->u.var), cas->name))
          break;
      if (&stmt2->entry != type_iface_get_stmts(iface)) {
        const var_t *m = stmt2->u.var;
        /* proxy prototype - use local prototype */
        write_declspec(fp, type_function_get_ret(m->declspec.type), NULL);
        fprintf(fp, " CALLBACK %s_%s_Proxy(\n", iface->name, get_name(m));
        write_args(fp, type_function_get_args(m->declspec.type), iface->name, 1, TRUE, NAME_DEFAULT);
        fprintf(fp, ")");
        if (body) {
          const decl_spec_t *rt = type_function_get_ret(m->declspec.type);
          fprintf(fp, "\n{\n");
          fprintf(fp, "    %s\n", comment);
          if (rt->type->name && strcmp(rt->type->name, "HRESULT") == 0)
            fprintf(fp, "    return E_NOTIMPL;\n");
          else if (type_get_type(rt->type) != TYPE_VOID) {
            fprintf(fp, "    ");
            write_declspec( fp, rt, "rv" );
            fprintf(fp, ";\n");
            fprintf(fp, "    memset(&rv, 0, sizeof rv);\n");
            fprintf(fp, "    return rv;\n");
          }
          fprintf(fp, "}\n\n");
        }
        else
          fprintf(fp, ";\n");
        /* stub prototype - use remotable prototype */
        write_declspec(fp, type_function_get_ret(func->declspec.type), NULL);
        fprintf(fp, " __RPC_STUB %s_%s_Stub(\n", iface->name, get_name(m));
        write_args(fp, type_function_get_args(func->declspec.type), iface->name, 1, TRUE, NAME_DEFAULT);
        fprintf(fp, ")");
        if (body)
          /* Remotable methods must all return HRESULTs.  */
          fprintf(fp, "\n{\n    %s\n    return E_NOTIMPL;\n}\n\n", comment);
        else
          fprintf(fp, ";\n");
      }
      else
        error_loc("invalid call_as attribute (%s -> %s)\n", func->name, cas->name);
    }
  }
}

static void put_locals(FILE *fp, const type_t *iface, int body)
{
    fputs( (char *)output_buffer, fp );
    free( output_buffer );
    write_locals( fp, iface, body );
    init_output_buffer();
}

static void write_local_stubs_stmts(FILE *local_stubs, const statement_list_t *stmts)
{
  const statement_t *stmt;
  if (stmts) LIST_FOR_EACH_ENTRY( stmt, stmts, const statement_t, entry )
  {
    if (stmt->type == STMT_TYPE && type_get_type(stmt->u.type) == TYPE_INTERFACE)
      write_locals(local_stubs, stmt->u.type, TRUE);
  }
}

void write_local_stubs( const struct idl_ctx *ctx )
{
    FILE *local_stubs;

    if (!local_stubs_name) return;
    if (!(local_stubs = fopen( local_stubs_name, "w" ))) error( "Could not open %s for output\n", local_stubs_name );

    fprintf( local_stubs, "/* call_as/local stubs for %s */\n\n", ctx->input );
    fprintf( local_stubs, "#include <objbase.h>\n" );
    fprintf( local_stubs, "#include \"%s\"\n\n", header_name );

    write_local_stubs_stmts( local_stubs, ctx->statements );

    fclose(local_stubs);
}

static void write_function_proto(FILE *header, const type_t *iface, const var_t *fun, const char *prefix)
{
  const char *callconv = get_attrp(fun->declspec.type->attrs, ATTR_CALLCONV);

  if (!callconv) callconv = "__cdecl";
  /* FIXME: do we need to handle call_as? */
  write_declspec(header, type_function_get_ret(fun->declspec.type), NULL);
  fprintf(header, " %s ", callconv);
  fprintf(header, "%s%s(\n", prefix, get_name(fun));
  if (type_function_get_args(fun->declspec.type))
    write_args(header, type_function_get_args(fun->declspec.type), iface->name, 0, TRUE, NAME_DEFAULT);
  else
    fprintf(header, "    void");
  fprintf(header, ");\n\n");
}

static void put_parameterized_type_forward( const type_t *type )
{
    const type_t *iface = type->details.parameterized.type;
    char *args;

    if (type_get_type(iface) == TYPE_DELEGATE) iface = type_delegate_get_iface(iface);

    put_line( "#if defined(__cplusplus) && !defined(CINTERFACE)" );
    put_namespace_start( type->namespace );

    args = format_parameterized_type_args(type, "class ", "");
    put_line( "template <%s>", args );
    put_line( "struct %s_impl;", iface->name );
    put_line( "" );
    put_line( "template <%s>", args );
    free(args);
    args = format_parameterized_type_args(type, "", "");
    put_line( "struct %s : %s_impl<%s> {};", iface->name, iface->name, args );
    free(args);

    put_namespace_end( type->namespace );
    put_line( "#endif" );
    put_line( "" );
}

static void write_parameterized_implementation(FILE *header, type_t *type, int declonly)
{
    const statement_t *stmt;
    typeref_list_t *params = type->details.parameterized.params;
    typeref_t *ref;
    type_t *iface = type->details.parameterized.type, *base;
    char *args = NULL;

    init_output_buffer();

    put_line( "#if defined(__cplusplus) && !defined(CINTERFACE)" );
    put_line( "} /* extern \"C\" */" );
    put_namespace_start( type->namespace );

    if (type_get_type(iface) == TYPE_DELEGATE) iface = type_delegate_get_iface(iface);
    base = type_iface_get_inherit(iface);

    args = format_parameterized_type_args(type, "class ", "");
    put_line( "template <%s>", args );
    free(args);
    put_line( "struct %s_impl%s", iface->name, base ? strmake( " : %s", base->name ) : "" );
    put_line( "{" );

    put_line( "private:" );
    if (params) LIST_FOR_EACH_ENTRY(ref, params, typeref_t, entry)
    {
        put_line( "typedef typename Windows::Foundation::Internal::GetAbiType<%s>::type     %s_abi;",
                  ref->type->name, ref->type->name );
        put_line( "typedef typename Windows::Foundation::Internal::GetLogicalType<%s>::type %s_logical;",
                  ref->type->name, ref->type->name );
    }
    indentation -= 1;

    put_line( "public:" );
    if (params) LIST_FOR_EACH_ENTRY( ref, params, typeref_t, entry )
        put_line( "typedef %s %s_complex;", ref->type->name, ref->type->name );

    STATEMENTS_FOR_EACH_FUNC(stmt, type_iface_get_stmts(iface))
    {
        const var_t *func = stmt->u.var;
        const char *callconv;
        if (is_callas(func->attrs)) continue;

        callconv = get_attrp(func->declspec.type->attrs, ATTR_CALLCONV);
        if (!callconv) callconv = "STDMETHODCALLTYPE";

        indent(header, 1);
        put_str( "virtual " );
        put_declspec( header, type_function_get_ret( func->declspec.type ), NULL );
        put_str( " %s %s(", callconv, get_name( func ) );
        put_args( header, type_function_get_args( func->declspec.type ), NULL, 0, 0, NAME_DEFAULT );
        put_line( ") = 0;" );
        indentation -= 1;
    }
    put_line( "};" );

    put_namespace_end( type->namespace );
    put_line( "extern \"C\" {" );
    put_line( "#endif" );
    put_line( "" );

    fputs( (char *)output_buffer, header );
    free( output_buffer );
}

static void put_forward_interface( const type_t *iface )
{
    put_line( "#ifndef __%s_FWD_DEFINED__", iface->c_name );
    put_line( "#define __%s_FWD_DEFINED__", iface->c_name );
    put_line( "typedef interface %s %s;", iface->c_name, iface->c_name );
    put_line( "#ifdef __cplusplus" );
    if (iface->namespace && !is_global_namespace( iface->namespace ))
        put_line( "#define %s %s", iface->c_name, iface->qualified_name );
    if (!iface->impl_name)
    {
        put_namespace_start( iface->namespace );
        put_line( "interface %s;", iface->name );
        put_namespace_end( iface->namespace );
    }
    put_line( "#endif /* __cplusplus */" );
    put_line( "#endif" );
    put_line( "" );
}

static void write_forward(FILE *header, type_t *iface)
{
  fprintf(header, "#ifndef __%s_FWD_DEFINED__\n", iface->c_name);
  fprintf(header, "#define __%s_FWD_DEFINED__\n", iface->c_name);
  fprintf(header, "typedef interface %s %s;\n", iface->c_name, iface->c_name);
  fprintf(header, "#ifdef __cplusplus\n");
  if (iface->namespace && !is_global_namespace(iface->namespace))
    fprintf(header, "#define %s %s\n", iface->c_name, iface->qualified_name);
  if (!iface->impl_name)
  {
    write_namespace_start(header, iface->namespace);
    write_line(header, 0, "interface %s;", iface->name);
    write_namespace_end(header, iface->namespace);
  }
  fprintf(header, "#endif /* __cplusplus */\n");
  fprintf(header, "#endif\n\n" );
}

static void write_com_interface_start(FILE *header, const type_t *iface)
{
    int dispinterface = is_attr( iface->attrs, ATTR_DISPINTERFACE );
    expr_t *contract = get_attrp( iface->attrs, ATTR_CONTRACT );

    init_output_buffer();

    put_line( "/*****************************************************************************" );
    put_line( " * %s %sinterface", iface->name, dispinterface ? "disp" : "" );
    put_line( " */" );
    if (contract) put_apicontract_guard_start( contract );
    put_line( "#ifndef __%s_%sINTERFACE_DEFINED__", iface->c_name, dispinterface ? "DISP" : "" );
    put_line( "#define __%s_%sINTERFACE_DEFINED__", iface->c_name, dispinterface ? "DISP" : "" );
    put_line( "" );

    fputs( (char *)output_buffer, header );
    free( output_buffer );
}

static void write_com_interface_end(FILE *header, type_t *iface)
{
    int dispinterface = is_attr( iface->attrs, ATTR_DISPINTERFACE );
    const struct uuid *uuid = get_attrp( iface->attrs, ATTR_UUID );
    expr_t *contract = get_attrp( iface->attrs, ATTR_CONTRACT );
    type_t *type;

    init_output_buffer();

    if (uuid) put_guid( dispinterface ? "DIID" : "IID", iface->c_name, uuid );

    /* C++ interface */
    put_line( "#if defined(__cplusplus) && !defined(CINTERFACE)" );
    if (!is_global_namespace( iface->namespace ))
    {
        put_line( "} /* extern \"C\" */" );
        put_namespace_start( iface->namespace );
    }
    if (uuid)
    {
        if (strchr( iface->name, '<' )) put_line( "template<>" );
        put_line( "MIDL_INTERFACE(\"%s\")", uuid_string( uuid ) );
        indent( header, 0 );
    }
    else
    {
        indent( header, 0 );
        if (strchr( iface->name, '<' )) put_str( "template<> struct " );
        else put_str( "interface " );
    }
    if (iface->impl_name)
    {
        put_line( "%s : %s", iface->name, iface->impl_name );
        put_line( "{" );
    }
    else if (type_iface_get_inherit( iface ))
    {
        put_line( "%s : public %s", iface->name, type_iface_get_inherit( iface )->name );
        put_line( "{" );
    }
    else
    {
        put_line( "%s", iface->name );
        put_line( "{" );
        put_line( "BEGIN_INTERFACE" );
    }
    /* dispinterfaces don't have real functions, so don't write C++ functions for
     * them */
    if (!dispinterface && !iface->impl_name) put_cpp_method_def( header, iface );
    if (!type_iface_get_inherit( iface ) && !iface->impl_name) put_line( "END_INTERFACE" );
    put_line( "};" );
    if (!is_global_namespace( iface->namespace ))
    {
        put_namespace_end( iface->namespace );
        put_line( "extern \"C\" {" );
    }
    if (uuid) put_uuid_decl( iface, uuid );
    put_line( "#else" );
    /* C interface */
    put_line( "typedef struct %sVtbl {", iface->c_name );
    put_line( "BEGIN_INTERFACE" );
    if (dispinterface) put_c_disp_method_def( header, iface );
    else put_c_method_def( header, iface );
    put_line( "END_INTERFACE" );
    put_line( "} %sVtbl;", iface->c_name );
    put_line( "interface %s {", iface->c_name );
    put_line( "    CONST_VTBL %sVtbl* lpVtbl;", iface->c_name );
    put_line( "};" );
    put_line( "" );
    put_line( "#ifdef COBJMACROS" );
    /* dispinterfaces don't have real functions, so don't write macros for them,
     * only for the interface this interface inherits from, i.e. IDispatch */
    put_line( "#ifndef WIDL_C_INLINE_WRAPPERS" );
    type = dispinterface ? type_iface_get_inherit( iface ) : iface;
    put_method_macro( header, type, type, iface->c_name );
    put_line( "#else" );
    put_inline_wrappers( header, type, type, iface->c_name );
    put_line( "#endif" );
    if (winrt_mode) put_widl_using_macros( iface );
    put_line( "#endif" );
    put_line( "" );
    put_line( "#endif" );
    put_line( "" );
    /* dispinterfaces don't have real functions, so don't write prototypes for
     * them */
    if (!dispinterface && !winrt_mode)
    {
        put_method_proto( header, iface );
        put_locals( header, iface, FALSE );
        put_line( "" );
    }
    put_line( "#endif  /* __%s_%sINTERFACE_DEFINED__ */", iface->c_name, dispinterface ? "DISP" : "" );
    if (contract) put_apicontract_guard_end( contract );
    put_line( "" );

    fputs( (char *)output_buffer, header );
    free( output_buffer );
}

static void write_rpc_interface_start(FILE *header, const type_t *iface)
{
    unsigned int ver = get_attrv( iface->attrs, ATTR_VERSION );
    const var_t *var = get_attrp( iface->attrs, ATTR_IMPLICIT_HANDLE );
    expr_t *contract = get_attrp( iface->attrs, ATTR_CONTRACT );

    init_output_buffer();

    put_line( "/*****************************************************************************" );
    put_line( " * %s interface (v%d.%d)", iface->name, MAJORVERSION( ver ), MINORVERSION( ver ) );
    put_line( " */" );
    if (contract) put_apicontract_guard_start( contract );
    put_line( "#ifndef __%s_INTERFACE_DEFINED__", iface->name );
    put_line( "#define __%s_INTERFACE_DEFINED__", iface->name );
    put_line( "" );
    if (var)
    {
        put_str( "extern " );
        put_declspec( header, &var->declspec, var->name );
        put_line( ";" );
    }
    if (old_names)
    {
        put_line( "extern RPC_IF_HANDLE %s%s_ClientIfHandle;", prefix_client, iface->name );
        put_line( "extern RPC_IF_HANDLE %s%s_ServerIfHandle;", prefix_server, iface->name );
    }
    else
    {
        put_line( "extern RPC_IF_HANDLE %s%s_v%d_%d_c_ifspec;", prefix_client, iface->name,
                  MAJORVERSION( ver ), MINORVERSION( ver ) );
        put_line( "extern RPC_IF_HANDLE %s%s_v%d_%d_s_ifspec;", prefix_server, iface->name,
                  MAJORVERSION( ver ), MINORVERSION( ver ) );
    }

    fputs( (char *)output_buffer, header );
    free( output_buffer );
}

static void write_rpc_interface_end(FILE *header, const type_t *iface)
{
  expr_t *contract = get_attrp(iface->attrs, ATTR_CONTRACT);
  fprintf(header, "\n#endif  /* __%s_INTERFACE_DEFINED__ */\n", iface->name);
  if (contract) put_apicontract_guard_end( contract, fprintf, header );
  fprintf(header, "\n");
}

static void write_coclass(FILE *header, type_t *cocl)
{
  const struct uuid *uuid = get_attrp(cocl->attrs, ATTR_UUID);

  fprintf(header, "/*****************************************************************************\n");
  fprintf(header, " * %s coclass\n", cocl->name);
  fprintf(header, " */\n\n");
  if (uuid)
      write_guid(header, "CLSID", cocl->name, uuid);
  fprintf(header, "\n#ifdef __cplusplus\n");
  if (uuid)
  {
      fprintf(header, "class DECLSPEC_UUID(\"%s\") %s;\n", uuid_string(uuid), cocl->name);
      write_uuid_decl(header, cocl, uuid);
  }
  else
  {
      fprintf(header, "class %s;\n", cocl->name);
  }
  fprintf(header, "#endif\n");
  fprintf(header, "\n");
}

static void put_coclass_forward( const type_t *klass )
{
    put_line( "#ifndef __%s_FWD_DEFINED__", klass->name );
    put_line( "#define __%s_FWD_DEFINED__", klass->name );
    put_line( "#ifdef __cplusplus" );
    put_line( "typedef class %s %s;", klass->name, klass->name );
    put_line( "#else" );
    put_line( "typedef struct %s %s;", klass->name, klass->name );
    put_line( "#endif /* defined __cplusplus */" );
    put_line( "#endif /* defined __%s_FWD_DEFINED__ */", klass->name );
    put_line( "" );
}

static void write_apicontract(FILE *header, type_t *apicontract)
{
    char *name = format_apicontract_macro(apicontract);
    fprintf(header, "#if !defined(%s_VERSION)\n", name);
    fprintf(header, "#define %s_VERSION %#x\n", name, get_attrv(apicontract->attrs, ATTR_CONTRACTVERSION));
    fprintf(header, "#endif // defined(%s_VERSION)\n\n", name);
    free(name);
}

static void write_runtimeclass(FILE *header, type_t *runtimeclass)
{
    expr_t *contract = get_attrp(runtimeclass->attrs, ATTR_CONTRACT);
    char *name, *c_name;
    size_t i, len;

    init_output_buffer();

    name = format_namespace(runtimeclass->namespace, "", ".", runtimeclass->name, NULL);
    c_name = format_namespace(runtimeclass->namespace, "", "_", runtimeclass->name, NULL);
    put_line( "/*" );
    put_line( " * Class %s", name );
    put_line( " */" );
    if (contract) put_apicontract_guard_start( contract );
    put_line( "#ifndef RUNTIMECLASS_%s_DEFINED", c_name );
    put_line( "#define RUNTIMECLASS_%s_DEFINED", c_name );
    put_line( "#if !defined(_MSC_VER) && !defined(__MINGW32__)" );
    put_str( "static const WCHAR RuntimeClass_%s[] = {", c_name );
    for (i = 0, len = strlen( name ); i < len; ++i) put_str( "'%c',", name[i] );
    put_line( "0};" );
    put_line( "#elif defined(__GNUC__) && !defined(__cplusplus)" );
    /* FIXME: MIDL generates extern const here but GCC warns if extern is initialized */
    put_line( "const DECLSPEC_SELECTANY WCHAR RuntimeClass_%s[] = L\"%s\";", c_name, name );
    put_line( "#else" );
    put_str( "extern const DECLSPEC_SELECTANY WCHAR RuntimeClass_%s[] = {", c_name );
    for (i = 0, len = strlen( name ); i < len; ++i) put_str( "'%c',", name[i] );
    put_line( "0};" );
    put_line( "#endif" );
    put_line( "#endif /* RUNTIMECLASS_%s_DEFINED */", c_name );
    free( c_name );
    free( name );
    if (contract) put_apicontract_guard_end( contract );
    put_line( "" );

    fputs( (char *)output_buffer, header );
    free( output_buffer );
}

static void put_runtimeclass_forward( const type_t *klass )
{
    put_line( "#ifndef __%s_FWD_DEFINED__", klass->c_name );
    put_line( "#define __%s_FWD_DEFINED__", klass->c_name );
    put_line( "#ifdef __cplusplus" );
    put_namespace_start( klass->namespace );
    put_line( "class %s;", klass->name );
    put_namespace_end( klass->namespace );
    put_line( "#else" );
    put_line( "typedef struct %s %s;", klass->c_name, klass->c_name );
    put_line( "#endif /* defined __cplusplus */" );
    put_line( "#endif /* defined __%s_FWD_DEFINED__ */", klass->c_name );
    put_line( "" );
}

static void put_import( const char *import )
{
    char *name = replace_extension( get_basename( import ), ".idl", "" );
    if (!strendswith( name, ".h" )) name = strmake( "%s.h", name );
    put_line( "#include <%s>", name );
    free( name );
}

static void put_imports( const statement_list_t *stmts )
{
    const statement_t *stmt;
    if (stmts) LIST_FOR_EACH_ENTRY( stmt, stmts, const statement_t, entry )
    {
        switch (stmt->type)
        {
        case STMT_TYPE:
            if (type_get_type( stmt->u.type ) == TYPE_INTERFACE)
                put_imports( type_iface_get_stmts( stmt->u.type ) );
            break;
        case STMT_LIBRARY:
            put_imports( stmt->u.lib->stmts );
            break;
        case STMT_IMPORT:
            put_import( stmt->u.str );
            break;
        case STMT_TYPEREF:
        case STMT_IMPORTLIB:
            /* not included in header */
            break;
        case STMT_TYPEDEF:
        case STMT_MODULE:
        case STMT_CPPQUOTE:
        case STMT_PRAGMA:
        case STMT_DECLARATION:
            /* not processed here */
            break;
        }
    }
}

static void put_forward_decls( const statement_list_t *stmts )
{
    const statement_t *stmt;
    if (stmts) LIST_FOR_EACH_ENTRY( stmt, stmts, const statement_t, entry )
    {
        switch (stmt->type)
        {
        case STMT_TYPE:
            if (type_get_type( stmt->u.type ) == TYPE_INTERFACE || type_get_type( stmt->u.type ) == TYPE_DELEGATE)
            {
                const type_t *iface = stmt->u.type;
                if (type_get_type( iface ) == TYPE_DELEGATE) iface = type_delegate_get_iface( iface );
                if (is_object( iface ) || is_attr( iface->attrs, ATTR_DISPINTERFACE ))
                {
                    put_forward_interface( iface );
                    if (type_iface_get_async_iface( iface ))
                        put_forward_interface( type_iface_get_async_iface( iface ) );
                }
            }
            else if (type_get_type( stmt->u.type ) == TYPE_COCLASS)
                put_coclass_forward( stmt->u.type );
            else if (type_get_type( stmt->u.type ) == TYPE_RUNTIMECLASS)
                put_runtimeclass_forward( stmt->u.type );
            else if (type_get_type( stmt->u.type ) == TYPE_PARAMETERIZED_TYPE)
                put_parameterized_type_forward( stmt->u.type );
            break;
        case STMT_LIBRARY:
            put_forward_decls( stmt->u.lib->stmts );
            break;
        case STMT_TYPEREF:
        case STMT_IMPORTLIB:
            /* not included in header */
            break;
        case STMT_IMPORT:
        case STMT_TYPEDEF:
        case STMT_MODULE:
        case STMT_CPPQUOTE:
        case STMT_PRAGMA:
        case STMT_DECLARATION:
            /* not processed here */
            break;
        }
    }
}

static void write_header_stmts(FILE *header, const statement_list_t *stmts, const type_t *iface, int ignore_funcs)
{
  const statement_t *stmt;
  if (stmts) LIST_FOR_EACH_ENTRY( stmt, stmts, const statement_t, entry )
  {
    switch (stmt->type)
    {
      case STMT_TYPE:
        if (type_get_type(stmt->u.type) == TYPE_INTERFACE || type_get_type(stmt->u.type) == TYPE_DELEGATE)
        {
          type_t *iface = stmt->u.type, *async_iface;
          if (type_get_type(stmt->u.type) == TYPE_DELEGATE) iface = type_delegate_get_iface(iface);
          async_iface = type_iface_get_async_iface(iface);
          if (is_object(iface) && !is_object_interface++) default_callconv = "STDMETHODCALLTYPE";
          if (is_attr(stmt->u.type->attrs, ATTR_DISPINTERFACE) || is_object(stmt->u.type))
          {
            write_com_interface_start(header, iface);
            write_header_stmts(header, type_iface_get_stmts(iface), stmt->u.type, TRUE);
            write_com_interface_end(header, iface);
            if (async_iface)
            {
              write_com_interface_start(header, async_iface);
              write_com_interface_end(header, async_iface);
            }
          }
          else
          {
            write_rpc_interface_start(header, iface);
            write_header_stmts(header, type_iface_get_stmts(iface), iface, FALSE);
            write_rpc_interface_end(header, iface);
          }
          if (is_object(iface) && !--is_object_interface) default_callconv = NULL;
        }
        else if (type_get_type(stmt->u.type) == TYPE_COCLASS)
          write_coclass(header, stmt->u.type);
        else if (type_get_type(stmt->u.type) == TYPE_APICONTRACT)
          write_apicontract(header, stmt->u.type);
        else if (type_get_type(stmt->u.type) == TYPE_RUNTIMECLASS)
          write_runtimeclass(header, stmt->u.type);
        else if (type_get_type(stmt->u.type) != TYPE_PARAMETERIZED_TYPE)
          write_type_definition(header, stmt->u.type, stmt->declonly);
        else
        {
          if (!is_object_interface++) default_callconv = "STDMETHODCALLTYPE";
          write_parameterized_implementation(header, stmt->u.type, stmt->declonly);
          if (!--is_object_interface) default_callconv = NULL;
        }
        break;
      case STMT_TYPEREF:
        /* FIXME: shouldn't write out forward declarations for undefined
        * interfaces but a number of our IDL files depend on this */
        if (type_get_type(stmt->u.type) == TYPE_INTERFACE && !stmt->u.type->written)
          write_forward(header, stmt->u.type);
        break;
      case STMT_IMPORTLIB:
      case STMT_MODULE:
      case STMT_PRAGMA:
        /* not included in header */
        break;
      case STMT_IMPORT:
        /* not processed here */
        break;
      case STMT_TYPEDEF:
      {
        typeref_t *ref;
        if (stmt->u.type_list) LIST_FOR_EACH_ENTRY(ref, stmt->u.type_list, typeref_t, entry)
          write_typedef(header, ref->type, stmt->declonly);
        break;
      }
      case STMT_LIBRARY:
        fprintf(header, "#ifndef __%s_LIBRARY_DEFINED__\n", stmt->u.lib->name);
        fprintf(header, "#define __%s_LIBRARY_DEFINED__\n", stmt->u.lib->name);
        write_library(header, stmt->u.lib);
        write_header_stmts(header, stmt->u.lib->stmts, NULL, FALSE);
        fprintf(header, "#endif /* __%s_LIBRARY_DEFINED__ */\n", stmt->u.lib->name);
        break;
      case STMT_CPPQUOTE:
        fprintf(header, "%s\n", stmt->u.str);
        break;
      case STMT_DECLARATION:
        if (iface && type_get_type(stmt->u.var->declspec.type) == TYPE_FUNCTION)
        {
          if (!ignore_funcs)
          {
            int prefixes_differ = strcmp(prefix_client, prefix_server);

            if (prefixes_differ)
            {
              fprintf(header, "/* client prototype */\n");
              write_function_proto(header, iface, stmt->u.var, prefix_client);
              fprintf(header, "/* server prototype */\n");
            }
            write_function_proto(header, iface, stmt->u.var, prefix_server);
          }
        }
        else
          write_declaration(header, stmt->u.var);
        break;
    }
  }
}

void write_header( const struct idl_ctx *ctx )
{
    FILE *header;

    if (!do_header) return;
    if (!(header = fopen( header_name, "w" ))) error( "Could not open %s for output\n", header_name );

    init_output_buffer();
    put_line( "/*** Autogenerated by WIDL %s from %s - Do not edit ***/", PACKAGE_VERSION, input_name );
    put_line( "" );
    put_line( "#ifdef _WIN32" );
    put_line( "#ifndef __REQUIRED_RPCNDR_H_VERSION__" );
    put_line( "#define __REQUIRED_RPCNDR_H_VERSION__ 475" );
    put_line( "#endif" );
    put_line( "#include <rpc.h>" );
    put_line( "#include <rpcndr.h>" );
    if (!for_each_serializable( stmts, NULL, serializable_exists ))
        put_line( "#include <midles.h>" );
    put_line( "#endif" );
    put_line( "" );
    put_line( "#ifndef COM_NO_WINDOWS_H" );
    put_line( "#include <windows.h>" );
    put_line( "#include <ole2.h>" );
    put_line( "#endif" );
    put_line( "" );
    put_line( "#ifndef __%s__", header_token );
    put_line( "#define __%s__", header_token );
    put_line( "" );
    put_line( "#ifndef __WIDL_INLINE" );
    put_line( "#if defined(__cplusplus) || defined(_MSC_VER)" );
    put_line( "#define __WIDL_INLINE inline" );
    put_line( "#elif defined(__GNUC__)" );
    put_line( "#define __WIDL_INLINE __inline__" );
    put_line( "#endif" );
    put_line( "#endif" );
    put_line( "" );
    put_line( "/* Forward declarations */" );
    put_line( "" );
    put_forward_decls( stmts );
    put_line( "/* Headers for imported files */" );
    put_line( "" );
    put_imports( stmts );
    put_line( "" );
    put_line( "#ifdef __cplusplus" );
    put_line( "extern \"C\" {" );
    put_line( "#endif" );
    put_line( "" );
    fputs( (char *)output_buffer, header );
    free( output_buffer );

    write_header_stmts( header, stmts, NULL, FALSE );

    init_output_buffer();
    put_line( "/* Begin additional prototypes for all interfaces */" );
    put_line( "" );
    for_each_serializable( stmts, header, put_serialize_function_decl );
    put_user_types();
    put_generic_handle_routines();
    put_context_handle_rundowns();
    put_line( "" );
    put_line( "/* End additional prototypes */" );
    put_line( "" );
    put_line( "#ifdef __cplusplus" );
    put_line( "}"  /* extern \"C\" */ );
    put_line( "#endif" );
    put_line( "" );
    put_line( "#endif /* __%s__ */", header_token );
    fputs( (char *)output_buffer, header );
    free( output_buffer );

    fclose( header );
}
