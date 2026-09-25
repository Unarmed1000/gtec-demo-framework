#ifndef FSLDEMOSERVICE_FRAMEPACING_IMPL_FRAMEPACINGSERVICEOPTIONPARSER_HPP
#define FSLDEMOSERVICE_FRAMEPACING_IMPL_FRAMEPACINGSERVICEOPTIONPARSER_HPP
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
#include <FslDemoService/FramePacing/FramePacingMarkerSlot.hpp>
#include <FslService/Impl/AServiceOptionParser.hpp>
#include <cstdint>
#include <optional>
#include <string>

namespace Fsl
{
  class FramePacingServiceOptionParser final : public AServiceOptionParser
  {
    bool m_enabled{false};
    int32_t m_moduleSizePx;
    int32_t m_captureHeightPx{0};
    FramePacingMarkerSlot m_slot{FramePacingMarkerSlot::TopLeft};
    std::optional<std::string> m_runName;
    TimeSpan m_runDuration;
    std::optional<uint32_t> m_runId;

  public:
    FramePacingServiceOptionParser();

    [[nodiscard]] std::string GetName() const final
    {
      return {"FramePacingServiceOptionParser"};
    }

    void OnArgumentSetup(std::deque<Option>& rOptions) final;
    OptionParseResult OnParse(const int32_t cmdId, const StringViewLite& strOptArg) final;
    bool OnParsingComplete() final;

    //! @brief true if the marker should be drawn from the start
    [[nodiscard]] bool IsEnabled() const noexcept
    {
      return m_enabled;
    }

    [[nodiscard]] int32_t GetModuleSizePx() const noexcept
    {
      return m_moduleSizePx;
    }

    [[nodiscard]] int32_t GetCaptureHeightPx() const noexcept
    {
      return m_captureHeightPx;
    }

    [[nodiscard]] FramePacingMarkerSlot GetSlot() const noexcept
    {
      return m_slot;
    }

    //! @brief If set a run with this name is started at the first frame
    [[nodiscard]] const std::optional<std::string>& GetRunName() const noexcept
    {
      return m_runName;
    }

    //! @brief The duration of the measured part of the automatically started run (zero = until the app exits)
    [[nodiscard]] TimeSpan GetRunDuration() const noexcept
    {
      return m_runDuration;
    }

    //! @brief If set this is used as the id of the first run
    [[nodiscard]] std::optional<uint32_t> GetRunId() const noexcept
    {
      return m_runId;
    }
  };
}

#endif
