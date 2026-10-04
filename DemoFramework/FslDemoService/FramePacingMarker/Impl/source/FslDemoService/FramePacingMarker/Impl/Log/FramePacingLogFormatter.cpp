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


#include <FslDemoService/FramePacingMarker/Impl/Log/FramePacingLogFormatter.hpp>
#include <fmt/format.h>
#include <algorithm>
#include <iterator>

namespace Fsl::FramePacingLogFormatter
{
  void AppendFramesHeader(std::string& rDst, const std::vector<FramePacingLogColumnInfo>& columns)
  {
    rDst.append(FrameIndexColumnName);
    for (const FramePacingLogColumnInfo& column : columns)
    {
      rDst.push_back(',');
      // A column name has no character that needs quotes
      rDst.append(column.Name);
    }
    rDst.push_back('\n');
  }


  void AppendRow(std::string& rDst, const FramePacingLogRow& row, const std::vector<FramePacingLogColumnInfo>& columns)
  {
    auto dst = std::back_inserter(rDst);
    dst = fmt::format_to(dst, "{}", row.FrameIndex);
    const std::size_t columnCount = std::min(columns.size(), static_cast<std::size_t>(FramePacingLogRow::MaxColumns));
    for (std::size_t i = 0; i < columnCount; ++i)
    {
      *dst++ = ',';
      if (row.HasValue.test(i))
      {
        if (columns[i].IsUnsigned)
        {
          dst = fmt::format_to(dst, "{}", static_cast<uint64_t>(row.Values[i]));
        }
        else
        {
          dst = fmt::format_to(dst, "{}", row.Values[i]);
        }
      }
    }
    *dst++ = '\n';
  }


  void AppendEventsHeader(std::string& rDst)
  {
    rDst.append(FrameIndexColumnName);
    rDst.append(",timeTicks,event,details\n");
  }


  void AppendEvent(std::string& rDst, const uint64_t frameIndex, const TickCount time, const std::string_view name, const std::string_view details)
  {
    fmt::format_to(std::back_inserter(rDst), "{},{},", frameIndex, time.Ticks());
    AppendField(rDst, name);
    rDst.push_back(',');
    AppendField(rDst, details);
    rDst.push_back('\n');
  }


  void AppendField(std::string& rDst, const std::string_view field)
  {
    if (field.find_first_of(",\"\r\n") == std::string_view::npos)
    {
      rDst.append(field);
      return;
    }
    rDst.push_back('"');
    for (const char ch : field)
    {
      if (ch == '"')
      {
        rDst.push_back('"');
      }
      rDst.push_back(ch);
    }
    rDst.push_back('"');
  }


  std::string_view ToString(const FramePacingLogUnit unit) noexcept
  {
    switch (unit)
    {
    case FramePacingLogUnit::Ticks:
      return "ticks";
    case FramePacingLogUnit::DurationTicks:
      return "durationTicks";
    case FramePacingLogUnit::Nanoseconds:
      return "nanoseconds";
    case FramePacingLogUnit::Count:
      return "count";
    case FramePacingLogUnit::Id:
      return "id";
    case FramePacingLogUnit::Flag:
      return "flag";
    case FramePacingLogUnit::Pixels:
      return "pixels";
    case FramePacingLogUnit::Code:
      return "code";
    default:
      return "unknown";
    }
  }
}
