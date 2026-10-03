#ifndef FSLUTIL_VULKAN1_0_UTIL_APIVERSIONUTIL_HPP
#define FSLUTIL_VULKAN1_0_UTIL_APIVERSIONUTIL_HPP
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

//! The Vulkan api version the framework requires (the baseline).
namespace Fsl::Vulkan::ApiVersionUtil
{
  //! The minimum Vulkan api version required from the loader, the instance and the physical device.
  constexpr uint32_t MinimumApiVersion = VK_API_VERSION_1_3;

  //! @brief Get the instance api version supported by the loader (1.0 if the loader predates vkEnumerateInstanceVersion).
  uint32_t GetLoaderApiVersion();

  //! @brief Check that the loader supports the minimum api version.
  //! @throws NotSupportedException if it does not.
  void CheckLoader();

  //! @brief Check if the physical device supports the minimum api version.
  bool IsSupported(const VkPhysicalDeviceProperties& properties) noexcept;

  //! @brief Check that the physical device supports the minimum api version.
  //! @throws NotSupportedException if it does not.
  void CheckPhysicalDevice(const VkPhysicalDeviceProperties& properties);
}

#endif
