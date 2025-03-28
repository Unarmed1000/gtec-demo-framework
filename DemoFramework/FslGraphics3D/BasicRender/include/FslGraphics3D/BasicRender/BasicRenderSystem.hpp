#ifndef FSLGRAPHICS3D_BASICRENDER_BASICRENDERSYSTEM_HPP
#define FSLGRAPHICS3D_BASICRENDER_BASICRENDERSYSTEM_HPP
/****************************************************************************************************************************************************
 * Copyright 2021-2022 NXP
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
 *    * Neither the name of the NXP. nor the names of
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

#include <FslBase/Log/Log3Core.hpp>
#include <FslBase/Span/ReadOnlyFlexSpan.hpp>
#include <FslGraphics/Render/Basic/BasicBufferType.hpp>
#include <FslGraphics/Render/Basic/IBasicHandleManager.hpp>
#include <FslGraphics/Render/Basic/IBasicRenderSystem.hpp>
#include <FslGraphics3D/BasicRender/BasicRenderSystemCreateInfo.hpp>
#include <FslGraphics3D/BasicRender/Buffer/BasicBufferManager.hpp>
#include <FslGraphics3D/BasicRender/Material/BasicMaterialManager.hpp>
#include <FslGraphics3D/BasicRender/Shader/BasicShaderManager.hpp>
#include <FslGraphics3D/BasicRender/Texture/BasicTextureManager.hpp>
#include <memory>

namespace Fsl
{
  struct BasicNativeDependentCreateInfo;

  namespace Graphics3D
  {
    struct BasicRenderSystemCreateInfo;

    class BasicRenderSystem final : public IBasicRenderSystem
    {
      struct DeviceResources
      {
        DeviceResources(const DeviceResources&) = delete;
        DeviceResources& operator=(const DeviceResources&) = delete;
        DeviceResources(DeviceResources&& other) noexcept = delete;
        DeviceResources& operator=(DeviceResources&& other) noexcept = delete;

        bool IsValid{false};
        std::shared_ptr<INativeDevice> Device;
        BasicShaderManager Shaders;
        BasicTextureManager Textures;
        BasicBufferManager Buffers;
        BasicMaterialManager Materials;

        explicit DeviceResources(const Graphics3D::BasicRenderSystemCreateInfo& createInfo)
          : Device(createInfo.Device)
          , Shaders(createInfo.Device)
          , Textures(createInfo.MaxFramesInFlight, createInfo.Device)
          , Buffers(createInfo.MaxFramesInFlight, createInfo.Device)
          , Materials(createInfo.MaxFramesInFlight, createInfo.Device, Shaders)
        {
        }

        void CreateDependentResources()
        {
          Textures.CreateDependentResources();
          Buffers.CreateDependentResources();
          Materials.CreateDependentResources();
        }

        void DestroyDependentResources()
        {
          Materials.DestroyDependentResources();
          Buffers.DestroyDependentResources();
          Textures.DestroyDependentResources();
        }

        void OnRenderSystemEvent(const BasicRenderSystemEvent theEvent)
        {
          switch (theEvent)
          {
          case BasicRenderSystemEvent::SwapchainRecreated:
            Buffers.OnRenderSystemEvent(theEvent);
            Textures.OnRenderSystemEvent(theEvent);
            Materials.OnRenderSystemEvent(theEvent);
            break;
          default:
            Materials.OnRenderSystemEvent(theEvent);
            Textures.OnRenderSystemEvent(theEvent);
            Buffers.OnRenderSystemEvent(theEvent);
            break;
          }
        }

        void PreUpdate()
        {
          Materials.PreUpdate();
          Buffers.PreUpdate();
          Textures.PreUpdate();
        }
      };

      struct DependentResources
      {
        // As long as we dont contain any RAII resources we can allow these
        // DependentResources(const DependentResources&) = delete;
        // DependentResources& operator=(const DependentResources&) = delete;
        // DependentResources(DependentResources&& other) noexcept = delete;
        // DependentResources& operator=(DependentResources&& other) noexcept = delete;

        bool IsValid{false};

        DependentResources() = default;

        explicit DependentResources(bool isValid)
          : IsValid(isValid)
        {
        }

        void Reset()
        {
          IsValid = false;
        }
      };

      enum class CachingState
      {
        Invalid,
        UserCached,
        ForceCached
      };

      struct FrameRecord
      {
        // As long as we dont contain any RAII resources we can allow these
        // FrameRecord(const FrameRecord&) = delete;
        // FrameRecord& operator=(const FrameRecord&) = delete;
        // FrameRecord(FrameRecord&& other) noexcept = delete;
        // FrameRecord& operator=(FrameRecord&& other) noexcept = delete;

        bool IsValid{false};
        CachingState CacheState{CachingState::Invalid};
        bool BeginCommands{false};
        bool CameraBound{false};

        FrameRecord() = default;

        explicit FrameRecord(bool isValid)
          : IsValid(isValid)
        {
        }
      };

      class HandleManager final : public IBasicHandleManager
      {
        BasicShaderManager* m_pShaders{nullptr};

      public:
        HandleManager(const HandleManager&) = delete;
        HandleManager& operator=(const HandleManager&) = delete;
        HandleManager(HandleManager&& other) noexcept = delete;
        HandleManager& operator=(HandleManager&& other) noexcept = delete;

        explicit HandleManager(BasicShaderManager* pShaders)
          : m_pShaders(pShaders)
        {
        }

        ~HandleManager() noexcept override
        {
          Dispose();
        }

        void Dispose() noexcept
        {
          m_pShaders = nullptr;
        }

        // IBasicHandleManager
        bool DestroyHandle(const BasicShaderHandle handle) noexcept final
        {
          if (m_pShaders != nullptr)
          {
            return m_pShaders->DestroyShader(handle);
          }
          FSLLOG3_DEBUG_WARNING("Handle destroyed after the render system was shutdown");
          return false;
        }
      };


      std::unique_ptr<DeviceResources> m_deviceResources;
      std::shared_ptr<HandleManager> m_handleManager;
      DependentResources m_dependentResources;

      FrameRecord m_frame;

    public:
      explicit BasicRenderSystem(const BasicRenderSystemCreateInfo& createInfo);
      ~BasicRenderSystem() override;

      //! Throw away all resources
      void Dispose() noexcept;

      void CreateDependentResources(const BasicNativeDependentCreateInfo& createInfo);
      void DestroyDependentResources();
      void OnRenderSystemEvent(const BasicRenderSystemEvent theEvent);
      void PreUpdate();

      void BeginFrame(const BasicNativeBeginFrameInfo& frameInfo);
      void EndFrame();

      // IBasicRenderSystem
      std::shared_ptr<INativeTexture2D> CreateTexture2D(const Bitmap& bitmap, const Texture2DFilterHint filterHint,
                                                        const TextureFlags textureFlags) final;
      std::shared_ptr<IDynamicNativeTexture2D> CreateDynamicTexture2D(const Bitmap& bitmap, const Texture2DFilterHint filterHint,
                                                                      const TextureFlags textureFlags) final;
      std::shared_ptr<INativeTexture2D> CreateTexture2D(const ReadOnlyRawBitmap& bitmap, const Texture2DFilterHint filterHint,
                                                        const TextureFlags textureFlags) final;
      std::shared_ptr<IDynamicNativeTexture2D> CreateDynamicTexture2D(const ReadOnlyRawBitmap& bitmap, const Texture2DFilterHint filterHint,
                                                                      const TextureFlags textureFlags) final;
      std::shared_ptr<INativeTexture2D> CreateTexture2D(const Texture& texture, const Texture2DFilterHint filterHint,
                                                        const TextureFlags textureFlags) final;
      std::shared_ptr<IDynamicNativeTexture2D> CreateDynamicTexture2D(const Texture& texture, const Texture2DFilterHint filterHint,
                                                                      const TextureFlags textureFlags) final;
      std::shared_ptr<INativeTexture2D> CreateTexture2D(const ReadOnlyRawTexture& texture, const Texture2DFilterHint filterHint,
                                                        const TextureFlags textureFlags) final;
      std::shared_ptr<IDynamicNativeTexture2D> CreateDynamicTexture2D(const ReadOnlyRawTexture& texture, const Texture2DFilterHint filterHint,
                                                                      const TextureFlags textureFlags) final;

      PxExtent2D GetTextureExtentPx(const std::shared_ptr<INativeTexture2D>& texture) const noexcept final;

      const IBasicNativeTexture* TryGetNativeTexture(const BasicNativeTextureHandle& hTexture) const noexcept final;

      std::shared_ptr<IBasicStaticBuffer> CreateBuffer(const ReadOnlySpan<uint16_t> indexSpan, const BasicBufferUsage usage) final;
      std::shared_ptr<IBasicDynamicBuffer> CreateDynamicBuffer(const ReadOnlySpan<uint16_t> indexSpan) final;
      std::shared_ptr<IBasicDynamicBuffer> CreateDynamicBuffer(const ReadOnlySpan<uint16_t> indexSpan, const uint32_t capacity) final;
      std::shared_ptr<IBasicStaticBuffer> CreateStaticBuffer(const ReadOnlySpan<uint16_t> indexSpan) final;
      std::shared_ptr<IBasicStaticBuffer> CreateBuffer(const ReadOnlyFlexVertexSpan& vertexSpan, const BasicBufferUsage usage) final;
      std::shared_ptr<IBasicDynamicBuffer> CreateDynamicBuffer(const ReadOnlyFlexVertexSpan& vertexSpan) final;
      std::shared_ptr<IBasicDynamicBuffer> CreateDynamicBuffer(const ReadOnlyFlexVertexSpan& vertexSpan, const uint32_t capacity) final;
      std::shared_ptr<IBasicDynamicBuffer> CreateDynamicBuffer(const VertexDeclarationSpan vertexDeclaration, const uint32_t capacity) final;
      std::shared_ptr<IBasicStaticBuffer> CreateStaticBuffer(const ReadOnlyFlexVertexSpan& vertexSpan) final;

      // Shaders
      BasicShader CreateShader(const BasicShaderCreateInfo& createInfo) final;

      BasicMaterial CreateMaterial(const BasicMaterialCreateInfo& createInfo, const std::shared_ptr<INativeTexture2D>& texture,
                                   const bool isDynamic) final;
      // void DestroyMaterial(BasicMaterial)
      BasicMaterial CloneMaterial(const BasicMaterial& sourceMaterial, const std::shared_ptr<INativeTexture2D>& texture, const bool isDynamic) final;
      BasicMaterial CloneMaterial(const BasicMaterial& sourceMaterial, const BasicMaterialInfo& materialInfo, const bool isDynamic) final;
      BasicMaterialInfo GetMaterialInfo(const BasicMaterial& sourceMaterial) const final;
      void SetMaterialInfo(const BasicMaterial& sourceMaterial, const BasicMaterialInfo& materialInfo) final;

      std::shared_ptr<INativeTexture2D> GetMaterialTexture(const BasicMaterial& hMaterial) const final;
      std::shared_ptr<INativeTexture2D> TryGetMaterialTexture(const BasicMaterial& hMaterial) const final;

      void BeginCache() final;
      void EndCache() final;

      void BeginCmds() final;
      void EndCmds() final;
      void CmdSetCamera(const BasicCameraInfo& cameraInfo) final;
      void CmdBindMaterial(const BasicMaterial& material) final;
      void CmdBindIndexBuffer(const std::shared_ptr<IBasicStaticBuffer>& indexBuffer) final;
      void CmdBindVertexBuffer(const std::shared_ptr<IBasicStaticBuffer>& vertexBuffer) final;
      void CmdDraw(const uint32_t vertexCount, const uint32_t firstVertex) final;
      void CmdDrawIndexed(const uint32_t indexCount, const uint32_t firstIndex) final;

    private:
      void DoBeginCache(const bool forced);
      void DoEndCache();
      std::shared_ptr<IBasicStaticBuffer> DoCreateBuffer(const BasicBufferType bufferType, const ReadOnlyFlexSpan span, const BasicBufferUsage usage);
      std::shared_ptr<IBasicDynamicBuffer> DoCreateDynamicBuffer(const BasicBufferType bufferType, const ReadOnlyFlexSpan span);
      std::shared_ptr<IBasicDynamicBuffer> DoCreateDynamicBuffer(const BasicBufferType bufferType, const ReadOnlyFlexSpan span,
                                                                 const uint32_t capacity);
      std::shared_ptr<IBasicStaticBuffer> DoCreateStaticBuffer(const BasicBufferType bufferType, const ReadOnlyFlexSpan span);
    };
  }
}

#endif
