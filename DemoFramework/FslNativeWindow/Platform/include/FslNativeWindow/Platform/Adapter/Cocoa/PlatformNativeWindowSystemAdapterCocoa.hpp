#ifndef FSLNATIVEWINDOW_PLATFORM_ADAPTER_COCOA_PLATFORMNATIVEWINDOWSYSTEMADAPTERCOCOA_HPP
#define FSLNATIVEWINDOW_PLATFORM_ADAPTER_COCOA_PLATFORMNATIVEWINDOWSYSTEMADAPTERCOCOA_HPP
#if defined(FSL_WINDOWSYSTEM_COCOA)
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

#include <FslNativeWindow/Platform/Adapter/PlatformNativeWindowSystemAdapter.hpp>
#include <FslNativeWindow/Platform/PlatformNativeWindowAllocationFunction.hpp>
#include <memory>

namespace Fsl
{
  class PlatformNativeWindowAdapterCocoa;

  //! Native macOS window system (AppKit).
  //! The NSApplication event loop is pumped manually from ProcessMessages, so the normal demo main loop can be used unchanged.
  //! @note Must be created and used from the main thread.
  class PlatformNativeWindowSystemAdapterCocoa : public PlatformNativeWindowSystemAdapter
  {
    std::weak_ptr<PlatformNativeWindowAdapterCocoa> m_window;
    PlatformNativeWindowAllocationFunction m_allocationFunction;

  public:
    explicit PlatformNativeWindowSystemAdapterCocoa(const NativeWindowSystemSetup& setup,
                                                    const PlatformNativeWindowAllocationFunction& allocateWindowFunction = nullptr,
                                                    const PlatformNativeWindowSystemParams& systemParams = PlatformNativeWindowSystemParams());
    ~PlatformNativeWindowSystemAdapterCocoa() override;

    std::shared_ptr<IPlatformNativeWindowAdapter>
      CreateNativeWindow(const NativeWindowSetup& nativeWindowSetup,
                         const PlatformNativeWindowAllocationParams* const pPlatformCustomWindowAllocationParams = nullptr) override;
    bool ProcessMessages(const NativeWindowProcessMessagesArgs& args) override;
  };
}    // namespace Fsl

#endif
#endif
