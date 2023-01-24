/*
 * IDL Compiler
 *
 * Copyright 2002 Ove Kaaven
 * Copyright 2004 Mike McCormack
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "widl.h"
#include "utils.h"
#include "parser.h"
#include "header.h"
#include "typegen.h"

static FILE* proxy;
static int indent = 0;

static void print_proxy( const char *format, ... ) __attribute__((format (printf, 1, 2)));
static void print_proxy( const char *format, ... )
{
  va_list va;
  va_start( va, format );
  print( proxy, indent, format, va );
  va_end( va );
}

static void write_stubdesc(int expr_eval_routines)
{
  print_proxy( "static const MIDL_STUB_DESC Object_StubDesc =\n{\n");
  indent++;
  print_proxy( "0,\n");
  print_proxy( "NdrOleAllocate,\n");
  print_proxy( "NdrOleFree,\n");
  print_proxy( "{0}, 0, 0, %s, 0,\n", expr_eval_routines ? "ExprEvalRoutines" : "0");
  print_proxy( "__MIDL_TypeFormatString.Format,\n");
  print_proxy( "1, /* -error bounds_check flag */\n");
  print_proxy( "0x%x, /* Ndr library version */\n", get_stub_mode() == MODE_Oif ? 0x50002 : 0x10001);
  print_proxy( "0,\n");
  print_proxy( "0x50200ca, /* MIDL Version 5.2.202 */\n");
  print_proxy( "0,\n");
  print_proxy("%s,\n", list_empty(&user_type_list) ? "0" : "UserMarshalRoutines");
  print_proxy( "0,  /* notify & notify_flag routine table */\n");
  print_proxy( "1,  /* Flags */\n");
  print_proxy( "0,  /* Reserved3 */\n");
  print_proxy( "0,  /* Reserved4 */\n");
  print_proxy( "0   /* Reserved5 */\n");
  indent--;
  print_proxy( "};\n");
  print_proxy( "\n");
}

static void clear_output_vars( const var_list_t *args )
{
  const var_t *arg;

  if (!args) return;
  LIST_FOR_EACH_ENTRY( arg, args, const var_t, entry )
  {
      if (is_attr(arg->attrs, ATTR_IN)) continue;
      if (!is_attr(arg->attrs, ATTR_OUT)) continue;
      if (is_ptr(arg->declspec.type))
      {
          if (type_get_type(type_pointer_get_ref_type(arg->declspec.type)) == TYPE_BASIC) continue;
          if (type_get_type(type_pointer_get_ref_type(arg->declspec.type)) == TYPE_ENUM) continue;
      }
      print_proxy( "if (%s) MIDL_memset( %s, 0, ", arg->name, arg->name );
      if (is_array(arg->declspec.type) && type_array_has_conformance(arg->declspec.type))
      {
          write_expr( proxy, type_array_get_conformance(arg->declspec.type), 1, 1, NULL, NULL, "" );
          fprintf( proxy, " * " );
      }
      fprintf( proxy, "sizeof( *%s ));\n", arg->name );
  }
}

static int need_delegation(const type_t *iface)
{
    const type_t *parent = type_iface_get_inherit( iface );
    return parent && type_iface_get_inherit(parent) && (parent->ignore || is_local( parent->attrs ));
}

static int get_delegation_indirect(const type_t *iface, const type_t ** delegate_to)
{
  const type_t * cur_iface;
  for (cur_iface = iface; cur_iface != NULL; cur_iface = type_iface_get_inherit(cur_iface))
    if (need_delegation(cur_iface))
    {
      if(delegate_to)
        *delegate_to = type_iface_get_inherit(cur_iface);
      return 1;
    }
  return 0;
}

static int need_delegation_indirect(const type_t *iface)
{
  return get_delegation_indirect(iface, NULL);
}

static void free_variable( const var_t *arg, const char *local_var_prefix )
{
  unsigned int type_offset = arg->typestring_offset;
  type_t *type = arg->declspec.type;

  write_parameter_conf_or_var_exprs(proxy, indent, local_var_prefix, PHASE_FREE, arg, FALSE);

  switch (typegen_detect_type(type, arg->attrs, TDT_IGNORE_STRINGS))
  {
  case TGT_ENUM:
  case TGT_BASIC:
    break;

  case TGT_STRUCT:
    if (get_struct_fc(type) != FC_STRUCT)
      print_proxy("/* FIXME: %s code for %s struct type 0x%x missing */\n", __FUNCTION__, arg->name, get_struct_fc(type) );
    break;

  case TGT_IFACE_POINTER:
  case TGT_POINTER:
  case TGT_ARRAY:
    print_proxy( "NdrClearOutParameters( &__frame->_StubMsg, ");
    fprintf(proxy, "&__MIDL_TypeFormatString.Format[%u], ", type_offset );
    fprintf(proxy, "(void *)%s );\n", arg->name );
    break;

  default:
    print_proxy("/* FIXME: %s code for %s type %d missing */\n", __FUNCTION__, arg->name, type_get_type(type) );
  }
}

