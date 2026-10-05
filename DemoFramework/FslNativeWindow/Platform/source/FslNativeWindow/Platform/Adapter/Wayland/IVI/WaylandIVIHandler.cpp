#if !defined(__ANDROID__) && defined(__linux__) && !defined(FSL_WINDOWSYSTEM_X11) && defined(FSL_WINDOWSYSTEM_WAYLAND) && \
  defined(FSL_WINDOWSYSTEM_WAYLAND_XDG)
/****************************************************************************************************************************************************
 * Copyright 2023 NXP
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
 *    * Neither the name of the NXP. nor the names of
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

#include "WaylandIVIHandler.hpp"
#include "../PlatformNativeWindowSystemContextWayland.hpp"
#ifdef FSL_WINDOWSYSTEM_WAYLAND_IVI
#include <FslBase/Log/Log3Fmt.hpp>
#include <FslNativeWindow/Base/NativeWindowEventHelper.hpp>
#include <ilm/ivi-application-client-protocol.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>
#include <memory>
#endif


namespace Fsl
{
  namespace
  {
#ifdef FSL_WINDOWSYSTEM_WAYLAND_IVI
    namespace LocalConfig
    {
      //! The id of the surface of the window is this plus the id of the process
      constexpr uint32_t IviSurfaceIdBase = 9000;
    }

    //! The compositor gives the surface a size: the window takes it and the framework is told, as for a xdg toplevel
    void OnIviSurfaceConfigure(void* data, ivi_surface* /*pIviSurface*/, int32_t width, int32_t height)
    {
      auto* pWindow = static_cast<PlatformNativeWindowContextWayland*>(data);
      if (pWindow == nullptr || width <= 0 || height <= 0)
      {
        return;
      }
      const PxSize2D newSizePx = PxSize2D::Create(width, height);
      if (newSizePx == pWindow->Geometry)
      {
        return;
      }
      pWindow->Geometry = newSizePx;
      if (pWindow->Native != nullptr && pWindow->ResizeWindowCallback)
      {
        pWindow->ResizeWindowCallback(pWindow->Native, width, height, 0, 0);
      }
      const std::shared_ptr<INativeWindowEventQueue> eventQueue = pWindow->EventQueue.lock();
      if (eventQueue)
      {
        eventQueue->PostEvent(NativeWindowEventHelper::EncodeWindowResizedEvent());
      }
    }

    const ivi_surface_listener g_iviSurfaceListener = {OnIviSurfaceConfigure};
#endif
  }


  void WaylandIVIHandler::Create(const PlatformNativeWindowSystemContextWayland& context, PlatformNativeWindowContextWayland& rWindow)
  {
#ifdef FSL_WINDOWSYSTEM_WAYLAND_IVI
    // Nothing waits for a configure of a IVI surface, so the window has the size it was asked to have until the compositor gives it one
    if (rWindow.Geometry.RawWidth() <= 0 || rWindow.Geometry.RawHeight() <= 0)
    {
      rWindow.Geometry = rWindow.DesiredWindowSize;
    }
    if (context.Handles.Ivi.Application)
    {
      const uint32_t surfaceId = LocalConfig::IviSurfaceIdBase + static_cast<uint32_t>(getpid());
      rWindow.Handles.Ivi.Surface.reset(
        ivi_application_surface_create(context.Handles.Ivi.Application.get(), surfaceId, rWindow.Handles.Surface.get()));
      if (rWindow.Handles.Ivi.Surface)
      {
        ivi_surface_add_listener(rWindow.Handles.Ivi.Surface.get(), &g_iviSurfaceListener, &rWindow);
      }
    }
    else
    {
      FSLLOG3_WARNING("Wayland IVI: the compositor has no ivi_application, the surface of the window gets no role");
    }
#endif
  }


  bool WaylandIVIHandler::TryRegistryHandleGlobal(PlatformNativeWindowSystemContextWayland& rContext, wl_registry* pRegistry, uint32_t name,
                                                  const char* interface, uint32_t version)
  {
#ifdef FSL_WINDOWSYSTEM_WAYLAND_IVI
    if (strcmp(interface, ivi_application_interface.name) == 0)
    {
      rContext.Handles.Ivi.Application.reset(static_cast<ivi_application*>(wl_registry_bind(pRegistry, name, &ivi_application_interface, 1)));
      return true;
    }
#endif
    return false;
  }

}

#endif
