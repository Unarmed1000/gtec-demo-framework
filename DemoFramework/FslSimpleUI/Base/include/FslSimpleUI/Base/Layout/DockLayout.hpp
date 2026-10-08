#ifndef FSLSIMPLEUI_BASE_LAYOUT_DOCKLAYOUT_HPP
#define FSLSIMPLEUI_BASE_LAYOUT_DOCKLAYOUT_HPP
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

#include <FslSimpleUI/Base/Layout/ComplexLayout.hpp>
#include <FslSimpleUI/Base/Layout/DockType.hpp>
#include <memory>

namespace Fsl::UI
{
  struct DockLayoutWindowRecord : GenericWindowCollectionRecordBase
  {
    DockType Dock{DockType::Left};

    explicit DockLayoutWindowRecord(const std::shared_ptr<BaseWindow>& window)
      : GenericWindowCollectionRecordBase(window)
    {
    }
  };

  //! A layout that places its children at the edges of its area, in the order they were added in. A child takes a band at its edge:
  //! at the left or the right it is as wide as it asks for and as high as the space that is left, at the top or the bottom it is as
  //! high as it asks for and as wide as the space that is left. The next child gets what the children before it left.
  //!
  //! With LastChildFill the last child gets all of the space that is left, whatever edge it was added for.
  //!
  //! A child is never placed outside the area of the layout, so a child that asks for more than there is does not push the others out
  //! (which a star row or column of a GridLayout does: it is never smaller than its content asks for).
  class DockLayout : public ComplexLayout<DockLayoutWindowRecord>
  {
    using base_type = ComplexLayout<DockLayoutWindowRecord>;

    bool m_lastChildFill{false};
    bool m_limitToAvailableSpace{false};

  public:
    explicit DockLayout(const std::shared_ptr<BaseWindowContext>& context);

    //! @brief If the last child gets all of the space the children before it left (default: false)
    [[nodiscard]] bool GetLastChildFill() const noexcept
    {
      return m_lastChildFill;
    }

    bool SetLastChildFill(const bool value);

    //! @brief If the size the layout asks for is no larger than the space it is offered (default: false). With it the layout does not
    //!        ask for more because a child does.
    [[nodiscard]] bool GetLimitToAvailableSpace() const noexcept
    {
      return m_limitToAvailableSpace;
    }

    bool SetLimitToAvailableSpace(const bool value);

    //! @brief Add a child at the left edge
    using base_type::AddChild;

    //! @brief Add a child at a edge of the space the children before it left
    void AddChild(const std::shared_ptr<BaseWindow>& window, const DockType dock);

  protected:
    PxSize2D ArrangeOverride(const PxSize2D& finalSizePx) override;
    PxSize2D MeasureOverride(const PxAvailableSize& availableSizePx) override;
  };
}

#endif
