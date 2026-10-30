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

#include "WaylandFrameCallback.hpp"
#include <FslBase/Log/Log3Fmt.hpp>
#include <wayland-client.h>
#include <algorithm>
#include <cassert>
#include <limits>
#include "WaylandEventPump.hpp"

namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
      //! Calls that do not come for this long are logged as stopped
      constexpr TimeSpan StoppedAfter(TimeSpan::TicksPerSecond);
    }

    void OnCallbackDone(void* data, wl_callback* /*pCallback*/, uint32_t /*time*/)
    {
      auto* pPending = static_cast<WaylandFrameCallback::PendingCallback*>(data);
      assert(pPending != nullptr);
      assert(pPending->pOwner != nullptr);
      pPending->pOwner->OnDone(*pPending);
    }

    wl_callback_listener CreateCallbackListener() noexcept
    {
      wl_callback_listener listener{};
      listener.done = OnCallbackDone;
      return listener;
    }

    const wl_callback_listener g_callbackListener = CreateCallbackListener();

    //! A poll takes whole milliseconds, a part of one is rounded up so the wait does not end before the time it was given
    int32_t ToPollMilliseconds(const TimeSpan timeLeft) noexcept
    {
      const int64_t ticks = std::max(timeLeft.Ticks(), static_cast<int64_t>(0));
      const int64_t milliseconds = (ticks + (TimeSpan::TicksPerMillisecond - 1)) / TimeSpan::TicksPerMillisecond;
      return static_cast<int32_t>(std::min(milliseconds, static_cast<int64_t>(std::numeric_limits<int32_t>::max())));
    }
  }


  WaylandFrameCallback::~WaylandFrameCallback() noexcept
  {
    Reset();
  }


  void WaylandFrameCallback::Reset() noexcept
  {
    // A callback object uses the queue, so the queue goes last
    for (PendingCallback& rPending : m_pending)
    {
      rPending.Callback.reset();
    }
    if (m_pSurfaceWrapper != nullptr)
    {
      wl_proxy_wrapper_destroy(m_pSurfaceWrapper);
      m_pSurfaceWrapper = nullptr;
    }
    if (m_pQueue != nullptr)
    {
      wl_event_queue_destroy(m_pQueue);
      m_pQueue = nullptr;
    }
    m_pDisplay = nullptr;
    m_highestPresentId = 0;
    m_callbackCount = 0;
    m_lastCallbackTime = {};
    m_isStopped = false;
  }


  void WaylandFrameCallback::SetSurface(wl_display* const pDisplay, wl_surface* const pSurface) noexcept
  {
    Reset();
    if (pDisplay == nullptr || pSurface == nullptr)
    {
      return;
    }
    wl_event_queue* const pQueue = wl_display_create_queue(pDisplay);
    if (pQueue == nullptr)
    {
      FSLLOG3_WARNING("Wayland: the event queue of the frame callbacks could not be created");
      return;
    }
    // A callback goes to the queue of the object it was made from. The wrapper is that object, and the surface itself stays on the default
    // queue with its listener.
    void* const pWrapper = wl_proxy_create_wrapper(pSurface);
    if (pWrapper == nullptr)
    {
      FSLLOG3_WARNING("Wayland: the surface wrapper of the frame callbacks could not be created");
      wl_event_queue_destroy(pQueue);
      return;
    }
    wl_proxy_set_queue(static_cast<wl_proxy*>(pWrapper), pQueue);
    m_pDisplay = pDisplay;
    m_pQueue = pQueue;
    m_pSurfaceWrapper = static_cast<wl_surface*>(pWrapper);
  }


  void WaylandFrameCallback::Request(const uint64_t presentId) noexcept
  {
    if (m_pSurfaceWrapper == nullptr)
    {
      return;
    }
    PendingCallback* pFree = nullptr;
    for (PendingCallback& rPending : m_pending)
    {
      if (!rPending.Callback)
      {
        pFree = &rPending;
        break;
      }
    }
    if (pFree == nullptr)
    {
      return;
    }
    // The request is part of the state of the surface that the next commit applies, whoever makes that commit
    pFree->Callback.reset(wl_surface_frame(m_pSurfaceWrapper));
    if (!pFree->Callback)
    {
      return;
    }
    pFree->pOwner = this;
    pFree->PresentId = presentId;
    // Only this thread dispatches the queue, so a call that was read already waits for the listener
    wl_callback_add_listener(pFree->Callback.get(), &g_callbackListener, pFree);
    if (m_callbackCount == 0u && m_lastCallbackTime.Ticks() == 0)
    {
      m_lastCallbackTime = m_timer.GetTimestamp();
    }
  }


  bool WaylandFrameCallback::TryWait(const uint64_t presentId, const TimeSpan timeout) noexcept
  {
    if (m_pQueue == nullptr)
    {
      return false;
    }
    const TickCount deadline = m_timer.GetTimestamp() + timeout;
    bool isTimeUp = false;
    while (wl_display_dispatch_queue_pending(m_pDisplay, m_pQueue) >= 0)
    {
      if (m_highestPresentId >= presentId)
      {
        return true;
      }
      if (isTimeUp)
      {
        break;
      }
      const TimeSpan timeLeft = deadline - m_timer.GetTimestamp();
      isTimeUp = timeLeft.Ticks() <= 0;
      // A lost connection is reported by the message loop of the window system
      if (!WaylandEventPump::TryReadEvents(m_pDisplay, m_pQueue, ToPollMilliseconds(timeLeft)))
      {
        break;
      }
    }
    UpdateStopped();
    return false;
  }


  void WaylandFrameCallback::OnDone(PendingCallback& rCallback) noexcept
  {
    // The event destroys the callback object on the side of the compositor
    m_highestPresentId = std::max(m_highestPresentId, rCallback.PresentId);
    rCallback.Callback.reset();

    const TickCount now = m_timer.GetTimestamp();
    ++m_callbackCount;
    if (m_callbackCount == 1u)
    {
      FSLLOG3_VERBOSE("Wayland: frame callback: the first call came {:.1f} ms after the first request",
                      (now - m_lastCallbackTime).TotalMilliseconds());
    }
    if (m_isStopped)
    {
      m_isStopped = false;
      FSLLOG3_VERBOSE("Wayland: frame callback: the calls came back after {:.1f} ms", (now - m_lastCallbackTime).TotalMilliseconds());
    }
    m_lastCallbackTime = now;
  }


  void WaylandFrameCallback::UpdateStopped() noexcept
  {
    if (m_isStopped)
    {
      return;
    }
    const bool hasPending =
      std::any_of(m_pending.begin(), m_pending.end(), [](const PendingCallback& pending) { return pending.Callback != nullptr; });
    if (hasPending && (m_timer.GetTimestamp() - m_lastCallbackTime) >= LocalConfig::StoppedAfter)
    {
      m_isStopped = true;
      FSLLOG3_VERBOSE("Wayland: frame callback: no call for {:.1f} ms while one is asked for (the compositor is not drawing the window)",
                      (m_timer.GetTimestamp() - m_lastCallbackTime).TotalMilliseconds());
    }
  }
}
#endif
