/****************************************************************************************************************************************************
 * Copyright 2020 NXP
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

#include "OptionParser.hpp"
#include <FslBase/BasicTypes.hpp>
#include <FslBase/Exceptions.hpp>
#include <FslBase/Getopt/OptionBaseValues.hpp>
#include <FslBase/Log/Log3Fmt.hpp>
#include <FslBase/Math/MathHelper.hpp>
#include <FslBase/String/StringParseUtil.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include "DefaultValues.hpp"

namespace Fsl
{
  namespace
  {
    struct CommandId
    {
      enum Enum
      {
        Iterations = DEMO_APP_OPTION_BASE,
        HeatmapScale
      };
    };
  }

  OptionParser::OptionParser()
    : m_iterations(DefaultValues::MandelbrotDefaultIterations)
    , m_heatmapScale(DefaultValues::HeatmapScale)
  {
  }


  OptionParser::~OptionParser() = default;


  void OptionParser::OnArgumentSetup(std::deque<Option>& rOptions)
  {
    rOptions.emplace_back("i", "Iterations", OptionArgument::OptionRequired, CommandId::Iterations, "The number of iterations to perform (>=1).");
    rOptions.emplace_back("HeatmapScale", OptionArgument::OptionRequired, CommandId::HeatmapScale, "The initial heatmap scale");
  }


  OptionParseResult OptionParser::OnParse(const int32_t cmdId, const StringViewLite& strOptArg)
  {
    //    bool boolValue;
    uint16_t valueUInt16 = 0;
    uint32_t valueUInt32 = 0;

    switch (cmdId)
    {
    case CommandId::Iterations:
      if (StringParseUtil::Parse(valueUInt16, strOptArg) <= 0)
      {
        return OptionParseResult::Failed;
      }
      m_iterations = valueUInt16;
      return OptionParseResult::Parsed;
    case CommandId::HeatmapScale:
      if (StringParseUtil::Parse(valueUInt32, strOptArg) <= 0)
      {
        return OptionParseResult::Failed;
      }
      m_heatmapScale = valueUInt32;
      return OptionParseResult::Parsed;
    default:
      return OptionParseResult::NotHandled;
    }
  }
}
