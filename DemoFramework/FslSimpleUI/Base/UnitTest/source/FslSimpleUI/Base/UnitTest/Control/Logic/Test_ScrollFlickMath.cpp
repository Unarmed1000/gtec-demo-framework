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

#include <FslBase/Math/Dp/DpPoint2F.hpp>
#include <FslBase/Math/Dp/DpValueF.hpp>
#include <FslBase/UnitTest/Helper/Common.hpp>
#include <FslBase/UnitTest/Helper/TestFixtureFslBase.hpp>
#include <FslSimpleUI/Base/Control/Logic/ScrollFlickMath.hpp>

using namespace Fsl;

namespace
{
  using TestControlLogicScrollFlickMath = TestFixtureFslBase;

  constexpr DpValueF Deceleration(1000.0f);

  //! How far a flick travels until it is at rest: v0 * t + 0.5 * a * t * t, with t = speed / deceleration
  DpPoint2F Travel(const DpPoint2F velocityDpf)
  {
    const DpPoint2F accelerationDpf = UI::ScrollFlickMath::CalcDeceleration(velocityDpf, Deceleration);
    const float time = velocityDpf.Length().Value / Deceleration.Value;
    return DpPoint2F::Create((velocityDpf.X.Value * time) + (0.5f * accelerationDpf.X.Value * time * time),
                             (velocityDpf.Y.Value * time) + (0.5f * accelerationDpf.Y.Value * time * time));
  }
}


TEST_F(TestControlLogicScrollFlickMath, NoVelocity_NoAcceleration)
{
  const DpPoint2F res = UI::ScrollFlickMath::CalcDeceleration(DpPoint2F(), Deceleration);

  EXPECT_FLOAT_EQ(0.0f, res.X.Value);
  EXPECT_FLOAT_EQ(0.0f, res.Y.Value);
}


TEST_F(TestControlLogicScrollFlickMath, AlongAAxis_IsAgainstTheVelocity)
{
  const DpPoint2F towardsStart = UI::ScrollFlickMath::CalcDeceleration(DpPoint2F::Create(0.0f, 500.0f), Deceleration);
  const DpPoint2F towardsEnd = UI::ScrollFlickMath::CalcDeceleration(DpPoint2F::Create(0.0f, -500.0f), Deceleration);

  EXPECT_FLOAT_EQ(-1000.0f, towardsStart.Y.Value);
  EXPECT_FLOAT_EQ(1000.0f, towardsEnd.Y.Value);
}


TEST_F(TestControlLogicScrollFlickMath, AlongAAxis_NothingOnTheOtherAxis)
{
  const DpPoint2F vertical = UI::ScrollFlickMath::CalcDeceleration(DpPoint2F::Create(0.0f, -500.0f), Deceleration);
  const DpPoint2F sideways = UI::ScrollFlickMath::CalcDeceleration(DpPoint2F::Create(500.0f, 0.0f), Deceleration);

  EXPECT_FLOAT_EQ(0.0f, vertical.X.Value);
  EXPECT_FLOAT_EQ(0.0f, sideways.Y.Value);
}


TEST_F(TestControlLogicScrollFlickMath, Diagonal_HasTheSizeOfTheDeceleration)
{
  const DpPoint2F res = UI::ScrollFlickMath::CalcDeceleration(DpPoint2F::Create(300.0f, -400.0f), Deceleration);

  // Against a velocity of the size 500 with the direction (0.6, -0.8)
  EXPECT_FLOAT_EQ(-600.0f, res.X.Value);
  EXPECT_FLOAT_EQ(800.0f, res.Y.Value);
  EXPECT_FLOAT_EQ(1000.0f, res.Length().Value);
}


TEST_F(TestControlLogicScrollFlickMath, NoDeceleration_NoAcceleration)
{
  const DpPoint2F res = UI::ScrollFlickMath::CalcDeceleration(DpPoint2F::Create(300.0f, -400.0f), DpValueF(0.0f));

  EXPECT_FLOAT_EQ(0.0f, res.X.Value);
  EXPECT_FLOAT_EQ(0.0f, res.Y.Value);
}


TEST_F(TestControlLogicScrollFlickMath, FlicksOfTheSameSpeed_TravelTheSameDistanceBothWays)
{
  const DpPoint2F towardsStart = Travel(DpPoint2F::Create(0.0f, 500.0f));
  const DpPoint2F towardsEnd = Travel(DpPoint2F::Create(0.0f, -500.0f));

  // The square of the speed divided by twice the deceleration
  EXPECT_FLOAT_EQ(125.0f, towardsStart.Y.Value);
  EXPECT_FLOAT_EQ(-125.0f, towardsEnd.Y.Value);
}


TEST_F(TestControlLogicScrollFlickMath, DiagonalFlick_TravelsAlongItsVelocity)
{
  const DpPoint2F res = Travel(DpPoint2F::Create(300.0f, -400.0f));

  // 125 along the direction (0.6, -0.8)
  EXPECT_FLOAT_EQ(75.0f, res.X.Value);
  EXPECT_FLOAT_EQ(-100.0f, res.Y.Value);
}
