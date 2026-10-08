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

#include <FslSimpleUI/Base/BaseWindowContext.hpp>
#include <FslSimpleUI/Base/PropertyTypeFlags.hpp>
#include <FslSimpleUI/Base/UIDrawContext.hpp>
#include <FslSimpleUI/Base/WindowFlags.hpp>
#include <FslSimpleUI/Render/Base/DrawCommandBuffer.hpp>
#include <Shared/FramePacing/SampleMenuRow.hpp>

namespace Fsl
{
  SampleMenuRow::SampleMenuRow(const std::shared_ptr<UI::BaseWindowContext>& context)
    : UI::ContentControl(context)
    , m_fill(context->TheUIContext.Get()->MeshManager)
  {
    Enable(UI::WindowFlags::DrawEnabled);
  }


  void SampleMenuRow::SetFillSprite(const std::shared_ptr<ISizedSprite>& value)
  {
    if (m_fill.SetSprite(value))
    {
      PropertyUpdated(UI::PropertyType::Content);
    }
  }


  void SampleMenuRow::SetHighlightColor(const UI::UIColor value)
  {
    if (value != m_highlightColor)
    {
      m_highlightColor = value;
      PropertyUpdated(UI::PropertyType::Other);
    }
  }


  void SampleMenuRow::SetHighlighted(const bool value)
  {
    if (value != m_isHighlighted)
    {
      m_isHighlighted = value;
      PropertyUpdated(UI::PropertyType::Other);
    }
  }


  void SampleMenuRow::WinDraw(const UI::UIDrawContext& context)
  {
    base_type::WinDraw(context);

    if (m_isHighlighted)
    {
      // The bar is drawn before the content, which is a child window
      context.CommandBuffer.Draw(m_fill.Get(), context.TargetRect.Location(), RenderSizePx(),
                                 GetFinalBaseColor() * GetContext()->ColorConverter.Convert(m_highlightColor), context.ClipContext);
    }
  }
}