static void proxy_free_variables( var_list_t *args, const char *local_var_prefix )
{
  const var_t *arg;

  if (!args) return;
  LIST_FOR_EACH_ENTRY( arg, args, const var_t, entry )
    if (is_attr(arg->attrs, ATTR_OUT))
    {
      free_variable( arg, local_var_prefix );
      fprintf(proxy, "\n");
    }
}

static void gen_proxy(type_t *iface, const var_t *func, int idx,
                      unsigned int proc_offset)
{
  var_t *retval = type_function_get_retval(func->declspec.type);
  int has_ret = !is_void(retval->declspec.type);
  int has_full_pointer = is_full_pointer_function(func);
  const char *callconv = get_attrp(func->declspec.type->attrs, ATTR_CALLCONV);
  const var_list_t *args = type_function_get_args(func->declspec.type);
  if (!callconv) callconv = "STDMETHODCALLTYPE";

  indent = 0;
  if (is_interpreted_func( iface, func ))
  {
      if (get_stub_mode() == MODE_Oif && !is_callas( func->attrs )) return;
      write_type_decl_left(proxy, &retval->declspec);
      print_proxy( " %s %s_%s_Proxy(\n", callconv, iface->name, get_name(func));
      write_args(proxy, args, iface->name, 1, TRUE, NAME_DEFAULT);
      print_proxy( ")\n");
      write_client_call_routine( proxy, iface, func, "Object", proc_offset );
      return;
  }
  print_proxy( "static void __finally_%s_%s_Proxy( struct __proxy_frame *__frame )\n",
               iface->name, get_name(func) );
  print_proxy( "{\n");
  indent++;
  if (has_full_pointer) write_full_pointer_free(proxy, indent, func);
  print_proxy( "NdrProxyFreeBuffer( __frame->This, &__frame->_StubMsg );\n" );
  indent--;
  print_proxy( "}\n");
  print_proxy( "\n");

  write_type_decl_left(proxy, &retval->declspec);
  print_proxy( " %s %s_%s_Proxy(\n", callconv, iface->name, get_name(func));
  write_args(proxy, args, iface->name, 1, TRUE, NAME_DEFAULT);
  print_proxy( ")\n");
  print_proxy( "{\n");
  indent ++;
  print_proxy( "struct __proxy_frame __f, * const __frame = &__f;\n" );
  /* local variables */
  if (has_ret) {
    print_proxy( "%s", "" );
    write_type_decl(proxy, &retval->declspec, retval->name);
    fprintf( proxy, ";\n" );
  }
  print_proxy( "RPC_MESSAGE _RpcMessage;\n" );
  if (has_ret) {
    if (decl_indirect(retval->declspec.type))
        print_proxy("void *_p_%s = &%s;\n", retval->name, retval->name);
  }
  print_proxy( "\n");

  print_proxy( "RpcExceptionInit( __proxy_filter, __finally_%s_%s_Proxy );\n", iface->name, get_name(func) );
  print_proxy( "__frame->This = This;\n" );

  if (has_full_pointer)
    write_full_pointer_init(proxy, indent, func, FALSE);

  /* FIXME: trace */
  clear_output_vars( type_function_get_args(func->declspec.type) );

  print_proxy( "RpcTryExcept\n" );
  print_proxy( "{\n" );
  indent++;
  print_proxy( "NdrProxyInitialize(This, &_RpcMessage, &__frame->_StubMsg, &Object_StubDesc, %d);\n", idx);
  write_pointer_checks( proxy, indent, func );

  print_proxy( "RpcTryFinally\n" );
  print_proxy( "{\n" );
  indent++;

  write_remoting_arguments(proxy, indent, func, "", PASS_IN, PHASE_BUFFERSIZE);

  print_proxy( "NdrProxyGetBuffer(This, &__frame->_StubMsg);\n" );

  write_remoting_arguments(proxy, indent, func, "", PASS_IN, PHASE_MARSHAL);

  print_proxy( "NdrProxySendReceive(This, &__frame->_StubMsg);\n" );
  fprintf(proxy, "\n");
  print_proxy( "__frame->_StubMsg.BufferStart = _RpcMessage.Buffer;\n" );
  print_proxy( "__frame->_StubMsg.BufferEnd   = __frame->_StubMsg.BufferStart + _RpcMessage.BufferLength;\n\n" );

  print_proxy("if ((_RpcMessage.DataRepresentation & 0xffff) != NDR_LOCAL_DATA_REPRESENTATION)\n");
  indent++;
  print_proxy("NdrConvert( &__frame->_StubMsg, &__MIDL_ProcFormatString.Format[%u]);\n", proc_offset );
  indent--;
  fprintf(proxy, "\n");

  write_remoting_arguments(proxy, indent, func, "", PASS_OUT, PHASE_UNMARSHAL);

  if (has_ret)
  {
      if (decl_indirect(retval->declspec.type))
          print_proxy("MIDL_memset(&%s, 0, sizeof(%s));\n", retval->name, retval->name);
      else if (is_ptr(retval->declspec.type) || is_array(retval->declspec.type))
          print_proxy("%s = 0;\n", retval->name);
      write_remoting_arguments(proxy, indent, func, "", PASS_RETURN, PHASE_UNMARSHAL);
  }

  indent--;
  print_proxy( "}\n");
  print_proxy( "RpcFinally\n" );
  print_proxy( "{\n" );
  indent++;
  print_proxy( "__finally_%s_%s_Proxy( __frame );\n", iface->name, get_name(func) );
  indent--;
  print_proxy( "}\n");
  print_proxy( "RpcEndFinally\n" );
  indent--;
  print_proxy( "}\n" );
  print_proxy( "RpcExcept(__frame->_StubMsg.dwStubPhase != PROXY_SENDRECEIVE)\n" );
  print_proxy( "{\n" );
  if (has_ret) {
    indent++;
    proxy_free_variables( type_function_get_args(func->declspec.type), "" );
    print_proxy( "/* coverity[uninit_use_in_call:SUPPRESS] */\n" );
    print_proxy( "_RetVal = NdrProxyErrorHandler(RpcExceptionCode());\n" );
    indent--;
  }
  print_proxy( "}\n" );
  print_proxy( "RpcEndExcept\n" );

  if (has_ret) {
    print_proxy( "return _RetVal;\n" );
  }
  indent--;
  print_proxy( "}\n");
  print_proxy( "\n");
}

