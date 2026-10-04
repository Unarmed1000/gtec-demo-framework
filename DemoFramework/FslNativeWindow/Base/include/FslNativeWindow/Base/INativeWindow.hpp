#ifndef FSLNATIVEWINDOW_BASE_INATIVEWINDOW_HPP
#define FSLNATIVEWINDOW_BASE_INATIVEWINDOW_HPP
/****************************************************************************************************************************************************
 * Copyright (c) 2014 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *
 *    * Neither the name of the Freescale Semiconductor, Inc. nor the names of
 *      its contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************************************************************************************/

#include <FslNativeWindow/Base/NativeWindowCapabilityFlags.hpp>
#include <FslNativeWindow/Base/NativeWindowDisplayInfo.hpp>
#include <FslNativeWindow/Base/NativeWindowMetrics.hpp>
#include <FslNativeWindow/Base/NativeWindowTimingSupport.hpp>
#include <FslNativeWindow/Base/NativeWindowVSyncInfo.hpp>

namespace Fsl
{
  struct PxExtent2D;
  struct Vector2;

  class INativeWindow
  {
  public:
    INativeWindow(const INativeWindow&) = delete;
    INativeWindow& operator=(const INativeWindow&) = delete;
    virtual ~INativeWindow() = default;

    //! @brief Get information about what capabilities this native window implementation supports.
    //! @note  The returned capabilities for a specific implementation will always be the same (so it does not change between calls).
    [[nodiscard]] virtual NativeWindowCapabilityFlags GetCapabilityFlags() const = 0;

    //! @brief Get window metrics
    [[nodiscard]] virtual NativeWindowMetrics GetWindowMetrics() const = 0;

    //! @brief Get information about the display the window is presented on.
    //! @note  This is cheap to call (the backends cache the information).
    //! @return the display info, members that are unknown are left at their default value (IsDefault() is true if nothing is known).
    [[nodiscard]] virtual NativeWindowDisplayInfo TryGetDisplayInfo() const = 0;

    //! @brief Get when the display the window is on refreshes, as the window system reports it right now.
    //! @note  It is a hint of the platform, see NativeWindowVSyncInfo. It asks the window system, so call it once per frame at most.
    //! @return the vsync info, IsValid() is false if the platform does not report it (NativeWindowCapabilityFlags::GetVSyncInfo).
    [[nodiscard]] virtual NativeWindowVSyncInfo TryGetVSyncInfo() const = 0;

    //! @brief Get what the window system has that tells when a frame is shown and what of it is used, for logs.
    //! @note  It allocates, so it is for the start of a app and not for every frame.
    [[nodiscard]] virtual NativeWindowTimingSupport GetTimingSupport() const = 0;

    //! @brief Get the windows native DPI.
    //! @return true if the DPI could be retrieved, else false
    virtual bool TryGetDpi(Vector2& rDPI) const = 0;

    //! @brief Get the windows density DPI.
    //! @return true if the DPI could be retrieved, else false
    virtual bool TryGetDensityDpi(uint32_t& rDensityDpi) const = 0;

    //! @brief Get the size of the client area (the actual area where we are drawing pixels)
    //! @return true if the size could be retrieved, else false
    virtual bool TryGetExtent(PxExtent2D& rExtent) const = 0;

    //! @brief Try to enable mouse capture for the given window.
    //! @return true if the request succeeded.
    virtual bool TryCaptureMouse(const bool enableCapture) = 0;

  protected:
    INativeWindow() = default;
  };
}

#endif
