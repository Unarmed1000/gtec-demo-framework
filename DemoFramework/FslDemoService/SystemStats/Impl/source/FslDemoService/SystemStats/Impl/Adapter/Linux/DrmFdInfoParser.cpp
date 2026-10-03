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


#include <FslDemoService/SystemStats/Impl/Adapter/Linux/DrmFdInfoParser.hpp>
#include <algorithm>

namespace Fsl::DrmFdInfoParser
{
  namespace
  {
    constexpr std::string_view KeyClientId("drm-client-id");
    constexpr std::string_view KeyEngineCapacityPrefix("drm-engine-capacity-");
    constexpr std::string_view KeyEnginePrefix("drm-engine-");
    constexpr std::string_view KeyResidentPrefix("drm-resident-");
    constexpr std::string_view KeyTotalPrefix("drm-total-");

    constexpr bool IsSpace(const char ch) noexcept
    {
      return ch == ' ' || ch == '\t' || ch == '\r';
    }

    constexpr std::string_view Trim(std::string_view value) noexcept
    {
      while (!value.empty() && IsSpace(value.front()))
      {
        value.remove_prefix(1);
      }
      while (!value.empty() && IsSpace(value.back()))
      {
        value.remove_suffix(1);
      }
      return value;
    }

    //! Parse the unsigned number a value starts with.
    //! @param rRest what follows the number
    bool TryParseNumber(const std::string_view value, uint64_t& rNumber, std::string_view& rRest) noexcept
    {
      std::size_t index = 0;
      uint64_t number = 0;
      while (index < value.size() && value[index] >= '0' && value[index] <= '9')
      {
        const auto digit = static_cast<uint64_t>(value[index] - '0');
        if (number > ((UINT64_MAX - digit) / 10u))
        {
          // Too large to be a real value
          return false;
        }
        number = (number * 10u) + digit;
        ++index;
      }
      if (index == 0u)
      {
        return false;
      }
      rNumber = number;
      rRest = Trim(value.substr(index));
      return true;
    }

    //! A size: a number with a optional unit
    bool TryParseBytes(const std::string_view value, uint64_t& rBytes) noexcept
    {
      uint64_t number = 0;
      std::string_view unit;
      if (!TryParseNumber(value, number, unit))
      {
        return false;
      }
      uint64_t scale = 1;
      if (unit == "KiB")
      {
        scale = 1024u;
      }
      else if (unit == "MiB")
      {
        scale = uint64_t{1024} * 1024u;
      }
      else if (!unit.empty())
      {
        return false;
      }
      if (number > (UINT64_MAX / scale))
      {
        return false;
      }
      rBytes = number * scale;
      return true;
    }

    void ParseLine(const std::string_view key, const std::string_view value, DrmFdInfo& rInfo, uint64_t& rResidentBytes, bool& rHasResident,
                   uint64_t& rTotalBytes, bool& rHasTotal) noexcept
    {
      uint64_t number = 0;
      std::string_view rest;
      if (key == KeyClientId)
      {
        if (TryParseNumber(value, number, rest))
        {
          rInfo.IsDrmClient = true;
          rInfo.ClientId = number;
        }
      }
      else if (key.starts_with(KeyEngineCapacityPrefix))
      {
        // The number of engines of a kind, not a busy time
      }
      else if (key.starts_with(KeyEnginePrefix))
      {
        const std::string_view name = key.substr(KeyEnginePrefix.size());
        if (!name.empty() && name.size() <= DrmFdInfo::MaxEngineNameLength && rInfo.EngineCount < DrmFdInfo::MaxEngines &&
            TryParseNumber(value, number, rest) && rest == "ns")
        {
          DrmFdInfo::Engine& rEngine = rInfo.Engines[rInfo.EngineCount];
          std::copy(name.begin(), name.end(), rEngine.Name.begin());
          rEngine.NameLength = static_cast<uint32_t>(name.size());
          rEngine.BusyNanoseconds = number;
          ++rInfo.EngineCount;
        }
      }
      else if (key.starts_with(KeyResidentPrefix))
      {
        if (TryParseBytes(value, number))
        {
          rResidentBytes += number;
          rHasResident = true;
        }
      }
      else if (key.starts_with(KeyTotalPrefix))
      {
        if (TryParseBytes(value, number))
        {
          rTotalBytes += number;
          rHasTotal = true;
        }
      }
    }
  }


  DrmFdInfo Parse(const std::string_view content) noexcept
  {
    DrmFdInfo info;
    uint64_t residentBytes = 0;
    uint64_t totalBytes = 0;
    bool hasResident = false;
    bool hasTotal = false;

    std::string_view remaining = content;
    while (!remaining.empty())
    {
      const std::size_t lineEnd = remaining.find('\n');
      const std::string_view line = remaining.substr(0, lineEnd);
      remaining = lineEnd == std::string_view::npos ? std::string_view() : remaining.substr(lineEnd + 1u);

      const std::size_t separator = line.find(':');
      if (separator != std::string_view::npos)
      {
        ParseLine(Trim(line.substr(0, separator)), Trim(line.substr(separator + 1u)), info, residentBytes, hasResident, totalBytes, hasTotal);
      }
    }

    if (!info.IsDrmClient)
    {
      // The keys of something that is not a DRM client mean nothing
      return {};
    }
    // What is resident is what the client really uses, a driver that does not report it reports what is allocated
    info.HasMemory = hasResident || hasTotal;
    info.MemoryBytes = hasResident ? residentBytes : totalBytes;
    return info;
  }
}