static void gen_stub(type_t *iface, const var_t *func, const char *cas,
                     unsigned int proc_offset)
{
  const var_t *arg;
  int has_ret = !is_void(type_function_get_rettype(func->declspec.type));
  int has_full_pointer = is_full_pointer_function(func);

  if (is_interpreted_func( iface, func )) return;

  indent = 0;
  print_proxy( "struct __frame_%s_%s_Stub\n{\n", iface->name, get_name(func));
  indent++;
  print_proxy( "__DECL_EXCEPTION_FRAME\n" );
  print_proxy( "MIDL_STUB_MESSAGE _StubMsg;\n");
  print_proxy( "%s * _This;\n", iface->name );
  declare_stub_args( proxy, indent, func );
  indent--;
  print_proxy( "};\n\n" );

  print_proxy( "static void __finally_%s_%s_Stub(", iface->name, get_name(func) );
  print_proxy( " struct __frame_%s_%s_Stub *__frame )\n{\n", iface->name, get_name(func) );
  indent++;
  write_remoting_arguments(proxy, indent, func, "__frame->", PASS_OUT, PHASE_FREE);
  if (has_full_pointer)
    write_full_pointer_free(proxy, indent, func);
  indent--;
  print_proxy( "}\n\n" );

  print_proxy( "void __RPC_STUB %s_%s_Stub(\n", iface->name, get_name(func));
  indent++;
  print_proxy( "IRpcStubBuffer* This,\n");
  print_proxy( "IRpcChannelBuffer *_pRpcChannelBuffer,\n");
  print_proxy( "PRPC_MESSAGE _pRpcMessage,\n");
  print_proxy( "DWORD* _pdwStubPhase)\n");
  indent--;
  print_proxy( "{\n");
  indent++;
  print_proxy( "struct __frame_%s_%s_Stub __f, * const __frame = &__f;\n\n",
               iface->name, get_name(func) );

  print_proxy("__frame->_This = (%s*)((CStdStubBuffer*)This)->pvServerObject;\n\n", iface->name);

  /* FIXME: trace */

  print_proxy("NdrStubInitialize(_pRpcMessage, &__frame->_StubMsg, &Object_StubDesc, _pRpcChannelBuffer);\n");
  fprintf(proxy, "\n");
  print_proxy( "RpcExceptionInit( 0, __finally_%s_%s_Stub );\n", iface->name, get_name(func) );

  write_parameters_init(proxy, indent, func, "__frame->");

  print_proxy("RpcTryFinally\n");
  print_proxy("{\n");
  indent++;
  if (has_full_pointer)
    write_full_pointer_init(proxy, indent, func, TRUE);
  print_proxy("if ((_pRpcMessage->DataRepresentation & 0xffff) != NDR_LOCAL_DATA_REPRESENTATION)\n");
  indent++;
  print_proxy("NdrConvert( &__frame->_StubMsg, &__MIDL_ProcFormatString.Format[%u]);\n", proc_offset );
  indent--;
  fprintf(proxy, "\n");

  write_remoting_arguments(proxy, indent, func, "__frame->", PASS_IN, PHASE_UNMARSHAL);
  fprintf(proxy, "\n");

  assign_stub_out_args( proxy, indent, func, "__frame->" );

  print_proxy("*_pdwStubPhase = STUB_CALL_SERVER;\n");
  fprintf(proxy, "\n");
  print_proxy( "%s", has_ret ? "__frame->_RetVal = " : "" );
  if (cas) fprintf(proxy, "%s_%s_Stub", iface->name, cas);
  else fprintf(proxy, "__frame->_This->lpVtbl->%s", get_name(func));
  fprintf(proxy, "(__frame->_This");

  if (type_function_get_args(func->declspec.type))
  {
      LIST_FOR_EACH_ENTRY( arg, type_function_get_args(func->declspec.type), const var_t, entry )
          fprintf(proxy, ", %s__frame->%s", is_array(arg->declspec.type) && !type_array_is_decl_as_ptr(arg->declspec.type) ? "*" :"" , arg->name);
  }
  fprintf(proxy, ");\n");
  fprintf(proxy, "\n");
  print_proxy("*_pdwStubPhase = STUB_MARSHAL;\n");
  fprintf(proxy, "\n");

  write_remoting_arguments(proxy, indent, func, "__frame->", PASS_OUT, PHASE_BUFFERSIZE);

  if (!is_void(type_function_get_rettype(func->declspec.type)))
    write_remoting_arguments(proxy, indent, func, "__frame->", PASS_RETURN, PHASE_BUFFERSIZE);

  print_proxy("NdrStubGetBuffer(This, _pRpcChannelBuffer, &__frame->_StubMsg);\n");

  write_remoting_arguments(proxy, indent, func, "__frame->", PASS_OUT, PHASE_MARSHAL);
  fprintf(proxy, "\n");

  /* marshall the return value */
  if (!is_void(type_function_get_rettype(func->declspec.type)))
    write_remoting_arguments(proxy, indent, func, "__frame->", PASS_RETURN, PHASE_MARSHAL);

  indent--;
  print_proxy("}\n");
  print_proxy("RpcFinally\n");
  print_proxy("{\n");
  indent++;
  print_proxy( "__finally_%s_%s_Stub( __frame );\n", iface->name, get_name(func) );
  indent--;
  print_proxy("}\n");
  print_proxy("RpcEndFinally\n");

  print_proxy("_pRpcMessage->BufferLength = __frame->_StubMsg.Buffer - (unsigned char *)_pRpcMessage->Buffer;\n");
  indent--;

  print_proxy("}\n");
  print_proxy("\n");
}

