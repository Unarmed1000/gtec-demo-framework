//****************************************************************************************************************************************************
//* BSD 3-Clause License
//*
//* Copyright (c) 2026, Mana Battery
//* All rights reserved.
//*
//* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
//*
//* 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
//* 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the
//*    documentation and/or other materials provided with the distribution.
//* 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this
//*    software without specific prior written permission.
//*
//* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
//* THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
//* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
//* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//****************************************************************************************************************************************************

#include "RaymarchBackground.hpp"
#include <FslBase/UncheckedNumericCast.hpp>
#include <FslDemoApp/Base/Service/Content/IContentManager.hpp>
#include <RapidVulkan/Check.hpp>
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>

namespace Fsl
{
  namespace
  {
    //! The push constants of Raymarch.frag
    struct PushConstants
    {
      //! x = the travel of the camera, y = the sway of the camera, z = the colors and the shape of the lattice, w = the scene
      std::array<float, 4> Phase{};
      //! The size of the screen in pixels
      std::array<float, 2> Resolution{};
      //! The number of steps the ray is marched in
      float Steps{0.0f};
      float Reserved{0.0f};
    };

    RapidVulkan::PipelineLayout CreatePipelineLayout(const VkDevice device)
    {
      VkPushConstantRange pushConstantRange{};
      pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
      pushConstantRange.offset = 0;
      pushConstantRange.size = sizeof(PushConstants);

      VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
      pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
      pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
      pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

      return {device, pipelineLayoutCreateInfo};
    }

