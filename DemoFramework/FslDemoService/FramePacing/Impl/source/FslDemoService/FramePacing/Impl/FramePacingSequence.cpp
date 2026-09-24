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

#include <FslDemoService/FramePacing/Impl/FramePacingSequence.hpp>

namespace Fsl
{
  FramePacingSequence::FramePacingSequence() noexcept
    : FramePacingSequence(DefaultMarkerDuration, DefaultMarkerDuration)
  {
  }


  FramePacingSequence::FramePacingSequence(const TimeSpan startMarkerDuration, const TimeSpan endMarkerDuration) noexcept
    : m_startMarkerDuration(startMarkerDuration)
    , m_endMarkerDuration(endMarkerDuration)
  {
  }


  bool FramePacingSequence::BeginRun(const TimeSpan measureDuration) noexcept
  {
    if (m_state != FramePacingRunState::Idle)
    {
      return false;
    }
    m_measureDuration = measureDuration;
    SetState(FramePacingRunState::Starting);
    return true;
  }


  TimeSpan FramePacingSequence::GetMeasuredTime(const TickCount now) const noexcept
  {
    if (m_state != FramePacingRunState::Measuring || m_framesInState == 0)
    {
      return {};
    }
    const TimeSpan measuredTime = now - m_stateBeginTime;
    // The state only changes when a frame is started, so cap it to the duration
    return m_measureDuration > TimeSpan() && measuredTime > m_measureDuration ? m_measureDuration : measuredTime;
  }


  void FramePacingSequence::EndRun() noexcept
  {
    if (m_state == FramePacingRunState::Starting || m_state == FramePacingRunState::Measuring)
    {
      SetState(FramePacingRunState::Ending);
    }
  }


  FramePacingMarkerKind FramePacingSequence::Advance(const TickCount now) noexcept
  {
    if (m_framesInState == 0)
    {
      m_stateBeginTime = now;
    }

    switch (m_state)
    {
    case FramePacingRunState::Starting:
      if (!IsMarkerDone(now, m_startMarkerDuration))
      {
        ++m_framesInState;
        return FramePacingMarkerKind::SequenceStart;
      }
      SetState(FramePacingRunState::Measuring);
      m_stateBeginTime = now;
      ++m_framesInState;
      return FramePacingMarkerKind::Frame;
    case FramePacingRunState::Measuring:
      if (m_measureDuration > TimeSpan() && (now - m_stateBeginTime) >= m_measureDuration)
      {
        SetState(FramePacingRunState::Ending);
        m_stateBeginTime = now;
        ++m_framesInState;
        return FramePacingMarkerKind::SequenceEnd;
      }
      ++m_framesInState;
      return FramePacingMarkerKind::Frame;
    case FramePacingRunState::Ending:
      if (!IsMarkerDone(now, m_endMarkerDuration))
      {
        ++m_framesInState;
        return FramePacingMarkerKind::SequenceEnd;
      }
      SetState(FramePacingRunState::Idle);
      return FramePacingMarkerKind::Frame;
    case FramePacingRunState::Idle:
    default:
      return FramePacingMarkerKind::Frame;
    }
  }


  void FramePacingSequence::SetState(const FramePacingRunState state) noexcept
  {
    m_state = state;
    m_framesInState = 0;
  }


  bool FramePacingSequence::IsMarkerDone(const TickCount now, const TimeSpan duration) const noexcept
  {
    // A marker is always shown for at least one frame
    return m_framesInState > 0 && (now - m_stateBeginTime) >= duration;
  }
}
