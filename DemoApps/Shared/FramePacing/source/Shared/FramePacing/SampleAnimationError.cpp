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
    m_errors = {};
    m_errorNext = 0;
    m_errorCount = 0;
  }


  void SampleAnimationError::AddFrame(const uint64_t presentId, const TimeSpan animationTime) noexcept
  {
    if (m_count == Capacity)
    {
      // Nobody takes the frames: the oldest one makes room, and the frame after it has nothing to be compared with
      m_first = (m_first + 1u) % Capacity;
      --m_count;
      m_hasShown = false;
    }
    Frame& rFrame = m_frames[(m_first + m_count) % Capacity];
    rFrame = {};
    rFrame.PresentId = presentId;
    rFrame.AnimationTime = animationTime;
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
    }
    m_hasShown = true;
    m_shownAnimationTime = frame.AnimationTime;
    m_shownDisplayTime = displayTime;
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
