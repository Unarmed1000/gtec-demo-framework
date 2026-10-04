#if !defined(__ANDROID__) && (defined(__linux__) || defined(FSL_PLATFORM_APPLE)) && defined(FSL_WINDOWSYSTEM_X11)
#ifndef FSLNATIVEWINDOW_PLATFORM_ADAPTER_X11_X11PRESENTVSYNC_HPP
#define FSLNATIVEWINDOW_PLATFORM_ADAPTER_X11_X11PRESENTVSYNC_HPP
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

#include <FslBase/System/HighResolutionTimer.hpp>
#include <FslBase/Time/TimeSpan.hpp>
#include <FslNativeWindow/Base/NativeWindowVSyncInfo.hpp>
#include <FslNativeWindow/Platform/PlatformNativeWindowSystemTypes.hpp>
#include <cstdint>
#include <string>

namespace Fsl
{
  //! When the display of a X11 window refreshes, from the Present extension.
  //!
  //! Present is the extension of the X server that knows the vertical blanks: it counts them for the output a window is on (the media
  //! stream counter, MSC) and has the time of each (the unadjusted system time, UST, in microseconds). For every image a graphics API
  //! presents to the window the X server sends a PresentCompleteNotify event with the counter and the time the image was shown, to
  //! everyone that selected the events of the window. So listening is enough: a app that presents gets the time of a vertical blank
  //! of the output its window is on with every frame, and nothing is asked of the X server.
  //!
  //! A vertical blank can be asked for as well (PresentNotifyMSC), and that is only done when the presents of the app bring no events
  //! (a driver that does not present through this extension): the events of the presents already hold the time, so a request per
  //! frame would be traffic for nothing.
  //!
  //! Under Xwayland the counter and the times are made up by Xwayland from what the Wayland compositor tells it, and when the window
  //! is not shown (a locked or blanked desktop) the counter ticks once per second and a FIFO present waits for it.
  //!
  //! The times are on the clock of the X server, which is CLOCK_MONOTONIC on Linux and so the clock of the HighResolutionTimer. That
  //! is checked: a time that is far from the time of the framework is not used.
  //!
  //! It needs libXpresent, which the package of the window system links when it has it (FSL_WINDOWSYSTEM_X11_PRESENT). Without it
  //! this class reports that the source is not available.
  class X11PresentVSync
  {
    HighResolutionTimer m_timer;
    Display* m_pDisplay{nullptr};
    Window m_window{0};
    bool m_isAvailable{false};
    std::string m_unavailableReason;
    int m_majorOpcode{0};
    uint32_t m_eventId{0};
    uint32_t m_requestSerial{0};
    bool m_isRequestPending{false};
    //! The calls of RequestVBlankTime since a present of the graphics API brought a event
    uint32_t m_framesWithoutPresentEvent{0};
    bool m_hasTime{false};
    bool m_isClockUsable{true};
    uint64_t m_lastUst{0};
    uint64_t m_lastMsc{0};
    //! The vertical blank the refresh period is measured from
    uint64_t m_referenceUst{0};
    uint64_t m_referenceMsc{0};
    //! The refresh period measured from two vertical blanks (zero: not measured yet)
    TimeSpan m_measuredPeriod;
    uint64_t m_eventCount{0};
    //! The last answer to "are the times on the refresh of the output", so a change is logged once
    mutable bool m_wasOnRefresh{true};

  public:
    X11PresentVSync(const X11PresentVSync&) = delete;
    X11PresentVSync& operator=(const X11PresentVSync&) = delete;

    X11PresentVSync(Display* const pDisplay, const Window window);
    ~X11PresentVSync();

    //! @brief True if the X server has the extension and the events of the window could be selected
    [[nodiscard]] bool IsAvailable() const noexcept
    {
      return m_isAvailable;
    }

    //! @brief Why the source is not available (empty if it is)
    [[nodiscard]] const std::string& GetUnavailableReason() const noexcept
    {
      return m_unavailableReason;
    }

    //! @brief Called once per frame. Asks the X server for the vertical blank of the window if the presents of the graphics API
    //!        have not brought a event for a while, and does nothing otherwise.
    void RequestVBlankTime();

    //! @brief Take the event if it is one of the Present extension.
    //! @return true if the event was a Present event (it is consumed), false if it is not one and the caller handles it
    [[nodiscard]] bool TryHandleEvent(XEvent& rEvent);

    //! @brief The time of the last vertical blank the X server reported and the refresh period.
    //! @param fallbackRefreshPeriod the refresh period of the mode of the output. It is reported until a period was measured from two
    //!        vertical blanks, and the measured one is checked against it: times that do not come at the refresh rate of the output
    //!        are not vertical blanks (a X server that shows a image as soon as it gets it and counts that as a refresh, as Xwayland
    //!        does for a software renderer), and nothing is reported then.
    [[nodiscard]] NativeWindowVSyncInfo GetVSyncInfo(const TimeSpan fallbackRefreshPeriod) const noexcept;

  private:
    void OnVBlank(const uint64_t ust, const uint64_t msc) noexcept;
  };
}

#endif
#endif
