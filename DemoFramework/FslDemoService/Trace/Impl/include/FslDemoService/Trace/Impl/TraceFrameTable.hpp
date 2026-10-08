#ifndef FSLDEMOSERVICE_TRACE_IMPL_TRACEFRAMETABLE_HPP
#define FSLDEMOSERVICE_TRACE_IMPL_TRACEFRAMETABLE_HPP
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


#include <FslDemoService/Trace/Impl/TraceRecords.hpp>
#include <FslDemoService/Trace/TraceTypes.hpp>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Fsl
{
  //! The values of the trace and the rows of the frames that can still change.
  //!
  //! A row is opened when its frame begins and stays open while the frames after it are drawn, as some values are only known later (when
  //! the frame reached the display). It is closed, and can then be written, when its place is needed for a newer frame. It has no file
  //! access and no platform dependency.
  class TraceFrameTable final
  {
  public:
    //! The number of frames a row stays open
    static constexpr uint32_t OpenRowCount = 64;

  private:
    std::vector<TraceValueInfo> m_values;
    std::array<TraceFrameRow, OpenRowCount> m_rows{};
    std::array<bool, OpenRowCount> m_isOpen{};
    bool m_valuesLocked{false};
    bool m_hasFrame{false};
    uint64_t m_newestFrameIndex{0};

  public:
    //! @brief Add a value.
    //! @return the value, the existing one if the name is known. Not valid if the name is not usable, the values are locked or there
    //!         is no room for more.
    TraceValue RegisterValue(const std::string_view name, const TraceUnit unit, const std::string_view description);

    //! @return the value of the name, not valid if there is none.
    [[nodiscard]] TraceValue FindValue(const std::string_view name) const noexcept;

    //! @return true if the name can be the name of a value: not empty and only letters, digits, '_' and '.'
    [[nodiscard]] static bool IsValidValueName(const std::string_view name) noexcept;

    //! @brief No more values can be added (the first row was written).
    void LockValues() noexcept
    {
      m_valuesLocked = true;
    }

    [[nodiscard]] bool IsValuesLocked() const noexcept
    {
      return m_valuesLocked;
    }

    [[nodiscard]] const std::vector<TraceValueInfo>& GetValues() const noexcept
    {
      return m_values;
    }

    //! @return true if it is a value of this table.
    [[nodiscard]] bool IsValue(const TraceValue value) const noexcept
    {
      return value.IsValid() && value.Value <= m_values.size();
    }

    //! @return true if it is a value of this table that is a time.
    [[nodiscard]] bool IsTime(const TraceValue value) const noexcept
    {
      return IsValue(value) && m_values[value.Value - 1u].Unit == TraceUnit::Ticks;
    }

    //! @brief Open the row of a frame. The frames are expected in order.
    //! @param rClosedRow the row that was closed to make room, if the method returns true.
    //! @return true if a row was closed.
    bool BeginFrame(const uint64_t frameIndex, const uint32_t runId, TraceFrameRow& rClosedRow) noexcept;

    //! @brief Set a value of a open row. Nothing happens if the row of the frame is not open or the value is not valid.
    void SetValue(const uint64_t frameIndex, const TraceValue value, const int64_t number) noexcept;

    //! @return true if the row of the frame can still be changed.
    [[nodiscard]] bool IsOpen(const uint64_t frameIndex) const noexcept;

    //! @brief Close the oldest open row.
    //! @return false if no row is open.
    bool TryCloseOldest(TraceFrameRow& rClosedRow) noexcept;
  };
}

#endif
