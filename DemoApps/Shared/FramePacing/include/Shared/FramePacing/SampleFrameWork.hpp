#ifndef SHARED_FRAMEPACING_SAMPLEFRAMEWORK_HPP
#define SHARED_FRAMEPACING_SAMPLEFRAMEWORK_HPP
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
#include <optional>

namespace Fsl
{
  //! What a frame cost, once everything that is measured about it is known. The three are values of their own that are counted from
  //! the start of the frame, not parts of a sum: the CPU and the GPU each work for a time, and the frame takes until the last of
  //! them is done.
  struct SampleFrameWorkRecord
  {
    //! How long the CPU worked on the frame
    TimeSpan CpuTime;
    //! How long the GPU worked on the frame (zero: it was not measured)
    TimeSpan GpuTime;
    //! From the start of the frame to the end of the last work on it: the later of the end of the CPU's work and the end of the
    //! GPU's work
    TimeSpan FrameTime;

    constexpr bool operator==(const SampleFrameWorkRecord&) const noexcept = default;
  };

  //! Puts what is measured about the work on a frame together. The CPU's work is known when the frame ends, the GPU's work a
  //! frame or more later (a app does not wait for the GPU to measure it), and a frame that was not measured gets nothing. So a
  //! frame is held here until its GPU time has come, or until it is clear that none will: a later frame got its GPU time, or
  //! MaxWaitFrames frames have started since.
  //!
  //! How long a frame took needs the time the GPU was done with it. Where a app only knows how long the GPU worked, the GPU is
  //! taken to have started when the CPU's work ended, which is the earliest it can have.
  class SampleFrameWork final
  {
  public:
    //! The frames that can be held
    static constexpr std::size_t Capacity = 8;
    //! The frames that can start after a frame before it is given up on
    static constexpr uint64_t MaxWaitFrames = 4;

  private:
    struct Frame
    {
      uint64_t FrameId{0};
      TickCount StartTime;
      TickCount CpuEndTime;
      TimeSpan GpuTime;
      std::optional<TickCount> GpuEndTime;
      bool CpuWorkEnded{false};
      bool HasGpuTime{false};
    };

    std::array<Frame, Capacity> m_frames{};
    //! The index of the oldest frame that is held and the number of frames that are held
    std::size_t m_first{0};
    std::size_t m_count{0};
    //! The newest frame that was started and the newest frame a GPU time came for
    uint64_t m_newestFrameId{0};
    uint64_t m_newestGpuFrameId{0};

  public:
    //! @brief Forget every frame
    void Clear() noexcept;

    //! @brief A frame started. The frames are numbered by the caller, each number larger than the one before.
    void BeginFrame(const uint64_t frameId, const TickCount startTime) noexcept;

    //! @brief The CPU's work on the frame that started last is done.
    void EndCpuWork(const TickCount cpuEndTime) noexcept;

    //! @brief How long the GPU worked on a frame, and when it was done with it if the app knows.
    void AddGpuTime(const uint64_t frameId, const TimeSpan gpuTime, const std::optional<TickCount> gpuEndTime) noexcept;

    //! @brief When the GPU worked on a frame.
    void AddGpuInterval(const uint64_t frameId, const TickCount gpuBeginTime, const TickCount gpuEndTime) noexcept;

    //! @brief Take the oldest frame everything is known about. The frames come in the order they started.
    //! @return false if the oldest frame still waits for something
    bool TryPop(SampleFrameWorkRecord& rRecord) noexcept;

  private:
    [[nodiscard]] Frame* TryFind(const uint64_t frameId) noexcept;
  };
}

#endif
