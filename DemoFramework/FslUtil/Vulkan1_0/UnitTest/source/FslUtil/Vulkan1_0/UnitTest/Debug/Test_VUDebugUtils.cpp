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
#include <FslBase/Exceptions.hpp>
#include <FslBase/Log/Log3Fmt.hpp>
#include <FslUtil/Vulkan1_0/Debug/VUDebugUtils.hpp>
#include <FslUtil/Vulkan1_0/Debug/VUDebugUtilsLog.hpp>
#include <FslUtil/Vulkan1_0/Debug/VUDebugUtilsMessenger.hpp>
#include <FslUtil/Vulkan1_0/Debug/VUScopedCmdDebugLabel.hpp>
#include <FslUtil/Vulkan1_0/UnitTest/Helper/Common.hpp>
#include <FslUtil/Vulkan1_0/UnitTest/Helper/TestFixtureFslUtil_Vulkan1_0.hpp>
#include <FslUtil/Vulkan1_0/Util/ApiVersionUtil.hpp>
#include <FslUtil/Vulkan1_0/Util/InstanceUtil.hpp>
#include <FslUtil/Vulkan1_0/Util/PhysicalDeviceUtil.hpp>
#include <FslUtil/Vulkan1_0/Util/QueueUtil.hpp>
#include <FslUtil/Vulkan1_0/VUBufferMemory.hpp>
#include <FslUtil/Vulkan1_0/VUDevice.hpp>
#include <RapidVulkan/CommandBuffer.hpp>
#include <RapidVulkan/CommandPool.hpp>
#include <RapidVulkan/Instance.hpp>
#include <algorithm>
#include <exception>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

using namespace Fsl;
using namespace Fsl::Vulkan;

namespace
{
  namespace LocalConfig
  {
    constexpr auto ValidationLayerName = "VK_LAYER_KHRONOS_validation";
    constexpr auto BufferName = "TestBuffer";
  }

  struct MessageRecord
  {
    VkDebugUtilsMessageSeverityFlagBitsEXT Severity{};
    std::string Text;
  };

  VKAPI_ATTR VkBool32 VKAPI_CALL TestCallback(const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                              const VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                              const VkDebugUtilsMessengerCallbackDataEXT* const pCallbackData, void* const pUserData) noexcept
  {
    try
    {
      auto* const pMessages = static_cast<std::vector<MessageRecord>*>(pUserData);
      if (pMessages != nullptr && pCallbackData != nullptr)
      {
        pMessages->push_back(MessageRecord{messageSeverity, VUDebugUtilsLog::ToLogString(messageTypes, *pCallbackData)});
      }
    }
    catch (const std::exception& ex)
    {
      // A exception must never escape into the Vulkan implementation that called us
      FSLLOG3_ERROR("Failed to record a Vulkan debug message: {}", ex.what());
    }
    // Abort the call that caused the message, so the invalid calls made by the tests never reach the driver
    return VK_TRUE;
  }


  // Creates a instance with VK_EXT_debug_utils (and the validation layer when it is installed), a messenger that records the messages and a device.
  // NOLINTNEXTLINE(readability-identifier-naming)
  class TestFixtureFslUtil_Vulkan1_0_VUDebugUtils : public TestFixtureFslUtil_Vulkan1_0
  {
    std::string m_reason;

  protected:
    std::vector<MessageRecord> m_messages;
    RapidVulkan::Instance m_instance;
    VUDebugUtilsMessenger m_messenger;
    VUDevice m_device;
    uint32_t m_queueFamilyIndex{0};
    bool m_hasValidationLayer{false};

  public:
    TestFixtureFslUtil_Vulkan1_0_VUDebugUtils(const TestFixtureFslUtil_Vulkan1_0_VUDebugUtils&) = delete;
    TestFixtureFslUtil_Vulkan1_0_VUDebugUtils& operator=(const TestFixtureFslUtil_Vulkan1_0_VUDebugUtils&) = delete;

    TestFixtureFslUtil_Vulkan1_0_VUDebugUtils()
    {
      try
      {
        Create();
      }
      catch (const std::exception& ex)
      {
        m_reason = ex.what();
        Destroy();
      }
    }

    ~TestFixtureFslUtil_Vulkan1_0_VUDebugUtils() override
    {
      Destroy();
    }

  protected:
    [[nodiscard]] bool IsReady() const
    {
      return m_instance.IsValid() && m_device.IsValid() && VUDebugUtils::IsEnabled();
    }

    void SkipTest(const std::string& testName)
    {
      std::cout << "\nSkipped '" << testName << "' (cant create a instance with VK_EXT_debug_utils: '" << m_reason << "')\n";
    }

