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
#include <FslBase/Math/Pixel/PxValue.hpp>
#include <FslBase/UnitTest/Helper/Common.hpp>
#include <FslBase/UnitTest/Helper/TestFixtureFslBase.hpp>
#include <FslSimpleUI/Base/Control/Logic/ScrollMakeVisibleMath.hpp>

using namespace Fsl;

namespace
{
  using TestControlLogicScrollMakeVisibleMath = TestFixtureFslBase;

  //! A view of 600 pixels
  constexpr PxSize1D ViewPx = PxSize1D::Create(600);

  constexpr int32_t Move(const int32_t positionPx, const int32_t sizePx)
  {
    return UI::ScrollMakeVisibleMath::CalcMoveDistancePx(ViewPx, PxValue(positionPx), PxSize1D::Create(sizePx)).Value;
  }

  constexpr int32_t Clamp(const int32_t offsetPx, const int32_t contentPx)
  {
    return UI::ScrollMakeVisibleMath::ClampOffsetPx(PxValue(offsetPx), ViewPx, PxSize1D::Create(contentPx)).Value;
  }
}


TEST_F(TestControlLogicScrollMakeVisibleMath, CalcMoveDistancePx_InsideTheView_NoMove)
{
  EXPECT_EQ(0, Move(200, 100));
  // At the two edges
  EXPECT_EQ(0, Move(0, 100));
  EXPECT_EQ(0, Move(500, 100));
  // As large as the view
  EXPECT_EQ(0, Move(0, 600));
}


TEST_F(TestControlLogicScrollMakeVisibleMath, CalcMoveDistancePx_BeforeTheView_ItsStartGoesToTheStartOfTheView)
{
  EXPECT_EQ(250, Move(-250, 100));
  // Partly inside
  EXPECT_EQ(30, Move(-30, 100));
}


TEST_F(TestControlLogicScrollMakeVisibleMath, CalcMoveDistancePx_AfterTheView_ItsEndGoesToTheEndOfTheView)
{
  // 900 to 1000, the end of the view is at 600
  EXPECT_EQ(-400, Move(900, 100));
  // Partly inside
  EXPECT_EQ(-30, Move(530, 100));
}


TEST_F(TestControlLogicScrollMakeVisibleMath, CalcMoveDistancePx_LargerThanTheView_IsShownFromItsStart)
{
  // Below the start of the view: its start goes to the start of the view, not its end to the end of the view
  EXPECT_EQ(-100, Move(100, 900));
  // Before the start of the view
  EXPECT_EQ(100, Move(-100, 900));
  // At the start of the view already
  EXPECT_EQ(0, Move(0, 900));
}


TEST_F(TestControlLogicScrollMakeVisibleMath, CalcMoveDistancePx_NoSize_ThePlaceIsBroughtIn)
{
  EXPECT_EQ(0, Move(600, 0));
  EXPECT_EQ(-1, Move(601, 0));
  EXPECT_EQ(1, Move(-1, 0));
}


TEST_F(TestControlLogicScrollMakeVisibleMath, ClampOffsetPx_InsideTheContent_IsKept)
{
  // 2000 pixels of content in a view of 600: from -1400 to 0
  EXPECT_EQ(0, Clamp(0, 2000));
  EXPECT_EQ(-700, Clamp(-700, 2000));
  EXPECT_EQ(-1400, Clamp(-1400, 2000));
}


TEST_F(TestControlLogicScrollMakeVisibleMath, ClampOffsetPx_PastAEnd_IsTheEnd)
{
  EXPECT_EQ(0, Clamp(30, 2000));
  EXPECT_EQ(-1400, Clamp(-1500, 2000));
}


TEST_F(TestControlLogicScrollMakeVisibleMath, ClampOffsetPx_ContentThatFits_IsAtItsStart)
{
  EXPECT_EQ(0, Clamp(-10, 600));
  EXPECT_EQ(0, Clamp(-10, 100));
  EXPECT_EQ(0, Clamp(10, 100));
}
