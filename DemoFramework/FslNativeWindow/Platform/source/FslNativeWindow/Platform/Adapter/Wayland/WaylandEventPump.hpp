#if !defined(__ANDROID__) && defined(__linux__) && !defined(FSL_WINDOWSYSTEM_X11) && defined(FSL_WINDOWSYSTEM_WAYLAND)
#ifndef FSLNATIVEWINDOW_PLATFORM_ADAPTER_WAYLAND_WAYLANDEVENTPUMP_HPP
#define FSLNATIVEWINDOW_PLATFORM_ADAPTER_WAYLAND_WAYLANDEVENTPUMP_HPP
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

struct wl_display;
struct wl_event_queue;

namespace Fsl::WaylandEventPump
{
  //! @brief One turn of the message loop of a Wayland client that does not block: the requests that wait are sent, the events the
  //!        compositor has sent are read from the socket and the events of the default queue are dispatched.
  //! @note  Reading is done the way the library asks for when more than one thread uses the display (wl_display_prepare_read and
  //!        wl_display_read_events), as the graphics API reads the same socket for a queue of its own.
  //! @return false if the connection to the compositor is lost (the socket was closed, or a protocol error was reported). Nothing can
  //!         be sent or received after that.
  [[nodiscard]] bool TryProcessEvents(wl_display* const pDisplay) noexcept;

  //! @brief Wait for what the compositor sends, read it from the socket and leave it on the queues it belongs to. Nothing is dispatched.
  //!        Use it for a queue of the caller, which dispatches that queue and calls this again until it has what it waits for.
  //! @param pQueue the queue of the caller. If it holds events already this returns at once, so they get dispatched first.
  //! @param timeoutMilliseconds the longest to wait, zero only looks
  //! @return false if the connection to the compositor is lost
  [[nodiscard]] bool TryReadEvents(wl_display* const pDisplay, wl_event_queue* const pQueue, const int32_t timeoutMilliseconds) noexcept;

  //! @brief Why the connection to the compositor was lost, for a log (empty if the display has no error)
  [[nodiscard]] std::string DescribeError(wl_display* const pDisplay);
}

#endif
#endif