static void gen_stub_thunk( type_t *iface, const var_t *func, unsigned int proc_offset )
{
    int has_ret = !is_void( type_function_get_rettype( func->declspec.type ));
    const var_t *arg, *callas = is_callas( func->attrs );
    const var_list_t *args = type_function_get_args( func->declspec.type );

    indent = 0;
    print_proxy( "void __RPC_API %s_%s_Thunk( PMIDL_STUB_MESSAGE pStubMsg )\n",
                 iface->name, get_name(func) );
    print_proxy( "{\n");
    indent++;
    write_func_param_struct( proxy, iface, func->declspec.type,
                             "*pParamStruct = (struct _PARAM_STRUCT *)pStubMsg->StackTop", has_ret );
    print_proxy( "%s%s_%s_Stub( pParamStruct->This",
                 has_ret ? "pParamStruct->_RetVal = " : "", iface->name, callas->name );
    indent++;
    if (args) LIST_FOR_EACH_ENTRY( arg, args, const var_t, entry )
    {
        fprintf( proxy, ",\n%*spParamStruct->%s", 4 * indent, "", arg->name );
    }
    fprintf( proxy, " );\n" );
    indent--;
    indent--;
    print_proxy( "}\n\n");
}

int count_methods(const type_t *iface)
{
    const statement_t *stmt;
    int count = 0;

    if (type_iface_get_inherit(iface))
        count = count_methods(type_iface_get_inherit(iface));

    STATEMENTS_FOR_EACH_FUNC(stmt, type_iface_get_stmts(iface)) {
        const var_t *func = stmt->u.var;
        if (!is_callas(func->attrs)) count++;
    }
    return count;
}

static const statement_t * get_callas_source(const type_t * iface, const var_t * def)
{
  const statement_t * source;
  STATEMENTS_FOR_EACH_FUNC( source, type_iface_get_stmts(iface)) {
    const var_t * cas = is_callas(source->u.var->attrs );
    if (cas && !strcmp(def->name, cas->name))
      return source;
  }
  return NULL;
}

static void put_proxy_procformatstring_offsets( const type_t *iface, int skip )
{
    const statement_t *stmt;
    type_t *base;

    if (!(base = type_iface_get_inherit( iface ))) return; /* skip IUnknown */
    put_proxy_procformatstring_offsets( base, need_delegation( iface ) );

    STATEMENTS_FOR_EACH_FUNC( stmt, type_iface_get_stmts(iface) )
    {
        const var_t *func = stmt->u.var;
        int missing = 0;

        if (is_callas(func->attrs)) continue;
        if (is_local(func->attrs))
        {
            const statement_t * callas_source = get_callas_source(iface, func);
            if (!callas_source)
                missing = 1;
            else
                func = callas_source->u.var;
        }
        if (skip || missing)
            put_line( "(unsigned short)-1,  /* %s::%s */", iface->name, get_name( func ) );
        else
            put_line( "%u,  /* %s::%s */", func->procstring_offset, iface->name, get_name( func ) );
    }
}

