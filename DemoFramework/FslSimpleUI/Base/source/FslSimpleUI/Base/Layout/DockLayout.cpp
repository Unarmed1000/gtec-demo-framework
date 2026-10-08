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
//* Inspired by WPF DockPanel which was released under the MIT license
//****************************************************************************************************************************************************

#include <FslBase/Math/Pixel/PxRectangle.hpp>
#include <FslBase/Math/Pixel/PxSize1D.hpp>
#include <FslBase/Math/Pixel/PxSize2D.hpp>
#include <FslBase/Math/Pixel/PxValue.hpp>
#include <FslSimpleUI/Base/Layout/DockLayout.hpp>
#include <FslSimpleUI/Base/PropertyTypeFlags.hpp>
#include <FslSimpleUI/Base/PxAvailableSize.hpp>
#include <cassert>
#include <cstddef>

namespace Fsl::UI
{
  DockLayout::DockLayout(const std::shared_ptr<BaseWindowContext>& context)
    : ComplexLayout<DockLayoutWindowRecord>(context)
  {
  }


  bool DockLayout::SetLastChildFill(const bool value)
  {
    const bool changed = value != m_lastChildFill;
    if (changed)
    {
      m_lastChildFill = value;
      PropertyUpdated(PropertyType::Layout);
    }
    return changed;
  }


  bool DockLayout::SetLimitToAvailableSpace(const bool value)
  {
    const bool changed = value != m_limitToAvailableSpace;
    if (changed)
    {
      m_limitToAvailableSpace = value;
      PropertyUpdated(PropertyType::Layout);
    }
    return changed;
  }


  void DockLayout::AddChild(const std::shared_ptr<BaseWindow>& window, const DockType dock)
  {
    base_type::AddChild(window);
    // The record of the child that was just added
    assert(!empty());
    (end() - 1)->Dock = dock;
  }


  PxSize2D DockLayout::ArrangeOverride(const PxSize2D& finalSizePx)
  {
    const std::size_t childCount = size();
    const std::size_t nonFillChildCount = childCount - ((m_lastChildFill && childCount > 0u) ? 1u : 0u);

    // The bands the children took so far at each edge
    PxSize1D accumulatedLeftPx;
    PxSize1D accumulatedTopPx;
    PxSize1D accumulatedRightPx;
    PxSize1D accumulatedBottomPx;

    std::size_t index = 0;
    for (const auto& entry : *this)
    {
      assert(entry.Window);
      const PxSize2D childDesiredSizePx = entry.Window->DesiredSizePx();

      // The space that is left (the bands can be larger than the layout, the space that is left is not less than nothing)
      PxValue rectLeftPx = accumulatedLeftPx.Value();
      PxValue rectTopPx = accumulatedTopPx.Value();
      PxSize1D rectWidthPx(finalSizePx.Width() - (accumulatedLeftPx + accumulatedRightPx));
      PxSize1D rectHeightPx(finalSizePx.Height() - (accumulatedTopPx + accumulatedBottomPx));

      if (index < nonFillChildCount)
      {
        switch (entry.Dock)
        {
        case DockType::Left:
          accumulatedLeftPx += childDesiredSizePx.Width();
          rectWidthPx = childDesiredSizePx.Width();
          break;
        case DockType::Right:
          accumulatedRightPx += childDesiredSizePx.Width();
          rectLeftPx = PxSize1D(finalSizePx.Width() - accumulatedRightPx).Value();
          rectWidthPx = childDesiredSizePx.Width();
          break;
        case DockType::Top:
          accumulatedTopPx += childDesiredSizePx.Height();
          rectHeightPx = childDesiredSizePx.Height();
          break;
        case DockType::Bottom:
          accumulatedBottomPx += childDesiredSizePx.Height();
          rectTopPx = PxSize1D(finalSizePx.Height() - accumulatedBottomPx).Value();
          rectHeightPx = childDesiredSizePx.Height();
          break;
        }
      }
      entry.Window->Arrange(PxRectangle(rectLeftPx, rectTopPx, rectWidthPx, rectHeightPx));
      ++index;
    }
    // The layout is as large as the area it got, so there is nothing to limit here
    return finalSizePx;
  }


  PxSize2D DockLayout::MeasureOverride(const PxAvailableSize& availableSizePx)
  {
    // The size the layout needs for the children so far
    PxSize1D parentWidthPx;
    PxSize1D parentHeightPx;
    // The size the children took so far
    PxSize1D accumulatedWidthPx;
    PxSize1D accumulatedHeightPx;

    for (const auto& entry : *this)
    {
      assert(entry.Window);
      // A child is offered the space the children before it left
      entry.Window->Measure(PxAvailableSize::Subtract(availableSizePx, PxSize2D(accumulatedWidthPx, accumulatedHeightPx)));
      const PxSize2D childDesiredSizePx = entry.Window->DesiredSizePx();

      // A child takes space along the axis of its band only: a child at the left makes the layout as high as it is (after the
      // children above and below it), but it takes no height from the children after it.
      switch (entry.Dock)
      {
      case DockType::Left:
      case DockType::Right:
        parentHeightPx = PxSize1D::Max(parentHeightPx, accumulatedHeightPx + childDesiredSizePx.Height());
        accumulatedWidthPx += childDesiredSizePx.Width();
        break;
      case DockType::Top:
      case DockType::Bottom:
        parentWidthPx = PxSize1D::Max(parentWidthPx, accumulatedWidthPx + childDesiredSizePx.Width());
        accumulatedHeightPx += childDesiredSizePx.Height();
        break;
      }
    }

    // What the last children took is part of the size too
    parentWidthPx = PxSize1D::Max(parentWidthPx, accumulatedWidthPx);
    parentHeightPx = PxSize1D::Max(parentHeightPx, accumulatedHeightPx);

    const PxSize2D desiredSizePx(parentWidthPx, parentHeightPx);
    return !m_limitToAvailableSpace ? desiredSizePx : PxAvailableSize::MinPxSize2D(desiredSizePx, availableSizePx);
  }
}
