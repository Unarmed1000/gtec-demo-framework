#ifdef _WIN32
#ifndef FSLNATIVEWINDOW_PLATFORM_ADAPTER_WIN32_WIN32VSYNCSOURCES_HPP
#define FSLNATIVEWINDOW_PLATFORM_ADAPTER_WIN32_WIN32VSYNCSOURCES_HPP
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

#include <FslNativeWindow/Base/NativeWindowTimingSupport.hpp>
#include <FslNativeWindow/Base/NativeWindowVSyncInfo.hpp>
#include <FslNativeWindow/Platform/PlatformNativeWindowSystemTypes.hpp>
#include <memory>
#include <string>

namespace Fsl
{
  //! The ways Windows can tell when the display of a window refreshes, and the one that is used.
  //!
  //! Compared on the primary 240 Hz monitor of a two monitor system with the vertical blank time of the desktop compositor, which was
  //! measured there to be within a few microseconds of when a frame is shown:
  //!
  //! | Name     | What it is                                                                 | Compared with the compositor       |
  //! |----------|----------------------------------------------------------------------------|------------------------------------|
  //! | dxgi     | IDXGIOutput::WaitForVBlank on a thread, for the output of the monitor the | 0.06 ms late, up to 0.19 ms        |
  //! |          | window is on: the time the wait returns. The default: it is the documented|                                    |
  //! |          | way to follow the vertical blank of one display.                           |                                    |
  //! | dwm      | DwmGetCompositionTimingInfo: the last vertical blank and the refresh period| it is the reference                |
  //! |          | the compositor measured. No thread and exact, but one clock for the       |                                    |
  //! |          | desktop, not one per monitor.                                              |                                    |
  //!
  //! Tried and not kept, as DXGI is enough: D3DKMTWaitForVerticalBlankEvent (it is what the DXGI wait calls, and it measured the
  //! same) and the scan line of the monitor (D3DKMTGetScanLine with the lines of the mode: 0.04 ms early, within 0.08 ms, no
  //! thread, but a call of the kernel thunk layer that is not meant for applications). The compositor time is kept until holding a
  //! frame with the DXGI wait has been measured against display times as it has.
  //! Not built: the compositor clock of Windows 11 (DCompositionWaitForCompositorClock and its frame statistics), which Microsoft
  //! documents as the replacement of the DXGI wait for apps that follow the compositor and not one display.
  //!
  //! With more than one monitor the time has to be the one of the monitor the window is on, which is looked up from the window every
  //! time: dxgi is per monitor and follows the window. The compositor has one clock for the desktop, and which monitor
  //! it follows can not be relied on (the primary one, and on current Windows 11 the one with the highest refresh rate; seen: 240 Hz
  //! reported for a window on a 120 Hz monitor). So "auto" takes dxgi, and dwm is for a system with one monitor or when asked for. All times are
  //! QueryPerformanceCounter times, the clock of the HighResolutionTimer.
  class Win32VSyncSources
  {
    struct State;
    std::unique_ptr<State> m_state;

  public:
    Win32VSyncSources(const Win32VSyncSources&) = delete;
    Win32VSyncSources& operator=(const Win32VSyncSources&) = delete;

    //! @param requestedSource the name of the source to use, empty or "auto" for the best that works
    explicit Win32VSyncSources(const std::string& requestedSource);
    ~Win32VSyncSources();

    //! @brief The time of a recent vertical blank of the monitor the window is on and its refresh period (invalid if not known)
    //! @note  Called once per frame. It follows the window when it moves to another monitor.
    [[nodiscard]] NativeWindowVSyncInfo TryGetVSyncInfo(const HWND hWnd);

    //! @brief Add the sources, which of them work for the monitor of the window and which one is used
    void FillTimingSupport(const HWND hWnd, NativeWindowTimingSupport& rSupport);

    //! @brief The display settings changed (a mode, a refresh rate, a monitor): what is known about the monitor is read again
    void OnDisplayChanged() noexcept;
  };
}

#endif
#endif
