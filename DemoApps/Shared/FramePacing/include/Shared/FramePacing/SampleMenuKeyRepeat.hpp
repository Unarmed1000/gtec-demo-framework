#ifndef SHARED_FRAMEPACING_SAMPLEMENUKEYREPEAT_HPP
#define SHARED_FRAMEPACING_SAMPLEMENUKEYREPEAT_HPP
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

#include <FslBase/Time/TimeSpan.hpp>
#include <FslNativeWindow/Base/VirtualKey.hpp>
#include <algorithm>
#include <cstdint>

namespace Fsl
{
  //! How a key of the keyboard menu repeats while it is held down
  enum class SampleMenuKeyRepeatMode
  {
    //! At one rate (the cursor: every entry it passes can be seen)
    Steady,
    //! Faster the longer the key is held (a slider: the far end of a long range is reached in a few seconds)
    Accelerating
  };

  //! The key of the keyboard menu that is held down. The framework tells a app when a key goes down and when it comes up, a key that
  //! is held does not repeat by itself, so the menu counts the repeats from the time the key has been down.
  class SampleMenuKeyRepeat final
  {
    VirtualKey::Enum m_key{VirtualKey::Undefined};
    SampleMenuKeyRepeatMode m_mode{SampleMenuKeyRepeatMode::Steady};
    //! How long the key has been down
    TimeSpan m_heldTime;
    //! The part of a repeat that is left over, in ticks times repeats per second
    int64_t m_repeatRemainder{0};

  public:
    //! A key that is held does nothing for this long, so a key press is one step
    static constexpr TimeSpan Delay = TimeSpan::FromMilliseconds(400);
    static constexpr int64_t SteadyRepeatsPerSecond = 12;
    //! The rates of SampleMenuKeyRepeatMode::Accelerating: the first one from the delay on, the next ones from the time the key has
    //! been down for
    static constexpr int64_t SlowRepeatsPerSecond = 20;
    static constexpr int64_t MediumRepeatsPerSecond = 100;
    static constexpr int64_t FastRepeatsPerSecond = 400;
    static constexpr TimeSpan MediumFrom = TimeSpan::FromMilliseconds(1500);
    static constexpr TimeSpan FastFrom = TimeSpan::FromMilliseconds(3000);
    //! A frame that took longer than this repeats as if it took this long: a hitch does not move a slider by a second of repeats
    static constexpr TimeSpan MaxUpdateTime = TimeSpan::FromMilliseconds(100);

    //! @brief A key went down: it is the key that is held from now on (the last key that went down is the one that repeats)
    constexpr void Press(const VirtualKey::Enum key, const SampleMenuKeyRepeatMode mode) noexcept
    {
      m_key = key;
      m_mode = mode;
      m_heldTime = {};
      m_repeatRemainder = 0;
    }

    //! @brief A key came up. The key that is held stays held when it is another one.
    constexpr void Release(const VirtualKey::Enum key) noexcept
    {
      if (key == m_key)
      {
        Clear();
      }
    }

    //! @brief No key is held
    constexpr void Clear() noexcept
    {
      m_key = VirtualKey::Undefined;
      m_heldTime = {};
      m_repeatRemainder = 0;
    }

    //! @brief The key that is held, VirtualKey::Undefined when none is
    [[nodiscard]] constexpr VirtualKey::Enum GetKey() const noexcept
    {
      return m_key;
    }

    //! @brief Time went by (a frame)
    //! @param elapsedTime the time since the last call, or since the key went down
    //! @return how often the key that is held repeats in that time. Zero when no key is held.
    [[nodiscard]] constexpr uint32_t Update(const TimeSpan elapsedTime) noexcept
    {
      if (m_key == VirtualKey::Undefined || elapsedTime <= TimeSpan())
      {
        return 0;
      }
      const TimeSpan time = std::min(elapsedTime, MaxUpdateTime);
      m_heldTime += time;
      if (m_heldTime <= Delay)
      {
        return 0;
      }
      // The part of the time that is after the delay, at the rate the key has reached
      const TimeSpan repeatTime = std::min(time, m_heldTime - Delay);
      m_repeatRemainder += repeatTime.Ticks() * RepeatsPerSecond();
      const int64_t repeats = m_repeatRemainder / TimeSpan::TicksPerSecond;
      m_repeatRemainder -= repeats * TimeSpan::TicksPerSecond;
      return static_cast<uint32_t>(repeats);
    }

  private:
    [[nodiscard]] constexpr int64_t RepeatsPerSecond() const noexcept
    {
      if (m_mode == SampleMenuKeyRepeatMode::Steady)
      {
        return SteadyRepeatsPerSecond;
      }
      if (m_heldTime >= FastFrom)
      {
        return FastRepeatsPerSecond;
      }
      return m_heldTime >= MediumFrom ? MediumRepeatsPerSecond : SlowRepeatsPerSecond;
    }
  };
}

#endif
