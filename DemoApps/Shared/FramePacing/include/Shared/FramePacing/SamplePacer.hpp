#ifndef SHARED_FRAMEPACING_SAMPLEPACER_HPP
#define SHARED_FRAMEPACING_SAMPLEPACER_HPP
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

#include <FslBase/Time/NanosecondTickCount.hpp>
#include <FslBase/Time/NanosecondTimeSpan.hpp>
#include <FslBase/Time/TickCount.hpp>
#include <FslBase/Time/TimeSpan.hpp>
#include <Shared/FramePacing/SamplePacerAim.hpp>
#include <Shared/FramePacing/SamplePacerGpuWaitReport.hpp>
#include <Shared/FramePacing/SamplePacerKind.hpp>
#include <Shared/FramePacing/SamplePacerPlan.hpp>
#include <Shared/FramePacing/SamplePacerRating.hpp>
#include <Shared/FramePacing/SamplePacerSystemWait.hpp>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

namespace Fsl
{
  //! How the sample's frame pacer is set up
  struct SamplePacerConfig
  {
    //! The time between two refreshes of the display, in nanoseconds: a tick of 100 nanoseconds is too coarse for it (at 240 Hz one
    //! tick is 24 parts per million of the period). The pacer of the library takes it as that. The default is 60 Hz.
    NanosecondTimeSpan RefreshPeriod{16666667};
    //! The frame rate the app wants to run at (0 = the refresh rate of the display)
    uint32_t TargetFps{0};
    //! Adapt the swap interval to how the frames do (false = always the swap interval of the target frame rate)
    bool Adaptive{true};
    //! Give the pacer when the display showed the frames (AddDisplayTime). The pacer paces the same with it and counts the animation
    //! error of the frames from them (GetDisplayErrors), where the app has display times (SamplePacerCapabilities): they are then
    //! part of the set it uses, and a change of it starts nothing again. Only for an app that measures its presents, on a display
    //! with a fixed refresh rate.
    bool PresentFeedback{false};
    //! What the pacer is to pace with: the set of capabilities that the kind names as the ones it uses
    SamplePacerKind Kind{SamplePacerKind::TimerPeriodOnly};
    //! What the app can do and can tell the pacer right now. The pacer is told all of it, and of the vertical blank times and the
    //! wait for a present it uses what the kind names: a capability the app does not have is left out by the pacer, so a kind the
    //! app has not everything for paces as the kind of what is left.
    SamplePacerCapabilities Capabilities;
    //! The pacer uses the present that takes the time the frame before stays on screen at least, where the app has one
    //! (SamplePacerCapabilities): it plans that time for the present (SamplePacerPresentPlan::MinimumDuration). It changes no tier.
    bool TimedPresent{false};
    //! The pacer holds the loop with a wait for the GPU's work, where the app can make that wait (SamplePacerCapabilities) and the
    //! pacer does not wait for a present: it names the frame (SamplePacerFrameStartPlan::WaitForGpuWorkFrameId).
    bool GpuWait{false};
    //! What the pacer optimizes for. The default is the one of the library.
    SamplePacerAim Aim{SamplePacerAim::Smoothness};
    //! The presents that may wait to be shown while a frame is made, the frame itself counted (one or more).
    //! SamplePacerKind::TimerWaitForPresent: before a frame the pacer asks for a wait until the present that many back was shown.
    //! With SamplePacerAim::Smoothness it is also the reserve of the pacer at one refresh per frame: that many less one frames are
    //! made ahead of the display.
    //! Zero, the default: the pacer picks the number, which is what a app is to leave to it (the app says its aim). One or more is
    //! for measuring.
    uint32_t WaitingPresents{0};
    //! The frames the app lets be in the works at the same time (the frames in flight of a Vulkan host), one or more. With two or
    //! more the CPU can work on a frame while the GPU works on the one before, which a pacer that is given the work of the GPU takes
    //! into account.
    uint32_t MaxFramesInFlight{1};
    //! SamplePacerKind::TimerPeriodOnly with SamplePacerAim::LowLatency: the refreshes of the one pause it makes half a second after
    //! it started, so the presents that queued up at the start are shown before the next one is added (zero: no pause).
    uint32_t StartupPauseRefreshes{4};
    //! SamplePacerKind::VBlankPeriodOnly: where in a refresh a frame is to be ready (presented, and with the GPU work reported the
    //! GPU done with it), in percent of the refresh period after a vertical blank (0 to 100). A frame that is ready there is shown
    //! at the next vertical blank. The default is the one of the library, the middle.
    uint32_t ReadyPlacePercent{50};
    //! The images the swapchain of the app has (zero: not known). One is on screen and one is drawn into, so the pacer keeps no more
    //! frames ahead of the display than that many less two.
    uint32_t SwapChainImages{0};
    //! True where the system holds the frame loop while its queue of frames is full (a acquire or a present that waits for the
    //! display) and the app reports those waits (AddSystemWait). SamplePacerKind::TimerPeriodOnly with SamplePacerAim::Smoothness
    //! then lets the system pace the loop, and a frame the system held is not late.
    bool SystemHoldsLoop{false};

