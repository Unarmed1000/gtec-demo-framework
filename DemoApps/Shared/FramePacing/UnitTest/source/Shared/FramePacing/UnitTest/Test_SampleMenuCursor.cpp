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

#include <FslBase/UnitTest/Helper/Common.hpp>
#include <FslBase/UnitTest/Helper/TestFixtureFslBase.hpp>
#include <Shared/FramePacing/SampleMenuCursor.hpp>
#include <array>
#include <cstddef>

using namespace Fsl;

namespace
{
  using TestSampleMenuCursor = TestFixtureFslBase;

  constexpr SampleMenuCursorMove Previous = SampleMenuCursorMove::Previous;
  constexpr SampleMenuCursorMove Next = SampleMenuCursorMove::Next;

  //! A menu of the entries of the array: true for a entry that can be used
  template <std::size_t TCount>
  constexpr std::size_t Move(const std::array<bool, TCount>& enabled, const std::size_t index, const SampleMenuCursorMove move)
  {
    return SampleMenuCursorUtil::Move(enabled.size(), index, move, [&enabled](const std::size_t entryIndex) { return enabled[entryIndex]; });
  }

  template <std::size_t TCount>
  constexpr std::size_t Nearest(const std::array<bool, TCount>& enabled, const std::size_t index)
  {
    return SampleMenuCursorUtil::Nearest(enabled.size(), index, [&enabled](const std::size_t entryIndex) { return enabled[entryIndex]; });
  }

  constexpr std::array<bool, 5> AllEnabled = {true, true, true, true, true};
  constexpr std::array<bool, 5> NoneEnabled = {false, false, false, false, false};
  //! The second, the third and the last entry can not be used
  constexpr std::array<bool, 5> SomeEnabled = {true, false, false, true, false};
  constexpr std::array<bool, 5> OneEnabled = {false, false, true, false, false};
}


TEST(TestSampleMenuCursor, Move_Next)
{
  EXPECT_EQ(1u, Move(AllEnabled, 0, Next));
  EXPECT_EQ(4u, Move(AllEnabled, 3, Next));
}


TEST(TestSampleMenuCursor, Move_Previous)
{
  EXPECT_EQ(0u, Move(AllEnabled, 1, Previous));
  EXPECT_EQ(3u, Move(AllEnabled, 4, Previous));
}


TEST(TestSampleMenuCursor, Move_GoesRoundAtTheEnds)
{
  EXPECT_EQ(0u, Move(AllEnabled, 4, Next));
  EXPECT_EQ(4u, Move(AllEnabled, 0, Previous));
}


TEST(TestSampleMenuCursor, Move_PassesOverTheEntriesThatCanNotBeUsed)
{
  EXPECT_EQ(3u, Move(SomeEnabled, 0, Next));
  EXPECT_EQ(0u, Move(SomeEnabled, 3, Previous));
  // Around the end, over the last entry
  EXPECT_EQ(0u, Move(SomeEnabled, 3, Next));
  EXPECT_EQ(3u, Move(SomeEnabled, 0, Previous));
}


TEST(TestSampleMenuCursor, Move_FromAEntryThatCanNotBeUsed)
{
  // The control the cursor is at was disabled
  EXPECT_EQ(3u, Move(SomeEnabled, 1, Next));
  EXPECT_EQ(0u, Move(SomeEnabled, 1, Previous));
  EXPECT_EQ(0u, Move(SomeEnabled, 4, Next));
  EXPECT_EQ(3u, Move(SomeEnabled, 4, Previous));
}


TEST(TestSampleMenuCursor, Move_TheOnlyEntryThatCanBeUsed_Stays)
{
  EXPECT_EQ(2u, Move(OneEnabled, 2, Next));
  EXPECT_EQ(2u, Move(OneEnabled, 2, Previous));
  // And it is found from every other entry
  EXPECT_EQ(2u, Move(OneEnabled, 0, Next));
  EXPECT_EQ(2u, Move(OneEnabled, 0, Previous));
  EXPECT_EQ(2u, Move(OneEnabled, 4, Next));
  EXPECT_EQ(2u, Move(OneEnabled, 4, Previous));
}


TEST(TestSampleMenuCursor, Move_NoEntryCanBeUsed_Stays)
{
  EXPECT_EQ(0u, Move(NoneEnabled, 0, Next));
  EXPECT_EQ(3u, Move(NoneEnabled, 3, Next));
  EXPECT_EQ(3u, Move(NoneEnabled, 3, Previous));
}


TEST(TestSampleMenuCursor, Move_OneEntry)
{
  const std::array<bool, 1> enabled = {true};
  const std::array<bool, 1> disabled = {false};

  EXPECT_EQ(0u, Move(enabled, 0, Next));
  EXPECT_EQ(0u, Move(enabled, 0, Previous));
  EXPECT_EQ(0u, Move(disabled, 0, Next));
  EXPECT_EQ(0u, Move(disabled, 0, Previous));
}


TEST(TestSampleMenuCursor, Move_NoEntries_OrAIndexThatIsNotInTheList_Stays)
{
  const auto anyEntry = [](const std::size_t /*entryIndex*/) { return true; };

  EXPECT_EQ(0u, SampleMenuCursorUtil::Move(0, 0, Next, anyEntry));
  EXPECT_EQ(0u, SampleMenuCursorUtil::Move(0, 0, Previous, anyEntry));
  EXPECT_EQ(7u, SampleMenuCursorUtil::Move(5, 7, Next, anyEntry));
  EXPECT_EQ(5u, SampleMenuCursorUtil::Move(5, 5, Previous, anyEntry));
}


TEST(TestSampleMenuCursor, Nearest_AEntryThatCanBeUsed_IsTheEntry)
{
  EXPECT_EQ(0u, Nearest(AllEnabled, 0));
  EXPECT_EQ(4u, Nearest(AllEnabled, 4));
  EXPECT_EQ(3u, Nearest(SomeEnabled, 3));
}


TEST(TestSampleMenuCursor, Nearest_AEntryThatCanNotBeUsed_IsTheNextThatCan)
{
  EXPECT_EQ(3u, Nearest(SomeEnabled, 1));
  EXPECT_EQ(3u, Nearest(SomeEnabled, 2));
  // Around the end
  EXPECT_EQ(0u, Nearest(SomeEnabled, 4));
  EXPECT_EQ(2u, Nearest(OneEnabled, 3));
}


TEST(TestSampleMenuCursor, Nearest_NoEntryCanBeUsed_Stays)
{
  EXPECT_EQ(0u, Nearest(NoneEnabled, 0));
  EXPECT_EQ(3u, Nearest(NoneEnabled, 3));
}


TEST(TestSampleMenuCursor, Nearest_NoEntries_OrAIndexThatIsNotInTheList_Stays)
{
  const auto anyEntry = [](const std::size_t /*entryIndex*/) { return true; };

  EXPECT_EQ(0u, SampleMenuCursorUtil::Nearest(0, 0, anyEntry));
  EXPECT_EQ(7u, SampleMenuCursorUtil::Nearest(5, 7, anyEntry));
}
