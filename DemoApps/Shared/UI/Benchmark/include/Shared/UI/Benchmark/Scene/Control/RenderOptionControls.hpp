#ifndef SHARED_UI_BENCHMARK_SCENE_CONTROL_RENDEROPTIONCONTROLS_HPP
#define SHARED_UI_BENCHMARK_SCENE_CONTROL_RENDEROPTIONCONTROLS_HPP
/****************************************************************************************************************************************************
 * Copyright 2021 NXP
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

#include <FslSimpleUI/Base/Control/Switch.hpp>
#include <memory>
#include <utility>

namespace Fsl
{
  namespace UI
  {
    struct RenderSystemInfo;
  }

  struct RenderOptionControls
  {
    std::shared_ptr<UI::Switch> SwitchBatch;
    std::shared_ptr<UI::Switch> SwitchFillBuffers;
    std::shared_ptr<UI::Switch> SwitchDepthBuffer;
    std::shared_ptr<UI::Switch> SwitchDrawReorder;
    std::shared_ptr<UI::Switch> SwitchPreferFastReorder;
    std::shared_ptr<UI::Switch> SwitchMeshCaching;


    RenderOptionControls() = default;
    RenderOptionControls(std::shared_ptr<UI::Switch> switchBatch, std::shared_ptr<UI::Switch> switchFillBuffers,
                         std::shared_ptr<UI::Switch> switchDepthBuffer, std::shared_ptr<UI::Switch> switchDrawReorder,
                         std::shared_ptr<UI::Switch> switchRenderOptionPreferFastReorder, std::shared_ptr<UI::Switch> switchMeshCaching)
      : SwitchBatch(std::move(switchBatch))
      , SwitchFillBuffers(std::move(switchFillBuffers))
      , SwitchDepthBuffer(std::move(switchDepthBuffer))
      , SwitchDrawReorder(std::move(switchDrawReorder))
      , SwitchPreferFastReorder(std::move(switchRenderOptionPreferFastReorder))
      , SwitchMeshCaching(std::move(switchMeshCaching))
    {
    }

    void RestoreUISettings(const UI::RenderSystemInfo& systemInfo);
    void StoreUISettings(UI::RenderSystemInfo& rSystemInfo);
    void SetEnabled(const bool isEnabled);
    void FinishAnimation();
  };
}

#endif
