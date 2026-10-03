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
#include <FslUtil/OpenGLES3/GLGpuFrameTimer.hpp>
#include <FslUtil/OpenGLES3/GLUtil.hpp>

namespace Fsl::GLES3
{
  namespace
  {
    namespace LocalConfig
    {
      // The values of https://registry.khronos.org/OpenGL/extensions/EXT/EXT_disjoint_timer_query.txt
      constexpr GLenum TimeElapsedExt = 0x88BF;
      constexpr GLenum GpuDisjointExt = 0x8FBB;
      constexpr int64_t NanosecondsPerTick = 100;
    }
  }


  GLGpuFrameTimer::GLGpuFrameTimer()
    : m_isSupported(GLUtil::HasExtension("GL_EXT_disjoint_timer_query"))
  {
    if (!m_isSupported)
    {
      FSLLOG3_INFO("GL_EXT_disjoint_timer_query is not available, the GPU time of a frame is not measured");
      return;
    }
    glGenQueries(static_cast<GLsizei>(m_queries.size()), m_queries.data());
    // Clear the disjoint flag, it is set from the start on some drivers
    GLint disjoint = 0;
    glGetIntegerv(LocalConfig::GpuDisjointExt, &disjoint);
  }


  GLGpuFrameTimer::~GLGpuFrameTimer() noexcept
  {
    if (m_isSupported)
    {
      glDeleteQueries(static_cast<GLsizei>(m_queries.size()), m_queries.data());
    }
  }


  void GLGpuFrameTimer::BeginFrame()
  {
    if (!m_isSupported || m_isFrameActive)
    {
      return;
    }

    // A result is only valid if the GPU timer was not disjoint since it was started (reading the flag clears it)
    GLint disjoint = 0;
    glGetIntegerv(LocalConfig::GpuDisjointExt, &disjoint);

    // Read the results the GPU has, oldest first
    while (m_pending[m_oldestQuery])
    {
      const GLuint hQuery = m_queries[m_oldestQuery];
      GLuint available = GL_FALSE;
      glGetQueryObjectuiv(hQuery, GL_QUERY_RESULT_AVAILABLE, &available);
      if (available == GL_FALSE)
      {
        break;
      }
      GLuint elapsedNanoseconds = 0;
      glGetQueryObjectuiv(hQuery, GL_QUERY_RESULT, &elapsedNanoseconds);
      if (disjoint == 0)
      {
        m_gpuTime = TimeSpan(static_cast<int64_t>(elapsedNanoseconds) / LocalConfig::NanosecondsPerTick);
        ++m_measurementId;
      }
      m_pending[m_oldestQuery] = false;
      m_oldestQuery = (m_oldestQuery + 1) % QueryCount;
    }

    // All the queries are still in use: this frame is not measured
    if (m_pending[m_nextQuery])
    {
      return;
    }
    glBeginQuery(LocalConfig::TimeElapsedExt, m_queries[m_nextQuery]);
    m_isFrameActive = true;
  }


  void GLGpuFrameTimer::EndFrame()
  {
    if (!m_isFrameActive)
    {
      return;
    }
    glEndQuery(LocalConfig::TimeElapsedExt);
    m_pending[m_nextQuery] = true;
    m_nextQuery = (m_nextQuery + 1) % QueryCount;
    m_isFrameActive = false;
  }
}
