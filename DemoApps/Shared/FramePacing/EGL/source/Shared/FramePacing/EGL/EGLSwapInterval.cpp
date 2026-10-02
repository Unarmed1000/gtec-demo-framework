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

#include <FslBase/Log/Log3Fmt.hpp>
#include <FslDemoHost/EGL/Config/Service/IEGLHostInfo.hpp>
#include <Shared/FramePacing/EGL/EGLSwapInterval.hpp>
#include <EGL/egl.h>
#include <algorithm>
#include <array>
#include <utility>

namespace Fsl
{
  namespace
  {
    //! The longest swap interval the config of the surface supports (1 if it can not be read)
    uint32_t ReadMaxSwapInterval(const EGLDisplay hDisplay, const EGLSurface hSurface)
    {
      EGLint configId = 0;
      EGLConfig hConfig{};
      EGLint configCount = 0;
      EGLint maxSwapInterval = 1;
      const bool found = eglQuerySurface(hDisplay, hSurface, EGL_CONFIG_ID, &configId) == EGL_TRUE;
      const std::array<EGLint, 3> configAttribs = {EGL_CONFIG_ID, configId, EGL_NONE};
      if (!found || eglChooseConfig(hDisplay, configAttribs.data(), &hConfig, 1, &configCount) != EGL_TRUE || configCount != 1 ||
          eglGetConfigAttrib(hDisplay, hConfig, EGL_MAX_SWAP_INTERVAL, &maxSwapInterval) != EGL_TRUE)
      {
        // Do not leave a error behind for the host to find
        eglGetError();
        return 1u;
      }
      return static_cast<uint32_t>(std::max(maxSwapInterval, EGLint{1}));
    }
  }


  EGLSwapInterval::EGLSwapInterval(std::shared_ptr<IEGLHostInfo> hostInfo)
    : m_hostInfo(std::move(hostInfo))
    , m_maxSwapInterval(ReadMaxSwapInterval(m_hostInfo->GetDisplay(), m_hostInfo->GetSurface()))
  {
  }


  uint32_t EGLSwapInterval::Set(const uint32_t swapInterval)
  {
    const uint32_t newSwapInterval = std::clamp(swapInterval, 1u, m_maxSwapInterval);
    if (newSwapInterval == m_swapInterval)
    {
      return m_swapInterval;
    }
    if (eglSwapInterval(m_hostInfo->GetDisplay(), static_cast<EGLint>(newSwapInterval)) == EGL_TRUE)
    {
      m_swapInterval = newSwapInterval;
    }
    else
    {
      FSLLOG3_WARNING("eglSwapInterval({}) failed, using a swap interval of {}", newSwapInterval, m_swapInterval);
      // Do not leave a error behind for the host to find, and do not try a longer swap interval than the one that is set again
      eglGetError();
      m_maxSwapInterval = m_swapInterval;
    }
    return m_swapInterval;
  }
}
