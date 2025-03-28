/****************************************************************************************************************************************************
 * Copyright 2024 NXP
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *
 *    * Neither the name of the NXP. nor the names of
 *      its contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************************************************************************************/

#include <FslBase/Exceptions.hpp>
#include <FslBase/UnitTest/Helper/TestFixtureFslBase.hpp>
#include <FslGraphics/Transition/TransitionColorU16.hpp>
#include <array>
#include <limits>
#include <vector>

using namespace Fsl;

namespace
{
  using TestTransition_TransitionColorU16 = TestFixtureFslBase;
}


TEST(TestTransition_TransitionColorU16, Construct_Default)
{
  TransitionColorU16 transitionValue;

  EXPECT_TRUE(transitionValue.IsCompleted());
  EXPECT_EQ(TimeSpan(0), transitionValue.GetStartDelay());
  EXPECT_EQ(ColorU16(), transitionValue.GetValue());
  EXPECT_EQ(ColorU16(), transitionValue.GetActualValue());
  EXPECT_EQ(TimeSpan(0), transitionValue.GetTransitionTime());
}


TEST(TestTransition_TransitionColorU16, Construct_CacheAndTimespan)
{
  TransitionColorU16 transitionValue(TimeSpan(10));

  EXPECT_TRUE(transitionValue.IsCompleted());
  EXPECT_EQ(TimeSpan(0), transitionValue.GetStartDelay());
  EXPECT_EQ(ColorU16(), transitionValue.GetValue());
  EXPECT_EQ(ColorU16(), transitionValue.GetActualValue());
  EXPECT_EQ(TimeSpan(10), transitionValue.GetTransitionTime());
}


TEST(TestTransition_TransitionColorU16, SetTransitionTime)
{
  TransitionColorU16 transitionValue;

  EXPECT_TRUE(transitionValue.IsCompleted());
  EXPECT_EQ(TimeSpan(0), transitionValue.GetStartDelay());
  EXPECT_EQ(ColorU16(), transitionValue.GetValue());
  EXPECT_EQ(ColorU16(), transitionValue.GetActualValue());
  EXPECT_EQ(TimeSpan(0), transitionValue.GetTransitionTime());

  transitionValue.SetTransitionTime(TimeSpan(10));

  EXPECT_TRUE(transitionValue.IsCompleted());
  EXPECT_EQ(TimeSpan(0), transitionValue.GetStartDelay());
  EXPECT_EQ(ColorU16(), transitionValue.GetValue());
  EXPECT_EQ(ColorU16(), transitionValue.GetActualValue());
  EXPECT_EQ(TimeSpan(10), transitionValue.GetTransitionTime());
}

TEST(TestTransition_TransitionColorU16, SetValue)
{
  TransitionColorU16 transitionValue(TimeSpan(2), TransitionType::Linear);

  const ColorU16 value = ColorU16::CreateR8G8B8A8UNorm(42, 20, 255, 0);

  EXPECT_TRUE(transitionValue.IsCompleted());
  transitionValue.SetValue(value);

  EXPECT_FALSE(transitionValue.IsCompleted());
  EXPECT_EQ(ColorU16(), transitionValue.GetValue());
  EXPECT_EQ(value, transitionValue.GetActualValue());

  transitionValue.Update(TimeSpan(1));
  EXPECT_FALSE(transitionValue.IsCompleted());
  EXPECT_EQ(uint16_t(std::round(value.RawR() * 0.5f)), transitionValue.GetValue().RawR());
  EXPECT_EQ(uint16_t(std::round(value.RawG() * 0.5f)), transitionValue.GetValue().RawG());
  EXPECT_EQ(uint16_t(std::round(value.RawB() * 0.5f)), transitionValue.GetValue().RawB());
  EXPECT_EQ(uint16_t(std::round(value.RawA() * 0.5f)), transitionValue.GetValue().RawA());
  EXPECT_EQ(value, transitionValue.GetActualValue());

  transitionValue.Update(TimeSpan(1));
  EXPECT_TRUE(transitionValue.IsCompleted());
  // We do expect to hit the exact provided value (so no float eq here)
  EXPECT_EQ(value, transitionValue.GetValue());
  EXPECT_EQ(value, transitionValue.GetActualValue());

  // Calling update after the end time should change nothing
  transitionValue.Update(TimeSpan(1));
  EXPECT_TRUE(transitionValue.IsCompleted());
  // We do expect to hit the exact provided value (so no float eq here)
  EXPECT_EQ(value, transitionValue.GetValue());
  EXPECT_EQ(value, transitionValue.GetActualValue());
}


