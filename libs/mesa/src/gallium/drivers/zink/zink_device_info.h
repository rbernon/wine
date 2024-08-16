#ifndef ZINK_DEVICE_INFO_H
#define ZINK_DEVICE_INFO_H

#include "util/u_memory.h"

#include <vulkan/vulkan_core.h>

#ifdef VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan_beta.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <vulkan/vulkan_win32.h>
#endif

struct zink_screen;

struct zink_device_info {
   uint32_t device_version;

   bool have_KHR_maintenance1;
   bool have_KHR_maintenance2;
   bool have_KHR_maintenance3;
   bool have_KHR_maintenance4;
   bool have_KHR_maintenance5;
   bool have_KHR_maintenance6;
   bool have_KHR_maintenance7;
   bool have_KHR_external_memory;
   bool have_KHR_external_memory_fd;
   bool have_KHR_vulkan_memory_model;
   bool have_KHR_workgroup_memory_explicit_layout;
   bool have_KHR_pipeline_executable_properties;
   bool have_KHR_external_semaphore_fd;
   bool have_KHR_create_renderpass2;
   bool have_KHR_synchronization2;
   bool have_KHR_external_memory_win32;
   bool have_KHR_external_semaphore_win32;
   bool have_EXT_external_memory_dma_buf;
   bool have_KHR_buffer_device_address;
   bool have_EXT_external_memory_host;
   bool have_EXT_queue_family_foreign;
   bool have_KHR_swapchain_mutable_format;
   bool have_KHR_incremental_present;
   bool have_EXT_provoking_vertex;
   bool have_EXT_shader_viewport_index_layer;
   bool have_KHR_get_memory_requirements2;
   bool have_EXT_post_depth_coverage;
   bool have_EXT_depth_clip_control;
   bool have_EXT_depth_clamp_zero_one;
   bool have_EXT_shader_subgroup_ballot;
   bool have_EXT_shader_subgroup_vote;
   bool have_EXT_legacy_vertex_attributes;
   bool have_EXT_shader_atomic_float;
   bool have_KHR_shader_atomic_int64;
   bool have_KHR_8bit_storage;
   bool have_KHR_16bit_storage;
   bool have_EXT_image_2d_view_of_3d;
   bool have_KHR_driver_properties;
   bool have_EXT_memory_budget;
   bool have_EXT_memory_priority;
   bool have_EXT_pageable_device_local_memory;
   bool have_KHR_draw_indirect_count;
   bool have_EXT_dynamic_rendering_unused_attachments;
   bool have_EXT_shader_object;
   bool have_EXT_attachment_feedback_loop_layout;
   bool have_EXT_attachment_feedback_loop_dynamic_state;
   bool have_EXT_fragment_shader_interlock;
   bool have_EXT_sample_locations;
   bool have_KHR_shader_draw_parameters;
   bool have_KHR_sampler_mirror_clamp_to_edge;
   bool have_EXT_descriptor_buffer;
   bool have_EXT_conditional_rendering;
   bool have_EXT_transform_feedback;
   bool have_EXT_index_type_uint8;
   bool have_KHR_image_format_list;
   bool have_KHR_sampler_ycbcr_conversion;
   bool have_KHR_imageless_framebuffer;
   bool have_EXT_robustness2;
   bool have_EXT_image_robustness;
   bool have_EXT_image_drm_format_modifier;
   bool have_EXT_vertex_attribute_divisor;
   bool have_EXT_calibrated_timestamps;
   bool have_NV_linear_color_attachment;
   bool have_KHR_dynamic_rendering;
   bool have_KHR_dynamic_rendering_local_read;
   bool have_EXT_multisampled_render_to_single_sampled;
   bool have_KHR_shader_clock;
   bool have_INTEL_shader_integer_functions2;
   bool have_EXT_sampler_filter_minmax;
   bool have_EXT_custom_border_color;
   bool have_EXT_non_seamless_cube_map;
   bool have_EXT_border_color_swizzle;
   bool have_EXT_blend_operation_advanced;
   bool have_EXT_extended_dynamic_state;
   bool have_EXT_extended_dynamic_state2;
   bool have_EXT_extended_dynamic_state3;
   bool have_EXT_pipeline_creation_cache_control;
   bool have_EXT_shader_stencil_export;
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
   bool have_KHR_portability_subset;
#endif
   bool have_NV_compute_shader_derivatives;
   bool have_KHR_timeline_semaphore;
   bool have_EXT_color_write_enable;
   bool have_EXT_4444_formats;
   bool have_EXT_host_image_copy;
   bool have_EXT_scalar_block_layout;
   bool have_KHR_swapchain;
   bool have_EXT_rasterization_order_attachment_access;
   bool have_KHR_shader_float16_int8;
   bool have_EXT_multi_draw;
   bool have_EXT_primitives_generated_query;
   bool have_KHR_pipeline_library;
   bool have_EXT_graphics_pipeline_library;
   bool have_KHR_push_descriptor;
   bool have_KHR_descriptor_update_template;
   bool have_EXT_line_rasterization;
   bool have_EXT_vertex_input_dynamic_state;
   bool have_EXT_primitive_topology_list_restart;
   bool have_KHR_dedicated_allocation;
   bool have_EXT_descriptor_indexing;
   bool have_EXT_depth_clip_enable;
   bool have_EXT_shader_demote_to_helper_invocation;
   bool have_KHR_shader_float_controls;
   bool have_KHR_format_feature_flags2;
   bool have_vulkan11;
   bool have_vulkan12;
   bool have_vulkan13;

