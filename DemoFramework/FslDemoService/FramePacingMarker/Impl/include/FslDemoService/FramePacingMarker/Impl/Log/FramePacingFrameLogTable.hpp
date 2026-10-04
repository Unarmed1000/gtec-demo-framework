#ifndef FSLDEMOSERVICE_FRAMEPACINGMARKER_IMPL_LOG_FRAMEPACINGFRAMELOGTABLE_HPP
#define FSLDEMOSERVICE_FRAMEPACINGMARKER_IMPL_LOG_FRAMEPACINGFRAMELOGTABLE_HPP
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


#include <FslDemoService/FramePacingMarker/FramePacingLogColumn.hpp>
#include <FslDemoService/FramePacingMarker/FramePacingLogUnit.hpp>
#include <array>
#include <bitset>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Fsl
{
  //! The values of one frame of the frame pacing log
  struct FramePacingLogRow
  {
    static constexpr uint32_t MaxColumns = 128;

    uint64_t FrameIndex{0};
    std::bitset<MaxColumns> HasValue;
    //! A unsigned value is stored as its bits
    std::array<int64_t, MaxColumns> Values{};
  };

  struct FramePacingLogColumnInfo
  {
    std::string Name;
    FramePacingLogUnit Unit{FramePacingLogUnit::Count};
    std::string Description;
    //! True once a unsigned value was set, the values of the column are then written as unsigned numbers
    bool IsUnsigned{false};
  };


  //! The columns and the rows of the frame pacing log that can still change.
  //!
  //! A row is opened when its frame begins and stays open while the frames after it are drawn, as some values are only known later (when
  //! the frame reached the display). It is closed, and can then be written, when its place is needed for a newer frame. It has no file
  //! access and no platform dependency.
  class FramePacingFrameLogTable final
  {
  public:
    //! The number of frames a row stays open
    static constexpr uint32_t OpenRowCount = 64;

  private:
    std::vector<FramePacingLogColumnInfo> m_columns;
    std::array<FramePacingLogRow, OpenRowCount> m_rows{};
    std::array<bool, OpenRowCount> m_isOpen{};
    bool m_columnsLocked{false};
    bool m_hasFrame{false};
    uint64_t m_newestFrameIndex{0};

  public:
    //! @brief Add a column.
    //! @return the column, the existing one if the name is known. Not valid if the name is not usable, the columns are locked or there
    //!         is no room for more.
    FramePacingLogColumn RegisterColumn(const std::string_view name, const FramePacingLogUnit unit, const std::string_view description);

    //! @return true if the name can be the name of a column: not empty and only letters, digits, '_' and '.'
    [[nodiscard]] static bool IsValidColumnName(const std::string_view name) noexcept;

    //! @brief No more columns can be added (the header of the file was written).
    void LockColumns() noexcept
    {
      m_columnsLocked = true;
    }

    [[nodiscard]] bool IsColumnsLocked() const noexcept
    {
      return m_columnsLocked;
    }

    [[nodiscard]] const std::vector<FramePacingLogColumnInfo>& GetColumns() const noexcept
    {
      return m_columns;
    }

    //! @brief Open the row of a frame. The frames are expected in order.
    //! @param rClosedRow the row that was closed to make room, if the method returns true.
    //! @return true if a row was closed.
    bool BeginFrame(const uint64_t frameIndex, FramePacingLogRow& rClosedRow) noexcept;

    //! @brief Set a value of a open row. Nothing happens if the row of the frame is not open or the column is not valid.
    void SetValue(const uint64_t frameIndex, const FramePacingLogColumn column, const int64_t value, const bool isUnsigned) noexcept;

    //! @return true if the row of the frame can still be changed.
    [[nodiscard]] bool IsOpen(const uint64_t frameIndex) const noexcept;

    //! @brief Close the oldest open row.
    //! @return false if no row is open.
    bool TryCloseOldest(FramePacingLogRow& rClosedRow) noexcept;
  };
}

#endif
