#ifndef SHARED_FRAMEPACING_SAMPLEMEASUREDPRESENTS_HPP
#define SHARED_FRAMEPACING_SAMPLEMEASUREDPRESENTS_HPP
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
#include <Shared/FramePacing/SampleMeasuredPresentsStats.hpp>
#include <array>
#include <cstdint>
#include <optional>

namespace Fsl
{
  //! Relates the measured presents of an app (VK_EXT_present_timing) to the frames of the sample: when did a frame reach the display, how far
  //! was that from the time the frame pacer aimed for, and how long did it take from the start of the frame.
  //!
  //! A measurement arrives a few frames after its present, so the frames are remembered by the id of their present. All times are
  //! HighResolutionTimer timestamps.
  class SampleMeasuredPresents final
  {
    struct FrameRecord
    {
      //! Zero = unused
      uint64_t PresentId{0};
      TickCount CpuStartTime;
      std::optional<TickCount> IntendedDisplayTime;
      bool IsMeasured{false};
      std::optional<TickCount> DisplayTime;
      std::optional<TickCount> QueueOperationsEndTime;
    };

    //! The frames that are remembered, a measurement of a older frame is ignored
    static constexpr uint32_t FrameCapacity = 128;

    std::array<FrameRecord, FrameCapacity> m_frames{};
    //! How long the CPU start to the GPU start and to the GPU end was for the last frame the app measured
    std::optional<TimeSpan> m_lastGpuStart;
    std::optional<TimeSpan> m_lastGpuEnd;
    std::optional<TimeSpan> m_lastDisplayError;
    std::optional<TimeSpan> m_lastLatency;
    //! What was measured since the start, for the summary
    uint64_t m_totalMeasuredFrames{0};
    uint64_t m_totalTimedFrames{0};
    uint64_t m_totalPacedFrames{0};
    int64_t m_totalDisplayErrorTicks{0};
    int64_t m_totalAbsDisplayErrorTicks{0};
    int64_t m_worstAbsDisplayErrorTicks{0};
    int64_t m_totalLatencyTicks{0};
    uint64_t m_totalQueueFrames{0};
    int64_t m_totalQueueTicks{0};

  public:
    //! @brief Remember a frame.
    //! @param presentId the id of the present of the frame (not zero).
    //! @param cpuStartTime when the CPU started on the frame.
    //! @param intendedDisplayTime when the frame pacer aims to have the frame on the display (empty if the frame is not paced).
    void AddFrame(const uint64_t presentId, const TickCount cpuStartTime, const std::optional<TickCount> intendedDisplayTime) noexcept;

    //! @brief The present of a frame was measured.
    //! @param displayTime when the frame reached the display (empty if the presentation engine did not report it).
    //! @param queueOperationsEndTime when the present was handed to the presentation engine (empty if not reported).
    void AddPresentTiming(const uint64_t presentId, const std::optional<TickCount> displayTime,
                          const std::optional<TickCount> queueOperationsEndTime);

    //! @brief The GPU work of a frame was measured.
    void AddGpuInterval(const uint64_t presentId, const TickCount gpuStartTime, const TickCount gpuEndTime) noexcept;

    //! @return true if a present was measured.
    [[nodiscard]] bool HasMeasurements() const noexcept
    {
      return m_totalMeasuredFrames > 0u;
    }

    //! @brief Calculate what the remembered frames say.
    [[nodiscard]] SampleMeasuredPresentsStats CalcStats() const;

    //! The last measured frame: the time from 'the frame pacer aimed for' to 'reached the display' (empty if it was not paced or not timed)
    [[nodiscard]] std::optional<TimeSpan> GetLastDisplayError() const noexcept
    {
      return m_lastDisplayError;
    }

    //! The last measured frame: the time from 'the CPU started on the frame' to 'reached the display' (empty if it was not timed)
    [[nodiscard]] std::optional<TimeSpan> GetLastLatency() const noexcept
    {
      return m_lastLatency;
    }

    //! The last frame the GPU work was measured for: the time from 'the CPU started on the frame' to the GPU starting and finishing it
    [[nodiscard]] std::optional<TimeSpan> GetLastGpuStart() const noexcept
    {
      return m_lastGpuStart;
    }

    [[nodiscard]] std::optional<TimeSpan> GetLastGpuEnd() const noexcept
    {
      return m_lastGpuEnd;
    }

    //! @brief Forget the last GPU interval (the app stopped measuring it).
    void ClearGpuInterval() noexcept
    {
      m_lastGpuStart.reset();
      m_lastGpuEnd.reset();
    }

    //! @brief Forget the remembered frames (the app stopped measuring the presents). What was measured since the start is kept.
    void ClearFrames() noexcept
    {
      m_frames = {};
      m_lastDisplayError.reset();
      m_lastLatency.reset();
    }

    //! @brief Write what was measured since the start to the log (verbose), nothing if nothing was measured.
    void LogSummary() const noexcept;

  private:
    [[nodiscard]] FrameRecord* TryGetFrame(const uint64_t presentId) noexcept;
  };
}

#endif
