#if !defined(__ANDROID__) && defined(__linux__) && !defined(FSL_WINDOWSYSTEM_X11) && defined(FSL_WINDOWSYSTEM_WAYLAND) && \
  defined(FSL_WINDOWSYSTEM_WAYLAND_XDG)
#ifndef FSLNATIVEWINDOW_PLATFORM_ADAPTER_WAYLAND_WAYLANDOUTPUTSIZE_HPP
#define FSLNATIVEWINDOW_PLATFORM_ADAPTER_WAYLAND_WAYLANDOUTPUTSIZE_HPP
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

#include <wayland-client.h>
#include <algorithm>
#include <memory>
#include <vector>
#include "PlatformNativeWindowSystemContextWayland.hpp"

namespace Fsl::WaylandOutputSize
{
  //! The size of a output in the logical units of the compositor (the units a surface is sized in)
  struct LogicalSize
  {
    int32_t Width{0};
    int32_t Height{0};

    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
      return Width > 0 && Height > 0;
    }
  };

  //! @brief The logical size of a output: its current mode, turned the way the output is turned, divided by the scale of the output.
  //! @note  A surface of this size with a buffer of the scale of the output covers the output pixel for pixel.
  //! @return a size that is not valid if the output has not said what its mode is
  [[nodiscard]] inline LogicalSize CalcLogicalSize(const OutputInfo& output) noexcept
  {
    const OutputModeRecord* pMode = nullptr;
    for (const OutputModeRecord& mode : output.Modes)
    {
      if ((mode.Flags & WL_OUTPUT_MODE_CURRENT) != 0u)
      {
        pMode = &mode;
        break;
      }
    }
    if (pMode == nullptr || pMode->Width <= 0 || pMode->Height <= 0)
    {
      return {};
    }
    const int32_t scale = std::max(output.Geometry.Scale, 1);
    LogicalSize size;
    switch (output.Geometry.OutputTransform)
    {
    case WL_OUTPUT_TRANSFORM_90:
    case WL_OUTPUT_TRANSFORM_270:
    case WL_OUTPUT_TRANSFORM_FLIPPED_90:
    case WL_OUTPUT_TRANSFORM_FLIPPED_270:
      // The mode is that of the panel, which is on its side
      size.Width = pMode->Height / scale;
      size.Height = pMode->Width / scale;
      break;
    default:
      size.Width = pMode->Width / scale;
      size.Height = pMode->Height / scale;
      break;
    }
    return size;
  }

  //! @brief The output a window that was given no place is taken to be on: the first of the outputs the surface has entered that has
  //!        said what its mode is, else the first output that has.
  //! @return null if no output has a mode
  [[nodiscard]] inline const OutputInfo* TryFindOutput(const std::vector<std::unique_ptr<OutputInfo>>& outputs,
                                                       const std::vector<wl_output*>& enteredOutputs) noexcept
  {
    for (const wl_output* const pEnteredOutput : enteredOutputs)
    {
      for (const auto& output : outputs)
      {
        if (output && output->Output.get() == pEnteredOutput && CalcLogicalSize(*output).IsValid())
        {
          return output.get();
        }
      }
    }
    for (const auto& output : outputs)
    {
      if (output && CalcLogicalSize(*output).IsValid())
      {
        return output.get();
      }
    }
    return nullptr;
  }
}

#endif
#endif
