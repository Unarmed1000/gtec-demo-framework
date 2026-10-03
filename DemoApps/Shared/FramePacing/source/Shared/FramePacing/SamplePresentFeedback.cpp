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

#include <FslBase/Log/Log3Fmt.hpp>
#include <Shared/FramePacing/SamplePresentFeedback.hpp>
#include <algorithm>
#include <cstdlib>

namespace Fsl
{
  void SamplePresentFeedback::AddFrame(const uint64_t presentId, const TickCount cpuStartTime,
                                       const std::optional<TickCount> intendedDisplayTime) noexcept
  {
    if (presentId == 0u)
    {
      return;
    }
    FrameRecord& rFrame = m_frames[presentId % FrameCapacity];
    rFrame = {};
    rFrame.PresentId = presentId;
    rFrame.CpuStartTime = cpuStartTime;
    rFrame.IntendedDisplayTime = intendedDisplayTime;
  }


  void SamplePresentFeedback::AddPresentTiming(const uint64_t presentId, const std::optional<TickCount> displayTime,
                                               const std::optional<TickCount> queueOperationsEndTime)
  {
    FrameRecord* const pFrame = TryGetFrame(presentId);
    if (pFrame == nullptr || pFrame->IsMeasured)
    {
      return;
    }
    pFrame->IsMeasured = true;
    pFrame->DisplayTime = displayTime;
    pFrame->QueueOperationsEndTime = queueOperationsEndTime;
    ++m_totalMeasuredFrames;
    if (queueOperationsEndTime.has_value())
    {
      ++m_totalQueueFrames;
      m_totalQueueTicks += (queueOperationsEndTime.value() - pFrame->CpuStartTime).Ticks();
    }
    // Counted from when the CPU started on the frame (-1 = not available)
    FSLLOG3_VERBOSE4("Present feedback: id {} handed over {:.3f} ms, displayed {:.3f} ms, aimed for {:.3f} ms", presentId,
                     queueOperationsEndTime.has_value() ? (queueOperationsEndTime.value() - pFrame->CpuStartTime).TotalMilliseconds() : -1.0,
                     displayTime.has_value() ? (displayTime.value() - pFrame->CpuStartTime).TotalMilliseconds() : -1.0,
                     pFrame->IntendedDisplayTime.has_value() ? (pFrame->IntendedDisplayTime.value() - pFrame->CpuStartTime).TotalMilliseconds()
                                                             : -1.0);
    if (!displayTime.has_value())
    {
      // The presentation engine has no display time for the frame
      return;
    }

    ++m_totalTimedFrames;
    m_lastLatency = displayTime.value() - pFrame->CpuStartTime;
    m_totalLatencyTicks += m_lastLatency.value().Ticks();

    m_lastDisplayError.reset();
    if (pFrame->IntendedDisplayTime.has_value())
    {
      const TimeSpan displayError = displayTime.value() - pFrame->IntendedDisplayTime.value();
      m_lastDisplayError = displayError;

      ++m_totalPacedFrames;
      m_totalDisplayErrorTicks += displayError.Ticks();
      const int64_t absErrorTicks = std::llabs(displayError.Ticks());
      m_totalAbsDisplayErrorTicks += absErrorTicks;
      m_worstAbsDisplayErrorTicks = std::max(m_worstAbsDisplayErrorTicks, absErrorTicks);
    }
  }


  void SamplePresentFeedback::AddGpuInterval(const uint64_t presentId, const TickCount gpuStartTime, const TickCount gpuEndTime) noexcept
  {
    const FrameRecord* const pFrame = TryGetFrame(presentId);
    if (pFrame == nullptr)
    {
      return;
    }
    m_lastGpuStart = gpuStartTime - pFrame->CpuStartTime;
    m_lastGpuEnd = gpuEndTime - pFrame->CpuStartTime;
  }


