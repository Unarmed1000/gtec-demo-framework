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
#include <FslUtil/Vulkan1_0/Debug/VUDebugUtils.hpp>
#include <FslUtil/Vulkan1_0/Debug/VUDebugUtilsLog.hpp>
#include <FslUtil/Vulkan1_0/Debug/VUDebugUtilsMessenger.hpp>
#include <FslUtil/Vulkan1_0/Debug/VUScopedCmdDebugLabel.hpp>
#include <FslUtil/Vulkan1_0/UnitTest/Helper/Common.hpp>
#include <FslUtil/Vulkan1_0/UnitTest/Helper/TestFixtureFslUtil_Vulkan1_0.hpp>
#include <stdexcept>
#include <string>

using namespace Fsl;
using namespace Fsl::Vulkan;

namespace
{
  // The debug utils have not been initialized in these tests, so nothing here is allowed to reach Vulkan.
  using TestFixtureFslUtil_Vulkan1_0_VUDebugUtils_Uninitialized = TestFixtureFslUtil_Vulkan1_0;

  //! A handle that is not VK_NULL_HANDLE, it is never used as the calls are expected to do nothing
  template <typename THandle>
  THandle FakeHandle()
  {
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    return reinterpret_cast<THandle>(static_cast<uintptr_t>(0x1000));
  }
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUDebugUtils_Uninitialized, IsEnabled)
{
  EXPECT_FALSE(VUDebugUtils::IsEnabled());
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUDebugUtils_Uninitialized, Shutdown)
{
  VUDebugUtils::Shutdown();

  EXPECT_FALSE(VUDebugUtils::IsEnabled());
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUDebugUtils_Uninitialized, Init_NullInstance)
{
  EXPECT_THROW(VUDebugUtils::Init(VK_NULL_HANDLE), std::invalid_argument);

  EXPECT_FALSE(VUDebugUtils::IsEnabled());
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUDebugUtils_Uninitialized, SetObjectName)
{
  auto* const device = FakeHandle<VkDevice>();

  VUDebugUtils::SetObjectNameRaw(device, VK_OBJECT_TYPE_BUFFER, 0x1000, "Name");
  VUDebugUtils::SetObjectName(device, VK_OBJECT_TYPE_DEVICE, device, "Name");
  VUDebugUtils::SetObjectName(device, VK_OBJECT_TYPE_DEVICE, device, std::string("Name"));
  VUDebugUtils::SetObjectName(device, VK_OBJECT_TYPE_DEVICE, device, nullptr);
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUDebugUtils_Uninitialized, CmdLabels)
{
  auto* const commandBuffer = FakeHandle<VkCommandBuffer>();

  VUDebugUtils::CmdBeginLabel(commandBuffer, "Label");
  VUDebugUtils::CmdInsertLabel(commandBuffer, "Label");
  VUDebugUtils::CmdEndLabel(commandBuffer);
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUDebugUtils_Uninitialized, QueueLabels)
{
  auto* const queue = FakeHandle<VkQueue>();

  VUDebugUtils::QueueBeginLabel(queue, "Label");
  VUDebugUtils::QueueInsertLabel(queue, "Label");
  VUDebugUtils::QueueEndLabel(queue);
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUDebugUtils_Uninitialized, ScopedCmdDebugLabel)
{
  const VUScopedCmdDebugLabel scopedLabel(FakeHandle<VkCommandBuffer>(), "Label");
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUDebugUtils_Uninitialized, CreateMessenger)
{
  const VkDebugUtilsMessengerCreateInfoEXT createInfo = VUDebugUtilsLog::BuildCreateInfo(LogType::Info);
  VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;

  EXPECT_EQ(VK_ERROR_EXTENSION_NOT_PRESENT, VUDebugUtils::CreateMessenger(FakeHandle<VkInstance>(), createInfo, &messenger));
  EXPECT_TRUE(messenger == VK_NULL_HANDLE);
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUDebugUtils_Uninitialized, Messenger_Default)
{
  VUDebugUtilsMessenger messenger;

  EXPECT_FALSE(messenger.IsValid());
  EXPECT_TRUE(messenger.Get() == VK_NULL_HANDLE);
  EXPECT_TRUE(messenger.GetInstance() == VK_NULL_HANDLE);

  messenger.Reset();
  EXPECT_FALSE(messenger.IsValid());
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUDebugUtils_Uninitialized, Messenger_Reset_NullInstance)
{
  const VkDebugUtilsMessengerCreateInfoEXT createInfo = VUDebugUtilsLog::BuildCreateInfo(LogType::Info);
  VUDebugUtilsMessenger messenger;

  EXPECT_THROW(messenger.Reset(VK_NULL_HANDLE, createInfo), std::invalid_argument);
  EXPECT_FALSE(messenger.IsValid());
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUDebugUtils_Uninitialized, Messenger_Reset_NotEnabled)
{
  const VkDebugUtilsMessengerCreateInfoEXT createInfo = VUDebugUtilsLog::BuildCreateInfo(LogType::Info);
  VUDebugUtilsMessenger messenger;

  EXPECT_ANY_THROW(messenger.Reset(FakeHandle<VkInstance>(), createInfo));
  EXPECT_FALSE(messenger.IsValid());
}
