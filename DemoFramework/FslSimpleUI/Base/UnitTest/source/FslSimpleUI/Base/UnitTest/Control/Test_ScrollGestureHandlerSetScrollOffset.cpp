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
#include <FslBase/Math/Vector2.hpp>
#include <FslBase/Time/MillisecondTickCount32.hpp>
#include <FslBase/Time/TimeSpan.hpp>
#include <FslBase/Transition/TransitionType.hpp>
#include <FslBase/UnitTest/Helper/Common.hpp>
#include <FslBase/UnitTest/Helper/TestFixtureFslBase.hpp>
#include <FslSimpleUI/Base/Control/ScrollGestureAnimationConfig.hpp>
#include <FslSimpleUI/Base/Control/ScrollGestureHandler.hpp>
#include <FslSimpleUI/Base/Control/ScrollModeFlags.hpp>
#include <FslSimpleUI/Base/Event/EventTransactionState.hpp>
#include <FslSimpleUI/Base/MovementOwnership.hpp>

using namespace Fsl;

namespace
{
  constexpr uint16_t DensityDpi = 160;
  constexpr PxSize2D ViewPx = PxSize2D::Create(100, 100);
  //! Content that is larger than the view on both axes: 300 pixels to scroll sideways, 400 vertically
  constexpr PxSize2D ContentPx = PxSize2D::Create(400, 500);
  //! Content a flick does not reach the ends of
  constexpr PxSize2D LongContentPx = PxSize2D::Create(100100, 100100);
  constexpr int32_t MiddleOfLongContentPx = -50000;
  constexpr PxPoint2 PressPx = PxPoint2::Create(50, 50);

  constexpr UI::ScrollGestureAnimationConfig Config()
  {
    return {DpValueF(1000.0f),     TransitionType::EaseOutSine,     1.0f,
            Vector2(10.0f, 10.0f), TimeSpan::FromMilliseconds(500), TransitionType::EaseOutSine};
  }

  class TestControlScrollGestureHandlerSetScrollOffset : public TestFixtureFslBase
  {
  protected:
    UI::ScrollGestureHandler m_handler{DensityDpi};
    int32_t m_timeMs{1000};

    //! The first arrange, as for a scroll viewer that had its first update and layout
    void FirstArrange(const UI::ScrollModeFlags scrollMode, const PxSize2D contentPx = ContentPx)
    {
      m_handler.SetScrollMode(scrollMode);
      m_handler.UpdateAnimation(TimeSpan(0), Config());
      m_handler.Arrange(ViewPx, contentPx);
    }

    void Press(const PxPoint2 positionPx)
    {
      m_handler.AddMovement(MillisecondTickCount32(m_timeMs), positionPx, UI::EventTransactionState::Begin, false, UI::MovementOwnership::Unhandled);
    }

    void MoveTo(const PxPoint2 positionPx, const int32_t afterMs = 10)
    {
      m_timeMs += afterMs;
      m_handler.AddMovement(MillisecondTickCount32(m_timeMs), positionPx, UI::EventTransactionState::Begin, true, UI::MovementOwnership::Unhandled);
    }

    void Release(const PxPoint2 positionPx, const int32_t afterMs = 10)
    {
      m_timeMs += afterMs;
      m_handler.AddMovement(MillisecondTickCount32(m_timeMs), positionPx, UI::EventTransactionState::End, false, UI::MovementOwnership::Unhandled);
    }

    //! What a update and a layout do: the gestures are read and the content is placed
    PxPoint2 UpdateAndArrange(const PxSize2D contentPx, const TimeSpan time = TimeSpan(0))
    {
      m_handler.UpdateAnimation(time, Config());
      return m_handler.Arrange(ViewPx, contentPx);
    }
  };
}


TEST_F(TestControlScrollGestureHandlerSetScrollOffset, TheContentIsAtTheOffset)
{
  FirstArrange(UI::ScrollModeFlags::TranslateY);

  m_handler.SetScrollOffset(PxPoint2::Create(0, -120));

  EXPECT_EQ(PxPoint2::Create(0, -120), m_handler.Arrange(ViewPx, ContentPx));
  // And it stays there, with nothing left to animate
  EXPECT_EQ(PxPoint2::Create(0, -120), UpdateAndArrange(ContentPx, TimeSpan::FromMilliseconds(100)));
  EXPECT_FALSE(m_handler.UpdateAnimationState(false));
}