    constexpr bool operator==(const SamplePacerConfig&) const noexcept = default;
  };

  //! What the swap interval rule of the pacer decided for a frame
  enum class SamplePacerChange
  {
    //! The swap interval is the one of the frame before (not called None, that is a macro of the X11 headers)
    Unchanged,
    //! A longer swap interval: a lower frame rate
    Slower,
    //! A shorter swap interval: a higher frame rate
    Faster
  };

  //! What the pacer plans for a frame. The times the frame pacing marker carries are in nanoseconds, as the pacer has them: nothing is
  //! rounded on the way to the marker.
  struct SamplePacerSchedule
  {
    //! The id the pacer gave the frame, which present feedback about the frame is given with (zero = no frame)
    uint64_t FrameId{0};
    //! The number of display refreshes the frame is held for
    uint32_t SwapInterval{1};
    //! The time from the animation time of the previous frame to the one of this frame
    NanosecondTimeSpan AnimationStep;
    //! When the pacer aims for the frame to be shown (on the clock of the HighResolutionTimer)
    NanosecondTickCount IntendedDisplayTime;
    //! The start of the frame plus its swap interval (a HighResolutionTimer timestamp): what a loop that paces by waiting holds to
    TickCount NextFrameStartTime;
    //! The frame time the pacer aims for: the swap interval as a time
    NanosecondTimeSpan TargetFrameTime;
    //! The frame time the app wants: the swap interval of the target frame rate as a time
    NanosecondTimeSpan PreferredFrameTime;
    SamplePacerChange Change{SamplePacerChange::Unchanged};
  };

  //! The frames the swap interval rule of the pacer decides on
  struct SamplePacerStatus
  {
    uint32_t SwapInterval{1};
    //! The swap interval of the target frame rate: the rule never runs faster than this
    uint32_t PreferredSwapInterval{1};
    uint32_t Frames{0};
    uint32_t LateFrames{0};
    //! How far the frames of the frame window began before the times the pacer gave for them, added up over the frames that were
    //! not late (for each the time the pacer gave for the start of the next frame minus the time that frame began). About zero: the
    //! app waits for those times. Positive: the loop runs ahead of them. Negative: it is behind.
    TimeSpan StartsAhead;
    //! The average time the frames needed
    TimeSpan AverageWork;
    //! The time the frames span: from when the oldest was shown to when the newest was
    TimeSpan WindowSpan;
    //! True if the rule has the frames it needs to decide on
    bool WindowFull{false};
  };

  //! What the display times the pacer was given say of the frames: the animation error where the app runs, counted
  //! since the pacer was made (the display error state of the pacer library). A frame is judged when it and the frame before it
  //! were both reported with a display time: its animation error is its animation time step less the time between the two.
  struct SamplePacerDisplayErrors
  {
    //! The display times that were taken
    uint64_t Reports{0};
    //! The display times that were not: for a frame the pacer does not keep (more than 64 frames old) or not newer than the one before
    uint64_t Refused{0};
    //! The frames that were judged
    uint64_t JudgedFrames{0};
    //! The judged frames with a animation error of more than a millisecond, either way
    uint64_t ErrorFrames{0};
    //! The judged frames shown half a refresh or more off where their animation time step put them: at another refresh
    uint64_t OffTargetFrames{0};
    //! Of those, the frames shown later: the frame before them was on screen a refresh or more longer than it was made for
    uint64_t LateFrames{0};
    //! The frames whose time from their start to their display was counted, the sum of those times and the longest of them
    uint64_t StartToDisplayFrames{0};
    NanosecondTimeSpan StartToDisplayTotal;
    NanosecondTimeSpan StartToDisplayLongest;
    //! The same of the frames shown in about the last second
    uint32_t RecentJudgedFrames{0};
    uint32_t RecentErrorFrames{0};
    uint32_t RecentOffTargetFrames{0};
    uint32_t RecentLateFrames{0};
  };

