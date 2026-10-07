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
#include <RapidVulkan/GraphicsPipeline.hpp>
#include <RapidVulkan/PipelineLayout.hpp>
#include <RapidVulkan/ShaderModule.hpp>
#include <Shared/FramePacing/RaymarchParams.hpp>
#include <vulkan/vulkan.h>

namespace Fsl
{
  class IContentManager;

  //! Draws the background of the sample (its GPU load): one triangle that covers the screen, the fragment shader of the scene does the
  //! work.
  class RaymarchBackground final
  {
    struct Resources
    {
      RapidVulkan::ShaderModule VertShader;
      //! The raymarched scenes (the flight and the hall)
      RapidVulkan::ShaderModule FragShader;
      RapidVulkan::ShaderModule BlobsFragShader;
      RapidVulkan::ShaderModule LaceFragShader;
      RapidVulkan::ShaderModule MandelbrotFragShader;
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
      RapidVulkan::GraphicsPipeline MandelbrotPipeline;
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
        MandelbrotPipeline.Reset();
        LacePipeline.Reset();
        BlobsPipeline.Reset();
        Pipeline.Reset();
      }
    };

    Resources m_resources;
    DependentResources m_dependentResources;

  public:
    RaymarchBackground(const Vulkan::VUDevice& device, const IContentManager& contentManager);

    void OnBuildResources(const VulkanBasic::BuildResourcesContext& context, const VkRenderPass hRenderPass);
    void OnFreeResources() noexcept;

    //! Draw the background inside the render pass it was built for (it is not drawn if params.Steps is zero)
    void Draw(const VkCommandBuffer hCmdBuffer, const RaymarchParams& params);
  };
}

#endif
