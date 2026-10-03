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

#include <FslUtil/Vulkan1_0/Debug/VUDebugUtilsLog.hpp>
#include <FslUtil/Vulkan1_0/UnitTest/Helper/Common.hpp>
#include <FslUtil/Vulkan1_0/UnitTest/Helper/TestFixtureFslUtil_Vulkan1_0.hpp>
#include <array>

using namespace Fsl;
using namespace Fsl::Vulkan;

namespace
{
  using TestFixtureFslUtil_Vulkan1_0_VUDebugUtilsLog = TestFixtureFslUtil_Vulkan1_0;

  VkDebugUtilsMessengerCallbackDataEXT CreateCallbackData(const char* const pszMessageIdName, const char* const pszMessage)
  {
    VkDebugUtilsMessengerCallbackDataEXT callbackData{};
    callbackData.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
    callbackData.pMessageIdName = pszMessageIdName;
    callbackData.pMessage = pszMessage;
    return callbackData;
  }

  VkDebugUtilsObjectNameInfoEXT CreateObject(const VkObjectType objectType, const uint64_t handle, const char* const pszName)
  {
    VkDebugUtilsObjectNameInfoEXT object{};
    object.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    object.objectType = objectType;
    object.objectHandle = handle;
    object.pObjectName = pszName;
    return object;
  }

