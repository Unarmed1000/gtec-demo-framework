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
#include <FslBase/Math/Pixel/PxVector2.hpp>
#include <FslGraphics/Sprite/Info/Core/RenderBasicImageInfo.hpp>
#include <FslSimpleUI/Base/PropertyTypeFlags.hpp>
#include <FslSimpleUI/Base/UIDrawContext.hpp>
#include <FslSimpleUI/Base/WindowContext.hpp>
#include <FslSimpleUI/Render/Base/DrawCommandBuffer.hpp>
#include <FslSimpleUI/Render/Base/ICustomDrawData.hpp>
#include <FslSimpleUI/Render/Base/UIRenderColor.hpp>
#include <FslSimpleUI/Render/Builder/UIRawBasicMeshBuilder2D.hpp>
#include <Shared/FramePacing/SampleAnimationErrorChart.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
      //! How wide a frame is
      constexpr int32_t EntryWidthDp = 1;
      //! The dashes of the refresh lines and the space between them, as the report has them (3 on, 4 off)
      constexpr int32_t DashOnDp = 3;
      constexpr int32_t DashOffDp = 4;
      //! How long the mark at the edge is that a bar gets that would be longer than the scale
      constexpr int32_t ClipMarkDp = 3;

      //! The colors of the report of the mb-framepacing tools
      constexpr UI::UIColor BarColor(PackedColor32(0xFFE5534B));            // red
      constexpr UI::UIColor ClipMarkColor(PackedColor32(0xFFFFFFFF));       // white
      constexpr UI::UIColor BandColor(PackedColor32(0x0FFFFFFF));           // white, 6 %
      constexpr UI::UIColor ZeroLineColor(PackedColor32(0x73FFFFFF));       // white, 45 %
      constexpr UI::UIColor RefreshLineColor(PackedColor32(0xCCD29922));    // amber, 80 %

      //! The value of a frame that is not judged
      constexpr int16_t Gap = std::numeric_limits<int16_t>::min();
    }

    //! The number of rectangles a chart that is the given size draws at the most
    uint32_t CalcMaxRects(const int32_t widthPx, const int32_t entryWidthPx, const int32_t dashPeriodPx) noexcept
    {
      const int32_t entries = (widthPx / std::max(entryWidthPx, 1)) + 1;
      const int32_t dashes = (widthPx / std::max(dashPeriodPx, 1)) + 1;
      // A bar and a mark for each frame, two dashed lines, the band and the line of no error
      return static_cast<uint32_t>((2 * entries) + (2 * dashes) + 2);
    }
  }

  struct SampleAnimationErrorChart::DrawData final : public UI::ICustomDrawData
  {
    //! The frames, the oldest first when read from First
    std::array<int16_t, SampleAnimationErrorChart::Capacity> Values{};
    uint32_t First{0};
    uint32_t Count{0};
    int32_t BandRefreshThousandths{0};
    //! In pixels
    int32_t EntryWidthPx{1};
    int32_t DashOnPx{3};
    int32_t DashOffPx{4};
    int32_t ClipMarkPx{3};
    int32_t LinePx{1};
    //! Premultiplied
    UI::UIRenderColor BarColor;
    UI::UIRenderColor ClipMarkColor;
    UI::UIRenderColor BandColor;
    UI::UIRenderColor ZeroLineColor;
    UI::UIRenderColor RefreshLineColor;

    void SetResolution(const SpriteUnitConverter& unitConverter) noexcept
    {
      EntryWidthPx = std::max(unitConverter.DpToPxSize1D(LocalConfig::EntryWidthDp).RawValue(), 1);
      DashOnPx = std::max(unitConverter.DpToPxSize1D(LocalConfig::DashOnDp).RawValue(), 1);
      DashOffPx = std::max(unitConverter.DpToPxSize1D(LocalConfig::DashOffDp).RawValue(), 1);
      ClipMarkPx = std::max(unitConverter.DpToPxSize1D(LocalConfig::ClipMarkDp).RawValue(), 1);
      LinePx = std::max(unitConverter.DpToPxSize1D(1).RawValue(), 1);
    }
  };

  namespace
  {
    void DrawChart(UI::UIRawBasicMeshBuilder2D& rBuilder, const PxVector2 dstPositionPxf, const PxSize2D dstSizePx,
                   const UI::DrawClipContext& clipContext, const RenderBasicImageInfo& renderInfo, const UI::ICustomDrawData* const pCustomDrawData);
  }


  SampleAnimationErrorChart::SampleAnimationErrorChart(const std::shared_ptr<UI::BaseWindowContext>& context)
    : UI::BaseWindow(context)
    , m_drawData(std::make_shared<DrawData>())
    , m_mesh(context->TheUIContext.Get()->MeshManager, 6)
  {
    m_drawData->SetResolution(context->UnitConverter);
    Enable(UI::WindowFlags(UI::WindowFlags::DrawEnabled | UI::WindowFlags::PostLayoutEnabled));
  }


  SampleAnimationErrorChart::~SampleAnimationErrorChart() = default;


  void SampleAnimationErrorChart::SetFillSprite(const std::shared_ptr<BasicImageSprite>& value)
  {
    if (m_mesh.SetSprite(value))
    {
      PropertyUpdated(UI::PropertyType::Content);
    }
  }


  void SampleAnimationErrorChart::SetNoErrorBand(const int32_t refreshThousandths)
  {
    const int32_t band = std::clamp(refreshThousandths, 0, ScaleRefreshThousandths);
    if (band != m_drawData->BandRefreshThousandths)
    {
      m_drawData->BandRefreshThousandths = band;
      PropertyUpdated(UI::PropertyType::Content);
    }
  }


  void SampleAnimationErrorChart::AddError(const int32_t refreshThousandths)
  {
    // What is beyond the scale is kept as that (it gets a mark), and no value is the one of a gap
    AddEntry(static_cast<int16_t>(std::clamp(refreshThousandths, -(ScaleRefreshThousandths + 1), ScaleRefreshThousandths + 1)));
  }


  void SampleAnimationErrorChart::AddGap()
  {
    AddEntry(LocalConfig::Gap);
  }


  void SampleAnimationErrorChart::Clear()
  {
    if (m_drawData->Count != 0u)
    {
      m_drawData->First = 0;
      m_drawData->Count = 0;
      PropertyUpdated(UI::PropertyType::Content);
    }
  }


  void SampleAnimationErrorChart::WinResolutionChanged(const UI::ResolutionChangedInfo& info)
  {
    UI::BaseWindow::WinResolutionChanged(info);
    m_drawData->SetResolution(GetContext()->UnitConverter);
  }


  void SampleAnimationErrorChart::WinPostLayout()
  {
    const DrawData& drawData = *m_drawData;
    m_mesh.EnsureCapacity(CalcMaxRects(RenderSizePx().RawWidth(), drawData.EntryWidthPx, drawData.DashOnPx + drawData.DashOffPx) * 6u);
  }


  void SampleAnimationErrorChart::WinDraw(const UI::UIDrawContext& context)
  {
    if (!m_mesh.IsValid())
    {
      return;
    }
    const UI::UIRenderColor finalBaseColor(GetFinalBaseColor());
    {
      const UI::UIColorConverter& colorConverter = GetContext()->ColorConverter;
      DrawData& rDrawData = *m_drawData;
      rDrawData.BarColor = UI::UIRenderColor::Premultiply(colorConverter.Convert(LocalConfig::BarColor) * finalBaseColor);
      rDrawData.ClipMarkColor = UI::UIRenderColor::Premultiply(colorConverter.Convert(LocalConfig::ClipMarkColor) * finalBaseColor);
      rDrawData.BandColor = UI::UIRenderColor::Premultiply(colorConverter.Convert(LocalConfig::BandColor) * finalBaseColor);
      rDrawData.ZeroLineColor = UI::UIRenderColor::Premultiply(colorConverter.Convert(LocalConfig::ZeroLineColor) * finalBaseColor);
      rDrawData.RefreshLineColor = UI::UIRenderColor::Premultiply(colorConverter.Convert(LocalConfig::RefreshLineColor) * finalBaseColor);
    }
    context.CommandBuffer.DrawCustom(m_mesh.Get(), context.TargetRect.Location(), RenderSizePx(), finalBaseColor, context.ClipContext, DrawChart,
                                     m_drawData);
  }


  void SampleAnimationErrorChart::AddEntry(const int16_t value)
  {
    DrawData& rDrawData = *m_drawData;
    if (rDrawData.Count == Capacity)
    {
      rDrawData.First = (rDrawData.First + 1u) % Capacity;
      --rDrawData.Count;
    }
    rDrawData.Values[(rDrawData.First + rDrawData.Count) % Capacity] = value;
    ++rDrawData.Count;
    PropertyUpdated(UI::PropertyType::Content);
  }


  namespace
  {
    void DrawChart(UI::UIRawBasicMeshBuilder2D& rBuilder, const PxVector2 dstPositionPxf, const PxSize2D dstSizePx,
                   const UI::DrawClipContext& clipContext, const RenderBasicImageInfo& renderInfo, const UI::ICustomDrawData* const pCustomDrawData)
    {
      FSL_PARAM_NOT_USED(clipContext);
      const auto* const pDrawData = dynamic_cast<const SampleAnimationErrorChart::DrawData*>(pCustomDrawData);
      const int32_t widthPx = dstSizePx.RawWidth();
      const int32_t heightPx = dstSizePx.RawHeight();
      if (pDrawData == nullptr || widthPx <= 0 || heightPx < 4)
      {
        return;
      }
      const SampleAnimationErrorChart::DrawData& drawData = *pDrawData;
      const float x0 = dstPositionPxf.X.Value;
      const float y0 = dstPositionPxf.Y.Value;
      const auto addRect = [&rBuilder, &renderInfo, x0, y0](const int32_t left, const int32_t top, const int32_t right, const int32_t bottom)
      {
        rBuilder.AddRect(x0 + static_cast<float>(left), y0 + static_cast<float>(top), x0 + static_cast<float>(right),
                         y0 + static_cast<float>(bottom), renderInfo.TextureArea);
      };

      // The line of no error is in the middle, and the scale reaches the edges
      const int32_t zeroPx = heightPx / 2;
      const int32_t halfHeightPx = zeroPx;
      const auto toPx = [halfHeightPx](const int32_t refreshThousandths)
      {
        return static_cast<int32_t>(std::lround((static_cast<double>(refreshThousandths) * static_cast<double>(halfHeightPx)) /
                                                static_cast<double>(SampleAnimationErrorChart::ScaleRefreshThousandths)));
      };

      // What counts as no error
      const int32_t bandPx = toPx(drawData.BandRefreshThousandths);
      if (bandPx > 0)
      {
        rBuilder.SetColor(drawData.BandColor);
        addRect(0, zeroPx - bandPx, widthPx, zeroPx + bandPx);
      }

      // A frame shown a whole refresh too soon (above) and too late (below)
      rBuilder.SetColor(drawData.RefreshLineColor);
      const int32_t refreshPx = toPx(1000);
      const int32_t dashPeriodPx = drawData.DashOnPx + drawData.DashOffPx;
      for (const int32_t linePx : {zeroPx - refreshPx, zeroPx + refreshPx})
      {
        for (int32_t x = 0; x < widthPx; x += dashPeriodPx)
        {
          addRect(x, linePx, std::min(x + drawData.DashOnPx, widthPx), linePx + drawData.LinePx);
        }
      }

      rBuilder.SetColor(drawData.ZeroLineColor);
      addRect(0, zeroPx, widthPx, zeroPx + drawData.LinePx);

      // The frames, the newest at the right edge
      const auto maxEntries = static_cast<uint32_t>(widthPx / drawData.EntryWidthPx);
      const uint32_t entries = std::min(drawData.Count, maxEntries);
      int32_t x = widthPx;
      for (uint32_t i = 0; i < entries; ++i)
      {
        x -= drawData.EntryWidthPx;
        const int16_t value = drawData.Values[(drawData.First + drawData.Count - 1u - i) % SampleAnimationErrorChart::Capacity];
        if (value == LocalConfig::Gap)
        {
          continue;
        }
        const bool isClipped = value > SampleAnimationErrorChart::ScaleRefreshThousandths || value < -SampleAnimationErrorChart::ScaleRefreshThousandths;
        const int32_t barPx = toPx(std::clamp(static_cast<int32_t>(value), -SampleAnimationErrorChart::ScaleRefreshThousandths,
                                              SampleAnimationErrorChart::ScaleRefreshThousandths));
        if (barPx == 0)
        {
          continue;
        }
        // Up is too soon
        const int32_t top = barPx > 0 ? std::max(zeroPx - barPx, 0) : zeroPx;
        const int32_t bottom = barPx > 0 ? zeroPx : std::min(zeroPx - barPx, heightPx);
        rBuilder.SetColor(drawData.BarColor);
        addRect(x, top, x + drawData.EntryWidthPx, bottom);
        if (isClipped)
        {
          // The bar would be longer than the scale
          rBuilder.SetColor(drawData.ClipMarkColor);
          if (barPx > 0)
          {
            addRect(x, top, x + drawData.EntryWidthPx, std::min(top + drawData.ClipMarkPx, bottom));
          }
          else
          {
            addRect(x, std::max(bottom - drawData.ClipMarkPx, top), x + drawData.EntryWidthPx, bottom);
          }
        }
      }
    }
  }
}
