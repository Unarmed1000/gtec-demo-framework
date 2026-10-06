#ifndef FSLDEMOSERVICE_FRAMEPACINGMARKER_IFRAMEPACINGFRAMELOG_HPP
#define FSLDEMOSERVICE_FRAMEPACINGMARKER_IFRAMEPACINGFRAMELOG_HPP
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
#include <FslBase/Time/TimeSpan.hpp>
#include <FslDemoService/FramePacingMarker/FramePacingLogColumn.hpp>
#include <FslDemoService/FramePacingMarker/FramePacingLogUnit.hpp>
#include <cstdint>
#include <string_view>

namespace Fsl
{
  //! The frame pacing log (--FramePacing.Log <file>): one row per frame with everything that is known about the frame, written as whole
  //! numbers to a CSV file, and a second file with the events and the facts of the run.
  //!
  //! The frame pacing marker service writes what it knows itself (what the marker carries). Anybody else adds what only they know: a host
  //! the times of its swap, a app the values of its frame pacer. Register the columns once, before the first frame, and set their values
  //! each frame. A value can be set for a earlier frame as well, as some things are only known later (when a frame reached the display).
  //!
  //! Everything here does nothing while the log is off, so there is no need to check for it. Use IsLogEnabled to skip the work of finding
  //! a value out.
  class IFramePacingFrameLog
  {
  public:
    virtual ~IFramePacingFrameLog() = default;

    //! @brief Check if the frames are being logged.
    [[nodiscard]] virtual bool IsLogEnabled() const noexcept = 0;

    //! @brief Add a column to the log. Columns can be added until the first rows are written, so do it at startup.
    //! @param name the name of the column: letters, digits, '_' and '.'. A name that exists returns that column.
    //! @param unit what the values count.
    //! @param description what the value is, written to the events file.
    //! @return the column (not valid if the log is off or the column could not be added).
    virtual FramePacingLogColumn RegisterColumn(const std::string_view name, const FramePacingLogUnit unit, const std::string_view description) = 0;

    //! @brief The index of the frame that is being drawn, which is the frame index of the marker and the first column of the log.
    [[nodiscard]] virtual uint64_t GetLogFrameIndex() const noexcept = 0;

    //! @brief Set a value of the frame that is being drawn.
    //! @note  A frame is the one that is being drawn from the moment the host begins its draw. During the update of a frame it is
    //!        still the frame before: keep a value of the update and set it during the draw.
    virtual void SetLogInt64(const FramePacingLogColumn column, const int64_t value) noexcept = 0;
    virtual void SetLogUInt64(const FramePacingLogColumn column, const uint64_t value) noexcept = 0;

    //! @brief Set a value of a earlier frame. It is ignored if the row of the frame was written already (a row is kept for 64 frames).
    virtual void SetLogInt64At(const uint64_t frameIndex, const FramePacingLogColumn column, const int64_t value) noexcept = 0;
    virtual void SetLogUInt64At(const uint64_t frameIndex, const FramePacingLogColumn column, const uint64_t value) noexcept = 0;

    //! @brief Write a event: something that happened at a moment and is not a value of every frame (a swapchain was created, a setting
    //!        was changed). It is written with the index of the frame that is being drawn and the time.
    //! @param name the name of the event.
    //! @param details what happened, by convention "key=value;key=value".
    virtual void AddLogEvent(const std::string_view name, const std::string_view details) = 0;

    //! @brief Write a fact about the run (the graphics API, the present mode, a setting). A fact is a event named "fact".
    void SetLogFact(const std::string_view key, const std::string_view value)
    {
      AddLogFact(key, value);
    }

    // The typed forms, so a time is never mistaken for a duration

    void SetLogValue(const FramePacingLogColumn column, const TickCount value) noexcept
    {
      SetLogInt64(column, value.Ticks());
    }

    void SetLogValue(const FramePacingLogColumn column, const TimeSpan value) noexcept
    {
      SetLogInt64(column, value.Ticks());
    }

    void SetLogValue(const FramePacingLogColumn column, const bool value) noexcept
    {
      SetLogInt64(column, value ? 1 : 0);
    }

    void SetLogValueAt(const uint64_t frameIndex, const FramePacingLogColumn column, const TickCount value) noexcept
    {
      SetLogInt64At(frameIndex, column, value.Ticks());
    }

    void SetLogValueAt(const uint64_t frameIndex, const FramePacingLogColumn column, const TimeSpan value) noexcept
    {
      SetLogInt64At(frameIndex, column, value.Ticks());
    }

  protected:
    virtual void AddLogFact(const std::string_view key, const std::string_view value) = 0;
  };
}

#endif
