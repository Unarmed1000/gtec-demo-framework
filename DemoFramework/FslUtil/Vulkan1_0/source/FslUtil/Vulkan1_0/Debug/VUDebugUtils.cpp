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
#include <FslUtil/Vulkan1_0/Debug/VUDebugUtils.hpp>
#include <atomic>
#include <stdexcept>

namespace Fsl::Vulkan::VUDebugUtils
{
  namespace
  {
    struct FunctionTable
    {
      PFN_vkCreateDebugUtilsMessengerEXT CreateMessenger{nullptr};
      PFN_vkDestroyDebugUtilsMessengerEXT DestroyMessenger{nullptr};
      PFN_vkSetDebugUtilsObjectNameEXT SetObjectName{nullptr};
      PFN_vkCmdBeginDebugUtilsLabelEXT CmdBeginLabel{nullptr};
      PFN_vkCmdEndDebugUtilsLabelEXT CmdEndLabel{nullptr};
      PFN_vkCmdInsertDebugUtilsLabelEXT CmdInsertLabel{nullptr};
      PFN_vkQueueBeginDebugUtilsLabelEXT QueueBeginLabel{nullptr};
      PFN_vkQueueEndDebugUtilsLabelEXT QueueEndLabel{nullptr};
      PFN_vkQueueInsertDebugUtilsLabelEXT QueueInsertLabel{nullptr};

      [[nodiscard]] bool IsComplete() const noexcept
      {
        return CreateMessenger != nullptr && DestroyMessenger != nullptr && SetObjectName != nullptr && CmdBeginLabel != nullptr &&
               CmdEndLabel != nullptr && CmdInsertLabel != nullptr && QueueBeginLabel != nullptr && QueueEndLabel != nullptr &&
               QueueInsertLabel != nullptr;
      }
    };

    // The entry points are looked up through the instance, so they dispatch on the handle they are called with and work for all its devices.
    // Only written by Init, everything else reaches it through g_activeFunctions.
    FunctionTable g_functions;

    // Null while debug utils are disabled, else it points to the completely filled g_functions.
    // Publishing the table as one atomic pointer means that a thread that names a object or records a label sees either all of the entry points
    // or none of them.
    std::atomic<const FunctionTable*> g_activeFunctions{nullptr};

    const FunctionTable* GetActiveFunctions() noexcept
    {
      return g_activeFunctions.load(std::memory_order_acquire);
    }

    template <typename TFunction>
    TFunction GetFunction(const VkInstance instance, const char* const pszName) noexcept
    {
      return reinterpret_cast<TFunction>(vkGetInstanceProcAddr(instance, pszName));
    }

    VkDebugUtilsLabelEXT ToLabel(const char* const pszName) noexcept
    {
      VkDebugUtilsLabelEXT label{};
      label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
      label.pLabelName = pszName != nullptr ? pszName : "";
      return label;
    }
  }


  bool Init(const VkInstance instance)
  {
    if (instance == VK_NULL_HANDLE)
    {
      throw std::invalid_argument("instance can not be VK_NULL_HANDLE");
    }

    // Nothing is allowed to use the table while it is being filled
    Shutdown();

    FunctionTable functions;
    functions.CreateMessenger = GetFunction<PFN_vkCreateDebugUtilsMessengerEXT>(instance, "vkCreateDebugUtilsMessengerEXT");
    functions.DestroyMessenger = GetFunction<PFN_vkDestroyDebugUtilsMessengerEXT>(instance, "vkDestroyDebugUtilsMessengerEXT");
    functions.SetObjectName = GetFunction<PFN_vkSetDebugUtilsObjectNameEXT>(instance, "vkSetDebugUtilsObjectNameEXT");
    functions.CmdBeginLabel = GetFunction<PFN_vkCmdBeginDebugUtilsLabelEXT>(instance, "vkCmdBeginDebugUtilsLabelEXT");
    functions.CmdEndLabel = GetFunction<PFN_vkCmdEndDebugUtilsLabelEXT>(instance, "vkCmdEndDebugUtilsLabelEXT");
    functions.CmdInsertLabel = GetFunction<PFN_vkCmdInsertDebugUtilsLabelEXT>(instance, "vkCmdInsertDebugUtilsLabelEXT");
    functions.QueueBeginLabel = GetFunction<PFN_vkQueueBeginDebugUtilsLabelEXT>(instance, "vkQueueBeginDebugUtilsLabelEXT");
    functions.QueueEndLabel = GetFunction<PFN_vkQueueEndDebugUtilsLabelEXT>(instance, "vkQueueEndDebugUtilsLabelEXT");
    functions.QueueInsertLabel = GetFunction<PFN_vkQueueInsertDebugUtilsLabelEXT>(instance, "vkQueueInsertDebugUtilsLabelEXT");

    if (!functions.IsComplete())
    {
      FSLLOG3_WARNING("VK_EXT_debug_utils entry points not found, was the instance created with the extension enabled?");
      return false;
    }

    g_functions = functions;
    g_activeFunctions.store(&g_functions, std::memory_order_release);
    return true;
  }


