#ifndef FSLNATIVEWINDOW_BASE_NATIVEWINDOWTIMINGSUPPORT_HPP
#define FSLNATIVEWINDOW_BASE_NATIVEWINDOWTIMINGSUPPORT_HPP
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

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace Fsl
{
  //! What the window system has that tells when a frame is shown, and what of it the framework uses. It is for logs: a log that has
  //! it says by itself what the platform offered for frame pacing (Doc/FramePacingPlatformSupport.md).
  struct NativeWindowTimingSupport
  {
    //! The window system: "Win32", "Wayland", "X11", ... (empty if the platform does not say)
    std::string WindowSystem;
    //! Where INativeWindow::TryGetVSyncInfo gets its times from (empty: it has no source and reports them as unknown)
    std::string VSyncSource;
    //! What the window system has that has to do with when a frame is shown, used or not. On Wayland the globals of the compositor
    //! ("wp_presentation"), elsewhere the calls or the extensions of the platform.
    std::vector<std::string> Available;
    //! What the framework looked for and the window system does not have
    std::vector<std::string> NotAvailable;
    //! The entries of Available the framework uses
    std::vector<std::string> Used;
    //! The version the window system offers of a entry of Available, for the entries that have one (Wayland: the version of the global)
    std::vector<std::pair<std::string, uint32_t>> Versions;
  };
}

#endif
