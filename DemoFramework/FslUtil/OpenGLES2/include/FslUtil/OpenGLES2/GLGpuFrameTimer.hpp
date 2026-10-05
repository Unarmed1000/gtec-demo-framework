#ifndef FSLUTIL_OPENGLES2_GLGPUFRAMETIMER_HPP
#define FSLUTIL_OPENGLES2_GLGPUFRAMETIMER_HPP
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

// Make sure Common.hpp is the first include file (to make the error message as helpful as possible when disabled)
#include <FslBase/Time/TimeSpan.hpp>
#include <FslUtil/OpenGLES2/Common.hpp>
#include <GLES2/gl2.h>
#include <array>
#include <cstdint>

namespace Fsl::GLES2
{
  //! @brief Measures the time the GPU needs for a frame with a GL_EXT_disjoint_timer_query time elapsed query around the commands of the
  //!        frame.
  //! @note  OpenGL ES 2 has no query objects of its own, so the functions of the extension are used (they are looked up when the timer
  //!        is created). The queries are a ring, a result is read once the GPU has it, so reading never waits (a frame is not measured if
  //!        all the queries of the ring are still in use). A result the driver flags as disjoint (GL_GPU_DISJOINT_EXT) is thrown away.
  //!        Without the extension the timer is not supported and the GPU time stays zero. It needs the GL context it is created in.
  class GLGpuFrameTimer final
  {
    static constexpr uint32_t QueryCount = 4;

    //! The functions of GL_EXT_disjoint_timer_query the timer uses
    struct QueryFunctions
    {
      void(GL_APIENTRYP GenQueries)(GLsizei n, GLuint* ids){nullptr};
      void(GL_APIENTRYP DeleteQueries)(GLsizei n, const GLuint* ids){nullptr};
      void(GL_APIENTRYP BeginQuery)(GLenum target, GLuint id){nullptr};
      void(GL_APIENTRYP EndQuery)(GLenum target){nullptr};
      void(GL_APIENTRYP GetQueryObjectuiv)(GLuint id, GLenum pname, GLuint* params){nullptr};
    };

    QueryFunctions m_functions;
    std::array<GLuint, QueryCount> m_queries{};
    std::array<bool, QueryCount> m_pending{};
    //! The query the next frame uses
    uint32_t m_nextQuery{0};
    //! The oldest query that may be pending
    uint32_t m_oldestQuery{0};
    bool m_isSupported{false};
    bool m_isFrameActive{false};
    TimeSpan m_gpuTime;
    uint64_t m_measurementId{0};

  public:
    GLGpuFrameTimer(const GLGpuFrameTimer&) = delete;
    GLGpuFrameTimer& operator=(const GLGpuFrameTimer&) = delete;

    GLGpuFrameTimer();
    ~GLGpuFrameTimer() noexcept;

    //! @return true if GL_EXT_disjoint_timer_query is available (if not the GPU time stays zero)
    [[nodiscard]] bool IsSupported() const noexcept
    {
      return m_isSupported;
    }

    //! @brief Call it before the first GL command of the frame. It reads the results the GPU has for the earlier frames.
    void BeginFrame();

    //! @brief Call it after the last GL command of the frame.
    void EndFrame();

    //! @return the GPU time of the last frame that was measured (zero if no frame was measured yet)
    [[nodiscard]] TimeSpan GetGpuTime() const noexcept
    {
      return m_gpuTime;
    }

    //! @return a number that grows by one for every measured frame, so a caller can tell if GetGpuTime has a new value
    [[nodiscard]] uint64_t GetMeasurementId() const noexcept
    {
      return m_measurementId;
    }
  };
}

#endif
