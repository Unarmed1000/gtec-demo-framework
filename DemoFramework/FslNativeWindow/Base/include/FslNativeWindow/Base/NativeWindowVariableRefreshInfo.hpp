#ifndef FSLNATIVEWINDOW_BASE_NATIVEWINDOWVARIABLEREFRESHINFO_HPP
#define FSLNATIVEWINDOW_BASE_NATIVEWINDOWVARIABLEREFRESHINFO_HPP
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

#include <FslBase/BasicTypes.hpp>

namespace Fsl
{
  //! The answer to a question about variable refresh: a platform can often not tell
  enum class NativeWindowVariableRefreshAnswer
  {
    Unknown,
    No,
    Yes
  };

  //! What is known about variable refresh (G-SYNC, FreeSync, Adaptive-Sync, HDMI VRR) on the display the window is on.
  //!
  //! No platform has one call for it. "The display can do it", "it is switched on" and "the display is doing it now" are three
  //! questions, a platform answers some of them or none, and what a platform says is kept apart from what was measured.
  //! Doc/FramePacingPlatformSupport.md lists what each platform can tell.
  struct NativeWindowVariableRefreshInfo
  {
    //! The window system says: the display and the graphics device can do variable refresh
    NativeWindowVariableRefreshAnswer Supported{NativeWindowVariableRefreshAnswer::Unknown};
    //! The window system says: it is switched on in the settings
    NativeWindowVariableRefreshAnswer Enabled{NativeWindowVariableRefreshAnswer::Unknown};
    //! The window system says: the display refreshes at a variable rate now
    NativeWindowVariableRefreshAnswer Active{NativeWindowVariableRefreshAnswer::Unknown};
    //! Measured from when the display refreshes. Yes: the display was seen to refresh off the rate of its mode. No: it refreshes at
    //! the rate of its mode, which a display with variable refresh on also does while the frames come at that rate. So a No is no
    //! proof that variable refresh is off.
    NativeWindowVariableRefreshAnswer Observed{NativeWindowVariableRefreshAnswer::Unknown};

    //! The measurement behind Observed. The median time between two refreshes of the display in thousandths of the refresh period
    //! of its mode (1000: the display refreshes at the rate of its mode, 4000: four periods between two refreshes). Zero: not measured.
    uint32_t ObservedIntervalMilliPeriods{0};
    //! The share of the measured refreshes that did not come one refresh period after the one before, in thousandths
    uint32_t ObservedOffPeriodPerMille{0};
    //! How many refreshes the two numbers above were measured from (zero: nothing was measured)
    uint32_t ObservedIntervalCount{0};

    //! What gave Supported, Enabled and Active (a text that lives as long as the app, never null; empty: nothing did)
    const char* Source{""};
    //! What Observed and its numbers were measured from (a text that lives as long as the app, never null; empty: nothing was measured)
    const char* ObservedSource{""};

    //! @return true if any of the answers is known or anything was measured
    [[nodiscard]] constexpr bool HasInfo() const noexcept
    {
      return Supported != NativeWindowVariableRefreshAnswer::Unknown || Enabled != NativeWindowVariableRefreshAnswer::Unknown ||
             Active != NativeWindowVariableRefreshAnswer::Unknown || Observed != NativeWindowVariableRefreshAnswer::Unknown ||
             ObservedIntervalCount != 0u;
    }

    //! @return true if the window system says the display refreshes at a variable rate now, or it was measured to
    [[nodiscard]] constexpr bool IsSeen() const noexcept
    {
      return Active == NativeWindowVariableRefreshAnswer::Yes || Observed == NativeWindowVariableRefreshAnswer::Yes;
    }
  };

  namespace NativeWindowVariableRefreshRule
  {
    //! The refreshes a measurement has to be made from before it gives an answer
    constexpr uint32_t MinIntervalCount = 60;
    //! The display follows the frames and not its mode when the median time between two refreshes is at least this, in thousandths
    //! of the refresh period of the mode (a display can not refresh faster than its mode, so a shorter time is never variable refresh)
    constexpr uint32_t MinVariableIntervalMilliPeriods = 1100;
    //! ... and more than this share of the refreshes did not come one refresh period after the one before, in thousandths
    constexpr uint32_t MinVariableOffPeriodPerMille = 500;

    //! @brief The rule that turns a measurement of when the display refreshes into the answer NativeWindowVariableRefreshInfo::Observed.
    //! @note  Where the numbers are from (a 240 Hz mode, see Doc/FramePacingPlatformSupport.md): at a fixed refresh rate the median was
    //!        994 to 1009 and the share off the period 234 at the most, idle and under CPU load, at 30 to 240 frames per second. With
    //!        variable refresh active the median was 1982 to 2007 at 120 frames per second and 4000 at 60, with every refresh off the
    //!        period. A display with variable refresh active that gets its frames at the rate of its mode measures like a fixed one.
    [[nodiscard]] constexpr NativeWindowVariableRefreshAnswer ToObserved(const uint32_t intervalMilliPeriods, const uint32_t offPeriodPerMille,
                                                                         const uint32_t intervalCount) noexcept
    {
      if (intervalCount < MinIntervalCount || intervalMilliPeriods == 0u)
      {
        return NativeWindowVariableRefreshAnswer::Unknown;
      }
      return (intervalMilliPeriods >= MinVariableIntervalMilliPeriods && offPeriodPerMille > MinVariableOffPeriodPerMille)
               ? NativeWindowVariableRefreshAnswer::Yes
               : NativeWindowVariableRefreshAnswer::No;
    }
  }
}

#endif
