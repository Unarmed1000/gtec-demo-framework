#if !defined(__ANDROID__) && defined(__linux__) && !defined(FSL_WINDOWSYSTEM_X11) && defined(FSL_WINDOWSYSTEM_WAYLAND) && \
  defined(FSL_WINDOWSYSTEM_WAYLAND_XDG)
/*
 * Copyright (C) 2011 Benjamin Franzke
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission. The copyright holders make no representations
 * about the suitability of this software for any purpose. It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

// Wayland code adapted for the DemoFramework by Freescale 2014


#include <FslBase/Exceptions.hpp>
#include <FslBase/Log/Log3Fmt.hpp>
#include <FslBase/Log/Math/LogRectangle.hpp>
#include <FslBase/Log/Math/Pixel/FmtPxPoint2.hpp>
#include <FslBase/Log/Math/Pixel/FmtPxSize2D.hpp>
#include <FslBase/Math/Pixel/TypeConverter_Math.hpp>
#include <FslBase/Math/Vector2.hpp>
#include <FslBase/Time/TimeSpanUtil.hpp>
#include <FslBase/UncheckedNumericCast.hpp>
#include <FslNativeWindow/Base/NativeWindowEventHelper.hpp>
#include <FslNativeWindow/Base/NativeWindowSetup.hpp>
#include <FslNativeWindow/Base/NativeWindowSystemSetup.hpp>
#include <FslNativeWindow/Platform/Adapter/Wayland/PlatformNativeWindowAdapterWayland.hpp>
#include <FslNativeWindow/Platform/Adapter/Wayland/PlatformNativeWindowSystemAdapterWayland.hpp>
#include <df-xdg-decoration-client-protocol.h>    // XDG wayland-scanner created header
#include <df-xdg-shell-client-protocol.h>         // XDG wayland-scanner created header
#include <fmt/format.h>
#include <linux/input.h>
#include <unistd.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include "IVI/WaylandIVIHandler.hpp"
#include "PlatformNativeWindowSystemContextWayland.hpp"
#include "WaylandEventPump.hpp"

// XDG shell decided to break C/C++ library support conventions and require us to
// run their wayland-scanner tool to generate non-app dependent headers and code.
// If they had gone for the normal semantic versioned headers the build process would
// be much simpler for all end users and much less error prone.

namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
      constexpr const char* const Title = "FSL Framework";
      constexpr int MinWindowWidth = 64;
      constexpr int MinWindowHeight = 64;
      //! The DPI of a window on a output that has no physical size (the X11 and the Win32 adapter default to the same)
      constexpr int32_t MagicDefaultDpi = 96;
      //! The size of the cursor in the logical units of the compositor
      constexpr int32_t CursorSize = 32;
      //! A notch of a mouse wheel is ten units of wl_pointer.axis, which is a 24.8 fixed point value
      constexpr int32_t WheelNotchFixed = 10 * 256;
    }

    //! The time of a input event. The compositor counts milliseconds from a start of its own in 32 bits, the bits are kept.
    MillisecondTickCount32 ToMillisecondTickCount32(const uint32_t time) noexcept
    {
      return MillisecondTickCount32::FromMilliseconds(static_cast<int32_t>(time));
    }


    // void ConfigureWindowGeometry(PlatformNativeWindowContextWayland& rWindow)
    // {
    //   // bool windowSizeChanged = true;
    //   // if(windowSizeChanged)
    //   // {

    //     // if (rWindow.Handles.Surface && rWindow.Handles.XdgSurface)
    //     // {
    //       // rWindow.WindowSize = rWindow.Geometry;
    //       // FSLLOG3_VERBOSE5("ConfigureWindowGeometry: New window size:{}", rWindow.WindowSize);
    //       // xdg_surface_set_window_geometry(rWindow.Handles.XdgSurface.get(), 0, 0, rWindow.WindowSize.RawWidth(),
    //       rWindow.WindowSize.RawHeight());
    //     // }
    //   // }
    // }

    void PostWindowConfigChanged(const PlatformNativeWindowContextWayland& window)
    {
      const auto eventQueue = window.EventQueue.lock();
      if (eventQueue)
      {
        eventQueue->PostEvent(NativeWindowEventHelper::EncodeWindowConfigChanged());
      }
    }

    //! The scale the buffers of a window are to have: the highest scale of the outputs the surface is on, so it is sharp on each of
    //! them. A surface that has not entered a output yet gets the scale of the only output, or one.
    int32_t CalcOutputScale(const std::vector<std::unique_ptr<OutputInfo>>& outputs, const std::vector<wl_output*>& enteredOutputs)
    {
      int32_t scale = 0;
      for (const wl_output* const pEnteredOutput : enteredOutputs)
      {
        for (const auto& output : outputs)
        {
          if (output && output->Output.get() == pEnteredOutput)
          {
            scale = std::max(scale, output->Geometry.Scale);
          }
        }
      }
      if (scale <= 0 && enteredOutputs.empty() && outputs.size() == 1u && outputs.front())
      {
        scale = outputs.front()->Geometry.Scale;
      }
      return std::max(scale, 1);
    }

    //! Sets the size of the window in buffer pixels from what the compositor asked for and the scale of the buffer, and tells the
    //! framework when it changed.
    void UpdateGeometry(PlatformNativeWindowContextWayland& rWindow)
    {
      const int32_t scale = rWindow.BufferScale;
      // The compositor gives a size in its logical units. A side it leaves to the window is the size that was asked for, in pixels.
      int32_t width = rWindow.ConfiguredLogicalWidth > 0 ? rWindow.ConfiguredLogicalWidth * scale : rWindow.DesiredWindowSize.RawWidth();
      int32_t height = rWindow.ConfiguredLogicalHeight > 0 ? rWindow.ConfiguredLogicalHeight * scale : rWindow.DesiredWindowSize.RawHeight();

      // Apply window size constraints
      width = std::max(width, LocalConfig::MinWindowWidth);
      height = std::max(height, LocalConfig::MinWindowHeight);
      // The size of a buffer has to be a multiple of its scale
      width -= width % scale;
      height -= height % scale;

      const PxSize2D originalSizePx = rWindow.Geometry;
      rWindow.Geometry = PxSize2D::Create(width, height);

      const bool resized = originalSizePx != rWindow.Geometry;
      FSLLOG3_VERBOSE5("UpdateGeometry result: resized: {} geometry {} buffer scale {}", resized, rWindow.Geometry, scale);

      if (resized && rWindow.Native != nullptr && rWindow.ResizeWindowCallback)
      {
        rWindow.ResizeWindowCallback(rWindow.Native, width, height, 0, 0);
      }

      if (resized)
      {    // Let the framework know that we might have been resized
        const std::shared_ptr<INativeWindowEventQueue> eventQueue = rWindow.EventQueue.lock();
        if (eventQueue)
        {
          eventQueue->PostEvent(NativeWindowEventHelper::EncodeWindowResizedEvent());
        }
      }
    }

    //! Makes the buffer scale of a window that follows its output the scale of that output. The window keeps its size in the logical
    //! units of the compositor, so its size in pixels changes with the scale.
    void UpdateBufferScale(PlatformNativeWindowContextWayland& rWindow)
    {
      if (!rWindow.FollowOutputScale || rWindow.SystemContext == nullptr || !rWindow.Handles.Surface)
      {
        return;
      }
      const int32_t scale = CalcOutputScale(rWindow.SystemContext->Outputs, rWindow.EnteredOutputs);
      if (scale == rWindow.BufferScale)
      {
        return;
      }
      FSLLOG3_VERBOSE("Wayland: the buffer scale of the window is {} (it was {})", scale, rWindow.BufferScale);
      rWindow.BufferScale = scale;
      rWindow.SystemContext->PointerScale = scale;
      // It takes effect with the next buffer that is committed
      wl_surface_set_buffer_scale(rWindow.Handles.Surface.get(), scale);
      if (rWindow.Geometry.RawWidth() > 0)
      {
        // The window has a size already: it is the same size in logical units and another in pixels
        UpdateGeometry(rWindow);
      }
    }

    void OnSurfaceEnter(void* data, wl_surface* pSurface, wl_output* pOutput)
    {
      FSLLOG3_VERBOSE5("OnSurfaceEnter");
      auto* pWindow = static_cast<PlatformNativeWindowContextWayland*>(data);
      if (pWindow == nullptr || pOutput == nullptr)
      {
        return;
      }
      pWindow->EnteredOutputs.push_back(pOutput);
      UpdateBufferScale(*pWindow);
      // The display info might have changed
      PostWindowConfigChanged(*pWindow);
    }

    void OnSurfaceLeave(void* data, wl_surface* pSurface, wl_output* pOutput)
    {
      FSLLOG3_VERBOSE5("OnSurfaceLeave");
      auto* pWindow = static_cast<PlatformNativeWindowContextWayland*>(data);
      if (pWindow == nullptr)
      {
        return;
      }
      auto& rEnteredOutputs = pWindow->EnteredOutputs;
      rEnteredOutputs.erase(std::remove(rEnteredOutputs.begin(), rEnteredOutputs.end(), pOutput), rEnteredOutputs.end());
      UpdateBufferScale(*pWindow);
      // The display info might have changed
      PostWindowConfigChanged(*pWindow);
    }

    // void OnWaylandWindowContext_HandlePreferredBufferScale(void *data, wl_surface *wl_surface, int32_t factor)
    // {
    // }
    // void OnWaylandWindowContext_HandlePreferredBufferTransform(void *data, wl_surface *wl_surface, uint32_t transform)
    // {
    // }

    //! The listener is filled member by member and names only the callbacks every version of libwayland has. Newer versions add
    //! callbacks to it (preferred_buffer_scale and preferred_buffer_transform came with version 6 of wl_surface), so a initializer list
    //! is either incomplete for a new libwayland, which GCC warns about, or does not compile against a old one.
    //! The callbacks that are not named are null. That is safe: the compositor only sends a event to a object of a version that has
    //! it, and every interface is bound here at a version whose events are all named (wl_compositor at 3 at most, and wl_surface
    //! got no event between version 1 and 6).
    wl_surface_listener CreateSurfaceListener() noexcept
    {
      wl_surface_listener listener{};
      listener.enter = OnSurfaceEnter;
      listener.leave = OnSurfaceLeave;
      return listener;
    }

    const wl_surface_listener g_surfaceListener = CreateSurfaceListener();


    void OnXdgWmBasePing(void* data, xdg_wm_base* shell, uint32_t serial)
    {
      xdg_wm_base_pong(shell, serial);
    }


    void OnXdgToplevelConfigure(void* data, xdg_toplevel* toplevel, int32_t width, int32_t height, wl_array* states)
    {
      auto* pWindow = static_cast<PlatformNativeWindowContextWayland*>(data);
      if (pWindow == nullptr)
      {
        return;
      }

      pWindow->Fullscreen = false;
      pWindow->Maximized = false;

      // The states are a array of uint32_t (wl_array_for_each of the library is C only)
      if (states != nullptr && states->data != nullptr)
      {
        const std::span<const uint32_t> stateSpan(static_cast<const uint32_t*>(states->data), states->size / sizeof(uint32_t));
        for (const uint32_t state : stateSpan)
        {
          switch (state)
          {
          case XDG_TOPLEVEL_STATE_FULLSCREEN:
            pWindow->Fullscreen = true;
            break;
          case XDG_TOPLEVEL_STATE_MAXIMIZED:
            pWindow->Maximized = true;
            break;
          // case XDG_TOPLEVEL_STATE_ACTIVATED:
          // case XDG_TOPLEVEL_STATE_SUSPENDED
          default:
            break;
          }
        }
      }

      FSLLOG3_VERBOSE5("OnXdgToplevelConfigure width {} height {} Fullscreen {} Maximized {}, current active config geometry: {}", width, height,
                       pWindow->Fullscreen, pWindow->Maximized, pWindow->Geometry);

      // The size is in the logical units of the compositor, and zero where it leaves a side to the window. The window then takes the
      // size it last had while it was neither maximized nor fullscreen, so a configure that only changes a state (the window got the
      // focus) does not undo a resize by the user. Without such a size it is the size the window was created with (UpdateGeometry).
      const bool isFloating = !pWindow->Fullscreen && !pWindow->Maximized;
      if (width > 0)
      {
        pWindow->ConfiguredLogicalWidth = width;
        pWindow->FloatingLogicalWidth = isFloating ? width : pWindow->FloatingLogicalWidth;
      }
      else
      {
        pWindow->ConfiguredLogicalWidth = pWindow->FloatingLogicalWidth;
      }
      if (height > 0)
      {
        pWindow->ConfiguredLogicalHeight = height;
        pWindow->FloatingLogicalHeight = isFloating ? height : pWindow->FloatingLogicalHeight;
      }
      else
      {
        pWindow->ConfiguredLogicalHeight = pWindow->FloatingLogicalHeight;
      }
      UpdateGeometry(*pWindow);
    }

    void OnXdgSurfaceConfigure(void* data, xdg_surface* surface, uint32_t serial)
    {
      FSLLOG3_VERBOSE5("OnXdgSurfaceConfigure");
      auto* pWindow = static_cast<PlatformNativeWindowContextWayland*>(data);
      if (pWindow != nullptr)
      {
        // ConfigureWindowGeometry(*pWindow);
        pWindow->WaitForConfigure = false;
      }
      xdg_surface_ack_configure(surface, serial);
    }

    void OnXdgToplevelClose(void* data, xdg_toplevel* xdgToplevel)
    {
      FSLLOG3_VERBOSE5("OnXdgToplevelClose");
      auto* pWindow = static_cast<PlatformNativeWindowContextWayland*>(data);
      if (pWindow != nullptr && pWindow->SystemContext != nullptr)
      {
        pWindow->SystemContext->RequestClose = true;
      }
    }

    const xdg_wm_base_listener g_wmBaseListener = {
      OnXdgWmBasePing,
    };

    const xdg_surface_listener g_xdgSurfaceListener = {OnXdgSurfaceConfigure};

    //! Filled member by member with the callbacks of the version the interface is bound at (see CreateSurfaceListener)
    xdg_toplevel_listener CreateXdgToplevelListener() noexcept
    {
      xdg_toplevel_listener listener{};
      listener.configure = OnXdgToplevelConfigure;
      listener.close = OnXdgToplevelClose;
      return listener;
    }

    const xdg_toplevel_listener g_xdgToplevelListener = CreateXdgToplevelListener();

    void OnShellSurfacePing(void* /*data*/, wl_shell_surface* shellSurface, uint32_t serial)
    {
      wl_shell_surface_pong(shellSurface, serial);
    }

    void OnShellSurfaceConfigure(void* data, wl_shell_surface* /*shellSurface*/, uint32_t /*edges*/, int32_t width, int32_t height)
    {
      auto* pWindow = static_cast<PlatformNativeWindowContextWayland*>(data);
      if (pWindow == nullptr || width <= 0 || height <= 0)
      {
        return;
      }
      // The size is in the logical units of the compositor, as that of a xdg toplevel
      pWindow->ConfiguredLogicalWidth = width;
      pWindow->ConfiguredLogicalHeight = height;
      UpdateGeometry(*pWindow);
    }

    void OnShellSurfacePopupDone(void* /*data*/, wl_shell_surface* /*shellSurface*/)
    {
    }

    const wl_shell_surface_listener g_shellSurfaceListener = {OnShellSurfacePing, OnShellSurfaceConfigure, OnShellSurfacePopupDone};

    //! The window of a compositor that has no xdg_wm_base. With the shell of the core protocol (wl_shell, which xdg-shell replaced and
    //! which a old or a small compositor can still be all there is) the surface becomes a toplevel or a fullscreen surface of it.
    //! Without any shell the surface has no role: it is created and drawn to, and if it is shown is up to the compositor.
    //! Nothing waits for a configure here, the window has the size it was asked to have until the compositor gives it one.
    void CreateSurfaceRoleWithoutXdg(const PlatformNativeWindowSystemContextWayland& context, PlatformNativeWindowContextWayland& rWindow)
    {
      rWindow.WaitForConfigure = false;
      if (context.Handles.Shell)
      {
        FSLLOG3_INFO("Wayland: the compositor has no xdg_wm_base, the window uses wl_shell (no window decorations, no close request)");
        rWindow.Handles.ShellSurface.reset(wl_shell_get_shell_surface(context.Handles.Shell.get(), rWindow.Handles.Surface.get()));
        if (!rWindow.Handles.ShellSurface)
        {
          throw GraphicsException("wl_shell_get_shell_surface Failure");
        }
        if (wl_shell_surface_add_listener(rWindow.Handles.ShellSurface.get(), &g_shellSurfaceListener, &rWindow) != 0)
        {
          throw GraphicsException("wl_shell_surface_add_listener Failure");
        }
        wl_shell_surface_set_title(rWindow.Handles.ShellSurface.get(), LocalConfig::Title);
        if (rWindow.Fullscreen)
        {
          wl_shell_surface_set_fullscreen(rWindow.Handles.ShellSurface.get(), WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT, 0, nullptr);
        }
        else
        {
          wl_shell_surface_set_toplevel(rWindow.Handles.ShellSurface.get());
        }
      }
      else
      {
        FSLLOG3_WARNING(
          "Wayland: the compositor has neither xdg_wm_base nor wl_shell. The window has a surface without a role, which "
          "most compositors do not show.");
        rWindow.Fullscreen = false;
      }
      UpdateGeometry(rWindow);
      if (rWindow.Handles.ShellSurface)
      {
        // The size of a fullscreen surface comes with a configure, which has arrived when the compositor has answered
        if (wl_display_roundtrip(context.Handles.Display.get()) < 0)
        {
          throw GraphicsException(fmt::format("The connection to the compositor was lost while the window was created: {}",
                                              WaylandEventPump::DescribeError(context.Handles.Display.get())));
        }
      }
    }

    void CreateWlSurface(const PlatformNativeWindowSystemContextWayland& context, PlatformNativeWindowContextWayland& rWindow)
    {
      try
      {
        // Ensure no pre-existing handles are valid
        rWindow.Handles.Reset();
        rWindow.EnteredOutputs.clear();

        rWindow.Handles.Surface.reset(wl_compositor_create_surface(context.Handles.Compositor.get()));
        if (!rWindow.Handles.Surface)
        {
          throw GraphicsException("wl_compositor_create_surface Failure");
        }
        wl_surface_add_listener(rWindow.Handles.Surface.get(), &g_surfaceListener, &rWindow);

        // A new surface has a buffer scale of one. The scale of the output is known when there is one output, else it comes when the
        // surface enters a output.
        rWindow.BufferScale = 1;
        if (rWindow.SystemContext != nullptr)
        {
          rWindow.SystemContext->PointerScale = 1;
        }
        UpdateBufferScale(rWindow);


        if (!context.Handles.Ivi.Enabled)
        {
          if (!context.Handles.WmBase)
          {
            // No configure of a xdg surface ever arrives then, so the wait below is not made
            CreateSurfaceRoleWithoutXdg(context, rWindow);
          }
          if (context.Handles.WmBase)
          {
            rWindow.Handles.XdgSurface.reset(xdg_wm_base_get_xdg_surface(context.Handles.WmBase.get(), rWindow.Handles.Surface.get()));
            if (!rWindow.Handles.XdgSurface)
            {
              throw GraphicsException("xdg_wm_base_get_shell_surface Failure");
            }

            if (xdg_surface_add_listener(rWindow.Handles.XdgSurface.get(), &g_xdgSurfaceListener, &rWindow) != 0)
            {
              throw GraphicsException("xdg_surface_add_listener Failure");
            }

            rWindow.Handles.XdgToplevel.reset(xdg_surface_get_toplevel(rWindow.Handles.XdgSurface.get()));
            if (!rWindow.Handles.XdgToplevel)
            {
              throw GraphicsException("xdg_surface_get_toplevel Failure");
            }

            if (xdg_toplevel_add_listener(rWindow.Handles.XdgToplevel.get(), &g_xdgToplevelListener, &rWindow) != 0)
            {
              throw GraphicsException("xdg_toplevel_add_listener Failure");
            };

            xdg_toplevel_set_title(rWindow.Handles.XdgToplevel.get(), LocalConfig::Title);

            if (context.Handles.DecorationManager)
            {
              FSLLOG3_VERBOSE5("Nice 'Wayland XDG Decorations' support found, requesting server side window decorations.");
              // Let the compositor do all the complicated window management
              rWindow.Handles.XdgToplevelDecoration.reset(
                zxdg_decoration_manager_v1_get_toplevel_decoration(context.Handles.DecorationManager.get(), rWindow.Handles.XdgToplevel.get()));
              if (rWindow.Handles.XdgToplevelDecoration)
              {
                zxdg_toplevel_decoration_v1_set_mode(rWindow.Handles.XdgToplevelDecoration.get(), ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
              }
            }
            else
            {
              // Unfortunately wayland window managers are not required to support XDG Decorations.
              // This does not really respect cross platform app developers time and it makes little sense that each app has to provide a custom
              // window decoration implementation for all possible wayland backends that the app need to run on just to match the native look and feel
              // of the current wayland backend. It makes more sense solving it "once per wayland backend" instead of "once per app per wayland
              // backend" especially since there are millions of apps vs a few wayland backends.
              FSLLOG3_INFO_IF(!rWindow.Fullscreen,
                              "Your 'Wayland window manager' does not support 'XDG Decorations' so no window decorations will be shown.");
            }

            // xdg_surface_set_window_geometry(rWindow.Handles.XdgSurface.get(), 0, 0, rWindow.DesiredWindowSize.RawWidth(),
            // rWindow.DesiredWindowSize.RawHeight());

            // What the window is to be from the start is asked for before the first commit, so the first configure is for it
            if (rWindow.Fullscreen)
            {
              xdg_toplevel_set_fullscreen(rWindow.Handles.XdgToplevel.get(), nullptr);
            }
            rWindow.WaitForConfigure = true;
            wl_surface_commit(rWindow.Handles.Surface.get());
          }

          while (rWindow.WaitForConfigure)
          {
            if (wl_display_dispatch(context.Handles.Display.get()) < 0)
            {
              throw GraphicsException(fmt::format("The connection to the compositor was lost while the window waited for its first configure: {}",
                                                  WaylandEventPump::DescribeError(context.Handles.Display.get())));
            }
          }
        }
        else
        {
          WaylandIVIHandler::Create(context, rWindow);
        }
      }
      catch (const std::exception&)
      {
        rWindow.Handles.Reset();
        throw;
      }
    }


    //! Used to extract window dimensions
    // void CreateWlDummySurface(const PlatformNativeWindowSystemContextWayland& context, PlatformNativeWindowContextWayland& rWindow)
    // {
    //   CreateWlSurface(context, rWindow);

    //   while (!rWindow.Configured)
    //   {
    //     wl_display_dispatch(context.Handles.Display.get());
    //   }
    //   if (rWindow.Fullscreen && context.Handles.WmBase)
    //   {
    //     rWindow.Handles.Reset();
    //     rWindow.Fullscreen = false;
    //     wl_display_dispatch(context.Handles.Display.get());
    //   }
    // }


    //! Loads the cursor theme at the scale of the window, so the cursor is sharp on a scaled output. It is loaded once per scale, and
    //! when no theme can be loaded there is no default cursor and the compositor keeps the cursor it shows.
    void UpdateCursorTheme(PlatformNativeWindowSystemContextWayland& rContext)
    {
      const int32_t scale = std::max(rContext.PointerScale, 1);
      if (!rContext.Handles.Shm || rContext.CursorScale == scale)
      {
        return;
      }
      rContext.CursorScale = scale;
      rContext.Handles.DefaultCursor = nullptr;
      rContext.Handles.CursorTheme.reset(wl_cursor_theme_load(nullptr, LocalConfig::CursorSize * scale, rContext.Handles.Shm.get()));
      if (!rContext.Handles.CursorTheme)
      {
        FSLLOG3_VERBOSE("Wayland: no cursor theme could be loaded, the cursor is left to the compositor");
        return;
      }
      rContext.Handles.DefaultCursor = wl_cursor_theme_get_cursor(rContext.Handles.CursorTheme.get(), "left_ptr");
    }

    void OnPointerEnter(void* data, wl_pointer* pointer, uint32_t serial, wl_surface* surface, wl_fixed_t sx, wl_fixed_t sy)
    {
      auto* pContext = static_cast<PlatformNativeWindowSystemContextWayland*>(data);
      assert(pContext != nullptr);

      // Disabled the cursor hiding as this should really be requested by the app if it needs it.
      // if (pContext->Window->Fullscreen)
      // {
      // // Hide the cursor
      // // wl_pointer_set_cursor(pointer, serial, nullptr, 0, 0);
      // } else
      UpdateCursorTheme(*pContext);
      const wl_cursor* const pCursor = pContext->Handles.DefaultCursor;
      wl_surface* const pCursorSurface = pContext->Handles.CursorSurface.get();
      if (pCursor != nullptr && pCursor->image_count > 0u && pCursorSurface != nullptr)
      {
        wl_cursor_image* const pImage = pCursor->images[0];
        wl_buffer* const pBuffer = wl_cursor_image_get_buffer(pImage);
        if (pBuffer != nullptr)
        {
          const auto imageWidth = UncheckedNumericCast<int32_t>(pImage->width);
          const auto imageHeight = UncheckedNumericCast<int32_t>(pImage->height);
          // The theme gives the size it has that is nearest to the one asked for. A buffer has to be a multiple of its scale in size,
          // so a image that is not gets a scale of one and the compositor enlarges it.
          int32_t cursorScale = pContext->CompositorVersion >= 3u ? std::max(pContext->CursorScale, 1) : 1;
          if ((imageWidth % cursorScale) != 0 || (imageHeight % cursorScale) != 0)
          {
            cursorScale = 1;
          }
          // The hotspot and the damage are in the logical units of the compositor
          wl_pointer_set_cursor(pointer, serial, pCursorSurface, UncheckedNumericCast<int32_t>(pImage->hotspot_x) / cursorScale,
                                UncheckedNumericCast<int32_t>(pImage->hotspot_y) / cursorScale);
          if (pContext->CompositorVersion >= 3u)
          {
            wl_surface_set_buffer_scale(pCursorSurface, cursorScale);
          }
          wl_surface_attach(pCursorSurface, pBuffer, 0, 0);
          wl_surface_damage(pCursorSurface, 0, 0, imageWidth / cursorScale, imageHeight / cursorScale);
          wl_surface_commit(pCursorSurface);
        }
      }

      // The position is in the logical units of the compositor (a 24.8 fixed point value)
      pContext->MousePosition = PxPoint2::Create((sx * pContext->PointerScale) / 256, (sy * pContext->PointerScale) / 256);
    }


    void OnPointerLeave(void* data, wl_pointer* pointer, uint32_t serial, wl_surface* surface)
    {
    }


    void OnPointerMotion(void* data, wl_pointer* pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
    {
      auto* pContext = static_cast<PlatformNativeWindowSystemContextWayland*>(data);
      assert(pContext != nullptr);

      const std::shared_ptr<INativeWindowEventQueue> eventQueue = pContext->EventQueue.lock();

      // The position is in the logical units of the compositor (a 24.8 fixed point value)
      pContext->MousePosition = PxPoint2::Create((sx * pContext->PointerScale) / 256, (sy * pContext->PointerScale) / 256);
      if (eventQueue)
      {
        eventQueue->PostEvent(NativeWindowEventHelper::EncodeInputMouseMoveEvent(ToMillisecondTickCount32(time), pContext->MousePosition));
      }
    }


    VirtualMouseButton ToVirtualMouseButton(const uint32_t button) noexcept
    {
      switch (button)
      {
      case BTN_LEFT:
        return VirtualMouseButton::Left;
      case BTN_RIGHT:
        return VirtualMouseButton::Right;
      case BTN_MIDDLE:
        return VirtualMouseButton::Middle;
      default:
        return VirtualMouseButton::Undefined;
      }
    }

    void OnPointerButton(void* data, wl_pointer* wlPointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
    {
      auto* pContext = static_cast<PlatformNativeWindowSystemContextWayland*>(data);
      assert(pContext != nullptr);
      const std::shared_ptr<INativeWindowEventQueue> eventQueue = pContext->EventQueue.lock();

      // The event names the button for a press and for a release
      const VirtualMouseButton mouseButton = ToVirtualMouseButton(button);
      if (mouseButton == VirtualMouseButton::Undefined)
      {
        // A button the framework has no name for
        return;
      }
      pContext->MouseButton = mouseButton;
      pContext->MouseIsPressed = (state == WL_POINTER_BUTTON_STATE_PRESSED);
      if (eventQueue)
      {
        eventQueue->PostEvent(NativeWindowEventHelper::EncodeInputMouseButtonEvent(ToMillisecondTickCount32(time), pContext->MouseButton,
                                                                                   pContext->MouseIsPressed, pContext->MousePosition));
      }
    }


    void OnPointerAxis(void* data, wl_pointer* wlPointer, uint32_t time, uint32_t axis, wl_fixed_t value)
    {
      auto* pContext = static_cast<PlatformNativeWindowSystemContextWayland*>(data);
      assert(pContext != nullptr);
      if (axis != WL_POINTER_AXIS_VERTICAL_SCROLL)
      {
        // The wheel event of the framework is the vertical one
        return;
      }
      const std::shared_ptr<INativeWindowEventQueue> eventQueue = pContext->EventQueue.lock();

      // The compositor counts a scroll down as positive, the framework a turn of the wheel away from the user, and a notch as
      // MouseWheelDeltaPerNotch (what the other window systems report). A touchpad and a wheel with fine steps send parts of a
      // notch: they are passed on as they come, and what is less than one unit of the delta is kept until it adds up.
      pContext->WheelRemainder -= static_cast<int32_t>(value) * NativeWindowEventHelper::MouseWheelDeltaPerNotch;
      const int32_t wheelDelta = pContext->WheelRemainder / LocalConfig::WheelNotchFixed;
      if (wheelDelta == 0)
      {
        return;
      }
      pContext->WheelRemainder -= wheelDelta * LocalConfig::WheelNotchFixed;
      pContext->ZDelta = wheelDelta;
      if (eventQueue)
      {
        eventQueue->PostEvent(
          NativeWindowEventHelper::EncodeInputMouseWheelEvent(ToMillisecondTickCount32(time), pContext->ZDelta, pContext->MousePosition));
      }
    }

    //! Filled member by member with the callbacks of the version the interface is bound at (see CreateSurfaceListener)
    wl_pointer_listener CreatePointerListener() noexcept
    {
      wl_pointer_listener listener{};
      listener.enter = OnPointerEnter;
      listener.leave = OnPointerLeave;
      listener.motion = OnPointerMotion;
      listener.button = OnPointerButton;
      listener.axis = OnPointerAxis;
      return listener;
    }

    const wl_pointer_listener g_pointerListener = CreatePointerListener();


    void OnKeyboardKeymap(void* data, wl_keyboard* keyboard, uint32_t format, int fd, uint32_t size)
    {
      // The keys are reported by their position (WaylandUtil::TryToVirtualKey), so the keymap is not read. The file is ours to close.
      if (fd >= 0)
      {
        close(fd);
      }
    }


    void OnKeyboardEnter(void* data, wl_keyboard* keyboard, uint32_t serial, wl_surface* surface, wl_array* keys)
    {
    }


    void OnKeyboardLeave(void* data, wl_keyboard* keyboard, uint32_t serial, wl_surface* surface)
    {
    }


    void OnKeyboardKey(void* data, wl_keyboard* keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
    {
      auto* pContext = static_cast<PlatformNativeWindowSystemContextWayland*>(data);
      assert(pContext != nullptr);
      const std::shared_ptr<INativeWindowEventQueue> eventQueue = pContext->EventQueue.lock();
      if (eventQueue)
      {
        // Try to translate the keykode and post it if successfull
        const VirtualKey::Enum virtualKey = WaylandUtil::TryToVirtualKey(key);
        if (virtualKey != VirtualKey::Undefined)
        {
          // Post the event
          const bool isPressed = state != 0;
          eventQueue->PostEvent(NativeWindowEventHelper::EncodeInputKeyEvent(virtualKey, isPressed));
        }
      }
    }


    void OnKeyboardModifiers(void* data, wl_keyboard* keyboard, uint32_t serial, uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked,
                             uint32_t group)
    {
    }

    //! Filled member by member with the callbacks of the version the interface is bound at (see CreateSurfaceListener)
    wl_keyboard_listener CreateKeyboardListener() noexcept
    {
      wl_keyboard_listener listener{};
      listener.keymap = OnKeyboardKeymap;
      listener.enter = OnKeyboardEnter;
      listener.leave = OnKeyboardLeave;
      listener.key = OnKeyboardKey;
      listener.modifiers = OnKeyboardModifiers;
      return listener;
    }

    const wl_keyboard_listener g_keyboardListener = CreateKeyboardListener();

#if 0
    void PrintGlobalInfo(const GlobalInfo& global)
    {
      FSLLOG3_VERBOSE5("interface: {}, Version: {}, name: {}", global.Interface, global.Version, global.Id);
    }

    void PrintOutputInfo(const OutputInfo& output)
    {
      const char* subpixel_orientation = nullptr;
      const char* transform = nullptr;

      PrintGlobalInfo(output.Global);

      switch (output.Geometry.Subpixel)
      {
      case WL_OUTPUT_SUBPIXEL_UNKNOWN:
        subpixel_orientation = "unknown";
        break;
      case WL_OUTPUT_SUBPIXEL_NONE:
        subpixel_orientation = "none";
        break;
      case WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB:
        subpixel_orientation = "horizontal rgb";
        break;
      case WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR:
        subpixel_orientation = "horizontal bgr";
        break;
      case WL_OUTPUT_SUBPIXEL_VERTICAL_RGB:
        subpixel_orientation = "vertical rgb";
        break;
      case WL_OUTPUT_SUBPIXEL_VERTICAL_BGR:
        subpixel_orientation = "vertical bgr";
        break;
      default:
        FSLLOG3_WARNING("unknown subpixel orientation {}", static_cast<uint64_t>(output.Geometry.Subpixel));
        subpixel_orientation = "unexpected value";
        break;
      }

      switch (output.Geometry.OutputTransform)
      {
      case WL_OUTPUT_TRANSFORM_NORMAL:
        transform = "normal";
        break;
      case WL_OUTPUT_TRANSFORM_90:
        transform = "90";
        break;
      case WL_OUTPUT_TRANSFORM_180:
        transform = "180";
        break;
      case WL_OUTPUT_TRANSFORM_270:
        transform = "270";
        break;
      case WL_OUTPUT_TRANSFORM_FLIPPED:
        transform = "flipped";
        break;
      case WL_OUTPUT_TRANSFORM_FLIPPED_90:
        transform = "flipped 90";
        break;
      case WL_OUTPUT_TRANSFORM_FLIPPED_180:
        transform = "flipped 180";
        break;
      case WL_OUTPUT_TRANSFORM_FLIPPED_270:
        transform = "flipped 270";
        break;
      default:
        FSLLOG3_WARNING("unknown output transform {}", static_cast<uint64_t>(output.Geometry.OutputTransform));
        transform = "unexpected value";
        break;
      }

      FSLLOG3_INFO("x: {}, y: {}", output.Geometry.X, output.Geometry.Y);
      if (output.Version >= 2)
      {
        FSLLOG3_INFO("scale: {}", output.Geometry.Scale);
      }

      FSLLOG3_INFO("physical_width: {} mm, physical_height: {} mm", output.Geometry.PhysicalWidth, output.Geometry.PhysicalHeight);
      FSLLOG3_INFO("make: {}, model: {}", output.Geometry.Make, output.Geometry.Model);
      FSLLOG3_INFO("subpixel_orientation: {}, output_transform: {}", subpixel_orientation, transform);

      for (const auto& mode : output.Modes)
      {
        FSLLOG3_INFO("mode:");

        FSLLOG3_INFO("width: {} px, height: {} px, refresh: {} Hz", mode.Width, mode.Height, (float)mode.Refresh / 1000);
        FSLLOG3_INFO("flags:");
        if ((mode.Flags & WL_OUTPUT_MODE_CURRENT) != 0u)
          FSLLOG3_INFO(" current");
        if ((mode.Flags & WL_OUTPUT_MODE_PREFERRED) != 0u)
          FSLLOG3_INFO(" preferred");
      }
    }
#endif

    //! A output changed (its mode, its scale or its size): the window gets the scale of its output, and the framework is told to read
    //! the DPI and the display info again. Nothing happens before there is a window.
    void OnOutputChanged(const OutputInfo& output)
    {
      const PlatformNativeWindowSystemContextWayland* const pContext = output.SystemContext;
      if (pContext == nullptr || pContext->Window == nullptr)
      {
        return;
      }
      UpdateBufferScale(*pContext->Window);
      PostWindowConfigChanged(*pContext->Window);
    }

    void OnOutputGeometry(void* data, wl_output* wlOutput, int32_t x, int32_t y, int32_t physicalWidth, int32_t physicalHeight, int32_t subpixel,
                          const char* make, const char* model, int32_t outputTransform)
    {
      auto* output = static_cast<OutputInfo*>(data);
      assert(output != nullptr);

      FSLLOG3_VERBOSE5("OnOutputGeometry: x:{} y:{} physicalWidth:{} physicalHeight:{} subpixel:{} output_transform:{}", x, y, physicalWidth,
                       physicalHeight, subpixel, outputTransform);

      output->Geometry.X = x;
      output->Geometry.Y = y;
      output->Geometry.PhysicalWidth = physicalWidth;
      output->Geometry.PhysicalHeight = physicalHeight;
      output->Geometry.Subpixel = static_cast<wl_output_subpixel>(subpixel);
      output->Geometry.Make = make != nullptr ? make : "";
      output->Geometry.Model = model != nullptr ? model : "";
      output->Geometry.OutputTransform = static_cast<wl_output_transform>(outputTransform);
      if (output->Version < 2u)
      {
        // A output of version one has no done event that ends a set of changes
        OnOutputChanged(*output);
      }
    }

    void OnOutputMode(void* data, wl_output* wlOutput, uint32_t flags, int32_t width, int32_t height, int32_t refresh)
    {
      auto* output = static_cast<OutputInfo*>(data);
      assert(output != nullptr);

      FSLLOG3_VERBOSE5("OnOutputMode");

      if ((flags & WL_OUTPUT_MODE_CURRENT) != 0u)
      {
        output->CurrentRefreshMilliHz = refresh;
        // A output has one current mode, and the event is sent again when it changes
        for (OutputModeRecord& rMode : output->Modes)
        {
          rMode.Flags &= ~static_cast<uint32_t>(WL_OUTPUT_MODE_CURRENT);
        }
      }

      // A mode that was listed before is updated, so the list does not grow with every change of the mode
      const auto itrFind = std::find_if(output->Modes.begin(), output->Modes.end(), [width, height, refresh](const OutputModeRecord& entry)
                                        { return entry.Width == width && entry.Height == height && entry.Refresh == refresh; });
      if (itrFind != output->Modes.end())
      {
        itrFind->Flags = flags;
      }
      else
      {
        OutputModeRecord mode;
        mode.Flags = flags;
        mode.Width = width;
        mode.Height = height;
        mode.Refresh = refresh;
        output->Modes.push_back(mode);
      }
      if (output->Version < 2u)
      {
        OnOutputChanged(*output);
      }
    }

    void OnOutputDone(void* data, wl_output* wlOutput)
    {
      // The end of a set of changes of the output. Nothing waits for the first one: the events of a new output arrive within a
      // roundtrip.
      const auto* const pOutput = static_cast<const OutputInfo*>(data);
      if (pOutput != nullptr)
      {
        OnOutputChanged(*pOutput);
      }
    }

    void OnOutputScale(void* data, wl_output* wlOutput, int32_t scale)
    {
      FSLLOG3_VERBOSE5("OnOutputScale: {}", scale);

      auto* output = static_cast<OutputInfo*>(data);
      if (output != nullptr)
      {
        output->Geometry.Scale = scale;
      }
    }

    //! Filled member by member with the callbacks of the version the interface is bound at (see CreateSurfaceListener)
    wl_output_listener CreateOutputListener() noexcept
    {
      wl_output_listener listener{};
      listener.geometry = OnOutputGeometry;
      listener.mode = OnOutputMode;
      listener.done = OnOutputDone;
      listener.scale = OnOutputScale;
      return listener;
    }

    const wl_output_listener g_outputListener = CreateOutputListener();


    void AddOutputInfo(PlatformNativeWindowSystemContextWayland& rContext, const uint32_t id, const uint32_t version)
    {
      auto output = std::make_unique<OutputInfo>(GlobalInfo(id, version, wl_output_interface.name), version);
      FSLLOG3_VERBOSE5("Allocated Space for OutputInfo");

      output->SystemContext = &rContext;
      output->Output.reset(static_cast<wl_output*>(wl_registry_bind(rContext.Handles.Registry.get(), id, &wl_output_interface, output->Version)));
      if (!output->Output)
      {
        return;
      }
      wl_output_add_listener(output->Output.get(), &g_outputListener, output.get());

      rContext.RoundtripNeeded = true;
      rContext.Outputs.push_back(std::move(output));
    }


    void OnSeatCapabilities(void* data, wl_seat* seat, uint32_t caps)
    {
      auto* pContext = static_cast<PlatformNativeWindowSystemContextWayland*>(data);

      if (((caps & WL_SEAT_CAPABILITY_POINTER) != 0u) && !pContext->Handles.Pointer)
      {
        pContext->Handles.Pointer.reset(wl_seat_get_pointer(seat));
        wl_pointer_add_listener(pContext->Handles.Pointer.get(), &g_pointerListener, pContext);
      }
      else if (((caps & WL_SEAT_CAPABILITY_POINTER) == 0u) && pContext->Handles.Pointer)
      {
        pContext->Handles.Pointer.reset();
      }

      if (((caps & WL_SEAT_CAPABILITY_KEYBOARD) != 0u) && !pContext->Handles.Keyboard)
      {
        pContext->Handles.Keyboard.reset(wl_seat_get_keyboard(seat));
        wl_keyboard_add_listener(pContext->Handles.Keyboard.get(), &g_keyboardListener, pContext);
      }
      else if (((caps & WL_SEAT_CAPABILITY_KEYBOARD) == 0u) && pContext->Handles.Keyboard)
      {
        pContext->Handles.Keyboard.reset();
      }
    }

    //! Filled member by member with the callbacks of the version the interface is bound at (see CreateSurfaceListener)
    wl_seat_listener CreateSeatListener() noexcept
    {
      wl_seat_listener listener{};
      listener.capabilities = OnSeatCapabilities;
      return listener;
    }

    const wl_seat_listener g_seatListener = CreateSeatListener();

    //! The globals a compositor can have that are about when a frame is shown (Doc/FramePacingPlatformSupport.md). These are looked
    //! for, and the log says for each of them if the compositor has it. Only wp_presentation is bound (WaylandPresentationTime).
    namespace LocalConfig
    {
      constexpr std::array<const char*, 6> FrameTimingGlobals = {
        "wp_presentation",    "wp_fifo_manager_v1", "wp_commit_timing_manager_v1", "wp_tearing_control_manager_v1", "wp_linux_drm_syncobj_manager_v1",
        "zwp_linux_dmabuf_v1"};
    }

    bool IsFrameTimingGlobal(const char* const pszInterface) noexcept
    {
      return std::any_of(LocalConfig::FrameTimingGlobals.begin(), LocalConfig::FrameTimingGlobals.end(),
                         [pszInterface](const char* const pszEntry) { return strcmp(pszInterface, pszEntry) == 0; });
    }

    const GlobalInfo* TryFindFrameTimingGlobal(const PlatformNativeWindowSystemContextWayland& context, const char* const pszInterface) noexcept
    {
      const auto itrFind = std::find_if(context.FrameTimingGlobals.begin(), context.FrameTimingGlobals.end(),
                                        [pszInterface](const GlobalInfo& entry) { return entry.Interface == pszInterface; });
      return itrFind != context.FrameTimingGlobals.end() ? &(*itrFind) : nullptr;
    }

    //! What was looked for and what the compositor has of it, for the verbose log of any app
    void LogFrameTimingGlobals(const PlatformNativeWindowSystemContextWayland& context)
    {
      FSLLOG3_VERBOSE("Wayland: the globals of the compositor that are about when a frame is shown (looked for: {})",
                      LocalConfig::FrameTimingGlobals.size());
      for (const char* const pszInterface : LocalConfig::FrameTimingGlobals)
      {
        const GlobalInfo* const pGlobal = TryFindFrameTimingGlobal(context, pszInterface);
        if (pGlobal == nullptr)
        {
          FSLLOG3_VERBOSE("- {}: not available", pszInterface);
        }
        else
        {
          const bool isUsed = context.PresentationTime.IsBound() && pGlobal->Interface == "wp_presentation";
          FSLLOG3_VERBOSE("- {}: available (version {}), {}", pszInterface, pGlobal->Version,
                          isUsed ? "used for the vsync time of the window" : "not used by the framework");
        }
      }
    }

    void OnRegistryGlobal(void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version)
    {
      FSLLOG3_VERBOSE5("Wayland registry handle global '{}'", interface)

      auto* pContext = static_cast<PlatformNativeWindowSystemContextWayland*>(data);
      assert(pContext != nullptr);

      if (IsFrameTimingGlobal(interface))
      {
        pContext->FrameTimingGlobals.emplace_back(name, version, interface);
      }
      // The one of them that is used: it says when the frames of the window were shown
      pContext->PresentationTime.TryRegistryHandleGlobal(registry, name, interface, version);

      if (strcmp(interface, wl_compositor_interface.name) == 0)
      {
        // Version 3 has wl_surface.set_buffer_scale. No version up to it adds a event to wl_surface, so the surface listener of this
        // file stays complete (see CreateSurfaceListener).
        pContext->CompositorVersion = std::min(version, uint32_t{3});
        pContext->Handles.Compositor.reset(
          static_cast<wl_compositor*>(wl_registry_bind(registry, name, &wl_compositor_interface, pContext->CompositorVersion)));
      }
      else if (strcmp(interface, xdg_wm_base_interface.name) == 0)
      {
        pContext->Handles.WmBase.reset(static_cast<xdg_wm_base*>(wl_registry_bind(registry, name, &xdg_wm_base_interface, 1)));
        xdg_wm_base_add_listener(pContext->Handles.WmBase.get(), &g_wmBaseListener, pContext);
      }
      else if (strcmp(interface, wl_shell_interface.name) == 0)
      {
        pContext->Handles.Shell.reset(static_cast<wl_shell*>(wl_registry_bind(registry, name, &wl_shell_interface, 1)));
      }
      else if (strcmp(interface, wl_seat_interface.name) == 0)
      {
        if (!pContext->Handles.Seat)
        {
          pContext->Handles.Seat.reset(static_cast<wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, 1)));
          if (pContext->Handles.Seat)
          {
            wl_seat_add_listener(pContext->Handles.Seat.get(), &g_seatListener, pContext);
          }
        }
        else
        {
          // The pointer and the keyboard of the framework are those of one seat
          FSLLOG3_VERBOSE("Wayland: the compositor has more than one seat, the first one is used");
        }
      }
      else if (strcmp(interface, wl_shm_interface.name) == 0)
      {
        // The cursor theme needs it, the theme is loaded when the pointer enters the window (UpdateCursorTheme)
        pContext->Handles.Shm.reset(static_cast<wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, 1)));
      }
      else if (strcmp(interface, wl_output_interface.name) == 0)
      {
        AddOutputInfo(*pContext, name, version);
      }
      else if (strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0)
      {
        pContext->Handles.DecorationManager.reset(
          static_cast<zxdg_decoration_manager_v1*>(wl_registry_bind(registry, name, &zxdg_decoration_manager_v1_interface, 1)));
      }
      else if (WaylandIVIHandler::TryRegistryHandleGlobal(*pContext, registry, name, interface, version))
      {
      }
    }

    void OnRegistryGlobalRemove(void* data, wl_registry* registry, uint32_t name)
    {
      auto* pContext = static_cast<PlatformNativeWindowSystemContextWayland*>(data);
      if (pContext == nullptr)
      {
        return;
      }
      // A output that was unplugged or switched off: its record goes, and the window is no longer on it
      auto& rOutputs = pContext->Outputs;
      const auto itrFind =
        std::find_if(rOutputs.begin(), rOutputs.end(), [name](const std::unique_ptr<OutputInfo>& info) { return info && info->Global.Id == name; });
      if (itrFind == rOutputs.end())
      {
        return;
      }
      FSLLOG3_VERBOSE("Wayland: a output was removed");
      PlatformNativeWindowContextWayland* const pWindow = pContext->Window;
      if (pWindow != nullptr)
      {
        const wl_output* const pRemovedOutput = (*itrFind)->Output.get();
        auto& rEnteredOutputs = pWindow->EnteredOutputs;
        rEnteredOutputs.erase(std::remove(rEnteredOutputs.begin(), rEnteredOutputs.end(), pRemovedOutput), rEnteredOutputs.end());
      }
      rOutputs.erase(itrFind);
      if (pWindow != nullptr)
      {
        UpdateBufferScale(*pWindow);
        PostWindowConfigChanged(*pWindow);
      }
    }

    const wl_registry_listener g_registryListener = {OnRegistryGlobal, OnRegistryGlobalRemove};

    uint32_t CalcDPI(const uint32_t width, const uint32_t millimeterWidth)
    {
      assert(width > 0);
      assert(millimeterWidth > 0);
      // 1mm = 0.0393701f inches
      const auto w = static_cast<double>(width);
      const double inchesWidth = static_cast<double>(millimeterWidth) * 0.0393701;
      return static_cast<uint32_t>(std::round(w / inchesWidth));
    }

    //! True if the output says how large it is. The protocol allows a physical size of zero, and a output without a panel of a known
    //! size has that: a virtual display, a projector.
    bool HasPhysicalSize(const OutputInfo& output) noexcept
    {
      return output.Geometry.PhysicalWidth > 0 && output.Geometry.PhysicalHeight > 0;
    }

    //! The DPI of a output: the pixels of its current mode over its physical size, or the default when it has no physical size (or
    //! no current mode yet).
    Point2 CalcOutputDPI(const OutputInfo& output)
    {
      int32_t width = 0;
      int32_t height = 0;
      for (const auto& mode : output.Modes)
      {
        if ((mode.Flags & WL_OUTPUT_MODE_CURRENT) != 0u)
        {
          width = mode.Width;
          height = mode.Height;
        }
      }
      if (width <= 0 || height <= 0 || !HasPhysicalSize(output))
      {
        return {LocalConfig::MagicDefaultDpi, LocalConfig::MagicDefaultDpi};
      }
      return {
        UncheckedNumericCast<int32_t>(CalcDPI(UncheckedNumericCast<uint32_t>(width), UncheckedNumericCast<uint32_t>(output.Geometry.PhysicalWidth))),
        UncheckedNumericCast<int32_t>(
          CalcDPI(UncheckedNumericCast<uint32_t>(height), UncheckedNumericCast<uint32_t>(output.Geometry.PhysicalHeight)))};
    }

    //! The output the window is on: the most recently entered output that is known. A surface that has not entered a output yet can
    //! only be placed when there is one output. Null if it can not be told.
    const OutputInfo* TryGetWindowOutput(const std::vector<std::unique_ptr<OutputInfo>>& outputs, const std::vector<wl_output*>& enteredOutputs)
    {
      for (std::size_t i = enteredOutputs.size(); i > 0; --i)
      {
        const wl_output* const pEnteredOutput = enteredOutputs[i - 1];
        const auto itrFind = std::find_if(outputs.begin(), outputs.end(), [pEnteredOutput](const std::unique_ptr<OutputInfo>& info)
                                          { return info && info->Output.get() == pEnteredOutput; });
        if (itrFind != outputs.end())
        {
          return itrFind->get();
        }
      }
      return (enteredOutputs.empty() && outputs.size() == 1u) ? outputs.front().get() : nullptr;
    }

    std::shared_ptr<IPlatformNativeWindowAdapter>
      AllocateWindow(const NativeWindowSetup& nativeWindowSetup, const PlatformNativeWindowParams& windowParams,
                     const PlatformNativeWindowAllocationParams* const pPlatformCustomWindowAllocationParams)
    {
      return std::make_shared<PlatformNativeWindowAdapterWayland>(nativeWindowSetup, windowParams, pPlatformCustomWindowAllocationParams);
    }

    //! Registers a window at the window system and takes it out again unless its creation completed, so the window system never keeps
    //! a window or a surface that is gone.
    class ScopedWindowRegistration final
    {
      PlatformNativeWindowSystemContextWayland& m_rContext;
      bool m_keep{false};

    public:
      ScopedWindowRegistration(const ScopedWindowRegistration&) = delete;
      ScopedWindowRegistration& operator=(const ScopedWindowRegistration&) = delete;

      ScopedWindowRegistration(PlatformNativeWindowSystemContextWayland& rContext, PlatformNativeWindowContextWayland* const pWindow) noexcept
        : m_rContext(rContext)
      {
        m_rContext.Window = pWindow;
      }

      ~ScopedWindowRegistration() noexcept
      {
        if (!m_keep)
        {
          m_rContext.PresentationTime.SetSurface(nullptr);
          m_rContext.Window = nullptr;
        }
      }

      void Keep() noexcept
      {
        m_keep = true;
      }
    };

    void ExtractOutputs(std::vector<PlatformNativeWindowAdapterWayland::WaylandDisplayGeometry>& rDst,
                        const std::vector<std::unique_ptr<OutputInfo>>& outputs)
    {
      for (const auto& info : outputs)
      {
        PlatformNativeWindowAdapterWayland::WaylandDisplayGeometry currentDisplayGeometry{};
        currentDisplayGeometry.PhysicalWidth = info->Geometry.PhysicalWidth;
        currentDisplayGeometry.PhysicalHeight = info->Geometry.PhysicalHeight;
        for (const auto& mode : info->Modes)
        {
          if ((mode.Flags & WL_OUTPUT_MODE_CURRENT) != 0u)
          {
            currentDisplayGeometry.Width = mode.Width;
            currentDisplayGeometry.Height = mode.Height;
          }
        }
        rDst.push_back(currentDisplayGeometry);
      }
    }
  }


  PlatformNativeWindowSystemAdapterWayland::PlatformNativeWindowSystemAdapterWayland(
    const NativeWindowSystemSetup& setup, const PlatformNativeWindowAllocationFunction& allocateWindowFunction,
    const PlatformNativeWindowSystemParams& systemParams)
    : PlatformNativeWindowSystemAdapter(setup, nullptr)
    , m_allocationFunction(allocateWindowFunction ? allocateWindowFunction : AllocateWindow)
    , m_windowSystemContext(std::make_shared<PlatformNativeWindowSystemContextWayland>(setup.GetEventQueue()))
  {
    // TODO: handle proper shutdown in case of exception here
    m_windowSystemContext->Handles.Display.reset(wl_display_connect(nullptr));
    if (!m_windowSystemContext->Handles.Display)
    {
      throw GraphicsException("wl_display_connect Failure");
    }

    try
    {
      m_platformDisplay = m_windowSystemContext->Handles.Display.get();

      m_windowSystemContext->Handles.Registry.reset(wl_display_get_registry(m_windowSystemContext->Handles.Display.get()));
      if (!m_windowSystemContext->Handles.Registry)
      {
        throw GraphicsException("wl_display_get_registry Failure");
      }

      if (wl_registry_add_listener(m_windowSystemContext->Handles.Registry.get(), &g_registryListener, m_windowSystemContext.get()) == -1)
      {
        throw GraphicsException("wl_registry_add_listener Failure");
      }

      // A roundtrip: the compositor has answered every request made so far when it returns, so every global is listed. One
      // dispatch only handles what happens to have arrived.
      if (wl_display_roundtrip(m_windowSystemContext->Handles.Display.get()) == -1)
      {
        throw GraphicsException(
          fmt::format("wl_display_roundtrip Failure: {}", WaylandEventPump::DescribeError(m_windowSystemContext->Handles.Display.get())));
      }
      if (!m_windowSystemContext->Handles.Compositor)
      {
        throw GraphicsException("The compositor has no wl_compositor, a window can not be created");
      }

      const auto eventQueue = m_windowSystemContext->EventQueue.lock();
      if (eventQueue)
      {
        const NativeWindowEvent event = NativeWindowEventHelper::EncodeGamepadConfiguration(0);
        eventQueue->PostEvent(event);
      }
    }
    catch (const std::exception&)
    {
      throw;
    }
  }


  PlatformNativeWindowSystemAdapterWayland::~PlatformNativeWindowSystemAdapterWayland()
  {
    m_windowSystemContext->PresentationTime.Reset();
    m_windowSystemContext->Outputs.clear();

    m_windowSystemContext->Handles.CursorSurface.reset();

    m_windowSystemContext->Handles.Ivi.Reset();
    m_windowSystemContext->Handles.DecorationManager.reset();
    m_windowSystemContext->Handles.Keyboard.reset();
    m_windowSystemContext->Handles.Pointer.reset();

    // m_windowSystemContext.DefaultCursor.reset();
    m_windowSystemContext->Handles.CursorTheme.reset();
    m_windowSystemContext->Handles.Shm.reset();

    m_windowSystemContext->Handles.Seat.reset();
    m_windowSystemContext->Handles.WmBase.reset();
    m_windowSystemContext->Handles.Shell.reset();
    m_windowSystemContext->Handles.Compositor.reset();
    m_windowSystemContext->Handles.Registry.reset();

    if (wl_display_flush(m_windowSystemContext->Handles.Display.get()) == -1)
    {
      FSLLOG3_WARNING("wl_display_flush Failure");
    }

    m_windowSystemContext->Handles.Display.reset();

    m_windowSystemContext->MarkAsShutdown();

    m_windowSystemContext.reset();
  }


  std::shared_ptr<IPlatformNativeWindowAdapter> PlatformNativeWindowSystemAdapterWayland::CreateNativeWindow(
    const NativeWindowSetup& nativeWindowSetup, const PlatformNativeWindowAllocationParams* const pPlatformCustomWindowAllocationParams)
  {
    return m_allocationFunction(nativeWindowSetup, PlatformNativeWindowParams(m_windowSystemContext, m_platformDisplay, nullptr, nullptr, nullptr),
                                pPlatformCustomWindowAllocationParams);
  }


  bool PlatformNativeWindowSystemAdapterWayland::ProcessMessages(const NativeWindowProcessMessagesArgs& args)
  {
    // Send what waits, read what the compositor sent and dispatch it. The graphics API reads the socket too when it presents, but only
    // while it presents, and it does not report a lost connection to the app.
    wl_display* const pDisplay = m_windowSystemContext->Handles.Display.get();
    if (!m_windowSystemContext->RequestClose && !WaylandEventPump::TryProcessEvents(pDisplay))
    {
      FSLLOG3_ERROR("Wayland: the connection to the compositor was lost ({}), the app is closed", WaylandEventPump::DescribeError(pDisplay));
      m_windowSystemContext->RequestClose = true;
    }
    if (m_windowSystemContext->RequestClose)
    {
      return false;
    }

    // Ask when the frame that is drawn next is shown (it rides on the commit the graphics API makes for it)
    m_windowSystemContext->PresentationTime.RequestFeedback();
    return true;
  }


  PlatformNativeWindowAdapterWayland::PlatformNativeWindowAdapterWayland(
    const NativeWindowSetup& nativeWindowSetup, const PlatformNativeWindowParams& platformWindowParams,
    const PlatformNativeWindowAllocationParams* const pPlatformCustomWindowAllocationParams)
    : PlatformNativeWindowAdapter(nativeWindowSetup, platformWindowParams, pPlatformCustomWindowAllocationParams,
                                  NativeWindowCapabilityFlags::GetDpi | NativeWindowCapabilityFlags::GetDisplayInfo |
                                    NativeWindowCapabilityFlags::GetVSyncInfo)
    , m_windowSystemContext(platformWindowParams.WindowSystemWaylandContext)
  {
    const NativeWindowConfig nativeWindowConfig = nativeWindowSetup.GetConfig();
    // The vsync source of the window. Wayland has one: the presentation-time protocol. Checked first, as nothing has been created yet
    m_requestedVSyncSource = nativeWindowConfig.GetVSyncSource();
    if (!m_requestedVSyncSource.empty() && m_requestedVSyncSource != "auto" && m_requestedVSyncSource != "presentation-time")
    {
      throw NotSupportedException(
        fmt::format("VSyncSource '{}' is not a vsync source of this window system (auto, presentation-time)", m_requestedVSyncSource));
    }
    const auto windowSystemContext = platformWindowParams.WindowSystemWaylandContext.lock();
    if (!windowSystemContext)
    {
      throw std::invalid_argument("Can not be null");
    }
    m_windowContext = std::make_unique<PlatformNativeWindowContextWayland>(windowSystemContext->EventQueue);
    m_windowContext->SystemContext = windowSystemContext.get();
    // The buffers of the window follow the scale of its output unless that was switched off (--BufferScale 1), where the compositor
    // has wl_surface.set_buffer_scale. A IVI surface is left as it was: its size does not come from a xdg configure.
    m_windowContext->FollowOutputScale = nativeWindowSetup.GetConfig().GetBufferScale() != 1u && windowSystemContext->CompositorVersion >= 3u &&
                                         !windowSystemContext->Handles.Ivi.Enabled;

    FSLLOG3_WARNING_IF(nativeWindowSetup.GetConfig().GetDisplayId() != 0, "Wayland only supports the main display. Using DisplayId 0 instead of {}",
                       nativeWindowSetup.GetConfig().GetDisplayId());
    if (nativeWindowConfig.GetWindowMode() != WindowMode::Window)
    {
      FSLLOG3_VERBOSE("Fullscreen: Window Size/Position not defined, setting them to MAX Display Resolution");
      if (windowSystemContext->Handles.Ivi.Enabled)
      {
        m_windowContext->DesiredWindowSize = PxSize2D::Create(480, 504);
        m_windowContext->Fullscreen = false;
        FSLLOG3_VERBOSE("Wayland IVI not configured to fullscreen access even though requested to do so");
      }
      else
      {
        m_windowContext->DesiredWindowSize = PxSize2D::Create(250, 250);
        m_windowContext->Fullscreen = true;
      }
    }
    else
    {
      FSLLOG3_WARNING("Wayland does not allow the app to control the window position, so position request was ignored");
      const Rectangle windowRectangle = nativeWindowConfig.GetWindowRectangle();
      m_windowContext->DesiredWindowSize = TypeConverter::UncheckedTo<PxSize2D>(windowRectangle.GetSize());
    }
    FSLLOG3_VERBOSE("Creating window of size:{} and fullscreen:{}", m_windowContext->DesiredWindowSize, m_windowContext->Fullscreen);

    assert(windowSystemContext->Window == nullptr);
    // The window system knows the window from here on, and forgets it again if the rest of this fails
    ScopedWindowRegistration windowRegistration(*windowSystemContext, m_windowContext.get());
    windowSystemContext->MousePosition = PxPoint2::Create(0, 0);

    // if (m_windowContext->Fullscreen && windowSystemContext->Handles.WmBase)
    // {
    //   CreateWlDummySurface(*windowSystemContext, *m_windowContext);
    //   m_windowContext->Fullscreen = false;
    // }

    FSLLOG3_INFO_IF(nativeWindowSetup.GetVerbosityLevel() > 0,
                    "PlatformNativeWindowAdapterWayland: Creating window: (DesiredWindowSize={} Fullscreen: {})", m_windowContext->DesiredWindowSize,
                    m_windowContext->Fullscreen);


    CreateWlSurface(*windowSystemContext, *m_windowContext);
    m_platformSurface = m_windowContext->Handles.Surface.get();

    // Created before the native window, so nothing that can fail comes after that
    windowSystemContext->Handles.CursorSurface.reset(wl_compositor_create_surface(windowSystemContext->Handles.Compositor.get()));
    if (!windowSystemContext->Handles.CursorSurface)
    {
      throw GraphicsException("wl_compositor_create_surface CursorSurface failure");
    }

    // The display times of the frames of this surface are asked for from now on
    windowSystemContext->PresentationTime.SetSurface(m_platformSurface);

    void* nativeWindowHolder = nullptr;
    if (platformWindowParams.CreateWaylandWindow)
    {
      nativeWindowHolder = platformWindowParams.CreateWaylandWindow(m_windowContext->Handles.Surface.get(), m_windowContext->Geometry.RawWidth(),
                                                                    m_windowContext->Geometry.RawHeight());
    }
    m_windowContext->Native = static_cast<PlatformNativeWindowType>(nativeWindowHolder);
    m_platformWindow = m_windowContext->Native;

    // Assign the Destroy Callback, there must be a smarter way to call them to avoid using globals.
    if (platformWindowParams.DestroyWaylandWindow)
    {
      m_destroyWindowCallback = platformWindowParams.DestroyWaylandWindow;
    }
    if (platformWindowParams.ResizeWaylandWindow)
    {
      m_windowContext->ResizeWindowCallback = platformWindowParams.ResizeWaylandWindow;
    }

    do
    {
      windowSystemContext->RoundtripNeeded = false;
      wl_display_roundtrip(windowSystemContext->Handles.Display.get());
    } while (windowSystemContext->RoundtripNeeded);

    // The registry has listed every global by now
    LogFrameTimingGlobals(*windowSystemContext);

    ExtractOutputs(m_displayOutput, windowSystemContext->Outputs);

    // The outputs have said what they are by now, so the scale of the only output is known before the window has its first buffer
    UpdateBufferScale(*m_windowContext);

    // The DPI is that of the output the window is on and is read when it is asked for (TryGetNativeDpi). This is what it is before
    // the window is on a output, and a output without a physical size is said once, as a window on it gets the default.
    m_cachedScreenDPI = Point2(LocalConfig::MagicDefaultDpi, LocalConfig::MagicDefaultDpi);
    for (const auto& output : windowSystemContext->Outputs)
    {
      if (output && !HasPhysicalSize(*output))
      {
        FSLLOG3_INFO("Wayland: a output reports no physical size, the DPI of a window on it is the default of {}", LocalConfig::MagicDefaultDpi);
        break;
      }
    }

    {    // Post the activation message to let the framework know we are ready
      const std::shared_ptr<INativeWindowEventQueue> eventQueue = m_windowContext->EventQueue.lock();
      if (eventQueue)
      {
        eventQueue->PostEvent(NativeWindowEventHelper::EncodeWindowActivationEvent(true));
      }
    }
    windowRegistration.Keep();
  }


  PlatformNativeWindowAdapterWayland::~PlatformNativeWindowAdapterWayland()
  {
    {    // The display times were asked for the surface of this window
      const auto windowSystemContext = m_windowSystemContext.lock();
      if (windowSystemContext)
      {
        windowSystemContext->PresentationTime.SetSurface(nullptr);
        // The window system must not keep a window that is gone
        if (windowSystemContext->Window == m_windowContext.get())
        {
          windowSystemContext->Window = nullptr;
        }
      }
    }
    if (m_windowContext->Native != nullptr)
    {
      if (m_destroyWindowCallback)
      {
        m_destroyWindowCallback(m_windowContext->Native);
      }
    }

    m_windowContext->Handles.Reset();
    // m_windowContext->Callback.reset();
  }


  bool PlatformNativeWindowAdapterWayland::TryGetNativeSize(PxPoint2& rSize) const
  {
    const auto width = m_windowContext->Geometry.RawWidth();
    const auto height = m_windowContext->Geometry.RawHeight();
    if (width <= 0 || height <= 0)
    {
      return false;
    }

    rSize = PxPoint2::Create(width, height);
    FSLLOG3_VERBOSE5("TryGetNativeSize: {}", rSize)
    return true;
  }


  bool PlatformNativeWindowAdapterWayland::TryGetNativeDpi(Vector2& rDPI) const
  {
    const auto windowSystemContext = m_windowSystemContext.lock();
    const OutputInfo* const pOutputInfo =
      windowSystemContext ? TryGetWindowOutput(windowSystemContext->Outputs, m_windowContext->EnteredOutputs) : nullptr;
    // The DPI of the output is that of its own pixels. A pixel of the window is one of them when the buffer has the scale of the
    // output, and several when the compositor enlarges the window. A output without a physical size has the default DPI at a scale
    // of one, which is the scale times that for a buffer that follows it (what a Win32 window reports for a scaled display).
    const int32_t bufferScale = m_windowContext->BufferScale;
    Point2 dpi = m_cachedScreenDPI * bufferScale;
    if (pOutputInfo != nullptr)
    {
      const int32_t outputScale = std::max(pOutputInfo->Geometry.Scale, 1);
      const Point2 outputDpi = CalcOutputDPI(*pOutputInfo);
      dpi = HasPhysicalSize(*pOutputInfo) ? Point2((outputDpi.X * bufferScale) / outputScale, (outputDpi.Y * bufferScale) / outputScale)
                                          : outputDpi * bufferScale;
    }
    rDPI = Vector2(dpi.X, dpi.Y);
    return true;
  }


  NativeWindowTimingSupport PlatformNativeWindowAdapterWayland::GetTimingSupport() const
  {
    // What the compositor offers, and what is used of it: presentation-time gives the vsync time, the refresh rate comes from wl_output
    NativeWindowTimingSupport support;
    support.WindowSystem = "Wayland";
    support.RequestedVSyncSource = m_requestedVSyncSource.empty() ? "auto" : m_requestedVSyncSource;
    const auto windowSystemContext = m_windowSystemContext.lock();
    {
      const bool isUsed = windowSystemContext && windowSystemContext->PresentationTime.IsBound();
      support.VSyncSources.emplace_back("presentation-time", isUsed ? NativeWindowVSyncSourceState::Used : NativeWindowVSyncSourceState::NotAvailable,
                                        isUsed ? "The presentation-time protocol (wp_presentation): when the compositor showed a frame of the window"
                                               : "The presentation-time protocol (wp_presentation): the compositor does not have it");
    }
    if (windowSystemContext)
    {
      for (const char* const pszInterface : LocalConfig::FrameTimingGlobals)
      {
        const GlobalInfo* const pGlobal = TryFindFrameTimingGlobal(*windowSystemContext, pszInterface);
        if (pGlobal != nullptr)
        {
          support.Available.emplace_back(pszInterface);
          support.Versions.emplace_back(pszInterface, pGlobal->Version);
        }
        else
        {
          support.NotAvailable.emplace_back(pszInterface);
        }
      }
      if (windowSystemContext->PresentationTime.IsBound())
      {
        support.VSyncSource = "wp_presentation";
        support.Used.emplace_back("wp_presentation");
      }
    }
    return support;
  }


  NativeWindowVSyncInfo PlatformNativeWindowAdapterWayland::TryGetNativeVSyncInfo() const
  {
    // When the compositor last showed a frame of this window: a vertical blank of the output, on the clock of the HighResolutionTimer
    const auto windowSystemContext = m_windowSystemContext.lock();
    if (!windowSystemContext || !windowSystemContext->PresentationTime.IsBound())
    {
      return {};
    }
    NativeWindowVSyncInfo info = windowSystemContext->PresentationTime.GetVSyncInfo();
    if (info.VSyncTime.Ticks() > 0 && info.RefreshPeriod.Ticks() <= 0)
    {
      // The compositor gave no refresh period with the time, so the one of the mode of the output is used
      info.RefreshPeriod = TryGetNativeDisplayInfo().RefreshInterval;
    }
    return info;
  }


  NativeWindowDisplayInfo PlatformNativeWindowAdapterWayland::TryGetNativeDisplayInfo() const
  {
    const auto windowSystemContext = m_windowSystemContext.lock();
    if (!windowSystemContext)
    {
      return {};
    }
    const auto& outputs = windowSystemContext->Outputs;

    const OutputInfo* const pOutputInfo = TryGetWindowOutput(outputs, m_windowContext->EnteredOutputs);

    if (pOutputInfo == nullptr || pOutputInfo->CurrentRefreshMilliHz <= 0)
    {
      return {};
    }
    return NativeWindowDisplayInfo(TimeSpanUtil::FromFrequencyRational(static_cast<uint64_t>(pOutputInfo->CurrentRefreshMilliHz), 1000u));
  }
}
#endif