   VkPhysicalDeviceFeatures2 feats;
   VkPhysicalDeviceSubgroupProperties subgroup;
   VkPhysicalDeviceVulkan11Features feats11;
   VkPhysicalDeviceVulkan12Features feats12;
   VkPhysicalDeviceVulkan13Features feats13;

   VkPhysicalDeviceProperties props;
   VkPhysicalDeviceProperties vk_layered_props;
   VkPhysicalDeviceLayeredApiPropertiesKHR layered_props;
   VkPhysicalDeviceDriverPropertiesKHR vk_layered_driver_props;
   VkPhysicalDeviceVulkan11Properties props11;
   VkPhysicalDeviceVulkan12Properties props12;
   VkPhysicalDeviceVulkan13Properties props13;

   VkPhysicalDeviceMemoryProperties mem_props;
   VkPhysicalDeviceIDProperties deviceid_props;

   
   
   
   VkPhysicalDeviceMaintenance4FeaturesKHR maint4_feats;
   VkPhysicalDeviceMaintenance5FeaturesKHR maint5_feats;
   VkPhysicalDeviceMaintenance5PropertiesKHR maint5_props;
   VkPhysicalDeviceMaintenance6FeaturesKHR maint6_feats;
   VkPhysicalDeviceMaintenance6PropertiesKHR maint6_props;
   VkPhysicalDeviceMaintenance7FeaturesKHR maint7_feats;
   VkPhysicalDeviceMaintenance7PropertiesKHR maint7_props;
   
   
   
   VkPhysicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR explicit_layout_feats;
   VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR pipestats_feats;
   
   
   VkPhysicalDeviceSynchronization2FeaturesKHR sync2_feats;
   
   
   
   VkPhysicalDeviceBufferDeviceAddressFeaturesKHR bda_feats;
   VkPhysicalDeviceExternalMemoryHostPropertiesEXT ext_host_mem_props;
   
   
   
   VkPhysicalDeviceProvokingVertexFeaturesEXT pv_feats;
   VkPhysicalDeviceProvokingVertexPropertiesEXT pv_props;
   
   
   
