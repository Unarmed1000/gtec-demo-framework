#ifndef FSLDEMOSERVICE_FRAMEPACING_IMPL_FRAMEPACINGSERVICE_HPP
#define FSLDEMOSERVICE_FRAMEPACING_IMPL_FRAMEPACINGSERVICE_HPP
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

#include <FslBase/System/HighResolutionTimer.hpp>
#include <FslDemoService/FramePacing/IFramePacingService.hpp>
#include <FslDemoService/FramePacing/Impl/FramePacingSequence.hpp>
#include <FslDemoService/FramePacing/Impl/IFramePacingServiceControl.hpp>
#include <FslService/Impl/ServiceType/Local/ThreadLocalService.hpp>
#include <memory>
#include <optional>
#include <random>
#include <string>

namespace Fsl
{
  class FramePacingServiceOptionParser;

  class FramePacingService final
    : public ThreadLocalService
    , public IFramePacingService
    , public IFramePacingServiceControl
  {
    struct PendingRun
    {
      std::string Name;
      TimeSpan Duration;
    };

    HighResolutionTimer m_timer;
    std::mt19937 m_random;
    FramePacingSequence m_sequence;

    bool m_enabled;
    FramePacingMarkerSlot m_slot;
    int32_t m_moduleSizePx;
    int32_t m_captureHeightPx;

    //! A run requested on the command line, it is started at the first frame
    std::optional<PendingRun> m_pendingRun;
    std::optional<uint32_t> m_nextRunId;

    uint32_t m_runId{0};
    std::string m_runName;
    int64_t m_runStartUtcTicks{0};

    //! The number of frames that were started so far
    uint64_t m_frameCount{0};
    bool m_hasFrame{false};
    FramePacingMarkerKind m_frameKind{FramePacingMarkerKind::Frame};
    uint64_t m_frameIndex{0};
    int64_t m_frameAnimationTicks{0};

  public:
    FramePacingService(const ServiceProvider& serviceProvider, const std::shared_ptr<FramePacingServiceOptionParser>& optionParser);
    ~FramePacingService() final;

    // From IFramePacingService
    bool IsEnabled() const noexcept final;
    void SetEnabled(const bool enabled) noexcept final;
    FramePacingMarkerSlot GetSlot() const noexcept final;
    void SetSlot(const FramePacingMarkerSlot slot) noexcept final;
    int32_t GetModuleSizePx() const noexcept final;
    void SetModuleSizePx(const int32_t moduleSizePx) noexcept final;
    int32_t GetCaptureHeightPx() const noexcept final;
    void SetCaptureHeightPx(const int32_t captureHeightPx) noexcept final;
    bool BeginRun(const StringViewLite name, const TimeSpan duration) final;
    void EndRun() noexcept final;
    FramePacingRunState GetRunState() const noexcept final;
    uint32_t GetRunId() const noexcept final;
    TimeSpan GetRunDuration() const noexcept final;
    TimeSpan GetRunMeasuredTime() const noexcept final;

    // From IFramePacingServiceControl
    void BeginFrame(const FrameInfo& frameInfo) final;
    bool TryGetFrameRecord(FramePacingFrameRecord& rRecord) const noexcept final;

  private:
    uint32_t CreateRunId();
  };
}

#endif
