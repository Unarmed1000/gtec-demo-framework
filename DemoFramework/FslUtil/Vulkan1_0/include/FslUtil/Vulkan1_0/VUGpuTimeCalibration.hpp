#ifndef FSLUTIL_VULKAN1_0_VUGPUTIMECALIBRATION_HPP
#define FSLUTIL_VULKAN1_0_VUGPUTIMECALIBRATION_HPP
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

// Make sure Common.hpp is the first include file (to make the error message as helpful as possible when disabled)
#include <FslBase/Time/TickCount.hpp>
#include <FslUtil/Vulkan1_0/Common.hpp>
#include <FslUtil/Vulkan1_0/VUCalibratedTimestamps.hpp>
#include <FslUtil/Vulkan1_0/VUDeviceTimestamp.hpp>
#include <cstdint>

namespace Fsl::Vulkan
{
  //! Converts the timestamps of a device (timestamp queries, VUGpuFrameTimer) to the clock of the framework, so the work of the GPU can be
  //! placed on the same timeline as the work of the CPU.
  //!
  //! It keeps one moment that was read on both clocks and measures from there, so call Calibrate now and then to keep the two clocks from
  //! drifting apart (once a second is plenty).
  class VUGpuTimeCalibration final
  {
    VUCalibratedTimestamps m_calibratedTimestamps;
    double m_timestampPeriod{0.0};
    uint64_t m_timestampMask{0};
    VUCalibratedTimestamp m_calibration;
    bool m_hasCalibration{false};

  public:
    VUGpuTimeCalibration() = default;

    //! @param timestampPeriod the number of nanoseconds a device timestamp counts in (VkPhysicalDeviceLimits::timestampPeriod).
    //! @param timestampMask the valid bits of a device timestamp (from the timestampValidBits of the queue family).
    VUGpuTimeCalibration(const VUCalibratedTimestamps& calibratedTimestamps, const double timestampPeriod, const uint64_t timestampMask);

    //! @return true if device timestamps can be converted (it needs VK_KHR_calibrated_timestamps and a device with timestamps).
    [[nodiscard]] bool IsSupported() const noexcept;

    //! @brief Read both clocks again.
    //! @return true if TryToHostTime can be used.
    bool Calibrate() noexcept;

    //! @brief Convert a device timestamp to the clock of the framework (comparable with HighResolutionTimer::GetTimestamp).
    //! @return false if not supported or not calibrated yet.
    [[nodiscard]] bool TryToHostTime(const VUDeviceTimestamp timestamp, TickCount& rHostTime) const noexcept;

    //! @brief Convert a device timestamp to the clock of the framework given a moment that was read on both clocks.
    //! @param calibration the moment that was read on both clocks, the timestamp is expected to be within half the range of the device clock
    //!        of it.
    //! @param timestampPeriod the number of nanoseconds a device timestamp counts in.
    //! @param timestampMask the valid bits of a device timestamp (all bits if zero).
    [[nodiscard]] static TickCount ToHostTime(const VUDeviceTimestamp timestamp, const VUCalibratedTimestamp& calibration,
                                              const double timestampPeriod, const uint64_t timestampMask) noexcept;
  };
}

#endif
