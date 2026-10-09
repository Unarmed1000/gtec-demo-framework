#ifndef SHARED_FRAMEPACING_FRAMEPACINGSHARED_HPP
#define SHARED_FRAMEPACING_FRAMEPACINGSHARED_HPP
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

#include <FslBase/Math/Pixel/PxSize2D.hpp>
#include <FslBase/System/HighResolutionTimer.hpp>
#include <FslBase/Time/NanosecondTickCount.hpp>
#include <FslBase/Time/NanosecondTimeSpan.hpp>
#include <FslBase/Time/TickCount.hpp>
#include <FslBase/Time/TimeSpan.hpp>
#include <FslDemoApp/Base/DemoAppConfig.hpp>
#include <FslDemoApp/Base/DemoTime.hpp>
#include <FslDemoService/FramePacingMarker/FramePacingRunState.hpp>
#include <FslDemoService/Trace/TraceTypes.hpp>
#include <FslGraphics/Color.hpp>
#include <FslGraphics/Render/Texture2D.hpp>
#include <FslSimpleUI/App/UIDemoAppExtension.hpp>
#include <FslSimpleUI/Base/Control/BackgroundLabelButton.hpp>
#include <FslSimpleUI/Base/Control/Label.hpp>
#include <FslSimpleUI/Base/Control/RadioButton.hpp>
#include <FslSimpleUI/Base/Control/SelectorLabel.hpp>
#include <FslSimpleUI/Base/Control/SliderAndFmtValueLabel.hpp>
#include <FslSimpleUI/Base/Control/Switch.hpp>
#include <Shared/FramePacing/RaymarchParams.hpp>
#include <Shared/FramePacing/SampleAnimationError.hpp>
#include <Shared/FramePacing/SampleConfig.hpp>
#include <Shared/FramePacing/SampleExplicitSync.hpp>
#include <Shared/FramePacing/SampleFrameStats.hpp>
#include <Shared/FramePacing/SampleFrameWork.hpp>
#include <Shared/FramePacing/SampleFrameWorkAverage.hpp>
#include <Shared/FramePacing/SampleGpuWaitRequest.hpp>
#include <Shared/FramePacing/SampleKeyboardMenu.hpp>
#include <Shared/FramePacing/SampleMeasuredPresents.hpp>
#include <Shared/FramePacing/SamplePacer.hpp>
#include <Shared/FramePacing/SamplePacerKind.hpp>
#include <Shared/FramePacing/SamplePacerPlan.hpp>
#include <Shared/FramePacing/SamplePacerRating.hpp>
#include <Shared/FramePacing/SamplePacerTierChoice.hpp>
#include <Shared/FramePacing/SamplePresentWaitRequest.hpp>
#include <Shared/FramePacing/SampleStatsLevel.hpp>
#include <Shared/FramePacing/SampleSwapchainRefresh.hpp>
#include <fmt/format.h>
#include <array>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace Fsl
{
  class IFramePacingMarkerService;
  class ITraceService;
  class SampleAnimationErrorChart;
  namespace UI
  {
    class BaseWindow;
    class ChartData;
    namespace Theme
    {
      class IThemeControlFactory;
    }
  }
  class INativeBatch2D;
  class INativeWindow;
  class KeyEvent;
  class WindowFocusEvent;

  //! How the app presents its frames
  enum class SamplePresentMethod
  {
    //! The app presents with a swap interval (eglSwapInterval): the present is given the swap interval the frame pacer says and can
    //! hold a frame for it. The frame starts at Update.
    SwapInterval,
    //! The present holds a frame for one refresh only (a Vulkan FIFO present): a frame of more refreshes is held by the time the
    //! frame pacer says to wait until (WaitForPresent). The frame starts at Draw, which the host calls after it waited for a free
    //! buffer.
    WaitThenPresent
  };

  //! Shows how an app can control the mb-framepacing frame marker through the IFramePacingMarkerService.
  //! The marker itself is drawn by the host on top of every frame, so apps only need to enable it (or use --FramePacing).
  //! All rendering goes through the API independent INativeBatch2D so the same code is used by the GLES2, GLES3 and Vulkan samples.
  //!
  //! The sample can also pace its frames with the experimental frame pacer of the mb-framepacing SDK (SamplePacer). The framework has
  //! no frame pacer, so the pacer lives in the sample: it gives every frame its animation time and says what to wait for before a
  //! frame starts and before it is presented. The sample and the app only carry that out, and the marker service is told what the
  //! frame was paced by (IFramePacingMarkerService::SetFrameSchedule).
  class FramePacingShared final : public UI::EventListener
  {
    //! One value label per value the frame pacing marker carries
    struct MarkerStatsUIRecord
    {
      std::shared_ptr<UI::Label> Kind;
      std::shared_ptr<UI::Label> FrameIndex;
      std::shared_ptr<UI::Label> AnimationTime;
      std::shared_ptr<UI::Label> RunId;
      std::shared_ptr<UI::Label> IntendedDisplayTime;
      std::shared_ptr<UI::Label> TargetFrameTime;
      std::shared_ptr<UI::Label> CpuStartTime;
      std::shared_ptr<UI::Label> CpuBusyTime;
      std::shared_ptr<UI::Label> PreferredFrameTime;
      std::shared_ptr<UI::Label> Static;
      std::shared_ptr<UI::Label> RunStartTime;
      std::shared_ptr<UI::Label> RunSequenceId;
      std::shared_ptr<UI::Label> SyncMarker;
    };

    //! One value label per value of the frame pacing section of the stats panel
    struct PacerStatsUIRecord
    {
      //! What is certain about explicit sync (Wayland only, null on other window systems)
      std::shared_ptr<UI::Label> ExplicitSync;
      //! The refresh rate the pacing counts with and where it is from, and the rate the frames are paced at with the rate the app
      //! prefers
      std::shared_ptr<UI::Label> RefreshRate;
      std::shared_ptr<UI::Label> PacedRate;
      std::shared_ptr<UI::Label> SwapInterval;
      std::shared_ptr<UI::Label> FrameTime;
      std::shared_ptr<UI::Label> LateFrames;
      std::shared_ptr<UI::Label> Work;
      std::shared_ptr<UI::Label> AverageWork;
      std::shared_ptr<UI::Label> PresentWait;
      //! If the present of the app can hold a frame for two refreshes or more, as the pacer library rates what the app can do
      std::shared_ptr<UI::Label> DisplaySideHolds;
      std::shared_ptr<UI::Label> IntervalChanges;
      std::shared_ptr<UI::Label> LastChange;
      std::shared_ptr<UI::Label> FrameWindow;
      std::shared_ptr<UI::Label> DisplayReports;
      std::shared_ptr<UI::Label> DisplayLate;
      std::shared_ptr<UI::Label> VariableRefresh;
      //! What the measured presents say (only an app that measures them has values for these)
      std::shared_ptr<UI::Label> DisplayError;
      std::shared_ptr<UI::Label> DisplayInterval;
      std::shared_ptr<UI::Label> AnimationError;
      std::shared_ptr<UI::Label> Latency;
      std::shared_ptr<UI::Label> TimedFrames;
      std::shared_ptr<UI::Label> GpuWork;
      std::shared_ptr<UI::Label> DisplayRefresh;
    };

    //! How much the values of the frame pacing section ask to be looked at (the color of the value). A value that is not here has
    //! nothing to compare it with.
    struct PacerStatsLevelRecord
    {
      SampleStatsLevel SwapInterval{SampleStatsLevel::Normal};
      SampleStatsLevel FrameTime{SampleStatsLevel::Normal};
      SampleStatsLevel LateFrames{SampleStatsLevel::Normal};
      SampleStatsLevel AverageWork{SampleStatsLevel::Normal};
      SampleStatsLevel DisplayLate{SampleStatsLevel::Normal};
      SampleStatsLevel VariableRefresh{SampleStatsLevel::Normal};
      SampleStatsLevel DisplayInterval{SampleStatsLevel::Normal};
      SampleStatsLevel DisplayRefresh{SampleStatsLevel::Normal};
    };

    //! What the swap interval rule of the frame pacer changed since the pacer was set up
    struct PacerChangeRecord
    {
      uint32_t SlowerCount{0};
      uint32_t FasterCount{0};
      SamplePacerChange LastChange{SamplePacerChange::Unchanged};
      //! When the frame of the last change started (a HighResolutionTimer timestamp)
      TickCount LastChangeTime;
      //! Since when the changes are counted: when the frame pacer was switched on or one of its settings was changed last
      TickCount StartTime;
    };

    //! The marker the stats panel showed last, so the panel can show how far the times of a marker are from the ones of the marker
    //! before it
    struct MarkerStepRecord
    {
      bool HasMarker{false};
      uint64_t FrameIndex{0};
      NanosecondTimeSpan AnimationTime;
      std::optional<NanosecondTickCount> IntendedDisplayTime;
      std::optional<NanosecondTickCount> CpuStartTime;
      //! The step from the marker before to this marker (empty if there is no marker before or a time is unknown)
      std::optional<NanosecondTimeSpan> AnimationStep;
      std::optional<NanosecondTimeSpan> IntendedDisplayStep;
      std::optional<NanosecondTimeSpan> CpuStartStep;
    };

    struct UIRecord
    {
      //! The color the theme gives the text of a label: the color of a value that is as it should be
      UI::UIColor DefaultFontColor;
      std::shared_ptr<UI::Label> LabelStatus;
      std::shared_ptr<UI::Label> LabelRun;
      std::shared_ptr<UI::BackgroundLabelButton> ButtonRun;
      std::shared_ptr<UI::BackgroundLabelButton> ButtonTimedRun;
      std::shared_ptr<UI::SliderAndFmtValueLabel<int32_t>> SliderDuration;
      std::shared_ptr<UI::Switch> SwitchPacer;
      std::shared_ptr<UI::Label> LabelRefreshRate;
      std::shared_ptr<UI::SliderAndFmtValueLabel<int32_t>> SliderRefreshRate;
      std::shared_ptr<UI::SliderAndFmtValueLabel<int32_t>> SliderTargetFps;
      std::shared_ptr<UI::Switch> SwitchAdaptive;
      //! The aim of the pacer: on is low latency, off is smoothness
      std::shared_ptr<UI::Switch> SwitchLowLatency;
      std::shared_ptr<UI::Switch> SwitchTimedPresent;
      std::shared_ptr<UI::Switch> SwitchPacerDisplayReports;
      //! What the frames can be paced with (below the switch of the frame pacer), one group of radio buttons: the tiers of the
      //! pacer library the samples reach, with the best first. A tier that can not be used here is disabled, and the one that is
      //! checked is the one the run is paced by.
      std::array<std::shared_ptr<UI::RadioButton>, 4> RadioTiers;
      //! What the pacer that is checked does, below the group: a line of the pacer library for a tier. The label has the line of
      //! every pacer and is as wide as the longest, so the side bar keeps its width when another one is checked.
      std::shared_ptr<UI::SelectorLabel> LabelTierDescription;
      std::shared_ptr<UI::SliderAndFmtValueLabel<int32_t>> SliderCpuLoad;
      std::shared_ptr<UI::SliderAndFmtValueLabel<int32_t>> SliderGpuLoad;
      //! The resolution the background is drawn at, in percent
      //! The scene of the raymarched background
      std::shared_ptr<UI::RadioButton> RadioBackgroundMandelbrot;
      std::shared_ptr<UI::RadioButton> RadioBackgroundBlobs;
      std::shared_ptr<UI::RadioButton> RadioBackgroundLace;
      std::shared_ptr<UI::RadioButton> RadioBackgroundFlight;
      std::shared_ptr<UI::RadioButton> RadioBackgroundHall;
      //! The two overlays: the values of the last marker and the frame pacing stats, each can be hidden with its switch
      std::shared_ptr<UI::Switch> SwitchMarkerStats;
      std::shared_ptr<UI::Switch> SwitchPacerStats;
      std::shared_ptr<UI::BaseWindow> MarkerStatsOverlay;
      std::shared_ptr<UI::BaseWindow> PacerStatsOverlay;
      //! The chart of the work per frame at the bottom, and the test pattern (the moving bar and box): each has a switch too
      std::shared_ptr<UI::Switch> SwitchWorkChart;
      std::shared_ptr<UI::Switch> SwitchAnimationErrorChart;
      std::shared_ptr<UI::Switch> SwitchTestPattern;
      //! The bar with the controls at the right of the window
      std::shared_ptr<UI::BaseWindow> RightBar;
      std::shared_ptr<UI::Switch> SwitchBoxAnimation;
      std::shared_ptr<UI::Switch> SwitchBoxAnimationFast;
      //! Draws the sync marker of the service at the bottom left
      std::shared_ptr<UI::Switch> SwitchSyncMarker;
      //! The optional measurements of the app: when its frames reach the display, and when the GPU worked on them
      std::shared_ptr<UI::Switch> SwitchPresentTiming;
      std::shared_ptr<UI::Switch> SwitchGpuTimeline;
      std::shared_ptr<UI::BaseWindow> WorkChartBar;
      //! The legend of the work chart: each line has the average of the last frames
      std::shared_ptr<UI::Label> LabelWorkFrame;
      std::shared_ptr<UI::Label> LabelWorkGpu;
      std::shared_ptr<UI::Label> LabelWorkCpu;
      std::shared_ptr<UI::BaseWindow> AnimationErrorChartBar;
      std::shared_ptr<SampleAnimationErrorChart> AnimationErrorChart;
      //! The labels of the lines of the chart where a frame is a refresh too soon and a refresh too late
      std::shared_ptr<UI::Label> LabelAnimationErrorEarly;
      std::shared_ptr<UI::Label> LabelAnimationErrorLate;
      //! The legend of the chart: the stutter of the last second, and the stutter of the run by its cause
      std::shared_ptr<UI::Label> LabelStutterRecent;
      std::shared_ptr<UI::Label> LabelStutterRun;
      MarkerStatsUIRecord MarkerStats;
      PacerStatsUIRecord PacerStats;
    };

    UI::CallbackEventListenerScope m_uiEventListener;
    std::shared_ptr<UIDemoAppExtension> m_uiExtension;
    std::shared_ptr<IFramePacingMarkerService> m_framePacing;
    std::shared_ptr<INativeBatch2D> m_nativeBatch;
    Texture2D m_fillTexture;
    std::string m_runName;
    UIRecord m_ui;
    //! The cursor that is moved over the controls of the side bar with the arrow keys
    SampleKeyboardMenu m_keyboardMenu;
    PxSize2D m_windowSizePx;
    FramePacingRunState m_cachedRunState{FramePacingRunState::Idle};
    uint32_t m_cachedRunId{0};
    //! The measured time shown in the UI in 1/10 seconds
    int64_t m_cachedMeasuredTenths{-1};
    //! Reused for every per-frame text so updating the marker stats does not allocate
    fmt::memory_buffer m_formatBuffer;
    MarkerStepRecord m_markerStep;

    HighResolutionTimer m_timer;
    SamplePresentMethod m_presentMethod;
    //! The window the refresh rate of the display is read from
    std::weak_ptr<INativeWindow> m_window;
    //! The refresh rate given on the command line (it overrides the window system and the slider)
    std::optional<double> m_refreshRateOverrideHz;
    //! The refresh period the window system reports for the mode of the display, in nanoseconds as it is given (zero = unknown, the
    //! slider is used), and the same as a rate in Hz for the controls
    NanosecondTimeSpan m_detectedRefreshPeriod;
    double m_detectedRefreshRateHz{0.0};
    //! The frame pacer (empty while it is off)
    std::unique_ptr<SamplePacer> m_pacer;
    SamplePacerConfig m_pacerConfig;
    //! What the frame pacer planned for the current frame
    SamplePacerSchedule m_schedule;
    //! The update time of the current frame
    DemoTime m_updateTime;
    //! True once the current frame was started (a frame is only started once, even if the host draws it again)
    bool m_frameStarted{false};
    //! The number of the frame that was started last, counted from one (GetFrameId)
    uint64_t m_frameId{0};
    //! True if the app is to flush its commands after the last one of a frame (IsFlushWanted)
    bool m_flushWanted{false};
    //! When the current frame started (a HighResolutionTimer timestamp)
    TickCount m_frameStartTime;
    //! The time from the start of the previous frame to the start of the current frame
    TimeSpan m_frameInterval;
    //! The frames of the last two seconds: their frame times, and the late frames while the frame pacer is off (the pacer counts its
    //! own)
    SampleFrameStats m_frameStats;
    PacerChangeRecord m_pacerChanges;
    //! Set by the functions that fill the frame pacing section and shown by UpdatePacerStats
    PacerStatsLevelRecord m_pacerStatsLevels;
    //! How long the CPU and the GPU worked on the last frame that ended (the GPU time is zero if the app does not measure it)
    TimeSpan m_lastCpuTime;
    TimeSpan m_lastGpuTime;
    //! How long the present of the last frame that ended was delayed by WaitForPresent
    TimeSpan m_lastPresentWait;
    //! The work per frame of the chart, in microseconds: the CPU time, the GPU time and how long the frame took. Three values of
    //! their own, not parts of a sum.
    std::shared_ptr<UI::ChartData> m_workChartData;
    //! Puts what is measured about the work on a frame together for the chart: the GPU time of a frame comes frames later
    SampleFrameWork m_frameWork;
    //! The average of what the last frames cost, for the legend of the work chart, and when the legend was written last
    SampleFrameWorkAverage m_frameWorkAverage;
    TickCount m_workLegendTime;
    //! The animation error of the frames, from the display times the system reports: they come frames after the frame
    SampleAnimationError m_animationError;
    //! The refresh period the labels and the band of the chart of the animation error were made for
    TimeSpan m_animationErrorRefreshPeriod;
    //! When the legend of the chart of the animation error was written last
    TickCount m_stutterLegendTime;
    //! The time the current frame is animated for
    NanosecondTimeSpan m_animationTime;
    //! The animation time minus the time of the framework (zero until the frame pacer was used), so the animation does not jump when the
    //! frame pacer is switched off
    NanosecondTimeSpan m_animationOffset;
    //! Relates the presents the app measured to the frames of the sample
    SampleMeasuredPresents m_measuredPresents;
    //! True if the app measures when its frames are presented
    bool m_presentsMeasured{false};
    //! True if the app can give a present a target time (SetTimedPresentSupport)
    bool m_timedPresentSupported{false};
    //! The tier of the pacer library the run is in (empty: the frame pacer is off) and what the pacer library rates this app at
    //! on this system. They are worked out every update and logged when they change.
    struct TierState
    {
      std::optional<SamplePacerTier> InUse;
      //! The tier in use is the one with a present that takes a time on top
      SamplePacerRating Best;

      constexpr bool operator==(const TierState&) const noexcept = default;
    };

    TierState m_tierState;
    bool m_tierKnown{false};
    //! True once a frame has ended: the app has said what its swapchain can do by then (it only knows after its first frame began)
    bool m_tierFactsReady{false};
    //! The longest swap interval the present of the app can hold a frame for (zero: not known, or the present has none)
    uint32_t m_presentSwapIntervalMax{0};
    //! What the window system said when the sample started: what is certain about explicit sync, and the name of its vsync source
    SampleExplicitSync m_explicitSync{SampleExplicitSync::NotApplicable};
    std::string m_vsyncSourceName;
    //! The time the present of the current frame is given: the frame before it stays on screen at least this long (zero = none)
    TimeSpan m_presentRelativeTarget;
    //! When the display refreshes according to the window system, read once per frame: the time of a vertical blank and the time
    //! between two refreshes (both zero if the platform does not say). They are kept in nanoseconds as the window system gives them,
    //! so a vertical blank many refreshes away is placed with the period that was given and not with one rounded to a tick.
    NanosecondTickCount m_vsyncTime;
    NanosecondTimeSpan m_vsyncPeriod;
    //! True once the display was seen to refresh at a variable rate: the window says so, it was measured, or the swapchain says so.
    //! The vertical blanks of such a display follow the frames, so the pacer is not given them. It stays set until the refresh rate
    //! of the display changes, so the tier does not flip back and forth.
    bool m_variableRefreshSeen{false};
    //! The refresh rate of the display that m_variableRefreshSeen belongs to
    double m_variableRefreshSeenRateHz{0.0};
    //! What was last written to the log about it
    bool m_loggedVariableRefreshSeen{false};
    //! What the swapchain of the app says about the refresh of the display (SetSwapchainRefresh)
    SampleSwapchainRefresh m_swapchainRefresh{SampleSwapchainRefresh::Unknown};
    //! When the frame that is about to start was planned first, which is where its wait began (zero: it was not planned yet)
    TickCount m_frameStartWaitTime;
    //! What the pacer was asked to pace with (--Pacer.Kind, then the radio button the user checked last). The kind in use is in
    //! m_pacerConfig: a kind that needs something the app can not do is replaced by the best one of what the app has.
    SamplePacerKind m_pacerKind{SamplePacerKind::VBlankWaitForPresent};
    //! The presents that may wait to be shown while a frame is made, as the app said (SetPresentWaitSupport). Zero: the app can not
    //! wait for a present.
    uint32_t m_presentWaitingPresents{0};
    //! The frames the app lets be in the works at the same time, as it said (SetMaxFramesInFlight)
    uint32_t m_maxFramesInFlight{1};
    //! The presents the pacer lets wait while a frame is made, as the command line said (--Pacer.WaitingPresents).
    //! Zero: not said.
    uint32_t m_waitingPresentsOption{0};
    //! Where in a refresh the pacer of the vertical blank times has a frame ready (--Pacer.ReadyPlace)
    uint32_t m_readyPlacePercent{50};
    //! The frames between two changes of the pacer kind (--Pacer.KindChange), zero: the kind is not changed
    uint32_t m_kindChangeFrames{0};
    //! The paced frames since the kind was changed last
    uint32_t m_kindChangeFrameCount{0};
    //! Where in SamplePacerTierChoiceUtil::KindChangeOrder the run is
    std::size_t m_kindChangeIndex{0};
    //! The pacer is told that the system holds the frame loop while its queue is full (--Pacer.SystemHoldsLoop)
    bool m_systemHoldsLoop{false};
    //! The pacer is to hold the loop with a wait for the GPU's work where the app can make it, as the command line said
    //! (--Pacer.GpuWait)
    bool m_pacerGpuWait{false};
    //! True if the waits the app makes by itself before a frame are reported to the pacer, as the command line said
    //! (--Pacer.SystemWaits)
    bool m_pacerSystemWaits{true};
    //! True if the app can wait until the GPU is done with a frame it names, as it said (SetGpuWaitSupport)
    bool m_gpuWaitSupported{false};
    //! The images of the swapchain of the app, as it said (SetSwapchainImageCount). Zero: not known.
    uint32_t m_swapchainImageCount{0};
    //! When m_vsyncTime was read, and the vertical blank the pacer was given last (it is only given a new one)
    TickCount m_vsyncReadTime;
    NanosecondTickCount m_pacerVBlankTime;
    //! The tiers that can be used right now, in the order of SamplePacerTierChoiceUtil::Tiers (the radio buttons that are
    //! enabled), and what the run is to be paced with: the kind that was asked for (m_pacerKind) or the one that comes closest to
    //! it. They are what Update decided.
    std::array<bool, 4> m_tierOffered{};
    SamplePacerKind m_selectedKind{SamplePacerKind::TimerPeriodOnly};
    //! The kind the radio buttons were last set to (empty: not yet). A button that is checked and is not its button was checked
    //! by the user.
    std::optional<SamplePacerKind> m_shownKind;
    //! What the start of the frame that is about to start waits for, and if the pacer was asked for it yet
    SamplePacerFrameStartPlan m_frameStartPlan;
    bool m_frameStartPlanned{false};
    //! True once the app was given the present of m_frameStartPlan to wait for (it is given once per frame)
    bool m_presentWaitRequested{false};
    //! True once the app was given the frame of m_frameStartPlan whose GPU work to wait for (it is given once per frame)
    bool m_gpuWaitRequested{false};
    //! How the frame that ended last is to be presented, as the pacer says
    SamplePacerPresentPlan m_presentPlan;
    //! The last present the frame pacer was told about: the id of the present where the app gives one (AddPresentCall), and the id
    //! the frame pacer gave the frame where the host presents (zero: none). A frame can be presented twice, when its first present
    //! was not taken, so the two are not the same thing.
    uint64_t m_reportedPresentId{0};
    uint64_t m_reportedPresentFrameId{0};
    //! The frame that runs long now and then (--CpuSpike): how long it is busy on top of the CPU load, how many frames apart they
    //! are, and what the current frame was busy for on top of the CPU load
    int32_t m_cpuSpikeMs{SampleConfig::CpuSpikeMs.Get()};
    int32_t m_cpuSpikeIntervalFrames{SampleConfig::CpuSpikeIntervalFrames.Get()};
    int32_t m_frameCpuSpikeMs{0};
    //! The refreshes of the pause the pacer makes once after it started (SampleConfig::StartupPauseRefreshes)
    int32_t m_startupPauseRefreshes{SampleConfig::StartupPauseRefreshes.Get()};
    //! What was last written to the log about the background, so a change is written as a event
    RaymarchScene m_loggedBackgroundScene{RaymarchScene::Flight};
    bool m_hasLoggedBackground{false};
    //! What the app said it can measure (SetMeasurementSupport)
    bool m_presentTimingSupported{false};
    bool m_gpuTimelineSupported{false};
    //! The duration of a refresh of the display according to the swapchain (zero if unknown)
    TimeSpan m_measuredRefreshDuration;

    //! The values of a frame the sample adds to the trace (--Trace): what its frame pacer planned and what the frame cost
    struct LogColumns
    {
      TraceValue PacerOn;
      TraceValue SwapInterval;
      TraceValue PreferredSwapInterval;
      TraceValue Change;
      TraceValue AnimationStep;
      TraceValue WindowFrames;
      TraceValue WindowLateFrames;
      TraceValue WindowStartsAhead;
      TraceValue WindowAverageWork;
      TraceValue WindowSpan;
      TraceValue WindowFull;
      TraceValue FrameWaitStart;
      TraceValue FrameWaitTarget;
      TraceValue FrameStart;
      TraceValue EndFrame;
      TraceValue WorkCpu;
      TraceValue WorkGpu;
      TraceValue WorkGpuFrameIndex;
      TraceValue GpuTime;
      TraceValue GpuWorkBegin;
      TraceValue GpuWorkEnd;
      TraceValue FlushCall;
      TraceValue PresentWait;
      TraceValue PresentWaitBegin;
      TraceValue PresentWaitTarget;
      TraceValue PresentWaitEnd;
      TraceValue CpuLoad;
      TraceValue GpuLoad;
      TraceValue PacerFrameId;
      TraceValue NextFrameStart;
      TraceValue DisplayReportsOn;
      TraceValue DisplayReportTime;
      TraceValue DisplayReports;
      TraceValue DisplayRefused;
      TraceValue DisplayJudgedFrames;
      TraceValue DisplayErrorFrames;
      TraceValue DisplayOffTargetFrames;
      TraceValue DisplayLateFrames;
      TraceValue DisplayStartToDisplayFrames;
      TraceValue DisplayStartToDisplayTotal;
      TraceValue DisplayStartToDisplayLongest;
      TraceValue AnimationError;
      TraceValue PresentTarget;
      TraceValue CpuSpike;
      TraceValue PacerKind;
      TraceValue RefreshesBehindClock;
      TraceValue PresentWaitTimeouts;
      TraceValue GpuWaitTimeouts;
      TraceValue PresentWaitsStopped;
      TraceValue VBlankReading;
      TraceValue VBlankJumps;
      TraceValue ShownLaterByWaits;
      TraceValue ReadyPlace;
      TraceValue ReadyPlaceTries;
      TraceValue ReadyPlaceTriesTakenBack;
      TraceValue SystemHeldFrames;
      TraceValue FrameSlotHeldFrames;
      TraceValue DisplayHeldRefreshes;
      TraceValue PacerGpuTime;
      TraceValue StartupPauses;
    };

    //! What the frame pacer has to be told about a present when its display time arrives
    struct PacerPresentFrame
    {
      uint64_t PresentId{0};
      //! The id the frame pacer gave the frame (zero = the frame was not paced)
      uint64_t PacerFrameId{0};
      //! The frame of the log (valid if HasLogFrame)
      TraceFrameIndex LogFrameIndex;
      bool HasLogFrame{false};
    };

    std::array<PacerPresentFrame, 64> m_pacerPresentFrames{};

    //! The present of a frame of the frame pacer: a pacer that names a present to wait for does it by the id it gave the frame
    struct PacerFramePresent
    {
      uint64_t PacerFrameId{0};
      uint64_t PresentId{0};
    };

    std::array<PacerFramePresent, 16> m_pacerFramePresents{};

    //! The id the frame pacer gave a frame of the sample: a GPU time arrives frames later, with the number of the sample's frame
    struct PacerFrame
    {
      uint64_t FrameId{0};
      uint64_t PacerFrameId{0};
    };

    std::array<PacerFrame, 16> m_pacerFrames{};

    //! The frame of the log a present belongs to
    struct LogPresentFrame
    {
      uint64_t PresentId{0};
      TraceFrameIndex FrameIndex;
    };

    //! The zones of the sample in the trace: what it does in its update and its draw, and where it waits
    struct TraceZones
    {
      TraceZone KeyboardMenu;
      TraceZone PacerUpdate;
      TraceZone TierUpdate;
      TraceZone StatsUI;
      TraceZone WorkChart;
      TraceZone AnimationError;
      TraceZone RunUI;
      TraceZone StartFrame;
      TraceZone WaitForFrameStart;
      TraceZone CpuLoad;
      TraceZone HoldBeforePresent;
      TraceZone SampleDraw;
      TraceZone DrawAnimation;
      TraceZone DrawBoxAnimation;
      TraceZone EndFrame;
      TraceZone WaitForPresent;
      TraceZone BackgroundDraw;
      TraceZone GpuTimer;
      TraceZone Measurements;
      TraceZone Flush;
    };

    //! The trace service, which is the log of the frames (null: the trace is off and nothing is logged)
    std::shared_ptr<ITraceService> m_trace;
    TraceZones m_traceZones;

    LogColumns m_logColumns;
    std::array<LogPresentFrame, 64> m_logPresentFrames{};

    //! The frame of the sample a present belongs to (a app reports when the GPU worked on a frame with the id of its present)
    struct PresentFrame
    {
      uint64_t PresentId{0};
      uint64_t FrameId{0};
    };

    std::array<PresentFrame, 16> m_presentFrames{};

    //! The frame of the log a frame of the sample belongs to
    struct LogFrame
    {
      uint64_t FrameId{0};
      TraceFrameIndex FrameIndex;
    };

    std::array<LogFrame, 64> m_logFrames{};
    //! True while what the frame that started is has not been written to the log. A frame gets its row in the log when the host begins
    //! its draw, so a frame that starts in the update is written once it is drawn.
    bool m_frameStartLogPending{false};
    //! When the frame that is still to be written began to wait for its start, and the time its start was held to (zero: not held)
    TickCount m_frameStartLogWaitTime;
    TickCount m_frameStartLogTargetTime;
    //! The frame of the log the GPU time that was reported last was measured on (empty: none was, or its frame is not in the log)
    std::optional<TraceFrameIndex> m_gpuTimeLogFrameIndex;
    //! What was last written to the log about the settings of the frame pacer, so a change is written as a event
    SamplePacerConfig m_loggedPacerConfig;
    bool m_loggedPacerOn{false};
    bool m_hasLoggedPacerConfig{false};

  public:
    //! The color the app should clear the screen with
    static constexpr Color ClearColor = Color(0xFF333333);

    //! @param runName the base name of the runs this sample starts (normally the app name)
    //! @param presentMethod how the app holds a frame for the display refreshes the frame pacer decides on
    FramePacingShared(const DemoAppConfig& config, std::string runName, const SamplePresentMethod presentMethod);
    ~FramePacingShared() final;

    [[nodiscard]] std::shared_ptr<UIDemoAppExtension> GetUIDemoAppExtension() const
    {
      return m_uiExtension;
    }

    // From EventListener
    void OnClickInput(const std::shared_ptr<UI::WindowInputClickEvent>& theEvent) final;
    void OnSelect(const std::shared_ptr<UI::WindowSelectEvent>& theEvent) final;

    // Called from the parent app
    void OnKeyEvent(const KeyEvent& event);
    //! The window got or lost the input focus: the trace gets a event for it
    void OnWindowFocusEvent(const WindowFocusEvent& event);
    void ConfigurationChanged(const DemoWindowMetrics& windowMetrics);
    //! @param demoTime the update time of the frame. The frame is animated for it unless the frame pacer is on.
    void Update(const DemoTime& demoTime);
    //! Draw the frame for its animation time (this is exactly what the marker reports)
    void Draw();
    //! Call it once the app has drawn the frame (on Vulkan: once the frame was submitted), with the frame pacer on or off. The work
    //! of the CPU on the frame ends here and the frame pacer says how the frame is to be presented. The app does not wait for the
    //! GPU before it.
    //! @param gpuTime the time the GPU needs for a frame, measured by the app without waiting for the GPU (a timer or timestamp query
    //!        of an earlier frame), zero if the app can not measure it. It is for the stats and the trace: the frame pacer is given
    //!        the work of the GPU with AddGpuTime and AddGpuInterval.
    void EndFrame(const TimeSpan gpuTime = {});
    //! Wait until the time the frame pacer gave for the present of the frame that just ended (EndFrame), where it gave one. Returns
    //! at once if the frame pacer is off or gave none.
    void WaitForPresent();
    //! The wait for the start of a frame, for an app that can make it before the frame takes anything from the display (a Vulkan
    //! app makes it before it acquires a swapchain image). Without this call the wait is made when the frame starts in the draw.
    void WaitForFrameStart();

    //! For a app that can wait until a present was shown (Vulkan with VK_KHR_present_wait2): the number of presents that may wait to
    //! be shown while a frame is made, zero if it can not wait for one right now. Call it every frame before Update, as it can change
    //! when the swapchain is recreated. A app that can never wait for a present does not call it.
    void SetPresentWaitSupport(const uint32_t waitingPresents) noexcept
    {
      m_presentWaitingPresents = waitingPresents;
    }
    //! The frames the app lets be in the works at the same time (the frames in flight of a Vulkan app), one or more. A pacer that is
    //! given the work of the GPU needs it. A app that does not know leaves it at one.
    void SetMaxFramesInFlight(const uint32_t maxFramesInFlight) noexcept
    {
      m_maxFramesInFlight = maxFramesInFlight;
    }
    //! The images the swapchain of the app has (zero: not known). The pacer keeps no more frames ahead of the display than a
    //! swapchain of that many images can hold.
    void SetSwapchainImageCount(const uint32_t imageCount) noexcept
    {
      m_swapchainImageCount = imageCount;
    }
    //! A wait of the app before the frame starts that the pacer did not ask for: the wait for a frame slot, the acquire of a image.
    //! Call it after the wait and before the frame is started (StartFrame), for every such wait, also one that returned at once.
    void AddSystemWait(const SamplePacerSystemWait kind, const TickCount beginTime, const TickCount endTime);
    //! True if the app base is to wait for a present before a frame by itself, where it was asked to: without the frame pacer.
    //! False: the frame pacer says which present a frame waits for (GetPresentWaitRequest), or it paces with a kind without such a
    //! wait. It is what Update decided.
    [[nodiscard]] bool IsPresentWaitByHost() const noexcept
    {
      return m_pacer == nullptr;
    }
    //! Before a frame takes anything: the present the frame pacer wants the app to wait for until it was shown. Call it before
    //! WaitForFrameStart, wait for the present if there is one and say what became of the wait with AddPresentWait. A present is
    //! only given once per frame.
    [[nodiscard]] SamplePresentWaitRequest GetPresentWaitRequest();
    //! What became of the wait for the present GetPresentWaitRequest gave (HighResolutionTimer timestamps).
    //! @param shown true if the wait ended because the present was shown, false if it ended without that
    void AddPresentWait(const uint64_t presentId, const TickCount beginTime, const TickCount endTime, const bool shown);
    //! True if the app can wait until the GPU is done with a frame it names (a fence, the frame slot of a Vulkan app). The pacer
    //! then holds the loop with that wait where it was asked to (--Pacer.GpuWait).
    void SetGpuWaitSupport(const bool supported) noexcept
    {
      m_gpuWaitSupported = supported;
    }
    //! Before a frame takes anything: the frame the frame pacer wants the app to wait for until the GPU is done with it. Call it
    //! after GetPresentWaitRequest and before WaitForFrameStart, wait for the frame if there is one and say what became of the wait
    //! with AddGpuWait. A frame is only given once per frame.
    [[nodiscard]] SampleGpuWaitRequest GetGpuWaitRequest();
    //! What became of the wait for the frame GetGpuWaitRequest gave (HighResolutionTimer timestamps).
    //! @param done true if the wait ended because the GPU was done with the frame, false if its time ran out
    void AddGpuWait(const uint64_t presentId, const TickCount beginTime, const TickCount endTime, const bool done);
    //! The present with the given id was called (HighResolutionTimer timestamps). Call it after the present and before the next frame
    //! takes anything (before GetPresentWaitRequest and WaitForFrameStart), it can be called again for the same present. A app whose
    //! host presents its frames (a swap) does not call it: the sample asks the frame pacing service for the times of the swap.
    //! @param accepted false if the system did not take the present, so the frame will not be shown
    void AddPresentCall(const uint64_t presentId, const TickCount callTime, const TickCount returnTime, const bool accepted);
    //! The app made its swapchain anew: the presents from before are gone, which the frame pacer is told
    void OnSwapchainRecreated();
    //! True if the frame pacer gave the frame that just ended (EndFrame) a time to wait until before its present. The app then calls
    //! WaitForPresent, whatever its present holds the frame for.
    [[nodiscard]] bool IsPresentWaitPlanned() const noexcept
    {
      return m_pacer != nullptr && m_presentPlan.PresentTime.Ticks() != 0;
    }

    //! Tell the sample which optional measurements the app can make. Each has a switch, so what it adds can be seen by switching it off.
    //! @param presentTimingSupported the app can measure when its frames reach the display (VK_EXT_present_timing)
    //! @param gpuTimelineSupported the app can tell when the GPU worked on a frame on the clock of the CPU (VK_KHR_calibrated_timestamps)
    void SetMeasurementSupport(const bool presentTimingSupported, const bool gpuTimelineSupported);
    //! Tell the sample if the app can give a present a target time (the time the frame before stays on screen at least). The frame
    //! pacer plans that time where it is asked to. Call it every frame, as it can change when the swapchain is recreated.
    void SetTimedPresentSupport(const bool supported);

    //! @brief A app that presents with a swap interval says what the longest one is that it can set (the EGL config decides it, and it
    //!        can be one). It is what the tier the side bar shows as the best of the system depends on.
    void SetPresentSwapIntervalMax(const uint32_t maxSwapInterval) noexcept
    {
      m_presentSwapIntervalMax = maxSwapInterval;
    }
    //! The time the present of the current frame is to be given, after WaitForPresent: the frame is not to be shown before this long
    //! after the frame before it was shown. Zero if the frame pacer gave none.
    [[nodiscard]] TimeSpan GetPresentRelativeTarget() const noexcept
    {
      return m_presentRelativeTarget;
    }
    //! True if the app should measure when its frames reach the display (supported and its switch is on)
    [[nodiscard]] bool IsPresentTimingWanted() const;
    //! True if the app should report when the GPU worked on its frames (supported and its switch is on)
    [[nodiscard]] bool IsGpuTimelineWanted() const;
    //! Tell the sample if the app measures when its frames reach the display (VK_EXT_present_timing). Call it every frame, as it can change
    //! when the swapchain is recreated.
    //! @param refreshDuration the duration of a refresh of the display according to the swapchain (zero if unknown)
    void SetPresentsMeasured(const bool enabled, const TimeSpan refreshDuration = {});
    //! Tell the sample what the swapchain says about how the display refreshes. Call it every frame, an app without a swapchain that
    //! says does not call it.
    void SetSwapchainRefresh(const SampleSwapchainRefresh swapchainRefresh) noexcept
    {
      m_swapchainRefresh = swapchainRefresh;
    }
    //! True once the display was seen to refresh at a variable rate: the frame pacer is not given the vertical blank times then
    [[nodiscard]] bool IsVariableRefreshSeen() const noexcept
    {
      return m_variableRefreshSeen;
    }
    //! The id of the present of the frame being drawn. Call it during the app's draw after GetRaymarchParams, which starts the frame.
    void SetFramePresentId(const uint64_t presentId);
    //! A present was measured.
    //! @param displayTime when the frame reached the display as a HighResolutionTimer timestamp (empty if it was not reported)
    //! @param queueOperationsEndTime when the present was handed to the presentation engine (empty if not reported)
    //! @param isComplete true if the presentation engine is done with the present. A complete present without a display time is
    //!        counted as not shown by the chart of the animation error. The frame pacer is told nothing about it.
    void AddPresentTiming(const uint64_t presentId, const std::optional<TickCount> displayTime, const std::optional<TickCount> queueOperationsEndTime,
                          const bool isComplete);
    //! The GPU work of a frame was measured (HighResolutionTimer timestamps).
    void AddGpuInterval(const uint64_t presentId, const TickCount gpuStartTime, const TickCount gpuEndTime);
    //! The number of the frame the sample started last, counted from one. The app keeps it for a frame once the frame was started
    //! (SamplePresentMethod says where) and gives it back with the GPU time of the frame, which is only known a frame or more later.
    [[nodiscard]] uint64_t GetFrameId() const noexcept
    {
      return m_frameId;
    }
    //! The GPU time of a frame was measured. Call it for every result, before EndFrame is given the newest one: the log then says
    //! which frame a GPU time belongs to.
    //! @param frameId what GetFrameId returned for the frame the time was measured on
    //! @param gpuEndTime when the GPU finished the frame as a HighResolutionTimer timestamp, for an app that does not report it with
    //!        AddGpuInterval (empty if it is not known)
    void AddGpuTime(const uint64_t frameId, const TimeSpan gpuTime, const std::optional<TickCount> gpuEndTime = {});
    //! True if the app is to flush its commands after the last one of a frame (--GLFlush, only for a app that presents with a swap)
    [[nodiscard]] bool IsFlushWanted() const noexcept
    {
      return m_flushWanted;
    }
    //! The app related the clock its GPU times are on to the clock of the framework anew: the trace gets a event with how far
    //! off a time can be. Call it during the draw of a frame.
    //! @param readTime how long the read of the clock of the GPU took, a time can be off by up to half of it
    void AddGpuClockCalibration(const TimeSpan readTime);
    //! The same for a app that is told how far off a read can be and that measures the rate of the clock of the GPU (Vulkan).
    //! @param readTime how long the read of the two clocks took
    //! @param maxDeviation how far the two clocks can be from having been read at the same moment
    //! @param clockRateDeviationPpm how much longer (positive) or shorter a count of the clock of the GPU takes than the device
    //!        states, in parts per million. Empty until it was measured.
    void AddGpuClockCalibration(const TimeSpan readTime, const TimeSpan maxDeviation, const std::optional<double> clockRateDeviationPpm);
    //! The app flushes its commands now: the trace gets the time. Call it right before the flush, during the draw of the frame.
    void MarkFlush();

    //! The trace service for the zones of the app, null if the trace is off
    [[nodiscard]] ITraceService* TryGetTrace() const noexcept
    {
      return m_trace.get();
    }
    //! The zone of the trace for the app's draw of the background
    [[nodiscard]] TraceZone GetBackgroundDrawZone() const noexcept
    {
      return m_traceZones.BackgroundDraw;
    }
    //! The zone of the trace for the app's work with its GPU timer at the start of a frame
    [[nodiscard]] TraceZone GetGpuTimerZone() const noexcept
    {
      return m_traceZones.GpuTimer;
    }
    //! The zone of the trace for where the app hands the sample what it measured
    [[nodiscard]] TraceZone GetMeasurementsZone() const noexcept
    {
      return m_traceZones.Measurements;
    }
    //! The zone of the trace for the app's flush of its commands
    [[nodiscard]] TraceZone GetFlushZone() const noexcept
    {
      return m_traceZones.Flush;
    }

    //! What the app draws the raymarched background of the current frame with, before it calls Draw (the GPU load of the sample).
    //! Call it during the app's draw, as the frame can start there.
    [[nodiscard]] RaymarchParams GetRaymarchParams();

    [[nodiscard]] bool IsPacerEnabled() const noexcept
    {
      return m_pacer != nullptr;
    }

    //! The swap interval the present of the frame that just ended is to be given, for a present that takes one: what the frame
    //! pacer says, which is never more than the longest one the app said its present takes (SetPresentSwapIntervalMax). 1 if the
    //! frame pacer is off. Valid once the frame ended (EndFrame).
    [[nodiscard]] uint32_t GetSwapInterval() const noexcept
    {
      return m_pacer != nullptr ? m_presentPlan.SwapInterval : 1u;
    }

  private:
    void ToggleRun();
    void StartTimedRun();
    void UpdateUI();
    //! Apply the pacer settings of the UI
    void UpdatePacer();
    //! The scene of the background that is selected in the UI
    [[nodiscard]] RaymarchScene GetBackgroundScene() const;
    //! Start the frame: the frame pacer plans it, its animation time is set and the simulated CPU load runs
    void StartFrame();
    //! Ask the frame pacer what the start of the frame that is about to start waits for, unless that was done already. The present
    //! of the frame before is reported first where the sample is the one that knows of it.
    void PlanFrameStart();
    //! Tell the frame pacer about a present of a frame it paced
    void ReportPresent(const uint64_t pacerFrameId, const TickCount callTime, const TickCount returnTime, const bool accepted);
    //! Sleep (a long wait) or yield (a short wait) until the given HighResolutionTimer timestamp
    //! @param zone the zone of the trace the wait is, which is only there if there is something to wait for
    void WaitUntil(const TickCount time, const TraceZone zone) const;
    //! What the app can do on this system right now, as far as the tiers depend on it
    [[nodiscard]] SamplePacerCapabilities GetPacerCapabilities() const noexcept;
    //! @brief True if the pacer is to use the present that takes a time: the switch is on and the app has such a present
    [[nodiscard]] bool IsTimedPresentRequested() const;
    //! The kind that is asked for from here on (--Pacer.Kind, then the one the user checked last)
    void SetRequestedKind(const SamplePacerKind kind) noexcept;
    //! Take a tier the user checked as the one that is asked for
    void ReadTierRadioButtons();
    //! Work out which tiers can be used right now and what the run is to be paced with
    void UpdateTierChoice();
    //! Enable the radio buttons of the tiers that can be used and check the one of the given kind
    void ShowTiers(const SamplePacerKind kind);
    //! @return the refresh period of the display the window is on, in nanoseconds (zero if the window system does not know it)
    [[nodiscard]] NanosecondTimeSpan ReadDisplayRefreshPeriod() const;
    //! Take the refresh period the window system reports now (m_detectedRefreshPeriod)
    //! @return true if it changed
    bool UpdateDetectedRefreshPeriod();
    //! @return the refresh rate the frame pacer uses in Hz: the command line, else the window system, else the slider
    [[nodiscard]] double GetRefreshRateHz() const;
    //! @return the refresh period the frame pacer is given: the one of the window system as it is where that is used, else the
    //!         period of the rate of the command line or the slider
    [[nodiscard]] NanosecondTimeSpan GetRefreshPeriod() const;
    void UpdateRefreshRateUI();
    //! Work out the tiers of the pacer library the run uses and the best this system reaches, and show and log them if they changed
    void UpdateTier();
    //! Update the frame pacing section of the stats panel
    void UpdatePacerStats();
    //! Write the values of the frame pacing section and say how much each asks to be looked at (m_pacerStatsLevels)
    void FillPacerStats();
    //! The time a frame is to take right now: what the frame pacer says, and one refresh without it (zero if it is not known)
    [[nodiscard]] TimeSpan GetTargetFrameTime() const noexcept;
    //! Update the rows of the frame pacing section that show what the measured presents say
    void UpdateMeasuredPresentsStats();
    void UpdateVariableRefreshStats();
    //! Add the values of the sample to the trace
    void RegisterLogColumns();
    //! Write a wait before the present to the trace: when it began, the time it aimed at and when it woke
    void LogPresentWait(const TickCount beginTime, const TickCount targetTime, const TickCount endTime);
    //! Write what the frame that just started is to the trace
    void LogFrameStart(const TickCount waitStartTime);
    //! Keep the CPU busy for the given time (the simulated CPU load)
    void BurnCpu(const TimeSpan duration) const;
    //! Create the two overlays: every value of the last marker and the frame pacing stats (fills in the overlay members of m_ui)
    std::shared_ptr<UI::BaseWindow> CreateStatsWindow(UI::Theme::IThemeControlFactory& rUIFactory);
    //! Create the bar at the bottom with the chart of the work per frame
    std::shared_ptr<UI::BaseWindow> CreateWorkChartBar(UI::Theme::IThemeControlFactory& rUIFactory);
    std::shared_ptr<UI::BaseWindow> CreateAnimationErrorChartBar(UI::Theme::IThemeControlFactory& rUIFactory);
    //! Take the frames whose display times have come: their animation error goes to the chart and to the trace
    void UpdateAnimationError();
    //! Show the overlays and the chart their switches are on for
    void UpdateStatsVisibility();
    //! Let the service draw the sync marker while its switch is on
    void UpdateSyncMarker();
    void UpdateMarkerStats();

    //! Format into the reused buffer and set it as the label content (the label only copies it if the text changed)
    template <typename... TArgs>
    void SetFormattedContent(UI::Label& rLabel, fmt::format_string<TArgs...> format, TArgs&&... args)
    {
      m_formatBuffer.clear();
      fmt::format_to(std::back_inserter(m_formatBuffer), format, std::forward<TArgs>(args)...);
      rLabel.SetContent(StringViewLite(m_formatBuffer.data(), m_formatBuffer.size()));
    }
    void DrawAnimation(const double animationSeconds);
    void DrawBoxAnimation(const double animationSeconds);
  };
}

#endif
