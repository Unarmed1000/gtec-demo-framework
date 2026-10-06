#ifndef SHARED_FRAMEPACING_SAMPLEPACINGTIERCLASSIFIER_HPP
#define SHARED_FRAMEPACING_SAMPLEPACINGTIERCLASSIFIER_HPP
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

#include <Shared/FramePacing/SamplePacerHold.hpp>
#include <Shared/FramePacing/SamplePacingTier.hpp>
#include <cstdint>
#include <string_view>

namespace Fsl
{
  struct NativeWindowTimingSupport;

  //! What a sample knows about how its frames are held, the input of the classifier
  struct SamplePacingTierFacts
  {
    //! True if the present has a swap interval and holds the frame itself (eglSwapInterval). False if a present holds a frame for one
    //! refresh only (a Vulkan FIFO present), so the sample has to hold it.
    bool PresentHasSwapInterval{false};
    //! The longest swap interval the present can hold a frame for, zero if that is not known. With a longest one of one the swap
    //! interval can not hold a frame for more than one refresh at all.
    uint32_t PresentSwapIntervalMax{0};
    //! True if the frame pacer is on
    bool PacerOn{false};
    //! True if the sample held the last frame itself although the present has a swap interval (the swap interval the EGL config allows
    //! is too small for the frame)
    bool SampleHeldFrame{false};
    //! How the sample holds a frame, as it was resolved (SamplePacerHold::Auto is resolved here as the sample resolves it)
    SamplePacerHold HoldMethod{SamplePacerHold::Wait};
    //! True if a present can carry a target time
    bool PresentSchedulingSupported{false};
    //! True if the window system says when the display refreshes and the display does not refresh at a variable rate
    bool HasVSyncTime{false};
    SampleExplicitSync ExplicitSync{SampleExplicitSync::NotApplicable};
  };

  namespace SamplePacingTierClassifier
  {
    //! The name of the global a Wayland compositor that offers explicit sync has
    constexpr std::string_view ExplicitSyncGlobalName = "wp_linux_drm_syncobj_manager_v1";

    //! @brief The tier a run is in and the best tier it could be in. A pure function of the facts.
    [[nodiscard]] SamplePacingTierInfo Classify(const SamplePacingTierFacts& facts) noexcept;

    //! @brief What is certain about explicit sync from what the window system says it offers: the global is listed as available or
    //!        as not available on Wayland, and not at all on a window system that is not Wayland.
    [[nodiscard]] SampleExplicitSync ToExplicitSync(const NativeWindowTimingSupport& support) noexcept;

    //! The number of the last tier (nothing holds a frame)
    constexpr int32_t TierCount = 4;

    //! @brief The number of a tier: 1 is the best, TierCount is the one where nothing holds a frame
    [[nodiscard]] int32_t ToNumber(const SamplePacingTier tier) noexcept;

    //! @brief A reason as it is shown on screen
    [[nodiscard]] std::string_view ToDisplayString(const SamplePacingTierReason reason) noexcept;

    //! @brief A reason as it is written to a log (one word)
    [[nodiscard]] std::string_view ToLogString(const SamplePacingTierReason reason) noexcept;

    //! @brief What is shown on screen about explicit sync (empty when it does not apply)
    [[nodiscard]] std::string_view ToDisplayString(const SampleExplicitSync value) noexcept;
  }
}

#endif
