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

#include <FslBase/Time/TimeSpan.hpp>
#include <FslBase/UnitTest/Helper/Common.hpp>
#include <FslBase/UnitTest/Helper/TestFixtureFslBase.hpp>
#include <FslNativeWindow/Base/VirtualKey.hpp>
#include <Shared/FramePacing/SampleMenuKeyRepeat.hpp>
#include <cstdint>

using namespace Fsl;

namespace
{
  using TestSampleMenuKeyRepeat = TestFixtureFslBase;

  constexpr SampleMenuKeyRepeatMode Steady = SampleMenuKeyRepeatMode::Steady;
  constexpr SampleMenuKeyRepeatMode Accelerating = SampleMenuKeyRepeatMode::Accelerating;
  //! A frame of a display of 100 Hz: a time the rates of the repeat divide without a rest
  constexpr TimeSpan Frame = TimeSpan::FromMilliseconds(10);

  //! Frames go by while the key is held, returns how often it repeated in them
  uint32_t Hold(SampleMenuKeyRepeat& rKeyRepeat, const TimeSpan time, const TimeSpan frame = Frame)
  {
    uint32_t repeats = 0;
    for (TimeSpan elapsed; elapsed < time; elapsed += frame)
    {
      repeats += rKeyRepeat.Update(frame);
    }
    return repeats;
  }
}


TEST(TestSampleMenuKeyRepeat, NoKey_NoRepeats)
{
  SampleMenuKeyRepeat keyRepeat;

  EXPECT_EQ(VirtualKey::Undefined, keyRepeat.GetKey());
  EXPECT_EQ(0u, Hold(keyRepeat, TimeSpan::FromSeconds(5)));
}


TEST(TestSampleMenuKeyRepeat, Press_IsTheKeyThatIsHeld)
{
  SampleMenuKeyRepeat keyRepeat;

  keyRepeat.Press(VirtualKey::DownArrow, Steady);

  EXPECT_EQ(VirtualKey::DownArrow, keyRepeat.GetKey());
}


TEST(TestSampleMenuKeyRepeat, NoRepeatsDuringTheDelay)
{
  SampleMenuKeyRepeat keyRepeat;
  keyRepeat.Press(VirtualKey::DownArrow, Steady);

  EXPECT_EQ(0u, Hold(keyRepeat, SampleMenuKeyRepeat::Delay));
  // The delay is over, the first repeat comes a twelfth of a second later
  EXPECT_EQ(0u, Hold(keyRepeat, TimeSpan::FromMilliseconds(80)));
  EXPECT_EQ(1u, Hold(keyRepeat, TimeSpan::FromMilliseconds(10)));
}


TEST(TestSampleMenuKeyRepeat, Steady_RepeatsAtOneRate)
{
  SampleMenuKeyRepeat keyRepeat;
  keyRepeat.Press(VirtualKey::DownArrow, Steady);
  Hold(keyRepeat, SampleMenuKeyRepeat::Delay);

  EXPECT_EQ(static_cast<uint32_t>(SampleMenuKeyRepeat::SteadyRepeatsPerSecond), Hold(keyRepeat, TimeSpan::FromSeconds(1)));
  // And it stays that rate
  EXPECT_EQ(static_cast<uint32_t>(SampleMenuKeyRepeat::SteadyRepeatsPerSecond) * 5u, Hold(keyRepeat, TimeSpan::FromSeconds(5)));
}


TEST(TestSampleMenuKeyRepeat, Accelerating_RepeatsFasterTheLongerTheKeyIsHeld)
{
  SampleMenuKeyRepeat keyRepeat;
  keyRepeat.Press(VirtualKey::RightArrow, Accelerating);
  Hold(keyRepeat, SampleMenuKeyRepeat::Delay);

  // From 0.4 s to 1.5 s at the slow rate, 20 a second. The frame that reaches 1.5 s is at the medium rate already: 21.8 + 1.
  const uint32_t slow = Hold(keyRepeat, SampleMenuKeyRepeat::MediumFrom - SampleMenuKeyRepeat::Delay);
  EXPECT_EQ(22u, slow);
  // From 1.5 s to 3 s at the medium rate, 100 a second, and the frame that reaches 3 s is at the fast rate: 0.8 left over + 149 + 4
  const uint32_t medium = Hold(keyRepeat, SampleMenuKeyRepeat::FastFrom - SampleMenuKeyRepeat::MediumFrom);
  EXPECT_EQ(153u, medium);
  // And from there at the fast rate
  EXPECT_EQ(static_cast<uint32_t>(SampleMenuKeyRepeat::FastRepeatsPerSecond), Hold(keyRepeat, TimeSpan::FromSeconds(1)));
}


TEST(TestSampleMenuKeyRepeat, Accelerating_ASliderOfAThousandStepsIsCrossedInAFewSeconds)
{
  SampleMenuKeyRepeat keyRepeat;
  keyRepeat.Press(VirtualKey::RightArrow, Accelerating);

  EXPECT_GE(Hold(keyRepeat, TimeSpan::FromSeconds(6)), 1024u);
}


