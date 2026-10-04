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

// Include gtest before anything that includes the polluted standard X11 header that cause all kind of issues withs its bad defines.
#include <gtest/gtest.h>
// Then include the rest
#include <FslBase/System/HighResolutionTimer.hpp>
#include <FslBase/Time/TickCount.hpp>
#include <FslBase/Time/TimeSpan.hpp>
#include <FslUtil/Vulkan1_0/Util/PhysicalDeviceUtil.hpp>
#include <FslUtil/Vulkan1_0/VUCalibratedTimestamps.hpp>
#include <FslUtil/Vulkan1_0/VUDevice.hpp>
#include <stdexcept>
#include "Util/TestFixtureFslUtil_Vulkan1_0_PhysicalDevice.hpp"

using namespace Fsl;
using namespace Fsl::Vulkan;

namespace
{
  using TestFixtureFslUtil_Vulkan1_0_VUCalibratedTimestamps = TestFixtureFslUtil_Vulkan1_0_PhysicalDevice;

  //! Creates a device with one queue and the given extensions
  VUDevice CreateDevice(const VkPhysicalDevice physicalDevice, const uint32_t extensionCount, const char* const* ppszExtensions)
  {
    const float queuePriority = 0.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = extensionCount;
    deviceCreateInfo.ppEnabledExtensionNames = ppszExtensions;
    return {physicalDevice, deviceCreateInfo};
  }
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUCalibratedTimestamps, Default)
{
  const VUCalibratedTimestamps calibratedTimestamps;

  EXPECT_FALSE(calibratedTimestamps.IsSupported());
  EXPECT_FALSE(calibratedTimestamps.HasEntryPoint());

  VUCalibratedTimestamp timestamp;
  timestamp.DeviceTimestamp = VUDeviceTimestamp(42);
  timestamp.HostTime = TickCount(42);
  EXPECT_FALSE(calibratedTimestamps.TryGet(timestamp));
  EXPECT_EQ(VUDeviceTimestamp(), timestamp.DeviceTimestamp);
  EXPECT_EQ(TickCount(), timestamp.HostTime);
}

#ifdef FSL_VULKAN_CALIBRATED_TIMESTAMPS_SUPPORTED

TEST_F(TestFixtureFslUtil_Vulkan1_0_VUCalibratedTimestamps, Construct_NullHandles)
{
  EXPECT_THROW(VUCalibratedTimestamps(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE), std::invalid_argument);
}


// A device that was created without the extension has no entry point for it, which is 'not supported' and not a failure
TEST_F(TestFixtureFslUtil_Vulkan1_0_VUCalibratedTimestamps, Construct_ExtensionNotEnabled)
{
  if (!IsReady())
  {
    SkipTest("Construct_ExtensionNotEnabled");
    return;
  }
  const VUDevice device = CreateDevice(m_physicalDevice, 0, nullptr);

  const VUCalibratedTimestamps calibratedTimestamps(m_instance.Get(), m_physicalDevice, device.Get());

  EXPECT_FALSE(calibratedTimestamps.IsSupported());
  EXPECT_FALSE(calibratedTimestamps.HasEntryPoint());
  VUCalibratedTimestamp timestamp;
  EXPECT_FALSE(calibratedTimestamps.TryGet(timestamp));
  EXPECT_EQ(VK_ERROR_EXTENSION_NOT_PRESENT, calibratedTimestamps.GetTimestamps(0, nullptr, nullptr, nullptr));
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUCalibratedTimestamps, TryGet_HostTimeIsTheFrameworkClock)
{
  if (!IsReady())
  {
    SkipTest("TryGet_HostTimeIsTheFrameworkClock");
    return;
  }
  const char* pszExtensionName = VK_KHR_CALIBRATED_TIMESTAMPS_EXTENSION_NAME;
  if (!PhysicalDeviceUtil::IsDeviceExtensionsAvailable(m_physicalDevice, 1, &pszExtensionName))
  {
    // The EXT version has the same entry points under another name
    pszExtensionName = "VK_EXT_calibrated_timestamps";
    if (!PhysicalDeviceUtil::IsDeviceExtensionsAvailable(m_physicalDevice, 1, &pszExtensionName))
    {
      SkipTest("TryGet_HostTimeIsTheFrameworkClock", "calibrated timestamps are not supported");
      return;
    }
  }
  const VUDevice device = CreateDevice(m_physicalDevice, 1, &pszExtensionName);

  const VUCalibratedTimestamps calibratedTimestamps(m_instance.Get(), m_physicalDevice, device.Get());
  ASSERT_TRUE(calibratedTimestamps.HasEntryPoint());
  if (!calibratedTimestamps.IsSupported())
  {
    SkipTest("TryGet_HostTimeIsTheFrameworkClock", "the device clock or the host clock can not be calibrated");
    return;
  }

  // The host time has to be a time HighResolutionTimer could have returned between the two reads around it.
  // The conversions round, so allow a little.
  const TimeSpan tolerance = TimeSpan::FromMilliseconds(1);
  const HighResolutionTimer timer;

  const TickCount before = timer.GetTimestamp();
  VUCalibratedTimestamp first;
  ASSERT_TRUE(calibratedTimestamps.TryGet(first));
  VUCalibratedTimestamp second;
  ASSERT_TRUE(calibratedTimestamps.TryGet(second));
  const TickCount after = timer.GetTimestamp();

  EXPECT_GE(first.HostTime, before - tolerance);
  EXPECT_LE(first.HostTime, after + tolerance);
  EXPECT_GE(second.HostTime, before - tolerance);
  EXPECT_LE(second.HostTime, after + tolerance);
  // Both clocks go forward
  EXPECT_GE(second.HostTime, first.HostTime);
  EXPECT_NE(first.DeviceTimestamp, second.DeviceTimestamp);
  EXPECT_GE(first.MaxDeviation, TimeSpan());
  EXPECT_LT(first.MaxDeviation, TimeSpan::FromMilliseconds(100));
}

#endif
