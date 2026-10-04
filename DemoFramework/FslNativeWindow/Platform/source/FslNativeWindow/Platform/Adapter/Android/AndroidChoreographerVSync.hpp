#ifdef __ANDROID__
#ifndef FSLNATIVEWINDOW_PLATFORM_ADAPTER_ANDROID_ANDROIDCHOREOGRAPHERVSYNC_HPP
#define FSLNATIVEWINDOW_PLATFORM_ADAPTER_ANDROID_ANDROIDCHOREOGRAPHERVSYNC_HPP
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

#include <FslNativeWindow/Base/NativeWindowVSyncInfo.hpp>
#include <cstdint>
#include <memory>
#include <string>

namespace Fsl
{
  //! When the display of a Android app refreshes, from the choreographer.
  //!
  //! The choreographer is what Android paces its apps with. A app posts a vsync callback (AChoreographer_postVsyncCallback, API level
  //! 33) and is called when it is time to render the next frame, with the frame timelines the frame can be made for: for each the
  //! time the frame is expected to be presented and the deadline it has to be ready by. The expected presentation time of the
  //! timeline the platform prefers is the time of a vertical blank of the display, and that is what is reported. The refresh period
  //! is the vsync period of the refresh rate callback of the choreographer (API level 30), which the platform calls once after it
  //! was registered and again whenever the refresh rate changes.
  //!
  //! A callback is for one frame, so one is posted per frame (RequestVSyncTime). The callbacks are run by the looper of the thread
  //! the object was made on, which has to be the thread that polls the events of the app (the thread of android_main).
  //!
  //! The times are CLOCK_MONOTONIC, the clock of the HighResolutionTimer.
  //!
  //! The functions are looked up in libandroid when the object is made, so the app builds and starts for any minimum API level. On
  //! a device below API level 33 the source reports that it is not available.
  //!
  //! Not used: the deadline of a timeline, which says how late a frame can be handed over and still be shown at its time. The
  //! interface of the framework has no place for it yet.
  class AndroidChoreographerVSync
  {
    struct State;
    //! Shared with the vsync callbacks that are posted: a callback can not be taken back, so one that runs after this object is gone
    //! finds the state marked as dead
    std::shared_ptr<State> m_state;
    bool m_isAvailable{false};
    std::string m_unavailableReason;

  public:
    AndroidChoreographerVSync(const AndroidChoreographerVSync&) = delete;
    AndroidChoreographerVSync& operator=(const AndroidChoreographerVSync&) = delete;

    //! @note Has to be made on the thread whose looper the app polls
    AndroidChoreographerVSync();
    ~AndroidChoreographerVSync();

    //! @brief True if the device has the vsync callback of the choreographer
    [[nodiscard]] bool IsAvailable() const noexcept
    {
      return m_isAvailable;
    }

    //! @brief Why the source is not available (empty if it is)
    [[nodiscard]] const std::string& GetUnavailableReason() const noexcept
    {
      return m_unavailableReason;
    }

    //! @brief The API level of the device
    [[nodiscard]] static uint32_t GetDeviceApiLevel() noexcept;

    //! @brief Called once per frame: posts a vsync callback unless one is waiting to be run
    void RequestVSyncTime();

    //! @brief The time of a vertical blank from the last vsync callback and the vsync period (invalid until both have been reported)
    [[nodiscard]] NativeWindowVSyncInfo GetVSyncInfo() const noexcept;
  };
}

#endif
#endif
