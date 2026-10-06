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

#include <FslBase/UnitTest/Helper/Common.hpp>
#include <FslBase/UnitTest/Helper/TestFixtureFslBase.hpp>
#include <FslNativeWindow/Base/NativeWindowTimingSupport.hpp>
#include <Shared/FramePacing/SamplePacerHold.hpp>
#include <Shared/FramePacing/SamplePacingTier.hpp>
#include <Shared/FramePacing/SamplePacingTierClassifier.hpp>
#include <string>

using namespace Fsl;

namespace
{
  using TestSamplePacingTierClassifier = TestFixtureFslBase;

  //! A sample whose present holds a frame for one refresh only (Vulkan FIFO), with the frame pacer on
  SamplePacingTierFacts VulkanFacts(const SamplePacerHold holdMethod, const bool presentScheduling, const bool hasVSyncTime)
  {
    SamplePacingTierFacts facts;
    facts.PresentHasSwapInterval = false;
    facts.PacerOn = true;
    facts.HoldMethod = holdMethod;
    facts.PresentSchedulingSupported = presentScheduling;
    facts.HasVSyncTime = hasVSyncTime;
    return facts;
  }

  //! A sample whose present has a swap interval (eglSwapInterval), with the frame pacer on
  SamplePacingTierFacts EglFacts(const bool sampleHeldFrame, const SamplePacerHold holdMethod, const bool hasVSyncTime)
  {
    SamplePacingTierFacts facts;
    facts.PresentHasSwapInterval = true;
    facts.PacerOn = true;
    facts.SampleHeldFrame = sampleHeldFrame;
    facts.HoldMethod = holdMethod;
    facts.HasVSyncTime = hasVSyncTime;
    return facts;
  }
}


// The rows of the table in Doc/FramePacingPlatformSupport.md ("The configurations, best first")

TEST(TestSamplePacingTierClassifier, Vulkan_TimedPresent_IsTier1)
{
  const auto info = SamplePacingTierClassifier::Classify(VulkanFacts(SamplePacerHold::Schedule, true, true));

  EXPECT_EQ(SamplePacingTier::Tier1, info.InUse);
  EXPECT_EQ(SamplePacingTierReason::TimedPresent, info.InUseReason);
  EXPECT_EQ(SamplePacingTier::Tier1, info.Best);
  EXPECT_EQ(SamplePacingTierReason::TimedPresent, info.BestReason);
}


TEST(TestSamplePacingTierClassifier, OpenGLES_SwapInterval_IsTier1)
{
  const auto info = SamplePacingTierClassifier::Classify(EglFacts(false, SamplePacerHold::Wait, false));

  EXPECT_EQ(SamplePacingTier::Tier1, info.InUse);
  EXPECT_EQ(SamplePacingTierReason::SwapInterval, info.InUseReason);
  EXPECT_EQ(SamplePacingTier::Tier1, info.Best);
  EXPECT_EQ(SamplePacingTierReason::SwapInterval, info.BestReason);
}


TEST(TestSamplePacingTierClassifier, Vulkan_VSyncWait_IsTier2)
{
  const auto info = SamplePacingTierClassifier::Classify(VulkanFacts(SamplePacerHold::VSync, false, true));

  EXPECT_EQ(SamplePacingTier::Tier2, info.InUse);
  EXPECT_EQ(SamplePacingTierReason::VSyncTime, info.InUseReason);
  EXPECT_EQ(SamplePacingTier::Tier2, info.Best);
  EXPECT_EQ(SamplePacingTierReason::VSyncTime, info.BestReason);
}


TEST(TestSamplePacingTierClassifier, Vulkan_Timer_NoVSyncTime_IsTier3)
{
  const auto info = SamplePacingTierClassifier::Classify(VulkanFacts(SamplePacerHold::Wait, false, false));

  EXPECT_EQ(SamplePacingTier::Tier3, info.InUse);
  EXPECT_EQ(SamplePacingTierReason::Timer, info.InUseReason);
  EXPECT_EQ(SamplePacingTier::Tier3, info.Best);
  EXPECT_EQ(SamplePacingTierReason::Timer, info.BestReason);
}