static int put_proxy_methods( type_t *iface, int skip )
{
    const statement_t *stmt;
    type_t *base;
    int i = 0;

    if ((base = type_iface_get_inherit( iface ))) i = put_proxy_methods( base, need_delegation( iface ) );

    STATEMENTS_FOR_EACH_FUNC( stmt, type_iface_get_stmts( iface ) )
    {
        const var_t *func = stmt->u.var;
        if (!is_callas( func->attrs ))
        {
            if (skip || (is_local( func->attrs ) && !get_callas_source( iface, func )))
                put_line( "0,  /* %s::%s */", iface->name, get_name( func ) );
            else if (is_interpreted_func( iface, func ) && get_stub_mode() == MODE_Oif &&
                     !is_local( func->attrs ) && base)
                put_line( "(void *)-1,  /* %s::%s */", iface->name, get_name( func ) );
            else
                put_line( "%s_%s_Proxy,", iface->name, get_name( func ) );
            i++;
        }
    }
    return i;
}

static int put_stub_methods( type_t *iface, int skip )
{
    const statement_t *stmt;
    type_t *base;
    int i = 0;

    if (!(base = type_iface_get_inherit( iface ))) return i; /* skip IUnknown */
    i = put_stub_methods( base, need_delegation( iface ) );

    STATEMENTS_FOR_EACH_FUNC( stmt, type_iface_get_stmts( iface ) )
    {
        const var_t *func = stmt->u.var;
        if (!is_callas( func->attrs ))
        {
            int missing = 0;
            const char *fname = get_name( func );
            if (is_local( func->attrs ))
            {
                const statement_t *callas_source = get_callas_source( iface, func );
                if (!callas_source) missing = 1;
                else fname = get_name( callas_source->u.var );
            }
            if (i) put_line( "," );
            if (skip || missing)
                put_str( "STUB_FORWARDING_FUNCTION" );
            else if (is_interpreted_func( iface, func ))
                put_str( "(PRPC_STUB_FUNCTION)%s", get_stub_mode() == MODE_Oif ? "NdrStubCall2" : "NdrStubCall" );
            else
                put_str( "%s_%s_Stub", iface->name, fname );
            i++;
        }
    }
    return i;
}

static void put_thunk_methods( type_t *iface, int skip )
{
    const statement_t *stmt;
    type_t *base;

    if (!(base = type_iface_get_inherit( iface ))) return; /* skip IUnknown */
    put_thunk_methods( base, need_delegation( iface ) );

    STATEMENTS_FOR_EACH_FUNC( stmt, type_iface_get_stmts(iface) )
    {
        var_t *func = stmt->u.var;
        const statement_t * callas_source = NULL;

        if (is_callas(func->attrs)) continue;
        if (is_local(func->attrs)) callas_source = get_callas_source(iface, func);

        if (!skip && callas_source && is_interpreted_func( iface, func ))
            put_line( "%s_%s_Thunk,", iface->name, get_name( callas_source->u.var ) );
        else
            put_line( "0, /* %s::%s */", iface->name, get_name( func ) );
    }
}