  //! The mb-framepacing frame pacer (experimental, its TierPacer) behind the types of the framework, so the rest of the sample does
  //! not depend on the library. It paces a frame loop with nothing but a steady clock and a present that waits for vsync, and uses
  //! what the app has on top of that (SamplePacerConfig::Capabilities) as far as the kind says (SamplePacerConfig::Kind).
  //!
  //! The calls of a frame, in this order: PlanFrame, where it names a present AddPresentWait (or a frame whose GPU work to wait for,
  //! AddGpuWait) and PlanFrame again, then BeginFrame, EndFrame and AddPresent. The pacer says what to wait for and the caller
  //! carries it out. A call for something the pacer does not use right now (a vertical blank, a wait for a present) is taken and
  //! does nothing.
  class SamplePacer final
  {
    struct Impl;
    std::unique_ptr<Impl> m_impl;

  public:
    SamplePacer(const SamplePacer&) = delete;
    SamplePacer& operator=(const SamplePacer&) = delete;

    //! @return true if the pacer library is available on this platform
    [[nodiscard]] static bool IsSupported() noexcept;

    //! @brief What a app with the given capabilities is worth, by the definition of the pacer library (its rating of a set of
    //!        capabilities): the best tier it reaches and if the display side can hold a frame. It needs no pacer. Without the
    //!        pacer library it is the baseline.
    [[nodiscard]] static SamplePacerRating Rate(const SamplePacerCapabilities& capabilities) noexcept;

    //! @brief How the pacer library writes a tier: its major tier and its sub tier. "1.1" is the best and "3.4" the baseline every
    //!        app reaches. Empty without the pacer library.
    //! @param timedPresent true for the tier of the same pacer where the display places the frame (a present at a time)
    [[nodiscard]] static std::string_view GetTierNumber(const SamplePacerTier tier, const bool timedPresent = false) noexcept;

    //! @brief A few words for a tier, as shown on screen: the name the pacer library gives it (empty without the pacer library).
    [[nodiscard]] static std::string_view GetTierName(const SamplePacerTier tier, const bool timedPresent = false) noexcept;

    //! @brief One word for a tier, as written to a log.
    [[nodiscard]] static std::string_view GetTierLogName(const SamplePacerTier tier, const bool timedPresent = false) noexcept;

    //! @brief A sentence of the pacer library that says what the pacer uses in a tier and what that gives (empty without the pacer
    //!        library).
    [[nodiscard]] static std::string_view GetTierDescription(const SamplePacerTier tier) noexcept;

    //! @brief The same in one line of the pacer library, of at most GetTierShortDescriptionMaxLength characters (empty without the
    //!        pacer library).
    [[nodiscard]] static std::string_view GetTierShortDescription(const SamplePacerTier tier, const bool timedPresent = false) noexcept;
    [[nodiscard]] static uint32_t GetTierShortDescriptionMaxLength() noexcept;

    explicit SamplePacer(const SamplePacerConfig& config);
    ~SamplePacer();

    //! @brief Change how the pacer is set up. With other settings the pacer starts again: its frame window is empty and it is back at
    //!        the swap interval of the target frame rate, the animation goes on. The config it has changes nothing.
    //! @note  Another SamplePacerConfig::Kind among the kinds that give plans, or other SamplePacerConfig::Capabilities, is the same
    //!        pacer with another set to use: nothing starts again. The frames and their ids, the animation time and the swap interval
    //!        go on, and the change is in force from the frame after the one that is open. SamplePacerConfig::TimedPresent is part
    //!        of that set.
    void SetConfig(const SamplePacerConfig& config);

    //! @brief The tier that paces the frame now, as the pacer says it (its working tier). It is below the tier of the kind until a
    //!        vertical blank was read and while the waits for a present are stopped. The baseline without the pacer library.
    [[nodiscard]] SamplePacerTier GetTier() const noexcept;