// What a run is in and what it could be in are two things

TEST(TestSamplePacingTierClassifier, Vulkan_TimerForced_TimedPresentThere_InUse3Best1)
{
  // The default hold of the sample is the timer
  const auto info = SamplePacingTierClassifier::Classify(VulkanFacts(SamplePacerHold::Wait, true, true));

  EXPECT_EQ(SamplePacingTier::Tier3, info.InUse);
  EXPECT_EQ(SamplePacingTierReason::Timer, info.InUseReason);
  EXPECT_EQ(SamplePacingTier::Tier1, info.Best);
  EXPECT_EQ(SamplePacingTierReason::TimedPresent, info.BestReason);
}


TEST(TestSamplePacingTierClassifier, Vulkan_TimerForced_VSyncTimeThere_InUse3Best2)
{
  const auto info = SamplePacingTierClassifier::Classify(VulkanFacts(SamplePacerHold::Wait, false, true));

  EXPECT_EQ(SamplePacingTier::Tier3, info.InUse);
  EXPECT_EQ(SamplePacingTier::Tier2, info.Best);
  EXPECT_EQ(SamplePacingTierReason::VSyncTime, info.BestReason);
}


TEST(TestSamplePacingTierClassifier, Vulkan_VSyncForced_TimedPresentThere_InUse2Best1)
{
  const auto info = SamplePacingTierClassifier::Classify(VulkanFacts(SamplePacerHold::VSync, true, true));

  EXPECT_EQ(SamplePacingTier::Tier2, info.InUse);
  EXPECT_EQ(SamplePacingTier::Tier1, info.Best);
}


TEST(TestSamplePacingTierClassifier, Vulkan_VariableRefreshSeen_TheVSyncTimeDoesNotCount)
{
  // The sample resolves the hold to the timer when the display refreshes at a variable rate, and says the system has no vsync time
  const auto info = SamplePacingTierClassifier::Classify(VulkanFacts(SamplePacerHold::Wait, false, false));

  EXPECT_EQ(SamplePacingTier::Tier3, info.InUse);
  EXPECT_EQ(SamplePacingTier::Tier3, info.Best);
}


TEST(TestSamplePacingTierClassifier, Vulkan_AutoThatWasNotResolved_IsResolvedAsTheSampleDoes)
{
  EXPECT_EQ(SamplePacingTier::Tier1, SamplePacingTierClassifier::Classify(VulkanFacts(SamplePacerHold::Auto, true, true)).InUse);
  EXPECT_EQ(SamplePacingTier::Tier2, SamplePacingTierClassifier::Classify(VulkanFacts(SamplePacerHold::Auto, false, true)).InUse);
  EXPECT_EQ(SamplePacingTier::Tier3, SamplePacingTierClassifier::Classify(VulkanFacts(SamplePacerHold::Auto, false, false)).InUse);
}


// The pacer is off

TEST(TestSamplePacingTierClassifier, PacerOff_IsTier4_TheBestIsStillSaid)
{
  SamplePacingTierFacts facts = VulkanFacts(SamplePacerHold::Wait, true, true);
  facts.PacerOn = false;

  const auto info = SamplePacingTierClassifier::Classify(facts);

  EXPECT_EQ(SamplePacingTier::Tier4, info.InUse);
  EXPECT_EQ(SamplePacingTierReason::PacerOff, info.InUseReason);
  EXPECT_EQ(SamplePacingTier::Tier1, info.Best);
}


TEST(TestSamplePacingTierClassifier, OpenGLES_PacerOff_IsTier4)
{
  SamplePacingTierFacts facts = EglFacts(false, SamplePacerHold::Wait, false);
  facts.PacerOn = false;

  const auto info = SamplePacingTierClassifier::Classify(facts);

  EXPECT_EQ(SamplePacingTier::Tier4, info.InUse);
  EXPECT_EQ(SamplePacingTier::Tier1, info.Best);
  EXPECT_EQ(SamplePacingTierReason::SwapInterval, info.BestReason);
}


