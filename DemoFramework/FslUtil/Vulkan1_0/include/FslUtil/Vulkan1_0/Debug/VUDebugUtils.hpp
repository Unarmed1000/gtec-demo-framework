#ifndef FSLUTIL_VULKAN1_0_DEBUG_VUDEBUGUTILS_HPP
#define FSLUTIL_VULKAN1_0_DEBUG_VUDEBUGUTILS_HPP
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

// Make sure Common.hpp is the first include file (to make the error message as helpful as possible when disabled)
#include <FslUtil/Vulkan1_0/Common.hpp>
#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>
#include <type_traits>

//! Access to VK_EXT_debug_utils (object names, command buffer and queue labels, debug messengers).
//!
//! The Vulkan loader does not export the entry points of this extension, so they are looked up when Init is called and kept in a table that is
//! shared by the process. Until then, and when the extension is not available, everything here does nothing. This allows code to name its objects
//! and label its commands without checking if the extension is enabled.
//!
//! Init and Shutdown follow the rule Vulkan has for creating and destroying the instance: call them from the thread that owns the instance while
//! no other thread is using it. Everything else can be called from any thread and sees either all of the entry points or none of them.
namespace Fsl::Vulkan::VUDebugUtils
{
  //! @brief Look up the VK_EXT_debug_utils entry points.
  //! @param instance a instance that was created with VK_EXT_debug_utils enabled.
  //! @return true if the entry points were found and debug utils are now enabled.
  bool Init(const VkInstance instance);

  //! @brief Forget the entry points. Call this before the instance that was supplied to Init is destroyed.
  void Shutdown() noexcept;

  //! @return true if Init found the entry points. Use it to skip work that is only needed to build a name.
  bool IsEnabled() noexcept;

  //! @brief Give the object a name that is shown by validation messages and tools like RenderDoc
  //! @param objectHandle the handle as a uint64_t, use SetObjectName to have it converted.
  void SetObjectNameRaw(const VkDevice device, const VkObjectType objectType, const uint64_t objectHandle, const char* const pszName) noexcept;

  //! @brief Give the object a name that is shown by validation messages and tools like RenderDoc
  //! @note  The object type is supplied by the caller as non dispatchable handles are the same type on a 32bit target.
  template <typename THandle>
  void SetObjectName(const VkDevice device, const VkObjectType objectType, const THandle handle, const char* const pszName) noexcept
  {
    if constexpr (std::is_pointer_v<THandle>)
    {
      SetObjectNameRaw(device, objectType, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(handle)), pszName);
    }
    else
    {
      SetObjectNameRaw(device, objectType, static_cast<uint64_t>(handle), pszName);
    }
  }

  template <typename THandle>
  void SetObjectName(const VkDevice device, const VkObjectType objectType, const THandle handle, const std::string& name) noexcept
  {
    SetObjectName(device, objectType, handle, name.c_str());
  }

  //! @brief Open a label region in the command buffer, it must be closed with CmdEndLabel (see VUScopedCmdDebugLabel).
  void CmdBeginLabel(const VkCommandBuffer commandBuffer, const char* const pszName) noexcept;
  void CmdEndLabel(const VkCommandBuffer commandBuffer) noexcept;
  //! @brief Insert a single label in the command buffer
  void CmdInsertLabel(const VkCommandBuffer commandBuffer, const char* const pszName) noexcept;

  //! @brief Open a label region on the queue, it must be closed with QueueEndLabel.
  void QueueBeginLabel(const VkQueue queue, const char* const pszName) noexcept;
  void QueueEndLabel(const VkQueue queue) noexcept;
  //! @brief Insert a single label on the queue
  void QueueInsertLabel(const VkQueue queue, const char* const pszName) noexcept;

  //! @brief Create a debug messenger (see VUDebugUtilsMessenger for a RAII wrapper)
  //! @return VK_ERROR_EXTENSION_NOT_PRESENT if debug utils are not enabled.
  VkResult CreateMessenger(const VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT& createInfo,
                           VkDebugUtilsMessengerEXT* const pMessenger) noexcept;
  void DestroyMessenger(const VkInstance instance, const VkDebugUtilsMessengerEXT messenger) noexcept;
}

#endif