static void write_proxy( type_t *iface, unsigned int *proc_offset )
{
    int count;
    const statement_t *stmt;
    int first_func = 1;
    int needs_stub_thunks = 0;
    int needs_inline_stubs = need_inline_stubs( iface ) || need_delegation( iface );

    STATEMENTS_FOR_EACH_FUNC( stmt, type_iface_get_stmts( iface ) )
    {
        var_t *func = stmt->u.var;
        if (first_func)
        {
            fprintf( proxy, "/*****************************************************************************\n" );
            fprintf( proxy, " * %s interface\n", iface->name );
            fprintf( proxy, " */\n" );
            first_func = 0;
        }
        if (!is_local( func->attrs ))
        {
            const var_t *cas = is_callas( func->attrs );
            const char *cname = cas ? cas->name : NULL;
            int idx = func->func_idx;
            if (cname)
            {
                const statement_t *stmt2;
                STATEMENTS_FOR_EACH_FUNC( stmt2, type_iface_get_stmts( iface ) )
                {
                    const var_t *m = stmt2->u.var;
                    if (!strcmp( m->name, cname ))
                    {
                        idx = m->func_idx;
                        break;
                    }
                }
            }
            func->procstring_offset = *proc_offset;
            gen_proxy( iface, func, idx, *proc_offset );
            gen_stub( iface, func, cname, *proc_offset );
            if (cas && is_interpreted_func( iface, func ))
            {
                needs_stub_thunks = 1;
                gen_stub_thunk( iface, func, *proc_offset );
            }
            *proc_offset += get_size_procformatstring_func( iface, func );
        }
    }

    init_output_buffer();

    count = count_methods( iface );

    put_line( "static const unsigned short %s_FormatStringOffsetTable[] =", iface->name );
    put_line( "{" );
    indent++;
    put_proxy_procformatstring_offsets( iface, 0 );
    indent--;
    put_line( "};" );
    put_line( "" );

    /* proxy info */
    if (get_stub_mode() == MODE_Oif)
    {
        put_line( "static const MIDL_STUBLESS_PROXY_INFO %s_ProxyInfo =", iface->name );
        put_line( "{" );
        indent++;
        put_line( "&Object_StubDesc," );
        put_line( "__MIDL_ProcFormatString.Format," );
        put_line( "&%s_FormatStringOffsetTable[-3],", iface->name );
        put_line( "0," );
        put_line( "0," );
        put_line( "0" );
        indent--;
        put_line( "};" );
        put_line( "" );
    }

    /* proxy vtable */
    put_line( "static %sCINTERFACE_PROXY_VTABLE(%d) _%sProxyVtbl =",
              (get_stub_mode() != MODE_Os || need_delegation_indirect( iface )) ? "" : "const ",
              count, iface->name );
    put_line( "{" );
    indent++;
    put_line( "{" );
    indent++;
    if (get_stub_mode() == MODE_Oif) put_line( "&%s_ProxyInfo,", iface->name );
    put_line( "&IID_%s,", iface->name );
    indent--;
    put_line( "}," );
    put_line( "{" );
    indent++;
    put_proxy_methods( iface, FALSE );
    indent--;
    put_line( "}" );
    indent--;
    put_line( "};" );
    put_line( "" );

    /* stub thunk table */
    if (needs_stub_thunks)
    {
        put_line( "static const STUB_THUNK %s_StubThunkTable[] =", iface->name );
        put_line( "{" );
        indent++;
        put_thunk_methods( iface, 0 );
        indent--;
        put_line( "};" );
        put_line( "" );
    }

    /* server info */
    put_line( "static const MIDL_SERVER_INFO %s_ServerInfo =", iface->name );
    put_line( "{" );
    indent++;
    put_line( "&Object_StubDesc," );
    put_line( "0," );
    put_line( "__MIDL_ProcFormatString.Format," );
    put_line( "&%s_FormatStringOffsetTable[-3],", iface->name );
    if (needs_stub_thunks) put_line( "&%s_StubThunkTable[-3],", iface->name );
    else put_line( "0," );
    put_line( "0," );
    put_line( "0," );
    put_line( "0" );
    indent--;
    put_line( "};" );
    put_line( "" );

    /* stub vtable */
    if (needs_inline_stubs)
    {
        put_line( "static const PRPC_STUB_FUNCTION %s_table[] =", iface->name );
        put_line( "{" );
        indent++;
        put_stub_methods( iface, FALSE );
        put_line( "" );
        indent--;
        put_line( "};" );
        put_line( "" );
    }
    put_line( "static %sCInterfaceStubVtbl _%sStubVtbl =", need_delegation_indirect( iface ) ? "" : "const ",
              iface->name );
    put_line( "{" );
    indent++;
    put_line( "{" );
    indent++;
    put_line( "&IID_%s,", iface->name );
    put_line( "&%s_ServerInfo,", iface->name );
    put_line( "%d,", count );
    if (needs_inline_stubs) put_line( "&%s_table[-3]", iface->name );
    else put_line( "0" );
    indent--;
    put_line( "}," );
    put_line( "{" );
    indent++;
    put_line( "%s_%s", type_iface_get_async_iface( iface ) == iface ? "CStdAsyncStubBuffer" : "CStdStubBuffer",
              need_delegation_indirect( iface ) ? "DELEGATING_METHODS" : "METHODS" );
    indent--;
    put_line( "}" );
    indent--;
    put_line( "};" );
    put_line( "" );

    fputs( (char *)output_buffer, proxy );
    free( output_buffer );
}

static int does_any_iface(const statement_list_t *stmts, type_pred_t pred)
{
  const statement_t *stmt;

  if (stmts)
    LIST_FOR_EACH_ENTRY(stmt, stmts, const statement_t, entry)
    {
      if (stmt->type == STMT_TYPE && type_get_type(stmt->u.type) == TYPE_INTERFACE)
      {
        if (pred(stmt->u.type))
          return TRUE;
      }
    }

  return FALSE;
}

int need_proxy(const type_t *iface)
{
    if (!is_object( iface )) return 0;
    if (is_local( iface->attrs )) return 0;
    if (is_attr( iface->attrs, ATTR_DISPINTERFACE )) return 0;
    return 1;
}

int need_stub(const type_t *iface)
{
  return !is_object(iface) && !is_local(iface->attrs);
}

int need_proxy_file(const statement_list_t *stmts)
{
    return does_any_iface(stmts, need_proxy);
}

int need_proxy_delegation(const statement_list_t *stmts)
{
    return does_any_iface(stmts, need_delegation);
}

int need_inline_stubs(const type_t *iface)
{
    const statement_t *stmt;

    if (get_stub_mode() == MODE_Os) return 1;

    STATEMENTS_FOR_EACH_FUNC( stmt, type_iface_get_stmts(iface) )
    {
        const var_t *func = stmt->u.var;
        if (is_local( func->attrs )) continue;
        if (!is_interpreted_func( iface, func )) return 1;
    }
    return 0;
}

static int need_proxy_and_inline_stubs(const type_t *iface)
{
    const statement_t *stmt;

    if (!need_proxy( iface )) return 0;
    if (get_stub_mode() == MODE_Os) return 1;

    STATEMENTS_FOR_EACH_FUNC( stmt, type_iface_get_stmts(iface) )
    {
        const var_t *func = stmt->u.var;
        if (is_local( func->attrs )) continue;
        if (!is_interpreted_func( iface, func )) return 1;
    }
    return 0;
}

