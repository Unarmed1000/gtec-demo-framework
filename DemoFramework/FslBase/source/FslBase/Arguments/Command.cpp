/****************************************************************************************************************************************************
 * Copyright 2019, 2022 NXP
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

#include <FslBase/Arguments/Command.hpp>
#include <cassert>
#include <stdexcept>
#include <utility>

namespace Fsl::Arguments
{
  Command::Command(std::string smartName, const uint32_t commandId, const CommandType commandType, const bool required)
    : Id(commandId)
    , Type(commandType)
    , Required(required)
  {
    if (smartName.empty())
    {
      throw std::invalid_argument("smartName can not be empty");
    }
    if (smartName.size() == 1)
    {
      ShortName = std::move(smartName);
    }
    else
    {
      Name = std::move(smartName);
    }

    Validate();
  }


  Command::Command(std::string shortName, std::string name, const uint32_t commandId, const CommandType commandType, const bool required)
    : ShortName(std::move(shortName))
    , Name(std::move(name))
    , Id(commandId)
    , Type(commandType)
    , Required(required)
  {
    Validate();
  }


  void Command::Validate() const
  {
    if (ShortName.empty() && Name.empty())
    {
      throw std::invalid_argument("ShortName or Name must be valid");
    }

    if (!ShortName.empty() && ShortName.size() != 1)
    {
      throw std::invalid_argument("A short name is expected to have a length of 1");
    }

    if (!Name.empty() && Name.size() <= 1)
    {
      throw std::invalid_argument("A name is expected to have a length that is greater than one");
    }

    if (Type == CommandType::Undefined)
    {
      throw std::invalid_argument("The argument type can not be undefined");
    }
  }

  bool Command::IsValid() const
  {
    return (!ShortName.empty() || !Name.empty()) && (ShortName.empty() || ShortName.size() == 1) && (Name.empty() || Name.size() > 1) &&
           Type != CommandType::Undefined;
  }
}
