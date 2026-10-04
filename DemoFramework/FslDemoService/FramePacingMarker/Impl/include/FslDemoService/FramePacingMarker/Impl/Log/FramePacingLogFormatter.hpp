#ifndef FSLDEMOSERVICE_FRAMEPACINGMARKER_IMPL_LOG_FRAMEPACINGLOGFORMATTER_HPP
#define FSLDEMOSERVICE_FRAMEPACINGMARKER_IMPL_LOG_FRAMEPACINGLOGFORMATTER_HPP
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


#include <FslBase/Time/TickCount.hpp>
#include <FslDemoService/FramePacingMarker/Impl/Log/FramePacingFrameLogTable.hpp>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

//! The text of the two files of the frame pacing log. Both are CSV: a comma between the fields, a line per record, a field that contains a
//! comma, a quote or a line end is put in quotes with its quotes doubled.
namespace Fsl::FramePacingLogFormatter
{
  //! The version of the format, written as a fact. Raise it when a existing column changes its meaning or its name.
  constexpr uint32_t FormatVersion = 1;

  //! The name of the first column of both files
  constexpr std::string_view FrameIndexColumnName("frameIndex");

  //! @brief Append the first line of the frames file: the names of the columns.
  void AppendFramesHeader(std::string& rDst, const std::vector<FramePacingLogColumnInfo>& columns);

  //! @brief Append the line of a frame: its frame index and a whole number per column, empty where there is no value.
  void AppendRow(std::string& rDst, const FramePacingLogRow& row, const std::vector<FramePacingLogColumnInfo>& columns);

  //! @brief Append the first line of the events file.
  void AppendEventsHeader(std::string& rDst);

  //! @brief Append the line of a event.
  void AppendEvent(std::string& rDst, const uint64_t frameIndex, const TickCount time, const std::string_view name, const std::string_view details);

  //! @brief Append a field, in quotes if it has to be.
  void AppendField(std::string& rDst, const std::string_view field);

  //! @return the name a unit is written with.
  [[nodiscard]] std::string_view ToString(const FramePacingLogUnit unit) noexcept;
}

#endif
