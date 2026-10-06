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
#include <cstdlib>

using namespace Fsl;

namespace
{
  constexpr uint16_t DensityDpi = 160;
  constexpr uint16_t DoubleDensityDpi = 320;
  constexpr PxSize2D ViewPx = PxSize2D::Create(100, 100);
  //! Content of twice the view: the scroll offset goes from -100 to 0 on both axes
  constexpr PxSize2D ContentPx = PxSize2D::Create(200, 200);
  //! Content of three times the view: the scroll offset goes from -200 to 0 on both axes
  constexpr PxSize2D BigContentPx = PxSize2D::Create(300, 300);
  //! Content a flick does not reach the ends of
  constexpr PxSize2D LongContentPx = PxSize2D::Create(100100, 100100);
  constexpr int32_t MiddleOfLongContentPx = -50000;

  constexpr TimeSpan BounceTime = TimeSpan::FromMilliseconds(500);
  constexpr TimeSpan LongerThanAnyAnimation = TimeSpan::FromSeconds(10);
  constexpr PxPoint2 PressPx = PxPoint2::Create(50, 50);

  constexpr UI::ScrollGestureAnimationConfig Config(const TimeSpan bounceTime = BounceTime, const float flickTimeMultiplier = 1.0f)
  {
    return {DpValueF(1000.0f), TransitionType::EaseOutSine, flickTimeMultiplier, Vector2(10.0f, 10.0f), bounceTime, TransitionType::EaseOutSine};
  }

  class TestControlScrollGestureHandlerAnimation : public TestFixtureFslBase
  {
  protected:
    UI::ScrollGestureHandler m_handler{DensityDpi};
    int32_t m_timeMs{1000};

    //! The first arrange, as for a scroll viewer that had its first update and layout
    void FirstArrange(const UI::ScrollModeFlags scrollMode, const PxSize2D contentPx, const UI::ScrollGestureAnimationConfig& config = Config())
    {
      m_handler.SetScrollMode(scrollMode);
      m_handler.UpdateAnimation(TimeSpan(0), config);
      m_handler.Arrange(ViewPx, contentPx);
    }

