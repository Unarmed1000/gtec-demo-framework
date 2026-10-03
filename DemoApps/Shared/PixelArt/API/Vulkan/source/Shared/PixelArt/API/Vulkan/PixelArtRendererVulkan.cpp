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

#include <FslBase/Log/Log3Fmt.hpp>
#include <FslBase/Span/SpanUtil_Array.hpp>
#include <FslBase/UncheckedNumericCast.hpp>
#include <FslDemoApp/Base/Service/Content/IContentManager.hpp>
#include <FslGraphics/Bitmap/ReadOnlyRawBitmap.hpp>
#include <FslGraphics/Texture/Texture.hpp>
#include <FslUtil/Vulkan1_0/Debug/VUScopedCmdDebugLabel.hpp>
#include <FslUtil/Vulkan1_0/Draft/VulkanImageCreator.hpp>
#include <RapidVulkan/Check.hpp>
#include <Shared/PixelArt/API/Vulkan/PixelArtRendererVulkan.hpp>
#include <Shared/PixelArt/Base/PixelArtSceneLoader.hpp>
#include <fmt/format.h>
#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <utility>

namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
      constexpr IO::PathView VertexShader("PixelArt/Common/PixelArt.vert.spv");
      constexpr VkFormat BufferFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
      //! The uniform buffer binding and the four iChannel bindings
      constexpr uint32_t BindingCount = 1 + PixelArtConfig::ChannelCount;
    }

    //! PixelArtFrameBlock of PixelArtVulkan.glsl (std140)
    struct FrameUniforms
    {
      std::array<float, 4> Time{};
      std::array<float, 4> Date{};
      std::array<float, 4> ChannelTime{};
      std::array<float, 4> Misc{};
      std::array<float, PixelArtConfig::MaxParams> Params{};
    };

    //! PixelArtPassBlock of PixelArtVulkan.glsl
    struct PassPushConstants
    {
      std::array<float, 4> Resolution{};
      std::array<float, 4> Mouse{};
      std::array<float, 4> Pass{};
      std::array<float, std::size_t{4} * PixelArtConfig::ChannelCount> ChannelResolution{};
    };
    static_assert(sizeof(PassPushConstants) <= 128, "Vulkan only promises 128 bytes of push constants");

    uint32_t ToSamplerIndex(const PixelArtFilter filter, const PixelArtWrap wrap) noexcept
    {
      return (static_cast<uint32_t>(filter) * 2u) + static_cast<uint32_t>(wrap);
    }

    VkSamplerCreateInfo CreateSamplerCreateInfo(const PixelArtFilter filter, const PixelArtWrap wrap)
    {
      const VkFilter vkFilter = filter == PixelArtFilter::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
      const VkSamplerAddressMode addressMode = wrap == PixelArtWrap::Repeat ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      VkSamplerCreateInfo createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
      createInfo.magFilter = vkFilter;
      createInfo.minFilter = vkFilter;
      createInfo.mipmapMode = filter == PixelArtFilter::Mipmap ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
      createInfo.addressModeU = addressMode;
      createInfo.addressModeV = addressMode;
      createInfo.addressModeW = addressMode;
      createInfo.mipLodBias = 0.0f;
      createInfo.anisotropyEnable = VK_FALSE;
      createInfo.maxAnisotropy = 1.0f;
      createInfo.compareEnable = VK_FALSE;
      createInfo.compareOp = VK_COMPARE_OP_NEVER;
      createInfo.minLod = 0.0f;
      // Only the mipmap filter reads the smaller levels
      createInfo.maxLod = filter == PixelArtFilter::Mipmap ? VK_LOD_CLAMP_NONE : 0.0f;
      createInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
      return createInfo;
    }

    RapidVulkan::DescriptorSetLayout CreateDescriptorSetLayout(const VkDevice device)
    {
      std::array<VkDescriptorSetLayoutBinding, LocalConfig::BindingCount> bindings{};
      bindings[0].binding = 0;
      bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      bindings[0].descriptorCount = 1;
      bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
      for (uint32_t i = 1; i < bindings.size(); ++i)
      {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
      }

      VkDescriptorSetLayoutCreateInfo createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
      createInfo.bindingCount = UncheckedNumericCast<uint32_t>(bindings.size());
      createInfo.pBindings = bindings.data();
      return {device, createInfo};
    }

    RapidVulkan::PipelineLayout CreatePipelineLayout(const RapidVulkan::DescriptorSetLayout& descriptorSetLayout)
    {
      VkPushConstantRange pushConstantRange{};
      pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
      pushConstantRange.offset = 0;
      pushConstantRange.size = sizeof(PassPushConstants);

      VkPipelineLayoutCreateInfo createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
      createInfo.setLayoutCount = 1;
      createInfo.pSetLayouts = descriptorSetLayout.GetPointer();
      createInfo.pushConstantRangeCount = 1;
      createInfo.pPushConstantRanges = &pushConstantRange;
      return {descriptorSetLayout.GetDevice(), createInfo};
    }

    RapidVulkan::DescriptorPool CreateDescriptorPool(const VkDevice device, const uint32_t setCount)
    {
      std::array<VkDescriptorPoolSize, 2> poolSizes{};
      poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      poolSizes[0].descriptorCount = setCount;
      poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      poolSizes[1].descriptorCount = setCount * PixelArtConfig::ChannelCount;

      VkDescriptorPoolCreateInfo createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
      createInfo.maxSets = setCount;
      createInfo.poolSizeCount = UncheckedNumericCast<uint32_t>(poolSizes.size());
      createInfo.pPoolSizes = poolSizes.data();
      return {device, createInfo};
    }

    VkDescriptorSet AllocateDescriptorSet(const RapidVulkan::DescriptorPool& descriptorPool, const RapidVulkan::DescriptorSetLayout& layout)
    {
      VkDescriptorSetAllocateInfo allocInfo{};
      allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      allocInfo.descriptorPool = descriptorPool.Get();
      allocInfo.descriptorSetCount = 1;
      allocInfo.pSetLayouts = layout.GetPointer();

      VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
      RapidVulkan::CheckError(vkAllocateDescriptorSets(descriptorPool.GetDevice(), &allocInfo, &descriptorSet), "vkAllocateDescriptorSets", __FILE__,
                              __LINE__);
      return descriptorSet;
    }

    //! A render pass that draws to one RGBA16F image and leaves it ready to be read by a fragment shader
    RapidVulkan::RenderPass CreateBufferRenderPass(const VkDevice device, const VkAttachmentLoadOp loadOp)
    {
      const VkAttachmentReference colorAttachmentReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

      VkSubpassDescription subpassDescription{};
      subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
      subpassDescription.colorAttachmentCount = 1;
      subpassDescription.pColorAttachments = &colorAttachmentReference;

      std::array<VkSubpassDependency, 2> subpassDependencies{};
      // The image was read by an earlier pass (or the last frame)
      subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
      subpassDependencies[0].dstSubpass = 0;
      subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      subpassDependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
      subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      subpassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
      // The later passes read it
      subpassDependencies[1].srcSubpass = 0;
      subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
      subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      subpassDependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      subpassDependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

      VkAttachmentDescription attachment{};
      attachment.format = LocalConfig::BufferFormat;
      attachment.samples = VK_SAMPLE_COUNT_1_BIT;
      attachment.loadOp = loadOp;
      attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      return {
        device, 0, 1, &attachment, 1, &subpassDescription, UncheckedNumericCast<uint32_t>(subpassDependencies.size()), subpassDependencies.data()};
    }

    //! A fullscreen triangle without vertex buffer, the viewport is set when it draws
    RapidVulkan::GraphicsPipeline CreatePipeline(const RapidVulkan::PipelineLayout& pipelineLayout, const VkShaderModule vertexShaderModule,
                                                 const VkShaderModule fragmentShaderModule, const VkRenderPass renderPass)
    {
      assert(pipelineLayout.IsValid());
      assert(renderPass != VK_NULL_HANDLE);

      std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};
      shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
      shaderStages[0].module = vertexShaderModule;
      shaderStages[0].pName = "main";
      shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      shaderStages[1].module = fragmentShaderModule;
      shaderStages[1].pName = "main";

      VkPipelineVertexInputStateCreateInfo vertexInputState{};
      vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

      VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{};
      inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
      inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
      inputAssemblyState.primitiveRestartEnable = VK_FALSE;

      VkPipelineViewportStateCreateInfo viewportState{};
      viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
      viewportState.viewportCount = 1;
      viewportState.scissorCount = 1;

      VkPipelineRasterizationStateCreateInfo rasterizationState{};
      rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
      rasterizationState.depthClampEnable = VK_FALSE;
      rasterizationState.rasterizerDiscardEnable = VK_FALSE;
      rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
      rasterizationState.cullMode = VK_CULL_MODE_NONE;
      rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
      rasterizationState.depthBiasEnable = VK_FALSE;
      rasterizationState.lineWidth = 1.0f;

      VkPipelineMultisampleStateCreateInfo multisampleState{};
      multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
      multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

      VkPipelineColorBlendAttachmentState colorBlendAttachment{};
      colorBlendAttachment.blendEnable = VK_FALSE;
      colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

      VkPipelineColorBlendStateCreateInfo colorBlendState{};
      colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
      colorBlendState.logicOpEnable = VK_FALSE;
      colorBlendState.logicOp = VK_LOGIC_OP_COPY;
      colorBlendState.attachmentCount = 1;
      colorBlendState.pAttachments = &colorBlendAttachment;

      VkPipelineDepthStencilStateCreateInfo depthStencilState{};
      depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
      depthStencilState.depthTestEnable = VK_FALSE;
      depthStencilState.depthWriteEnable = VK_FALSE;
      depthStencilState.depthCompareOp = VK_COMPARE_OP_ALWAYS;
      depthStencilState.minDepthBounds = 0.0f;
      depthStencilState.maxDepthBounds = 1.0f;

      // The size of a pass changes with the window and the render scale, so the viewport is not part of the pipeline
      constexpr std::array<VkDynamicState, 2> DynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
      VkPipelineDynamicStateCreateInfo dynamicState{};
      dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
      dynamicState.dynamicStateCount = UncheckedNumericCast<uint32_t>(DynamicStates.size());
      dynamicState.pDynamicStates = DynamicStates.data();

      VkGraphicsPipelineCreateInfo createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
      createInfo.stageCount = UncheckedNumericCast<uint32_t>(shaderStages.size());
      createInfo.pStages = shaderStages.data();
      createInfo.pVertexInputState = &vertexInputState;
      createInfo.pInputAssemblyState = &inputAssemblyState;
      createInfo.pViewportState = &viewportState;
      createInfo.pRasterizationState = &rasterizationState;
      createInfo.pMultisampleState = &multisampleState;
      createInfo.pDepthStencilState = &depthStencilState;
      createInfo.pColorBlendState = &colorBlendState;
      createInfo.pDynamicState = &dynamicState;
      createInfo.layout = pipelineLayout.Get();
      createInfo.renderPass = renderPass;
      createInfo.subpass = 0;

      return {pipelineLayout.GetDevice(), VK_NULL_HANDLE, createInfo};
    }

    void SetViewport(const VkCommandBuffer hCmdBuffer, const VkExtent2D extent)
    {
      VkViewport viewport{};
      viewport.width = static_cast<float>(extent.width);
      viewport.height = static_cast<float>(extent.height);
      viewport.minDepth = 0.0f;
      viewport.maxDepth = 1.0f;
      const VkRect2D scissor{{0, 0}, extent};
      vkCmdSetViewport(hCmdBuffer, 0, 1, &viewport);
      vkCmdSetScissor(hCmdBuffer, 0, 1, &scissor);
    }

    VkExtent2D ToExtent(const PxSize2D sizePx) noexcept
    {
      return {static_cast<uint32_t>(sizePx.RawWidth()), static_cast<uint32_t>(sizePx.RawHeight())};
    }
  }


  PixelArtRendererVulkan::PixelArtRendererVulkan(const Vulkan::VUDevice& device, const Vulkan::VUDeviceQueueRecord& deviceQueue,
                                                 std::shared_ptr<IContentManager> contentManager, const uint32_t maxFramesInFlight,
                                                 const bool srgbFramebuffer)
    : m_device(device)
    , m_deviceQueue(deviceQueue)
    , m_contentManager(std::move(contentManager))
    , m_srgbFramebuffer(srgbFramebuffer)
    , m_descriptorSetLayout(CreateDescriptorSetLayout(device.Get()))
    , m_pipelineLayout(CreatePipelineLayout(m_descriptorSetLayout))
    , m_descriptorPool(CreateDescriptorPool(device.Get(), maxFramesInFlight * 2 * PixelArtConfig::PassCount))
    , m_bufferRenderPass(CreateBufferRenderPass(device.Get(), VK_ATTACHMENT_LOAD_OP_DONT_CARE))
    , m_clearRenderPass(CreateBufferRenderPass(device.Get(), VK_ATTACHMENT_LOAD_OP_CLEAR))
  {
    m_vertexShader.Reset(m_device.Get(), 0, m_contentManager->ReadBytes(LocalConfig::VertexShader));

    for (uint32_t filterIndex = 0; filterIndex < 3; ++filterIndex)
    {
      for (uint32_t wrapIndex = 0; wrapIndex < 2; ++wrapIndex)
      {
        const auto filter = static_cast<PixelArtFilter>(filterIndex);
        const auto wrap = static_cast<PixelArtWrap>(wrapIndex);
        m_samplers[ToSamplerIndex(filter, wrap)].Reset(m_device.Get(), CreateSamplerCreateInfo(filter, wrap));
      }
    }

    {
      constexpr std::array<uint8_t, 4> BlackPixel = {0x00, 0x00, 0x00, 0xFF};
      const auto rawBitmap =
        ReadOnlyRawBitmap::Create(SpanUtil::AsReadOnlySpan(BlackPixel), PxSize2D::Create(1, 1), PixelFormat::R8G8B8A8_UNORM, BitmapOrigin::UpperLeft);
      Vulkan::VulkanImageCreator imageCreator(m_device, m_deviceQueue.Queue, m_deviceQueue.QueueFamilyIndex);
      m_blackTexture = std::make_shared<Vulkan::VUTexture>(
        imageCreator.CreateTexture(rawBitmap, CreateSamplerCreateInfo(PixelArtFilter::Nearest, PixelArtWrap::Clamp), "PixelArt.Black"));
    }

    m_frames.resize(maxFramesInFlight);
    for (FrameRecord& rFrame : m_frames)
    {
      VkBufferCreateInfo bufferCreateInfo{};
      bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      bufferCreateInfo.size = sizeof(FrameUniforms);
      bufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
      bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      rFrame.UniformBuffer.Reset(m_device, bufferCreateInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
      // Updated every frame, so it stays mapped
      rFrame.UniformBuffer.Map();
      for (auto& rSets : rFrame.DescriptorSets)
      {
        for (auto& rSet : rSets)
        {
          rSet = AllocateDescriptorSet(m_descriptorPool, m_descriptorSetLayout);
        }
      }
    }
  }


  PixelArtRendererVulkan::~PixelArtRendererVulkan() = default;


  void PixelArtRendererVulkan::LoadScene(const PixelArtSceneDesc& scene)
  {
    // The descriptor sets and the pipelines of the current scene might still be in use
    RapidVulkan::CheckError(vkDeviceWaitIdle(m_device.Get()), "vkDeviceWaitIdle", __FILE__, __LINE__);

    // Everything is created before the current scene is replaced, so a scene that fails leaves the current one as it was
    auto newScene = std::make_unique<SceneRecord>();
    newScene->Output = scene.Output;
    for (uint32_t passIndex = 0; passIndex < PixelArtConfig::PassCount; ++passIndex)
    {
      const PixelArtPassDesc& passDesc = scene.Passes[passIndex];
      if (!passDesc.Enabled)
      {
        continue;
      }
      const auto pass = PixelArtPassUtil::FromIndex(passIndex);
      const char* const pszTab = PixelArtPassUtil::GetTabName(pass);
      PassRecord& rPass = newScene->Passes[passIndex];
      rPass.Enabled = true;

      for (uint32_t channelIndex = 0; channelIndex < PixelArtConfig::ChannelCount; ++channelIndex)
      {
        const PixelArtChannelDesc& channelDesc = passDesc.Channels[channelIndex];
        ChannelRecord& rChannel = rPass.Channels[channelIndex];
        rChannel.Source = channelDesc.Source;
        rChannel.Sampler = GetSampler(channelDesc.Filter, channelDesc.Wrap);
        switch (channelDesc.Source)
        {
        case PixelArtChannelSource::Buffer:
          rChannel.BufferIndex = PixelArtPassUtil::ToIndex(channelDesc.Buffer);
          break;
        case PixelArtChannelSource::Texture:
        case PixelArtChannelSource::Cubemap:
          try
          {
            rChannel.Texture = GetTexture(channelDesc);
          }
          catch (const std::exception& ex)
          {
            throw std::runtime_error(fmt::format("{} iChannel{}: {}", pszTab, channelIndex, ex.what()));
          }
          break;
        case PixelArtChannelSource::Unused:
        default:
          rChannel.Texture = m_blackTexture;
          rChannel.Sampler = GetSampler(PixelArtFilter::Nearest, PixelArtWrap::Clamp);
          break;
        }
      }

      // The Vulkan app compiles a small .frag per tab (it includes the prologue, Params.glsl and the tab) when it is built
      const IO::Path shaderPath(fmt::format("PixelArt/{}/{}.frag.spv", scene.Id, pszTab));
      if (!m_contentManager->Exists(shaderPath))
      {
        throw std::runtime_error(fmt::format("{} was not found, the Vulkan app needs a Content.bld/PixelArt/{}/{}.frag for every tab of the scene",
                                             shaderPath.ToUTF8String(), scene.Id, pszTab));
      }
      rPass.FragmentShader.Reset(m_device.Get(), 0, m_contentManager->ReadBytes(shaderPath));
      if (pass != PixelArtPass::Image)
      {
        rPass.Pipeline = CreatePipeline(m_pipelineLayout, m_vertexShader.Get(), rPass.FragmentShader.Get(), m_bufferRenderPass.Get());
      }
    }
    m_scene = std::move(newScene);
    CreateImagePipeline();
    // The new scene starts with empty buffers
    m_clearPending = true;
    m_bufferFrame = 0;
    UpdateDescriptorSets();
  }


  void PixelArtRendererVulkan::OnBuildResources(const VkRenderPass mainRenderPass)
  {
    m_mainRenderPass = mainRenderPass;
    CreateImagePipeline();
  }


  void PixelArtRendererVulkan::OnFreeResources() noexcept
  {
    if (m_scene)
    {
      m_scene->Passes[PixelArtPassUtil::ToIndex(PixelArtPass::Image)].Pipeline.Reset();
    }
    m_mainRenderPass = VK_NULL_HANDLE;
  }


  void PixelArtRendererVulkan::PrepareFrame(const uint32_t frameIndex, const PixelArtFrameState& frameState)
  {
    if (!m_scene)
    {
      return;
    }
    if (frameState.RenderSizePx != m_targetSizePx)
    {
      // The buffers might still be in use by the frames in flight
      RapidVulkan::CheckError(vkDeviceWaitIdle(m_device.Get()), "vkDeviceWaitIdle", __FILE__, __LINE__);
      ResizeTargets(frameState.RenderSizePx);
      UpdateDescriptorSets();
    }
    if (frameState.Frame == 0)
    {
      m_clearPending = true;
    }

    FrameUniforms uniforms;
    uniforms.Time = {frameState.Time, frameState.TimeDelta, frameState.FrameRate, static_cast<float>(frameState.Frame)};
    uniforms.Date = frameState.Date;
    uniforms.Misc = {44100.0f, 0.0f, 0.0f, 0.0f};
    const std::size_t paramCount = std::min(frameState.Params.size(), uniforms.Params.size());
    for (std::size_t i = 0; i < paramCount; ++i)
    {
      uniforms.Params[i] = frameState.Params[i];
    }
    m_frames.at(frameIndex).UniformBuffer.Upload(0, &uniforms, sizeof(FrameUniforms));
  }


  void PixelArtRendererVulkan::RecordBufferPasses(const VkCommandBuffer hCmdBuffer, const uint32_t frameIndex, const PixelArtFrameState& frameState)
  {
    if (!m_scene || !m_targets[0][0].IsValid())
    {
      return;
    }
    const VkExtent2D extent = ToExtent(m_targetSizePx);
    if (m_clearPending)
    {
      // Both images of every buffer start black (a buffer can read the image it did not draw this frame)
      const Vulkan::VUScopedCmdDebugLabel scopedLabel(hCmdBuffer, "PixelArt.ClearBuffers");
      const VkClearValue clearValue{};
      for (const auto& targets : m_targets)
      {
        for (const auto& target : targets)
        {
          VkRenderPassBeginInfo beginInfo{};
          beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
          beginInfo.renderPass = m_clearRenderPass.Get();
          beginInfo.framebuffer = target.GetFramebuffer();
          beginInfo.renderArea.extent = extent;
          beginInfo.clearValueCount = 1;
          beginInfo.pClearValues = &clearValue;
          vkCmdBeginRenderPass(hCmdBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
          vkCmdEndRenderPass(hCmdBuffer);
        }
      }
      m_clearPending = false;
      m_bufferFrame = 0;
    }

    const uint32_t parity = m_bufferFrame % 2;
    for (uint32_t bufferIndex = 0; bufferIndex < PixelArtConfig::BufferCount; ++bufferIndex)
    {
      if (!m_scene->Passes[bufferIndex].Enabled)
      {
        continue;
      }
      // Label the pass with the name of its tab ('Buffer A' etc), so it can be found in tools like RenderDoc
      const Vulkan::VUScopedCmdDebugLabel scopedLabel(hCmdBuffer, PixelArtPassUtil::GetTabName(PixelArtPassUtil::FromIndex(bufferIndex)));
      // Draw to the image that does not have the last output, so the pass can read that
      const Vulkan::VUFramebuffer& target = m_targets[bufferIndex][1u - parity];
      VkRenderPassBeginInfo beginInfo{};
      beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
      beginInfo.renderPass = m_bufferRenderPass.Get();
      beginInfo.framebuffer = target.GetFramebuffer();
      beginInfo.renderArea.extent = extent;
      vkCmdBeginRenderPass(hCmdBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
      SetViewport(hCmdBuffer, extent);
      DrawPass(hCmdBuffer, frameIndex, PixelArtPassUtil::FromIndex(bufferIndex), frameState, m_targetSizePx);
      vkCmdEndRenderPass(hCmdBuffer);
    }
  }


  void PixelArtRendererVulkan::RecordImagePass(const VkCommandBuffer hCmdBuffer, const uint32_t frameIndex, const PixelArtFrameState& frameState,
                                               const VkExtent2D swapchainExtent)
  {
    if (!m_scene || !m_targets[0][0].IsValid() || !m_scene->Passes[PixelArtPassUtil::ToIndex(PixelArtPass::Image)].Pipeline.IsValid())
    {
      return;
    }
    // The image pass draws to the scene area, the top left part of the window left of the UI panel (it can not be larger than the swapchain)
    const VkExtent2D sceneExtent{std::min(static_cast<uint32_t>(frameState.SceneSizePx.RawWidth()), swapchainExtent.width),
                                 std::min(static_cast<uint32_t>(frameState.SceneSizePx.RawHeight()), swapchainExtent.height)};
    if (sceneExtent.width == 0 || sceneExtent.height == 0)
    {
      return;
    }
    {
      const Vulkan::VUScopedCmdDebugLabel scopedLabel(hCmdBuffer, PixelArtPassUtil::GetTabName(PixelArtPass::Image));
      SetViewport(hCmdBuffer, sceneExtent);
      DrawPass(hCmdBuffer, frameIndex, PixelArtPass::Image, frameState,
               PxSize2D::Create(static_cast<int32_t>(sceneExtent.width), static_cast<int32_t>(sceneExtent.height)));
    }
    // The buffers swapped their images
    ++m_bufferFrame;
  }


  std::shared_ptr<Vulkan::VUTexture> PixelArtRendererVulkan::GetTexture(const PixelArtChannelDesc& channel)
  {
    const bool isCubemap = channel.Source == PixelArtChannelSource::Cubemap;
    const bool generateMipMaps = channel.Filter == PixelArtFilter::Mipmap;
    const std::string key = fmt::format("{}|{}|{}|{}|{}", channel.Path.ToUTF8String(), isCubemap, channel.Srgb, channel.VFlip, generateMipMaps);
    const auto itr = m_textures.find(key);
    if (itr != m_textures.end())
    {
      return itr->second;
    }

    // shadertoy's VFlip means the first row of the texture is the bottom of the image (the same layout the OpenGL ES app uses)
    const BitmapOrigin origin = (isCubemap || !channel.VFlip) ? BitmapOrigin::UpperLeft : BitmapOrigin::LowerLeft;
    Texture texture =
      m_contentManager->ReadTexture(channel.Path, PixelFormat::R8G8B8A8_UNORM, origin, PixelChannelOrder::Undefined, !isCubemap && generateMipMaps);
    if (isCubemap != (texture.GetTextureType() == TextureType::TexCube))
    {
      throw std::runtime_error(fmt::format("'{}' is not a {}", channel.Path.ToUTF8String(), isCubemap ? "cubemap" : "2D texture"));
    }
    if (channel.Srgb)
    {
      texture.ChangeCompatiblePixelFormatFlags(PixelFormatFlags::NF_Srgb);
    }
    Vulkan::VulkanImageCreator imageCreator(m_device, m_deviceQueue.Queue, m_deviceQueue.QueueFamilyIndex);
    auto vkTexture = std::make_shared<Vulkan::VUTexture>(
      imageCreator.CreateTexture(texture, CreateSamplerCreateInfo(channel.Filter, channel.Wrap), "PixelArt.Texture"));
    m_textures.emplace(key, vkTexture);
    return vkTexture;
  }


  VkSampler PixelArtRendererVulkan::GetSampler(const PixelArtFilter filter, const PixelArtWrap wrap) const noexcept
  {
    return m_samplers[ToSamplerIndex(filter, wrap)].Get();
  }


  void PixelArtRendererVulkan::CreateImagePipeline()
  {
    if (!m_scene || m_mainRenderPass == VK_NULL_HANDLE)
    {
      return;
    }
    PassRecord& rPass = m_scene->Passes[PixelArtPassUtil::ToIndex(PixelArtPass::Image)];
    rPass.Pipeline = CreatePipeline(m_pipelineLayout, m_vertexShader.Get(), rPass.FragmentShader.Get(), m_mainRenderPass);
  }


  void PixelArtRendererVulkan::ResizeTargets(const PxSize2D sizePx)
  {
    const VkExtent2D extent = ToExtent(sizePx);
    for (uint32_t bufferIndex = 0; bufferIndex < PixelArtConfig::BufferCount; ++bufferIndex)
    {
      for (uint32_t i = 0; i < 2; ++i)
      {
        m_targets[bufferIndex][i].Reset(m_device, extent, LocalConfig::BufferFormat, m_bufferRenderPass.Get(),
                                        fmt::format("PixelArt.Buffer{}.{}", static_cast<char>('A' + bufferIndex), i));
      }
    }
    m_targetSizePx = sizePx;
    m_clearPending = true;
  }


  void PixelArtRendererVulkan::UpdateDescriptorSets()
  {
    if (!m_scene || !m_targets[0][0].IsValid())
    {
      return;
    }
    for (FrameRecord& rFrame : m_frames)
    {
      const VkDescriptorBufferInfo bufferInfo = rFrame.UniformBuffer.GetDescriptorBufferInfo();
      for (uint32_t parity = 0; parity < 2; ++parity)
      {
        for (uint32_t passIndex = 0; passIndex < PixelArtConfig::PassCount; ++passIndex)
        {
          const PassRecord& pass = m_scene->Passes[passIndex];
          if (!pass.Enabled)
          {
            continue;
          }
          std::array<VkDescriptorImageInfo, PixelArtConfig::ChannelCount> imageInfos{};
          for (uint32_t channelIndex = 0; channelIndex < PixelArtConfig::ChannelCount; ++channelIndex)
          {
            const ChannelRecord& channel = pass.Channels[channelIndex];
            VkDescriptorImageInfo& rImageInfo = imageInfos[channelIndex];
            rImageInfo.sampler = channel.Sampler;
            rImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            if (channel.Source == PixelArtChannelSource::Buffer)
            {
              // A buffer drawn earlier this frame has its new output, the pass itself or a later buffer has the output of the last frame
              const uint32_t target = channel.BufferIndex < passIndex ? (1u - parity) : parity;
              rImageInfo.imageView = m_targets[channel.BufferIndex][target].GetImageView();
            }
            else
            {
              rImageInfo.imageView = channel.Texture->GetImageView();
            }
          }

          const VkDescriptorSet descriptorSet = rFrame.DescriptorSets[parity][passIndex];
          std::array<VkWriteDescriptorSet, LocalConfig::BindingCount> writes{};
          writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
          writes[0].dstSet = descriptorSet;
          writes[0].dstBinding = 0;
          writes[0].descriptorCount = 1;
          writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
          writes[0].pBufferInfo = &bufferInfo;
          for (uint32_t channelIndex = 0; channelIndex < PixelArtConfig::ChannelCount; ++channelIndex)
          {
            VkWriteDescriptorSet& rWrite = writes[1 + channelIndex];
            rWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            rWrite.dstSet = descriptorSet;
            rWrite.dstBinding = 1 + channelIndex;
            rWrite.descriptorCount = 1;
            rWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            rWrite.pImageInfo = &imageInfos[channelIndex];
          }
          vkUpdateDescriptorSets(m_device.Get(), UncheckedNumericCast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }
      }
    }
  }


  void PixelArtRendererVulkan::DrawPass(const VkCommandBuffer hCmdBuffer, const uint32_t frameIndex, const PixelArtPass pass,
                                        const PixelArtFrameState& frameState, const PxSize2D passSizePx)
  {
    const uint32_t passIndex = PixelArtPassUtil::ToIndex(pass);
    const PassRecord& rPass = m_scene->Passes[passIndex];
    const bool isImagePass = pass == PixelArtPass::Image;

    PassPushConstants pushConstants;
    const auto passWidth = static_cast<float>(passSizePx.RawWidth());
    const auto passHeight = static_cast<float>(passSizePx.RawHeight());
    pushConstants.Resolution = {passWidth, passHeight, 1.0f, 0.0f};
    {
      // iMouse is given in the pixels of the pass
      const float scaleX = frameState.SceneSizePx.RawWidth() > 0 ? passWidth / static_cast<float>(frameState.SceneSizePx.RawWidth()) : 1.0f;
      const float scaleY = frameState.SceneSizePx.RawHeight() > 0 ? passHeight / static_cast<float>(frameState.SceneSizePx.RawHeight()) : 1.0f;
      const auto& mouse = frameState.MouseWindowPx;
      pushConstants.Mouse = {mouse[0] * scaleX, mouse[1] * scaleY, mouse[2] * scaleX, mouse[3] * scaleY};
    }
    {
      // What happens to the color of the image pass: a sRGB swapchain encodes linear colors to sRGB
      float outputMode = 0.0f;
      if (isImagePass)
      {
        if (m_scene->Output == PixelArtOutput::Linear && !m_srgbFramebuffer)
        {
          outputMode = 1.0f;
        }
        else if (m_scene->Output == PixelArtOutput::Gamma && m_srgbFramebuffer)
        {
          outputMode = 2.0f;
        }
      }
      pushConstants.Pass = {isImagePass ? 1.0f : 0.0f, outputMode, 0.0f, 0.0f};
    }
    for (uint32_t channelIndex = 0; channelIndex < PixelArtConfig::ChannelCount; ++channelIndex)
    {
      const ChannelRecord& channel = rPass.Channels[channelIndex];
      const std::size_t resolutionIndex = std::size_t{channelIndex} * 4u;
      float width = 0.0f;
      float height = 0.0f;
      if (channel.Source == PixelArtChannelSource::Buffer)
      {
        width = static_cast<float>(m_targetSizePx.RawWidth());
        height = static_cast<float>(m_targetSizePx.RawHeight());
      }
      else if (channel.Texture)
      {
        const VkExtent2D extent = channel.Texture->GetExtent2D();
        width = static_cast<float>(extent.width);
        height = static_cast<float>(extent.height);
      }
      pushConstants.ChannelResolution[resolutionIndex + 0] = width;
      pushConstants.ChannelResolution[resolutionIndex + 1] = height;
      pushConstants.ChannelResolution[resolutionIndex + 2] = 1.0f;
    }

    const VkDescriptorSet descriptorSet = m_frames.at(frameIndex).DescriptorSets[m_bufferFrame % 2][passIndex];
    vkCmdBindPipeline(hCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rPass.Pipeline.Get());
    vkCmdBindDescriptorSets(hCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout.Get(), 0, 1, &descriptorSet, 0, nullptr);
    vkCmdPushConstants(hCmdBuffer, m_pipelineLayout.Get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PassPushConstants), &pushConstants);
    vkCmdDraw(hCmdBuffer, 3, 1, 0, 0);
  }
}
