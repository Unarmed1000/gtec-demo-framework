#ifndef SHARED_FRAMEPACING_SAMPLEANIMATIONERRORCHART_HPP
#define SHARED_FRAMEPACING_SAMPLEANIMATIONERRORCHART_HPP
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

#include <FslBase/Math/Pixel/PxSize1D.hpp>
#include <FslGraphics/Sprite/BasicImageSprite.hpp>
#include <FslSimpleUI/Base/BaseWindow.hpp>
#include <FslSimpleUI/Base/Mesh/CustomBasicSpriteBasicMesh.hpp>
#include <FslSimpleUI/Base/UIColor.hpp>
#include <cstdint>
#include <memory>

namespace Fsl
{
  //! The chart of the animation error of the FramePacing samples. It draws as the animation error panel of the report of the
  //! mb-framepacing tools does, so the two read the same:
  //! - A bar for each frame, from the line of no error: up when the frame was shown too soon, down when it was shown too late. A
  //!   frame without error draws nothing, and neither does a frame that is not judged, which is a gap.
  //! - A faint band around the line of no error: what is inside it counts as no error (SampleAnimationError::ErrorThreshold).
  //! - A dashed line where a frame was shown a whole refresh too soon and one where it was shown a whole refresh too late.
  //! The scale is fixed, two refreshes to each side: a bar that would be longer ends at the edge and gets a mark there.
  //!
  //! The newest frame is at the right edge and a frame is as wide as one dp, as in the charts of FslSimpleUI.
  //! The labels of the dashed lines are not drawn by the chart, the layout it is in places them.
  class SampleAnimationErrorChart final : public UI::BaseWindow
  {
  public:
    //! The frames the chart remembers (it draws as many of them as it has room for)
    static constexpr uint32_t Capacity = 4096;
    //! How far the scale goes to each side, in thousandths of a refresh
    static constexpr int32_t ScaleRefreshThousandths = 2000;

    //! What the chart is drawn from (it is defined with the drawing)
    struct DrawData;

  private:
    std::shared_ptr<DrawData> m_drawData;
    UI::CustomBasicSpriteBasicMesh m_mesh;

  public:
    explicit SampleAnimationErrorChart(const std::shared_ptr<UI::BaseWindowContext>& context);
    ~SampleAnimationErrorChart() override;

    //! @brief The sprite everything is drawn with: a fill sprite that can be see through (the band and the lines are)
    void SetFillSprite(const std::shared_ptr<BasicImageSprite>& value);

    //! @brief The band around the line of no error, in thousandths of a refresh to each side
    void SetNoErrorBand(const int32_t refreshThousandths);

    //! @brief Add a frame.
    //! @param refreshThousandths its animation error in thousandths of a refresh (SampleAnimationError::ToRefreshThousandths)
    void AddError(const int32_t refreshThousandths);

    //! @brief Add a frame that is not judged: a gap
    void AddGap();

    //! @brief Forget the frames
    void Clear();

    void WinResolutionChanged(const UI::ResolutionChangedInfo& info) final;
    void WinPostLayout() final;
    void WinDraw(const UI::UIDrawContext& context) final;

  private:
    void AddEntry(const int16_t value);
  };
}

#endif
