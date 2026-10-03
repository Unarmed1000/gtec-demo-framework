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
#include <FslBase/Time/TimeSpan.hpp>
#include <FslUtil/Vulkan1_0/UnitTest/Helper/Common.hpp>
#include <FslUtil/Vulkan1_0/UnitTest/Helper/TestFixtureFslUtil_Vulkan1_0.hpp>
#include <FslUtil/Vulkan1_0/VUGpuTimeCalibration.hpp>
#include <limits>

using namespace Fsl;
using namespace Fsl::Vulkan;

namespace
{
  using TestFixtureFslUtil_Vulkan1_0_VUGpuTimeCalibration = TestFixtureFslUtil_Vulkan1_0;

  //! A device timestamp that counts in ticks of the framework clock
  constexpr double PeriodOneTick = 100.0;
  constexpr uint64_t AllBits = std::numeric_limits<uint64_t>::max();
  //! A device clock with 36 valid bits
  constexpr uint64_t Mask36 = (uint64_t{1} << 36u) - 1u;

  VUCalibratedTimestamp CreateCalibration(const uint64_t deviceTimestamp, const int64_t hostTicks)
  {
    VUCalibratedTimestamp calibration;
    calibration.DeviceTimestamp = VUDeviceTimestamp(deviceTimestamp);
    calibration.HostTime = TickCount(hostTicks);
    return calibration;
  }
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUGpuTimeCalibration, Default)
{
  VUGpuTimeCalibration calibration;

  EXPECT_FALSE(calibration.IsSupported());
  EXPECT_FALSE(calibration.Calibrate());

  TickCount hostTime(42);
  EXPECT_FALSE(calibration.TryToHostTime(VUDeviceTimestamp(1000), hostTime));
  EXPECT_EQ(TickCount(), hostTime);
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUGpuTimeCalibration, NotSupported_WithoutCalibratedTimestamps)
{
  // A device with timestamps, but nothing to relate them to the host clock with
  VUGpuTimeCalibration calibration(VUCalibratedTimestamps(), 1.0, AllBits);

  EXPECT_FALSE(calibration.IsSupported());
  EXPECT_FALSE(calibration.Calibrate());

  TickCount hostTime;
  EXPECT_FALSE(calibration.TryToHostTime(VUDeviceTimestamp(1000), hostTime));
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUGpuTimeCalibration, ToHostTime_AtTheCalibration)
{
  const VUCalibratedTimestamp calibration = CreateCalibration(1000, 5000);

  EXPECT_EQ(TickCount(5000), VUGpuTimeCalibration::ToHostTime(VUDeviceTimestamp(1000), calibration, PeriodOneTick, AllBits));
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUGpuTimeCalibration, ToHostTime_AfterAndBeforeTheCalibration)
{
  const VUCalibratedTimestamp calibration = CreateCalibration(1000, 5000);

  EXPECT_EQ(TickCount(5010), VUGpuTimeCalibration::ToHostTime(VUDeviceTimestamp(1010), calibration, PeriodOneTick, AllBits));
  // A timestamp query that was written before the clocks were read
  EXPECT_EQ(TickCount(4990), VUGpuTimeCalibration::ToHostTime(VUDeviceTimestamp(990), calibration, PeriodOneTick, AllBits));
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUGpuTimeCalibration, ToHostTime_Period)
{
  const VUCalibratedTimestamp calibration = CreateCalibration(1000, 5000);

  // One nanosecond per count: 300 counts are three ticks
  EXPECT_EQ(TickCount(5003), VUGpuTimeCalibration::ToHostTime(VUDeviceTimestamp(1300), calibration, 1.0, AllBits));
  EXPECT_EQ(TickCount(4997), VUGpuTimeCalibration::ToHostTime(VUDeviceTimestamp(700), calibration, 1.0, AllBits));
  // Half a nanosecond per count
  EXPECT_EQ(TickCount(5002), VUGpuTimeCalibration::ToHostTime(VUDeviceTimestamp(1400), calibration, 0.5, AllBits));
  // A millisecond per count
  EXPECT_EQ(TickCount(5000 + 20000), VUGpuTimeCalibration::ToHostTime(VUDeviceTimestamp(1002), calibration, 1000000.0, AllBits));
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUGpuTimeCalibration, ToHostTime_WrapAround_ValidBits)
{
  {    // The device clock wrapped around between the calibration and the timestamp
    const VUCalibratedTimestamp calibration = CreateCalibration(Mask36 - 5u, 5000);
    EXPECT_EQ(TickCount(5016), VUGpuTimeCalibration::ToHostTime(VUDeviceTimestamp(10), calibration, PeriodOneTick, Mask36));
  }
  {    // The device clock wrapped around between the timestamp and the calibration
    const VUCalibratedTimestamp calibration = CreateCalibration(10, 5000);
    EXPECT_EQ(TickCount(4984), VUGpuTimeCalibration::ToHostTime(VUDeviceTimestamp(Mask36 - 5u), calibration, PeriodOneTick, Mask36));
  }
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUGpuTimeCalibration, ToHostTime_WrapAround_AllBits)
{
  {
    const VUCalibratedTimestamp calibration = CreateCalibration(AllBits - 5u, 5000);
    EXPECT_EQ(TickCount(5016), VUGpuTimeCalibration::ToHostTime(VUDeviceTimestamp(10), calibration, PeriodOneTick, AllBits));
  }
  {
    const VUCalibratedTimestamp calibration = CreateCalibration(10, 5000);
    EXPECT_EQ(TickCount(4984), VUGpuTimeCalibration::ToHostTime(VUDeviceTimestamp(AllBits - 5u), calibration, PeriodOneTick, AllBits));
  }
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUGpuTimeCalibration, ToHostTime_NoMaskIsAllBits)
{
  const VUCalibratedTimestamp calibration = CreateCalibration(AllBits - 5u, 5000);

  EXPECT_EQ(VUGpuTimeCalibration::ToHostTime(VUDeviceTimestamp(10), calibration, PeriodOneTick, AllBits),
            VUGpuTimeCalibration::ToHostTime(VUDeviceTimestamp(10), calibration, PeriodOneTick, 0u));
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUGpuTimeCalibration, ToHostTime_BitsOutsideTheMaskAreIgnored)
{
  const VUCalibratedTimestamp calibration = CreateCalibration(1000, 5000);

  // A driver is free to leave anything in the bits that are not valid
  EXPECT_EQ(TickCount(5010),
            VUGpuTimeCalibration::ToHostTime(VUDeviceTimestamp(1010u | (uint64_t{0xABC} << 40u)), calibration, PeriodOneTick, Mask36));
}
