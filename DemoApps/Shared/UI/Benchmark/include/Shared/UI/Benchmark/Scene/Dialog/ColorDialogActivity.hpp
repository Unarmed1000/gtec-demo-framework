#ifndef SHARED_UI_BENCHMARK_SCENE_DIALOG_COLORDIALOGACTIVITY_HPP
#define SHARED_UI_BENCHMARK_SCENE_DIALOG_COLORDIALOGACTIVITY_HPP
/****************************************************************************************************************************************************
 * Copyright 2022 NXP
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

#include <FslSimpleUI/Base/Control/SliderAndFmtValueLabel.hpp>
#include <FslSimpleUI/Theme/Base/WindowType.hpp>
#include <Shared/UI/Benchmark/Activity/DialogActivity.hpp>
#include <memory>

namespace Fsl::UI
{
  class ButtonBase;
  class IRenderSystem;
  class Image;

  class ColorDialogActivity final : public DialogActivity
  {
    enum class State
    {
      Ready,
      Closing,
    };

    State m_state{State::Ready};
    std::shared_ptr<ButtonBase> m_buttonBack;
    std::shared_ptr<Image> m_image;

  public:
    // Since we dont have a proper activity concept with async return values we use the shared settings object for now
    ColorDialogActivity(std::weak_ptr<IActivityStack> activityStack, const std::shared_ptr<Theme::IThemeControlFactory>& themeControlFactory,
                        const UIColor initialColor);
    ~ColorDialogActivity() override;

    UI::UIColor GetCurrentColor() const;

    void OnSelect(const std::shared_ptr<WindowSelectEvent>& theEvent) final;
    void OnKeyEvent(const KeyEvent& theEvent) final;

  private:
    void DoScheduleClose();
  };
}


#endif
