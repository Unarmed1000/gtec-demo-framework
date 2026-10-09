#ifndef SHARED_FRAMEPACING_SAMPLEANIMATIONERROR_HPP
#define SHARED_FRAMEPACING_SAMPLEANIMATIONERROR_HPP
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
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace Fsl
{
  //! Why a frame has a animation error that counts (one over SampleAnimationError::ErrorThreshold). The two are the causes the
  //! mb-framepacing tools tell apart, with their words.
  enum class SampleAnimationErrorCause
  {
    //! No error that counts
    NoError,
    //! The display was uneven: the frame, or the one before it, was not on screen for the time it was to be (half a refresh or more
    //! off its target). A frame that is shown a refresh late is one, and so is the frame after it, which comes on time and has moved
    //! too much.
    Pacing,
    //! The display was even and the animation was not: the frames came when they were to, and the animation moved by another time
    DeltaTimeJitter
  };

  //! What is known about the animation error of a frame (see SampleAnimationError)
  struct SampleAnimationErrorRecord
  {
    //! The id of the present of the frame
    uint64_t PresentId{0};
    //! false: the frame has no error that can be told (it or the frame before it has no display time, or it is the first one), and
    //! the rest is zero
    bool IsJudged{false};
    //! How far the animation moved from the frame before this one
    TimeSpan AnimationStep;
    //! How long after the frame before it this frame was shown
    TimeSpan DisplayStep;
    //! AnimationStep - DisplayStep. Zero: what moves was drawn where it is when the frame is seen. Negative: the frame was shown later
    //! than its animation says, so what moves moved too little. Positive: it was shown sooner, so it moved too much.
    TimeSpan Error;
    //! If the error counts and why
    SampleAnimationErrorCause Cause{SampleAnimationErrorCause::NoError};

    constexpr bool operator==(const SampleAnimationErrorRecord&) const noexcept = default;
  };

  //! The frames with a animation error that counts, which together are the stutter, by their cause
  struct SampleStutterCount
  {
    uint32_t Pacing{0};
    uint32_t DeltaTimeJitter{0};

    [[nodiscard]] constexpr uint32_t Total() const noexcept
    {
      return Pacing + DeltaTimeJitter;
    }

    constexpr bool operator==(const SampleStutterCount&) const noexcept = default;
  };

  //! What the animation errors of the last frames say (see SampleAnimationError)
  struct SampleAnimationErrorStats
  {
    //! The frames the values are of: the last ones that were judged
    uint32_t Frames{0};
    //! The frames of them with a error that is larger than SampleAnimationError::ErrorThreshold (in either direction)
    uint32_t ErrorFrames{0};
    //! The average of the errors without their sign
    TimeSpan AverageAbsError;
    //! The error that is the furthest from zero, with its sign
    TimeSpan WorstError;

    constexpr bool operator==(const SampleAnimationErrorStats&) const noexcept = default;
  };

  //! The animation error of the frames of a app that is told when its frames were shown (VK_EXT_present_timing and the like).
  //!
  //! A frame is drawn for a moment, its animation time, and is seen at another one, its display time. Between two frames that are
  //! shown one after the other the animation should move as far as the time that passes on screen. The difference is the animation
  //! error, which is what stutter is: animation error = animation time step - display time step. It is the measure of the
  //! mb-framepacing tools, which take the display times from a capture of the screen. Here they are the ones the system reports, so
  //! the error is as good as those are.
  //!
  //! The display time of a frame comes a few frames after the frame, and a frame can be reported as not shown, or not be reported at
  //! all (it is given up on when MaxWaitFrames frames have been added since). So the frames are held here until they are known,
  //! and are then given in the order they were presented in.
  //! A step is only judged between two frames that were presented one after the other and that both have a display time: a frame
  //! without one is not judged, and neither is the frame after it, as the mb-framepacing report does it (a step over a frame that is
  //! not known would be half a guess). The first frame has nothing to be compared with and is not judged either.
  class SampleAnimationError final
  {
  public:
    //! The frames that can be held
    static constexpr std::size_t Capacity = 64;
    //! The frames that can be added after a frame before it is given up on
    static constexpr uint64_t MaxWaitFrames = 32;
    //! The frames the stats are of
    static constexpr std::size_t StatsFrames = 240;
    //! What counts as a error in the numbers: one that is larger than this, in either direction (1 ms, the threshold of the
    //! mb-framepacing tools)
    static constexpr TimeSpan ErrorThreshold = TimeSpan::FromMilliseconds(1);
    //! The largest value of ToRefreshThousandths, in either direction (32 refreshes)
    static constexpr int32_t MaxRefreshThousandths = 32000;
    //! The frames with a error that the count of the last second can hold
    static constexpr std::size_t StutterCapacity = 512;
    //! The time the newest errors are counted over (CalcRecentStutter)
    static constexpr TimeSpan RecentTime = TimeSpan::FromSeconds(1);

  private:
    struct Frame
    {
      uint64_t PresentId{0};
      TimeSpan AnimationTime;
      //! How long the frame is to be on screen, and the refresh period of the display when it was presented
      TimeSpan TargetFrameTime;
      TimeSpan RefreshPeriod;
      std::optional<TickCount> DisplayTime;
      bool NotShown{false};
    };

    std::array<Frame, Capacity> m_frames{};
    //! The index of the oldest frame that is held and the number of frames that are held
    std::size_t m_first{0};
    std::size_t m_count{0};
    uint64_t m_newestPresentId{0};
    //! The frame that was taken last, if it has a display time
    bool m_hasShown{false};
    TimeSpan m_shownAnimationTime;
    TickCount m_shownDisplayTime;
    TimeSpan m_shownTargetFrameTime;
    //! True if the step before the last one that was judged was off its target (the display was uneven there)
    bool m_lastStepUneven{false};
    //! The frames with a error that counts: all of them by cause, and the newest ones with their display time
    SampleStutterCount m_runStutter;
    struct StutterEntry
    {
      TickCount DisplayTime;
      SampleAnimationErrorCause Cause{SampleAnimationErrorCause::NoError};
    };
    std::array<StutterEntry, StutterCapacity> m_stutter{};
    std::size_t m_stutterNext{0};
    std::size_t m_stutterCount{0};
    //! The last errors, for the stats
    std::array<int64_t, StatsFrames> m_errors{};
    std::size_t m_errorNext{0};
    std::size_t m_errorCount{0};

  public:
    //! @brief Forget every frame and the errors (the app stopped measuring its presents)
    void Clear() noexcept;

    //! @brief A frame is presented. The ids of the presents are given in the order of the frames, each larger than the one before.
    //! @param animationTime the moment the frame was drawn for
    //! @param targetFrameTime how long the frame is to be on screen (its swap interval in time)
    //! @param refreshPeriod the time between two refreshes of the display (zero: not known, a error then counts as pacing)
    void AddFrame(const uint64_t presentId, const TimeSpan animationTime, const TimeSpan targetFrameTime, const TimeSpan refreshPeriod) noexcept;

    //! @brief When the frame was shown.
    void AddDisplayTime(const uint64_t presentId, const TickCount displayTime) noexcept;

    //! @brief The system is done with the present and has no display time for it.
    void AddNotShown(const uint64_t presentId) noexcept;

    //! @brief Take the oldest frame if everything is known about it. The frames come in the order they were presented in, the ones
    //!        that are not judged as well.
    //! @return false if there is none (the oldest frame still waits for its display time)
    bool TryPop(SampleAnimationErrorRecord& rRecord) noexcept;

    //! @brief What the errors of the last StatsFrames frames that were judged say
    [[nodiscard]] SampleAnimationErrorStats CalcStats() const noexcept;

    //! @brief The frames with a error that counts since the last Clear, of the frames that were taken
    [[nodiscard]] SampleStutterCount GetRunStutter() const noexcept
    {
      return m_runStutter;
    }

    //! @brief The frames with a error that counts that were shown in the last RecentTime before the frame that was taken last was
    //!        shown
    [[nodiscard]] SampleStutterCount CalcRecentStutter() const noexcept;

    //! @brief The error in thousandths of a refresh, which is what a chart draws: -1000 is a frame that was shown a refresh too late
    //! @return zero if the refresh period is not known, and not more than MaxRefreshThousandths in either direction
    [[nodiscard]] static int32_t ToRefreshThousandths(const TimeSpan error, const TimeSpan refreshPeriod) noexcept;

  private:
    [[nodiscard]] Frame* TryFind(const uint64_t presentId) noexcept;
  };
}

#endif
