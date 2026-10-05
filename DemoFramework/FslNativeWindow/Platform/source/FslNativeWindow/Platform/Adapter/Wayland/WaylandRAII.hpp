#ifndef FSLNATIVEWINDOW_PLATFORM_ADAPTER_WAYLAND_WAYLANDRAII_HPP
#define FSLNATIVEWINDOW_PLATFORM_ADAPTER_WAYLAND_WAYLANDRAII_HPP
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

#include <FslBase/BasicTypes.hpp>
#include <df-xdg-decoration-client-protocol.h>    // XDG wayland-scanner created header
#include <df-xdg-shell-client-protocol.h>         // XDG wayland-scanner created header
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <memory>

namespace Fsl
{
  struct CustomDeleterWlCallback
  {
    void operator()(wl_callback* pVal)
    {
      if (pVal != nullptr)
      {
        wl_callback_destroy(pVal);
      }
    }
  };


  struct CustomDeleterWlCompositor
  {
    void operator()(wl_compositor* pVal)
    {
      if (pVal != nullptr)
      {
        wl_compositor_destroy(pVal);
      }
    }
  };

  // struct CustomDeleterWlCursor
  // {
  //   void operator()(wl_cursor* pVal)
  //   {
  //     FSL_PARAM_NOT_USED(pVal);
  //     // Cursors do not need to be destroyed
  //     // if(pVal != nullptr)
  //     // {
  //     //   wl_cursor_destroy(pVal);
  //     // }
  //   }
  // };

  struct CustomDeleterWlCursorTheme
  {
    void operator()(wl_cursor_theme* pVal)
    {
      if (pVal != nullptr)
      {
        wl_cursor_theme_destroy(pVal);
      }
    }
  };

  struct CustomDeleterWlDisplay
  {
    void operator()(wl_display* pVal)
    {
      if (pVal != nullptr)
      {
        wl_display_disconnect(pVal);
      }
    }
  };

  struct CustomDeleterWlKeyboard
  {
    void operator()(wl_keyboard* pVal)
    {
      if (pVal != nullptr)
      {
        // wl_keyboard_release(pVal);
        wl_keyboard_destroy(pVal);
      }
    }
  };


  struct CustomDeleterWlOutput
  {
    void operator()(wl_output* pVal)
    {
      if (pVal != nullptr)
      {
        wl_output_destroy(pVal);
      }
    }
  };


  struct CustomDeleterWlPointer
  {
    void operator()(wl_pointer* pVal)
    {
      if (pVal != nullptr)
      {
        // wl_pointer_release(pVal);
        wl_pointer_destroy(pVal);
      }
    }
  };

  struct CustomDeleterWlSeat
  {
    void operator()(wl_seat* pVal)
    {
      if (pVal != nullptr)
      {
        // wl_seat.release came with version 5 of the interface. A request the bound version does not have is a protocol error.
        if (wl_seat_get_version(pVal) >= WL_SEAT_RELEASE_SINCE_VERSION)
        {
          wl_seat_release(pVal);
        }
        else
        {
          wl_seat_destroy(pVal);
        }
      }
    }
  };

  struct CustomDeleterWlRegistry
  {
    void operator()(wl_registry* pVal)
    {
      if (pVal != nullptr)
      {
        wl_registry_destroy(pVal);
      }
    }
  };


  struct CustomDeleterWlShell
  {
    void operator()(wl_shell* pVal)
    {
      if (pVal != nullptr)
      {
        wl_shell_destroy(pVal);
      }
    }
  };

  struct CustomDeleterWlShellSurface
  {
    void operator()(wl_shell_surface* pVal)
    {
      if (pVal != nullptr)
      {
        wl_shell_surface_destroy(pVal);
      }
    }
  };

  struct CustomDeleterWlShm
  {
    void operator()(wl_shm* pVal)
    {
      if (pVal != nullptr)
      {
        wl_shm_destroy(pVal);
      }
    }
  };

  struct CustomDeleterWlSurface
  {
    void operator()(wl_surface* pVal)
    {
      if (pVal != nullptr)
      {
        wl_surface_destroy(pVal);
      }
    }
  };

  struct CustomDeleterXdgWmBase
  {
    void operator()(xdg_wm_base* pVal)
    {
      if (pVal != nullptr)
      {
        xdg_wm_base_destroy(pVal);
      }
    }
  };


  struct CustomDeleterXdgSurface
  {
    void operator()(xdg_surface* pVal)
    {
      if (pVal != nullptr)
      {
        xdg_surface_destroy(pVal);
      }
    }
  };


  struct CustomDeleterXdgToplevel
  {
    void operator()(xdg_toplevel* pVal)
    {
      if (pVal != nullptr)
      {
        xdg_toplevel_destroy(pVal);
      }
    }
  };


  struct CustomDeleterZxdgDecorationManagerV1
  {
    void operator()(zxdg_decoration_manager_v1* pVal)
    {
      if (pVal != nullptr)
      {
        zxdg_decoration_manager_v1_destroy(pVal);
      }
    }
  };

  struct CustomDeleterZxdgToplevelDecorationV1
  {
    void operator()(zxdg_toplevel_decoration_v1* pVal)
    {
      if (pVal != nullptr)
      {
        zxdg_toplevel_decoration_v1_destroy(pVal);
      }
    }
  };

  using ScopedWaylandCallback = std::unique_ptr<wl_callback, CustomDeleterWlCallback>;
  using ScopedWaylandCompositor = std::unique_ptr<wl_compositor, CustomDeleterWlCompositor>;
  // using ScopedWaylandCursor = std::unique_ptr<wl_cursor, CustomDeleterWlCursor>;
  using ScopedWaylandCursorTheme = std::unique_ptr<wl_cursor_theme, CustomDeleterWlCursorTheme>;
  using ScopedWaylandDisplay = std::unique_ptr<wl_display, CustomDeleterWlDisplay>;
  using ScopedWaylandKeyboard = std::unique_ptr<wl_keyboard, CustomDeleterWlKeyboard>;
  using ScopedWaylandOutput = std::unique_ptr<wl_output, CustomDeleterWlOutput>;
  using ScopedWaylandPointer = std::unique_ptr<wl_pointer, CustomDeleterWlPointer>;
  using ScopedWaylandRegistry = std::unique_ptr<wl_registry, CustomDeleterWlRegistry>;
  using ScopedWaylandSeat = std::unique_ptr<wl_seat, CustomDeleterWlSeat>;
  using ScopedWaylandShell = std::unique_ptr<wl_shell, CustomDeleterWlShell>;
  using ScopedWaylandShellSurface = std::unique_ptr<wl_shell_surface, CustomDeleterWlShellSurface>;
  using ScopedWaylandShm = std::unique_ptr<wl_shm, CustomDeleterWlShm>;
  using ScopedWaylandSurface = std::unique_ptr<wl_surface, CustomDeleterWlSurface>;

  using ScopedWaylandXdgWmBase = std::unique_ptr<xdg_wm_base, CustomDeleterXdgWmBase>;
  using ScopedWaylandXdgSurface = std::unique_ptr<xdg_surface, CustomDeleterXdgSurface>;
  using ScopedWaylandXdgToplevel = std::unique_ptr<xdg_toplevel, CustomDeleterXdgToplevel>;
  using ScopedWaylandXdgDecorationManagerV1 = std::unique_ptr<zxdg_decoration_manager_v1, CustomDeleterZxdgDecorationManagerV1>;
  using ScopedWaylandXdgToplevelDecorationV1 = std::unique_ptr<zxdg_toplevel_decoration_v1, CustomDeleterZxdgToplevelDecorationV1>;

}

#endif
#endif