  SamplePresentFeedbackStats SamplePresentFeedback::CalcStats() const
  {
    SamplePresentFeedbackStats stats;
    int64_t displayErrorTicks = 0;
    int64_t worstDisplayErrorTicks = 0;
    int64_t latencyTicks = 0;
    int64_t queueTicks = 0;
    uint32_t queueFrames = 0;
    int64_t intervalTicks = 0;
    int64_t minIntervalTicks = 0;
    int64_t maxIntervalTicks = 0;
    uint32_t intervals = 0;
    for (const FrameRecord& frame : m_frames)
    {
      if (frame.PresentId == 0u || !frame.IsMeasured)
      {
        continue;
      }
      ++stats.MeasuredFrames;
      if (frame.QueueOperationsEndTime.has_value())
      {
        queueTicks += (frame.QueueOperationsEndTime.value() - frame.CpuStartTime).Ticks();
        ++queueFrames;
      }
      if (!frame.DisplayTime.has_value())
      {
        continue;
      }
      ++stats.TimedFrames;
      latencyTicks += (frame.DisplayTime.value() - frame.CpuStartTime).Ticks();

      // The interval to the frame before it, if that has a display time as well
      const FrameRecord& previousFrame = m_frames[(frame.PresentId - 1u) % FrameCapacity];
      if (previousFrame.PresentId == (frame.PresentId - 1u) && previousFrame.PresentId != 0u && previousFrame.IsMeasured &&
          previousFrame.DisplayTime.has_value())
      {
        const int64_t ticks = (frame.DisplayTime.value() - previousFrame.DisplayTime.value()).Ticks();
        minIntervalTicks = intervals == 0u ? ticks : std::min(minIntervalTicks, ticks);
        maxIntervalTicks = intervals == 0u ? ticks : std::max(maxIntervalTicks, ticks);
        intervalTicks += ticks;
        ++intervals;
      }

      if (frame.IntendedDisplayTime.has_value())
      {
        ++stats.PacedFrames;
        const int64_t errorTicks = (frame.DisplayTime.value() - frame.IntendedDisplayTime.value()).Ticks();
        displayErrorTicks += errorTicks;
        if (std::llabs(errorTicks) > std::llabs(worstDisplayErrorTicks))
        {
          worstDisplayErrorTicks = errorTicks;
        }
      }
    }
    if (stats.TimedFrames > 0u)
    {
      stats.AverageLatency = TimeSpan(latencyTicks / static_cast<int64_t>(stats.TimedFrames));
    }
    if (queueFrames > 0u)
    {
      stats.AverageQueueTime = TimeSpan(queueTicks / static_cast<int64_t>(queueFrames));
    }
    if (intervals > 0u)
    {
      stats.AverageDisplayInterval = TimeSpan(intervalTicks / static_cast<int64_t>(intervals));
      stats.MinDisplayInterval = TimeSpan(minIntervalTicks);
      stats.MaxDisplayInterval = TimeSpan(maxIntervalTicks);
    }
    if (stats.PacedFrames > 0u)
    {
      stats.AverageDisplayError = TimeSpan(displayErrorTicks / static_cast<int64_t>(stats.PacedFrames));
      stats.WorstDisplayError = TimeSpan(worstDisplayErrorTicks);
    }
    return stats;
  }


  void SamplePresentFeedback::LogSummary() const noexcept
  {
    if (m_totalMeasuredFrames == 0u)
    {
      return;
    }
    FSLLOG3_VERBOSE("Present feedback: {} presents measured, {} with a display time", m_totalMeasuredFrames, m_totalTimedFrames);
    if (m_totalTimedFrames > 0u)
    {
      FSLLOG3_VERBOSE("Present feedback: average latency (CPU start to display) {:.3f} ms",
                      TimeSpan(m_totalLatencyTicks / static_cast<int64_t>(m_totalTimedFrames)).TotalMilliseconds());
    }
    if (m_totalQueueFrames > 0u)
    {
      FSLLOG3_VERBOSE("Present feedback: average hand over (CPU start to the present being handed to the presentation engine) {:.3f} ms",
                      TimeSpan(m_totalQueueTicks / static_cast<int64_t>(m_totalQueueFrames)).TotalMilliseconds());
    }
    if (m_totalPacedFrames > 0u)
    {
      const auto pacedFrames = static_cast<int64_t>(m_totalPacedFrames);
      FSLLOG3_VERBOSE("Present feedback: {} paced frames, display error average {:.3f} ms, average size {:.3f} ms, worst {:.3f} ms",
                      m_totalPacedFrames, TimeSpan(m_totalDisplayErrorTicks / pacedFrames).TotalMilliseconds(),
                      TimeSpan(m_totalAbsDisplayErrorTicks / pacedFrames).TotalMilliseconds(),
                      TimeSpan(m_worstAbsDisplayErrorTicks).TotalMilliseconds());
    }
  }


  SamplePresentFeedback::FrameRecord* SamplePresentFeedback::TryGetFrame(const uint64_t presentId) noexcept
  {
    if (presentId == 0u)
    {
      return nullptr;
    }
    FrameRecord& rFrame = m_frames[presentId % FrameCapacity];
    // The slot belongs to a newer frame if the frame is older than what is remembered
    return rFrame.PresentId == presentId ? &rFrame : nullptr;
  }
}
