/****************************************************************************************************************************************************
 * Copyright 2026 NXP
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *
 *    * Neither the name of the NXP. nor the names of
 *      its contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************************************************************************************/

#include <FslBase/Log/Log3Fmt.hpp>
#include <FslBase/Log/String/FmtStringViewLite.hpp>
#include <FslDemoHost/Vulkan/Config/InstanceApiVersionUtil.hpp>
#include <vulkan/vulkan.h>

namespace Fsl::Vulkan::InstanceApiVersionUtil
{
  namespace
  {
    namespace LocalConfig
    {
      constexpr uint32_t MaxMinorVersion = 4;
    }

    constexpr bool IsDigit(const char ch) noexcept
    {
      return ch >= '0' && ch <= '9';
    }
  }

  const char* const g_optionDescription =
    "Override the Vulkan instance api version (1.0 to 1.4). It never lowers the version requested by the app. GPU assisted validation needs "
    "1.1 or newer.";


  bool TryParse(const StringViewLite& strVersion, uint32_t& rApiVersion)
  {
    // Expected format: 'major.minor'
    if (strVersion.size() != 3 || !IsDigit(strVersion[0]) || strVersion[1] != '.' || !IsDigit(strVersion[2]))
    {
      FSLLOG3_ERROR("Invalid Vulkan api version '{}', expected a value like 1.1", strVersion);
      return false;
    }
    const auto major = static_cast<uint32_t>(strVersion[0] - '0');
    const auto minor = static_cast<uint32_t>(strVersion[2] - '0');
    if (major != 1 || minor > LocalConfig::MaxMinorVersion)
    {
      FSLLOG3_ERROR("Unsupported Vulkan api version '{}', expected a value between 1.0 and 1.{}", strVersion, LocalConfig::MaxMinorVersion);
      return false;
    }
    rApiVersion = VK_MAKE_VERSION(major, minor, 0);
    return true;
  }


  uint32_t Select(const uint32_t appApiVersion, const uint32_t overrideApiVersion)
  {
    uint32_t apiVersion = appApiVersion;
    if (overrideApiVersion != 0)
    {
      if (overrideApiVersion >= appApiVersion)
      {
        apiVersion = overrideApiVersion;
      }
      else
      {
        FSLLOG3_WARNING("The requested Vulkan api version {}.{} is lower than the version {}.{} required by the app, so it was ignored",
                        VK_VERSION_MAJOR(overrideApiVersion), VK_VERSION_MINOR(overrideApiVersion), VK_VERSION_MAJOR(appApiVersion),
                        VK_VERSION_MINOR(appApiVersion));
      }
    }
    FSLLOG3_VERBOSE("Vulkan instance api version: {}.{}", VK_VERSION_MAJOR(apiVersion), VK_VERSION_MINOR(apiVersion));
    return apiVersion;
  }
}
