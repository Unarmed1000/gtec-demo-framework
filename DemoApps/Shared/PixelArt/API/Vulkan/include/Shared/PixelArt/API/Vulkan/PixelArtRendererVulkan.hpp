#ifndef SHARED_PIXELART_API_VULKAN_PIXELARTRENDERERVULKAN_HPP
#define SHARED_PIXELART_API_VULKAN_PIXELARTRENDERERVULKAN_HPP
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

#include <FslBase/Math/Pixel/PxSize2D.hpp>
#include <FslUtil/Vulkan1_0/VUBufferMemory.hpp>
#include <FslUtil/Vulkan1_0/VUDevice.hpp>
#include <FslUtil/Vulkan1_0/VUDeviceQueueRecord.hpp>
#include <FslUtil/Vulkan1_0/VUFramebuffer.hpp>
#include <FslUtil/Vulkan1_0/VUTexture.hpp>
#include <RapidVulkan/DescriptorPool.hpp>
#include <RapidVulkan/DescriptorSetLayout.hpp>
#include <RapidVulkan/GraphicsPipeline.hpp>
#include <RapidVulkan/PipelineLayout.hpp>
#include <RapidVulkan/RenderPass.hpp>
#include <RapidVulkan/Sampler.hpp>
#include <RapidVulkan/ShaderModule.hpp>
#include <Shared/PixelArt/Base/IPixelArtSceneRenderer.hpp>
#include <Shared/PixelArt/Base/PixelArtFrameState.hpp>
#include <Shared/PixelArt/Base/PixelArtSceneDesc.hpp>
#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Fsl
{
  class IContentManager;

  //! Draws the PixelArt scenes with Vulkan: a fullscreen triangle per pass, the buffers are RGBA16F images (two per buffer, so a buffer
  //! can read what it drew the frame before).
  //!
  //! Every buffer is drawn once per frame, so all of them swap their two images together: the images a pass reads only depend on whether
  //! the frame number since the scene started is even or odd. The descriptor sets are made for both cases (and every frame in flight)
  //! when the scene or the buffers change, a frame only picks the set.
  class PixelArtRendererVulkan final : public IPixelArtSceneRenderer
  {
    struct ChannelRecord
    {
      PixelArtChannelSource Source{PixelArtChannelSource::Unused};
      uint32_t BufferIndex{0};
      std::shared_ptr<Vulkan::VUTexture> Texture;
      VkSampler Sampler{VK_NULL_HANDLE};
    };

    struct PassRecord
    {
      bool Enabled{false};
      RapidVulkan::ShaderModule FragmentShader;
      RapidVulkan::GraphicsPipeline Pipeline;
      std::array<ChannelRecord, PixelArtConfig::ChannelCount> Channels{};
    };

    struct SceneRecord
    {
      PixelArtOutput Output{PixelArtOutput::Gamma};
      std::array<PassRecord, PixelArtConfig::PassCount> Passes{};
    };

    struct FrameRecord
    {
      Vulkan::VUBufferMemory UniformBuffer;
      //! [parity][pass]
      std::array<std::array<VkDescriptorSet, PixelArtConfig::PassCount>, 2> DescriptorSets{};
    };

    const Vulkan::VUDevice& m_device;
    Vulkan::VUDeviceQueueRecord m_deviceQueue;
    std::shared_ptr<IContentManager> m_contentManager;
    bool m_srgbFramebuffer;

    RapidVulkan::ShaderModule m_vertexShader;
    //! [filter][wrap]
    std::array<RapidVulkan::Sampler, 6> m_samplers;
    std::shared_ptr<Vulkan::VUTexture> m_blackTexture;
    std::map<std::string, std::shared_ptr<Vulkan::VUTexture>> m_textures;
    RapidVulkan::DescriptorSetLayout m_descriptorSetLayout;
    RapidVulkan::PipelineLayout m_pipelineLayout;
    RapidVulkan::DescriptorPool m_descriptorPool;
    std::vector<FrameRecord> m_frames;
    //! The buffers draw with this, it leaves the image ready to be read
    RapidVulkan::RenderPass m_bufferRenderPass;
    //! The same as m_bufferRenderPass, but it clears the image
    RapidVulkan::RenderPass m_clearRenderPass;
    //! The main render pass of the app, the image pass draws in it (VK_NULL_HANDLE while the app has no resources)
    VkRenderPass m_mainRenderPass{VK_NULL_HANDLE};

    std::unique_ptr<SceneRecord> m_scene;
    std::array<std::array<Vulkan::VUFramebuffer, 2>, PixelArtConfig::BufferCount> m_targets;
    PxSize2D m_targetSizePx;
    //! The frame of the scene the buffers are at (decides which image of a buffer has its last output)
    uint32_t m_bufferFrame{0};
    bool m_clearPending{true};

  public:
    PixelArtRendererVulkan(const Vulkan::VUDevice& device, const Vulkan::VUDeviceQueueRecord& deviceQueue,
                           std::shared_ptr<IContentManager> contentManager, const uint32_t maxFramesInFlight, const bool srgbFramebuffer);
    ~PixelArtRendererVulkan() final;

    void LoadScene(const PixelArtSceneDesc& scene) final;

    //! The main render pass of the app was created (the image pass draws in it)
    void OnBuildResources(const VkRenderPass mainRenderPass);
    void OnFreeResources() noexcept;

    //! Call before the command buffer of the frame is recorded: it resizes the buffers if needed and uploads the uniforms
    void PrepareFrame(const uint32_t frameIndex, const PixelArtFrameState& frameState);
    //! Draw the buffer passes, outside of any render pass
    void RecordBufferPasses(const VkCommandBuffer hCmdBuffer, const uint32_t frameIndex, const PixelArtFrameState& frameState);
    //! Draw the image pass, inside the main render pass
    void RecordImagePass(const VkCommandBuffer hCmdBuffer, const uint32_t frameIndex, const PixelArtFrameState& frameState,
                         const VkExtent2D swapchainExtent);

  private:
    std::shared_ptr<Vulkan::VUTexture> GetTexture(const PixelArtChannelDesc& channel);
    [[nodiscard]] VkSampler GetSampler(const PixelArtFilter filter, const PixelArtWrap wrap) const noexcept;
    void CreateImagePipeline();
    void ResizeTargets(const PxSize2D sizePx);
    void UpdateDescriptorSets();
    void DrawPass(const VkCommandBuffer hCmdBuffer, const uint32_t frameIndex, const PixelArtPass pass, const PixelArtFrameState& frameState,
                  const PxSize2D passSizePx);
  };
}

#endif
