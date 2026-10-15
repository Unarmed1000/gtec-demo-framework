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

#include <Shared/FramePacing/SampleAnimationError.hpp>
#include <algorithm>
#include <cstdlib>

namespace Fsl
{
  void SampleAnimationError::Clear() noexcept
  {
    m_frames = {};
    m_first = 0;
    m_count = 0;
    m_newestPresentId = 0;
    m_hasShown = false;
    m_shownAnimationTime = {};
    m_shownDisplayTime = {};
    m_shownIntendedDisplayTime = {};
    m_lastStepUneven = false;
    m_runStutter = {};
    m_stutter = {};
    m_stutterNext = 0;
    m_stutterCount = 0;
    m_errors = {};
    m_errorNext = 0;
    m_errorCount = 0;
  }


  void SampleAnimationError::AddFrame(const uint64_t presentId, const TimeSpan animationTime, const TimeSpan targetFrameTime,
                                      const std::optional<TickCount> intendedDisplayTime, const TimeSpan refreshPeriod) noexcept
  {
    if (m_count == Capacity)
    {
      // Nobody takes the frames: the oldest one makes room, and the frame after it has nothing to be compared with
      m_first = (m_first + 1u) % Capacity;
      --m_count;
      m_hasShown = false;
      m_lastStepUneven = false;
    }
    Frame& rFrame = m_frames[(m_first + m_count) % Capacity];
    rFrame = {};
    rFrame.PresentId = presentId;
    rFrame.AnimationTime = animationTime;
    rFrame.TargetFrameTime = targetFrameTime;
    rFrame.RefreshPeriod = refreshPeriod;
    rFrame.IntendedDisplayTime = intendedDisplayTime;
    ++m_count;
    m_newestPresentId = presentId;
  }


  void SampleAnimationError::AddDisplayTime(const uint64_t presentId, const TickCount displayTime) noexcept
  {
    Frame* const pFrame = TryFind(presentId);
    if (pFrame != nullptr)
    {
      pFrame->DisplayTime = displayTime;
      pFrame->NotShown = false;
    }
  }


  void SampleAnimationError::AddNotShown(const uint64_t presentId) noexcept
  {
    Frame* const pFrame = TryFind(presentId);
    if (pFrame != nullptr && !pFrame->DisplayTime.has_value())
    {
      pFrame->NotShown = true;
    }
  }


  bool SampleAnimationError::TryPop(SampleAnimationErrorRecord& rRecord) noexcept
  {
    if (m_count == 0u)
    {
      return false;
    }
    const Frame frame = m_frames[m_first];
    const bool gaveUp = (m_newestPresentId - frame.PresentId) >= MaxWaitFrames;
    if (!frame.DisplayTime.has_value() && !frame.NotShown && !gaveUp)
    {
      return false;
    }
    m_first = (m_first + 1u) % Capacity;
    --m_count;

    rRecord = {};
    rRecord.PresentId = frame.PresentId;
    if (!frame.DisplayTime.has_value())
    {
      // Not shown, or not known when: the step into it and the step out of it are not judged
      m_hasShown = false;
      m_lastStepUneven = false;
      return true;
    }
    const TickCount displayTime = *frame.DisplayTime;

    if (m_hasShown)
    {
      rRecord.IsJudged = true;
      rRecord.AnimationStep = frame.AnimationTime - m_shownAnimationTime;
      rRecord.DisplayStep = displayTime - m_shownDisplayTime;
      rRecord.Error = rRecord.AnimationStep - rRecord.DisplayStep;

      m_errors[m_errorNext] = rRecord.Error.Ticks();
      m_errorNext = (m_errorNext + 1u) % StatsFrames;
      m_errorCount = std::min(m_errorCount + 1u, StatsFrames);

      // The rules of the mb-framepacing tools. Half a refresh is their tolerance, and without a refresh period nothing can be told
      // about the display: no frame is late and a error is taken to be the display's.
      const int64_t refreshTicks = frame.RefreshPeriod.Ticks();
      const int64_t halfRefreshTicks = refreshTicks / 2;
      rRecord.IntendedPeriod = CalcIntendedPeriod(frame.TargetFrameTime, frame.RefreshPeriod);
      // Late: the frame before stayed on screen half a refresh or more longer than this frame was animated for and than its intended
      // period. A frame that did not move (a paused app) is never late.
      rRecord.IsLate = refreshTicks > 0 && rRecord.AnimationStep.Ticks() != 0 && rRecord.Error.Ticks() <= -halfRefreshTicks &&
                       rRecord.DisplayStep.Ticks() >= (rRecord.IntendedPeriod.Ticks() + halfRefreshTicks);
      // The display was uneven where the frame before this one was not on screen for its intended period: half a refresh or more off
      const int64_t offIntendedTicks = std::abs((rRecord.DisplayStep - rRecord.IntendedPeriod).Ticks());
      const bool isStepUneven = refreshTicks <= 0 || offIntendedTicks >= halfRefreshTicks;
      const int64_t absErrorTicks = std::abs(rRecord.Error.Ticks());
      if (absErrorTicks > ErrorThreshold.Ticks())
      {
        bool isPacing = true;
        if (refreshTicks > 0)
        {
          if (frame.IntendedDisplayTime.has_value() && m_shownIntendedDisplayTime.has_value())
          {
            // The pacer said when both frames were to be shown: the display is to blame if it was as far or further off that step
            // than the animation was
            const TimeSpan intendedStep = *frame.IntendedDisplayTime - *m_shownIntendedDisplayTime;
            isPacing = std::abs((rRecord.DisplayStep - intendedStep).Ticks()) >= std::abs((rRecord.AnimationStep - intendedStep).Ticks());
          }
          else
          {
            // And the frame after a uneven step has the other half of it: it comes on time and has moved by the wrong time
            isPacing = isStepUneven || m_lastStepUneven;
          }
        }
        rRecord.Cause = isPacing ? SampleAnimationErrorCause::Pacing : SampleAnimationErrorCause::DeltaTimeJitter;
        rRecord.IsDeltaTimeJitter = !isPacing && absErrorTicks < rRecord.IntendedPeriod.Ticks() && !rRecord.IsLate;
        ++m_runStutter.ErrorFrames;
        if (rRecord.IsLate)
        {
          ++m_runStutter.Late;
        }
        if (rRecord.IsDeltaTimeJitter)
        {
          ++m_runStutter.DeltaTimeJitter;
        }
        m_stutter[m_stutterNext] = {displayTime, rRecord.IsLate, rRecord.IsDeltaTimeJitter};
        m_stutterNext = (m_stutterNext + 1u) % StutterCapacity;
        m_stutterCount = std::min(m_stutterCount + 1u, StutterCapacity);
      }
      m_lastStepUneven = isStepUneven;
    }
    else
    {
      m_lastStepUneven = false;
    }
    m_hasShown = true;
    m_shownAnimationTime = frame.AnimationTime;
    m_shownDisplayTime = displayTime;
    m_shownIntendedDisplayTime = frame.IntendedDisplayTime;
    return true;
  }