    //! @brief True if the pacer plans a time for the present: the time the frame before stays on screen at least
    //!        (SamplePacerPresentPlan::MinimumDuration), where the app has such a present and the config asks for it. It changes
    //!        no tier.
    [[nodiscard]] bool IsTimedPresentInUse() const noexcept;

    //! @brief The presents made so far are gone (a swapchain that was made anew). None of them can be waited for anymore, and a
    //!        pacer that makes a pause after it started makes it once more. Nothing else changes.
    void ForgetPresents() noexcept;

    //! @brief Before a frame takes anything: what to wait for before it starts. It changes nothing, so a frame can be planned again.
    //! @param now the time now (a HighResolutionTimer timestamp)
    [[nodiscard]] SamplePacerFrameStartPlan PlanFrame(const TickCount now) const noexcept;

    //! @brief What became of the wait for a present the plan asked for. Call it before BeginFrame, and plan the frame again after
    //!        it: the wait can have taken long, and the time to wait until can have moved with what it told the pacer.
    void AddPresentWait(const SamplePacerPresentWaitReport& report) noexcept;

    //! @brief What became of the wait for the GPU's work the plan asked for. Call it before BeginFrame, and plan the frame again
    //!        after it, as with the wait for a present.
    void AddGpuWait(const SamplePacerGpuWaitReport& report) noexcept;

    //! @brief How long the CPU has worked on the frame that was begun, at the given time: for a marker that is drawn while the work
    //!        of the frame goes on. Zero if it is not known.
    //! @param now the time now (a HighResolutionTimer timestamp)
    [[nodiscard]] TimeSpan GetCpuBusyAt(const TickCount now) const noexcept;

    //! @brief Start a frame.
    //! @param cpuStartTime the time the frame starts (a HighResolutionTimer timestamp)
    SamplePacerSchedule BeginFrame(const TickCount cpuStartTime) noexcept;

    //! @brief The CPU work of the frame is done, how is it to be presented.
    //! @param workDoneTime the time now (a HighResolutionTimer timestamp)
    SamplePacerPresentPlan EndFrame(const TickCount workDoneTime) noexcept;

    //! @brief A frame was presented. Call it after the present and before the next frame is planned.
    void AddPresent(const SamplePacerPresentReport& report) noexcept;

    //! @brief The work of the GPU on a frame that was presented earlier. Call it for every frame the app has a GPU time for, in the
    //!        order of the frames. The pacer puts it together with the work of the CPU, which it has from BeginFrame and EndFrame:
    //!        the caller adds no GPU time to anything it gives the pacer.
    void AddGpuWork(const SamplePacerGpuWorkReport& report) noexcept;

    //! @brief The GPU time the pacer judges a frame with (the newest it was given).
    [[nodiscard]] TimeSpan GetGpuTime() const noexcept;

    //! @brief SamplePacerKind::TimerPeriodOnly: the pauses it made after a start, counted since the pacer was made. Zero for the
    //!        other kinds.
    [[nodiscard]] uint64_t GetStartupPauses() const noexcept;

    //! @brief The refreshes that were lost and that the animation time was not moved over, so how far the animation is behind the
    //!        clock, counted since the pacer was made.
    [[nodiscard]] uint64_t GetRefreshesBehindClock() const noexcept;

    //! @brief SamplePacerKind::TimerWaitForPresent: the waits for a present that ended without the present being shown, counted since
    //!        the pacer was made. Zero for the other kinds.
    [[nodiscard]] uint64_t GetPresentWaitTimeouts() const noexcept;

    //! @brief The waits for the GPU's work that ended without the GPU done with the frame, counted since the pacer was made.
    [[nodiscard]] uint64_t GetGpuWaitTimeouts() const noexcept;

    //! @brief SamplePacerKind::TimerWaitForPresent: true while the pacer does not wait for presents because its waits ran out, which
    //!        is what a window that is covered or minimised gives. It paces on its timer then, and only asks if a present was shown
    //!        (a wait with no time to wait); the first that was ends it. False for the other kinds.
    [[nodiscard]] bool IsPresentWaitStopped() const noexcept;

