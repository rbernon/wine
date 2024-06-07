#include "vk_enum_to_str.h"
#include "zink_device_info.h"
#include "zink_screen.h"

bool
zink_get_physical_device_info(struct zink_screen *screen) 
{
   struct zink_device_info *info = &screen->info;
   bool support_KHR_maintenance1 = false;
   bool support_KHR_maintenance2 = false;
   bool support_KHR_maintenance3 = false;
   bool support_KHR_maintenance4 = false;
   bool support_KHR_maintenance5 = false;
   bool support_KHR_maintenance6 = false;
   bool support_KHR_external_memory = false;
   bool support_KHR_external_memory_fd = false;
   bool support_KHR_vulkan_memory_model = false;
   bool support_KHR_workgroup_memory_explicit_layout = false;
   bool support_KHR_pipeline_executable_properties = false;
   bool support_KHR_external_semaphore_fd = false;
   bool support_KHR_create_renderpass2 = false;
   bool support_KHR_synchronization2 = false;
   bool support_KHR_external_memory_win32 = false;
   bool support_KHR_external_semaphore_win32 = false;
   bool support_EXT_external_memory_dma_buf = false;
   bool support_KHR_buffer_device_address = false;
   bool support_EXT_external_memory_host = false;
   bool support_EXT_queue_family_foreign = false;
   bool support_KHR_swapchain_mutable_format = false;
   bool support_KHR_incremental_present = false;
   bool support_EXT_provoking_vertex = false;
   bool support_EXT_shader_viewport_index_layer = false;
   bool support_KHR_get_memory_requirements2 = false;
   bool support_EXT_post_depth_coverage = false;
   bool support_EXT_depth_clip_control = false;
   bool support_EXT_depth_clamp_zero_one = false;
   bool support_EXT_shader_subgroup_ballot = false;
   bool support_EXT_shader_subgroup_vote = false;
   bool support_EXT_shader_atomic_float = false;
   bool support_KHR_shader_atomic_int64 = false;
   bool support_KHR_8bit_storage = false;
   bool support_KHR_16bit_storage = false;
   bool support_EXT_image_2d_view_of_3d = false;
   bool support_KHR_driver_properties = false;
   bool support_EXT_memory_budget = false;
   bool support_EXT_memory_priority = false;
   bool support_EXT_pageable_device_local_memory = false;
   bool support_KHR_draw_indirect_count = false;
   bool support_EXT_dynamic_rendering_unused_attachments = false;
   bool support_EXT_shader_object = false;
   bool support_EXT_attachment_feedback_loop_layout = false;
   bool support_EXT_attachment_feedback_loop_dynamic_state = false;
   bool support_NV_device_generated_commands = false;
   bool support_EXT_fragment_shader_interlock = false;
   bool support_EXT_sample_locations = false;
   bool support_KHR_shader_draw_parameters = false;
   bool support_KHR_sampler_mirror_clamp_to_edge = false;
   bool support_EXT_descriptor_buffer = false;
   bool support_EXT_conditional_rendering = false;
   bool support_EXT_transform_feedback = false;
   bool support_EXT_index_type_uint8 = false;
   bool support_KHR_image_format_list = false;
   bool support_KHR_sampler_ycbcr_conversion = false;
   bool support_KHR_imageless_framebuffer = false;
   bool support_EXT_robustness2 = false;
   bool support_EXT_image_robustness = false;
   bool support_EXT_image_drm_format_modifier = false;
   bool support_EXT_vertex_attribute_divisor = false;
   bool support_EXT_calibrated_timestamps = false;
   bool support_NV_linear_color_attachment = false;
   bool support_KHR_dynamic_rendering = false;
   bool support_KHR_dynamic_rendering_local_read = false;
   bool support_EXT_multisampled_render_to_single_sampled = false;
   bool support_KHR_shader_clock = false;
   bool support_INTEL_shader_integer_functions2 = false;
   bool support_EXT_sampler_filter_minmax = false;
   bool support_EXT_custom_border_color = false;
   bool support_EXT_non_seamless_cube_map = false;
   bool support_EXT_border_color_swizzle = false;
   bool support_EXT_blend_operation_advanced = false;
   bool support_EXT_extended_dynamic_state = false;
   bool support_EXT_extended_dynamic_state2 = false;
   bool support_EXT_extended_dynamic_state3 = false;
   bool support_EXT_pipeline_creation_cache_control = false;
   bool support_EXT_shader_stencil_export = false;
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
   bool support_KHR_portability_subset = false;
#endif
   bool support_NV_compute_shader_derivatives = false;
   bool support_KHR_timeline_semaphore = false;
   bool support_EXT_color_write_enable = false;
   bool support_EXT_4444_formats = false;
   bool support_EXT_host_image_copy = false;
   bool support_EXT_scalar_block_layout = false;
   bool support_KHR_swapchain = false;
   bool support_EXT_rasterization_order_attachment_access = false;
   bool support_KHR_shader_float16_int8 = false;
   bool support_EXT_multi_draw = false;
   bool support_EXT_primitives_generated_query = false;
   bool support_KHR_pipeline_library = false;
   bool support_EXT_graphics_pipeline_library = false;
   bool support_KHR_push_descriptor = false;
   bool support_KHR_descriptor_update_template = false;
   bool support_EXT_line_rasterization = false;
   bool support_EXT_vertex_input_dynamic_state = false;
   bool support_EXT_primitive_topology_list_restart = false;
   bool support_KHR_dedicated_allocation = false;
   bool support_EXT_descriptor_indexing = false;
   bool support_EXT_depth_clip_enable = false;
   bool support_EXT_shader_demote_to_helper_invocation = false;
   bool support_KHR_shader_float_controls = false;
   bool support_KHR_format_feature_flags2 = false;
   uint32_t num_extensions = 0;

   // get device memory properties
   screen->vk.GetPhysicalDeviceMemoryProperties(screen->pdev, &info->mem_props);

   // enumerate device supported extensions
   VkResult result = screen->vk.EnumerateDeviceExtensionProperties(screen->pdev, NULL, &num_extensions, NULL);
   if (result != VK_SUCCESS) {
      if (!screen->implicitly_loaded)
         mesa_loge("ZINK: vkEnumerateDeviceExtensionProperties failed (%s)", vk_Result_to_str(result));
   } else {
      if (num_extensions > 0) {
         VkExtensionProperties *extensions = MALLOC(sizeof(VkExtensionProperties) * num_extensions);
         if (!extensions) goto fail;
         result = screen->vk.EnumerateDeviceExtensionProperties(screen->pdev, NULL, &num_extensions, extensions);
         if (result != VK_SUCCESS) {
            if (!screen->implicitly_loaded)
               mesa_loge("ZINK: vkEnumerateDeviceExtensionProperties failed (%s)", vk_Result_to_str(result));
         }

         for (uint32_t i = 0; i < num_extensions; ++i) {
            if (!strcmp(extensions[i].extensionName, "VK_KHR_maintenance1")) {
               info->have_KHR_maintenance1 = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_maintenance2")) {
               info->have_KHR_maintenance2 = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_maintenance3")) {
               info->have_KHR_maintenance3 = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_maintenance4")) {
               support_KHR_maintenance4 = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_maintenance5")) {
               support_KHR_maintenance5 = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_maintenance6")) {
               support_KHR_maintenance6 = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_external_memory")) {
               info->have_KHR_external_memory = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_external_memory_fd")) {
               info->have_KHR_external_memory_fd = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_vulkan_memory_model")) {
               info->have_KHR_vulkan_memory_model = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_workgroup_memory_explicit_layout")) {
               support_KHR_workgroup_memory_explicit_layout = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_pipeline_executable_properties")) {
               support_KHR_pipeline_executable_properties = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_external_semaphore_fd")) {
               info->have_KHR_external_semaphore_fd = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_create_renderpass2")) {
               info->have_KHR_create_renderpass2 = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_synchronization2")) {
               support_KHR_synchronization2 = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_external_memory_win32")) {
               info->have_KHR_external_memory_win32 = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_external_semaphore_win32")) {
               info->have_KHR_external_semaphore_win32 = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_external_memory_dma_buf")) {
               info->have_EXT_external_memory_dma_buf = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_buffer_device_address")) {
               support_KHR_buffer_device_address = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_external_memory_host")) {
               support_EXT_external_memory_host = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_queue_family_foreign")) {
               info->have_EXT_queue_family_foreign = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_swapchain_mutable_format")) {
               info->have_KHR_swapchain_mutable_format = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_incremental_present")) {
               info->have_KHR_incremental_present = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_provoking_vertex")) {
               support_EXT_provoking_vertex = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_shader_viewport_index_layer")) {
               info->have_EXT_shader_viewport_index_layer = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_get_memory_requirements2")) {
               info->have_KHR_get_memory_requirements2 = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_post_depth_coverage")) {
               info->have_EXT_post_depth_coverage = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_depth_clip_control")) {
               support_EXT_depth_clip_control = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_depth_clamp_zero_one")) {
               support_EXT_depth_clamp_zero_one = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_shader_subgroup_ballot")) {
               info->have_EXT_shader_subgroup_ballot = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_shader_subgroup_vote")) {
               info->have_EXT_shader_subgroup_vote = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_shader_atomic_float")) {
               support_EXT_shader_atomic_float = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_shader_atomic_int64")) {
               support_KHR_shader_atomic_int64 = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_8bit_storage")) {
               support_KHR_8bit_storage = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_16bit_storage")) {
               support_KHR_16bit_storage = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_image_2d_view_of_3d")) {
               support_EXT_image_2d_view_of_3d = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_driver_properties")) {
               support_KHR_driver_properties = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_memory_budget")) {
               info->have_EXT_memory_budget = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_memory_priority")) {
               support_EXT_memory_priority = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_pageable_device_local_memory")) {
               support_EXT_pageable_device_local_memory = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_draw_indirect_count")) {
               info->have_KHR_draw_indirect_count = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_dynamic_rendering_unused_attachments")) {
               support_EXT_dynamic_rendering_unused_attachments = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_shader_object")) {
               support_EXT_shader_object = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_attachment_feedback_loop_layout")) {
               support_EXT_attachment_feedback_loop_layout = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_attachment_feedback_loop_dynamic_state")) {
               support_EXT_attachment_feedback_loop_dynamic_state = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_NV_device_generated_commands")) {
               support_NV_device_generated_commands = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_fragment_shader_interlock")) {
               support_EXT_fragment_shader_interlock = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_sample_locations")) {
               support_EXT_sample_locations = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_shader_draw_parameters")) {
               info->have_KHR_shader_draw_parameters = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_sampler_mirror_clamp_to_edge")) {
               info->have_KHR_sampler_mirror_clamp_to_edge = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_descriptor_buffer")) {
               support_EXT_descriptor_buffer = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_conditional_rendering")) {
               support_EXT_conditional_rendering = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_transform_feedback")) {
               support_EXT_transform_feedback = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_index_type_uint8")) {
               support_EXT_index_type_uint8 = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_image_format_list")) {
               info->have_KHR_image_format_list = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_sampler_ycbcr_conversion")) {
               info->have_KHR_sampler_ycbcr_conversion = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_imageless_framebuffer")) {
               support_KHR_imageless_framebuffer = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_robustness2")) {
               support_EXT_robustness2 = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_image_robustness")) {
               support_EXT_image_robustness = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_image_drm_format_modifier")) {
               info->have_EXT_image_drm_format_modifier = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_vertex_attribute_divisor")) {
               support_EXT_vertex_attribute_divisor = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_calibrated_timestamps")) {
               info->have_EXT_calibrated_timestamps = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_NV_linear_color_attachment")) {
               support_NV_linear_color_attachment = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_dynamic_rendering")) {
               support_KHR_dynamic_rendering = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_dynamic_rendering_local_read")) {
               support_KHR_dynamic_rendering_local_read = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_multisampled_render_to_single_sampled")) {
               support_EXT_multisampled_render_to_single_sampled = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_shader_clock")) {
               support_KHR_shader_clock = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_INTEL_shader_integer_functions2")) {
               support_INTEL_shader_integer_functions2 = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_sampler_filter_minmax")) {
               support_EXT_sampler_filter_minmax = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_custom_border_color")) {
               support_EXT_custom_border_color = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_non_seamless_cube_map")) {
               support_EXT_non_seamless_cube_map = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_border_color_swizzle")) {
               support_EXT_border_color_swizzle = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_blend_operation_advanced")) {
               support_EXT_blend_operation_advanced = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_extended_dynamic_state")) {
               support_EXT_extended_dynamic_state = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_extended_dynamic_state2")) {
               support_EXT_extended_dynamic_state2 = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_extended_dynamic_state3")) {
               support_EXT_extended_dynamic_state3 = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_pipeline_creation_cache_control")) {
               support_EXT_pipeline_creation_cache_control = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_shader_stencil_export")) {
               info->have_EXT_shader_stencil_export = true;
            }
         #ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
   if (!strcmp(extensions[i].extensionName, "VK_KHR_portability_subset")) {
               support_KHR_portability_subset = true;
            }
#endif
            if (!strcmp(extensions[i].extensionName, "VK_NV_compute_shader_derivatives")) {
               support_NV_compute_shader_derivatives = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_timeline_semaphore")) {
               support_KHR_timeline_semaphore = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_color_write_enable")) {
               support_EXT_color_write_enable = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_4444_formats")) {
               support_EXT_4444_formats = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_host_image_copy")) {
               support_EXT_host_image_copy = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_scalar_block_layout")) {
               support_EXT_scalar_block_layout = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_swapchain")) {
               info->have_KHR_swapchain = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_rasterization_order_attachment_access")) {
               support_EXT_rasterization_order_attachment_access = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_shader_float16_int8")) {
               support_KHR_shader_float16_int8 = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_multi_draw")) {
               support_EXT_multi_draw = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_primitives_generated_query")) {
               support_EXT_primitives_generated_query = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_pipeline_library")) {
               info->have_KHR_pipeline_library = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_graphics_pipeline_library")) {
               support_EXT_graphics_pipeline_library = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_push_descriptor")) {
               support_KHR_push_descriptor = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_descriptor_update_template")) {
               info->have_KHR_descriptor_update_template = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_line_rasterization")) {
               support_EXT_line_rasterization = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_vertex_input_dynamic_state")) {
               support_EXT_vertex_input_dynamic_state = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_primitive_topology_list_restart")) {
               support_EXT_primitive_topology_list_restart = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_dedicated_allocation")) {
               info->have_KHR_dedicated_allocation = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_descriptor_indexing")) {
               support_EXT_descriptor_indexing = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_depth_clip_enable")) {
               support_EXT_depth_clip_enable = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_EXT_shader_demote_to_helper_invocation")) {
               support_EXT_shader_demote_to_helper_invocation = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_shader_float_controls")) {
               info->have_KHR_shader_float_controls = true;
            }
            if (!strcmp(extensions[i].extensionName, "VK_KHR_format_feature_flags2")) {
               info->have_KHR_format_feature_flags2 = true;
            }
         }

         FREE(extensions);
      }
   }

   // get device features
   if (screen->vk.GetPhysicalDeviceFeatures2) {
      // check for device extension features
      info->feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

      if (VK_MAKE_VERSION(1,2,0) <= screen->vk_version) {
         /* VkPhysicalDeviceVulkan11Features was added in 1.2, not 1.1 as one would think */
         info->feats11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
         info->feats11.pNext = info->feats.pNext;
         info->feats.pNext = &info->feats11;
         info->have_vulkan11 = true;
      }
      if (VK_MAKE_VERSION(1,2,0) <= screen->vk_version) {
         info->feats12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
         info->feats12.pNext = info->feats.pNext;
         info->feats.pNext = &info->feats12;
         info->have_vulkan12 = true;
      }
      if (VK_MAKE_VERSION(1,3,0) <= screen->vk_version) {
         info->feats13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
         info->feats13.pNext = info->feats.pNext;
         info->feats.pNext = &info->feats13;
         info->have_vulkan13 = true;
      }

   if (support_KHR_maintenance4 && !info->have_vulkan13) {
         info->maint4_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES_KHR;
         info->maint4_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->maint4_feats;
      }
   if (support_KHR_maintenance5) {
         info->maint5_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR;
         info->maint5_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->maint5_feats;
      }
   if (support_KHR_maintenance6) {
         info->maint6_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_FEATURES_KHR;
         info->maint6_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->maint6_feats;
      }
   if (support_KHR_workgroup_memory_explicit_layout) {
         info->explicit_layout_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_WORKGROUP_MEMORY_EXPLICIT_LAYOUT_FEATURES_KHR;
         info->explicit_layout_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->explicit_layout_feats;
      }
   if (support_KHR_pipeline_executable_properties) {
         info->pipestats_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_EXECUTABLE_PROPERTIES_FEATURES_KHR;
         info->pipestats_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->pipestats_feats;
      }
   if (support_KHR_synchronization2 && !info->have_vulkan13) {
         info->sync2_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;
         info->sync2_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->sync2_feats;
      }
   if (support_KHR_buffer_device_address && !info->have_vulkan12) {
         info->bda_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR;
         info->bda_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->bda_feats;
      }
   if (support_EXT_provoking_vertex) {
         info->pv_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_FEATURES_EXT;
         info->pv_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->pv_feats;
      }
   if (support_EXT_depth_clip_control) {
         info->clip_control_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_CONTROL_FEATURES_EXT;
         info->clip_control_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->clip_control_feats;
      }
   if (support_EXT_depth_clamp_zero_one) {
         info->clamp_01_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLAMP_ZERO_ONE_FEATURES_EXT;
         info->clamp_01_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->clamp_01_feats;
      }
   if (support_EXT_shader_atomic_float) {
         info->atomic_float_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT;
         info->atomic_float_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->atomic_float_feats;
      }
   if (support_KHR_shader_atomic_int64 && !info->have_vulkan12) {
         info->atomic_int_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES_KHR;
         info->atomic_int_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->atomic_int_feats;
      }
   if (support_KHR_8bit_storage && !info->have_vulkan12) {
         info->storage_8bit_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR;
         info->storage_8bit_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->storage_8bit_feats;
      }
   if (support_KHR_16bit_storage && !info->have_vulkan11) {
         info->storage_16bit_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR;
         info->storage_16bit_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->storage_16bit_feats;
      }
   if (support_EXT_image_2d_view_of_3d) {
         info->view2d_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_2D_VIEW_OF_3D_FEATURES_EXT;
         info->view2d_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->view2d_feats;
      }
   if (support_EXT_memory_priority) {
         info->memprio_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT;
         info->memprio_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->memprio_feats;
      }
   if (support_EXT_pageable_device_local_memory) {
         info->mempage_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT;
         info->mempage_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->mempage_feats;
      }
   if (support_EXT_dynamic_rendering_unused_attachments) {
         info->unused_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_FEATURES_EXT;
         info->unused_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->unused_feats;
      }
   if (support_EXT_shader_object) {
         info->shobj_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT;
         info->shobj_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->shobj_feats;
      }
   if (support_EXT_attachment_feedback_loop_layout) {
         info->feedback_loop_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_FEATURES_EXT;
         info->feedback_loop_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->feedback_loop_feats;
      }
   if (support_EXT_attachment_feedback_loop_dynamic_state) {
         info->feedback_dyn_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_DYNAMIC_STATE_FEATURES_EXT;
         info->feedback_dyn_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->feedback_dyn_feats;
      }
   if (support_NV_device_generated_commands) {
         info->nv_dgc_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_FEATURES_NV;
         info->nv_dgc_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->nv_dgc_feats;
      }
   if (support_EXT_fragment_shader_interlock) {
         info->interlock_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT;
         info->interlock_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->interlock_feats;
      }
   if (support_EXT_descriptor_buffer) {
         info->db_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
         info->db_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->db_feats;
      }
   if (support_EXT_conditional_rendering) {
         info->cond_render_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT;
         info->cond_render_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->cond_render_feats;
      }
   if (support_EXT_transform_feedback) {
         info->tf_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT;
         info->tf_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->tf_feats;
      }
   if (support_EXT_index_type_uint8) {
         info->index_uint8_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT;
         info->index_uint8_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->index_uint8_feats;
      }
   if (support_KHR_imageless_framebuffer && !info->have_vulkan12) {
         info->imgless_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES_KHR;
         info->imgless_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->imgless_feats;
      }
   if (support_EXT_robustness2) {
         info->rb2_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
         info->rb2_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->rb2_feats;
      }
   if (support_EXT_image_robustness && !info->have_vulkan13) {
         info->rb_image_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES_EXT;
         info->rb_image_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->rb_image_feats;
      }
   if (support_EXT_vertex_attribute_divisor) {
         info->vdiv_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT;
         info->vdiv_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->vdiv_feats;
      }
   if (support_NV_linear_color_attachment) {
         info->linear_color_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINEAR_COLOR_ATTACHMENT_FEATURES_NV;
         info->linear_color_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->linear_color_feats;
      }
   if (support_KHR_dynamic_rendering && !info->have_vulkan13) {
         info->dynamic_render_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
         info->dynamic_render_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->dynamic_render_feats;
      }
   if (support_KHR_dynamic_rendering_local_read) {
         info->drlr_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_LOCAL_READ_FEATURES_KHR;
         info->drlr_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->drlr_feats;
      }
   if (support_EXT_multisampled_render_to_single_sampled) {
         info->msrtss_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_FEATURES_EXT;
         info->msrtss_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->msrtss_feats;
      }
   if (support_KHR_shader_clock) {
         info->shader_clock_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR;
         info->shader_clock_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->shader_clock_feats;
      }
   if (support_INTEL_shader_integer_functions2) {
         info->shader_int_fns2_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_FUNCTIONS_2_FEATURES_INTEL;
         info->shader_int_fns2_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->shader_int_fns2_feats;
      }
   if (support_EXT_custom_border_color) {
         info->border_color_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT;
         info->border_color_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->border_color_feats;
      }
   if (support_EXT_non_seamless_cube_map) {
         info->nonseamless_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_NON_SEAMLESS_CUBE_MAP_FEATURES_EXT;
         info->nonseamless_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->nonseamless_feats;
      }
   if (support_EXT_border_color_swizzle) {
         info->border_swizzle_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BORDER_COLOR_SWIZZLE_FEATURES_EXT;
         info->border_swizzle_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->border_swizzle_feats;
      }
   if (support_EXT_extended_dynamic_state) {
         info->dynamic_state_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT;
         info->dynamic_state_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->dynamic_state_feats;
      }
   if (support_EXT_extended_dynamic_state2) {
         info->dynamic_state2_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT;
         info->dynamic_state2_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->dynamic_state2_feats;
      }
   if (support_EXT_extended_dynamic_state3) {
         info->dynamic_state3_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT;
         info->dynamic_state3_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->dynamic_state3_feats;
      }
   if (support_EXT_pipeline_creation_cache_control && !info->have_vulkan13) {
         info->pipeline_cache_control_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_CREATION_CACHE_CONTROL_FEATURES_EXT;
         info->pipeline_cache_control_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->pipeline_cache_control_feats;
      }
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
   if (support_KHR_portability_subset) {
         info->portability_subset_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR;
         info->portability_subset_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->portability_subset_feats;
      }
#endif
   if (support_NV_compute_shader_derivatives) {
         info->shader_derivs_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_NV;
         info->shader_derivs_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->shader_derivs_feats;
      }
   if (support_KHR_timeline_semaphore && !info->have_vulkan12) {
         info->timeline_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR;
         info->timeline_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->timeline_feats;
      }
   if (support_EXT_color_write_enable) {
         info->cwrite_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COLOR_WRITE_ENABLE_FEATURES_EXT;
         info->cwrite_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->cwrite_feats;
      }
   if (support_EXT_4444_formats) {
         info->format_4444_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_4444_FORMATS_FEATURES_EXT;
         info->format_4444_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->format_4444_feats;
      }
   if (support_EXT_host_image_copy) {
         info->hic_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_FEATURES_EXT;
         info->hic_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->hic_feats;
      }
   if (support_EXT_scalar_block_layout && !info->have_vulkan12) {
         info->scalar_block_layout_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT;
         info->scalar_block_layout_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->scalar_block_layout_feats;
      }
   if (support_EXT_rasterization_order_attachment_access) {
         info->rast_order_access_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_FEATURES_EXT;
         info->rast_order_access_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->rast_order_access_feats;
      }
   if (support_KHR_shader_float16_int8 && !info->have_vulkan12) {
         info->shader_float16_int8_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR;
         info->shader_float16_int8_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->shader_float16_int8_feats;
      }
   if (support_EXT_multi_draw) {
         info->multidraw_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTI_DRAW_FEATURES_EXT;
         info->multidraw_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->multidraw_feats;
      }
   if (support_EXT_primitives_generated_query) {
         info->primgen_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVES_GENERATED_QUERY_FEATURES_EXT;
         info->primgen_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->primgen_feats;
      }
   if (support_EXT_graphics_pipeline_library) {
         info->gpl_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT;
         info->gpl_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->gpl_feats;
      }
   if (support_EXT_line_rasterization) {
         info->line_rast_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT;
         info->line_rast_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->line_rast_feats;
      }
   if (support_EXT_vertex_input_dynamic_state) {
         info->vertex_input_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT;
         info->vertex_input_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->vertex_input_feats;
      }
   if (support_EXT_primitive_topology_list_restart) {
         info->list_restart_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVE_TOPOLOGY_LIST_RESTART_FEATURES_EXT;
         info->list_restart_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->list_restart_feats;
      }
   if (support_EXT_descriptor_indexing && !info->have_vulkan12) {
         info->desc_indexing_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
         info->desc_indexing_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->desc_indexing_feats;
      }
   if (support_EXT_depth_clip_enable) {
         info->depth_clip_enable_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT;
         info->depth_clip_enable_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->depth_clip_enable_feats;
      }
   if (support_EXT_shader_demote_to_helper_invocation && !info->have_vulkan13) {
         info->demote_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES_EXT;
         info->demote_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->demote_feats;
      }

      screen->vk.GetPhysicalDeviceFeatures2(screen->pdev, &info->feats);
   } else {
      screen->vk.GetPhysicalDeviceFeatures(screen->pdev, &info->feats.features);
   }

   // check for device properties
   if (screen->vk.GetPhysicalDeviceProperties2) {
      VkPhysicalDeviceProperties2 props = {0};
      props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

      if (VK_MAKE_VERSION(1,2,0) <= screen->vk_version) {
         /* VkPhysicalDeviceVulkan11Properties was added in 1.2, not 1.1 as one would think */
         info->props11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
         info->props11.pNext = props.pNext;
         props.pNext = &info->props11;
      }
      if (VK_MAKE_VERSION(1,2,0) <= screen->vk_version) {
         info->props12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
         info->props12.pNext = props.pNext;
         props.pNext = &info->props12;
      }
      if (VK_MAKE_VERSION(1,3,0) <= screen->vk_version) {
         info->props13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES;
         info->props13.pNext = props.pNext;
         props.pNext = &info->props13;
      }

   if (support_KHR_maintenance5) {
         info->maint5_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_PROPERTIES_KHR;
         info->maint5_props.pNext = props.pNext;
         props.pNext = &info->maint5_props;
      }
   if (support_KHR_maintenance6) {
         info->maint6_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_PROPERTIES_KHR;
         info->maint6_props.pNext = props.pNext;
         props.pNext = &info->maint6_props;
      }
   if (support_EXT_external_memory_host) {
         info->ext_host_mem_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT;
         info->ext_host_mem_props.pNext = props.pNext;
         props.pNext = &info->ext_host_mem_props;
      }
   if (support_EXT_provoking_vertex) {
         info->pv_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_PROPERTIES_EXT;
         info->pv_props.pNext = props.pNext;
         props.pNext = &info->pv_props;
      }
   if (support_KHR_driver_properties && !info->have_vulkan12) {
         info->driver_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES_KHR;
         info->driver_props.pNext = props.pNext;
         props.pNext = &info->driver_props;
      }
   if (support_EXT_shader_object) {
         info->shobj_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_PROPERTIES_EXT;
         info->shobj_props.pNext = props.pNext;
         props.pNext = &info->shobj_props;
      }
   if (support_NV_device_generated_commands) {
         info->nv_dgc_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_PROPERTIES_NV;
         info->nv_dgc_props.pNext = props.pNext;
         props.pNext = &info->nv_dgc_props;
      }
   if (support_EXT_sample_locations) {
         info->sample_locations_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLE_LOCATIONS_PROPERTIES_EXT;
         info->sample_locations_props.pNext = props.pNext;
         props.pNext = &info->sample_locations_props;
      }
   if (support_EXT_descriptor_buffer) {
         info->db_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT;
         info->db_props.pNext = props.pNext;
         props.pNext = &info->db_props;
      }
   if (support_EXT_transform_feedback) {
         info->tf_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT;
         info->tf_props.pNext = props.pNext;
         props.pNext = &info->tf_props;
      }
   if (support_EXT_robustness2) {
         info->rb2_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_PROPERTIES_EXT;
         info->rb2_props.pNext = props.pNext;
         props.pNext = &info->rb2_props;
      }
   if (support_EXT_vertex_attribute_divisor) {
         info->vdiv_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_PROPERTIES_EXT;
         info->vdiv_props.pNext = props.pNext;
         props.pNext = &info->vdiv_props;
      }
   if (support_EXT_sampler_filter_minmax && !info->have_vulkan12) {
         info->reduction_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES_EXT;
         info->reduction_props.pNext = props.pNext;
         props.pNext = &info->reduction_props;
      }
   if (support_EXT_custom_border_color) {
         info->border_color_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_PROPERTIES_EXT;
         info->border_color_props.pNext = props.pNext;
         props.pNext = &info->border_color_props;
      }
   if (support_EXT_blend_operation_advanced) {
         info->blend_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_PROPERTIES_EXT;
         info->blend_props.pNext = props.pNext;
         props.pNext = &info->blend_props;
      }
   if (support_EXT_extended_dynamic_state3) {
         info->dynamic_state3_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_PROPERTIES_EXT;
         info->dynamic_state3_props.pNext = props.pNext;
         props.pNext = &info->dynamic_state3_props;
      }
   if (support_EXT_host_image_copy) {
         info->hic_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_PROPERTIES_EXT;
         info->hic_props.pNext = props.pNext;
         props.pNext = &info->hic_props;
      }
   if (support_EXT_multi_draw) {
         info->multidraw_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTI_DRAW_PROPERTIES_EXT;
         info->multidraw_props.pNext = props.pNext;
         props.pNext = &info->multidraw_props;
      }
   if (support_EXT_graphics_pipeline_library) {
         info->gpl_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_PROPERTIES_EXT;
         info->gpl_props.pNext = props.pNext;
         props.pNext = &info->gpl_props;
      }
   if (support_KHR_push_descriptor) {
         info->push_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR;
         info->push_props.pNext = props.pNext;
         props.pNext = &info->push_props;
      }
   if (support_EXT_line_rasterization) {
         info->line_rast_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_PROPERTIES_EXT;
         info->line_rast_props.pNext = props.pNext;
         props.pNext = &info->line_rast_props;
      }
   if (support_EXT_descriptor_indexing && !info->have_vulkan12) {
         info->desc_indexing_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES_EXT;
         info->desc_indexing_props.pNext = props.pNext;
         props.pNext = &info->desc_indexing_props;
      }

      if (screen->vk_version < VK_MAKE_VERSION(1,2,0) && screen->instance_info.have_KHR_external_memory_capabilities) {
         info->deviceid_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
         info->deviceid_props.pNext = props.pNext;
         props.pNext = &info->deviceid_props;
      }

      if (screen->vk_version >= VK_MAKE_VERSION(1,1,0)) {
         info->subgroup.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
         info->subgroup.pNext = props.pNext;
         props.pNext = &info->subgroup;
      }

      // note: setting up local VkPhysicalDeviceProperties2.
      screen->vk.GetPhysicalDeviceProperties2(screen->pdev, &props);
   }

   /* We re-apply the fields from VkPhysicalDeviceVulkanXYFeatures struct
    * onto their respective fields in the VkPhysicalDeviceExtensionNameFeatures
    * struct if the former is provided by the VK implementation.
    *
    * As for why this is done: the spec mentions that once an extension is
    * promoted to core and its feature fields are added in VulkanXYFeatures,
    * including both ExtensionNameFeatures and VulkanXYFeatures at the same
    * time is prohibited when using vkGetPhysicalDeviceFeatures2.
    */
   if (info->have_vulkan13) {
      info->maint4_feats.maintenance4 = info->feats13.maintenance4;
   }
   if (info->have_vulkan13) {
      info->sync2_feats.synchronization2 = info->feats13.synchronization2;
   }
   if (info->have_vulkan12) {
      info->bda_feats.bufferDeviceAddress = info->feats12.bufferDeviceAddress;
      info->bda_feats.bufferDeviceAddressCaptureReplay = info->feats12.bufferDeviceAddressCaptureReplay;
      info->bda_feats.bufferDeviceAddressMultiDevice = info->feats12.bufferDeviceAddressMultiDevice;
   }
   if (info->have_vulkan12) {
      info->atomic_int_feats.shaderBufferInt64Atomics = info->feats12.shaderBufferInt64Atomics;
      info->atomic_int_feats.shaderSharedInt64Atomics = info->feats12.shaderSharedInt64Atomics;
   }
   if (info->have_vulkan12) {
      info->storage_8bit_feats.storageBuffer8BitAccess = info->feats12.storageBuffer8BitAccess;
      info->storage_8bit_feats.uniformAndStorageBuffer8BitAccess = info->feats12.uniformAndStorageBuffer8BitAccess;
      info->storage_8bit_feats.storagePushConstant8 = info->feats12.storagePushConstant8;
   }
   if (info->have_vulkan11) {
      info->storage_16bit_feats.storageBuffer16BitAccess = info->feats11.storageBuffer16BitAccess;
      info->storage_16bit_feats.uniformAndStorageBuffer16BitAccess = info->feats11.uniformAndStorageBuffer16BitAccess;
      info->storage_16bit_feats.storagePushConstant16 = info->feats11.storagePushConstant16;
      info->storage_16bit_feats.storageInputOutput16 = info->feats11.storageInputOutput16;
   }
   if (info->have_vulkan12) {
      info->imgless_feats.imagelessFramebuffer = info->feats12.imagelessFramebuffer;
   }
   if (info->have_vulkan13) {
      info->rb_image_feats.robustImageAccess = info->feats13.robustImageAccess;
   }
   if (info->have_vulkan13) {
      info->dynamic_render_feats.dynamicRendering = info->feats13.dynamicRendering;
   }
   if (info->have_vulkan13) {
      info->pipeline_cache_control_feats.pipelineCreationCacheControl = info->feats13.pipelineCreationCacheControl;
   }
   if (info->have_vulkan12) {
      info->timeline_feats.timelineSemaphore = info->feats12.timelineSemaphore;
   }
   if (info->have_vulkan12) {
      info->scalar_block_layout_feats.scalarBlockLayout = info->feats12.scalarBlockLayout;
   }
   if (info->have_vulkan12) {
      info->shader_float16_int8_feats.shaderFloat16 = info->feats12.shaderFloat16;
      info->shader_float16_int8_feats.shaderInt8 = info->feats12.shaderInt8;
   }
   if (info->have_vulkan12) {
      info->desc_indexing_feats.shaderInputAttachmentArrayDynamicIndexing = info->feats12.shaderInputAttachmentArrayDynamicIndexing;
      info->desc_indexing_feats.shaderUniformTexelBufferArrayDynamicIndexing = info->feats12.shaderUniformTexelBufferArrayDynamicIndexing;
      info->desc_indexing_feats.shaderStorageTexelBufferArrayDynamicIndexing = info->feats12.shaderStorageTexelBufferArrayDynamicIndexing;
      info->desc_indexing_feats.shaderUniformBufferArrayNonUniformIndexing = info->feats12.shaderUniformBufferArrayNonUniformIndexing;
      info->desc_indexing_feats.shaderSampledImageArrayNonUniformIndexing = info->feats12.shaderSampledImageArrayNonUniformIndexing;
      info->desc_indexing_feats.shaderStorageBufferArrayNonUniformIndexing = info->feats12.shaderStorageBufferArrayNonUniformIndexing;
      info->desc_indexing_feats.shaderStorageImageArrayNonUniformIndexing = info->feats12.shaderStorageImageArrayNonUniformIndexing;
      info->desc_indexing_feats.shaderInputAttachmentArrayNonUniformIndexing = info->feats12.shaderInputAttachmentArrayNonUniformIndexing;
      info->desc_indexing_feats.shaderUniformTexelBufferArrayNonUniformIndexing = info->feats12.shaderUniformTexelBufferArrayNonUniformIndexing;
      info->desc_indexing_feats.shaderStorageTexelBufferArrayNonUniformIndexing = info->feats12.shaderStorageTexelBufferArrayNonUniformIndexing;
      info->desc_indexing_feats.descriptorBindingUniformBufferUpdateAfterBind = info->feats12.descriptorBindingUniformBufferUpdateAfterBind;
      info->desc_indexing_feats.descriptorBindingSampledImageUpdateAfterBind = info->feats12.descriptorBindingSampledImageUpdateAfterBind;
      info->desc_indexing_feats.descriptorBindingStorageImageUpdateAfterBind = info->feats12.descriptorBindingStorageImageUpdateAfterBind;
      info->desc_indexing_feats.descriptorBindingStorageBufferUpdateAfterBind = info->feats12.descriptorBindingStorageBufferUpdateAfterBind;
      info->desc_indexing_feats.descriptorBindingUniformTexelBufferUpdateAfterBind = info->feats12.descriptorBindingUniformTexelBufferUpdateAfterBind;
      info->desc_indexing_feats.descriptorBindingStorageTexelBufferUpdateAfterBind = info->feats12.descriptorBindingStorageTexelBufferUpdateAfterBind;
      info->desc_indexing_feats.descriptorBindingUpdateUnusedWhilePending = info->feats12.descriptorBindingUpdateUnusedWhilePending;
      info->desc_indexing_feats.descriptorBindingPartiallyBound = info->feats12.descriptorBindingPartiallyBound;
      info->desc_indexing_feats.descriptorBindingVariableDescriptorCount = info->feats12.descriptorBindingVariableDescriptorCount;
      info->desc_indexing_feats.runtimeDescriptorArray = info->feats12.runtimeDescriptorArray;
   }
   if (info->have_vulkan13) {
      info->demote_feats.shaderDemoteToHelperInvocation = info->feats13.shaderDemoteToHelperInvocation;
   }

   /* See above, but for VulkanXYProperties.
    * Unlike VulkanXYFeatures with all the booleans, VulkanXYProperties can
    * contain different types of data, including arrays. The C language hates us
    * when we assign an array to another array, therefore we use an memcpy here.
    */
   if (info->have_vulkan12) {
      memcpy(&info->driver_props.driverID,
             &info->props12.driverID,
             sizeof(info->driver_props.driverID));
      memcpy(&info->driver_props.driverName,
             &info->props12.driverName,
             sizeof(info->driver_props.driverName));
      memcpy(&info->driver_props.driverInfo,
             &info->props12.driverInfo,
             sizeof(info->driver_props.driverInfo));
      memcpy(&info->driver_props.conformanceVersion,
             &info->props12.conformanceVersion,
             sizeof(info->driver_props.conformanceVersion));
   }
   if (info->have_vulkan12) {
      memcpy(&info->reduction_props.filterMinmaxSingleComponentFormats,
             &info->props12.filterMinmaxSingleComponentFormats,
             sizeof(info->reduction_props.filterMinmaxSingleComponentFormats));
      memcpy(&info->reduction_props.filterMinmaxImageComponentMapping,
             &info->props12.filterMinmaxImageComponentMapping,
             sizeof(info->reduction_props.filterMinmaxImageComponentMapping));
   }
   if (info->have_vulkan12) {
      memcpy(&info->desc_indexing_props.maxUpdateAfterBindDescriptorsInAllPools,
             &info->props12.maxUpdateAfterBindDescriptorsInAllPools,
             sizeof(info->desc_indexing_props.maxUpdateAfterBindDescriptorsInAllPools));
      memcpy(&info->desc_indexing_props.shaderUniformBufferArrayNonUniformIndexingNative,
             &info->props12.shaderUniformBufferArrayNonUniformIndexingNative,
             sizeof(info->desc_indexing_props.shaderUniformBufferArrayNonUniformIndexingNative));
      memcpy(&info->desc_indexing_props.shaderSampledImageArrayNonUniformIndexingNative,
             &info->props12.shaderSampledImageArrayNonUniformIndexingNative,
             sizeof(info->desc_indexing_props.shaderSampledImageArrayNonUniformIndexingNative));
      memcpy(&info->desc_indexing_props.shaderStorageBufferArrayNonUniformIndexingNative,
             &info->props12.shaderStorageBufferArrayNonUniformIndexingNative,
             sizeof(info->desc_indexing_props.shaderStorageBufferArrayNonUniformIndexingNative));
      memcpy(&info->desc_indexing_props.shaderStorageImageArrayNonUniformIndexingNative,
             &info->props12.shaderStorageImageArrayNonUniformIndexingNative,
             sizeof(info->desc_indexing_props.shaderStorageImageArrayNonUniformIndexingNative));
      memcpy(&info->desc_indexing_props.shaderInputAttachmentArrayNonUniformIndexingNative,
             &info->props12.shaderInputAttachmentArrayNonUniformIndexingNative,
             sizeof(info->desc_indexing_props.shaderInputAttachmentArrayNonUniformIndexingNative));
      memcpy(&info->desc_indexing_props.robustBufferAccessUpdateAfterBind,
             &info->props12.robustBufferAccessUpdateAfterBind,
             sizeof(info->desc_indexing_props.robustBufferAccessUpdateAfterBind));
      memcpy(&info->desc_indexing_props.quadDivergentImplicitLod,
             &info->props12.quadDivergentImplicitLod,
             sizeof(info->desc_indexing_props.quadDivergentImplicitLod));
      memcpy(&info->desc_indexing_props.maxPerStageDescriptorUpdateAfterBindSamplers,
             &info->props12.maxPerStageDescriptorUpdateAfterBindSamplers,
             sizeof(info->desc_indexing_props.maxPerStageDescriptorUpdateAfterBindSamplers));
      memcpy(&info->desc_indexing_props.maxPerStageDescriptorUpdateAfterBindUniformBuffers,
             &info->props12.maxPerStageDescriptorUpdateAfterBindUniformBuffers,
             sizeof(info->desc_indexing_props.maxPerStageDescriptorUpdateAfterBindUniformBuffers));
      memcpy(&info->desc_indexing_props.maxPerStageDescriptorUpdateAfterBindStorageBuffers,
             &info->props12.maxPerStageDescriptorUpdateAfterBindStorageBuffers,
             sizeof(info->desc_indexing_props.maxPerStageDescriptorUpdateAfterBindStorageBuffers));
      memcpy(&info->desc_indexing_props.maxPerStageDescriptorUpdateAfterBindSampledImages,
             &info->props12.maxPerStageDescriptorUpdateAfterBindSampledImages,
             sizeof(info->desc_indexing_props.maxPerStageDescriptorUpdateAfterBindSampledImages));
      memcpy(&info->desc_indexing_props.maxPerStageDescriptorUpdateAfterBindStorageImages,
             &info->props12.maxPerStageDescriptorUpdateAfterBindStorageImages,
             sizeof(info->desc_indexing_props.maxPerStageDescriptorUpdateAfterBindStorageImages));
      memcpy(&info->desc_indexing_props.maxPerStageDescriptorUpdateAfterBindInputAttachments,
             &info->props12.maxPerStageDescriptorUpdateAfterBindInputAttachments,
             sizeof(info->desc_indexing_props.maxPerStageDescriptorUpdateAfterBindInputAttachments));
      memcpy(&info->desc_indexing_props.maxPerStageUpdateAfterBindResources,
             &info->props12.maxPerStageUpdateAfterBindResources,
             sizeof(info->desc_indexing_props.maxPerStageUpdateAfterBindResources));
      memcpy(&info->desc_indexing_props.maxDescriptorSetUpdateAfterBindSamplers,
             &info->props12.maxDescriptorSetUpdateAfterBindSamplers,
             sizeof(info->desc_indexing_props.maxDescriptorSetUpdateAfterBindSamplers));
      memcpy(&info->desc_indexing_props.maxDescriptorSetUpdateAfterBindUniformBuffers,
             &info->props12.maxDescriptorSetUpdateAfterBindUniformBuffers,
             sizeof(info->desc_indexing_props.maxDescriptorSetUpdateAfterBindUniformBuffers));
      memcpy(&info->desc_indexing_props.maxDescriptorSetUpdateAfterBindUniformBuffersDynamic,
             &info->props12.maxDescriptorSetUpdateAfterBindUniformBuffersDynamic,
             sizeof(info->desc_indexing_props.maxDescriptorSetUpdateAfterBindUniformBuffersDynamic));
      memcpy(&info->desc_indexing_props.maxDescriptorSetUpdateAfterBindStorageBuffers,
             &info->props12.maxDescriptorSetUpdateAfterBindStorageBuffers,
             sizeof(info->desc_indexing_props.maxDescriptorSetUpdateAfterBindStorageBuffers));
      memcpy(&info->desc_indexing_props.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic,
             &info->props12.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic,
             sizeof(info->desc_indexing_props.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic));
      memcpy(&info->desc_indexing_props.maxDescriptorSetUpdateAfterBindSampledImages,
             &info->props12.maxDescriptorSetUpdateAfterBindSampledImages,
             sizeof(info->desc_indexing_props.maxDescriptorSetUpdateAfterBindSampledImages));
      memcpy(&info->desc_indexing_props.maxDescriptorSetUpdateAfterBindStorageImages,
             &info->props12.maxDescriptorSetUpdateAfterBindStorageImages,
             sizeof(info->desc_indexing_props.maxDescriptorSetUpdateAfterBindStorageImages));
      memcpy(&info->desc_indexing_props.maxDescriptorSetUpdateAfterBindInputAttachments,
             &info->props12.maxDescriptorSetUpdateAfterBindInputAttachments,
             sizeof(info->desc_indexing_props.maxDescriptorSetUpdateAfterBindInputAttachments));
   }

   // enable the extensions if they match the conditions given by ext.enable_conds 
   if (screen->vk.GetPhysicalDeviceProperties2) {
   info->have_KHR_maintenance1 |= support_KHR_maintenance1
         ;
   info->have_KHR_maintenance2 |= support_KHR_maintenance2
         ;
   info->have_KHR_maintenance3 |= support_KHR_maintenance3
         ;
   info->have_KHR_maintenance4 |= support_KHR_maintenance4
         ;
   info->have_KHR_maintenance5 |= support_KHR_maintenance5
         ;
   info->have_KHR_maintenance6 |= support_KHR_maintenance6
         ;
   info->have_KHR_external_memory |= support_KHR_external_memory
         ;
   info->have_KHR_external_memory_fd |= support_KHR_external_memory_fd
         ;
   info->have_KHR_vulkan_memory_model |= support_KHR_vulkan_memory_model
         ;
   info->have_KHR_workgroup_memory_explicit_layout |= support_KHR_workgroup_memory_explicit_layout
         ;
   info->have_KHR_pipeline_executable_properties |= support_KHR_pipeline_executable_properties
         ;
   info->have_KHR_external_semaphore_fd |= support_KHR_external_semaphore_fd
         ;
   info->have_KHR_create_renderpass2 |= support_KHR_create_renderpass2
         ;
   info->have_KHR_synchronization2 |= support_KHR_synchronization2
         ;
   info->have_KHR_external_memory_win32 |= support_KHR_external_memory_win32
         ;
   info->have_KHR_external_semaphore_win32 |= support_KHR_external_semaphore_win32
         ;
   info->have_EXT_external_memory_dma_buf |= support_EXT_external_memory_dma_buf
         ;
   info->have_KHR_buffer_device_address |= support_KHR_buffer_device_address
         ;
   info->have_EXT_external_memory_host |= support_EXT_external_memory_host
         ;
   info->have_EXT_queue_family_foreign |= support_EXT_queue_family_foreign
         ;
   info->have_KHR_swapchain_mutable_format |= support_KHR_swapchain_mutable_format
         ;
   info->have_KHR_incremental_present |= support_KHR_incremental_present
         ;
   info->have_EXT_provoking_vertex |= support_EXT_provoking_vertex
         && (info->pv_feats.provokingVertexLast);
   info->have_EXT_shader_viewport_index_layer |= support_EXT_shader_viewport_index_layer
         ;
   info->have_KHR_get_memory_requirements2 |= support_KHR_get_memory_requirements2
         ;
   info->have_EXT_post_depth_coverage |= support_EXT_post_depth_coverage
         ;
   info->have_EXT_depth_clip_control |= support_EXT_depth_clip_control
         ;
   info->have_EXT_depth_clamp_zero_one |= support_EXT_depth_clamp_zero_one
         ;
   info->have_EXT_shader_subgroup_ballot |= support_EXT_shader_subgroup_ballot
         ;
   info->have_EXT_shader_subgroup_vote |= support_EXT_shader_subgroup_vote
         ;
   info->have_EXT_shader_atomic_float |= support_EXT_shader_atomic_float
         ;
   info->have_KHR_shader_atomic_int64 |= support_KHR_shader_atomic_int64
         ;
   info->have_KHR_8bit_storage |= support_KHR_8bit_storage
         && (info->storage_8bit_feats.storageBuffer8BitAccess);
   info->have_KHR_16bit_storage |= support_KHR_16bit_storage
         && (info->storage_16bit_feats.storageBuffer16BitAccess);
   info->have_EXT_image_2d_view_of_3d |= support_EXT_image_2d_view_of_3d
         ;
   info->have_KHR_driver_properties |= support_KHR_driver_properties
         ;
   info->have_EXT_memory_budget |= support_EXT_memory_budget
         ;
   info->have_EXT_memory_priority |= support_EXT_memory_priority
         ;
   info->have_EXT_pageable_device_local_memory |= support_EXT_pageable_device_local_memory
         ;
   info->have_KHR_draw_indirect_count |= support_KHR_draw_indirect_count
         ;
   info->have_EXT_dynamic_rendering_unused_attachments |= support_EXT_dynamic_rendering_unused_attachments
         ;
   info->have_EXT_shader_object |= support_EXT_shader_object
         ;
   info->have_EXT_attachment_feedback_loop_layout |= support_EXT_attachment_feedback_loop_layout
         ;
   info->have_EXT_attachment_feedback_loop_dynamic_state |= support_EXT_attachment_feedback_loop_dynamic_state
         ;
   info->have_NV_device_generated_commands |= support_NV_device_generated_commands
         ;
   info->have_EXT_fragment_shader_interlock |= support_EXT_fragment_shader_interlock
         && (info->interlock_feats.fragmentShaderSampleInterlock)
&& (info->interlock_feats.fragmentShaderPixelInterlock);
   info->have_EXT_sample_locations |= support_EXT_sample_locations
         ;
   info->have_KHR_shader_draw_parameters |= support_KHR_shader_draw_parameters
         ;
   info->have_KHR_sampler_mirror_clamp_to_edge |= support_KHR_sampler_mirror_clamp_to_edge
         ;
   info->have_EXT_descriptor_buffer |= support_EXT_descriptor_buffer
         ;
   info->have_EXT_conditional_rendering |= support_EXT_conditional_rendering
         && (info->cond_render_feats.conditionalRendering);
   info->have_EXT_transform_feedback |= support_EXT_transform_feedback
         && (info->tf_feats.transformFeedback);
   info->have_EXT_index_type_uint8 |= support_EXT_index_type_uint8
         && (info->index_uint8_feats.indexTypeUint8);
   info->have_KHR_image_format_list |= support_KHR_image_format_list
         ;
   info->have_KHR_sampler_ycbcr_conversion |= support_KHR_sampler_ycbcr_conversion
         ;
   info->have_KHR_imageless_framebuffer |= support_KHR_imageless_framebuffer
         ;
   info->have_EXT_robustness2 |= support_EXT_robustness2
         && (info->rb2_feats.nullDescriptor);
   info->have_EXT_image_robustness |= support_EXT_image_robustness
         ;
   info->have_EXT_image_drm_format_modifier |= support_EXT_image_drm_format_modifier
         ;
   info->have_EXT_vertex_attribute_divisor |= support_EXT_vertex_attribute_divisor
         && (info->vdiv_feats.vertexAttributeInstanceRateDivisor);
   info->have_EXT_calibrated_timestamps |= support_EXT_calibrated_timestamps
         ;
   info->have_NV_linear_color_attachment |= support_NV_linear_color_attachment
         ;
   info->have_KHR_dynamic_rendering |= support_KHR_dynamic_rendering
         ;
   info->have_KHR_dynamic_rendering_local_read |= support_KHR_dynamic_rendering_local_read
         ;
   info->have_EXT_multisampled_render_to_single_sampled |= support_EXT_multisampled_render_to_single_sampled
         ;
   info->have_KHR_shader_clock |= support_KHR_shader_clock
         && (info->shader_clock_feats.shaderSubgroupClock);
   info->have_INTEL_shader_integer_functions2 |= support_INTEL_shader_integer_functions2
         && (info->shader_int_fns2_feats.shaderIntegerFunctions2);
   info->have_EXT_sampler_filter_minmax |= support_EXT_sampler_filter_minmax
         && (info->reduction_props.filterMinmaxSingleComponentFormats);
   info->have_EXT_custom_border_color |= support_EXT_custom_border_color
         && (info->border_color_feats.customBorderColors);
   info->have_EXT_non_seamless_cube_map |= support_EXT_non_seamless_cube_map
         ;
   info->have_EXT_border_color_swizzle |= support_EXT_border_color_swizzle
         ;
   info->have_EXT_blend_operation_advanced |= support_EXT_blend_operation_advanced
         && (info->blend_props.advancedBlendNonPremultipliedSrcColor)
&& (info->blend_props.advancedBlendNonPremultipliedDstColor);
   info->have_EXT_extended_dynamic_state |= support_EXT_extended_dynamic_state
         && (info->dynamic_state_feats.extendedDynamicState);
   info->have_EXT_extended_dynamic_state2 |= support_EXT_extended_dynamic_state2
         && (info->dynamic_state2_feats.extendedDynamicState2);
   info->have_EXT_extended_dynamic_state3 |= support_EXT_extended_dynamic_state3
         ;
   info->have_EXT_pipeline_creation_cache_control |= support_EXT_pipeline_creation_cache_control
         && (info->pipeline_cache_control_feats.pipelineCreationCacheControl);
   info->have_EXT_shader_stencil_export |= support_EXT_shader_stencil_export
         ;
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
   info->have_KHR_portability_subset |= support_KHR_portability_subset
         ;
#endif
   info->have_NV_compute_shader_derivatives |= support_NV_compute_shader_derivatives
         && (info->shader_derivs_feats.computeDerivativeGroupQuads)
&& (info->shader_derivs_feats.computeDerivativeGroupLinear);
   info->have_KHR_timeline_semaphore |= support_KHR_timeline_semaphore
         ;
   info->have_EXT_color_write_enable |= support_EXT_color_write_enable
         ;
   info->have_EXT_4444_formats |= support_EXT_4444_formats
         ;
   info->have_EXT_host_image_copy |= support_EXT_host_image_copy
         ;
   info->have_EXT_scalar_block_layout |= support_EXT_scalar_block_layout
         && (info->scalar_block_layout_feats.scalarBlockLayout);
   info->have_KHR_swapchain |= support_KHR_swapchain
         ;
   info->have_EXT_rasterization_order_attachment_access |= support_EXT_rasterization_order_attachment_access
         && (info->rast_order_access_feats.rasterizationOrderColorAttachmentAccess);
   info->have_KHR_shader_float16_int8 |= support_KHR_shader_float16_int8
         ;
   info->have_EXT_multi_draw |= support_EXT_multi_draw
         && (info->multidraw_feats.multiDraw);
   info->have_EXT_primitives_generated_query |= support_EXT_primitives_generated_query
         ;
   info->have_KHR_pipeline_library |= support_KHR_pipeline_library
         ;
   info->have_EXT_graphics_pipeline_library |= support_EXT_graphics_pipeline_library
         ;
   info->have_KHR_push_descriptor |= support_KHR_push_descriptor
         ;
   info->have_KHR_descriptor_update_template |= support_KHR_descriptor_update_template
         ;
   info->have_EXT_line_rasterization |= support_EXT_line_rasterization
         ;
   info->have_EXT_vertex_input_dynamic_state |= support_EXT_vertex_input_dynamic_state
         && (info->vertex_input_feats.vertexInputDynamicState);
   info->have_EXT_primitive_topology_list_restart |= support_EXT_primitive_topology_list_restart
         && (info->list_restart_feats.primitiveTopologyListRestart);
   info->have_KHR_dedicated_allocation |= support_KHR_dedicated_allocation
         ;
   info->have_EXT_descriptor_indexing |= support_EXT_descriptor_indexing
         && (info->desc_indexing_feats.descriptorBindingPartiallyBound);
   info->have_EXT_depth_clip_enable |= support_EXT_depth_clip_enable
         ;
   info->have_EXT_shader_demote_to_helper_invocation |= support_EXT_shader_demote_to_helper_invocation
         && (info->demote_feats.shaderDemoteToHelperInvocation);
   info->have_KHR_shader_float_controls |= support_KHR_shader_float_controls
         ;
   info->have_KHR_format_feature_flags2 |= support_KHR_format_feature_flags2
         ;
   }

   // generate extension list
   num_extensions = 0;

   if (info->have_KHR_maintenance1) {
       info->extensions[num_extensions++] = "VK_KHR_maintenance1";
   } else {
       debug_printf("ZINK: VK_KHR_maintenance1 required!\n");
       goto fail;
   }
   if (info->have_KHR_maintenance2) {
       info->extensions[num_extensions++] = "VK_KHR_maintenance2";
   }
   if (info->have_KHR_maintenance3) {
       info->extensions[num_extensions++] = "VK_KHR_maintenance3";
   }
   if (info->have_KHR_maintenance4) {
       info->extensions[num_extensions++] = "VK_KHR_maintenance4";
   }
   if (info->have_KHR_maintenance5) {
       info->extensions[num_extensions++] = "VK_KHR_maintenance5";
   }
   if (info->have_KHR_maintenance6) {
       info->extensions[num_extensions++] = "VK_KHR_maintenance6";
   }
   if (info->have_KHR_external_memory) {
       info->extensions[num_extensions++] = "VK_KHR_external_memory";
   }
   if (info->have_KHR_external_memory_fd) {
       info->extensions[num_extensions++] = "VK_KHR_external_memory_fd";
   }
   if (info->have_KHR_vulkan_memory_model) {
       info->extensions[num_extensions++] = "VK_KHR_vulkan_memory_model";
   }
   if (info->have_KHR_workgroup_memory_explicit_layout) {
       info->extensions[num_extensions++] = "VK_KHR_workgroup_memory_explicit_layout";
   }
   if (info->have_KHR_pipeline_executable_properties) {
       info->extensions[num_extensions++] = "VK_KHR_pipeline_executable_properties";
   }
   if (info->have_KHR_external_semaphore_fd) {
       info->extensions[num_extensions++] = "VK_KHR_external_semaphore_fd";
   }
   if (info->have_KHR_create_renderpass2) {
       info->extensions[num_extensions++] = "VK_KHR_create_renderpass2";
   } else {
       debug_printf("ZINK: VK_KHR_create_renderpass2 required!\n");
       goto fail;
   }
   if (info->have_KHR_synchronization2) {
       info->extensions[num_extensions++] = "VK_KHR_synchronization2";
   }
   if (info->have_KHR_external_memory_win32) {
       info->extensions[num_extensions++] = "VK_KHR_external_memory_win32";
   }
   if (info->have_KHR_external_semaphore_win32) {
       info->extensions[num_extensions++] = "VK_KHR_external_semaphore_win32";
   }
   if (info->have_EXT_external_memory_dma_buf) {
       info->extensions[num_extensions++] = "VK_EXT_external_memory_dma_buf";
   }
   if (info->have_KHR_buffer_device_address) {
       info->extensions[num_extensions++] = "VK_KHR_buffer_device_address";
   }
   if (info->have_EXT_external_memory_host) {
       info->extensions[num_extensions++] = "VK_EXT_external_memory_host";
   }
   if (info->have_EXT_queue_family_foreign) {
       info->extensions[num_extensions++] = "VK_EXT_queue_family_foreign";
   }
   if (info->have_KHR_swapchain_mutable_format) {
       info->extensions[num_extensions++] = "VK_KHR_swapchain_mutable_format";
   }
   if (info->have_KHR_incremental_present) {
       info->extensions[num_extensions++] = "VK_KHR_incremental_present";
   }
   if (info->have_EXT_provoking_vertex) {
       info->extensions[num_extensions++] = "VK_EXT_provoking_vertex";
   }
   if (info->have_EXT_shader_viewport_index_layer) {
       info->extensions[num_extensions++] = "VK_EXT_shader_viewport_index_layer";
   }
   if (info->have_KHR_get_memory_requirements2) {
       info->extensions[num_extensions++] = "VK_KHR_get_memory_requirements2";
   }
   if (info->have_EXT_post_depth_coverage) {
       info->extensions[num_extensions++] = "VK_EXT_post_depth_coverage";
   }
   if (info->have_EXT_depth_clip_control) {
       info->extensions[num_extensions++] = "VK_EXT_depth_clip_control";
   }
   if (info->have_EXT_depth_clamp_zero_one) {
       info->extensions[num_extensions++] = "VK_EXT_depth_clamp_zero_one";
   }
   if (info->have_EXT_shader_subgroup_ballot) {
       info->extensions[num_extensions++] = "VK_EXT_shader_subgroup_ballot";
   }
   if (info->have_EXT_shader_subgroup_vote) {
       info->extensions[num_extensions++] = "VK_EXT_shader_subgroup_vote";
   }
   if (info->have_EXT_shader_atomic_float) {
       info->extensions[num_extensions++] = "VK_EXT_shader_atomic_float";
   }
   if (info->have_KHR_shader_atomic_int64) {
       info->extensions[num_extensions++] = "VK_KHR_shader_atomic_int64";
   }
   if (info->have_KHR_8bit_storage) {
       info->extensions[num_extensions++] = "VK_KHR_8bit_storage";
   }
   if (info->have_KHR_16bit_storage) {
       info->extensions[num_extensions++] = "VK_KHR_16bit_storage";
   }
   if (info->have_EXT_image_2d_view_of_3d) {
       info->extensions[num_extensions++] = "VK_EXT_image_2d_view_of_3d";
   }
   if (info->have_KHR_driver_properties) {
       info->extensions[num_extensions++] = "VK_KHR_driver_properties";
   }
   if (info->have_EXT_memory_budget) {
       info->extensions[num_extensions++] = "VK_EXT_memory_budget";
   }
   if (info->have_EXT_memory_priority) {
       info->extensions[num_extensions++] = "VK_EXT_memory_priority";
   }
   if (info->have_EXT_pageable_device_local_memory) {
       info->extensions[num_extensions++] = "VK_EXT_pageable_device_local_memory";
   }
   if (info->have_KHR_draw_indirect_count) {
       info->extensions[num_extensions++] = "VK_KHR_draw_indirect_count";
   }
   if (info->have_EXT_dynamic_rendering_unused_attachments) {
       info->extensions[num_extensions++] = "VK_EXT_dynamic_rendering_unused_attachments";
   }
   if (info->have_EXT_shader_object) {
       info->extensions[num_extensions++] = "VK_EXT_shader_object";
   }
   if (info->have_EXT_attachment_feedback_loop_layout) {
       info->extensions[num_extensions++] = "VK_EXT_attachment_feedback_loop_layout";
   }
   if (info->have_EXT_attachment_feedback_loop_dynamic_state) {
       info->extensions[num_extensions++] = "VK_EXT_attachment_feedback_loop_dynamic_state";
   }
   if (info->have_NV_device_generated_commands) {
       info->extensions[num_extensions++] = "VK_NV_device_generated_commands";
   }
   if (info->have_EXT_fragment_shader_interlock) {
       info->extensions[num_extensions++] = "VK_EXT_fragment_shader_interlock";
   }
   if (info->have_EXT_sample_locations) {
       info->extensions[num_extensions++] = "VK_EXT_sample_locations";
   }
   if (info->have_KHR_shader_draw_parameters) {
       info->extensions[num_extensions++] = "VK_KHR_shader_draw_parameters";
   }
   if (info->have_KHR_sampler_mirror_clamp_to_edge) {
       info->extensions[num_extensions++] = "VK_KHR_sampler_mirror_clamp_to_edge";
   }
   if (info->have_EXT_descriptor_buffer) {
       info->extensions[num_extensions++] = "VK_EXT_descriptor_buffer";
   }
   if (info->have_EXT_conditional_rendering) {
       info->extensions[num_extensions++] = "VK_EXT_conditional_rendering";
   }
   if (info->have_EXT_transform_feedback) {
       info->extensions[num_extensions++] = "VK_EXT_transform_feedback";
   }
   if (info->have_EXT_index_type_uint8) {
       info->extensions[num_extensions++] = "VK_EXT_index_type_uint8";
   }
   if (info->have_KHR_image_format_list) {
       info->extensions[num_extensions++] = "VK_KHR_image_format_list";
   }
   if (info->have_KHR_sampler_ycbcr_conversion) {
       info->extensions[num_extensions++] = "VK_KHR_sampler_ycbcr_conversion";
   }
   if (info->have_KHR_imageless_framebuffer) {
       info->extensions[num_extensions++] = "VK_KHR_imageless_framebuffer";
   } else {
       debug_printf("ZINK: VK_KHR_imageless_framebuffer required!\n");
       goto fail;
   }
   if (info->have_EXT_robustness2) {
       info->extensions[num_extensions++] = "VK_EXT_robustness2";
   }
   if (info->have_EXT_image_robustness) {
       info->extensions[num_extensions++] = "VK_EXT_image_robustness";
   }
   if (info->have_EXT_image_drm_format_modifier) {
       info->extensions[num_extensions++] = "VK_EXT_image_drm_format_modifier";
   }
   if (info->have_EXT_vertex_attribute_divisor) {
       info->extensions[num_extensions++] = "VK_EXT_vertex_attribute_divisor";
   }
   if (info->have_EXT_calibrated_timestamps) {
       info->extensions[num_extensions++] = "VK_EXT_calibrated_timestamps";
   }
   if (info->have_NV_linear_color_attachment) {
       info->extensions[num_extensions++] = "VK_NV_linear_color_attachment";
   }
   if (info->have_KHR_dynamic_rendering) {
       info->extensions[num_extensions++] = "VK_KHR_dynamic_rendering";
   }
   if (info->have_KHR_dynamic_rendering_local_read) {
       info->extensions[num_extensions++] = "VK_KHR_dynamic_rendering_local_read";
   }
   if (info->have_EXT_multisampled_render_to_single_sampled) {
       info->extensions[num_extensions++] = "VK_EXT_multisampled_render_to_single_sampled";
   }
   if (info->have_KHR_shader_clock) {
       info->extensions[num_extensions++] = "VK_KHR_shader_clock";
   }
   if (info->have_INTEL_shader_integer_functions2) {
       info->extensions[num_extensions++] = "VK_INTEL_shader_integer_functions2";
   }
   if (info->have_EXT_sampler_filter_minmax) {
       info->extensions[num_extensions++] = "VK_EXT_sampler_filter_minmax";
   }
   if (info->have_EXT_custom_border_color) {
       info->extensions[num_extensions++] = "VK_EXT_custom_border_color";
   }
   if (info->have_EXT_non_seamless_cube_map) {
       info->extensions[num_extensions++] = "VK_EXT_non_seamless_cube_map";
   }
   if (info->have_EXT_border_color_swizzle) {
       info->extensions[num_extensions++] = "VK_EXT_border_color_swizzle";
   }
   if (info->have_EXT_blend_operation_advanced) {
       info->extensions[num_extensions++] = "VK_EXT_blend_operation_advanced";
   }
   if (info->have_EXT_extended_dynamic_state) {
       info->extensions[num_extensions++] = "VK_EXT_extended_dynamic_state";
   }
   if (info->have_EXT_extended_dynamic_state2) {
       info->extensions[num_extensions++] = "VK_EXT_extended_dynamic_state2";
   }
   if (info->have_EXT_extended_dynamic_state3) {
       info->extensions[num_extensions++] = "VK_EXT_extended_dynamic_state3";
   }
   if (info->have_EXT_pipeline_creation_cache_control) {
       info->extensions[num_extensions++] = "VK_EXT_pipeline_creation_cache_control";
   }
   if (info->have_EXT_shader_stencil_export) {
       info->extensions[num_extensions++] = "VK_EXT_shader_stencil_export";
   }
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
   if (info->have_KHR_portability_subset) {
       info->extensions[num_extensions++] = "VK_KHR_portability_subset";
   }
#endif
   if (info->have_NV_compute_shader_derivatives) {
       info->extensions[num_extensions++] = "VK_NV_compute_shader_derivatives";
   }
   if (info->have_KHR_timeline_semaphore) {
       info->extensions[num_extensions++] = "VK_KHR_timeline_semaphore";
   }
   if (info->have_EXT_color_write_enable) {
       info->extensions[num_extensions++] = "VK_EXT_color_write_enable";
   }
   if (info->have_EXT_4444_formats) {
       info->extensions[num_extensions++] = "VK_EXT_4444_formats";
   }
   if (info->have_EXT_host_image_copy) {
       info->extensions[num_extensions++] = "VK_EXT_host_image_copy";
   }
   if (info->have_EXT_scalar_block_layout) {
       info->extensions[num_extensions++] = "VK_EXT_scalar_block_layout";
   }
   if (info->have_KHR_swapchain) {
       info->extensions[num_extensions++] = "VK_KHR_swapchain";
   }
   if (info->have_EXT_rasterization_order_attachment_access) {
       info->extensions[num_extensions++] = "VK_EXT_rasterization_order_attachment_access";
   }
   if (info->have_KHR_shader_float16_int8) {
       info->extensions[num_extensions++] = "VK_KHR_shader_float16_int8";
   }
   if (info->have_EXT_multi_draw) {
       info->extensions[num_extensions++] = "VK_EXT_multi_draw";
   }
   if (info->have_EXT_primitives_generated_query) {
       info->extensions[num_extensions++] = "VK_EXT_primitives_generated_query";
   }
   if (info->have_KHR_pipeline_library) {
       info->extensions[num_extensions++] = "VK_KHR_pipeline_library";
   }
   if (info->have_EXT_graphics_pipeline_library) {
       info->extensions[num_extensions++] = "VK_EXT_graphics_pipeline_library";
   }
   if (info->have_KHR_push_descriptor) {
       info->extensions[num_extensions++] = "VK_KHR_push_descriptor";
   }
   if (info->have_KHR_descriptor_update_template) {
       info->extensions[num_extensions++] = "VK_KHR_descriptor_update_template";
   } else {
       debug_printf("ZINK: VK_KHR_descriptor_update_template required!\n");
       goto fail;
   }
   if (info->have_EXT_line_rasterization) {
       info->extensions[num_extensions++] = "VK_EXT_line_rasterization";
   }
   if (info->have_EXT_vertex_input_dynamic_state) {
       info->extensions[num_extensions++] = "VK_EXT_vertex_input_dynamic_state";
   }
   if (info->have_EXT_primitive_topology_list_restart) {
       info->extensions[num_extensions++] = "VK_EXT_primitive_topology_list_restart";
   }
   if (info->have_KHR_dedicated_allocation) {
       info->extensions[num_extensions++] = "VK_KHR_dedicated_allocation";
   }
   if (info->have_EXT_descriptor_indexing) {
       info->extensions[num_extensions++] = "VK_EXT_descriptor_indexing";
   }
   if (info->have_EXT_depth_clip_enable) {
       info->extensions[num_extensions++] = "VK_EXT_depth_clip_enable";
   }
   if (info->have_EXT_shader_demote_to_helper_invocation) {
       info->extensions[num_extensions++] = "VK_EXT_shader_demote_to_helper_invocation";
   }
   if (info->have_KHR_shader_float_controls) {
       info->extensions[num_extensions++] = "VK_KHR_shader_float_controls";
   }
   if (info->have_KHR_format_feature_flags2) {
       info->extensions[num_extensions++] = "VK_KHR_format_feature_flags2";
   }

   info->num_extensions = num_extensions;

   info->feats.pNext = NULL;

      if (VK_MAKE_VERSION(1,2,0) <= screen->vk_version) {
         /* VkPhysicalDeviceVulkan11Features was added in 1.2, not 1.1 as one would think */
         info->feats11.pNext = info->feats.pNext;
         info->feats.pNext = &info->feats11;
      }
      if (VK_MAKE_VERSION(1,2,0) <= screen->vk_version) {
         info->feats12.pNext = info->feats.pNext;
         info->feats.pNext = &info->feats12;
      }
      if (VK_MAKE_VERSION(1,3,0) <= screen->vk_version) {
         info->feats13.pNext = info->feats.pNext;
         info->feats.pNext = &info->feats13;
      }

   if (info->have_KHR_maintenance4 && !info->have_vulkan13) {
         info->maint4_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES_KHR;
         info->maint4_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->maint4_feats;
      }
   if (info->have_KHR_maintenance5) {
         info->maint5_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR;
         info->maint5_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->maint5_feats;
      }
   if (info->have_KHR_maintenance6) {
         info->maint6_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_FEATURES_KHR;
         info->maint6_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->maint6_feats;
      }
   if (info->have_KHR_workgroup_memory_explicit_layout) {
         info->explicit_layout_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_WORKGROUP_MEMORY_EXPLICIT_LAYOUT_FEATURES_KHR;
         info->explicit_layout_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->explicit_layout_feats;
      }
   if (info->have_KHR_pipeline_executable_properties) {
         info->pipestats_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_EXECUTABLE_PROPERTIES_FEATURES_KHR;
         info->pipestats_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->pipestats_feats;
      }
   if (info->have_KHR_synchronization2 && !info->have_vulkan13) {
         info->sync2_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;
         info->sync2_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->sync2_feats;
      }
   if (info->have_KHR_buffer_device_address && !info->have_vulkan12) {
         info->bda_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR;
         info->bda_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->bda_feats;
      }
   if (info->have_EXT_provoking_vertex) {
         info->pv_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_FEATURES_EXT;
         info->pv_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->pv_feats;
      }
   if (info->have_EXT_depth_clip_control) {
         info->clip_control_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_CONTROL_FEATURES_EXT;
         info->clip_control_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->clip_control_feats;
      }
   if (info->have_EXT_depth_clamp_zero_one) {
         info->clamp_01_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLAMP_ZERO_ONE_FEATURES_EXT;
         info->clamp_01_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->clamp_01_feats;
      }
   if (info->have_EXT_shader_atomic_float) {
         info->atomic_float_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT;
         info->atomic_float_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->atomic_float_feats;
      }
   if (info->have_KHR_shader_atomic_int64 && !info->have_vulkan12) {
         info->atomic_int_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES_KHR;
         info->atomic_int_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->atomic_int_feats;
      }
   if (info->have_KHR_8bit_storage && !info->have_vulkan12) {
         info->storage_8bit_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR;
         info->storage_8bit_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->storage_8bit_feats;
      }
   if (info->have_KHR_16bit_storage && !info->have_vulkan11) {
         info->storage_16bit_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR;
         info->storage_16bit_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->storage_16bit_feats;
      }
   if (info->have_EXT_image_2d_view_of_3d) {
         info->view2d_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_2D_VIEW_OF_3D_FEATURES_EXT;
         info->view2d_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->view2d_feats;
      }
   if (info->have_EXT_memory_priority) {
         info->memprio_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT;
         info->memprio_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->memprio_feats;
      }
   if (info->have_EXT_pageable_device_local_memory) {
         info->mempage_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT;
         info->mempage_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->mempage_feats;
      }
   if (info->have_EXT_dynamic_rendering_unused_attachments) {
         info->unused_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_FEATURES_EXT;
         info->unused_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->unused_feats;
      }
   if (info->have_EXT_shader_object) {
         info->shobj_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT;
         info->shobj_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->shobj_feats;
      }
   if (info->have_EXT_attachment_feedback_loop_layout) {
         info->feedback_loop_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_FEATURES_EXT;
         info->feedback_loop_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->feedback_loop_feats;
      }
   if (info->have_EXT_attachment_feedback_loop_dynamic_state) {
         info->feedback_dyn_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_DYNAMIC_STATE_FEATURES_EXT;
         info->feedback_dyn_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->feedback_dyn_feats;
      }
   if (info->have_NV_device_generated_commands) {
         info->nv_dgc_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_FEATURES_NV;
         info->nv_dgc_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->nv_dgc_feats;
      }
   if (info->have_EXT_fragment_shader_interlock) {
         info->interlock_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT;
         info->interlock_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->interlock_feats;
      }
   if (info->have_EXT_descriptor_buffer) {
         info->db_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
         info->db_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->db_feats;
      }
   if (info->have_EXT_conditional_rendering) {
         info->cond_render_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT;
         info->cond_render_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->cond_render_feats;
      }
   if (info->have_EXT_transform_feedback) {
         info->tf_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT;
         info->tf_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->tf_feats;
      }
   if (info->have_EXT_index_type_uint8) {
         info->index_uint8_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT;
         info->index_uint8_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->index_uint8_feats;
      }
   if (info->have_KHR_imageless_framebuffer && !info->have_vulkan12) {
         info->imgless_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES_KHR;
         info->imgless_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->imgless_feats;
      }
   if (info->have_EXT_robustness2) {
         info->rb2_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
         info->rb2_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->rb2_feats;
      }
   if (info->have_EXT_image_robustness && !info->have_vulkan13) {
         info->rb_image_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES_EXT;
         info->rb_image_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->rb_image_feats;
      }
   if (info->have_EXT_vertex_attribute_divisor) {
         info->vdiv_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT;
         info->vdiv_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->vdiv_feats;
      }
   if (info->have_NV_linear_color_attachment) {
         info->linear_color_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINEAR_COLOR_ATTACHMENT_FEATURES_NV;
         info->linear_color_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->linear_color_feats;
      }
   if (info->have_KHR_dynamic_rendering && !info->have_vulkan13) {
         info->dynamic_render_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
         info->dynamic_render_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->dynamic_render_feats;
      }
   if (info->have_KHR_dynamic_rendering_local_read) {
         info->drlr_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_LOCAL_READ_FEATURES_KHR;
         info->drlr_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->drlr_feats;
      }
   if (info->have_EXT_multisampled_render_to_single_sampled) {
         info->msrtss_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_FEATURES_EXT;
         info->msrtss_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->msrtss_feats;
      }
   if (info->have_KHR_shader_clock) {
         info->shader_clock_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR;
         info->shader_clock_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->shader_clock_feats;
      }
   if (info->have_INTEL_shader_integer_functions2) {
         info->shader_int_fns2_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_FUNCTIONS_2_FEATURES_INTEL;
         info->shader_int_fns2_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->shader_int_fns2_feats;
      }
   if (info->have_EXT_custom_border_color) {
         info->border_color_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT;
         info->border_color_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->border_color_feats;
      }
   if (info->have_EXT_non_seamless_cube_map) {
         info->nonseamless_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_NON_SEAMLESS_CUBE_MAP_FEATURES_EXT;
         info->nonseamless_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->nonseamless_feats;
      }
   if (info->have_EXT_border_color_swizzle) {
         info->border_swizzle_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BORDER_COLOR_SWIZZLE_FEATURES_EXT;
         info->border_swizzle_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->border_swizzle_feats;
      }
   if (info->have_EXT_extended_dynamic_state) {
         info->dynamic_state_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT;
         info->dynamic_state_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->dynamic_state_feats;
      }
   if (info->have_EXT_extended_dynamic_state2) {
         info->dynamic_state2_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT;
         info->dynamic_state2_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->dynamic_state2_feats;
      }
   if (info->have_EXT_extended_dynamic_state3) {
         info->dynamic_state3_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT;
         info->dynamic_state3_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->dynamic_state3_feats;
      }
   if (info->have_EXT_pipeline_creation_cache_control && !info->have_vulkan13) {
         info->pipeline_cache_control_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_CREATION_CACHE_CONTROL_FEATURES_EXT;
         info->pipeline_cache_control_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->pipeline_cache_control_feats;
      }
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
   if (info->have_KHR_portability_subset) {
         info->portability_subset_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR;
         info->portability_subset_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->portability_subset_feats;
      }
#endif
   if (info->have_NV_compute_shader_derivatives) {
         info->shader_derivs_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_NV;
         info->shader_derivs_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->shader_derivs_feats;
      }
   if (info->have_KHR_timeline_semaphore && !info->have_vulkan12) {
         info->timeline_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR;
         info->timeline_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->timeline_feats;
      }
   if (info->have_EXT_color_write_enable) {
         info->cwrite_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COLOR_WRITE_ENABLE_FEATURES_EXT;
         info->cwrite_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->cwrite_feats;
      }
   if (info->have_EXT_4444_formats) {
         info->format_4444_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_4444_FORMATS_FEATURES_EXT;
         info->format_4444_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->format_4444_feats;
      }
   if (info->have_EXT_host_image_copy) {
         info->hic_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_FEATURES_EXT;
         info->hic_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->hic_feats;
      }
   if (info->have_EXT_scalar_block_layout && !info->have_vulkan12) {
         info->scalar_block_layout_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT;
         info->scalar_block_layout_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->scalar_block_layout_feats;
      }
   if (info->have_EXT_rasterization_order_attachment_access) {
         info->rast_order_access_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_FEATURES_EXT;
         info->rast_order_access_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->rast_order_access_feats;
      }
   if (info->have_KHR_shader_float16_int8 && !info->have_vulkan12) {
         info->shader_float16_int8_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR;
         info->shader_float16_int8_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->shader_float16_int8_feats;
      }
   if (info->have_EXT_multi_draw) {
         info->multidraw_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTI_DRAW_FEATURES_EXT;
         info->multidraw_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->multidraw_feats;
      }
   if (info->have_EXT_primitives_generated_query) {
         info->primgen_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVES_GENERATED_QUERY_FEATURES_EXT;
         info->primgen_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->primgen_feats;
      }
   if (info->have_EXT_graphics_pipeline_library) {
         info->gpl_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT;
         info->gpl_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->gpl_feats;
      }
   if (info->have_EXT_line_rasterization) {
         info->line_rast_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT;
         info->line_rast_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->line_rast_feats;
      }
   if (info->have_EXT_vertex_input_dynamic_state) {
         info->vertex_input_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT;
         info->vertex_input_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->vertex_input_feats;
      }
   if (info->have_EXT_primitive_topology_list_restart) {
         info->list_restart_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVE_TOPOLOGY_LIST_RESTART_FEATURES_EXT;
         info->list_restart_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->list_restart_feats;
      }
   if (info->have_EXT_descriptor_indexing && !info->have_vulkan12) {
         info->desc_indexing_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
         info->desc_indexing_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->desc_indexing_feats;
      }
   if (info->have_EXT_depth_clip_enable) {
         info->depth_clip_enable_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT;
         info->depth_clip_enable_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->depth_clip_enable_feats;
      }
   if (info->have_EXT_shader_demote_to_helper_invocation && !info->have_vulkan13) {
         info->demote_feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES_EXT;
         info->demote_feats.pNext = info->feats.pNext;
         info->feats.pNext = &info->demote_feats;
      }

   return true;

fail:
   return false;
}

