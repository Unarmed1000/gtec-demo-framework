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


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUGpuTimeCalibration, ToHostTimeAtRate)
{
  const VUCalibratedTimestamp calibration = CreateCalibration(1000, 5000);

  EXPECT_EQ(TickCount(5000), VUGpuTimeCalibration::ToHostTimeAtRate(VUDeviceTimestamp(1000), calibration, 1.0, AllBits));
  EXPECT_EQ(TickCount(5100), VUGpuTimeCalibration::ToHostTimeAtRate(VUDeviceTimestamp(1100), calibration, 1.0, AllBits));
  // A device clock whose counts take a little longer than a tick
  EXPECT_EQ(TickCount(5000 + 1000022), VUGpuTimeCalibration::ToHostTimeAtRate(VUDeviceTimestamp(1000 + 1000000), calibration, 1.000022, AllBits));
  EXPECT_EQ(TickCount(5000 - 1000022), VUGpuTimeCalibration::ToHostTimeAtRate(VUDeviceTimestamp(1000 - 1000000), calibration, 1.000022, Mask36));
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUGpuTimeCalibration, AddCalibration_ConvertsFromTheNewest)
{
  VUGpuTimeCalibration calibration(VUCalibratedTimestamps(), PeriodOneTick, AllBits);
  EXPECT_EQ(0u, calibration.GetCalibrationCount());

  calibration.AddCalibration(CreateCalibration(1000, 5000));
  EXPECT_EQ(1u, calibration.GetCalibrationCount());
  TickCount hostTime;
  ASSERT_TRUE(calibration.TryToHostTime(VUDeviceTimestamp(1010), hostTime));
  EXPECT_EQ(TickCount(5010), hostTime);

  // The clocks moved three ticks apart: a timestamp is measured from the newest read
  calibration.AddCalibration(CreateCalibration(2000, 6003));
  EXPECT_EQ(2u, calibration.GetCalibrationCount());
  EXPECT_EQ(TickCount(6003), calibration.GetCalibration().HostTime);
  ASSERT_TRUE(calibration.TryToHostTime(VUDeviceTimestamp(2010), hostTime));
  EXPECT_EQ(TickCount(6013), hostTime);
  // And one from before it as well
  ASSERT_TRUE(calibration.TryToHostTime(VUDeviceTimestamp(1990), hostTime));
  EXPECT_EQ(TickCount(5993), hostTime);
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUGpuTimeCalibration, AddCalibration_NoRateBeforeTheReadsAreFarApart)
{
  VUGpuTimeCalibration calibration(VUCalibratedTimestamps(), PeriodOneTick, AllBits);

  // One second apart, the device clock 22 parts per million slow
  calibration.AddCalibration(CreateCalibration(0, 1000));
  const auto deviceOneSecond = static_cast<uint64_t>(TimeSpan::TicksPerSecond - 220);
  calibration.AddCalibration(CreateCalibration(deviceOneSecond, 1000 + TimeSpan::TicksPerSecond));

  EXPECT_FALSE(calibration.HasMeasuredClockRate());
  EXPECT_DOUBLE_EQ(0.0, calibration.GetClockRateDeviationPpm());
  // The period the device states is used
  TickCount hostTime;
  ASSERT_TRUE(calibration.TryToHostTime(VUDeviceTimestamp(deviceOneSecond + 10000000u), hostTime));
  EXPECT_EQ(TickCount(1000 + TimeSpan::TicksPerSecond + 10000000), hostTime);
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUGpuTimeCalibration, AddCalibration_MeasuresTheRateOfTheDeviceClock)
{
  VUGpuTimeCalibration calibration(VUCalibratedTimestamps(), PeriodOneTick, AllBits);

  // Two seconds apart, the device clock 22 parts per million slow: it counts 440 less than the framework clock ticks
  const int64_t twoSeconds = 2 * TimeSpan::TicksPerSecond;
  calibration.AddCalibration(CreateCalibration(0, 1000));
  calibration.AddCalibration(CreateCalibration(static_cast<uint64_t>(twoSeconds - 440), 1000 + twoSeconds));

  EXPECT_TRUE(calibration.HasMeasuredClockRate());
  EXPECT_NEAR(22.0, calibration.GetClockRateDeviationPpm(), 0.01);

  // One second of device counts after the newest read is 220 ticks more than the stated period gives
  TickCount hostTime;
  ASSERT_TRUE(calibration.TryToHostTime(VUDeviceTimestamp(static_cast<uint64_t>(twoSeconds - 440) + 10000000), hostTime));
  EXPECT_EQ(TickCount(1000 + twoSeconds + 10000220), hostTime);
  // At the read itself nothing changes
  ASSERT_TRUE(calibration.TryToHostTime(VUDeviceTimestamp(static_cast<uint64_t>(twoSeconds - 440)), hostTime));
  EXPECT_EQ(TickCount(1000 + twoSeconds), hostTime);
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUGpuTimeCalibration, AddCalibration_RateFromReadsInBetweenIsNotLost)
{
  VUGpuTimeCalibration calibration(VUCalibratedTimestamps(), PeriodOneTick, AllBits);

  // Four reads a second, the device clock 22 parts per million slow: 55 counts less per read
  const int64_t quarterSecond = TimeSpan::TicksPerSecond / 4;
  for (int64_t i = 0; i < 8; ++i)
  {
    calibration.AddCalibration(CreateCalibration(static_cast<uint64_t>(i * (quarterSecond - 55)), 1000 + (i * quarterSecond)));
    EXPECT_FALSE(calibration.HasMeasuredClockRate()) << "read " << i;
  }
  // The ninth read is two seconds after the first
  calibration.AddCalibration(CreateCalibration(static_cast<uint64_t>(8 * (quarterSecond - 55)), 1000 + (8 * quarterSecond)));

  EXPECT_TRUE(calibration.HasMeasuredClockRate());
  EXPECT_NEAR(22.0, calibration.GetClockRateDeviationPpm(), 0.01);
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUGpuTimeCalibration, AddCalibration_ARateThatCanNotBeRightIsNotUsed)
{
  VUGpuTimeCalibration calibration(VUCalibratedTimestamps(), PeriodOneTick, AllBits);
  const int64_t twoSeconds = 2 * TimeSpan::TicksPerSecond;

  // The device clock counted half of what the framework clock ticked: one of the two reads is wrong
  calibration.AddCalibration(CreateCalibration(0, 1000));
  calibration.AddCalibration(CreateCalibration(static_cast<uint64_t>(twoSeconds / 2), 1000 + twoSeconds));
  EXPECT_FALSE(calibration.HasMeasuredClockRate());

  // The stated period is still used, from the newest read
  TickCount hostTime;
  ASSERT_TRUE(calibration.TryToHostTime(VUDeviceTimestamp(static_cast<uint64_t>(twoSeconds / 2) + 500), hostTime));
  EXPECT_EQ(TickCount(1000 + twoSeconds + 500), hostTime);

  // And the next rate is measured from that read on, not from the first one
  calibration.AddCalibration(CreateCalibration(static_cast<uint64_t>((twoSeconds / 2) + twoSeconds - 440), 1000 + (2 * twoSeconds)));
  EXPECT_TRUE(calibration.HasMeasuredClockRate());
  EXPECT_NEAR(22.0, calibration.GetClockRateDeviationPpm(), 0.01);
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUGpuTimeCalibration, CalibrateIfOlderThan_NotSupported)
{
  VUGpuTimeCalibration calibration(VUCalibratedTimestamps(), PeriodOneTick, AllBits);

  // Nothing to read the clocks with
  EXPECT_FALSE(calibration.CalibrateIfOlderThan(TimeSpan()));
  EXPECT_EQ(0u, calibration.GetCalibrationCount());
  EXPECT_EQ(TimeSpan(), calibration.GetLastReadTime());
}
