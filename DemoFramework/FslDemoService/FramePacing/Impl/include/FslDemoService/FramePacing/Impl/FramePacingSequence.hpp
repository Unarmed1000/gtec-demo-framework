#ifndef FSLDEMOSERVICE_FRAMEPACING_IMPL_FRAMEPACINGSEQUENCE_HPP
#define FSLDEMOSERVICE_FRAMEPACING_IMPL_FRAMEPACINGSEQUENCE_HPP
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
#include <FslBase/Time/TimeSpan.hpp>
#include <FslDemoService/FramePacing/FramePacingRunState.hpp>
#include <FslDemoService/FramePacing/Impl/FramePacingMarkerKind.hpp>
#include <cstdint>

namespace Fsl
{
  //! The run state machine: Idle -> Starting (start marker) -> Measuring (frame markers) -> Ending (end marker) -> Idle.
  //! The start and end markers are shown for at least one frame and at least their configured duration.
  class FramePacingSequence
  {
    TimeSpan m_startMarkerDuration;
    TimeSpan m_endMarkerDuration;
    TimeSpan m_measureDuration;
    FramePacingRunState m_state{FramePacingRunState::Idle};
    //! The time of the first frame in the current state (only valid if m_framesInState > 0)
    TickCount m_stateBeginTime;
    uint32_t m_framesInState{0};

  public:
    //! Roughly three frames of a 30 fps capture
    static constexpr TimeSpan DefaultMarkerDuration = TimeSpan(100 * TimeSpan::TicksPerMillisecond);

    FramePacingSequence() noexcept;
    FramePacingSequence(const TimeSpan startMarkerDuration, const TimeSpan endMarkerDuration) noexcept;

    [[nodiscard]] FramePacingRunState GetState() const noexcept
    {
      return m_state;
    }

    //! @brief The duration of the measured part of the current (or last) run, zero means until EndRun is called.
    [[nodiscard]] TimeSpan GetMeasureDuration() const noexcept
    {
      return m_measureDuration;
    }

    //! @brief How long the run has been measuring at the given time (zero unless the state is Measuring).
    [[nodiscard]] TimeSpan GetMeasuredTime(const TickCount now) const noexcept;

    //! @brief Begin a run.
    //! @param measureDuration the duration of the measured part, zero means until EndRun is called.
    //! @return false if a run is already active.
    bool BeginRun(const TimeSpan measureDuration) noexcept;

    //! @brief Request the active run to end (the end marker is shown next).
    void EndRun() noexcept;

    //! @brief Advance the state machine to the given time and get the marker kind to draw for the frame.
    FramePacingMarkerKind Advance(const TickCount now) noexcept;

  private:
    void SetState(const FramePacingRunState state) noexcept;
    [[nodiscard]] bool IsMarkerDone(const TickCount now, const TimeSpan duration) const noexcept;
  };
}

#endif