int need_stub_files(const statement_list_t *stmts)
{
  return does_any_iface(stmts, need_stub);
}

int need_inline_stubs_file(const statement_list_t *stmts)
{
  return does_any_iface(stmts, need_inline_stubs);
}

static void write_proxy_stmts( const statement_list_t *stmts, unsigned int *proc_offset )
{
    const statement_t *stmt;
    if (stmts) LIST_FOR_EACH_ENTRY( stmt, stmts, const statement_t, entry )
    {
        if (stmt->type == STMT_TYPE && type_get_type( stmt->u.type ) == TYPE_INTERFACE)
        {
            type_t *iface = stmt->u.type;
            if (need_proxy( iface ))
            {
                write_proxy( iface, proc_offset );
                if (type_iface_get_async_iface( iface ))
                    write_proxy( type_iface_get_async_iface( iface ), proc_offset );
            }
        }
    }
}

static int cmp_iid( const void *ptr1, const void *ptr2 )
{
    const type_t * const *iface1 = ptr1;
    const type_t * const *iface2 = ptr2;
    const struct uuid *uuid1 = get_attrp( (*iface1)->attrs, ATTR_UUID );
    const struct uuid *uuid2 = get_attrp( (*iface2)->attrs, ATTR_UUID );
    return memcmp( uuid1, uuid2, sizeof(*uuid1) );
}

static void build_iface_list( const statement_list_t *stmts, type_t **ifaces[], int *count )
{
    const statement_t *stmt;

    if (!stmts) return;
    LIST_FOR_EACH_ENTRY( stmt, stmts, const statement_t, entry )
    {
        if (stmt->type == STMT_TYPE && type_get_type(stmt->u.type) == TYPE_INTERFACE)
        {
            type_t *iface = stmt->u.type;
            if (type_iface_get_inherit(iface) && need_proxy(iface))
            {
                *ifaces = xrealloc( *ifaces, (*count + 1) * sizeof(**ifaces) );
                (*ifaces)[(*count)++] = iface;
                if (type_iface_get_async_iface(iface))
                {
                    iface = type_iface_get_async_iface(iface);
                    *ifaces = xrealloc( *ifaces, (*count + 1) * sizeof(**ifaces) );
                    (*ifaces)[(*count)++] = iface;
                }
            }
        }
    }
}

static type_t **sort_interfaces( const statement_list_t *stmts, int *count )
{
    type_t **ifaces = NULL;

    *count = 0;
    build_iface_list( stmts, &ifaces, count );
    qsort( ifaces, *count, sizeof(*ifaces), cmp_iid );
    return ifaces;
}