   VkPhysicalDeviceDepthClipControlFeaturesEXT clip_control_feats;
   VkPhysicalDeviceDepthClampZeroOneFeaturesEXT clamp_01_feats;
   
   
   VkPhysicalDeviceLegacyVertexAttributesFeaturesEXT legacyverts_feats;
   VkPhysicalDeviceLegacyVertexAttributesPropertiesEXT legacyverts_props;
   VkPhysicalDeviceShaderAtomicFloatFeaturesEXT atomic_float_feats;
   VkPhysicalDeviceShaderAtomicInt64FeaturesKHR atomic_int_feats;
   VkPhysicalDevice8BitStorageFeaturesKHR storage_8bit_feats;
   VkPhysicalDevice16BitStorageFeaturesKHR storage_16bit_feats;
   VkPhysicalDeviceImage2DViewOf3DFeaturesEXT view2d_feats;
   VkPhysicalDeviceDriverPropertiesKHR driver_props;
   
   VkPhysicalDeviceMemoryPriorityFeaturesEXT memprio_feats;
   VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT mempage_feats;
   
   VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT unused_feats;
   VkPhysicalDeviceShaderObjectFeaturesEXT shobj_feats;
   VkPhysicalDeviceShaderObjectPropertiesEXT shobj_props;
   VkPhysicalDeviceAttachmentFeedbackLoopLayoutFeaturesEXT feedback_loop_feats;
   VkPhysicalDeviceAttachmentFeedbackLoopDynamicStateFeaturesEXT feedback_dyn_feats;
   VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT interlock_feats;
   VkPhysicalDeviceSampleLocationsPropertiesEXT sample_locations_props;
   
   
   VkPhysicalDeviceDescriptorBufferFeaturesEXT db_feats;
   VkPhysicalDeviceDescriptorBufferPropertiesEXT db_props;
   VkPhysicalDeviceConditionalRenderingFeaturesEXT cond_render_feats;
   VkPhysicalDeviceTransformFeedbackFeaturesEXT tf_feats;
   VkPhysicalDeviceTransformFeedbackPropertiesEXT tf_props;
   VkPhysicalDeviceIndexTypeUint8FeaturesEXT index_uint8_feats;
   
   
   VkPhysicalDeviceImagelessFramebufferFeaturesKHR imgless_feats;
   VkPhysicalDeviceRobustness2FeaturesEXT rb2_feats;
   VkPhysicalDeviceRobustness2PropertiesEXT rb2_props;
   VkPhysicalDeviceImageRobustnessFeaturesEXT rb_image_feats;
   
   VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT vdiv_feats;
   VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT vdiv_props;
   
   VkPhysicalDeviceLinearColorAttachmentFeaturesNV linear_color_feats;
   VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_render_feats;
   VkPhysicalDeviceDynamicRenderingLocalReadFeaturesKHR drlr_feats;
   VkPhysicalDeviceMultisampledRenderToSingleSampledFeaturesEXT msrtss_feats;
   VkPhysicalDeviceShaderClockFeaturesKHR shader_clock_feats;
   VkPhysicalDeviceShaderIntegerFunctions2FeaturesINTEL shader_int_fns2_feats;
   VkPhysicalDeviceSamplerFilterMinmaxPropertiesEXT reduction_props;
   VkPhysicalDeviceCustomBorderColorFeaturesEXT border_color_feats;
   VkPhysicalDeviceCustomBorderColorPropertiesEXT border_color_props;
   VkPhysicalDeviceNonSeamlessCubeMapFeaturesEXT nonseamless_feats;
   VkPhysicalDeviceBorderColorSwizzleFeaturesEXT border_swizzle_feats;
   VkPhysicalDeviceBlendOperationAdvancedPropertiesEXT blend_props;
   VkPhysicalDeviceExtendedDynamicStateFeaturesEXT dynamic_state_feats;
   VkPhysicalDeviceExtendedDynamicState2FeaturesEXT dynamic_state2_feats;
   VkPhysicalDeviceExtendedDynamicState3FeaturesEXT dynamic_state3_feats;
   VkPhysicalDeviceExtendedDynamicState3PropertiesEXT dynamic_state3_props;
   VkPhysicalDevicePipelineCreationCacheControlFeaturesEXT pipeline_cache_control_feats;
   
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
   VkPhysicalDevicePortabilitySubsetFeaturesKHR portability_subset_feats;
#endif
   VkPhysicalDeviceComputeShaderDerivativesFeaturesNV shader_derivs_feats;
   VkPhysicalDeviceTimelineSemaphoreFeaturesKHR timeline_feats;
   VkPhysicalDeviceColorWriteEnableFeaturesEXT cwrite_feats;
   VkPhysicalDevice4444FormatsFeaturesEXT format_4444_feats;
   VkPhysicalDeviceHostImageCopyFeaturesEXT hic_feats;
   VkPhysicalDeviceHostImageCopyPropertiesEXT hic_props;
   VkPhysicalDeviceScalarBlockLayoutFeaturesEXT scalar_block_layout_feats;
   
   VkPhysicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT rast_order_access_feats;
   VkPhysicalDeviceShaderFloat16Int8FeaturesKHR shader_float16_int8_feats;
   VkPhysicalDeviceMultiDrawFeaturesEXT multidraw_feats;
   VkPhysicalDeviceMultiDrawPropertiesEXT multidraw_props;
   VkPhysicalDevicePrimitivesGeneratedQueryFeaturesEXT primgen_feats;
   
   VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT gpl_feats;
   VkPhysicalDeviceGraphicsPipelineLibraryPropertiesEXT gpl_props;
   VkPhysicalDevicePushDescriptorPropertiesKHR push_props;
   
   VkPhysicalDeviceLineRasterizationFeaturesEXT line_rast_feats;
   VkPhysicalDeviceLineRasterizationPropertiesEXT line_rast_props;
   VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT vertex_input_feats;
   VkPhysicalDevicePrimitiveTopologyListRestartFeaturesEXT list_restart_feats;
   
   VkPhysicalDeviceDescriptorIndexingFeaturesEXT desc_indexing_feats;
   VkPhysicalDeviceDescriptorIndexingPropertiesEXT desc_indexing_props;
   VkPhysicalDeviceDepthClipEnableFeaturesEXT depth_clip_enable_feats;
   VkPhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT demote_feats;
   
   

    const char *extensions[103];
    uint32_t num_extensions;
};

bool
zink_get_physical_device_info(struct zink_screen *screen);

void
zink_verify_device_extensions(struct zink_screen *screen);

/* stub functions that get inserted into the dispatch table if they are not
 * properly loaded.
 */
