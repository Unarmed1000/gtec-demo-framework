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

#include <FslUtil/Vulkan1_0/Debug/VUDebugUtils.hpp>
#include <FslUtil/Vulkan1_0/Debug/VUDebugUtilsMessenger.hpp>
#include <RapidVulkan/CheckError.hpp>
#include <stdexcept>

namespace Fsl::Vulkan
{
  VUDebugUtilsMessenger& VUDebugUtilsMessenger::operator=(VUDebugUtilsMessenger&& other) noexcept
  {
    if (this != &other)
    {
      Reset();

      m_instance = other.m_instance;
      m_messenger = other.m_messenger;

      other.m_instance = VK_NULL_HANDLE;
      other.m_messenger = VK_NULL_HANDLE;
    }
    return *this;
  }


  VUDebugUtilsMessenger::VUDebugUtilsMessenger(VUDebugUtilsMessenger&& other) noexcept
    : m_instance(other.m_instance)
    , m_messenger(other.m_messenger)
  {
    other.m_instance = VK_NULL_HANDLE;
    other.m_messenger = VK_NULL_HANDLE;
  }


  VUDebugUtilsMessenger::VUDebugUtilsMessenger(const VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT& createInfo)
  {
    Reset(instance, createInfo);
  }


  void VUDebugUtilsMessenger::Reset() noexcept
  {
    if (!IsValid())
    {
      return;
    }

    VUDebugUtils::DestroyMessenger(m_instance, m_messenger);
    m_instance = VK_NULL_HANDLE;
    m_messenger = VK_NULL_HANDLE;
  }


  void VUDebugUtilsMessenger::Reset(const VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT& createInfo)
  {
    if (instance == VK_NULL_HANDLE)
    {
      throw std::invalid_argument("instance can not be VK_NULL_HANDLE");
    }
    Reset();

    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
    RapidVulkan::CheckError(VUDebugUtils::CreateMessenger(instance, createInfo, &messenger), "vkCreateDebugUtilsMessengerEXT", __FILE__, __LINE__);

    m_instance = instance;
    m_messenger = messenger;
  }
}
