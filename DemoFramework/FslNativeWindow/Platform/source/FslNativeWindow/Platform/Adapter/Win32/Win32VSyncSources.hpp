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
#include <FslNativeWindow/Base/NativeWindowVariableRefreshInfo.hpp>
#include <FslNativeWindow/Platform/PlatformNativeWindowSystemTypes.hpp>
#include <memory>
#include <string>

namespace Fsl
{
  //! How Windows tells when the display of a window refreshes.
  //!
  //! There is one source, "dxgi": IDXGIOutput::WaitForVBlank on a thread, for the output of the monitor the window is on. The time the
  //! wait returns is the time of the vertical blank. It is the documented way to follow the vertical blank of one display. Compared
  //! on a 240 Hz monitor with the vertical blank time of the desktop compositor it was 0.06 ms late, up to 0.19 ms.
  //!
  //! With more than one monitor the time has to be the one of the monitor the window is on, which is looked up from the window every
  //! time: the source is per monitor and follows the window.
  //!
  //! Tried and not kept, as DXGI is enough:
  //! - The vertical blank time of the desktop compositor (DwmGetCompositionTimingInfo). Exact and without a thread, but one clock for
  //!   the desktop: on current Windows 11 it follows the monitor with the highest refresh rate. On a 120 Hz monitor next to a 240 Hz
  //!   one a frame held with it ran at twice the frame rate that was asked for, where the DXGI wait held it right, and on the 240 Hz
  //!   monitor both held a frame equally well.
  //! - D3DKMTWaitForVerticalBlankEvent (it is what the DXGI wait calls, and it measured the same).
  //! - The scan line of the monitor (D3DKMTGetScanLine with the lines of the mode: 0.04 ms early, within 0.08 ms, no thread, but a
  //!   call of the kernel thunk layer that is not meant for applications).
  //! Not built: the compositor clock of Windows 11 (DCompositionWaitForCompositorClock and its frame statistics), which Microsoft
  //! documents as the replacement of the DXGI wait for apps that follow the compositor and not one display.
  //!
  //! All times are QueryPerformanceCounter times, the clock of the HighResolutionTimer.
  class Win32VSyncSources
  {
    struct State;
    std::unique_ptr<State> m_state;

  public:
    Win32VSyncSources(const Win32VSyncSources&) = delete;
    Win32VSyncSources& operator=(const Win32VSyncSources&) = delete;

    //! @param requestedSource the name of the source to use: empty, "auto" or "dxgi".
    //! @throws NotSupportedException for any other name
    explicit Win32VSyncSources(const std::string& requestedSource);
    ~Win32VSyncSources();

    //! @brief The time of a recent vertical blank of the monitor the window is on and its refresh period (invalid if not known)
    //! @note  Called once per frame. It follows the window when it moves to another monitor.
    [[nodiscard]] NativeWindowVSyncInfo TryGetVSyncInfo(const HWND hWnd);

    //! @brief What is known about variable refresh on the monitor the window is on. Windows has no call for it, so it is what the
    //!        vertical blank waits show: how far apart the vertical blanks are, in refresh periods of the mode.
    //! @note  Cheap, it reads what the wait thread measured.
    [[nodiscard]] NativeWindowVariableRefreshInfo GetVariableRefreshInfo(const HWND hWnd);

    //! @brief Add the source and if it works for the monitor of the window
    void FillTimingSupport(const HWND hWnd, NativeWindowTimingSupport& rSupport);

    //! @brief The display settings changed (a mode, a refresh rate, a monitor): what is known about the monitor is read again
    void OnDisplayChanged() noexcept;
  };
}

#endif
#endif