void VKAPI_PTR zink_stub_TrimCommandPoolKHR(void);
void VKAPI_PTR zink_stub_GetDescriptorSetLayoutSupportKHR(void);
void VKAPI_PTR zink_stub_GetDeviceBufferMemoryRequirementsKHR(void);
void VKAPI_PTR zink_stub_GetDeviceImageMemoryRequirementsKHR(void);
void VKAPI_PTR zink_stub_GetDeviceImageSparseMemoryRequirementsKHR(void);
void VKAPI_PTR zink_stub_CmdBindIndexBuffer2KHR(void);
void VKAPI_PTR zink_stub_GetRenderingAreaGranularityKHR(void);
void VKAPI_PTR zink_stub_GetDeviceImageSubresourceLayoutKHR(void);
void VKAPI_PTR zink_stub_GetImageSubresourceLayout2KHR(void);
void VKAPI_PTR zink_stub_CmdBindDescriptorSets2KHR(void);
void VKAPI_PTR zink_stub_CmdPushConstants2KHR(void);
void VKAPI_PTR zink_stub_CmdPushDescriptorSet2KHR(void);
void VKAPI_PTR zink_stub_CmdPushDescriptorSetWithTemplate2KHR(void);
void VKAPI_PTR zink_stub_CmdSetDescriptorBufferOffsets2EXT(void);
void VKAPI_PTR zink_stub_CmdBindDescriptorBufferEmbeddedSamplers2EXT(void);
void VKAPI_PTR zink_stub_GetMemoryFdKHR(void);
void VKAPI_PTR zink_stub_GetMemoryFdPropertiesKHR(void);
void VKAPI_PTR zink_stub_GetPipelineExecutablePropertiesKHR(void);
void VKAPI_PTR zink_stub_GetPipelineExecutableStatisticsKHR(void);
void VKAPI_PTR zink_stub_GetPipelineExecutableInternalRepresentationsKHR(void);
void VKAPI_PTR zink_stub_ImportSemaphoreFdKHR(void);
void VKAPI_PTR zink_stub_GetSemaphoreFdKHR(void);
void VKAPI_PTR zink_stub_CreateRenderPass2KHR(void);
void VKAPI_PTR zink_stub_CmdBeginRenderPass2KHR(void);
void VKAPI_PTR zink_stub_CmdNextSubpass2KHR(void);
void VKAPI_PTR zink_stub_CmdEndRenderPass2KHR(void);
void VKAPI_PTR zink_stub_CmdSetEvent2KHR(void);
void VKAPI_PTR zink_stub_CmdResetEvent2KHR(void);
void VKAPI_PTR zink_stub_CmdWaitEvents2KHR(void);
void VKAPI_PTR zink_stub_CmdPipelineBarrier2KHR(void);
void VKAPI_PTR zink_stub_CmdWriteTimestamp2KHR(void);
void VKAPI_PTR zink_stub_QueueSubmit2KHR(void);
void VKAPI_PTR zink_stub_CmdWriteBufferMarker2AMD(void);
void VKAPI_PTR zink_stub_GetQueueCheckpointData2NV(void);
void VKAPI_PTR zink_stub_GetMemoryWin32HandleKHR(void);
void VKAPI_PTR zink_stub_GetMemoryWin32HandlePropertiesKHR(void);
void VKAPI_PTR zink_stub_ImportSemaphoreWin32HandleKHR(void);
void VKAPI_PTR zink_stub_GetSemaphoreWin32HandleKHR(void);
void VKAPI_PTR zink_stub_GetBufferDeviceAddressKHR(void);
void VKAPI_PTR zink_stub_GetBufferOpaqueCaptureAddressKHR(void);
void VKAPI_PTR zink_stub_GetDeviceMemoryOpaqueCaptureAddressKHR(void);
void VKAPI_PTR zink_stub_GetMemoryHostPointerPropertiesEXT(void);
void VKAPI_PTR zink_stub_GetImageMemoryRequirements2KHR(void);
void VKAPI_PTR zink_stub_GetBufferMemoryRequirements2KHR(void);
void VKAPI_PTR zink_stub_GetImageSparseMemoryRequirements2KHR(void);
void VKAPI_PTR zink_stub_SetDeviceMemoryPriorityEXT(void);
void VKAPI_PTR zink_stub_CmdDrawIndirectCountKHR(void);
void VKAPI_PTR zink_stub_CmdDrawIndexedIndirectCountKHR(void);
void VKAPI_PTR zink_stub_CreateShadersEXT(void);
void VKAPI_PTR zink_stub_DestroyShaderEXT(void);
void VKAPI_PTR zink_stub_GetShaderBinaryDataEXT(void);
void VKAPI_PTR zink_stub_CmdBindShadersEXT(void);
void VKAPI_PTR zink_stub_CmdSetCullModeEXT(void);
void VKAPI_PTR zink_stub_CmdSetFrontFaceEXT(void);
void VKAPI_PTR zink_stub_CmdSetPrimitiveTopologyEXT(void);
void VKAPI_PTR zink_stub_CmdSetViewportWithCountEXT(void);
void VKAPI_PTR zink_stub_CmdSetScissorWithCountEXT(void);
void VKAPI_PTR zink_stub_CmdBindVertexBuffers2EXT(void);
void VKAPI_PTR zink_stub_CmdSetDepthTestEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetDepthWriteEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetDepthCompareOpEXT(void);
void VKAPI_PTR zink_stub_CmdSetDepthBoundsTestEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetStencilTestEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetStencilOpEXT(void);
void VKAPI_PTR zink_stub_CmdSetVertexInputEXT(void);
void VKAPI_PTR zink_stub_CmdSetPatchControlPointsEXT(void);
void VKAPI_PTR zink_stub_CmdSetRasterizerDiscardEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetDepthBiasEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetLogicOpEXT(void);
void VKAPI_PTR zink_stub_CmdSetPrimitiveRestartEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetTessellationDomainOriginEXT(void);
void VKAPI_PTR zink_stub_CmdSetDepthClampEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetPolygonModeEXT(void);
void VKAPI_PTR zink_stub_CmdSetRasterizationSamplesEXT(void);
void VKAPI_PTR zink_stub_CmdSetSampleMaskEXT(void);
void VKAPI_PTR zink_stub_CmdSetAlphaToCoverageEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetAlphaToOneEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetLogicOpEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetColorBlendEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetColorBlendEquationEXT(void);
void VKAPI_PTR zink_stub_CmdSetColorWriteMaskEXT(void);
void VKAPI_PTR zink_stub_CmdSetRasterizationStreamEXT(void);
void VKAPI_PTR zink_stub_CmdSetConservativeRasterizationModeEXT(void);
void VKAPI_PTR zink_stub_CmdSetExtraPrimitiveOverestimationSizeEXT(void);
void VKAPI_PTR zink_stub_CmdSetDepthClipEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetSampleLocationsEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetColorBlendAdvancedEXT(void);
void VKAPI_PTR zink_stub_CmdSetProvokingVertexModeEXT(void);
void VKAPI_PTR zink_stub_CmdSetLineRasterizationModeEXT(void);
void VKAPI_PTR zink_stub_CmdSetLineStippleEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetDepthClipNegativeOneToOneEXT(void);
void VKAPI_PTR zink_stub_CmdSetViewportWScalingEnableNV(void);
void VKAPI_PTR zink_stub_CmdSetViewportSwizzleNV(void);
void VKAPI_PTR zink_stub_CmdSetCoverageToColorEnableNV(void);
void VKAPI_PTR zink_stub_CmdSetCoverageToColorLocationNV(void);
void VKAPI_PTR zink_stub_CmdSetCoverageModulationModeNV(void);
void VKAPI_PTR zink_stub_CmdSetCoverageModulationTableEnableNV(void);
void VKAPI_PTR zink_stub_CmdSetCoverageModulationTableNV(void);
void VKAPI_PTR zink_stub_CmdSetShadingRateImageEnableNV(void);
void VKAPI_PTR zink_stub_CmdSetRepresentativeFragmentTestEnableNV(void);
void VKAPI_PTR zink_stub_CmdSetCoverageReductionModeNV(void);
void VKAPI_PTR zink_stub_CmdSetAttachmentFeedbackLoopEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetSampleLocationsEXT(void);
void VKAPI_PTR zink_stub_GetDescriptorSetLayoutSizeEXT(void);
void VKAPI_PTR zink_stub_GetDescriptorSetLayoutBindingOffsetEXT(void);
void VKAPI_PTR zink_stub_GetDescriptorEXT(void);
void VKAPI_PTR zink_stub_CmdBindDescriptorBuffersEXT(void);
void VKAPI_PTR zink_stub_CmdSetDescriptorBufferOffsetsEXT(void);
void VKAPI_PTR zink_stub_CmdBindDescriptorBufferEmbeddedSamplersEXT(void);
void VKAPI_PTR zink_stub_GetBufferOpaqueCaptureDescriptorDataEXT(void);
void VKAPI_PTR zink_stub_GetImageOpaqueCaptureDescriptorDataEXT(void);
void VKAPI_PTR zink_stub_GetImageViewOpaqueCaptureDescriptorDataEXT(void);
void VKAPI_PTR zink_stub_GetSamplerOpaqueCaptureDescriptorDataEXT(void);
void VKAPI_PTR zink_stub_GetAccelerationStructureOpaqueCaptureDescriptorDataEXT(void);
void VKAPI_PTR zink_stub_CmdBeginConditionalRenderingEXT(void);
void VKAPI_PTR zink_stub_CmdEndConditionalRenderingEXT(void);
void VKAPI_PTR zink_stub_CmdBindTransformFeedbackBuffersEXT(void);
void VKAPI_PTR zink_stub_CmdBeginTransformFeedbackEXT(void);
void VKAPI_PTR zink_stub_CmdEndTransformFeedbackEXT(void);
void VKAPI_PTR zink_stub_CmdBeginQueryIndexedEXT(void);
void VKAPI_PTR zink_stub_CmdEndQueryIndexedEXT(void);
void VKAPI_PTR zink_stub_CmdDrawIndirectByteCountEXT(void);
void VKAPI_PTR zink_stub_CreateSamplerYcbcrConversionKHR(void);
void VKAPI_PTR zink_stub_DestroySamplerYcbcrConversionKHR(void);
void VKAPI_PTR zink_stub_GetImageDrmFormatModifierPropertiesEXT(void);
void VKAPI_PTR zink_stub_GetCalibratedTimestampsEXT(void);
void VKAPI_PTR zink_stub_CmdBeginRenderingKHR(void);
void VKAPI_PTR zink_stub_CmdEndRenderingKHR(void);
void VKAPI_PTR zink_stub_CmdSetRenderingAttachmentLocationsKHR(void);
void VKAPI_PTR zink_stub_CmdSetRenderingInputAttachmentIndicesKHR(void);
void VKAPI_PTR zink_stub_CmdSetCullModeEXT(void);
void VKAPI_PTR zink_stub_CmdSetFrontFaceEXT(void);
void VKAPI_PTR zink_stub_CmdSetPrimitiveTopologyEXT(void);
void VKAPI_PTR zink_stub_CmdSetViewportWithCountEXT(void);
void VKAPI_PTR zink_stub_CmdSetScissorWithCountEXT(void);
void VKAPI_PTR zink_stub_CmdBindVertexBuffers2EXT(void);
void VKAPI_PTR zink_stub_CmdSetDepthTestEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetDepthWriteEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetDepthCompareOpEXT(void);
void VKAPI_PTR zink_stub_CmdSetDepthBoundsTestEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetStencilTestEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetStencilOpEXT(void);
void VKAPI_PTR zink_stub_CmdSetPatchControlPointsEXT(void);
void VKAPI_PTR zink_stub_CmdSetRasterizerDiscardEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetDepthBiasEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetLogicOpEXT(void);
void VKAPI_PTR zink_stub_CmdSetPrimitiveRestartEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetDepthClampEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetPolygonModeEXT(void);
void VKAPI_PTR zink_stub_CmdSetRasterizationSamplesEXT(void);
void VKAPI_PTR zink_stub_CmdSetSampleMaskEXT(void);
void VKAPI_PTR zink_stub_CmdSetAlphaToCoverageEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetAlphaToOneEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetLogicOpEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetColorBlendEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetColorBlendEquationEXT(void);
void VKAPI_PTR zink_stub_CmdSetColorWriteMaskEXT(void);
void VKAPI_PTR zink_stub_CmdSetTessellationDomainOriginEXT(void);
void VKAPI_PTR zink_stub_CmdSetRasterizationStreamEXT(void);
void VKAPI_PTR zink_stub_CmdSetConservativeRasterizationModeEXT(void);
void VKAPI_PTR zink_stub_CmdSetExtraPrimitiveOverestimationSizeEXT(void);
void VKAPI_PTR zink_stub_CmdSetDepthClipEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetSampleLocationsEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetColorBlendAdvancedEXT(void);
void VKAPI_PTR zink_stub_CmdSetProvokingVertexModeEXT(void);
void VKAPI_PTR zink_stub_CmdSetLineRasterizationModeEXT(void);
void VKAPI_PTR zink_stub_CmdSetLineStippleEnableEXT(void);
void VKAPI_PTR zink_stub_CmdSetDepthClipNegativeOneToOneEXT(void);
void VKAPI_PTR zink_stub_CmdSetViewportWScalingEnableNV(void);
void VKAPI_PTR zink_stub_CmdSetViewportSwizzleNV(void);
void VKAPI_PTR zink_stub_CmdSetCoverageToColorEnableNV(void);
void VKAPI_PTR zink_stub_CmdSetCoverageToColorLocationNV(void);
void VKAPI_PTR zink_stub_CmdSetCoverageModulationModeNV(void);
void VKAPI_PTR zink_stub_CmdSetCoverageModulationTableEnableNV(void);
void VKAPI_PTR zink_stub_CmdSetCoverageModulationTableNV(void);
void VKAPI_PTR zink_stub_CmdSetShadingRateImageEnableNV(void);
void VKAPI_PTR zink_stub_CmdSetRepresentativeFragmentTestEnableNV(void);
void VKAPI_PTR zink_stub_CmdSetCoverageReductionModeNV(void);
void VKAPI_PTR zink_stub_GetSemaphoreCounterValueKHR(void);
void VKAPI_PTR zink_stub_WaitSemaphoresKHR(void);
void VKAPI_PTR zink_stub_SignalSemaphoreKHR(void);
void VKAPI_PTR zink_stub_CmdSetColorWriteEnableEXT(void);
void VKAPI_PTR zink_stub_CopyMemoryToImageEXT(void);
void VKAPI_PTR zink_stub_CopyImageToMemoryEXT(void);
void VKAPI_PTR zink_stub_CopyImageToImageEXT(void);
void VKAPI_PTR zink_stub_TransitionImageLayoutEXT(void);
void VKAPI_PTR zink_stub_GetImageSubresourceLayout2EXT(void);
void VKAPI_PTR zink_stub_CreateSwapchainKHR(void);
void VKAPI_PTR zink_stub_DestroySwapchainKHR(void);
void VKAPI_PTR zink_stub_GetSwapchainImagesKHR(void);
void VKAPI_PTR zink_stub_AcquireNextImageKHR(void);
void VKAPI_PTR zink_stub_QueuePresentKHR(void);
void VKAPI_PTR zink_stub_GetDeviceGroupPresentCapabilitiesKHR(void);
void VKAPI_PTR zink_stub_GetDeviceGroupSurfacePresentModesKHR(void);
void VKAPI_PTR zink_stub_AcquireNextImage2KHR(void);
void VKAPI_PTR zink_stub_CmdDrawMultiEXT(void);
void VKAPI_PTR zink_stub_CmdDrawMultiIndexedEXT(void);
void VKAPI_PTR zink_stub_CmdPushDescriptorSetKHR(void);
void VKAPI_PTR zink_stub_CmdPushDescriptorSetWithTemplateKHR(void);
void VKAPI_PTR zink_stub_CmdPushDescriptorSetWithTemplateKHR(void);
void VKAPI_PTR zink_stub_CreateDescriptorUpdateTemplateKHR(void);
void VKAPI_PTR zink_stub_DestroyDescriptorUpdateTemplateKHR(void);
void VKAPI_PTR zink_stub_UpdateDescriptorSetWithTemplateKHR(void);
void VKAPI_PTR zink_stub_CmdPushDescriptorSetWithTemplateKHR(void);
void VKAPI_PTR zink_stub_CmdSetLineStippleEXT(void);
void VKAPI_PTR zink_stub_CmdSetVertexInputEXT(void);

#endif
