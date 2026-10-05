#ifndef VULKAN_FRAMEPACING_RAYMARCHBACKGROUND_HPP
#define VULKAN_FRAMEPACING_RAYMARCHBACKGROUND_HPP
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

#include <FslDemoApp/Vulkan/Basic/BuildResourcesContext.hpp>
#include <FslUtil/Vulkan1_0/VUDevice.hpp>
#include <FslUtil/Vulkan1_0/VUTexture.hpp>
#include <RapidVulkan/DescriptorPool.hpp>
#include <RapidVulkan/DescriptorSetLayout.hpp>
#include <RapidVulkan/Framebuffer.hpp>
#include <RapidVulkan/GraphicsPipeline.hpp>
#include <RapidVulkan/PipelineLayout.hpp>
#include <RapidVulkan/RenderPass.hpp>
#include <RapidVulkan/ShaderModule.hpp>
#include <Shared/FramePacing/RaymarchParams.hpp>
#include <vulkan/vulkan.h>

namespace Fsl
{
  class IContentManager;

  //! Draws the raymarched background of the sample (its GPU load): one triangle that covers the screen, the fragment shader does the work.
  class RaymarchBackground final
  {
    struct Resources
    {
      RapidVulkan::ShaderModule VertShader;
      //! The raymarched scenes (the flight and the hall)
      RapidVulkan::ShaderModule FragShader;
      RapidVulkan::ShaderModule BlobsFragShader;
      RapidVulkan::ShaderModule LaceFragShader;
      //! Enlarges the background that was drawn at a lower resolution to the screen
      RapidVulkan::ShaderModule UpscaleFragShader;
      RapidVulkan::DescriptorSetLayout UpscaleSetLayout;
      RapidVulkan::DescriptorPool UpscalePool;
      //! The one descriptor set of the pool: the picture the background was drawn into (it goes with the pool)
      VkDescriptorSet UpscaleSet{VK_NULL_HANDLE};
      RapidVulkan::PipelineLayout UpscalePipelineLayout;
      RapidVulkan::PipelineLayout PipelineLayout;

      Resources() = default;
      ~Resources() = default;
      Resources(const Resources&) = delete;
      Resources& operator=(const Resources&) = delete;
      Resources(Resources&& other) noexcept = delete;
      Resources& operator=(Resources&& other) noexcept = delete;
    };

    struct DependentResources
    {
      //! The raymarched scenes (the flight and the hall)
      RapidVulkan::GraphicsPipeline Pipeline;
      RapidVulkan::GraphicsPipeline BlobsPipeline;
      RapidVulkan::GraphicsPipeline LacePipeline;
      //! What the background is drawn into when it is drawn at a lower resolution. The picture has the size of the window and the
      //! background is drawn into the upper left part of it, so a change of the resolution needs no new picture.
      RapidVulkan::RenderPass OffscreenRenderPass;
      Vulkan::VUTexture OffscreenTexture;
      RapidVulkan::Framebuffer OffscreenFramebuffer;
      //! The pipelines of the scenes for that render pass (a pipeline belongs to the render pass it was made for)
      RapidVulkan::GraphicsPipeline OffscreenPipeline;
      RapidVulkan::GraphicsPipeline OffscreenBlobsPipeline;
      RapidVulkan::GraphicsPipeline OffscreenLacePipeline;
      //! Enlarges the part of the picture that was drawn into to the screen, in the main render pass
      RapidVulkan::GraphicsPipeline UpscalePipeline;
      VkExtent2D Extent{};

      DependentResources() = default;
      ~DependentResources() = default;
      DependentResources(const DependentResources&) = delete;
      DependentResources& operator=(const DependentResources&) = delete;
      DependentResources(DependentResources&& other) noexcept = delete;
      DependentResources& operator=(DependentResources&& other) noexcept = delete;

      void Reset() noexcept
      {
        // Reset in destruction order
        Extent = {};
        UpscalePipeline.Reset();
        OffscreenLacePipeline.Reset();
        OffscreenBlobsPipeline.Reset();
        OffscreenPipeline.Reset();
        OffscreenFramebuffer.Reset();
        OffscreenTexture.Reset();
        OffscreenRenderPass.Reset();
        LacePipeline.Reset();
        BlobsPipeline.Reset();
        Pipeline.Reset();
      }
    };

    Resources m_resources;
    DependentResources m_dependentResources;

  public:
    RaymarchBackground(const Vulkan::VUDevice& device, const IContentManager& contentManager);

    void OnBuildResources(const Vulkan::VUDevice& device, const VulkanBasic::BuildResourcesContext& context, const VkRenderPass hRenderPass);
    void OnFreeResources() noexcept;

    //! Draw the background inside the render pass it was built for (it is not drawn if params.Steps is zero)
    //! Draws the background at a lower resolution into its own picture, when params.RenderScale asks for that (it does nothing
    //! otherwise). Call it before the main render pass begins: it is a render pass of its own.
    void DrawOffscreen(const VkCommandBuffer hCmdBuffer, const RaymarchParams& params);

    //! Draws the background in the main render pass: the scene itself, or the picture DrawOffscreen drew enlarged to the screen
    void Draw(const VkCommandBuffer hCmdBuffer, const RaymarchParams& params);

  private:
    [[nodiscard]] bool IsDrawnOffscreen(const RaymarchParams& params) const noexcept;
    [[nodiscard]] VkExtent2D OffscreenExtent(const RaymarchParams& params) const noexcept;
  };
}

#endif
