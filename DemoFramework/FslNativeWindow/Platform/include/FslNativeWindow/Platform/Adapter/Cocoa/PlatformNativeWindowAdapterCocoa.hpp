#ifndef FSLNATIVEWINDOW_PLATFORM_ADAPTER_COCOA_PLATFORMNATIVEWINDOWADAPTERCOCOA_HPP
#define FSLNATIVEWINDOW_PLATFORM_ADAPTER_COCOA_PLATFORMNATIVEWINDOWADAPTERCOCOA_HPP
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

#include <FslBase/Math/Pixel/PxPoint2.hpp>
#include <FslBase/Math/Vector2.hpp>
#include <FslNativeWindow/Base/NativeWindowDisplayInfo.hpp>
#include <FslNativeWindow/Base/VirtualKey.hpp>
#include <FslNativeWindow/Base/VirtualMouseButton.hpp>
#include <FslNativeWindow/Platform/Adapter/PlatformNativeWindowAdapter.hpp>
#include <cstdint>
#include <memory>

namespace Fsl
{
  class NativeWindowSetup;

  //! A NSWindow whose content view is backed by a CAMetalLayer.
  //! GetPlatformWindow() returns the CAMetalLayer* (as a void*).
  class PlatformNativeWindowAdapterCocoa : public PlatformNativeWindowAdapter
  {
    // The Objective-C objects are only visible to the Objective-C++ implementation
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    PxPoint2 m_cachedWindowSize;
    Vector2 m_cachedDpi;
    uint32_t m_cachedDensityDpi{160};
    NativeWindowDisplayInfo m_cachedDisplayInfo;
    bool m_closeRequested{false};

  public:
    PlatformNativeWindowAdapterCocoa(const NativeWindowSetup& nativeWindowSetup, const PlatformNativeWindowParams& platformWindowParams,
                                     const PlatformNativeWindowAllocationParams* const pPlatformCustomWindowAllocationParams);
    ~PlatformNativeWindowAdapterCocoa() override;

    bool IsCloseRequested() const noexcept
    {
      return m_closeRequested;
    }

    // Callbacks from the Objective-C view and window delegate
    void OnCloseRequested();
    void OnResized();
    void OnScreenConfigChanged();
    void OnKey(const VirtualKey::Enum key, const bool isPressed);
    void OnMouseButton(const uint32_t timestampMs, const VirtualMouseButton button, const bool isPressed, const PxPoint2 positionPx);
    void OnMouseMove(const uint32_t timestampMs, const PxPoint2 positionPx);
    void OnMouseWheel(const uint32_t timestampMs, const int32_t delta, const PxPoint2 positionPx);

  protected:
    bool TryGetNativeSize(PxPoint2& rSize) const override;
    bool TryGetNativeDpi(Vector2& rDPI) const override;
    bool TryGetNativeDensityDpi(uint32_t& rDensityDpi) const override;
    NativeWindowDisplayInfo TryGetNativeDisplayInfo() const override;

  private:
    //! @param postEvents if true a WindowConfigChanged event is posted when the cached values changed.
    void UpdateScreenInfo(const bool postEvents);
    void UpdateWindowSize(const bool postEvents);
  };
}    // namespace Fsl

#endif
#endif
