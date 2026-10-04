#ifndef FSLNATIVEWINDOW_BASE_NATIVEWINDOWVSYNCINFO_HPP
#define FSLNATIVEWINDOW_BASE_NATIVEWINDOWVSYNCINFO_HPP
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

#include <FslBase/Time/TickCount.hpp>
#include <FslBase/Time/TimeSpan.hpp>

namespace Fsl
{
  //! When the display the window is on refreshes, as the window system reports it. A graphics API that only has a present that waits for
  //! the next refresh does not tell a app when the refreshes happen, the window system of the platform often can.
  //!
  //! It is a hint: what the time of a vertical blank means and how exact it is differs between the platforms, and a platform can report it
  //! for another display than the one the window is on. Both members are zero when the platform does not report them.
  struct NativeWindowVSyncInfo
  {
    //! The time of a recent vertical blank of the display as a HighResolutionTimer timestamp (zero if unknown)
    TickCount VSyncTime;
    //! The time between two refreshes as the window system measured it (TimeSpan() if unknown). The time of any other vertical blank
    //! is VSyncTime plus or minus a whole number of these.
    TimeSpan RefreshPeriod;

    constexpr NativeWindowVSyncInfo() noexcept = default;

    constexpr NativeWindowVSyncInfo(const TickCount vsyncTime, const TimeSpan refreshPeriod) noexcept
      : VSyncTime(vsyncTime)
      , RefreshPeriod(refreshPeriod)
    {
    }

    //! @return true if both the time of a vertical blank and the refresh period are known
    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
      return VSyncTime.Ticks() > 0 && RefreshPeriod.Ticks() > 0;
    }
  };
}

#endif
