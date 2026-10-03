#ifndef FSLUTIL_VULKAN1_0_UNITTEST_UTIL_TESTFIXTUREFSLUTIL_VULKAN1_0_PHYSICALDEVICE_HPP
#define FSLUTIL_VULKAN1_0_UNITTEST_UTIL_TESTFIXTUREFSLUTIL_VULKAN1_0_PHYSICALDEVICE_HPP
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

#include <FslBase/Exceptions.hpp>
#include <FslUtil/Vulkan1_0/UnitTest/Helper/Common.hpp>
#include <FslUtil/Vulkan1_0/UnitTest/Helper/TestFixtureFslUtil_Vulkan1_0.hpp>
#include <FslUtil/Vulkan1_0/Util/ApiVersionUtil.hpp>
#include <FslUtil/Vulkan1_0/Util/InstanceUtil.hpp>
#include <RapidVulkan/Instance.hpp>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

//! Creates a instance (with the validation layer when it is installed) and selects the first physical device that supports the api version
//! baseline. The tests that use it are skipped when that is not possible.
// NOLINTNEXTLINE(readability-identifier-naming)
class TestFixtureFslUtil_Vulkan1_0_PhysicalDevice : public TestFixtureFslUtil_Vulkan1_0
{
  std::string m_reason;

protected:
  RapidVulkan::Instance m_instance;
  VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
  bool m_hasValidationLayer{false};

public:
  TestFixtureFslUtil_Vulkan1_0_PhysicalDevice()
  {
    try
    {
      Fsl::Vulkan::ApiVersionUtil::CheckLoader();

      std::vector<const char*> layers;
      const char* const pszValidationLayerName = "VK_LAYER_KHRONOS_validation";
      m_hasValidationLayer = Fsl::Vulkan::InstanceUtil::IsInstanceLayersAvailable(1, &pszValidationLayerName);
      if (m_hasValidationLayer)
      {
        layers.push_back(pszValidationLayerName);
      }

      const std::vector<const char*> extensions;
      m_instance = Fsl::Vulkan::InstanceUtil::CreateInstance("TestFixtureFslUtil_Vulkan1_0_PhysicalDevice", VK_MAKE_VERSION(1, 0, 0),
                                                             Fsl::Vulkan::ApiVersionUtil::MinimumApiVersion, 0, layers, extensions);
      m_physicalDevice = FindPhysicalDevice(m_instance.Get());
    }
    catch (const std::exception& ex)
    {
      m_reason = ex.what();
      m_physicalDevice = VK_NULL_HANDLE;
      m_instance.Reset();
    }
  }

protected:
  [[nodiscard]] bool IsReady() const
  {
    return m_instance.IsValid() && m_physicalDevice != VK_NULL_HANDLE;
  }

  void SkipTest(const std::string& testName, const std::string& reason = std::string())
  {
    std::cout << "\nSkipped '" << testName << "' (" << (reason.empty() ? m_reason : reason) << ")\n";
  }

private:
  static VkPhysicalDevice FindPhysicalDevice(const VkInstance instance)
  {
    for (const auto& physicalDevice : Fsl::Vulkan::InstanceUtil::EnumeratePhysicalDevices(instance))
    {
      VkPhysicalDeviceProperties deviceProperties{};
      vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
      if (Fsl::Vulkan::ApiVersionUtil::IsSupported(deviceProperties))
      {
        return physicalDevice;
      }
    }
    throw Fsl::NotSupportedException("No physical device that supports the api version baseline was found");
  }
};

#endif