    //! Scroll like the wheel does and arrange
    PxPoint2 ScrollBy(const int32_t deltaPx, const PxSize2D contentPx)
    {
      m_handler.AddScrollDelta(PxValueF(static_cast<float>(deltaPx)));
      return m_handler.Arrange(ViewPx, contentPx);
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

    //! A press, five fast moves and a release: a drag of five steps that ends with the velocity of a flick
    void Flick(const PxPoint2 stepPx)
    {
      PxPoint2 positionPx = PressPx;
      Press(positionPx);
      for (int32_t i = 0; i < 5; ++i)
      {
        positionPx += stepPx;
        MoveTo(positionPx);
      }
      Release(positionPx);
    }

    //! A press, a move and a release after the finger was held still: a drag that ends without a flick
    void DragAndHold(const PxPoint2 distancePx)
    {
      Press(PressPx);
      MoveTo(PressPx + distancePx, 200);
      MoveTo(PressPx + distancePx, 500);
      Release(PressPx + distancePx, 500);
    }

    //! What a update and a layout do: the gestures are read and the content is placed
    PxPoint2 UpdateAndArrange(const PxSize2D contentPx, const TimeSpan time = TimeSpan(0), const UI::ScrollGestureAnimationConfig& config = Config())
    {
      m_handler.UpdateAnimation(time, config);
      return m_handler.Arrange(ViewPx, contentPx);
    }

    //! Runs a flick to its end, and the bounce that can follow it
    PxPoint2 RunToRest(const PxSize2D contentPx)
    {
      UpdateAndArrange(contentPx, LongerThanAnyAnimation);
      return UpdateAndArrange(contentPx, LongerThanAnyAnimation);
    }

    //! Content that became smaller while the view was scrolled past its new end, which starts a bounce
    PxPoint2 StartBounce(const UI::ScrollGestureAnimationConfig& config = Config())
    {
      FirstArrange(UI::ScrollModeFlags::TranslateY, BigContentPx, config);
      EXPECT_EQ(PxPoint2::Create(0, -150), ScrollBy(-150, BigContentPx));
      EXPECT_FALSE(m_handler.UpdateAnimationState(false));
      return m_handler.Arrange(ViewPx, ContentPx);
    }
  };
}


TEST_F(TestControlScrollGestureHandlerAnimation, ContentShrinksPastTheOffset_BouncesBackToTheNewEnd)
{
  StartBounce();
  EXPECT_TRUE(m_handler.UpdateAnimationState(false));

  const PxPoint2 locationPx = UpdateAndArrange(ContentPx, BounceTime + TimeSpan::FromMilliseconds(100));

  EXPECT_EQ(PxPoint2::Create(0, -100), locationPx);
  EXPECT_FALSE(m_handler.UpdateAnimationState(false));
}


TEST_F(TestControlScrollGestureHandlerAnimation, WhileBouncing_TheContentIsPastTheEnd)
{
  const PxPoint2 firstPx = StartBounce();
  const PxPoint2 secondPx = m_handler.Arrange(ViewPx, ContentPx);

  // 50 pixels past the end, which the spring holds back to a tenth
  EXPECT_EQ(PxPoint2::Create(0, -105), firstPx);
  // The arrange that begins the bounce and the one after it show the content at the same place
  EXPECT_EQ(firstPx, secondPx);
}


TEST_F(TestControlScrollGestureHandlerAnimation, Arrange_DoesNotEndABounce)
{
  StartBounce();

  m_handler.Arrange(ViewPx, ContentPx);

  EXPECT_TRUE(m_handler.UpdateAnimationState(false));
}


TEST_F(TestControlScrollGestureHandlerAnimation, BounceTimeOfZero_TheContentIsAtTheEndAtOnce)
{
  const PxPoint2 locationPx = StartBounce(Config(TimeSpan(0)));

  EXPECT_EQ(PxPoint2::Create(0, -100), locationPx);
  EXPECT_FALSE(m_handler.UpdateAnimationState(false));
  // And it stays there
  EXPECT_EQ(PxPoint2::Create(0, -100), UpdateAndArrange(ContentPx, TimeSpan::FromMilliseconds(16), Config(TimeSpan(0))));
  EXPECT_FALSE(m_handler.UpdateAnimationState(false));
}


TEST_F(TestControlScrollGestureHandlerAnimation, ConfigurationChanged_DoesNotEndABounce)
{
  StartBounce();

  m_handler.ConfigurationChanged(DoubleDensityDpi);

  EXPECT_TRUE(m_handler.UpdateAnimationState(false));
}


TEST_F(TestControlScrollGestureHandlerAnimation, ConfigurationChangedDuringABounce_StillSettlesAtTheEnd)
{
  StartBounce();
  UpdateAndArrange(ContentPx, TimeSpan::FromMilliseconds(100));

  m_handler.ConfigurationChanged(DoubleDensityDpi);
  const PxPoint2 locationPx = UpdateAndArrange(ContentPx, BounceTime + TimeSpan::FromMilliseconds(100));

  EXPECT_EQ(PxPoint2::Create(0, -100), locationPx);
  EXPECT_FALSE(m_handler.UpdateAnimationState(false));
}


TEST_F(TestControlScrollGestureHandlerAnimation, ForceComplete_SettlesABounce)
{
  StartBounce();

  EXPECT_TRUE(m_handler.TryForceCompleteAnimation());

  EXPECT_FALSE(m_handler.UpdateAnimationState(false));
  EXPECT_EQ(PxPoint2::Create(0, -100), m_handler.Arrange(ViewPx, ContentPx));
  EXPECT_FALSE(m_handler.UpdateAnimationState(false));
}


TEST_F(TestControlScrollGestureHandlerAnimation, ForceComplete_NothingRunning_ReportsNoChange)
{
  FirstArrange(UI::ScrollModeFlags::TranslateY, ContentPx);

  EXPECT_FALSE(m_handler.TryForceCompleteAnimation());
}


TEST_F(TestControlScrollGestureHandlerAnimation, UpdateAnimationState_Forced_SettlesABounce)
{
  StartBounce();

  EXPECT_FALSE(m_handler.UpdateAnimationState(true));

  EXPECT_EQ(PxPoint2::Create(0, -100), m_handler.Arrange(ViewPx, ContentPx));
}


TEST_F(TestControlScrollGestureHandlerAnimation, UpdateAnimationState_NotForced_LeavesTheBounceRunning)
{
  StartBounce();

  EXPECT_TRUE(m_handler.UpdateAnimationState(false));
  EXPECT_TRUE(m_handler.UpdateAnimationState(false));
}


TEST_F(TestControlScrollGestureHandlerAnimation, MoveOfThreeDp_IsNotADrag)
{
  FirstArrange(UI::ScrollModeFlags::TranslateY, BigContentPx);
  ScrollBy(-100, BigContentPx);

  Press(PressPx);
  MoveTo(PressPx + PxPoint2::Create(0, -3));

  EXPECT_EQ(PxPoint2::Create(0, -100), UpdateAndArrange(BigContentPx));
}


TEST_F(TestControlScrollGestureHandlerAnimation, MoveOfFiveDp_IsADrag)
{
  FirstArrange(UI::ScrollModeFlags::TranslateY, BigContentPx);
  ScrollBy(-100, BigContentPx);

  Press(PressPx);
  MoveTo(PressPx + PxPoint2::Create(0, -5));

  EXPECT_EQ(PxPoint2::Create(0, -105), UpdateAndArrange(BigContentPx));
}


TEST_F(TestControlScrollGestureHandlerAnimation, ConfigurationChanged_TheDragThresholdFollowsTheDensity)
{
  FirstArrange(UI::ScrollModeFlags::TranslateY, BigContentPx);
  ScrollBy(-100, BigContentPx);
  m_handler.ConfigurationChanged(DoubleDensityDpi);

  // Six pixels are three dp now, which is a tap that wobbles
  Press(PressPx);
  MoveTo(PressPx + PxPoint2::Create(0, -6));
  EXPECT_EQ(PxPoint2::Create(0, -100), UpdateAndArrange(BigContentPx));

  // Ten pixels are five dp
  MoveTo(PressPx + PxPoint2::Create(0, -10));
  EXPECT_EQ(PxPoint2::Create(0, -110), UpdateAndArrange(BigContentPx));
}


TEST_F(TestControlScrollGestureHandlerAnimation, Flick_TravelsTheSameDistanceBothWays)
{
  FirstArrange(UI::ScrollModeFlags::TranslateY, LongContentPx);
  ScrollBy(MiddleOfLongContentPx, LongContentPx);

  Flick(PxPoint2::Create(0, -10));
  UpdateAndArrange(LongContentPx);
  const PxPoint2 afterFlickTowardsEndPx = RunToRest(LongContentPx);

  Flick(PxPoint2::Create(0, 10));
  UpdateAndArrange(LongContentPx);
  const PxPoint2 afterFlickTowardsStartPx = RunToRest(LongContentPx);

  // The first flick went past the 50 pixels of its drag
  const int32_t travelPx = MiddleOfLongContentPx - afterFlickTowardsEndPx.Y.Value;
  EXPECT_GT(travelPx, 100);
  // And the second one took the content back to where it was
  EXPECT_EQ(PxPoint2::Create(0, MiddleOfLongContentPx), afterFlickTowardsStartPx);
}


TEST_F(TestControlScrollGestureHandlerAnimation, VerticalFlick_BothAxes_DoesNotDriftSideways)
{
  FirstArrange(UI::ScrollModeFlags::Translate, LongContentPx);
  ScrollBy(MiddleOfLongContentPx, LongContentPx);
  // To the middle sideways too, so a drift is not hidden by the start of the content
  DragAndHold(PxPoint2::Create(-90, 0));
  ASSERT_EQ(PxPoint2::Create(-90, MiddleOfLongContentPx), RunToRest(LongContentPx));

  Flick(PxPoint2::Create(0, -10));
  UpdateAndArrange(LongContentPx);
  const PxPoint2 restPx = RunToRest(LongContentPx);

  EXPECT_EQ(-90, restPx.X.Value);
  EXPECT_LT(restPx.Y.Value, MiddleOfLongContentPx - 100);
}


TEST_F(TestControlScrollGestureHandlerAnimation, FlickWhileTheDensityChanges_KeepsItsDistanceInDp)
{
  // How far the flick goes at the density it began at
  FirstArrange(UI::ScrollModeFlags::TranslateY, LongContentPx);
  ScrollBy(MiddleOfLongContentPx, LongContentPx);
  Flick(PxPoint2::Create(0, -10));
  const PxPoint2 flickStartPx = UpdateAndArrange(LongContentPx);
  const int32_t travelPx = flickStartPx.Y.Value - RunToRest(LongContentPx).Y.Value;
  ASSERT_GT(travelPx, 50);

  // The same flick again, and the density doubles right after it began
  Flick(PxPoint2::Create(0, -10));
  const PxPoint2 secondFlickStartPx = UpdateAndArrange(LongContentPx);
  m_handler.ConfigurationChanged(DoubleDensityDpi);
  const int32_t travelAtDoubleDensityPx = secondFlickStartPx.Y.Value - RunToRest(LongContentPx).Y.Value;

  // The same distance in dp is twice the pixels
  EXPECT_LE(std::abs(travelAtDoubleDensityPx - (travelPx * 2)), 1);
}


TEST_F(TestControlScrollGestureHandlerAnimation, ForceComplete_FlickPastTheStart_StopsAtTheStart)
{
  FirstArrange(UI::ScrollModeFlags::TranslateY, LongContentPx);
  ScrollBy(-20, LongContentPx);
  // Towards the start of the content, and far past it
  Flick(PxPoint2::Create(0, 10));
  UpdateAndArrange(LongContentPx);
  ASSERT_TRUE(m_handler.UpdateAnimationState(false));

  EXPECT_TRUE(m_handler.TryForceCompleteAnimation());

  // At the start, and without a bounce back to it
  EXPECT_EQ(PxPoint2::Create(0, 0), m_handler.Arrange(ViewPx, LongContentPx));
  EXPECT_FALSE(m_handler.UpdateAnimationState(false));
}


TEST_F(TestControlScrollGestureHandlerAnimation, FlickWithoutAnimationTime_IsAtRestAtOnce)
{
  const UI::ScrollGestureAnimationConfig config = Config(BounceTime, 0.0f);
  FirstArrange(UI::ScrollModeFlags::TranslateY, LongContentPx, config);
  ScrollBy(MiddleOfLongContentPx, LongContentPx);

  Flick(PxPoint2::Create(0, -10));
  const PxPoint2 locationPx = UpdateAndArrange(LongContentPx, TimeSpan(0), config);

  // Past the 50 pixels of the drag, and nothing is left to animate
  EXPECT_LT(locationPx.Y.Value, MiddleOfLongContentPx - 100);
  EXPECT_FALSE(m_handler.UpdateAnimationState(false));
  EXPECT_EQ(locationPx, UpdateAndArrange(LongContentPx, TimeSpan::FromMilliseconds(16), config));
}
