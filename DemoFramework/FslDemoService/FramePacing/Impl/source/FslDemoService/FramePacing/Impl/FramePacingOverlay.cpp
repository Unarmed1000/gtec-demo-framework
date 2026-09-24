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
#include <FslDemoApp/Shared/Host/DemoWindowMetrics.hpp>
#include <FslDemoService/FramePacing/IFramePacingService.hpp>
#include <FslDemoService/FramePacing/Impl/FramePacingFrameRecord.hpp>
#include <FslDemoService/FramePacing/Impl/FramePacingOverlay.hpp>
#include <FslDemoService/FramePacing/Impl/IFramePacingServiceControl.hpp>
#include <FslDemoService/Graphics/IGraphicsService.hpp>
#include <FslGraphics/Bitmap/ReadOnlyRawBitmap.hpp>
#include <FslGraphics/Colors.hpp>
#include <FslGraphics/Render/Basic/BasicCameraInfo.hpp>
#include <FslGraphics/Render/Basic/IBasicDynamicBuffer.hpp>
#include <FslGraphics/Render/Basic/IBasicRenderSystem.hpp>
#include <FslGraphics/Render/Basic/Material/BasicMaterialCreateInfo.hpp>
#include <FslGraphics/Vertices/ReadOnlyFlexVertexSpanUtil.hpp>
#include <FslGraphics/Vertices/VertexPositionColorTexture.hpp>
#include <FslService/Consumer/ServiceProvider.hpp>
#include <mb/framemarker/FrameMarker.hpp>
#include <algorithm>
#include <array>
#include <exception>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace Fsl
{
  namespace
  {
    namespace FM = MB::FrameMarker;

    namespace LocalConfig
    {
      constexpr std::size_t MaxSlots = 3;
      //! A start marker is only drawn in one slot, frame and end markers can be drawn in all slots
      constexpr std::size_t VertexCapacity = std::max(FM::MaxTriangleVertexCount(), MaxSlots* FM::MaxFrameTriangleVertexCount());
    }

    FM::MarkerKind ToMarkerKind(const FramePacingMarkerKind kind) noexcept
    {
      switch (kind)
      {
      case FramePacingMarkerKind::SequenceStart:
        return FM::MarkerKind::SequenceStart;
      case FramePacingMarkerKind::SequenceEnd:
        return FM::MarkerKind::SequenceEnd;
      case FramePacingMarkerKind::Frame:
      default:
        return FM::MarkerKind::Frame;
      }
    }

    FM::MarkerSlot ToMarkerSlot(const FramePacingMarkerSlot slot) noexcept
    {
      switch (slot)
      {
      case FramePacingMarkerSlot::MiddleLeft:
        return FM::MarkerSlot::MiddleLeft;
      case FramePacingMarkerSlot::BottomLeft:
        return FM::MarkerSlot::BottomLeft;
      case FramePacingMarkerSlot::TopLeft:
      case FramePacingMarkerSlot::All:
      default:
        return FM::MarkerSlot::TopLeft;
      }
    }

    int32_t ToInt32(const uint32_t value) noexcept
    {
      return value <= static_cast<uint32_t>(std::numeric_limits<int32_t>::max()) ? static_cast<int32_t>(value) : std::numeric_limits<int32_t>::max();
    }

    //! The integer downscale ratio from the window to the capture (1 if it is not a integer ratio), used to align the marker so module edges
    //! land on stored pixel edges.
    int32_t CalcAlignPx(const int32_t windowHeightPx, const int32_t captureHeightPx) noexcept
    {
      if (captureHeightPx <= 0 || captureHeightPx >= windowHeightPx || (windowHeightPx % captureHeightPx) != 0)
      {
        return 1;
      }
      return windowHeightPx / captureHeightPx;
    }

    //! A pixel space projection: origin at the top-left corner, +y down and integer coordinates on pixel edges.
    //! The vertices use z = 0 which maps to the middle of the depth range (so it is never clipped), the Vulkan clip space adjustment is applied
    //! by the render system.
    Matrix CreatePixelProjection(const PxSize2D sizePx)
    {
      const auto screenWidth = static_cast<float>(sizePx.RawWidth());
      const auto screenHeight = static_cast<float>(sizePx.RawHeight());
      return Matrix::CreateOrthographicOffCenter(0.0f, screenWidth, screenHeight, 0.0f, -1.0f, 1.0f);
    }
  }

  struct FramePacingOverlay::Buffers
  {
    //! The marker geometry generated by the library
    std::array<FM::Vertex, LocalConfig::VertexCapacity> MarkerVertices{};
    //! The marker geometry in the render system vertex format
    std::vector<VertexPositionColorTexture> Vertices;

    Buffers()
      : Vertices(LocalConfig::VertexCapacity)
    {
    }
  };


  std::unique_ptr<FramePacingOverlay> FramePacingOverlay::TryCreate(const ServiceProvider& serviceProvider)
  {
    auto service = serviceProvider.TryGet<IFramePacingServiceControl>();
    auto graphicsService = serviceProvider.TryGet<IGraphicsService>();
    if (!service || !graphicsService)
    {
      return {};
    }
    // If the marker is enabled from the start (command line) the resources are created before the first frame
    const auto framePacingService = serviceProvider.TryGet<IFramePacingService>();
    const bool createResources = framePacingService && framePacingService->IsEnabled();
    return std::make_unique<FramePacingOverlay>(std::move(service), std::move(graphicsService), createResources);
  }


  FramePacingOverlay::FramePacingOverlay(std::shared_ptr<IFramePacingServiceControl> service, std::shared_ptr<IGraphicsService> graphicsService,
                                         const bool createResources)
    : m_service(std::move(service))
    , m_graphicsService(std::move(graphicsService))
    , m_buffers(std::make_unique<Buffers>())
  {
    if (createResources)
    {
      TryCreateResources(false);
    }
  }


  FramePacingOverlay::~FramePacingOverlay() = default;


  void FramePacingOverlay::Draw(const DemoWindowMetrics& windowMetrics)
  {
    FramePacingFrameRecord record;
    if (m_disabled || !m_service->TryGetFrameRecord(record))
    {
      return;
    }

    const int32_t windowWidthPx = ToInt32(windowMetrics.ExtentPx.Width.Value);
    const int32_t windowHeightPx = ToInt32(windowMetrics.ExtentPx.Height.Value);
    if (windowWidthPx <= 0 || windowHeightPx <= 0)
    {
      return;
    }

    if (!m_vertexBuffer)
    {
      // The marker was enabled at runtime. The resources are created on demand so apps that never show the marker never allocate them.
      // Render resources created during a frame can first be used the following frame (the fill texture would not be bound yet), so the
      // marker is first drawn next frame.
      TryCreateResources(true);
      return;
    }

    const PxSize2D sizePx = PxSize2D::Create(windowWidthPx, windowHeightPx);
    if (sizePx != m_cachedSizePx)
    {
      m_cachedSizePx = sizePx;
      m_projection = CreatePixelProjection(sizePx);
    }

    const int32_t moduleSizePx = record.CaptureHeightPx > 0 ? FM::RecommendModuleSizePx(windowHeightPx, record.CaptureHeightPx) : record.ModuleSizePx;
    const FM::Options options{moduleSizePx, FM::RecommendedQuietZoneModules};
    const int32_t alignPx = CalcAlignPx(windowHeightPx, record.CaptureHeightPx);
    const FM::Payload payload{record.FrameIndex, record.AnimationTicks, record.RunId, ToMarkerKind(record.Kind)};
    const FM::StartMetadata metadata{record.StartUtcTicks, record.RunName.AsStringView()};

    // Start and end markers are only drawn once (the start marker is larger than the frame marker so it could overlap the other slots)
    std::array<FM::MarkerSlot, LocalConfig::MaxSlots> slots{FM::MarkerSlot::TopLeft, FM::MarkerSlot::MiddleLeft, FM::MarkerSlot::BottomLeft};
    std::size_t slotCount = LocalConfig::MaxSlots;
    if (record.Slot != FramePacingMarkerSlot::All || record.Kind != FramePacingMarkerKind::Frame)
    {
      slots[0] = ToMarkerSlot(record.Slot);
      slotCount = 1;
    }

    // Generate the triangles for all slots (TL, TR, BL)(BL, TR, BR) per quad, every vertex on a pixel corner
    const std::span<FM::Vertex> markerVertices(m_buffers->MarkerVertices);
    std::size_t vertexCount = 0;
    for (std::size_t slotIndex = 0; slotIndex < slotCount; ++slotIndex)
    {
      const FM::Point origin = FM::RecommendedOrigin(slots[slotIndex], windowWidthPx, windowHeightPx, options, alignPx);
      const std::span<FM::Vertex> dst = markerVertices.subspan(vertexCount);
      vertexCount += record.Kind == FramePacingMarkerKind::SequenceStart ? FM::GenerateStartTriangles(payload, metadata, options, origin, dst)
                                                                         : FM::GenerateTriangles(payload, options, origin, dst);
    }
    if (vertexCount == 0)
    {
      return;
    }

    // Convert to the render system format (the fill texture is white so the vertex color is the final color)
    std::vector<VertexPositionColorTexture>& vertices = m_buffers->Vertices;
    for (std::size_t i = 0; i < vertexCount; ++i)
    {
      const FM::Vertex& src = markerVertices[i];
      vertices[i] = VertexPositionColorTexture(static_cast<float>(src.X), static_cast<float>(src.Y), 0.0f,
                                               src.Luma == 0 ? Colors::Black() : Colors::White(), 0.5f, 0.5f);
    }

    // SetData must only be called once per frame per buffer
    m_vertexBuffer->SetData(ReadOnlyFlexVertexSpanUtil::AsSpan(vertices.data(), vertexCount, VertexPositionColorTexture::AsVertexDeclarationSpan()));

    IBasicRenderSystem& renderSystem = *m_renderSystem;
    renderSystem.BeginCmds();
    renderSystem.CmdSetCamera(BasicCameraInfo(m_projection));
    renderSystem.CmdBindMaterial(m_material);
    renderSystem.CmdBindVertexBuffer(m_vertexBuffer);
    renderSystem.CmdDraw(static_cast<uint32_t>(vertexCount), 0);
    renderSystem.EndCmds();
  }


  bool FramePacingOverlay::TryCreateResources(const bool disableOnFailure)
  {
    try
    {
      m_renderSystem = m_graphicsService->GetBasicRenderSystem();

      constexpr std::array<uint8_t, 4> WhitePixel = {0xFF, 0xFF, 0xFF, 0xFF};
      const auto rawBitmap =
        ReadOnlyRawBitmap::Create(SpanUtil::AsReadOnlySpan(WhitePixel), PxSize2D::Create(1, 1), PixelFormat::R8G8B8A8_UNORM, BitmapOrigin::UpperLeft);
      m_fillTexture = m_renderSystem->CreateTexture2D(rawBitmap, Texture2DFilterHint::Nearest);

      // The marker must reach the capture unmodified: opaque, no depth test/write and no culling
      const BasicMaterialInfo materialInfo(BlendState::Opaque, BasicCullMode::Disabled, BasicFrontFace::CounterClockwise,
                                           BasicMaterialDepthInfo(false, false, BasicCompareOp::Less));
      m_material =
        m_renderSystem->CreateMaterial(BasicMaterialCreateInfo(materialInfo, VertexPositionColorTexture::AsVertexDeclarationSpan()), m_fillTexture);

      m_vertexBuffer = m_renderSystem->CreateDynamicBuffer(VertexPositionColorTexture::AsVertexDeclarationSpan(),
                                                           static_cast<uint32_t>(LocalConfig::VertexCapacity));
      return true;
    }
    catch (const std::exception& ex)
    {
      m_vertexBuffer.reset();
      m_material = {};
      m_fillTexture.reset();
      m_renderSystem.reset();
      if (disableOnFailure)
      {
        FSLLOG3_WARNING("FramePacing: the marker can not be drawn as the basic render system is unavailable: {}", ex.what());
        m_disabled = true;
      }
      else
      {
        FSLLOG3_VERBOSE("FramePacing: render resources not available yet, they will be created on demand: {}", ex.what());
      }
      return false;
    }
  }
}
