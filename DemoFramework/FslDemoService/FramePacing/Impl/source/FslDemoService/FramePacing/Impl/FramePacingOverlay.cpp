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
#include <FslDemoService/FramePacing/Impl/IFramePacingFrameSource.hpp>
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
#include <array>
#include <cstdint>
#include <exception>
#include <limits>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

namespace Fsl
{
  namespace
  {
    namespace FM = MB::FrameMarker;

    namespace LocalConfig
    {
      //! The main marker (frame, start or end) and the optional sync marker
      constexpr std::size_t MaxMarkers = 2;
      constexpr std::size_t VertexCapacity = MaxMarkers * FM::MaxTriangleVertexCount();
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

    static_assert(std::tuple_size_v<decltype(FramePacingFrameRecord::RunSequenceId)> == FM::SequenceIdByteCount);

    //! How long the CPU has worked on the frame so far, in 100ns ticks (0 if unknown)
    uint32_t CalcCpuBusyTicks(const int64_t cpuStartTicks, const int64_t nowTicks) noexcept
    {
      if (cpuStartTicks <= 0 || nowTicks <= cpuStartTicks)
      {
        return 0u;
      }
      const int64_t busyTicks = nowTicks - cpuStartTicks;
      return std::cmp_less_equal(busyTicks, std::numeric_limits<uint32_t>::max()) ? static_cast<uint32_t>(busyTicks)
                                                                                  : std::numeric_limits<uint32_t>::max();
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
    auto service = serviceProvider.TryGet<IFramePacingFrameSource>();
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


  FramePacingOverlay::FramePacingOverlay(std::shared_ptr<IFramePacingFrameSource> service, std::shared_ptr<IGraphicsService> graphicsService,
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
    // The framework has no frame pacer, so the intended display time and the target frame time are unknown (0).
    // The marker is the last thing drawn before the frame is presented, so the CPU busy time is measured now.
    const uint32_t cpuBusyTicks = CalcCpuBusyTicks(record.CpuStartTicks, m_timer.GetTimestamp().Ticks());
    const FM::Payload payload{record.FrameIndex,    record.AnimationTicks, record.RunId, ToMarkerKind(record.Kind), 0, 0u,
                              record.CpuStartTicks, cpuBusyTicks};
    const FM::StartMetadata metadata{record.StartUtcTicks, FM::SequenceId{record.RunSequenceId}};

    // Encode the marker once, then generate the triangles (TL, TR, BL)(BL, TR, BR) per quad, every vertex on a pixel corner.
    // The main marker (frame, start or end) is drawn at the top left
    FM::ModuleMatrix matrix;
    if (!FM::GenerateModules(payload, matrix, metadata))
    {
      return;
    }
    const std::span<FM::Vertex> markerVertices(m_buffers->MarkerVertices);
    const FM::Point origin = FM::RecommendedOrigin(payload.Kind, windowWidthPx, windowHeightPx, options, alignPx);
    std::size_t vertexCount = FM::ModulesToTriangles(matrix, options, origin, markerVertices);
    if (record.SyncMarkerEnabled && vertexCount > 0)
    {
      // The sync marker carries the same frame index at the bottom left (the analysis detects tearing when the two disagree)
      FM::Payload syncPayload = payload;
      syncPayload.Kind = FM::MarkerKind::Sync;
      FM::ModuleMatrix syncMatrix;
      if (FM::GenerateModules(syncPayload, syncMatrix))
      {
        const FM::Point syncOrigin = FM::RecommendedOrigin(FM::MarkerKind::Sync, windowWidthPx, windowHeightPx, options, alignPx);
        vertexCount += FM::ModulesToTriangles(syncMatrix, options, syncOrigin, markerVertices.subspan(vertexCount));
      }
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
