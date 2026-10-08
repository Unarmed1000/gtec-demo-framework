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

#include <FslDemoService/Trace/Impl/TraceAnonymiser.hpp>
#include <algorithm>
#include <utility>

namespace Fsl
{
  namespace
  {
    constexpr bool IsSeparator(const char ch) noexcept
    {
      return ch == '/' || ch == '\\';
    }

    constexpr char ToLower(const char ch) noexcept
    {
      return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch - 'A' + 'a') : ch;
    }

    //! @return the number of characters of the text that match the pattern from the position on, zero if it does not match there.
    std::size_t MatchAt(const std::string_view text, const std::size_t position, const std::string_view pattern, const bool isDirectory) noexcept
    {
      std::size_t textIndex = position;
      std::size_t patternIndex = 0;
      while (patternIndex < pattern.size())
      {
        if (textIndex >= text.size())
        {
          return 0;
        }
        if (isDirectory && IsSeparator(pattern[patternIndex]))
        {
          if (!IsSeparator(text[textIndex]))
          {
            return 0;
          }
          // A run of separators is one separator
          while (patternIndex < pattern.size() && IsSeparator(pattern[patternIndex]))
          {
            ++patternIndex;
          }
          while (textIndex < text.size() && IsSeparator(text[textIndex]))
          {
            ++textIndex;
          }
          continue;
        }
        if (ToLower(text[textIndex]) != ToLower(pattern[patternIndex]))
        {
          return 0;
        }
        ++textIndex;
        ++patternIndex;
      }
      return textIndex - position;
    }

    //! @return the number of parts of a directory, a drive ("c:") is one
    std::size_t CountDirectoryParts(const std::string_view directory) noexcept
    {
      std::size_t count = 0;
      std::size_t index = 0;
      while (index < directory.size())
      {
        while (index < directory.size() && IsSeparator(directory[index]))
        {
          ++index;
        }
        const std::size_t begin = index;
        while (index < directory.size() && !IsSeparator(directory[index]))
        {
          ++index;
        }
        if (index > begin)
        {
          ++count;
        }
      }
      return count;
    }
  }


  void TraceAnonymiser::AddText(const std::string_view text, const std::string_view replacement)
  {
    if (text.size() < MinTextLength || text == replacement)
    {
      return;
    }
    Add(Entry{std::string(text), std::string(replacement), false});
  }


  void TraceAnonymiser::AddDirectory(const std::string_view directory, const std::string_view replacement)
  {
    std::string_view trimmed = directory;
    while (!trimmed.empty() && IsSeparator(trimmed.back()))
    {
      trimmed.remove_suffix(1);
    }
    // A drive or the root alone is not a directory worth hiding, and it would match far too much
    if (trimmed.size() < MinTextLength || CountDirectoryParts(trimmed) < 2u)
    {
      return;
    }
    Add(Entry{std::string(trimmed), std::string(replacement), true});
  }


  bool TraceAnonymiser::Apply(std::string& rText) const
  {
    bool changed = false;
    for (const Entry& entry : m_entries)
    {
      std::size_t position = 0;
      while (position < rText.size())
      {
        const std::size_t length = MatchAt(rText, position, entry.Text, entry.IsDirectory);
        if (length > 0u)
        {
          rText.replace(position, length, entry.Replacement);
          position += entry.Replacement.size();
          changed = true;
        }
        else
        {
          ++position;
        }
      }
    }
    return changed;
  }


  void TraceAnonymiser::Add(Entry entry)
  {
    const auto itrFind =
      std::find_if(m_entries.begin(), m_entries.end(), [&entry](const Entry& existing) { return existing.Text.size() < entry.Text.size(); });
    m_entries.insert(itrFind, std::move(entry));
  }
}
