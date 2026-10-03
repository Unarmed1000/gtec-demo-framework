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
  VUGpuTimeCalibration::VUGpuTimeCalibration(const VUCalibratedTimestamps& calibratedTimestamps, const double timestampPeriod,
                                             const uint64_t timestampMask)
    : m_calibratedTimestamps(calibratedTimestamps)
    , m_timestampPeriod(timestampPeriod)
    , m_timestampMask(timestampMask)
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
    if (m_calibratedTimestamps.TryGet(calibration))
    {
      m_calibration = calibration;
      m_hasCalibration = true;
    }
    // A failed read keeps the previous calibration
    return m_hasCalibration;
  }


  bool VUGpuTimeCalibration::TryToHostTime(const VUDeviceTimestamp timestamp, TickCount& rHostTime) const noexcept
  {
    if (!m_hasCalibration)
    {
      rHostTime = {};
      return false;
    }
    rHostTime = ToHostTime(timestamp, m_calibration, m_timestampPeriod, m_timestampMask);
    return true;
  }


  TickCount VUGpuTimeCalibration::ToHostTime(const VUDeviceTimestamp timestamp, const VUCalibratedTimestamp& calibration,
                                             const double timestampPeriod, const uint64_t timestampMask) noexcept
  {
    const uint64_t mask = timestampMask != 0u ? timestampMask : std::numeric_limits<uint64_t>::max();
    // The device clock wraps around at its valid bits, so the distance is taken within them and can be before or after the calibration
    const uint64_t distance = (timestamp.Value - calibration.DeviceTimestamp.Value) & mask;
    const uint64_t signBit = (mask >> 1u) + 1u;
    const double signedDistance = (distance & signBit) != 0u ? -static_cast<double>(((~distance) & mask) + 1u) : static_cast<double>(distance);

    const int64_t ticks = std::llround((signedDistance * timestampPeriod) / static_cast<double>(TickCount::NanoSecondsPerTick));
    return calibration.HostTime + TimeSpan(ticks);
  }
}
