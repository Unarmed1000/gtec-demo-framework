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
#include <FslUtil/Vulkan1_0/Debug/VUDeviceFault.hpp>
#include <FslUtil/Vulkan1_0/Util/PhysicalDeviceUtil.hpp>
#include <FslUtil/Vulkan1_0/VUDevice.hpp>
#include <stdexcept>
#include "../Util/TestFixtureFslUtil_Vulkan1_0_PhysicalDevice.hpp"

using namespace Fsl;
using namespace Fsl::Vulkan;

namespace
{
  using TestFixtureFslUtil_Vulkan1_0_VUDeviceFault = TestFixtureFslUtil_Vulkan1_0_PhysicalDevice;

  VkDeviceCreateInfo CreateDeviceCreateInfo(const VkDeviceQueueCreateInfo& queueCreateInfo)
  {
    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    return deviceCreateInfo;
  }
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUDeviceFault, Default)
{
  const VUDeviceFault deviceFault;

  EXPECT_FALSE(deviceFault.IsSupported());
  EXPECT_EQ(VUDeviceFaultApi::Disabled, deviceFault.GetApi());
  EXPECT_EQ(0u, deviceFault.LogFaults());

  const VUDeviceFaultReport report = deviceFault.GetReport(TimeSpan());
  EXPECT_EQ(VK_ERROR_EXTENSION_NOT_PRESENT, report.Result);
  EXPECT_TRUE(report.Faults.empty());
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUDeviceFault, Construct_NullDevice)
{
  EXPECT_THROW(VUDeviceFault(VK_NULL_HANDLE, VUDeviceFaultApi::Khr), std::invalid_argument);
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUDeviceFault, Construct_Disabled)
{
  if (!IsReady())
  {
    SkipTest("Construct_Disabled");
    return;
  }

  const float queuePriority = 0.0f;
  VkDeviceQueueCreateInfo queueCreateInfo{};
  queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queueCreateInfo.queueCount = 1;
  queueCreateInfo.pQueuePriorities = &queuePriority;
  const VUDevice device(m_physicalDevice, CreateDeviceCreateInfo(queueCreateInfo));

  const VUDeviceFault deviceFault(device.Get(), VUDeviceFaultApi::Disabled);

  EXPECT_FALSE(deviceFault.IsSupported());
  EXPECT_EQ(0u, deviceFault.LogFaults());
}


// VK_KHR_device_fault can be queried while the device is healthy, which the EXT version does not allow (VUID-vkGetDeviceFaultInfoEXT-device-07336).
// So only the KHR path can be exercised without losing a device.
TEST_F(TestFixtureFslUtil_Vulkan1_0_VUDeviceFault, GetReport_Khr_HealthyDeviceHasNoFaults)
{
  if (!IsReady())
  {
    SkipTest("GetReport_Khr_HealthyDeviceHasNoFaults");
    return;
  }
#ifdef VK_KHR_device_fault
  const char* const pszExtensionName = VK_KHR_DEVICE_FAULT_EXTENSION_NAME;
  VkPhysicalDeviceFaultFeaturesKHR faultFeatures{};
  faultFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_KHR;
  if (PhysicalDeviceUtil::IsDeviceExtensionsAvailable(m_physicalDevice, 1, &pszExtensionName))
  {
    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &faultFeatures;
    vkGetPhysicalDeviceFeatures2(m_physicalDevice, &features2);
  }
  if (faultFeatures.deviceFault != VK_TRUE)
  {
    SkipTest("GetReport_Khr_HealthyDeviceHasNoFaults", "VK_KHR_device_fault is not supported");
    return;
  }

  // Enable just the fault reporting
  VkPhysicalDeviceFaultFeaturesKHR enabledFeatures{};
  enabledFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_KHR;
  enabledFeatures.deviceFault = VK_TRUE;

  const float queuePriority = 0.0f;
  VkDeviceQueueCreateInfo queueCreateInfo{};
  queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queueCreateInfo.queueCount = 1;
  queueCreateInfo.pQueuePriorities = &queuePriority;

  VkDeviceCreateInfo deviceCreateInfo = CreateDeviceCreateInfo(queueCreateInfo);
  deviceCreateInfo.pNext = &enabledFeatures;
  deviceCreateInfo.enabledExtensionCount = 1;
  deviceCreateInfo.ppEnabledExtensionNames = &pszExtensionName;
  const VUDevice device(m_physicalDevice, deviceCreateInfo);

  const VUDeviceFault deviceFault(device.Get(), VUDeviceFaultApi::Khr);
  ASSERT_TRUE(deviceFault.IsSupported());
  EXPECT_EQ(VUDeviceFaultApi::Khr, deviceFault.GetApi());

  // Nothing went wrong, so there is nothing to report and nothing to wait for
  const VUDeviceFaultReport report = deviceFault.GetReport(TimeSpan());
  EXPECT_TRUE(report.Result == VK_SUCCESS || report.Result == VK_TIMEOUT) << static_cast<int32_t>(report.Result);
  EXPECT_TRUE(report.Faults.empty());
#else
  SkipTest("GetReport_Khr_HealthyDeviceHasNoFaults", "built with Vulkan headers that lack VK_KHR_device_fault");
#endif
}
