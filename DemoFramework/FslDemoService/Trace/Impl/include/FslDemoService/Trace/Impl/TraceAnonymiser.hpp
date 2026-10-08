#ifndef FSLDEMOSERVICE_TRACE_IMPL_TRACEANONYMISER_HPP
#define FSLDEMOSERVICE_TRACE_IMPL_TRACEANONYMISER_HPP
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


#include <string>
#include <string_view>
#include <vector>

namespace Fsl
{
  //! Replaces what names the machine in a text of the trace: the hardware names it was told and the directories it was told.
  //!
  //! A text is found however its letters are cased. A directory is found however its separators are written as well ('/' or '\', one
  //! or more), as the same directory is spelled in more than one way on a machine.
  class TraceAnonymiser final
  {
    struct Entry
    {
      std::string Text;
      std::string Replacement;
      bool IsDirectory{false};
    };

    //! The longest first, so a directory inside another one is replaced as itself
    std::vector<Entry> m_entries;

  public:
    //! The shortest text that is replaced: a shorter one would match far too much
    static constexpr std::size_t MinTextLength = 3;

    //! @brief Replace a text. A text that is too short, or equal to what replaces it, is ignored.
    void AddText(const std::string_view text, const std::string_view replacement);

    //! @brief Replace a directory. A drive or the root alone is ignored.
    void AddDirectory(const std::string_view directory, const std::string_view replacement);

    [[nodiscard]] bool IsEmpty() const noexcept
    {
      return m_entries.empty();
    }

    //! @brief Replace everything that was added in the text.
    //! @return true if the text was changed.
    bool Apply(std::string& rText) const;

  private:
    void Add(Entry entry);
  };
}

#endif
