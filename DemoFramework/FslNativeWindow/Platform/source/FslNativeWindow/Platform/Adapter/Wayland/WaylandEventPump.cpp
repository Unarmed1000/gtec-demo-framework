#if !defined(__ANDROID__) && defined(__linux__) && !defined(FSL_WINDOWSYSTEM_X11) && defined(FSL_WINDOWSYSTEM_WAYLAND)
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

#include "WaylandEventPump.hpp"
#include <fmt/format.h>
#include <poll.h>
#include <wayland-client.h>
#include <cerrno>
#include <cstdint>
#include <cstring>

namespace Fsl::WaylandEventPump
{
  bool TryProcessEvents(wl_display* const pDisplay) noexcept
  {
    if (pDisplay == nullptr)
    {
      return false;
    }

    // The socket can only be read while the default queue is empty, so what is queued already is dispatched first
    while (wl_display_prepare_read(pDisplay) != 0)
    {
      if (wl_display_dispatch_pending(pDisplay) < 0)
      {
        return false;
      }
    }

    // The requests that were made since the last flush. EAGAIN: the socket is full, the rest goes with a later flush.
    if (wl_display_flush(pDisplay) < 0 && errno != EAGAIN)
    {
      wl_display_cancel_read(pDisplay);
      return false;
    }

    pollfd pollInfo{};
    pollInfo.fd = wl_display_get_fd(pDisplay);
    pollInfo.events = POLLIN;
    int pollResult = 0;
    do
    {
      pollResult = poll(&pollInfo, 1, 0);
    } while (pollResult < 0 && errno == EINTR);

    if (pollResult > 0 && (pollInfo.revents & POLLIN) != 0)
    {
      if (wl_display_read_events(pDisplay) < 0)
      {
        return false;
      }
    }
    else
    {
      wl_display_cancel_read(pDisplay);
      if (pollResult < 0 || (pollResult > 0 && (pollInfo.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0))
      {
        return false;
      }
    }
    return wl_display_dispatch_pending(pDisplay) >= 0;
  }


  std::string DescribeError(wl_display* const pDisplay)
  {
    if (pDisplay == nullptr)
    {
      return "no display";
    }
    const int error = wl_display_get_error(pDisplay);
    if (error == 0)
    {
      return {};
    }
    if (error == EPROTO)
    {
      const wl_interface* pInterface = nullptr;
      uint32_t objectId = 0;
      const uint32_t code = wl_display_get_protocol_error(pDisplay, &pInterface, &objectId);
      return fmt::format("protocol error {} on {}@{}", code, pInterface != nullptr ? pInterface->name : "an unknown interface", objectId);
    }
    return fmt::format("{} (errno {})", std::strerror(error), error);
  }
}
#endif
