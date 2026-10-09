#ifndef SHARED_FRAMEPACING_SAMPLEEXPLICITSYNCUTIL_HPP
#define SHARED_FRAMEPACING_SAMPLEEXPLICITSYNCUTIL_HPP
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


#include <Shared/FramePacing/SampleExplicitSync.hpp>
#include <string_view>

namespace Fsl
{
  struct NativeWindowTimingSupport;

  namespace SampleExplicitSyncUtil
  {
    //! The name of the global a Wayland compositor that offers explicit sync has
    constexpr std::string_view ExplicitSyncGlobalName = "wp_linux_drm_syncobj_manager_v1";

    //! @brief What is certain about explicit sync from what the window system says it offers: the global is listed as available or
    //!        as not available on Wayland, and not at all on a window system that is not Wayland.
    [[nodiscard]] SampleExplicitSync ToExplicitSync(const NativeWindowTimingSupport& support) noexcept;

    //! @brief What is shown on screen about explicit sync (empty when it does not apply)
    [[nodiscard]] std::string_view ToDisplayString(const SampleExplicitSync value) noexcept;
  }
}

#endif