void
zink_verify_device_extensions(struct zink_screen *screen)
{
   if (screen->info.have_KHR_maintenance1) {
#ifdef _WIN32
      if (!screen->vk.TrimCommandPoolKHR) {
#ifndef NDEBUG
         screen->vk.TrimCommandPoolKHR = (PFN_vkTrimCommandPoolKHR)zink_stub_TrimCommandPoolKHR;
#else
         screen->vk.TrimCommandPoolKHR = (PFN_vkTrimCommandPoolKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_KHR_maintenance2) {
   }
   if (screen->info.have_KHR_maintenance3) {
#ifdef _WIN32
      if (!screen->vk.GetDescriptorSetLayoutSupportKHR) {
#ifndef NDEBUG
         screen->vk.GetDescriptorSetLayoutSupportKHR = (PFN_vkGetDescriptorSetLayoutSupportKHR)zink_stub_GetDescriptorSetLayoutSupportKHR;
#else
         screen->vk.GetDescriptorSetLayoutSupportKHR = (PFN_vkGetDescriptorSetLayoutSupportKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_KHR_maintenance4) {
#ifdef _WIN32
      if (!screen->vk.GetDeviceBufferMemoryRequirementsKHR) {
#ifndef NDEBUG
         screen->vk.GetDeviceBufferMemoryRequirementsKHR = (PFN_vkGetDeviceBufferMemoryRequirementsKHR)zink_stub_GetDeviceBufferMemoryRequirementsKHR;
#else
         screen->vk.GetDeviceBufferMemoryRequirementsKHR = (PFN_vkGetDeviceBufferMemoryRequirementsKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.GetDeviceImageMemoryRequirementsKHR) {
#ifndef NDEBUG
         screen->vk.GetDeviceImageMemoryRequirementsKHR = (PFN_vkGetDeviceImageMemoryRequirementsKHR)zink_stub_GetDeviceImageMemoryRequirementsKHR;
#else
         screen->vk.GetDeviceImageMemoryRequirementsKHR = (PFN_vkGetDeviceImageMemoryRequirementsKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.GetDeviceImageSparseMemoryRequirementsKHR) {
#ifndef NDEBUG
         screen->vk.GetDeviceImageSparseMemoryRequirementsKHR = (PFN_vkGetDeviceImageSparseMemoryRequirementsKHR)zink_stub_GetDeviceImageSparseMemoryRequirementsKHR;
#else
         screen->vk.GetDeviceImageSparseMemoryRequirementsKHR = (PFN_vkGetDeviceImageSparseMemoryRequirementsKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_KHR_maintenance5) {
#ifdef _WIN32
      if (!screen->vk.CmdBindIndexBuffer2KHR) {
#ifndef NDEBUG
         screen->vk.CmdBindIndexBuffer2KHR = (PFN_vkCmdBindIndexBuffer2KHR)zink_stub_CmdBindIndexBuffer2KHR;
#else
         screen->vk.CmdBindIndexBuffer2KHR = (PFN_vkCmdBindIndexBuffer2KHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.GetRenderingAreaGranularityKHR) {
#ifndef NDEBUG
         screen->vk.GetRenderingAreaGranularityKHR = (PFN_vkGetRenderingAreaGranularityKHR)zink_stub_GetRenderingAreaGranularityKHR;
#else
         screen->vk.GetRenderingAreaGranularityKHR = (PFN_vkGetRenderingAreaGranularityKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.GetDeviceImageSubresourceLayoutKHR) {
#ifndef NDEBUG
         screen->vk.GetDeviceImageSubresourceLayoutKHR = (PFN_vkGetDeviceImageSubresourceLayoutKHR)zink_stub_GetDeviceImageSubresourceLayoutKHR;
#else
         screen->vk.GetDeviceImageSubresourceLayoutKHR = (PFN_vkGetDeviceImageSubresourceLayoutKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.GetImageSubresourceLayout2KHR) {
#ifndef NDEBUG
         screen->vk.GetImageSubresourceLayout2KHR = (PFN_vkGetImageSubresourceLayout2KHR)zink_stub_GetImageSubresourceLayout2KHR;
#else
         screen->vk.GetImageSubresourceLayout2KHR = (PFN_vkGetImageSubresourceLayout2KHR)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_KHR_maintenance6) {
#ifdef _WIN32
      if (!screen->vk.CmdBindDescriptorSets2KHR) {
#ifndef NDEBUG
         screen->vk.CmdBindDescriptorSets2KHR = (PFN_vkCmdBindDescriptorSets2KHR)zink_stub_CmdBindDescriptorSets2KHR;
#else
         screen->vk.CmdBindDescriptorSets2KHR = (PFN_vkCmdBindDescriptorSets2KHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdPushConstants2KHR) {
#ifndef NDEBUG
         screen->vk.CmdPushConstants2KHR = (PFN_vkCmdPushConstants2KHR)zink_stub_CmdPushConstants2KHR;
#else
         screen->vk.CmdPushConstants2KHR = (PFN_vkCmdPushConstants2KHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdPushDescriptorSet2KHR) {
#ifndef NDEBUG
         screen->vk.CmdPushDescriptorSet2KHR = (PFN_vkCmdPushDescriptorSet2KHR)zink_stub_CmdPushDescriptorSet2KHR;
#else
         screen->vk.CmdPushDescriptorSet2KHR = (PFN_vkCmdPushDescriptorSet2KHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdPushDescriptorSetWithTemplate2KHR) {
#ifndef NDEBUG
         screen->vk.CmdPushDescriptorSetWithTemplate2KHR = (PFN_vkCmdPushDescriptorSetWithTemplate2KHR)zink_stub_CmdPushDescriptorSetWithTemplate2KHR;
#else
         screen->vk.CmdPushDescriptorSetWithTemplate2KHR = (PFN_vkCmdPushDescriptorSetWithTemplate2KHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetDescriptorBufferOffsets2EXT) {
#ifndef NDEBUG
         screen->vk.CmdSetDescriptorBufferOffsets2EXT = (PFN_vkCmdSetDescriptorBufferOffsets2EXT)zink_stub_CmdSetDescriptorBufferOffsets2EXT;
#else
         screen->vk.CmdSetDescriptorBufferOffsets2EXT = (PFN_vkCmdSetDescriptorBufferOffsets2EXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdBindDescriptorBufferEmbeddedSamplers2EXT) {
#ifndef NDEBUG
         screen->vk.CmdBindDescriptorBufferEmbeddedSamplers2EXT = (PFN_vkCmdBindDescriptorBufferEmbeddedSamplers2EXT)zink_stub_CmdBindDescriptorBufferEmbeddedSamplers2EXT;
#else
         screen->vk.CmdBindDescriptorBufferEmbeddedSamplers2EXT = (PFN_vkCmdBindDescriptorBufferEmbeddedSamplers2EXT)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_KHR_external_memory) {
   }
   if (screen->info.have_KHR_external_memory_fd) {
#ifdef _WIN32
      if (!screen->vk.GetMemoryFdKHR) {
#ifndef NDEBUG
         screen->vk.GetMemoryFdKHR = (PFN_vkGetMemoryFdKHR)zink_stub_GetMemoryFdKHR;
#else
         screen->vk.GetMemoryFdKHR = (PFN_vkGetMemoryFdKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.GetMemoryFdPropertiesKHR) {
#ifndef NDEBUG
         screen->vk.GetMemoryFdPropertiesKHR = (PFN_vkGetMemoryFdPropertiesKHR)zink_stub_GetMemoryFdPropertiesKHR;
#else
         screen->vk.GetMemoryFdPropertiesKHR = (PFN_vkGetMemoryFdPropertiesKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_KHR_vulkan_memory_model) {
   }
   if (screen->info.have_KHR_workgroup_memory_explicit_layout) {
   }
   if (screen->info.have_KHR_pipeline_executable_properties) {
#ifdef _WIN32
      if (!screen->vk.GetPipelineExecutablePropertiesKHR) {
#ifndef NDEBUG
         screen->vk.GetPipelineExecutablePropertiesKHR = (PFN_vkGetPipelineExecutablePropertiesKHR)zink_stub_GetPipelineExecutablePropertiesKHR;
#else
         screen->vk.GetPipelineExecutablePropertiesKHR = (PFN_vkGetPipelineExecutablePropertiesKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.GetPipelineExecutableStatisticsKHR) {
#ifndef NDEBUG
         screen->vk.GetPipelineExecutableStatisticsKHR = (PFN_vkGetPipelineExecutableStatisticsKHR)zink_stub_GetPipelineExecutableStatisticsKHR;
#else
         screen->vk.GetPipelineExecutableStatisticsKHR = (PFN_vkGetPipelineExecutableStatisticsKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.GetPipelineExecutableInternalRepresentationsKHR) {
#ifndef NDEBUG
         screen->vk.GetPipelineExecutableInternalRepresentationsKHR = (PFN_vkGetPipelineExecutableInternalRepresentationsKHR)zink_stub_GetPipelineExecutableInternalRepresentationsKHR;
#else
         screen->vk.GetPipelineExecutableInternalRepresentationsKHR = (PFN_vkGetPipelineExecutableInternalRepresentationsKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_KHR_external_semaphore_fd) {
#ifdef _WIN32
      if (!screen->vk.ImportSemaphoreFdKHR) {
#ifndef NDEBUG
         screen->vk.ImportSemaphoreFdKHR = (PFN_vkImportSemaphoreFdKHR)zink_stub_ImportSemaphoreFdKHR;
#else
         screen->vk.ImportSemaphoreFdKHR = (PFN_vkImportSemaphoreFdKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.GetSemaphoreFdKHR) {
#ifndef NDEBUG
         screen->vk.GetSemaphoreFdKHR = (PFN_vkGetSemaphoreFdKHR)zink_stub_GetSemaphoreFdKHR;
#else
         screen->vk.GetSemaphoreFdKHR = (PFN_vkGetSemaphoreFdKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_KHR_create_renderpass2) {
#ifdef _WIN32
      if (!screen->vk.CreateRenderPass2KHR) {
#ifndef NDEBUG
         screen->vk.CreateRenderPass2KHR = (PFN_vkCreateRenderPass2KHR)zink_stub_CreateRenderPass2KHR;
#else
         screen->vk.CreateRenderPass2KHR = (PFN_vkCreateRenderPass2KHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdBeginRenderPass2KHR) {
#ifndef NDEBUG
         screen->vk.CmdBeginRenderPass2KHR = (PFN_vkCmdBeginRenderPass2KHR)zink_stub_CmdBeginRenderPass2KHR;
#else
         screen->vk.CmdBeginRenderPass2KHR = (PFN_vkCmdBeginRenderPass2KHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdNextSubpass2KHR) {
#ifndef NDEBUG
         screen->vk.CmdNextSubpass2KHR = (PFN_vkCmdNextSubpass2KHR)zink_stub_CmdNextSubpass2KHR;
#else
         screen->vk.CmdNextSubpass2KHR = (PFN_vkCmdNextSubpass2KHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdEndRenderPass2KHR) {
#ifndef NDEBUG
         screen->vk.CmdEndRenderPass2KHR = (PFN_vkCmdEndRenderPass2KHR)zink_stub_CmdEndRenderPass2KHR;
#else
         screen->vk.CmdEndRenderPass2KHR = (PFN_vkCmdEndRenderPass2KHR)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_KHR_synchronization2) {
#ifdef _WIN32
      if (!screen->vk.CmdSetEvent2KHR) {
#ifndef NDEBUG
         screen->vk.CmdSetEvent2KHR = (PFN_vkCmdSetEvent2KHR)zink_stub_CmdSetEvent2KHR;
#else
         screen->vk.CmdSetEvent2KHR = (PFN_vkCmdSetEvent2KHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdResetEvent2KHR) {
#ifndef NDEBUG
         screen->vk.CmdResetEvent2KHR = (PFN_vkCmdResetEvent2KHR)zink_stub_CmdResetEvent2KHR;
#else
         screen->vk.CmdResetEvent2KHR = (PFN_vkCmdResetEvent2KHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdWaitEvents2KHR) {
#ifndef NDEBUG
         screen->vk.CmdWaitEvents2KHR = (PFN_vkCmdWaitEvents2KHR)zink_stub_CmdWaitEvents2KHR;
#else
         screen->vk.CmdWaitEvents2KHR = (PFN_vkCmdWaitEvents2KHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdPipelineBarrier2KHR) {
#ifndef NDEBUG
         screen->vk.CmdPipelineBarrier2KHR = (PFN_vkCmdPipelineBarrier2KHR)zink_stub_CmdPipelineBarrier2KHR;
#else
         screen->vk.CmdPipelineBarrier2KHR = (PFN_vkCmdPipelineBarrier2KHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdWriteTimestamp2KHR) {
#ifndef NDEBUG
         screen->vk.CmdWriteTimestamp2KHR = (PFN_vkCmdWriteTimestamp2KHR)zink_stub_CmdWriteTimestamp2KHR;
#else
         screen->vk.CmdWriteTimestamp2KHR = (PFN_vkCmdWriteTimestamp2KHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.QueueSubmit2KHR) {
#ifndef NDEBUG
         screen->vk.QueueSubmit2KHR = (PFN_vkQueueSubmit2KHR)zink_stub_QueueSubmit2KHR;
#else
         screen->vk.QueueSubmit2KHR = (PFN_vkQueueSubmit2KHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdWriteBufferMarker2AMD) {
#ifndef NDEBUG
         screen->vk.CmdWriteBufferMarker2AMD = (PFN_vkCmdWriteBufferMarker2AMD)zink_stub_CmdWriteBufferMarker2AMD;
#else
         screen->vk.CmdWriteBufferMarker2AMD = (PFN_vkCmdWriteBufferMarker2AMD)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.GetQueueCheckpointData2NV) {
#ifndef NDEBUG
         screen->vk.GetQueueCheckpointData2NV = (PFN_vkGetQueueCheckpointData2NV)zink_stub_GetQueueCheckpointData2NV;
#else
         screen->vk.GetQueueCheckpointData2NV = (PFN_vkGetQueueCheckpointData2NV)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_KHR_external_memory_win32) {
#ifdef _WIN32
      if (!screen->vk.GetMemoryWin32HandleKHR) {
#ifndef NDEBUG
         screen->vk.GetMemoryWin32HandleKHR = (PFN_vkGetMemoryWin32HandleKHR)zink_stub_GetMemoryWin32HandleKHR;
#else
         screen->vk.GetMemoryWin32HandleKHR = (PFN_vkGetMemoryWin32HandleKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.GetMemoryWin32HandlePropertiesKHR) {
#ifndef NDEBUG
         screen->vk.GetMemoryWin32HandlePropertiesKHR = (PFN_vkGetMemoryWin32HandlePropertiesKHR)zink_stub_GetMemoryWin32HandlePropertiesKHR;
#else
         screen->vk.GetMemoryWin32HandlePropertiesKHR = (PFN_vkGetMemoryWin32HandlePropertiesKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_KHR_external_semaphore_win32) {
#ifdef _WIN32
      if (!screen->vk.ImportSemaphoreWin32HandleKHR) {
#ifndef NDEBUG
         screen->vk.ImportSemaphoreWin32HandleKHR = (PFN_vkImportSemaphoreWin32HandleKHR)zink_stub_ImportSemaphoreWin32HandleKHR;
#else
         screen->vk.ImportSemaphoreWin32HandleKHR = (PFN_vkImportSemaphoreWin32HandleKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.GetSemaphoreWin32HandleKHR) {
#ifndef NDEBUG
         screen->vk.GetSemaphoreWin32HandleKHR = (PFN_vkGetSemaphoreWin32HandleKHR)zink_stub_GetSemaphoreWin32HandleKHR;
#else
         screen->vk.GetSemaphoreWin32HandleKHR = (PFN_vkGetSemaphoreWin32HandleKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_EXT_external_memory_dma_buf) {
   }
   if (screen->info.have_KHR_buffer_device_address) {
#ifdef _WIN32
      if (!screen->vk.GetBufferDeviceAddressKHR) {
#ifndef NDEBUG
         screen->vk.GetBufferDeviceAddressKHR = (PFN_vkGetBufferDeviceAddressKHR)zink_stub_GetBufferDeviceAddressKHR;
#else
         screen->vk.GetBufferDeviceAddressKHR = (PFN_vkGetBufferDeviceAddressKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.GetBufferOpaqueCaptureAddressKHR) {
#ifndef NDEBUG
         screen->vk.GetBufferOpaqueCaptureAddressKHR = (PFN_vkGetBufferOpaqueCaptureAddressKHR)zink_stub_GetBufferOpaqueCaptureAddressKHR;
#else
         screen->vk.GetBufferOpaqueCaptureAddressKHR = (PFN_vkGetBufferOpaqueCaptureAddressKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.GetDeviceMemoryOpaqueCaptureAddressKHR) {
#ifndef NDEBUG
         screen->vk.GetDeviceMemoryOpaqueCaptureAddressKHR = (PFN_vkGetDeviceMemoryOpaqueCaptureAddressKHR)zink_stub_GetDeviceMemoryOpaqueCaptureAddressKHR;
#else
         screen->vk.GetDeviceMemoryOpaqueCaptureAddressKHR = (PFN_vkGetDeviceMemoryOpaqueCaptureAddressKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_EXT_external_memory_host) {
#ifdef _WIN32
      if (!screen->vk.GetMemoryHostPointerPropertiesEXT) {
#ifndef NDEBUG
         screen->vk.GetMemoryHostPointerPropertiesEXT = (PFN_vkGetMemoryHostPointerPropertiesEXT)zink_stub_GetMemoryHostPointerPropertiesEXT;
#else
         screen->vk.GetMemoryHostPointerPropertiesEXT = (PFN_vkGetMemoryHostPointerPropertiesEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_EXT_queue_family_foreign) {
   }
   if (screen->info.have_KHR_swapchain_mutable_format) {
   }
   if (screen->info.have_KHR_incremental_present) {
   }
   if (screen->info.have_EXT_provoking_vertex) {
   }
   if (screen->info.have_EXT_shader_viewport_index_layer) {
   }
   if (screen->info.have_KHR_get_memory_requirements2) {
#ifdef _WIN32
      if (!screen->vk.GetImageMemoryRequirements2KHR) {
#ifndef NDEBUG
         screen->vk.GetImageMemoryRequirements2KHR = (PFN_vkGetImageMemoryRequirements2KHR)zink_stub_GetImageMemoryRequirements2KHR;
#else
         screen->vk.GetImageMemoryRequirements2KHR = (PFN_vkGetImageMemoryRequirements2KHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.GetBufferMemoryRequirements2KHR) {
#ifndef NDEBUG
         screen->vk.GetBufferMemoryRequirements2KHR = (PFN_vkGetBufferMemoryRequirements2KHR)zink_stub_GetBufferMemoryRequirements2KHR;
#else
         screen->vk.GetBufferMemoryRequirements2KHR = (PFN_vkGetBufferMemoryRequirements2KHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.GetImageSparseMemoryRequirements2KHR) {
#ifndef NDEBUG
         screen->vk.GetImageSparseMemoryRequirements2KHR = (PFN_vkGetImageSparseMemoryRequirements2KHR)zink_stub_GetImageSparseMemoryRequirements2KHR;
#else
         screen->vk.GetImageSparseMemoryRequirements2KHR = (PFN_vkGetImageSparseMemoryRequirements2KHR)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_EXT_post_depth_coverage) {
   }
   if (screen->info.have_EXT_depth_clip_control) {
   }
   if (screen->info.have_EXT_depth_clamp_zero_one) {
   }
   if (screen->info.have_EXT_shader_subgroup_ballot) {
   }
   if (screen->info.have_EXT_shader_subgroup_vote) {
   }
   if (screen->info.have_EXT_shader_atomic_float) {
   }
   if (screen->info.have_KHR_shader_atomic_int64) {
   }
   if (screen->info.have_KHR_8bit_storage) {
   }
   if (screen->info.have_KHR_16bit_storage) {
   }
   if (screen->info.have_EXT_image_2d_view_of_3d) {
   }
   if (screen->info.have_KHR_driver_properties) {
   }
   if (screen->info.have_EXT_memory_budget) {
   }
   if (screen->info.have_EXT_memory_priority) {
   }
   if (screen->info.have_EXT_pageable_device_local_memory) {
#ifdef _WIN32
      if (!screen->vk.SetDeviceMemoryPriorityEXT) {
#ifndef NDEBUG
         screen->vk.SetDeviceMemoryPriorityEXT = (PFN_vkSetDeviceMemoryPriorityEXT)zink_stub_SetDeviceMemoryPriorityEXT;
#else
         screen->vk.SetDeviceMemoryPriorityEXT = (PFN_vkSetDeviceMemoryPriorityEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_KHR_draw_indirect_count) {
#ifdef _WIN32
      if (!screen->vk.CmdDrawIndirectCountKHR) {
#ifndef NDEBUG
         screen->vk.CmdDrawIndirectCountKHR = (PFN_vkCmdDrawIndirectCountKHR)zink_stub_CmdDrawIndirectCountKHR;
#else
         screen->vk.CmdDrawIndirectCountKHR = (PFN_vkCmdDrawIndirectCountKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdDrawIndexedIndirectCountKHR) {
#ifndef NDEBUG
         screen->vk.CmdDrawIndexedIndirectCountKHR = (PFN_vkCmdDrawIndexedIndirectCountKHR)zink_stub_CmdDrawIndexedIndirectCountKHR;
#else
         screen->vk.CmdDrawIndexedIndirectCountKHR = (PFN_vkCmdDrawIndexedIndirectCountKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_EXT_dynamic_rendering_unused_attachments) {
   }
   if (screen->info.have_EXT_shader_object) {
#ifdef _WIN32
      if (!screen->vk.CreateShadersEXT) {
#ifndef NDEBUG
         screen->vk.CreateShadersEXT = (PFN_vkCreateShadersEXT)zink_stub_CreateShadersEXT;
#else
         screen->vk.CreateShadersEXT = (PFN_vkCreateShadersEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.DestroyShaderEXT) {
#ifndef NDEBUG
         screen->vk.DestroyShaderEXT = (PFN_vkDestroyShaderEXT)zink_stub_DestroyShaderEXT;
#else
         screen->vk.DestroyShaderEXT = (PFN_vkDestroyShaderEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.GetShaderBinaryDataEXT) {
#ifndef NDEBUG
         screen->vk.GetShaderBinaryDataEXT = (PFN_vkGetShaderBinaryDataEXT)zink_stub_GetShaderBinaryDataEXT;
#else
         screen->vk.GetShaderBinaryDataEXT = (PFN_vkGetShaderBinaryDataEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdBindShadersEXT) {
#ifndef NDEBUG
         screen->vk.CmdBindShadersEXT = (PFN_vkCmdBindShadersEXT)zink_stub_CmdBindShadersEXT;
#else
         screen->vk.CmdBindShadersEXT = (PFN_vkCmdBindShadersEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetCullModeEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetCullModeEXT = (PFN_vkCmdSetCullModeEXT)zink_stub_CmdSetCullModeEXT;
#else
         screen->vk.CmdSetCullModeEXT = (PFN_vkCmdSetCullModeEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetFrontFaceEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetFrontFaceEXT = (PFN_vkCmdSetFrontFaceEXT)zink_stub_CmdSetFrontFaceEXT;
#else
         screen->vk.CmdSetFrontFaceEXT = (PFN_vkCmdSetFrontFaceEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetPrimitiveTopologyEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetPrimitiveTopologyEXT = (PFN_vkCmdSetPrimitiveTopologyEXT)zink_stub_CmdSetPrimitiveTopologyEXT;
#else
         screen->vk.CmdSetPrimitiveTopologyEXT = (PFN_vkCmdSetPrimitiveTopologyEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetViewportWithCountEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetViewportWithCountEXT = (PFN_vkCmdSetViewportWithCountEXT)zink_stub_CmdSetViewportWithCountEXT;
#else
         screen->vk.CmdSetViewportWithCountEXT = (PFN_vkCmdSetViewportWithCountEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetScissorWithCountEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetScissorWithCountEXT = (PFN_vkCmdSetScissorWithCountEXT)zink_stub_CmdSetScissorWithCountEXT;
#else
         screen->vk.CmdSetScissorWithCountEXT = (PFN_vkCmdSetScissorWithCountEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdBindVertexBuffers2EXT) {
#ifndef NDEBUG
         screen->vk.CmdBindVertexBuffers2EXT = (PFN_vkCmdBindVertexBuffers2EXT)zink_stub_CmdBindVertexBuffers2EXT;
#else
         screen->vk.CmdBindVertexBuffers2EXT = (PFN_vkCmdBindVertexBuffers2EXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetDepthTestEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetDepthTestEnableEXT = (PFN_vkCmdSetDepthTestEnableEXT)zink_stub_CmdSetDepthTestEnableEXT;
#else
         screen->vk.CmdSetDepthTestEnableEXT = (PFN_vkCmdSetDepthTestEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetDepthWriteEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetDepthWriteEnableEXT = (PFN_vkCmdSetDepthWriteEnableEXT)zink_stub_CmdSetDepthWriteEnableEXT;
#else
         screen->vk.CmdSetDepthWriteEnableEXT = (PFN_vkCmdSetDepthWriteEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetDepthCompareOpEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetDepthCompareOpEXT = (PFN_vkCmdSetDepthCompareOpEXT)zink_stub_CmdSetDepthCompareOpEXT;
#else
         screen->vk.CmdSetDepthCompareOpEXT = (PFN_vkCmdSetDepthCompareOpEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetDepthBoundsTestEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetDepthBoundsTestEnableEXT = (PFN_vkCmdSetDepthBoundsTestEnableEXT)zink_stub_CmdSetDepthBoundsTestEnableEXT;
#else
         screen->vk.CmdSetDepthBoundsTestEnableEXT = (PFN_vkCmdSetDepthBoundsTestEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetStencilTestEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetStencilTestEnableEXT = (PFN_vkCmdSetStencilTestEnableEXT)zink_stub_CmdSetStencilTestEnableEXT;
#else
         screen->vk.CmdSetStencilTestEnableEXT = (PFN_vkCmdSetStencilTestEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetStencilOpEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetStencilOpEXT = (PFN_vkCmdSetStencilOpEXT)zink_stub_CmdSetStencilOpEXT;
#else
         screen->vk.CmdSetStencilOpEXT = (PFN_vkCmdSetStencilOpEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetVertexInputEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetVertexInputEXT = (PFN_vkCmdSetVertexInputEXT)zink_stub_CmdSetVertexInputEXT;
#else
         screen->vk.CmdSetVertexInputEXT = (PFN_vkCmdSetVertexInputEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetPatchControlPointsEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetPatchControlPointsEXT = (PFN_vkCmdSetPatchControlPointsEXT)zink_stub_CmdSetPatchControlPointsEXT;
#else
         screen->vk.CmdSetPatchControlPointsEXT = (PFN_vkCmdSetPatchControlPointsEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetRasterizerDiscardEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetRasterizerDiscardEnableEXT = (PFN_vkCmdSetRasterizerDiscardEnableEXT)zink_stub_CmdSetRasterizerDiscardEnableEXT;
#else
         screen->vk.CmdSetRasterizerDiscardEnableEXT = (PFN_vkCmdSetRasterizerDiscardEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetDepthBiasEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetDepthBiasEnableEXT = (PFN_vkCmdSetDepthBiasEnableEXT)zink_stub_CmdSetDepthBiasEnableEXT;
#else
         screen->vk.CmdSetDepthBiasEnableEXT = (PFN_vkCmdSetDepthBiasEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetLogicOpEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetLogicOpEXT = (PFN_vkCmdSetLogicOpEXT)zink_stub_CmdSetLogicOpEXT;
#else
         screen->vk.CmdSetLogicOpEXT = (PFN_vkCmdSetLogicOpEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetPrimitiveRestartEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetPrimitiveRestartEnableEXT = (PFN_vkCmdSetPrimitiveRestartEnableEXT)zink_stub_CmdSetPrimitiveRestartEnableEXT;
#else
         screen->vk.CmdSetPrimitiveRestartEnableEXT = (PFN_vkCmdSetPrimitiveRestartEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetTessellationDomainOriginEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetTessellationDomainOriginEXT = (PFN_vkCmdSetTessellationDomainOriginEXT)zink_stub_CmdSetTessellationDomainOriginEXT;
#else
         screen->vk.CmdSetTessellationDomainOriginEXT = (PFN_vkCmdSetTessellationDomainOriginEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetDepthClampEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetDepthClampEnableEXT = (PFN_vkCmdSetDepthClampEnableEXT)zink_stub_CmdSetDepthClampEnableEXT;
#else
         screen->vk.CmdSetDepthClampEnableEXT = (PFN_vkCmdSetDepthClampEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetPolygonModeEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetPolygonModeEXT = (PFN_vkCmdSetPolygonModeEXT)zink_stub_CmdSetPolygonModeEXT;
#else
         screen->vk.CmdSetPolygonModeEXT = (PFN_vkCmdSetPolygonModeEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetRasterizationSamplesEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetRasterizationSamplesEXT = (PFN_vkCmdSetRasterizationSamplesEXT)zink_stub_CmdSetRasterizationSamplesEXT;
#else
         screen->vk.CmdSetRasterizationSamplesEXT = (PFN_vkCmdSetRasterizationSamplesEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetSampleMaskEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetSampleMaskEXT = (PFN_vkCmdSetSampleMaskEXT)zink_stub_CmdSetSampleMaskEXT;
#else
         screen->vk.CmdSetSampleMaskEXT = (PFN_vkCmdSetSampleMaskEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetAlphaToCoverageEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetAlphaToCoverageEnableEXT = (PFN_vkCmdSetAlphaToCoverageEnableEXT)zink_stub_CmdSetAlphaToCoverageEnableEXT;
#else
         screen->vk.CmdSetAlphaToCoverageEnableEXT = (PFN_vkCmdSetAlphaToCoverageEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetAlphaToOneEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetAlphaToOneEnableEXT = (PFN_vkCmdSetAlphaToOneEnableEXT)zink_stub_CmdSetAlphaToOneEnableEXT;
#else
         screen->vk.CmdSetAlphaToOneEnableEXT = (PFN_vkCmdSetAlphaToOneEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetLogicOpEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetLogicOpEnableEXT = (PFN_vkCmdSetLogicOpEnableEXT)zink_stub_CmdSetLogicOpEnableEXT;
#else
         screen->vk.CmdSetLogicOpEnableEXT = (PFN_vkCmdSetLogicOpEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetColorBlendEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetColorBlendEnableEXT = (PFN_vkCmdSetColorBlendEnableEXT)zink_stub_CmdSetColorBlendEnableEXT;
#else
         screen->vk.CmdSetColorBlendEnableEXT = (PFN_vkCmdSetColorBlendEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetColorBlendEquationEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetColorBlendEquationEXT = (PFN_vkCmdSetColorBlendEquationEXT)zink_stub_CmdSetColorBlendEquationEXT;
#else
         screen->vk.CmdSetColorBlendEquationEXT = (PFN_vkCmdSetColorBlendEquationEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetColorWriteMaskEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetColorWriteMaskEXT = (PFN_vkCmdSetColorWriteMaskEXT)zink_stub_CmdSetColorWriteMaskEXT;
#else
         screen->vk.CmdSetColorWriteMaskEXT = (PFN_vkCmdSetColorWriteMaskEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetRasterizationStreamEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetRasterizationStreamEXT = (PFN_vkCmdSetRasterizationStreamEXT)zink_stub_CmdSetRasterizationStreamEXT;
#else
         screen->vk.CmdSetRasterizationStreamEXT = (PFN_vkCmdSetRasterizationStreamEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetConservativeRasterizationModeEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetConservativeRasterizationModeEXT = (PFN_vkCmdSetConservativeRasterizationModeEXT)zink_stub_CmdSetConservativeRasterizationModeEXT;
#else
         screen->vk.CmdSetConservativeRasterizationModeEXT = (PFN_vkCmdSetConservativeRasterizationModeEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetExtraPrimitiveOverestimationSizeEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetExtraPrimitiveOverestimationSizeEXT = (PFN_vkCmdSetExtraPrimitiveOverestimationSizeEXT)zink_stub_CmdSetExtraPrimitiveOverestimationSizeEXT;
#else
         screen->vk.CmdSetExtraPrimitiveOverestimationSizeEXT = (PFN_vkCmdSetExtraPrimitiveOverestimationSizeEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetDepthClipEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetDepthClipEnableEXT = (PFN_vkCmdSetDepthClipEnableEXT)zink_stub_CmdSetDepthClipEnableEXT;
#else
         screen->vk.CmdSetDepthClipEnableEXT = (PFN_vkCmdSetDepthClipEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetSampleLocationsEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetSampleLocationsEnableEXT = (PFN_vkCmdSetSampleLocationsEnableEXT)zink_stub_CmdSetSampleLocationsEnableEXT;
#else
         screen->vk.CmdSetSampleLocationsEnableEXT = (PFN_vkCmdSetSampleLocationsEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetColorBlendAdvancedEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetColorBlendAdvancedEXT = (PFN_vkCmdSetColorBlendAdvancedEXT)zink_stub_CmdSetColorBlendAdvancedEXT;
#else
         screen->vk.CmdSetColorBlendAdvancedEXT = (PFN_vkCmdSetColorBlendAdvancedEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetProvokingVertexModeEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetProvokingVertexModeEXT = (PFN_vkCmdSetProvokingVertexModeEXT)zink_stub_CmdSetProvokingVertexModeEXT;
#else
         screen->vk.CmdSetProvokingVertexModeEXT = (PFN_vkCmdSetProvokingVertexModeEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetLineRasterizationModeEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetLineRasterizationModeEXT = (PFN_vkCmdSetLineRasterizationModeEXT)zink_stub_CmdSetLineRasterizationModeEXT;
#else
         screen->vk.CmdSetLineRasterizationModeEXT = (PFN_vkCmdSetLineRasterizationModeEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetLineStippleEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetLineStippleEnableEXT = (PFN_vkCmdSetLineStippleEnableEXT)zink_stub_CmdSetLineStippleEnableEXT;
#else
         screen->vk.CmdSetLineStippleEnableEXT = (PFN_vkCmdSetLineStippleEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetDepthClipNegativeOneToOneEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetDepthClipNegativeOneToOneEXT = (PFN_vkCmdSetDepthClipNegativeOneToOneEXT)zink_stub_CmdSetDepthClipNegativeOneToOneEXT;
#else
         screen->vk.CmdSetDepthClipNegativeOneToOneEXT = (PFN_vkCmdSetDepthClipNegativeOneToOneEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetViewportWScalingEnableNV) {
#ifndef NDEBUG
         screen->vk.CmdSetViewportWScalingEnableNV = (PFN_vkCmdSetViewportWScalingEnableNV)zink_stub_CmdSetViewportWScalingEnableNV;
#else
         screen->vk.CmdSetViewportWScalingEnableNV = (PFN_vkCmdSetViewportWScalingEnableNV)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetViewportSwizzleNV) {
#ifndef NDEBUG
         screen->vk.CmdSetViewportSwizzleNV = (PFN_vkCmdSetViewportSwizzleNV)zink_stub_CmdSetViewportSwizzleNV;
#else
         screen->vk.CmdSetViewportSwizzleNV = (PFN_vkCmdSetViewportSwizzleNV)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetCoverageToColorEnableNV) {
#ifndef NDEBUG
         screen->vk.CmdSetCoverageToColorEnableNV = (PFN_vkCmdSetCoverageToColorEnableNV)zink_stub_CmdSetCoverageToColorEnableNV;
#else
         screen->vk.CmdSetCoverageToColorEnableNV = (PFN_vkCmdSetCoverageToColorEnableNV)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetCoverageToColorLocationNV) {
#ifndef NDEBUG
         screen->vk.CmdSetCoverageToColorLocationNV = (PFN_vkCmdSetCoverageToColorLocationNV)zink_stub_CmdSetCoverageToColorLocationNV;
#else
         screen->vk.CmdSetCoverageToColorLocationNV = (PFN_vkCmdSetCoverageToColorLocationNV)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetCoverageModulationModeNV) {
#ifndef NDEBUG
         screen->vk.CmdSetCoverageModulationModeNV = (PFN_vkCmdSetCoverageModulationModeNV)zink_stub_CmdSetCoverageModulationModeNV;
#else
         screen->vk.CmdSetCoverageModulationModeNV = (PFN_vkCmdSetCoverageModulationModeNV)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetCoverageModulationTableEnableNV) {
#ifndef NDEBUG
         screen->vk.CmdSetCoverageModulationTableEnableNV = (PFN_vkCmdSetCoverageModulationTableEnableNV)zink_stub_CmdSetCoverageModulationTableEnableNV;
#else
         screen->vk.CmdSetCoverageModulationTableEnableNV = (PFN_vkCmdSetCoverageModulationTableEnableNV)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetCoverageModulationTableNV) {
#ifndef NDEBUG
         screen->vk.CmdSetCoverageModulationTableNV = (PFN_vkCmdSetCoverageModulationTableNV)zink_stub_CmdSetCoverageModulationTableNV;
#else
         screen->vk.CmdSetCoverageModulationTableNV = (PFN_vkCmdSetCoverageModulationTableNV)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetShadingRateImageEnableNV) {
#ifndef NDEBUG
         screen->vk.CmdSetShadingRateImageEnableNV = (PFN_vkCmdSetShadingRateImageEnableNV)zink_stub_CmdSetShadingRateImageEnableNV;
#else
         screen->vk.CmdSetShadingRateImageEnableNV = (PFN_vkCmdSetShadingRateImageEnableNV)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetRepresentativeFragmentTestEnableNV) {
#ifndef NDEBUG
         screen->vk.CmdSetRepresentativeFragmentTestEnableNV = (PFN_vkCmdSetRepresentativeFragmentTestEnableNV)zink_stub_CmdSetRepresentativeFragmentTestEnableNV;
#else
         screen->vk.CmdSetRepresentativeFragmentTestEnableNV = (PFN_vkCmdSetRepresentativeFragmentTestEnableNV)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetCoverageReductionModeNV) {
#ifndef NDEBUG
         screen->vk.CmdSetCoverageReductionModeNV = (PFN_vkCmdSetCoverageReductionModeNV)zink_stub_CmdSetCoverageReductionModeNV;
#else
         screen->vk.CmdSetCoverageReductionModeNV = (PFN_vkCmdSetCoverageReductionModeNV)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_EXT_attachment_feedback_loop_layout) {
   }
   if (screen->info.have_EXT_attachment_feedback_loop_dynamic_state) {
#ifdef _WIN32
      if (!screen->vk.CmdSetAttachmentFeedbackLoopEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetAttachmentFeedbackLoopEnableEXT = (PFN_vkCmdSetAttachmentFeedbackLoopEnableEXT)zink_stub_CmdSetAttachmentFeedbackLoopEnableEXT;
#else
         screen->vk.CmdSetAttachmentFeedbackLoopEnableEXT = (PFN_vkCmdSetAttachmentFeedbackLoopEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_NV_device_generated_commands) {
#ifdef _WIN32
      if (!screen->vk.GetGeneratedCommandsMemoryRequirementsNV) {
#ifndef NDEBUG
         screen->vk.GetGeneratedCommandsMemoryRequirementsNV = (PFN_vkGetGeneratedCommandsMemoryRequirementsNV)zink_stub_GetGeneratedCommandsMemoryRequirementsNV;
#else
         screen->vk.GetGeneratedCommandsMemoryRequirementsNV = (PFN_vkGetGeneratedCommandsMemoryRequirementsNV)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdPreprocessGeneratedCommandsNV) {
#ifndef NDEBUG
         screen->vk.CmdPreprocessGeneratedCommandsNV = (PFN_vkCmdPreprocessGeneratedCommandsNV)zink_stub_CmdPreprocessGeneratedCommandsNV;
#else
         screen->vk.CmdPreprocessGeneratedCommandsNV = (PFN_vkCmdPreprocessGeneratedCommandsNV)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdExecuteGeneratedCommandsNV) {
#ifndef NDEBUG
         screen->vk.CmdExecuteGeneratedCommandsNV = (PFN_vkCmdExecuteGeneratedCommandsNV)zink_stub_CmdExecuteGeneratedCommandsNV;
#else
         screen->vk.CmdExecuteGeneratedCommandsNV = (PFN_vkCmdExecuteGeneratedCommandsNV)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdBindPipelineShaderGroupNV) {
#ifndef NDEBUG
         screen->vk.CmdBindPipelineShaderGroupNV = (PFN_vkCmdBindPipelineShaderGroupNV)zink_stub_CmdBindPipelineShaderGroupNV;
#else
         screen->vk.CmdBindPipelineShaderGroupNV = (PFN_vkCmdBindPipelineShaderGroupNV)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CreateIndirectCommandsLayoutNV) {
#ifndef NDEBUG
         screen->vk.CreateIndirectCommandsLayoutNV = (PFN_vkCreateIndirectCommandsLayoutNV)zink_stub_CreateIndirectCommandsLayoutNV;
#else
         screen->vk.CreateIndirectCommandsLayoutNV = (PFN_vkCreateIndirectCommandsLayoutNV)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.DestroyIndirectCommandsLayoutNV) {
#ifndef NDEBUG
         screen->vk.DestroyIndirectCommandsLayoutNV = (PFN_vkDestroyIndirectCommandsLayoutNV)zink_stub_DestroyIndirectCommandsLayoutNV;
#else
         screen->vk.DestroyIndirectCommandsLayoutNV = (PFN_vkDestroyIndirectCommandsLayoutNV)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_EXT_fragment_shader_interlock) {
   }
   if (screen->info.have_EXT_sample_locations) {
#ifdef _WIN32
      if (!screen->vk.CmdSetSampleLocationsEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetSampleLocationsEXT = (PFN_vkCmdSetSampleLocationsEXT)zink_stub_CmdSetSampleLocationsEXT;
#else
         screen->vk.CmdSetSampleLocationsEXT = (PFN_vkCmdSetSampleLocationsEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_KHR_shader_draw_parameters) {
   }
   if (screen->info.have_KHR_sampler_mirror_clamp_to_edge) {
   }
   if (screen->info.have_EXT_descriptor_buffer) {
#ifdef _WIN32
      if (!screen->vk.GetDescriptorSetLayoutSizeEXT) {
#ifndef NDEBUG
         screen->vk.GetDescriptorSetLayoutSizeEXT = (PFN_vkGetDescriptorSetLayoutSizeEXT)zink_stub_GetDescriptorSetLayoutSizeEXT;
#else
         screen->vk.GetDescriptorSetLayoutSizeEXT = (PFN_vkGetDescriptorSetLayoutSizeEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.GetDescriptorSetLayoutBindingOffsetEXT) {
#ifndef NDEBUG
         screen->vk.GetDescriptorSetLayoutBindingOffsetEXT = (PFN_vkGetDescriptorSetLayoutBindingOffsetEXT)zink_stub_GetDescriptorSetLayoutBindingOffsetEXT;
#else
         screen->vk.GetDescriptorSetLayoutBindingOffsetEXT = (PFN_vkGetDescriptorSetLayoutBindingOffsetEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.GetDescriptorEXT) {
#ifndef NDEBUG
         screen->vk.GetDescriptorEXT = (PFN_vkGetDescriptorEXT)zink_stub_GetDescriptorEXT;
#else
         screen->vk.GetDescriptorEXT = (PFN_vkGetDescriptorEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdBindDescriptorBuffersEXT) {
#ifndef NDEBUG
         screen->vk.CmdBindDescriptorBuffersEXT = (PFN_vkCmdBindDescriptorBuffersEXT)zink_stub_CmdBindDescriptorBuffersEXT;
#else
         screen->vk.CmdBindDescriptorBuffersEXT = (PFN_vkCmdBindDescriptorBuffersEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetDescriptorBufferOffsetsEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetDescriptorBufferOffsetsEXT = (PFN_vkCmdSetDescriptorBufferOffsetsEXT)zink_stub_CmdSetDescriptorBufferOffsetsEXT;
#else
         screen->vk.CmdSetDescriptorBufferOffsetsEXT = (PFN_vkCmdSetDescriptorBufferOffsetsEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdBindDescriptorBufferEmbeddedSamplersEXT) {
#ifndef NDEBUG
         screen->vk.CmdBindDescriptorBufferEmbeddedSamplersEXT = (PFN_vkCmdBindDescriptorBufferEmbeddedSamplersEXT)zink_stub_CmdBindDescriptorBufferEmbeddedSamplersEXT;
#else
         screen->vk.CmdBindDescriptorBufferEmbeddedSamplersEXT = (PFN_vkCmdBindDescriptorBufferEmbeddedSamplersEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.GetBufferOpaqueCaptureDescriptorDataEXT) {
#ifndef NDEBUG
         screen->vk.GetBufferOpaqueCaptureDescriptorDataEXT = (PFN_vkGetBufferOpaqueCaptureDescriptorDataEXT)zink_stub_GetBufferOpaqueCaptureDescriptorDataEXT;
#else
         screen->vk.GetBufferOpaqueCaptureDescriptorDataEXT = (PFN_vkGetBufferOpaqueCaptureDescriptorDataEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.GetImageOpaqueCaptureDescriptorDataEXT) {
#ifndef NDEBUG
         screen->vk.GetImageOpaqueCaptureDescriptorDataEXT = (PFN_vkGetImageOpaqueCaptureDescriptorDataEXT)zink_stub_GetImageOpaqueCaptureDescriptorDataEXT;
#else
         screen->vk.GetImageOpaqueCaptureDescriptorDataEXT = (PFN_vkGetImageOpaqueCaptureDescriptorDataEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.GetImageViewOpaqueCaptureDescriptorDataEXT) {
#ifndef NDEBUG
         screen->vk.GetImageViewOpaqueCaptureDescriptorDataEXT = (PFN_vkGetImageViewOpaqueCaptureDescriptorDataEXT)zink_stub_GetImageViewOpaqueCaptureDescriptorDataEXT;
#else
         screen->vk.GetImageViewOpaqueCaptureDescriptorDataEXT = (PFN_vkGetImageViewOpaqueCaptureDescriptorDataEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.GetSamplerOpaqueCaptureDescriptorDataEXT) {
#ifndef NDEBUG
         screen->vk.GetSamplerOpaqueCaptureDescriptorDataEXT = (PFN_vkGetSamplerOpaqueCaptureDescriptorDataEXT)zink_stub_GetSamplerOpaqueCaptureDescriptorDataEXT;
#else
         screen->vk.GetSamplerOpaqueCaptureDescriptorDataEXT = (PFN_vkGetSamplerOpaqueCaptureDescriptorDataEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.GetAccelerationStructureOpaqueCaptureDescriptorDataEXT) {
#ifndef NDEBUG
         screen->vk.GetAccelerationStructureOpaqueCaptureDescriptorDataEXT = (PFN_vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT)zink_stub_GetAccelerationStructureOpaqueCaptureDescriptorDataEXT;
#else
         screen->vk.GetAccelerationStructureOpaqueCaptureDescriptorDataEXT = (PFN_vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_EXT_conditional_rendering) {
#ifdef _WIN32
      if (!screen->vk.CmdBeginConditionalRenderingEXT) {
#ifndef NDEBUG
         screen->vk.CmdBeginConditionalRenderingEXT = (PFN_vkCmdBeginConditionalRenderingEXT)zink_stub_CmdBeginConditionalRenderingEXT;
#else
         screen->vk.CmdBeginConditionalRenderingEXT = (PFN_vkCmdBeginConditionalRenderingEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdEndConditionalRenderingEXT) {
#ifndef NDEBUG
         screen->vk.CmdEndConditionalRenderingEXT = (PFN_vkCmdEndConditionalRenderingEXT)zink_stub_CmdEndConditionalRenderingEXT;
#else
         screen->vk.CmdEndConditionalRenderingEXT = (PFN_vkCmdEndConditionalRenderingEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_EXT_transform_feedback) {
#ifdef _WIN32
      if (!screen->vk.CmdBindTransformFeedbackBuffersEXT) {
#ifndef NDEBUG
         screen->vk.CmdBindTransformFeedbackBuffersEXT = (PFN_vkCmdBindTransformFeedbackBuffersEXT)zink_stub_CmdBindTransformFeedbackBuffersEXT;
#else
         screen->vk.CmdBindTransformFeedbackBuffersEXT = (PFN_vkCmdBindTransformFeedbackBuffersEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdBeginTransformFeedbackEXT) {
#ifndef NDEBUG
         screen->vk.CmdBeginTransformFeedbackEXT = (PFN_vkCmdBeginTransformFeedbackEXT)zink_stub_CmdBeginTransformFeedbackEXT;
#else
         screen->vk.CmdBeginTransformFeedbackEXT = (PFN_vkCmdBeginTransformFeedbackEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdEndTransformFeedbackEXT) {
#ifndef NDEBUG
         screen->vk.CmdEndTransformFeedbackEXT = (PFN_vkCmdEndTransformFeedbackEXT)zink_stub_CmdEndTransformFeedbackEXT;
#else
         screen->vk.CmdEndTransformFeedbackEXT = (PFN_vkCmdEndTransformFeedbackEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdBeginQueryIndexedEXT) {
#ifndef NDEBUG
         screen->vk.CmdBeginQueryIndexedEXT = (PFN_vkCmdBeginQueryIndexedEXT)zink_stub_CmdBeginQueryIndexedEXT;
#else
         screen->vk.CmdBeginQueryIndexedEXT = (PFN_vkCmdBeginQueryIndexedEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdEndQueryIndexedEXT) {
#ifndef NDEBUG
         screen->vk.CmdEndQueryIndexedEXT = (PFN_vkCmdEndQueryIndexedEXT)zink_stub_CmdEndQueryIndexedEXT;
#else
         screen->vk.CmdEndQueryIndexedEXT = (PFN_vkCmdEndQueryIndexedEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdDrawIndirectByteCountEXT) {
#ifndef NDEBUG
         screen->vk.CmdDrawIndirectByteCountEXT = (PFN_vkCmdDrawIndirectByteCountEXT)zink_stub_CmdDrawIndirectByteCountEXT;
#else
         screen->vk.CmdDrawIndirectByteCountEXT = (PFN_vkCmdDrawIndirectByteCountEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_EXT_index_type_uint8) {
   }
   if (screen->info.have_KHR_image_format_list) {
   }
   if (screen->info.have_KHR_sampler_ycbcr_conversion) {
#ifdef _WIN32
      if (!screen->vk.CreateSamplerYcbcrConversionKHR) {
#ifndef NDEBUG
         screen->vk.CreateSamplerYcbcrConversionKHR = (PFN_vkCreateSamplerYcbcrConversionKHR)zink_stub_CreateSamplerYcbcrConversionKHR;
#else
         screen->vk.CreateSamplerYcbcrConversionKHR = (PFN_vkCreateSamplerYcbcrConversionKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.DestroySamplerYcbcrConversionKHR) {
#ifndef NDEBUG
         screen->vk.DestroySamplerYcbcrConversionKHR = (PFN_vkDestroySamplerYcbcrConversionKHR)zink_stub_DestroySamplerYcbcrConversionKHR;
#else
         screen->vk.DestroySamplerYcbcrConversionKHR = (PFN_vkDestroySamplerYcbcrConversionKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_KHR_imageless_framebuffer) {
   }
   if (screen->info.have_EXT_robustness2) {
   }
   if (screen->info.have_EXT_image_robustness) {
   }
   if (screen->info.have_EXT_image_drm_format_modifier) {
#ifdef _WIN32
      if (!screen->vk.GetImageDrmFormatModifierPropertiesEXT) {
#ifndef NDEBUG
         screen->vk.GetImageDrmFormatModifierPropertiesEXT = (PFN_vkGetImageDrmFormatModifierPropertiesEXT)zink_stub_GetImageDrmFormatModifierPropertiesEXT;
#else
         screen->vk.GetImageDrmFormatModifierPropertiesEXT = (PFN_vkGetImageDrmFormatModifierPropertiesEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_EXT_vertex_attribute_divisor) {
   }
   if (screen->info.have_EXT_calibrated_timestamps) {
#ifdef _WIN32
      if (!screen->vk.GetCalibratedTimestampsEXT) {
#ifndef NDEBUG
         screen->vk.GetCalibratedTimestampsEXT = (PFN_vkGetCalibratedTimestampsEXT)zink_stub_GetCalibratedTimestampsEXT;
#else
         screen->vk.GetCalibratedTimestampsEXT = (PFN_vkGetCalibratedTimestampsEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_NV_linear_color_attachment) {
   }
   if (screen->info.have_KHR_dynamic_rendering) {
#ifdef _WIN32
      if (!screen->vk.CmdBeginRenderingKHR) {
#ifndef NDEBUG
         screen->vk.CmdBeginRenderingKHR = (PFN_vkCmdBeginRenderingKHR)zink_stub_CmdBeginRenderingKHR;
#else
         screen->vk.CmdBeginRenderingKHR = (PFN_vkCmdBeginRenderingKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdEndRenderingKHR) {
#ifndef NDEBUG
         screen->vk.CmdEndRenderingKHR = (PFN_vkCmdEndRenderingKHR)zink_stub_CmdEndRenderingKHR;
#else
         screen->vk.CmdEndRenderingKHR = (PFN_vkCmdEndRenderingKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_KHR_dynamic_rendering_local_read) {
#ifdef _WIN32
      if (!screen->vk.CmdSetRenderingAttachmentLocationsKHR) {
#ifndef NDEBUG
         screen->vk.CmdSetRenderingAttachmentLocationsKHR = (PFN_vkCmdSetRenderingAttachmentLocationsKHR)zink_stub_CmdSetRenderingAttachmentLocationsKHR;
#else
         screen->vk.CmdSetRenderingAttachmentLocationsKHR = (PFN_vkCmdSetRenderingAttachmentLocationsKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetRenderingInputAttachmentIndicesKHR) {
#ifndef NDEBUG
         screen->vk.CmdSetRenderingInputAttachmentIndicesKHR = (PFN_vkCmdSetRenderingInputAttachmentIndicesKHR)zink_stub_CmdSetRenderingInputAttachmentIndicesKHR;
#else
         screen->vk.CmdSetRenderingInputAttachmentIndicesKHR = (PFN_vkCmdSetRenderingInputAttachmentIndicesKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_EXT_multisampled_render_to_single_sampled) {
   }
   if (screen->info.have_KHR_shader_clock) {
   }
   if (screen->info.have_INTEL_shader_integer_functions2) {
   }
   if (screen->info.have_EXT_sampler_filter_minmax) {
   }
   if (screen->info.have_EXT_custom_border_color) {
   }
   if (screen->info.have_EXT_non_seamless_cube_map) {
   }
   if (screen->info.have_EXT_border_color_swizzle) {
   }
   if (screen->info.have_EXT_blend_operation_advanced) {
   }
   if (screen->info.have_EXT_extended_dynamic_state) {
#ifdef _WIN32
      if (!screen->vk.CmdSetCullModeEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetCullModeEXT = (PFN_vkCmdSetCullModeEXT)zink_stub_CmdSetCullModeEXT;
#else
         screen->vk.CmdSetCullModeEXT = (PFN_vkCmdSetCullModeEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetFrontFaceEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetFrontFaceEXT = (PFN_vkCmdSetFrontFaceEXT)zink_stub_CmdSetFrontFaceEXT;
#else
         screen->vk.CmdSetFrontFaceEXT = (PFN_vkCmdSetFrontFaceEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetPrimitiveTopologyEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetPrimitiveTopologyEXT = (PFN_vkCmdSetPrimitiveTopologyEXT)zink_stub_CmdSetPrimitiveTopologyEXT;
#else
         screen->vk.CmdSetPrimitiveTopologyEXT = (PFN_vkCmdSetPrimitiveTopologyEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetViewportWithCountEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetViewportWithCountEXT = (PFN_vkCmdSetViewportWithCountEXT)zink_stub_CmdSetViewportWithCountEXT;
#else
         screen->vk.CmdSetViewportWithCountEXT = (PFN_vkCmdSetViewportWithCountEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetScissorWithCountEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetScissorWithCountEXT = (PFN_vkCmdSetScissorWithCountEXT)zink_stub_CmdSetScissorWithCountEXT;
#else
         screen->vk.CmdSetScissorWithCountEXT = (PFN_vkCmdSetScissorWithCountEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdBindVertexBuffers2EXT) {
#ifndef NDEBUG
         screen->vk.CmdBindVertexBuffers2EXT = (PFN_vkCmdBindVertexBuffers2EXT)zink_stub_CmdBindVertexBuffers2EXT;
#else
         screen->vk.CmdBindVertexBuffers2EXT = (PFN_vkCmdBindVertexBuffers2EXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetDepthTestEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetDepthTestEnableEXT = (PFN_vkCmdSetDepthTestEnableEXT)zink_stub_CmdSetDepthTestEnableEXT;
#else
         screen->vk.CmdSetDepthTestEnableEXT = (PFN_vkCmdSetDepthTestEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetDepthWriteEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetDepthWriteEnableEXT = (PFN_vkCmdSetDepthWriteEnableEXT)zink_stub_CmdSetDepthWriteEnableEXT;
#else
         screen->vk.CmdSetDepthWriteEnableEXT = (PFN_vkCmdSetDepthWriteEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetDepthCompareOpEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetDepthCompareOpEXT = (PFN_vkCmdSetDepthCompareOpEXT)zink_stub_CmdSetDepthCompareOpEXT;
#else
         screen->vk.CmdSetDepthCompareOpEXT = (PFN_vkCmdSetDepthCompareOpEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetDepthBoundsTestEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetDepthBoundsTestEnableEXT = (PFN_vkCmdSetDepthBoundsTestEnableEXT)zink_stub_CmdSetDepthBoundsTestEnableEXT;
#else
         screen->vk.CmdSetDepthBoundsTestEnableEXT = (PFN_vkCmdSetDepthBoundsTestEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetStencilTestEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetStencilTestEnableEXT = (PFN_vkCmdSetStencilTestEnableEXT)zink_stub_CmdSetStencilTestEnableEXT;
#else
         screen->vk.CmdSetStencilTestEnableEXT = (PFN_vkCmdSetStencilTestEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetStencilOpEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetStencilOpEXT = (PFN_vkCmdSetStencilOpEXT)zink_stub_CmdSetStencilOpEXT;
#else
         screen->vk.CmdSetStencilOpEXT = (PFN_vkCmdSetStencilOpEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_EXT_extended_dynamic_state2) {
#ifdef _WIN32
      if (!screen->vk.CmdSetPatchControlPointsEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetPatchControlPointsEXT = (PFN_vkCmdSetPatchControlPointsEXT)zink_stub_CmdSetPatchControlPointsEXT;
#else
         screen->vk.CmdSetPatchControlPointsEXT = (PFN_vkCmdSetPatchControlPointsEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetRasterizerDiscardEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetRasterizerDiscardEnableEXT = (PFN_vkCmdSetRasterizerDiscardEnableEXT)zink_stub_CmdSetRasterizerDiscardEnableEXT;
#else
         screen->vk.CmdSetRasterizerDiscardEnableEXT = (PFN_vkCmdSetRasterizerDiscardEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetDepthBiasEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetDepthBiasEnableEXT = (PFN_vkCmdSetDepthBiasEnableEXT)zink_stub_CmdSetDepthBiasEnableEXT;
#else
         screen->vk.CmdSetDepthBiasEnableEXT = (PFN_vkCmdSetDepthBiasEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetLogicOpEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetLogicOpEXT = (PFN_vkCmdSetLogicOpEXT)zink_stub_CmdSetLogicOpEXT;
#else
         screen->vk.CmdSetLogicOpEXT = (PFN_vkCmdSetLogicOpEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetPrimitiveRestartEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetPrimitiveRestartEnableEXT = (PFN_vkCmdSetPrimitiveRestartEnableEXT)zink_stub_CmdSetPrimitiveRestartEnableEXT;
#else
         screen->vk.CmdSetPrimitiveRestartEnableEXT = (PFN_vkCmdSetPrimitiveRestartEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_EXT_extended_dynamic_state3) {
#ifdef _WIN32
      if (!screen->vk.CmdSetDepthClampEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetDepthClampEnableEXT = (PFN_vkCmdSetDepthClampEnableEXT)zink_stub_CmdSetDepthClampEnableEXT;
#else
         screen->vk.CmdSetDepthClampEnableEXT = (PFN_vkCmdSetDepthClampEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetPolygonModeEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetPolygonModeEXT = (PFN_vkCmdSetPolygonModeEXT)zink_stub_CmdSetPolygonModeEXT;
#else
         screen->vk.CmdSetPolygonModeEXT = (PFN_vkCmdSetPolygonModeEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetRasterizationSamplesEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetRasterizationSamplesEXT = (PFN_vkCmdSetRasterizationSamplesEXT)zink_stub_CmdSetRasterizationSamplesEXT;
#else
         screen->vk.CmdSetRasterizationSamplesEXT = (PFN_vkCmdSetRasterizationSamplesEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetSampleMaskEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetSampleMaskEXT = (PFN_vkCmdSetSampleMaskEXT)zink_stub_CmdSetSampleMaskEXT;
#else
         screen->vk.CmdSetSampleMaskEXT = (PFN_vkCmdSetSampleMaskEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetAlphaToCoverageEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetAlphaToCoverageEnableEXT = (PFN_vkCmdSetAlphaToCoverageEnableEXT)zink_stub_CmdSetAlphaToCoverageEnableEXT;
#else
         screen->vk.CmdSetAlphaToCoverageEnableEXT = (PFN_vkCmdSetAlphaToCoverageEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetAlphaToOneEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetAlphaToOneEnableEXT = (PFN_vkCmdSetAlphaToOneEnableEXT)zink_stub_CmdSetAlphaToOneEnableEXT;
#else
         screen->vk.CmdSetAlphaToOneEnableEXT = (PFN_vkCmdSetAlphaToOneEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetLogicOpEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetLogicOpEnableEXT = (PFN_vkCmdSetLogicOpEnableEXT)zink_stub_CmdSetLogicOpEnableEXT;
#else
         screen->vk.CmdSetLogicOpEnableEXT = (PFN_vkCmdSetLogicOpEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetColorBlendEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetColorBlendEnableEXT = (PFN_vkCmdSetColorBlendEnableEXT)zink_stub_CmdSetColorBlendEnableEXT;
#else
         screen->vk.CmdSetColorBlendEnableEXT = (PFN_vkCmdSetColorBlendEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetColorBlendEquationEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetColorBlendEquationEXT = (PFN_vkCmdSetColorBlendEquationEXT)zink_stub_CmdSetColorBlendEquationEXT;
#else
         screen->vk.CmdSetColorBlendEquationEXT = (PFN_vkCmdSetColorBlendEquationEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetColorWriteMaskEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetColorWriteMaskEXT = (PFN_vkCmdSetColorWriteMaskEXT)zink_stub_CmdSetColorWriteMaskEXT;
#else
         screen->vk.CmdSetColorWriteMaskEXT = (PFN_vkCmdSetColorWriteMaskEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetTessellationDomainOriginEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetTessellationDomainOriginEXT = (PFN_vkCmdSetTessellationDomainOriginEXT)zink_stub_CmdSetTessellationDomainOriginEXT;
#else
         screen->vk.CmdSetTessellationDomainOriginEXT = (PFN_vkCmdSetTessellationDomainOriginEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetRasterizationStreamEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetRasterizationStreamEXT = (PFN_vkCmdSetRasterizationStreamEXT)zink_stub_CmdSetRasterizationStreamEXT;
#else
         screen->vk.CmdSetRasterizationStreamEXT = (PFN_vkCmdSetRasterizationStreamEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetConservativeRasterizationModeEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetConservativeRasterizationModeEXT = (PFN_vkCmdSetConservativeRasterizationModeEXT)zink_stub_CmdSetConservativeRasterizationModeEXT;
#else
         screen->vk.CmdSetConservativeRasterizationModeEXT = (PFN_vkCmdSetConservativeRasterizationModeEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetExtraPrimitiveOverestimationSizeEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetExtraPrimitiveOverestimationSizeEXT = (PFN_vkCmdSetExtraPrimitiveOverestimationSizeEXT)zink_stub_CmdSetExtraPrimitiveOverestimationSizeEXT;
#else
         screen->vk.CmdSetExtraPrimitiveOverestimationSizeEXT = (PFN_vkCmdSetExtraPrimitiveOverestimationSizeEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetDepthClipEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetDepthClipEnableEXT = (PFN_vkCmdSetDepthClipEnableEXT)zink_stub_CmdSetDepthClipEnableEXT;
#else
         screen->vk.CmdSetDepthClipEnableEXT = (PFN_vkCmdSetDepthClipEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetSampleLocationsEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetSampleLocationsEnableEXT = (PFN_vkCmdSetSampleLocationsEnableEXT)zink_stub_CmdSetSampleLocationsEnableEXT;
#else
         screen->vk.CmdSetSampleLocationsEnableEXT = (PFN_vkCmdSetSampleLocationsEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetColorBlendAdvancedEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetColorBlendAdvancedEXT = (PFN_vkCmdSetColorBlendAdvancedEXT)zink_stub_CmdSetColorBlendAdvancedEXT;
#else
         screen->vk.CmdSetColorBlendAdvancedEXT = (PFN_vkCmdSetColorBlendAdvancedEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetProvokingVertexModeEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetProvokingVertexModeEXT = (PFN_vkCmdSetProvokingVertexModeEXT)zink_stub_CmdSetProvokingVertexModeEXT;
#else
         screen->vk.CmdSetProvokingVertexModeEXT = (PFN_vkCmdSetProvokingVertexModeEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetLineRasterizationModeEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetLineRasterizationModeEXT = (PFN_vkCmdSetLineRasterizationModeEXT)zink_stub_CmdSetLineRasterizationModeEXT;
#else
         screen->vk.CmdSetLineRasterizationModeEXT = (PFN_vkCmdSetLineRasterizationModeEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetLineStippleEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetLineStippleEnableEXT = (PFN_vkCmdSetLineStippleEnableEXT)zink_stub_CmdSetLineStippleEnableEXT;
#else
         screen->vk.CmdSetLineStippleEnableEXT = (PFN_vkCmdSetLineStippleEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetDepthClipNegativeOneToOneEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetDepthClipNegativeOneToOneEXT = (PFN_vkCmdSetDepthClipNegativeOneToOneEXT)zink_stub_CmdSetDepthClipNegativeOneToOneEXT;
#else
         screen->vk.CmdSetDepthClipNegativeOneToOneEXT = (PFN_vkCmdSetDepthClipNegativeOneToOneEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetViewportWScalingEnableNV) {
#ifndef NDEBUG
         screen->vk.CmdSetViewportWScalingEnableNV = (PFN_vkCmdSetViewportWScalingEnableNV)zink_stub_CmdSetViewportWScalingEnableNV;
#else
         screen->vk.CmdSetViewportWScalingEnableNV = (PFN_vkCmdSetViewportWScalingEnableNV)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetViewportSwizzleNV) {
#ifndef NDEBUG
         screen->vk.CmdSetViewportSwizzleNV = (PFN_vkCmdSetViewportSwizzleNV)zink_stub_CmdSetViewportSwizzleNV;
#else
         screen->vk.CmdSetViewportSwizzleNV = (PFN_vkCmdSetViewportSwizzleNV)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetCoverageToColorEnableNV) {
#ifndef NDEBUG
         screen->vk.CmdSetCoverageToColorEnableNV = (PFN_vkCmdSetCoverageToColorEnableNV)zink_stub_CmdSetCoverageToColorEnableNV;
#else
         screen->vk.CmdSetCoverageToColorEnableNV = (PFN_vkCmdSetCoverageToColorEnableNV)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetCoverageToColorLocationNV) {
#ifndef NDEBUG
         screen->vk.CmdSetCoverageToColorLocationNV = (PFN_vkCmdSetCoverageToColorLocationNV)zink_stub_CmdSetCoverageToColorLocationNV;
#else
         screen->vk.CmdSetCoverageToColorLocationNV = (PFN_vkCmdSetCoverageToColorLocationNV)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetCoverageModulationModeNV) {
#ifndef NDEBUG
         screen->vk.CmdSetCoverageModulationModeNV = (PFN_vkCmdSetCoverageModulationModeNV)zink_stub_CmdSetCoverageModulationModeNV;
#else
         screen->vk.CmdSetCoverageModulationModeNV = (PFN_vkCmdSetCoverageModulationModeNV)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetCoverageModulationTableEnableNV) {
#ifndef NDEBUG
         screen->vk.CmdSetCoverageModulationTableEnableNV = (PFN_vkCmdSetCoverageModulationTableEnableNV)zink_stub_CmdSetCoverageModulationTableEnableNV;
#else
         screen->vk.CmdSetCoverageModulationTableEnableNV = (PFN_vkCmdSetCoverageModulationTableEnableNV)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetCoverageModulationTableNV) {
#ifndef NDEBUG
         screen->vk.CmdSetCoverageModulationTableNV = (PFN_vkCmdSetCoverageModulationTableNV)zink_stub_CmdSetCoverageModulationTableNV;
#else
         screen->vk.CmdSetCoverageModulationTableNV = (PFN_vkCmdSetCoverageModulationTableNV)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetShadingRateImageEnableNV) {
#ifndef NDEBUG
         screen->vk.CmdSetShadingRateImageEnableNV = (PFN_vkCmdSetShadingRateImageEnableNV)zink_stub_CmdSetShadingRateImageEnableNV;
#else
         screen->vk.CmdSetShadingRateImageEnableNV = (PFN_vkCmdSetShadingRateImageEnableNV)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetRepresentativeFragmentTestEnableNV) {
#ifndef NDEBUG
         screen->vk.CmdSetRepresentativeFragmentTestEnableNV = (PFN_vkCmdSetRepresentativeFragmentTestEnableNV)zink_stub_CmdSetRepresentativeFragmentTestEnableNV;
#else
         screen->vk.CmdSetRepresentativeFragmentTestEnableNV = (PFN_vkCmdSetRepresentativeFragmentTestEnableNV)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdSetCoverageReductionModeNV) {
#ifndef NDEBUG
         screen->vk.CmdSetCoverageReductionModeNV = (PFN_vkCmdSetCoverageReductionModeNV)zink_stub_CmdSetCoverageReductionModeNV;
#else
         screen->vk.CmdSetCoverageReductionModeNV = (PFN_vkCmdSetCoverageReductionModeNV)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_EXT_pipeline_creation_cache_control) {
   }
   if (screen->info.have_EXT_shader_stencil_export) {
   }
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
   if (screen->info.have_KHR_portability_subset) {
   }
#endif
   if (screen->info.have_NV_compute_shader_derivatives) {
   }
   if (screen->info.have_KHR_timeline_semaphore) {
#ifdef _WIN32
      if (!screen->vk.GetSemaphoreCounterValueKHR) {
#ifndef NDEBUG
         screen->vk.GetSemaphoreCounterValueKHR = (PFN_vkGetSemaphoreCounterValueKHR)zink_stub_GetSemaphoreCounterValueKHR;
#else
         screen->vk.GetSemaphoreCounterValueKHR = (PFN_vkGetSemaphoreCounterValueKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.WaitSemaphoresKHR) {
#ifndef NDEBUG
         screen->vk.WaitSemaphoresKHR = (PFN_vkWaitSemaphoresKHR)zink_stub_WaitSemaphoresKHR;
#else
         screen->vk.WaitSemaphoresKHR = (PFN_vkWaitSemaphoresKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.SignalSemaphoreKHR) {
#ifndef NDEBUG
         screen->vk.SignalSemaphoreKHR = (PFN_vkSignalSemaphoreKHR)zink_stub_SignalSemaphoreKHR;
#else
         screen->vk.SignalSemaphoreKHR = (PFN_vkSignalSemaphoreKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_EXT_color_write_enable) {
#ifdef _WIN32
      if (!screen->vk.CmdSetColorWriteEnableEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetColorWriteEnableEXT = (PFN_vkCmdSetColorWriteEnableEXT)zink_stub_CmdSetColorWriteEnableEXT;
#else
         screen->vk.CmdSetColorWriteEnableEXT = (PFN_vkCmdSetColorWriteEnableEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_EXT_4444_formats) {
   }
   if (screen->info.have_EXT_host_image_copy) {
#ifdef _WIN32
      if (!screen->vk.CopyMemoryToImageEXT) {
#ifndef NDEBUG
         screen->vk.CopyMemoryToImageEXT = (PFN_vkCopyMemoryToImageEXT)zink_stub_CopyMemoryToImageEXT;
#else
         screen->vk.CopyMemoryToImageEXT = (PFN_vkCopyMemoryToImageEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CopyImageToMemoryEXT) {
#ifndef NDEBUG
         screen->vk.CopyImageToMemoryEXT = (PFN_vkCopyImageToMemoryEXT)zink_stub_CopyImageToMemoryEXT;
#else
         screen->vk.CopyImageToMemoryEXT = (PFN_vkCopyImageToMemoryEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CopyImageToImageEXT) {
#ifndef NDEBUG
         screen->vk.CopyImageToImageEXT = (PFN_vkCopyImageToImageEXT)zink_stub_CopyImageToImageEXT;
#else
         screen->vk.CopyImageToImageEXT = (PFN_vkCopyImageToImageEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.TransitionImageLayoutEXT) {
#ifndef NDEBUG
         screen->vk.TransitionImageLayoutEXT = (PFN_vkTransitionImageLayoutEXT)zink_stub_TransitionImageLayoutEXT;
#else
         screen->vk.TransitionImageLayoutEXT = (PFN_vkTransitionImageLayoutEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.GetImageSubresourceLayout2EXT) {
#ifndef NDEBUG
         screen->vk.GetImageSubresourceLayout2EXT = (PFN_vkGetImageSubresourceLayout2EXT)zink_stub_GetImageSubresourceLayout2EXT;
#else
         screen->vk.GetImageSubresourceLayout2EXT = (PFN_vkGetImageSubresourceLayout2EXT)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_EXT_scalar_block_layout) {
   }
   if (screen->info.have_KHR_swapchain) {
#ifdef _WIN32
      if (!screen->vk.CreateSwapchainKHR) {
#ifndef NDEBUG
         screen->vk.CreateSwapchainKHR = (PFN_vkCreateSwapchainKHR)zink_stub_CreateSwapchainKHR;
#else
         screen->vk.CreateSwapchainKHR = (PFN_vkCreateSwapchainKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.DestroySwapchainKHR) {
#ifndef NDEBUG
         screen->vk.DestroySwapchainKHR = (PFN_vkDestroySwapchainKHR)zink_stub_DestroySwapchainKHR;
#else
         screen->vk.DestroySwapchainKHR = (PFN_vkDestroySwapchainKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.GetSwapchainImagesKHR) {
#ifndef NDEBUG
         screen->vk.GetSwapchainImagesKHR = (PFN_vkGetSwapchainImagesKHR)zink_stub_GetSwapchainImagesKHR;
#else
         screen->vk.GetSwapchainImagesKHR = (PFN_vkGetSwapchainImagesKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.AcquireNextImageKHR) {
#ifndef NDEBUG
         screen->vk.AcquireNextImageKHR = (PFN_vkAcquireNextImageKHR)zink_stub_AcquireNextImageKHR;
#else
         screen->vk.AcquireNextImageKHR = (PFN_vkAcquireNextImageKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.QueuePresentKHR) {
#ifndef NDEBUG
         screen->vk.QueuePresentKHR = (PFN_vkQueuePresentKHR)zink_stub_QueuePresentKHR;
#else
         screen->vk.QueuePresentKHR = (PFN_vkQueuePresentKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.GetDeviceGroupPresentCapabilitiesKHR) {
#ifndef NDEBUG
         screen->vk.GetDeviceGroupPresentCapabilitiesKHR = (PFN_vkGetDeviceGroupPresentCapabilitiesKHR)zink_stub_GetDeviceGroupPresentCapabilitiesKHR;
#else
         screen->vk.GetDeviceGroupPresentCapabilitiesKHR = (PFN_vkGetDeviceGroupPresentCapabilitiesKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.GetDeviceGroupSurfacePresentModesKHR) {
#ifndef NDEBUG
         screen->vk.GetDeviceGroupSurfacePresentModesKHR = (PFN_vkGetDeviceGroupSurfacePresentModesKHR)zink_stub_GetDeviceGroupSurfacePresentModesKHR;
#else
         screen->vk.GetDeviceGroupSurfacePresentModesKHR = (PFN_vkGetDeviceGroupSurfacePresentModesKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.AcquireNextImage2KHR) {
#ifndef NDEBUG
         screen->vk.AcquireNextImage2KHR = (PFN_vkAcquireNextImage2KHR)zink_stub_AcquireNextImage2KHR;
#else
         screen->vk.AcquireNextImage2KHR = (PFN_vkAcquireNextImage2KHR)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_EXT_rasterization_order_attachment_access) {
   }
   if (screen->info.have_KHR_shader_float16_int8) {
   }
   if (screen->info.have_EXT_multi_draw) {
#ifdef _WIN32
      if (!screen->vk.CmdDrawMultiEXT) {
#ifndef NDEBUG
         screen->vk.CmdDrawMultiEXT = (PFN_vkCmdDrawMultiEXT)zink_stub_CmdDrawMultiEXT;
#else
         screen->vk.CmdDrawMultiEXT = (PFN_vkCmdDrawMultiEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdDrawMultiIndexedEXT) {
#ifndef NDEBUG
         screen->vk.CmdDrawMultiIndexedEXT = (PFN_vkCmdDrawMultiIndexedEXT)zink_stub_CmdDrawMultiIndexedEXT;
#else
         screen->vk.CmdDrawMultiIndexedEXT = (PFN_vkCmdDrawMultiIndexedEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_EXT_primitives_generated_query) {
   }
   if (screen->info.have_KHR_pipeline_library) {
   }
   if (screen->info.have_EXT_graphics_pipeline_library) {
   }
   if (screen->info.have_KHR_push_descriptor) {
#ifdef _WIN32
      if (!screen->vk.CmdPushDescriptorSetKHR) {
#ifndef NDEBUG
         screen->vk.CmdPushDescriptorSetKHR = (PFN_vkCmdPushDescriptorSetKHR)zink_stub_CmdPushDescriptorSetKHR;
#else
         screen->vk.CmdPushDescriptorSetKHR = (PFN_vkCmdPushDescriptorSetKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdPushDescriptorSetWithTemplateKHR) {
#ifndef NDEBUG
         screen->vk.CmdPushDescriptorSetWithTemplateKHR = (PFN_vkCmdPushDescriptorSetWithTemplateKHR)zink_stub_CmdPushDescriptorSetWithTemplateKHR;
#else
         screen->vk.CmdPushDescriptorSetWithTemplateKHR = (PFN_vkCmdPushDescriptorSetWithTemplateKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdPushDescriptorSetWithTemplateKHR) {
#ifndef NDEBUG
         screen->vk.CmdPushDescriptorSetWithTemplateKHR = (PFN_vkCmdPushDescriptorSetWithTemplateKHR)zink_stub_CmdPushDescriptorSetWithTemplateKHR;
#else
         screen->vk.CmdPushDescriptorSetWithTemplateKHR = (PFN_vkCmdPushDescriptorSetWithTemplateKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_KHR_descriptor_update_template) {
#ifdef _WIN32
      if (!screen->vk.CreateDescriptorUpdateTemplateKHR) {
#ifndef NDEBUG
         screen->vk.CreateDescriptorUpdateTemplateKHR = (PFN_vkCreateDescriptorUpdateTemplateKHR)zink_stub_CreateDescriptorUpdateTemplateKHR;
#else
         screen->vk.CreateDescriptorUpdateTemplateKHR = (PFN_vkCreateDescriptorUpdateTemplateKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.DestroyDescriptorUpdateTemplateKHR) {
#ifndef NDEBUG
         screen->vk.DestroyDescriptorUpdateTemplateKHR = (PFN_vkDestroyDescriptorUpdateTemplateKHR)zink_stub_DestroyDescriptorUpdateTemplateKHR;
#else
         screen->vk.DestroyDescriptorUpdateTemplateKHR = (PFN_vkDestroyDescriptorUpdateTemplateKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.UpdateDescriptorSetWithTemplateKHR) {
#ifndef NDEBUG
         screen->vk.UpdateDescriptorSetWithTemplateKHR = (PFN_vkUpdateDescriptorSetWithTemplateKHR)zink_stub_UpdateDescriptorSetWithTemplateKHR;
#else
         screen->vk.UpdateDescriptorSetWithTemplateKHR = (PFN_vkUpdateDescriptorSetWithTemplateKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
#ifdef _WIN32
      if (!screen->vk.CmdPushDescriptorSetWithTemplateKHR) {
#ifndef NDEBUG
         screen->vk.CmdPushDescriptorSetWithTemplateKHR = (PFN_vkCmdPushDescriptorSetWithTemplateKHR)zink_stub_CmdPushDescriptorSetWithTemplateKHR;
#else
         screen->vk.CmdPushDescriptorSetWithTemplateKHR = (PFN_vkCmdPushDescriptorSetWithTemplateKHR)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_EXT_line_rasterization) {
#ifdef _WIN32
      if (!screen->vk.CmdSetLineStippleEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetLineStippleEXT = (PFN_vkCmdSetLineStippleEXT)zink_stub_CmdSetLineStippleEXT;
#else
         screen->vk.CmdSetLineStippleEXT = (PFN_vkCmdSetLineStippleEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_EXT_vertex_input_dynamic_state) {
#ifdef _WIN32
      if (!screen->vk.CmdSetVertexInputEXT) {
#ifndef NDEBUG
         screen->vk.CmdSetVertexInputEXT = (PFN_vkCmdSetVertexInputEXT)zink_stub_CmdSetVertexInputEXT;
#else
         screen->vk.CmdSetVertexInputEXT = (PFN_vkCmdSetVertexInputEXT)zink_stub_function_not_loaded;
#endif
      }
#endif
   }
   if (screen->info.have_EXT_primitive_topology_list_restart) {
   }
   if (screen->info.have_KHR_dedicated_allocation) {
   }
   if (screen->info.have_EXT_descriptor_indexing) {
   }
   if (screen->info.have_EXT_depth_clip_enable) {
   }
   if (screen->info.have_EXT_shader_demote_to_helper_invocation) {
   }
   if (screen->info.have_KHR_shader_float_controls) {
   }
   if (screen->info.have_KHR_format_feature_flags2) {
   }
}

#ifndef NDEBUG
/* generated stub functions */


   
void
zink_stub_TrimCommandPoolKHR()
{
   mesa_loge("ZINK: vkTrimCommandPoolKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_GetDescriptorSetLayoutSupportKHR()
{
   mesa_loge("ZINK: vkGetDescriptorSetLayoutSupportKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_GetDeviceBufferMemoryRequirementsKHR()
{
   mesa_loge("ZINK: vkGetDeviceBufferMemoryRequirementsKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_GetDeviceImageMemoryRequirementsKHR()
{
   mesa_loge("ZINK: vkGetDeviceImageMemoryRequirementsKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_GetDeviceImageSparseMemoryRequirementsKHR()
{
   mesa_loge("ZINK: vkGetDeviceImageSparseMemoryRequirementsKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdBindIndexBuffer2KHR()
{
   mesa_loge("ZINK: vkCmdBindIndexBuffer2KHR is not loaded properly!");
   abort();
}
   
void
zink_stub_GetRenderingAreaGranularityKHR()
{
   mesa_loge("ZINK: vkGetRenderingAreaGranularityKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_GetDeviceImageSubresourceLayoutKHR()
{
   mesa_loge("ZINK: vkGetDeviceImageSubresourceLayoutKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_GetImageSubresourceLayout2KHR()
{
   mesa_loge("ZINK: vkGetImageSubresourceLayout2KHR is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdBindDescriptorSets2KHR()
{
   mesa_loge("ZINK: vkCmdBindDescriptorSets2KHR is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdPushConstants2KHR()
{
   mesa_loge("ZINK: vkCmdPushConstants2KHR is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdPushDescriptorSet2KHR()
{
   mesa_loge("ZINK: vkCmdPushDescriptorSet2KHR is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdPushDescriptorSetWithTemplate2KHR()
{
   mesa_loge("ZINK: vkCmdPushDescriptorSetWithTemplate2KHR is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetDescriptorBufferOffsets2EXT()
{
   mesa_loge("ZINK: vkCmdSetDescriptorBufferOffsets2EXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdBindDescriptorBufferEmbeddedSamplers2EXT()
{
   mesa_loge("ZINK: vkCmdBindDescriptorBufferEmbeddedSamplers2EXT is not loaded properly!");
   abort();
}
   
void
zink_stub_GetMemoryFdKHR()
{
   mesa_loge("ZINK: vkGetMemoryFdKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_GetMemoryFdPropertiesKHR()
{
   mesa_loge("ZINK: vkGetMemoryFdPropertiesKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_GetPipelineExecutablePropertiesKHR()
{
   mesa_loge("ZINK: vkGetPipelineExecutablePropertiesKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_GetPipelineExecutableStatisticsKHR()
{
   mesa_loge("ZINK: vkGetPipelineExecutableStatisticsKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_GetPipelineExecutableInternalRepresentationsKHR()
{
   mesa_loge("ZINK: vkGetPipelineExecutableInternalRepresentationsKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_ImportSemaphoreFdKHR()
{
   mesa_loge("ZINK: vkImportSemaphoreFdKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_GetSemaphoreFdKHR()
{
   mesa_loge("ZINK: vkGetSemaphoreFdKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_CreateRenderPass2KHR()
{
   mesa_loge("ZINK: vkCreateRenderPass2KHR is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdBeginRenderPass2KHR()
{
   mesa_loge("ZINK: vkCmdBeginRenderPass2KHR is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdNextSubpass2KHR()
{
   mesa_loge("ZINK: vkCmdNextSubpass2KHR is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdEndRenderPass2KHR()
{
   mesa_loge("ZINK: vkCmdEndRenderPass2KHR is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetEvent2KHR()
{
   mesa_loge("ZINK: vkCmdSetEvent2KHR is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdResetEvent2KHR()
{
   mesa_loge("ZINK: vkCmdResetEvent2KHR is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdWaitEvents2KHR()
{
   mesa_loge("ZINK: vkCmdWaitEvents2KHR is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdPipelineBarrier2KHR()
{
   mesa_loge("ZINK: vkCmdPipelineBarrier2KHR is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdWriteTimestamp2KHR()
{
   mesa_loge("ZINK: vkCmdWriteTimestamp2KHR is not loaded properly!");
   abort();
}
   
void
zink_stub_QueueSubmit2KHR()
{
   mesa_loge("ZINK: vkQueueSubmit2KHR is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdWriteBufferMarker2AMD()
{
   mesa_loge("ZINK: vkCmdWriteBufferMarker2AMD is not loaded properly!");
   abort();
}
   
void
zink_stub_GetQueueCheckpointData2NV()
{
   mesa_loge("ZINK: vkGetQueueCheckpointData2NV is not loaded properly!");
   abort();
}
   
void
zink_stub_GetMemoryWin32HandleKHR()
{
   mesa_loge("ZINK: vkGetMemoryWin32HandleKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_GetMemoryWin32HandlePropertiesKHR()
{
   mesa_loge("ZINK: vkGetMemoryWin32HandlePropertiesKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_ImportSemaphoreWin32HandleKHR()
{
   mesa_loge("ZINK: vkImportSemaphoreWin32HandleKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_GetSemaphoreWin32HandleKHR()
{
   mesa_loge("ZINK: vkGetSemaphoreWin32HandleKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_GetBufferDeviceAddressKHR()
{
   mesa_loge("ZINK: vkGetBufferDeviceAddressKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_GetBufferOpaqueCaptureAddressKHR()
{
   mesa_loge("ZINK: vkGetBufferOpaqueCaptureAddressKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_GetDeviceMemoryOpaqueCaptureAddressKHR()
{
   mesa_loge("ZINK: vkGetDeviceMemoryOpaqueCaptureAddressKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_GetMemoryHostPointerPropertiesEXT()
{
   mesa_loge("ZINK: vkGetMemoryHostPointerPropertiesEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_GetImageMemoryRequirements2KHR()
{
   mesa_loge("ZINK: vkGetImageMemoryRequirements2KHR is not loaded properly!");
   abort();
}
   
void
zink_stub_GetBufferMemoryRequirements2KHR()
{
   mesa_loge("ZINK: vkGetBufferMemoryRequirements2KHR is not loaded properly!");
   abort();
}
   
void
zink_stub_GetImageSparseMemoryRequirements2KHR()
{
   mesa_loge("ZINK: vkGetImageSparseMemoryRequirements2KHR is not loaded properly!");
   abort();
}
   
void
zink_stub_SetDeviceMemoryPriorityEXT()
{
   mesa_loge("ZINK: vkSetDeviceMemoryPriorityEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdDrawIndirectCountKHR()
{
   mesa_loge("ZINK: vkCmdDrawIndirectCountKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdDrawIndexedIndirectCountKHR()
{
   mesa_loge("ZINK: vkCmdDrawIndexedIndirectCountKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_CreateShadersEXT()
{
   mesa_loge("ZINK: vkCreateShadersEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_DestroyShaderEXT()
{
   mesa_loge("ZINK: vkDestroyShaderEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_GetShaderBinaryDataEXT()
{
   mesa_loge("ZINK: vkGetShaderBinaryDataEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdBindShadersEXT()
{
   mesa_loge("ZINK: vkCmdBindShadersEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetCullModeEXT()
{
   mesa_loge("ZINK: vkCmdSetCullModeEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetFrontFaceEXT()
{
   mesa_loge("ZINK: vkCmdSetFrontFaceEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetPrimitiveTopologyEXT()
{
   mesa_loge("ZINK: vkCmdSetPrimitiveTopologyEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetViewportWithCountEXT()
{
   mesa_loge("ZINK: vkCmdSetViewportWithCountEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetScissorWithCountEXT()
{
   mesa_loge("ZINK: vkCmdSetScissorWithCountEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdBindVertexBuffers2EXT()
{
   mesa_loge("ZINK: vkCmdBindVertexBuffers2EXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetDepthTestEnableEXT()
{
   mesa_loge("ZINK: vkCmdSetDepthTestEnableEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetDepthWriteEnableEXT()
{
   mesa_loge("ZINK: vkCmdSetDepthWriteEnableEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetDepthCompareOpEXT()
{
   mesa_loge("ZINK: vkCmdSetDepthCompareOpEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetDepthBoundsTestEnableEXT()
{
   mesa_loge("ZINK: vkCmdSetDepthBoundsTestEnableEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetStencilTestEnableEXT()
{
   mesa_loge("ZINK: vkCmdSetStencilTestEnableEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetStencilOpEXT()
{
   mesa_loge("ZINK: vkCmdSetStencilOpEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetVertexInputEXT()
{
   mesa_loge("ZINK: vkCmdSetVertexInputEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetPatchControlPointsEXT()
{
   mesa_loge("ZINK: vkCmdSetPatchControlPointsEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetRasterizerDiscardEnableEXT()
{
   mesa_loge("ZINK: vkCmdSetRasterizerDiscardEnableEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetDepthBiasEnableEXT()
{
   mesa_loge("ZINK: vkCmdSetDepthBiasEnableEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetLogicOpEXT()
{
   mesa_loge("ZINK: vkCmdSetLogicOpEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetPrimitiveRestartEnableEXT()
{
   mesa_loge("ZINK: vkCmdSetPrimitiveRestartEnableEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetTessellationDomainOriginEXT()
{
   mesa_loge("ZINK: vkCmdSetTessellationDomainOriginEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetDepthClampEnableEXT()
{
   mesa_loge("ZINK: vkCmdSetDepthClampEnableEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetPolygonModeEXT()
{
   mesa_loge("ZINK: vkCmdSetPolygonModeEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetRasterizationSamplesEXT()
{
   mesa_loge("ZINK: vkCmdSetRasterizationSamplesEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetSampleMaskEXT()
{
   mesa_loge("ZINK: vkCmdSetSampleMaskEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetAlphaToCoverageEnableEXT()
{
   mesa_loge("ZINK: vkCmdSetAlphaToCoverageEnableEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetAlphaToOneEnableEXT()
{
   mesa_loge("ZINK: vkCmdSetAlphaToOneEnableEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetLogicOpEnableEXT()
{
   mesa_loge("ZINK: vkCmdSetLogicOpEnableEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetColorBlendEnableEXT()
{
   mesa_loge("ZINK: vkCmdSetColorBlendEnableEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetColorBlendEquationEXT()
{
   mesa_loge("ZINK: vkCmdSetColorBlendEquationEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetColorWriteMaskEXT()
{
   mesa_loge("ZINK: vkCmdSetColorWriteMaskEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetRasterizationStreamEXT()
{
   mesa_loge("ZINK: vkCmdSetRasterizationStreamEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetConservativeRasterizationModeEXT()
{
   mesa_loge("ZINK: vkCmdSetConservativeRasterizationModeEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetExtraPrimitiveOverestimationSizeEXT()
{
   mesa_loge("ZINK: vkCmdSetExtraPrimitiveOverestimationSizeEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetDepthClipEnableEXT()
{
   mesa_loge("ZINK: vkCmdSetDepthClipEnableEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetSampleLocationsEnableEXT()
{
   mesa_loge("ZINK: vkCmdSetSampleLocationsEnableEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetColorBlendAdvancedEXT()
{
   mesa_loge("ZINK: vkCmdSetColorBlendAdvancedEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetProvokingVertexModeEXT()
{
   mesa_loge("ZINK: vkCmdSetProvokingVertexModeEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetLineRasterizationModeEXT()
{
   mesa_loge("ZINK: vkCmdSetLineRasterizationModeEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetLineStippleEnableEXT()
{
   mesa_loge("ZINK: vkCmdSetLineStippleEnableEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetDepthClipNegativeOneToOneEXT()
{
   mesa_loge("ZINK: vkCmdSetDepthClipNegativeOneToOneEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetViewportWScalingEnableNV()
{
   mesa_loge("ZINK: vkCmdSetViewportWScalingEnableNV is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetViewportSwizzleNV()
{
   mesa_loge("ZINK: vkCmdSetViewportSwizzleNV is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetCoverageToColorEnableNV()
{
   mesa_loge("ZINK: vkCmdSetCoverageToColorEnableNV is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetCoverageToColorLocationNV()
{
   mesa_loge("ZINK: vkCmdSetCoverageToColorLocationNV is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetCoverageModulationModeNV()
{
   mesa_loge("ZINK: vkCmdSetCoverageModulationModeNV is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetCoverageModulationTableEnableNV()
{
   mesa_loge("ZINK: vkCmdSetCoverageModulationTableEnableNV is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetCoverageModulationTableNV()
{
   mesa_loge("ZINK: vkCmdSetCoverageModulationTableNV is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetShadingRateImageEnableNV()
{
   mesa_loge("ZINK: vkCmdSetShadingRateImageEnableNV is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetRepresentativeFragmentTestEnableNV()
{
   mesa_loge("ZINK: vkCmdSetRepresentativeFragmentTestEnableNV is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetCoverageReductionModeNV()
{
   mesa_loge("ZINK: vkCmdSetCoverageReductionModeNV is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetAttachmentFeedbackLoopEnableEXT()
{
   mesa_loge("ZINK: vkCmdSetAttachmentFeedbackLoopEnableEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_GetGeneratedCommandsMemoryRequirementsNV()
{
   mesa_loge("ZINK: vkGetGeneratedCommandsMemoryRequirementsNV is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdPreprocessGeneratedCommandsNV()
{
   mesa_loge("ZINK: vkCmdPreprocessGeneratedCommandsNV is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdExecuteGeneratedCommandsNV()
{
   mesa_loge("ZINK: vkCmdExecuteGeneratedCommandsNV is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdBindPipelineShaderGroupNV()
{
   mesa_loge("ZINK: vkCmdBindPipelineShaderGroupNV is not loaded properly!");
   abort();
}
   
void
zink_stub_CreateIndirectCommandsLayoutNV()
{
   mesa_loge("ZINK: vkCreateIndirectCommandsLayoutNV is not loaded properly!");
   abort();
}
   
void
zink_stub_DestroyIndirectCommandsLayoutNV()
{
   mesa_loge("ZINK: vkDestroyIndirectCommandsLayoutNV is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetSampleLocationsEXT()
{
   mesa_loge("ZINK: vkCmdSetSampleLocationsEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_GetDescriptorSetLayoutSizeEXT()
{
   mesa_loge("ZINK: vkGetDescriptorSetLayoutSizeEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_GetDescriptorSetLayoutBindingOffsetEXT()
{
   mesa_loge("ZINK: vkGetDescriptorSetLayoutBindingOffsetEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_GetDescriptorEXT()
{
   mesa_loge("ZINK: vkGetDescriptorEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdBindDescriptorBuffersEXT()
{
   mesa_loge("ZINK: vkCmdBindDescriptorBuffersEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetDescriptorBufferOffsetsEXT()
{
   mesa_loge("ZINK: vkCmdSetDescriptorBufferOffsetsEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdBindDescriptorBufferEmbeddedSamplersEXT()
{
   mesa_loge("ZINK: vkCmdBindDescriptorBufferEmbeddedSamplersEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_GetBufferOpaqueCaptureDescriptorDataEXT()
{
   mesa_loge("ZINK: vkGetBufferOpaqueCaptureDescriptorDataEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_GetImageOpaqueCaptureDescriptorDataEXT()
{
   mesa_loge("ZINK: vkGetImageOpaqueCaptureDescriptorDataEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_GetImageViewOpaqueCaptureDescriptorDataEXT()
{
   mesa_loge("ZINK: vkGetImageViewOpaqueCaptureDescriptorDataEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_GetSamplerOpaqueCaptureDescriptorDataEXT()
{
   mesa_loge("ZINK: vkGetSamplerOpaqueCaptureDescriptorDataEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_GetAccelerationStructureOpaqueCaptureDescriptorDataEXT()
{
   mesa_loge("ZINK: vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdBeginConditionalRenderingEXT()
{
   mesa_loge("ZINK: vkCmdBeginConditionalRenderingEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdEndConditionalRenderingEXT()
{
   mesa_loge("ZINK: vkCmdEndConditionalRenderingEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdBindTransformFeedbackBuffersEXT()
{
   mesa_loge("ZINK: vkCmdBindTransformFeedbackBuffersEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdBeginTransformFeedbackEXT()
{
   mesa_loge("ZINK: vkCmdBeginTransformFeedbackEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdEndTransformFeedbackEXT()
{
   mesa_loge("ZINK: vkCmdEndTransformFeedbackEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdBeginQueryIndexedEXT()
{
   mesa_loge("ZINK: vkCmdBeginQueryIndexedEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdEndQueryIndexedEXT()
{
   mesa_loge("ZINK: vkCmdEndQueryIndexedEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdDrawIndirectByteCountEXT()
{
   mesa_loge("ZINK: vkCmdDrawIndirectByteCountEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CreateSamplerYcbcrConversionKHR()
{
   mesa_loge("ZINK: vkCreateSamplerYcbcrConversionKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_DestroySamplerYcbcrConversionKHR()
{
   mesa_loge("ZINK: vkDestroySamplerYcbcrConversionKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_GetImageDrmFormatModifierPropertiesEXT()
{
   mesa_loge("ZINK: vkGetImageDrmFormatModifierPropertiesEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_GetCalibratedTimestampsEXT()
{
   mesa_loge("ZINK: vkGetCalibratedTimestampsEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdBeginRenderingKHR()
{
   mesa_loge("ZINK: vkCmdBeginRenderingKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdEndRenderingKHR()
{
   mesa_loge("ZINK: vkCmdEndRenderingKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetRenderingAttachmentLocationsKHR()
{
   mesa_loge("ZINK: vkCmdSetRenderingAttachmentLocationsKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetRenderingInputAttachmentIndicesKHR()
{
   mesa_loge("ZINK: vkCmdSetRenderingInputAttachmentIndicesKHR is not loaded properly!");
   abort();
}
                                                                                                                                                   
void
zink_stub_GetSemaphoreCounterValueKHR()
{
   mesa_loge("ZINK: vkGetSemaphoreCounterValueKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_WaitSemaphoresKHR()
{
   mesa_loge("ZINK: vkWaitSemaphoresKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_SignalSemaphoreKHR()
{
   mesa_loge("ZINK: vkSignalSemaphoreKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdSetColorWriteEnableEXT()
{
   mesa_loge("ZINK: vkCmdSetColorWriteEnableEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CopyMemoryToImageEXT()
{
   mesa_loge("ZINK: vkCopyMemoryToImageEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CopyImageToMemoryEXT()
{
   mesa_loge("ZINK: vkCopyImageToMemoryEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CopyImageToImageEXT()
{
   mesa_loge("ZINK: vkCopyImageToImageEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_TransitionImageLayoutEXT()
{
   mesa_loge("ZINK: vkTransitionImageLayoutEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_GetImageSubresourceLayout2EXT()
{
   mesa_loge("ZINK: vkGetImageSubresourceLayout2EXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CreateSwapchainKHR()
{
   mesa_loge("ZINK: vkCreateSwapchainKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_DestroySwapchainKHR()
{
   mesa_loge("ZINK: vkDestroySwapchainKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_GetSwapchainImagesKHR()
{
   mesa_loge("ZINK: vkGetSwapchainImagesKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_AcquireNextImageKHR()
{
   mesa_loge("ZINK: vkAcquireNextImageKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_QueuePresentKHR()
{
   mesa_loge("ZINK: vkQueuePresentKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_GetDeviceGroupPresentCapabilitiesKHR()
{
   mesa_loge("ZINK: vkGetDeviceGroupPresentCapabilitiesKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_GetDeviceGroupSurfacePresentModesKHR()
{
   mesa_loge("ZINK: vkGetDeviceGroupSurfacePresentModesKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_AcquireNextImage2KHR()
{
   mesa_loge("ZINK: vkAcquireNextImage2KHR is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdDrawMultiEXT()
{
   mesa_loge("ZINK: vkCmdDrawMultiEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdDrawMultiIndexedEXT()
{
   mesa_loge("ZINK: vkCmdDrawMultiIndexedEXT is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdPushDescriptorSetKHR()
{
   mesa_loge("ZINK: vkCmdPushDescriptorSetKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_CmdPushDescriptorSetWithTemplateKHR()
{
   mesa_loge("ZINK: vkCmdPushDescriptorSetWithTemplateKHR is not loaded properly!");
   abort();
}
      
void
zink_stub_CreateDescriptorUpdateTemplateKHR()
{
   mesa_loge("ZINK: vkCreateDescriptorUpdateTemplateKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_DestroyDescriptorUpdateTemplateKHR()
{
   mesa_loge("ZINK: vkDestroyDescriptorUpdateTemplateKHR is not loaded properly!");
   abort();
}
   
void
zink_stub_UpdateDescriptorSetWithTemplateKHR()
{
   mesa_loge("ZINK: vkUpdateDescriptorSetWithTemplateKHR is not loaded properly!");
   abort();
}
      
void
zink_stub_CmdSetLineStippleEXT()
{
   mesa_loge("ZINK: vkCmdSetLineStippleEXT is not loaded properly!");
   abort();
}
   #endif
