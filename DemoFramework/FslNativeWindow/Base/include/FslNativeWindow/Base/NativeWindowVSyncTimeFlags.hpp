#ifndef FSLNATIVEWINDOW_BASE_NATIVEWINDOWVSYNCTIMEFLAGS_HPP
#define FSLNATIVEWINDOW_BASE_NATIVEWINDOWVSYNCTIMEFLAGS_HPP
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
  //! What a window system says about the time of a vertical blank it reports (NativeWindowVSyncInfo::VSyncTime): how the time was
  //! obtained, which says how good it is. A window system that says nothing about its times sets none of them, so a flag that is
  //! not set means "not said" and not "no".
  //! Today only Wayland says something: the kind flags of wp_presentation_feedback.presented, which have the same meaning.
  enum class NativeWindowVSyncTimeFlags : uint32_t
  {
    //! The window system says nothing about the time
    NoFlags = 0x00,
    //! The frame the time is of was shown in sync with the refresh of the display, so it was shown whole (no tearing).
    //! Wayland: WP_PRESENTATION_FEEDBACK_KIND_VSYNC.
    InSyncWithDisplay = 0x01,
    //! The time was taken by the display hardware. A time without this flag is one the window system took itself, and it can be off
    //! the vertical blank.
    //! Wayland: WP_PRESENTATION_FEEDBACK_KIND_HW_CLOCK.
    HardwareClock = 0x02,
    //! The display hardware signalled that the frame was shown. Without this flag the window system worked out when that was, for
    //! example with a timer.
    //! Wayland: WP_PRESENTATION_FEEDBACK_KIND_HW_COMPLETION.
    HardwareCompletion = 0x04,
    //! The content of the frame was shown without a copy of it being made (the display read the buffer of the app).
    //! Wayland: WP_PRESENTATION_FEEDBACK_KIND_ZERO_COPY.
    ZeroCopy = 0x08
  };

  inline constexpr NativeWindowVSyncTimeFlags operator|(const NativeWindowVSyncTimeFlags lhs, const NativeWindowVSyncTimeFlags rhs) noexcept
  {
    return static_cast<NativeWindowVSyncTimeFlags>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
  }

  inline constexpr NativeWindowVSyncTimeFlags operator&(const NativeWindowVSyncTimeFlags lhs, const NativeWindowVSyncTimeFlags rhs) noexcept
  {
    return static_cast<NativeWindowVSyncTimeFlags>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
  }

  namespace NativeWindowVSyncTimeFlagsUtil
  {
    //! @return true if every one of the given flags is set
    inline constexpr bool IsFlagged(const NativeWindowVSyncTimeFlags srcFlags, const NativeWindowVSyncTimeFlags flags) noexcept
    {
      return (srcFlags & flags) == flags;
    }

    //! @return the flags as a number, as they are written to a log: 1 InSyncWithDisplay, 2 HardwareClock, 4 HardwareCompletion,
    //!         8 ZeroCopy
    inline constexpr uint32_t ToLogCode(const NativeWindowVSyncTimeFlags flags) noexcept
    {
      return static_cast<uint32_t>(flags);
    }
  }
}

#endif
