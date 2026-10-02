#ifndef SHARED_FRAMEPACING_EGL_EGLSWAPINTERVAL_HPP
#define SHARED_FRAMEPACING_EGL_EGLSWAPINTERVAL_HPP
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
#include <memory>

namespace Fsl
{
  class IEGLHostInfo;

  //! Sets the EGL swap interval: the number of display refreshes eglSwapBuffers holds a frame for.
  //! A EGL config only supports a range of swap intervals and it can be as small as one refresh, so Set returns the interval that was
  //! set and the app has to hold the frame for the rest itself.
  class EGLSwapInterval final
  {
    std::shared_ptr<IEGLHostInfo> m_hostInfo;
    //! The longest swap interval the EGL config supports
    uint32_t m_maxSwapInterval{1};
    //! The swap interval that is set (the EGL default is 1)
    uint32_t m_swapInterval{1};

  public:
    explicit EGLSwapInterval(std::shared_ptr<IEGLHostInfo> hostInfo);

    //! @brief Set the swap interval of the following eglSwapBuffers calls (call it with the context of the host current).
    //! @return the swap interval that is set: the requested one limited to what the EGL config supports.
    uint32_t Set(const uint32_t swapInterval);

    [[nodiscard]] uint32_t Get() const noexcept
    {
      return m_swapInterval;
    }

    [[nodiscard]] uint32_t Max() const noexcept
    {
      return m_maxSwapInterval;
    }
  };
}

#endif