TEST(TestSampleMenuKeyRepeat, TheRepeatsDoNotDependOnTheFrameTime)
{
  SampleMenuKeyRepeat keyRepeat;
  keyRepeat.Press(VirtualKey::DownArrow, Steady);
  const uint32_t shortFrames = Hold(keyRepeat, TimeSpan::FromSeconds(3), TimeSpan::FromMilliseconds(4));

  keyRepeat.Press(VirtualKey::DownArrow, Steady);
  const uint32_t longFrames = Hold(keyRepeat, TimeSpan::FromSeconds(3), TimeSpan::FromMilliseconds(40));

  // 2.6 seconds at 12 repeats per second
  EXPECT_EQ(31u, shortFrames);
  EXPECT_EQ(31u, longFrames);
}


TEST(TestSampleMenuKeyRepeat, AFrameThatTookLong_RepeatsAsAFrameOfTheLongestTime)
{
  SampleMenuKeyRepeat keyRepeat;
  keyRepeat.Press(VirtualKey::RightArrow, Accelerating);
  Hold(keyRepeat, TimeSpan::FromSeconds(4));

  // A hitch of two seconds at 400 repeats per second
  EXPECT_EQ(40u, keyRepeat.Update(TimeSpan::FromSeconds(2)));
}


TEST(TestSampleMenuKeyRepeat, Release_EndsTheRepeats)
{
  SampleMenuKeyRepeat keyRepeat;
  keyRepeat.Press(VirtualKey::DownArrow, Steady);
  Hold(keyRepeat, TimeSpan::FromSeconds(1));

  keyRepeat.Release(VirtualKey::DownArrow);

  EXPECT_EQ(VirtualKey::Undefined, keyRepeat.GetKey());
  EXPECT_EQ(0u, Hold(keyRepeat, TimeSpan::FromSeconds(1)));
}


TEST(TestSampleMenuKeyRepeat, ReleaseOfAnotherKey_TheHeldKeyGoesOn)
{
  SampleMenuKeyRepeat keyRepeat;
  keyRepeat.Press(VirtualKey::DownArrow, Steady);
  Hold(keyRepeat, SampleMenuKeyRepeat::Delay);

  keyRepeat.Release(VirtualKey::UpArrow);

  EXPECT_EQ(VirtualKey::DownArrow, keyRepeat.GetKey());
  EXPECT_EQ(static_cast<uint32_t>(SampleMenuKeyRepeat::SteadyRepeatsPerSecond), Hold(keyRepeat, TimeSpan::FromSeconds(1)));
}


TEST(TestSampleMenuKeyRepeat, PressOfAnotherKey_ItIsTheHeldKeyAndItsDelayBegins)
{
  SampleMenuKeyRepeat keyRepeat;
  keyRepeat.Press(VirtualKey::DownArrow, Steady);
  Hold(keyRepeat, TimeSpan::FromSeconds(1));

  keyRepeat.Press(VirtualKey::UpArrow, Steady);

  EXPECT_EQ(VirtualKey::UpArrow, keyRepeat.GetKey());
  EXPECT_EQ(0u, Hold(keyRepeat, SampleMenuKeyRepeat::Delay));
  // The key that went down first comes up: the last one is still held
  keyRepeat.Release(VirtualKey::DownArrow);
  EXPECT_EQ(static_cast<uint32_t>(SampleMenuKeyRepeat::SteadyRepeatsPerSecond), Hold(keyRepeat, TimeSpan::FromSeconds(1)));
}


TEST(TestSampleMenuKeyRepeat, PressAgain_TheDelayBeginsAgain)
{
  SampleMenuKeyRepeat keyRepeat;
  keyRepeat.Press(VirtualKey::RightArrow, Accelerating);
  Hold(keyRepeat, TimeSpan::FromSeconds(4));
  keyRepeat.Release(VirtualKey::RightArrow);

  keyRepeat.Press(VirtualKey::RightArrow, Accelerating);

  EXPECT_EQ(0u, Hold(keyRepeat, SampleMenuKeyRepeat::Delay));
  // And it is slow again
  EXPECT_EQ(2u, Hold(keyRepeat, TimeSpan::FromMilliseconds(100)));
}


TEST(TestSampleMenuKeyRepeat, Clear_NoKeyIsHeld)
{
  SampleMenuKeyRepeat keyRepeat;
  keyRepeat.Press(VirtualKey::DownArrow, Steady);
  Hold(keyRepeat, TimeSpan::FromSeconds(1));

  keyRepeat.Clear();

  EXPECT_EQ(VirtualKey::Undefined, keyRepeat.GetKey());
  EXPECT_EQ(0u, Hold(keyRepeat, TimeSpan::FromSeconds(1)));
}


TEST(TestSampleMenuKeyRepeat, NoTime_NoRepeats)
{
  SampleMenuKeyRepeat keyRepeat;
  keyRepeat.Press(VirtualKey::DownArrow, Steady);
  Hold(keyRepeat, TimeSpan::FromSeconds(1));

  EXPECT_EQ(0u, keyRepeat.Update(TimeSpan()));
  EXPECT_EQ(0u, keyRepeat.Update(TimeSpan::FromMilliseconds(-10)));
}
