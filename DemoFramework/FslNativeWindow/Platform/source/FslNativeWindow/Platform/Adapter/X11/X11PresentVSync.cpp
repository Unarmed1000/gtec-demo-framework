#if !defined(__ANDROID__) && (defined(__linux__) || defined(FSL_PLATFORM_APPLE)) && defined(FSL_WINDOWSYSTEM_X11)
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

#include "X11PresentVSync.hpp"
#include <FslBase/Log/Log3Fmt.hpp>
#include <FslBase/Time/NanosecondTickCount.hpp>
#include <FslBase/Time/TickCount.hpp>

#ifdef FSL_WINDOWSYSTEM_X11_PRESENT
#include <X11/extensions/Xpresent.h>
#endif

namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
      //! The time of the X server is in microseconds
      constexpr int64_t TicksPerMicrosecond = TickCount::TicksPerMicrosecond;
      constexpr int64_t NanosecondsPerMicrosecond = NanosecondTickCount::NanosecondsPerMicrosecond;
      //! A vertical blank time further than this from the time of the framework is not on the clock of the framework
      constexpr int64_t MaxClockDistanceTicks = TickCount::TicksPerSecond * 10;
      //! The measured period is the refresh period of the output if the two differ by less than a twentieth (5 %) of it
      constexpr int64_t RefreshToleranceDivisor = 20;
      //! A vertical blank is only asked for after this many frames without a event of a present of the graphics API
      constexpr uint32_t FramesBeforeRequest = 120;
      //! The refresh period is measured from a vertical blank that is replaced when it is more than this many refreshes old
      constexpr uint64_t MaxMeasureRefreshes = 240;
    }
  }

#ifdef FSL_WINDOWSYSTEM_X11_PRESENT

  X11PresentVSync::X11PresentVSync(Display* const pDisplay, const Window window)
    : m_pDisplay(pDisplay)
    , m_window(window)
  {
    if (pDisplay == nullptr || window == 0u)
    {
      m_unavailableReason = "no window";
      return;
    }
    int eventBase = 0;
    int errorBase = 0;
    if (XPresentQueryExtension(pDisplay, &m_majorOpcode, &eventBase, &errorBase) == 0)
    {
      m_unavailableReason = "the X server does not have the Present extension";
      return;
    }
    int majorVersion = 0;
    int minorVersion = 0;
    XPresentQueryVersion(pDisplay, &majorVersion, &minorVersion);
    // The events of the extension that are about this window, they arrive as generic events
    m_eventId = static_cast<uint32_t>(XPresentSelectInput(pDisplay, window, PresentCompleteNotifyMask));
    m_isAvailable = true;
    FSLLOG3_VERBOSE("X11: the Present extension {}.{} gives the vsync time of the window", majorVersion, minorVersion);
  }


  X11PresentVSync::~X11PresentVSync()
  {
    if (m_isAvailable && m_pDisplay != nullptr)
    {
      XPresentFreeInput(m_pDisplay, m_window, m_eventId);
    }
  }


  void X11PresentVSync::RequestVBlankTime()
  {
    if (!m_isAvailable)
    {
      return;
    }
    if (m_framesWithoutPresentEvent < LocalConfig::FramesBeforeRequest)
    {
      // The presents of the graphics API bring the times, or it is too early to say that they do not
      ++m_framesWithoutPresentEvent;
      return;
    }
    if (m_isRequestPending)
    {
      return;
    }
    // A target of zero is a vertical blank that has passed, so the X server answers with the one the output of the window is at
    ++m_requestSerial;
    XPresentNotifyMSC(m_pDisplay, m_window, m_requestSerial, 0, 0, 0);
    m_isRequestPending = true;
  }


  bool X11PresentVSync::TryHandleEvent(XEvent& rEvent)
  {
    if (!m_isAvailable || rEvent.type != GenericEvent || rEvent.xcookie.extension != m_majorOpcode)
    {
      return false;
    }
    if (XGetEventData(m_pDisplay, &rEvent.xcookie) != 0)
    {
      if (rEvent.xcookie.evtype == PresentCompleteNotify)
      {
        const auto* const pEvent = static_cast<const XPresentCompleteNotifyEvent*>(rEvent.xcookie.data);
        if (pEvent != nullptr && pEvent->window == m_window)
        {
          if (pEvent->kind == PresentCompleteKindNotifyMSC)
          {
            if (pEvent->serial_number == m_requestSerial)
            {
              m_isRequestPending = false;
            }
          }
          else
          {
            // A image the graphics API presented: its events bring the times, so none has to be asked for
            m_framesWithoutPresentEvent = 0;
          }
          // The answer to the request, or a image a graphics API presented to the window: both say when a vertical blank was
          OnVBlank(pEvent->ust, pEvent->msc);
        }
      }
      XFreeEventData(m_pDisplay, &rEvent.xcookie);
    }
    return true;
  }

