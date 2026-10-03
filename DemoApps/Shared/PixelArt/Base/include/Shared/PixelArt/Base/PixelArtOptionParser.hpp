#ifndef SHARED_PIXELART_BASE_PIXELARTOPTIONPARSER_HPP
#define SHARED_PIXELART_BASE_PIXELARTOPTIONPARSER_HPP
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

#include <FslDemoApp/Base/ADemoOptionParser.hpp>
#include <string>
#include <utility>
#include <vector>

namespace Fsl
{
  //! The command line options of the PixelArt apps
  class PixelArtOptionParser : public ADemoOptionParser
  {
    std::string m_scene;
    float m_renderScale{1.0f};
    float m_timeSpeed{1.0f};
    float m_startTime{0.0f};
    bool m_paused{false};
    bool m_hideUI{false};
    bool m_gpuChart{false};
    std::vector<std::pair<std::string, float>> m_params;

  public:
    PixelArtOptionParser();
    ~PixelArtOptionParser() override;

    //! The scene to start with: the name of its folder or its number in the list (empty = the first scene)
    [[nodiscard]] const std::string& GetScene() const noexcept
    {
      return m_scene;
    }

    [[nodiscard]] float GetRenderScale() const noexcept
    {
      return m_renderScale;
    }

    [[nodiscard]] float GetTimeSpeed() const noexcept
    {
      return m_timeSpeed;
    }

    //! The iTime the scene starts at in seconds
    [[nodiscard]] float GetStartTime() const noexcept
    {
      return m_startTime;
    }

    [[nodiscard]] bool IsPaused() const noexcept
    {
      return m_paused;
    }

    [[nodiscard]] bool IsUIHidden() const noexcept
    {
      return m_hideUI;
    }

    [[nodiscard]] bool IsGpuChartEnabled() const noexcept
    {
      return m_gpuChart;
    }

    //! The params of the start scene that are given on the command line
    [[nodiscard]] const std::vector<std::pair<std::string, float>>& GetParams() const noexcept
    {
      return m_params;
    }

  protected:
    void OnArgumentSetup(std::deque<Option>& rOptions) override;
    OptionParseResult OnParse(const int32_t cmdId, const StringViewLite& strOptArg) override;
    bool OnParsingComplete() override;
  };
}

#endif
