#if !defined(__ANDROID__) && defined(__linux__) && !defined(FSL_WINDOWSYSTEM_X11) && defined(FSL_WINDOWSYSTEM_WAYLAND)
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

#include "WaylandPresentationTime.hpp"
#include <FslBase/Log/Log3Fmt.hpp>
#include <FslBase/Time/NanosecondTickCountUtil.hpp>
#include <FslBase/Time/TickCount.hpp>
#include <df-presentation-time-client-protocol.h>    // wayland-scanner created header
#include <wayland-client.h>
#include <cassert>
#include <cstring>
#include <ctime>

namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
      //! Version one has everything that is used here
      constexpr uint32_t BindVersion = 1;
      constexpr int64_t NanosecondsPerSecond = 1000000000;
    }

    //! The kind flags of wp_presentation_feedback.presented as the flags of the framework. A flag the framework has no name for is
    //! left out.
    NativeWindowVSyncTimeFlags ToTimeFlags(const uint32_t kindFlags) noexcept
    {
      NativeWindowVSyncTimeFlags flags = NativeWindowVSyncTimeFlags::NoFlags;
      if ((kindFlags & WP_PRESENTATION_FEEDBACK_KIND_VSYNC) != 0u)
      {
        flags = flags | NativeWindowVSyncTimeFlags::InSyncWithDisplay;
      }
      if ((kindFlags & WP_PRESENTATION_FEEDBACK_KIND_HW_CLOCK) != 0u)
      {
        flags = flags | NativeWindowVSyncTimeFlags::HardwareClock;
      }
      if ((kindFlags & WP_PRESENTATION_FEEDBACK_KIND_HW_COMPLETION) != 0u)
      {
        flags = flags | NativeWindowVSyncTimeFlags::HardwareCompletion;
      }
      if ((kindFlags & WP_PRESENTATION_FEEDBACK_KIND_ZERO_COPY) != 0u)
      {
        flags = flags | NativeWindowVSyncTimeFlags::ZeroCopy;
      }
      return flags;
    }

    void OnPresentationClockId(void* data, struct wp_presentation* /*pPresentation*/, uint32_t clockId)
    {
      auto* pThis = static_cast<WaylandPresentationTime*>(data);
      assert(pThis != nullptr);
      pThis->OnClockId(clockId);
    }

    // The listeners are filled member by member and name only the callbacks of version 1 of the protocol, so they compile without a
    // warning against any version of it. A callback a newer version adds is null, and its event is not sent to a object bound at version 1.
    wp_presentation_listener CreatePresentationListener() noexcept
    {
      wp_presentation_listener listener{};
      listener.clock_id = OnPresentationClockId;
      return listener;
    }

    const wp_presentation_listener g_presentationListener = CreatePresentationListener();

    void OnFeedbackSyncOutput(void* /*data*/, struct wp_presentation_feedback* /*pFeedback*/, wl_output* /*pOutput*/)
    {
      // The output the frame was shown on: the refresh period comes with the presented event, so nothing is needed from it
    }

    void OnFeedbackPresented(void* data, struct wp_presentation_feedback* /*pFeedback*/, uint32_t tvSecHi, uint32_t tvSecLo, uint32_t tvNsec,
                             uint32_t refresh, uint32_t /*seqHi*/, uint32_t /*seqLo*/, uint32_t flags)
    {
      auto* pThis = static_cast<WaylandPresentationTime*>(data);
      assert(pThis != nullptr);
      pThis->OnPresented((static_cast<uint64_t>(tvSecHi) << 32) | static_cast<uint64_t>(tvSecLo), tvNsec, refresh, flags);
    }

    void OnFeedbackDiscarded(void* data, struct wp_presentation_feedback* /*pFeedback*/)
    {
      auto* pThis = static_cast<WaylandPresentationTime*>(data);
      assert(pThis != nullptr);
      pThis->OnDiscarded();
    }

    wp_presentation_feedback_listener CreateFeedbackListener() noexcept
    {
      wp_presentation_feedback_listener listener{};
      listener.sync_output = OnFeedbackSyncOutput;
      listener.presented = OnFeedbackPresented;
      listener.discarded = OnFeedbackDiscarded;
      return listener;
    }

    const wp_presentation_feedback_listener g_feedbackListener = CreateFeedbackListener();
  }


  WaylandPresentationTime::~WaylandPresentationTime() noexcept
  {
    Reset();
  }


  bool WaylandPresentationTime::TryRegistryHandleGlobal(wl_registry* const pRegistry, const uint32_t name, const char* const pszInterface,
                                                        const uint32_t version)
  {
    if (pRegistry == nullptr || pszInterface == nullptr || strcmp(pszInterface, wp_presentation_interface.name) != 0)
    {
      return false;
    }
    if (m_pPresentation == nullptr && version >= LocalConfig::BindVersion)
    {
      m_pPresentation = static_cast<struct wp_presentation*>(wl_registry_bind(pRegistry, name, &wp_presentation_interface, LocalConfig::BindVersion));
      if (m_pPresentation != nullptr)
      {
        // The compositor answers with the clock its display times are on
        wp_presentation_add_listener(m_pPresentation, &g_presentationListener, this);
      }
    }
    return true;
  }


  void WaylandPresentationTime::Reset() noexcept
  {
    DestroyFeedback();
    m_pSurface = nullptr;
    if (m_pPresentation != nullptr)
    {
      wp_presentation_destroy(m_pPresentation);
      m_pPresentation = nullptr;
    }
    m_hasClockId = false;
    m_presentedTime = {};
    m_refreshPeriod = {};
  }


  void WaylandPresentationTime::SetSurface(wl_surface* const pSurface) noexcept
  {
    if (pSurface != m_pSurface)
    {
      // A outstanding request is for the old surface
      DestroyFeedback();
      m_pSurface = pSurface;
      m_presentedTime = {};
      m_refreshPeriod = {};
    }
  }


  void WaylandPresentationTime::RequestFeedback() noexcept
  {
    if (m_pPresentation == nullptr || m_pSurface == nullptr || m_pFeedback != nullptr)
    {
      return;
    }
    // The request is part of the state of the surface that the next commit applies, whoever makes that commit
    m_pFeedback = wp_presentation_feedback(m_pPresentation, m_pSurface);
    if (m_pFeedback != nullptr)
    {
      wp_presentation_feedback_add_listener(m_pFeedback, &g_feedbackListener, this);
    }
  }


  void WaylandPresentationTime::OnClockId(const uint32_t clockId) noexcept
  {
    m_clockId = clockId;
    m_hasClockId = true;
    FSLLOG3_VERBOSE("Wayland: presentation-time reports its times on clock id {} ({})", clockId,
                    clockId == static_cast<uint32_t>(CLOCK_MONOTONIC) ? "CLOCK_MONOTONIC, the clock of the framework"
                                                                      : "not the clock of the framework");
  }


  void WaylandPresentationTime::OnPresented(const uint64_t seconds, const uint32_t nanoseconds, const uint32_t refreshNanoseconds,
                                            const uint32_t flags) noexcept
  {
    // The event destroys the feedback object on the side of the compositor
    DestroyFeedback();
    ++m_presentedCount;
    if (m_presentedCount == 1u || flags != m_lastFlags)
    {
      // How good the times are, said once and again when it changes
      FSLLOG3_VERBOSE(
        "Wayland: presentation-time display time with flags 0x{:x} (0x1 in sync with the display, 0x2 a time of the display hardware, "
        "0x4 the hardware signalled the frame was shown, 0x8 zero copy), refresh period {} ns",
        flags, refreshNanoseconds);
    }
    m_lastFlags = flags;
    m_timeFlags = ToTimeFlags(flags);
    if (!m_hasClockId)
    {
      return;
    }
    // The time is on the clock of the compositor. Both clocks are read now, and the age of the display time on its own clock is taken
    // from the time of the framework: that holds for any clock, and it is as exact as two clock reads next to each other.
    timespec now{};
    if (clock_gettime(static_cast<clockid_t>(m_clockId), &now) != 0)
    {
      return;
    }
    const TickCount timerNow = m_timer.GetTimestamp();
    const int64_t nowNanoseconds = (static_cast<int64_t>(now.tv_sec) * LocalConfig::NanosecondsPerSecond) + static_cast<int64_t>(now.tv_nsec);
    const int64_t presentedNanoseconds = (static_cast<int64_t>(seconds) * LocalConfig::NanosecondsPerSecond) + static_cast<int64_t>(nanoseconds);
    // The compositor gives the time and the period in nanoseconds, and they are kept as that
    m_presentedTime =
      NanosecondTickCount(NanosecondTickCountUtil::FromTickCount(timerNow).TotalNanoseconds() - (nowNanoseconds - presentedNanoseconds));
    // Zero if the output has no constant refresh rate
    m_refreshPeriod = NanosecondTimeSpan(static_cast<int64_t>(refreshNanoseconds));
  }


  void WaylandPresentationTime::OnDiscarded() noexcept
  {
    // The frame the request was made for was never shown
    DestroyFeedback();
    ++m_discardedCount;
  }


  void WaylandPresentationTime::DestroyFeedback() noexcept
  {
    if (m_pFeedback != nullptr)
    {
      wp_presentation_feedback_destroy(m_pFeedback);
      m_pFeedback = nullptr;
    }
  }
}
#endif
