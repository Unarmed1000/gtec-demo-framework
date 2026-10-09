#ifndef SHARED_FRAMEPACING_SAMPLEFRAMEWORKAVERAGE_HPP
#define SHARED_FRAMEPACING_SAMPLEFRAMEWORKAVERAGE_HPP
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

#include <FslBase/Time/TimeSpan.hpp>
#include <Shared/FramePacing/SampleFrameWork.hpp>
#include <array>
#include <cstddef>
#include <cstdint>

namespace Fsl
{
  //! The average of what the last frames cost (SampleFrameWorkRecord), for the legend of the work chart.
  //! A frame the GPU time was not measured for (zero) is left out of the average of the GPU time.
  class SampleFrameWorkAverage final
  {
  public:
    //! The frames the average is of, at the most
    static constexpr std::size_t Capacity = 240;

  private:
    std::array<SampleFrameWorkRecord, Capacity> m_records{};
    std::size_t m_next{0};
    std::size_t m_count{0};

  public:
    //! @brief Forget the frames
    void Clear() noexcept
    {
      m_next = 0;
      m_count = 0;
    }

    //! @brief Add a frame, the oldest one makes room when there are Capacity of them
    void Add(const SampleFrameWorkRecord& record) noexcept
    {
      m_records[m_next] = record;
      m_next = (m_next + 1u) % Capacity;
      if (m_count < Capacity)
      {
        ++m_count;
      }
    }

    //! @brief The frames the average is of
    [[nodiscard]] std::size_t Count() const noexcept
    {
      return m_count;
    }

    //! @brief The average of the frames (all zero without a frame; the GPU time zero if it was measured for none of them)
    [[nodiscard]] SampleFrameWorkRecord CalcAverage() const noexcept
    {
      SampleFrameWorkRecord average;
      if (m_count == 0u)
      {
        return average;
      }
      int64_t cpuTicks = 0;
      int64_t gpuTicks = 0;
      int64_t frameTicks = 0;
      int64_t gpuFrames = 0;
      for (std::size_t i = 0; i < m_count; ++i)
      {
        const SampleFrameWorkRecord& record = m_records[i];
        cpuTicks += record.CpuTime.Ticks();
        frameTicks += record.FrameTime.Ticks();
        if (record.GpuTime.Ticks() > 0)
        {
          gpuTicks += record.GpuTime.Ticks();
          ++gpuFrames;
        }
      }
      const auto count = static_cast<int64_t>(m_count);
      average.CpuTime = TimeSpan(cpuTicks / count);
      average.FrameTime = TimeSpan(frameTicks / count);
      average.GpuTime = TimeSpan(gpuFrames > 0 ? (gpuTicks / gpuFrames) : 0);
      return average;
    }
  };
}

#endif
