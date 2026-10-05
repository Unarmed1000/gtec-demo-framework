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
        HideTestPattern,
        BoxAnimation,
        TimedRunDuration,
        Pacer,
        PacerRefreshRate,
        PacerTargetFps,
        PacerAdaptive,
        PacerPresentFeedback,
        PacerHold,
        PacerProfile,
        PacerVSyncPhase,
        PacerDrain,
        CpuLoad,
        GpuLoad,
        BackgroundScale,
        Background
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
    rOptions.emplace_back("Pacer.RefreshRate", OptionArgument::OptionRequired, CommandId::PacerRefreshRate,
                          "The refresh rate of the display in Hz the frame pacer uses, decimals are allowed (59.94). Defaults to the "
                          "rate the window system reports, and to the UI slider if it does not know it.");
    rOptions.emplace_back("Pacer.TargetFps", OptionArgument::OptionRequired, CommandId::PacerTargetFps,
                          "The frame rate the frame pacer aims for (0 = the refresh rate of the display, the default).");
    rOptions.emplace_back("Pacer.Adaptive", OptionArgument::OptionRequired, CommandId::PacerAdaptive,
                          "true (default): the frame pacer adapts its swap interval to how the frames do. false: a fixed frame rate.");
    rOptions.emplace_back("Pacer.PresentFeedback", OptionArgument::OptionRequired, CommandId::PacerPresentFeedback,
                          "true: the frame pacer is given when the display showed the frames, where the app measures its presents (Vulkan "
                          "with VK_EXT_present_timing). It paces the same with it and counts what the display did. Only for a display with "
                          "a fixed refresh rate. false (default).");
    rOptions.emplace_back("Pacer.Hold", OptionArgument::OptionRequired, CommandId::PacerHold,
                          "How a frame is held for more than one refresh where the present has no swap interval (Vulkan): wait (the "
                          "default, sleep on a timer and present: a guess), vsync (wait on the vsync of the window system and present "
                          "in the middle of the time it leaves, needs the window system to say when the display refreshes), schedule (a "
                          "target time on the present, needs VK_EXT_present_timing) or auto (schedule, else vsync, else wait). A method the "
                          "system can not do falls back to vsync, else wait.");
    rOptions.emplace_back("Pacer.Profile", OptionArgument::OptionRequired, CommandId::PacerProfile,
                          "Where a frame waits for the time the pacer gives for the next frame, where the app has to do the waiting "
                          "(Vulkan): early (the default, the frame is rendered right away and its present waits), late (the start of the "
                          "frame waits and the frame is presented when it is done) or off (no wait, to capture the loop without it). With "
                          "--Pacer.Hold vsync the wait is for the vsync of the window system, else for a time on the clock of the app.");
    rOptions.emplace_back("Pacer.VSyncPhase", OptionArgument::OptionRequired, CommandId::PacerVSyncPhase,
                          "For the vsync hold: where in the refresh before the one a frame is aimed at the present is done, in percent "
                          "of the refresh (1 to 99, the default is 65: the middle of what was measured to be shown at the target).");
    rOptions.emplace_back("Pacer.Drain", OptionArgument::OptionRequired, CommandId::PacerDrain,
                          "The refreshes the app waits once, half a second after it began to wait for the time of the pacer, so the "
                          "presents that are queued between the app and the display are shown before the next one is added (Vulkan, 0 to "
                          "32, the default is 4, 0 is no such wait).");
    rOptions.emplace_back("CpuLoad", OptionArgument::OptionRequired, CommandId::CpuLoad,
                          "Simulate a CPU load: the time in milliseconds the app spends busy every frame (0 = none, the default).");
    rOptions.emplace_back("GpuLoad", OptionArgument::OptionRequired, CommandId::GpuLoad,
                          "A GPU load: the number of steps the background takes for every pixel, for the lace the number of samples "
                          "(0 = no background, the default is a low load of 16).");
    rOptions.emplace_back("BackgroundScale", OptionArgument::OptionRequired, CommandId::BackgroundScale,
                          "The resolution the background is drawn at, in percent of the resolution of the window (10-100, the default "
                          "is 100). Below 100 it is drawn into a smaller picture that is enlarged, for a GPU that is limited by the "
                          "number of pixels.");
    rOptions.emplace_back("Background", OptionArgument::OptionRequired, CommandId::Background,
                          "The scene of the background: blobs (a flight through blobs that melt into each other, cheap at a low load, "
                          "the default), lace (circles packed into circles, cheap at a low load, the load is the samples per pixel), "
                          "flight (a raymarched flight through a fractal lattice) or hall (a raymarched hall of columns that scrolls "
                          "sideways at a constant speed, which makes a stutter easy to see).");
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
    case CommandId::PacerPresentFeedback:
      StringParseUtil::Parse(m_pacerPresentFeedback, strOptArg);
      return OptionParseResult::Parsed;
    case CommandId::PacerHold:
      if (strOptArg == "auto")
      {
        m_pacerHold = SamplePacerHold::Auto;
        return OptionParseResult::Parsed;
      }
      if (strOptArg == "vsync")
      {
        m_pacerHold = SamplePacerHold::VSync;
        return OptionParseResult::Parsed;
      }
      if (strOptArg == "wait")
      {
        m_pacerHold = SamplePacerHold::Wait;
        return OptionParseResult::Parsed;
      }
      if (strOptArg == "schedule")
      {
        m_pacerHold = SamplePacerHold::Schedule;
        return OptionParseResult::Parsed;
      }
      FSLLOG3_ERROR("Pacer.Hold must be 'auto', 'vsync', 'wait' or 'schedule'");
      return OptionParseResult::Failed;
    case CommandId::PacerProfile:
      if (strOptArg == "late")
      {
        m_pacerProfile = SamplePacerProfile::RenderLate;
        return OptionParseResult::Parsed;
      }
      if (strOptArg == "early")
      {
        m_pacerProfile = SamplePacerProfile::RenderEarly;
        return OptionParseResult::Parsed;
      }
      if (strOptArg == "off")
      {
        m_pacerProfile = SamplePacerProfile::Off;
        return OptionParseResult::Parsed;
      }
      FSLLOG3_ERROR("Pacer.Profile must be 'late', 'early' or 'off'");
      return OptionParseResult::Failed;
    case CommandId::PacerDrain:
      return TryParseInRange(m_pacerDrainRefreshes, strOptArg, SampleConfig::DrainRefreshes, "Pacer.Drain") ? OptionParseResult::Parsed
                                                                                                            : OptionParseResult::Failed;
    case CommandId::PacerVSyncPhase:
      return TryParseInRange(m_pacerVSyncPhasePercent, strOptArg, SampleConfig::VSyncPhasePercent, "Pacer.VSyncPhase") ? OptionParseResult::Parsed
                                                                                                                       : OptionParseResult::Failed;
    case CommandId::CpuLoad:
      return TryParseInRange(m_cpuLoadMs, strOptArg, SampleConfig::CpuLoadMs, "CpuLoad") ? OptionParseResult::Parsed : OptionParseResult::Failed;
    case CommandId::GpuLoad:
      return TryParseInRange(m_gpuLoadSteps, strOptArg, SampleConfig::GpuLoadSteps, "GpuLoad") ? OptionParseResult::Parsed
                                                                                               : OptionParseResult::Failed;
    case CommandId::BackgroundScale:
      return TryParseInRange(m_backgroundScalePercent, strOptArg, SampleConfig::BackgroundScalePercent, "BackgroundScale")
               ? OptionParseResult::Parsed
               : OptionParseResult::Failed;
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
      FSLLOG3_ERROR("Background must be 'blobs', 'lace', 'flight' or 'hall'");
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
