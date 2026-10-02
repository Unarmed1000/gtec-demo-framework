#ifndef SHARED_FRAMEPACING_SAMPLEFRAMESTATS_HPP
#define SHARED_FRAMEPACING_SAMPLEFRAMESTATS_HPP
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
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace Fsl
{
  //! The frame times of a number of frames
  struct SampleFrameTimes
  {
    TimeSpan Average;
    TimeSpan Min;
    TimeSpan Max;
  };

  //! The frames of the last two seconds as the sample measures them: their frame times and how many of them were late (the frame pacer
  //! counts its own late frames, these are for the sample while the pacer is off).
  //! A frame is late the way the pacer counts it: the time from the start of the previous frame to its start, in whole display refreshes,
  //! is longer than the refreshes the previous frame was held for.
  class SampleFrameStats final
  {
    struct FrameRecord
    {
      TickCount StartTime;
      //! The time from the start of the previous frame to the start of this frame
      TimeSpan Interval;
      bool Late{false};
    };

    //! Two seconds of frames at 500 fps, at a higher frame rate the window is shorter
    static constexpr std::size_t Capacity = 1024;

    std::array<FrameRecord, Capacity> m_frames{};
    std::size_t m_firstIndex{0};
    std::size_t m_count{0};
    uint32_t m_lateCount{0};

  public:
    //! How far back the frames are counted
    static constexpr TimeSpan WindowLength = TimeSpan(2 * TimeSpan::TicksPerSecond);

    constexpr void Clear() noexcept
    {
      m_firstIndex = 0;
      m_count = 0;
      m_lateCount = 0;
    }

    //! @brief Add a frame.
    //! @param startTime when the frame started (a HighResolutionTimer timestamp)
    //! @param frameInterval the time from the start of the previous frame to the start of this frame
    //! @param refreshPeriod the time between two refreshes of the display
    //! @param swapInterval the number of refreshes the previous frame was held for
    constexpr void AddFrame(const TickCount startTime, const TimeSpan frameInterval, const TimeSpan refreshPeriod,
                            const uint32_t swapInterval) noexcept
    {
      // Forget the frames that are older than the window, and the oldest one if there is no room
      while (m_count > 0 && ((startTime - m_frames[m_firstIndex].StartTime) > WindowLength || m_count >= Capacity))
      {
        RemoveFirst();
      }

      const int64_t periodTicks = refreshPeriod.Ticks();
      // The interval in whole refreshes, rounded to the nearest one
      const int64_t refreshes = periodTicks > 0 ? ((frameInterval.Ticks() + (periodTicks / 2)) / periodTicks) : 0;
      const bool late = std::cmp_greater(refreshes, swapInterval);

      m_frames[(m_firstIndex + m_count) % Capacity] = FrameRecord{startTime, frameInterval, late};
      ++m_count;
      if (late)
      {
        ++m_lateCount;
      }
    }

    [[nodiscard]] constexpr uint32_t FrameCount() const noexcept
    {
      return static_cast<uint32_t>(m_count);
    }

    [[nodiscard]] constexpr uint32_t LateFrameCount() const noexcept
    {
      return m_lateCount;
    }

    //! The average, the shortest and the longest frame time of the frames (all zero without frames)
    [[nodiscard]] constexpr SampleFrameTimes FrameTimes() const noexcept
    {
      if (m_count == 0)
      {
        return {};
      }
      int64_t sumTicks = 0;
      TimeSpan minInterval = m_frames[m_firstIndex].Interval;
      TimeSpan maxInterval = minInterval;
      for (std::size_t i = 0; i < m_count; ++i)
      {
        const TimeSpan interval = m_frames[(m_firstIndex + i) % Capacity].Interval;
        sumTicks += interval.Ticks();
        if (interval < minInterval)
        {
          minInterval = interval;
        }
        if (interval > maxInterval)
        {
          maxInterval = interval;
        }
      }
      return SampleFrameTimes{TimeSpan(sumTicks / static_cast<int64_t>(m_count)), minInterval, maxInterval};
    }

  private:
    constexpr void RemoveFirst() noexcept
    {
      if (m_frames[m_firstIndex].Late)
      {
        --m_lateCount;
      }
      m_firstIndex = (m_firstIndex + 1) % Capacity;
      --m_count;
    }
  };
}

#endif
