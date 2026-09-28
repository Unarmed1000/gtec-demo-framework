#ifndef FSLDEMOSERVICE_FRAMEPACING_CONTROL_IFRAMEPACINGSERVICECONTROL_HPP
#define FSLDEMOSERVICE_FRAMEPACING_CONTROL_IFRAMEPACINGSERVICECONTROL_HPP
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

#include <FslBase/Time/TickCount.hpp>
#include <FslDemoService/FramePacing/Control/IFramePacingOverlay.hpp>
#include <memory>

namespace Fsl
{
  struct FrameInfo;
  class ServiceProvider;

  //! The host side of the frame pacing service.
  //! The service is only registered on platforms that support the marker library, so use TryGet<IFramePacingServiceControl>().
  class IFramePacingServiceControl
  {
  public:
    virtual ~IFramePacingServiceControl() = default;

    //! @brief Called by the host once per rendered frame before the app starts drawing.
    //! @param cpuStartTime when the CPU started working on the frame (a HighResolutionTimer timestamp taken before the app update).
    virtual void BeginFrame(const FrameInfo& frameInfo, const TickCount cpuStartTime) = 0;

    //! @brief Create the overlay that draws the marker. The host owns it and draws it as the very last thing of every frame.
    //! @return null if the overlay can not be created (for example if the graphics service is unavailable).
    virtual std::unique_ptr<IFramePacingOverlay> CreateOverlay(const ServiceProvider& serviceProvider) = 0;
  };
}

#endif