    RapidVulkan::GraphicsPipeline CreatePipeline(const RapidVulkan::PipelineLayout& pipelineLayout, const VkExtent2D& extent,
                                                 const VkShaderModule vertexShaderModule, const VkShaderModule fragmentShaderModule,
                                                 const VkRenderPass renderPass, const uint32_t subpass, const bool dynamicViewport)
    {
      assert(pipelineLayout.IsValid());
      assert(vertexShaderModule != VK_NULL_HANDLE);
      assert(fragmentShaderModule != VK_NULL_HANDLE);
      assert(renderPass != VK_NULL_HANDLE);

      std::array<VkPipelineShaderStageCreateInfo, 2> pipelineShaderStageCreateInfo{};
      pipelineShaderStageCreateInfo[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      pipelineShaderStageCreateInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
      pipelineShaderStageCreateInfo[0].module = vertexShaderModule;
      pipelineShaderStageCreateInfo[0].pName = "main";

      pipelineShaderStageCreateInfo[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      pipelineShaderStageCreateInfo[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      pipelineShaderStageCreateInfo[1].module = fragmentShaderModule;
      pipelineShaderStageCreateInfo[1].pName = "main";

      // The vertex shader makes the triangle from the vertex index, so there is no vertex buffer
      VkPipelineVertexInputStateCreateInfo pipelineVertexInputCreateInfo{};
      pipelineVertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

      VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo{};
      pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
      pipelineInputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
      pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

      VkViewport viewport{};
      viewport.width = static_cast<float>(extent.width);
      viewport.height = static_cast<float>(extent.height);
      viewport.minDepth = 0.0f;
      viewport.maxDepth = 1.0f;

      const VkRect2D scissor{{0, 0}, extent};

      VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo{};
      pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
      pipelineViewportStateCreateInfo.viewportCount = 1;
      pipelineViewportStateCreateInfo.pViewports = &viewport;
      pipelineViewportStateCreateInfo.scissorCount = 1;
      pipelineViewportStateCreateInfo.pScissors = &scissor;

      VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo{};
      pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
      pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
      pipelineRasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
      pipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
      pipelineRasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
      pipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
      pipelineRasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
      pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;

      VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo{};
      pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
      pipelineMultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

      // The background is opaque and the first thing drawn
      VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState{};
      pipelineColorBlendAttachmentState.blendEnable = VK_FALSE;
      pipelineColorBlendAttachmentState.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

      VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo{};
      pipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
      pipelineColorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
      pipelineColorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
      pipelineColorBlendStateCreateInfo.attachmentCount = 1;
      pipelineColorBlendStateCreateInfo.pAttachments = &pipelineColorBlendAttachmentState;

      VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo{};
      depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
      depthStencilStateCreateInfo.depthTestEnable = VK_FALSE;
      depthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;
      depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_ALWAYS;
      depthStencilStateCreateInfo.minDepthBounds = 0.0f;
      depthStencilStateCreateInfo.maxDepthBounds = 1.0f;

      VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{};
      graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
      graphicsPipelineCreateInfo.stageCount = UncheckedNumericCast<uint32_t>(pipelineShaderStageCreateInfo.size());
      graphicsPipelineCreateInfo.pStages = pipelineShaderStageCreateInfo.data();
      graphicsPipelineCreateInfo.pVertexInputState = &pipelineVertexInputCreateInfo;
      graphicsPipelineCreateInfo.pInputAssemblyState = &pipelineInputAssemblyStateCreateInfo;
      graphicsPipelineCreateInfo.pViewportState = &pipelineViewportStateCreateInfo;
      graphicsPipelineCreateInfo.pRasterizationState = &pipelineRasterizationStateCreateInfo;
      graphicsPipelineCreateInfo.pMultisampleState = &pipelineMultisampleStateCreateInfo;
      graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
      graphicsPipelineCreateInfo.pColorBlendState = &pipelineColorBlendStateCreateInfo;
      // The viewport and the scissor of the pipeline are those of the whole target; a draw into a part of it sets its own
      constexpr std::array<VkDynamicState, 2> DynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
      VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo{};
      pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
      pipelineDynamicStateCreateInfo.dynamicStateCount = UncheckedNumericCast<uint32_t>(DynamicStates.size());
      pipelineDynamicStateCreateInfo.pDynamicStates = DynamicStates.data();
      graphicsPipelineCreateInfo.pDynamicState = dynamicViewport ? &pipelineDynamicStateCreateInfo : nullptr;
      graphicsPipelineCreateInfo.layout = pipelineLayout.Get();
      graphicsPipelineCreateInfo.renderPass = renderPass;
      graphicsPipelineCreateInfo.subpass = subpass;

      return {pipelineLayout.GetDevice(), VK_NULL_HANDLE, graphicsPipelineCreateInfo};
    }

    //! The layout of the one descriptor set of the enlarging: the picture the background was drawn into
    RapidVulkan::DescriptorSetLayout CreateUpscaleSetLayout(const VkDevice device)
    {
      VkDescriptorSetLayoutBinding binding{};
      binding.binding = 0;
      binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      binding.descriptorCount = 1;
      binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

      VkDescriptorSetLayoutCreateInfo createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
      createInfo.bindingCount = 1;
      createInfo.pBindings = &binding;
      return {device, createInfo};
    }

    RapidVulkan::DescriptorPool CreateUpscalePool(const VkDevice device)
    {
      VkDescriptorPoolSize poolSize{};
      poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      poolSize.descriptorCount = 1;

      VkDescriptorPoolCreateInfo createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
      createInfo.maxSets = 1;
      createInfo.poolSizeCount = 1;
      createInfo.pPoolSizes = &poolSize;
      return {device, createInfo};
    }

    //! The same push constants as the scenes, and the descriptor set with the picture
    RapidVulkan::PipelineLayout CreateUpscalePipelineLayout(const RapidVulkan::DescriptorSetLayout& setLayout)
    {
      VkPushConstantRange pushConstantRange{};
      pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
      pushConstantRange.offset = 0;
      pushConstantRange.size = sizeof(PushConstants);

      VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
      pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
      pipelineLayoutCreateInfo.setLayoutCount = 1;
      pipelineLayoutCreateInfo.pSetLayouts = setLayout.GetPointer();
      pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
      pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

      return {setLayout.GetDevice(), pipelineLayoutCreateInfo};
    }

    //! The render pass the background is drawn in at a lower resolution. The picture is one for all frames: the frame before read
    //! it in a fragment shader when it enlarged it, and this frame reads it the same way after it drew into it, so both are named.
    RapidVulkan::RenderPass CreateOffscreenRenderPass(const VkDevice device, const VkFormat format)
    {
      VkAttachmentDescription attachment{};
      attachment.format = format;
      attachment.samples = VK_SAMPLE_COUNT_1_BIT;
      // The scene covers all of the part that is drawn into, and nothing outside that part is read
      attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      const VkAttachmentReference colorAttachmentReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

      VkSubpassDescription subpassDescription{};
      subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
      subpassDescription.colorAttachmentCount = 1;
      subpassDescription.pColorAttachments = &colorAttachmentReference;

      std::array<VkSubpassDependency, 2> subpassDependency{};
      subpassDependency[0].srcSubpass = VK_SUBPASS_EXTERNAL;
      subpassDependency[0].dstSubpass = 0;
      subpassDependency[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      subpassDependency[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
      subpassDependency[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      subpassDependency[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      subpassDependency[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

      subpassDependency[1].srcSubpass = 0;
      subpassDependency[1].dstSubpass = VK_SUBPASS_EXTERNAL;
      subpassDependency[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      subpassDependency[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      subpassDependency[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      subpassDependency[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      subpassDependency[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

      return {device, 0, 1, &attachment, 1, &subpassDescription, UncheckedNumericCast<uint32_t>(subpassDependency.size()), subpassDependency.data()};
    }

    //! The picture the background is drawn into at a lower resolution, with the sampler that enlarges it
    Vulkan::VUTexture CreateOffscreenTexture(const Vulkan::VUDevice& device, const VkExtent2D& extent, const VkFormat format)
    {
      VkImageCreateInfo imageCreateInfo{};
      imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
      imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
      imageCreateInfo.format = format;
      imageCreateInfo.extent = {extent.width, extent.height, 1};
      imageCreateInfo.mipLevels = 1;
      imageCreateInfo.arrayLayers = 1;
      imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
      imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
      imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
      imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

      VkImageSubresourceRange subresourceRange{};
      subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      subresourceRange.baseMipLevel = 0;
      subresourceRange.levelCount = 1;
      subresourceRange.baseArrayLayer = 0;
      subresourceRange.layerCount = 1;

      Vulkan::VUImageMemoryView imageMemoryView(device, imageCreateInfo, subresourceRange, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                "BackgroundOffscreen");

      VkSamplerCreateInfo samplerCreateInfo{};
      samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
      samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
      samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
      samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
      samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      samplerCreateInfo.maxAnisotropy = 1.0f;
      samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
      samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

      Vulkan::VUTexture texture(std::move(imageMemoryView), samplerCreateInfo);
      // The render pass leaves the picture in this layout, which is the one it is read in
      texture.SetImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
      return texture;
    }
  }


  RaymarchBackground::RaymarchBackground(const Vulkan::VUDevice& device, const IContentManager& contentManager)
  {
    m_resources.VertShader.Reset(device.Get(), 0, contentManager.ReadBytes("Raymarch.vert.spv"));
    m_resources.FragShader.Reset(device.Get(), 0, contentManager.ReadBytes("Raymarch.frag.spv"));
    // Every scene that is not raymarched has a fragment shader of its own, with the same push constants
    m_resources.BlobsFragShader.Reset(device.Get(), 0, contentManager.ReadBytes("Blobs.frag.spv"));
    m_resources.LaceFragShader.Reset(device.Get(), 0, contentManager.ReadBytes("Lace.frag.spv"));
    m_resources.PipelineLayout = CreatePipelineLayout(device.Get());

    // What enlarges the background that was drawn at a lower resolution
    m_resources.UpscaleFragShader.Reset(device.Get(), 0, contentManager.ReadBytes("Upscale.frag.spv"));
    m_resources.UpscaleSetLayout = CreateUpscaleSetLayout(device.Get());
    m_resources.UpscalePool = CreateUpscalePool(device.Get());
    {
      VkDescriptorSetAllocateInfo allocateInfo{};
      allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      allocateInfo.descriptorPool = m_resources.UpscalePool.Get();
      allocateInfo.descriptorSetCount = 1;
      allocateInfo.pSetLayouts = m_resources.UpscaleSetLayout.GetPointer();
      RapidVulkan::CheckError(vkAllocateDescriptorSets(device.Get(), &allocateInfo, &m_resources.UpscaleSet), "vkAllocateDescriptorSets", __FILE__,
                              __LINE__);
    }
    m_resources.UpscalePipelineLayout = CreateUpscalePipelineLayout(m_resources.UpscaleSetLayout);
  }


  void RaymarchBackground::OnBuildResources(const Vulkan::VUDevice& device, const VulkanBasic::BuildResourcesContext& context,
                                            const VkRenderPass hRenderPass)
  {
    // The viewport is part of the pipeline, the host rebuilds the resources when the window is resized
    m_dependentResources.Extent = context.SwapchainImageExtent;
    m_dependentResources.Pipeline = CreatePipeline(m_resources.PipelineLayout, context.SwapchainImageExtent, m_resources.VertShader.Get(),
                                                   m_resources.FragShader.Get(), hRenderPass, 0, false);
    m_dependentResources.BlobsPipeline = CreatePipeline(m_resources.PipelineLayout, context.SwapchainImageExtent, m_resources.VertShader.Get(),
                                                        m_resources.BlobsFragShader.Get(), hRenderPass, 0, false);
    m_dependentResources.LacePipeline = CreatePipeline(m_resources.PipelineLayout, context.SwapchainImageExtent, m_resources.VertShader.Get(),
                                                       m_resources.LaceFragShader.Get(), hRenderPass, 0, false);

    // The background at a lower resolution: a picture of the size of the window, a render pass into it with a pipeline per scene
    // (their viewport is set when they draw, it is the part of the picture that is drawn into), and the pipeline that enlarges it
    const VkRenderPass hOffscreenRenderPass =
      (m_dependentResources.OffscreenRenderPass = CreateOffscreenRenderPass(device.Get(), context.SwapchainImageFormat)).Get();
    m_dependentResources.OffscreenTexture = CreateOffscreenTexture(device, context.SwapchainImageExtent, context.SwapchainImageFormat);
    const VkImageView hOffscreenView = m_dependentResources.OffscreenTexture.ImageView().Get();
    m_dependentResources.OffscreenFramebuffer = RapidVulkan::Framebuffer(device.Get(), 0, hOffscreenRenderPass, 1, &hOffscreenView,
                                                                         context.SwapchainImageExtent.width, context.SwapchainImageExtent.height, 1);
    m_dependentResources.OffscreenPipeline = CreatePipeline(m_resources.PipelineLayout, context.SwapchainImageExtent, m_resources.VertShader.Get(),
                                                            m_resources.FragShader.Get(), hOffscreenRenderPass, 0, true);
    m_dependentResources.OffscreenBlobsPipeline =
      CreatePipeline(m_resources.PipelineLayout, context.SwapchainImageExtent, m_resources.VertShader.Get(), m_resources.BlobsFragShader.Get(),
                     hOffscreenRenderPass, 0, true);
    m_dependentResources.OffscreenLacePipeline =
      CreatePipeline(m_resources.PipelineLayout, context.SwapchainImageExtent, m_resources.VertShader.Get(), m_resources.LaceFragShader.Get(),
                     hOffscreenRenderPass, 0, true);
    m_dependentResources.UpscalePipeline = CreatePipeline(m_resources.UpscalePipelineLayout, context.SwapchainImageExtent,
                                                          m_resources.VertShader.Get(), m_resources.UpscaleFragShader.Get(), hRenderPass, 0, false);
    {
      const VkDescriptorImageInfo imageInfo = m_dependentResources.OffscreenTexture.GetDescriptorImageInfo();
      VkWriteDescriptorSet write{};
      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.dstSet = m_resources.UpscaleSet;
      write.dstBinding = 0;
      write.descriptorCount = 1;
      write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      write.pImageInfo = &imageInfo;
      vkUpdateDescriptorSets(device.Get(), 1, &write, 0, nullptr);
    }
  }


  void RaymarchBackground::OnFreeResources() noexcept
  {
    m_dependentResources.Reset();
  }


  bool RaymarchBackground::IsDrawnOffscreen(const RaymarchParams& params) const noexcept
  {
    return params.RenderScale < 0.999f && m_dependentResources.UpscalePipeline.IsValid();
  }


  VkExtent2D RaymarchBackground::OffscreenExtent(const RaymarchParams& params) const noexcept
  {
    const float scale = std::clamp(params.RenderScale, 0.01f, 1.0f);
    return {std::max(static_cast<uint32_t>(std::lround(static_cast<float>(m_dependentResources.Extent.width) * scale)), 1u),
            std::max(static_cast<uint32_t>(std::lround(static_cast<float>(m_dependentResources.Extent.height) * scale)), 1u)};
  }


  void RaymarchBackground::DrawOffscreen(const VkCommandBuffer hCmdBuffer, const RaymarchParams& params)
  {
    if (params.Steps <= 0 || !IsDrawnOffscreen(params))
    {
      return;
    }
    const VkExtent2D extent = OffscreenExtent(params);

    VkRenderPassBeginInfo renderPassBeginInfo{};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = m_dependentResources.OffscreenRenderPass.Get();
    renderPassBeginInfo.framebuffer = m_dependentResources.OffscreenFramebuffer.Get();
    renderPassBeginInfo.renderArea.extent = extent;
    vkCmdBeginRenderPass(hCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    {
      VkViewport viewport{};
      viewport.width = static_cast<float>(extent.width);
      viewport.height = static_cast<float>(extent.height);
      viewport.minDepth = 0.0f;
      viewport.maxDepth = 1.0f;
      vkCmdSetViewport(hCmdBuffer, 0, 1, &viewport);
      const VkRect2D scissor{{0, 0}, extent};
      vkCmdSetScissor(hCmdBuffer, 0, 1, &scissor);

      // The scene is told the size it is drawn at, so it is the same picture with fewer pixels
      PushConstants pushConstants;
      pushConstants.Phase = {params.TravelPhase, params.SwayPhase, params.MorphPhase, params.SceneAsFloat()};
      pushConstants.Resolution = {static_cast<float>(extent.width), static_cast<float>(extent.height)};
      pushConstants.Steps = static_cast<float>(params.Steps);

      VkPipeline hPipeline = m_dependentResources.OffscreenPipeline.Get();
      if (params.Scene == RaymarchScene::Blobs)
      {
        hPipeline = m_dependentResources.OffscreenBlobsPipeline.Get();
      }
      else if (params.Scene == RaymarchScene::Lace)
      {
        hPipeline = m_dependentResources.OffscreenLacePipeline.Get();
      }
      vkCmdBindPipeline(hCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hPipeline);
      vkCmdPushConstants(hCmdBuffer, m_resources.PipelineLayout.Get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);
      vkCmdDraw(hCmdBuffer, 3, 1, 0, 0);
    }
    vkCmdEndRenderPass(hCmdBuffer);
  }


  void RaymarchBackground::Draw(const VkCommandBuffer hCmdBuffer, const RaymarchParams& params)
  {
    if (params.Steps <= 0 || !m_dependentResources.Pipeline.IsValid())
    {
      return;
    }

    if (IsDrawnOffscreen(params))
    {
      // The part of the picture DrawOffscreen drew into is enlarged to the screen
      const VkExtent2D extent = OffscreenExtent(params);
      const auto pictureWidth = static_cast<float>(m_dependentResources.Extent.width);
      const auto pictureHeight = static_cast<float>(m_dependentResources.Extent.height);
      const auto drawWidth = static_cast<float>(extent.width);
      const auto drawHeight = static_cast<float>(extent.height);
      PushConstants upscaleConstants;
      // The part that was drawn into, and the middle of its last pixels: a sample further out would mix in what is outside it
      upscaleConstants.Phase = {drawWidth / pictureWidth, drawHeight / pictureHeight, (drawWidth - 0.5f) / pictureWidth,
                                (drawHeight - 0.5f) / pictureHeight};
      upscaleConstants.Resolution = {pictureWidth, pictureHeight};
      vkCmdBindPipeline(hCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_dependentResources.UpscalePipeline.Get());
      vkCmdBindDescriptorSets(hCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_resources.UpscalePipelineLayout.Get(), 0, 1, &m_resources.UpscaleSet, 0,
                              nullptr);
      vkCmdPushConstants(hCmdBuffer, m_resources.UpscalePipelineLayout.Get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants),
                         &upscaleConstants);
      vkCmdDraw(hCmdBuffer, 3, 1, 0, 0);
      return;
    }

    PushConstants pushConstants;
    pushConstants.Phase = {params.TravelPhase, params.SwayPhase, params.MorphPhase, params.SceneAsFloat()};
    pushConstants.Resolution = {static_cast<float>(m_dependentResources.Extent.width), static_cast<float>(m_dependentResources.Extent.height)};
    pushConstants.Steps = static_cast<float>(params.Steps);

    // The scene selects the pipeline. The two raymarched scenes share one, their shader gets the scene with the phases.
    VkPipeline hPipeline = m_dependentResources.Pipeline.Get();
    if (params.Scene == RaymarchScene::Blobs)
    {
      hPipeline = m_dependentResources.BlobsPipeline.Get();
    }
    else if (params.Scene == RaymarchScene::Lace)
    {
      hPipeline = m_dependentResources.LacePipeline.Get();
    }
    vkCmdBindPipeline(hCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hPipeline);
    vkCmdPushConstants(hCmdBuffer, m_resources.PipelineLayout.Get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);
    vkCmdDraw(hCmdBuffer, 3, 1, 0, 0);
  }
}
