#ifndef SHARED_FRAMEPACING_SAMPLEPRESENTLOG_HPP
#define SHARED_FRAMEPACING_SAMPLEPRESENTLOG_HPP
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

#include <FslBase/IO/Path.hpp>
#include <FslBase/Time/TickCount.hpp>
#include <FslBase/Time/TimeSpan.hpp>
#include <Shared/FramePacing/SamplePacer.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Fsl
{
  //! What is known about a frame when it starts (see SamplePresentLog)
  struct SamplePresentLogFrame
  {
    //! The id of the present of the frame (not zero)
    uint64_t PresentId{0};
    //! When the CPU started on the frame
    TickCount CpuStartTime;
    //! The time the frame is animated for
    TimeSpan AnimationTime;
    //! The duration of a refresh of the display according to the swapchain (zero if unknown)
    TimeSpan RefreshDuration;
    //! What the frame pacer planned for the frame (empty if the frame is not paced)
    std::optional<SamplePacerSchedule> Schedule;
  };


  //! Writes one row per present to a CSV file, so the frame loop of the sample can be looked at afterwards: where it waited, what the
  //! frame pacer planned and when the frames reached the display.
  //!
  //! Every time is a HighResolutionTimer timestamp in whole 100ns ticks. A field is empty if the value is not available. The rows are
  //! kept in memory and written when the log is saved, so a frame costs no file access. The columns are:
  //! frameIndex (the frame index of the frame pacing marker), presentId, cpuStartTicks, endFrameTicks, acquireCallTicks,
  //! acquireReturnTicks, presentCallTicks, presentReturnTicks, queueOperationsEndTicks, firstPixelOutTicks, refreshDurationNs, swapInterval,
  //! intendedDisplayTicks, animationTimeTicks, change, imageIndex, resultReadAtPresentId (the present of the frame in which the measurement
  //! of this present was read, so how late it arrived).
  class SamplePresentLog final
  {
    struct Row
    {
      uint64_t PresentId{0};
      std::optional<uint64_t> FrameIndex;
      TickCount CpuStartTime;
      std::optional<TickCount> EndFrameTime;
      std::optional<TickCount> AcquireCallTime;
      std::optional<TickCount> AcquireReturnTime;
      std::optional<TickCount> PresentCallTime;
      std::optional<TickCount> PresentReturnTime;
      std::optional<TickCount> QueueOperationsEndTime;
      std::optional<TickCount> DisplayTime;
      TimeSpan RefreshDuration;
      TimeSpan AnimationTime;
      std::optional<SamplePacerSchedule> Schedule;
      std::optional<uint32_t> ImageIndex;
      std::optional<uint64_t> ResultReadAtPresentId;
    };

    IO::Path m_path;
    std::vector<Row> m_rows;
    std::size_t m_maxRows{0};

  public:
    //! The frames a log holds, the frames after that are not logged
    static constexpr std::size_t DefaultMaxRows = 200000;

    //! A log that is switched off
    SamplePresentLog() = default;

    //! @param path the file to write (a empty path switches the log off)
    explicit SamplePresentLog(IO::Path path, const std::size_t maxRows = DefaultMaxRows);

    [[nodiscard]] bool IsEnabled() const noexcept
    {
      return m_maxRows > 0u;
    }

    [[nodiscard]] std::size_t GetRowCount() const noexcept
    {
      return m_rows.size();
    }

    //! @brief A frame started. The frames must be added in the order of their present ids.
    void AddFrame(const SamplePresentLogFrame& frame);

    //! @brief The frame index the frame pacing marker of the frame carried.
    void SetFrameIndex(const uint64_t presentId, const uint64_t frameIndex) noexcept;

    //! @brief The work of the frame was done (what the frame pacer is told with EndFrame).
    void SetEndFrameTime(const uint64_t presentId, const TickCount endFrameTime) noexcept;

    //! @brief When the swapchain was called for the frame.
    void SetPresentCalls(const uint64_t presentId, const uint32_t imageIndex, const TickCount acquireCallTime, const TickCount acquireReturnTime,
                         const TickCount presentCallTime, const TickCount presentReturnTime) noexcept;

    //! @brief The present of the frame was measured.
    //! @param readAtPresentId the id of the present of the frame in which the measurement was read.
    void SetPresentTiming(const uint64_t presentId, const std::optional<TickCount> queueOperationsEndTime, const std::optional<TickCount> displayTime,
                          const uint64_t readAtPresentId) noexcept;

    //! @brief The rows as the content of the CSV file.
    [[nodiscard]] std::string ToCsv() const;

    //! @brief Write the file. A failure is logged, as this is called when the sample shuts down.
    //! @return true if the file was written.
    bool TrySave() const noexcept;

  private:
    [[nodiscard]] Row* TryGetRow(const uint64_t presentId) noexcept;
  };
}

#endif
