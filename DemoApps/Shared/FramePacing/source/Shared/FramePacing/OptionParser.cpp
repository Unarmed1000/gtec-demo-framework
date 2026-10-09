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

#include <FslBase/Getopt/OptionBaseValues.hpp>
#include <FslBase/Log/Log3Fmt.hpp>
#include <FslBase/String/StringParseUtil.hpp>
#include <Shared/FramePacing/OptionParser.hpp>
#include <cmath>

namespace Fsl
{
  namespace
  {
    struct CommandId
    {
      enum Enum
      {
        HideMarkerStats = DEMO_APP_OPTION_BASE,
        HidePacingStats,
        HideWorkChart,
        HideAnimationErrorChart,
        HideTestPattern,
        BoxAnimation,
        TimedRunDuration,
        Pacer,
        PacerKind,
        PacerAim,
        PacerWaitingPresents,
        PacerReadyPlace,
        PacerKindChange,
        PacerSystemHoldsLoop,
        PacerPresentAtTime,
        PacerTimedPresent,
        PacerGpuWait,
        PacerSystemWaits,
        PacerRefreshRate,
        PacerTargetFps,
        PacerAdaptive,
        PacerDisplayReports,
        PacerStartupPause,
        CpuLoad,
        CpuSpike,
        CpuSpikeInterval,
        GpuLoad,
        Background,
        GLFlush
      };
    };

    //! Parse a integer option that has to be inside the range of the constrained value
    bool TryParseInRange(int32_t& rValue, const StringViewLite strOptArg, const ConstrainedValue<int32_t> range, const char* const pszName)
    {
      int32_t value = 0;
      StringParseUtil::Parse(value, strOptArg);
      if (value < range.Min() || value > range.Max())
      {
        FSLLOG3_ERROR("{} must be in the range [{},{}]", pszName, range.Min(), range.Max());
        return false;
      }
      rValue = value;
      return true;
    }
  }


  OptionParser::OptionParser() = default;


  OptionParser::~OptionParser() = default;