  void Shutdown() noexcept
  {
    // The table is left as it is, so a call that already picked it up is not handed a null entry point
    g_activeFunctions.store(nullptr, std::memory_order_release);
  }


  bool IsEnabled() noexcept
  {
    return GetActiveFunctions() != nullptr;
  }


  void SetObjectNameRaw(const VkDevice device, const VkObjectType objectType, const uint64_t objectHandle, const char* const pszName) noexcept
  {
    const FunctionTable* const pFunctions = GetActiveFunctions();
    if (pFunctions == nullptr || device == VK_NULL_HANDLE || objectHandle == 0u || pszName == nullptr)
    {
      return;
    }

    VkDebugUtilsObjectNameInfoEXT nameInfo{};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = objectType;
    nameInfo.objectHandle = objectHandle;
    nameInfo.pObjectName = pszName;
    // A name is a debug aid, so a failure to set it is not a error
    pFunctions->SetObjectName(device, &nameInfo);
  }


  void CmdBeginLabel(const VkCommandBuffer commandBuffer, const char* const pszName) noexcept
  {
    const FunctionTable* const pFunctions = GetActiveFunctions();
    if (pFunctions != nullptr && commandBuffer != VK_NULL_HANDLE)
    {
      const VkDebugUtilsLabelEXT label = ToLabel(pszName);
      pFunctions->CmdBeginLabel(commandBuffer, &label);
    }
  }


  void CmdEndLabel(const VkCommandBuffer commandBuffer) noexcept
  {
    const FunctionTable* const pFunctions = GetActiveFunctions();
    if (pFunctions != nullptr && commandBuffer != VK_NULL_HANDLE)
    {
      pFunctions->CmdEndLabel(commandBuffer);
    }
  }


  void CmdInsertLabel(const VkCommandBuffer commandBuffer, const char* const pszName) noexcept
  {
    const FunctionTable* const pFunctions = GetActiveFunctions();
    if (pFunctions != nullptr && commandBuffer != VK_NULL_HANDLE)
    {
      const VkDebugUtilsLabelEXT label = ToLabel(pszName);
      pFunctions->CmdInsertLabel(commandBuffer, &label);
    }
  }


  void QueueBeginLabel(const VkQueue queue, const char* const pszName) noexcept
  {
    const FunctionTable* const pFunctions = GetActiveFunctions();
    if (pFunctions != nullptr && queue != VK_NULL_HANDLE)
    {
      const VkDebugUtilsLabelEXT label = ToLabel(pszName);
      pFunctions->QueueBeginLabel(queue, &label);
    }
  }


  void QueueEndLabel(const VkQueue queue) noexcept
  {
    const FunctionTable* const pFunctions = GetActiveFunctions();
    if (pFunctions != nullptr && queue != VK_NULL_HANDLE)
    {
      pFunctions->QueueEndLabel(queue);
    }
  }


  void QueueInsertLabel(const VkQueue queue, const char* const pszName) noexcept
  {
    const FunctionTable* const pFunctions = GetActiveFunctions();
    if (pFunctions != nullptr && queue != VK_NULL_HANDLE)
    {
      const VkDebugUtilsLabelEXT label = ToLabel(pszName);
      pFunctions->QueueInsertLabel(queue, &label);
    }
  }


  VkResult CreateMessenger(const VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT& createInfo,
                           VkDebugUtilsMessengerEXT* const pMessenger) noexcept
  {
    const FunctionTable* const pFunctions = GetActiveFunctions();
    if (pFunctions == nullptr)
    {
      return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    return pFunctions->CreateMessenger(instance, &createInfo, nullptr, pMessenger);
  }


  void DestroyMessenger(const VkInstance instance, const VkDebugUtilsMessengerEXT messenger) noexcept
  {
    const FunctionTable* const pFunctions = GetActiveFunctions();
    if (pFunctions != nullptr && instance != VK_NULL_HANDLE && messenger != VK_NULL_HANDLE)
    {
      pFunctions->DestroyMessenger(instance, messenger, nullptr);
    }
  }
}