    [[nodiscard]] bool HasMessageContaining(const std::string& text) const
    {
      return std::any_of(m_messages.begin(), m_messages.end(),
                         [&text](const MessageRecord& entry) { return entry.Text.find(text) != std::string::npos; });
    }

  private:
    void Create()
    {
      ApiVersionUtil::CheckLoader();

      std::vector<const char*> layers;
      {
        const char* const pszLayerName = LocalConfig::ValidationLayerName;
        m_hasValidationLayer = InstanceUtil::IsInstanceLayersAvailable(1, &pszLayerName);
        if (m_hasValidationLayer)
        {
          layers.push_back(pszLayerName);
        }
      }

      const std::vector<const char*> extensions = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
      if (!InstanceUtil::IsInstanceExtensionsAvailable(static_cast<uint32_t>(extensions.size()), extensions.data(),
                                                       static_cast<uint32_t>(layers.size()), layers.data()))
      {
        throw NotSupportedException("VK_EXT_debug_utils is not available");
      }

      VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfo{};
      messengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
      messengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
      messengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
      messengerCreateInfo.pfnUserCallback = TestCallback;
      messengerCreateInfo.pUserData = &m_messages;

      m_instance = InstanceUtil::CreateInstance("TestFixtureFslUtil_Vulkan1_0_VUDebugUtils", VK_MAKE_VERSION(1, 0, 0),
                                                ApiVersionUtil::MinimumApiVersion, 0, layers, extensions, nullptr, &messengerCreateInfo);
      if (!VUDebugUtils::Init(m_instance.Get()))
      {
        throw NotSupportedException("The VK_EXT_debug_utils entry points were not found");
      }
      m_messenger.Reset(m_instance.Get(), messengerCreateInfo);

      const VkPhysicalDevice physicalDevice = FindDevice(m_instance.Get(), m_queueFamilyIndex);

      const float queuePriorities = 0.0f;
      VkDeviceQueueCreateInfo deviceQueueCreateInfo{};
      deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      deviceQueueCreateInfo.queueFamilyIndex = m_queueFamilyIndex;
      deviceQueueCreateInfo.queueCount = 1;
      deviceQueueCreateInfo.pQueuePriorities = &queuePriorities;

      VkDeviceCreateInfo deviceCreateInfo{};
      deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
      deviceCreateInfo.queueCreateInfoCount = 1;
      deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;
      m_device.Reset(physicalDevice, deviceCreateInfo);
    }

    void Destroy() noexcept
    {
      m_device.Reset();
      m_messenger.Reset();
      // The debug utils are shared by the process, so they must not be left enabled for the other tests
      VUDebugUtils::Shutdown();
      m_instance.Reset();
    }

    static VkPhysicalDevice FindDevice(const VkInstance instance, uint32_t& rQueueFamilyIndex)
    {
      // Locate a device that supports the api version baseline and compute
      for (const auto& physicalDevice : InstanceUtil::EnumeratePhysicalDevices(instance))
      {
        VkPhysicalDeviceProperties deviceProperties{};
        vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
        if (!ApiVersionUtil::IsSupported(deviceProperties))
        {
          continue;
        }

        const auto queueProperties = PhysicalDeviceUtil::GetPhysicalDeviceQueueFamilyProperties(physicalDevice);
        for (std::size_t i = 0; i < queueProperties.size(); ++i)
        {
          if ((queueProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0u)
          {
            rQueueFamilyIndex = static_cast<uint32_t>(i);
            return physicalDevice;
          }
        }
      }
      throw NotSupportedException("No physical devices that supports the api version baseline and compute found");
    }
  };

