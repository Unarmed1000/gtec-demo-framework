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
#include <FslBase/System/HighResolutionTimer.hpp>
#include <FslBase/Time/TickCount.hpp>
#include <FslBase/Time/TimeSpan.hpp>
#include <FslUtil/Vulkan1_0/Common.hpp>
#include <FslUtil/Vulkan1_0/VUCalibratedTimestamps.hpp>
#include <FslUtil/Vulkan1_0/VUDeviceTimestamp.hpp>
#include <cstdint>

namespace Fsl::Vulkan
{
  //! Converts the timestamps of a device (timestamp queries, VUGpuFrameTimer) to the clock of the framework, so the work of the GPU can be
  //! placed on the same timeline as the work of the CPU.
  //!
  //! A timestamp is converted from the newest moment that was read on both clocks. The two clocks do not run at the same rate (a
  //! device clock was measured to be 22 parts per million off the period it states, which is 22 microseconds for every second since
  //! the read), so the rate of the device clock is measured from two reads that are a while apart and used in place of the stated
  //! period. Call CalibrateIfOlderThan once per frame: a few reads per second keep what is left under the uncertainty of a read.
  class VUGpuTimeCalibration final
  {
    VUCalibratedTimestamps m_calibratedTimestamps;
    HighResolutionTimer m_timer;
    double m_timestampPeriod{0.0};
    uint64_t m_timestampMask{0};
    //! The newest moment that was read on both clocks: timestamps are converted from it
    VUCalibratedTimestamp m_calibration;
    //! The moment the rate of the device clock is measured from
    VUCalibratedTimestamp m_rateCalibration;
    //! How many ticks of the framework clock one count of the device clock takes: the stated one until it was measured
    double m_hostTicksPerDeviceCount{0.0};
    //! How long the last read of the two clocks took
    TimeSpan m_lastReadTime;
    uint32_t m_calibrationCount{0};
    bool m_hasCalibration{false};
    bool m_hasMeasuredRate{false};

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

    //! @brief Read both clocks again if the newest read is older than the given time (or there is none).
    //! @return true if the clocks were read and the read was used.
    bool CalibrateIfOlderThan(const TimeSpan maxAge) noexcept;

    //! @brief Use a moment that was read on both clocks, as Calibrate does with the one it reads. Timestamps are converted from it
    //!        from now on, and the rate of the device clock is measured again once it is far enough from the moment the rate was
    //!        last measured from.
    void AddCalibration(const VUCalibratedTimestamp& calibration) noexcept;

    //! @return how many moments were used (Calibrate, AddCalibration), so a caller can tell a new one.
    [[nodiscard]] uint32_t GetCalibrationCount() const noexcept
    {
      return m_calibrationCount;
    }

    //! @return the newest moment that was read on both clocks (default if none).
    [[nodiscard]] const VUCalibratedTimestamp& GetCalibration() const noexcept
    {
      return m_calibration;
    }

    //! @return how long the last read of the two clocks by Calibrate took.
    [[nodiscard]] TimeSpan GetLastReadTime() const noexcept
    {
      return m_lastReadTime;
    }

    //! @return true once the rate of the device clock was measured. Until then the period the device states is used.
    [[nodiscard]] bool HasMeasuredClockRate() const noexcept
    {
      return m_hasMeasuredRate;
    }

    //! @return how much longer (positive) or shorter a count of the device clock was measured to take than the period the device
    //!         states, in parts per million. Zero until it was measured.
    [[nodiscard]] double GetClockRateDeviationPpm() const noexcept;

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

    //! @brief The same with the rate of the device clock given as it was measured.
    //! @param hostTicksPerDeviceCount how many ticks of the framework clock one count of the device clock takes.
    [[nodiscard]] static TickCount ToHostTimeAtRate(const VUDeviceTimestamp timestamp, const VUCalibratedTimestamp& calibration,
                                                    const double hostTicksPerDeviceCount, const uint64_t timestampMask) noexcept;
  };
}

#endif
