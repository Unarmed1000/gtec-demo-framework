#ifndef FSLDEMOSERVICE_FRAMEPACING_IMPL_FRAMEPACINGOVERLAY_HPP
#define FSLDEMOSERVICE_FRAMEPACING_IMPL_FRAMEPACINGOVERLAY_HPP
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

#include <FslGraphics/Render/Texture2D.hpp>
#include <memory>

namespace Fsl
{
  struct DemoWindowMetrics;
  class IFramePacingServiceControl;
  class IGraphicsService;
  class ServiceProvider;

  //! Draws the frame pacing marker of the current frame. The host calls Draw as the very last thing before the frame is presented.
  class FramePacingOverlay
  {
    struct QuadBuffer;

    std::shared_ptr<IFramePacingServiceControl> m_service;
    std::shared_ptr<IGraphicsService> m_graphicsService;
    std::unique_ptr<QuadBuffer> m_quads;
    Texture2D m_fillTexture;

  public:
    FramePacingOverlay(const FramePacingOverlay&) = delete;
    FramePacingOverlay& operator=(const FramePacingOverlay&) = delete;

    //! @brief Create a overlay, returns null if the frame pacing service is unavailable.
    static std::unique_ptr<FramePacingOverlay> TryCreate(const ServiceProvider& serviceProvider);

    FramePacingOverlay(std::shared_ptr<IFramePacingServiceControl> service, std::shared_ptr<IGraphicsService> graphicsService);
    ~FramePacingOverlay();

    //! @brief Draw the marker (does nothing if the marker is disabled).
    void Draw(const DemoWindowMetrics& windowMetrics);
  };
}

#endif
