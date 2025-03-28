#ifndef SHARED_UI_BENCHMARK_PERSISTENCE_INPUT_APPINPUTCOMMANDLIST_HPP
#define SHARED_UI_BENCHMARK_PERSISTENCE_INPUT_APPINPUTCOMMANDLIST_HPP
/****************************************************************************************************************************************************
 * Copyright 2021, 2023 NXP
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *
 *    * Neither the name of the NXP. nor the names of
 *      its contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************************************************************************************/

#include <FslBase/Math/Pixel/PxPoint2.hpp>
#include <FslBase/Math/Pixel/PxSize2D.hpp>
#include <FslBase/Span/ReadOnlySpan.hpp>
#include <Shared/UI/Benchmark/Persistence/Input/InputCommandRecord.hpp>
#include <vector>

namespace Fsl
{
  class AppInputCommandList
  {
    PxSize2D m_recordResolution;
    uint32_t m_recordDensityDpi{0};
    std::vector<InputCommandRecord> m_entries;
    uint32_t m_frameCount{0};

  public:
    AppInputCommandList& operator=(const AppInputCommandList& other) = default;
    AppInputCommandList(const AppInputCommandList& other) = default;

    // move assignment operator
    AppInputCommandList& operator=(AppInputCommandList&& other) noexcept
    {
      if (this != &other)
      {
        // Claim ownership here
        m_recordResolution = other.m_recordResolution;
        m_recordDensityDpi = other.m_recordDensityDpi;
        m_entries = std::move(other.m_entries);
        m_frameCount = other.m_frameCount;

        // Remove the data from other
        other.m_recordResolution = {};
        other.m_recordDensityDpi = 0;
        other.m_frameCount = 0;
      }
      return *this;
    }
    // move constructor
    AppInputCommandList(AppInputCommandList&& other) noexcept
      : m_recordResolution(other.m_recordResolution)
      , m_recordDensityDpi(other.m_recordDensityDpi)
      , m_entries(std::move(other.m_entries))
      , m_frameCount(other.m_frameCount)
    {
      // Remove the data from other
      other.m_recordResolution = {};
      other.m_recordDensityDpi = 0;
      other.m_frameCount = 0;
    }

    AppInputCommandList() = default;
    AppInputCommandList(const PxSize2D recordResolution, const uint32_t recordDensityDpi, const ReadOnlySpan<InputCommandRecord> entries,
                        const uint32_t frameCount);
    AppInputCommandList(const PxSize2D recordResolution, const uint32_t recordDensityDpi, std::vector<InputCommandRecord> entries,
                        const uint32_t frameCount);

    PxSize2D GetRecordResolution() const noexcept
    {
      return m_recordResolution;
    }

    uint32_t GetRecordDensityDpi() const noexcept
    {
      return m_recordDensityDpi;
    }

    uint32_t GetFrameCount() const noexcept
    {
      return m_frameCount;
    }

    void SetFrameCount(const uint32_t frameCount);
    void SetRecordInfo(const PxSize2D recordResolution, const uint32_t recordDensityDpi);

    const std::vector<InputCommandRecord>& AsVector() const noexcept
    {
      return m_entries;
    }

    std::vector<InputCommandRecord>& AsVector() noexcept
    {
      return m_entries;
    }

    ReadOnlySpan<InputCommandRecord> AsSpan() const;
  };
}

#endif
