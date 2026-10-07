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


#include <Shared/FramePacing/SampleFrameWork.hpp>
#include <algorithm>

namespace Fsl
{
  void SampleFrameWork::Clear() noexcept
  {
    m_frames = {};
    m_first = 0;
    m_count = 0;
    m_newestFrameId = 0;
    m_newestGpuFrameId = 0;
  }


  void SampleFrameWork::BeginFrame(const uint64_t frameId, const TickCount startTime) noexcept
  {
    if (m_count == Capacity)
    {
      // Nobody takes the frames: the oldest one makes room
      m_first = (m_first + 1u) % Capacity;
      --m_count;
    }
    Frame& rFrame = m_frames[(m_first + m_count) % Capacity];
    rFrame = {};
    rFrame.FrameId = frameId;
    rFrame.StartTime = startTime;
    ++m_count;
    m_newestFrameId = frameId;
  }


  void SampleFrameWork::EndCpuWork(const TickCount cpuEndTime) noexcept
  {
    Frame* const pFrame = TryFind(m_newestFrameId);
    if (pFrame != nullptr)
    {
      pFrame->CpuEndTime = cpuEndTime;
      pFrame->CpuWorkEnded = true;
    }
  }


  void SampleFrameWork::AddGpuTime(const uint64_t frameId, const TimeSpan gpuTime, const std::optional<TickCount> gpuEndTime) noexcept
  {
    m_newestGpuFrameId = std::max(m_newestGpuFrameId, frameId);
    Frame* const pFrame = TryFind(frameId);
    if (pFrame != nullptr)
    {
      pFrame->GpuTime = TimeSpan(std::max(gpuTime.Ticks(), int64_t{0}));
      pFrame->HasGpuTime = true;
      if (gpuEndTime.has_value())
      {
        pFrame->GpuEndTime = gpuEndTime;
      }
    }
  }


  void SampleFrameWork::AddGpuInterval(const uint64_t frameId, const TickCount gpuBeginTime, const TickCount gpuEndTime) noexcept
  {
    m_newestGpuFrameId = std::max(m_newestGpuFrameId, frameId);
    Frame* const pFrame = TryFind(frameId);
    if (pFrame != nullptr)
    {
      if (!pFrame->HasGpuTime)
      {
        pFrame->GpuTime = TimeSpan(std::max((gpuEndTime - gpuBeginTime).Ticks(), int64_t{0}));
        pFrame->HasGpuTime = true;
      }
      pFrame->GpuEndTime = gpuEndTime;
    }
  }


  bool SampleFrameWork::TryPop(SampleFrameWorkRecord& rRecord) noexcept
  {
    if (m_count == 0u)
    {
      return false;
    }
    const Frame& frame = m_frames[m_first];
    if (!frame.CpuWorkEnded)
    {
      return false;
    }
    // The GPU times come in the order of the frames, so a later frame that has one says this frame was not measured
    const bool gaveUp = frame.FrameId < m_newestGpuFrameId || (m_newestFrameId - frame.FrameId) >= MaxWaitFrames;
    if (!frame.HasGpuTime && !gaveUp)
    {
      return false;
    }

    rRecord = {};
    rRecord.CpuTime = TimeSpan(std::max((frame.CpuEndTime - frame.StartTime).Ticks(), int64_t{0}));
    rRecord.GpuTime = frame.GpuTime;
    // The frame takes until the last work on it is done. Without a time for the end of the GPU's work the GPU is taken to have
    // started when the CPU's work ended.
    const TickCount gpuEndTime = frame.GpuEndTime.value_or(frame.CpuEndTime + frame.GpuTime);
    const TickCount lastEndTime = std::max(frame.CpuEndTime, gpuEndTime);
    rRecord.FrameTime = TimeSpan(std::max((lastEndTime - frame.StartTime).Ticks(), int64_t{0}));

    m_first = (m_first + 1u) % Capacity;
    --m_count;
    return true;
  }


  SampleFrameWork::Frame* SampleFrameWork::TryFind(const uint64_t frameId) noexcept
  {
    for (std::size_t i = 0; i < m_count; ++i)
    {
      Frame& rFrame = m_frames[(m_first + i) % Capacity];
      if (rFrame.FrameId == frameId)
      {
        return &rFrame;
      }
    }
    return nullptr;
  }
}