  VkDebugUtilsLabelEXT CreateLabel(const char* const pszName)
  {
    VkDebugUtilsLabelEXT label{};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = pszName;
    return label;
  }
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUDebugUtilsLog, ToLogString_Message)
{
  const auto callbackData = CreateCallbackData("VUID-test", "Hello world");

  EXPECT_EQ("Vulkan validation [VUID-test]: Hello world", VUDebugUtilsLog::ToLogString(VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, callbackData));
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUDebugUtilsLog, ToLogString_NullStrings)
{
  const auto callbackData = CreateCallbackData(nullptr, nullptr);

  EXPECT_EQ("Vulkan general: ", VUDebugUtilsLog::ToLogString(VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, callbackData));
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUDebugUtilsLog, ToLogString_MessageTypes)
{
  const auto callbackData = CreateCallbackData(nullptr, "A");

  EXPECT_EQ("Vulkan unknown: A", VUDebugUtilsLog::ToLogString(0, callbackData));
  EXPECT_EQ("Vulkan performance: A", VUDebugUtilsLog::ToLogString(VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, callbackData));
  EXPECT_EQ("Vulkan general|validation|performance: A",
            VUDebugUtilsLog::ToLogString(VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                                         callbackData));
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUDebugUtilsLog, ToLogString_ValidationObjects)
{
  const std::array<VkDebugUtilsObjectNameInfoEXT, 2> objects = {CreateObject(VK_OBJECT_TYPE_BUFFER, 0x10, "MyBuffer"),
                                                                CreateObject(VK_OBJECT_TYPE_DEVICE_MEMORY, 0x2f, nullptr)};
  auto callbackData = CreateCallbackData("VUID-test", "Bad");
  callbackData.objectCount = static_cast<uint32_t>(objects.size());
  callbackData.pObjects = objects.data();

  EXPECT_EQ("Vulkan validation [VUID-test]: Bad\n  object 0: VK_OBJECT_TYPE_BUFFER 0x10 'MyBuffer'\n  object 1: VK_OBJECT_TYPE_DEVICE_MEMORY 0x2f",
            VUDebugUtilsLog::ToLogString(VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, callbackData));
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUDebugUtilsLog, ToLogString_GeneralUnnamedObjectsAreNotListed)
{
  const std::array<VkDebugUtilsObjectNameInfoEXT, 1> objects = {CreateObject(VK_OBJECT_TYPE_INSTANCE, 0x10, nullptr)};
  auto callbackData = CreateCallbackData("Loader Message", "Hello");
  callbackData.objectCount = static_cast<uint32_t>(objects.size());
  callbackData.pObjects = objects.data();

  EXPECT_EQ("Vulkan general [Loader Message]: Hello", VUDebugUtilsLog::ToLogString(VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, callbackData));
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUDebugUtilsLog, ToLogString_GeneralNamedObjectsAreListed)
{
  const std::array<VkDebugUtilsObjectNameInfoEXT, 1> objects = {CreateObject(VK_OBJECT_TYPE_INSTANCE, 0x10, "Main")};
  auto callbackData = CreateCallbackData("Loader Message", "Hello");
  callbackData.objectCount = static_cast<uint32_t>(objects.size());
  callbackData.pObjects = objects.data();

  EXPECT_EQ("Vulkan general [Loader Message]: Hello\n  object 0: VK_OBJECT_TYPE_INSTANCE 0x10 'Main'",
            VUDebugUtilsLog::ToLogString(VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, callbackData));
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUDebugUtilsLog, ToLogString_Labels)
{
  const std::array<VkDebugUtilsLabelEXT, 1> queueLabels = {CreateLabel("Frame")};
  const std::array<VkDebugUtilsLabelEXT, 3> cmdLabels = {CreateLabel("Scene"), CreateLabel(nullptr), CreateLabel("Blur")};
  auto callbackData = CreateCallbackData(nullptr, "Bad");
  callbackData.queueLabelCount = static_cast<uint32_t>(queueLabels.size());
  callbackData.pQueueLabels = queueLabels.data();
  callbackData.cmdBufLabelCount = static_cast<uint32_t>(cmdLabels.size());
  callbackData.pCmdBufLabels = cmdLabels.data();

  EXPECT_EQ("Vulkan validation: Bad\n  queue labels: 'Frame'\n  command buffer labels: 'Scene' > '' > 'Blur'",
            VUDebugUtilsLog::ToLogString(VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, callbackData));
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUDebugUtilsLog, ToLogString_CountWithoutArrayIsIgnored)
{
  auto callbackData = CreateCallbackData(nullptr, "Bad");
  callbackData.objectCount = 2;
  callbackData.queueLabelCount = 2;
  callbackData.cmdBufLabelCount = 2;

  EXPECT_EQ("Vulkan validation: Bad", VUDebugUtilsLog::ToLogString(VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, callbackData));
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUDebugUtilsLog, BuildCreateInfo_Severity)
{
  constexpr VkDebugUtilsMessageSeverityFlagsEXT ErrorAndWarning =
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
  constexpr VkDebugUtilsMessageSeverityFlagsEXT Info = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
  constexpr VkDebugUtilsMessageSeverityFlagsEXT Verbose = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;

  EXPECT_EQ(ErrorAndWarning, VUDebugUtilsLog::BuildCreateInfo(LogType::Info).messageSeverity);
  EXPECT_EQ(ErrorAndWarning, VUDebugUtilsLog::BuildCreateInfo(LogType::Verbose2).messageSeverity);
  EXPECT_EQ(ErrorAndWarning | Info, VUDebugUtilsLog::BuildCreateInfo(LogType::Verbose3).messageSeverity);
  EXPECT_EQ(ErrorAndWarning | Info, VUDebugUtilsLog::BuildCreateInfo(LogType::Verbose4).messageSeverity);
  EXPECT_EQ(ErrorAndWarning | Info | Verbose, VUDebugUtilsLog::BuildCreateInfo(LogType::Verbose5).messageSeverity);
  // The info messages can be forced on as that is how the output of debugPrintf is delivered
  EXPECT_EQ(ErrorAndWarning | Info, VUDebugUtilsLog::BuildCreateInfo(LogType::Info, true).messageSeverity);
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUDebugUtilsLog, BuildCreateInfo_IsComplete)
{
  const VkDebugUtilsMessengerCreateInfoEXT createInfo = VUDebugUtilsLog::BuildCreateInfo(LogType::Info);

  EXPECT_EQ(VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT, createInfo.sType);
  EXPECT_EQ(nullptr, createInfo.pNext);
  EXPECT_NE(nullptr, createInfo.pfnUserCallback);
  EXPECT_EQ(nullptr, createInfo.pUserData);
  EXPECT_EQ(static_cast<VkDebugUtilsMessageTypeFlagsEXT>(VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                                         VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                                         VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT),
            createInfo.messageType);
}
