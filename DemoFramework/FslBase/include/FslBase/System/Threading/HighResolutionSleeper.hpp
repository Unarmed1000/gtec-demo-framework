#ifndef FSLBASE_SYSTEM_THREADING_HIGHRESOLUTIONSLEEPER_HPP
#define FSLBASE_SYSTEM_THREADING_HIGHRESOLUTIONSLEEPER_HPP
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
#include <memory>
#include <string_view>

namespace Fsl
{
  //! @brief Sleeps until a time on the clock of the HighResolutionTimer, with the most precise sleep the platform offers.
  //! @note A sleep can wake late, so a wait that has to end on a time sleeps until GetWakeMargin before it and spins the rest.
  class HighResolutionSleeper
  {
    struct Impl;
    std::unique_ptr<Impl> m_impl;

  public:
    HighResolutionSleeper(const HighResolutionSleeper&) = delete;
    HighResolutionSleeper& operator=(const HighResolutionSleeper&) = delete;

    HighResolutionSleeper();
    ~HighResolutionSleeper();

    //! @return how long before a time a sleep has to aim for the wake not to be late
    [[nodiscard]] static TimeSpan GetWakeMargin() noexcept;

    //! @return the name of the sleep this platform uses
    [[nodiscard]] static std::string_view GetMethodName() noexcept;

    //! @brief Sleep until the time. Returns at once if it has passed.
    //! @note It can return early if the sleep is interrupted and late by the wake error, a caller that needs the time checks it.
    void SleepUntil(const TickCount time) noexcept;
  };
}

#endif
