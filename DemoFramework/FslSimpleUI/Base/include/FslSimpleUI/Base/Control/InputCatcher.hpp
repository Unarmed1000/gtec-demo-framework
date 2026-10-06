#ifndef FSLSIMPLEUI_BASE_CONTROL_INPUTCATCHER_HPP
#define FSLSIMPLEUI_BASE_CONTROL_INPUTCATCHER_HPP
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

#include <FslSimpleUI/Base/Control/ContentControl.hpp>
#include <memory>

namespace Fsl::UI
{
  //! @brief Takes every click on its area: a click its content leaves unhandled (on a label, a background, the space between controls or a
  //!        ScrollViewer that does not scroll) is marked handled, so it does not reach the app.
  //! @note  A app only gets a click the UI did not handle (MouseButtonEvent::IsHandled). Most windows do not take clicks, so put a panel that
  //!        is drawn on top of the app's scene inside a InputCatcher to keep a click on the panel from reaching the scene (a camera drag).
  //!        The controls inside it work as before, the InputCatcher only marks the click once they had it.
  class InputCatcher : public ContentControl
  {
    using base_type = ContentControl;

  public:
    explicit InputCatcher(const std::shared_ptr<BaseWindowContext>& context);

  protected:
    void OnClickInput(const std::shared_ptr<WindowInputClickEvent>& theEvent) override;
    void OnScrollWheelInput(const std::shared_ptr<WindowInputScrollWheelEvent>& theEvent) override;
  };
}

#endif
