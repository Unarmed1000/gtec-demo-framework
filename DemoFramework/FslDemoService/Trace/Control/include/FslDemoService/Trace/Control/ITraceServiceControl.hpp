#ifndef FSLDEMOSERVICE_TRACE_CONTROL_ITRACESERVICECONTROL_HPP
#define FSLDEMOSERVICE_TRACE_CONTROL_ITRACESERVICECONTROL_HPP
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


#include <FslDemoService/Trace/TraceTypes.hpp>

namespace Fsl
{
  //! The side of the trace service the one that runs the frame loop uses: it says where a frame begins.
  class ITraceServiceControl
  {
  public:
    virtual ~ITraceServiceControl() = default;

    //! @brief A frame begins: what is set from now on is a value of this frame, and the frame that is too old to change is written.
    //!        The frames are expected in order.
    virtual void BeginFrame(const TraceFrameIndex frameIndex, const TraceRunId runId) = 0;

    //! @brief Name the two times a frame lasts from and to. A frame is drawn as a span between them, which carries its values.
    //! @param begin a value of the unit TraceUnit::Ticks.
    //! @param end a value of the unit TraceUnit::Ticks.
    virtual void SetFrameBounds(const TraceValue begin, const TraceValue end) = 0;
  };
}

#endif
