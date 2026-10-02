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
#include <array>
#include <cassert>

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
                                                 const VkRenderPass renderPass, const uint32_t subpass)
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
      graphicsPipelineCreateInfo.layout = pipelineLayout.Get();
      graphicsPipelineCreateInfo.renderPass = renderPass;
      graphicsPipelineCreateInfo.subpass = subpass;

      return {pipelineLayout.GetDevice(), VK_NULL_HANDLE, graphicsPipelineCreateInfo};
    }
  }


  RaymarchBackground::RaymarchBackground(const Vulkan::VUDevice& device, const IContentManager& contentManager)
  {
    m_resources.VertShader.Reset(device.Get(), 0, contentManager.ReadBytes("Raymarch.vert.spv"));
    m_resources.FragShader.Reset(device.Get(), 0, contentManager.ReadBytes("Raymarch.frag.spv"));
    m_resources.PipelineLayout = CreatePipelineLayout(device.Get());
  }


  void RaymarchBackground::OnBuildResources(const VulkanBasic::BuildResourcesContext& context, const VkRenderPass hRenderPass)
  {
    // The viewport is part of the pipeline, the host rebuilds the resources when the window is resized
    m_dependentResources.Extent = context.SwapchainImageExtent;
    m_dependentResources.Pipeline = CreatePipeline(m_resources.PipelineLayout, context.SwapchainImageExtent, m_resources.VertShader.Get(),
                                                   m_resources.FragShader.Get(), hRenderPass, 0);
  }


  void RaymarchBackground::OnFreeResources() noexcept
  {
    m_dependentResources.Reset();
  }


  void RaymarchBackground::Draw(const VkCommandBuffer hCmdBuffer, const RaymarchParams& params)
  {
    if (params.Steps <= 0 || !m_dependentResources.Pipeline.IsValid())
    {
      return;
    }

    PushConstants pushConstants;
    pushConstants.Phase = {params.TravelPhase, params.SwayPhase, params.MorphPhase, params.SceneAsFloat()};
    pushConstants.Resolution = {static_cast<float>(m_dependentResources.Extent.width), static_cast<float>(m_dependentResources.Extent.height)};
    pushConstants.Steps = static_cast<float>(params.Steps);

    vkCmdBindPipeline(hCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_dependentResources.Pipeline.Get());
    vkCmdPushConstants(hCmdBuffer, m_resources.PipelineLayout.Get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);
    vkCmdDraw(hCmdBuffer, 3, 1, 0, 0);
  }
}