  VUBufferMemory CreateBuffer(const VUDevice& device)
  {
    VkBufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = 64;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    return {device.GetPhysicalDevice(), device.Get(), bufferCreateInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
  }
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUDebugUtils, Init)
{
  if (!IsReady())
  {
    SkipTest("Init");
    return;
  }

  EXPECT_TRUE(VUDebugUtils::IsEnabled());
  EXPECT_TRUE(m_messenger.IsValid());
  EXPECT_EQ(m_instance.Get(), m_messenger.GetInstance());
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUDebugUtils, Messenger_Move)
{
  if (!IsReady())
  {
    SkipTest("Messenger_Move");
    return;
  }

  const VkDebugUtilsMessengerEXT handle = m_messenger.Get();
  VUDebugUtilsMessenger moved(std::move(m_messenger));

  EXPECT_TRUE(moved.IsValid());
  EXPECT_TRUE(moved.Get() == handle);

  moved.Reset();
  EXPECT_FALSE(moved.IsValid());
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUDebugUtils, ValidUseProducesNoMessages)
{
  if (!IsReady())
  {
    SkipTest("ValidUseProducesNoMessages");
    return;
  }

  const VUBufferMemory buffer = CreateBuffer(m_device);
  VUDebugUtils::SetObjectName(m_device.Get(), VK_OBJECT_TYPE_BUFFER, buffer.GetBuffer(), LocalConfig::BufferName);
  VUDebugUtils::SetObjectName(m_device.Get(), VK_OBJECT_TYPE_DEVICE_MEMORY, buffer.GetMemory(), std::string("TestMemory"));
  VUDebugUtils::SetObjectName(m_device.Get(), VK_OBJECT_TYPE_DEVICE, m_device.Get(), "TestDevice");

  VkQueue queue = VK_NULL_HANDLE;
  vkGetDeviceQueue(m_device.Get(), m_queueFamilyIndex, 0, &queue);
  VUDebugUtils::QueueBeginLabel(queue, "QueueRegion");
  VUDebugUtils::QueueInsertLabel(queue, "QueueLabel");
  VUDebugUtils::QueueEndLabel(queue);

  const RapidVulkan::CommandPool commandPool(m_device.Get(), 0, m_queueFamilyIndex);
  RapidVulkan::CommandBuffer commandBuffer(m_device.Get(), commandPool.Get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  commandBuffer.Begin(beginInfo);
  {
    const VUScopedCmdDebugLabel scopedLabel(commandBuffer.Get(), "Region");
    VUDebugUtils::CmdInsertLabel(commandBuffer.Get(), "Label");
    {
      const VUScopedCmdDebugLabel scopedNestedLabel(commandBuffer.Get(), "NestedRegion");
    }
  }
  commandBuffer.End();

  for (const auto& entry : m_messages)
  {
    std::cout << entry.Text << "\n";
  }
  EXPECT_TRUE(m_messages.empty());
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUDebugUtils, ValidationErrorContainsObjectName)
{
  if (!IsReady())
  {
    SkipTest("ValidationErrorContainsObjectName");
    return;
  }
  if (!m_hasValidationLayer)
  {
    std::cout << "\nSkipped 'ValidationErrorContainsObjectName' (" << LocalConfig::ValidationLayerName << " is not installed)\n";
    return;
  }

  const VUBufferMemory buffer = CreateBuffer(m_device);
  VUDebugUtils::SetObjectName(m_device.Get(), VK_OBJECT_TYPE_BUFFER, buffer.GetBuffer(), LocalConfig::BufferName);
  ASSERT_TRUE(m_messages.empty());

  // The buffer is already bound to its memory, so binding it again is a error the validation layer reports with the buffer as one of its objects.
  // The test callback aborts the call, so it never reaches the driver.
  const VkResult result = vkBindBufferMemory(m_device.Get(), buffer.GetBuffer(), buffer.GetMemory(), 0);

  EXPECT_NE(VK_SUCCESS, result);
  ASSERT_FALSE(m_messages.empty());
  EXPECT_EQ(VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, m_messages.front().Severity);
  EXPECT_TRUE(HasMessageContaining("Vulkan validation"));
  EXPECT_TRUE(HasMessageContaining(std::string("'") + LocalConfig::BufferName + "'")) << m_messages.front().Text;
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUDebugUtils, ValidationErrorContainsCommandBufferLabels)
{
  if (!IsReady())
  {
    SkipTest("ValidationErrorContainsCommandBufferLabels");
    return;
  }
  if (!m_hasValidationLayer)
  {
    std::cout << "\nSkipped 'ValidationErrorContainsCommandBufferLabels' (" << LocalConfig::ValidationLayerName << " is not installed)\n";
    return;
  }

  const RapidVulkan::CommandPool commandPool(m_device.Get(), 0, m_queueFamilyIndex);
  RapidVulkan::CommandBuffer commandBuffer(m_device.Get(), commandPool.Get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  commandBuffer.Begin(beginInfo);
  {
    const VUScopedCmdDebugLabel scopedLabel(commandBuffer.Get(), "Region");
    ASSERT_TRUE(m_messages.empty());

    // Ending a render pass that was never started is a error, the test callback aborts the call so it never reaches the driver.
    vkCmdEndRenderPass(commandBuffer.Get());

    ASSERT_FALSE(m_messages.empty());
    EXPECT_TRUE(HasMessageContaining("command buffer labels: 'Region'")) << m_messages.front().Text;
  }
  commandBuffer.End();
}
