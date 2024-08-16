#include "nir_to_spirv/nir_to_spirv.h"

#include "nir.h"
#include "nir_builder.h"
#include "nir_search.h"
#include "nir_search_helpers.h"

/* What follows is NIR algebraic transform code for the following 2
 * transforms:
 *    ('b2b32', 'a') => ('b2i32', 'a')
 *    ('b2b1', 'a') => ('ine', 'a', 0)
 */


static const nir_search_value_union zink_nir_lower_b2b_values[] = {
   /* ('b2b32', 'a') => ('b2i32', 'a') */
   { .variable = {
      { nir_search_value_variable, -1 },
      0, /* a */
      false,
      nir_type_invalid,
      -1,
      {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      false,
      false,
      false,
      nir_op_b2b32,
      -1, 0,
      { 0 },
      -1,
   } },

   /* replace0_0 -> 0 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      false,
      false,
      false,
      false,
      false,
      false,
      nir_op_b2i32,
      -1, 0,
      { 0 },
      -1,
   } },

   /* ('b2b1', 'a') => ('ine', 'a', 0) */
   /* search1_0 -> 0 in the cache */
   { .expression = {
      { nir_search_value_expression, 1 },
      false,
      false,
      false,
      false,
      false,
      false,
      nir_op_b2b1,
      -1, 0,
      { 0 },
      -1,
   } },

   /* replace1_0 -> 0 in the cache */
   { .constant = {
      { nir_search_value_constant, -1 },
      nir_type_int, { 0x0ull /* 0 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 1 },
      false,
      false,
      false,
      false,
      false,
      false,
      nir_op_ine,
      0, 1,
      { 0, 4 },
      -1,
   } },

};



static const struct transform zink_nir_lower_b2b_transforms[] = {
   { ~0, ~0, ~0 }, /* Sentinel */

   { 1, 2, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 3, 5, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

};

static const struct per_op_table zink_nir_lower_b2b_pass_op_table[nir_num_search_ops] = {
   [nir_op_b2b32] = {
      .filter = NULL,
      
      .num_filtered_states = 1,
      .table = (const uint16_t []) {
      
         2,
      },
   },
   [nir_op_b2b1] = {
      .filter = NULL,
      
      .num_filtered_states = 1,
      .table = (const uint16_t []) {
      
         3,
      },
   },
};

/* Mapping from state index to offset in transforms (0 being no transforms) */
static const uint16_t zink_nir_lower_b2b_transform_offsets[] = {
   0,
   0,
   1,
   3,
};

static const nir_algebraic_table zink_nir_lower_b2b_table = {
   .transforms = zink_nir_lower_b2b_transforms,
   .transform_offsets = zink_nir_lower_b2b_transform_offsets,
   .pass_op_table = zink_nir_lower_b2b_pass_op_table,
   .values = zink_nir_lower_b2b_values,
   .expression_cond = NULL,
   .variable_cond = NULL,
};

bool
zink_nir_lower_b2b(
   nir_shader *shader
) {
   bool progress = false;
   bool condition_flags[1];
   const nir_shader_compiler_options *options = shader->options;
   const shader_info *info = &shader->info;
   (void) options;
   (void) info;

   STATIC_ASSERT(6 == ARRAY_SIZE(zink_nir_lower_b2b_values));
   condition_flags[0] = true;

   nir_foreach_function_impl(impl, shader) {
     progress |= nir_algebraic_impl(impl, condition_flags, &zink_nir_lower_b2b_table);
   }

   return progress;
}

