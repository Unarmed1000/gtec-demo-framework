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
#include <FslBase/Time/TickCount.hpp>
#include <FslBase/Time/TimeSpan.hpp>
#include <FslDemoApp/Base/DemoAppConfig.hpp>
#include <FslDemoApp/Base/DemoTime.hpp>
#include <FslDemoService/FramePacingMarker/FramePacingLogColumn.hpp>
#include <FslDemoService/FramePacingMarker/FramePacingRunState.hpp>
#include <FslGraphics/Color.hpp>
#include <FslGraphics/Render/Texture2D.hpp>
#include <FslSimpleUI/App/UIDemoAppExtension.hpp>
#include <FslSimpleUI/Base/Control/BackgroundLabelButton.hpp>
#include <FslSimpleUI/Base/Control/Label.hpp>
#include <FslSimpleUI/Base/Control/RadioButton.hpp>
#include <FslSimpleUI/Base/Control/SliderAndFmtValueLabel.hpp>
#include <FslSimpleUI/Base/Control/Switch.hpp>
#include <Shared/FramePacing/RaymarchParams.hpp>
#include <Shared/FramePacing/SampleConfig.hpp>
#include <Shared/FramePacing/SampleFrameStats.hpp>
#include <Shared/FramePacing/SamplePacer.hpp>
#include <Shared/FramePacing/SamplePacerHold.hpp>
#include <Shared/FramePacing/SamplePacerProfile.hpp>
#include <Shared/FramePacing/SamplePresentFeedback.hpp>
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
  class IFramePacingFrameLog;
  class IFramePacingMarkerService;
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

  //! How the app holds a frame for the display refreshes the frame pacer of the sample decides on
  enum class SamplePresentMethod
  {
    //! The app presents with a swap interval (eglSwapInterval), so the present waits for the display and the next frame starts when
    //! the frame is shown. The frame starts at Update.
    SwapInterval,
    //! The present holds a frame for one refresh only (a Vulkan FIFO present), so the app waits before it presents (WaitForPresent).
    //! The frame starts at Draw, which the host calls after it waited for a free buffer.
    WaitThenPresent
  };

  //! Shows how an app can control the mb-framepacing frame marker through the IFramePacingMarkerService.
  //! The marker itself is drawn by the host on top of every frame, so apps only need to enable it (or use --FramePacing).
  //! All rendering goes through the API independent INativeBatch2D so the same code is used by the GLES2, GLES3 and Vulkan samples.
  //!
  //! The sample can also pace its frames with the experimental frame pacer of the mb-framepacing SDK (SamplePacer). The framework has
  //! no frame pacer, so the pacer lives in the sample: it gives every frame its animation time and the swap interval to hold it for.
  //! The app applies the swap interval (see SamplePresentMethod) and the marker service is told what the frame was paced by
  //! (IFramePacingMarkerService::SetFrameSchedule).
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
      std::shared_ptr<UI::Label> SwapInterval;
      std::shared_ptr<UI::Label> FrameTime;
      std::shared_ptr<UI::Label> LateFrames;
      std::shared_ptr<UI::Label> Work;
      std::shared_ptr<UI::Label> AverageWork;
      std::shared_ptr<UI::Label> PresentWait;
      std::shared_ptr<UI::Label> IntervalChanges;
      std::shared_ptr<UI::Label> LastChange;
      std::shared_ptr<UI::Label> FrameWindow;
      std::shared_ptr<UI::Label> Feedback;
      std::shared_ptr<UI::Label> FeedbackLate;
      std::shared_ptr<UI::Label> VariableRefresh;
      //! What the measured presents say (only an app that measures them has values for these)
      std::shared_ptr<UI::Label> DisplayError;
      std::shared_ptr<UI::Label> DisplayInterval;
      std::shared_ptr<UI::Label> Latency;
      std::shared_ptr<UI::Label> TimedFrames;
      std::shared_ptr<UI::Label> GpuWork;
      std::shared_ptr<UI::Label> DisplayRefresh;
    };

    //! What the swap interval rule of the frame pacer changed since the pacer was set up
    struct PacerChangeRecord
    {
      uint32_t SlowerCount{0};
      uint32_t FasterCount{0};
      SamplePacerChange LastChange{SamplePacerChange::Unchanged};
      //! When the frame of the last change started (a HighResolutionTimer timestamp)
      TickCount LastChangeTime;
    };

    //! The marker the stats panel showed last, so the panel can show how far the times of a marker are from the ones of the marker
    //! before it
    struct MarkerStepRecord
    {
      bool HasMarker{false};
      uint64_t FrameIndex{0};
      TimeSpan AnimationTime;
      std::optional<TickCount> CpuStartTime;
      //! The step from the marker before to this marker (empty if there is no marker before or a time is unknown)
      std::optional<TimeSpan> AnimationStep;
      std::optional<TimeSpan> CpuStartStep;
    };

    struct UIRecord
    {
      std::shared_ptr<UI::Label> LabelStatus;
      std::shared_ptr<UI::Label> LabelRun;
      std::shared_ptr<UI::BackgroundLabelButton> ButtonRun;
      std::shared_ptr<UI::BackgroundLabelButton> ButtonTimedRun;
      std::shared_ptr<UI::SliderAndFmtValueLabel<int32_t>> SliderDuration;
      std::shared_ptr<UI::Switch> SwitchPacer;
      std::shared_ptr<UI::Label> LabelRefreshRate;
      std::shared_ptr<UI::SliderAndFmtValueLabel<int32_t>> SliderRefreshRate;
      std::shared_ptr<UI::Label> LabelPacedRate;
      std::shared_ptr<UI::SliderAndFmtValueLabel<int32_t>> SliderTargetFps;
      std::shared_ptr<UI::Switch> SwitchAdaptive;
      std::shared_ptr<UI::Switch> SwitchPacerFeedback;
      std::shared_ptr<UI::RadioButton> RadioHoldAuto;
      std::shared_ptr<UI::RadioButton> RadioHoldVSync;
      std::shared_ptr<UI::RadioButton> RadioHoldWait;
      std::shared_ptr<UI::RadioButton> RadioHoldSchedule;
      std::shared_ptr<UI::Label> LabelPacerStatus;
      std::shared_ptr<UI::Label> LabelPacerFrames;
      std::shared_ptr<UI::SliderAndFmtValueLabel<int32_t>> SliderCpuLoad;
      std::shared_ptr<UI::SliderAndFmtValueLabel<int32_t>> SliderGpuLoad;
      //! The scene of the raymarched background
      std::shared_ptr<UI::RadioButton> RadioBackgroundFlight;
      std::shared_ptr<UI::RadioButton> RadioBackgroundHall;
      //! The two overlays: the values of the last marker and the frame pacing stats, each can be hidden with its switch
      std::shared_ptr<UI::Switch> SwitchMarkerStats;
      std::shared_ptr<UI::Switch> SwitchPacerStats;
      std::shared_ptr<UI::BaseWindow> MarkerStatsOverlay;
      std::shared_ptr<UI::BaseWindow> PacerStatsOverlay;
      //! The chart of the work per frame at the bottom, and the test pattern (the moving bar and box): each has a switch too
      std::shared_ptr<UI::Switch> SwitchWorkChart;
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
    //! The refresh rate the window system reports (0 = unknown, the slider is used)
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
    //! When the current frame started (a HighResolutionTimer timestamp)
    TickCount m_frameStartTime;
    //! The time from the start of the previous frame to the start of the current frame
    TimeSpan m_frameInterval;
    //! The frames of the last two seconds: their frame times, and the late frames while the frame pacer is off (the pacer counts its
    //! own)
    SampleFrameStats m_frameStats;
    PacerChangeRecord m_pacerChanges;
    //! How long the CPU and the GPU worked on the last frame that ended (the GPU time is zero if the app does not measure it)
    TimeSpan m_lastCpuTime;
    TimeSpan m_lastGpuTime;
    //! How long the present of the last frame that ended was delayed by WaitForPresent
    TimeSpan m_lastPresentWait;
    //! When the app started to wait for the GPU in the current frame (BeginGpuWait), zero if it did not
    TickCount m_gpuWaitStartTime;
    //! The work per frame of the chart: the CPU time and the GPU time in microseconds
    std::shared_ptr<UI::ChartData> m_workChartData;
    //! True if a frame ended since the chart was updated
    bool m_workSamplePending{false};
    //! After a delayed present (WaitForPresent) the next frame does not start before this (zero = no wait)
    TickCount m_nextFrameStartTime;
    //! The time the current frame is animated for
    TimeSpan m_animationTime;
    //! The animation time minus the time of the framework (zero until the frame pacer was used), so the animation does not jump when the
    //! frame pacer is switched off
    TimeSpan m_animationOffset;
    //! Relates the presents the app measured to the frames of the sample
    SamplePresentFeedback m_presentFeedback;
    //! True if the app measures when its frames are presented
    bool m_presentFeedbackEnabled{false};
    //! True if the app can give a present a target time (SetPresentSchedulingSupport)
    bool m_presentSchedulingSupported{false};
    //! The target time of the present of the current frame, counted from when the frame before it was shown (zero = not scheduled)
    TimeSpan m_presentRelativeTarget;
    //! When the display refreshes according to the window system, read once per frame: the time of a vertical blank and the time
    //! between two refreshes (both zero if the platform does not say)
    TickCount m_vsyncTime;
    TimeSpan m_vsyncPeriod;
    //! True once the display was seen to refresh at a variable rate: the window says so, it was measured, or the swapchain says so.
    //! The vertical blanks of such a display follow the frames, so the vsync wait can not hold a frame and the sample does not use it.
    //! It stays set until the hold selection or the refresh rate of the display changes, so the method does not flip back and forth.
    bool m_variableRefreshSeen{false};
    //! The hold selection and the refresh rate of the display that m_variableRefreshSeen belongs to
    SamplePacerHold m_variableRefreshSeenHold{SamplePacerHold::Auto};
    double m_variableRefreshSeenRateHz{0.0};
    //! What was last written to the log about it
    bool m_loggedVariableRefreshSeen{false};
    //! What the swapchain of the app says about the refresh of the display (SetSwapchainRefresh)
    SampleSwapchainRefresh m_swapchainRefresh{SampleSwapchainRefresh::Unknown};
    //! Where in the refresh before the one a frame is aimed at the vsync wait presents, in percent of the refresh
    int32_t m_vsyncPhasePercent{SampleConfig::VSyncPhasePercent.Get()};
    //! When WaitForFrameStart began to wait for the frame that is about to start (zero: it was not called for it)
    TickCount m_frameStartWaitTime;
    //! Where a frame waits for the time the frame pacer gives for the start of the next frame
    SamplePacerProfile m_pacerProfile{SamplePacerProfile::RenderEarly};
    //! SamplePacerProfile::RenderEarly: when the present of the frame before was due and when it was let go (zero: there was none)
    TickCount m_presentDueTime;
    TickCount m_presentTime;
    //! SamplePacerProfile::RenderLate: the time the start of the current frame was held to (zero: it was not held)
    TickCount m_frameStartDueTime;
    //! The refreshes of the wait that is made once when the waiting for the time of the pacer begins (SampleConfig::DrainRefreshes),
    //! and if that wait is still to be made
    int32_t m_drainRefreshes{SampleConfig::DrainRefreshes.Get()};
    bool m_drainPending{true};
    //! When the first frame waited for the time of the pacer (zero: none has yet), the wait above is made a while after it
    TickCount m_drainCountStartTime;
    //! What was last written to the log about how the frames are held
    SamplePacerHold m_loggedHold{SamplePacerHold::Wait};
    //! What the app said it can measure (SetMeasurementSupport)
    bool m_presentTimingSupported{false};
    bool m_gpuTimelineSupported{false};
    //! The duration of a refresh of the display according to the swapchain (zero if unknown)
    TimeSpan m_measuredRefreshDuration;

    //! The columns the sample adds to the frame pacing log (--FramePacing.Log): what its frame pacer planned and what the frame cost
    struct LogColumns
    {
      FramePacingLogColumn PacerOn;
      FramePacingLogColumn SwapInterval;
      FramePacingLogColumn PreferredSwapInterval;
      FramePacingLogColumn Change;
      FramePacingLogColumn AnimationStep;
      FramePacingLogColumn WindowFrames;
      FramePacingLogColumn WindowLateFrames;
      FramePacingLogColumn WindowStartsAhead;
      FramePacingLogColumn WindowAverageWork;
      FramePacingLogColumn WindowSpan;
      FramePacingLogColumn WindowFull;
      FramePacingLogColumn FrameWaitStart;
      FramePacingLogColumn FrameStart;
      FramePacingLogColumn EndFrame;
      FramePacingLogColumn WorkCpu;
      FramePacingLogColumn WorkGpu;
      FramePacingLogColumn GpuWorkBegin;
      FramePacingLogColumn GpuWorkEnd;
      FramePacingLogColumn PresentWait;
      FramePacingLogColumn CpuLoad;
      FramePacingLogColumn GpuLoad;
      FramePacingLogColumn PacerFrameId;
      FramePacingLogColumn NextFrameStart;
      FramePacingLogColumn FeedbackOn;
      FramePacingLogColumn FeedbackDisplay;
      FramePacingLogColumn FeedbackPresent;
      FramePacingLogColumn FeedbackReportedNotShown;
      FramePacingLogColumn FeedbackUsed;
      FramePacingLogColumn FeedbackRefused;
      FramePacingLogColumn FeedbackNotShown;
      FramePacingLogColumn FeedbackMissing;
      FramePacingLogColumn FeedbackLateRefreshes;
      FramePacingLogColumn HoldMethod;
      FramePacingLogColumn HoldTarget;
      FramePacingLogColumn PresentTarget;
    };

    //! What the frame pacer has to be told about a present when its display time arrives
    struct PacerPresentFrame
    {
      uint64_t PresentId{0};
      //! The id the frame pacer gave the frame (zero = the frame was not paced)
      uint64_t PacerFrameId{0};
      //! When the present was called (valid if HasPresentCallTime)
      TickCount PresentCallTime;
      bool HasPresentCallTime{false};
      //! The frame of the log (valid if HasLogFrame)
      uint64_t LogFrameIndex{0};
      bool HasLogFrame{false};
    };

    std::array<PacerPresentFrame, 64> m_pacerPresentFrames{};

    //! The frame of the log a present belongs to
    struct LogPresentFrame
    {
      uint64_t PresentId{0};
      uint64_t FrameIndex{0};
    };

    //! The frame pacing log (null if the frames are not logged)
    std::shared_ptr<IFramePacingFrameLog> m_frameLog;
    LogColumns m_logColumns;
    std::array<LogPresentFrame, 64> m_logPresentFrames{};
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
    void OnSelect(const std::shared_ptr<UI::WindowSelectEvent>& theEvent) final;

    // Called from the parent app
    void OnKeyEvent(const KeyEvent& event);
    void ConfigurationChanged(const DemoWindowMetrics& windowMetrics);
    //! @param demoTime the update time of the frame. The frame is animated for it unless the frame pacer is on.
    void Update(const DemoTime& demoTime);
    //! Draw the frame for its animation time (this is exactly what the marker reports)
    void Draw();
    //! Call it when the CPU is done with the frame and the app starts to wait for the GPU (glFinish), before EndFrame. The wait is
    //! then the GPU time of the frame, for an app that can not measure the time the GPU works on a frame.
    void BeginGpuWait();
    //! Call it once the app has drawn the frame (on Vulkan: once the frame was submitted), with the frame pacer on or off. It tells
    //! the frame pacer how long the frame needed.
    //! @param gpuTime the time the GPU needs for a frame if the app measures it apart from the CPU time (it is added to the CPU time)
    void EndFrame(const TimeSpan gpuTime = {});
    //! Wait until the frame can be presented so it is shown at the refresh the frame pacer aims for: for an app whose present can not
    //! hold a frame for the whole swap interval (SamplePresentMethod::WaitThenPresent, or a EGL config with a short max swap interval).
    //! Returns at once if the frame pacer is off or the present holds the frame long enough.
    //! @param presentSwapInterval the number of display refreshes the present itself holds the frame for
    void WaitForPresent(const uint32_t presentSwapInterval = 1);
    //! The wait for the start of a frame, for an app that can make it before the frame takes anything from the display (a Vulkan
    //! app makes it before it acquires a swapchain image). Without this call the wait is made when the frame starts in the draw.
    void WaitForFrameStart();

    //! Tell the sample which optional measurements the app can make. Each has a switch, so what it adds can be seen by switching it off.
    //! @param presentTimingSupported the app can measure when its frames reach the display (VK_EXT_present_timing)
    //! @param gpuTimelineSupported the app can tell when the GPU worked on a frame on the clock of the CPU (VK_KHR_calibrated_timestamps)
    void SetMeasurementSupport(const bool presentTimingSupported, const bool gpuTimelineSupported);
    //! Tell the sample if the app can give a present a target time, so the presentation engine holds a frame for its swap interval and
    //! the sample does not have to wait before it presents. Call it every frame, as it can change when the swapchain is recreated.
    void SetPresentSchedulingSupport(const bool supported);
    //! How the frames are held right now: the method that was asked for, or the one the system falls back to (never Auto).
    //! The vsync wait falls back to the sleep once the display was seen to refresh at a variable rate.
    [[nodiscard]] SamplePacerHold GetHoldMethod() const;
    //! The target time of the present of the current frame after WaitForPresent: the frame is not to be shown before this long after the
    //! frame before it was shown. Zero if the present is not scheduled.
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
    void SetPresentFeedback(const bool enabled, const TimeSpan refreshDuration = {});
    //! Tell the sample what the swapchain says about how the display refreshes. Call it every frame, an app without a swapchain that
    //! says does not call it.
    void SetSwapchainRefresh(const SampleSwapchainRefresh swapchainRefresh) noexcept
    {
      m_swapchainRefresh = swapchainRefresh;
    }
    //! True once the display was seen to refresh at a variable rate (see GetHoldMethod: the vsync wait is not used then)
    [[nodiscard]] bool IsVariableRefreshSeen() const noexcept
    {
      return m_variableRefreshSeen;
    }
    //! The id of the present of the frame being drawn. Call it during the app's draw after GetRaymarchParams, which starts the frame.
    void SetFramePresentId(const uint64_t presentId);
    //! When the present with the given id was called (a HighResolutionTimer timestamp). The frame pacer is told with the display time of
    //! the frame, as the sample can wait between the end of a frame and its present.
    void SetPresentCallTime(const uint64_t presentId, const TickCount presentCallTime);
    //! A present was measured.
    //! @param displayTime when the frame reached the display as a HighResolutionTimer timestamp (empty if it was not reported)
    //! @param queueOperationsEndTime when the present was handed to the presentation engine (empty if not reported)
    //! @param isComplete true if the presentation engine is done with the present. A complete present without a display time is
    //!        reported to the frame pacer as not shown.
    void AddPresentTiming(const uint64_t presentId, const std::optional<TickCount> displayTime, const std::optional<TickCount> queueOperationsEndTime,
                          const bool isComplete);
    //! The GPU work of a frame was measured (HighResolutionTimer timestamps).
    void AddGpuInterval(const uint64_t presentId, const TickCount gpuStartTime, const TickCount gpuEndTime);

    //! What the app draws the raymarched background of the current frame with, before it calls Draw (the GPU load of the sample).
    //! Call it during the app's draw, as the frame can start there.
    [[nodiscard]] RaymarchParams GetRaymarchParams();

    [[nodiscard]] bool IsPacerEnabled() const noexcept
    {
      return m_pacer != nullptr;
    }

    //! The number of display refreshes the current frame should be held for (1 if the frame pacer is off)
    [[nodiscard]] uint32_t GetSwapInterval() const noexcept
    {
      return m_pacer ? m_schedule.SwapInterval : 1u;
    }

  private:
    void ToggleRun();
    void StartTimedRun();
    void UpdateUI();
    //! Apply the pacer settings of the UI
    void UpdatePacer();
    //! Start the frame: the frame pacer plans it, its animation time is set and the simulated CPU load runs
    void StartFrame();
    //! Sleep (a long wait) or yield (a short wait) until the given HighResolutionTimer timestamp
    void WaitUntil(const TickCount time) const;
    [[nodiscard]] SamplePacerHold GetRequestedHold() const;
    void WaitForPresentOnVSync(const uint32_t presentSwapInterval);
    //! The wait of a frame the present holds for its whole swap interval, see SamplePacerProfile
    void WaitForPacerTime(const uint32_t presentSwapInterval, const SamplePacerHold holdMethod);
    //! The vertical blank of the window system nearest to the given time (only with a vsync time and a refresh period)
    [[nodiscard]] TickCount ToNearestVBlank(const TickCount time) const noexcept;
    //! @return the refresh rate of the display the window is on in Hz (0 if the window system does not know it)
    [[nodiscard]] double ReadDisplayRefreshRateHz() const;
    //! @return the refresh rate the frame pacer uses in Hz: the command line, else the window system, else the slider
    [[nodiscard]] double GetRefreshRateHz() const;
    void UpdateRefreshRateUI();
    void UpdatePacerStatus();
    //! Update the frame pacing section of the stats panel
    void UpdatePacerStats();
    //! Update the rows of the frame pacing section that show what the measured presents say
    void UpdatePresentFeedbackStats();
    void UpdateVariableRefreshStats();
    //! Add the columns of the sample to the frame pacing log
    void RegisterLogColumns();
    //! Write what the frame that just started is to the frame pacing log
    void LogFrameStart(const TickCount waitStartTime);
    //! Keep the CPU busy for the given time (the simulated CPU load)
    void BurnCpu(const TimeSpan duration) const;
    //! Create the two overlays: every value of the last marker and the frame pacing stats (fills in the overlay members of m_ui)
    std::shared_ptr<UI::BaseWindow> CreateStatsWindow(UI::Theme::IThemeControlFactory& rUIFactory);
    //! Create the bar at the bottom with the chart of the work per frame
    std::shared_ptr<UI::BaseWindow> CreateWorkChartBar(UI::Theme::IThemeControlFactory& rUIFactory);
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
