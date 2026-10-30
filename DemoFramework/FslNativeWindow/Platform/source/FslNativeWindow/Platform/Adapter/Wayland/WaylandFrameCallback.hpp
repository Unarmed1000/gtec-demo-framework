#ifndef FSLNATIVEWINDOW_PLATFORM_ADAPTER_WAYLAND_WAYLANDFRAMECALLBACK_HPP
#define FSLNATIVEWINDOW_PLATFORM_ADAPTER_WAYLAND_WAYLANDFRAMECALLBACK_HPP
#if !defined(__ANDROID__) && defined(__linux__) && !defined(FSL_WINDOWSYSTEM_X11) && defined(FSL_WINDOWSYSTEM_WAYLAND) && \
  defined(FSL_WINDOWSYSTEM_WAYLAND_XDG)
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
#include <array>
#include <cstddef>
#include <cstdint>
#include "WaylandRAII.hpp"

struct wl_display;
struct wl_event_queue;
struct wl_surface;

namespace Fsl
{
  //! When the compositor called for a new frame after a commit of a surface (wl_surface.frame), which a app can wait for.
  //!
  //! The commits of a surface are done by the graphics API (the Vulkan window system layer in vkQueuePresentKHR, EGL in eglSwapBuffers), so
  //! a callback is asked for just before a present and rides on the commit of that present. The calls of the compositor come in the
  //! order of the commits, so a later one answers for a earlier present as well, and one number is all that is kept: the highest present
  //! that was answered for.
  //!
  //! The callbacks are on a event queue of their own, through a wrapper of the surface that leaves the surface itself on the default queue.
  //! A thread of the graphics API can read them from the socket, but only TryWait dispatches that queue. So the wait runs no other
  //! listener in the middle of a frame, and everything here runs on the thread that calls TryWait: nothing here needs a lock.
  //!
  //! The time a call of the compositor carries is not used: it is in milliseconds with a base the protocol leaves undefined.
  class WaylandFrameCallback
  {
  public:
    //! One callback that was asked for and did not come yet
    struct PendingCallback
    {
      WaylandFrameCallback* pOwner{nullptr};
      ScopedWaylandCallback Callback;
      uint64_t PresentId{0};
    };

  private:
    //! A window that is not shown gets no calls, so the requests have a limit. While it is reached a present asks for nothing: a
    //! wait for that present runs out, and the later calls answer for it if they come.
    static constexpr std::size_t MaxPending = 8;

    HighResolutionTimer m_timer;
    wl_display* m_pDisplay{nullptr};
    wl_event_queue* m_pQueue{nullptr};
    wl_surface* m_pSurfaceWrapper{nullptr};
    std::array<PendingCallback, MaxPending> m_pending;
    //! The highest present the compositor called for a new frame after
    uint64_t m_highestPresentId{0};
    uint64_t m_callbackCount{0};
    //! When the last call came, and until the first one when the first request was made
    TickCount m_lastCallbackTime;
    bool m_isStopped{false};

  public:
    WaylandFrameCallback(const WaylandFrameCallback&) = delete;
    WaylandFrameCallback& operator=(const WaylandFrameCallback&) = delete;

    WaylandFrameCallback() = default;
    ~WaylandFrameCallback() noexcept;

    //! @brief Release everything. Must be called before the surface is destroyed and the display is disconnected.
    void Reset() noexcept;

    //! @brief The surface the callbacks are asked for (nullptr for none, which drops what is outstanding)
    void SetSurface(wl_display* const pDisplay, wl_surface* const pSurface) noexcept;

    //! @brief Ask for the call of the compositor after the commit of the present that is about to be made
    void Request(const uint64_t presentId) noexcept;

    //! @brief Wait until the compositor called for a new frame after the given present or a later one
    //! @param timeout the longest to wait, zero only looks
    //! @return true if it did
    [[nodiscard]] bool TryWait(const uint64_t presentId, const TimeSpan timeout) noexcept;

    // The event handler, called by the listener
    void OnDone(PendingCallback& rCallback) noexcept;

  private:
    void UpdateStopped() noexcept;
  };
}

#endif
#endif
