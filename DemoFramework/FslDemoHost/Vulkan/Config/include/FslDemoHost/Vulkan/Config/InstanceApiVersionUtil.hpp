#ifndef FSLDEMOHOST_VULKAN_CONFIG_INSTANCEAPIVERSIONUTIL_HPP
#define FSLDEMOHOST_VULKAN_CONFIG_INSTANCEAPIVERSIONUTIL_HPP
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

#include <FslBase/String/StringViewLite.hpp>
#include <cstdint>

namespace Fsl::Vulkan::InstanceApiVersionUtil
{
  //! The description of the command line option
  extern const char* const g_optionDescription;

  //! Parse a 'major.minor' string (1.0 to 1.4) into a Vulkan api version.
  //! @return true if parsed, false if the string was invalid (an error is logged).
  bool TryParse(const StringViewLite& strVersion, uint32_t& rApiVersion);

  //! Select the instance api version to use.
  //! @param appApiVersion the api version requested by the app.
  //! @param overrideApiVersion the api version requested by the user (0 = no override).
  //! @note The override never lowers the version requested by the app.
  uint32_t Select(const uint32_t appApiVersion, const uint32_t overrideApiVersion);
}

#endif