// OpenGL ES when the swap interval the EGL config allows is too small: the sample holds the frame itself

TEST(TestSamplePacingTierClassifier, OpenGLES_SampleHoldsTheFrame_Timer_IsTier3)
{
  const auto info = SamplePacingTierClassifier::Classify(EglFacts(true, SamplePacerHold::Wait, false));

  EXPECT_EQ(SamplePacingTier::Tier3, info.InUse);
  EXPECT_EQ(SamplePacingTierReason::Timer, info.InUseReason);
  // The swap interval is still what this system has for a frame it can hold (how long that is, is not known here)
  EXPECT_EQ(SamplePacingTier::Tier1, info.Best);
}


TEST(TestSamplePacingTierClassifier, OpenGLES_SwapIntervalOfOneAtMost_TheBestIsWhatTheSampleCanDo)
{
  // The EGL config allows a swap interval of one only: it can not hold a frame for more than one refresh, so it is not a tier 1
  SamplePacingTierFacts facts = EglFacts(true, SamplePacerHold::Wait, true);
  facts.PresentSwapIntervalMax = 1;

  const auto info = SamplePacingTierClassifier::Classify(facts);

  EXPECT_EQ(SamplePacingTier::Tier3, info.InUse);
  EXPECT_EQ(SamplePacingTier::Tier2, info.Best);
  EXPECT_EQ(SamplePacingTierReason::VSyncTime, info.BestReason);
}


TEST(TestSamplePacingTierClassifier, OpenGLES_SwapIntervalOfOneAtMost_NoVSyncTime_TheBestIsTheTimer)
{
  SamplePacingTierFacts facts = EglFacts(true, SamplePacerHold::Wait, false);
  facts.PresentSwapIntervalMax = 1;

  const auto info = SamplePacingTierClassifier::Classify(facts);

  EXPECT_EQ(SamplePacingTier::Tier3, info.Best);
  EXPECT_EQ(SamplePacingTierReason::Timer, info.BestReason);
}


TEST(TestSamplePacingTierClassifier, OpenGLES_SwapIntervalOfFourAtMost_IsTier1)
{
  SamplePacingTierFacts facts = EglFacts(false, SamplePacerHold::Wait, true);
  facts.PresentSwapIntervalMax = 4;

  const auto info = SamplePacingTierClassifier::Classify(facts);

  EXPECT_EQ(SamplePacingTier::Tier1, info.InUse);
  EXPECT_EQ(SamplePacingTier::Tier1, info.Best);
  EXPECT_EQ(SamplePacingTierReason::SwapInterval, info.BestReason);
}


TEST(TestSamplePacingTierClassifier, OpenGLES_SampleHoldsTheFrame_VSync_IsTier2)
{
  const auto info = SamplePacingTierClassifier::Classify(EglFacts(true, SamplePacerHold::VSync, true));

  EXPECT_EQ(SamplePacingTier::Tier2, info.InUse);
  EXPECT_EQ(SamplePacingTierReason::VSyncTime, info.InUseReason);
}


// Explicit sync: only what is certain

TEST(TestSamplePacingTierClassifier, ExplicitSync_NotWayland_DoesNotApply)
{
  NativeWindowTimingSupport support;
  support.WindowSystem = "Win32";
  support.Available = {"DwmFlush"};

  EXPECT_EQ(SampleExplicitSync::NotApplicable, SamplePacingTierClassifier::ToExplicitSync(support));
  EXPECT_TRUE(SamplePacingTierClassifier::ToDisplayString(SampleExplicitSync::NotApplicable).empty());
}


