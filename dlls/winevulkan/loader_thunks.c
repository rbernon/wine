/* Automatically generated from Vulkan vk.xml and video.xml; DO NOT EDIT!
 *
 * This file is generated from Vulkan vk.xml file covered
 * by the following copyright and permission notice:
 *
 * Copyright 2015-2025 The Khronos Group Inc.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 * and from Vulkan video.xml file covered
 * by the following copyright and permission notice:
 *
 * Copyright 2021-2025 The Khronos Group Inc.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "vulkan_loader.h"

WINE_DEFAULT_DEBUG_CHANNEL(vulkan);

VkResult WINAPI vkAcquireNextImage2KHR(VkDevice device, const VkAcquireNextImageInfoKHR *pAcquireInfo, uint32_t *pImageIndex)
{
    struct vkAcquireNextImage2KHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pAcquireInfo = pAcquireInfo;
    params.pImageIndex = pImageIndex;
    status = UNIX_CALL(vkAcquireNextImage2KHR, &params);
    assert(!status && "vkAcquireNextImage2KHR");
    return params.result;
}

VkResult WINAPI vkAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t *pImageIndex)
{
    struct vkAcquireNextImageKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.swapchain = swapchain;
    params.timeout = timeout;
    params.semaphore = semaphore;
    params.fence = fence;
    params.pImageIndex = pImageIndex;
    status = UNIX_CALL(vkAcquireNextImageKHR, &params);
    assert(!status && "vkAcquireNextImageKHR");
    return params.result;
}

VkResult WINAPI vkAcquireProfilingLockKHR(VkDevice device, const VkAcquireProfilingLockInfoKHR *pInfo)
{
    struct vkAcquireProfilingLockKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkAcquireProfilingLockKHR, &params);
    assert(!status && "vkAcquireProfilingLockKHR");
    return params.result;
}

VkResult WINAPI vkAllocateDescriptorSets(VkDevice device, const VkDescriptorSetAllocateInfo *pAllocateInfo, VkDescriptorSet *pDescriptorSets)
{
    struct vkAllocateDescriptorSets_params params;
    NTSTATUS status;
    params.device = device;
    params.pAllocateInfo = pAllocateInfo;
    params.pDescriptorSets = pDescriptorSets;
    status = UNIX_CALL(vkAllocateDescriptorSets, &params);
    assert(!status && "vkAllocateDescriptorSets");
    return params.result;
}

VkResult WINAPI vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo *pAllocateInfo, const VkAllocationCallbacks *pAllocator, VkDeviceMemory *pMemory)
{
    struct vkAllocateMemory_params params;
    NTSTATUS status;
    params.device = device;
    params.pAllocateInfo = pAllocateInfo;
    params.pAllocator = pAllocator;
    params.pMemory = pMemory;
    status = UNIX_CALL(vkAllocateMemory, &params);
    assert(!status && "vkAllocateMemory");
    return params.result;
}

void WINAPI vkAntiLagUpdateAMD(VkDevice device, const VkAntiLagDataAMD *pData)
{
    struct vkAntiLagUpdateAMD_params params;
    NTSTATUS status;
    params.device = device;
    params.pData = pData;
    status = UNIX_CALL(vkAntiLagUpdateAMD, &params);
    assert(!status && "vkAntiLagUpdateAMD");
}

VkResult WINAPI vkBeginCommandBuffer(VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo *pBeginInfo)
{
    struct vkBeginCommandBuffer_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.pBeginInfo = pBeginInfo;
    status = UNIX_CALL(vkBeginCommandBuffer, &params);
    assert(!status && "vkBeginCommandBuffer");
    return params.result;
}

VkResult WINAPI vkBindAccelerationStructureMemoryNV(VkDevice device, uint32_t bindInfoCount, const VkBindAccelerationStructureMemoryInfoNV *pBindInfos)
{
    struct vkBindAccelerationStructureMemoryNV_params params;
    NTSTATUS status;
    params.device = device;
    params.bindInfoCount = bindInfoCount;
    params.pBindInfos = pBindInfos;
    status = UNIX_CALL(vkBindAccelerationStructureMemoryNV, &params);
    assert(!status && "vkBindAccelerationStructureMemoryNV");
    return params.result;
}

VkResult WINAPI vkBindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset)
{
    struct vkBindBufferMemory_params params;
    NTSTATUS status;
    params.device = device;
    params.buffer = buffer;
    params.memory = memory;
    params.memoryOffset = memoryOffset;
    status = UNIX_CALL(vkBindBufferMemory, &params);
    assert(!status && "vkBindBufferMemory");
    return params.result;
}

VkResult WINAPI vkBindBufferMemory2(VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo *pBindInfos)
{
    struct vkBindBufferMemory2_params params;
    NTSTATUS status;
    params.device = device;
    params.bindInfoCount = bindInfoCount;
    params.pBindInfos = pBindInfos;
    status = UNIX_CALL(vkBindBufferMemory2, &params);
    assert(!status && "vkBindBufferMemory2");
    return params.result;
}

VkResult WINAPI vkBindBufferMemory2KHR(VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo *pBindInfos)
{
    struct vkBindBufferMemory2KHR_params params;
    NTSTATUS status;
    params.device = device;
    params.bindInfoCount = bindInfoCount;
    params.pBindInfos = pBindInfos;
    status = UNIX_CALL(vkBindBufferMemory2KHR, &params);
    assert(!status && "vkBindBufferMemory2KHR");
    return params.result;
}

VkResult WINAPI vkBindImageMemory(VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset)
{
    struct vkBindImageMemory_params params;
    NTSTATUS status;
    params.device = device;
    params.image = image;
    params.memory = memory;
    params.memoryOffset = memoryOffset;
    status = UNIX_CALL(vkBindImageMemory, &params);
    assert(!status && "vkBindImageMemory");
    return params.result;
}

VkResult WINAPI vkBindImageMemory2(VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo *pBindInfos)
{
    struct vkBindImageMemory2_params params;
    NTSTATUS status;
    params.device = device;
    params.bindInfoCount = bindInfoCount;
    params.pBindInfos = pBindInfos;
    status = UNIX_CALL(vkBindImageMemory2, &params);
    assert(!status && "vkBindImageMemory2");
    return params.result;
}

VkResult WINAPI vkBindImageMemory2KHR(VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo *pBindInfos)
{
    struct vkBindImageMemory2KHR_params params;
    NTSTATUS status;
    params.device = device;
    params.bindInfoCount = bindInfoCount;
    params.pBindInfos = pBindInfos;
    status = UNIX_CALL(vkBindImageMemory2KHR, &params);
    assert(!status && "vkBindImageMemory2KHR");
    return params.result;
}

VkResult WINAPI vkBindOpticalFlowSessionImageNV(VkDevice device, VkOpticalFlowSessionNV session, VkOpticalFlowSessionBindingPointNV bindingPoint, VkImageView view, VkImageLayout layout)
{
    struct vkBindOpticalFlowSessionImageNV_params params;
    NTSTATUS status;
    params.device = device;
    params.session = session;
    params.bindingPoint = bindingPoint;
    params.view = view;
    params.layout = layout;
    status = UNIX_CALL(vkBindOpticalFlowSessionImageNV, &params);
    assert(!status && "vkBindOpticalFlowSessionImageNV");
    return params.result;
}

VkResult WINAPI vkBindVideoSessionMemoryKHR(VkDevice device, VkVideoSessionKHR videoSession, uint32_t bindSessionMemoryInfoCount, const VkBindVideoSessionMemoryInfoKHR *pBindSessionMemoryInfos)
{
    struct vkBindVideoSessionMemoryKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.videoSession = videoSession;
    params.bindSessionMemoryInfoCount = bindSessionMemoryInfoCount;
    params.pBindSessionMemoryInfos = pBindSessionMemoryInfos;
    status = UNIX_CALL(vkBindVideoSessionMemoryKHR, &params);
    assert(!status && "vkBindVideoSessionMemoryKHR");
    return params.result;
}

VkResult WINAPI vkBuildAccelerationStructuresKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR *pInfos, const VkAccelerationStructureBuildRangeInfoKHR * const*ppBuildRangeInfos)
{
    struct vkBuildAccelerationStructuresKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.deferredOperation = deferredOperation;
    params.infoCount = infoCount;
    params.pInfos = pInfos;
    params.ppBuildRangeInfos = ppBuildRangeInfos;
    status = UNIX_CALL(vkBuildAccelerationStructuresKHR, &params);
    assert(!status && "vkBuildAccelerationStructuresKHR");
    return params.result;
}

VkResult WINAPI vkBuildMicromapsEXT(VkDevice device, VkDeferredOperationKHR deferredOperation, uint32_t infoCount, const VkMicromapBuildInfoEXT *pInfos)
{
    struct vkBuildMicromapsEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.deferredOperation = deferredOperation;
    params.infoCount = infoCount;
    params.pInfos = pInfos;
    status = UNIX_CALL(vkBuildMicromapsEXT, &params);
    assert(!status && "vkBuildMicromapsEXT");
    return params.result;
}

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdBeginConditionalRenderingEXT == 22 );
VK_FAST_THUNK( vkCmdBeginConditionalRenderingEXT, "22", 2 )
#else
void WINAPI vkCmdBeginConditionalRenderingEXT(VkCommandBuffer commandBuffer, const VkConditionalRenderingBeginInfoEXT *pConditionalRenderingBegin)
{
    struct vkCmdBeginConditionalRenderingEXT_params params;
    params.commandBuffer = commandBuffer;
    params.pConditionalRenderingBegin = pConditionalRenderingBegin;
    UNIX_CALL(vkCmdBeginConditionalRenderingEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdBeginDebugUtilsLabelEXT == 23 );
VK_FAST_THUNK( vkCmdBeginDebugUtilsLabelEXT, "23", 2 )
#else
void WINAPI vkCmdBeginDebugUtilsLabelEXT(VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT *pLabelInfo)
{
    struct vkCmdBeginDebugUtilsLabelEXT_params params;
    params.commandBuffer = commandBuffer;
    params.pLabelInfo = pLabelInfo;
    UNIX_CALL(vkCmdBeginDebugUtilsLabelEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdBeginQuery == 24 );
VK_FAST_THUNK( vkCmdBeginQuery, "24", 4 )
#else
void WINAPI vkCmdBeginQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags)
{
    struct vkCmdBeginQuery_params params;
    params.commandBuffer = commandBuffer;
    params.queryPool = queryPool;
    params.query = query;
    params.flags = flags;
    UNIX_CALL(vkCmdBeginQuery, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 5
C_ASSERT( unix_vkCmdBeginQueryIndexedEXT == 25 );
VK_FAST_THUNK( vkCmdBeginQueryIndexedEXT, "25", 5 )
#else
void WINAPI vkCmdBeginQueryIndexedEXT(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags, uint32_t index)
{
    struct vkCmdBeginQueryIndexedEXT_params params;
    params.commandBuffer = commandBuffer;
    params.queryPool = queryPool;
    params.query = query;
    params.flags = flags;
    params.index = index;
    UNIX_CALL(vkCmdBeginQueryIndexedEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdBeginRenderPass == 26 );
VK_FAST_THUNK( vkCmdBeginRenderPass, "26", 3 )
#else
void WINAPI vkCmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo *pRenderPassBegin, VkSubpassContents contents)
{
    struct vkCmdBeginRenderPass_params params;
    params.commandBuffer = commandBuffer;
    params.pRenderPassBegin = pRenderPassBegin;
    params.contents = contents;
    UNIX_CALL(vkCmdBeginRenderPass, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdBeginRenderPass2 == 27 );
VK_FAST_THUNK( vkCmdBeginRenderPass2, "27", 3 )
#else
void WINAPI vkCmdBeginRenderPass2(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo *pRenderPassBegin, const VkSubpassBeginInfo *pSubpassBeginInfo)
{
    struct vkCmdBeginRenderPass2_params params;
    params.commandBuffer = commandBuffer;
    params.pRenderPassBegin = pRenderPassBegin;
    params.pSubpassBeginInfo = pSubpassBeginInfo;
    UNIX_CALL(vkCmdBeginRenderPass2, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdBeginRenderPass2KHR == 28 );
VK_FAST_THUNK( vkCmdBeginRenderPass2KHR, "28", 3 )
#else
void WINAPI vkCmdBeginRenderPass2KHR(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo *pRenderPassBegin, const VkSubpassBeginInfo *pSubpassBeginInfo)
{
    struct vkCmdBeginRenderPass2KHR_params params;
    params.commandBuffer = commandBuffer;
    params.pRenderPassBegin = pRenderPassBegin;
    params.pSubpassBeginInfo = pSubpassBeginInfo;
    UNIX_CALL(vkCmdBeginRenderPass2KHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdBeginRendering == 29 );
VK_FAST_THUNK( vkCmdBeginRendering, "29", 2 )
#else
void WINAPI vkCmdBeginRendering(VkCommandBuffer commandBuffer, const VkRenderingInfo *pRenderingInfo)
{
    struct vkCmdBeginRendering_params params;
    params.commandBuffer = commandBuffer;
    params.pRenderingInfo = pRenderingInfo;
    UNIX_CALL(vkCmdBeginRendering, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdBeginRenderingKHR == 30 );
VK_FAST_THUNK( vkCmdBeginRenderingKHR, "30", 2 )
#else
void WINAPI vkCmdBeginRenderingKHR(VkCommandBuffer commandBuffer, const VkRenderingInfo *pRenderingInfo)
{
    struct vkCmdBeginRenderingKHR_params params;
    params.commandBuffer = commandBuffer;
    params.pRenderingInfo = pRenderingInfo;
    UNIX_CALL(vkCmdBeginRenderingKHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 5
C_ASSERT( unix_vkCmdBeginTransformFeedbackEXT == 31 );
VK_FAST_THUNK( vkCmdBeginTransformFeedbackEXT, "31", 5 )
#else
void WINAPI vkCmdBeginTransformFeedbackEXT(VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer *pCounterBuffers, const VkDeviceSize *pCounterBufferOffsets)
{
    struct vkCmdBeginTransformFeedbackEXT_params params;
    params.commandBuffer = commandBuffer;
    params.firstCounterBuffer = firstCounterBuffer;
    params.counterBufferCount = counterBufferCount;
    params.pCounterBuffers = pCounterBuffers;
    params.pCounterBufferOffsets = pCounterBufferOffsets;
    UNIX_CALL(vkCmdBeginTransformFeedbackEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdBeginVideoCodingKHR == 32 );
VK_FAST_THUNK( vkCmdBeginVideoCodingKHR, "32", 2 )
#else
void WINAPI vkCmdBeginVideoCodingKHR(VkCommandBuffer commandBuffer, const VkVideoBeginCodingInfoKHR *pBeginInfo)
{
    struct vkCmdBeginVideoCodingKHR_params params;
    params.commandBuffer = commandBuffer;
    params.pBeginInfo = pBeginInfo;
    UNIX_CALL(vkCmdBeginVideoCodingKHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdBindDescriptorBufferEmbeddedSamplers2EXT == 33 );
VK_FAST_THUNK( vkCmdBindDescriptorBufferEmbeddedSamplers2EXT, "33", 2 )
#else
void WINAPI vkCmdBindDescriptorBufferEmbeddedSamplers2EXT(VkCommandBuffer commandBuffer, const VkBindDescriptorBufferEmbeddedSamplersInfoEXT *pBindDescriptorBufferEmbeddedSamplersInfo)
{
    struct vkCmdBindDescriptorBufferEmbeddedSamplers2EXT_params params;
    params.commandBuffer = commandBuffer;
    params.pBindDescriptorBufferEmbeddedSamplersInfo = pBindDescriptorBufferEmbeddedSamplersInfo;
    UNIX_CALL(vkCmdBindDescriptorBufferEmbeddedSamplers2EXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdBindDescriptorBufferEmbeddedSamplersEXT == 34 );
VK_FAST_THUNK( vkCmdBindDescriptorBufferEmbeddedSamplersEXT, "34", 4 )
#else
void WINAPI vkCmdBindDescriptorBufferEmbeddedSamplersEXT(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t set)
{
    struct vkCmdBindDescriptorBufferEmbeddedSamplersEXT_params params;
    params.commandBuffer = commandBuffer;
    params.pipelineBindPoint = pipelineBindPoint;
    params.layout = layout;
    params.set = set;
    UNIX_CALL(vkCmdBindDescriptorBufferEmbeddedSamplersEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdBindDescriptorBuffersEXT == 35 );
VK_FAST_THUNK( vkCmdBindDescriptorBuffersEXT, "35", 3 )
#else
void WINAPI vkCmdBindDescriptorBuffersEXT(VkCommandBuffer commandBuffer, uint32_t bufferCount, const VkDescriptorBufferBindingInfoEXT *pBindingInfos)
{
    struct vkCmdBindDescriptorBuffersEXT_params params;
    params.commandBuffer = commandBuffer;
    params.bufferCount = bufferCount;
    params.pBindingInfos = pBindingInfos;
    UNIX_CALL(vkCmdBindDescriptorBuffersEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 8
C_ASSERT( unix_vkCmdBindDescriptorSets == 36 );
VK_FAST_THUNK( vkCmdBindDescriptorSets, "36", 8 )
#else
void WINAPI vkCmdBindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet *pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t *pDynamicOffsets)
{
    struct vkCmdBindDescriptorSets_params params;
    params.commandBuffer = commandBuffer;
    params.pipelineBindPoint = pipelineBindPoint;
    params.layout = layout;
    params.firstSet = firstSet;
    params.descriptorSetCount = descriptorSetCount;
    params.pDescriptorSets = pDescriptorSets;
    params.dynamicOffsetCount = dynamicOffsetCount;
    params.pDynamicOffsets = pDynamicOffsets;
    UNIX_CALL(vkCmdBindDescriptorSets, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdBindDescriptorSets2 == 37 );
VK_FAST_THUNK( vkCmdBindDescriptorSets2, "37", 2 )
#else
void WINAPI vkCmdBindDescriptorSets2(VkCommandBuffer commandBuffer, const VkBindDescriptorSetsInfo *pBindDescriptorSetsInfo)
{
    struct vkCmdBindDescriptorSets2_params params;
    params.commandBuffer = commandBuffer;
    params.pBindDescriptorSetsInfo = pBindDescriptorSetsInfo;
    UNIX_CALL(vkCmdBindDescriptorSets2, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdBindDescriptorSets2KHR == 38 );
VK_FAST_THUNK( vkCmdBindDescriptorSets2KHR, "38", 2 )
#else
void WINAPI vkCmdBindDescriptorSets2KHR(VkCommandBuffer commandBuffer, const VkBindDescriptorSetsInfo *pBindDescriptorSetsInfo)
{
    struct vkCmdBindDescriptorSets2KHR_params params;
    params.commandBuffer = commandBuffer;
    params.pBindDescriptorSetsInfo = pBindDescriptorSetsInfo;
    UNIX_CALL(vkCmdBindDescriptorSets2KHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdBindIndexBuffer == 39 );
VK_FAST_THUNK( vkCmdBindIndexBuffer, "39", 4 )
#else
void WINAPI vkCmdBindIndexBuffer(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType)
{
    struct vkCmdBindIndexBuffer_params params;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    params.indexType = indexType;
    UNIX_CALL(vkCmdBindIndexBuffer, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 5
C_ASSERT( unix_vkCmdBindIndexBuffer2 == 40 );
VK_FAST_THUNK( vkCmdBindIndexBuffer2, "40", 5 )
#else
void WINAPI vkCmdBindIndexBuffer2(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, VkIndexType indexType)
{
    struct vkCmdBindIndexBuffer2_params params;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    params.size = size;
    params.indexType = indexType;
    UNIX_CALL(vkCmdBindIndexBuffer2, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 5
C_ASSERT( unix_vkCmdBindIndexBuffer2KHR == 41 );
VK_FAST_THUNK( vkCmdBindIndexBuffer2KHR, "41", 5 )
#else
void WINAPI vkCmdBindIndexBuffer2KHR(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, VkIndexType indexType)
{
    struct vkCmdBindIndexBuffer2KHR_params params;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    params.size = size;
    params.indexType = indexType;
    UNIX_CALL(vkCmdBindIndexBuffer2KHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdBindInvocationMaskHUAWEI == 42 );
VK_FAST_THUNK( vkCmdBindInvocationMaskHUAWEI, "42", 3 )
#else
void WINAPI vkCmdBindInvocationMaskHUAWEI(VkCommandBuffer commandBuffer, VkImageView imageView, VkImageLayout imageLayout)
{
    struct vkCmdBindInvocationMaskHUAWEI_params params;
    params.commandBuffer = commandBuffer;
    params.imageView = imageView;
    params.imageLayout = imageLayout;
    UNIX_CALL(vkCmdBindInvocationMaskHUAWEI, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdBindPipeline == 43 );
VK_FAST_THUNK( vkCmdBindPipeline, "43", 3 )
#else
void WINAPI vkCmdBindPipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline)
{
    struct vkCmdBindPipeline_params params;
    params.commandBuffer = commandBuffer;
    params.pipelineBindPoint = pipelineBindPoint;
    params.pipeline = pipeline;
    UNIX_CALL(vkCmdBindPipeline, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdBindPipelineShaderGroupNV == 44 );
VK_FAST_THUNK( vkCmdBindPipelineShaderGroupNV, "44", 4 )
#else
void WINAPI vkCmdBindPipelineShaderGroupNV(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline, uint32_t groupIndex)
{
    struct vkCmdBindPipelineShaderGroupNV_params params;
    params.commandBuffer = commandBuffer;
    params.pipelineBindPoint = pipelineBindPoint;
    params.pipeline = pipeline;
    params.groupIndex = groupIndex;
    UNIX_CALL(vkCmdBindPipelineShaderGroupNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdBindShadersEXT == 45 );
VK_FAST_THUNK( vkCmdBindShadersEXT, "45", 4 )
#else
void WINAPI vkCmdBindShadersEXT(VkCommandBuffer commandBuffer, uint32_t stageCount, const VkShaderStageFlagBits *pStages, const VkShaderEXT *pShaders)
{
    struct vkCmdBindShadersEXT_params params;
    params.commandBuffer = commandBuffer;
    params.stageCount = stageCount;
    params.pStages = pStages;
    params.pShaders = pShaders;
    UNIX_CALL(vkCmdBindShadersEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdBindShadingRateImageNV == 46 );
VK_FAST_THUNK( vkCmdBindShadingRateImageNV, "46", 3 )
#else
void WINAPI vkCmdBindShadingRateImageNV(VkCommandBuffer commandBuffer, VkImageView imageView, VkImageLayout imageLayout)
{
    struct vkCmdBindShadingRateImageNV_params params;
    params.commandBuffer = commandBuffer;
    params.imageView = imageView;
    params.imageLayout = imageLayout;
    UNIX_CALL(vkCmdBindShadingRateImageNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 6
C_ASSERT( unix_vkCmdBindTransformFeedbackBuffersEXT == 47 );
VK_FAST_THUNK( vkCmdBindTransformFeedbackBuffersEXT, "47", 6 )
#else
void WINAPI vkCmdBindTransformFeedbackBuffersEXT(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer *pBuffers, const VkDeviceSize *pOffsets, const VkDeviceSize *pSizes)
{
    struct vkCmdBindTransformFeedbackBuffersEXT_params params;
    params.commandBuffer = commandBuffer;
    params.firstBinding = firstBinding;
    params.bindingCount = bindingCount;
    params.pBuffers = pBuffers;
    params.pOffsets = pOffsets;
    params.pSizes = pSizes;
    UNIX_CALL(vkCmdBindTransformFeedbackBuffersEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 5
C_ASSERT( unix_vkCmdBindVertexBuffers == 48 );
VK_FAST_THUNK( vkCmdBindVertexBuffers, "48", 5 )
#else
void WINAPI vkCmdBindVertexBuffers(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer *pBuffers, const VkDeviceSize *pOffsets)
{
    struct vkCmdBindVertexBuffers_params params;
    params.commandBuffer = commandBuffer;
    params.firstBinding = firstBinding;
    params.bindingCount = bindingCount;
    params.pBuffers = pBuffers;
    params.pOffsets = pOffsets;
    UNIX_CALL(vkCmdBindVertexBuffers, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 7
C_ASSERT( unix_vkCmdBindVertexBuffers2 == 49 );
VK_FAST_THUNK( vkCmdBindVertexBuffers2, "49", 7 )
#else
void WINAPI vkCmdBindVertexBuffers2(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer *pBuffers, const VkDeviceSize *pOffsets, const VkDeviceSize *pSizes, const VkDeviceSize *pStrides)
{
    struct vkCmdBindVertexBuffers2_params params;
    params.commandBuffer = commandBuffer;
    params.firstBinding = firstBinding;
    params.bindingCount = bindingCount;
    params.pBuffers = pBuffers;
    params.pOffsets = pOffsets;
    params.pSizes = pSizes;
    params.pStrides = pStrides;
    UNIX_CALL(vkCmdBindVertexBuffers2, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 7
C_ASSERT( unix_vkCmdBindVertexBuffers2EXT == 50 );
VK_FAST_THUNK( vkCmdBindVertexBuffers2EXT, "50", 7 )
#else
void WINAPI vkCmdBindVertexBuffers2EXT(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer *pBuffers, const VkDeviceSize *pOffsets, const VkDeviceSize *pSizes, const VkDeviceSize *pStrides)
{
    struct vkCmdBindVertexBuffers2EXT_params params;
    params.commandBuffer = commandBuffer;
    params.firstBinding = firstBinding;
    params.bindingCount = bindingCount;
    params.pBuffers = pBuffers;
    params.pOffsets = pOffsets;
    params.pSizes = pSizes;
    params.pStrides = pStrides;
    UNIX_CALL(vkCmdBindVertexBuffers2EXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 8
C_ASSERT( unix_vkCmdBlitImage == 51 );
VK_FAST_THUNK( vkCmdBlitImage, "51", 8 )
#else
void WINAPI vkCmdBlitImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageBlit *pRegions, VkFilter filter)
{
    struct vkCmdBlitImage_params params;
    params.commandBuffer = commandBuffer;
    params.srcImage = srcImage;
    params.srcImageLayout = srcImageLayout;
    params.dstImage = dstImage;
    params.dstImageLayout = dstImageLayout;
    params.regionCount = regionCount;
    params.pRegions = pRegions;
    params.filter = filter;
    UNIX_CALL(vkCmdBlitImage, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdBlitImage2 == 52 );
VK_FAST_THUNK( vkCmdBlitImage2, "52", 2 )
#else
void WINAPI vkCmdBlitImage2(VkCommandBuffer commandBuffer, const VkBlitImageInfo2 *pBlitImageInfo)
{
    struct vkCmdBlitImage2_params params;
    params.commandBuffer = commandBuffer;
    params.pBlitImageInfo = pBlitImageInfo;
    UNIX_CALL(vkCmdBlitImage2, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdBlitImage2KHR == 53 );
VK_FAST_THUNK( vkCmdBlitImage2KHR, "53", 2 )
#else
void WINAPI vkCmdBlitImage2KHR(VkCommandBuffer commandBuffer, const VkBlitImageInfo2 *pBlitImageInfo)
{
    struct vkCmdBlitImage2KHR_params params;
    params.commandBuffer = commandBuffer;
    params.pBlitImageInfo = pBlitImageInfo;
    UNIX_CALL(vkCmdBlitImage2KHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 9
C_ASSERT( unix_vkCmdBuildAccelerationStructureNV == 54 );
VK_FAST_THUNK( vkCmdBuildAccelerationStructureNV, "54", 9 )
#else
void WINAPI vkCmdBuildAccelerationStructureNV(VkCommandBuffer commandBuffer, const VkAccelerationStructureInfoNV *pInfo, VkBuffer instanceData, VkDeviceSize instanceOffset, VkBool32 update, VkAccelerationStructureNV dst, VkAccelerationStructureNV src, VkBuffer scratch, VkDeviceSize scratchOffset)
{
    struct vkCmdBuildAccelerationStructureNV_params params;
    params.commandBuffer = commandBuffer;
    params.pInfo = pInfo;
    params.instanceData = instanceData;
    params.instanceOffset = instanceOffset;
    params.update = update;
    params.dst = dst;
    params.src = src;
    params.scratch = scratch;
    params.scratchOffset = scratchOffset;
    UNIX_CALL(vkCmdBuildAccelerationStructureNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 6
C_ASSERT( unix_vkCmdBuildAccelerationStructuresIndirectKHR == 55 );
VK_FAST_THUNK( vkCmdBuildAccelerationStructuresIndirectKHR, "55", 6 )
#else
void WINAPI vkCmdBuildAccelerationStructuresIndirectKHR(VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR *pInfos, const VkDeviceAddress *pIndirectDeviceAddresses, const uint32_t *pIndirectStrides, const uint32_t * const*ppMaxPrimitiveCounts)
{
    struct vkCmdBuildAccelerationStructuresIndirectKHR_params params;
    params.commandBuffer = commandBuffer;
    params.infoCount = infoCount;
    params.pInfos = pInfos;
    params.pIndirectDeviceAddresses = pIndirectDeviceAddresses;
    params.pIndirectStrides = pIndirectStrides;
    params.ppMaxPrimitiveCounts = ppMaxPrimitiveCounts;
    UNIX_CALL(vkCmdBuildAccelerationStructuresIndirectKHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdBuildAccelerationStructuresKHR == 56 );
VK_FAST_THUNK( vkCmdBuildAccelerationStructuresKHR, "56", 4 )
#else
void WINAPI vkCmdBuildAccelerationStructuresKHR(VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR *pInfos, const VkAccelerationStructureBuildRangeInfoKHR * const*ppBuildRangeInfos)
{
    struct vkCmdBuildAccelerationStructuresKHR_params params;
    params.commandBuffer = commandBuffer;
    params.infoCount = infoCount;
    params.pInfos = pInfos;
    params.ppBuildRangeInfos = ppBuildRangeInfos;
    UNIX_CALL(vkCmdBuildAccelerationStructuresKHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdBuildClusterAccelerationStructureIndirectNV == 57 );
VK_FAST_THUNK( vkCmdBuildClusterAccelerationStructureIndirectNV, "57", 2 )
#else
void WINAPI vkCmdBuildClusterAccelerationStructureIndirectNV(VkCommandBuffer commandBuffer, const VkClusterAccelerationStructureCommandsInfoNV *pCommandInfos)
{
    struct vkCmdBuildClusterAccelerationStructureIndirectNV_params params;
    params.commandBuffer = commandBuffer;
    params.pCommandInfos = pCommandInfos;
    UNIX_CALL(vkCmdBuildClusterAccelerationStructureIndirectNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdBuildMicromapsEXT == 58 );
VK_FAST_THUNK( vkCmdBuildMicromapsEXT, "58", 3 )
#else
void WINAPI vkCmdBuildMicromapsEXT(VkCommandBuffer commandBuffer, uint32_t infoCount, const VkMicromapBuildInfoEXT *pInfos)
{
    struct vkCmdBuildMicromapsEXT_params params;
    params.commandBuffer = commandBuffer;
    params.infoCount = infoCount;
    params.pInfos = pInfos;
    UNIX_CALL(vkCmdBuildMicromapsEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdBuildPartitionedAccelerationStructuresNV == 59 );
VK_FAST_THUNK( vkCmdBuildPartitionedAccelerationStructuresNV, "59", 2 )
#else
void WINAPI vkCmdBuildPartitionedAccelerationStructuresNV(VkCommandBuffer commandBuffer, const VkBuildPartitionedAccelerationStructureInfoNV *pBuildInfo)
{
    struct vkCmdBuildPartitionedAccelerationStructuresNV_params params;
    params.commandBuffer = commandBuffer;
    params.pBuildInfo = pBuildInfo;
    UNIX_CALL(vkCmdBuildPartitionedAccelerationStructuresNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 5
C_ASSERT( unix_vkCmdClearAttachments == 60 );
VK_FAST_THUNK( vkCmdClearAttachments, "60", 5 )
#else
void WINAPI vkCmdClearAttachments(VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkClearAttachment *pAttachments, uint32_t rectCount, const VkClearRect *pRects)
{
    struct vkCmdClearAttachments_params params;
    params.commandBuffer = commandBuffer;
    params.attachmentCount = attachmentCount;
    params.pAttachments = pAttachments;
    params.rectCount = rectCount;
    params.pRects = pRects;
    UNIX_CALL(vkCmdClearAttachments, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 6
C_ASSERT( unix_vkCmdClearColorImage == 61 );
VK_FAST_THUNK( vkCmdClearColorImage, "61", 6 )
#else
void WINAPI vkCmdClearColorImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue *pColor, uint32_t rangeCount, const VkImageSubresourceRange *pRanges)
{
    struct vkCmdClearColorImage_params params;
    params.commandBuffer = commandBuffer;
    params.image = image;
    params.imageLayout = imageLayout;
    params.pColor = pColor;
    params.rangeCount = rangeCount;
    params.pRanges = pRanges;
    UNIX_CALL(vkCmdClearColorImage, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 6
C_ASSERT( unix_vkCmdClearDepthStencilImage == 62 );
VK_FAST_THUNK( vkCmdClearDepthStencilImage, "62", 6 )
#else
void WINAPI vkCmdClearDepthStencilImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue *pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange *pRanges)
{
    struct vkCmdClearDepthStencilImage_params params;
    params.commandBuffer = commandBuffer;
    params.image = image;
    params.imageLayout = imageLayout;
    params.pDepthStencil = pDepthStencil;
    params.rangeCount = rangeCount;
    params.pRanges = pRanges;
    UNIX_CALL(vkCmdClearDepthStencilImage, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdControlVideoCodingKHR == 63 );
VK_FAST_THUNK( vkCmdControlVideoCodingKHR, "63", 2 )
#else
void WINAPI vkCmdControlVideoCodingKHR(VkCommandBuffer commandBuffer, const VkVideoCodingControlInfoKHR *pCodingControlInfo)
{
    struct vkCmdControlVideoCodingKHR_params params;
    params.commandBuffer = commandBuffer;
    params.pCodingControlInfo = pCodingControlInfo;
    UNIX_CALL(vkCmdControlVideoCodingKHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdConvertCooperativeVectorMatrixNV == 64 );
VK_FAST_THUNK( vkCmdConvertCooperativeVectorMatrixNV, "64", 3 )
#else
void WINAPI vkCmdConvertCooperativeVectorMatrixNV(VkCommandBuffer commandBuffer, uint32_t infoCount, const VkConvertCooperativeVectorMatrixInfoNV *pInfos)
{
    struct vkCmdConvertCooperativeVectorMatrixNV_params params;
    params.commandBuffer = commandBuffer;
    params.infoCount = infoCount;
    params.pInfos = pInfos;
    UNIX_CALL(vkCmdConvertCooperativeVectorMatrixNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdCopyAccelerationStructureKHR == 65 );
VK_FAST_THUNK( vkCmdCopyAccelerationStructureKHR, "65", 2 )
#else
void WINAPI vkCmdCopyAccelerationStructureKHR(VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureInfoKHR *pInfo)
{
    struct vkCmdCopyAccelerationStructureKHR_params params;
    params.commandBuffer = commandBuffer;
    params.pInfo = pInfo;
    UNIX_CALL(vkCmdCopyAccelerationStructureKHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdCopyAccelerationStructureNV == 66 );
VK_FAST_THUNK( vkCmdCopyAccelerationStructureNV, "66", 4 )
#else
void WINAPI vkCmdCopyAccelerationStructureNV(VkCommandBuffer commandBuffer, VkAccelerationStructureNV dst, VkAccelerationStructureNV src, VkCopyAccelerationStructureModeKHR mode)
{
    struct vkCmdCopyAccelerationStructureNV_params params;
    params.commandBuffer = commandBuffer;
    params.dst = dst;
    params.src = src;
    params.mode = mode;
    UNIX_CALL(vkCmdCopyAccelerationStructureNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdCopyAccelerationStructureToMemoryKHR == 67 );
VK_FAST_THUNK( vkCmdCopyAccelerationStructureToMemoryKHR, "67", 2 )
#else
void WINAPI vkCmdCopyAccelerationStructureToMemoryKHR(VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureToMemoryInfoKHR *pInfo)
{
    struct vkCmdCopyAccelerationStructureToMemoryKHR_params params;
    params.commandBuffer = commandBuffer;
    params.pInfo = pInfo;
    UNIX_CALL(vkCmdCopyAccelerationStructureToMemoryKHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 5
C_ASSERT( unix_vkCmdCopyBuffer == 68 );
VK_FAST_THUNK( vkCmdCopyBuffer, "68", 5 )
#else
void WINAPI vkCmdCopyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy *pRegions)
{
    struct vkCmdCopyBuffer_params params;
    params.commandBuffer = commandBuffer;
    params.srcBuffer = srcBuffer;
    params.dstBuffer = dstBuffer;
    params.regionCount = regionCount;
    params.pRegions = pRegions;
    UNIX_CALL(vkCmdCopyBuffer, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdCopyBuffer2 == 69 );
VK_FAST_THUNK( vkCmdCopyBuffer2, "69", 2 )
#else
void WINAPI vkCmdCopyBuffer2(VkCommandBuffer commandBuffer, const VkCopyBufferInfo2 *pCopyBufferInfo)
{
    struct vkCmdCopyBuffer2_params params;
    params.commandBuffer = commandBuffer;
    params.pCopyBufferInfo = pCopyBufferInfo;
    UNIX_CALL(vkCmdCopyBuffer2, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdCopyBuffer2KHR == 70 );
VK_FAST_THUNK( vkCmdCopyBuffer2KHR, "70", 2 )
#else
void WINAPI vkCmdCopyBuffer2KHR(VkCommandBuffer commandBuffer, const VkCopyBufferInfo2 *pCopyBufferInfo)
{
    struct vkCmdCopyBuffer2KHR_params params;
    params.commandBuffer = commandBuffer;
    params.pCopyBufferInfo = pCopyBufferInfo;
    UNIX_CALL(vkCmdCopyBuffer2KHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 6
C_ASSERT( unix_vkCmdCopyBufferToImage == 71 );
VK_FAST_THUNK( vkCmdCopyBufferToImage, "71", 6 )
#else
void WINAPI vkCmdCopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkBufferImageCopy *pRegions)
{
    struct vkCmdCopyBufferToImage_params params;
    params.commandBuffer = commandBuffer;
    params.srcBuffer = srcBuffer;
    params.dstImage = dstImage;
    params.dstImageLayout = dstImageLayout;
    params.regionCount = regionCount;
    params.pRegions = pRegions;
    UNIX_CALL(vkCmdCopyBufferToImage, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdCopyBufferToImage2 == 72 );
VK_FAST_THUNK( vkCmdCopyBufferToImage2, "72", 2 )
#else
void WINAPI vkCmdCopyBufferToImage2(VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2 *pCopyBufferToImageInfo)
{
    struct vkCmdCopyBufferToImage2_params params;
    params.commandBuffer = commandBuffer;
    params.pCopyBufferToImageInfo = pCopyBufferToImageInfo;
    UNIX_CALL(vkCmdCopyBufferToImage2, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdCopyBufferToImage2KHR == 73 );
VK_FAST_THUNK( vkCmdCopyBufferToImage2KHR, "73", 2 )
#else
void WINAPI vkCmdCopyBufferToImage2KHR(VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2 *pCopyBufferToImageInfo)
{
    struct vkCmdCopyBufferToImage2KHR_params params;
    params.commandBuffer = commandBuffer;
    params.pCopyBufferToImageInfo = pCopyBufferToImageInfo;
    UNIX_CALL(vkCmdCopyBufferToImage2KHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 7
C_ASSERT( unix_vkCmdCopyImage == 74 );
VK_FAST_THUNK( vkCmdCopyImage, "74", 7 )
#else
void WINAPI vkCmdCopyImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageCopy *pRegions)
{
    struct vkCmdCopyImage_params params;
    params.commandBuffer = commandBuffer;
    params.srcImage = srcImage;
    params.srcImageLayout = srcImageLayout;
    params.dstImage = dstImage;
    params.dstImageLayout = dstImageLayout;
    params.regionCount = regionCount;
    params.pRegions = pRegions;
    UNIX_CALL(vkCmdCopyImage, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdCopyImage2 == 75 );
VK_FAST_THUNK( vkCmdCopyImage2, "75", 2 )
#else
void WINAPI vkCmdCopyImage2(VkCommandBuffer commandBuffer, const VkCopyImageInfo2 *pCopyImageInfo)
{
    struct vkCmdCopyImage2_params params;
    params.commandBuffer = commandBuffer;
    params.pCopyImageInfo = pCopyImageInfo;
    UNIX_CALL(vkCmdCopyImage2, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdCopyImage2KHR == 76 );
VK_FAST_THUNK( vkCmdCopyImage2KHR, "76", 2 )
#else
void WINAPI vkCmdCopyImage2KHR(VkCommandBuffer commandBuffer, const VkCopyImageInfo2 *pCopyImageInfo)
{
    struct vkCmdCopyImage2KHR_params params;
    params.commandBuffer = commandBuffer;
    params.pCopyImageInfo = pCopyImageInfo;
    UNIX_CALL(vkCmdCopyImage2KHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 6
C_ASSERT( unix_vkCmdCopyImageToBuffer == 77 );
VK_FAST_THUNK( vkCmdCopyImageToBuffer, "77", 6 )
#else
void WINAPI vkCmdCopyImageToBuffer(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferImageCopy *pRegions)
{
    struct vkCmdCopyImageToBuffer_params params;
    params.commandBuffer = commandBuffer;
    params.srcImage = srcImage;
    params.srcImageLayout = srcImageLayout;
    params.dstBuffer = dstBuffer;
    params.regionCount = regionCount;
    params.pRegions = pRegions;
    UNIX_CALL(vkCmdCopyImageToBuffer, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdCopyImageToBuffer2 == 78 );
VK_FAST_THUNK( vkCmdCopyImageToBuffer2, "78", 2 )
#else
void WINAPI vkCmdCopyImageToBuffer2(VkCommandBuffer commandBuffer, const VkCopyImageToBufferInfo2 *pCopyImageToBufferInfo)
{
    struct vkCmdCopyImageToBuffer2_params params;
    params.commandBuffer = commandBuffer;
    params.pCopyImageToBufferInfo = pCopyImageToBufferInfo;
    UNIX_CALL(vkCmdCopyImageToBuffer2, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdCopyImageToBuffer2KHR == 79 );
VK_FAST_THUNK( vkCmdCopyImageToBuffer2KHR, "79", 2 )
#else
void WINAPI vkCmdCopyImageToBuffer2KHR(VkCommandBuffer commandBuffer, const VkCopyImageToBufferInfo2 *pCopyImageToBufferInfo)
{
    struct vkCmdCopyImageToBuffer2KHR_params params;
    params.commandBuffer = commandBuffer;
    params.pCopyImageToBufferInfo = pCopyImageToBufferInfo;
    UNIX_CALL(vkCmdCopyImageToBuffer2KHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdCopyMemoryIndirectNV == 80 );
VK_FAST_THUNK( vkCmdCopyMemoryIndirectNV, "80", 4 )
#else
void WINAPI vkCmdCopyMemoryIndirectNV(VkCommandBuffer commandBuffer, VkDeviceAddress copyBufferAddress, uint32_t copyCount, uint32_t stride)
{
    struct vkCmdCopyMemoryIndirectNV_params params;
    params.commandBuffer = commandBuffer;
    params.copyBufferAddress = copyBufferAddress;
    params.copyCount = copyCount;
    params.stride = stride;
    UNIX_CALL(vkCmdCopyMemoryIndirectNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdCopyMemoryToAccelerationStructureKHR == 81 );
VK_FAST_THUNK( vkCmdCopyMemoryToAccelerationStructureKHR, "81", 2 )
#else
void WINAPI vkCmdCopyMemoryToAccelerationStructureKHR(VkCommandBuffer commandBuffer, const VkCopyMemoryToAccelerationStructureInfoKHR *pInfo)
{
    struct vkCmdCopyMemoryToAccelerationStructureKHR_params params;
    params.commandBuffer = commandBuffer;
    params.pInfo = pInfo;
    UNIX_CALL(vkCmdCopyMemoryToAccelerationStructureKHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 7
C_ASSERT( unix_vkCmdCopyMemoryToImageIndirectNV == 82 );
VK_FAST_THUNK( vkCmdCopyMemoryToImageIndirectNV, "82", 7 )
#else
void WINAPI vkCmdCopyMemoryToImageIndirectNV(VkCommandBuffer commandBuffer, VkDeviceAddress copyBufferAddress, uint32_t copyCount, uint32_t stride, VkImage dstImage, VkImageLayout dstImageLayout, const VkImageSubresourceLayers *pImageSubresources)
{
    struct vkCmdCopyMemoryToImageIndirectNV_params params;
    params.commandBuffer = commandBuffer;
    params.copyBufferAddress = copyBufferAddress;
    params.copyCount = copyCount;
    params.stride = stride;
    params.dstImage = dstImage;
    params.dstImageLayout = dstImageLayout;
    params.pImageSubresources = pImageSubresources;
    UNIX_CALL(vkCmdCopyMemoryToImageIndirectNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdCopyMemoryToMicromapEXT == 83 );
VK_FAST_THUNK( vkCmdCopyMemoryToMicromapEXT, "83", 2 )
#else
void WINAPI vkCmdCopyMemoryToMicromapEXT(VkCommandBuffer commandBuffer, const VkCopyMemoryToMicromapInfoEXT *pInfo)
{
    struct vkCmdCopyMemoryToMicromapEXT_params params;
    params.commandBuffer = commandBuffer;
    params.pInfo = pInfo;
    UNIX_CALL(vkCmdCopyMemoryToMicromapEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdCopyMicromapEXT == 84 );
VK_FAST_THUNK( vkCmdCopyMicromapEXT, "84", 2 )
#else
void WINAPI vkCmdCopyMicromapEXT(VkCommandBuffer commandBuffer, const VkCopyMicromapInfoEXT *pInfo)
{
    struct vkCmdCopyMicromapEXT_params params;
    params.commandBuffer = commandBuffer;
    params.pInfo = pInfo;
    UNIX_CALL(vkCmdCopyMicromapEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdCopyMicromapToMemoryEXT == 85 );
VK_FAST_THUNK( vkCmdCopyMicromapToMemoryEXT, "85", 2 )
#else
void WINAPI vkCmdCopyMicromapToMemoryEXT(VkCommandBuffer commandBuffer, const VkCopyMicromapToMemoryInfoEXT *pInfo)
{
    struct vkCmdCopyMicromapToMemoryEXT_params params;
    params.commandBuffer = commandBuffer;
    params.pInfo = pInfo;
    UNIX_CALL(vkCmdCopyMicromapToMemoryEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 8
C_ASSERT( unix_vkCmdCopyQueryPoolResults == 86 );
VK_FAST_THUNK( vkCmdCopyQueryPoolResults, "86", 8 )
#else
void WINAPI vkCmdCopyQueryPoolResults(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride, VkQueryResultFlags flags)
{
    struct vkCmdCopyQueryPoolResults_params params;
    params.commandBuffer = commandBuffer;
    params.queryPool = queryPool;
    params.firstQuery = firstQuery;
    params.queryCount = queryCount;
    params.dstBuffer = dstBuffer;
    params.dstOffset = dstOffset;
    params.stride = stride;
    params.flags = flags;
    UNIX_CALL(vkCmdCopyQueryPoolResults, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdCuLaunchKernelNVX == 87 );
VK_FAST_THUNK( vkCmdCuLaunchKernelNVX, "87", 2 )
#else
void WINAPI vkCmdCuLaunchKernelNVX(VkCommandBuffer commandBuffer, const VkCuLaunchInfoNVX *pLaunchInfo)
{
    struct vkCmdCuLaunchKernelNVX_params params;
    params.commandBuffer = commandBuffer;
    params.pLaunchInfo = pLaunchInfo;
    UNIX_CALL(vkCmdCuLaunchKernelNVX, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdCudaLaunchKernelNV == 88 );
VK_FAST_THUNK( vkCmdCudaLaunchKernelNV, "88", 2 )
#else
void WINAPI vkCmdCudaLaunchKernelNV(VkCommandBuffer commandBuffer, const VkCudaLaunchInfoNV *pLaunchInfo)
{
    struct vkCmdCudaLaunchKernelNV_params params;
    params.commandBuffer = commandBuffer;
    params.pLaunchInfo = pLaunchInfo;
    UNIX_CALL(vkCmdCudaLaunchKernelNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdDebugMarkerBeginEXT == 89 );
VK_FAST_THUNK( vkCmdDebugMarkerBeginEXT, "89", 2 )
#else
void WINAPI vkCmdDebugMarkerBeginEXT(VkCommandBuffer commandBuffer, const VkDebugMarkerMarkerInfoEXT *pMarkerInfo)
{
    struct vkCmdDebugMarkerBeginEXT_params params;
    params.commandBuffer = commandBuffer;
    params.pMarkerInfo = pMarkerInfo;
    UNIX_CALL(vkCmdDebugMarkerBeginEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 1
C_ASSERT( unix_vkCmdDebugMarkerEndEXT == 90 );
VK_FAST_THUNK( vkCmdDebugMarkerEndEXT, "90", 1 )
#else
void WINAPI vkCmdDebugMarkerEndEXT(VkCommandBuffer commandBuffer)
{
    struct vkCmdDebugMarkerEndEXT_params params;
    params.commandBuffer = commandBuffer;
    UNIX_CALL(vkCmdDebugMarkerEndEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdDebugMarkerInsertEXT == 91 );
VK_FAST_THUNK( vkCmdDebugMarkerInsertEXT, "91", 2 )
#else
void WINAPI vkCmdDebugMarkerInsertEXT(VkCommandBuffer commandBuffer, const VkDebugMarkerMarkerInfoEXT *pMarkerInfo)
{
    struct vkCmdDebugMarkerInsertEXT_params params;
    params.commandBuffer = commandBuffer;
    params.pMarkerInfo = pMarkerInfo;
    UNIX_CALL(vkCmdDebugMarkerInsertEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdDecodeVideoKHR == 92 );
VK_FAST_THUNK( vkCmdDecodeVideoKHR, "92", 2 )
#else
void WINAPI vkCmdDecodeVideoKHR(VkCommandBuffer commandBuffer, const VkVideoDecodeInfoKHR *pDecodeInfo)
{
    struct vkCmdDecodeVideoKHR_params params;
    params.commandBuffer = commandBuffer;
    params.pDecodeInfo = pDecodeInfo;
    UNIX_CALL(vkCmdDecodeVideoKHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdDecompressMemoryIndirectCountNV == 93 );
VK_FAST_THUNK( vkCmdDecompressMemoryIndirectCountNV, "93", 4 )
#else
void WINAPI vkCmdDecompressMemoryIndirectCountNV(VkCommandBuffer commandBuffer, VkDeviceAddress indirectCommandsAddress, VkDeviceAddress indirectCommandsCountAddress, uint32_t stride)
{
    struct vkCmdDecompressMemoryIndirectCountNV_params params;
    params.commandBuffer = commandBuffer;
    params.indirectCommandsAddress = indirectCommandsAddress;
    params.indirectCommandsCountAddress = indirectCommandsCountAddress;
    params.stride = stride;
    UNIX_CALL(vkCmdDecompressMemoryIndirectCountNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdDecompressMemoryNV == 94 );
VK_FAST_THUNK( vkCmdDecompressMemoryNV, "94", 3 )
#else
void WINAPI vkCmdDecompressMemoryNV(VkCommandBuffer commandBuffer, uint32_t decompressRegionCount, const VkDecompressMemoryRegionNV *pDecompressMemoryRegions)
{
    struct vkCmdDecompressMemoryNV_params params;
    params.commandBuffer = commandBuffer;
    params.decompressRegionCount = decompressRegionCount;
    params.pDecompressMemoryRegions = pDecompressMemoryRegions;
    UNIX_CALL(vkCmdDecompressMemoryNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdDispatch == 95 );
VK_FAST_THUNK( vkCmdDispatch, "95", 4 )
#else
void WINAPI vkCmdDispatch(VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    struct vkCmdDispatch_params params;
    params.commandBuffer = commandBuffer;
    params.groupCountX = groupCountX;
    params.groupCountY = groupCountY;
    params.groupCountZ = groupCountZ;
    UNIX_CALL(vkCmdDispatch, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 7
C_ASSERT( unix_vkCmdDispatchBase == 96 );
VK_FAST_THUNK( vkCmdDispatchBase, "96", 7 )
#else
void WINAPI vkCmdDispatchBase(VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    struct vkCmdDispatchBase_params params;
    params.commandBuffer = commandBuffer;
    params.baseGroupX = baseGroupX;
    params.baseGroupY = baseGroupY;
    params.baseGroupZ = baseGroupZ;
    params.groupCountX = groupCountX;
    params.groupCountY = groupCountY;
    params.groupCountZ = groupCountZ;
    UNIX_CALL(vkCmdDispatchBase, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 7
C_ASSERT( unix_vkCmdDispatchBaseKHR == 97 );
VK_FAST_THUNK( vkCmdDispatchBaseKHR, "97", 7 )
#else
void WINAPI vkCmdDispatchBaseKHR(VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    struct vkCmdDispatchBaseKHR_params params;
    params.commandBuffer = commandBuffer;
    params.baseGroupX = baseGroupX;
    params.baseGroupY = baseGroupY;
    params.baseGroupZ = baseGroupZ;
    params.groupCountX = groupCountX;
    params.groupCountY = groupCountY;
    params.groupCountZ = groupCountZ;
    UNIX_CALL(vkCmdDispatchBaseKHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdDispatchIndirect == 98 );
VK_FAST_THUNK( vkCmdDispatchIndirect, "98", 3 )
#else
void WINAPI vkCmdDispatchIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset)
{
    struct vkCmdDispatchIndirect_params params;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    UNIX_CALL(vkCmdDispatchIndirect, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 5
C_ASSERT( unix_vkCmdDraw == 99 );
VK_FAST_THUNK( vkCmdDraw, "99", 5 )
#else
void WINAPI vkCmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    struct vkCmdDraw_params params;
    params.commandBuffer = commandBuffer;
    params.vertexCount = vertexCount;
    params.instanceCount = instanceCount;
    params.firstVertex = firstVertex;
    params.firstInstance = firstInstance;
    UNIX_CALL(vkCmdDraw, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdDrawClusterHUAWEI == 100 );
VK_FAST_THUNK( vkCmdDrawClusterHUAWEI, "100", 4 )
#else
void WINAPI vkCmdDrawClusterHUAWEI(VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    struct vkCmdDrawClusterHUAWEI_params params;
    params.commandBuffer = commandBuffer;
    params.groupCountX = groupCountX;
    params.groupCountY = groupCountY;
    params.groupCountZ = groupCountZ;
    UNIX_CALL(vkCmdDrawClusterHUAWEI, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdDrawClusterIndirectHUAWEI == 101 );
VK_FAST_THUNK( vkCmdDrawClusterIndirectHUAWEI, "101", 3 )
#else
void WINAPI vkCmdDrawClusterIndirectHUAWEI(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset)
{
    struct vkCmdDrawClusterIndirectHUAWEI_params params;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    UNIX_CALL(vkCmdDrawClusterIndirectHUAWEI, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 6
C_ASSERT( unix_vkCmdDrawIndexed == 102 );
VK_FAST_THUNK( vkCmdDrawIndexed, "102", 6 )
#else
void WINAPI vkCmdDrawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
    struct vkCmdDrawIndexed_params params;
    params.commandBuffer = commandBuffer;
    params.indexCount = indexCount;
    params.instanceCount = instanceCount;
    params.firstIndex = firstIndex;
    params.vertexOffset = vertexOffset;
    params.firstInstance = firstInstance;
    UNIX_CALL(vkCmdDrawIndexed, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 5
C_ASSERT( unix_vkCmdDrawIndexedIndirect == 103 );
VK_FAST_THUNK( vkCmdDrawIndexedIndirect, "103", 5 )
#else
void WINAPI vkCmdDrawIndexedIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride)
{
    struct vkCmdDrawIndexedIndirect_params params;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    params.drawCount = drawCount;
    params.stride = stride;
    UNIX_CALL(vkCmdDrawIndexedIndirect, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 7
C_ASSERT( unix_vkCmdDrawIndexedIndirectCount == 104 );
VK_FAST_THUNK( vkCmdDrawIndexedIndirectCount, "104", 7 )
#else
void WINAPI vkCmdDrawIndexedIndirectCount(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
{
    struct vkCmdDrawIndexedIndirectCount_params params;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    params.countBuffer = countBuffer;
    params.countBufferOffset = countBufferOffset;
    params.maxDrawCount = maxDrawCount;
    params.stride = stride;
    UNIX_CALL(vkCmdDrawIndexedIndirectCount, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 7
C_ASSERT( unix_vkCmdDrawIndexedIndirectCountAMD == 105 );
VK_FAST_THUNK( vkCmdDrawIndexedIndirectCountAMD, "105", 7 )
#else
void WINAPI vkCmdDrawIndexedIndirectCountAMD(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
{
    struct vkCmdDrawIndexedIndirectCountAMD_params params;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    params.countBuffer = countBuffer;
    params.countBufferOffset = countBufferOffset;
    params.maxDrawCount = maxDrawCount;
    params.stride = stride;
    UNIX_CALL(vkCmdDrawIndexedIndirectCountAMD, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 7
C_ASSERT( unix_vkCmdDrawIndexedIndirectCountKHR == 106 );
VK_FAST_THUNK( vkCmdDrawIndexedIndirectCountKHR, "106", 7 )
#else
void WINAPI vkCmdDrawIndexedIndirectCountKHR(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
{
    struct vkCmdDrawIndexedIndirectCountKHR_params params;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    params.countBuffer = countBuffer;
    params.countBufferOffset = countBufferOffset;
    params.maxDrawCount = maxDrawCount;
    params.stride = stride;
    UNIX_CALL(vkCmdDrawIndexedIndirectCountKHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 5
C_ASSERT( unix_vkCmdDrawIndirect == 107 );
VK_FAST_THUNK( vkCmdDrawIndirect, "107", 5 )
#else
void WINAPI vkCmdDrawIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride)
{
    struct vkCmdDrawIndirect_params params;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    params.drawCount = drawCount;
    params.stride = stride;
    UNIX_CALL(vkCmdDrawIndirect, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 7
C_ASSERT( unix_vkCmdDrawIndirectByteCountEXT == 108 );
VK_FAST_THUNK( vkCmdDrawIndirectByteCountEXT, "108", 7 )
#else
void WINAPI vkCmdDrawIndirectByteCountEXT(VkCommandBuffer commandBuffer, uint32_t instanceCount, uint32_t firstInstance, VkBuffer counterBuffer, VkDeviceSize counterBufferOffset, uint32_t counterOffset, uint32_t vertexStride)
{
    struct vkCmdDrawIndirectByteCountEXT_params params;
    params.commandBuffer = commandBuffer;
    params.instanceCount = instanceCount;
    params.firstInstance = firstInstance;
    params.counterBuffer = counterBuffer;
    params.counterBufferOffset = counterBufferOffset;
    params.counterOffset = counterOffset;
    params.vertexStride = vertexStride;
    UNIX_CALL(vkCmdDrawIndirectByteCountEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 7
C_ASSERT( unix_vkCmdDrawIndirectCount == 109 );
VK_FAST_THUNK( vkCmdDrawIndirectCount, "109", 7 )
#else
void WINAPI vkCmdDrawIndirectCount(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
{
    struct vkCmdDrawIndirectCount_params params;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    params.countBuffer = countBuffer;
    params.countBufferOffset = countBufferOffset;
    params.maxDrawCount = maxDrawCount;
    params.stride = stride;
    UNIX_CALL(vkCmdDrawIndirectCount, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 7
C_ASSERT( unix_vkCmdDrawIndirectCountAMD == 110 );
VK_FAST_THUNK( vkCmdDrawIndirectCountAMD, "110", 7 )
#else
void WINAPI vkCmdDrawIndirectCountAMD(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
{
    struct vkCmdDrawIndirectCountAMD_params params;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    params.countBuffer = countBuffer;
    params.countBufferOffset = countBufferOffset;
    params.maxDrawCount = maxDrawCount;
    params.stride = stride;
    UNIX_CALL(vkCmdDrawIndirectCountAMD, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 7
C_ASSERT( unix_vkCmdDrawIndirectCountKHR == 111 );
VK_FAST_THUNK( vkCmdDrawIndirectCountKHR, "111", 7 )
#else
void WINAPI vkCmdDrawIndirectCountKHR(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
{
    struct vkCmdDrawIndirectCountKHR_params params;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    params.countBuffer = countBuffer;
    params.countBufferOffset = countBufferOffset;
    params.maxDrawCount = maxDrawCount;
    params.stride = stride;
    UNIX_CALL(vkCmdDrawIndirectCountKHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdDrawMeshTasksEXT == 112 );
VK_FAST_THUNK( vkCmdDrawMeshTasksEXT, "112", 4 )
#else
void WINAPI vkCmdDrawMeshTasksEXT(VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    struct vkCmdDrawMeshTasksEXT_params params;
    params.commandBuffer = commandBuffer;
    params.groupCountX = groupCountX;
    params.groupCountY = groupCountY;
    params.groupCountZ = groupCountZ;
    UNIX_CALL(vkCmdDrawMeshTasksEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 7
C_ASSERT( unix_vkCmdDrawMeshTasksIndirectCountEXT == 113 );
VK_FAST_THUNK( vkCmdDrawMeshTasksIndirectCountEXT, "113", 7 )
#else
void WINAPI vkCmdDrawMeshTasksIndirectCountEXT(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
{
    struct vkCmdDrawMeshTasksIndirectCountEXT_params params;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    params.countBuffer = countBuffer;
    params.countBufferOffset = countBufferOffset;
    params.maxDrawCount = maxDrawCount;
    params.stride = stride;
    UNIX_CALL(vkCmdDrawMeshTasksIndirectCountEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 7
C_ASSERT( unix_vkCmdDrawMeshTasksIndirectCountNV == 114 );
VK_FAST_THUNK( vkCmdDrawMeshTasksIndirectCountNV, "114", 7 )
#else
void WINAPI vkCmdDrawMeshTasksIndirectCountNV(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
{
    struct vkCmdDrawMeshTasksIndirectCountNV_params params;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    params.countBuffer = countBuffer;
    params.countBufferOffset = countBufferOffset;
    params.maxDrawCount = maxDrawCount;
    params.stride = stride;
    UNIX_CALL(vkCmdDrawMeshTasksIndirectCountNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 5
C_ASSERT( unix_vkCmdDrawMeshTasksIndirectEXT == 115 );
VK_FAST_THUNK( vkCmdDrawMeshTasksIndirectEXT, "115", 5 )
#else
void WINAPI vkCmdDrawMeshTasksIndirectEXT(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride)
{
    struct vkCmdDrawMeshTasksIndirectEXT_params params;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    params.drawCount = drawCount;
    params.stride = stride;
    UNIX_CALL(vkCmdDrawMeshTasksIndirectEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 5
C_ASSERT( unix_vkCmdDrawMeshTasksIndirectNV == 116 );
VK_FAST_THUNK( vkCmdDrawMeshTasksIndirectNV, "116", 5 )
#else
void WINAPI vkCmdDrawMeshTasksIndirectNV(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride)
{
    struct vkCmdDrawMeshTasksIndirectNV_params params;
    params.commandBuffer = commandBuffer;
    params.buffer = buffer;
    params.offset = offset;
    params.drawCount = drawCount;
    params.stride = stride;
    UNIX_CALL(vkCmdDrawMeshTasksIndirectNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdDrawMeshTasksNV == 117 );
VK_FAST_THUNK( vkCmdDrawMeshTasksNV, "117", 3 )
#else
void WINAPI vkCmdDrawMeshTasksNV(VkCommandBuffer commandBuffer, uint32_t taskCount, uint32_t firstTask)
{
    struct vkCmdDrawMeshTasksNV_params params;
    params.commandBuffer = commandBuffer;
    params.taskCount = taskCount;
    params.firstTask = firstTask;
    UNIX_CALL(vkCmdDrawMeshTasksNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 6
C_ASSERT( unix_vkCmdDrawMultiEXT == 118 );
VK_FAST_THUNK( vkCmdDrawMultiEXT, "118", 6 )
#else
void WINAPI vkCmdDrawMultiEXT(VkCommandBuffer commandBuffer, uint32_t drawCount, const VkMultiDrawInfoEXT *pVertexInfo, uint32_t instanceCount, uint32_t firstInstance, uint32_t stride)
{
    struct vkCmdDrawMultiEXT_params params;
    params.commandBuffer = commandBuffer;
    params.drawCount = drawCount;
    params.pVertexInfo = pVertexInfo;
    params.instanceCount = instanceCount;
    params.firstInstance = firstInstance;
    params.stride = stride;
    UNIX_CALL(vkCmdDrawMultiEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 7
C_ASSERT( unix_vkCmdDrawMultiIndexedEXT == 119 );
VK_FAST_THUNK( vkCmdDrawMultiIndexedEXT, "119", 7 )
#else
void WINAPI vkCmdDrawMultiIndexedEXT(VkCommandBuffer commandBuffer, uint32_t drawCount, const VkMultiDrawIndexedInfoEXT *pIndexInfo, uint32_t instanceCount, uint32_t firstInstance, uint32_t stride, const int32_t *pVertexOffset)
{
    struct vkCmdDrawMultiIndexedEXT_params params;
    params.commandBuffer = commandBuffer;
    params.drawCount = drawCount;
    params.pIndexInfo = pIndexInfo;
    params.instanceCount = instanceCount;
    params.firstInstance = firstInstance;
    params.stride = stride;
    params.pVertexOffset = pVertexOffset;
    UNIX_CALL(vkCmdDrawMultiIndexedEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdEncodeVideoKHR == 120 );
VK_FAST_THUNK( vkCmdEncodeVideoKHR, "120", 2 )
#else
void WINAPI vkCmdEncodeVideoKHR(VkCommandBuffer commandBuffer, const VkVideoEncodeInfoKHR *pEncodeInfo)
{
    struct vkCmdEncodeVideoKHR_params params;
    params.commandBuffer = commandBuffer;
    params.pEncodeInfo = pEncodeInfo;
    UNIX_CALL(vkCmdEncodeVideoKHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 1
C_ASSERT( unix_vkCmdEndConditionalRenderingEXT == 121 );
VK_FAST_THUNK( vkCmdEndConditionalRenderingEXT, "121", 1 )
#else
void WINAPI vkCmdEndConditionalRenderingEXT(VkCommandBuffer commandBuffer)
{
    struct vkCmdEndConditionalRenderingEXT_params params;
    params.commandBuffer = commandBuffer;
    UNIX_CALL(vkCmdEndConditionalRenderingEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 1
C_ASSERT( unix_vkCmdEndDebugUtilsLabelEXT == 122 );
VK_FAST_THUNK( vkCmdEndDebugUtilsLabelEXT, "122", 1 )
#else
void WINAPI vkCmdEndDebugUtilsLabelEXT(VkCommandBuffer commandBuffer)
{
    struct vkCmdEndDebugUtilsLabelEXT_params params;
    params.commandBuffer = commandBuffer;
    UNIX_CALL(vkCmdEndDebugUtilsLabelEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdEndQuery == 123 );
VK_FAST_THUNK( vkCmdEndQuery, "123", 3 )
#else
void WINAPI vkCmdEndQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query)
{
    struct vkCmdEndQuery_params params;
    params.commandBuffer = commandBuffer;
    params.queryPool = queryPool;
    params.query = query;
    UNIX_CALL(vkCmdEndQuery, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdEndQueryIndexedEXT == 124 );
VK_FAST_THUNK( vkCmdEndQueryIndexedEXT, "124", 4 )
#else
void WINAPI vkCmdEndQueryIndexedEXT(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, uint32_t index)
{
    struct vkCmdEndQueryIndexedEXT_params params;
    params.commandBuffer = commandBuffer;
    params.queryPool = queryPool;
    params.query = query;
    params.index = index;
    UNIX_CALL(vkCmdEndQueryIndexedEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 1
C_ASSERT( unix_vkCmdEndRenderPass == 125 );
VK_FAST_THUNK( vkCmdEndRenderPass, "125", 1 )
#else
void WINAPI vkCmdEndRenderPass(VkCommandBuffer commandBuffer)
{
    struct vkCmdEndRenderPass_params params;
    params.commandBuffer = commandBuffer;
    UNIX_CALL(vkCmdEndRenderPass, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdEndRenderPass2 == 126 );
VK_FAST_THUNK( vkCmdEndRenderPass2, "126", 2 )
#else
void WINAPI vkCmdEndRenderPass2(VkCommandBuffer commandBuffer, const VkSubpassEndInfo *pSubpassEndInfo)
{
    struct vkCmdEndRenderPass2_params params;
    params.commandBuffer = commandBuffer;
    params.pSubpassEndInfo = pSubpassEndInfo;
    UNIX_CALL(vkCmdEndRenderPass2, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdEndRenderPass2KHR == 127 );
VK_FAST_THUNK( vkCmdEndRenderPass2KHR, "127", 2 )
#else
void WINAPI vkCmdEndRenderPass2KHR(VkCommandBuffer commandBuffer, const VkSubpassEndInfo *pSubpassEndInfo)
{
    struct vkCmdEndRenderPass2KHR_params params;
    params.commandBuffer = commandBuffer;
    params.pSubpassEndInfo = pSubpassEndInfo;
    UNIX_CALL(vkCmdEndRenderPass2KHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 1
C_ASSERT( unix_vkCmdEndRendering == 128 );
VK_FAST_THUNK( vkCmdEndRendering, "128", 1 )
#else
void WINAPI vkCmdEndRendering(VkCommandBuffer commandBuffer)
{
    struct vkCmdEndRendering_params params;
    params.commandBuffer = commandBuffer;
    UNIX_CALL(vkCmdEndRendering, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 1
C_ASSERT( unix_vkCmdEndRenderingKHR == 129 );
VK_FAST_THUNK( vkCmdEndRenderingKHR, "129", 1 )
#else
void WINAPI vkCmdEndRenderingKHR(VkCommandBuffer commandBuffer)
{
    struct vkCmdEndRenderingKHR_params params;
    params.commandBuffer = commandBuffer;
    UNIX_CALL(vkCmdEndRenderingKHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 5
C_ASSERT( unix_vkCmdEndTransformFeedbackEXT == 130 );
VK_FAST_THUNK( vkCmdEndTransformFeedbackEXT, "130", 5 )
#else
void WINAPI vkCmdEndTransformFeedbackEXT(VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer *pCounterBuffers, const VkDeviceSize *pCounterBufferOffsets)
{
    struct vkCmdEndTransformFeedbackEXT_params params;
    params.commandBuffer = commandBuffer;
    params.firstCounterBuffer = firstCounterBuffer;
    params.counterBufferCount = counterBufferCount;
    params.pCounterBuffers = pCounterBuffers;
    params.pCounterBufferOffsets = pCounterBufferOffsets;
    UNIX_CALL(vkCmdEndTransformFeedbackEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdEndVideoCodingKHR == 131 );
VK_FAST_THUNK( vkCmdEndVideoCodingKHR, "131", 2 )
#else
void WINAPI vkCmdEndVideoCodingKHR(VkCommandBuffer commandBuffer, const VkVideoEndCodingInfoKHR *pEndCodingInfo)
{
    struct vkCmdEndVideoCodingKHR_params params;
    params.commandBuffer = commandBuffer;
    params.pEndCodingInfo = pEndCodingInfo;
    UNIX_CALL(vkCmdEndVideoCodingKHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdExecuteCommands == 132 );
VK_FAST_THUNK( vkCmdExecuteCommands, "132", 3 )
#else
void WINAPI vkCmdExecuteCommands(VkCommandBuffer commandBuffer, uint32_t commandBufferCount, const VkCommandBuffer *pCommandBuffers)
{
    struct vkCmdExecuteCommands_params params;
    params.commandBuffer = commandBuffer;
    params.commandBufferCount = commandBufferCount;
    params.pCommandBuffers = pCommandBuffers;
    UNIX_CALL(vkCmdExecuteCommands, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdExecuteGeneratedCommandsEXT == 133 );
VK_FAST_THUNK( vkCmdExecuteGeneratedCommandsEXT, "133", 3 )
#else
void WINAPI vkCmdExecuteGeneratedCommandsEXT(VkCommandBuffer commandBuffer, VkBool32 isPreprocessed, const VkGeneratedCommandsInfoEXT *pGeneratedCommandsInfo)
{
    struct vkCmdExecuteGeneratedCommandsEXT_params params;
    params.commandBuffer = commandBuffer;
    params.isPreprocessed = isPreprocessed;
    params.pGeneratedCommandsInfo = pGeneratedCommandsInfo;
    UNIX_CALL(vkCmdExecuteGeneratedCommandsEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdExecuteGeneratedCommandsNV == 134 );
VK_FAST_THUNK( vkCmdExecuteGeneratedCommandsNV, "134", 3 )
#else
void WINAPI vkCmdExecuteGeneratedCommandsNV(VkCommandBuffer commandBuffer, VkBool32 isPreprocessed, const VkGeneratedCommandsInfoNV *pGeneratedCommandsInfo)
{
    struct vkCmdExecuteGeneratedCommandsNV_params params;
    params.commandBuffer = commandBuffer;
    params.isPreprocessed = isPreprocessed;
    params.pGeneratedCommandsInfo = pGeneratedCommandsInfo;
    UNIX_CALL(vkCmdExecuteGeneratedCommandsNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 5
C_ASSERT( unix_vkCmdFillBuffer == 135 );
VK_FAST_THUNK( vkCmdFillBuffer, "135", 5 )
#else
void WINAPI vkCmdFillBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data)
{
    struct vkCmdFillBuffer_params params;
    params.commandBuffer = commandBuffer;
    params.dstBuffer = dstBuffer;
    params.dstOffset = dstOffset;
    params.size = size;
    params.data = data;
    UNIX_CALL(vkCmdFillBuffer, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdInsertDebugUtilsLabelEXT == 136 );
VK_FAST_THUNK( vkCmdInsertDebugUtilsLabelEXT, "136", 2 )
#else
void WINAPI vkCmdInsertDebugUtilsLabelEXT(VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT *pLabelInfo)
{
    struct vkCmdInsertDebugUtilsLabelEXT_params params;
    params.commandBuffer = commandBuffer;
    params.pLabelInfo = pLabelInfo;
    UNIX_CALL(vkCmdInsertDebugUtilsLabelEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdNextSubpass == 137 );
VK_FAST_THUNK( vkCmdNextSubpass, "137", 2 )
#else
void WINAPI vkCmdNextSubpass(VkCommandBuffer commandBuffer, VkSubpassContents contents)
{
    struct vkCmdNextSubpass_params params;
    params.commandBuffer = commandBuffer;
    params.contents = contents;
    UNIX_CALL(vkCmdNextSubpass, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdNextSubpass2 == 138 );
VK_FAST_THUNK( vkCmdNextSubpass2, "138", 3 )
#else
void WINAPI vkCmdNextSubpass2(VkCommandBuffer commandBuffer, const VkSubpassBeginInfo *pSubpassBeginInfo, const VkSubpassEndInfo *pSubpassEndInfo)
{
    struct vkCmdNextSubpass2_params params;
    params.commandBuffer = commandBuffer;
    params.pSubpassBeginInfo = pSubpassBeginInfo;
    params.pSubpassEndInfo = pSubpassEndInfo;
    UNIX_CALL(vkCmdNextSubpass2, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdNextSubpass2KHR == 139 );
VK_FAST_THUNK( vkCmdNextSubpass2KHR, "139", 3 )
#else
void WINAPI vkCmdNextSubpass2KHR(VkCommandBuffer commandBuffer, const VkSubpassBeginInfo *pSubpassBeginInfo, const VkSubpassEndInfo *pSubpassEndInfo)
{
    struct vkCmdNextSubpass2KHR_params params;
    params.commandBuffer = commandBuffer;
    params.pSubpassBeginInfo = pSubpassBeginInfo;
    params.pSubpassEndInfo = pSubpassEndInfo;
    UNIX_CALL(vkCmdNextSubpass2KHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdOpticalFlowExecuteNV == 140 );
VK_FAST_THUNK( vkCmdOpticalFlowExecuteNV, "140", 3 )
#else
void WINAPI vkCmdOpticalFlowExecuteNV(VkCommandBuffer commandBuffer, VkOpticalFlowSessionNV session, const VkOpticalFlowExecuteInfoNV *pExecuteInfo)
{
    struct vkCmdOpticalFlowExecuteNV_params params;
    params.commandBuffer = commandBuffer;
    params.session = session;
    params.pExecuteInfo = pExecuteInfo;
    UNIX_CALL(vkCmdOpticalFlowExecuteNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 10
C_ASSERT( unix_vkCmdPipelineBarrier == 141 );
VK_FAST_THUNK( vkCmdPipelineBarrier, "141", 10 )
#else
void WINAPI vkCmdPipelineBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const VkMemoryBarrier *pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier *pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier *pImageMemoryBarriers)
{
    struct vkCmdPipelineBarrier_params params;
    params.commandBuffer = commandBuffer;
    params.srcStageMask = srcStageMask;
    params.dstStageMask = dstStageMask;
    params.dependencyFlags = dependencyFlags;
    params.memoryBarrierCount = memoryBarrierCount;
    params.pMemoryBarriers = pMemoryBarriers;
    params.bufferMemoryBarrierCount = bufferMemoryBarrierCount;
    params.pBufferMemoryBarriers = pBufferMemoryBarriers;
    params.imageMemoryBarrierCount = imageMemoryBarrierCount;
    params.pImageMemoryBarriers = pImageMemoryBarriers;
    UNIX_CALL(vkCmdPipelineBarrier, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdPipelineBarrier2 == 142 );
VK_FAST_THUNK( vkCmdPipelineBarrier2, "142", 2 )
#else
void WINAPI vkCmdPipelineBarrier2(VkCommandBuffer commandBuffer, const VkDependencyInfo *pDependencyInfo)
{
    struct vkCmdPipelineBarrier2_params params;
    params.commandBuffer = commandBuffer;
    params.pDependencyInfo = pDependencyInfo;
    UNIX_CALL(vkCmdPipelineBarrier2, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdPipelineBarrier2KHR == 143 );
VK_FAST_THUNK( vkCmdPipelineBarrier2KHR, "143", 2 )
#else
void WINAPI vkCmdPipelineBarrier2KHR(VkCommandBuffer commandBuffer, const VkDependencyInfo *pDependencyInfo)
{
    struct vkCmdPipelineBarrier2KHR_params params;
    params.commandBuffer = commandBuffer;
    params.pDependencyInfo = pDependencyInfo;
    UNIX_CALL(vkCmdPipelineBarrier2KHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdPreprocessGeneratedCommandsEXT == 144 );
VK_FAST_THUNK( vkCmdPreprocessGeneratedCommandsEXT, "144", 3 )
#else
void WINAPI vkCmdPreprocessGeneratedCommandsEXT(VkCommandBuffer commandBuffer, const VkGeneratedCommandsInfoEXT *pGeneratedCommandsInfo, VkCommandBuffer stateCommandBuffer)
{
    struct vkCmdPreprocessGeneratedCommandsEXT_params params;
    params.commandBuffer = commandBuffer;
    params.pGeneratedCommandsInfo = pGeneratedCommandsInfo;
    params.stateCommandBuffer = stateCommandBuffer;
    UNIX_CALL(vkCmdPreprocessGeneratedCommandsEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdPreprocessGeneratedCommandsNV == 145 );
VK_FAST_THUNK( vkCmdPreprocessGeneratedCommandsNV, "145", 2 )
#else
void WINAPI vkCmdPreprocessGeneratedCommandsNV(VkCommandBuffer commandBuffer, const VkGeneratedCommandsInfoNV *pGeneratedCommandsInfo)
{
    struct vkCmdPreprocessGeneratedCommandsNV_params params;
    params.commandBuffer = commandBuffer;
    params.pGeneratedCommandsInfo = pGeneratedCommandsInfo;
    UNIX_CALL(vkCmdPreprocessGeneratedCommandsNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 6
C_ASSERT( unix_vkCmdPushConstants == 146 );
VK_FAST_THUNK( vkCmdPushConstants, "146", 6 )
#else
void WINAPI vkCmdPushConstants(VkCommandBuffer commandBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void *pValues)
{
    struct vkCmdPushConstants_params params;
    params.commandBuffer = commandBuffer;
    params.layout = layout;
    params.stageFlags = stageFlags;
    params.offset = offset;
    params.size = size;
    params.pValues = pValues;
    UNIX_CALL(vkCmdPushConstants, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdPushConstants2 == 147 );
VK_FAST_THUNK( vkCmdPushConstants2, "147", 2 )
#else
void WINAPI vkCmdPushConstants2(VkCommandBuffer commandBuffer, const VkPushConstantsInfo *pPushConstantsInfo)
{
    struct vkCmdPushConstants2_params params;
    params.commandBuffer = commandBuffer;
    params.pPushConstantsInfo = pPushConstantsInfo;
    UNIX_CALL(vkCmdPushConstants2, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdPushConstants2KHR == 148 );
VK_FAST_THUNK( vkCmdPushConstants2KHR, "148", 2 )
#else
void WINAPI vkCmdPushConstants2KHR(VkCommandBuffer commandBuffer, const VkPushConstantsInfo *pPushConstantsInfo)
{
    struct vkCmdPushConstants2KHR_params params;
    params.commandBuffer = commandBuffer;
    params.pPushConstantsInfo = pPushConstantsInfo;
    UNIX_CALL(vkCmdPushConstants2KHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 6
C_ASSERT( unix_vkCmdPushDescriptorSet == 149 );
VK_FAST_THUNK( vkCmdPushDescriptorSet, "149", 6 )
#else
void WINAPI vkCmdPushDescriptorSet(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t set, uint32_t descriptorWriteCount, const VkWriteDescriptorSet *pDescriptorWrites)
{
    struct vkCmdPushDescriptorSet_params params;
    params.commandBuffer = commandBuffer;
    params.pipelineBindPoint = pipelineBindPoint;
    params.layout = layout;
    params.set = set;
    params.descriptorWriteCount = descriptorWriteCount;
    params.pDescriptorWrites = pDescriptorWrites;
    UNIX_CALL(vkCmdPushDescriptorSet, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdPushDescriptorSet2 == 150 );
VK_FAST_THUNK( vkCmdPushDescriptorSet2, "150", 2 )
#else
void WINAPI vkCmdPushDescriptorSet2(VkCommandBuffer commandBuffer, const VkPushDescriptorSetInfo *pPushDescriptorSetInfo)
{
    struct vkCmdPushDescriptorSet2_params params;
    params.commandBuffer = commandBuffer;
    params.pPushDescriptorSetInfo = pPushDescriptorSetInfo;
    UNIX_CALL(vkCmdPushDescriptorSet2, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdPushDescriptorSet2KHR == 151 );
VK_FAST_THUNK( vkCmdPushDescriptorSet2KHR, "151", 2 )
#else
void WINAPI vkCmdPushDescriptorSet2KHR(VkCommandBuffer commandBuffer, const VkPushDescriptorSetInfo *pPushDescriptorSetInfo)
{
    struct vkCmdPushDescriptorSet2KHR_params params;
    params.commandBuffer = commandBuffer;
    params.pPushDescriptorSetInfo = pPushDescriptorSetInfo;
    UNIX_CALL(vkCmdPushDescriptorSet2KHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 6
C_ASSERT( unix_vkCmdPushDescriptorSetKHR == 152 );
VK_FAST_THUNK( vkCmdPushDescriptorSetKHR, "152", 6 )
#else
void WINAPI vkCmdPushDescriptorSetKHR(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t set, uint32_t descriptorWriteCount, const VkWriteDescriptorSet *pDescriptorWrites)
{
    struct vkCmdPushDescriptorSetKHR_params params;
    params.commandBuffer = commandBuffer;
    params.pipelineBindPoint = pipelineBindPoint;
    params.layout = layout;
    params.set = set;
    params.descriptorWriteCount = descriptorWriteCount;
    params.pDescriptorWrites = pDescriptorWrites;
    UNIX_CALL(vkCmdPushDescriptorSetKHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 5
C_ASSERT( unix_vkCmdPushDescriptorSetWithTemplate == 153 );
VK_FAST_THUNK( vkCmdPushDescriptorSetWithTemplate, "153", 5 )
#else
void WINAPI vkCmdPushDescriptorSetWithTemplate(VkCommandBuffer commandBuffer, VkDescriptorUpdateTemplate descriptorUpdateTemplate, VkPipelineLayout layout, uint32_t set, const void *pData)
{
    struct vkCmdPushDescriptorSetWithTemplate_params params;
    params.commandBuffer = commandBuffer;
    params.descriptorUpdateTemplate = descriptorUpdateTemplate;
    params.layout = layout;
    params.set = set;
    params.pData = pData;
    UNIX_CALL(vkCmdPushDescriptorSetWithTemplate, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdPushDescriptorSetWithTemplate2 == 154 );
VK_FAST_THUNK( vkCmdPushDescriptorSetWithTemplate2, "154", 2 )
#else
void WINAPI vkCmdPushDescriptorSetWithTemplate2(VkCommandBuffer commandBuffer, const VkPushDescriptorSetWithTemplateInfo *pPushDescriptorSetWithTemplateInfo)
{
    struct vkCmdPushDescriptorSetWithTemplate2_params params;
    params.commandBuffer = commandBuffer;
    params.pPushDescriptorSetWithTemplateInfo = pPushDescriptorSetWithTemplateInfo;
    UNIX_CALL(vkCmdPushDescriptorSetWithTemplate2, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdPushDescriptorSetWithTemplate2KHR == 155 );
VK_FAST_THUNK( vkCmdPushDescriptorSetWithTemplate2KHR, "155", 2 )
#else
void WINAPI vkCmdPushDescriptorSetWithTemplate2KHR(VkCommandBuffer commandBuffer, const VkPushDescriptorSetWithTemplateInfo *pPushDescriptorSetWithTemplateInfo)
{
    struct vkCmdPushDescriptorSetWithTemplate2KHR_params params;
    params.commandBuffer = commandBuffer;
    params.pPushDescriptorSetWithTemplateInfo = pPushDescriptorSetWithTemplateInfo;
    UNIX_CALL(vkCmdPushDescriptorSetWithTemplate2KHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 5
C_ASSERT( unix_vkCmdPushDescriptorSetWithTemplateKHR == 156 );
VK_FAST_THUNK( vkCmdPushDescriptorSetWithTemplateKHR, "156", 5 )
#else
void WINAPI vkCmdPushDescriptorSetWithTemplateKHR(VkCommandBuffer commandBuffer, VkDescriptorUpdateTemplate descriptorUpdateTemplate, VkPipelineLayout layout, uint32_t set, const void *pData)
{
    struct vkCmdPushDescriptorSetWithTemplateKHR_params params;
    params.commandBuffer = commandBuffer;
    params.descriptorUpdateTemplate = descriptorUpdateTemplate;
    params.layout = layout;
    params.set = set;
    params.pData = pData;
    UNIX_CALL(vkCmdPushDescriptorSetWithTemplateKHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdResetEvent == 157 );
VK_FAST_THUNK( vkCmdResetEvent, "157", 3 )
#else
void WINAPI vkCmdResetEvent(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask)
{
    struct vkCmdResetEvent_params params;
    params.commandBuffer = commandBuffer;
    params.event = event;
    params.stageMask = stageMask;
    UNIX_CALL(vkCmdResetEvent, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdResetEvent2 == 158 );
VK_FAST_THUNK( vkCmdResetEvent2, "158", 3 )
#else
void WINAPI vkCmdResetEvent2(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2 stageMask)
{
    struct vkCmdResetEvent2_params params;
    params.commandBuffer = commandBuffer;
    params.event = event;
    params.stageMask = stageMask;
    UNIX_CALL(vkCmdResetEvent2, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdResetEvent2KHR == 159 );
VK_FAST_THUNK( vkCmdResetEvent2KHR, "159", 3 )
#else
void WINAPI vkCmdResetEvent2KHR(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2 stageMask)
{
    struct vkCmdResetEvent2KHR_params params;
    params.commandBuffer = commandBuffer;
    params.event = event;
    params.stageMask = stageMask;
    UNIX_CALL(vkCmdResetEvent2KHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdResetQueryPool == 160 );
VK_FAST_THUNK( vkCmdResetQueryPool, "160", 4 )
#else
void WINAPI vkCmdResetQueryPool(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount)
{
    struct vkCmdResetQueryPool_params params;
    params.commandBuffer = commandBuffer;
    params.queryPool = queryPool;
    params.firstQuery = firstQuery;
    params.queryCount = queryCount;
    UNIX_CALL(vkCmdResetQueryPool, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 7
C_ASSERT( unix_vkCmdResolveImage == 161 );
VK_FAST_THUNK( vkCmdResolveImage, "161", 7 )
#else
void WINAPI vkCmdResolveImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageResolve *pRegions)
{
    struct vkCmdResolveImage_params params;
    params.commandBuffer = commandBuffer;
    params.srcImage = srcImage;
    params.srcImageLayout = srcImageLayout;
    params.dstImage = dstImage;
    params.dstImageLayout = dstImageLayout;
    params.regionCount = regionCount;
    params.pRegions = pRegions;
    UNIX_CALL(vkCmdResolveImage, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdResolveImage2 == 162 );
VK_FAST_THUNK( vkCmdResolveImage2, "162", 2 )
#else
void WINAPI vkCmdResolveImage2(VkCommandBuffer commandBuffer, const VkResolveImageInfo2 *pResolveImageInfo)
{
    struct vkCmdResolveImage2_params params;
    params.commandBuffer = commandBuffer;
    params.pResolveImageInfo = pResolveImageInfo;
    UNIX_CALL(vkCmdResolveImage2, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdResolveImage2KHR == 163 );
VK_FAST_THUNK( vkCmdResolveImage2KHR, "163", 2 )
#else
void WINAPI vkCmdResolveImage2KHR(VkCommandBuffer commandBuffer, const VkResolveImageInfo2 *pResolveImageInfo)
{
    struct vkCmdResolveImage2KHR_params params;
    params.commandBuffer = commandBuffer;
    params.pResolveImageInfo = pResolveImageInfo;
    UNIX_CALL(vkCmdResolveImage2KHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetAlphaToCoverageEnableEXT == 164 );
VK_FAST_THUNK( vkCmdSetAlphaToCoverageEnableEXT, "164", 2 )
#else
void WINAPI vkCmdSetAlphaToCoverageEnableEXT(VkCommandBuffer commandBuffer, VkBool32 alphaToCoverageEnable)
{
    struct vkCmdSetAlphaToCoverageEnableEXT_params params;
    params.commandBuffer = commandBuffer;
    params.alphaToCoverageEnable = alphaToCoverageEnable;
    UNIX_CALL(vkCmdSetAlphaToCoverageEnableEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetAlphaToOneEnableEXT == 165 );
VK_FAST_THUNK( vkCmdSetAlphaToOneEnableEXT, "165", 2 )
#else
void WINAPI vkCmdSetAlphaToOneEnableEXT(VkCommandBuffer commandBuffer, VkBool32 alphaToOneEnable)
{
    struct vkCmdSetAlphaToOneEnableEXT_params params;
    params.commandBuffer = commandBuffer;
    params.alphaToOneEnable = alphaToOneEnable;
    UNIX_CALL(vkCmdSetAlphaToOneEnableEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetAttachmentFeedbackLoopEnableEXT == 166 );
VK_FAST_THUNK( vkCmdSetAttachmentFeedbackLoopEnableEXT, "166", 2 )
#else
void WINAPI vkCmdSetAttachmentFeedbackLoopEnableEXT(VkCommandBuffer commandBuffer, VkImageAspectFlags aspectMask)
{
    struct vkCmdSetAttachmentFeedbackLoopEnableEXT_params params;
    params.commandBuffer = commandBuffer;
    params.aspectMask = aspectMask;
    UNIX_CALL(vkCmdSetAttachmentFeedbackLoopEnableEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetBlendConstants == 167 );
VK_FAST_THUNK( vkCmdSetBlendConstants, "167", 2 )
#else
void WINAPI vkCmdSetBlendConstants(VkCommandBuffer commandBuffer, const float blendConstants[4])
{
    struct vkCmdSetBlendConstants_params params;
    params.commandBuffer = commandBuffer;
    params.blendConstants = blendConstants;
    UNIX_CALL(vkCmdSetBlendConstants, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetCheckpointNV == 168 );
VK_FAST_THUNK( vkCmdSetCheckpointNV, "168", 2 )
#else
void WINAPI vkCmdSetCheckpointNV(VkCommandBuffer commandBuffer, const void *pCheckpointMarker)
{
    struct vkCmdSetCheckpointNV_params params;
    params.commandBuffer = commandBuffer;
    params.pCheckpointMarker = pCheckpointMarker;
    UNIX_CALL(vkCmdSetCheckpointNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdSetCoarseSampleOrderNV == 169 );
VK_FAST_THUNK( vkCmdSetCoarseSampleOrderNV, "169", 4 )
#else
void WINAPI vkCmdSetCoarseSampleOrderNV(VkCommandBuffer commandBuffer, VkCoarseSampleOrderTypeNV sampleOrderType, uint32_t customSampleOrderCount, const VkCoarseSampleOrderCustomNV *pCustomSampleOrders)
{
    struct vkCmdSetCoarseSampleOrderNV_params params;
    params.commandBuffer = commandBuffer;
    params.sampleOrderType = sampleOrderType;
    params.customSampleOrderCount = customSampleOrderCount;
    params.pCustomSampleOrders = pCustomSampleOrders;
    UNIX_CALL(vkCmdSetCoarseSampleOrderNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdSetColorBlendAdvancedEXT == 170 );
VK_FAST_THUNK( vkCmdSetColorBlendAdvancedEXT, "170", 4 )
#else
void WINAPI vkCmdSetColorBlendAdvancedEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorBlendAdvancedEXT *pColorBlendAdvanced)
{
    struct vkCmdSetColorBlendAdvancedEXT_params params;
    params.commandBuffer = commandBuffer;
    params.firstAttachment = firstAttachment;
    params.attachmentCount = attachmentCount;
    params.pColorBlendAdvanced = pColorBlendAdvanced;
    UNIX_CALL(vkCmdSetColorBlendAdvancedEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdSetColorBlendEnableEXT == 171 );
VK_FAST_THUNK( vkCmdSetColorBlendEnableEXT, "171", 4 )
#else
void WINAPI vkCmdSetColorBlendEnableEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkBool32 *pColorBlendEnables)
{
    struct vkCmdSetColorBlendEnableEXT_params params;
    params.commandBuffer = commandBuffer;
    params.firstAttachment = firstAttachment;
    params.attachmentCount = attachmentCount;
    params.pColorBlendEnables = pColorBlendEnables;
    UNIX_CALL(vkCmdSetColorBlendEnableEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdSetColorBlendEquationEXT == 172 );
VK_FAST_THUNK( vkCmdSetColorBlendEquationEXT, "172", 4 )
#else
void WINAPI vkCmdSetColorBlendEquationEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorBlendEquationEXT *pColorBlendEquations)
{
    struct vkCmdSetColorBlendEquationEXT_params params;
    params.commandBuffer = commandBuffer;
    params.firstAttachment = firstAttachment;
    params.attachmentCount = attachmentCount;
    params.pColorBlendEquations = pColorBlendEquations;
    UNIX_CALL(vkCmdSetColorBlendEquationEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdSetColorWriteEnableEXT == 173 );
VK_FAST_THUNK( vkCmdSetColorWriteEnableEXT, "173", 3 )
#else
void WINAPI vkCmdSetColorWriteEnableEXT(VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkBool32 *pColorWriteEnables)
{
    struct vkCmdSetColorWriteEnableEXT_params params;
    params.commandBuffer = commandBuffer;
    params.attachmentCount = attachmentCount;
    params.pColorWriteEnables = pColorWriteEnables;
    UNIX_CALL(vkCmdSetColorWriteEnableEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdSetColorWriteMaskEXT == 174 );
VK_FAST_THUNK( vkCmdSetColorWriteMaskEXT, "174", 4 )
#else
void WINAPI vkCmdSetColorWriteMaskEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorComponentFlags *pColorWriteMasks)
{
    struct vkCmdSetColorWriteMaskEXT_params params;
    params.commandBuffer = commandBuffer;
    params.firstAttachment = firstAttachment;
    params.attachmentCount = attachmentCount;
    params.pColorWriteMasks = pColorWriteMasks;
    UNIX_CALL(vkCmdSetColorWriteMaskEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetConservativeRasterizationModeEXT == 175 );
VK_FAST_THUNK( vkCmdSetConservativeRasterizationModeEXT, "175", 2 )
#else
void WINAPI vkCmdSetConservativeRasterizationModeEXT(VkCommandBuffer commandBuffer, VkConservativeRasterizationModeEXT conservativeRasterizationMode)
{
    struct vkCmdSetConservativeRasterizationModeEXT_params params;
    params.commandBuffer = commandBuffer;
    params.conservativeRasterizationMode = conservativeRasterizationMode;
    UNIX_CALL(vkCmdSetConservativeRasterizationModeEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetCoverageModulationModeNV == 176 );
VK_FAST_THUNK( vkCmdSetCoverageModulationModeNV, "176", 2 )
#else
void WINAPI vkCmdSetCoverageModulationModeNV(VkCommandBuffer commandBuffer, VkCoverageModulationModeNV coverageModulationMode)
{
    struct vkCmdSetCoverageModulationModeNV_params params;
    params.commandBuffer = commandBuffer;
    params.coverageModulationMode = coverageModulationMode;
    UNIX_CALL(vkCmdSetCoverageModulationModeNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetCoverageModulationTableEnableNV == 177 );
VK_FAST_THUNK( vkCmdSetCoverageModulationTableEnableNV, "177", 2 )
#else
void WINAPI vkCmdSetCoverageModulationTableEnableNV(VkCommandBuffer commandBuffer, VkBool32 coverageModulationTableEnable)
{
    struct vkCmdSetCoverageModulationTableEnableNV_params params;
    params.commandBuffer = commandBuffer;
    params.coverageModulationTableEnable = coverageModulationTableEnable;
    UNIX_CALL(vkCmdSetCoverageModulationTableEnableNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdSetCoverageModulationTableNV == 178 );
VK_FAST_THUNK( vkCmdSetCoverageModulationTableNV, "178", 3 )
#else
void WINAPI vkCmdSetCoverageModulationTableNV(VkCommandBuffer commandBuffer, uint32_t coverageModulationTableCount, const float *pCoverageModulationTable)
{
    struct vkCmdSetCoverageModulationTableNV_params params;
    params.commandBuffer = commandBuffer;
    params.coverageModulationTableCount = coverageModulationTableCount;
    params.pCoverageModulationTable = pCoverageModulationTable;
    UNIX_CALL(vkCmdSetCoverageModulationTableNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetCoverageReductionModeNV == 179 );
VK_FAST_THUNK( vkCmdSetCoverageReductionModeNV, "179", 2 )
#else
void WINAPI vkCmdSetCoverageReductionModeNV(VkCommandBuffer commandBuffer, VkCoverageReductionModeNV coverageReductionMode)
{
    struct vkCmdSetCoverageReductionModeNV_params params;
    params.commandBuffer = commandBuffer;
    params.coverageReductionMode = coverageReductionMode;
    UNIX_CALL(vkCmdSetCoverageReductionModeNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetCoverageToColorEnableNV == 180 );
VK_FAST_THUNK( vkCmdSetCoverageToColorEnableNV, "180", 2 )
#else
void WINAPI vkCmdSetCoverageToColorEnableNV(VkCommandBuffer commandBuffer, VkBool32 coverageToColorEnable)
{
    struct vkCmdSetCoverageToColorEnableNV_params params;
    params.commandBuffer = commandBuffer;
    params.coverageToColorEnable = coverageToColorEnable;
    UNIX_CALL(vkCmdSetCoverageToColorEnableNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetCoverageToColorLocationNV == 181 );
VK_FAST_THUNK( vkCmdSetCoverageToColorLocationNV, "181", 2 )
#else
void WINAPI vkCmdSetCoverageToColorLocationNV(VkCommandBuffer commandBuffer, uint32_t coverageToColorLocation)
{
    struct vkCmdSetCoverageToColorLocationNV_params params;
    params.commandBuffer = commandBuffer;
    params.coverageToColorLocation = coverageToColorLocation;
    UNIX_CALL(vkCmdSetCoverageToColorLocationNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetCullMode == 182 );
VK_FAST_THUNK( vkCmdSetCullMode, "182", 2 )
#else
void WINAPI vkCmdSetCullMode(VkCommandBuffer commandBuffer, VkCullModeFlags cullMode)
{
    struct vkCmdSetCullMode_params params;
    params.commandBuffer = commandBuffer;
    params.cullMode = cullMode;
    UNIX_CALL(vkCmdSetCullMode, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetCullModeEXT == 183 );
VK_FAST_THUNK( vkCmdSetCullModeEXT, "183", 2 )
#else
void WINAPI vkCmdSetCullModeEXT(VkCommandBuffer commandBuffer, VkCullModeFlags cullMode)
{
    struct vkCmdSetCullModeEXT_params params;
    params.commandBuffer = commandBuffer;
    params.cullMode = cullMode;
    UNIX_CALL(vkCmdSetCullModeEXT, &params);
}
#endif /* VK_FAST_THUNK */

void WINAPI vkCmdSetDepthBias(VkCommandBuffer commandBuffer, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor)
{
    struct vkCmdSetDepthBias_params params;
    params.commandBuffer = commandBuffer;
    params.depthBiasConstantFactor = depthBiasConstantFactor;
    params.depthBiasClamp = depthBiasClamp;
    params.depthBiasSlopeFactor = depthBiasSlopeFactor;
    UNIX_CALL(vkCmdSetDepthBias, &params);
}

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetDepthBias2EXT == 185 );
VK_FAST_THUNK( vkCmdSetDepthBias2EXT, "185", 2 )
#else
void WINAPI vkCmdSetDepthBias2EXT(VkCommandBuffer commandBuffer, const VkDepthBiasInfoEXT *pDepthBiasInfo)
{
    struct vkCmdSetDepthBias2EXT_params params;
    params.commandBuffer = commandBuffer;
    params.pDepthBiasInfo = pDepthBiasInfo;
    UNIX_CALL(vkCmdSetDepthBias2EXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetDepthBiasEnable == 186 );
VK_FAST_THUNK( vkCmdSetDepthBiasEnable, "186", 2 )
#else
void WINAPI vkCmdSetDepthBiasEnable(VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable)
{
    struct vkCmdSetDepthBiasEnable_params params;
    params.commandBuffer = commandBuffer;
    params.depthBiasEnable = depthBiasEnable;
    UNIX_CALL(vkCmdSetDepthBiasEnable, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetDepthBiasEnableEXT == 187 );
VK_FAST_THUNK( vkCmdSetDepthBiasEnableEXT, "187", 2 )
#else
void WINAPI vkCmdSetDepthBiasEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable)
{
    struct vkCmdSetDepthBiasEnableEXT_params params;
    params.commandBuffer = commandBuffer;
    params.depthBiasEnable = depthBiasEnable;
    UNIX_CALL(vkCmdSetDepthBiasEnableEXT, &params);
}
#endif /* VK_FAST_THUNK */

void WINAPI vkCmdSetDepthBounds(VkCommandBuffer commandBuffer, float minDepthBounds, float maxDepthBounds)
{
    struct vkCmdSetDepthBounds_params params;
    params.commandBuffer = commandBuffer;
    params.minDepthBounds = minDepthBounds;
    params.maxDepthBounds = maxDepthBounds;
    UNIX_CALL(vkCmdSetDepthBounds, &params);
}

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetDepthBoundsTestEnable == 189 );
VK_FAST_THUNK( vkCmdSetDepthBoundsTestEnable, "189", 2 )
#else
void WINAPI vkCmdSetDepthBoundsTestEnable(VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable)
{
    struct vkCmdSetDepthBoundsTestEnable_params params;
    params.commandBuffer = commandBuffer;
    params.depthBoundsTestEnable = depthBoundsTestEnable;
    UNIX_CALL(vkCmdSetDepthBoundsTestEnable, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetDepthBoundsTestEnableEXT == 190 );
VK_FAST_THUNK( vkCmdSetDepthBoundsTestEnableEXT, "190", 2 )
#else
void WINAPI vkCmdSetDepthBoundsTestEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable)
{
    struct vkCmdSetDepthBoundsTestEnableEXT_params params;
    params.commandBuffer = commandBuffer;
    params.depthBoundsTestEnable = depthBoundsTestEnable;
    UNIX_CALL(vkCmdSetDepthBoundsTestEnableEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetDepthClampEnableEXT == 191 );
VK_FAST_THUNK( vkCmdSetDepthClampEnableEXT, "191", 2 )
#else
void WINAPI vkCmdSetDepthClampEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthClampEnable)
{
    struct vkCmdSetDepthClampEnableEXT_params params;
    params.commandBuffer = commandBuffer;
    params.depthClampEnable = depthClampEnable;
    UNIX_CALL(vkCmdSetDepthClampEnableEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdSetDepthClampRangeEXT == 192 );
VK_FAST_THUNK( vkCmdSetDepthClampRangeEXT, "192", 3 )
#else
void WINAPI vkCmdSetDepthClampRangeEXT(VkCommandBuffer commandBuffer, VkDepthClampModeEXT depthClampMode, const VkDepthClampRangeEXT *pDepthClampRange)
{
    struct vkCmdSetDepthClampRangeEXT_params params;
    params.commandBuffer = commandBuffer;
    params.depthClampMode = depthClampMode;
    params.pDepthClampRange = pDepthClampRange;
    UNIX_CALL(vkCmdSetDepthClampRangeEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetDepthClipEnableEXT == 193 );
VK_FAST_THUNK( vkCmdSetDepthClipEnableEXT, "193", 2 )
#else
void WINAPI vkCmdSetDepthClipEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthClipEnable)
{
    struct vkCmdSetDepthClipEnableEXT_params params;
    params.commandBuffer = commandBuffer;
    params.depthClipEnable = depthClipEnable;
    UNIX_CALL(vkCmdSetDepthClipEnableEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetDepthClipNegativeOneToOneEXT == 194 );
VK_FAST_THUNK( vkCmdSetDepthClipNegativeOneToOneEXT, "194", 2 )
#else
void WINAPI vkCmdSetDepthClipNegativeOneToOneEXT(VkCommandBuffer commandBuffer, VkBool32 negativeOneToOne)
{
    struct vkCmdSetDepthClipNegativeOneToOneEXT_params params;
    params.commandBuffer = commandBuffer;
    params.negativeOneToOne = negativeOneToOne;
    UNIX_CALL(vkCmdSetDepthClipNegativeOneToOneEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetDepthCompareOp == 195 );
VK_FAST_THUNK( vkCmdSetDepthCompareOp, "195", 2 )
#else
void WINAPI vkCmdSetDepthCompareOp(VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp)
{
    struct vkCmdSetDepthCompareOp_params params;
    params.commandBuffer = commandBuffer;
    params.depthCompareOp = depthCompareOp;
    UNIX_CALL(vkCmdSetDepthCompareOp, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetDepthCompareOpEXT == 196 );
VK_FAST_THUNK( vkCmdSetDepthCompareOpEXT, "196", 2 )
#else
void WINAPI vkCmdSetDepthCompareOpEXT(VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp)
{
    struct vkCmdSetDepthCompareOpEXT_params params;
    params.commandBuffer = commandBuffer;
    params.depthCompareOp = depthCompareOp;
    UNIX_CALL(vkCmdSetDepthCompareOpEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetDepthTestEnable == 197 );
VK_FAST_THUNK( vkCmdSetDepthTestEnable, "197", 2 )
#else
void WINAPI vkCmdSetDepthTestEnable(VkCommandBuffer commandBuffer, VkBool32 depthTestEnable)
{
    struct vkCmdSetDepthTestEnable_params params;
    params.commandBuffer = commandBuffer;
    params.depthTestEnable = depthTestEnable;
    UNIX_CALL(vkCmdSetDepthTestEnable, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetDepthTestEnableEXT == 198 );
VK_FAST_THUNK( vkCmdSetDepthTestEnableEXT, "198", 2 )
#else
void WINAPI vkCmdSetDepthTestEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthTestEnable)
{
    struct vkCmdSetDepthTestEnableEXT_params params;
    params.commandBuffer = commandBuffer;
    params.depthTestEnable = depthTestEnable;
    UNIX_CALL(vkCmdSetDepthTestEnableEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetDepthWriteEnable == 199 );
VK_FAST_THUNK( vkCmdSetDepthWriteEnable, "199", 2 )
#else
void WINAPI vkCmdSetDepthWriteEnable(VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable)
{
    struct vkCmdSetDepthWriteEnable_params params;
    params.commandBuffer = commandBuffer;
    params.depthWriteEnable = depthWriteEnable;
    UNIX_CALL(vkCmdSetDepthWriteEnable, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetDepthWriteEnableEXT == 200 );
VK_FAST_THUNK( vkCmdSetDepthWriteEnableEXT, "200", 2 )
#else
void WINAPI vkCmdSetDepthWriteEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable)
{
    struct vkCmdSetDepthWriteEnableEXT_params params;
    params.commandBuffer = commandBuffer;
    params.depthWriteEnable = depthWriteEnable;
    UNIX_CALL(vkCmdSetDepthWriteEnableEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetDescriptorBufferOffsets2EXT == 201 );
VK_FAST_THUNK( vkCmdSetDescriptorBufferOffsets2EXT, "201", 2 )
#else
void WINAPI vkCmdSetDescriptorBufferOffsets2EXT(VkCommandBuffer commandBuffer, const VkSetDescriptorBufferOffsetsInfoEXT *pSetDescriptorBufferOffsetsInfo)
{
    struct vkCmdSetDescriptorBufferOffsets2EXT_params params;
    params.commandBuffer = commandBuffer;
    params.pSetDescriptorBufferOffsetsInfo = pSetDescriptorBufferOffsetsInfo;
    UNIX_CALL(vkCmdSetDescriptorBufferOffsets2EXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 7
C_ASSERT( unix_vkCmdSetDescriptorBufferOffsetsEXT == 202 );
VK_FAST_THUNK( vkCmdSetDescriptorBufferOffsetsEXT, "202", 7 )
#else
void WINAPI vkCmdSetDescriptorBufferOffsetsEXT(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t setCount, const uint32_t *pBufferIndices, const VkDeviceSize *pOffsets)
{
    struct vkCmdSetDescriptorBufferOffsetsEXT_params params;
    params.commandBuffer = commandBuffer;
    params.pipelineBindPoint = pipelineBindPoint;
    params.layout = layout;
    params.firstSet = firstSet;
    params.setCount = setCount;
    params.pBufferIndices = pBufferIndices;
    params.pOffsets = pOffsets;
    UNIX_CALL(vkCmdSetDescriptorBufferOffsetsEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetDeviceMask == 203 );
VK_FAST_THUNK( vkCmdSetDeviceMask, "203", 2 )
#else
void WINAPI vkCmdSetDeviceMask(VkCommandBuffer commandBuffer, uint32_t deviceMask)
{
    struct vkCmdSetDeviceMask_params params;
    params.commandBuffer = commandBuffer;
    params.deviceMask = deviceMask;
    UNIX_CALL(vkCmdSetDeviceMask, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetDeviceMaskKHR == 204 );
VK_FAST_THUNK( vkCmdSetDeviceMaskKHR, "204", 2 )
#else
void WINAPI vkCmdSetDeviceMaskKHR(VkCommandBuffer commandBuffer, uint32_t deviceMask)
{
    struct vkCmdSetDeviceMaskKHR_params params;
    params.commandBuffer = commandBuffer;
    params.deviceMask = deviceMask;
    UNIX_CALL(vkCmdSetDeviceMaskKHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdSetDiscardRectangleEXT == 205 );
VK_FAST_THUNK( vkCmdSetDiscardRectangleEXT, "205", 4 )
#else
void WINAPI vkCmdSetDiscardRectangleEXT(VkCommandBuffer commandBuffer, uint32_t firstDiscardRectangle, uint32_t discardRectangleCount, const VkRect2D *pDiscardRectangles)
{
    struct vkCmdSetDiscardRectangleEXT_params params;
    params.commandBuffer = commandBuffer;
    params.firstDiscardRectangle = firstDiscardRectangle;
    params.discardRectangleCount = discardRectangleCount;
    params.pDiscardRectangles = pDiscardRectangles;
    UNIX_CALL(vkCmdSetDiscardRectangleEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetDiscardRectangleEnableEXT == 206 );
VK_FAST_THUNK( vkCmdSetDiscardRectangleEnableEXT, "206", 2 )
#else
void WINAPI vkCmdSetDiscardRectangleEnableEXT(VkCommandBuffer commandBuffer, VkBool32 discardRectangleEnable)
{
    struct vkCmdSetDiscardRectangleEnableEXT_params params;
    params.commandBuffer = commandBuffer;
    params.discardRectangleEnable = discardRectangleEnable;
    UNIX_CALL(vkCmdSetDiscardRectangleEnableEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetDiscardRectangleModeEXT == 207 );
VK_FAST_THUNK( vkCmdSetDiscardRectangleModeEXT, "207", 2 )
#else
void WINAPI vkCmdSetDiscardRectangleModeEXT(VkCommandBuffer commandBuffer, VkDiscardRectangleModeEXT discardRectangleMode)
{
    struct vkCmdSetDiscardRectangleModeEXT_params params;
    params.commandBuffer = commandBuffer;
    params.discardRectangleMode = discardRectangleMode;
    UNIX_CALL(vkCmdSetDiscardRectangleModeEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdSetEvent == 208 );
VK_FAST_THUNK( vkCmdSetEvent, "208", 3 )
#else
void WINAPI vkCmdSetEvent(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask)
{
    struct vkCmdSetEvent_params params;
    params.commandBuffer = commandBuffer;
    params.event = event;
    params.stageMask = stageMask;
    UNIX_CALL(vkCmdSetEvent, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdSetEvent2 == 209 );
VK_FAST_THUNK( vkCmdSetEvent2, "209", 3 )
#else
void WINAPI vkCmdSetEvent2(VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfo *pDependencyInfo)
{
    struct vkCmdSetEvent2_params params;
    params.commandBuffer = commandBuffer;
    params.event = event;
    params.pDependencyInfo = pDependencyInfo;
    UNIX_CALL(vkCmdSetEvent2, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdSetEvent2KHR == 210 );
VK_FAST_THUNK( vkCmdSetEvent2KHR, "210", 3 )
#else
void WINAPI vkCmdSetEvent2KHR(VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfo *pDependencyInfo)
{
    struct vkCmdSetEvent2KHR_params params;
    params.commandBuffer = commandBuffer;
    params.event = event;
    params.pDependencyInfo = pDependencyInfo;
    UNIX_CALL(vkCmdSetEvent2KHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdSetExclusiveScissorEnableNV == 211 );
VK_FAST_THUNK( vkCmdSetExclusiveScissorEnableNV, "211", 4 )
#else
void WINAPI vkCmdSetExclusiveScissorEnableNV(VkCommandBuffer commandBuffer, uint32_t firstExclusiveScissor, uint32_t exclusiveScissorCount, const VkBool32 *pExclusiveScissorEnables)
{
    struct vkCmdSetExclusiveScissorEnableNV_params params;
    params.commandBuffer = commandBuffer;
    params.firstExclusiveScissor = firstExclusiveScissor;
    params.exclusiveScissorCount = exclusiveScissorCount;
    params.pExclusiveScissorEnables = pExclusiveScissorEnables;
    UNIX_CALL(vkCmdSetExclusiveScissorEnableNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdSetExclusiveScissorNV == 212 );
VK_FAST_THUNK( vkCmdSetExclusiveScissorNV, "212", 4 )
#else
void WINAPI vkCmdSetExclusiveScissorNV(VkCommandBuffer commandBuffer, uint32_t firstExclusiveScissor, uint32_t exclusiveScissorCount, const VkRect2D *pExclusiveScissors)
{
    struct vkCmdSetExclusiveScissorNV_params params;
    params.commandBuffer = commandBuffer;
    params.firstExclusiveScissor = firstExclusiveScissor;
    params.exclusiveScissorCount = exclusiveScissorCount;
    params.pExclusiveScissors = pExclusiveScissors;
    UNIX_CALL(vkCmdSetExclusiveScissorNV, &params);
}
#endif /* VK_FAST_THUNK */

void WINAPI vkCmdSetExtraPrimitiveOverestimationSizeEXT(VkCommandBuffer commandBuffer, float extraPrimitiveOverestimationSize)
{
    struct vkCmdSetExtraPrimitiveOverestimationSizeEXT_params params;
    params.commandBuffer = commandBuffer;
    params.extraPrimitiveOverestimationSize = extraPrimitiveOverestimationSize;
    UNIX_CALL(vkCmdSetExtraPrimitiveOverestimationSizeEXT, &params);
}

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdSetFragmentShadingRateEnumNV == 214 );
VK_FAST_THUNK( vkCmdSetFragmentShadingRateEnumNV, "214", 3 )
#else
void WINAPI vkCmdSetFragmentShadingRateEnumNV(VkCommandBuffer commandBuffer, VkFragmentShadingRateNV shadingRate, const VkFragmentShadingRateCombinerOpKHR combinerOps[2])
{
    struct vkCmdSetFragmentShadingRateEnumNV_params params;
    params.commandBuffer = commandBuffer;
    params.shadingRate = shadingRate;
    params.combinerOps = combinerOps;
    UNIX_CALL(vkCmdSetFragmentShadingRateEnumNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdSetFragmentShadingRateKHR == 215 );
VK_FAST_THUNK( vkCmdSetFragmentShadingRateKHR, "215", 3 )
#else
void WINAPI vkCmdSetFragmentShadingRateKHR(VkCommandBuffer commandBuffer, const VkExtent2D *pFragmentSize, const VkFragmentShadingRateCombinerOpKHR combinerOps[2])
{
    struct vkCmdSetFragmentShadingRateKHR_params params;
    params.commandBuffer = commandBuffer;
    params.pFragmentSize = pFragmentSize;
    params.combinerOps = combinerOps;
    UNIX_CALL(vkCmdSetFragmentShadingRateKHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetFrontFace == 216 );
VK_FAST_THUNK( vkCmdSetFrontFace, "216", 2 )
#else
void WINAPI vkCmdSetFrontFace(VkCommandBuffer commandBuffer, VkFrontFace frontFace)
{
    struct vkCmdSetFrontFace_params params;
    params.commandBuffer = commandBuffer;
    params.frontFace = frontFace;
    UNIX_CALL(vkCmdSetFrontFace, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetFrontFaceEXT == 217 );
VK_FAST_THUNK( vkCmdSetFrontFaceEXT, "217", 2 )
#else
void WINAPI vkCmdSetFrontFaceEXT(VkCommandBuffer commandBuffer, VkFrontFace frontFace)
{
    struct vkCmdSetFrontFaceEXT_params params;
    params.commandBuffer = commandBuffer;
    params.frontFace = frontFace;
    UNIX_CALL(vkCmdSetFrontFaceEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetLineRasterizationModeEXT == 218 );
VK_FAST_THUNK( vkCmdSetLineRasterizationModeEXT, "218", 2 )
#else
void WINAPI vkCmdSetLineRasterizationModeEXT(VkCommandBuffer commandBuffer, VkLineRasterizationModeEXT lineRasterizationMode)
{
    struct vkCmdSetLineRasterizationModeEXT_params params;
    params.commandBuffer = commandBuffer;
    params.lineRasterizationMode = lineRasterizationMode;
    UNIX_CALL(vkCmdSetLineRasterizationModeEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdSetLineStipple == 219 );
VK_FAST_THUNK( vkCmdSetLineStipple, "219", 3 )
#else
void WINAPI vkCmdSetLineStipple(VkCommandBuffer commandBuffer, uint32_t lineStippleFactor, uint16_t lineStipplePattern)
{
    struct vkCmdSetLineStipple_params params;
    params.commandBuffer = commandBuffer;
    params.lineStippleFactor = lineStippleFactor;
    params.lineStipplePattern = lineStipplePattern;
    UNIX_CALL(vkCmdSetLineStipple, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdSetLineStippleEXT == 220 );
VK_FAST_THUNK( vkCmdSetLineStippleEXT, "220", 3 )
#else
void WINAPI vkCmdSetLineStippleEXT(VkCommandBuffer commandBuffer, uint32_t lineStippleFactor, uint16_t lineStipplePattern)
{
    struct vkCmdSetLineStippleEXT_params params;
    params.commandBuffer = commandBuffer;
    params.lineStippleFactor = lineStippleFactor;
    params.lineStipplePattern = lineStipplePattern;
    UNIX_CALL(vkCmdSetLineStippleEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetLineStippleEnableEXT == 221 );
VK_FAST_THUNK( vkCmdSetLineStippleEnableEXT, "221", 2 )
#else
void WINAPI vkCmdSetLineStippleEnableEXT(VkCommandBuffer commandBuffer, VkBool32 stippledLineEnable)
{
    struct vkCmdSetLineStippleEnableEXT_params params;
    params.commandBuffer = commandBuffer;
    params.stippledLineEnable = stippledLineEnable;
    UNIX_CALL(vkCmdSetLineStippleEnableEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdSetLineStippleKHR == 222 );
VK_FAST_THUNK( vkCmdSetLineStippleKHR, "222", 3 )
#else
void WINAPI vkCmdSetLineStippleKHR(VkCommandBuffer commandBuffer, uint32_t lineStippleFactor, uint16_t lineStipplePattern)
{
    struct vkCmdSetLineStippleKHR_params params;
    params.commandBuffer = commandBuffer;
    params.lineStippleFactor = lineStippleFactor;
    params.lineStipplePattern = lineStipplePattern;
    UNIX_CALL(vkCmdSetLineStippleKHR, &params);
}
#endif /* VK_FAST_THUNK */

void WINAPI vkCmdSetLineWidth(VkCommandBuffer commandBuffer, float lineWidth)
{
    struct vkCmdSetLineWidth_params params;
    params.commandBuffer = commandBuffer;
    params.lineWidth = lineWidth;
    UNIX_CALL(vkCmdSetLineWidth, &params);
}

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetLogicOpEXT == 224 );
VK_FAST_THUNK( vkCmdSetLogicOpEXT, "224", 2 )
#else
void WINAPI vkCmdSetLogicOpEXT(VkCommandBuffer commandBuffer, VkLogicOp logicOp)
{
    struct vkCmdSetLogicOpEXT_params params;
    params.commandBuffer = commandBuffer;
    params.logicOp = logicOp;
    UNIX_CALL(vkCmdSetLogicOpEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetLogicOpEnableEXT == 225 );
VK_FAST_THUNK( vkCmdSetLogicOpEnableEXT, "225", 2 )
#else
void WINAPI vkCmdSetLogicOpEnableEXT(VkCommandBuffer commandBuffer, VkBool32 logicOpEnable)
{
    struct vkCmdSetLogicOpEnableEXT_params params;
    params.commandBuffer = commandBuffer;
    params.logicOpEnable = logicOpEnable;
    UNIX_CALL(vkCmdSetLogicOpEnableEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetPatchControlPointsEXT == 226 );
VK_FAST_THUNK( vkCmdSetPatchControlPointsEXT, "226", 2 )
#else
void WINAPI vkCmdSetPatchControlPointsEXT(VkCommandBuffer commandBuffer, uint32_t patchControlPoints)
{
    struct vkCmdSetPatchControlPointsEXT_params params;
    params.commandBuffer = commandBuffer;
    params.patchControlPoints = patchControlPoints;
    UNIX_CALL(vkCmdSetPatchControlPointsEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetPolygonModeEXT == 227 );
VK_FAST_THUNK( vkCmdSetPolygonModeEXT, "227", 2 )
#else
void WINAPI vkCmdSetPolygonModeEXT(VkCommandBuffer commandBuffer, VkPolygonMode polygonMode)
{
    struct vkCmdSetPolygonModeEXT_params params;
    params.commandBuffer = commandBuffer;
    params.polygonMode = polygonMode;
    UNIX_CALL(vkCmdSetPolygonModeEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetPrimitiveRestartEnable == 228 );
VK_FAST_THUNK( vkCmdSetPrimitiveRestartEnable, "228", 2 )
#else
void WINAPI vkCmdSetPrimitiveRestartEnable(VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable)
{
    struct vkCmdSetPrimitiveRestartEnable_params params;
    params.commandBuffer = commandBuffer;
    params.primitiveRestartEnable = primitiveRestartEnable;
    UNIX_CALL(vkCmdSetPrimitiveRestartEnable, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetPrimitiveRestartEnableEXT == 229 );
VK_FAST_THUNK( vkCmdSetPrimitiveRestartEnableEXT, "229", 2 )
#else
void WINAPI vkCmdSetPrimitiveRestartEnableEXT(VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable)
{
    struct vkCmdSetPrimitiveRestartEnableEXT_params params;
    params.commandBuffer = commandBuffer;
    params.primitiveRestartEnable = primitiveRestartEnable;
    UNIX_CALL(vkCmdSetPrimitiveRestartEnableEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetPrimitiveTopology == 230 );
VK_FAST_THUNK( vkCmdSetPrimitiveTopology, "230", 2 )
#else
void WINAPI vkCmdSetPrimitiveTopology(VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology)
{
    struct vkCmdSetPrimitiveTopology_params params;
    params.commandBuffer = commandBuffer;
    params.primitiveTopology = primitiveTopology;
    UNIX_CALL(vkCmdSetPrimitiveTopology, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetPrimitiveTopologyEXT == 231 );
VK_FAST_THUNK( vkCmdSetPrimitiveTopologyEXT, "231", 2 )
#else
void WINAPI vkCmdSetPrimitiveTopologyEXT(VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology)
{
    struct vkCmdSetPrimitiveTopologyEXT_params params;
    params.commandBuffer = commandBuffer;
    params.primitiveTopology = primitiveTopology;
    UNIX_CALL(vkCmdSetPrimitiveTopologyEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetProvokingVertexModeEXT == 232 );
VK_FAST_THUNK( vkCmdSetProvokingVertexModeEXT, "232", 2 )
#else
void WINAPI vkCmdSetProvokingVertexModeEXT(VkCommandBuffer commandBuffer, VkProvokingVertexModeEXT provokingVertexMode)
{
    struct vkCmdSetProvokingVertexModeEXT_params params;
    params.commandBuffer = commandBuffer;
    params.provokingVertexMode = provokingVertexMode;
    UNIX_CALL(vkCmdSetProvokingVertexModeEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetRasterizationSamplesEXT == 233 );
VK_FAST_THUNK( vkCmdSetRasterizationSamplesEXT, "233", 2 )
#else
void WINAPI vkCmdSetRasterizationSamplesEXT(VkCommandBuffer commandBuffer, VkSampleCountFlagBits rasterizationSamples)
{
    struct vkCmdSetRasterizationSamplesEXT_params params;
    params.commandBuffer = commandBuffer;
    params.rasterizationSamples = rasterizationSamples;
    UNIX_CALL(vkCmdSetRasterizationSamplesEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetRasterizationStreamEXT == 234 );
VK_FAST_THUNK( vkCmdSetRasterizationStreamEXT, "234", 2 )
#else
void WINAPI vkCmdSetRasterizationStreamEXT(VkCommandBuffer commandBuffer, uint32_t rasterizationStream)
{
    struct vkCmdSetRasterizationStreamEXT_params params;
    params.commandBuffer = commandBuffer;
    params.rasterizationStream = rasterizationStream;
    UNIX_CALL(vkCmdSetRasterizationStreamEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetRasterizerDiscardEnable == 235 );
VK_FAST_THUNK( vkCmdSetRasterizerDiscardEnable, "235", 2 )
#else
void WINAPI vkCmdSetRasterizerDiscardEnable(VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable)
{
    struct vkCmdSetRasterizerDiscardEnable_params params;
    params.commandBuffer = commandBuffer;
    params.rasterizerDiscardEnable = rasterizerDiscardEnable;
    UNIX_CALL(vkCmdSetRasterizerDiscardEnable, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetRasterizerDiscardEnableEXT == 236 );
VK_FAST_THUNK( vkCmdSetRasterizerDiscardEnableEXT, "236", 2 )
#else
void WINAPI vkCmdSetRasterizerDiscardEnableEXT(VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable)
{
    struct vkCmdSetRasterizerDiscardEnableEXT_params params;
    params.commandBuffer = commandBuffer;
    params.rasterizerDiscardEnable = rasterizerDiscardEnable;
    UNIX_CALL(vkCmdSetRasterizerDiscardEnableEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetRayTracingPipelineStackSizeKHR == 237 );
VK_FAST_THUNK( vkCmdSetRayTracingPipelineStackSizeKHR, "237", 2 )
#else
void WINAPI vkCmdSetRayTracingPipelineStackSizeKHR(VkCommandBuffer commandBuffer, uint32_t pipelineStackSize)
{
    struct vkCmdSetRayTracingPipelineStackSizeKHR_params params;
    params.commandBuffer = commandBuffer;
    params.pipelineStackSize = pipelineStackSize;
    UNIX_CALL(vkCmdSetRayTracingPipelineStackSizeKHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetRenderingAttachmentLocations == 238 );
VK_FAST_THUNK( vkCmdSetRenderingAttachmentLocations, "238", 2 )
#else
void WINAPI vkCmdSetRenderingAttachmentLocations(VkCommandBuffer commandBuffer, const VkRenderingAttachmentLocationInfo *pLocationInfo)
{
    struct vkCmdSetRenderingAttachmentLocations_params params;
    params.commandBuffer = commandBuffer;
    params.pLocationInfo = pLocationInfo;
    UNIX_CALL(vkCmdSetRenderingAttachmentLocations, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetRenderingAttachmentLocationsKHR == 239 );
VK_FAST_THUNK( vkCmdSetRenderingAttachmentLocationsKHR, "239", 2 )
#else
void WINAPI vkCmdSetRenderingAttachmentLocationsKHR(VkCommandBuffer commandBuffer, const VkRenderingAttachmentLocationInfo *pLocationInfo)
{
    struct vkCmdSetRenderingAttachmentLocationsKHR_params params;
    params.commandBuffer = commandBuffer;
    params.pLocationInfo = pLocationInfo;
    UNIX_CALL(vkCmdSetRenderingAttachmentLocationsKHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetRenderingInputAttachmentIndices == 240 );
VK_FAST_THUNK( vkCmdSetRenderingInputAttachmentIndices, "240", 2 )
#else
void WINAPI vkCmdSetRenderingInputAttachmentIndices(VkCommandBuffer commandBuffer, const VkRenderingInputAttachmentIndexInfo *pInputAttachmentIndexInfo)
{
    struct vkCmdSetRenderingInputAttachmentIndices_params params;
    params.commandBuffer = commandBuffer;
    params.pInputAttachmentIndexInfo = pInputAttachmentIndexInfo;
    UNIX_CALL(vkCmdSetRenderingInputAttachmentIndices, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetRenderingInputAttachmentIndicesKHR == 241 );
VK_FAST_THUNK( vkCmdSetRenderingInputAttachmentIndicesKHR, "241", 2 )
#else
void WINAPI vkCmdSetRenderingInputAttachmentIndicesKHR(VkCommandBuffer commandBuffer, const VkRenderingInputAttachmentIndexInfo *pInputAttachmentIndexInfo)
{
    struct vkCmdSetRenderingInputAttachmentIndicesKHR_params params;
    params.commandBuffer = commandBuffer;
    params.pInputAttachmentIndexInfo = pInputAttachmentIndexInfo;
    UNIX_CALL(vkCmdSetRenderingInputAttachmentIndicesKHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetRepresentativeFragmentTestEnableNV == 242 );
VK_FAST_THUNK( vkCmdSetRepresentativeFragmentTestEnableNV, "242", 2 )
#else
void WINAPI vkCmdSetRepresentativeFragmentTestEnableNV(VkCommandBuffer commandBuffer, VkBool32 representativeFragmentTestEnable)
{
    struct vkCmdSetRepresentativeFragmentTestEnableNV_params params;
    params.commandBuffer = commandBuffer;
    params.representativeFragmentTestEnable = representativeFragmentTestEnable;
    UNIX_CALL(vkCmdSetRepresentativeFragmentTestEnableNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetSampleLocationsEXT == 243 );
VK_FAST_THUNK( vkCmdSetSampleLocationsEXT, "243", 2 )
#else
void WINAPI vkCmdSetSampleLocationsEXT(VkCommandBuffer commandBuffer, const VkSampleLocationsInfoEXT *pSampleLocationsInfo)
{
    struct vkCmdSetSampleLocationsEXT_params params;
    params.commandBuffer = commandBuffer;
    params.pSampleLocationsInfo = pSampleLocationsInfo;
    UNIX_CALL(vkCmdSetSampleLocationsEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetSampleLocationsEnableEXT == 244 );
VK_FAST_THUNK( vkCmdSetSampleLocationsEnableEXT, "244", 2 )
#else
void WINAPI vkCmdSetSampleLocationsEnableEXT(VkCommandBuffer commandBuffer, VkBool32 sampleLocationsEnable)
{
    struct vkCmdSetSampleLocationsEnableEXT_params params;
    params.commandBuffer = commandBuffer;
    params.sampleLocationsEnable = sampleLocationsEnable;
    UNIX_CALL(vkCmdSetSampleLocationsEnableEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdSetSampleMaskEXT == 245 );
VK_FAST_THUNK( vkCmdSetSampleMaskEXT, "245", 3 )
#else
void WINAPI vkCmdSetSampleMaskEXT(VkCommandBuffer commandBuffer, VkSampleCountFlagBits samples, const VkSampleMask *pSampleMask)
{
    struct vkCmdSetSampleMaskEXT_params params;
    params.commandBuffer = commandBuffer;
    params.samples = samples;
    params.pSampleMask = pSampleMask;
    UNIX_CALL(vkCmdSetSampleMaskEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdSetScissor == 246 );
VK_FAST_THUNK( vkCmdSetScissor, "246", 4 )
#else
void WINAPI vkCmdSetScissor(VkCommandBuffer commandBuffer, uint32_t firstScissor, uint32_t scissorCount, const VkRect2D *pScissors)
{
    struct vkCmdSetScissor_params params;
    params.commandBuffer = commandBuffer;
    params.firstScissor = firstScissor;
    params.scissorCount = scissorCount;
    params.pScissors = pScissors;
    UNIX_CALL(vkCmdSetScissor, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdSetScissorWithCount == 247 );
VK_FAST_THUNK( vkCmdSetScissorWithCount, "247", 3 )
#else
void WINAPI vkCmdSetScissorWithCount(VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D *pScissors)
{
    struct vkCmdSetScissorWithCount_params params;
    params.commandBuffer = commandBuffer;
    params.scissorCount = scissorCount;
    params.pScissors = pScissors;
    UNIX_CALL(vkCmdSetScissorWithCount, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdSetScissorWithCountEXT == 248 );
VK_FAST_THUNK( vkCmdSetScissorWithCountEXT, "248", 3 )
#else
void WINAPI vkCmdSetScissorWithCountEXT(VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D *pScissors)
{
    struct vkCmdSetScissorWithCountEXT_params params;
    params.commandBuffer = commandBuffer;
    params.scissorCount = scissorCount;
    params.pScissors = pScissors;
    UNIX_CALL(vkCmdSetScissorWithCountEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetShadingRateImageEnableNV == 249 );
VK_FAST_THUNK( vkCmdSetShadingRateImageEnableNV, "249", 2 )
#else
void WINAPI vkCmdSetShadingRateImageEnableNV(VkCommandBuffer commandBuffer, VkBool32 shadingRateImageEnable)
{
    struct vkCmdSetShadingRateImageEnableNV_params params;
    params.commandBuffer = commandBuffer;
    params.shadingRateImageEnable = shadingRateImageEnable;
    UNIX_CALL(vkCmdSetShadingRateImageEnableNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdSetStencilCompareMask == 250 );
VK_FAST_THUNK( vkCmdSetStencilCompareMask, "250", 3 )
#else
void WINAPI vkCmdSetStencilCompareMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t compareMask)
{
    struct vkCmdSetStencilCompareMask_params params;
    params.commandBuffer = commandBuffer;
    params.faceMask = faceMask;
    params.compareMask = compareMask;
    UNIX_CALL(vkCmdSetStencilCompareMask, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 6
C_ASSERT( unix_vkCmdSetStencilOp == 251 );
VK_FAST_THUNK( vkCmdSetStencilOp, "251", 6 )
#else
void WINAPI vkCmdSetStencilOp(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp)
{
    struct vkCmdSetStencilOp_params params;
    params.commandBuffer = commandBuffer;
    params.faceMask = faceMask;
    params.failOp = failOp;
    params.passOp = passOp;
    params.depthFailOp = depthFailOp;
    params.compareOp = compareOp;
    UNIX_CALL(vkCmdSetStencilOp, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 6
C_ASSERT( unix_vkCmdSetStencilOpEXT == 252 );
VK_FAST_THUNK( vkCmdSetStencilOpEXT, "252", 6 )
#else
void WINAPI vkCmdSetStencilOpEXT(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp)
{
    struct vkCmdSetStencilOpEXT_params params;
    params.commandBuffer = commandBuffer;
    params.faceMask = faceMask;
    params.failOp = failOp;
    params.passOp = passOp;
    params.depthFailOp = depthFailOp;
    params.compareOp = compareOp;
    UNIX_CALL(vkCmdSetStencilOpEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdSetStencilReference == 253 );
VK_FAST_THUNK( vkCmdSetStencilReference, "253", 3 )
#else
void WINAPI vkCmdSetStencilReference(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t reference)
{
    struct vkCmdSetStencilReference_params params;
    params.commandBuffer = commandBuffer;
    params.faceMask = faceMask;
    params.reference = reference;
    UNIX_CALL(vkCmdSetStencilReference, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetStencilTestEnable == 254 );
VK_FAST_THUNK( vkCmdSetStencilTestEnable, "254", 2 )
#else
void WINAPI vkCmdSetStencilTestEnable(VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable)
{
    struct vkCmdSetStencilTestEnable_params params;
    params.commandBuffer = commandBuffer;
    params.stencilTestEnable = stencilTestEnable;
    UNIX_CALL(vkCmdSetStencilTestEnable, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetStencilTestEnableEXT == 255 );
VK_FAST_THUNK( vkCmdSetStencilTestEnableEXT, "255", 2 )
#else
void WINAPI vkCmdSetStencilTestEnableEXT(VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable)
{
    struct vkCmdSetStencilTestEnableEXT_params params;
    params.commandBuffer = commandBuffer;
    params.stencilTestEnable = stencilTestEnable;
    UNIX_CALL(vkCmdSetStencilTestEnableEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdSetStencilWriteMask == 256 );
VK_FAST_THUNK( vkCmdSetStencilWriteMask, "256", 3 )
#else
void WINAPI vkCmdSetStencilWriteMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t writeMask)
{
    struct vkCmdSetStencilWriteMask_params params;
    params.commandBuffer = commandBuffer;
    params.faceMask = faceMask;
    params.writeMask = writeMask;
    UNIX_CALL(vkCmdSetStencilWriteMask, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetTessellationDomainOriginEXT == 257 );
VK_FAST_THUNK( vkCmdSetTessellationDomainOriginEXT, "257", 2 )
#else
void WINAPI vkCmdSetTessellationDomainOriginEXT(VkCommandBuffer commandBuffer, VkTessellationDomainOrigin domainOrigin)
{
    struct vkCmdSetTessellationDomainOriginEXT_params params;
    params.commandBuffer = commandBuffer;
    params.domainOrigin = domainOrigin;
    UNIX_CALL(vkCmdSetTessellationDomainOriginEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 5
C_ASSERT( unix_vkCmdSetVertexInputEXT == 258 );
VK_FAST_THUNK( vkCmdSetVertexInputEXT, "258", 5 )
#else
void WINAPI vkCmdSetVertexInputEXT(VkCommandBuffer commandBuffer, uint32_t vertexBindingDescriptionCount, const VkVertexInputBindingDescription2EXT *pVertexBindingDescriptions, uint32_t vertexAttributeDescriptionCount, const VkVertexInputAttributeDescription2EXT *pVertexAttributeDescriptions)
{
    struct vkCmdSetVertexInputEXT_params params;
    params.commandBuffer = commandBuffer;
    params.vertexBindingDescriptionCount = vertexBindingDescriptionCount;
    params.pVertexBindingDescriptions = pVertexBindingDescriptions;
    params.vertexAttributeDescriptionCount = vertexAttributeDescriptionCount;
    params.pVertexAttributeDescriptions = pVertexAttributeDescriptions;
    UNIX_CALL(vkCmdSetVertexInputEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdSetViewport == 259 );
VK_FAST_THUNK( vkCmdSetViewport, "259", 4 )
#else
void WINAPI vkCmdSetViewport(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewport *pViewports)
{
    struct vkCmdSetViewport_params params;
    params.commandBuffer = commandBuffer;
    params.firstViewport = firstViewport;
    params.viewportCount = viewportCount;
    params.pViewports = pViewports;
    UNIX_CALL(vkCmdSetViewport, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdSetViewportShadingRatePaletteNV == 260 );
VK_FAST_THUNK( vkCmdSetViewportShadingRatePaletteNV, "260", 4 )
#else
void WINAPI vkCmdSetViewportShadingRatePaletteNV(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkShadingRatePaletteNV *pShadingRatePalettes)
{
    struct vkCmdSetViewportShadingRatePaletteNV_params params;
    params.commandBuffer = commandBuffer;
    params.firstViewport = firstViewport;
    params.viewportCount = viewportCount;
    params.pShadingRatePalettes = pShadingRatePalettes;
    UNIX_CALL(vkCmdSetViewportShadingRatePaletteNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdSetViewportSwizzleNV == 261 );
VK_FAST_THUNK( vkCmdSetViewportSwizzleNV, "261", 4 )
#else
void WINAPI vkCmdSetViewportSwizzleNV(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewportSwizzleNV *pViewportSwizzles)
{
    struct vkCmdSetViewportSwizzleNV_params params;
    params.commandBuffer = commandBuffer;
    params.firstViewport = firstViewport;
    params.viewportCount = viewportCount;
    params.pViewportSwizzles = pViewportSwizzles;
    UNIX_CALL(vkCmdSetViewportSwizzleNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdSetViewportWScalingEnableNV == 262 );
VK_FAST_THUNK( vkCmdSetViewportWScalingEnableNV, "262", 2 )
#else
void WINAPI vkCmdSetViewportWScalingEnableNV(VkCommandBuffer commandBuffer, VkBool32 viewportWScalingEnable)
{
    struct vkCmdSetViewportWScalingEnableNV_params params;
    params.commandBuffer = commandBuffer;
    params.viewportWScalingEnable = viewportWScalingEnable;
    UNIX_CALL(vkCmdSetViewportWScalingEnableNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdSetViewportWScalingNV == 263 );
VK_FAST_THUNK( vkCmdSetViewportWScalingNV, "263", 4 )
#else
void WINAPI vkCmdSetViewportWScalingNV(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewportWScalingNV *pViewportWScalings)
{
    struct vkCmdSetViewportWScalingNV_params params;
    params.commandBuffer = commandBuffer;
    params.firstViewport = firstViewport;
    params.viewportCount = viewportCount;
    params.pViewportWScalings = pViewportWScalings;
    UNIX_CALL(vkCmdSetViewportWScalingNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdSetViewportWithCount == 264 );
VK_FAST_THUNK( vkCmdSetViewportWithCount, "264", 3 )
#else
void WINAPI vkCmdSetViewportWithCount(VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport *pViewports)
{
    struct vkCmdSetViewportWithCount_params params;
    params.commandBuffer = commandBuffer;
    params.viewportCount = viewportCount;
    params.pViewports = pViewports;
    UNIX_CALL(vkCmdSetViewportWithCount, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdSetViewportWithCountEXT == 265 );
VK_FAST_THUNK( vkCmdSetViewportWithCountEXT, "265", 3 )
#else
void WINAPI vkCmdSetViewportWithCountEXT(VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport *pViewports)
{
    struct vkCmdSetViewportWithCountEXT_params params;
    params.commandBuffer = commandBuffer;
    params.viewportCount = viewportCount;
    params.pViewports = pViewports;
    UNIX_CALL(vkCmdSetViewportWithCountEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 1
C_ASSERT( unix_vkCmdSubpassShadingHUAWEI == 266 );
VK_FAST_THUNK( vkCmdSubpassShadingHUAWEI, "266", 1 )
#else
void WINAPI vkCmdSubpassShadingHUAWEI(VkCommandBuffer commandBuffer)
{
    struct vkCmdSubpassShadingHUAWEI_params params;
    params.commandBuffer = commandBuffer;
    UNIX_CALL(vkCmdSubpassShadingHUAWEI, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 2
C_ASSERT( unix_vkCmdTraceRaysIndirect2KHR == 267 );
VK_FAST_THUNK( vkCmdTraceRaysIndirect2KHR, "267", 2 )
#else
void WINAPI vkCmdTraceRaysIndirect2KHR(VkCommandBuffer commandBuffer, VkDeviceAddress indirectDeviceAddress)
{
    struct vkCmdTraceRaysIndirect2KHR_params params;
    params.commandBuffer = commandBuffer;
    params.indirectDeviceAddress = indirectDeviceAddress;
    UNIX_CALL(vkCmdTraceRaysIndirect2KHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 6
C_ASSERT( unix_vkCmdTraceRaysIndirectKHR == 268 );
VK_FAST_THUNK( vkCmdTraceRaysIndirectKHR, "268", 6 )
#else
void WINAPI vkCmdTraceRaysIndirectKHR(VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR *pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR *pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR *pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR *pCallableShaderBindingTable, VkDeviceAddress indirectDeviceAddress)
{
    struct vkCmdTraceRaysIndirectKHR_params params;
    params.commandBuffer = commandBuffer;
    params.pRaygenShaderBindingTable = pRaygenShaderBindingTable;
    params.pMissShaderBindingTable = pMissShaderBindingTable;
    params.pHitShaderBindingTable = pHitShaderBindingTable;
    params.pCallableShaderBindingTable = pCallableShaderBindingTable;
    params.indirectDeviceAddress = indirectDeviceAddress;
    UNIX_CALL(vkCmdTraceRaysIndirectKHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 8
C_ASSERT( unix_vkCmdTraceRaysKHR == 269 );
VK_FAST_THUNK( vkCmdTraceRaysKHR, "269", 8 )
#else
void WINAPI vkCmdTraceRaysKHR(VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR *pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR *pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR *pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR *pCallableShaderBindingTable, uint32_t width, uint32_t height, uint32_t depth)
{
    struct vkCmdTraceRaysKHR_params params;
    params.commandBuffer = commandBuffer;
    params.pRaygenShaderBindingTable = pRaygenShaderBindingTable;
    params.pMissShaderBindingTable = pMissShaderBindingTable;
    params.pHitShaderBindingTable = pHitShaderBindingTable;
    params.pCallableShaderBindingTable = pCallableShaderBindingTable;
    params.width = width;
    params.height = height;
    params.depth = depth;
    UNIX_CALL(vkCmdTraceRaysKHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 15
C_ASSERT( unix_vkCmdTraceRaysNV == 270 );
VK_FAST_THUNK( vkCmdTraceRaysNV, "270", 15 )
#else
void WINAPI vkCmdTraceRaysNV(VkCommandBuffer commandBuffer, VkBuffer raygenShaderBindingTableBuffer, VkDeviceSize raygenShaderBindingOffset, VkBuffer missShaderBindingTableBuffer, VkDeviceSize missShaderBindingOffset, VkDeviceSize missShaderBindingStride, VkBuffer hitShaderBindingTableBuffer, VkDeviceSize hitShaderBindingOffset, VkDeviceSize hitShaderBindingStride, VkBuffer callableShaderBindingTableBuffer, VkDeviceSize callableShaderBindingOffset, VkDeviceSize callableShaderBindingStride, uint32_t width, uint32_t height, uint32_t depth)
{
    struct vkCmdTraceRaysNV_params params;
    params.commandBuffer = commandBuffer;
    params.raygenShaderBindingTableBuffer = raygenShaderBindingTableBuffer;
    params.raygenShaderBindingOffset = raygenShaderBindingOffset;
    params.missShaderBindingTableBuffer = missShaderBindingTableBuffer;
    params.missShaderBindingOffset = missShaderBindingOffset;
    params.missShaderBindingStride = missShaderBindingStride;
    params.hitShaderBindingTableBuffer = hitShaderBindingTableBuffer;
    params.hitShaderBindingOffset = hitShaderBindingOffset;
    params.hitShaderBindingStride = hitShaderBindingStride;
    params.callableShaderBindingTableBuffer = callableShaderBindingTableBuffer;
    params.callableShaderBindingOffset = callableShaderBindingOffset;
    params.callableShaderBindingStride = callableShaderBindingStride;
    params.width = width;
    params.height = height;
    params.depth = depth;
    UNIX_CALL(vkCmdTraceRaysNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 5
C_ASSERT( unix_vkCmdUpdateBuffer == 271 );
VK_FAST_THUNK( vkCmdUpdateBuffer, "271", 5 )
#else
void WINAPI vkCmdUpdateBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize, const void *pData)
{
    struct vkCmdUpdateBuffer_params params;
    params.commandBuffer = commandBuffer;
    params.dstBuffer = dstBuffer;
    params.dstOffset = dstOffset;
    params.dataSize = dataSize;
    params.pData = pData;
    UNIX_CALL(vkCmdUpdateBuffer, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 3
C_ASSERT( unix_vkCmdUpdatePipelineIndirectBufferNV == 272 );
VK_FAST_THUNK( vkCmdUpdatePipelineIndirectBufferNV, "272", 3 )
#else
void WINAPI vkCmdUpdatePipelineIndirectBufferNV(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline)
{
    struct vkCmdUpdatePipelineIndirectBufferNV_params params;
    params.commandBuffer = commandBuffer;
    params.pipelineBindPoint = pipelineBindPoint;
    params.pipeline = pipeline;
    UNIX_CALL(vkCmdUpdatePipelineIndirectBufferNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 11
C_ASSERT( unix_vkCmdWaitEvents == 273 );
VK_FAST_THUNK( vkCmdWaitEvents, "273", 11 )
#else
void WINAPI vkCmdWaitEvents(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent *pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, uint32_t memoryBarrierCount, const VkMemoryBarrier *pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier *pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier *pImageMemoryBarriers)
{
    struct vkCmdWaitEvents_params params;
    params.commandBuffer = commandBuffer;
    params.eventCount = eventCount;
    params.pEvents = pEvents;
    params.srcStageMask = srcStageMask;
    params.dstStageMask = dstStageMask;
    params.memoryBarrierCount = memoryBarrierCount;
    params.pMemoryBarriers = pMemoryBarriers;
    params.bufferMemoryBarrierCount = bufferMemoryBarrierCount;
    params.pBufferMemoryBarriers = pBufferMemoryBarriers;
    params.imageMemoryBarrierCount = imageMemoryBarrierCount;
    params.pImageMemoryBarriers = pImageMemoryBarriers;
    UNIX_CALL(vkCmdWaitEvents, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdWaitEvents2 == 274 );
VK_FAST_THUNK( vkCmdWaitEvents2, "274", 4 )
#else
void WINAPI vkCmdWaitEvents2(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent *pEvents, const VkDependencyInfo *pDependencyInfos)
{
    struct vkCmdWaitEvents2_params params;
    params.commandBuffer = commandBuffer;
    params.eventCount = eventCount;
    params.pEvents = pEvents;
    params.pDependencyInfos = pDependencyInfos;
    UNIX_CALL(vkCmdWaitEvents2, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdWaitEvents2KHR == 275 );
VK_FAST_THUNK( vkCmdWaitEvents2KHR, "275", 4 )
#else
void WINAPI vkCmdWaitEvents2KHR(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent *pEvents, const VkDependencyInfo *pDependencyInfos)
{
    struct vkCmdWaitEvents2KHR_params params;
    params.commandBuffer = commandBuffer;
    params.eventCount = eventCount;
    params.pEvents = pEvents;
    params.pDependencyInfos = pDependencyInfos;
    UNIX_CALL(vkCmdWaitEvents2KHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 6
C_ASSERT( unix_vkCmdWriteAccelerationStructuresPropertiesKHR == 276 );
VK_FAST_THUNK( vkCmdWriteAccelerationStructuresPropertiesKHR, "276", 6 )
#else
void WINAPI vkCmdWriteAccelerationStructuresPropertiesKHR(VkCommandBuffer commandBuffer, uint32_t accelerationStructureCount, const VkAccelerationStructureKHR *pAccelerationStructures, VkQueryType queryType, VkQueryPool queryPool, uint32_t firstQuery)
{
    struct vkCmdWriteAccelerationStructuresPropertiesKHR_params params;
    params.commandBuffer = commandBuffer;
    params.accelerationStructureCount = accelerationStructureCount;
    params.pAccelerationStructures = pAccelerationStructures;
    params.queryType = queryType;
    params.queryPool = queryPool;
    params.firstQuery = firstQuery;
    UNIX_CALL(vkCmdWriteAccelerationStructuresPropertiesKHR, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 6
C_ASSERT( unix_vkCmdWriteAccelerationStructuresPropertiesNV == 277 );
VK_FAST_THUNK( vkCmdWriteAccelerationStructuresPropertiesNV, "277", 6 )
#else
void WINAPI vkCmdWriteAccelerationStructuresPropertiesNV(VkCommandBuffer commandBuffer, uint32_t accelerationStructureCount, const VkAccelerationStructureNV *pAccelerationStructures, VkQueryType queryType, VkQueryPool queryPool, uint32_t firstQuery)
{
    struct vkCmdWriteAccelerationStructuresPropertiesNV_params params;
    params.commandBuffer = commandBuffer;
    params.accelerationStructureCount = accelerationStructureCount;
    params.pAccelerationStructures = pAccelerationStructures;
    params.queryType = queryType;
    params.queryPool = queryPool;
    params.firstQuery = firstQuery;
    UNIX_CALL(vkCmdWriteAccelerationStructuresPropertiesNV, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 5
C_ASSERT( unix_vkCmdWriteBufferMarker2AMD == 278 );
VK_FAST_THUNK( vkCmdWriteBufferMarker2AMD, "278", 5 )
#else
void WINAPI vkCmdWriteBufferMarker2AMD(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkBuffer dstBuffer, VkDeviceSize dstOffset, uint32_t marker)
{
    struct vkCmdWriteBufferMarker2AMD_params params;
    params.commandBuffer = commandBuffer;
    params.stage = stage;
    params.dstBuffer = dstBuffer;
    params.dstOffset = dstOffset;
    params.marker = marker;
    UNIX_CALL(vkCmdWriteBufferMarker2AMD, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 5
C_ASSERT( unix_vkCmdWriteBufferMarkerAMD == 279 );
VK_FAST_THUNK( vkCmdWriteBufferMarkerAMD, "279", 5 )
#else
void WINAPI vkCmdWriteBufferMarkerAMD(VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkBuffer dstBuffer, VkDeviceSize dstOffset, uint32_t marker)
{
    struct vkCmdWriteBufferMarkerAMD_params params;
    params.commandBuffer = commandBuffer;
    params.pipelineStage = pipelineStage;
    params.dstBuffer = dstBuffer;
    params.dstOffset = dstOffset;
    params.marker = marker;
    UNIX_CALL(vkCmdWriteBufferMarkerAMD, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 6
C_ASSERT( unix_vkCmdWriteMicromapsPropertiesEXT == 280 );
VK_FAST_THUNK( vkCmdWriteMicromapsPropertiesEXT, "280", 6 )
#else
void WINAPI vkCmdWriteMicromapsPropertiesEXT(VkCommandBuffer commandBuffer, uint32_t micromapCount, const VkMicromapEXT *pMicromaps, VkQueryType queryType, VkQueryPool queryPool, uint32_t firstQuery)
{
    struct vkCmdWriteMicromapsPropertiesEXT_params params;
    params.commandBuffer = commandBuffer;
    params.micromapCount = micromapCount;
    params.pMicromaps = pMicromaps;
    params.queryType = queryType;
    params.queryPool = queryPool;
    params.firstQuery = firstQuery;
    UNIX_CALL(vkCmdWriteMicromapsPropertiesEXT, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdWriteTimestamp == 281 );
VK_FAST_THUNK( vkCmdWriteTimestamp, "281", 4 )
#else
void WINAPI vkCmdWriteTimestamp(VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t query)
{
    struct vkCmdWriteTimestamp_params params;
    params.commandBuffer = commandBuffer;
    params.pipelineStage = pipelineStage;
    params.queryPool = queryPool;
    params.query = query;
    UNIX_CALL(vkCmdWriteTimestamp, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdWriteTimestamp2 == 282 );
VK_FAST_THUNK( vkCmdWriteTimestamp2, "282", 4 )
#else
void WINAPI vkCmdWriteTimestamp2(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkQueryPool queryPool, uint32_t query)
{
    struct vkCmdWriteTimestamp2_params params;
    params.commandBuffer = commandBuffer;
    params.stage = stage;
    params.queryPool = queryPool;
    params.query = query;
    UNIX_CALL(vkCmdWriteTimestamp2, &params);
}
#endif /* VK_FAST_THUNK */

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkCmdWriteTimestamp2KHR == 283 );
VK_FAST_THUNK( vkCmdWriteTimestamp2KHR, "283", 4 )
#else
void WINAPI vkCmdWriteTimestamp2KHR(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkQueryPool queryPool, uint32_t query)
{
    struct vkCmdWriteTimestamp2KHR_params params;
    params.commandBuffer = commandBuffer;
    params.stage = stage;
    params.queryPool = queryPool;
    params.query = query;
    UNIX_CALL(vkCmdWriteTimestamp2KHR, &params);
}
#endif /* VK_FAST_THUNK */

VkResult WINAPI vkCompileDeferredNV(VkDevice device, VkPipeline pipeline, uint32_t shader)
{
    struct vkCompileDeferredNV_params params;
    NTSTATUS status;
    params.device = device;
    params.pipeline = pipeline;
    params.shader = shader;
    status = UNIX_CALL(vkCompileDeferredNV, &params);
    assert(!status && "vkCompileDeferredNV");
    return params.result;
}

VkResult WINAPI vkConvertCooperativeVectorMatrixNV(VkDevice device, const VkConvertCooperativeVectorMatrixInfoNV *pInfo)
{
    struct vkConvertCooperativeVectorMatrixNV_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkConvertCooperativeVectorMatrixNV, &params);
    assert(!status && "vkConvertCooperativeVectorMatrixNV");
    return params.result;
}

VkResult WINAPI vkCopyAccelerationStructureKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyAccelerationStructureInfoKHR *pInfo)
{
    struct vkCopyAccelerationStructureKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.deferredOperation = deferredOperation;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkCopyAccelerationStructureKHR, &params);
    assert(!status && "vkCopyAccelerationStructureKHR");
    return params.result;
}

VkResult WINAPI vkCopyAccelerationStructureToMemoryKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyAccelerationStructureToMemoryInfoKHR *pInfo)
{
    struct vkCopyAccelerationStructureToMemoryKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.deferredOperation = deferredOperation;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkCopyAccelerationStructureToMemoryKHR, &params);
    assert(!status && "vkCopyAccelerationStructureToMemoryKHR");
    return params.result;
}

VkResult WINAPI vkCopyImageToImage(VkDevice device, const VkCopyImageToImageInfo *pCopyImageToImageInfo)
{
    struct vkCopyImageToImage_params params;
    NTSTATUS status;
    params.device = device;
    params.pCopyImageToImageInfo = pCopyImageToImageInfo;
    status = UNIX_CALL(vkCopyImageToImage, &params);
    assert(!status && "vkCopyImageToImage");
    return params.result;
}

VkResult WINAPI vkCopyImageToImageEXT(VkDevice device, const VkCopyImageToImageInfo *pCopyImageToImageInfo)
{
    struct vkCopyImageToImageEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pCopyImageToImageInfo = pCopyImageToImageInfo;
    status = UNIX_CALL(vkCopyImageToImageEXT, &params);
    assert(!status && "vkCopyImageToImageEXT");
    return params.result;
}

VkResult WINAPI vkCopyImageToMemory(VkDevice device, const VkCopyImageToMemoryInfo *pCopyImageToMemoryInfo)
{
    struct vkCopyImageToMemory_params params;
    NTSTATUS status;
    params.device = device;
    params.pCopyImageToMemoryInfo = pCopyImageToMemoryInfo;
    status = UNIX_CALL(vkCopyImageToMemory, &params);
    assert(!status && "vkCopyImageToMemory");
    return params.result;
}

VkResult WINAPI vkCopyImageToMemoryEXT(VkDevice device, const VkCopyImageToMemoryInfo *pCopyImageToMemoryInfo)
{
    struct vkCopyImageToMemoryEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pCopyImageToMemoryInfo = pCopyImageToMemoryInfo;
    status = UNIX_CALL(vkCopyImageToMemoryEXT, &params);
    assert(!status && "vkCopyImageToMemoryEXT");
    return params.result;
}

VkResult WINAPI vkCopyMemoryToAccelerationStructureKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyMemoryToAccelerationStructureInfoKHR *pInfo)
{
    struct vkCopyMemoryToAccelerationStructureKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.deferredOperation = deferredOperation;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkCopyMemoryToAccelerationStructureKHR, &params);
    assert(!status && "vkCopyMemoryToAccelerationStructureKHR");
    return params.result;
}

VkResult WINAPI vkCopyMemoryToImage(VkDevice device, const VkCopyMemoryToImageInfo *pCopyMemoryToImageInfo)
{
    struct vkCopyMemoryToImage_params params;
    NTSTATUS status;
    params.device = device;
    params.pCopyMemoryToImageInfo = pCopyMemoryToImageInfo;
    status = UNIX_CALL(vkCopyMemoryToImage, &params);
    assert(!status && "vkCopyMemoryToImage");
    return params.result;
}

VkResult WINAPI vkCopyMemoryToImageEXT(VkDevice device, const VkCopyMemoryToImageInfo *pCopyMemoryToImageInfo)
{
    struct vkCopyMemoryToImageEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pCopyMemoryToImageInfo = pCopyMemoryToImageInfo;
    status = UNIX_CALL(vkCopyMemoryToImageEXT, &params);
    assert(!status && "vkCopyMemoryToImageEXT");
    return params.result;
}

VkResult WINAPI vkCopyMemoryToMicromapEXT(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyMemoryToMicromapInfoEXT *pInfo)
{
    struct vkCopyMemoryToMicromapEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.deferredOperation = deferredOperation;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkCopyMemoryToMicromapEXT, &params);
    assert(!status && "vkCopyMemoryToMicromapEXT");
    return params.result;
}

VkResult WINAPI vkCopyMicromapEXT(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyMicromapInfoEXT *pInfo)
{
    struct vkCopyMicromapEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.deferredOperation = deferredOperation;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkCopyMicromapEXT, &params);
    assert(!status && "vkCopyMicromapEXT");
    return params.result;
}

VkResult WINAPI vkCopyMicromapToMemoryEXT(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyMicromapToMemoryInfoEXT *pInfo)
{
    struct vkCopyMicromapToMemoryEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.deferredOperation = deferredOperation;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkCopyMicromapToMemoryEXT, &params);
    assert(!status && "vkCopyMicromapToMemoryEXT");
    return params.result;
}

VkResult WINAPI vkCreateAccelerationStructureKHR(VkDevice device, const VkAccelerationStructureCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkAccelerationStructureKHR *pAccelerationStructure)
{
    struct vkCreateAccelerationStructureKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pAccelerationStructure = pAccelerationStructure;
    status = UNIX_CALL(vkCreateAccelerationStructureKHR, &params);
    assert(!status && "vkCreateAccelerationStructureKHR");
    return params.result;
}

VkResult WINAPI vkCreateAccelerationStructureNV(VkDevice device, const VkAccelerationStructureCreateInfoNV *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkAccelerationStructureNV *pAccelerationStructure)
{
    struct vkCreateAccelerationStructureNV_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pAccelerationStructure = pAccelerationStructure;
    status = UNIX_CALL(vkCreateAccelerationStructureNV, &params);
    assert(!status && "vkCreateAccelerationStructureNV");
    return params.result;
}

VkResult WINAPI vkCreateBuffer(VkDevice device, const VkBufferCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkBuffer *pBuffer)
{
    struct vkCreateBuffer_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pBuffer = pBuffer;
    status = UNIX_CALL(vkCreateBuffer, &params);
    assert(!status && "vkCreateBuffer");
    return params.result;
}

VkResult WINAPI vkCreateBufferView(VkDevice device, const VkBufferViewCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkBufferView *pView)
{
    struct vkCreateBufferView_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pView = pView;
    status = UNIX_CALL(vkCreateBufferView, &params);
    assert(!status && "vkCreateBufferView");
    return params.result;
}

VkResult WINAPI vkCreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkComputePipelineCreateInfo *pCreateInfos, const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines)
{
    struct vkCreateComputePipelines_params params;
    NTSTATUS status;
    params.device = device;
    params.pipelineCache = pipelineCache;
    params.createInfoCount = createInfoCount;
    params.pCreateInfos = pCreateInfos;
    params.pAllocator = pAllocator;
    params.pPipelines = pPipelines;
    status = UNIX_CALL(vkCreateComputePipelines, &params);
    assert(!status && "vkCreateComputePipelines");
    return params.result;
}

VkResult WINAPI vkCreateCuFunctionNVX(VkDevice device, const VkCuFunctionCreateInfoNVX *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkCuFunctionNVX *pFunction)
{
    struct vkCreateCuFunctionNVX_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pFunction = pFunction;
    status = UNIX_CALL(vkCreateCuFunctionNVX, &params);
    assert(!status && "vkCreateCuFunctionNVX");
    return params.result;
}

VkResult WINAPI vkCreateCuModuleNVX(VkDevice device, const VkCuModuleCreateInfoNVX *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkCuModuleNVX *pModule)
{
    struct vkCreateCuModuleNVX_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pModule = pModule;
    status = UNIX_CALL(vkCreateCuModuleNVX, &params);
    assert(!status && "vkCreateCuModuleNVX");
    return params.result;
}

VkResult WINAPI vkCreateCudaFunctionNV(VkDevice device, const VkCudaFunctionCreateInfoNV *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkCudaFunctionNV *pFunction)
{
    struct vkCreateCudaFunctionNV_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pFunction = pFunction;
    status = UNIX_CALL(vkCreateCudaFunctionNV, &params);
    assert(!status && "vkCreateCudaFunctionNV");
    return params.result;
}

VkResult WINAPI vkCreateCudaModuleNV(VkDevice device, const VkCudaModuleCreateInfoNV *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkCudaModuleNV *pModule)
{
    struct vkCreateCudaModuleNV_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pModule = pModule;
    status = UNIX_CALL(vkCreateCudaModuleNV, &params);
    assert(!status && "vkCreateCudaModuleNV");
    return params.result;
}

VkResult WINAPI vkCreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugReportCallbackEXT *pCallback)
{
    struct vkCreateDebugReportCallbackEXT_params params;
    NTSTATUS status;
    params.instance = instance;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pCallback = pCallback;
    status = UNIX_CALL(vkCreateDebugReportCallbackEXT, &params);
    assert(!status && "vkCreateDebugReportCallbackEXT");
    return params.result;
}

VkResult WINAPI vkCreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pMessenger)
{
    struct vkCreateDebugUtilsMessengerEXT_params params;
    NTSTATUS status;
    params.instance = instance;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pMessenger = pMessenger;
    status = UNIX_CALL(vkCreateDebugUtilsMessengerEXT, &params);
    assert(!status && "vkCreateDebugUtilsMessengerEXT");
    return params.result;
}

VkResult WINAPI vkCreateDeferredOperationKHR(VkDevice device, const VkAllocationCallbacks *pAllocator, VkDeferredOperationKHR *pDeferredOperation)
{
    struct vkCreateDeferredOperationKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pAllocator = pAllocator;
    params.pDeferredOperation = pDeferredOperation;
    status = UNIX_CALL(vkCreateDeferredOperationKHR, &params);
    assert(!status && "vkCreateDeferredOperationKHR");
    return params.result;
}

VkResult WINAPI vkCreateDescriptorPool(VkDevice device, const VkDescriptorPoolCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDescriptorPool *pDescriptorPool)
{
    struct vkCreateDescriptorPool_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pDescriptorPool = pDescriptorPool;
    status = UNIX_CALL(vkCreateDescriptorPool, &params);
    assert(!status && "vkCreateDescriptorPool");
    return params.result;
}

VkResult WINAPI vkCreateDescriptorSetLayout(VkDevice device, const VkDescriptorSetLayoutCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDescriptorSetLayout *pSetLayout)
{
    struct vkCreateDescriptorSetLayout_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pSetLayout = pSetLayout;
    status = UNIX_CALL(vkCreateDescriptorSetLayout, &params);
    assert(!status && "vkCreateDescriptorSetLayout");
    return params.result;
}

VkResult WINAPI vkCreateDescriptorUpdateTemplate(VkDevice device, const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDescriptorUpdateTemplate *pDescriptorUpdateTemplate)
{
    struct vkCreateDescriptorUpdateTemplate_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pDescriptorUpdateTemplate = pDescriptorUpdateTemplate;
    status = UNIX_CALL(vkCreateDescriptorUpdateTemplate, &params);
    assert(!status && "vkCreateDescriptorUpdateTemplate");
    return params.result;
}

VkResult WINAPI vkCreateDescriptorUpdateTemplateKHR(VkDevice device, const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDescriptorUpdateTemplate *pDescriptorUpdateTemplate)
{
    struct vkCreateDescriptorUpdateTemplateKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pDescriptorUpdateTemplate = pDescriptorUpdateTemplate;
    status = UNIX_CALL(vkCreateDescriptorUpdateTemplateKHR, &params);
    assert(!status && "vkCreateDescriptorUpdateTemplateKHR");
    return params.result;
}

VkResult WINAPI vkCreateEvent(VkDevice device, const VkEventCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkEvent *pEvent)
{
    struct vkCreateEvent_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pEvent = pEvent;
    status = UNIX_CALL(vkCreateEvent, &params);
    assert(!status && "vkCreateEvent");
    return params.result;
}

VkResult WINAPI vkCreateFence(VkDevice device, const VkFenceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkFence *pFence)
{
    struct vkCreateFence_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pFence = pFence;
    status = UNIX_CALL(vkCreateFence, &params);
    assert(!status && "vkCreateFence");
    return params.result;
}

VkResult WINAPI vkCreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkFramebuffer *pFramebuffer)
{
    struct vkCreateFramebuffer_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pFramebuffer = pFramebuffer;
    status = UNIX_CALL(vkCreateFramebuffer, &params);
    assert(!status && "vkCreateFramebuffer");
    return params.result;
}

VkResult WINAPI vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo *pCreateInfos, const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines)
{
    struct vkCreateGraphicsPipelines_params params;
    NTSTATUS status;
    params.device = device;
    params.pipelineCache = pipelineCache;
    params.createInfoCount = createInfoCount;
    params.pCreateInfos = pCreateInfos;
    params.pAllocator = pAllocator;
    params.pPipelines = pPipelines;
    status = UNIX_CALL(vkCreateGraphicsPipelines, &params);
    assert(!status && "vkCreateGraphicsPipelines");
    return params.result;
}

VkResult WINAPI vkCreateImage(VkDevice device, const VkImageCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkImage *pImage)
{
    struct vkCreateImage_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pImage = pImage;
    status = UNIX_CALL(vkCreateImage, &params);
    assert(!status && "vkCreateImage");
    return params.result;
}

VkResult WINAPI vkCreateImageView(VkDevice device, const VkImageViewCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkImageView *pView)
{
    struct vkCreateImageView_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pView = pView;
    status = UNIX_CALL(vkCreateImageView, &params);
    assert(!status && "vkCreateImageView");
    return params.result;
}

VkResult WINAPI vkCreateIndirectCommandsLayoutEXT(VkDevice device, const VkIndirectCommandsLayoutCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkIndirectCommandsLayoutEXT *pIndirectCommandsLayout)
{
    struct vkCreateIndirectCommandsLayoutEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pIndirectCommandsLayout = pIndirectCommandsLayout;
    status = UNIX_CALL(vkCreateIndirectCommandsLayoutEXT, &params);
    assert(!status && "vkCreateIndirectCommandsLayoutEXT");
    return params.result;
}

VkResult WINAPI vkCreateIndirectCommandsLayoutNV(VkDevice device, const VkIndirectCommandsLayoutCreateInfoNV *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkIndirectCommandsLayoutNV *pIndirectCommandsLayout)
{
    struct vkCreateIndirectCommandsLayoutNV_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pIndirectCommandsLayout = pIndirectCommandsLayout;
    status = UNIX_CALL(vkCreateIndirectCommandsLayoutNV, &params);
    assert(!status && "vkCreateIndirectCommandsLayoutNV");
    return params.result;
}

VkResult WINAPI vkCreateIndirectExecutionSetEXT(VkDevice device, const VkIndirectExecutionSetCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkIndirectExecutionSetEXT *pIndirectExecutionSet)
{
    struct vkCreateIndirectExecutionSetEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pIndirectExecutionSet = pIndirectExecutionSet;
    status = UNIX_CALL(vkCreateIndirectExecutionSetEXT, &params);
    assert(!status && "vkCreateIndirectExecutionSetEXT");
    return params.result;
}

VkResult WINAPI vkCreateMicromapEXT(VkDevice device, const VkMicromapCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkMicromapEXT *pMicromap)
{
    struct vkCreateMicromapEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pMicromap = pMicromap;
    status = UNIX_CALL(vkCreateMicromapEXT, &params);
    assert(!status && "vkCreateMicromapEXT");
    return params.result;
}

VkResult WINAPI vkCreateOpticalFlowSessionNV(VkDevice device, const VkOpticalFlowSessionCreateInfoNV *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkOpticalFlowSessionNV *pSession)
{
    struct vkCreateOpticalFlowSessionNV_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pSession = pSession;
    status = UNIX_CALL(vkCreateOpticalFlowSessionNV, &params);
    assert(!status && "vkCreateOpticalFlowSessionNV");
    return params.result;
}

VkResult WINAPI vkCreatePipelineBinariesKHR(VkDevice device, const VkPipelineBinaryCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkPipelineBinaryHandlesInfoKHR *pBinaries)
{
    struct vkCreatePipelineBinariesKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pBinaries = pBinaries;
    status = UNIX_CALL(vkCreatePipelineBinariesKHR, &params);
    assert(!status && "vkCreatePipelineBinariesKHR");
    return params.result;
}

VkResult WINAPI vkCreatePipelineCache(VkDevice device, const VkPipelineCacheCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkPipelineCache *pPipelineCache)
{
    struct vkCreatePipelineCache_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pPipelineCache = pPipelineCache;
    status = UNIX_CALL(vkCreatePipelineCache, &params);
    assert(!status && "vkCreatePipelineCache");
    return params.result;
}

VkResult WINAPI vkCreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkPipelineLayout *pPipelineLayout)
{
    struct vkCreatePipelineLayout_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pPipelineLayout = pPipelineLayout;
    status = UNIX_CALL(vkCreatePipelineLayout, &params);
    assert(!status && "vkCreatePipelineLayout");
    return params.result;
}

VkResult WINAPI vkCreatePrivateDataSlot(VkDevice device, const VkPrivateDataSlotCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkPrivateDataSlot *pPrivateDataSlot)
{
    struct vkCreatePrivateDataSlot_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pPrivateDataSlot = pPrivateDataSlot;
    status = UNIX_CALL(vkCreatePrivateDataSlot, &params);
    assert(!status && "vkCreatePrivateDataSlot");
    return params.result;
}

VkResult WINAPI vkCreatePrivateDataSlotEXT(VkDevice device, const VkPrivateDataSlotCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkPrivateDataSlot *pPrivateDataSlot)
{
    struct vkCreatePrivateDataSlotEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pPrivateDataSlot = pPrivateDataSlot;
    status = UNIX_CALL(vkCreatePrivateDataSlotEXT, &params);
    assert(!status && "vkCreatePrivateDataSlotEXT");
    return params.result;
}

VkResult WINAPI vkCreateQueryPool(VkDevice device, const VkQueryPoolCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkQueryPool *pQueryPool)
{
    struct vkCreateQueryPool_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pQueryPool = pQueryPool;
    status = UNIX_CALL(vkCreateQueryPool, &params);
    assert(!status && "vkCreateQueryPool");
    return params.result;
}

VkResult WINAPI vkCreateRayTracingPipelinesKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkRayTracingPipelineCreateInfoKHR *pCreateInfos, const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines)
{
    struct vkCreateRayTracingPipelinesKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.deferredOperation = deferredOperation;
    params.pipelineCache = pipelineCache;
    params.createInfoCount = createInfoCount;
    params.pCreateInfos = pCreateInfos;
    params.pAllocator = pAllocator;
    params.pPipelines = pPipelines;
    status = UNIX_CALL(vkCreateRayTracingPipelinesKHR, &params);
    assert(!status && "vkCreateRayTracingPipelinesKHR");
    return params.result;
}

VkResult WINAPI vkCreateRayTracingPipelinesNV(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkRayTracingPipelineCreateInfoNV *pCreateInfos, const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines)
{
    struct vkCreateRayTracingPipelinesNV_params params;
    NTSTATUS status;
    params.device = device;
    params.pipelineCache = pipelineCache;
    params.createInfoCount = createInfoCount;
    params.pCreateInfos = pCreateInfos;
    params.pAllocator = pAllocator;
    params.pPipelines = pPipelines;
    status = UNIX_CALL(vkCreateRayTracingPipelinesNV, &params);
    assert(!status && "vkCreateRayTracingPipelinesNV");
    return params.result;
}

VkResult WINAPI vkCreateRenderPass(VkDevice device, const VkRenderPassCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkRenderPass *pRenderPass)
{
    struct vkCreateRenderPass_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pRenderPass = pRenderPass;
    status = UNIX_CALL(vkCreateRenderPass, &params);
    assert(!status && "vkCreateRenderPass");
    return params.result;
}

VkResult WINAPI vkCreateRenderPass2(VkDevice device, const VkRenderPassCreateInfo2 *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkRenderPass *pRenderPass)
{
    struct vkCreateRenderPass2_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pRenderPass = pRenderPass;
    status = UNIX_CALL(vkCreateRenderPass2, &params);
    assert(!status && "vkCreateRenderPass2");
    return params.result;
}

VkResult WINAPI vkCreateRenderPass2KHR(VkDevice device, const VkRenderPassCreateInfo2 *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkRenderPass *pRenderPass)
{
    struct vkCreateRenderPass2KHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pRenderPass = pRenderPass;
    status = UNIX_CALL(vkCreateRenderPass2KHR, &params);
    assert(!status && "vkCreateRenderPass2KHR");
    return params.result;
}

VkResult WINAPI vkCreateSampler(VkDevice device, const VkSamplerCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSampler *pSampler)
{
    struct vkCreateSampler_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pSampler = pSampler;
    status = UNIX_CALL(vkCreateSampler, &params);
    assert(!status && "vkCreateSampler");
    return params.result;
}

VkResult WINAPI vkCreateSamplerYcbcrConversion(VkDevice device, const VkSamplerYcbcrConversionCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSamplerYcbcrConversion *pYcbcrConversion)
{
    struct vkCreateSamplerYcbcrConversion_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pYcbcrConversion = pYcbcrConversion;
    status = UNIX_CALL(vkCreateSamplerYcbcrConversion, &params);
    assert(!status && "vkCreateSamplerYcbcrConversion");
    return params.result;
}

VkResult WINAPI vkCreateSamplerYcbcrConversionKHR(VkDevice device, const VkSamplerYcbcrConversionCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSamplerYcbcrConversion *pYcbcrConversion)
{
    struct vkCreateSamplerYcbcrConversionKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pYcbcrConversion = pYcbcrConversion;
    status = UNIX_CALL(vkCreateSamplerYcbcrConversionKHR, &params);
    assert(!status && "vkCreateSamplerYcbcrConversionKHR");
    return params.result;
}

VkResult WINAPI vkCreateSemaphore(VkDevice device, const VkSemaphoreCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSemaphore *pSemaphore)
{
    struct vkCreateSemaphore_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pSemaphore = pSemaphore;
    status = UNIX_CALL(vkCreateSemaphore, &params);
    assert(!status && "vkCreateSemaphore");
    return params.result;
}

VkResult WINAPI vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkShaderModule *pShaderModule)
{
    struct vkCreateShaderModule_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pShaderModule = pShaderModule;
    status = UNIX_CALL(vkCreateShaderModule, &params);
    assert(!status && "vkCreateShaderModule");
    return params.result;
}

VkResult WINAPI vkCreateShadersEXT(VkDevice device, uint32_t createInfoCount, const VkShaderCreateInfoEXT *pCreateInfos, const VkAllocationCallbacks *pAllocator, VkShaderEXT *pShaders)
{
    struct vkCreateShadersEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.createInfoCount = createInfoCount;
    params.pCreateInfos = pCreateInfos;
    params.pAllocator = pAllocator;
    params.pShaders = pShaders;
    status = UNIX_CALL(vkCreateShadersEXT, &params);
    assert(!status && "vkCreateShadersEXT");
    return params.result;
}

VkResult WINAPI vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSwapchainKHR *pSwapchain)
{
    struct vkCreateSwapchainKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pSwapchain = pSwapchain;
    status = UNIX_CALL(vkCreateSwapchainKHR, &params);
    assert(!status && "vkCreateSwapchainKHR");
    return params.result;
}

VkResult WINAPI vkCreateValidationCacheEXT(VkDevice device, const VkValidationCacheCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkValidationCacheEXT *pValidationCache)
{
    struct vkCreateValidationCacheEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pValidationCache = pValidationCache;
    status = UNIX_CALL(vkCreateValidationCacheEXT, &params);
    assert(!status && "vkCreateValidationCacheEXT");
    return params.result;
}

VkResult WINAPI vkCreateVideoSessionKHR(VkDevice device, const VkVideoSessionCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkVideoSessionKHR *pVideoSession)
{
    struct vkCreateVideoSessionKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pVideoSession = pVideoSession;
    status = UNIX_CALL(vkCreateVideoSessionKHR, &params);
    assert(!status && "vkCreateVideoSessionKHR");
    return params.result;
}

VkResult WINAPI vkCreateVideoSessionParametersKHR(VkDevice device, const VkVideoSessionParametersCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkVideoSessionParametersKHR *pVideoSessionParameters)
{
    struct vkCreateVideoSessionParametersKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pVideoSessionParameters = pVideoSessionParameters;
    status = UNIX_CALL(vkCreateVideoSessionParametersKHR, &params);
    assert(!status && "vkCreateVideoSessionParametersKHR");
    return params.result;
}

VkResult WINAPI vkCreateWin32SurfaceKHR(VkInstance instance, const VkWin32SurfaceCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSurfaceKHR *pSurface)
{
    struct vkCreateWin32SurfaceKHR_params params;
    NTSTATUS status;
    params.instance = instance;
    params.pCreateInfo = pCreateInfo;
    params.pAllocator = pAllocator;
    params.pSurface = pSurface;
    status = UNIX_CALL(vkCreateWin32SurfaceKHR, &params);
    assert(!status && "vkCreateWin32SurfaceKHR");
    return params.result;
}

VkResult WINAPI vkDebugMarkerSetObjectNameEXT(VkDevice device, const VkDebugMarkerObjectNameInfoEXT *pNameInfo)
{
    struct vkDebugMarkerSetObjectNameEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pNameInfo = pNameInfo;
    status = UNIX_CALL(vkDebugMarkerSetObjectNameEXT, &params);
    assert(!status && "vkDebugMarkerSetObjectNameEXT");
    return params.result;
}

VkResult WINAPI vkDebugMarkerSetObjectTagEXT(VkDevice device, const VkDebugMarkerObjectTagInfoEXT *pTagInfo)
{
    struct vkDebugMarkerSetObjectTagEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pTagInfo = pTagInfo;
    status = UNIX_CALL(vkDebugMarkerSetObjectTagEXT, &params);
    assert(!status && "vkDebugMarkerSetObjectTagEXT");
    return params.result;
}

void WINAPI vkDebugReportMessageEXT(VkInstance instance, VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char *pLayerPrefix, const char *pMessage)
{
    struct vkDebugReportMessageEXT_params params;
    NTSTATUS status;
    params.instance = instance;
    params.flags = flags;
    params.objectType = objectType;
    params.object = object;
    params.location = location;
    params.messageCode = messageCode;
    params.pLayerPrefix = pLayerPrefix;
    params.pMessage = pMessage;
    status = UNIX_CALL(vkDebugReportMessageEXT, &params);
    assert(!status && "vkDebugReportMessageEXT");
}

VkResult WINAPI vkDeferredOperationJoinKHR(VkDevice device, VkDeferredOperationKHR operation)
{
    struct vkDeferredOperationJoinKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.operation = operation;
    status = UNIX_CALL(vkDeferredOperationJoinKHR, &params);
    assert(!status && "vkDeferredOperationJoinKHR");
    return params.result;
}

void WINAPI vkDestroyAccelerationStructureKHR(VkDevice device, VkAccelerationStructureKHR accelerationStructure, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyAccelerationStructureKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.accelerationStructure = accelerationStructure;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyAccelerationStructureKHR, &params);
    assert(!status && "vkDestroyAccelerationStructureKHR");
}

void WINAPI vkDestroyAccelerationStructureNV(VkDevice device, VkAccelerationStructureNV accelerationStructure, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyAccelerationStructureNV_params params;
    NTSTATUS status;
    params.device = device;
    params.accelerationStructure = accelerationStructure;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyAccelerationStructureNV, &params);
    assert(!status && "vkDestroyAccelerationStructureNV");
}

void WINAPI vkDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyBuffer_params params;
    NTSTATUS status;
    params.device = device;
    params.buffer = buffer;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyBuffer, &params);
    assert(!status && "vkDestroyBuffer");
}

void WINAPI vkDestroyBufferView(VkDevice device, VkBufferView bufferView, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyBufferView_params params;
    NTSTATUS status;
    params.device = device;
    params.bufferView = bufferView;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyBufferView, &params);
    assert(!status && "vkDestroyBufferView");
}

void WINAPI vkDestroyCuFunctionNVX(VkDevice device, VkCuFunctionNVX function, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyCuFunctionNVX_params params;
    NTSTATUS status;
    params.device = device;
    params.function = function;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyCuFunctionNVX, &params);
    assert(!status && "vkDestroyCuFunctionNVX");
}

void WINAPI vkDestroyCuModuleNVX(VkDevice device, VkCuModuleNVX module, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyCuModuleNVX_params params;
    NTSTATUS status;
    params.device = device;
    params.module = module;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyCuModuleNVX, &params);
    assert(!status && "vkDestroyCuModuleNVX");
}

void WINAPI vkDestroyCudaFunctionNV(VkDevice device, VkCudaFunctionNV function, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyCudaFunctionNV_params params;
    NTSTATUS status;
    params.device = device;
    params.function = function;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyCudaFunctionNV, &params);
    assert(!status && "vkDestroyCudaFunctionNV");
}

void WINAPI vkDestroyCudaModuleNV(VkDevice device, VkCudaModuleNV module, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyCudaModuleNV_params params;
    NTSTATUS status;
    params.device = device;
    params.module = module;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyCudaModuleNV, &params);
    assert(!status && "vkDestroyCudaModuleNV");
}

void WINAPI vkDestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyDebugReportCallbackEXT_params params;
    NTSTATUS status;
    params.instance = instance;
    params.callback = callback;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyDebugReportCallbackEXT, &params);
    assert(!status && "vkDestroyDebugReportCallbackEXT");
}

void WINAPI vkDestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyDebugUtilsMessengerEXT_params params;
    NTSTATUS status;
    params.instance = instance;
    params.messenger = messenger;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyDebugUtilsMessengerEXT, &params);
    assert(!status && "vkDestroyDebugUtilsMessengerEXT");
}

void WINAPI vkDestroyDeferredOperationKHR(VkDevice device, VkDeferredOperationKHR operation, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyDeferredOperationKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.operation = operation;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyDeferredOperationKHR, &params);
    assert(!status && "vkDestroyDeferredOperationKHR");
}

void WINAPI vkDestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyDescriptorPool_params params;
    NTSTATUS status;
    params.device = device;
    params.descriptorPool = descriptorPool;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyDescriptorPool, &params);
    assert(!status && "vkDestroyDescriptorPool");
}

void WINAPI vkDestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyDescriptorSetLayout_params params;
    NTSTATUS status;
    params.device = device;
    params.descriptorSetLayout = descriptorSetLayout;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyDescriptorSetLayout, &params);
    assert(!status && "vkDestroyDescriptorSetLayout");
}

void WINAPI vkDestroyDescriptorUpdateTemplate(VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyDescriptorUpdateTemplate_params params;
    NTSTATUS status;
    params.device = device;
    params.descriptorUpdateTemplate = descriptorUpdateTemplate;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyDescriptorUpdateTemplate, &params);
    assert(!status && "vkDestroyDescriptorUpdateTemplate");
}

void WINAPI vkDestroyDescriptorUpdateTemplateKHR(VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyDescriptorUpdateTemplateKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.descriptorUpdateTemplate = descriptorUpdateTemplate;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyDescriptorUpdateTemplateKHR, &params);
    assert(!status && "vkDestroyDescriptorUpdateTemplateKHR");
}

void WINAPI vkDestroyEvent(VkDevice device, VkEvent event, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyEvent_params params;
    NTSTATUS status;
    params.device = device;
    params.event = event;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyEvent, &params);
    assert(!status && "vkDestroyEvent");
}

void WINAPI vkDestroyFence(VkDevice device, VkFence fence, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyFence_params params;
    NTSTATUS status;
    params.device = device;
    params.fence = fence;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyFence, &params);
    assert(!status && "vkDestroyFence");
}

void WINAPI vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyFramebuffer_params params;
    NTSTATUS status;
    params.device = device;
    params.framebuffer = framebuffer;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyFramebuffer, &params);
    assert(!status && "vkDestroyFramebuffer");
}

void WINAPI vkDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyImage_params params;
    NTSTATUS status;
    params.device = device;
    params.image = image;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyImage, &params);
    assert(!status && "vkDestroyImage");
}

void WINAPI vkDestroyImageView(VkDevice device, VkImageView imageView, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyImageView_params params;
    NTSTATUS status;
    params.device = device;
    params.imageView = imageView;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyImageView, &params);
    assert(!status && "vkDestroyImageView");
}

void WINAPI vkDestroyIndirectCommandsLayoutEXT(VkDevice device, VkIndirectCommandsLayoutEXT indirectCommandsLayout, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyIndirectCommandsLayoutEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.indirectCommandsLayout = indirectCommandsLayout;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyIndirectCommandsLayoutEXT, &params);
    assert(!status && "vkDestroyIndirectCommandsLayoutEXT");
}

void WINAPI vkDestroyIndirectCommandsLayoutNV(VkDevice device, VkIndirectCommandsLayoutNV indirectCommandsLayout, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyIndirectCommandsLayoutNV_params params;
    NTSTATUS status;
    params.device = device;
    params.indirectCommandsLayout = indirectCommandsLayout;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyIndirectCommandsLayoutNV, &params);
    assert(!status && "vkDestroyIndirectCommandsLayoutNV");
}

void WINAPI vkDestroyIndirectExecutionSetEXT(VkDevice device, VkIndirectExecutionSetEXT indirectExecutionSet, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyIndirectExecutionSetEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.indirectExecutionSet = indirectExecutionSet;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyIndirectExecutionSetEXT, &params);
    assert(!status && "vkDestroyIndirectExecutionSetEXT");
}

void WINAPI vkDestroyMicromapEXT(VkDevice device, VkMicromapEXT micromap, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyMicromapEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.micromap = micromap;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyMicromapEXT, &params);
    assert(!status && "vkDestroyMicromapEXT");
}

void WINAPI vkDestroyOpticalFlowSessionNV(VkDevice device, VkOpticalFlowSessionNV session, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyOpticalFlowSessionNV_params params;
    NTSTATUS status;
    params.device = device;
    params.session = session;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyOpticalFlowSessionNV, &params);
    assert(!status && "vkDestroyOpticalFlowSessionNV");
}

void WINAPI vkDestroyPipeline(VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyPipeline_params params;
    NTSTATUS status;
    params.device = device;
    params.pipeline = pipeline;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyPipeline, &params);
    assert(!status && "vkDestroyPipeline");
}

void WINAPI vkDestroyPipelineBinaryKHR(VkDevice device, VkPipelineBinaryKHR pipelineBinary, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyPipelineBinaryKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pipelineBinary = pipelineBinary;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyPipelineBinaryKHR, &params);
    assert(!status && "vkDestroyPipelineBinaryKHR");
}

void WINAPI vkDestroyPipelineCache(VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyPipelineCache_params params;
    NTSTATUS status;
    params.device = device;
    params.pipelineCache = pipelineCache;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyPipelineCache, &params);
    assert(!status && "vkDestroyPipelineCache");
}

void WINAPI vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyPipelineLayout_params params;
    NTSTATUS status;
    params.device = device;
    params.pipelineLayout = pipelineLayout;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyPipelineLayout, &params);
    assert(!status && "vkDestroyPipelineLayout");
}

void WINAPI vkDestroyPrivateDataSlot(VkDevice device, VkPrivateDataSlot privateDataSlot, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyPrivateDataSlot_params params;
    NTSTATUS status;
    params.device = device;
    params.privateDataSlot = privateDataSlot;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyPrivateDataSlot, &params);
    assert(!status && "vkDestroyPrivateDataSlot");
}

void WINAPI vkDestroyPrivateDataSlotEXT(VkDevice device, VkPrivateDataSlot privateDataSlot, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyPrivateDataSlotEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.privateDataSlot = privateDataSlot;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyPrivateDataSlotEXT, &params);
    assert(!status && "vkDestroyPrivateDataSlotEXT");
}

void WINAPI vkDestroyQueryPool(VkDevice device, VkQueryPool queryPool, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyQueryPool_params params;
    NTSTATUS status;
    params.device = device;
    params.queryPool = queryPool;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyQueryPool, &params);
    assert(!status && "vkDestroyQueryPool");
}

void WINAPI vkDestroyRenderPass(VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyRenderPass_params params;
    NTSTATUS status;
    params.device = device;
    params.renderPass = renderPass;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyRenderPass, &params);
    assert(!status && "vkDestroyRenderPass");
}

void WINAPI vkDestroySampler(VkDevice device, VkSampler sampler, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroySampler_params params;
    NTSTATUS status;
    params.device = device;
    params.sampler = sampler;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroySampler, &params);
    assert(!status && "vkDestroySampler");
}

void WINAPI vkDestroySamplerYcbcrConversion(VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroySamplerYcbcrConversion_params params;
    NTSTATUS status;
    params.device = device;
    params.ycbcrConversion = ycbcrConversion;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroySamplerYcbcrConversion, &params);
    assert(!status && "vkDestroySamplerYcbcrConversion");
}

void WINAPI vkDestroySamplerYcbcrConversionKHR(VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroySamplerYcbcrConversionKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.ycbcrConversion = ycbcrConversion;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroySamplerYcbcrConversionKHR, &params);
    assert(!status && "vkDestroySamplerYcbcrConversionKHR");
}

void WINAPI vkDestroySemaphore(VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroySemaphore_params params;
    NTSTATUS status;
    params.device = device;
    params.semaphore = semaphore;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroySemaphore, &params);
    assert(!status && "vkDestroySemaphore");
}

void WINAPI vkDestroyShaderEXT(VkDevice device, VkShaderEXT shader, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyShaderEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.shader = shader;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyShaderEXT, &params);
    assert(!status && "vkDestroyShaderEXT");
}

void WINAPI vkDestroyShaderModule(VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyShaderModule_params params;
    NTSTATUS status;
    params.device = device;
    params.shaderModule = shaderModule;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyShaderModule, &params);
    assert(!status && "vkDestroyShaderModule");
}

void WINAPI vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroySurfaceKHR_params params;
    NTSTATUS status;
    params.instance = instance;
    params.surface = surface;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroySurfaceKHR, &params);
    assert(!status && "vkDestroySurfaceKHR");
}

void WINAPI vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroySwapchainKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.swapchain = swapchain;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroySwapchainKHR, &params);
    assert(!status && "vkDestroySwapchainKHR");
}

void WINAPI vkDestroyValidationCacheEXT(VkDevice device, VkValidationCacheEXT validationCache, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyValidationCacheEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.validationCache = validationCache;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyValidationCacheEXT, &params);
    assert(!status && "vkDestroyValidationCacheEXT");
}

void WINAPI vkDestroyVideoSessionKHR(VkDevice device, VkVideoSessionKHR videoSession, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyVideoSessionKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.videoSession = videoSession;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyVideoSessionKHR, &params);
    assert(!status && "vkDestroyVideoSessionKHR");
}

void WINAPI vkDestroyVideoSessionParametersKHR(VkDevice device, VkVideoSessionParametersKHR videoSessionParameters, const VkAllocationCallbacks *pAllocator)
{
    struct vkDestroyVideoSessionParametersKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.videoSessionParameters = videoSessionParameters;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkDestroyVideoSessionParametersKHR, &params);
    assert(!status && "vkDestroyVideoSessionParametersKHR");
}

VkResult WINAPI vkDeviceWaitIdle(VkDevice device)
{
    struct vkDeviceWaitIdle_params params;
    NTSTATUS status;
    params.device = device;
    status = UNIX_CALL(vkDeviceWaitIdle, &params);
    assert(!status && "vkDeviceWaitIdle");
    return params.result;
}

VkResult WINAPI vkEndCommandBuffer(VkCommandBuffer commandBuffer)
{
    struct vkEndCommandBuffer_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    status = UNIX_CALL(vkEndCommandBuffer, &params);
    assert(!status && "vkEndCommandBuffer");
    return params.result;
}

VkResult WINAPI vkEnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice, const char *pLayerName, uint32_t *pPropertyCount, VkExtensionProperties *pProperties)
{
    struct vkEnumerateDeviceExtensionProperties_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pLayerName = pLayerName;
    params.pPropertyCount = pPropertyCount;
    params.pProperties = pProperties;
    status = UNIX_CALL(vkEnumerateDeviceExtensionProperties, &params);
    assert(!status && "vkEnumerateDeviceExtensionProperties");
    return params.result;
}

VkResult WINAPI vkEnumerateDeviceLayerProperties(VkPhysicalDevice physicalDevice, uint32_t *pPropertyCount, VkLayerProperties *pProperties)
{
    struct vkEnumerateDeviceLayerProperties_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pPropertyCount = pPropertyCount;
    params.pProperties = pProperties;
    status = UNIX_CALL(vkEnumerateDeviceLayerProperties, &params);
    assert(!status && "vkEnumerateDeviceLayerProperties");
    return params.result;
}

VkResult WINAPI vkEnumeratePhysicalDeviceGroups(VkInstance instance, uint32_t *pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties *pPhysicalDeviceGroupProperties)
{
    struct vkEnumeratePhysicalDeviceGroups_params params;
    NTSTATUS status;
    params.instance = instance;
    params.pPhysicalDeviceGroupCount = pPhysicalDeviceGroupCount;
    params.pPhysicalDeviceGroupProperties = pPhysicalDeviceGroupProperties;
    status = UNIX_CALL(vkEnumeratePhysicalDeviceGroups, &params);
    assert(!status && "vkEnumeratePhysicalDeviceGroups");
    return params.result;
}

VkResult WINAPI vkEnumeratePhysicalDeviceGroupsKHR(VkInstance instance, uint32_t *pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties *pPhysicalDeviceGroupProperties)
{
    struct vkEnumeratePhysicalDeviceGroupsKHR_params params;
    NTSTATUS status;
    params.instance = instance;
    params.pPhysicalDeviceGroupCount = pPhysicalDeviceGroupCount;
    params.pPhysicalDeviceGroupProperties = pPhysicalDeviceGroupProperties;
    status = UNIX_CALL(vkEnumeratePhysicalDeviceGroupsKHR, &params);
    assert(!status && "vkEnumeratePhysicalDeviceGroupsKHR");
    return params.result;
}

VkResult WINAPI vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, uint32_t *pCounterCount, VkPerformanceCounterKHR *pCounters, VkPerformanceCounterDescriptionKHR *pCounterDescriptions)
{
    struct vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.queueFamilyIndex = queueFamilyIndex;
    params.pCounterCount = pCounterCount;
    params.pCounters = pCounters;
    params.pCounterDescriptions = pCounterDescriptions;
    status = UNIX_CALL(vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR, &params);
    assert(!status && "vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR");
    return params.result;
}

VkResult WINAPI vkEnumeratePhysicalDevices(VkInstance instance, uint32_t *pPhysicalDeviceCount, VkPhysicalDevice *pPhysicalDevices)
{
    struct vkEnumeratePhysicalDevices_params params;
    NTSTATUS status;
    params.instance = instance;
    params.pPhysicalDeviceCount = pPhysicalDeviceCount;
    params.pPhysicalDevices = pPhysicalDevices;
    status = UNIX_CALL(vkEnumeratePhysicalDevices, &params);
    assert(!status && "vkEnumeratePhysicalDevices");
    return params.result;
}

VkResult WINAPI vkFlushMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange *pMemoryRanges)
{
    struct vkFlushMappedMemoryRanges_params params;
    NTSTATUS status;
    params.device = device;
    params.memoryRangeCount = memoryRangeCount;
    params.pMemoryRanges = pMemoryRanges;
    status = UNIX_CALL(vkFlushMappedMemoryRanges, &params);
    assert(!status && "vkFlushMappedMemoryRanges");
    return params.result;
}

VkResult WINAPI vkFreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, uint32_t descriptorSetCount, const VkDescriptorSet *pDescriptorSets)
{
    struct vkFreeDescriptorSets_params params;
    NTSTATUS status;
    params.device = device;
    params.descriptorPool = descriptorPool;
    params.descriptorSetCount = descriptorSetCount;
    params.pDescriptorSets = pDescriptorSets;
    status = UNIX_CALL(vkFreeDescriptorSets, &params);
    assert(!status && "vkFreeDescriptorSets");
    return params.result;
}

void WINAPI vkFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks *pAllocator)
{
    struct vkFreeMemory_params params;
    NTSTATUS status;
    params.device = device;
    params.memory = memory;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkFreeMemory, &params);
    assert(!status && "vkFreeMemory");
}

void WINAPI vkGetAccelerationStructureBuildSizesKHR(VkDevice device, VkAccelerationStructureBuildTypeKHR buildType, const VkAccelerationStructureBuildGeometryInfoKHR *pBuildInfo, const uint32_t *pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR *pSizeInfo)
{
    struct vkGetAccelerationStructureBuildSizesKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.buildType = buildType;
    params.pBuildInfo = pBuildInfo;
    params.pMaxPrimitiveCounts = pMaxPrimitiveCounts;
    params.pSizeInfo = pSizeInfo;
    status = UNIX_CALL(vkGetAccelerationStructureBuildSizesKHR, &params);
    assert(!status && "vkGetAccelerationStructureBuildSizesKHR");
}

VkDeviceAddress WINAPI vkGetAccelerationStructureDeviceAddressKHR(VkDevice device, const VkAccelerationStructureDeviceAddressInfoKHR *pInfo)
{
    struct vkGetAccelerationStructureDeviceAddressKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkGetAccelerationStructureDeviceAddressKHR, &params);
    assert(!status && "vkGetAccelerationStructureDeviceAddressKHR");
    return params.result;
}

VkResult WINAPI vkGetAccelerationStructureHandleNV(VkDevice device, VkAccelerationStructureNV accelerationStructure, size_t dataSize, void *pData)
{
    struct vkGetAccelerationStructureHandleNV_params params;
    NTSTATUS status;
    params.device = device;
    params.accelerationStructure = accelerationStructure;
    params.dataSize = dataSize;
    params.pData = pData;
    status = UNIX_CALL(vkGetAccelerationStructureHandleNV, &params);
    assert(!status && "vkGetAccelerationStructureHandleNV");
    return params.result;
}

void WINAPI vkGetAccelerationStructureMemoryRequirementsNV(VkDevice device, const VkAccelerationStructureMemoryRequirementsInfoNV *pInfo, VkMemoryRequirements2KHR *pMemoryRequirements)
{
    struct vkGetAccelerationStructureMemoryRequirementsNV_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pMemoryRequirements = pMemoryRequirements;
    status = UNIX_CALL(vkGetAccelerationStructureMemoryRequirementsNV, &params);
    assert(!status && "vkGetAccelerationStructureMemoryRequirementsNV");
}

VkResult WINAPI vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT(VkDevice device, const VkAccelerationStructureCaptureDescriptorDataInfoEXT *pInfo, void *pData)
{
    struct vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pData = pData;
    status = UNIX_CALL(vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT, &params);
    assert(!status && "vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT");
    return params.result;
}

VkDeviceAddress WINAPI vkGetBufferDeviceAddress(VkDevice device, const VkBufferDeviceAddressInfo *pInfo)
{
    struct vkGetBufferDeviceAddress_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkGetBufferDeviceAddress, &params);
    assert(!status && "vkGetBufferDeviceAddress");
    return params.result;
}

VkDeviceAddress WINAPI vkGetBufferDeviceAddressEXT(VkDevice device, const VkBufferDeviceAddressInfo *pInfo)
{
    struct vkGetBufferDeviceAddressEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkGetBufferDeviceAddressEXT, &params);
    assert(!status && "vkGetBufferDeviceAddressEXT");
    return params.result;
}

VkDeviceAddress WINAPI vkGetBufferDeviceAddressKHR(VkDevice device, const VkBufferDeviceAddressInfo *pInfo)
{
    struct vkGetBufferDeviceAddressKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkGetBufferDeviceAddressKHR, &params);
    assert(!status && "vkGetBufferDeviceAddressKHR");
    return params.result;
}

void WINAPI vkGetBufferMemoryRequirements(VkDevice device, VkBuffer buffer, VkMemoryRequirements *pMemoryRequirements)
{
    struct vkGetBufferMemoryRequirements_params params;
    NTSTATUS status;
    params.device = device;
    params.buffer = buffer;
    params.pMemoryRequirements = pMemoryRequirements;
    status = UNIX_CALL(vkGetBufferMemoryRequirements, &params);
    assert(!status && "vkGetBufferMemoryRequirements");
}

void WINAPI vkGetBufferMemoryRequirements2(VkDevice device, const VkBufferMemoryRequirementsInfo2 *pInfo, VkMemoryRequirements2 *pMemoryRequirements)
{
    struct vkGetBufferMemoryRequirements2_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pMemoryRequirements = pMemoryRequirements;
    status = UNIX_CALL(vkGetBufferMemoryRequirements2, &params);
    assert(!status && "vkGetBufferMemoryRequirements2");
}

void WINAPI vkGetBufferMemoryRequirements2KHR(VkDevice device, const VkBufferMemoryRequirementsInfo2 *pInfo, VkMemoryRequirements2 *pMemoryRequirements)
{
    struct vkGetBufferMemoryRequirements2KHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pMemoryRequirements = pMemoryRequirements;
    status = UNIX_CALL(vkGetBufferMemoryRequirements2KHR, &params);
    assert(!status && "vkGetBufferMemoryRequirements2KHR");
}

uint64_t WINAPI vkGetBufferOpaqueCaptureAddress(VkDevice device, const VkBufferDeviceAddressInfo *pInfo)
{
    struct vkGetBufferOpaqueCaptureAddress_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkGetBufferOpaqueCaptureAddress, &params);
    assert(!status && "vkGetBufferOpaqueCaptureAddress");
    return params.result;
}

uint64_t WINAPI vkGetBufferOpaqueCaptureAddressKHR(VkDevice device, const VkBufferDeviceAddressInfo *pInfo)
{
    struct vkGetBufferOpaqueCaptureAddressKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkGetBufferOpaqueCaptureAddressKHR, &params);
    assert(!status && "vkGetBufferOpaqueCaptureAddressKHR");
    return params.result;
}

VkResult WINAPI vkGetBufferOpaqueCaptureDescriptorDataEXT(VkDevice device, const VkBufferCaptureDescriptorDataInfoEXT *pInfo, void *pData)
{
    struct vkGetBufferOpaqueCaptureDescriptorDataEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pData = pData;
    status = UNIX_CALL(vkGetBufferOpaqueCaptureDescriptorDataEXT, &params);
    assert(!status && "vkGetBufferOpaqueCaptureDescriptorDataEXT");
    return params.result;
}

VkResult WINAPI vkGetCalibratedTimestampsEXT(VkDevice device, uint32_t timestampCount, const VkCalibratedTimestampInfoKHR *pTimestampInfos, uint64_t *pTimestamps, uint64_t *pMaxDeviation)
{
    struct vkGetCalibratedTimestampsEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.timestampCount = timestampCount;
    params.pTimestampInfos = pTimestampInfos;
    params.pTimestamps = pTimestamps;
    params.pMaxDeviation = pMaxDeviation;
    status = UNIX_CALL(vkGetCalibratedTimestampsEXT, &params);
    assert(!status && "vkGetCalibratedTimestampsEXT");
    return params.result;
}

VkResult WINAPI vkGetCalibratedTimestampsKHR(VkDevice device, uint32_t timestampCount, const VkCalibratedTimestampInfoKHR *pTimestampInfos, uint64_t *pTimestamps, uint64_t *pMaxDeviation)
{
    struct vkGetCalibratedTimestampsKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.timestampCount = timestampCount;
    params.pTimestampInfos = pTimestampInfos;
    params.pTimestamps = pTimestamps;
    params.pMaxDeviation = pMaxDeviation;
    status = UNIX_CALL(vkGetCalibratedTimestampsKHR, &params);
    assert(!status && "vkGetCalibratedTimestampsKHR");
    return params.result;
}

void WINAPI vkGetClusterAccelerationStructureBuildSizesNV(VkDevice device, const VkClusterAccelerationStructureInputInfoNV *pInfo, VkAccelerationStructureBuildSizesInfoKHR *pSizeInfo)
{
    struct vkGetClusterAccelerationStructureBuildSizesNV_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pSizeInfo = pSizeInfo;
    status = UNIX_CALL(vkGetClusterAccelerationStructureBuildSizesNV, &params);
    assert(!status && "vkGetClusterAccelerationStructureBuildSizesNV");
}

VkResult WINAPI vkGetCudaModuleCacheNV(VkDevice device, VkCudaModuleNV module, size_t *pCacheSize, void *pCacheData)
{
    struct vkGetCudaModuleCacheNV_params params;
    NTSTATUS status;
    params.device = device;
    params.module = module;
    params.pCacheSize = pCacheSize;
    params.pCacheData = pCacheData;
    status = UNIX_CALL(vkGetCudaModuleCacheNV, &params);
    assert(!status && "vkGetCudaModuleCacheNV");
    return params.result;
}

uint32_t WINAPI vkGetDeferredOperationMaxConcurrencyKHR(VkDevice device, VkDeferredOperationKHR operation)
{
    struct vkGetDeferredOperationMaxConcurrencyKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.operation = operation;
    status = UNIX_CALL(vkGetDeferredOperationMaxConcurrencyKHR, &params);
    assert(!status && "vkGetDeferredOperationMaxConcurrencyKHR");
    return params.result;
}

VkResult WINAPI vkGetDeferredOperationResultKHR(VkDevice device, VkDeferredOperationKHR operation)
{
    struct vkGetDeferredOperationResultKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.operation = operation;
    status = UNIX_CALL(vkGetDeferredOperationResultKHR, &params);
    assert(!status && "vkGetDeferredOperationResultKHR");
    return params.result;
}

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkGetDescriptorEXT == 435 );
VK_FAST_THUNK( vkGetDescriptorEXT, "435", 4 )
#else
void WINAPI vkGetDescriptorEXT(VkDevice device, const VkDescriptorGetInfoEXT *pDescriptorInfo, size_t dataSize, void *pDescriptor)
{
    struct vkGetDescriptorEXT_params params;
    params.device = device;
    params.pDescriptorInfo = pDescriptorInfo;
    params.dataSize = dataSize;
    params.pDescriptor = pDescriptor;
    UNIX_CALL(vkGetDescriptorEXT, &params);
}
#endif /* VK_FAST_THUNK */

void WINAPI vkGetDescriptorSetHostMappingVALVE(VkDevice device, VkDescriptorSet descriptorSet, void **ppData)
{
    struct vkGetDescriptorSetHostMappingVALVE_params params;
    NTSTATUS status;
    params.device = device;
    params.descriptorSet = descriptorSet;
    params.ppData = ppData;
    status = UNIX_CALL(vkGetDescriptorSetHostMappingVALVE, &params);
    assert(!status && "vkGetDescriptorSetHostMappingVALVE");
}

void WINAPI vkGetDescriptorSetLayoutBindingOffsetEXT(VkDevice device, VkDescriptorSetLayout layout, uint32_t binding, VkDeviceSize *pOffset)
{
    struct vkGetDescriptorSetLayoutBindingOffsetEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.layout = layout;
    params.binding = binding;
    params.pOffset = pOffset;
    status = UNIX_CALL(vkGetDescriptorSetLayoutBindingOffsetEXT, &params);
    assert(!status && "vkGetDescriptorSetLayoutBindingOffsetEXT");
}

void WINAPI vkGetDescriptorSetLayoutHostMappingInfoVALVE(VkDevice device, const VkDescriptorSetBindingReferenceVALVE *pBindingReference, VkDescriptorSetLayoutHostMappingInfoVALVE *pHostMapping)
{
    struct vkGetDescriptorSetLayoutHostMappingInfoVALVE_params params;
    NTSTATUS status;
    params.device = device;
    params.pBindingReference = pBindingReference;
    params.pHostMapping = pHostMapping;
    status = UNIX_CALL(vkGetDescriptorSetLayoutHostMappingInfoVALVE, &params);
    assert(!status && "vkGetDescriptorSetLayoutHostMappingInfoVALVE");
}

void WINAPI vkGetDescriptorSetLayoutSizeEXT(VkDevice device, VkDescriptorSetLayout layout, VkDeviceSize *pLayoutSizeInBytes)
{
    struct vkGetDescriptorSetLayoutSizeEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.layout = layout;
    params.pLayoutSizeInBytes = pLayoutSizeInBytes;
    status = UNIX_CALL(vkGetDescriptorSetLayoutSizeEXT, &params);
    assert(!status && "vkGetDescriptorSetLayoutSizeEXT");
}

void WINAPI vkGetDescriptorSetLayoutSupport(VkDevice device, const VkDescriptorSetLayoutCreateInfo *pCreateInfo, VkDescriptorSetLayoutSupport *pSupport)
{
    struct vkGetDescriptorSetLayoutSupport_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pSupport = pSupport;
    status = UNIX_CALL(vkGetDescriptorSetLayoutSupport, &params);
    assert(!status && "vkGetDescriptorSetLayoutSupport");
}

void WINAPI vkGetDescriptorSetLayoutSupportKHR(VkDevice device, const VkDescriptorSetLayoutCreateInfo *pCreateInfo, VkDescriptorSetLayoutSupport *pSupport)
{
    struct vkGetDescriptorSetLayoutSupportKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pSupport = pSupport;
    status = UNIX_CALL(vkGetDescriptorSetLayoutSupportKHR, &params);
    assert(!status && "vkGetDescriptorSetLayoutSupportKHR");
}

void WINAPI vkGetDeviceAccelerationStructureCompatibilityKHR(VkDevice device, const VkAccelerationStructureVersionInfoKHR *pVersionInfo, VkAccelerationStructureCompatibilityKHR *pCompatibility)
{
    struct vkGetDeviceAccelerationStructureCompatibilityKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pVersionInfo = pVersionInfo;
    params.pCompatibility = pCompatibility;
    status = UNIX_CALL(vkGetDeviceAccelerationStructureCompatibilityKHR, &params);
    assert(!status && "vkGetDeviceAccelerationStructureCompatibilityKHR");
}

void WINAPI vkGetDeviceBufferMemoryRequirements(VkDevice device, const VkDeviceBufferMemoryRequirements *pInfo, VkMemoryRequirements2 *pMemoryRequirements)
{
    struct vkGetDeviceBufferMemoryRequirements_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pMemoryRequirements = pMemoryRequirements;
    status = UNIX_CALL(vkGetDeviceBufferMemoryRequirements, &params);
    assert(!status && "vkGetDeviceBufferMemoryRequirements");
}

void WINAPI vkGetDeviceBufferMemoryRequirementsKHR(VkDevice device, const VkDeviceBufferMemoryRequirements *pInfo, VkMemoryRequirements2 *pMemoryRequirements)
{
    struct vkGetDeviceBufferMemoryRequirementsKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pMemoryRequirements = pMemoryRequirements;
    status = UNIX_CALL(vkGetDeviceBufferMemoryRequirementsKHR, &params);
    assert(!status && "vkGetDeviceBufferMemoryRequirementsKHR");
}

VkResult WINAPI vkGetDeviceFaultInfoEXT(VkDevice device, VkDeviceFaultCountsEXT *pFaultCounts, VkDeviceFaultInfoEXT *pFaultInfo)
{
    struct vkGetDeviceFaultInfoEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pFaultCounts = pFaultCounts;
    params.pFaultInfo = pFaultInfo;
    status = UNIX_CALL(vkGetDeviceFaultInfoEXT, &params);
    assert(!status && "vkGetDeviceFaultInfoEXT");
    return params.result;
}

void WINAPI vkGetDeviceGroupPeerMemoryFeatures(VkDevice device, uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlags *pPeerMemoryFeatures)
{
    struct vkGetDeviceGroupPeerMemoryFeatures_params params;
    NTSTATUS status;
    params.device = device;
    params.heapIndex = heapIndex;
    params.localDeviceIndex = localDeviceIndex;
    params.remoteDeviceIndex = remoteDeviceIndex;
    params.pPeerMemoryFeatures = pPeerMemoryFeatures;
    status = UNIX_CALL(vkGetDeviceGroupPeerMemoryFeatures, &params);
    assert(!status && "vkGetDeviceGroupPeerMemoryFeatures");
}

void WINAPI vkGetDeviceGroupPeerMemoryFeaturesKHR(VkDevice device, uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlags *pPeerMemoryFeatures)
{
    struct vkGetDeviceGroupPeerMemoryFeaturesKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.heapIndex = heapIndex;
    params.localDeviceIndex = localDeviceIndex;
    params.remoteDeviceIndex = remoteDeviceIndex;
    params.pPeerMemoryFeatures = pPeerMemoryFeatures;
    status = UNIX_CALL(vkGetDeviceGroupPeerMemoryFeaturesKHR, &params);
    assert(!status && "vkGetDeviceGroupPeerMemoryFeaturesKHR");
}

VkResult WINAPI vkGetDeviceGroupPresentCapabilitiesKHR(VkDevice device, VkDeviceGroupPresentCapabilitiesKHR *pDeviceGroupPresentCapabilities)
{
    struct vkGetDeviceGroupPresentCapabilitiesKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pDeviceGroupPresentCapabilities = pDeviceGroupPresentCapabilities;
    status = UNIX_CALL(vkGetDeviceGroupPresentCapabilitiesKHR, &params);
    assert(!status && "vkGetDeviceGroupPresentCapabilitiesKHR");
    return params.result;
}

VkResult WINAPI vkGetDeviceGroupSurfacePresentModesKHR(VkDevice device, VkSurfaceKHR surface, VkDeviceGroupPresentModeFlagsKHR *pModes)
{
    struct vkGetDeviceGroupSurfacePresentModesKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.surface = surface;
    params.pModes = pModes;
    status = UNIX_CALL(vkGetDeviceGroupSurfacePresentModesKHR, &params);
    assert(!status && "vkGetDeviceGroupSurfacePresentModesKHR");
    return params.result;
}

void WINAPI vkGetDeviceImageMemoryRequirements(VkDevice device, const VkDeviceImageMemoryRequirements *pInfo, VkMemoryRequirements2 *pMemoryRequirements)
{
    struct vkGetDeviceImageMemoryRequirements_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pMemoryRequirements = pMemoryRequirements;
    status = UNIX_CALL(vkGetDeviceImageMemoryRequirements, &params);
    assert(!status && "vkGetDeviceImageMemoryRequirements");
}

void WINAPI vkGetDeviceImageMemoryRequirementsKHR(VkDevice device, const VkDeviceImageMemoryRequirements *pInfo, VkMemoryRequirements2 *pMemoryRequirements)
{
    struct vkGetDeviceImageMemoryRequirementsKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pMemoryRequirements = pMemoryRequirements;
    status = UNIX_CALL(vkGetDeviceImageMemoryRequirementsKHR, &params);
    assert(!status && "vkGetDeviceImageMemoryRequirementsKHR");
}

void WINAPI vkGetDeviceImageSparseMemoryRequirements(VkDevice device, const VkDeviceImageMemoryRequirements *pInfo, uint32_t *pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2 *pSparseMemoryRequirements)
{
    struct vkGetDeviceImageSparseMemoryRequirements_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pSparseMemoryRequirementCount = pSparseMemoryRequirementCount;
    params.pSparseMemoryRequirements = pSparseMemoryRequirements;
    status = UNIX_CALL(vkGetDeviceImageSparseMemoryRequirements, &params);
    assert(!status && "vkGetDeviceImageSparseMemoryRequirements");
}

void WINAPI vkGetDeviceImageSparseMemoryRequirementsKHR(VkDevice device, const VkDeviceImageMemoryRequirements *pInfo, uint32_t *pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2 *pSparseMemoryRequirements)
{
    struct vkGetDeviceImageSparseMemoryRequirementsKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pSparseMemoryRequirementCount = pSparseMemoryRequirementCount;
    params.pSparseMemoryRequirements = pSparseMemoryRequirements;
    status = UNIX_CALL(vkGetDeviceImageSparseMemoryRequirementsKHR, &params);
    assert(!status && "vkGetDeviceImageSparseMemoryRequirementsKHR");
}

void WINAPI vkGetDeviceImageSubresourceLayout(VkDevice device, const VkDeviceImageSubresourceInfo *pInfo, VkSubresourceLayout2 *pLayout)
{
    struct vkGetDeviceImageSubresourceLayout_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pLayout = pLayout;
    status = UNIX_CALL(vkGetDeviceImageSubresourceLayout, &params);
    assert(!status && "vkGetDeviceImageSubresourceLayout");
}

void WINAPI vkGetDeviceImageSubresourceLayoutKHR(VkDevice device, const VkDeviceImageSubresourceInfo *pInfo, VkSubresourceLayout2 *pLayout)
{
    struct vkGetDeviceImageSubresourceLayoutKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pLayout = pLayout;
    status = UNIX_CALL(vkGetDeviceImageSubresourceLayoutKHR, &params);
    assert(!status && "vkGetDeviceImageSubresourceLayoutKHR");
}

void WINAPI vkGetDeviceMemoryCommitment(VkDevice device, VkDeviceMemory memory, VkDeviceSize *pCommittedMemoryInBytes)
{
    struct vkGetDeviceMemoryCommitment_params params;
    NTSTATUS status;
    params.device = device;
    params.memory = memory;
    params.pCommittedMemoryInBytes = pCommittedMemoryInBytes;
    status = UNIX_CALL(vkGetDeviceMemoryCommitment, &params);
    assert(!status && "vkGetDeviceMemoryCommitment");
}

uint64_t WINAPI vkGetDeviceMemoryOpaqueCaptureAddress(VkDevice device, const VkDeviceMemoryOpaqueCaptureAddressInfo *pInfo)
{
    struct vkGetDeviceMemoryOpaqueCaptureAddress_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkGetDeviceMemoryOpaqueCaptureAddress, &params);
    assert(!status && "vkGetDeviceMemoryOpaqueCaptureAddress");
    return params.result;
}

uint64_t WINAPI vkGetDeviceMemoryOpaqueCaptureAddressKHR(VkDevice device, const VkDeviceMemoryOpaqueCaptureAddressInfo *pInfo)
{
    struct vkGetDeviceMemoryOpaqueCaptureAddressKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkGetDeviceMemoryOpaqueCaptureAddressKHR, &params);
    assert(!status && "vkGetDeviceMemoryOpaqueCaptureAddressKHR");
    return params.result;
}

void WINAPI vkGetDeviceMicromapCompatibilityEXT(VkDevice device, const VkMicromapVersionInfoEXT *pVersionInfo, VkAccelerationStructureCompatibilityKHR *pCompatibility)
{
    struct vkGetDeviceMicromapCompatibilityEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pVersionInfo = pVersionInfo;
    params.pCompatibility = pCompatibility;
    status = UNIX_CALL(vkGetDeviceMicromapCompatibilityEXT, &params);
    assert(!status && "vkGetDeviceMicromapCompatibilityEXT");
}

void WINAPI vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue *pQueue)
{
    struct vkGetDeviceQueue_params params;
    NTSTATUS status;
    params.device = device;
    params.queueFamilyIndex = queueFamilyIndex;
    params.queueIndex = queueIndex;
    params.pQueue = pQueue;
    status = UNIX_CALL(vkGetDeviceQueue, &params);
    assert(!status && "vkGetDeviceQueue");
}

void WINAPI vkGetDeviceQueue2(VkDevice device, const VkDeviceQueueInfo2 *pQueueInfo, VkQueue *pQueue)
{
    struct vkGetDeviceQueue2_params params;
    NTSTATUS status;
    params.device = device;
    params.pQueueInfo = pQueueInfo;
    params.pQueue = pQueue;
    status = UNIX_CALL(vkGetDeviceQueue2, &params);
    assert(!status && "vkGetDeviceQueue2");
}

VkResult WINAPI vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI(VkDevice device, VkRenderPass renderpass, VkExtent2D *pMaxWorkgroupSize)
{
    struct vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI_params params;
    NTSTATUS status;
    params.device = device;
    params.renderpass = renderpass;
    params.pMaxWorkgroupSize = pMaxWorkgroupSize;
    status = UNIX_CALL(vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI, &params);
    assert(!status && "vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI");
    return params.result;
}

VkResult WINAPI vkGetDynamicRenderingTilePropertiesQCOM(VkDevice device, const VkRenderingInfo *pRenderingInfo, VkTilePropertiesQCOM *pProperties)
{
    struct vkGetDynamicRenderingTilePropertiesQCOM_params params;
    NTSTATUS status;
    params.device = device;
    params.pRenderingInfo = pRenderingInfo;
    params.pProperties = pProperties;
    status = UNIX_CALL(vkGetDynamicRenderingTilePropertiesQCOM, &params);
    assert(!status && "vkGetDynamicRenderingTilePropertiesQCOM");
    return params.result;
}

VkResult WINAPI vkGetEncodedVideoSessionParametersKHR(VkDevice device, const VkVideoEncodeSessionParametersGetInfoKHR *pVideoSessionParametersInfo, VkVideoEncodeSessionParametersFeedbackInfoKHR *pFeedbackInfo, size_t *pDataSize, void *pData)
{
    struct vkGetEncodedVideoSessionParametersKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pVideoSessionParametersInfo = pVideoSessionParametersInfo;
    params.pFeedbackInfo = pFeedbackInfo;
    params.pDataSize = pDataSize;
    params.pData = pData;
    status = UNIX_CALL(vkGetEncodedVideoSessionParametersKHR, &params);
    assert(!status && "vkGetEncodedVideoSessionParametersKHR");
    return params.result;
}

VkResult WINAPI vkGetEventStatus(VkDevice device, VkEvent event)
{
    struct vkGetEventStatus_params params;
    NTSTATUS status;
    params.device = device;
    params.event = event;
    status = UNIX_CALL(vkGetEventStatus, &params);
    assert(!status && "vkGetEventStatus");
    return params.result;
}

VkResult WINAPI vkGetFenceStatus(VkDevice device, VkFence fence)
{
    struct vkGetFenceStatus_params params;
    NTSTATUS status;
    params.device = device;
    params.fence = fence;
    status = UNIX_CALL(vkGetFenceStatus, &params);
    assert(!status && "vkGetFenceStatus");
    return params.result;
}

VkResult WINAPI vkGetFramebufferTilePropertiesQCOM(VkDevice device, VkFramebuffer framebuffer, uint32_t *pPropertiesCount, VkTilePropertiesQCOM *pProperties)
{
    struct vkGetFramebufferTilePropertiesQCOM_params params;
    NTSTATUS status;
    params.device = device;
    params.framebuffer = framebuffer;
    params.pPropertiesCount = pPropertiesCount;
    params.pProperties = pProperties;
    status = UNIX_CALL(vkGetFramebufferTilePropertiesQCOM, &params);
    assert(!status && "vkGetFramebufferTilePropertiesQCOM");
    return params.result;
}

void WINAPI vkGetGeneratedCommandsMemoryRequirementsEXT(VkDevice device, const VkGeneratedCommandsMemoryRequirementsInfoEXT *pInfo, VkMemoryRequirements2 *pMemoryRequirements)
{
    struct vkGetGeneratedCommandsMemoryRequirementsEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pMemoryRequirements = pMemoryRequirements;
    status = UNIX_CALL(vkGetGeneratedCommandsMemoryRequirementsEXT, &params);
    assert(!status && "vkGetGeneratedCommandsMemoryRequirementsEXT");
}

void WINAPI vkGetGeneratedCommandsMemoryRequirementsNV(VkDevice device, const VkGeneratedCommandsMemoryRequirementsInfoNV *pInfo, VkMemoryRequirements2 *pMemoryRequirements)
{
    struct vkGetGeneratedCommandsMemoryRequirementsNV_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pMemoryRequirements = pMemoryRequirements;
    status = UNIX_CALL(vkGetGeneratedCommandsMemoryRequirementsNV, &params);
    assert(!status && "vkGetGeneratedCommandsMemoryRequirementsNV");
}

void WINAPI vkGetImageMemoryRequirements(VkDevice device, VkImage image, VkMemoryRequirements *pMemoryRequirements)
{
    struct vkGetImageMemoryRequirements_params params;
    NTSTATUS status;
    params.device = device;
    params.image = image;
    params.pMemoryRequirements = pMemoryRequirements;
    status = UNIX_CALL(vkGetImageMemoryRequirements, &params);
    assert(!status && "vkGetImageMemoryRequirements");
}

void WINAPI vkGetImageMemoryRequirements2(VkDevice device, const VkImageMemoryRequirementsInfo2 *pInfo, VkMemoryRequirements2 *pMemoryRequirements)
{
    struct vkGetImageMemoryRequirements2_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pMemoryRequirements = pMemoryRequirements;
    status = UNIX_CALL(vkGetImageMemoryRequirements2, &params);
    assert(!status && "vkGetImageMemoryRequirements2");
}

void WINAPI vkGetImageMemoryRequirements2KHR(VkDevice device, const VkImageMemoryRequirementsInfo2 *pInfo, VkMemoryRequirements2 *pMemoryRequirements)
{
    struct vkGetImageMemoryRequirements2KHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pMemoryRequirements = pMemoryRequirements;
    status = UNIX_CALL(vkGetImageMemoryRequirements2KHR, &params);
    assert(!status && "vkGetImageMemoryRequirements2KHR");
}

VkResult WINAPI vkGetImageOpaqueCaptureDescriptorDataEXT(VkDevice device, const VkImageCaptureDescriptorDataInfoEXT *pInfo, void *pData)
{
    struct vkGetImageOpaqueCaptureDescriptorDataEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pData = pData;
    status = UNIX_CALL(vkGetImageOpaqueCaptureDescriptorDataEXT, &params);
    assert(!status && "vkGetImageOpaqueCaptureDescriptorDataEXT");
    return params.result;
}

void WINAPI vkGetImageSparseMemoryRequirements(VkDevice device, VkImage image, uint32_t *pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements *pSparseMemoryRequirements)
{
    struct vkGetImageSparseMemoryRequirements_params params;
    NTSTATUS status;
    params.device = device;
    params.image = image;
    params.pSparseMemoryRequirementCount = pSparseMemoryRequirementCount;
    params.pSparseMemoryRequirements = pSparseMemoryRequirements;
    status = UNIX_CALL(vkGetImageSparseMemoryRequirements, &params);
    assert(!status && "vkGetImageSparseMemoryRequirements");
}

void WINAPI vkGetImageSparseMemoryRequirements2(VkDevice device, const VkImageSparseMemoryRequirementsInfo2 *pInfo, uint32_t *pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2 *pSparseMemoryRequirements)
{
    struct vkGetImageSparseMemoryRequirements2_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pSparseMemoryRequirementCount = pSparseMemoryRequirementCount;
    params.pSparseMemoryRequirements = pSparseMemoryRequirements;
    status = UNIX_CALL(vkGetImageSparseMemoryRequirements2, &params);
    assert(!status && "vkGetImageSparseMemoryRequirements2");
}

void WINAPI vkGetImageSparseMemoryRequirements2KHR(VkDevice device, const VkImageSparseMemoryRequirementsInfo2 *pInfo, uint32_t *pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2 *pSparseMemoryRequirements)
{
    struct vkGetImageSparseMemoryRequirements2KHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pSparseMemoryRequirementCount = pSparseMemoryRequirementCount;
    params.pSparseMemoryRequirements = pSparseMemoryRequirements;
    status = UNIX_CALL(vkGetImageSparseMemoryRequirements2KHR, &params);
    assert(!status && "vkGetImageSparseMemoryRequirements2KHR");
}

void WINAPI vkGetImageSubresourceLayout(VkDevice device, VkImage image, const VkImageSubresource *pSubresource, VkSubresourceLayout *pLayout)
{
    struct vkGetImageSubresourceLayout_params params;
    NTSTATUS status;
    params.device = device;
    params.image = image;
    params.pSubresource = pSubresource;
    params.pLayout = pLayout;
    status = UNIX_CALL(vkGetImageSubresourceLayout, &params);
    assert(!status && "vkGetImageSubresourceLayout");
}

void WINAPI vkGetImageSubresourceLayout2(VkDevice device, VkImage image, const VkImageSubresource2 *pSubresource, VkSubresourceLayout2 *pLayout)
{
    struct vkGetImageSubresourceLayout2_params params;
    NTSTATUS status;
    params.device = device;
    params.image = image;
    params.pSubresource = pSubresource;
    params.pLayout = pLayout;
    status = UNIX_CALL(vkGetImageSubresourceLayout2, &params);
    assert(!status && "vkGetImageSubresourceLayout2");
}

void WINAPI vkGetImageSubresourceLayout2EXT(VkDevice device, VkImage image, const VkImageSubresource2 *pSubresource, VkSubresourceLayout2 *pLayout)
{
    struct vkGetImageSubresourceLayout2EXT_params params;
    NTSTATUS status;
    params.device = device;
    params.image = image;
    params.pSubresource = pSubresource;
    params.pLayout = pLayout;
    status = UNIX_CALL(vkGetImageSubresourceLayout2EXT, &params);
    assert(!status && "vkGetImageSubresourceLayout2EXT");
}

void WINAPI vkGetImageSubresourceLayout2KHR(VkDevice device, VkImage image, const VkImageSubresource2 *pSubresource, VkSubresourceLayout2 *pLayout)
{
    struct vkGetImageSubresourceLayout2KHR_params params;
    NTSTATUS status;
    params.device = device;
    params.image = image;
    params.pSubresource = pSubresource;
    params.pLayout = pLayout;
    status = UNIX_CALL(vkGetImageSubresourceLayout2KHR, &params);
    assert(!status && "vkGetImageSubresourceLayout2KHR");
}

VkResult WINAPI vkGetImageViewAddressNVX(VkDevice device, VkImageView imageView, VkImageViewAddressPropertiesNVX *pProperties)
{
    struct vkGetImageViewAddressNVX_params params;
    NTSTATUS status;
    params.device = device;
    params.imageView = imageView;
    params.pProperties = pProperties;
    status = UNIX_CALL(vkGetImageViewAddressNVX, &params);
    assert(!status && "vkGetImageViewAddressNVX");
    return params.result;
}

uint64_t WINAPI vkGetImageViewHandle64NVX(VkDevice device, const VkImageViewHandleInfoNVX *pInfo)
{
    struct vkGetImageViewHandle64NVX_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkGetImageViewHandle64NVX, &params);
    assert(!status && "vkGetImageViewHandle64NVX");
    return params.result;
}

uint32_t WINAPI vkGetImageViewHandleNVX(VkDevice device, const VkImageViewHandleInfoNVX *pInfo)
{
    struct vkGetImageViewHandleNVX_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkGetImageViewHandleNVX, &params);
    assert(!status && "vkGetImageViewHandleNVX");
    return params.result;
}

VkResult WINAPI vkGetImageViewOpaqueCaptureDescriptorDataEXT(VkDevice device, const VkImageViewCaptureDescriptorDataInfoEXT *pInfo, void *pData)
{
    struct vkGetImageViewOpaqueCaptureDescriptorDataEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pData = pData;
    status = UNIX_CALL(vkGetImageViewOpaqueCaptureDescriptorDataEXT, &params);
    assert(!status && "vkGetImageViewOpaqueCaptureDescriptorDataEXT");
    return params.result;
}

void WINAPI vkGetLatencyTimingsNV(VkDevice device, VkSwapchainKHR swapchain, VkGetLatencyMarkerInfoNV *pLatencyMarkerInfo)
{
    struct vkGetLatencyTimingsNV_params params;
    NTSTATUS status;
    params.device = device;
    params.swapchain = swapchain;
    params.pLatencyMarkerInfo = pLatencyMarkerInfo;
    status = UNIX_CALL(vkGetLatencyTimingsNV, &params);
    assert(!status && "vkGetLatencyTimingsNV");
}

VkResult WINAPI vkGetMemoryHostPointerPropertiesEXT(VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, const void *pHostPointer, VkMemoryHostPointerPropertiesEXT *pMemoryHostPointerProperties)
{
    struct vkGetMemoryHostPointerPropertiesEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.handleType = handleType;
    params.pHostPointer = pHostPointer;
    params.pMemoryHostPointerProperties = pMemoryHostPointerProperties;
    status = UNIX_CALL(vkGetMemoryHostPointerPropertiesEXT, &params);
    assert(!status && "vkGetMemoryHostPointerPropertiesEXT");
    return params.result;
}

void WINAPI vkGetMicromapBuildSizesEXT(VkDevice device, VkAccelerationStructureBuildTypeKHR buildType, const VkMicromapBuildInfoEXT *pBuildInfo, VkMicromapBuildSizesInfoEXT *pSizeInfo)
{
    struct vkGetMicromapBuildSizesEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.buildType = buildType;
    params.pBuildInfo = pBuildInfo;
    params.pSizeInfo = pSizeInfo;
    status = UNIX_CALL(vkGetMicromapBuildSizesEXT, &params);
    assert(!status && "vkGetMicromapBuildSizesEXT");
}

void WINAPI vkGetPartitionedAccelerationStructuresBuildSizesNV(VkDevice device, const VkPartitionedAccelerationStructureInstancesInputNV *pInfo, VkAccelerationStructureBuildSizesInfoKHR *pSizeInfo)
{
    struct vkGetPartitionedAccelerationStructuresBuildSizesNV_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pSizeInfo = pSizeInfo;
    status = UNIX_CALL(vkGetPartitionedAccelerationStructuresBuildSizesNV, &params);
    assert(!status && "vkGetPartitionedAccelerationStructuresBuildSizesNV");
}

VkResult WINAPI vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(VkPhysicalDevice physicalDevice, uint32_t *pTimeDomainCount, VkTimeDomainKHR *pTimeDomains)
{
    struct vkGetPhysicalDeviceCalibrateableTimeDomainsEXT_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pTimeDomainCount = pTimeDomainCount;
    params.pTimeDomains = pTimeDomains;
    status = UNIX_CALL(vkGetPhysicalDeviceCalibrateableTimeDomainsEXT, &params);
    assert(!status && "vkGetPhysicalDeviceCalibrateableTimeDomainsEXT");
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceCalibrateableTimeDomainsKHR(VkPhysicalDevice physicalDevice, uint32_t *pTimeDomainCount, VkTimeDomainKHR *pTimeDomains)
{
    struct vkGetPhysicalDeviceCalibrateableTimeDomainsKHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pTimeDomainCount = pTimeDomainCount;
    params.pTimeDomains = pTimeDomains;
    status = UNIX_CALL(vkGetPhysicalDeviceCalibrateableTimeDomainsKHR, &params);
    assert(!status && "vkGetPhysicalDeviceCalibrateableTimeDomainsKHR");
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV(VkPhysicalDevice physicalDevice, uint32_t *pPropertyCount, VkCooperativeMatrixFlexibleDimensionsPropertiesNV *pProperties)
{
    struct vkGetPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pPropertyCount = pPropertyCount;
    params.pProperties = pProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV, &params);
    assert(!status && "vkGetPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV");
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR(VkPhysicalDevice physicalDevice, uint32_t *pPropertyCount, VkCooperativeMatrixPropertiesKHR *pProperties)
{
    struct vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pPropertyCount = pPropertyCount;
    params.pProperties = pProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR, &params);
    assert(!status && "vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR");
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceCooperativeMatrixPropertiesNV(VkPhysicalDevice physicalDevice, uint32_t *pPropertyCount, VkCooperativeMatrixPropertiesNV *pProperties)
{
    struct vkGetPhysicalDeviceCooperativeMatrixPropertiesNV_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pPropertyCount = pPropertyCount;
    params.pProperties = pProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceCooperativeMatrixPropertiesNV, &params);
    assert(!status && "vkGetPhysicalDeviceCooperativeMatrixPropertiesNV");
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceCooperativeVectorPropertiesNV(VkPhysicalDevice physicalDevice, uint32_t *pPropertyCount, VkCooperativeVectorPropertiesNV *pProperties)
{
    struct vkGetPhysicalDeviceCooperativeVectorPropertiesNV_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pPropertyCount = pPropertyCount;
    params.pProperties = pProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceCooperativeVectorPropertiesNV, &params);
    assert(!status && "vkGetPhysicalDeviceCooperativeVectorPropertiesNV");
    return params.result;
}

void WINAPI vkGetPhysicalDeviceExternalBufferProperties(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo *pExternalBufferInfo, VkExternalBufferProperties *pExternalBufferProperties)
{
    struct vkGetPhysicalDeviceExternalBufferProperties_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pExternalBufferInfo = pExternalBufferInfo;
    params.pExternalBufferProperties = pExternalBufferProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceExternalBufferProperties, &params);
    assert(!status && "vkGetPhysicalDeviceExternalBufferProperties");
}

void WINAPI vkGetPhysicalDeviceExternalBufferPropertiesKHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo *pExternalBufferInfo, VkExternalBufferProperties *pExternalBufferProperties)
{
    struct vkGetPhysicalDeviceExternalBufferPropertiesKHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pExternalBufferInfo = pExternalBufferInfo;
    params.pExternalBufferProperties = pExternalBufferProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceExternalBufferPropertiesKHR, &params);
    assert(!status && "vkGetPhysicalDeviceExternalBufferPropertiesKHR");
}

void WINAPI vkGetPhysicalDeviceExternalFenceProperties(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo *pExternalFenceInfo, VkExternalFenceProperties *pExternalFenceProperties)
{
    struct vkGetPhysicalDeviceExternalFenceProperties_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pExternalFenceInfo = pExternalFenceInfo;
    params.pExternalFenceProperties = pExternalFenceProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceExternalFenceProperties, &params);
    assert(!status && "vkGetPhysicalDeviceExternalFenceProperties");
}

void WINAPI vkGetPhysicalDeviceExternalFencePropertiesKHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo *pExternalFenceInfo, VkExternalFenceProperties *pExternalFenceProperties)
{
    struct vkGetPhysicalDeviceExternalFencePropertiesKHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pExternalFenceInfo = pExternalFenceInfo;
    params.pExternalFenceProperties = pExternalFenceProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceExternalFencePropertiesKHR, &params);
    assert(!status && "vkGetPhysicalDeviceExternalFencePropertiesKHR");
}

void WINAPI vkGetPhysicalDeviceExternalSemaphoreProperties(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo *pExternalSemaphoreInfo, VkExternalSemaphoreProperties *pExternalSemaphoreProperties)
{
    struct vkGetPhysicalDeviceExternalSemaphoreProperties_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pExternalSemaphoreInfo = pExternalSemaphoreInfo;
    params.pExternalSemaphoreProperties = pExternalSemaphoreProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceExternalSemaphoreProperties, &params);
    assert(!status && "vkGetPhysicalDeviceExternalSemaphoreProperties");
}

void WINAPI vkGetPhysicalDeviceExternalSemaphorePropertiesKHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo *pExternalSemaphoreInfo, VkExternalSemaphoreProperties *pExternalSemaphoreProperties)
{
    struct vkGetPhysicalDeviceExternalSemaphorePropertiesKHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pExternalSemaphoreInfo = pExternalSemaphoreInfo;
    params.pExternalSemaphoreProperties = pExternalSemaphoreProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceExternalSemaphorePropertiesKHR, &params);
    assert(!status && "vkGetPhysicalDeviceExternalSemaphorePropertiesKHR");
}

void WINAPI vkGetPhysicalDeviceFeatures(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures *pFeatures)
{
    struct vkGetPhysicalDeviceFeatures_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pFeatures = pFeatures;
    status = UNIX_CALL(vkGetPhysicalDeviceFeatures, &params);
    assert(!status && "vkGetPhysicalDeviceFeatures");
}

void WINAPI vkGetPhysicalDeviceFeatures2(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2 *pFeatures)
{
    struct vkGetPhysicalDeviceFeatures2_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pFeatures = pFeatures;
    status = UNIX_CALL(vkGetPhysicalDeviceFeatures2, &params);
    assert(!status && "vkGetPhysicalDeviceFeatures2");
}

void WINAPI vkGetPhysicalDeviceFeatures2KHR(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2 *pFeatures)
{
    struct vkGetPhysicalDeviceFeatures2KHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pFeatures = pFeatures;
    status = UNIX_CALL(vkGetPhysicalDeviceFeatures2KHR, &params);
    assert(!status && "vkGetPhysicalDeviceFeatures2KHR");
}

void WINAPI vkGetPhysicalDeviceFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties *pFormatProperties)
{
    struct vkGetPhysicalDeviceFormatProperties_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.format = format;
    params.pFormatProperties = pFormatProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceFormatProperties, &params);
    assert(!status && "vkGetPhysicalDeviceFormatProperties");
}

void WINAPI vkGetPhysicalDeviceFormatProperties2(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2 *pFormatProperties)
{
    struct vkGetPhysicalDeviceFormatProperties2_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.format = format;
    params.pFormatProperties = pFormatProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceFormatProperties2, &params);
    assert(!status && "vkGetPhysicalDeviceFormatProperties2");
}

void WINAPI vkGetPhysicalDeviceFormatProperties2KHR(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2 *pFormatProperties)
{
    struct vkGetPhysicalDeviceFormatProperties2KHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.format = format;
    params.pFormatProperties = pFormatProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceFormatProperties2KHR, &params);
    assert(!status && "vkGetPhysicalDeviceFormatProperties2KHR");
}

VkResult WINAPI vkGetPhysicalDeviceFragmentShadingRatesKHR(VkPhysicalDevice physicalDevice, uint32_t *pFragmentShadingRateCount, VkPhysicalDeviceFragmentShadingRateKHR *pFragmentShadingRates)
{
    struct vkGetPhysicalDeviceFragmentShadingRatesKHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pFragmentShadingRateCount = pFragmentShadingRateCount;
    params.pFragmentShadingRates = pFragmentShadingRates;
    status = UNIX_CALL(vkGetPhysicalDeviceFragmentShadingRatesKHR, &params);
    assert(!status && "vkGetPhysicalDeviceFragmentShadingRatesKHR");
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties *pImageFormatProperties)
{
    struct vkGetPhysicalDeviceImageFormatProperties_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.format = format;
    params.type = type;
    params.tiling = tiling;
    params.usage = usage;
    params.flags = flags;
    params.pImageFormatProperties = pImageFormatProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceImageFormatProperties, &params);
    assert(!status && "vkGetPhysicalDeviceImageFormatProperties");
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceImageFormatProperties2(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2 *pImageFormatInfo, VkImageFormatProperties2 *pImageFormatProperties)
{
    struct vkGetPhysicalDeviceImageFormatProperties2_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pImageFormatInfo = pImageFormatInfo;
    params.pImageFormatProperties = pImageFormatProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceImageFormatProperties2, &params);
    assert(!status && "vkGetPhysicalDeviceImageFormatProperties2");
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceImageFormatProperties2KHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2 *pImageFormatInfo, VkImageFormatProperties2 *pImageFormatProperties)
{
    struct vkGetPhysicalDeviceImageFormatProperties2KHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pImageFormatInfo = pImageFormatInfo;
    params.pImageFormatProperties = pImageFormatProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceImageFormatProperties2KHR, &params);
    assert(!status && "vkGetPhysicalDeviceImageFormatProperties2KHR");
    return params.result;
}

void WINAPI vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties *pMemoryProperties)
{
    struct vkGetPhysicalDeviceMemoryProperties_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pMemoryProperties = pMemoryProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceMemoryProperties, &params);
    assert(!status && "vkGetPhysicalDeviceMemoryProperties");
}

void WINAPI vkGetPhysicalDeviceMemoryProperties2(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2 *pMemoryProperties)
{
    struct vkGetPhysicalDeviceMemoryProperties2_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pMemoryProperties = pMemoryProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceMemoryProperties2, &params);
    assert(!status && "vkGetPhysicalDeviceMemoryProperties2");
}

void WINAPI vkGetPhysicalDeviceMemoryProperties2KHR(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2 *pMemoryProperties)
{
    struct vkGetPhysicalDeviceMemoryProperties2KHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pMemoryProperties = pMemoryProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceMemoryProperties2KHR, &params);
    assert(!status && "vkGetPhysicalDeviceMemoryProperties2KHR");
}

void WINAPI vkGetPhysicalDeviceMultisamplePropertiesEXT(VkPhysicalDevice physicalDevice, VkSampleCountFlagBits samples, VkMultisamplePropertiesEXT *pMultisampleProperties)
{
    struct vkGetPhysicalDeviceMultisamplePropertiesEXT_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.samples = samples;
    params.pMultisampleProperties = pMultisampleProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceMultisamplePropertiesEXT, &params);
    assert(!status && "vkGetPhysicalDeviceMultisamplePropertiesEXT");
}

VkResult WINAPI vkGetPhysicalDeviceOpticalFlowImageFormatsNV(VkPhysicalDevice physicalDevice, const VkOpticalFlowImageFormatInfoNV *pOpticalFlowImageFormatInfo, uint32_t *pFormatCount, VkOpticalFlowImageFormatPropertiesNV *pImageFormatProperties)
{
    struct vkGetPhysicalDeviceOpticalFlowImageFormatsNV_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pOpticalFlowImageFormatInfo = pOpticalFlowImageFormatInfo;
    params.pFormatCount = pFormatCount;
    params.pImageFormatProperties = pImageFormatProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceOpticalFlowImageFormatsNV, &params);
    assert(!status && "vkGetPhysicalDeviceOpticalFlowImageFormatsNV");
    return params.result;
}

VkResult WINAPI vkGetPhysicalDevicePresentRectanglesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t *pRectCount, VkRect2D *pRects)
{
    struct vkGetPhysicalDevicePresentRectanglesKHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.surface = surface;
    params.pRectCount = pRectCount;
    params.pRects = pRects;
    status = UNIX_CALL(vkGetPhysicalDevicePresentRectanglesKHR, &params);
    assert(!status && "vkGetPhysicalDevicePresentRectanglesKHR");
    return params.result;
}

void WINAPI vkGetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties *pProperties)
{
    struct vkGetPhysicalDeviceProperties_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pProperties = pProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceProperties, &params);
    assert(!status && "vkGetPhysicalDeviceProperties");
}

void WINAPI vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR(VkPhysicalDevice physicalDevice, const VkQueryPoolPerformanceCreateInfoKHR *pPerformanceQueryCreateInfo, uint32_t *pNumPasses)
{
    struct vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pPerformanceQueryCreateInfo = pPerformanceQueryCreateInfo;
    params.pNumPasses = pNumPasses;
    status = UNIX_CALL(vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR, &params);
    assert(!status && "vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR");
}

void WINAPI vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physicalDevice, uint32_t *pQueueFamilyPropertyCount, VkQueueFamilyProperties *pQueueFamilyProperties)
{
    struct vkGetPhysicalDeviceQueueFamilyProperties_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pQueueFamilyPropertyCount = pQueueFamilyPropertyCount;
    params.pQueueFamilyProperties = pQueueFamilyProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceQueueFamilyProperties, &params);
    assert(!status && "vkGetPhysicalDeviceQueueFamilyProperties");
}

void WINAPI vkGetPhysicalDeviceQueueFamilyProperties2(VkPhysicalDevice physicalDevice, uint32_t *pQueueFamilyPropertyCount, VkQueueFamilyProperties2 *pQueueFamilyProperties)
{
    struct vkGetPhysicalDeviceQueueFamilyProperties2_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pQueueFamilyPropertyCount = pQueueFamilyPropertyCount;
    params.pQueueFamilyProperties = pQueueFamilyProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceQueueFamilyProperties2, &params);
    assert(!status && "vkGetPhysicalDeviceQueueFamilyProperties2");
}

void WINAPI vkGetPhysicalDeviceQueueFamilyProperties2KHR(VkPhysicalDevice physicalDevice, uint32_t *pQueueFamilyPropertyCount, VkQueueFamilyProperties2 *pQueueFamilyProperties)
{
    struct vkGetPhysicalDeviceQueueFamilyProperties2KHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pQueueFamilyPropertyCount = pQueueFamilyPropertyCount;
    params.pQueueFamilyProperties = pQueueFamilyProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceQueueFamilyProperties2KHR, &params);
    assert(!status && "vkGetPhysicalDeviceQueueFamilyProperties2KHR");
}

void WINAPI vkGetPhysicalDeviceSparseImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageTiling tiling, uint32_t *pPropertyCount, VkSparseImageFormatProperties *pProperties)
{
    struct vkGetPhysicalDeviceSparseImageFormatProperties_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.format = format;
    params.type = type;
    params.samples = samples;
    params.usage = usage;
    params.tiling = tiling;
    params.pPropertyCount = pPropertyCount;
    params.pProperties = pProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceSparseImageFormatProperties, &params);
    assert(!status && "vkGetPhysicalDeviceSparseImageFormatProperties");
}

void WINAPI vkGetPhysicalDeviceSparseImageFormatProperties2(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2 *pFormatInfo, uint32_t *pPropertyCount, VkSparseImageFormatProperties2 *pProperties)
{
    struct vkGetPhysicalDeviceSparseImageFormatProperties2_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pFormatInfo = pFormatInfo;
    params.pPropertyCount = pPropertyCount;
    params.pProperties = pProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceSparseImageFormatProperties2, &params);
    assert(!status && "vkGetPhysicalDeviceSparseImageFormatProperties2");
}

void WINAPI vkGetPhysicalDeviceSparseImageFormatProperties2KHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2 *pFormatInfo, uint32_t *pPropertyCount, VkSparseImageFormatProperties2 *pProperties)
{
    struct vkGetPhysicalDeviceSparseImageFormatProperties2KHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pFormatInfo = pFormatInfo;
    params.pPropertyCount = pPropertyCount;
    params.pProperties = pProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceSparseImageFormatProperties2KHR, &params);
    assert(!status && "vkGetPhysicalDeviceSparseImageFormatProperties2KHR");
}

VkResult WINAPI vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV(VkPhysicalDevice physicalDevice, uint32_t *pCombinationCount, VkFramebufferMixedSamplesCombinationNV *pCombinations)
{
    struct vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pCombinationCount = pCombinationCount;
    params.pCombinations = pCombinations;
    status = UNIX_CALL(vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV, &params);
    assert(!status && "vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV");
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceSurfaceCapabilities2KHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR *pSurfaceInfo, VkSurfaceCapabilities2KHR *pSurfaceCapabilities)
{
    struct vkGetPhysicalDeviceSurfaceCapabilities2KHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pSurfaceInfo = pSurfaceInfo;
    params.pSurfaceCapabilities = pSurfaceCapabilities;
    status = UNIX_CALL(vkGetPhysicalDeviceSurfaceCapabilities2KHR, &params);
    assert(!status && "vkGetPhysicalDeviceSurfaceCapabilities2KHR");
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR *pSurfaceCapabilities)
{
    struct vkGetPhysicalDeviceSurfaceCapabilitiesKHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.surface = surface;
    params.pSurfaceCapabilities = pSurfaceCapabilities;
    status = UNIX_CALL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR, &params);
    assert(!status && "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceSurfaceFormats2KHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR *pSurfaceInfo, uint32_t *pSurfaceFormatCount, VkSurfaceFormat2KHR *pSurfaceFormats)
{
    struct vkGetPhysicalDeviceSurfaceFormats2KHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pSurfaceInfo = pSurfaceInfo;
    params.pSurfaceFormatCount = pSurfaceFormatCount;
    params.pSurfaceFormats = pSurfaceFormats;
    status = UNIX_CALL(vkGetPhysicalDeviceSurfaceFormats2KHR, &params);
    assert(!status && "vkGetPhysicalDeviceSurfaceFormats2KHR");
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t *pSurfaceFormatCount, VkSurfaceFormatKHR *pSurfaceFormats)
{
    struct vkGetPhysicalDeviceSurfaceFormatsKHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.surface = surface;
    params.pSurfaceFormatCount = pSurfaceFormatCount;
    params.pSurfaceFormats = pSurfaceFormats;
    status = UNIX_CALL(vkGetPhysicalDeviceSurfaceFormatsKHR, &params);
    assert(!status && "vkGetPhysicalDeviceSurfaceFormatsKHR");
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t *pPresentModeCount, VkPresentModeKHR *pPresentModes)
{
    struct vkGetPhysicalDeviceSurfacePresentModesKHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.surface = surface;
    params.pPresentModeCount = pPresentModeCount;
    params.pPresentModes = pPresentModes;
    status = UNIX_CALL(vkGetPhysicalDeviceSurfacePresentModesKHR, &params);
    assert(!status && "vkGetPhysicalDeviceSurfacePresentModesKHR");
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, VkSurfaceKHR surface, VkBool32 *pSupported)
{
    struct vkGetPhysicalDeviceSurfaceSupportKHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.queueFamilyIndex = queueFamilyIndex;
    params.surface = surface;
    params.pSupported = pSupported;
    status = UNIX_CALL(vkGetPhysicalDeviceSurfaceSupportKHR, &params);
    assert(!status && "vkGetPhysicalDeviceSurfaceSupportKHR");
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceToolProperties(VkPhysicalDevice physicalDevice, uint32_t *pToolCount, VkPhysicalDeviceToolProperties *pToolProperties)
{
    struct vkGetPhysicalDeviceToolProperties_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pToolCount = pToolCount;
    params.pToolProperties = pToolProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceToolProperties, &params);
    assert(!status && "vkGetPhysicalDeviceToolProperties");
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceToolPropertiesEXT(VkPhysicalDevice physicalDevice, uint32_t *pToolCount, VkPhysicalDeviceToolProperties *pToolProperties)
{
    struct vkGetPhysicalDeviceToolPropertiesEXT_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pToolCount = pToolCount;
    params.pToolProperties = pToolProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceToolPropertiesEXT, &params);
    assert(!status && "vkGetPhysicalDeviceToolPropertiesEXT");
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceVideoCapabilitiesKHR(VkPhysicalDevice physicalDevice, const VkVideoProfileInfoKHR *pVideoProfile, VkVideoCapabilitiesKHR *pCapabilities)
{
    struct vkGetPhysicalDeviceVideoCapabilitiesKHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pVideoProfile = pVideoProfile;
    params.pCapabilities = pCapabilities;
    status = UNIX_CALL(vkGetPhysicalDeviceVideoCapabilitiesKHR, &params);
    assert(!status && "vkGetPhysicalDeviceVideoCapabilitiesKHR");
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceVideoEncodeQualityLevelPropertiesKHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceVideoEncodeQualityLevelInfoKHR *pQualityLevelInfo, VkVideoEncodeQualityLevelPropertiesKHR *pQualityLevelProperties)
{
    struct vkGetPhysicalDeviceVideoEncodeQualityLevelPropertiesKHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pQualityLevelInfo = pQualityLevelInfo;
    params.pQualityLevelProperties = pQualityLevelProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceVideoEncodeQualityLevelPropertiesKHR, &params);
    assert(!status && "vkGetPhysicalDeviceVideoEncodeQualityLevelPropertiesKHR");
    return params.result;
}

VkResult WINAPI vkGetPhysicalDeviceVideoFormatPropertiesKHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceVideoFormatInfoKHR *pVideoFormatInfo, uint32_t *pVideoFormatPropertyCount, VkVideoFormatPropertiesKHR *pVideoFormatProperties)
{
    struct vkGetPhysicalDeviceVideoFormatPropertiesKHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.pVideoFormatInfo = pVideoFormatInfo;
    params.pVideoFormatPropertyCount = pVideoFormatPropertyCount;
    params.pVideoFormatProperties = pVideoFormatProperties;
    status = UNIX_CALL(vkGetPhysicalDeviceVideoFormatPropertiesKHR, &params);
    assert(!status && "vkGetPhysicalDeviceVideoFormatPropertiesKHR");
    return params.result;
}

VkBool32 WINAPI vkGetPhysicalDeviceWin32PresentationSupportKHR(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex)
{
    struct vkGetPhysicalDeviceWin32PresentationSupportKHR_params params;
    NTSTATUS status;
    params.physicalDevice = physicalDevice;
    params.queueFamilyIndex = queueFamilyIndex;
    status = UNIX_CALL(vkGetPhysicalDeviceWin32PresentationSupportKHR, &params);
    assert(!status && "vkGetPhysicalDeviceWin32PresentationSupportKHR");
    return params.result;
}

VkResult WINAPI vkGetPipelineBinaryDataKHR(VkDevice device, const VkPipelineBinaryDataInfoKHR *pInfo, VkPipelineBinaryKeyKHR *pPipelineBinaryKey, size_t *pPipelineBinaryDataSize, void *pPipelineBinaryData)
{
    struct vkGetPipelineBinaryDataKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pPipelineBinaryKey = pPipelineBinaryKey;
    params.pPipelineBinaryDataSize = pPipelineBinaryDataSize;
    params.pPipelineBinaryData = pPipelineBinaryData;
    status = UNIX_CALL(vkGetPipelineBinaryDataKHR, &params);
    assert(!status && "vkGetPipelineBinaryDataKHR");
    return params.result;
}

VkResult WINAPI vkGetPipelineCacheData(VkDevice device, VkPipelineCache pipelineCache, size_t *pDataSize, void *pData)
{
    struct vkGetPipelineCacheData_params params;
    NTSTATUS status;
    params.device = device;
    params.pipelineCache = pipelineCache;
    params.pDataSize = pDataSize;
    params.pData = pData;
    status = UNIX_CALL(vkGetPipelineCacheData, &params);
    assert(!status && "vkGetPipelineCacheData");
    return params.result;
}

VkResult WINAPI vkGetPipelineExecutableInternalRepresentationsKHR(VkDevice device, const VkPipelineExecutableInfoKHR *pExecutableInfo, uint32_t *pInternalRepresentationCount, VkPipelineExecutableInternalRepresentationKHR *pInternalRepresentations)
{
    struct vkGetPipelineExecutableInternalRepresentationsKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pExecutableInfo = pExecutableInfo;
    params.pInternalRepresentationCount = pInternalRepresentationCount;
    params.pInternalRepresentations = pInternalRepresentations;
    status = UNIX_CALL(vkGetPipelineExecutableInternalRepresentationsKHR, &params);
    assert(!status && "vkGetPipelineExecutableInternalRepresentationsKHR");
    return params.result;
}

VkResult WINAPI vkGetPipelineExecutablePropertiesKHR(VkDevice device, const VkPipelineInfoKHR *pPipelineInfo, uint32_t *pExecutableCount, VkPipelineExecutablePropertiesKHR *pProperties)
{
    struct vkGetPipelineExecutablePropertiesKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pPipelineInfo = pPipelineInfo;
    params.pExecutableCount = pExecutableCount;
    params.pProperties = pProperties;
    status = UNIX_CALL(vkGetPipelineExecutablePropertiesKHR, &params);
    assert(!status && "vkGetPipelineExecutablePropertiesKHR");
    return params.result;
}

VkResult WINAPI vkGetPipelineExecutableStatisticsKHR(VkDevice device, const VkPipelineExecutableInfoKHR *pExecutableInfo, uint32_t *pStatisticCount, VkPipelineExecutableStatisticKHR *pStatistics)
{
    struct vkGetPipelineExecutableStatisticsKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pExecutableInfo = pExecutableInfo;
    params.pStatisticCount = pStatisticCount;
    params.pStatistics = pStatistics;
    status = UNIX_CALL(vkGetPipelineExecutableStatisticsKHR, &params);
    assert(!status && "vkGetPipelineExecutableStatisticsKHR");
    return params.result;
}

VkDeviceAddress WINAPI vkGetPipelineIndirectDeviceAddressNV(VkDevice device, const VkPipelineIndirectDeviceAddressInfoNV *pInfo)
{
    struct vkGetPipelineIndirectDeviceAddressNV_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkGetPipelineIndirectDeviceAddressNV, &params);
    assert(!status && "vkGetPipelineIndirectDeviceAddressNV");
    return params.result;
}

void WINAPI vkGetPipelineIndirectMemoryRequirementsNV(VkDevice device, const VkComputePipelineCreateInfo *pCreateInfo, VkMemoryRequirements2 *pMemoryRequirements)
{
    struct vkGetPipelineIndirectMemoryRequirementsNV_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pMemoryRequirements = pMemoryRequirements;
    status = UNIX_CALL(vkGetPipelineIndirectMemoryRequirementsNV, &params);
    assert(!status && "vkGetPipelineIndirectMemoryRequirementsNV");
}

VkResult WINAPI vkGetPipelineKeyKHR(VkDevice device, const VkPipelineCreateInfoKHR *pPipelineCreateInfo, VkPipelineBinaryKeyKHR *pPipelineKey)
{
    struct vkGetPipelineKeyKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pPipelineCreateInfo = pPipelineCreateInfo;
    params.pPipelineKey = pPipelineKey;
    status = UNIX_CALL(vkGetPipelineKeyKHR, &params);
    assert(!status && "vkGetPipelineKeyKHR");
    return params.result;
}

VkResult WINAPI vkGetPipelinePropertiesEXT(VkDevice device, const VkPipelineInfoEXT *pPipelineInfo, VkBaseOutStructure *pPipelineProperties)
{
    struct vkGetPipelinePropertiesEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pPipelineInfo = pPipelineInfo;
    params.pPipelineProperties = pPipelineProperties;
    status = UNIX_CALL(vkGetPipelinePropertiesEXT, &params);
    assert(!status && "vkGetPipelinePropertiesEXT");
    return params.result;
}

void WINAPI vkGetPrivateData(VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t *pData)
{
    struct vkGetPrivateData_params params;
    NTSTATUS status;
    params.device = device;
    params.objectType = objectType;
    params.objectHandle = objectHandle;
    params.privateDataSlot = privateDataSlot;
    params.pData = pData;
    status = UNIX_CALL(vkGetPrivateData, &params);
    assert(!status && "vkGetPrivateData");
}

void WINAPI vkGetPrivateDataEXT(VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t *pData)
{
    struct vkGetPrivateDataEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.objectType = objectType;
    params.objectHandle = objectHandle;
    params.privateDataSlot = privateDataSlot;
    params.pData = pData;
    status = UNIX_CALL(vkGetPrivateDataEXT, &params);
    assert(!status && "vkGetPrivateDataEXT");
}

VkResult WINAPI vkGetQueryPoolResults(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void *pData, VkDeviceSize stride, VkQueryResultFlags flags)
{
    struct vkGetQueryPoolResults_params params;
    NTSTATUS status;
    params.device = device;
    params.queryPool = queryPool;
    params.firstQuery = firstQuery;
    params.queryCount = queryCount;
    params.dataSize = dataSize;
    params.pData = pData;
    params.stride = stride;
    params.flags = flags;
    status = UNIX_CALL(vkGetQueryPoolResults, &params);
    assert(!status && "vkGetQueryPoolResults");
    return params.result;
}

void WINAPI vkGetQueueCheckpointData2NV(VkQueue queue, uint32_t *pCheckpointDataCount, VkCheckpointData2NV *pCheckpointData)
{
    struct vkGetQueueCheckpointData2NV_params params;
    NTSTATUS status;
    params.queue = queue;
    params.pCheckpointDataCount = pCheckpointDataCount;
    params.pCheckpointData = pCheckpointData;
    status = UNIX_CALL(vkGetQueueCheckpointData2NV, &params);
    assert(!status && "vkGetQueueCheckpointData2NV");
}

void WINAPI vkGetQueueCheckpointDataNV(VkQueue queue, uint32_t *pCheckpointDataCount, VkCheckpointDataNV *pCheckpointData)
{
    struct vkGetQueueCheckpointDataNV_params params;
    NTSTATUS status;
    params.queue = queue;
    params.pCheckpointDataCount = pCheckpointDataCount;
    params.pCheckpointData = pCheckpointData;
    status = UNIX_CALL(vkGetQueueCheckpointDataNV, &params);
    assert(!status && "vkGetQueueCheckpointDataNV");
}

VkResult WINAPI vkGetRayTracingCaptureReplayShaderGroupHandlesKHR(VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void *pData)
{
    struct vkGetRayTracingCaptureReplayShaderGroupHandlesKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pipeline = pipeline;
    params.firstGroup = firstGroup;
    params.groupCount = groupCount;
    params.dataSize = dataSize;
    params.pData = pData;
    status = UNIX_CALL(vkGetRayTracingCaptureReplayShaderGroupHandlesKHR, &params);
    assert(!status && "vkGetRayTracingCaptureReplayShaderGroupHandlesKHR");
    return params.result;
}

VkResult WINAPI vkGetRayTracingShaderGroupHandlesKHR(VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void *pData)
{
    struct vkGetRayTracingShaderGroupHandlesKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pipeline = pipeline;
    params.firstGroup = firstGroup;
    params.groupCount = groupCount;
    params.dataSize = dataSize;
    params.pData = pData;
    status = UNIX_CALL(vkGetRayTracingShaderGroupHandlesKHR, &params);
    assert(!status && "vkGetRayTracingShaderGroupHandlesKHR");
    return params.result;
}

VkResult WINAPI vkGetRayTracingShaderGroupHandlesNV(VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void *pData)
{
    struct vkGetRayTracingShaderGroupHandlesNV_params params;
    NTSTATUS status;
    params.device = device;
    params.pipeline = pipeline;
    params.firstGroup = firstGroup;
    params.groupCount = groupCount;
    params.dataSize = dataSize;
    params.pData = pData;
    status = UNIX_CALL(vkGetRayTracingShaderGroupHandlesNV, &params);
    assert(!status && "vkGetRayTracingShaderGroupHandlesNV");
    return params.result;
}

VkDeviceSize WINAPI vkGetRayTracingShaderGroupStackSizeKHR(VkDevice device, VkPipeline pipeline, uint32_t group, VkShaderGroupShaderKHR groupShader)
{
    struct vkGetRayTracingShaderGroupStackSizeKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pipeline = pipeline;
    params.group = group;
    params.groupShader = groupShader;
    status = UNIX_CALL(vkGetRayTracingShaderGroupStackSizeKHR, &params);
    assert(!status && "vkGetRayTracingShaderGroupStackSizeKHR");
    return params.result;
}

void WINAPI vkGetRenderAreaGranularity(VkDevice device, VkRenderPass renderPass, VkExtent2D *pGranularity)
{
    struct vkGetRenderAreaGranularity_params params;
    NTSTATUS status;
    params.device = device;
    params.renderPass = renderPass;
    params.pGranularity = pGranularity;
    status = UNIX_CALL(vkGetRenderAreaGranularity, &params);
    assert(!status && "vkGetRenderAreaGranularity");
}

void WINAPI vkGetRenderingAreaGranularity(VkDevice device, const VkRenderingAreaInfo *pRenderingAreaInfo, VkExtent2D *pGranularity)
{
    struct vkGetRenderingAreaGranularity_params params;
    NTSTATUS status;
    params.device = device;
    params.pRenderingAreaInfo = pRenderingAreaInfo;
    params.pGranularity = pGranularity;
    status = UNIX_CALL(vkGetRenderingAreaGranularity, &params);
    assert(!status && "vkGetRenderingAreaGranularity");
}

void WINAPI vkGetRenderingAreaGranularityKHR(VkDevice device, const VkRenderingAreaInfo *pRenderingAreaInfo, VkExtent2D *pGranularity)
{
    struct vkGetRenderingAreaGranularityKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pRenderingAreaInfo = pRenderingAreaInfo;
    params.pGranularity = pGranularity;
    status = UNIX_CALL(vkGetRenderingAreaGranularityKHR, &params);
    assert(!status && "vkGetRenderingAreaGranularityKHR");
}

VkResult WINAPI vkGetSamplerOpaqueCaptureDescriptorDataEXT(VkDevice device, const VkSamplerCaptureDescriptorDataInfoEXT *pInfo, void *pData)
{
    struct vkGetSamplerOpaqueCaptureDescriptorDataEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pData = pData;
    status = UNIX_CALL(vkGetSamplerOpaqueCaptureDescriptorDataEXT, &params);
    assert(!status && "vkGetSamplerOpaqueCaptureDescriptorDataEXT");
    return params.result;
}

VkResult WINAPI vkGetSemaphoreCounterValue(VkDevice device, VkSemaphore semaphore, uint64_t *pValue)
{
    struct vkGetSemaphoreCounterValue_params params;
    NTSTATUS status;
    params.device = device;
    params.semaphore = semaphore;
    params.pValue = pValue;
    status = UNIX_CALL(vkGetSemaphoreCounterValue, &params);
    assert(!status && "vkGetSemaphoreCounterValue");
    return params.result;
}

VkResult WINAPI vkGetSemaphoreCounterValueKHR(VkDevice device, VkSemaphore semaphore, uint64_t *pValue)
{
    struct vkGetSemaphoreCounterValueKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.semaphore = semaphore;
    params.pValue = pValue;
    status = UNIX_CALL(vkGetSemaphoreCounterValueKHR, &params);
    assert(!status && "vkGetSemaphoreCounterValueKHR");
    return params.result;
}

VkResult WINAPI vkGetShaderBinaryDataEXT(VkDevice device, VkShaderEXT shader, size_t *pDataSize, void *pData)
{
    struct vkGetShaderBinaryDataEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.shader = shader;
    params.pDataSize = pDataSize;
    params.pData = pData;
    status = UNIX_CALL(vkGetShaderBinaryDataEXT, &params);
    assert(!status && "vkGetShaderBinaryDataEXT");
    return params.result;
}

VkResult WINAPI vkGetShaderInfoAMD(VkDevice device, VkPipeline pipeline, VkShaderStageFlagBits shaderStage, VkShaderInfoTypeAMD infoType, size_t *pInfoSize, void *pInfo)
{
    struct vkGetShaderInfoAMD_params params;
    NTSTATUS status;
    params.device = device;
    params.pipeline = pipeline;
    params.shaderStage = shaderStage;
    params.infoType = infoType;
    params.pInfoSize = pInfoSize;
    params.pInfo = pInfo;
    status = UNIX_CALL(vkGetShaderInfoAMD, &params);
    assert(!status && "vkGetShaderInfoAMD");
    return params.result;
}

void WINAPI vkGetShaderModuleCreateInfoIdentifierEXT(VkDevice device, const VkShaderModuleCreateInfo *pCreateInfo, VkShaderModuleIdentifierEXT *pIdentifier)
{
    struct vkGetShaderModuleCreateInfoIdentifierEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pCreateInfo = pCreateInfo;
    params.pIdentifier = pIdentifier;
    status = UNIX_CALL(vkGetShaderModuleCreateInfoIdentifierEXT, &params);
    assert(!status && "vkGetShaderModuleCreateInfoIdentifierEXT");
}

void WINAPI vkGetShaderModuleIdentifierEXT(VkDevice device, VkShaderModule shaderModule, VkShaderModuleIdentifierEXT *pIdentifier)
{
    struct vkGetShaderModuleIdentifierEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.shaderModule = shaderModule;
    params.pIdentifier = pIdentifier;
    status = UNIX_CALL(vkGetShaderModuleIdentifierEXT, &params);
    assert(!status && "vkGetShaderModuleIdentifierEXT");
}

VkResult WINAPI vkGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain, uint32_t *pSwapchainImageCount, VkImage *pSwapchainImages)
{
    struct vkGetSwapchainImagesKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.swapchain = swapchain;
    params.pSwapchainImageCount = pSwapchainImageCount;
    params.pSwapchainImages = pSwapchainImages;
    status = UNIX_CALL(vkGetSwapchainImagesKHR, &params);
    assert(!status && "vkGetSwapchainImagesKHR");
    return params.result;
}

VkResult WINAPI vkGetValidationCacheDataEXT(VkDevice device, VkValidationCacheEXT validationCache, size_t *pDataSize, void *pData)
{
    struct vkGetValidationCacheDataEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.validationCache = validationCache;
    params.pDataSize = pDataSize;
    params.pData = pData;
    status = UNIX_CALL(vkGetValidationCacheDataEXT, &params);
    assert(!status && "vkGetValidationCacheDataEXT");
    return params.result;
}

VkResult WINAPI vkGetVideoSessionMemoryRequirementsKHR(VkDevice device, VkVideoSessionKHR videoSession, uint32_t *pMemoryRequirementsCount, VkVideoSessionMemoryRequirementsKHR *pMemoryRequirements)
{
    struct vkGetVideoSessionMemoryRequirementsKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.videoSession = videoSession;
    params.pMemoryRequirementsCount = pMemoryRequirementsCount;
    params.pMemoryRequirements = pMemoryRequirements;
    status = UNIX_CALL(vkGetVideoSessionMemoryRequirementsKHR, &params);
    assert(!status && "vkGetVideoSessionMemoryRequirementsKHR");
    return params.result;
}

VkResult WINAPI vkInvalidateMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange *pMemoryRanges)
{
    struct vkInvalidateMappedMemoryRanges_params params;
    NTSTATUS status;
    params.device = device;
    params.memoryRangeCount = memoryRangeCount;
    params.pMemoryRanges = pMemoryRanges;
    status = UNIX_CALL(vkInvalidateMappedMemoryRanges, &params);
    assert(!status && "vkInvalidateMappedMemoryRanges");
    return params.result;
}

VkResult WINAPI vkLatencySleepNV(VkDevice device, VkSwapchainKHR swapchain, const VkLatencySleepInfoNV *pSleepInfo)
{
    struct vkLatencySleepNV_params params;
    NTSTATUS status;
    params.device = device;
    params.swapchain = swapchain;
    params.pSleepInfo = pSleepInfo;
    status = UNIX_CALL(vkLatencySleepNV, &params);
    assert(!status && "vkLatencySleepNV");
    return params.result;
}

VkResult WINAPI vkMapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void **ppData)
{
    struct vkMapMemory_params params;
    NTSTATUS status;
    params.device = device;
    params.memory = memory;
    params.offset = offset;
    params.size = size;
    params.flags = flags;
    params.ppData = ppData;
    status = UNIX_CALL(vkMapMemory, &params);
    assert(!status && "vkMapMemory");
    return params.result;
}

VkResult WINAPI vkMapMemory2(VkDevice device, const VkMemoryMapInfo *pMemoryMapInfo, void **ppData)
{
    struct vkMapMemory2_params params;
    NTSTATUS status;
    params.device = device;
    params.pMemoryMapInfo = pMemoryMapInfo;
    params.ppData = ppData;
    status = UNIX_CALL(vkMapMemory2, &params);
    assert(!status && "vkMapMemory2");
    return params.result;
}

VkResult WINAPI vkMapMemory2KHR(VkDevice device, const VkMemoryMapInfo *pMemoryMapInfo, void **ppData)
{
    struct vkMapMemory2KHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pMemoryMapInfo = pMemoryMapInfo;
    params.ppData = ppData;
    status = UNIX_CALL(vkMapMemory2KHR, &params);
    assert(!status && "vkMapMemory2KHR");
    return params.result;
}

VkResult WINAPI vkMergePipelineCaches(VkDevice device, VkPipelineCache dstCache, uint32_t srcCacheCount, const VkPipelineCache *pSrcCaches)
{
    struct vkMergePipelineCaches_params params;
    NTSTATUS status;
    params.device = device;
    params.dstCache = dstCache;
    params.srcCacheCount = srcCacheCount;
    params.pSrcCaches = pSrcCaches;
    status = UNIX_CALL(vkMergePipelineCaches, &params);
    assert(!status && "vkMergePipelineCaches");
    return params.result;
}

VkResult WINAPI vkMergeValidationCachesEXT(VkDevice device, VkValidationCacheEXT dstCache, uint32_t srcCacheCount, const VkValidationCacheEXT *pSrcCaches)
{
    struct vkMergeValidationCachesEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.dstCache = dstCache;
    params.srcCacheCount = srcCacheCount;
    params.pSrcCaches = pSrcCaches;
    status = UNIX_CALL(vkMergeValidationCachesEXT, &params);
    assert(!status && "vkMergeValidationCachesEXT");
    return params.result;
}

void WINAPI vkQueueBeginDebugUtilsLabelEXT(VkQueue queue, const VkDebugUtilsLabelEXT *pLabelInfo)
{
    struct vkQueueBeginDebugUtilsLabelEXT_params params;
    NTSTATUS status;
    params.queue = queue;
    params.pLabelInfo = pLabelInfo;
    status = UNIX_CALL(vkQueueBeginDebugUtilsLabelEXT, &params);
    assert(!status && "vkQueueBeginDebugUtilsLabelEXT");
}

VkResult WINAPI vkQueueBindSparse(VkQueue queue, uint32_t bindInfoCount, const VkBindSparseInfo *pBindInfo, VkFence fence)
{
    struct vkQueueBindSparse_params params;
    NTSTATUS status;
    params.queue = queue;
    params.bindInfoCount = bindInfoCount;
    params.pBindInfo = pBindInfo;
    params.fence = fence;
    status = UNIX_CALL(vkQueueBindSparse, &params);
    assert(!status && "vkQueueBindSparse");
    return params.result;
}

void WINAPI vkQueueEndDebugUtilsLabelEXT(VkQueue queue)
{
    struct vkQueueEndDebugUtilsLabelEXT_params params;
    NTSTATUS status;
    params.queue = queue;
    status = UNIX_CALL(vkQueueEndDebugUtilsLabelEXT, &params);
    assert(!status && "vkQueueEndDebugUtilsLabelEXT");
}

void WINAPI vkQueueInsertDebugUtilsLabelEXT(VkQueue queue, const VkDebugUtilsLabelEXT *pLabelInfo)
{
    struct vkQueueInsertDebugUtilsLabelEXT_params params;
    NTSTATUS status;
    params.queue = queue;
    params.pLabelInfo = pLabelInfo;
    status = UNIX_CALL(vkQueueInsertDebugUtilsLabelEXT, &params);
    assert(!status && "vkQueueInsertDebugUtilsLabelEXT");
}

void WINAPI vkQueueNotifyOutOfBandNV(VkQueue queue, const VkOutOfBandQueueTypeInfoNV *pQueueTypeInfo)
{
    struct vkQueueNotifyOutOfBandNV_params params;
    NTSTATUS status;
    params.queue = queue;
    params.pQueueTypeInfo = pQueueTypeInfo;
    status = UNIX_CALL(vkQueueNotifyOutOfBandNV, &params);
    assert(!status && "vkQueueNotifyOutOfBandNV");
}

VkResult WINAPI vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *pPresentInfo)
{
    struct vkQueuePresentKHR_params params;
    NTSTATUS status;
    params.queue = queue;
    params.pPresentInfo = pPresentInfo;
    status = UNIX_CALL(vkQueuePresentKHR, &params);
    assert(!status && "vkQueuePresentKHR");
    return params.result;
}

VkResult WINAPI vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo *pSubmits, VkFence fence)
{
    struct vkQueueSubmit_params params;
    NTSTATUS status;
    params.queue = queue;
    params.submitCount = submitCount;
    params.pSubmits = pSubmits;
    params.fence = fence;
    status = UNIX_CALL(vkQueueSubmit, &params);
    assert(!status && "vkQueueSubmit");
    return params.result;
}

VkResult WINAPI vkQueueSubmit2(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2 *pSubmits, VkFence fence)
{
    struct vkQueueSubmit2_params params;
    NTSTATUS status;
    params.queue = queue;
    params.submitCount = submitCount;
    params.pSubmits = pSubmits;
    params.fence = fence;
    status = UNIX_CALL(vkQueueSubmit2, &params);
    assert(!status && "vkQueueSubmit2");
    return params.result;
}

VkResult WINAPI vkQueueSubmit2KHR(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2 *pSubmits, VkFence fence)
{
    struct vkQueueSubmit2KHR_params params;
    NTSTATUS status;
    params.queue = queue;
    params.submitCount = submitCount;
    params.pSubmits = pSubmits;
    params.fence = fence;
    status = UNIX_CALL(vkQueueSubmit2KHR, &params);
    assert(!status && "vkQueueSubmit2KHR");
    return params.result;
}

VkResult WINAPI vkQueueWaitIdle(VkQueue queue)
{
    struct vkQueueWaitIdle_params params;
    NTSTATUS status;
    params.queue = queue;
    status = UNIX_CALL(vkQueueWaitIdle, &params);
    assert(!status && "vkQueueWaitIdle");
    return params.result;
}

VkResult WINAPI vkReleaseCapturedPipelineDataKHR(VkDevice device, const VkReleaseCapturedPipelineDataInfoKHR *pInfo, const VkAllocationCallbacks *pAllocator)
{
    struct vkReleaseCapturedPipelineDataKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pInfo = pInfo;
    params.pAllocator = pAllocator;
    status = UNIX_CALL(vkReleaseCapturedPipelineDataKHR, &params);
    assert(!status && "vkReleaseCapturedPipelineDataKHR");
    return params.result;
}

void WINAPI vkReleaseProfilingLockKHR(VkDevice device)
{
    struct vkReleaseProfilingLockKHR_params params;
    NTSTATUS status;
    params.device = device;
    status = UNIX_CALL(vkReleaseProfilingLockKHR, &params);
    assert(!status && "vkReleaseProfilingLockKHR");
}

VkResult WINAPI vkReleaseSwapchainImagesEXT(VkDevice device, const VkReleaseSwapchainImagesInfoEXT *pReleaseInfo)
{
    struct vkReleaseSwapchainImagesEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pReleaseInfo = pReleaseInfo;
    status = UNIX_CALL(vkReleaseSwapchainImagesEXT, &params);
    assert(!status && "vkReleaseSwapchainImagesEXT");
    return params.result;
}

VkResult WINAPI vkResetCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags)
{
    struct vkResetCommandBuffer_params params;
    NTSTATUS status;
    params.commandBuffer = commandBuffer;
    params.flags = flags;
    status = UNIX_CALL(vkResetCommandBuffer, &params);
    assert(!status && "vkResetCommandBuffer");
    return params.result;
}

VkResult WINAPI vkResetCommandPool(VkDevice device, VkCommandPool commandPool, VkCommandPoolResetFlags flags)
{
    struct vkResetCommandPool_params params;
    NTSTATUS status;
    params.device = device;
    params.commandPool = commandPool;
    params.flags = flags;
    status = UNIX_CALL(vkResetCommandPool, &params);
    assert(!status && "vkResetCommandPool");
    return params.result;
}

VkResult WINAPI vkResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags)
{
    struct vkResetDescriptorPool_params params;
    NTSTATUS status;
    params.device = device;
    params.descriptorPool = descriptorPool;
    params.flags = flags;
    status = UNIX_CALL(vkResetDescriptorPool, &params);
    assert(!status && "vkResetDescriptorPool");
    return params.result;
}

VkResult WINAPI vkResetEvent(VkDevice device, VkEvent event)
{
    struct vkResetEvent_params params;
    NTSTATUS status;
    params.device = device;
    params.event = event;
    status = UNIX_CALL(vkResetEvent, &params);
    assert(!status && "vkResetEvent");
    return params.result;
}

VkResult WINAPI vkResetFences(VkDevice device, uint32_t fenceCount, const VkFence *pFences)
{
    struct vkResetFences_params params;
    NTSTATUS status;
    params.device = device;
    params.fenceCount = fenceCount;
    params.pFences = pFences;
    status = UNIX_CALL(vkResetFences, &params);
    assert(!status && "vkResetFences");
    return params.result;
}

void WINAPI vkResetQueryPool(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount)
{
    struct vkResetQueryPool_params params;
    NTSTATUS status;
    params.device = device;
    params.queryPool = queryPool;
    params.firstQuery = firstQuery;
    params.queryCount = queryCount;
    status = UNIX_CALL(vkResetQueryPool, &params);
    assert(!status && "vkResetQueryPool");
}

void WINAPI vkResetQueryPoolEXT(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount)
{
    struct vkResetQueryPoolEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.queryPool = queryPool;
    params.firstQuery = firstQuery;
    params.queryCount = queryCount;
    status = UNIX_CALL(vkResetQueryPoolEXT, &params);
    assert(!status && "vkResetQueryPoolEXT");
}

VkResult WINAPI vkSetDebugUtilsObjectNameEXT(VkDevice device, const VkDebugUtilsObjectNameInfoEXT *pNameInfo)
{
    struct vkSetDebugUtilsObjectNameEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pNameInfo = pNameInfo;
    status = UNIX_CALL(vkSetDebugUtilsObjectNameEXT, &params);
    assert(!status && "vkSetDebugUtilsObjectNameEXT");
    return params.result;
}

VkResult WINAPI vkSetDebugUtilsObjectTagEXT(VkDevice device, const VkDebugUtilsObjectTagInfoEXT *pTagInfo)
{
    struct vkSetDebugUtilsObjectTagEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.pTagInfo = pTagInfo;
    status = UNIX_CALL(vkSetDebugUtilsObjectTagEXT, &params);
    assert(!status && "vkSetDebugUtilsObjectTagEXT");
    return params.result;
}

void WINAPI vkSetDeviceMemoryPriorityEXT(VkDevice device, VkDeviceMemory memory, float priority)
{
    struct vkSetDeviceMemoryPriorityEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.memory = memory;
    params.priority = priority;
    status = UNIX_CALL(vkSetDeviceMemoryPriorityEXT, &params);
    assert(!status && "vkSetDeviceMemoryPriorityEXT");
}

VkResult WINAPI vkSetEvent(VkDevice device, VkEvent event)
{
    struct vkSetEvent_params params;
    NTSTATUS status;
    params.device = device;
    params.event = event;
    status = UNIX_CALL(vkSetEvent, &params);
    assert(!status && "vkSetEvent");
    return params.result;
}

void WINAPI vkSetHdrMetadataEXT(VkDevice device, uint32_t swapchainCount, const VkSwapchainKHR *pSwapchains, const VkHdrMetadataEXT *pMetadata)
{
    struct vkSetHdrMetadataEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.swapchainCount = swapchainCount;
    params.pSwapchains = pSwapchains;
    params.pMetadata = pMetadata;
    status = UNIX_CALL(vkSetHdrMetadataEXT, &params);
    assert(!status && "vkSetHdrMetadataEXT");
}

void WINAPI vkSetLatencyMarkerNV(VkDevice device, VkSwapchainKHR swapchain, const VkSetLatencyMarkerInfoNV *pLatencyMarkerInfo)
{
    struct vkSetLatencyMarkerNV_params params;
    NTSTATUS status;
    params.device = device;
    params.swapchain = swapchain;
    params.pLatencyMarkerInfo = pLatencyMarkerInfo;
    status = UNIX_CALL(vkSetLatencyMarkerNV, &params);
    assert(!status && "vkSetLatencyMarkerNV");
}

VkResult WINAPI vkSetLatencySleepModeNV(VkDevice device, VkSwapchainKHR swapchain, const VkLatencySleepModeInfoNV *pSleepModeInfo)
{
    struct vkSetLatencySleepModeNV_params params;
    NTSTATUS status;
    params.device = device;
    params.swapchain = swapchain;
    params.pSleepModeInfo = pSleepModeInfo;
    status = UNIX_CALL(vkSetLatencySleepModeNV, &params);
    assert(!status && "vkSetLatencySleepModeNV");
    return params.result;
}

VkResult WINAPI vkSetPrivateData(VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t data)
{
    struct vkSetPrivateData_params params;
    NTSTATUS status;
    params.device = device;
    params.objectType = objectType;
    params.objectHandle = objectHandle;
    params.privateDataSlot = privateDataSlot;
    params.data = data;
    status = UNIX_CALL(vkSetPrivateData, &params);
    assert(!status && "vkSetPrivateData");
    return params.result;
}

VkResult WINAPI vkSetPrivateDataEXT(VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t data)
{
    struct vkSetPrivateDataEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.objectType = objectType;
    params.objectHandle = objectHandle;
    params.privateDataSlot = privateDataSlot;
    params.data = data;
    status = UNIX_CALL(vkSetPrivateDataEXT, &params);
    assert(!status && "vkSetPrivateDataEXT");
    return params.result;
}

VkResult WINAPI vkSignalSemaphore(VkDevice device, const VkSemaphoreSignalInfo *pSignalInfo)
{
    struct vkSignalSemaphore_params params;
    NTSTATUS status;
    params.device = device;
    params.pSignalInfo = pSignalInfo;
    status = UNIX_CALL(vkSignalSemaphore, &params);
    assert(!status && "vkSignalSemaphore");
    return params.result;
}

VkResult WINAPI vkSignalSemaphoreKHR(VkDevice device, const VkSemaphoreSignalInfo *pSignalInfo)
{
    struct vkSignalSemaphoreKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pSignalInfo = pSignalInfo;
    status = UNIX_CALL(vkSignalSemaphoreKHR, &params);
    assert(!status && "vkSignalSemaphoreKHR");
    return params.result;
}

void WINAPI vkSubmitDebugUtilsMessageEXT(VkInstance instance, VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData)
{
    struct vkSubmitDebugUtilsMessageEXT_params params;
    NTSTATUS status;
    params.instance = instance;
    params.messageSeverity = messageSeverity;
    params.messageTypes = messageTypes;
    params.pCallbackData = pCallbackData;
    status = UNIX_CALL(vkSubmitDebugUtilsMessageEXT, &params);
    assert(!status && "vkSubmitDebugUtilsMessageEXT");
}

VkResult WINAPI vkTransitionImageLayout(VkDevice device, uint32_t transitionCount, const VkHostImageLayoutTransitionInfo *pTransitions)
{
    struct vkTransitionImageLayout_params params;
    NTSTATUS status;
    params.device = device;
    params.transitionCount = transitionCount;
    params.pTransitions = pTransitions;
    status = UNIX_CALL(vkTransitionImageLayout, &params);
    assert(!status && "vkTransitionImageLayout");
    return params.result;
}

VkResult WINAPI vkTransitionImageLayoutEXT(VkDevice device, uint32_t transitionCount, const VkHostImageLayoutTransitionInfo *pTransitions)
{
    struct vkTransitionImageLayoutEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.transitionCount = transitionCount;
    params.pTransitions = pTransitions;
    status = UNIX_CALL(vkTransitionImageLayoutEXT, &params);
    assert(!status && "vkTransitionImageLayoutEXT");
    return params.result;
}

void WINAPI vkTrimCommandPool(VkDevice device, VkCommandPool commandPool, VkCommandPoolTrimFlags flags)
{
    struct vkTrimCommandPool_params params;
    NTSTATUS status;
    params.device = device;
    params.commandPool = commandPool;
    params.flags = flags;
    status = UNIX_CALL(vkTrimCommandPool, &params);
    assert(!status && "vkTrimCommandPool");
}

void WINAPI vkTrimCommandPoolKHR(VkDevice device, VkCommandPool commandPool, VkCommandPoolTrimFlags flags)
{
    struct vkTrimCommandPoolKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.commandPool = commandPool;
    params.flags = flags;
    status = UNIX_CALL(vkTrimCommandPoolKHR, &params);
    assert(!status && "vkTrimCommandPoolKHR");
}

void WINAPI vkUnmapMemory(VkDevice device, VkDeviceMemory memory)
{
    struct vkUnmapMemory_params params;
    NTSTATUS status;
    params.device = device;
    params.memory = memory;
    status = UNIX_CALL(vkUnmapMemory, &params);
    assert(!status && "vkUnmapMemory");
}

VkResult WINAPI vkUnmapMemory2(VkDevice device, const VkMemoryUnmapInfo *pMemoryUnmapInfo)
{
    struct vkUnmapMemory2_params params;
    NTSTATUS status;
    params.device = device;
    params.pMemoryUnmapInfo = pMemoryUnmapInfo;
    status = UNIX_CALL(vkUnmapMemory2, &params);
    assert(!status && "vkUnmapMemory2");
    return params.result;
}

VkResult WINAPI vkUnmapMemory2KHR(VkDevice device, const VkMemoryUnmapInfo *pMemoryUnmapInfo)
{
    struct vkUnmapMemory2KHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pMemoryUnmapInfo = pMemoryUnmapInfo;
    status = UNIX_CALL(vkUnmapMemory2KHR, &params);
    assert(!status && "vkUnmapMemory2KHR");
    return params.result;
}

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 4
C_ASSERT( unix_vkUpdateDescriptorSetWithTemplate == 617 );
VK_FAST_THUNK( vkUpdateDescriptorSetWithTemplate, "617", 4 )
#else
void WINAPI vkUpdateDescriptorSetWithTemplate(VkDevice device, VkDescriptorSet descriptorSet, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void *pData)
{
    struct vkUpdateDescriptorSetWithTemplate_params params;
    params.device = device;
    params.descriptorSet = descriptorSet;
    params.descriptorUpdateTemplate = descriptorUpdateTemplate;
    params.pData = pData;
    UNIX_CALL(vkUpdateDescriptorSetWithTemplate, &params);
}
#endif /* VK_FAST_THUNK */

void WINAPI vkUpdateDescriptorSetWithTemplateKHR(VkDevice device, VkDescriptorSet descriptorSet, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void *pData)
{
    struct vkUpdateDescriptorSetWithTemplateKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.descriptorSet = descriptorSet;
    params.descriptorUpdateTemplate = descriptorUpdateTemplate;
    params.pData = pData;
    status = UNIX_CALL(vkUpdateDescriptorSetWithTemplateKHR, &params);
    assert(!status && "vkUpdateDescriptorSetWithTemplateKHR");
}

#if defined(VK_FAST_THUNK) && VK_FAST_THUNK_MAX_ARGS >= 5
C_ASSERT( unix_vkUpdateDescriptorSets == 619 );
VK_FAST_THUNK( vkUpdateDescriptorSets, "619", 5 )
#else
void WINAPI vkUpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet *pDescriptorWrites, uint32_t descriptorCopyCount, const VkCopyDescriptorSet *pDescriptorCopies)
{
    struct vkUpdateDescriptorSets_params params;
    params.device = device;
    params.descriptorWriteCount = descriptorWriteCount;
    params.pDescriptorWrites = pDescriptorWrites;
    params.descriptorCopyCount = descriptorCopyCount;
    params.pDescriptorCopies = pDescriptorCopies;
    UNIX_CALL(vkUpdateDescriptorSets, &params);
}
#endif /* VK_FAST_THUNK */

void WINAPI vkUpdateIndirectExecutionSetPipelineEXT(VkDevice device, VkIndirectExecutionSetEXT indirectExecutionSet, uint32_t executionSetWriteCount, const VkWriteIndirectExecutionSetPipelineEXT *pExecutionSetWrites)
{
    struct vkUpdateIndirectExecutionSetPipelineEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.indirectExecutionSet = indirectExecutionSet;
    params.executionSetWriteCount = executionSetWriteCount;
    params.pExecutionSetWrites = pExecutionSetWrites;
    status = UNIX_CALL(vkUpdateIndirectExecutionSetPipelineEXT, &params);
    assert(!status && "vkUpdateIndirectExecutionSetPipelineEXT");
}

void WINAPI vkUpdateIndirectExecutionSetShaderEXT(VkDevice device, VkIndirectExecutionSetEXT indirectExecutionSet, uint32_t executionSetWriteCount, const VkWriteIndirectExecutionSetShaderEXT *pExecutionSetWrites)
{
    struct vkUpdateIndirectExecutionSetShaderEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.indirectExecutionSet = indirectExecutionSet;
    params.executionSetWriteCount = executionSetWriteCount;
    params.pExecutionSetWrites = pExecutionSetWrites;
    status = UNIX_CALL(vkUpdateIndirectExecutionSetShaderEXT, &params);
    assert(!status && "vkUpdateIndirectExecutionSetShaderEXT");
}

VkResult WINAPI vkUpdateVideoSessionParametersKHR(VkDevice device, VkVideoSessionParametersKHR videoSessionParameters, const VkVideoSessionParametersUpdateInfoKHR *pUpdateInfo)
{
    struct vkUpdateVideoSessionParametersKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.videoSessionParameters = videoSessionParameters;
    params.pUpdateInfo = pUpdateInfo;
    status = UNIX_CALL(vkUpdateVideoSessionParametersKHR, &params);
    assert(!status && "vkUpdateVideoSessionParametersKHR");
    return params.result;
}

VkResult WINAPI vkWaitForFences(VkDevice device, uint32_t fenceCount, const VkFence *pFences, VkBool32 waitAll, uint64_t timeout)
{
    struct vkWaitForFences_params params;
    NTSTATUS status;
    params.device = device;
    params.fenceCount = fenceCount;
    params.pFences = pFences;
    params.waitAll = waitAll;
    params.timeout = timeout;
    status = UNIX_CALL(vkWaitForFences, &params);
    assert(!status && "vkWaitForFences");
    return params.result;
}

VkResult WINAPI vkWaitForPresentKHR(VkDevice device, VkSwapchainKHR swapchain, uint64_t presentId, uint64_t timeout)
{
    struct vkWaitForPresentKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.swapchain = swapchain;
    params.presentId = presentId;
    params.timeout = timeout;
    status = UNIX_CALL(vkWaitForPresentKHR, &params);
    assert(!status && "vkWaitForPresentKHR");
    return params.result;
}

VkResult WINAPI vkWaitSemaphores(VkDevice device, const VkSemaphoreWaitInfo *pWaitInfo, uint64_t timeout)
{
    struct vkWaitSemaphores_params params;
    NTSTATUS status;
    params.device = device;
    params.pWaitInfo = pWaitInfo;
    params.timeout = timeout;
    status = UNIX_CALL(vkWaitSemaphores, &params);
    assert(!status && "vkWaitSemaphores");
    return params.result;
}

VkResult WINAPI vkWaitSemaphoresKHR(VkDevice device, const VkSemaphoreWaitInfo *pWaitInfo, uint64_t timeout)
{
    struct vkWaitSemaphoresKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.pWaitInfo = pWaitInfo;
    params.timeout = timeout;
    status = UNIX_CALL(vkWaitSemaphoresKHR, &params);
    assert(!status && "vkWaitSemaphoresKHR");
    return params.result;
}

VkResult WINAPI vkWriteAccelerationStructuresPropertiesKHR(VkDevice device, uint32_t accelerationStructureCount, const VkAccelerationStructureKHR *pAccelerationStructures, VkQueryType queryType, size_t dataSize, void *pData, size_t stride)
{
    struct vkWriteAccelerationStructuresPropertiesKHR_params params;
    NTSTATUS status;
    params.device = device;
    params.accelerationStructureCount = accelerationStructureCount;
    params.pAccelerationStructures = pAccelerationStructures;
    params.queryType = queryType;
    params.dataSize = dataSize;
    params.pData = pData;
    params.stride = stride;
    status = UNIX_CALL(vkWriteAccelerationStructuresPropertiesKHR, &params);
    assert(!status && "vkWriteAccelerationStructuresPropertiesKHR");
    return params.result;
}

VkResult WINAPI vkWriteMicromapsPropertiesEXT(VkDevice device, uint32_t micromapCount, const VkMicromapEXT *pMicromaps, VkQueryType queryType, size_t dataSize, void *pData, size_t stride)
{
    struct vkWriteMicromapsPropertiesEXT_params params;
    NTSTATUS status;
    params.device = device;
    params.micromapCount = micromapCount;
    params.pMicromaps = pMicromaps;
    params.queryType = queryType;
    params.dataSize = dataSize;
    params.pData = pData;
    params.stride = stride;
    status = UNIX_CALL(vkWriteMicromapsPropertiesEXT, &params);
    assert(!status && "vkWriteMicromapsPropertiesEXT");
    return params.result;
}

static const struct vulkan_func vk_device_dispatch_table[] =
{
    {"vkAcquireNextImage2KHR", vkAcquireNextImage2KHR},
    {"vkAcquireNextImageKHR", vkAcquireNextImageKHR},
    {"vkAcquireProfilingLockKHR", vkAcquireProfilingLockKHR},
    {"vkAllocateCommandBuffers", vkAllocateCommandBuffers},
    {"vkAllocateDescriptorSets", vkAllocateDescriptorSets},
    {"vkAllocateMemory", vkAllocateMemory},
    {"vkAntiLagUpdateAMD", vkAntiLagUpdateAMD},
    {"vkBeginCommandBuffer", vkBeginCommandBuffer},
    {"vkBindAccelerationStructureMemoryNV", vkBindAccelerationStructureMemoryNV},
    {"vkBindBufferMemory", vkBindBufferMemory},
    {"vkBindBufferMemory2", vkBindBufferMemory2},
    {"vkBindBufferMemory2KHR", vkBindBufferMemory2KHR},
    {"vkBindImageMemory", vkBindImageMemory},
    {"vkBindImageMemory2", vkBindImageMemory2},
    {"vkBindImageMemory2KHR", vkBindImageMemory2KHR},
    {"vkBindOpticalFlowSessionImageNV", vkBindOpticalFlowSessionImageNV},
    {"vkBindVideoSessionMemoryKHR", vkBindVideoSessionMemoryKHR},
    {"vkBuildAccelerationStructuresKHR", vkBuildAccelerationStructuresKHR},
    {"vkBuildMicromapsEXT", vkBuildMicromapsEXT},
    {"vkCmdBeginConditionalRenderingEXT", vkCmdBeginConditionalRenderingEXT},
    {"vkCmdBeginDebugUtilsLabelEXT", vkCmdBeginDebugUtilsLabelEXT},
    {"vkCmdBeginQuery", vkCmdBeginQuery},
    {"vkCmdBeginQueryIndexedEXT", vkCmdBeginQueryIndexedEXT},
    {"vkCmdBeginRenderPass", vkCmdBeginRenderPass},
    {"vkCmdBeginRenderPass2", vkCmdBeginRenderPass2},
    {"vkCmdBeginRenderPass2KHR", vkCmdBeginRenderPass2KHR},
    {"vkCmdBeginRendering", vkCmdBeginRendering},
    {"vkCmdBeginRenderingKHR", vkCmdBeginRenderingKHR},
    {"vkCmdBeginTransformFeedbackEXT", vkCmdBeginTransformFeedbackEXT},
    {"vkCmdBeginVideoCodingKHR", vkCmdBeginVideoCodingKHR},
    {"vkCmdBindDescriptorBufferEmbeddedSamplers2EXT", vkCmdBindDescriptorBufferEmbeddedSamplers2EXT},
    {"vkCmdBindDescriptorBufferEmbeddedSamplersEXT", vkCmdBindDescriptorBufferEmbeddedSamplersEXT},
    {"vkCmdBindDescriptorBuffersEXT", vkCmdBindDescriptorBuffersEXT},
    {"vkCmdBindDescriptorSets", vkCmdBindDescriptorSets},
    {"vkCmdBindDescriptorSets2", vkCmdBindDescriptorSets2},
    {"vkCmdBindDescriptorSets2KHR", vkCmdBindDescriptorSets2KHR},
    {"vkCmdBindIndexBuffer", vkCmdBindIndexBuffer},
    {"vkCmdBindIndexBuffer2", vkCmdBindIndexBuffer2},
    {"vkCmdBindIndexBuffer2KHR", vkCmdBindIndexBuffer2KHR},
    {"vkCmdBindInvocationMaskHUAWEI", vkCmdBindInvocationMaskHUAWEI},
    {"vkCmdBindPipeline", vkCmdBindPipeline},
    {"vkCmdBindPipelineShaderGroupNV", vkCmdBindPipelineShaderGroupNV},
    {"vkCmdBindShadersEXT", vkCmdBindShadersEXT},
    {"vkCmdBindShadingRateImageNV", vkCmdBindShadingRateImageNV},
    {"vkCmdBindTransformFeedbackBuffersEXT", vkCmdBindTransformFeedbackBuffersEXT},
    {"vkCmdBindVertexBuffers", vkCmdBindVertexBuffers},
    {"vkCmdBindVertexBuffers2", vkCmdBindVertexBuffers2},
    {"vkCmdBindVertexBuffers2EXT", vkCmdBindVertexBuffers2EXT},
    {"vkCmdBlitImage", vkCmdBlitImage},
    {"vkCmdBlitImage2", vkCmdBlitImage2},
    {"vkCmdBlitImage2KHR", vkCmdBlitImage2KHR},
    {"vkCmdBuildAccelerationStructureNV", vkCmdBuildAccelerationStructureNV},
    {"vkCmdBuildAccelerationStructuresIndirectKHR", vkCmdBuildAccelerationStructuresIndirectKHR},
    {"vkCmdBuildAccelerationStructuresKHR", vkCmdBuildAccelerationStructuresKHR},
    {"vkCmdBuildClusterAccelerationStructureIndirectNV", vkCmdBuildClusterAccelerationStructureIndirectNV},
    {"vkCmdBuildMicromapsEXT", vkCmdBuildMicromapsEXT},
    {"vkCmdBuildPartitionedAccelerationStructuresNV", vkCmdBuildPartitionedAccelerationStructuresNV},
    {"vkCmdClearAttachments", vkCmdClearAttachments},
    {"vkCmdClearColorImage", vkCmdClearColorImage},
    {"vkCmdClearDepthStencilImage", vkCmdClearDepthStencilImage},
    {"vkCmdControlVideoCodingKHR", vkCmdControlVideoCodingKHR},
    {"vkCmdConvertCooperativeVectorMatrixNV", vkCmdConvertCooperativeVectorMatrixNV},
    {"vkCmdCopyAccelerationStructureKHR", vkCmdCopyAccelerationStructureKHR},
    {"vkCmdCopyAccelerationStructureNV", vkCmdCopyAccelerationStructureNV},
    {"vkCmdCopyAccelerationStructureToMemoryKHR", vkCmdCopyAccelerationStructureToMemoryKHR},
    {"vkCmdCopyBuffer", vkCmdCopyBuffer},
    {"vkCmdCopyBuffer2", vkCmdCopyBuffer2},
    {"vkCmdCopyBuffer2KHR", vkCmdCopyBuffer2KHR},
    {"vkCmdCopyBufferToImage", vkCmdCopyBufferToImage},
    {"vkCmdCopyBufferToImage2", vkCmdCopyBufferToImage2},
    {"vkCmdCopyBufferToImage2KHR", vkCmdCopyBufferToImage2KHR},
    {"vkCmdCopyImage", vkCmdCopyImage},
    {"vkCmdCopyImage2", vkCmdCopyImage2},
    {"vkCmdCopyImage2KHR", vkCmdCopyImage2KHR},
    {"vkCmdCopyImageToBuffer", vkCmdCopyImageToBuffer},
    {"vkCmdCopyImageToBuffer2", vkCmdCopyImageToBuffer2},
    {"vkCmdCopyImageToBuffer2KHR", vkCmdCopyImageToBuffer2KHR},
    {"vkCmdCopyMemoryIndirectNV", vkCmdCopyMemoryIndirectNV},
    {"vkCmdCopyMemoryToAccelerationStructureKHR", vkCmdCopyMemoryToAccelerationStructureKHR},
    {"vkCmdCopyMemoryToImageIndirectNV", vkCmdCopyMemoryToImageIndirectNV},
    {"vkCmdCopyMemoryToMicromapEXT", vkCmdCopyMemoryToMicromapEXT},
    {"vkCmdCopyMicromapEXT", vkCmdCopyMicromapEXT},
    {"vkCmdCopyMicromapToMemoryEXT", vkCmdCopyMicromapToMemoryEXT},
    {"vkCmdCopyQueryPoolResults", vkCmdCopyQueryPoolResults},
    {"vkCmdCuLaunchKernelNVX", vkCmdCuLaunchKernelNVX},
    {"vkCmdCudaLaunchKernelNV", vkCmdCudaLaunchKernelNV},
    {"vkCmdDebugMarkerBeginEXT", vkCmdDebugMarkerBeginEXT},
    {"vkCmdDebugMarkerEndEXT", vkCmdDebugMarkerEndEXT},
    {"vkCmdDebugMarkerInsertEXT", vkCmdDebugMarkerInsertEXT},
    {"vkCmdDecodeVideoKHR", vkCmdDecodeVideoKHR},
    {"vkCmdDecompressMemoryIndirectCountNV", vkCmdDecompressMemoryIndirectCountNV},
    {"vkCmdDecompressMemoryNV", vkCmdDecompressMemoryNV},
    {"vkCmdDispatch", vkCmdDispatch},
    {"vkCmdDispatchBase", vkCmdDispatchBase},
    {"vkCmdDispatchBaseKHR", vkCmdDispatchBaseKHR},
    {"vkCmdDispatchIndirect", vkCmdDispatchIndirect},
    {"vkCmdDraw", vkCmdDraw},
    {"vkCmdDrawClusterHUAWEI", vkCmdDrawClusterHUAWEI},
    {"vkCmdDrawClusterIndirectHUAWEI", vkCmdDrawClusterIndirectHUAWEI},
    {"vkCmdDrawIndexed", vkCmdDrawIndexed},
    {"vkCmdDrawIndexedIndirect", vkCmdDrawIndexedIndirect},
    {"vkCmdDrawIndexedIndirectCount", vkCmdDrawIndexedIndirectCount},
    {"vkCmdDrawIndexedIndirectCountAMD", vkCmdDrawIndexedIndirectCountAMD},
    {"vkCmdDrawIndexedIndirectCountKHR", vkCmdDrawIndexedIndirectCountKHR},
    {"vkCmdDrawIndirect", vkCmdDrawIndirect},
    {"vkCmdDrawIndirectByteCountEXT", vkCmdDrawIndirectByteCountEXT},
    {"vkCmdDrawIndirectCount", vkCmdDrawIndirectCount},
    {"vkCmdDrawIndirectCountAMD", vkCmdDrawIndirectCountAMD},
    {"vkCmdDrawIndirectCountKHR", vkCmdDrawIndirectCountKHR},
    {"vkCmdDrawMeshTasksEXT", vkCmdDrawMeshTasksEXT},
    {"vkCmdDrawMeshTasksIndirectCountEXT", vkCmdDrawMeshTasksIndirectCountEXT},
    {"vkCmdDrawMeshTasksIndirectCountNV", vkCmdDrawMeshTasksIndirectCountNV},
    {"vkCmdDrawMeshTasksIndirectEXT", vkCmdDrawMeshTasksIndirectEXT},
    {"vkCmdDrawMeshTasksIndirectNV", vkCmdDrawMeshTasksIndirectNV},
    {"vkCmdDrawMeshTasksNV", vkCmdDrawMeshTasksNV},
    {"vkCmdDrawMultiEXT", vkCmdDrawMultiEXT},
    {"vkCmdDrawMultiIndexedEXT", vkCmdDrawMultiIndexedEXT},
    {"vkCmdEncodeVideoKHR", vkCmdEncodeVideoKHR},
    {"vkCmdEndConditionalRenderingEXT", vkCmdEndConditionalRenderingEXT},
    {"vkCmdEndDebugUtilsLabelEXT", vkCmdEndDebugUtilsLabelEXT},
    {"vkCmdEndQuery", vkCmdEndQuery},
    {"vkCmdEndQueryIndexedEXT", vkCmdEndQueryIndexedEXT},
    {"vkCmdEndRenderPass", vkCmdEndRenderPass},
    {"vkCmdEndRenderPass2", vkCmdEndRenderPass2},
    {"vkCmdEndRenderPass2KHR", vkCmdEndRenderPass2KHR},
    {"vkCmdEndRendering", vkCmdEndRendering},
    {"vkCmdEndRenderingKHR", vkCmdEndRenderingKHR},
    {"vkCmdEndTransformFeedbackEXT", vkCmdEndTransformFeedbackEXT},
    {"vkCmdEndVideoCodingKHR", vkCmdEndVideoCodingKHR},
    {"vkCmdExecuteCommands", vkCmdExecuteCommands},
    {"vkCmdExecuteGeneratedCommandsEXT", vkCmdExecuteGeneratedCommandsEXT},
    {"vkCmdExecuteGeneratedCommandsNV", vkCmdExecuteGeneratedCommandsNV},
    {"vkCmdFillBuffer", vkCmdFillBuffer},
    {"vkCmdInsertDebugUtilsLabelEXT", vkCmdInsertDebugUtilsLabelEXT},
    {"vkCmdNextSubpass", vkCmdNextSubpass},
    {"vkCmdNextSubpass2", vkCmdNextSubpass2},
    {"vkCmdNextSubpass2KHR", vkCmdNextSubpass2KHR},
    {"vkCmdOpticalFlowExecuteNV", vkCmdOpticalFlowExecuteNV},
    {"vkCmdPipelineBarrier", vkCmdPipelineBarrier},
    {"vkCmdPipelineBarrier2", vkCmdPipelineBarrier2},
    {"vkCmdPipelineBarrier2KHR", vkCmdPipelineBarrier2KHR},
    {"vkCmdPreprocessGeneratedCommandsEXT", vkCmdPreprocessGeneratedCommandsEXT},
    {"vkCmdPreprocessGeneratedCommandsNV", vkCmdPreprocessGeneratedCommandsNV},
    {"vkCmdPushConstants", vkCmdPushConstants},
    {"vkCmdPushConstants2", vkCmdPushConstants2},
    {"vkCmdPushConstants2KHR", vkCmdPushConstants2KHR},
    {"vkCmdPushDescriptorSet", vkCmdPushDescriptorSet},
    {"vkCmdPushDescriptorSet2", vkCmdPushDescriptorSet2},
    {"vkCmdPushDescriptorSet2KHR", vkCmdPushDescriptorSet2KHR},
    {"vkCmdPushDescriptorSetKHR", vkCmdPushDescriptorSetKHR},
    {"vkCmdPushDescriptorSetWithTemplate", vkCmdPushDescriptorSetWithTemplate},
    {"vkCmdPushDescriptorSetWithTemplate2", vkCmdPushDescriptorSetWithTemplate2},
    {"vkCmdPushDescriptorSetWithTemplate2KHR", vkCmdPushDescriptorSetWithTemplate2KHR},
    {"vkCmdPushDescriptorSetWithTemplateKHR", vkCmdPushDescriptorSetWithTemplateKHR},
    {"vkCmdResetEvent", vkCmdResetEvent},
    {"vkCmdResetEvent2", vkCmdResetEvent2},
    {"vkCmdResetEvent2KHR", vkCmdResetEvent2KHR},
    {"vkCmdResetQueryPool", vkCmdResetQueryPool},
    {"vkCmdResolveImage", vkCmdResolveImage},
    {"vkCmdResolveImage2", vkCmdResolveImage2},
    {"vkCmdResolveImage2KHR", vkCmdResolveImage2KHR},
    {"vkCmdSetAlphaToCoverageEnableEXT", vkCmdSetAlphaToCoverageEnableEXT},
    {"vkCmdSetAlphaToOneEnableEXT", vkCmdSetAlphaToOneEnableEXT},
    {"vkCmdSetAttachmentFeedbackLoopEnableEXT", vkCmdSetAttachmentFeedbackLoopEnableEXT},
    {"vkCmdSetBlendConstants", vkCmdSetBlendConstants},
    {"vkCmdSetCheckpointNV", vkCmdSetCheckpointNV},
    {"vkCmdSetCoarseSampleOrderNV", vkCmdSetCoarseSampleOrderNV},
    {"vkCmdSetColorBlendAdvancedEXT", vkCmdSetColorBlendAdvancedEXT},
    {"vkCmdSetColorBlendEnableEXT", vkCmdSetColorBlendEnableEXT},
    {"vkCmdSetColorBlendEquationEXT", vkCmdSetColorBlendEquationEXT},
    {"vkCmdSetColorWriteEnableEXT", vkCmdSetColorWriteEnableEXT},
    {"vkCmdSetColorWriteMaskEXT", vkCmdSetColorWriteMaskEXT},
    {"vkCmdSetConservativeRasterizationModeEXT", vkCmdSetConservativeRasterizationModeEXT},
    {"vkCmdSetCoverageModulationModeNV", vkCmdSetCoverageModulationModeNV},
    {"vkCmdSetCoverageModulationTableEnableNV", vkCmdSetCoverageModulationTableEnableNV},
    {"vkCmdSetCoverageModulationTableNV", vkCmdSetCoverageModulationTableNV},
    {"vkCmdSetCoverageReductionModeNV", vkCmdSetCoverageReductionModeNV},
    {"vkCmdSetCoverageToColorEnableNV", vkCmdSetCoverageToColorEnableNV},
    {"vkCmdSetCoverageToColorLocationNV", vkCmdSetCoverageToColorLocationNV},
    {"vkCmdSetCullMode", vkCmdSetCullMode},
    {"vkCmdSetCullModeEXT", vkCmdSetCullModeEXT},
    {"vkCmdSetDepthBias", vkCmdSetDepthBias},
    {"vkCmdSetDepthBias2EXT", vkCmdSetDepthBias2EXT},
    {"vkCmdSetDepthBiasEnable", vkCmdSetDepthBiasEnable},
    {"vkCmdSetDepthBiasEnableEXT", vkCmdSetDepthBiasEnableEXT},
    {"vkCmdSetDepthBounds", vkCmdSetDepthBounds},
    {"vkCmdSetDepthBoundsTestEnable", vkCmdSetDepthBoundsTestEnable},
    {"vkCmdSetDepthBoundsTestEnableEXT", vkCmdSetDepthBoundsTestEnableEXT},
    {"vkCmdSetDepthClampEnableEXT", vkCmdSetDepthClampEnableEXT},
    {"vkCmdSetDepthClampRangeEXT", vkCmdSetDepthClampRangeEXT},
    {"vkCmdSetDepthClipEnableEXT", vkCmdSetDepthClipEnableEXT},
    {"vkCmdSetDepthClipNegativeOneToOneEXT", vkCmdSetDepthClipNegativeOneToOneEXT},
    {"vkCmdSetDepthCompareOp", vkCmdSetDepthCompareOp},
    {"vkCmdSetDepthCompareOpEXT", vkCmdSetDepthCompareOpEXT},
    {"vkCmdSetDepthTestEnable", vkCmdSetDepthTestEnable},
    {"vkCmdSetDepthTestEnableEXT", vkCmdSetDepthTestEnableEXT},
    {"vkCmdSetDepthWriteEnable", vkCmdSetDepthWriteEnable},
    {"vkCmdSetDepthWriteEnableEXT", vkCmdSetDepthWriteEnableEXT},
    {"vkCmdSetDescriptorBufferOffsets2EXT", vkCmdSetDescriptorBufferOffsets2EXT},
    {"vkCmdSetDescriptorBufferOffsetsEXT", vkCmdSetDescriptorBufferOffsetsEXT},
    {"vkCmdSetDeviceMask", vkCmdSetDeviceMask},
    {"vkCmdSetDeviceMaskKHR", vkCmdSetDeviceMaskKHR},
    {"vkCmdSetDiscardRectangleEXT", vkCmdSetDiscardRectangleEXT},
    {"vkCmdSetDiscardRectangleEnableEXT", vkCmdSetDiscardRectangleEnableEXT},
    {"vkCmdSetDiscardRectangleModeEXT", vkCmdSetDiscardRectangleModeEXT},
    {"vkCmdSetEvent", vkCmdSetEvent},
    {"vkCmdSetEvent2", vkCmdSetEvent2},
    {"vkCmdSetEvent2KHR", vkCmdSetEvent2KHR},
    {"vkCmdSetExclusiveScissorEnableNV", vkCmdSetExclusiveScissorEnableNV},
    {"vkCmdSetExclusiveScissorNV", vkCmdSetExclusiveScissorNV},
    {"vkCmdSetExtraPrimitiveOverestimationSizeEXT", vkCmdSetExtraPrimitiveOverestimationSizeEXT},
    {"vkCmdSetFragmentShadingRateEnumNV", vkCmdSetFragmentShadingRateEnumNV},
    {"vkCmdSetFragmentShadingRateKHR", vkCmdSetFragmentShadingRateKHR},
    {"vkCmdSetFrontFace", vkCmdSetFrontFace},
    {"vkCmdSetFrontFaceEXT", vkCmdSetFrontFaceEXT},
    {"vkCmdSetLineRasterizationModeEXT", vkCmdSetLineRasterizationModeEXT},
    {"vkCmdSetLineStipple", vkCmdSetLineStipple},
    {"vkCmdSetLineStippleEXT", vkCmdSetLineStippleEXT},
    {"vkCmdSetLineStippleEnableEXT", vkCmdSetLineStippleEnableEXT},
    {"vkCmdSetLineStippleKHR", vkCmdSetLineStippleKHR},
    {"vkCmdSetLineWidth", vkCmdSetLineWidth},
    {"vkCmdSetLogicOpEXT", vkCmdSetLogicOpEXT},
    {"vkCmdSetLogicOpEnableEXT", vkCmdSetLogicOpEnableEXT},
    {"vkCmdSetPatchControlPointsEXT", vkCmdSetPatchControlPointsEXT},
    {"vkCmdSetPolygonModeEXT", vkCmdSetPolygonModeEXT},
    {"vkCmdSetPrimitiveRestartEnable", vkCmdSetPrimitiveRestartEnable},
    {"vkCmdSetPrimitiveRestartEnableEXT", vkCmdSetPrimitiveRestartEnableEXT},
    {"vkCmdSetPrimitiveTopology", vkCmdSetPrimitiveTopology},
    {"vkCmdSetPrimitiveTopologyEXT", vkCmdSetPrimitiveTopologyEXT},
    {"vkCmdSetProvokingVertexModeEXT", vkCmdSetProvokingVertexModeEXT},
    {"vkCmdSetRasterizationSamplesEXT", vkCmdSetRasterizationSamplesEXT},
    {"vkCmdSetRasterizationStreamEXT", vkCmdSetRasterizationStreamEXT},
    {"vkCmdSetRasterizerDiscardEnable", vkCmdSetRasterizerDiscardEnable},
    {"vkCmdSetRasterizerDiscardEnableEXT", vkCmdSetRasterizerDiscardEnableEXT},
    {"vkCmdSetRayTracingPipelineStackSizeKHR", vkCmdSetRayTracingPipelineStackSizeKHR},
    {"vkCmdSetRenderingAttachmentLocations", vkCmdSetRenderingAttachmentLocations},
    {"vkCmdSetRenderingAttachmentLocationsKHR", vkCmdSetRenderingAttachmentLocationsKHR},
    {"vkCmdSetRenderingInputAttachmentIndices", vkCmdSetRenderingInputAttachmentIndices},
    {"vkCmdSetRenderingInputAttachmentIndicesKHR", vkCmdSetRenderingInputAttachmentIndicesKHR},
    {"vkCmdSetRepresentativeFragmentTestEnableNV", vkCmdSetRepresentativeFragmentTestEnableNV},
    {"vkCmdSetSampleLocationsEXT", vkCmdSetSampleLocationsEXT},
    {"vkCmdSetSampleLocationsEnableEXT", vkCmdSetSampleLocationsEnableEXT},
    {"vkCmdSetSampleMaskEXT", vkCmdSetSampleMaskEXT},
    {"vkCmdSetScissor", vkCmdSetScissor},
    {"vkCmdSetScissorWithCount", vkCmdSetScissorWithCount},
    {"vkCmdSetScissorWithCountEXT", vkCmdSetScissorWithCountEXT},
    {"vkCmdSetShadingRateImageEnableNV", vkCmdSetShadingRateImageEnableNV},
    {"vkCmdSetStencilCompareMask", vkCmdSetStencilCompareMask},
    {"vkCmdSetStencilOp", vkCmdSetStencilOp},
    {"vkCmdSetStencilOpEXT", vkCmdSetStencilOpEXT},
    {"vkCmdSetStencilReference", vkCmdSetStencilReference},
    {"vkCmdSetStencilTestEnable", vkCmdSetStencilTestEnable},
    {"vkCmdSetStencilTestEnableEXT", vkCmdSetStencilTestEnableEXT},
    {"vkCmdSetStencilWriteMask", vkCmdSetStencilWriteMask},
    {"vkCmdSetTessellationDomainOriginEXT", vkCmdSetTessellationDomainOriginEXT},
    {"vkCmdSetVertexInputEXT", vkCmdSetVertexInputEXT},
    {"vkCmdSetViewport", vkCmdSetViewport},
    {"vkCmdSetViewportShadingRatePaletteNV", vkCmdSetViewportShadingRatePaletteNV},
    {"vkCmdSetViewportSwizzleNV", vkCmdSetViewportSwizzleNV},
    {"vkCmdSetViewportWScalingEnableNV", vkCmdSetViewportWScalingEnableNV},
    {"vkCmdSetViewportWScalingNV", vkCmdSetViewportWScalingNV},
    {"vkCmdSetViewportWithCount", vkCmdSetViewportWithCount},
    {"vkCmdSetViewportWithCountEXT", vkCmdSetViewportWithCountEXT},
    {"vkCmdSubpassShadingHUAWEI", vkCmdSubpassShadingHUAWEI},
    {"vkCmdTraceRaysIndirect2KHR", vkCmdTraceRaysIndirect2KHR},
    {"vkCmdTraceRaysIndirectKHR", vkCmdTraceRaysIndirectKHR},
    {"vkCmdTraceRaysKHR", vkCmdTraceRaysKHR},
    {"vkCmdTraceRaysNV", vkCmdTraceRaysNV},
    {"vkCmdUpdateBuffer", vkCmdUpdateBuffer},
    {"vkCmdUpdatePipelineIndirectBufferNV", vkCmdUpdatePipelineIndirectBufferNV},
    {"vkCmdWaitEvents", vkCmdWaitEvents},
    {"vkCmdWaitEvents2", vkCmdWaitEvents2},
    {"vkCmdWaitEvents2KHR", vkCmdWaitEvents2KHR},
    {"vkCmdWriteAccelerationStructuresPropertiesKHR", vkCmdWriteAccelerationStructuresPropertiesKHR},
    {"vkCmdWriteAccelerationStructuresPropertiesNV", vkCmdWriteAccelerationStructuresPropertiesNV},
    {"vkCmdWriteBufferMarker2AMD", vkCmdWriteBufferMarker2AMD},
    {"vkCmdWriteBufferMarkerAMD", vkCmdWriteBufferMarkerAMD},
    {"vkCmdWriteMicromapsPropertiesEXT", vkCmdWriteMicromapsPropertiesEXT},
    {"vkCmdWriteTimestamp", vkCmdWriteTimestamp},
    {"vkCmdWriteTimestamp2", vkCmdWriteTimestamp2},
    {"vkCmdWriteTimestamp2KHR", vkCmdWriteTimestamp2KHR},
    {"vkCompileDeferredNV", vkCompileDeferredNV},
    {"vkConvertCooperativeVectorMatrixNV", vkConvertCooperativeVectorMatrixNV},
    {"vkCopyAccelerationStructureKHR", vkCopyAccelerationStructureKHR},
    {"vkCopyAccelerationStructureToMemoryKHR", vkCopyAccelerationStructureToMemoryKHR},
    {"vkCopyImageToImage", vkCopyImageToImage},
    {"vkCopyImageToImageEXT", vkCopyImageToImageEXT},
    {"vkCopyImageToMemory", vkCopyImageToMemory},
    {"vkCopyImageToMemoryEXT", vkCopyImageToMemoryEXT},
    {"vkCopyMemoryToAccelerationStructureKHR", vkCopyMemoryToAccelerationStructureKHR},
    {"vkCopyMemoryToImage", vkCopyMemoryToImage},
    {"vkCopyMemoryToImageEXT", vkCopyMemoryToImageEXT},
    {"vkCopyMemoryToMicromapEXT", vkCopyMemoryToMicromapEXT},
    {"vkCopyMicromapEXT", vkCopyMicromapEXT},
    {"vkCopyMicromapToMemoryEXT", vkCopyMicromapToMemoryEXT},
    {"vkCreateAccelerationStructureKHR", vkCreateAccelerationStructureKHR},
    {"vkCreateAccelerationStructureNV", vkCreateAccelerationStructureNV},
    {"vkCreateBuffer", vkCreateBuffer},
    {"vkCreateBufferView", vkCreateBufferView},
    {"vkCreateCommandPool", vkCreateCommandPool},
    {"vkCreateComputePipelines", vkCreateComputePipelines},
    {"vkCreateCuFunctionNVX", vkCreateCuFunctionNVX},
    {"vkCreateCuModuleNVX", vkCreateCuModuleNVX},
    {"vkCreateCudaFunctionNV", vkCreateCudaFunctionNV},
    {"vkCreateCudaModuleNV", vkCreateCudaModuleNV},
    {"vkCreateDeferredOperationKHR", vkCreateDeferredOperationKHR},
    {"vkCreateDescriptorPool", vkCreateDescriptorPool},
    {"vkCreateDescriptorSetLayout", vkCreateDescriptorSetLayout},
    {"vkCreateDescriptorUpdateTemplate", vkCreateDescriptorUpdateTemplate},
    {"vkCreateDescriptorUpdateTemplateKHR", vkCreateDescriptorUpdateTemplateKHR},
    {"vkCreateEvent", vkCreateEvent},
    {"vkCreateFence", vkCreateFence},
    {"vkCreateFramebuffer", vkCreateFramebuffer},
    {"vkCreateGraphicsPipelines", vkCreateGraphicsPipelines},
    {"vkCreateImage", vkCreateImage},
    {"vkCreateImageView", vkCreateImageView},
    {"vkCreateIndirectCommandsLayoutEXT", vkCreateIndirectCommandsLayoutEXT},
    {"vkCreateIndirectCommandsLayoutNV", vkCreateIndirectCommandsLayoutNV},
    {"vkCreateIndirectExecutionSetEXT", vkCreateIndirectExecutionSetEXT},
    {"vkCreateMicromapEXT", vkCreateMicromapEXT},
    {"vkCreateOpticalFlowSessionNV", vkCreateOpticalFlowSessionNV},
    {"vkCreatePipelineBinariesKHR", vkCreatePipelineBinariesKHR},
    {"vkCreatePipelineCache", vkCreatePipelineCache},
    {"vkCreatePipelineLayout", vkCreatePipelineLayout},
    {"vkCreatePrivateDataSlot", vkCreatePrivateDataSlot},
    {"vkCreatePrivateDataSlotEXT", vkCreatePrivateDataSlotEXT},
    {"vkCreateQueryPool", vkCreateQueryPool},
    {"vkCreateRayTracingPipelinesKHR", vkCreateRayTracingPipelinesKHR},
    {"vkCreateRayTracingPipelinesNV", vkCreateRayTracingPipelinesNV},
    {"vkCreateRenderPass", vkCreateRenderPass},
    {"vkCreateRenderPass2", vkCreateRenderPass2},
    {"vkCreateRenderPass2KHR", vkCreateRenderPass2KHR},
    {"vkCreateSampler", vkCreateSampler},
    {"vkCreateSamplerYcbcrConversion", vkCreateSamplerYcbcrConversion},
    {"vkCreateSamplerYcbcrConversionKHR", vkCreateSamplerYcbcrConversionKHR},
    {"vkCreateSemaphore", vkCreateSemaphore},
    {"vkCreateShaderModule", vkCreateShaderModule},
    {"vkCreateShadersEXT", vkCreateShadersEXT},
    {"vkCreateSwapchainKHR", vkCreateSwapchainKHR},
    {"vkCreateValidationCacheEXT", vkCreateValidationCacheEXT},
    {"vkCreateVideoSessionKHR", vkCreateVideoSessionKHR},
    {"vkCreateVideoSessionParametersKHR", vkCreateVideoSessionParametersKHR},
    {"vkDebugMarkerSetObjectNameEXT", vkDebugMarkerSetObjectNameEXT},
    {"vkDebugMarkerSetObjectTagEXT", vkDebugMarkerSetObjectTagEXT},
    {"vkDeferredOperationJoinKHR", vkDeferredOperationJoinKHR},
    {"vkDestroyAccelerationStructureKHR", vkDestroyAccelerationStructureKHR},
    {"vkDestroyAccelerationStructureNV", vkDestroyAccelerationStructureNV},
    {"vkDestroyBuffer", vkDestroyBuffer},
    {"vkDestroyBufferView", vkDestroyBufferView},
    {"vkDestroyCommandPool", vkDestroyCommandPool},
    {"vkDestroyCuFunctionNVX", vkDestroyCuFunctionNVX},
    {"vkDestroyCuModuleNVX", vkDestroyCuModuleNVX},
    {"vkDestroyCudaFunctionNV", vkDestroyCudaFunctionNV},
    {"vkDestroyCudaModuleNV", vkDestroyCudaModuleNV},
    {"vkDestroyDeferredOperationKHR", vkDestroyDeferredOperationKHR},
    {"vkDestroyDescriptorPool", vkDestroyDescriptorPool},
    {"vkDestroyDescriptorSetLayout", vkDestroyDescriptorSetLayout},
    {"vkDestroyDescriptorUpdateTemplate", vkDestroyDescriptorUpdateTemplate},
    {"vkDestroyDescriptorUpdateTemplateKHR", vkDestroyDescriptorUpdateTemplateKHR},
    {"vkDestroyDevice", vkDestroyDevice},
    {"vkDestroyEvent", vkDestroyEvent},
    {"vkDestroyFence", vkDestroyFence},
    {"vkDestroyFramebuffer", vkDestroyFramebuffer},
    {"vkDestroyImage", vkDestroyImage},
    {"vkDestroyImageView", vkDestroyImageView},
    {"vkDestroyIndirectCommandsLayoutEXT", vkDestroyIndirectCommandsLayoutEXT},
    {"vkDestroyIndirectCommandsLayoutNV", vkDestroyIndirectCommandsLayoutNV},
    {"vkDestroyIndirectExecutionSetEXT", vkDestroyIndirectExecutionSetEXT},
    {"vkDestroyMicromapEXT", vkDestroyMicromapEXT},
    {"vkDestroyOpticalFlowSessionNV", vkDestroyOpticalFlowSessionNV},
    {"vkDestroyPipeline", vkDestroyPipeline},
    {"vkDestroyPipelineBinaryKHR", vkDestroyPipelineBinaryKHR},
    {"vkDestroyPipelineCache", vkDestroyPipelineCache},
    {"vkDestroyPipelineLayout", vkDestroyPipelineLayout},
    {"vkDestroyPrivateDataSlot", vkDestroyPrivateDataSlot},
    {"vkDestroyPrivateDataSlotEXT", vkDestroyPrivateDataSlotEXT},
    {"vkDestroyQueryPool", vkDestroyQueryPool},
    {"vkDestroyRenderPass", vkDestroyRenderPass},
    {"vkDestroySampler", vkDestroySampler},
    {"vkDestroySamplerYcbcrConversion", vkDestroySamplerYcbcrConversion},
    {"vkDestroySamplerYcbcrConversionKHR", vkDestroySamplerYcbcrConversionKHR},
    {"vkDestroySemaphore", vkDestroySemaphore},
    {"vkDestroyShaderEXT", vkDestroyShaderEXT},
    {"vkDestroyShaderModule", vkDestroyShaderModule},
    {"vkDestroySwapchainKHR", vkDestroySwapchainKHR},
    {"vkDestroyValidationCacheEXT", vkDestroyValidationCacheEXT},
    {"vkDestroyVideoSessionKHR", vkDestroyVideoSessionKHR},
    {"vkDestroyVideoSessionParametersKHR", vkDestroyVideoSessionParametersKHR},
    {"vkDeviceWaitIdle", vkDeviceWaitIdle},
    {"vkEndCommandBuffer", vkEndCommandBuffer},
    {"vkFlushMappedMemoryRanges", vkFlushMappedMemoryRanges},
    {"vkFreeCommandBuffers", vkFreeCommandBuffers},
    {"vkFreeDescriptorSets", vkFreeDescriptorSets},
    {"vkFreeMemory", vkFreeMemory},
    {"vkGetAccelerationStructureBuildSizesKHR", vkGetAccelerationStructureBuildSizesKHR},
    {"vkGetAccelerationStructureDeviceAddressKHR", vkGetAccelerationStructureDeviceAddressKHR},
    {"vkGetAccelerationStructureHandleNV", vkGetAccelerationStructureHandleNV},
    {"vkGetAccelerationStructureMemoryRequirementsNV", vkGetAccelerationStructureMemoryRequirementsNV},
    {"vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT", vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT},
    {"vkGetBufferDeviceAddress", vkGetBufferDeviceAddress},
    {"vkGetBufferDeviceAddressEXT", vkGetBufferDeviceAddressEXT},
    {"vkGetBufferDeviceAddressKHR", vkGetBufferDeviceAddressKHR},
    {"vkGetBufferMemoryRequirements", vkGetBufferMemoryRequirements},
    {"vkGetBufferMemoryRequirements2", vkGetBufferMemoryRequirements2},
    {"vkGetBufferMemoryRequirements2KHR", vkGetBufferMemoryRequirements2KHR},
    {"vkGetBufferOpaqueCaptureAddress", vkGetBufferOpaqueCaptureAddress},
    {"vkGetBufferOpaqueCaptureAddressKHR", vkGetBufferOpaqueCaptureAddressKHR},
    {"vkGetBufferOpaqueCaptureDescriptorDataEXT", vkGetBufferOpaqueCaptureDescriptorDataEXT},
    {"vkGetCalibratedTimestampsEXT", vkGetCalibratedTimestampsEXT},
    {"vkGetCalibratedTimestampsKHR", vkGetCalibratedTimestampsKHR},
    {"vkGetClusterAccelerationStructureBuildSizesNV", vkGetClusterAccelerationStructureBuildSizesNV},
    {"vkGetCudaModuleCacheNV", vkGetCudaModuleCacheNV},
    {"vkGetDeferredOperationMaxConcurrencyKHR", vkGetDeferredOperationMaxConcurrencyKHR},
    {"vkGetDeferredOperationResultKHR", vkGetDeferredOperationResultKHR},
    {"vkGetDescriptorEXT", vkGetDescriptorEXT},
    {"vkGetDescriptorSetHostMappingVALVE", vkGetDescriptorSetHostMappingVALVE},
    {"vkGetDescriptorSetLayoutBindingOffsetEXT", vkGetDescriptorSetLayoutBindingOffsetEXT},
    {"vkGetDescriptorSetLayoutHostMappingInfoVALVE", vkGetDescriptorSetLayoutHostMappingInfoVALVE},
    {"vkGetDescriptorSetLayoutSizeEXT", vkGetDescriptorSetLayoutSizeEXT},
    {"vkGetDescriptorSetLayoutSupport", vkGetDescriptorSetLayoutSupport},
    {"vkGetDescriptorSetLayoutSupportKHR", vkGetDescriptorSetLayoutSupportKHR},
    {"vkGetDeviceAccelerationStructureCompatibilityKHR", vkGetDeviceAccelerationStructureCompatibilityKHR},
    {"vkGetDeviceBufferMemoryRequirements", vkGetDeviceBufferMemoryRequirements},
    {"vkGetDeviceBufferMemoryRequirementsKHR", vkGetDeviceBufferMemoryRequirementsKHR},
    {"vkGetDeviceFaultInfoEXT", vkGetDeviceFaultInfoEXT},
    {"vkGetDeviceGroupPeerMemoryFeatures", vkGetDeviceGroupPeerMemoryFeatures},
    {"vkGetDeviceGroupPeerMemoryFeaturesKHR", vkGetDeviceGroupPeerMemoryFeaturesKHR},
    {"vkGetDeviceGroupPresentCapabilitiesKHR", vkGetDeviceGroupPresentCapabilitiesKHR},
    {"vkGetDeviceGroupSurfacePresentModesKHR", vkGetDeviceGroupSurfacePresentModesKHR},
    {"vkGetDeviceImageMemoryRequirements", vkGetDeviceImageMemoryRequirements},
    {"vkGetDeviceImageMemoryRequirementsKHR", vkGetDeviceImageMemoryRequirementsKHR},
    {"vkGetDeviceImageSparseMemoryRequirements", vkGetDeviceImageSparseMemoryRequirements},
    {"vkGetDeviceImageSparseMemoryRequirementsKHR", vkGetDeviceImageSparseMemoryRequirementsKHR},
    {"vkGetDeviceImageSubresourceLayout", vkGetDeviceImageSubresourceLayout},
    {"vkGetDeviceImageSubresourceLayoutKHR", vkGetDeviceImageSubresourceLayoutKHR},
    {"vkGetDeviceMemoryCommitment", vkGetDeviceMemoryCommitment},
    {"vkGetDeviceMemoryOpaqueCaptureAddress", vkGetDeviceMemoryOpaqueCaptureAddress},
    {"vkGetDeviceMemoryOpaqueCaptureAddressKHR", vkGetDeviceMemoryOpaqueCaptureAddressKHR},
    {"vkGetDeviceMicromapCompatibilityEXT", vkGetDeviceMicromapCompatibilityEXT},
    {"vkGetDeviceProcAddr", vkGetDeviceProcAddr},
    {"vkGetDeviceQueue", vkGetDeviceQueue},
    {"vkGetDeviceQueue2", vkGetDeviceQueue2},
    {"vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI", vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI},
    {"vkGetDynamicRenderingTilePropertiesQCOM", vkGetDynamicRenderingTilePropertiesQCOM},
    {"vkGetEncodedVideoSessionParametersKHR", vkGetEncodedVideoSessionParametersKHR},
    {"vkGetEventStatus", vkGetEventStatus},
    {"vkGetFenceStatus", vkGetFenceStatus},
    {"vkGetFramebufferTilePropertiesQCOM", vkGetFramebufferTilePropertiesQCOM},
    {"vkGetGeneratedCommandsMemoryRequirementsEXT", vkGetGeneratedCommandsMemoryRequirementsEXT},
    {"vkGetGeneratedCommandsMemoryRequirementsNV", vkGetGeneratedCommandsMemoryRequirementsNV},
    {"vkGetImageMemoryRequirements", vkGetImageMemoryRequirements},
    {"vkGetImageMemoryRequirements2", vkGetImageMemoryRequirements2},
    {"vkGetImageMemoryRequirements2KHR", vkGetImageMemoryRequirements2KHR},
    {"vkGetImageOpaqueCaptureDescriptorDataEXT", vkGetImageOpaqueCaptureDescriptorDataEXT},
    {"vkGetImageSparseMemoryRequirements", vkGetImageSparseMemoryRequirements},
    {"vkGetImageSparseMemoryRequirements2", vkGetImageSparseMemoryRequirements2},
    {"vkGetImageSparseMemoryRequirements2KHR", vkGetImageSparseMemoryRequirements2KHR},
    {"vkGetImageSubresourceLayout", vkGetImageSubresourceLayout},
    {"vkGetImageSubresourceLayout2", vkGetImageSubresourceLayout2},
    {"vkGetImageSubresourceLayout2EXT", vkGetImageSubresourceLayout2EXT},
    {"vkGetImageSubresourceLayout2KHR", vkGetImageSubresourceLayout2KHR},
    {"vkGetImageViewAddressNVX", vkGetImageViewAddressNVX},
    {"vkGetImageViewHandle64NVX", vkGetImageViewHandle64NVX},
    {"vkGetImageViewHandleNVX", vkGetImageViewHandleNVX},
    {"vkGetImageViewOpaqueCaptureDescriptorDataEXT", vkGetImageViewOpaqueCaptureDescriptorDataEXT},
    {"vkGetLatencyTimingsNV", vkGetLatencyTimingsNV},
    {"vkGetMemoryHostPointerPropertiesEXT", vkGetMemoryHostPointerPropertiesEXT},
    {"vkGetMicromapBuildSizesEXT", vkGetMicromapBuildSizesEXT},
    {"vkGetPartitionedAccelerationStructuresBuildSizesNV", vkGetPartitionedAccelerationStructuresBuildSizesNV},
    {"vkGetPipelineBinaryDataKHR", vkGetPipelineBinaryDataKHR},
    {"vkGetPipelineCacheData", vkGetPipelineCacheData},
    {"vkGetPipelineExecutableInternalRepresentationsKHR", vkGetPipelineExecutableInternalRepresentationsKHR},
    {"vkGetPipelineExecutablePropertiesKHR", vkGetPipelineExecutablePropertiesKHR},
    {"vkGetPipelineExecutableStatisticsKHR", vkGetPipelineExecutableStatisticsKHR},
    {"vkGetPipelineIndirectDeviceAddressNV", vkGetPipelineIndirectDeviceAddressNV},
    {"vkGetPipelineIndirectMemoryRequirementsNV", vkGetPipelineIndirectMemoryRequirementsNV},
    {"vkGetPipelineKeyKHR", vkGetPipelineKeyKHR},
    {"vkGetPipelinePropertiesEXT", vkGetPipelinePropertiesEXT},
    {"vkGetPrivateData", vkGetPrivateData},
    {"vkGetPrivateDataEXT", vkGetPrivateDataEXT},
    {"vkGetQueryPoolResults", vkGetQueryPoolResults},
    {"vkGetQueueCheckpointData2NV", vkGetQueueCheckpointData2NV},
    {"vkGetQueueCheckpointDataNV", vkGetQueueCheckpointDataNV},
    {"vkGetRayTracingCaptureReplayShaderGroupHandlesKHR", vkGetRayTracingCaptureReplayShaderGroupHandlesKHR},
    {"vkGetRayTracingShaderGroupHandlesKHR", vkGetRayTracingShaderGroupHandlesKHR},
    {"vkGetRayTracingShaderGroupHandlesNV", vkGetRayTracingShaderGroupHandlesNV},
    {"vkGetRayTracingShaderGroupStackSizeKHR", vkGetRayTracingShaderGroupStackSizeKHR},
    {"vkGetRenderAreaGranularity", vkGetRenderAreaGranularity},
    {"vkGetRenderingAreaGranularity", vkGetRenderingAreaGranularity},
    {"vkGetRenderingAreaGranularityKHR", vkGetRenderingAreaGranularityKHR},
    {"vkGetSamplerOpaqueCaptureDescriptorDataEXT", vkGetSamplerOpaqueCaptureDescriptorDataEXT},
    {"vkGetSemaphoreCounterValue", vkGetSemaphoreCounterValue},
    {"vkGetSemaphoreCounterValueKHR", vkGetSemaphoreCounterValueKHR},
    {"vkGetShaderBinaryDataEXT", vkGetShaderBinaryDataEXT},
    {"vkGetShaderInfoAMD", vkGetShaderInfoAMD},
    {"vkGetShaderModuleCreateInfoIdentifierEXT", vkGetShaderModuleCreateInfoIdentifierEXT},
    {"vkGetShaderModuleIdentifierEXT", vkGetShaderModuleIdentifierEXT},
    {"vkGetSwapchainImagesKHR", vkGetSwapchainImagesKHR},
    {"vkGetValidationCacheDataEXT", vkGetValidationCacheDataEXT},
    {"vkGetVideoSessionMemoryRequirementsKHR", vkGetVideoSessionMemoryRequirementsKHR},
    {"vkInvalidateMappedMemoryRanges", vkInvalidateMappedMemoryRanges},
    {"vkLatencySleepNV", vkLatencySleepNV},
    {"vkMapMemory", vkMapMemory},
    {"vkMapMemory2", vkMapMemory2},
    {"vkMapMemory2KHR", vkMapMemory2KHR},
    {"vkMergePipelineCaches", vkMergePipelineCaches},
    {"vkMergeValidationCachesEXT", vkMergeValidationCachesEXT},
    {"vkQueueBeginDebugUtilsLabelEXT", vkQueueBeginDebugUtilsLabelEXT},
    {"vkQueueBindSparse", vkQueueBindSparse},
    {"vkQueueEndDebugUtilsLabelEXT", vkQueueEndDebugUtilsLabelEXT},
    {"vkQueueInsertDebugUtilsLabelEXT", vkQueueInsertDebugUtilsLabelEXT},
    {"vkQueueNotifyOutOfBandNV", vkQueueNotifyOutOfBandNV},
    {"vkQueuePresentKHR", vkQueuePresentKHR},
    {"vkQueueSubmit", vkQueueSubmit},
    {"vkQueueSubmit2", vkQueueSubmit2},
    {"vkQueueSubmit2KHR", vkQueueSubmit2KHR},
    {"vkQueueWaitIdle", vkQueueWaitIdle},
    {"vkReleaseCapturedPipelineDataKHR", vkReleaseCapturedPipelineDataKHR},
    {"vkReleaseProfilingLockKHR", vkReleaseProfilingLockKHR},
    {"vkReleaseSwapchainImagesEXT", vkReleaseSwapchainImagesEXT},
    {"vkResetCommandBuffer", vkResetCommandBuffer},
    {"vkResetCommandPool", vkResetCommandPool},
    {"vkResetDescriptorPool", vkResetDescriptorPool},
    {"vkResetEvent", vkResetEvent},
    {"vkResetFences", vkResetFences},
    {"vkResetQueryPool", vkResetQueryPool},
    {"vkResetQueryPoolEXT", vkResetQueryPoolEXT},
    {"vkSetDebugUtilsObjectNameEXT", vkSetDebugUtilsObjectNameEXT},
    {"vkSetDebugUtilsObjectTagEXT", vkSetDebugUtilsObjectTagEXT},
    {"vkSetDeviceMemoryPriorityEXT", vkSetDeviceMemoryPriorityEXT},
    {"vkSetEvent", vkSetEvent},
    {"vkSetHdrMetadataEXT", vkSetHdrMetadataEXT},
    {"vkSetLatencyMarkerNV", vkSetLatencyMarkerNV},
    {"vkSetLatencySleepModeNV", vkSetLatencySleepModeNV},
    {"vkSetPrivateData", vkSetPrivateData},
    {"vkSetPrivateDataEXT", vkSetPrivateDataEXT},
    {"vkSignalSemaphore", vkSignalSemaphore},
    {"vkSignalSemaphoreKHR", vkSignalSemaphoreKHR},
    {"vkTransitionImageLayout", vkTransitionImageLayout},
    {"vkTransitionImageLayoutEXT", vkTransitionImageLayoutEXT},
    {"vkTrimCommandPool", vkTrimCommandPool},
    {"vkTrimCommandPoolKHR", vkTrimCommandPoolKHR},
    {"vkUnmapMemory", vkUnmapMemory},
    {"vkUnmapMemory2", vkUnmapMemory2},
    {"vkUnmapMemory2KHR", vkUnmapMemory2KHR},
    {"vkUpdateDescriptorSetWithTemplate", vkUpdateDescriptorSetWithTemplate},
    {"vkUpdateDescriptorSetWithTemplateKHR", vkUpdateDescriptorSetWithTemplateKHR},
    {"vkUpdateDescriptorSets", vkUpdateDescriptorSets},
    {"vkUpdateIndirectExecutionSetPipelineEXT", vkUpdateIndirectExecutionSetPipelineEXT},
    {"vkUpdateIndirectExecutionSetShaderEXT", vkUpdateIndirectExecutionSetShaderEXT},
    {"vkUpdateVideoSessionParametersKHR", vkUpdateVideoSessionParametersKHR},
    {"vkWaitForFences", vkWaitForFences},
    {"vkWaitForPresentKHR", vkWaitForPresentKHR},
    {"vkWaitSemaphores", vkWaitSemaphores},
    {"vkWaitSemaphoresKHR", vkWaitSemaphoresKHR},
    {"vkWriteAccelerationStructuresPropertiesKHR", vkWriteAccelerationStructuresPropertiesKHR},
    {"vkWriteMicromapsPropertiesEXT", vkWriteMicromapsPropertiesEXT},
};

static const struct vulkan_func vk_phys_dev_dispatch_table[] =
{
    {"vkCreateDevice", vkCreateDevice},
    {"vkEnumerateDeviceExtensionProperties", vkEnumerateDeviceExtensionProperties},
    {"vkEnumerateDeviceLayerProperties", vkEnumerateDeviceLayerProperties},
    {"vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR", vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR},
    {"vkGetPhysicalDeviceCalibrateableTimeDomainsEXT", vkGetPhysicalDeviceCalibrateableTimeDomainsEXT},
    {"vkGetPhysicalDeviceCalibrateableTimeDomainsKHR", vkGetPhysicalDeviceCalibrateableTimeDomainsKHR},
    {"vkGetPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV", vkGetPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV},
    {"vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR", vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR},
    {"vkGetPhysicalDeviceCooperativeMatrixPropertiesNV", vkGetPhysicalDeviceCooperativeMatrixPropertiesNV},
    {"vkGetPhysicalDeviceCooperativeVectorPropertiesNV", vkGetPhysicalDeviceCooperativeVectorPropertiesNV},
    {"vkGetPhysicalDeviceExternalBufferProperties", vkGetPhysicalDeviceExternalBufferProperties},
    {"vkGetPhysicalDeviceExternalBufferPropertiesKHR", vkGetPhysicalDeviceExternalBufferPropertiesKHR},
    {"vkGetPhysicalDeviceExternalFenceProperties", vkGetPhysicalDeviceExternalFenceProperties},
    {"vkGetPhysicalDeviceExternalFencePropertiesKHR", vkGetPhysicalDeviceExternalFencePropertiesKHR},
    {"vkGetPhysicalDeviceExternalSemaphoreProperties", vkGetPhysicalDeviceExternalSemaphoreProperties},
    {"vkGetPhysicalDeviceExternalSemaphorePropertiesKHR", vkGetPhysicalDeviceExternalSemaphorePropertiesKHR},
    {"vkGetPhysicalDeviceFeatures", vkGetPhysicalDeviceFeatures},
    {"vkGetPhysicalDeviceFeatures2", vkGetPhysicalDeviceFeatures2},
    {"vkGetPhysicalDeviceFeatures2KHR", vkGetPhysicalDeviceFeatures2KHR},
    {"vkGetPhysicalDeviceFormatProperties", vkGetPhysicalDeviceFormatProperties},
    {"vkGetPhysicalDeviceFormatProperties2", vkGetPhysicalDeviceFormatProperties2},
    {"vkGetPhysicalDeviceFormatProperties2KHR", vkGetPhysicalDeviceFormatProperties2KHR},
    {"vkGetPhysicalDeviceFragmentShadingRatesKHR", vkGetPhysicalDeviceFragmentShadingRatesKHR},
    {"vkGetPhysicalDeviceImageFormatProperties", vkGetPhysicalDeviceImageFormatProperties},
    {"vkGetPhysicalDeviceImageFormatProperties2", vkGetPhysicalDeviceImageFormatProperties2},
    {"vkGetPhysicalDeviceImageFormatProperties2KHR", vkGetPhysicalDeviceImageFormatProperties2KHR},
    {"vkGetPhysicalDeviceMemoryProperties", vkGetPhysicalDeviceMemoryProperties},
    {"vkGetPhysicalDeviceMemoryProperties2", vkGetPhysicalDeviceMemoryProperties2},
    {"vkGetPhysicalDeviceMemoryProperties2KHR", vkGetPhysicalDeviceMemoryProperties2KHR},
    {"vkGetPhysicalDeviceMultisamplePropertiesEXT", vkGetPhysicalDeviceMultisamplePropertiesEXT},
    {"vkGetPhysicalDeviceOpticalFlowImageFormatsNV", vkGetPhysicalDeviceOpticalFlowImageFormatsNV},
    {"vkGetPhysicalDevicePresentRectanglesKHR", vkGetPhysicalDevicePresentRectanglesKHR},
    {"vkGetPhysicalDeviceProperties", vkGetPhysicalDeviceProperties},
    {"vkGetPhysicalDeviceProperties2", vkGetPhysicalDeviceProperties2},
    {"vkGetPhysicalDeviceProperties2KHR", vkGetPhysicalDeviceProperties2KHR},
    {"vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR", vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR},
    {"vkGetPhysicalDeviceQueueFamilyProperties", vkGetPhysicalDeviceQueueFamilyProperties},
    {"vkGetPhysicalDeviceQueueFamilyProperties2", vkGetPhysicalDeviceQueueFamilyProperties2},
    {"vkGetPhysicalDeviceQueueFamilyProperties2KHR", vkGetPhysicalDeviceQueueFamilyProperties2KHR},
    {"vkGetPhysicalDeviceSparseImageFormatProperties", vkGetPhysicalDeviceSparseImageFormatProperties},
    {"vkGetPhysicalDeviceSparseImageFormatProperties2", vkGetPhysicalDeviceSparseImageFormatProperties2},
    {"vkGetPhysicalDeviceSparseImageFormatProperties2KHR", vkGetPhysicalDeviceSparseImageFormatProperties2KHR},
    {"vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV", vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV},
    {"vkGetPhysicalDeviceSurfaceCapabilities2KHR", vkGetPhysicalDeviceSurfaceCapabilities2KHR},
    {"vkGetPhysicalDeviceSurfaceCapabilitiesKHR", vkGetPhysicalDeviceSurfaceCapabilitiesKHR},
    {"vkGetPhysicalDeviceSurfaceFormats2KHR", vkGetPhysicalDeviceSurfaceFormats2KHR},
    {"vkGetPhysicalDeviceSurfaceFormatsKHR", vkGetPhysicalDeviceSurfaceFormatsKHR},
    {"vkGetPhysicalDeviceSurfacePresentModesKHR", vkGetPhysicalDeviceSurfacePresentModesKHR},
    {"vkGetPhysicalDeviceSurfaceSupportKHR", vkGetPhysicalDeviceSurfaceSupportKHR},
    {"vkGetPhysicalDeviceToolProperties", vkGetPhysicalDeviceToolProperties},
    {"vkGetPhysicalDeviceToolPropertiesEXT", vkGetPhysicalDeviceToolPropertiesEXT},
    {"vkGetPhysicalDeviceVideoCapabilitiesKHR", vkGetPhysicalDeviceVideoCapabilitiesKHR},
    {"vkGetPhysicalDeviceVideoEncodeQualityLevelPropertiesKHR", vkGetPhysicalDeviceVideoEncodeQualityLevelPropertiesKHR},
    {"vkGetPhysicalDeviceVideoFormatPropertiesKHR", vkGetPhysicalDeviceVideoFormatPropertiesKHR},
    {"vkGetPhysicalDeviceWin32PresentationSupportKHR", vkGetPhysicalDeviceWin32PresentationSupportKHR},
};

static const struct vulkan_func vk_instance_dispatch_table[] =
{
    {"vkCreateDebugReportCallbackEXT", vkCreateDebugReportCallbackEXT},
    {"vkCreateDebugUtilsMessengerEXT", vkCreateDebugUtilsMessengerEXT},
    {"vkCreateWin32SurfaceKHR", vkCreateWin32SurfaceKHR},
    {"vkDebugReportMessageEXT", vkDebugReportMessageEXT},
    {"vkDestroyDebugReportCallbackEXT", vkDestroyDebugReportCallbackEXT},
    {"vkDestroyDebugUtilsMessengerEXT", vkDestroyDebugUtilsMessengerEXT},
    {"vkDestroyInstance", vkDestroyInstance},
    {"vkDestroySurfaceKHR", vkDestroySurfaceKHR},
    {"vkEnumeratePhysicalDeviceGroups", vkEnumeratePhysicalDeviceGroups},
    {"vkEnumeratePhysicalDeviceGroupsKHR", vkEnumeratePhysicalDeviceGroupsKHR},
    {"vkEnumeratePhysicalDevices", vkEnumeratePhysicalDevices},
    {"vkSubmitDebugUtilsMessageEXT", vkSubmitDebugUtilsMessageEXT},
};

void *wine_vk_get_device_proc_addr(const char *name)
{
    unsigned int i;
    for (i = 0; i < ARRAY_SIZE(vk_device_dispatch_table); i++)
    {
        if (strcmp(vk_device_dispatch_table[i].name, name) == 0)
        {
            TRACE("Found name=%s in device table\n", debugstr_a(name));
            return vk_device_dispatch_table[i].func;
        }
    }
    return NULL;
}

void *wine_vk_get_phys_dev_proc_addr(const char *name)
{
    unsigned int i;
    for (i = 0; i < ARRAY_SIZE(vk_phys_dev_dispatch_table); i++)
    {
        if (strcmp(vk_phys_dev_dispatch_table[i].name, name) == 0)
        {
            TRACE("Found name=%s in physical device table\n", debugstr_a(name));
            return vk_phys_dev_dispatch_table[i].func;
        }
    }
    return NULL;
}

void *wine_vk_get_instance_proc_addr(const char *name)
{
    unsigned int i;
    for (i = 0; i < ARRAY_SIZE(vk_instance_dispatch_table); i++)
    {
        if (strcmp(vk_instance_dispatch_table[i].name, name) == 0)
        {
            TRACE("Found name=%s in instance table\n", debugstr_a(name));
            return vk_instance_dispatch_table[i].func;
        }
    }
    return NULL;
}