  void OptionParser::OnArgumentSetup(std::deque<Option>& rOptions)
  {
    rOptions.emplace_back("HideMarkerStats", OptionArgument::OptionNone, CommandId::HideMarkerStats,
                          "Start with the overlay with the values of the last frame pacing marker hidden (the UI has a switch for it).");
    rOptions.emplace_back("HidePacingStats", OptionArgument::OptionNone, CommandId::HidePacingStats,
                          "Start with the overlay with the frame pacing stats hidden (the UI has a switch for it).");
    rOptions.emplace_back("HideWorkChart", OptionArgument::OptionNone, CommandId::HideWorkChart,
                          "Start with the chart of the work per frame hidden (the UI has a switch for it).");
    rOptions.emplace_back("HideAnimationErrorChart", OptionArgument::OptionNone, CommandId::HideAnimationErrorChart,
                          "Start with the chart of the animation error hidden (the UI has a switch for it). The chart is only there where "
                          "the app is told when its frames were shown.");
    rOptions.emplace_back("HideTestPattern", OptionArgument::OptionNone, CommandId::HideTestPattern,
                          "Start with the test pattern (the moving bar and box) hidden (the UI has a switch for it).");
    rOptions.emplace_back("BoxAnimation", OptionArgument::OptionRequired, CommandId::BoxAnimation,
                          "Show the box animation of the mb-framepacing-explained videos: a white box that eases from side to side, "
                          "drawn for the animation time of the frame. off (the default), normal (a round trip in four seconds) "
                          "or fast (in two).");
    rOptions.emplace_back("TimedRunDuration", OptionArgument::OptionRequired, CommandId::TimedRunDuration,
                          "The duration in seconds of a timed run that is started in the UI (1 to 120, the default is 10).");
    rOptions.emplace_back("Pacer", OptionArgument::OptionNone, CommandId::Pacer,
                          "Start with the frame pacer of the sample on (the experimental mb-framepacing pacer).");
    rOptions.emplace_back("Pacer.Kind", OptionArgument::OptionRequired, CommandId::PacerKind,
                          "What the pacer of the library paces the frames with: the tier that is asked for. The pacer says what to wait "
                          "for before a frame starts and before it is presented, and the sample only does that. vblank-present-wait (the "
                          "default and the best tier the samples reach: the pacer is told when the display of the window refreshes, so "
                          "it aims a frame at a vertical blank, and before a frame it names a earlier present to wait for until it was "
                          "shown: Vulkan with --VkPresentWait n, which is then the number of presents the pacer lets wait and no longer "
                          "a wait of the host. From the wait it learns where in a refresh a frame has to be ready), vblank-period (the "
                          "vertical blank times without the wait), timer-present-wait (the wait, on a timer and without the vertical "
                          "blank times) or timer-period (a timer and the refresh period of the display, which every app has). A app "
                          "that lacks what a kind uses is paced with the best kind of what it has, so the default is the best tier "
                          "that is available.");
    rOptions.emplace_back("Pacer.Aim", OptionArgument::OptionRequired, CommandId::PacerAim,
                          "What the pacer optimizes for: smoothness (the default: at one refresh per frame the pacer keeps frames that "
                          "wait to be shown as a reserve, --Pacer.WaitingPresents less one, so a frame that runs a little long is "
                          "covered and a frame reaches the screen that many refreshes later) or low-latency (the frames that wait are "
                          "kept as few as the pacer can).");
    rOptions.emplace_back("Pacer.WaitingPresents", OptionArgument::OptionRequired, CommandId::PacerWaitingPresents,
                          "The presents the pacers timer-period and timer-present-wait let wait to be shown while a frame is made, the "
                          "frame itself counted (1 to 8). It is for measuring: not given or 0 the pacer picks the number, which is "
                          "what a app leaves to it (it picks two).");
    rOptions.emplace_back(
      "Pacer.ReadyPlace", OptionArgument::OptionRequired, CommandId::PacerReadyPlace,
      "Where in a refresh the pacers vblank-period and vblank-present-wait have a frame ready (presented, and with the GPU work reported the "
      "GPU done with it), in percent of the refresh after a vertical blank (0 to 100, the default is 50, the middle). "
      "A frame that is ready there is shown at the next vertical blank: earlier leaves more room for a frame that "
      "runs long, later shows a newer frame.");
    rOptions.emplace_back(
      "Pacer.KindChange", OptionArgument::OptionRequired, CommandId::PacerKindChange,
      "For measuring what a change of the pacer does: every that many frames the run goes on with the next of the four kinds "
      "(--Pacer.Kind says the first), in a order that has every change from one of them to another once in twelve changes. The pacer "
      "is not started again by it. 0, the default: the pacer is not changed.");
    rOptions.emplace_back("Pacer.PresentAtTime", OptionArgument::OptionRequired, CommandId::PacerPresentAtTime,
                          "true (default): the pacer uses the present that takes a time before which the frame is not shown, where the app "
                          "has one (Vulkan with a swapchain that takes a absolute target time), so the display places the frame: the "
                          "tiers of major tier 2, and of major tier 1 where the display leaves out a frame that is overdue (the present "
                          "mode FIFO latest ready). false: the frame loop places the frame, the tiers of major tier 3. A app without such "
                          "a present is in major tier 3 with both.");
    rOptions.emplace_back("Pacer.TimedPresent", OptionArgument::OptionNone, CommandId::PacerTimedPresent,
                          "The pacer uses the present that takes a time, where the app has one (Vulkan with a swapchain "
                          "that takes a time the frame before stays on screen at least): the pacer plans that time and the sample gives "
                          "it to the present, next to everything it does without it. It changes no tier: only a present at a time "
                          "lets the display place the frame (--Pacer.PresentAtTime), and a present takes one of the two.");
    rOptions.emplace_back("Pacer.GpuWait", OptionArgument::OptionNone, CommandId::PacerGpuWait,
                          "The kinds without a wait for a present (timer-period and vblank-period) hold the frame loop with a "
                          "wait for the GPU's work on an earlier frame, where the app can make that wait (Vulkan): the pacer names the "
                          "frame, the one before with the aim of low latency and the one before that with smoothness where two frames "
                          "are in flight (--VkFramesInFlight 2). It is the wait for a frame slot of the app base, made for the frame the "
                          "pacer names and reported to it.");
    rOptions.emplace_back("Pacer.SystemWaits", OptionArgument::OptionRequired, CommandId::PacerSystemWaits,
                          "true (default): the waits the app base makes by itself before a frame (the wait for a frame slot and the "
                          "acquire of a Vulkan app) are reported to the pacer. On a timer the pacer then does not take a frame "
                          "the system held for a late one, and on vertical blank times a start the side of the display held is not "
                          "stepped over by the animation time. false: they are not reported, for a run to hold a run with them against.");
    rOptions.emplace_back("Pacer.SystemHoldsLoop", OptionArgument::OptionNone, CommandId::PacerSystemHoldsLoop,
                          "Tell the pacer timer-period that the system holds the frame loop while its queue of frames is full (a acquire "
                          "or a present that waits for the display). With the aim of smoothness the pacer then lets the system pace the "
                          "loop, and a frame the system held is not late. Only for a system that does hold the loop. The Vulkan sample "
                          "reports its wait for the frame slot and its acquire to the pacer with or without it.");
    rOptions.emplace_back("Pacer.RefreshRate", OptionArgument::OptionRequired, CommandId::PacerRefreshRate,
                          "The refresh rate of the display in Hz the frame pacer uses, decimals are allowed (59.94). Defaults to the "
                          "rate the window system reports, and to the UI slider if it does not know it.");
    rOptions.emplace_back("Pacer.TargetFps", OptionArgument::OptionRequired, CommandId::PacerTargetFps,
                          "The frame rate the frame pacer aims for (0 = the refresh rate of the display, the default).");
    rOptions.emplace_back("Pacer.Adaptive", OptionArgument::OptionRequired, CommandId::PacerAdaptive,
                          "true (default): the frame pacer adapts its swap interval to how the frames do. false: a fixed frame rate.");
    rOptions.emplace_back("Pacer.DisplayReports", OptionArgument::OptionRequired, CommandId::PacerDisplayReports,
                          "true: the frame pacer is given when the display showed the frames, as display reports, where the app measures "
                          "its presents (Vulkan with VK_EXT_present_timing). It counts the animation error of the frames from them and "
                          "paces the same. Only for a display with a fixed refresh rate. false (default).");
    rOptions.emplace_back("Pacer.StartupPause", OptionArgument::OptionRequired, CommandId::PacerStartupPause,
                          "The refreshes of the pause the pacer makes once, half a second after it started, so the presents that are "
                          "queued between the app and the display are shown before the next one is added (timer-period and "
                          "vblank-period with the aim of low latency; 0 to 32, the default is 4, 0 is no such pause).");
    rOptions.emplace_back("CpuLoad", OptionArgument::OptionRequired, CommandId::CpuLoad,
                          "Simulate a CPU load: the time in milliseconds the app spends busy every frame (0 = none, the default).");
    rOptions.emplace_back("CpuSpike", OptionArgument::OptionRequired, CommandId::CpuSpike,
                          "Simulate a frame that runs long now and then: the time in milliseconds one frame in every "
                          "--CpuSpike.Interval frames is busy on top of the CPU load (0 = none, the default).");
    rOptions.emplace_back("CpuSpike.Interval", OptionArgument::OptionRequired, CommandId::CpuSpikeInterval,
                          "The number of frames from one frame that runs long to the next (2 to 100000, the default is 120).");
    rOptions.emplace_back("GpuLoad", OptionArgument::OptionRequired, CommandId::GpuLoad,
                          "A GPU load: the number of steps the background takes for every pixel; for the lace every doubling adds a "
                          "round of finer detail and the rest is samples per pixel (0 = no background, the default is a low load of 16).");
    rOptions.emplace_back("Background", OptionArgument::OptionRequired, CommandId::Background,
                          "The scene of the background: mandelbrot (a zoom into a spiral of the Mandelbrot set, Seahorse Valley, the "
                          "cheapest one and the default, the load is its iterations), blobs (a flight through blobs that melt into each "
                          "other, cheap at a low load), lace (circles packed into circles that the animation zooms into, cheap at a low "
                          "load, more load adds finer detail), "
                          "flight (a raymarched flight through a fractal lattice) or hall (a raymarched hall of columns that scrolls "
                          "sideways at a constant speed, which makes a stutter easy to see).");
    rOptions.emplace_back("GLFlush", OptionArgument::OptionNone, CommandId::GLFlush,
                          "OpenGL ES: call glFlush after the last command of a frame, so the GPU is asked to work on the frame at a known "
                          "moment and before a swap the sample delays. Without it the driver decides when, the swap at the latest (the "
                          "default). The trace says if it is on and when it was called.");
  }


