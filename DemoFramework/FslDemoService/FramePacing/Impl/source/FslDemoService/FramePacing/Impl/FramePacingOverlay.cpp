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
      //! The static grids of the main marker (frame, start or end) and the optional sync marker share one vertex buffer
      constexpr std::size_t MainGridFirstVertex = 0;
      constexpr std::size_t SyncGridFirstVertex = FM::GridVertexCount(FM::MarkerKind::Frame);
      constexpr std::size_t VertexCapacity = SyncGridFirstVertex + FM::GridVertexCount(FM::MarkerKind::Sync);
      //! The per-frame indices of both markers
      constexpr std::size_t IndexCapacity = 2 * FM::MaxIndexCount();

      static_assert(VertexCapacity <= (static_cast<std::size_t>(std::numeric_limits<uint16_t>::max()) + 1u), "every index must fit 16 bits");
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

    static_assert(FramePacingSequenceId::ByteCount == FM::SequenceIdByteCount);

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
    //! The static grids generated by the library
    std::array<FM::Vertex, LocalConfig::VertexCapacity> GridVertices{};
    //! The static grids in the render system vertex format
    std::vector<VertexPositionColorTexture> Vertices;
    //! The per-frame indices generated by the library
    std::array<uint32_t, LocalConfig::IndexCapacity> MarkerIndices{};
    //! The per-frame indices in the render system index format
    std::vector<uint16_t> Indices;

    Buffers()
      : Vertices(LocalConfig::VertexCapacity)
      , Indices(LocalConfig::IndexCapacity)
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
    const FM::StartMetadata metadata{FM::ToDateTimeTicks(record.RunStartTime), FM::SequenceId{record.RunSequenceId.Bytes}};

    // The main marker (frame, start or end) is drawn at the top left and the sync marker at the bottom left. Both grids are kept up to date
    // so the sync marker can be enabled at any time
    const FM::Point mainOrigin = FM::RecommendedOrigin(payload.Kind, windowWidthPx, windowHeightPx, options, alignPx);
    const FM::Point syncOrigin = FM::RecommendedOrigin(FM::MarkerKind::Sync, windowWidthPx, windowHeightPx, options, alignPx);
    const GridKey gridKey{options.ModuleSizePx, options.QuietZoneModules, mainOrigin.X, mainOrigin.Y, syncOrigin.X, syncOrigin.Y};
    if (gridKey != m_gridKey)
    {
      // The grids only change when the window size or the marker options change
      if (!TryUpdateGrids(options, mainOrigin, syncOrigin))
      {
        return;
      }
      m_gridKey = gridKey;
    }

    // Encode the marker once, then generate its indices into the static grid: the background, then two triangles (TL, TR, BL)(BL, TR, BR)
    // per horizontal run of dark modules
    FM::ModuleMatrix matrix;
    if (!FM::GenerateModules(payload, matrix, metadata))
    {
      return;
    }
    const std::span<uint32_t> markerIndices(m_buffers->MarkerIndices);
    std::size_t indexCount = FM::ModulesToGridIndices(matrix, markerIndices, static_cast<uint32_t>(LocalConfig::MainGridFirstVertex));
    bool syncMarkerDrawn = false;
    if (record.SyncMarkerEnabled && indexCount > 0)
    {
      // The sync marker carries the same frame index (the analysis detects tearing when the two disagree)
      FM::Payload syncPayload = payload;
      syncPayload.Kind = FM::MarkerKind::Sync;
      FM::ModuleMatrix syncMatrix;
      if (FM::GenerateModules(syncPayload, syncMatrix))
      {
        const std::size_t syncIndexCount =
          FM::ModulesToGridIndices(syncMatrix, markerIndices.subspan(indexCount), static_cast<uint32_t>(LocalConfig::SyncGridFirstVertex));
        indexCount += syncIndexCount;
        syncMarkerDrawn = syncIndexCount > 0;
      }
    }
    if (indexCount == 0)
    {
      return;
    }

    // Convert to the render system index format (every index fits 16 bits, see LocalConfig::VertexCapacity)
    std::vector<uint16_t>& indices = m_buffers->Indices;
    for (std::size_t i = 0; i < indexCount; ++i)
    {
      indices[i] = static_cast<uint16_t>(markerIndices[i]);
    }

    // SetData must only be called once per frame per buffer
    m_indexBuffer->SetData(ReadOnlySpan<uint16_t>(indices.data(), indexCount));

    IBasicRenderSystem& renderSystem = *m_renderSystem;
    renderSystem.BeginCmds();
    renderSystem.CmdSetCamera(BasicCameraInfo(m_projection));
    renderSystem.CmdBindMaterial(m_material);
    renderSystem.CmdBindVertexBuffer(m_vertexBuffer);
    renderSystem.CmdBindIndexBuffer(m_indexBuffer);
    renderSystem.CmdDrawIndexed(static_cast<uint32_t>(indexCount), 0);
    renderSystem.EndCmds();

    // Report every value the marker carried (the values the framework does not know stay empty)
    FramePacingMarkerInfo markerInfo;
    markerInfo.Kind = record.Kind;
    markerInfo.FrameIndex = record.FrameIndex;
    markerInfo.AnimationTime = TimeSpan(record.AnimationTicks);
    markerInfo.RunId = record.RunId;
    if (record.CpuStartTicks > 0)
    {
      markerInfo.CpuStartTime = TickCount(record.CpuStartTicks);
    }
    if (cpuBusyTicks > 0u)
    {
      markerInfo.CpuBusyTime = TimeSpan(static_cast<int64_t>(cpuBusyTicks));
    }
    if (record.Kind == FramePacingMarkerKind::SequenceStart)
    {
      markerInfo.RunStartTime = record.RunStartTime;
      markerInfo.RunSequenceId = record.RunSequenceId;
    }
    markerInfo.SyncMarker = syncMarkerDrawn;
    m_service->SetLastMarker(markerInfo);
  }


  bool FramePacingOverlay::TryUpdateGrids(const MB::FrameMarker::Options& options, const MB::FrameMarker::Point mainOrigin,
                                          const MB::FrameMarker::Point syncOrigin)
  {
    // The static grids: vertices 0..3 are the light background, then every module corner (dark), each vertex on a pixel corner
    const std::span<FM::Vertex> gridVertices(m_buffers->GridVertices);
    const std::size_t mainCount =
      FM::GridVertices(FM::MarkerKind::Frame, options, mainOrigin, gridVertices.subspan(LocalConfig::MainGridFirstVertex));
    const std::size_t syncCount = FM::GridVertices(FM::MarkerKind::Sync, options, syncOrigin, gridVertices.subspan(LocalConfig::SyncGridFirstVertex));
    if (mainCount != LocalConfig::SyncGridFirstVertex || (LocalConfig::SyncGridFirstVertex + syncCount) != LocalConfig::VertexCapacity)
    {
      return false;
    }

    // Convert to the render system format (the fill texture is white so the vertex color is the final color)
    std::vector<VertexPositionColorTexture>& vertices = m_buffers->Vertices;
    for (std::size_t i = 0; i < LocalConfig::VertexCapacity; ++i)
    {
      const FM::Vertex& src = gridVertices[i];
      vertices[i] = VertexPositionColorTexture(static_cast<float>(src.X), static_cast<float>(src.Y), 0.0f,
                                               src.Luma == 0 ? Colors::Black() : Colors::White(), 0.5f, 0.5f);
    }

    // SetData must only be called once per frame per buffer
    m_vertexBuffer->SetData(
      ReadOnlyFlexVertexSpanUtil::AsSpan(vertices.data(), LocalConfig::VertexCapacity, VertexPositionColorTexture::AsVertexDeclarationSpan()));
    return true;
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
      m_indexBuffer = m_renderSystem->CreateDynamicBuffer(ReadOnlySpan<uint16_t>(), static_cast<uint32_t>(LocalConfig::IndexCapacity));
      // The new vertex buffer holds no grids yet
      m_gridKey = {};
      return true;
    }
    catch (const std::exception& ex)
    {
      m_indexBuffer.reset();
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
