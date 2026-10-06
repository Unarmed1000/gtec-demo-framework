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

#include <FslBase/Math/Pixel/PxPoint2.hpp>
#include <FslBase/Math/Pixel/PxSize2D.hpp>
#include <FslBase/Math/Pixel/PxValueF.hpp>
#include <FslBase/UnitTest/Helper/Common.hpp>
#include <FslBase/UnitTest/Helper/TestFixtureFslBase.hpp>
#include <FslSimpleUI/Base/Control/ScrollGestureHandler.hpp>
#include <FslSimpleUI/Base/Control/ScrollModeFlags.hpp>

using namespace Fsl;

namespace
{
  constexpr uint16_t DensityDpi = 160;
  constexpr PxSize2D ViewPx = PxSize2D::Create(100, 100);
  //! Content that is larger than the view on both axes: 300 pixels to scroll sideways, 400 vertically
  constexpr PxSize2D ContentPx = PxSize2D::Create(400, 500);

  class TestControlScrollGestureHandlerScrollDelta : public TestFixtureFslBase
  {
  protected:
    UI::ScrollGestureHandler m_handler{DensityDpi};

    //! The first arrange, as for a scroll viewer that had its first layout
    void FirstArrange(const UI::ScrollModeFlags scrollMode, const PxSize2D contentPx = ContentPx)
    {
      m_handler.SetScrollMode(scrollMode);
      m_handler.Arrange(ViewPx, contentPx);
    }
  };
}


TEST_F(TestControlScrollGestureHandlerScrollDelta, BeforeTheFirstArrange_NothingIsTaken)
{
  m_handler.SetScrollMode(UI::ScrollModeFlags::TranslateY);

  // What there is to scroll is not known yet
  EXPECT_FALSE(m_handler.AddScrollDelta(PxValueF(-10.0f)));
}


TEST_F(TestControlScrollGestureHandlerScrollDelta, ContentThatFits_NothingIsTaken)
{
  FirstArrange(UI::ScrollModeFlags::TranslateY, ViewPx);

  EXPECT_FALSE(m_handler.AddScrollDelta(PxValueF(-10.0f)));
  EXPECT_EQ(PxPoint2::Create(0, 0), m_handler.Arrange(ViewPx, ViewPx));
}


TEST_F(TestControlScrollGestureHandlerScrollDelta, ADelta_MovesTheContentAtTheNextArrange)
{
  FirstArrange(UI::ScrollModeFlags::TranslateY);

  EXPECT_TRUE(m_handler.AddScrollDelta(PxValueF(-30.0f)));

  EXPECT_EQ(PxPoint2::Create(0, -30), m_handler.Arrange(ViewPx, ContentPx));
  // And it stays there
  EXPECT_EQ(PxPoint2::Create(0, -30), m_handler.Arrange(ViewPx, ContentPx));
}


TEST_F(TestControlScrollGestureHandlerScrollDelta, DeltasBeforeAArrange_AddUp)
{
  FirstArrange(UI::ScrollModeFlags::TranslateY);

  EXPECT_TRUE(m_handler.AddScrollDelta(PxValueF(-30.0f)));
  EXPECT_TRUE(m_handler.AddScrollDelta(PxValueF(-30.0f)));
  EXPECT_TRUE(m_handler.AddScrollDelta(PxValueF(10.0f)));

  EXPECT_EQ(PxPoint2::Create(0, -50), m_handler.Arrange(ViewPx, ContentPx));
}


TEST_F(TestControlScrollGestureHandlerScrollDelta, PartsOfAPixel_AddUpOverTime)
{
  FirstArrange(UI::ScrollModeFlags::TranslateY);

  m_handler.AddScrollDelta(PxValueF(-0.4f));
  EXPECT_EQ(PxPoint2::Create(0, 0), m_handler.Arrange(ViewPx, ContentPx));
  m_handler.AddScrollDelta(PxValueF(-0.4f));
  EXPECT_EQ(PxPoint2::Create(0, 0), m_handler.Arrange(ViewPx, ContentPx));
  m_handler.AddScrollDelta(PxValueF(-0.4f));
  EXPECT_EQ(PxPoint2::Create(0, -1), m_handler.Arrange(ViewPx, ContentPx));
}


TEST_F(TestControlScrollGestureHandlerScrollDelta, StopsAtTheEndOfTheContent)
{
  FirstArrange(UI::ScrollModeFlags::TranslateY);

  m_handler.AddScrollDelta(PxValueF(-10000.0f));

  // 500 pixels of content in a view of 100
  EXPECT_EQ(PxPoint2::Create(0, -400), m_handler.Arrange(ViewPx, ContentPx));
}


TEST_F(TestControlScrollGestureHandlerScrollDelta, StopsAtTheStartOfTheContent)
{
  FirstArrange(UI::ScrollModeFlags::TranslateY);
  m_handler.AddScrollDelta(PxValueF(-50.0f));
  m_handler.Arrange(ViewPx, ContentPx);

  m_handler.AddScrollDelta(PxValueF(10000.0f));

  EXPECT_EQ(PxPoint2::Create(0, 0), m_handler.Arrange(ViewPx, ContentPx));
}


TEST_F(TestControlScrollGestureHandlerScrollDelta, SidewaysOnly_TheDeltaScrollsSideways)
{
  FirstArrange(UI::ScrollModeFlags::TranslateX);

  EXPECT_TRUE(m_handler.AddScrollDelta(PxValueF(-30.0f)));

  EXPECT_EQ(PxPoint2::Create(-30, 0), m_handler.Arrange(ViewPx, ContentPx));
}


TEST_F(TestControlScrollGestureHandlerScrollDelta, BothAxes_TheDeltaScrollsVertically)
{
  FirstArrange(UI::ScrollModeFlags::Translate);

  EXPECT_TRUE(m_handler.AddScrollDelta(PxValueF(-30.0f)));

  EXPECT_EQ(PxPoint2::Create(0, -30), m_handler.Arrange(ViewPx, ContentPx));
}


TEST_F(TestControlScrollGestureHandlerScrollDelta, ContentShrinksToFit_APendingDeltaIsDropped)
{
  FirstArrange(UI::ScrollModeFlags::TranslateY);
  EXPECT_TRUE(m_handler.AddScrollDelta(PxValueF(-30.0f)));

  // The content fits now, so there is nothing to scroll
  EXPECT_EQ(PxPoint2::Create(0, 0), m_handler.Arrange(ViewPx, ViewPx));
  // And the delta is not applied when it grows again
  EXPECT_EQ(PxPoint2::Create(0, 0), m_handler.Arrange(ViewPx, ContentPx));
}