  SampleAnimationErrorStats SampleAnimationError::CalcStats() const noexcept
  {
    SampleAnimationErrorStats stats;
    if (m_errorCount == 0u)
    {
      return stats;
    }
    int64_t totalAbs = 0;
    int64_t worst = 0;
    for (std::size_t i = 0; i < m_errorCount; ++i)
    {
      const int64_t error = m_errors[i];
      totalAbs += std::abs(error);
      if (std::abs(error) > ErrorThreshold.Ticks())
      {
        ++stats.ErrorFrames;
      }
      if (std::abs(error) > std::abs(worst))
      {
        worst = error;
      }
    }
    stats.Frames = static_cast<uint32_t>(m_errorCount);
    stats.AverageAbsError = TimeSpan(totalAbs / static_cast<int64_t>(m_errorCount));
    stats.WorstError = TimeSpan(worst);
    return stats;
  }


  SampleStutterCount SampleAnimationError::CalcRecentStutter() const noexcept
  {
    SampleStutterCount count;
    if (!m_hasShown)
    {
      return count;
    }
    for (std::size_t i = 0; i < m_stutterCount; ++i)
    {
      const StutterEntry& entry = m_stutter[i];
      if ((m_shownDisplayTime - entry.DisplayTime) < RecentTime)
      {
        ++count.ErrorFrames;
        if (entry.IsLate)
        {
          ++count.Late;
        }
        if (entry.IsDeltaTimeJitter)
        {
          ++count.DeltaTimeJitter;
        }
      }
    }
    return count;
  }


  int32_t SampleAnimationError::ToRefreshThousandths(const TimeSpan error, const TimeSpan refreshPeriod) noexcept
  {
    if (refreshPeriod.Ticks() <= 0)
    {
      return 0;
    }
    // Kept to what fits before it is scaled, then rounded to the nearest thousandth, away from zero at the half
    const int64_t maxTicks = refreshPeriod.Ticks() * (int64_t{MaxRefreshThousandths} / 1000);
    const int64_t scaled = std::clamp(error.Ticks(), -maxTicks, maxTicks) * 1000;
    const int64_t half = refreshPeriod.Ticks() / 2;
    const int64_t thousandths = (scaled >= 0 ? (scaled + half) : (scaled - half)) / refreshPeriod.Ticks();
    return static_cast<int32_t>(std::clamp(thousandths, -int64_t{MaxRefreshThousandths}, int64_t{MaxRefreshThousandths}));
  }


  TimeSpan SampleAnimationError::CalcIntendedPeriod(const TimeSpan targetFrameTime, const TimeSpan refreshPeriod) noexcept
  {
    const int64_t refreshTicks = refreshPeriod.Ticks();
    if (refreshTicks <= 0)
    {
      return targetFrameTime;
    }
    // Whole refreshes, rounded up, where a target that is a twentieth of a refresh or less over a whole number is that number
    const int64_t slackTicks = refreshTicks / 20;
    const int64_t overSlackTicks = targetFrameTime.Ticks() - slackTicks;
    const int64_t refreshes = overSlackTicks > 0 ? std::max((overSlackTicks + refreshTicks - 1) / refreshTicks, int64_t{1}) : int64_t{1};
    return TimeSpan(refreshes * refreshTicks);
  }


  SampleAnimationError::Frame* SampleAnimationError::TryFind(const uint64_t presentId) noexcept
  {
    for (std::size_t i = 0; i < m_count; ++i)
    {
      Frame& rFrame = m_frames[(m_first + i) % Capacity];
      if (rFrame.PresentId == presentId)
      {
        return &rFrame;
      }
    }
    return nullptr;
  }
}
