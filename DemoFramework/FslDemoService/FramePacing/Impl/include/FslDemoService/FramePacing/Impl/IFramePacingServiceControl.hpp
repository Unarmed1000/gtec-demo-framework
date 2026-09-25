#ifndef FSLDEMOSERVICE_FRAMEPACING_IMPL_IFRAMEPACINGSERVICECONTROL_HPP
#define FSLDEMOSERVICE_FRAMEPACING_IMPL_IFRAMEPACINGSERVICECONTROL_HPP
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

#include <FslDemoService/FramePacing/Impl/FramePacingFrameRecord.hpp>

namespace Fsl
{
  struct FrameInfo;

  //! The host side of the frame pacing service.
  class IFramePacingServiceControl
  {
  public:
    virtual ~IFramePacingServiceControl() = default;

    //! @brief Called by the host once per rendered frame before the app starts drawing.
    virtual void BeginFrame(const FrameInfo& frameInfo) = 0;

    //! @brief Get the marker for the current frame.
    //! @return false if no marker should be drawn.
    virtual bool TryGetFrameRecord(FramePacingFrameRecord& rRecord) const noexcept = 0;
  };
}

#endif
