#ifndef SHARED_FRAMEPACING_SAMPLEPACINGTIER_HPP
#define SHARED_FRAMEPACING_SAMPLEPACINGTIER_HPP
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

namespace Fsl
{
  //! How well a frame can be held for more than one refresh: the tiers of Doc/FramePacingPlatformSupport.md ("The configurations, best
  //! first"). At one refresh per frame every tier is the same.
  enum class SamplePacingTier
  {
    //! The presentation engine or the driver holds the frame and counts the refreshes itself
    Tier1,
    //! The sample holds the frame and knows where the refreshes are: it waits on a vsync time of the window system
    Tier2,
    //! The sample holds the frame with a timer and does not know where the refreshes are
    Tier3,
    //! Nothing holds a frame for more than one refresh: the frame pacer is off. It is what a run uses, never the best of a system.
    Tier4
  };

  //! What a tier comes from
  enum class SamplePacingTierReason
  {
    //! The frame pacer is off
    PacerOff,
    //! The present carries a target time (VK_EXT_present_timing with a relative target time)
    TimedPresent,
    //! The present has a swap interval (eglSwapInterval)
    SwapInterval,
    //! A wait on the vsync time of the window system (INativeWindow::TryGetVSyncInfo)
    VSyncTime,
    //! A timer
    Timer
  };

  //! What is certain about explicit sync on Wayland (linux-drm-syncobj-v1). It is the driver that uses it, and a app can not ask if
  //! it does: what a app can see is if the compositor offers it.
  enum class SampleExplicitSync
  {
    //! The window system is not Wayland
    NotApplicable,
    //! The compositor does not offer it, so it is not in use
    NotOffered,
    //! The compositor offers it. If the driver uses it is not known.
    Offered
  };

  //! The tier a run is in and the best tier it could be in
  struct SamplePacingTierInfo
  {
    SamplePacingTier InUse{SamplePacingTier::Tier4};
    SamplePacingTierReason InUseReason{SamplePacingTierReason::PacerOff};
    //! The tier the hold method 'auto' gives on this system
    SamplePacingTier Best{SamplePacingTier::Tier3};
    SamplePacingTierReason BestReason{SamplePacingTierReason::Timer};
    SampleExplicitSync ExplicitSync{SampleExplicitSync::NotApplicable};

    constexpr bool operator==(const SamplePacingTierInfo& other) const noexcept = default;
  };
}

#endif