TEST(TestTransition_TransitionColorU16, SetActualValue)
{
  TransitionColorU16 transitionValue(TimeSpan(2), TransitionType::Linear);

  const ColorU16 value = ColorU16::CreateR8G8B8A8UNorm(42, 20, 255, 0);
  const ColorU16 overrideValue = ColorU16::CreateR8G8B8A8UNorm(80, 40, 0, 255);

  EXPECT_TRUE(transitionValue.IsCompleted());
  transitionValue.SetValue(value);

  EXPECT_FALSE(transitionValue.IsCompleted());
  EXPECT_EQ(ColorU16(), transitionValue.GetValue());
  EXPECT_EQ(value, transitionValue.GetActualValue());

  transitionValue.Update(TimeSpan(1));
  EXPECT_FALSE(transitionValue.IsCompleted());
  EXPECT_EQ(uint16_t(std::round(value.RawR() * 0.5f)), transitionValue.GetValue().RawR());
  EXPECT_EQ(uint16_t(std::round(value.RawG() * 0.5f)), transitionValue.GetValue().RawG());
  EXPECT_EQ(uint16_t(std::round(value.RawB() * 0.5f)), transitionValue.GetValue().RawB());
  EXPECT_EQ(uint16_t(std::round(value.RawA() * 0.5f)), transitionValue.GetValue().RawA());
  EXPECT_EQ(value, transitionValue.GetActualValue());

  // Override the transition by forcing the actual value
  transitionValue.SetActualValue(overrideValue);
  EXPECT_TRUE(transitionValue.IsCompleted());
  // We do expect to hit the exact provided value (so no float eq here)
  EXPECT_EQ(overrideValue, transitionValue.GetValue());
  EXPECT_EQ(overrideValue, transitionValue.GetActualValue());
}


TEST(TestTransition_TransitionColorU16, SetStartDelay)
{
  TransitionColorU16 transitionValue(TimeSpan(2), TransitionType::Linear);

  const ColorU16 value = ColorU16::CreateR8G8B8A8UNorm(42, 20, 255, 0);

  EXPECT_TRUE(transitionValue.IsCompleted());
  EXPECT_EQ(TimeSpan(0), transitionValue.GetStartDelay());
  transitionValue.SetStartDelay(TimeSpan(3));
  EXPECT_EQ(TimeSpan(3), transitionValue.GetStartDelay());
  EXPECT_TRUE(transitionValue.IsCompleted());
  transitionValue.SetValue(value);


  EXPECT_FALSE(transitionValue.IsCompleted());
  EXPECT_EQ(ColorU16(), transitionValue.GetValue());
  EXPECT_EQ(value, transitionValue.GetActualValue());

  // Start delay
  for (int i = 0; i < 3; ++i)
  {
    transitionValue.Update(TimeSpan(1));
    EXPECT_FALSE(transitionValue.IsCompleted());
    EXPECT_EQ(ColorU16(), transitionValue.GetValue());
    EXPECT_EQ(value, transitionValue.GetActualValue());
  }

  transitionValue.Update(TimeSpan(1));
  EXPECT_FALSE(transitionValue.IsCompleted());
  EXPECT_EQ(uint16_t(std::round(value.RawR() * 0.5f)), transitionValue.GetValue().RawR());
  EXPECT_EQ(uint16_t(std::round(value.RawG() * 0.5f)), transitionValue.GetValue().RawG());
  EXPECT_EQ(uint16_t(std::round(value.RawB() * 0.5f)), transitionValue.GetValue().RawB());
  EXPECT_EQ(uint16_t(std::round(value.RawA() * 0.5f)), transitionValue.GetValue().RawA());
  EXPECT_EQ(value, transitionValue.GetActualValue());

  transitionValue.Update(TimeSpan(1));
  EXPECT_TRUE(transitionValue.IsCompleted());
  // We do expect to hit the exact provided value (so no float eq here)
  EXPECT_EQ(value, transitionValue.GetValue());
  EXPECT_EQ(value, transitionValue.GetActualValue());
}
