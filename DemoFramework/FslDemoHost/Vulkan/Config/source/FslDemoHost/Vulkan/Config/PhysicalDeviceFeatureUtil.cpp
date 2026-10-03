/****************************************************************************************************************************************************
 * Copyright (c) 2016 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *
 *    * Neither the name of the Freescale Semiconductor, Inc. nor the names of
 *      its contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************************************************************************************/

// Make sure Common.hpp is the first include file (to make the error message as helpful as possible when disabled)
#include <FslBase/Exceptions.hpp>
#include <FslDemoHost/Vulkan/Config/PhysicalDeviceFeatureSet.hpp>
#include <FslDemoHost/Vulkan/Config/PhysicalDeviceFeatureUtil.hpp>
#include <FslUtil/Vulkan1_0/Common.hpp>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace Fsl::Vulkan
{
  namespace
  {
    //! @brief Based on VkPhysicalDeviceFeatures
    bool TryGetOffset(const PhysicalDeviceFeature featureName, std::size_t& rOffset)
    {
      switch (featureName)
      {
      case PhysicalDeviceFeature::RobustBufferAccess:
        rOffset = offsetof(VkPhysicalDeviceFeatures, robustBufferAccess);
        return true;
      case PhysicalDeviceFeature::FullDrawIndexUint32:
        rOffset = offsetof(VkPhysicalDeviceFeatures, fullDrawIndexUint32);
        return true;
      case PhysicalDeviceFeature::ImageCubeArray:
        rOffset = offsetof(VkPhysicalDeviceFeatures, imageCubeArray);
        return true;
      case PhysicalDeviceFeature::IndependentBlend:
        rOffset = offsetof(VkPhysicalDeviceFeatures, independentBlend);
        return true;
      case PhysicalDeviceFeature::GeometryShader:
        rOffset = offsetof(VkPhysicalDeviceFeatures, geometryShader);
        return true;
      case PhysicalDeviceFeature::TessellationShader:
        rOffset = offsetof(VkPhysicalDeviceFeatures, tessellationShader);
        return true;
      case PhysicalDeviceFeature::SampleRateShading:
        rOffset = offsetof(VkPhysicalDeviceFeatures, sampleRateShading);
        return true;
      case PhysicalDeviceFeature::DualSrcBlend:
        rOffset = offsetof(VkPhysicalDeviceFeatures, dualSrcBlend);
        return true;
      case PhysicalDeviceFeature::LogicOp:
        rOffset = offsetof(VkPhysicalDeviceFeatures, logicOp);
        return true;
      case PhysicalDeviceFeature::MultiDrawIndirect:
        rOffset = offsetof(VkPhysicalDeviceFeatures, multiDrawIndirect);
        return true;
      case PhysicalDeviceFeature::DrawIndirectFirstInstance:
        rOffset = offsetof(VkPhysicalDeviceFeatures, drawIndirectFirstInstance);
        return true;
      case PhysicalDeviceFeature::DepthClamp:
        rOffset = offsetof(VkPhysicalDeviceFeatures, depthClamp);
        return true;
      case PhysicalDeviceFeature::DepthBiasClamp:
        rOffset = offsetof(VkPhysicalDeviceFeatures, depthBiasClamp);
        return true;
      case PhysicalDeviceFeature::FillModeNonSolid:
        rOffset = offsetof(VkPhysicalDeviceFeatures, fillModeNonSolid);
        return true;
      case PhysicalDeviceFeature::DepthBounds:
        rOffset = offsetof(VkPhysicalDeviceFeatures, depthBounds);
        return true;
      case PhysicalDeviceFeature::WideLines:
        rOffset = offsetof(VkPhysicalDeviceFeatures, wideLines);
        return true;
      case PhysicalDeviceFeature::LargePoints:
        rOffset = offsetof(VkPhysicalDeviceFeatures, largePoints);
        return true;
      case PhysicalDeviceFeature::AlphaToOne:
        rOffset = offsetof(VkPhysicalDeviceFeatures, alphaToOne);
        return true;
      case PhysicalDeviceFeature::MultiViewport:
        rOffset = offsetof(VkPhysicalDeviceFeatures, multiViewport);
        return true;
      case PhysicalDeviceFeature::SamplerAnisotropy:
        rOffset = offsetof(VkPhysicalDeviceFeatures, samplerAnisotropy);
        return true;
      case PhysicalDeviceFeature::TextureCompressionETC2:
        rOffset = offsetof(VkPhysicalDeviceFeatures, textureCompressionETC2);
        return true;
      case PhysicalDeviceFeature::TextureCompressionASTC_LDR:
        rOffset = offsetof(VkPhysicalDeviceFeatures, textureCompressionASTC_LDR);
        return true;
      case PhysicalDeviceFeature::TextureCompressionBC:
        rOffset = offsetof(VkPhysicalDeviceFeatures, textureCompressionBC);
        return true;
      case PhysicalDeviceFeature::OcclusionQueryPrecise:
        rOffset = offsetof(VkPhysicalDeviceFeatures, occlusionQueryPrecise);
        return true;
      case PhysicalDeviceFeature::PipelineStatisticsQuery:
        rOffset = offsetof(VkPhysicalDeviceFeatures, pipelineStatisticsQuery);
        return true;
      case PhysicalDeviceFeature::VertexPipelineStoresAndAtomics:
        rOffset = offsetof(VkPhysicalDeviceFeatures, vertexPipelineStoresAndAtomics);
        return true;
      case PhysicalDeviceFeature::FragmentStoresAndAtomics:
        rOffset = offsetof(VkPhysicalDeviceFeatures, fragmentStoresAndAtomics);
        return true;
      case PhysicalDeviceFeature::ShaderTessellationAndGeometryPointSize:
        rOffset = offsetof(VkPhysicalDeviceFeatures, shaderTessellationAndGeometryPointSize);
        return true;
      case PhysicalDeviceFeature::ShaderImageGatherExtended:
        rOffset = offsetof(VkPhysicalDeviceFeatures, shaderImageGatherExtended);
        return true;
      case PhysicalDeviceFeature::ShaderStorageImageExtendedFormats:
        rOffset = offsetof(VkPhysicalDeviceFeatures, shaderStorageImageExtendedFormats);
        return true;
      case PhysicalDeviceFeature::ShaderStorageImageMultisample:
        rOffset = offsetof(VkPhysicalDeviceFeatures, shaderStorageImageMultisample);
        return true;
      case PhysicalDeviceFeature::ShaderStorageImageReadWithoutFormat:
        rOffset = offsetof(VkPhysicalDeviceFeatures, shaderStorageImageReadWithoutFormat);
        return true;
      case PhysicalDeviceFeature::ShaderStorageImageWriteWithoutFormat:
        rOffset = offsetof(VkPhysicalDeviceFeatures, shaderStorageImageWriteWithoutFormat);
        return true;
      case PhysicalDeviceFeature::ShaderUniformBufferArrayDynamicIndexing:
        rOffset = offsetof(VkPhysicalDeviceFeatures, shaderUniformBufferArrayDynamicIndexing);
        return true;
      case PhysicalDeviceFeature::ShaderSampledImageArrayDynamicIndexing:
        rOffset = offsetof(VkPhysicalDeviceFeatures, shaderSampledImageArrayDynamicIndexing);
        return true;
      case PhysicalDeviceFeature::ShaderStorageBufferArrayDynamicIndexing:
        rOffset = offsetof(VkPhysicalDeviceFeatures, shaderStorageBufferArrayDynamicIndexing);
        return true;
      case PhysicalDeviceFeature::ShaderStorageImageArrayDynamicIndexing:
        rOffset = offsetof(VkPhysicalDeviceFeatures, shaderStorageImageArrayDynamicIndexing);
        return true;
      case PhysicalDeviceFeature::ShaderClipDistance:
        rOffset = offsetof(VkPhysicalDeviceFeatures, shaderClipDistance);
        return true;
      case PhysicalDeviceFeature::ShaderCullDistance:
        rOffset = offsetof(VkPhysicalDeviceFeatures, shaderCullDistance);
        return true;
      case PhysicalDeviceFeature::ShaderFloat64:
        rOffset = offsetof(VkPhysicalDeviceFeatures, shaderFloat64);
        return true;
      case PhysicalDeviceFeature::ShaderInt64:
        rOffset = offsetof(VkPhysicalDeviceFeatures, shaderInt64);
        return true;
      case PhysicalDeviceFeature::ShaderInt16:
        rOffset = offsetof(VkPhysicalDeviceFeatures, shaderInt16);
        return true;
      case PhysicalDeviceFeature::ShaderResourceResidency:
        rOffset = offsetof(VkPhysicalDeviceFeatures, shaderResourceResidency);
        return true;
      case PhysicalDeviceFeature::ShaderResourceMinLod:
        rOffset = offsetof(VkPhysicalDeviceFeatures, shaderResourceMinLod);
        return true;
      case PhysicalDeviceFeature::SparseBinding:
        rOffset = offsetof(VkPhysicalDeviceFeatures, sparseBinding);
        return true;
      case PhysicalDeviceFeature::SparseResidencyBuffer:
        rOffset = offsetof(VkPhysicalDeviceFeatures, sparseResidencyBuffer);
        return true;
      case PhysicalDeviceFeature::SparseResidencyImage2D:
        rOffset = offsetof(VkPhysicalDeviceFeatures, sparseResidencyImage2D);
        return true;
      case PhysicalDeviceFeature::SparseResidencyImage3D:
        rOffset = offsetof(VkPhysicalDeviceFeatures, sparseResidencyImage3D);
        return true;
      case PhysicalDeviceFeature::SparseResidency2Samples:
        rOffset = offsetof(VkPhysicalDeviceFeatures, sparseResidency2Samples);
        return true;
      case PhysicalDeviceFeature::SparseResidency4Samples:
        rOffset = offsetof(VkPhysicalDeviceFeatures, sparseResidency4Samples);
        return true;
      case PhysicalDeviceFeature::SparseResidency8Samples:
        rOffset = offsetof(VkPhysicalDeviceFeatures, sparseResidency8Samples);
        return true;
      case PhysicalDeviceFeature::SparseResidency16Samples:
        rOffset = offsetof(VkPhysicalDeviceFeatures, sparseResidency16Samples);
        return true;
      case PhysicalDeviceFeature::SparseResidencyAliased:
        rOffset = offsetof(VkPhysicalDeviceFeatures, sparseResidencyAliased);
        return true;
      case PhysicalDeviceFeature::VariableMultisampleRate:
        rOffset = offsetof(VkPhysicalDeviceFeatures, variableMultisampleRate);
        return true;
      case PhysicalDeviceFeature::InheritedQueries:
        rOffset = offsetof(VkPhysicalDeviceFeatures, inheritedQueries);
        return true;
      case PhysicalDeviceFeature::Invalid:
      default:
        rOffset = 0;
        return false;
      }
    }

    //! @brief Based on VkPhysicalDeviceVulkan11Features
    bool TryGetOffset11(const PhysicalDeviceFeature featureName, std::size_t& rOffset)
    {
      switch (featureName)
      {
      case PhysicalDeviceFeature::StorageBuffer16BitAccess:
        rOffset = offsetof(VkPhysicalDeviceVulkan11Features, storageBuffer16BitAccess);
        return true;
      case PhysicalDeviceFeature::UniformAndStorageBuffer16BitAccess:
        rOffset = offsetof(VkPhysicalDeviceVulkan11Features, uniformAndStorageBuffer16BitAccess);
        return true;
      case PhysicalDeviceFeature::StoragePushConstant16:
        rOffset = offsetof(VkPhysicalDeviceVulkan11Features, storagePushConstant16);
        return true;
      case PhysicalDeviceFeature::StorageInputOutput16:
        rOffset = offsetof(VkPhysicalDeviceVulkan11Features, storageInputOutput16);
        return true;
      case PhysicalDeviceFeature::Multiview:
        rOffset = offsetof(VkPhysicalDeviceVulkan11Features, multiview);
        return true;
      case PhysicalDeviceFeature::MultiviewGeometryShader:
        rOffset = offsetof(VkPhysicalDeviceVulkan11Features, multiviewGeometryShader);
        return true;
      case PhysicalDeviceFeature::MultiviewTessellationShader:
        rOffset = offsetof(VkPhysicalDeviceVulkan11Features, multiviewTessellationShader);
        return true;
      case PhysicalDeviceFeature::VariablePointersStorageBuffer:
        rOffset = offsetof(VkPhysicalDeviceVulkan11Features, variablePointersStorageBuffer);
        return true;
      case PhysicalDeviceFeature::VariablePointers:
        rOffset = offsetof(VkPhysicalDeviceVulkan11Features, variablePointers);
        return true;
      case PhysicalDeviceFeature::ProtectedMemory:
        rOffset = offsetof(VkPhysicalDeviceVulkan11Features, protectedMemory);
        return true;
      case PhysicalDeviceFeature::SamplerYcbcrConversion:
        rOffset = offsetof(VkPhysicalDeviceVulkan11Features, samplerYcbcrConversion);
        return true;
      case PhysicalDeviceFeature::ShaderDrawParameters:
        rOffset = offsetof(VkPhysicalDeviceVulkan11Features, shaderDrawParameters);
        return true;
      default:
        rOffset = 0;
        return false;
      }
    }


    //! @brief Based on VkPhysicalDeviceVulkan12Features
    bool TryGetOffset12(const PhysicalDeviceFeature featureName, std::size_t& rOffset)
    {
      switch (featureName)
      {
      case PhysicalDeviceFeature::SamplerMirrorClampToEdge:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, samplerMirrorClampToEdge);
        return true;
      case PhysicalDeviceFeature::DrawIndirectCount:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, drawIndirectCount);
        return true;
      case PhysicalDeviceFeature::StorageBuffer8BitAccess:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, storageBuffer8BitAccess);
        return true;
      case PhysicalDeviceFeature::UniformAndStorageBuffer8BitAccess:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, uniformAndStorageBuffer8BitAccess);
        return true;
      case PhysicalDeviceFeature::StoragePushConstant8:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, storagePushConstant8);
        return true;
      case PhysicalDeviceFeature::ShaderBufferInt64Atomics:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, shaderBufferInt64Atomics);
        return true;
      case PhysicalDeviceFeature::ShaderSharedInt64Atomics:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, shaderSharedInt64Atomics);
        return true;
      case PhysicalDeviceFeature::ShaderFloat16:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, shaderFloat16);
        return true;
      case PhysicalDeviceFeature::ShaderInt8:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, shaderInt8);
        return true;
      case PhysicalDeviceFeature::DescriptorIndexing:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, descriptorIndexing);
        return true;
      case PhysicalDeviceFeature::ShaderInputAttachmentArrayDynamicIndexing:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, shaderInputAttachmentArrayDynamicIndexing);
        return true;
      case PhysicalDeviceFeature::ShaderUniformTexelBufferArrayDynamicIndexing:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, shaderUniformTexelBufferArrayDynamicIndexing);
        return true;
      case PhysicalDeviceFeature::ShaderStorageTexelBufferArrayDynamicIndexing:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, shaderStorageTexelBufferArrayDynamicIndexing);
        return true;
      case PhysicalDeviceFeature::ShaderUniformBufferArrayNonUniformIndexing:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, shaderUniformBufferArrayNonUniformIndexing);
        return true;
      case PhysicalDeviceFeature::ShaderSampledImageArrayNonUniformIndexing:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, shaderSampledImageArrayNonUniformIndexing);
        return true;
      case PhysicalDeviceFeature::ShaderStorageBufferArrayNonUniformIndexing:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, shaderStorageBufferArrayNonUniformIndexing);
        return true;
      case PhysicalDeviceFeature::ShaderStorageImageArrayNonUniformIndexing:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, shaderStorageImageArrayNonUniformIndexing);
        return true;
      case PhysicalDeviceFeature::ShaderInputAttachmentArrayNonUniformIndexing:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, shaderInputAttachmentArrayNonUniformIndexing);
        return true;
      case PhysicalDeviceFeature::ShaderUniformTexelBufferArrayNonUniformIndexing:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, shaderUniformTexelBufferArrayNonUniformIndexing);
        return true;
      case PhysicalDeviceFeature::ShaderStorageTexelBufferArrayNonUniformIndexing:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, shaderStorageTexelBufferArrayNonUniformIndexing);
        return true;
      case PhysicalDeviceFeature::DescriptorBindingUniformBufferUpdateAfterBind:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, descriptorBindingUniformBufferUpdateAfterBind);
        return true;
      case PhysicalDeviceFeature::DescriptorBindingSampledImageUpdateAfterBind:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, descriptorBindingSampledImageUpdateAfterBind);
        return true;
      case PhysicalDeviceFeature::DescriptorBindingStorageImageUpdateAfterBind:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, descriptorBindingStorageImageUpdateAfterBind);
        return true;
      case PhysicalDeviceFeature::DescriptorBindingStorageBufferUpdateAfterBind:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, descriptorBindingStorageBufferUpdateAfterBind);
        return true;
      case PhysicalDeviceFeature::DescriptorBindingUniformTexelBufferUpdateAfterBind:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, descriptorBindingUniformTexelBufferUpdateAfterBind);
        return true;
      case PhysicalDeviceFeature::DescriptorBindingStorageTexelBufferUpdateAfterBind:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, descriptorBindingStorageTexelBufferUpdateAfterBind);
        return true;
      case PhysicalDeviceFeature::DescriptorBindingUpdateUnusedWhilePending:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, descriptorBindingUpdateUnusedWhilePending);
        return true;
      case PhysicalDeviceFeature::DescriptorBindingPartiallyBound:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, descriptorBindingPartiallyBound);
        return true;
      case PhysicalDeviceFeature::DescriptorBindingVariableDescriptorCount:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, descriptorBindingVariableDescriptorCount);
        return true;
      case PhysicalDeviceFeature::RuntimeDescriptorArray:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, runtimeDescriptorArray);
        return true;
      case PhysicalDeviceFeature::SamplerFilterMinmax:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, samplerFilterMinmax);
        return true;
      case PhysicalDeviceFeature::ScalarBlockLayout:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, scalarBlockLayout);
        return true;
      case PhysicalDeviceFeature::ImagelessFramebuffer:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, imagelessFramebuffer);
        return true;
      case PhysicalDeviceFeature::UniformBufferStandardLayout:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, uniformBufferStandardLayout);
        return true;
      case PhysicalDeviceFeature::ShaderSubgroupExtendedTypes:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, shaderSubgroupExtendedTypes);
        return true;
      case PhysicalDeviceFeature::SeparateDepthStencilLayouts:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, separateDepthStencilLayouts);
        return true;
      case PhysicalDeviceFeature::HostQueryReset:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, hostQueryReset);
        return true;
      case PhysicalDeviceFeature::TimelineSemaphore:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, timelineSemaphore);
        return true;
      case PhysicalDeviceFeature::BufferDeviceAddress:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, bufferDeviceAddress);
        return true;
      case PhysicalDeviceFeature::BufferDeviceAddressCaptureReplay:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, bufferDeviceAddressCaptureReplay);
        return true;
      case PhysicalDeviceFeature::BufferDeviceAddressMultiDevice:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, bufferDeviceAddressMultiDevice);
        return true;
      case PhysicalDeviceFeature::VulkanMemoryModel:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, vulkanMemoryModel);
        return true;
      case PhysicalDeviceFeature::VulkanMemoryModelDeviceScope:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, vulkanMemoryModelDeviceScope);
        return true;
      case PhysicalDeviceFeature::VulkanMemoryModelAvailabilityVisibilityChains:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, vulkanMemoryModelAvailabilityVisibilityChains);
        return true;
      case PhysicalDeviceFeature::ShaderOutputViewportIndex:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, shaderOutputViewportIndex);
        return true;
      case PhysicalDeviceFeature::ShaderOutputLayer:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, shaderOutputLayer);
        return true;
      case PhysicalDeviceFeature::SubgroupBroadcastDynamicId:
        rOffset = offsetof(VkPhysicalDeviceVulkan12Features, subgroupBroadcastDynamicId);
        return true;
      default:
        rOffset = 0;
        return false;
      }
    }


    //! @brief Based on VkPhysicalDeviceVulkan13Features
    bool TryGetOffset13(const PhysicalDeviceFeature featureName, std::size_t& rOffset)
    {
      switch (featureName)
      {
      case PhysicalDeviceFeature::RobustImageAccess:
        rOffset = offsetof(VkPhysicalDeviceVulkan13Features, robustImageAccess);
        return true;
      case PhysicalDeviceFeature::InlineUniformBlock:
        rOffset = offsetof(VkPhysicalDeviceVulkan13Features, inlineUniformBlock);
        return true;
      case PhysicalDeviceFeature::DescriptorBindingInlineUniformBlockUpdateAfterBind:
        rOffset = offsetof(VkPhysicalDeviceVulkan13Features, descriptorBindingInlineUniformBlockUpdateAfterBind);
        return true;
      case PhysicalDeviceFeature::PipelineCreationCacheControl:
        rOffset = offsetof(VkPhysicalDeviceVulkan13Features, pipelineCreationCacheControl);
        return true;
      case PhysicalDeviceFeature::PrivateData:
        rOffset = offsetof(VkPhysicalDeviceVulkan13Features, privateData);
        return true;
      case PhysicalDeviceFeature::ShaderDemoteToHelperInvocation:
        rOffset = offsetof(VkPhysicalDeviceVulkan13Features, shaderDemoteToHelperInvocation);
        return true;
      case PhysicalDeviceFeature::ShaderTerminateInvocation:
        rOffset = offsetof(VkPhysicalDeviceVulkan13Features, shaderTerminateInvocation);
        return true;
      case PhysicalDeviceFeature::SubgroupSizeControl:
        rOffset = offsetof(VkPhysicalDeviceVulkan13Features, subgroupSizeControl);
        return true;
      case PhysicalDeviceFeature::ComputeFullSubgroups:
        rOffset = offsetof(VkPhysicalDeviceVulkan13Features, computeFullSubgroups);
        return true;
      case PhysicalDeviceFeature::Synchronization2:
        rOffset = offsetof(VkPhysicalDeviceVulkan13Features, synchronization2);
        return true;
      case PhysicalDeviceFeature::TextureCompressionASTC_HDR:
        rOffset = offsetof(VkPhysicalDeviceVulkan13Features, textureCompressionASTC_HDR);
        return true;
      case PhysicalDeviceFeature::ShaderZeroInitializeWorkgroupMemory:
        rOffset = offsetof(VkPhysicalDeviceVulkan13Features, shaderZeroInitializeWorkgroupMemory);
        return true;
      case PhysicalDeviceFeature::DynamicRendering:
        rOffset = offsetof(VkPhysicalDeviceVulkan13Features, dynamicRendering);
        return true;
      case PhysicalDeviceFeature::ShaderIntegerDotProduct:
        rOffset = offsetof(VkPhysicalDeviceVulkan13Features, shaderIntegerDotProduct);
        return true;
      case PhysicalDeviceFeature::Maintenance4:
        rOffset = offsetof(VkPhysicalDeviceVulkan13Features, maintenance4);
        return true;
      default:
        rOffset = 0;
        return false;
      }
    }

    //! @brief Locate the feature member in the feature set
    //! @return the member or nullptr if the feature is unknown
    template <typename TByte, typename TFeatureSet>
    TByte* TryGetMember(TFeatureSet& rFeatures, const PhysicalDeviceFeature featureName)
    {
      std::size_t offset = 0;
      if (TryGetOffset(featureName, offset))
      {
        return reinterpret_cast<TByte*>(&rFeatures.Features) + offset;
      }
      if (TryGetOffset11(featureName, offset))
      {
        return reinterpret_cast<TByte*>(&rFeatures.Features11) + offset;
      }
      if (TryGetOffset12(featureName, offset))
      {
        return reinterpret_cast<TByte*>(&rFeatures.Features12) + offset;
      }
      if (TryGetOffset13(featureName, offset))
      {
        return reinterpret_cast<TByte*>(&rFeatures.Features13) + offset;
      }
      return nullptr;
    }
  }


  VkBool32 PhysicalDeviceFeatureUtil::Get(const VkPhysicalDeviceFeatures& features, const PhysicalDeviceFeature featureName)
  {
    std::size_t offset = 0;
    if (!TryGetOffset(featureName, offset))
    {
      throw std::invalid_argument("Unknown Vulkan PhysicalDeviceFeature");
    }

    // Nasty lookup that breaks without warning if the type changes from VkBool32 in VkPhysicalDeviceFeatures (this is the reason we have the static
    // asserts at the bottom of this file)
    const auto* const pMember = reinterpret_cast<const VkBool32*>((reinterpret_cast<const uint8_t*>(&features) + offset));
    return *pMember;
  }


  void PhysicalDeviceFeatureUtil::Set(VkPhysicalDeviceFeatures& rFeatures, const PhysicalDeviceFeature featureName, const VkBool32 value)
  {
    std::size_t offset = 0;
    if (!TryGetOffset(featureName, offset))
    {
      throw std::invalid_argument("Unknown Vulkan PhysicalDeviceFeature");
    }

    // Nasty lookup that breaks without warning if the type changes from VkBool32 in VkPhysicalDeviceFeatures (this is the reason we have the static
    // asserts at the bottom of this file)
    auto* pMember = reinterpret_cast<VkBool32*>((reinterpret_cast<uint8_t*>(&rFeatures) + offset));
    *pMember = value;
  }


  VkBool32 PhysicalDeviceFeatureUtil::Get(const PhysicalDeviceFeatureSet& features, const PhysicalDeviceFeature featureName)
  {
    const auto* const pMember = TryGetMember<const uint8_t>(features, featureName);
    if (pMember == nullptr)
    {
      throw std::invalid_argument("Unknown Vulkan PhysicalDeviceFeature");
    }
    // Nasty lookup that breaks without warning if the member type changes from VkBool32 (this is the reason we have the static asserts at the
    // bottom of this file)
    return *reinterpret_cast<const VkBool32*>(pMember);
  }


  void PhysicalDeviceFeatureUtil::Set(PhysicalDeviceFeatureSet& rFeatures, const PhysicalDeviceFeature featureName, const VkBool32 value)
  {
    auto* const pMember = TryGetMember<uint8_t>(rFeatures, featureName);
    if (pMember == nullptr)
    {
      throw std::invalid_argument("Unknown Vulkan PhysicalDeviceFeature");
    }
    // Nasty lookup that breaks without warning if the member type changes from VkBool32 (this is the reason we have the static asserts at the
    // bottom of this file)
    *reinterpret_cast<VkBool32*>(pMember) = value;
  }


  const char* PhysicalDeviceFeatureUtil::ToString(const PhysicalDeviceFeature featureName)
  {
    switch (featureName)
    {
    case PhysicalDeviceFeature::RobustBufferAccess:
      return "RobustBufferAccess";
    case PhysicalDeviceFeature::FullDrawIndexUint32:
      return "FullDrawIndexUint32";
    case PhysicalDeviceFeature::ImageCubeArray:
      return "ImageCubeArray";
    case PhysicalDeviceFeature::IndependentBlend:
      return "IndependentBlend";
    case PhysicalDeviceFeature::GeometryShader:
      return "GeometryShader";
    case PhysicalDeviceFeature::TessellationShader:
      return "TessellationShader";
    case PhysicalDeviceFeature::SampleRateShading:
      return "SampleRateShading";
    case PhysicalDeviceFeature::DualSrcBlend:
      return "DualSrcBlend";
    case PhysicalDeviceFeature::LogicOp:
      return "LogicOp";
    case PhysicalDeviceFeature::MultiDrawIndirect:
      return "MultiDrawIndirect";
    case PhysicalDeviceFeature::DrawIndirectFirstInstance:
      return "DrawIndirectFirstInstance";
    case PhysicalDeviceFeature::DepthClamp:
      return "DepthClamp";
    case PhysicalDeviceFeature::DepthBiasClamp:
      return "DepthBiasClamp";
    case PhysicalDeviceFeature::FillModeNonSolid:
      return "FillModeNonSolid";
    case PhysicalDeviceFeature::DepthBounds:
      return "DepthBounds";
    case PhysicalDeviceFeature::WideLines:
      return "WideLines";
    case PhysicalDeviceFeature::LargePoints:
      return "LargePoints";
    case PhysicalDeviceFeature::AlphaToOne:
      return "AlphaToOne";
    case PhysicalDeviceFeature::MultiViewport:
      return "MultiViewport";
    case PhysicalDeviceFeature::SamplerAnisotropy:
      return "SamplerAnisotropy";
    case PhysicalDeviceFeature::TextureCompressionETC2:
      return "TextureCompressionETC2";
    case PhysicalDeviceFeature::TextureCompressionASTC_LDR:
      return "TextureCompressionASTC_LDR";
    case PhysicalDeviceFeature::TextureCompressionBC:
      return "TextureCompressionBC";
    case PhysicalDeviceFeature::OcclusionQueryPrecise:
      return "OcclusionQueryPrecise";
    case PhysicalDeviceFeature::PipelineStatisticsQuery:
      return "PipelineStatisticsQuery";
    case PhysicalDeviceFeature::VertexPipelineStoresAndAtomics:
      return "VertexPipelineStoresAndAtomics";
    case PhysicalDeviceFeature::FragmentStoresAndAtomics:
      return "FragmentStoresAndAtomics";
    case PhysicalDeviceFeature::ShaderTessellationAndGeometryPointSize:
      return "ShaderTessellationAndGeometryPointSize";
    case PhysicalDeviceFeature::ShaderImageGatherExtended:
      return "ShaderImageGatherExtended";
    case PhysicalDeviceFeature::ShaderStorageImageExtendedFormats:
      return "ShaderStorageImageExtendedFormats";
    case PhysicalDeviceFeature::ShaderStorageImageMultisample:
      return "ShaderStorageImageMultisample";
    case PhysicalDeviceFeature::ShaderStorageImageReadWithoutFormat:
      return "ShaderStorageImageReadWithoutFormat";
    case PhysicalDeviceFeature::ShaderStorageImageWriteWithoutFormat:
      return "ShaderStorageImageWriteWithoutFormat";
    case PhysicalDeviceFeature::ShaderUniformBufferArrayDynamicIndexing:
      return "ShaderUniformBufferArrayDynamicIndexing";
    case PhysicalDeviceFeature::ShaderSampledImageArrayDynamicIndexing:
      return "ShaderSampledImageArrayDynamicIndexing";
    case PhysicalDeviceFeature::ShaderStorageBufferArrayDynamicIndexing:
      return "ShaderStorageBufferArrayDynamicIndexing";
    case PhysicalDeviceFeature::ShaderStorageImageArrayDynamicIndexing:
      return "ShaderStorageImageArrayDynamicIndexing";
    case PhysicalDeviceFeature::ShaderClipDistance:
      return "ShaderClipDistance";
    case PhysicalDeviceFeature::ShaderCullDistance:
      return "ShaderCullDistance";
    case PhysicalDeviceFeature::ShaderFloat64:
      return "ShaderFloat64";
    case PhysicalDeviceFeature::ShaderInt64:
      return "ShaderInt64";
    case PhysicalDeviceFeature::ShaderInt16:
      return "ShaderInt16";
    case PhysicalDeviceFeature::ShaderResourceResidency:
      return "ShaderResourceResidency";
    case PhysicalDeviceFeature::ShaderResourceMinLod:
      return "ShaderResourceMinLod";
    case PhysicalDeviceFeature::SparseBinding:
      return "SparseBinding";
    case PhysicalDeviceFeature::SparseResidencyBuffer:
      return "SparseResidencyBuffer";
    case PhysicalDeviceFeature::SparseResidencyImage2D:
      return "SparseResidencyImage2D";
    case PhysicalDeviceFeature::SparseResidencyImage3D:
      return "SparseResidencyImage3D";
    case PhysicalDeviceFeature::SparseResidency2Samples:
      return "SparseResidency2Samples";
    case PhysicalDeviceFeature::SparseResidency4Samples:
      return "SparseResidency4Samples";
    case PhysicalDeviceFeature::SparseResidency8Samples:
      return "SparseResidency8Samples";
    case PhysicalDeviceFeature::SparseResidency16Samples:
      return "SparseResidency16Samples";
    case PhysicalDeviceFeature::SparseResidencyAliased:
      return "SparseResidencyAliased";
    case PhysicalDeviceFeature::VariableMultisampleRate:
      return "VariableMultisampleRate";
    case PhysicalDeviceFeature::InheritedQueries:
      return "InheritedQueries";
    case PhysicalDeviceFeature::StorageBuffer16BitAccess:
      return "StorageBuffer16BitAccess";
    case PhysicalDeviceFeature::UniformAndStorageBuffer16BitAccess:
      return "UniformAndStorageBuffer16BitAccess";
    case PhysicalDeviceFeature::StoragePushConstant16:
      return "StoragePushConstant16";
    case PhysicalDeviceFeature::StorageInputOutput16:
      return "StorageInputOutput16";
    case PhysicalDeviceFeature::Multiview:
      return "Multiview";
    case PhysicalDeviceFeature::MultiviewGeometryShader:
      return "MultiviewGeometryShader";
    case PhysicalDeviceFeature::MultiviewTessellationShader:
      return "MultiviewTessellationShader";
    case PhysicalDeviceFeature::VariablePointersStorageBuffer:
      return "VariablePointersStorageBuffer";
    case PhysicalDeviceFeature::VariablePointers:
      return "VariablePointers";
    case PhysicalDeviceFeature::ProtectedMemory:
      return "ProtectedMemory";
    case PhysicalDeviceFeature::SamplerYcbcrConversion:
      return "SamplerYcbcrConversion";
    case PhysicalDeviceFeature::ShaderDrawParameters:
      return "ShaderDrawParameters";
    case PhysicalDeviceFeature::SamplerMirrorClampToEdge:
      return "SamplerMirrorClampToEdge";
    case PhysicalDeviceFeature::DrawIndirectCount:
      return "DrawIndirectCount";
    case PhysicalDeviceFeature::StorageBuffer8BitAccess:
      return "StorageBuffer8BitAccess";
    case PhysicalDeviceFeature::UniformAndStorageBuffer8BitAccess:
      return "UniformAndStorageBuffer8BitAccess";
    case PhysicalDeviceFeature::StoragePushConstant8:
      return "StoragePushConstant8";
    case PhysicalDeviceFeature::ShaderBufferInt64Atomics:
      return "ShaderBufferInt64Atomics";
    case PhysicalDeviceFeature::ShaderSharedInt64Atomics:
      return "ShaderSharedInt64Atomics";
    case PhysicalDeviceFeature::ShaderFloat16:
      return "ShaderFloat16";
    case PhysicalDeviceFeature::ShaderInt8:
      return "ShaderInt8";
    case PhysicalDeviceFeature::DescriptorIndexing:
      return "DescriptorIndexing";
    case PhysicalDeviceFeature::ShaderInputAttachmentArrayDynamicIndexing:
      return "ShaderInputAttachmentArrayDynamicIndexing";
    case PhysicalDeviceFeature::ShaderUniformTexelBufferArrayDynamicIndexing:
      return "ShaderUniformTexelBufferArrayDynamicIndexing";
    case PhysicalDeviceFeature::ShaderStorageTexelBufferArrayDynamicIndexing:
      return "ShaderStorageTexelBufferArrayDynamicIndexing";
    case PhysicalDeviceFeature::ShaderUniformBufferArrayNonUniformIndexing:
      return "ShaderUniformBufferArrayNonUniformIndexing";
    case PhysicalDeviceFeature::ShaderSampledImageArrayNonUniformIndexing:
      return "ShaderSampledImageArrayNonUniformIndexing";
    case PhysicalDeviceFeature::ShaderStorageBufferArrayNonUniformIndexing:
      return "ShaderStorageBufferArrayNonUniformIndexing";
    case PhysicalDeviceFeature::ShaderStorageImageArrayNonUniformIndexing:
      return "ShaderStorageImageArrayNonUniformIndexing";
    case PhysicalDeviceFeature::ShaderInputAttachmentArrayNonUniformIndexing:
      return "ShaderInputAttachmentArrayNonUniformIndexing";
    case PhysicalDeviceFeature::ShaderUniformTexelBufferArrayNonUniformIndexing:
      return "ShaderUniformTexelBufferArrayNonUniformIndexing";
    case PhysicalDeviceFeature::ShaderStorageTexelBufferArrayNonUniformIndexing:
      return "ShaderStorageTexelBufferArrayNonUniformIndexing";
    case PhysicalDeviceFeature::DescriptorBindingUniformBufferUpdateAfterBind:
      return "DescriptorBindingUniformBufferUpdateAfterBind";
    case PhysicalDeviceFeature::DescriptorBindingSampledImageUpdateAfterBind:
      return "DescriptorBindingSampledImageUpdateAfterBind";
    case PhysicalDeviceFeature::DescriptorBindingStorageImageUpdateAfterBind:
      return "DescriptorBindingStorageImageUpdateAfterBind";
    case PhysicalDeviceFeature::DescriptorBindingStorageBufferUpdateAfterBind:
      return "DescriptorBindingStorageBufferUpdateAfterBind";
    case PhysicalDeviceFeature::DescriptorBindingUniformTexelBufferUpdateAfterBind:
      return "DescriptorBindingUniformTexelBufferUpdateAfterBind";
    case PhysicalDeviceFeature::DescriptorBindingStorageTexelBufferUpdateAfterBind:
      return "DescriptorBindingStorageTexelBufferUpdateAfterBind";
    case PhysicalDeviceFeature::DescriptorBindingUpdateUnusedWhilePending:
      return "DescriptorBindingUpdateUnusedWhilePending";
    case PhysicalDeviceFeature::DescriptorBindingPartiallyBound:
      return "DescriptorBindingPartiallyBound";
    case PhysicalDeviceFeature::DescriptorBindingVariableDescriptorCount:
      return "DescriptorBindingVariableDescriptorCount";
    case PhysicalDeviceFeature::RuntimeDescriptorArray:
      return "RuntimeDescriptorArray";
    case PhysicalDeviceFeature::SamplerFilterMinmax:
      return "SamplerFilterMinmax";
    case PhysicalDeviceFeature::ScalarBlockLayout:
      return "ScalarBlockLayout";
    case PhysicalDeviceFeature::ImagelessFramebuffer:
      return "ImagelessFramebuffer";
    case PhysicalDeviceFeature::UniformBufferStandardLayout:
      return "UniformBufferStandardLayout";
    case PhysicalDeviceFeature::ShaderSubgroupExtendedTypes:
      return "ShaderSubgroupExtendedTypes";
    case PhysicalDeviceFeature::SeparateDepthStencilLayouts:
      return "SeparateDepthStencilLayouts";
    case PhysicalDeviceFeature::HostQueryReset:
      return "HostQueryReset";
    case PhysicalDeviceFeature::TimelineSemaphore:
      return "TimelineSemaphore";
    case PhysicalDeviceFeature::BufferDeviceAddress:
      return "BufferDeviceAddress";
    case PhysicalDeviceFeature::BufferDeviceAddressCaptureReplay:
      return "BufferDeviceAddressCaptureReplay";
    case PhysicalDeviceFeature::BufferDeviceAddressMultiDevice:
      return "BufferDeviceAddressMultiDevice";
    case PhysicalDeviceFeature::VulkanMemoryModel:
      return "VulkanMemoryModel";
    case PhysicalDeviceFeature::VulkanMemoryModelDeviceScope:
      return "VulkanMemoryModelDeviceScope";
    case PhysicalDeviceFeature::VulkanMemoryModelAvailabilityVisibilityChains:
      return "VulkanMemoryModelAvailabilityVisibilityChains";
    case PhysicalDeviceFeature::ShaderOutputViewportIndex:
      return "ShaderOutputViewportIndex";
    case PhysicalDeviceFeature::ShaderOutputLayer:
      return "ShaderOutputLayer";
    case PhysicalDeviceFeature::SubgroupBroadcastDynamicId:
      return "SubgroupBroadcastDynamicId";
    case PhysicalDeviceFeature::RobustImageAccess:
      return "RobustImageAccess";
    case PhysicalDeviceFeature::InlineUniformBlock:
      return "InlineUniformBlock";
    case PhysicalDeviceFeature::DescriptorBindingInlineUniformBlockUpdateAfterBind:
      return "DescriptorBindingInlineUniformBlockUpdateAfterBind";
    case PhysicalDeviceFeature::PipelineCreationCacheControl:
      return "PipelineCreationCacheControl";
    case PhysicalDeviceFeature::PrivateData:
      return "PrivateData";
    case PhysicalDeviceFeature::ShaderDemoteToHelperInvocation:
      return "ShaderDemoteToHelperInvocation";
    case PhysicalDeviceFeature::ShaderTerminateInvocation:
      return "ShaderTerminateInvocation";
    case PhysicalDeviceFeature::SubgroupSizeControl:
      return "SubgroupSizeControl";
    case PhysicalDeviceFeature::ComputeFullSubgroups:
      return "ComputeFullSubgroups";
    case PhysicalDeviceFeature::Synchronization2:
      return "Synchronization2";
    case PhysicalDeviceFeature::TextureCompressionASTC_HDR:
      return "TextureCompressionASTC_HDR";
    case PhysicalDeviceFeature::ShaderZeroInitializeWorkgroupMemory:
      return "ShaderZeroInitializeWorkgroupMemory";
    case PhysicalDeviceFeature::DynamicRendering:
      return "DynamicRendering";
    case PhysicalDeviceFeature::ShaderIntegerDotProduct:
      return "ShaderIntegerDotProduct";
    case PhysicalDeviceFeature::Maintenance4:
      return "Maintenance4";
    case PhysicalDeviceFeature::Invalid:
    default:
      return "Unknown";
    }
  }


  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::robustBufferAccess)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::fullDrawIndexUint32)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::imageCubeArray)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::independentBlend)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::geometryShader)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::tessellationShader)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::sampleRateShading)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::dualSrcBlend)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::logicOp)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::multiDrawIndirect)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::drawIndirectFirstInstance)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::depthBiasClamp)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::fillModeNonSolid)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::depthBounds)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::wideLines)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::largePoints)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::alphaToOne)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::multiViewport)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::samplerAnisotropy)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::textureCompressionETC2)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::textureCompressionASTC_LDR)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::textureCompressionBC)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::occlusionQueryPrecise)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::pipelineStatisticsQuery)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::vertexPipelineStoresAndAtomics)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::fragmentStoresAndAtomics)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::shaderTessellationAndGeometryPointSize)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::shaderImageGatherExtended)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::shaderStorageImageExtendedFormats)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::shaderStorageImageMultisample)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::shaderStorageImageReadWithoutFormat)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::shaderStorageImageWriteWithoutFormat)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::shaderUniformBufferArrayDynamicIndexing)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::shaderSampledImageArrayDynamicIndexing)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::shaderStorageBufferArrayDynamicIndexing)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::shaderStorageImageArrayDynamicIndexing)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::shaderClipDistance)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::shaderCullDistance)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::shaderFloat64)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::shaderInt64)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::shaderInt16)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::shaderResourceResidency)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::shaderResourceMinLod)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::sparseBinding)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::sparseResidencyBuffer)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::sparseResidencyImage2D)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::sparseResidencyImage3D)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::sparseResidency2Samples)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::sparseResidency4Samples)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::sparseResidency8Samples)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::sparseResidency16Samples)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::sparseResidencyAliased)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::variableMultisampleRate)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceFeatures::inheritedQueries)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan11Features::storageBuffer16BitAccess)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan11Features::uniformAndStorageBuffer16BitAccess)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan11Features::storagePushConstant16)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan11Features::storageInputOutput16)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan11Features::multiview)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan11Features::multiviewGeometryShader)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan11Features::multiviewTessellationShader)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan11Features::variablePointersStorageBuffer)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan11Features::variablePointers)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan11Features::protectedMemory)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan11Features::samplerYcbcrConversion)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan11Features::shaderDrawParameters)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::samplerMirrorClampToEdge)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::drawIndirectCount)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::storageBuffer8BitAccess)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::uniformAndStorageBuffer8BitAccess)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::storagePushConstant8)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::shaderBufferInt64Atomics)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::shaderSharedInt64Atomics)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::shaderFloat16)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::shaderInt8)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::descriptorIndexing)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::shaderInputAttachmentArrayDynamicIndexing)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::shaderUniformTexelBufferArrayDynamicIndexing)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::shaderStorageTexelBufferArrayDynamicIndexing)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::shaderUniformBufferArrayNonUniformIndexing)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::shaderSampledImageArrayNonUniformIndexing)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::shaderStorageBufferArrayNonUniformIndexing)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::shaderStorageImageArrayNonUniformIndexing)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::shaderInputAttachmentArrayNonUniformIndexing)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::shaderUniformTexelBufferArrayNonUniformIndexing)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::shaderStorageTexelBufferArrayNonUniformIndexing)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::descriptorBindingUniformBufferUpdateAfterBind)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::descriptorBindingSampledImageUpdateAfterBind)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::descriptorBindingStorageImageUpdateAfterBind)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::descriptorBindingStorageBufferUpdateAfterBind)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::descriptorBindingUniformTexelBufferUpdateAfterBind)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::descriptorBindingStorageTexelBufferUpdateAfterBind)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::descriptorBindingUpdateUnusedWhilePending)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::descriptorBindingPartiallyBound)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::descriptorBindingVariableDescriptorCount)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::runtimeDescriptorArray)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::samplerFilterMinmax)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::scalarBlockLayout)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::imagelessFramebuffer)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::uniformBufferStandardLayout)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::shaderSubgroupExtendedTypes)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::separateDepthStencilLayouts)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::hostQueryReset)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::timelineSemaphore)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::bufferDeviceAddress)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::bufferDeviceAddressCaptureReplay)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::bufferDeviceAddressMultiDevice)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::vulkanMemoryModel)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::vulkanMemoryModelDeviceScope)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::vulkanMemoryModelAvailabilityVisibilityChains)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::shaderOutputViewportIndex)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::shaderOutputLayer)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan12Features::subgroupBroadcastDynamicId)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan13Features::robustImageAccess)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan13Features::inlineUniformBlock)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan13Features::descriptorBindingInlineUniformBlockUpdateAfterBind)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan13Features::pipelineCreationCacheControl)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan13Features::privateData)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan13Features::shaderDemoteToHelperInvocation)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan13Features::shaderTerminateInvocation)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan13Features::subgroupSizeControl)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan13Features::computeFullSubgroups)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan13Features::synchronization2)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan13Features::textureCompressionASTC_HDR)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan13Features::shaderZeroInitializeWorkgroupMemory)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan13Features::dynamicRendering)>, "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan13Features::shaderIntegerDotProduct)>,
                "struct member not of the expected type");
  static_assert(std::is_same_v<VkBool32, decltype(VkPhysicalDeviceVulkan13Features::maintenance4)>, "struct member not of the expected type");
}
