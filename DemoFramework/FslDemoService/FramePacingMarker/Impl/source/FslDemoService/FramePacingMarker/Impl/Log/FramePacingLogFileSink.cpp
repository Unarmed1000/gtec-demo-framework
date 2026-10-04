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


#include <FslBase/Exceptions.hpp>
#include <FslBase/Log/IO/FmtPath.hpp>
#include <FslBase/System/Platform/PlatformPathTransform.hpp>
#include <FslDemoService/FramePacingMarker/Impl/Log/FramePacingLogFileSink.hpp>
#include <fmt/format.h>
#include <string>

namespace Fsl
{
  namespace
  {
    void Open(std::ofstream& rFile, const IO::Path& path)
    {
      rFile.open(PlatformPathTransform::ToSystemPath(path), std::ios::out | std::ios::binary | std::ios::trunc);
      if (!rFile.is_open())
      {
        throw IOException(fmt::format("Failed to create file '{}'", path));
      }
    }
  }


  FramePacingLogFileSink::FramePacingLogFileSink(const IO::Path& framesPath, const IO::Path& eventsPath)
  {
    Open(m_frames, framesPath);
    Open(m_events, eventsPath);
  }


  void FramePacingLogFileSink::Write(const FramePacingLogStream stream, const std::string_view text)
  {
    std::ofstream& rFile = stream == FramePacingLogStream::Frames ? m_frames : m_events;
    rFile.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!rFile.good())
    {
      throw IOException("Failed to write to the frame pacing log");
    }
  }


  void FramePacingLogFileSink::Flush()
  {
    m_frames.flush();
    m_events.flush();
  }


  IO::Path FramePacingLogFileSink::ToEventsPath(const IO::Path& framesPath)
  {
    const std::string& path = framesPath.ToUTF8String();
    // The extension is what follows the last dot of the file name
    const std::size_t nameStart = path.find_last_of("/\\");
    const std::size_t dot = path.find_last_of('.');
    const bool hasExtension = dot != std::string::npos && (nameStart == std::string::npos || dot > nameStart);
    return IO::Path((hasExtension ? path.substr(0, dot) : path) + ".events.csv");
  }
}