TEST(TestSamplePacingTierClassifier, ExplicitSync_GlobalNotThere_NotOffered)
{
  NativeWindowTimingSupport support;
  support.WindowSystem = "Wayland";
  support.Available = {"wp_presentation"};
  support.NotAvailable = {"wp_fifo_manager_v1", "wp_linux_drm_syncobj_manager_v1"};

  EXPECT_EQ(SampleExplicitSync::NotOffered, SamplePacingTierClassifier::ToExplicitSync(support));
  EXPECT_EQ("not offered (not in use)", SamplePacingTierClassifier::ToDisplayString(SampleExplicitSync::NotOffered));
}


TEST(TestSamplePacingTierClassifier, ExplicitSync_GlobalThere_Offered)
{
  NativeWindowTimingSupport support;
  support.WindowSystem = "Wayland";
  support.Available = {"wp_presentation", "wp_linux_drm_syncobj_manager_v1"};

  EXPECT_EQ(SampleExplicitSync::Offered, SamplePacingTierClassifier::ToExplicitSync(support));
  // It does not say that it is in use: that can not be known
  EXPECT_EQ("offered by the compositor", SamplePacingTierClassifier::ToDisplayString(SampleExplicitSync::Offered));
}


TEST(TestSamplePacingTierClassifier, ExplicitSync_IsPassedOn_AndDoesNotChangeATier)
{
  SamplePacingTierFacts facts = VulkanFacts(SamplePacerHold::VSync, false, true);
  const auto without = SamplePacingTierClassifier::Classify(facts);
  facts.ExplicitSync = SampleExplicitSync::Offered;
  const auto with = SamplePacingTierClassifier::Classify(facts);

  EXPECT_EQ(SampleExplicitSync::Offered, with.ExplicitSync);
  EXPECT_EQ(without.InUse, with.InUse);
  EXPECT_EQ(without.Best, with.Best);
}


// The texts

TEST(TestSamplePacingTierClassifier, ToNumber)
{
  EXPECT_EQ(1, SamplePacingTierClassifier::ToNumber(SamplePacingTier::Tier1));
  EXPECT_EQ(2, SamplePacingTierClassifier::ToNumber(SamplePacingTier::Tier2));
  EXPECT_EQ(3, SamplePacingTierClassifier::ToNumber(SamplePacingTier::Tier3));
  // Nothing holds a frame: the last tier
  EXPECT_EQ(4, SamplePacingTierClassifier::ToNumber(SamplePacingTier::Tier4));
  EXPECT_EQ(4, SamplePacingTierClassifier::TierCount);
}


TEST(TestSamplePacingTierClassifier, ReasonTexts)
{
  EXPECT_EQ("pacer off", SamplePacingTierClassifier::ToDisplayString(SamplePacingTierReason::PacerOff));
  EXPECT_EQ("timed present", SamplePacingTierClassifier::ToDisplayString(SamplePacingTierReason::TimedPresent));
  EXPECT_EQ("swap interval", SamplePacingTierClassifier::ToDisplayString(SamplePacingTierReason::SwapInterval));
  EXPECT_EQ("vsync time", SamplePacingTierClassifier::ToDisplayString(SamplePacingTierReason::VSyncTime));
  EXPECT_EQ("timer", SamplePacingTierClassifier::ToDisplayString(SamplePacingTierReason::Timer));

  EXPECT_EQ("pacerOff", SamplePacingTierClassifier::ToLogString(SamplePacingTierReason::PacerOff));
  EXPECT_EQ("timedPresent", SamplePacingTierClassifier::ToLogString(SamplePacingTierReason::TimedPresent));
  EXPECT_EQ("swapInterval", SamplePacingTierClassifier::ToLogString(SamplePacingTierReason::SwapInterval));
  EXPECT_EQ("vsyncTime", SamplePacingTierClassifier::ToLogString(SamplePacingTierReason::VSyncTime));
  EXPECT_EQ("timer", SamplePacingTierClassifier::ToLogString(SamplePacingTierReason::Timer));
}
