#ifndef FSLDEMOSERVICE_FRAMEPACINGMARKER_IMPL_LOG_IFRAMEPACINGLOGSINK_HPP
#define FSLDEMOSERVICE_FRAMEPACINGMARKER_IMPL_LOG_IFRAMEPACINGLOGSINK_HPP
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


#include <string_view>

namespace Fsl
{
  //! The two files of the frame pacing log
  enum class FramePacingLogStream
  {
    Frames,
    Events
  };

  //! Where the text of the frame pacing log goes. It is called from the thread of the writer.
  class IFramePacingLogSink
  {
  public:
    virtual ~IFramePacingLogSink() = default;

    //! @brief Write text to a stream. A failure is reported with a exception, the writer then stops writing.
    virtual void Write(const FramePacingLogStream stream, const std::string_view text) = 0;

    //! @brief Make sure what was written survives the process being killed.
    virtual void Flush() = 0;
  };
}

#endif
