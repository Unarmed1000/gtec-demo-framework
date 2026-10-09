#ifndef FSLDEMOSERVICE_FRAMEPACINGMARKER_IFRAMEPACINGMARKERSERVICE_HPP
#define FSLDEMOSERVICE_FRAMEPACINGMARKER_IFRAMEPACINGMARKERSERVICE_HPP
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

#include <FslBase/String/StringViewLite.hpp>
#include <FslBase/Time/TickCount.hpp>
#include <FslBase/Time/TimeSpan.hpp>
#include <FslDemoService/FramePacingMarker/FramePacingFrameSchedule.hpp>
#include <FslDemoService/FramePacingMarker/FramePacingMarkerInfo.hpp>
#include <FslDemoService/FramePacingMarker/FramePacingRunState.hpp>
#include <cstdint>

namespace Fsl
{
  //! Controls the mb-framepacing frame marker (https://github.com/Unarmed1000/mb-framepacing).
  //! When enabled the host draws a QR marker containing the frame index and the animation time on top of every frame, so a capture of the
  //! display output can be analysed for animation error.
  //! The service is only available on platforms that support the marker library, so use TryGet<IFramePacingMarkerService>().
  class IFramePacingMarkerService
  {
  public:
    //! The default size of one QR module in pixels.
    static constexpr int32_t DefaultModuleSizePx = 6;

    virtual ~IFramePacingMarkerService() = default;

    //! @brief Check if the marker is drawn.
    [[nodiscard]] virtual bool IsEnabled() const noexcept = 0;
    //! @brief Enable or disable drawing of the marker.
    virtual void SetEnabled(const bool enabled) noexcept = 0;

    //! @brief Check if the small sync marker is drawn at the bottom left (the main marker is always drawn at the top left).
    [[nodiscard]] virtual bool IsSyncMarkerEnabled() const noexcept = 0;
    //! @brief Enable or disable the sync marker. It carries the run id and the frame index so the analysis can detect tearing, camera
    //!        capture needs it for its timing.
    virtual void SetSyncMarkerEnabled(const bool enabled) noexcept = 0;

    //! @brief The size of one QR module in pixels (only used if the capture height is zero).
    [[nodiscard]] virtual int32_t GetModuleSizePx() const noexcept = 0;
    //! @brief Set the size of one QR module in pixels (clamped to a valid size).
    virtual void SetModuleSizePx(const int32_t moduleSizePx) noexcept = 0;

    //! @brief The height in pixels the capture is stored at (0 = unknown, the module size is used as is).
    [[nodiscard]] virtual int32_t GetCaptureHeightPx() const noexcept = 0;
    //! @brief Set the height in pixels the capture is stored at, when not zero the module size is calculated so the marker survives the
    //!        downscale of the capture.
    virtual void SetCaptureHeightPx(const int32_t captureHeightPx) noexcept = 0;

    //! @brief Begin a measured run: a start marker, then frame markers and finally an end marker.
    //! @param name the name of the run, it is only written to the log. The start marker identifies the run by a random sequence id
    //!             which is logged next to the name.
    //! @param duration the duration of the measured part, if zero the run lasts until EndRun is called.
    //! @return true if the run was started, false if a run is already active.
    //! @note This enables the marker.
    virtual bool BeginRun(const StringViewLite name, const TimeSpan duration) = 0;

    //! @brief End the active run (does nothing if no run is active).
    virtual void EndRun() noexcept = 0;

    //! @brief Get the run state.
    [[nodiscard]] virtual FramePacingRunState GetRunState() const noexcept = 0;

    //! @brief Get the id of the current (or last) run.
    [[nodiscard]] virtual uint32_t GetRunId() const noexcept = 0;

    //! @brief Get the duration of the measured part of the current (or last) run, zero if it lasts until EndRun is called.
    [[nodiscard]] virtual TimeSpan GetRunDuration() const noexcept = 0;

    //! @brief Get how long the current run has been measuring (zero unless the run state is Measuring).
    [[nodiscard]] virtual TimeSpan GetRunMeasuredTime() const noexcept = 0;

    //! @brief Get every value the last drawn marker carried.
    //! @return false if the marker is disabled or no marker has been drawn yet.
    virtual bool TryGetLastMarker(FramePacingMarkerInfo& rInfo) const noexcept = 0;

    //! @brief Supply the pacing values of the frame being drawn (for an app with its own frame pacer, the framework has none).
    //! @note Call it during the app's Draw, before the marker is drawn (on Vulkan before AddSystemUI). It applies to that frame only: a
    //!       frame without the call reports the animation time of the framework and no pacing values.
    virtual void SetFrameSchedule(const FramePacingFrameSchedule& schedule) noexcept = 0;

    //! @brief Get when the host swapped the buffers of the last frame: when it called the swap and when the swap returned
    //!        (HighResolutionTimer timestamps). For an app whose frames the host presents (OpenGL ES: eglSwapBuffers) and that paces
    //!        them itself: the swap is where its frame loop can wait for the display, and the app can not time it. A Vulkan app presents
    //!        its frames itself and has the times of its present (DemoAppVulkanBasic::GetLastPresentCalls).
    //! @return false if no frame was swapped yet.
    virtual bool TryGetLastSwapTimes(TickCount& rCallTime, TickCount& rReturnTime) const noexcept = 0;
  };
}

#endif
