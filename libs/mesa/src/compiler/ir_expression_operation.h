/*
 * Copyright (C) 2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

enum ir_expression_operation {
   ir_unop_bit_not,
   ir_unop_logic_not,
   ir_unop_neg,
   ir_unop_abs,
   ir_unop_sign,
   ir_unop_rcp,
   ir_unop_rsq,
   ir_unop_sqrt,
   ir_unop_exp,
   ir_unop_log,
   ir_unop_exp2,
   ir_unop_log2,
   ir_unop_f2i,
   ir_unop_f2u,
   ir_unop_i2f,
   ir_unop_f2b,
   ir_unop_b2f,
   ir_unop_b2f16,
   ir_unop_i2b,
   ir_unop_b2i,
   ir_unop_u2f,
   ir_unop_i2u,
   ir_unop_u2i,
   ir_unop_d2f,
   ir_unop_f2d,
   ir_unop_f2f16,
   ir_unop_f2fmp,
   ir_unop_f162f,
   ir_unop_u2f16,
   ir_unop_f162u,
   ir_unop_i2f16,
   ir_unop_f162i,
   ir_unop_d2f16,
   ir_unop_f162d,
   ir_unop_u642f16,
   ir_unop_f162u64,
   ir_unop_i642f16,
   ir_unop_f162i64,
   ir_unop_i2i,
   ir_unop_i2imp,
   ir_unop_u2u,
   ir_unop_u2ump,
   ir_unop_d2i,
   ir_unop_i2d,
   ir_unop_d2u,
   ir_unop_u2d,
   ir_unop_d2b,
   ir_unop_f162b,
   ir_unop_bitcast_i2f,
   ir_unop_bitcast_f2i,
   ir_unop_bitcast_u2f,
   ir_unop_bitcast_f2u,
   ir_unop_bitcast_u642d,
   ir_unop_bitcast_i642d,
   ir_unop_bitcast_d2u64,
   ir_unop_bitcast_d2i64,
   ir_unop_i642i,
   ir_unop_u642i,
   ir_unop_i642u,
   ir_unop_u642u,
   ir_unop_i642b,
   ir_unop_i642f,
   ir_unop_u642f,
   ir_unop_i642d,
   ir_unop_u642d,
   ir_unop_i2i64,
   ir_unop_u2i64,
   ir_unop_b2i64,
   ir_unop_f2i64,
   ir_unop_d2i64,
   ir_unop_i2u64,
   ir_unop_u2u64,
   ir_unop_f2u64,
   ir_unop_d2u64,
   ir_unop_u642i64,
   ir_unop_i642u64,
   ir_unop_trunc,
   ir_unop_ceil,
   ir_unop_floor,
   ir_unop_fract,
   ir_unop_round_even,
   ir_unop_sin,
   ir_unop_cos,
   ir_unop_atan,
   ir_unop_dFdx,
   ir_unop_dFdx_coarse,
   ir_unop_dFdx_fine,
   ir_unop_dFdy,
   ir_unop_dFdy_coarse,
   ir_unop_dFdy_fine,
   ir_unop_pack_snorm_2x16,
   ir_unop_pack_snorm_4x8,
   ir_unop_pack_unorm_2x16,
   ir_unop_pack_unorm_4x8,
   ir_unop_pack_half_2x16,
   ir_unop_unpack_snorm_2x16,
   ir_unop_unpack_snorm_4x8,
   ir_unop_unpack_unorm_2x16,
   ir_unop_unpack_unorm_4x8,
   ir_unop_unpack_half_2x16,
   ir_unop_bitfield_reverse,
   ir_unop_bit_count,
   ir_unop_find_msb,
   ir_unop_find_lsb,
   ir_unop_clz,
   ir_unop_saturate,
   ir_unop_pack_double_2x32,
   ir_unop_unpack_double_2x32,
   ir_unop_pack_sampler_2x32,
   ir_unop_pack_image_2x32,
   ir_unop_unpack_sampler_2x32,
   ir_unop_unpack_image_2x32,
   ir_unop_frexp_sig,
   ir_unop_frexp_exp,
   ir_unop_subroutine_to_int,
   ir_unop_interpolate_at_centroid,
   ir_unop_get_buffer_size,
   ir_unop_ssbo_unsized_array_length,
   ir_unop_implicitly_sized_array_length,
   ir_unop_pack_int_2x32,
   ir_unop_pack_uint_2x32,
   ir_unop_unpack_int_2x32,
   ir_unop_unpack_uint_2x32,
   ir_binop_add,
   ir_binop_sub,
   ir_binop_add_sat,
   ir_binop_sub_sat,
   ir_binop_abs_sub,
   ir_binop_avg,
   ir_binop_avg_round,
   ir_binop_mul,
   ir_binop_mul_32x16,
   ir_binop_imul_high,
   ir_binop_div,
   ir_binop_carry,
   ir_binop_borrow,
   ir_binop_mod,
   ir_binop_less,
   ir_binop_gequal,
   ir_binop_equal,
   ir_binop_nequal,
   ir_binop_all_equal,
   ir_binop_any_nequal,
   ir_binop_lshift,
   ir_binop_rshift,
   ir_binop_bit_and,
   ir_binop_bit_xor,
   ir_binop_bit_or,
   ir_binop_logic_and,
   ir_binop_logic_xor,
   ir_binop_logic_or,
   ir_binop_dot,
   ir_binop_min,
   ir_binop_max,
   ir_binop_pow,
   ir_binop_ldexp,
   ir_binop_vector_extract,
   ir_binop_interpolate_at_offset,
   ir_binop_interpolate_at_sample,
   ir_binop_atan2,
   ir_triop_fma,
   ir_triop_lrp,
   ir_triop_csel,
   ir_triop_bitfield_extract,
   ir_triop_vector_insert,
   ir_quadop_bitfield_insert,
   ir_quadop_vector,

   /* Sentinels marking the last of each kind of operation. */
   ir_last_unop = ir_unop_unpack_uint_2x32,
   ir_last_binop = ir_binop_atan2,
   ir_last_triop = ir_triop_vector_insert,
   ir_last_quadop = ir_quadop_vector,
   ir_last_opcode = ir_quadop_vector
};
