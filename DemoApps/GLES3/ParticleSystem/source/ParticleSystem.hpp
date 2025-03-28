#ifndef GLES3_PARTICLESYSTEM_PARTICLESYSTEM_HPP
#define GLES3_PARTICLESYSTEM_PARTICLESYSTEM_HPP
/****************************************************************************************************************************************************
 * Copyright (c) 2015 Freescale Semiconductor, Inc.
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
 *    * Neither the name of the Freescale Semiconductor, Inc. nor the names of
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

#include <FslBase/Math/Matrix.hpp>
#include <FslDemoApp/OpenGLES3/DemoAppGLES3.hpp>
#include <FslSimpleUI/App/UIDemoAppExtension.hpp>
#include "AScene.hpp"

namespace Fsl
{
  class ParticleSystem
    : public DemoAppGLES3
    , public UI::EventListener
  {
    using base_type = DemoAppGLES3;

    UI::CallbackEventListenerScope m_uiEventListener;
    std::shared_ptr<UIDemoAppExtension> m_uiExtension;
    std::shared_ptr<AScene> m_scene;

  public:
    explicit ParticleSystem(const DemoAppConfig& config);
    ~ParticleSystem() override;
    void OnSelect(const std::shared_ptr<UI::WindowSelectEvent>& theEvent) final;
    void OnContentChanged(const std::shared_ptr<UI::WindowContentChangedEvent>& theEvent) final;

  protected:
    void OnKeyEvent(const KeyEvent& event) final;
    void OnMouseButtonEvent(const MouseButtonEvent& event) final;
    void OnMouseMoveEvent(const MouseMoveEvent& event) final;
    void OnMouseWheelEvent(const MouseWheelEvent& event) final;
    void ConfigurationChanged(const DemoWindowMetrics& windowMetrics) final;
    void Update(const DemoTime& demoTime) final;
    void Draw(const FrameInfo& frameInfo) final;

  private:
  };
}

#endif
