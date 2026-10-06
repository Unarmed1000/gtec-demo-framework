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
#include <FslUtil/Vulkan1_0/VUGpuTimeCalibration.hpp>
#include <cmath>
#include <limits>

namespace Fsl::Vulkan
{
  namespace
  {
    namespace LocalConfig
    {
      //! The rate of the device clock is measured from two reads that are at least this far apart. A read is a few microseconds
      //! uncertain, so over two seconds the rate is known to a few parts per million.
      constexpr TimeSpan MinRateBaseline = TimeSpan::FromSeconds(2);
      //! A measured rate that is further than this from the period the device states is not a rate: one of the reads was wrong
      constexpr double MaxRateDeviation = 0.001;
    }

    //! The counts from one device timestamp to another, within the valid bits of the device clock, so it can be before or after
    double SignedDistance(const VUDeviceTimestamp to, const VUDeviceTimestamp from, const uint64_t timestampMask) noexcept
    {
      const uint64_t mask = timestampMask != 0u ? timestampMask : std::numeric_limits<uint64_t>::max();
      const uint64_t distance = (to.Value - from.Value) & mask;
      const uint64_t signBit = (mask >> 1u) + 1u;
      return (distance & signBit) != 0u ? -static_cast<double>(((~distance) & mask) + 1u) : static_cast<double>(distance);
    }
  }


  VUGpuTimeCalibration::VUGpuTimeCalibration(const VUCalibratedTimestamps& calibratedTimestamps, const double timestampPeriod,
                                             const uint64_t timestampMask)
    : m_calibratedTimestamps(calibratedTimestamps)
    , m_timestampPeriod(timestampPeriod)
    , m_timestampMask(timestampMask)
    , m_hostTicksPerDeviceCount(timestampPeriod / static_cast<double>(TickCount::NanoSecondsPerTick))
  {
  }


  bool VUGpuTimeCalibration::IsSupported() const noexcept
  {
    return m_calibratedTimestamps.IsSupported() && m_timestampPeriod > 0.0 && m_timestampMask != 0u;
  }


  bool VUGpuTimeCalibration::Calibrate() noexcept
  {
    if (!IsSupported())
    {
      m_hasCalibration = false;
      return false;
    }
    VUCalibratedTimestamp calibration;
    const TickCount readBeginTime = m_timer.GetTimestamp();
    if (m_calibratedTimestamps.TryGet(calibration))
    {
      m_lastReadTime = m_timer.GetTimestamp() - readBeginTime;
      AddCalibration(calibration);
    }
    // A failed read keeps the previous calibration
    return m_hasCalibration;
  }


  bool VUGpuTimeCalibration::CalibrateIfOlderThan(const TimeSpan maxAge) noexcept
  {
    if (!IsSupported() || (m_hasCalibration && (m_timer.GetTimestamp() - m_calibration.HostTime) < maxAge))
    {
      return false;
    }
    const uint32_t calibrationCount = m_calibrationCount;
    Calibrate();
    return m_calibrationCount != calibrationCount;
  }


  void VUGpuTimeCalibration::AddCalibration(const VUCalibratedTimestamp& calibration) noexcept
  {
    if (!m_hasCalibration)
    {
      m_rateCalibration = calibration;
    }
    else if ((calibration.HostTime - m_rateCalibration.HostTime) >= LocalConfig::MinRateBaseline)
    {
      const double hostTicks = static_cast<double>((calibration.HostTime - m_rateCalibration.HostTime).Ticks());
      const double deviceCounts = SignedDistance(calibration.DeviceTimestamp, m_rateCalibration.DeviceTimestamp, m_timestampMask);
      const double statedRate = m_timestampPeriod / static_cast<double>(TickCount::NanoSecondsPerTick);
      if (deviceCounts > 0.0 && statedRate > 0.0)
      {
        const double measuredRate = hostTicks / deviceCounts;
        if (std::abs((measuredRate / statedRate) - 1.0) <= LocalConfig::MaxRateDeviation)
        {
          m_hostTicksPerDeviceCount = measuredRate;
          m_hasMeasuredRate = true;
        }
      }
      // Measured or not, the next rate is measured from here: a read that was wrong is not kept as the start of the next one
      m_rateCalibration = calibration;
    }
    m_calibration = calibration;
    m_hasCalibration = true;
    ++m_calibrationCount;
  }


  double VUGpuTimeCalibration::GetClockRateDeviationPpm() const noexcept
  {
    const double statedRate = m_timestampPeriod / static_cast<double>(TickCount::NanoSecondsPerTick);
    return (m_hasMeasuredRate && statedRate > 0.0) ? (((m_hostTicksPerDeviceCount / statedRate) - 1.0) * 1000000.0) : 0.0;
  }


  bool VUGpuTimeCalibration::TryToHostTime(const VUDeviceTimestamp timestamp, TickCount& rHostTime) const noexcept
  {
    if (!m_hasCalibration)
    {
      rHostTime = {};
      return false;
    }
    rHostTime = ToHostTimeAtRate(timestamp, m_calibration, m_hostTicksPerDeviceCount, m_timestampMask);
    return true;
  }


  TickCount VUGpuTimeCalibration::ToHostTime(const VUDeviceTimestamp timestamp, const VUCalibratedTimestamp& calibration,
                                             const double timestampPeriod, const uint64_t timestampMask) noexcept
  {
    return ToHostTimeAtRate(timestamp, calibration, timestampPeriod / static_cast<double>(TickCount::NanoSecondsPerTick), timestampMask);
  }


  TickCount VUGpuTimeCalibration::ToHostTimeAtRate(const VUDeviceTimestamp timestamp, const VUCalibratedTimestamp& calibration,
                                                   const double hostTicksPerDeviceCount, const uint64_t timestampMask) noexcept
  {
    // The device clock wraps around at its valid bits, so the distance is taken within them and can be before or after the calibration
    const double signedDistance = SignedDistance(timestamp, calibration.DeviceTimestamp, timestampMask);
    return calibration.HostTime + TimeSpan(std::llround(signedDistance * hostTicksPerDeviceCount));
  }
}