  OptionParseResult OptionParser::OnParse(const int32_t cmdId, const StringViewLite& strOptArg)
  {
    switch (cmdId)
    {
    case CommandId::HideMarkerStats:
      m_hideMarkerStats = true;
      return OptionParseResult::Parsed;
    case CommandId::HidePacingStats:
      m_hidePacingStats = true;
      return OptionParseResult::Parsed;
    case CommandId::HideWorkChart:
      m_hideWorkChart = true;
      return OptionParseResult::Parsed;
    case CommandId::HideAnimationErrorChart:
      m_hideAnimationErrorChart = true;
      return OptionParseResult::Parsed;
    case CommandId::HideTestPattern:
      m_hideTestPattern = true;
      return OptionParseResult::Parsed;
    case CommandId::BoxAnimation:
      if (strOptArg == "off")
      {
        m_boxAnimation = SampleBoxAnimationSpeed::Off;
        return OptionParseResult::Parsed;
      }
      if (strOptArg == "normal")
      {
        m_boxAnimation = SampleBoxAnimationSpeed::Normal;
        return OptionParseResult::Parsed;
      }
      if (strOptArg == "fast")
      {
        m_boxAnimation = SampleBoxAnimationSpeed::Fast;
        return OptionParseResult::Parsed;
      }
      FSLLOG3_ERROR("BoxAnimation must be 'off', 'normal' or 'fast'");
      return OptionParseResult::Failed;
    case CommandId::TimedRunDuration:
      return TryParseInRange(m_timedRunSeconds, strOptArg, SampleConfig::TimedRunSeconds, "TimedRunDuration") ? OptionParseResult::Parsed
                                                                                                              : OptionParseResult::Failed;
    case CommandId::Pacer:
      m_pacerEnabled = true;
      return OptionParseResult::Parsed;
    case CommandId::PacerKind:
      if (strOptArg == "timer-period")
      {
        m_pacerKind = SamplePacerKind::TimerPeriodOnly;
        return OptionParseResult::Parsed;
      }
      if (strOptArg == "timer-present-wait")
      {
        m_pacerKind = SamplePacerKind::TimerWaitForPresent;
        return OptionParseResult::Parsed;
      }
      if (strOptArg == "vblank-period")
      {
        m_pacerKind = SamplePacerKind::VBlankPeriodOnly;
        return OptionParseResult::Parsed;
      }
      if (strOptArg == "vblank-present-wait")
      {
        m_pacerKind = SamplePacerKind::VBlankWaitForPresent;
        return OptionParseResult::Parsed;
      }
      FSLLOG3_ERROR("Pacer.Kind must be 'timer-period', 'timer-present-wait', 'vblank-period' or 'vblank-present-wait'");
      return OptionParseResult::Failed;
    case CommandId::PacerAim:
      if (strOptArg == "smoothness")
      {
        m_pacerAim = SamplePacerAim::Smoothness;
        return OptionParseResult::Parsed;
      }
      if (strOptArg == "low-latency")
      {
        m_pacerAim = SamplePacerAim::LowLatency;
        return OptionParseResult::Parsed;
      }
      FSLLOG3_ERROR("Pacer.Aim must be 'smoothness' or 'low-latency'");
      return OptionParseResult::Failed;
    case CommandId::PacerWaitingPresents:
      return TryParseInRange(m_pacerWaitingPresents, strOptArg, SampleConfig::WaitingPresents, "Pacer.WaitingPresents") ? OptionParseResult::Parsed
                                                                                                                        : OptionParseResult::Failed;
    case CommandId::PacerPresentAtTime:
      StringParseUtil::Parse(m_pacerPresentAtTime, strOptArg);
      return OptionParseResult::Parsed;
    case CommandId::PacerTimedPresent:
      m_pacerTimedPresent = true;
      return OptionParseResult::Parsed;
    case CommandId::PacerGpuWait:
      m_pacerGpuWait = true;
      return OptionParseResult::Parsed;
    case CommandId::PacerSystemWaits:
      StringParseUtil::Parse(m_pacerSystemWaits, strOptArg);
      return OptionParseResult::Parsed;
    case CommandId::PacerSystemHoldsLoop:
      m_pacerSystemHoldsLoop = true;
      return OptionParseResult::Parsed;
    case CommandId::PacerKindChange:
      return TryParseInRange(m_pacerKindChangeFrames, strOptArg, SampleConfig::KindChangeFrames, "Pacer.KindChange") ? OptionParseResult::Parsed
                                                                                                                     : OptionParseResult::Failed;
    case CommandId::PacerReadyPlace:
      return TryParseInRange(m_pacerReadyPlacePercent, strOptArg, SampleConfig::ReadyPlacePercent, "Pacer.ReadyPlace") ? OptionParseResult::Parsed
                                                                                                                       : OptionParseResult::Failed;
    case CommandId::PacerRefreshRate:
      {
        double value = 0.0;
        StringParseUtil::Parse(value, strOptArg);
        if (!std::isfinite(value) || value < SampleConfig::MinRefreshRateHz || value > SampleConfig::MaxRefreshRateHz)
        {
          FSLLOG3_ERROR("Pacer.RefreshRate must be in the range [{},{}]", SampleConfig::MinRefreshRateHz, SampleConfig::MaxRefreshRateHz);
          return OptionParseResult::Failed;
        }
        m_pacerRefreshRateHz = value;
        return OptionParseResult::Parsed;
      }
    case CommandId::PacerTargetFps:
      return TryParseInRange(m_pacerTargetFps, strOptArg, SampleConfig::TargetFps, "Pacer.TargetFps") ? OptionParseResult::Parsed
                                                                                                      : OptionParseResult::Failed;
    case CommandId::PacerAdaptive:
      StringParseUtil::Parse(m_pacerAdaptive, strOptArg);
      return OptionParseResult::Parsed;
    case CommandId::PacerDisplayReports:
      StringParseUtil::Parse(m_pacerDisplayReports, strOptArg);
      return OptionParseResult::Parsed;
    case CommandId::PacerStartupPause:
      return TryParseInRange(m_pacerStartupPauseRefreshes, strOptArg, SampleConfig::StartupPauseRefreshes, "Pacer.StartupPause")
               ? OptionParseResult::Parsed
               : OptionParseResult::Failed;
    case CommandId::CpuLoad:
      return TryParseInRange(m_cpuLoadMs, strOptArg, SampleConfig::CpuLoadMs, "CpuLoad") ? OptionParseResult::Parsed : OptionParseResult::Failed;
    case CommandId::CpuSpike:
      return TryParseInRange(m_cpuSpikeMs, strOptArg, SampleConfig::CpuSpikeMs, "CpuSpike") ? OptionParseResult::Parsed : OptionParseResult::Failed;
    case CommandId::CpuSpikeInterval:
      return TryParseInRange(m_cpuSpikeIntervalFrames, strOptArg, SampleConfig::CpuSpikeIntervalFrames, "CpuSpike.Interval")
               ? OptionParseResult::Parsed
               : OptionParseResult::Failed;
    case CommandId::GpuLoad:
      return TryParseInRange(m_gpuLoadSteps, strOptArg, SampleConfig::GpuLoadSteps, "GpuLoad") ? OptionParseResult::Parsed
                                                                                               : OptionParseResult::Failed;
    case CommandId::GLFlush:
      m_glFlush = true;
      return OptionParseResult::Parsed;
    case CommandId::Background:
      if (strOptArg == "flight")
      {
        m_background = RaymarchScene::Flight;
        return OptionParseResult::Parsed;
      }
      if (strOptArg == "hall")
      {
        m_background = RaymarchScene::Hall;
        return OptionParseResult::Parsed;
      }
      if (strOptArg == "blobs")
      {
        m_background = RaymarchScene::Blobs;
        return OptionParseResult::Parsed;
      }
      if (strOptArg == "lace")
      {
        m_background = RaymarchScene::Lace;
        return OptionParseResult::Parsed;
      }
      if (strOptArg == "mandelbrot")
      {
        m_background = RaymarchScene::Mandelbrot;
        return OptionParseResult::Parsed;
      }
      FSLLOG3_ERROR("Background must be 'mandelbrot', 'blobs', 'lace', 'flight' or 'hall'");
      return OptionParseResult::Failed;
    default:
      return OptionParseResult::NotHandled;
    }
  }


  bool OptionParser::OnParsingComplete()
  {
    return true;
  }
}
