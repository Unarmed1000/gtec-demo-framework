#ifndef SHARED_FRAMEPACING_SAMPLEMENUCURSOR_HPP
#define SHARED_FRAMEPACING_SAMPLEMENUCURSOR_HPP
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

#include <cstddef>

namespace Fsl
{
  //! The way the cursor of the keyboard menu moves through the entries
  enum class SampleMenuCursorMove
  {
    //! Towards the first entry (the up arrow)
    Previous,
    //! Towards the last entry (the down arrow)
    Next
  };

  //! Where the cursor of the keyboard menu goes. The menu is a list of entries, and a entry that can not be used at the moment (a
  //! control that is disabled) is passed over.
  namespace SampleMenuCursorUtil
  {
    //! @brief The entry the cursor moves to from a entry: the nearest one in the direction that can be used. The list goes round, so
    //!        the entry after the last one is the first one.
    //! @param count the number of entries
    //! @param index the entry the cursor is at
    //! @param isEnabled called with the index of a entry, returns true if the entry can be used
    //! @return the index of the entry, or index when no other entry can be used (also for a index that is not in the list)
    template <typename TIsEnabled>
    [[nodiscard]] constexpr std::size_t Move(const std::size_t count, const std::size_t index, const SampleMenuCursorMove move,
                                             const TIsEnabled& isEnabled)
    {
      if (index >= count)
      {
        return index;
      }
      // One step back is count - 1 steps on
      const std::size_t step = move == SampleMenuCursorMove::Next ? 1u : count - 1u;
      std::size_t candidate = index;
      for (std::size_t i = 1; i < count; ++i)
      {
        candidate = (candidate + step) % count;
        if (isEnabled(candidate))
        {
          return candidate;
        }
      }
      return index;
    }

    //! @brief The entry the cursor is shown at when it appears: the entry it was at, or the next one that can be used
    //! @param count the number of entries
    //! @param index the entry the cursor was at
    //! @param isEnabled called with the index of a entry, returns true if the entry can be used
    //! @return the index of the entry, or index when no entry can be used (also for a index that is not in the list)
    template <typename TIsEnabled>
    [[nodiscard]] constexpr std::size_t Nearest(const std::size_t count, const std::size_t index, const TIsEnabled& isEnabled)
    {
      if (index >= count || isEnabled(index))
      {
        return index;
      }
      return Move(count, index, SampleMenuCursorMove::Next, isEnabled);
    }
  }
}

#endif