#else

  X11PresentVSync::X11PresentVSync(Display* const pDisplay, const Window window)
    : m_pDisplay(pDisplay)
    , m_window(window)
    , m_unavailableReason("built without libXpresent")
  {
  }


  X11PresentVSync::~X11PresentVSync() = default;


  void X11PresentVSync::RequestVBlankTime()
  {
  }


  bool X11PresentVSync::TryHandleEvent(XEvent& /*rEvent*/)
  {
    return false;
  }

#endif

  NativeWindowVSyncInfo X11PresentVSync::GetVSyncInfo(const NanosecondTimeSpan fallbackRefreshPeriod) const noexcept
  {
    if (!m_isAvailable || !m_hasTime || !m_isClockUsable)
    {
      return {};
    }
    if (m_measuredPeriod.TotalNanoseconds() > 0 && fallbackRefreshPeriod.TotalNanoseconds() > 0)
    {
      // The counter of the X server is only a refresh counter if it counts at the refresh rate of the output
      const int64_t difference = m_measuredPeriod.TotalNanoseconds() - fallbackRefreshPeriod.TotalNanoseconds();
      const int64_t allowed = fallbackRefreshPeriod.TotalNanoseconds() / LocalConfig::RefreshToleranceDivisor;
      const bool isOnRefresh = difference >= -allowed && difference <= allowed;
      if (isOnRefresh != m_wasOnRefresh)
      {
        m_wasOnRefresh = isOnRefresh;
        if (isOnRefresh)
        {
          FSLLOG3_INFO("X11: the times of the Present extension are at the refresh rate of the output again, they are used as the vsync time");
        }
        else
        {
          FSLLOG3_WARNING(
            "X11: the times of the Present extension come every {:.2f} ms and the output refreshes every {:.2f} ms, so they are "
            "not vertical blanks and are not used as the vsync time",
            m_measuredPeriod.TotalMilliseconds(), fallbackRefreshPeriod.TotalMilliseconds());
        }
      }
      if (!isOnRefresh)
      {
        return {};
      }
    }
    return {NanosecondTickCount(static_cast<int64_t>(m_lastUst) * LocalConfig::NanosecondsPerMicrosecond),
            m_measuredPeriod.TotalNanoseconds() > 0 ? m_measuredPeriod : fallbackRefreshPeriod};
  }


  void X11PresentVSync::OnVBlank(const uint64_t ust, const uint64_t msc) noexcept
  {
    ++m_eventCount;
    if (ust == 0u)
    {
      // The X server has no time for it (a window that is not shown on a output)
      return;
    }
    {    // The time is only of use if it is on the clock of the framework
      const int64_t ustTicks = static_cast<int64_t>(ust) * LocalConfig::TicksPerMicrosecond;
      const int64_t distance = m_timer.GetTimestamp().Ticks() - ustTicks;
      const bool isUsable = distance > -LocalConfig::MaxClockDistanceTicks && distance < LocalConfig::MaxClockDistanceTicks;
      if (!isUsable && m_isClockUsable)
      {
        FSLLOG3_WARNING(
          "X11: the vertical blank times of the Present extension are not on the clock of the framework ({} seconds apart), so "
          "they are not used",
          distance / TickCount::TicksPerSecond);
      }
      m_isClockUsable = isUsable;
    }
    if (!m_hasTime)
    {
      FSLLOG3_VERBOSE("X11: the first vertical blank time from the Present extension, {} microseconds, refresh counter {}", ust, msc);
      m_referenceUst = ust;
      m_referenceMsc = msc;
    }
    else if (msc > m_referenceMsc && ust > m_referenceUst)
    {
      // Two vertical blanks and the number of refreshes between them. The times are in microseconds, so the further apart the two
      // are the better the period, up to a limit so a change of the refresh rate is followed.
      const uint64_t refreshes = msc - m_referenceMsc;
      m_measuredPeriod = NanosecondTimeSpan(
        static_cast<int64_t>(((ust - m_referenceUst) * static_cast<uint64_t>(LocalConfig::NanosecondsPerMicrosecond)) / refreshes));
      if (refreshes > LocalConfig::MaxMeasureRefreshes)
      {
        m_referenceUst = ust;
        m_referenceMsc = msc;
      }
    }
    else if (msc < m_referenceMsc)
    {
      // The counter went back: another output
      m_referenceUst = ust;
      m_referenceMsc = msc;
      m_measuredPeriod = {};
    }
    if (!m_hasTime || msc >= m_lastMsc)
    {
      m_lastUst = ust;
      m_lastMsc = msc;
      m_hasTime = true;
    }
  }
}
#endif
