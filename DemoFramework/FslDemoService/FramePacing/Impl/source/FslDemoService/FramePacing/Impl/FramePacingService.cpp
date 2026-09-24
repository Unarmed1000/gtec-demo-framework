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
#include <FslDemoApp/Base/FrameInfo.hpp>
#include <FslDemoService/FramePacing/Impl/FramePacingService.hpp>
#include <FslDemoService/FramePacing/Impl/FramePacingServiceOptionParser.hpp>
#include <mb/framemarker/FrameMarker.hpp>
#include <algorithm>
#include <chrono>
#include <limits>
#include <string_view>

namespace Fsl
{
  namespace
  {
    int32_t ClampModuleSize(const int32_t moduleSizePx) noexcept
    {
      return std::clamp(moduleSizePx, MB::FrameMarker::MinModuleSizePx, MB::FrameMarker::MaxModuleSizePx);
    }
  }


  FramePacingService::FramePacingService(const ServiceProvider& serviceProvider, const std::shared_ptr<FramePacingServiceOptionParser>& optionParser)
    : ThreadLocalService(serviceProvider)
    , m_random(std::random_device{}())
    , m_enabled(optionParser->IsEnabled())
    , m_slot(optionParser->GetSlot())
    , m_moduleSizePx(ClampModuleSize(optionParser->GetModuleSizePx()))
    , m_captureHeightPx(std::max(optionParser->GetCaptureHeightPx(), 0))
    , m_nextRunId(optionParser->GetRunId())
  {
    if (optionParser->GetRunName().has_value())
    {
      m_pendingRun = PendingRun{optionParser->GetRunName().value(), optionParser->GetRunDuration()};
    }
    m_runId = m_nextRunId.has_value() ? m_nextRunId.value() : CreateRunId();
  }


  FramePacingService::~FramePacingService() = default;


  bool FramePacingService::IsEnabled() const noexcept
  {
    return m_enabled;
  }


  void FramePacingService::SetEnabled(const bool enabled) noexcept
  {
    m_enabled = enabled;
  }


  FramePacingMarkerSlot FramePacingService::GetSlot() const noexcept
  {
    return m_slot;
  }


  void FramePacingService::SetSlot(const FramePacingMarkerSlot slot) noexcept
  {
    m_slot = slot;
  }


  int32_t FramePacingService::GetModuleSizePx() const noexcept
  {
    return m_moduleSizePx;
  }


  void FramePacingService::SetModuleSizePx(const int32_t moduleSizePx) noexcept
  {
    m_moduleSizePx = ClampModuleSize(moduleSizePx);
  }


  int32_t FramePacingService::GetCaptureHeightPx() const noexcept
  {
    return m_captureHeightPx;
  }


  void FramePacingService::SetCaptureHeightPx(const int32_t captureHeightPx) noexcept
  {
    m_captureHeightPx = std::max(captureHeightPx, 0);
  }


  bool FramePacingService::BeginRun(const StringViewLite name, const TimeSpan duration)
  {
    if (name.size() > MaxRunNameBytes)
    {
      FSLLOG3_WARNING("FramePacing: run name is longer than {} bytes", MaxRunNameBytes);
      return false;
    }
    if (!m_sequence.BeginRun(duration))
    {
      FSLLOG3_WARNING("FramePacing: a run is already active");
      return false;
    }
    // A explicitly requested run id is only used for the first run
    m_runId = m_nextRunId.has_value() ? m_nextRunId.value() : CreateRunId();
    m_nextRunId.reset();
    m_runName = std::string(std::string_view(name));
    m_runStartUtcTicks = MB::FrameMarker::ToDateTimeTicks(std::chrono::system_clock::now());
    m_enabled = true;
    // A command line run is superseded by any explicit run
    m_pendingRun.reset();
    FSLLOG3_INFO("FramePacing: run '{}' (id {}) started", m_runName, m_runId);
    return true;
  }


  void FramePacingService::EndRun() noexcept
  {
    m_sequence.EndRun();
  }


  FramePacingRunState FramePacingService::GetRunState() const noexcept
  {
    return m_sequence.GetState();
  }


  uint32_t FramePacingService::GetRunId() const noexcept
  {
    return m_runId;
  }


  TimeSpan FramePacingService::GetRunDuration() const noexcept
  {
    return m_sequence.GetMeasureDuration();
  }


  TimeSpan FramePacingService::GetRunMeasuredTime() const noexcept
  {
    return m_sequence.GetMeasuredTime(m_timer.GetTimestamp());
  }


  void FramePacingService::BeginFrame(const FrameInfo& frameInfo)
  {
    if (m_pendingRun.has_value())
    {
      const PendingRun pendingRun = m_pendingRun.value();
      BeginRun(StringViewLite(pendingRun.Name), pendingRun.Duration);
    }

    const FramePacingRunState oldState = m_sequence.GetState();
    m_frameKind = m_sequence.Advance(m_timer.GetTimestamp());
    if (oldState != FramePacingRunState::Idle && m_sequence.GetState() == FramePacingRunState::Idle)
    {
      FSLLOG3_INFO("FramePacing: run '{}' (id {}) completed", m_runName, m_runId);
    }

    m_frameIndex = m_frameCount;
    ++m_frameCount;
    // The animation time the app uses, TickCount is in 100ns ticks exactly like the marker format.
    m_frameAnimationTicks = frameInfo.Time.CurrentTickCount.Ticks();
    m_hasFrame = true;
  }


  bool FramePacingService::TryGetFrameRecord(FramePacingFrameRecord& rRecord) const noexcept
  {
    if (!m_enabled || !m_hasFrame)
    {
      return false;
    }
    rRecord.Kind = m_frameKind;
    rRecord.FrameIndex = m_frameIndex;
    rRecord.AnimationTicks = m_frameAnimationTicks;
    rRecord.RunId = m_runId;
    rRecord.StartUtcTicks = m_runStartUtcTicks;
    rRecord.RunName = StringViewLite(m_runName);
    rRecord.Slot = m_slot;
    rRecord.ModuleSizePx = m_moduleSizePx;
    rRecord.CaptureHeightPx = m_captureHeightPx;
    return true;
  }


  uint32_t FramePacingService::CreateRunId()
  {
    std::uniform_int_distribution<uint32_t> distribution(1u, std::numeric_limits<uint32_t>::max());
    return distribution(m_random);
  }
}
