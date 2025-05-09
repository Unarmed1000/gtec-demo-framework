#if !defined(__ANDROID__) && defined(__linux__) && !defined(FSL_WINDOWSYSTEM_X11) && defined(FSL_WINDOWSYSTEM_WAYLAND)
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

#include "EGLNativeWindowSystemAdapterWayland.hpp"
#include <FslBase/Exceptions.hpp>
#include <FslBase/Log/Log3Fmt.hpp>
#include <FslGraphics/Exceptions.hpp>
#include <FslNativeWindow/EGL/Adapter/EGLNativeWindowAdapterTemplate.hpp>
#include <FslNativeWindow/Platform/Adapter/Wayland/PlatformNativeWindowAdapterWayland.hpp>
#include <FslNativeWindow/Platform/Adapter/Wayland/PlatformNativeWindowWaylandCombiner.hpp>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <cstring>

namespace Fsl
{
  namespace
  {
    std::shared_ptr<IPlatformNativeWindowAdapter>
      AllocateWindow(const NativeWindowSetup& nativeWindowSetup, const PlatformNativeWindowParams& windowParams,
                     const PlatformNativeWindowAllocationParams* const pPlatformCustomWindowAllocationParams)
    {
      PlatformCallbackNativeWindowWaylandCreate createWaylandWindow = [](void* hSurface, int hWidth, int hHeight) mutable
      {
        FSLLOG3_VERBOSE5("EGLNativeWindowSystemAdapterWayland: wl_egl_window_create width:{} height:{}", hWidth, hHeight);
        return (void*)wl_egl_window_create(static_cast<wl_surface*>(hSurface), hWidth, hHeight);
      };

      PlatformCallbackNativeWindowWaylandDestroy destroyWaylandWindow = [](void* nativeWindow)
      {
        FSLLOG3_VERBOSE5("EGLNativeWindowSystemAdapterWayland: wl_egl_window_destroy");
        wl_egl_window_destroy(static_cast<wl_egl_window*>(nativeWindow));
      };

      PlatformCallbackNativeWindowWaylandResize resizeWaylandWindow = [](void* nativeWindow, int width, int height, int dx, int dy)
      {
        FSLLOG3_VERBOSE5("EGLNativeWindowSystemAdapterWayland: wl_egl_window_resize width:{} height:{} dx:{} dy:{}", width, height, dx, dy);
        wl_egl_window_resize(static_cast<wl_egl_window*>(nativeWindow), width, height, dx, dy);
      };

      // Patch the params and forward
      PlatformNativeWindowParams customWindowParams(windowParams);
      customWindowParams.CreateWaylandWindow = createWaylandWindow;
      customWindowParams.DestroyWaylandWindow =
        PlatformNativeWindowWaylandDestroyCallbackCombiner::Combine(destroyWaylandWindow, windowParams.DestroyWaylandWindow);
      customWindowParams.ResizeWaylandWindow =
        PlatformNativeWindowWaylandResizeCallbackCombiner::Combine(resizeWaylandWindow, windowParams.ResizeWaylandWindow);

      return std::make_shared<EGLNativeWindowAdapterTemplate<PlatformNativeWindowAdapterWayland>>(nativeWindowSetup, customWindowParams,
                                                                                                  pPlatformCustomWindowAllocationParams);
    }


    // typedef EGLDisplay(*PFNEGLGETPLATFORMDISPLAYEXTPROC) (EGLenum platform, void* pNativeDisplay, const EGLint* pAttribList);


    // PFNEGLGETPLATFORMDISPLAYEXTPROC TryGetEglProcAddressPlatformWayland(const char* pAddress)
    //{
    //  const char* extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    //  if (extensions == nullptr || ! (strstr(extensions, "EGL_EXT_platform_wayland") || strstr(extensions, "EGL_KHR_platform_wayland")))
    //    return nullptr;
    //  return reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress(pAddress));
    //}


    // EGLDisplay WaylandGetPlatformDisplayEXT(EGLenum platform, void* pNativeDisplay, const EGLint* pAttribList)
    //{
    //  PFNEGLGETPLATFORMDISPLAYEXTPROC fnGetPlatformDisplay = TryGetEglProcAddressPlatformWayland("eglGetPlatformDisplayEXT");
    //  if (fnGetPlatformDisplay)
    //    return fnGetPlatformDisplay(platform, pNativeDisplay, pAttribList);

    //  return eglGetDisplay(static_cast<EGLNativeDisplayType>(pNativeDisplay));
    //}
  }


  EGLNativeWindowSystemAdapterWayland::EGLNativeWindowSystemAdapterWayland(const NativeWindowSystemSetup& setup)
    : EGLNativeWindowSystemAdapterTemplate<PlatformNativeWindowSystemAdapterWayland>(setup, AllocateWindow, EGLHandleForceConvert::Enabled)
  {
  }
}
#endif
