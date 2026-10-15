#ifndef FSLDEMOSERVICE_FRAMEPACINGMARKER_IFRAMEPACINGDETAILSERVICE_HPP
#define FSLDEMOSERVICE_FRAMEPACINGMARKER_IFRAMEPACINGDETAILSERVICE_HPP
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
#include <FslDemoService/FramePacingMarker/FramePacingFrameId.hpp>
#include <FslDemoService/FramePacingMarker/FramePacingPacerEnvironment.hpp>
#include <FslDemoService/FramePacingMarker/FramePacingPresentRecord.hpp>
#include <FslDemoService/FramePacingMarker/FramePacingSwapIntervalChangedBy.hpp>
#include <FslDemoService/FramePacingMarker/FramePacingSwapchainEnvironment.hpp>
#include <FslDemoService/FramePacingMarker/FramePacingWaitKind.hpp>
#include <cstdint>
#include <optional>
#include <string_view>

namespace Fsl
{
  //! What happened around each frame, written to the trace (--Trace) as the second part of the frame record of mb-framepacing
  //! (https://github.com/Unarmed1000/mb-framepacing, sdk/doc/trace-frame-record.md): the waits before a frame, its present, the GPU's
  //! work on it, when the platform says it was shown, and what the run runs on. Its tools read it beside a capture.
  //!
  //! A frame is named by the ids its marker carries, or would carry when no marker is drawn (GetFrameId), so every frame has its details.
  //! What is not known is left out. Everything here does nothing while the trace is off.
  //! The service is only available on platforms that support the marker library, so use TryGet<IFramePacingDetailService>().
  class IFramePacingDetailService
  {
  public:
    virtual ~IFramePacingDetailService() = default;

    //! @brief Check if the details are written (the trace is on). Use it to skip the work of finding a detail out.
    [[nodiscard]] virtual bool IsTraceEnabled() const noexcept = 0;

    //! @brief The ids of the frame that is being drawn. Keep them for a detail that is only known later (a display time).
    [[nodiscard]] virtual FramePacingFrameId GetFrameId() const noexcept = 0;

    //! @brief A wait before the CPU work of the frame, from its begin to its end (HighResolutionTimer timestamps).
    //! @param plannedTime the time it waited for, where it had one.
    virtual void AddWait(const FramePacingFrameId frameId, const FramePacingWaitKind kind, const TickCount beginTime, const TickCount endTime,
                         const std::optional<TickCount> plannedTime) noexcept = 0;

    //! @brief The present call of the frame.
    virtual void AddPresent(const FramePacingFrameId frameId, const FramePacingPresentRecord& record) noexcept = 0;

    //! @brief The GPU's work on the frame, from its begin to its end on the clock of the framework (HighResolutionTimer timestamps).
    virtual void AddGpuWork(const FramePacingFrameId frameId, const TickCount beginTime, const TickCount endTime) noexcept = 0;

    //! @brief When the platform says the frame was shown: the platform's word, not a measurement.
    //! @param source where the time comes from, in the platform's words.
    virtual void AddDisplayTime(const FramePacingFrameId frameId, const TickCount displayTime, const std::string_view source) noexcept = 0;

    //! @brief The frame was made and never presented. The time is when that was known.
    virtual void AddNotPresented(const FramePacingFrameId frameId, const TickCount time) noexcept = 0;

    //! @brief The swapchain was made anew, now.
    //! @param reason why, in a few words.
    virtual void AddSwapchainRecreated(const std::string_view reason) noexcept = 0;

    //! @brief The swap interval the presents are made with from now on. A change of it is written as a moment of the run, and every
    //!        present carries the one it was made with.
    virtual void SetSwapInterval(const uint32_t swapInterval, const FramePacingSwapIntervalChangedBy changedBy) noexcept = 0;

    //! @brief What the frames are shown with, from the one that presents them. It is written when a run starts and when it changes, with
    //!        the environment of the pacer.
    virtual void SetSwapchainEnvironment(const FramePacingSwapchainEnvironment& environment) = 0;

    //! @brief How the frames are paced, from the application. It is written when a run starts and when it changes, with the environment
    //!        of the swapchain.
    virtual void SetPacerEnvironment(const FramePacingPacerEnvironment& environment) = 0;
  };
}

#endif
