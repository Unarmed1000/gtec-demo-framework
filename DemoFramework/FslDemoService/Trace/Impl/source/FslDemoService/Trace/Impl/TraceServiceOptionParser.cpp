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
#include <FslDemoService/Trace/Impl/TraceServiceOptionParser.hpp>
#include <string_view>

namespace Fsl
{
  namespace
  {
    struct CommandId
    {
      enum Enum
      {
        Trace,
        Anonymise,
      };
    };
  }


  void TraceServiceOptionParser::OnArgumentSetup(std::deque<Option>& rOptions)
  {
    rOptions.emplace_back("Trace", OptionArgument::OptionRequired, CommandId::Trace,
                          "Write a Perfetto trace of the app to the given file: what its main thread was doing, and every value, event and "
                          "fact that is known about each frame. Open it in ui.perfetto.dev.");
    rOptions.emplace_back("Trace.Anonymise", OptionArgument::OptionRequired, CommandId::Anonymise,
                          "on (the default) or off. While on the trace names the vendor of the graphics device in place of its model and "
                          "placeholders in place of the directories of this machine.");
  }


  OptionParseResult TraceServiceOptionParser::OnParse(const int32_t cmdId, const StringViewLite& strOptArg)
  {
    switch (cmdId)
    {
    case CommandId::Trace:
      if (strOptArg.empty())
      {
        FSLLOG3_ERROR("Trace requires a file name");
        return OptionParseResult::Failed;
      }
      m_tracePath = IO::Path(strOptArg);
      return OptionParseResult::Parsed;
    case CommandId::Anonymise:
      {
        if (strOptArg == "on")
        {
          m_anonymise = true;
          return OptionParseResult::Parsed;
        }
        if (strOptArg == "off")
        {
          m_anonymise = false;
          return OptionParseResult::Parsed;
        }
        FSLLOG3_ERROR("Trace.Anonymise must be on or off");
        return OptionParseResult::Failed;
      }
    default:
      return OptionParseResult::NotHandled;
    }
  }


  bool TraceServiceOptionParser::OnParsingComplete()
  {
    return true;
  }
}