void write_proxies( const statement_list_t *stmts )
{
    int expr_eval_routines;
    unsigned int proc_offset = 0;
    char *file_id = proxy_token;
    int i, count, have_baseiid = 0;
    unsigned int table_version;
    type_t **interfaces;
    const type_t * delegate_to;

    if (!do_proxies) return;
    if (do_everything && !need_proxy_file( stmts )) return;

    if (!proxy && !(proxy = fopen( proxy_name, "w" ))) error( "Could not open %s for output\n", proxy_name );
    if (!proxy) return;

    init_output_buffer();
    put_line( "/*** Autogenerated by WIDL %s from %s - Do not edit ***/", PACKAGE_VERSION, input_name );
    put_line( "" );
    put_line( "#define __midl_proxy" );
    put_line( "#include \"objbase.h\"" );
    put_line( "" );
    put_line( "#ifndef DECLSPEC_HIDDEN" );
    put_line( "#define DECLSPEC_HIDDEN" );
    put_line( "#endif" );
    put_line( "" );

    put_line( "#ifndef __REDQ_RPCPROXY_H_VERSION__" );
    put_line( "#define __REQUIRED_RPCPROXY_H_VERSION__ %u", get_stub_mode() == MODE_Oif ? 475 : 440 );
    put_line( "#endif" );
    put_line( "" );
    if (get_stub_mode() == MODE_Oif) put_line( "#define USE_STUBLESS_PROXY" );
    put_line( "#include \"rpcproxy.h\"" );
    put_line( "#ifndef __RPCPROXY_H_VERSION__" );
    put_line( "#error This code needs a newer version of rpcproxy.h" );
    put_line( "#endif /* __RPCPROXY_H_VERSION__ */" );
    put_line( "" );
    put_line( "#include \"%s\"", header_name );
    put_line( "" );

    if (does_any_iface(stmts, need_proxy_and_inline_stubs))
    {
        put_exceptions();
        put_line( "" );
        put_line( "struct __proxy_frame" );
        put_line( "{" );
        put_line( "    __DECL_EXCEPTION_FRAME" );
        put_line( "    MIDL_STUB_MESSAGE _StubMsg;" );
        put_line( "    void             *This;" );
        put_line( "};" );
        put_line( "" );
        put_line( "static int __proxy_filter( struct __proxy_frame *__frame )" );
        put_line( "{" );
        put_line( "    return (__frame->_StubMsg.dwStubPhase != PROXY_SENDRECEIVE);" );
        put_line( "}" );
        put_line( "" );
    }

    put_format_string_decls( stmts, need_proxy );
    put_line( "static const MIDL_STUB_DESC Object_StubDesc;" );
    put_line( "" );

    fputs( (char *)output_buffer, proxy );
    free( output_buffer );

    write_proxy_stmts( stmts, &proc_offset );

  expr_eval_routines = write_expr_eval_routines(proxy, proxy_token);
  if (expr_eval_routines)
      write_expr_eval_routine_list(proxy, proxy_token);
  write_user_quad_list(proxy);
  write_stubdesc(expr_eval_routines);

  print_proxy( "#if !defined(__RPC_WIN%u__)\n", pointer_size == 8 ? 64 : 32);
  print_proxy( "#error Invalid build platform for this proxy.\n");
  print_proxy( "#endif\n");
  print_proxy( "\n");
  write_procformatstring(proxy, stmts, need_proxy);
  write_typeformatstring(proxy, stmts, need_proxy);

    init_output_buffer();
    interfaces = sort_interfaces( stmts, &count );
    put_line( "static const CInterfaceProxyVtbl* const _%s_ProxyVtblList[] =", file_id );
    put_line( "{" );
    for (i = 0; i < count; i++)
        put_line( "    (const CInterfaceProxyVtbl*)&_%sProxyVtbl,", interfaces[i]->name );
    put_line( "    0" );
    put_line( "};" );
    put_line( "" );

    put_line( "static const CInterfaceStubVtbl* const _%s_StubVtblList[] =", file_id );
    put_line( "{" );
    for (i = 0; i < count; i++) put_line( "    &_%sStubVtbl,", interfaces[i]->name );
    put_line( "    0" );
    put_line( "};" );
    put_line( "" );

    put_line( "static PCInterfaceName const _%s_InterfaceNamesList[] =", file_id );
    put_line( "{" );
    for (i = 0; i < count; i++) put_line( "    \"%s\",", interfaces[i]->name );
    put_line( "    0" );
    put_line( "};" );
    put_line( "" );

    for (i = 0; i < count; i++)
        if ((have_baseiid = get_delegation_indirect( interfaces[i], NULL ))) break;

    if (have_baseiid)
    {
        put_line( "static const IID * _%s_BaseIIDList[] =", file_id );
        put_line( "{" );
        for (i = 0; i < count; i++)
        {
            if (!get_delegation_indirect( interfaces[i], &delegate_to )) put_line( "    0," );
            else put_line( "    &IID_%s,  /* %s */", delegate_to->name, interfaces[i]->name );
        }
        put_line( "    0" );
        put_line( "};" );
        put_line( "" );
    }

    put_line( "static int __stdcall _%s_IID_Lookup(const IID* pIID, int* pIndex)", file_id );
    put_line( "{" );
    put_line( "    int low = 0, high = %d;", count - 1 );
    put_line( "" );
    put_line( "    while (low <= high)" );
    put_line( "    {" );
    put_line( "        int pos = (low + high) / 2;" );
    put_line( "        int res = IID_GENERIC_CHECK_IID(_%s, pIID, pos);", file_id );
    put_line( "        if (!res) { *pIndex = pos; return 1; }" );
    put_line( "        if (res > 0) low = pos + 1;" );
    put_line( "        else high = pos - 1;" );
    put_line( "    }" );
    put_line( "    return 0;" );
    put_line( "}" );
    put_line( "" );

    table_version = get_stub_mode() == MODE_Oif ? 2 : 1;
    for (i = 0; i < count; i++)
    {
        if (type_iface_get_async_iface( interfaces[i] ) != interfaces[i]) continue;
        if (table_version != 6)
        {
            put_line( "static const IID *_AsyncInterfaceTable[] =" );
            put_line( "{" );
            table_version = 6;
        }
        put_line( "    &IID_%s,", interfaces[i]->name );
        put_line( "    (IID*)(LONG_PTR)-1," );
    }
    if (table_version == 6)
    {
        put_line( "    0" );
        put_line( "};" );
        put_line( "" );
    }

    put_line( "const ExtendedProxyFileInfo %s_ProxyFileInfo =", file_id );
    put_line( "{" );
    put_line( "    (const PCInterfaceProxyVtblList*)_%s_ProxyVtblList,", file_id );
    put_line( "    (const PCInterfaceStubVtblList*)_%s_StubVtblList,", file_id );
    put_line( "    _%s_InterfaceNamesList,", file_id );
    if (have_baseiid) put_line( "    _%s_BaseIIDList,", file_id );
    else put_line( "    0," );
    put_line( "    _%s_IID_Lookup,", file_id );
    put_line( "    %d,", count );
    put_line( "    %u,", table_version );
    put_line( "    %s,", table_version == 6 ? "_AsyncInterfaceTable" : "0" );
    put_line( "    0," );
    put_line( "    0," );
    put_line( "    0" );
    put_line( "};" );
    fputs( (char *)output_buffer, proxy );
    free( output_buffer );

    fclose( proxy );
}
