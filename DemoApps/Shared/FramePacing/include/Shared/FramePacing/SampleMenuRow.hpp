#ifndef SHARED_FRAMEPACING_SAMPLEMENUROW_HPP
#define SHARED_FRAMEPACING_SAMPLEMENUROW_HPP
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

#include <FslGraphics/Sprite/ISizedSprite.hpp>
#include <FslSimpleUI/Base/Control/ContentControl.hpp>
#include <FslSimpleUI/Base/Mesh/SizedSpriteMesh.hpp>
#include <FslSimpleUI/Base/UIColor.hpp>
#include <memory>

namespace Fsl
{
  //! A row of the keyboard menu (SampleKeyboardMenu): what the cursor can be at, with the bar that shows the cursor behind it.
  //! The row is as large as its content and places it as it is placed without the row, and it draws nothing while the cursor is
  //! somewhere else, so a list looks the same with the rows as without them.
  class SampleMenuRow final : public UI::ContentControl
  {
    using base_type = UI::ContentControl;

    UI::SizedSpriteMesh m_fill;
    UI::UIColor m_highlightColor;
    bool m_isHighlighted{false};

  public:
    explicit SampleMenuRow(const std::shared_ptr<UI::BaseWindowContext>& context);

    //! @brief The sprite the bar is drawn with: a fill sprite that can be seen through
    void SetFillSprite(const std::shared_ptr<ISizedSprite>& value);

    //! @brief The color of the bar
    void SetHighlightColor(const UI::UIColor value);

    [[nodiscard]] bool IsHighlighted() const noexcept
    {
      return m_isHighlighted;
    }

    //! @brief Show or hide the bar: the cursor is at this row, or not
    void SetHighlighted(const bool value);

    void WinDraw(const UI::UIDrawContext& context) final;
  };
}

#endif
