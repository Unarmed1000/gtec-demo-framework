#ifndef FSLDEMOHOST_VULKAN_CONFIG_VALIDATIONFEATUREUTIL_HPP
#define FSLDEMOHOST_VULKAN_CONFIG_VALIDATIONFEATUREUTIL_HPP
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

#include <FslBase/String/StringViewLite.hpp>
#include <vulkan/vulkan.h>
#include <array>
#include <cstdint>

namespace Fsl::Vulkan
{
  //! The optional checks of VK_LAYER_KHRONOS_validation that can be enabled from the command line
  struct ValidationFeatures
  {
    //! Synchronization validation
    bool Sync{false};
    //! GPU assisted validation
    bool GpuAssisted{false};
    //! Best practices validation
    bool BestPractices{false};
    //! The debugPrintfEXT shader function, its output is sent as info messages
    bool DebugPrintf{false};

    [[nodiscard]] constexpr bool IsAnyEnabled() const noexcept
    {
      return Sync || GpuAssisted || BestPractices || DebugPrintf;
    }

    constexpr bool operator==(const ValidationFeatures& rhs) const noexcept = default;
  };


  //! Holds the VK_EXT_layer_settings settings that enable the validation features.
  //! The create info points into this object, so it must stay alive (and can not be moved) until the instance has been created.
  class ValidationLayerSettings final
  {
    static constexpr uint32_t MaxSettings = 4;

#ifdef VK_EXT_layer_settings
    VkBool32 m_enabled{VK_TRUE};
    std::array<VkLayerSettingEXT, MaxSettings> m_settings{};
    VkLayerSettingsCreateInfoEXT m_createInfo{};
#endif

  public:
    ValidationLayerSettings(const ValidationLayerSettings&) = delete;
    ValidationLayerSettings& operator=(const ValidationLayerSettings&) = delete;
    ~ValidationLayerSettings() = default;

    //! @param pNext the pNext chain to continue with after the layer settings
    ValidationLayerSettings(const ValidationFeatures& features, const void* const pNext) noexcept;

    //! @return true if the Vulkan headers the framework was built with has VK_EXT_layer_settings.
    [[nodiscard]] static bool IsSupported() noexcept;

    //! @return the name of the instance extension that must be enabled for the settings to be used (nullptr if !IsSupported())
    [[nodiscard]] static const char* GetExtensionName() noexcept;

    //! @return the create info to use as the pNext of VkInstanceCreateInfo (nullptr if !IsSupported() or no features were enabled)
    [[nodiscard]] const void* GetCreateInfo() const noexcept;
  };


  namespace ValidationFeatureUtil
  {
    //! The description of the command line option
    extern const char* const g_optionDescription;

    //! Parse a comma separated list of validation features.
    //! @return true if parsed, false if the string was invalid (an error is logged).
    bool TryParse(const StringViewLite& strFeatures, ValidationFeatures& rFeatures);
  }
}

#endif
