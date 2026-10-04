#if !defined(__ANDROID__) && defined(__linux__) && !defined(FSL_WINDOWSYSTEM_X11) && defined(FSL_WINDOWSYSTEM_WAYLAND)
#ifndef FSLNATIVEWINDOW_PLATFORM_ADAPTER_WAYLAND_WAYLANDPRESENTATIONTIME_HPP
#define FSLNATIVEWINDOW_PLATFORM_ADAPTER_WAYLAND_WAYLANDPRESENTATIONTIME_HPP
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
#include <FslBase/Time/TickCount.hpp>
#include <FslBase/Time/TimeSpan.hpp>
#include <FslNativeWindow/Base/NativeWindowVSyncInfo.hpp>
#include <cstdint>

struct wl_registry;
struct wl_surface;
struct wp_presentation;
struct wp_presentation_feedback;

namespace Fsl
{
  //! When the compositor showed the frames of a surface, from the presentation-time protocol (wp_presentation).
  //!
  //! The protocol reports, for a commit of a surface, the time its content was first shown and the refresh period of the output. A feedback
  //! object has to be requested before the commit it is about, and the commits of a surface are done by the graphics API (the Vulkan
  //! window system layer in vkQueuePresentKHR, EGL in eglSwapBuffers), so the request is made once per frame from the message loop of the
  //! window system and rides on the next commit. One request is outstanding at a time: that is a display time every frame or two, which
  //! is all a vsync time needs.
  //!
  //! The times are on the clock the compositor names (clock_id), they are converted to the clock of the HighResolutionTimer when they
  //! arrive. Everything here runs on the thread that dispatches the default queue of the display.
  class WaylandPresentationTime
  {
    HighResolutionTimer m_timer;
    struct wp_presentation* m_pPresentation{nullptr};
    struct wp_presentation_feedback* m_pFeedback{nullptr};
    wl_surface* m_pSurface{nullptr};
    bool m_hasClockId{false};
    uint32_t m_clockId{0};
    TickCount m_presentedTime;
    TimeSpan m_refreshPeriod;
    uint32_t m_lastFlags{0};
    uint64_t m_presentedCount{0};
    uint64_t m_discardedCount{0};

  public:
    WaylandPresentationTime(const WaylandPresentationTime&) = delete;
    WaylandPresentationTime& operator=(const WaylandPresentationTime&) = delete;

    WaylandPresentationTime() = default;
    ~WaylandPresentationTime() noexcept;

    //! @brief Bind the global if it is the presentation-time one.
    //! @return true if the interface was wp_presentation (it is bound once, a second global of that name is ignored)
    bool TryRegistryHandleGlobal(wl_registry* const pRegistry, const uint32_t name, const char* const pszInterface, const uint32_t version);

    //! @brief Release everything. Must be called before the display is disconnected.
    void Reset() noexcept;

    //! @brief True if the compositor has the protocol and it was bound
    [[nodiscard]] bool IsBound() const noexcept
    {
      return m_pPresentation != nullptr;
    }

    //! @brief The surface the display times are asked for (nullptr for none, which also drops a outstanding request)
    void SetSurface(wl_surface* const pSurface) noexcept;

    //! @brief Ask for the display time of the next commit of the surface, unless a request is outstanding. Called once per frame.
    void RequestFeedback() noexcept;

    //! @brief The last time a frame of the surface was shown and the refresh period the compositor reported with it.
    //! @note  The period is zero if the compositor gave none (a output without a constant refresh rate), IsValid() is false then.
    [[nodiscard]] NativeWindowVSyncInfo GetVSyncInfo() const noexcept
    {
      return {m_presentedTime, m_refreshPeriod, m_lastFlags};
    }

    //! @brief The last time a frame of the surface was shown (zero ticks if none was reported yet)
    [[nodiscard]] TickCount GetPresentedTime() const noexcept
    {
      return m_presentedTime;
    }

    //! @brief How many display times arrived, and how many requests ended with a frame that was never shown
    [[nodiscard]] uint64_t GetPresentedCount() const noexcept
    {
      return m_presentedCount;
    }

    [[nodiscard]] uint64_t GetDiscardedCount() const noexcept
    {
      return m_discardedCount;
    }

    //! @brief The flags of the last display time (wp_presentation_feedback.kind: 0x1 vsync, 0x2 hardware clock, 0x4 hardware completion,
    //!        0x8 zero copy), they say how good the time is
    [[nodiscard]] uint32_t GetLastFlags() const noexcept
    {
      return m_lastFlags;
    }

    // The event handlers, called by the listeners
    void OnClockId(const uint32_t clockId) noexcept;
    void OnPresented(const uint64_t seconds, const uint32_t nanoseconds, const uint32_t refreshNanoseconds, const uint32_t flags) noexcept;
    void OnDiscarded() noexcept;

  private:
    void DestroyFeedback() noexcept;
  };
}

#endif
#endif