TEST_F(TestControlScrollGestureHandlerSetScrollOffset, BothAxes)
{
  FirstArrange(UI::ScrollModeFlags::Translate);

  m_handler.SetScrollOffset(PxPoint2::Create(-70, -120));

  EXPECT_EQ(PxPoint2::Create(-70, -120), m_handler.Arrange(ViewPx, ContentPx));
}


TEST_F(TestControlScrollGestureHandlerSetScrollOffset, AAxisThatDoesNotScroll_StaysAtItsStart)
{
  FirstArrange(UI::ScrollModeFlags::TranslateY);

  m_handler.SetScrollOffset(PxPoint2::Create(-70, -120));

  EXPECT_EQ(PxPoint2::Create(0, -120), m_handler.Arrange(ViewPx, ContentPx));
}


TEST_F(TestControlScrollGestureHandlerSetScrollOffset, AWheelDistanceThatWaits_IsDropped)
{
  FirstArrange(UI::ScrollModeFlags::TranslateY);
  ASSERT_TRUE(m_handler.AddScrollDelta(PxValueF(-30.0f)));

  m_handler.SetScrollOffset(PxPoint2::Create(0, -120));

  EXPECT_EQ(PxPoint2::Create(0, -120), m_handler.Arrange(ViewPx, ContentPx));
}


TEST_F(TestControlScrollGestureHandlerSetScrollOffset, AWheelDistanceAfterIt_IsAdded)
{
  FirstArrange(UI::ScrollModeFlags::TranslateY);

  m_handler.SetScrollOffset(PxPoint2::Create(0, -120));
  ASSERT_TRUE(m_handler.AddScrollDelta(PxValueF(-30.0f)));

  EXPECT_EQ(PxPoint2::Create(0, -150), m_handler.Arrange(ViewPx, ContentPx));
}


TEST_F(TestControlScrollGestureHandlerSetScrollOffset, DuringAFlick_TheFlickEnds)
{
  FirstArrange(UI::ScrollModeFlags::TranslateY, LongContentPx);
  m_handler.SetScrollOffset(PxPoint2::Create(0, MiddleOfLongContentPx));
  ASSERT_EQ(PxPoint2::Create(0, MiddleOfLongContentPx), m_handler.Arrange(ViewPx, LongContentPx));
  // A press, five fast moves and a release: a drag that ends with the velocity of a flick
  PxPoint2 positionPx = PressPx;
  Press(positionPx);
  for (int32_t i = 0; i < 5; ++i)
  {
    positionPx += PxPoint2::Create(0, -20);
    MoveTo(positionPx);
  }
  Release(positionPx);
  UpdateAndArrange(LongContentPx);
  const PxPoint2 duringTheFlickPx = UpdateAndArrange(LongContentPx, TimeSpan::FromMilliseconds(16));
  ASSERT_TRUE(m_handler.UpdateAnimationState(false));
  ASSERT_LT(duringTheFlickPx.Y.Value, MiddleOfLongContentPx - 100);

  m_handler.SetScrollOffset(PxPoint2::Create(0, -120));

  EXPECT_EQ(PxPoint2::Create(0, -120), m_handler.Arrange(ViewPx, LongContentPx));
  EXPECT_EQ(PxPoint2::Create(0, -120), UpdateAndArrange(LongContentPx, TimeSpan::FromMilliseconds(100)));
  EXPECT_FALSE(m_handler.UpdateAnimationState(false));
}


TEST_F(TestControlScrollGestureHandlerSetScrollOffset, DuringADrag_TheDragNoLongerMovesTheContent)
{
  FirstArrange(UI::ScrollModeFlags::TranslateY);
  Press(PressPx);
  MoveTo(PressPx + PxPoint2::Create(0, -40), 200);
  ASSERT_EQ(PxPoint2::Create(0, -40), UpdateAndArrange(ContentPx));

  m_handler.SetScrollOffset(PxPoint2::Create(0, -120));

  EXPECT_EQ(PxPoint2::Create(0, -120), m_handler.Arrange(ViewPx, ContentPx));
  // The finger that is still down moves on and is let go
  MoveTo(PressPx + PxPoint2::Create(0, -60), 200);
  EXPECT_EQ(PxPoint2::Create(0, -120), UpdateAndArrange(ContentPx));
  Release(PressPx + PxPoint2::Create(0, -60), 500);
  EXPECT_EQ(PxPoint2::Create(0, -120), UpdateAndArrange(ContentPx));
}
