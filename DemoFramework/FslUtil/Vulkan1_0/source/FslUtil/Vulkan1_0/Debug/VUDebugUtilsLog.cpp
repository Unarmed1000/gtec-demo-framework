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

#include <FslBase/Log/Log3Fmt.hpp>
#include <FslUtil/Vulkan1_0/Debug/VUDebugUtilsLog.hpp>
#include <RapidVulkan/Debug/Strings/VkObjectType.hpp>
#include <fmt/format.h>
#include <exception>
#include <iterator>

namespace Fsl::Vulkan::VUDebugUtilsLog
{
  namespace
  {
    void AppendMessageTypes(fmt::memory_buffer& rBuffer, const VkDebugUtilsMessageTypeFlagsEXT messageTypes)
    {
      bool isFirst = true;
      const auto append = [&rBuffer, &isFirst](const char* const pszName)
      {
        fmt::format_to(std::back_inserter(rBuffer), "{}{}", isFirst ? "" : "|", pszName);
        isFirst = false;
      };

      if ((messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) != 0u)
      {
        append("general");
      }
      if ((messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) != 0u)
      {
        append("validation");
      }
      if ((messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) != 0u)
      {
        append("performance");
      }
      if (isFirst)
      {
        append("unknown");
      }
    }


    //! The general messages (mostly from the loader) refer to the instance, which says nothing. So the objects are only listed when the message is
    //! about how the objects are used or when one of them was given a name.
    bool IsObjectListRelevant(const VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT& callbackData) noexcept
    {
      if (callbackData.objectCount == 0u || callbackData.pObjects == nullptr)
      {
        return false;
      }
      if ((messageTypes & (VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)) != 0u)
      {
        return true;
      }
      for (uint32_t i = 0; i < callbackData.objectCount; ++i)
      {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (callbackData.pObjects[i].pObjectName != nullptr)
        {
          return true;
        }
      }
      return false;
    }


    void AppendLabels(fmt::memory_buffer& rBuffer, const char* const pszDesc, const uint32_t labelCount, const VkDebugUtilsLabelEXT* const pLabels)
    {
      if (labelCount == 0u || pLabels == nullptr)
      {
        return;
      }

      fmt::format_to(std::back_inserter(rBuffer), "\n  {}:", pszDesc);
      for (uint32_t i = 0; i < labelCount; ++i)
      {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        const char* const pszLabelName = pLabels[i].pLabelName;
        fmt::format_to(std::back_inserter(rBuffer), "{}'{}'", i == 0u ? " " : " > ", pszLabelName != nullptr ? pszLabelName : "");
      }
    }


    VKAPI_ATTR VkBool32 VKAPI_CALL LogCallback(const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                               const VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                               const VkDebugUtilsMessengerCallbackDataEXT* const pCallbackData, void* const /*pUserData*/) noexcept
    {
      if (pCallbackData == nullptr)
      {
        return VK_FALSE;
      }

      try
      {
        const std::string message = ToLogString(messageTypes, *pCallbackData);
        if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0)
        {
          FSLLOG3_ERROR("{}", message);
        }
        else if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0)
        {
          FSLLOG3_WARNING("{}", message);
        }
        else if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) != 0)
        {
          FSLLOG3_INFO("{}", message);
        }
        else
        {
          FSLLOG3_VERBOSE5("{}", message);
        }
      }
      catch (const std::exception&)
      {
        // A exception must never escape into the Vulkan implementation that called us
        FSLLOG3_ERROR("Vulkan: failed to format a debug message");
      }
      // The call that caused the message should not be aborted
      return VK_FALSE;
    }
  }


  VkDebugUtilsMessengerCreateInfoEXT BuildCreateInfo(const LogType logLevel, const bool forceInfo) noexcept
  {
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    // The loader is very talkative at the info and verbose levels
    if (forceInfo || logLevel >= LogType::Verbose3)
    {
      createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
    }
    if (logLevel >= LogType::Verbose5)
    {
      createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
    }
    createInfo.messageType =
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = LogCallback;
    return createInfo;
  }


  std::string ToLogString(const VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT& callbackData)
  {
    fmt::memory_buffer buffer;
    fmt::format_to(std::back_inserter(buffer), "Vulkan ");
    AppendMessageTypes(buffer, messageTypes);
    if (callbackData.pMessageIdName != nullptr)
    {
      fmt::format_to(std::back_inserter(buffer), " [{}]", callbackData.pMessageIdName);
    }
    fmt::format_to(std::back_inserter(buffer), ": {}", callbackData.pMessage != nullptr ? callbackData.pMessage : "");

    if (IsObjectListRelevant(messageTypes, callbackData))
    {
      for (uint32_t i = 0; i < callbackData.objectCount; ++i)
      {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        const VkDebugUtilsObjectNameInfoEXT& object = callbackData.pObjects[i];
        fmt::format_to(std::back_inserter(buffer), "\n  object {}: {} {:#x}", i, RapidVulkan::Debug::ToString(object.objectType),
                       object.objectHandle);
        if (object.pObjectName != nullptr)
        {
          fmt::format_to(std::back_inserter(buffer), " '{}'", object.pObjectName);
        }
      }
    }
    AppendLabels(buffer, "queue labels", callbackData.queueLabelCount, callbackData.pQueueLabels);
    AppendLabels(buffer, "command buffer labels", callbackData.cmdBufLabelCount, callbackData.pCmdBufLabels);
    return fmt::to_string(buffer);
  }
}
