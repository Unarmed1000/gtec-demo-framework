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
#include <FslBase/Log/String/FmtStringViewLite.hpp>
#include <FslDemoHost/Vulkan/Config/ValidationFeatureUtil.hpp>

namespace Fsl::Vulkan
{
  namespace
  {
    namespace LocalConfig
    {
      constexpr auto ValidationLayerName = "VK_LAYER_KHRONOS_validation";

      // The names used on the command line
      constexpr StringViewLite FeatureSync("sync");
      constexpr StringViewLite FeatureGpuAssisted("gpu");
      constexpr StringViewLite FeatureBestPractices("bestpractices");
      constexpr StringViewLite FeatureDebugPrintf("printf");

      // The settings of the validation layer (see its manifest or the Vulkan SDK documentation)
      constexpr auto SettingSync = "validate_sync";
      constexpr auto SettingGpuAssisted = "gpuav_enable";
      constexpr auto SettingBestPractices = "validate_best_practices";
      constexpr auto SettingDebugPrintf = "printf_enable";
    }

    bool TryEnableFeature(const StringViewLite& strFeature, ValidationFeatures& rFeatures)
    {
      if (strFeature == LocalConfig::FeatureSync)
      {
        rFeatures.Sync = true;
      }
      else if (strFeature == LocalConfig::FeatureGpuAssisted)
      {
        rFeatures.GpuAssisted = true;
      }
      else if (strFeature == LocalConfig::FeatureBestPractices)
      {
        rFeatures.BestPractices = true;
      }
      else if (strFeature == LocalConfig::FeatureDebugPrintf)
      {
        rFeatures.DebugPrintf = true;
      }
      else
      {
        return false;
      }
      return true;
    }
  }


  ValidationLayerSettings::ValidationLayerSettings([[maybe_unused]] const ValidationFeatures& features,
                                                   [[maybe_unused]] const void* const pNext) noexcept
  {
#ifdef VK_EXT_layer_settings
    uint32_t settingCount = 0;
    const auto addSetting = [this, &settingCount](const bool enabled, const char* const pszSettingName)
    {
      if (enabled)
      {
        VkLayerSettingEXT& rSetting = m_settings[settingCount];
        rSetting.pLayerName = LocalConfig::ValidationLayerName;
        rSetting.pSettingName = pszSettingName;
        rSetting.type = VK_LAYER_SETTING_TYPE_BOOL32_EXT;
        rSetting.valueCount = 1;
        rSetting.pValues = &m_enabled;
        ++settingCount;
      }
    };
    // Only the features that are enabled are set, the rest is left to the defaults and the vk_layer_settings.txt file
    addSetting(features.Sync, LocalConfig::SettingSync);
    addSetting(features.GpuAssisted, LocalConfig::SettingGpuAssisted);
    addSetting(features.BestPractices, LocalConfig::SettingBestPractices);
    addSetting(features.DebugPrintf, LocalConfig::SettingDebugPrintf);

    m_createInfo.sType = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT;
    m_createInfo.pNext = pNext;
    m_createInfo.settingCount = settingCount;
    m_createInfo.pSettings = m_settings.data();
#endif
  }


  bool ValidationLayerSettings::IsSupported() noexcept
  {
#ifdef VK_EXT_layer_settings
    return true;
#else
    return false;
#endif
  }


  const char* ValidationLayerSettings::GetExtensionName() noexcept
  {
#ifdef VK_EXT_layer_settings
    return VK_EXT_LAYER_SETTINGS_EXTENSION_NAME;
#else
    return nullptr;
#endif
  }


  const void* ValidationLayerSettings::GetCreateInfo() const noexcept
  {
#ifdef VK_EXT_layer_settings
    return m_createInfo.settingCount > 0u ? &m_createInfo : nullptr;
#else
    return nullptr;
#endif
  }


  namespace ValidationFeatureUtil
  {
    const char* const g_optionDescription =
      "Enable optional checks of the validation layer, a comma separated list of: sync (synchronization), gpu (GPU assisted), "
      "bestpractices, printf (debugPrintfEXT). It enables the validation layer unless '--VkValidate false' is used.";


    bool TryParse(const StringViewLite& strFeatures, ValidationFeatures& rFeatures)
    {
      ValidationFeatures features;
      StringViewLite::size_type startIndex = 0;
      while (startIndex <= strFeatures.size())
      {
        const auto endIndex = strFeatures.find(',', startIndex);
        const StringViewLite strFeature =
          strFeatures.substr(startIndex, endIndex != StringViewLite::npos ? (endIndex - startIndex) : StringViewLite::npos);
        if (!TryEnableFeature(strFeature, features))
        {
          FSLLOG3_ERROR("Unknown validation feature '{}', expected a comma separated list of: {}, {}, {}, {}", strFeature, LocalConfig::FeatureSync,
                        LocalConfig::FeatureGpuAssisted, LocalConfig::FeatureBestPractices, LocalConfig::FeatureDebugPrintf);
          return false;
        }
        if (endIndex == StringViewLite::npos)
        {
          break;
        }
        startIndex = endIndex + 1;
      }
      rFeatures = features;
      return true;
    }
  }
}
