#ifndef FSLDEMOSERVICE_FRAMEPACINGMARKER_FRAMEPACINGLOGCOLUMN_HPP
#define FSLDEMOSERVICE_FRAMEPACINGMARKER_FRAMEPACINGLOGCOLUMN_HPP
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


#include <cstdint>

namespace Fsl
{
  //! A column of the frame pacing log (IFramePacingFrameLog::RegisterColumn). The default one is not a column, and setting its value does
  //! nothing, which is what a registration returns while the log is off.
  struct FramePacingLogColumn
  {
    //! Zero = not a column, else the number of the column counted from one
    uint32_t Value{0};

    constexpr FramePacingLogColumn() noexcept = default;
    constexpr explicit FramePacingLogColumn(const uint32_t value) noexcept
      : Value(value)
    {
    }

    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
      return Value != 0u;
    }

    constexpr bool operator==(const FramePacingLogColumn& rhs) const noexcept = default;
  };
}

#endif
