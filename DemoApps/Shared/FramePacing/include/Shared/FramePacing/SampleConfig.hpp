#ifndef SHARED_FRAMEPACING_SAMPLECONFIG_HPP
#define SHARED_FRAMEPACING_SAMPLECONFIG_HPP
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

#include <FslBase/Math/ConstrainedValue.hpp>
#include <cstdint>

//! The default value and the range of the settings of the FramePacing samples, shared by the command line options and the UI
namespace Fsl::SampleConfig
{
  //! The duration of a timed run in seconds
  constexpr ConstrainedValue<int32_t> TimedRunSeconds(10, 1, 120);
  //! The refresh rate the slider offers when the window system does not know the refresh rate of the display
  constexpr ConstrainedValue<int32_t> RefreshRateHz(60, 24, 240);
  //! The refresh rates the command line accepts (decimals allowed)
  constexpr double MinRefreshRateHz = 1.0;
  constexpr double MaxRefreshRateHz = 1000.0;
  //! The frame rate the app wants to run at (0 = the refresh rate of the display)
  constexpr ConstrainedValue<int32_t> TargetFps(0, 0, 240);
  //! The simulated CPU load: the time the app spends busy every frame in milliseconds
  constexpr ConstrainedValue<int32_t> CpuLoadMs(0, 0, 50);
  //! A frame that runs long now and then: the time one frame in every CpuSpikeIntervalFrames is busy on top of the CPU load, in
  //! milliseconds (0 = no such frame)
  constexpr ConstrainedValue<int32_t> CpuSpikeMs(0, 0, 200);
  //! The number of frames from one frame that runs long to the next
  constexpr ConstrainedValue<int32_t> CpuSpikeIntervalFrames(120, 2, 100000);
  //! The refreshes of the pause the pacer makes once, half a second after it started, so the presents that are queued between the
  //! app and the display are shown before the next one is added (0 = no such pause).
  //! Measured on Windows at 240 Hz with a trace of the presents: the first presents of a window go through the compositor and take
  //! three refreshes to reach the display, the ones the app makes meanwhile queue up in the driver, and when the window is flipped
  //! directly a moment later they are still queued: a loop that makes one frame per refresh does not work them off. Without the wait
  //! a present reached the display after 9.7 to 11.1 ms for the whole run, with a wait of four refreshes after 2.8 ms from the wait
  //! on. The same wait on the second frame, before the queue has formed, took one refresh off or nothing.
  constexpr ConstrainedValue<int32_t> DrainRefreshes(4, 0, 32);
  //! The presents the pacer of the library lets wait to be shown while a frame is made, the frame itself counted
  //! (SamplePacerConfig::WaitingPresents). Zero is "not said": the pacer picks the number, which is what a app leaves to it. A
  //! number is for measuring.
  constexpr ConstrainedValue<int32_t> WaitingPresents(0, 0, 8);
  //! Where in a refresh the pacer of the vertical blank times has a frame ready, in percent of the refresh after a vertical blank
  //! (SamplePacerConfig::ReadyPlacePercent)
  constexpr ConstrainedValue<int32_t> ReadyPlacePercent(50, 0, 100);

  //! The frames between two changes of the pacer kind during a run, for measuring what a change does (zero: the kind is not changed)
  constexpr ConstrainedValue<int32_t> KindChangeFrames(0, 0, 100000);
  //! The GPU load: the number of steps the raymarched background takes for every pixel (0 = no background).
  //! The default is a low load, so the sample starts with a background and a GPU that has something to do.
  constexpr ConstrainedValue<int32_t> GpuLoadSteps(16, 0, 1024);
}

#endif
