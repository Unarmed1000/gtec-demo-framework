#ifndef FSLDEMOSERVICE_FRAMEPACING_IFRAMEPACINGSERVICE_HPP
#define FSLDEMOSERVICE_FRAMEPACING_IFRAMEPACINGSERVICE_HPP
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
#include <FslBase/Time/TimeSpan.hpp>
#include <FslDemoService/FramePacing/FramePacingMarkerSlot.hpp>
#include <FslDemoService/FramePacing/FramePacingRunState.hpp>
#include <cstddef>
#include <cstdint>

namespace Fsl
{
  //! Controls the mb-framepacing frame marker (https://github.com/Unarmed1000/mb-framepacing).
  //! When enabled the host draws a QR marker containing the frame index and the animation time on top of every frame, so a capture of the
  //! display output can be analysed for animation error.
  //! The service is only available on platforms that support the marker library, so use TryGet<IFramePacingService>().
  class IFramePacingService
  {
  public:
    //! The maximum length of a run name in UTF-8 bytes.
    static constexpr std::size_t MaxRunNameBytes = 64;
    //! The default size of one QR module in pixels.
    static constexpr int32_t DefaultModuleSizePx = 6;

    virtual ~IFramePacingService() = default;

    //! @brief Check if the marker is drawn.
    [[nodiscard]] virtual bool IsEnabled() const noexcept = 0;
    //! @brief Enable or disable drawing of the marker.
    virtual void SetEnabled(const bool enabled) noexcept = 0;

    //! @brief Get the slot the marker is drawn in.
    [[nodiscard]] virtual FramePacingMarkerSlot GetSlot() const noexcept = 0;
    //! @brief Set the slot the marker is drawn in.
    virtual void SetSlot(const FramePacingMarkerSlot slot) noexcept = 0;

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
    //! @param name the name of the run (at most MaxRunNameBytes UTF-8 bytes).
    //! @param duration the duration of the measured part, if zero the run lasts until EndRun is called.
    //! @return true if the run was started, false if a run is already active or the name is too long.
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
  };
}

#endif