    //! @brief SamplePacerKind::VBlankPeriodOnly: where the refreshes of the display are. Give it whenever the window system has a
    //!        new one, before the frame is planned, and only one that is of the display the window is on. Nothing for the other
    //!        kinds.
    //! @param vblankTime the time of a vertical blank, a recent one or the next, as the window system gave it
    //! @param period the time between two vertical blanks as the window system gave it (zero: it gave none)
    //! @param readTime when the two were read
    void AddVBlank(const NanosecondTickCount vblankTime, const NanosecondTimeSpan period, const TickCount readTime) noexcept;

    //! @brief SamplePacerKind::VBlankPeriodOnly: true once the pacer was given a vertical blank of the display it is on. Until then
    //!        it paces on its clock as SamplePacerKind::TimerPeriodOnly does. False for the other kinds.
    [[nodiscard]] bool HasVBlankReading() const noexcept;

    //! @brief SamplePacerKind::VBlankPeriodOnly: the vertical blanks it was given that were not where the ones before put them (more
    //!        than a eighth of a refresh off), counted since the pacer was made. Zero for the other kinds.
    [[nodiscard]] uint64_t GetVBlankJumps() const noexcept;

    //! @brief SamplePacerKind::VBlankWaitForPresent: the refreshes a wait for a present said a frame was shown later than the pacer
    //!        had worked out, counted since the pacer was made. Zero for the other kinds.
    [[nodiscard]] uint64_t GetShownLaterByWaits() const noexcept;

    //! @brief SamplePacerKind::VBlankWaitForPresent: where in the refresh before its vertical blank a frame is to be ready now, as a
    //!        time after the vertical blank before it. It starts at SamplePacerConfig::ReadyPlacePercent of the refresh and the
    //!        pacer moves it earlier when frames that were ready there are shown a vertical blank late. The other kinds say where it
    //!        is set and do not move it.
    [[nodiscard]] TimeSpan GetReadyPlaceNow() const noexcept;

    //! @brief The times the pacer tried the place a frame is to be ready at one step later again, counted since the pacer was made.
    [[nodiscard]] uint64_t GetReadyPlaceTries() const noexcept;

    //! @brief The tries it took back, because a frame was shown later in the frames after one.
    [[nodiscard]] uint64_t GetReadyPlaceTriesTakenBack() const noexcept;

    //! @brief The kinds on a timer (SamplePacerKind::TimerPeriodOnly and TimerWaitForPresent): a wait of the app before a frame
    //!        started that the pacer did not ask for. Give it
    //!        after the wait and before the BeginFrame of the frame, for every such wait, also one that returned at once. Nothing
    //!        for the other kinds.
    void AddSystemWait(const SamplePacerSystemWait kind, const TickCount beginTime, const TickCount endTime) noexcept;

    //! @brief The kinds on a timer: the frames whose start the side of the display held (a acquire, a present that
    //!        waited: for a eighth of a refresh or more), counted since the pacer was made. Zero for the other kinds.
    [[nodiscard]] uint64_t GetSystemHeldFrames() const noexcept;

    //! @brief The kinds on a timer: the frames whose start a wait for a frame slot held as long (the GPU was not done
    //!        with a earlier frame), counted since the pacer was made. Zero for the other kinds.
    [[nodiscard]] uint64_t GetFrameSlotHeldFrames() const noexcept;

    //! @brief The kinds on vertical blank times: the refreshes frame starts were late by while the side of the display held the loop,
    //!        which the animation time does not step over, counted since the pacer was made. They are part of
    //!        GetRefreshesBehindClock. Zero for the other kinds.
    [[nodiscard]] uint64_t GetDisplayHeldRefreshes() const noexcept;

    //! @brief A frame that was presented earlier was shown. Call it before the BeginFrame of the next frame, oldest frame first. It
    //!        does nothing unless the config asks for present feedback. The pacer counts the animation error from it
    //!        (GetDisplayErrors) and paces by none of it. A frame without a display time is not reported: no display time is not
    //!        "never shown".
    //! @param frameId the SamplePacerSchedule::FrameId of the frame
    //! @param displayTime when the display started to show the frame (a HighResolutionTimer timestamp)
    void AddDisplayTime(const uint64_t frameId, const TickCount displayTime) noexcept;

    [[nodiscard]] SamplePacerStatus GetStatus() const noexcept;
    //! @brief What the display times the pacer was given say of the frames.
    [[nodiscard]] SamplePacerDisplayErrors GetDisplayErrors() const noexcept;
  };
}

#endif
