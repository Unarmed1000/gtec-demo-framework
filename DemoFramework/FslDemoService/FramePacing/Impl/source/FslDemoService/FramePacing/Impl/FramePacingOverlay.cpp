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

#include <FslBase/Math/Pixel/PxRectangle.hpp>
#include <FslBase/Math/Pixel/PxSize2D.hpp>
#include <FslBase/Span/SpanUtil_Array.hpp>
#include <FslDemoApp/Shared/Host/DemoWindowMetrics.hpp>
#include <FslDemoService/FramePacing/Impl/FramePacingFrameRecord.hpp>
#include <FslDemoService/FramePacing/Impl/FramePacingOverlay.hpp>
#include <FslDemoService/FramePacing/Impl/IFramePacingServiceControl.hpp>
#include <FslDemoService/Graphics/IGraphicsService.hpp>
#include <FslGraphics/Bitmap/ReadOnlyRawBitmap.hpp>
#include <FslGraphics/Colors.hpp>
#include <FslGraphics/Render/Adapter/INativeBatch2D.hpp>
#include <FslService/Consumer/ServiceProvider.hpp>
#include <mb/framemarker/FrameMarker.hpp>
#include <array>
#include <limits>
#include <span>
#include <utility>

namespace Fsl
{
  namespace
  {
    namespace FM = MB::FrameMarker;

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
  }

  struct FramePacingOverlay::QuadBuffer
  {
    std::array<FM::Quad, FM::MaxQuadCount()> Quads{};
  };


  std::unique_ptr<FramePacingOverlay> FramePacingOverlay::TryCreate(const ServiceProvider& serviceProvider)
  {
    auto service = serviceProvider.TryGet<IFramePacingServiceControl>();
    auto graphicsService = serviceProvider.TryGet<IGraphicsService>();
    if (!service || !graphicsService)
    {
      return {};
    }
    return std::make_unique<FramePacingOverlay>(std::move(service), std::move(graphicsService));
  }


  FramePacingOverlay::FramePacingOverlay(std::shared_ptr<IFramePacingServiceControl> service, std::shared_ptr<IGraphicsService> graphicsService)
    : m_service(std::move(service))
    , m_graphicsService(std::move(graphicsService))
    , m_quads(std::make_unique<QuadBuffer>())
  {
  }


  FramePacingOverlay::~FramePacingOverlay() = default;


  void FramePacingOverlay::Draw(const DemoWindowMetrics& windowMetrics)
  {
    FramePacingFrameRecord record;
    if (!m_service->TryGetFrameRecord(record))
    {
      return;
    }

    const int32_t windowWidthPx = ToInt32(windowMetrics.ExtentPx.Width.Value);
    const int32_t windowHeightPx = ToInt32(windowMetrics.ExtentPx.Height.Value);
    if (windowWidthPx <= 0 || windowHeightPx <= 0)
    {
      return;
    }

    const std::shared_ptr<INativeBatch2D> batch = m_graphicsService->GetNativeBatch2D();
    if (!batch)
    {
      return;
    }

    if (!m_fillTexture.IsValid())
    {
      constexpr std::array<uint8_t, 4> WhitePixel = {0xFF, 0xFF, 0xFF, 0xFF};
      const auto rawBitmap =
        ReadOnlyRawBitmap::Create(SpanUtil::AsReadOnlySpan(WhitePixel), PxSize2D::Create(1, 1), PixelFormat::R8G8B8A8_UNORM, BitmapOrigin::UpperLeft);
      m_fillTexture.Reset(m_graphicsService->GetNativeGraphics(), rawBitmap, Texture2DFilterHint::Nearest);
    }

    const int32_t moduleSizePx = record.CaptureHeightPx > 0 ? FM::RecommendModuleSizePx(windowHeightPx, record.CaptureHeightPx) : record.ModuleSizePx;
    const FM::Options options{moduleSizePx, FM::RecommendedQuietZoneModules};
    const int32_t alignPx = CalcAlignPx(windowHeightPx, record.CaptureHeightPx);
    const FM::Payload payload{record.FrameIndex, record.AnimationTicks, record.RunId, ToMarkerKind(record.Kind)};
    const FM::StartMetadata metadata{record.StartUtcTicks, record.RunName.AsStringView()};

    // Start and end markers are only drawn once (the start marker is larger than the frame marker so it could overlap the other slots)
    std::array<FM::MarkerSlot, 3> slots{FM::MarkerSlot::TopLeft, FM::MarkerSlot::MiddleLeft, FM::MarkerSlot::BottomLeft};
    std::size_t slotCount = 3;
    if (record.Slot != FramePacingMarkerSlot::All || record.Kind != FramePacingMarkerKind::Frame)
    {
      slots[0] = ToMarkerSlot(record.Slot);
      slotCount = 1;
    }

    const std::span<FM::Quad> dstQuads(m_quads->Quads);

    // The marker must not be blended, it has to reach the capture as pure black and white
    batch->Begin(BlendState::Opaque);
    for (std::size_t slotIndex = 0; slotIndex < slotCount; ++slotIndex)
    {
      const FM::Point origin = FM::RecommendedOrigin(slots[slotIndex], windowWidthPx, windowHeightPx, options, alignPx);
      const std::size_t quadCount = record.Kind == FramePacingMarkerKind::SequenceStart
                                      ? FM::GenerateStartQuads(payload, metadata, options, origin, dstQuads)
                                      : FM::GenerateQuads(payload, options, origin, dstQuads);
      for (std::size_t i = 0; i < quadCount; ++i)
      {
        const FM::Quad& quad = dstQuads[i];
        batch->Draw(m_fillTexture, PxRectangle::CreateFromLeftTopRightBottom(quad.Left, quad.Top, quad.Right, quad.Bottom),
                    quad.Dark ? Colors::Black() : Colors::White());
      }
    }
    batch->End();
  }
}
