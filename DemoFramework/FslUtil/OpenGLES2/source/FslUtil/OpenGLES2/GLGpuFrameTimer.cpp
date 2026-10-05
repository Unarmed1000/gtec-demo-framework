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
#include <FslUtil/OpenGLES2/GLGpuFrameTimer.hpp>
#include <FslUtil/OpenGLES2/GLUtil.hpp>
#include <EGL/egl.h>

namespace Fsl::GLES2
{
  namespace
  {
    namespace LocalConfig
    {
      // The values of https://registry.khronos.org/OpenGL/extensions/EXT/EXT_disjoint_timer_query.txt
      constexpr GLenum QueryResultExt = 0x8866;
      constexpr GLenum QueryResultAvailableExt = 0x8867;
      constexpr GLenum TimeElapsedExt = 0x88BF;
      constexpr GLenum GpuDisjointExt = 0x8FBB;
      constexpr int64_t NanosecondsPerTick = 100;
    }

    //! Look up a function of the extension (nullptr if the driver does not have it)
    template <typename TFunction>
    void GetFunction(TFunction& rFunction, const char* const pszName)
    {
      rFunction = reinterpret_cast<TFunction>(eglGetProcAddress(pszName));
    }
  }


  GLGpuFrameTimer::GLGpuFrameTimer()
  {
    if (!GLUtil::HasExtension("GL_EXT_disjoint_timer_query"))
    {
      FSLLOG3_INFO("GL_EXT_disjoint_timer_query is not available, the GPU time of a frame is not measured");
      return;
    }
    GetFunction(m_functions.GenQueries, "glGenQueriesEXT");
    GetFunction(m_functions.DeleteQueries, "glDeleteQueriesEXT");
    GetFunction(m_functions.BeginQuery, "glBeginQueryEXT");
    GetFunction(m_functions.EndQuery, "glEndQueryEXT");
    GetFunction(m_functions.GetQueryObjectuiv, "glGetQueryObjectuivEXT");
    if (m_functions.GenQueries == nullptr || m_functions.DeleteQueries == nullptr || m_functions.BeginQuery == nullptr ||
        m_functions.EndQuery == nullptr || m_functions.GetQueryObjectuiv == nullptr)
    {
      FSLLOG3_INFO("The functions of GL_EXT_disjoint_timer_query are not available, the GPU time of a frame is not measured");
      return;
    }
    m_isSupported = true;

    m_functions.GenQueries(static_cast<GLsizei>(m_queries.size()), m_queries.data());
    // Clear the disjoint flag, it is set from the start on some drivers
    GLint disjoint = 0;
    glGetIntegerv(LocalConfig::GpuDisjointExt, &disjoint);
  }


  GLGpuFrameTimer::~GLGpuFrameTimer() noexcept
  {
    if (m_isSupported)
    {
      m_functions.DeleteQueries(static_cast<GLsizei>(m_queries.size()), m_queries.data());
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
      m_functions.GetQueryObjectuiv(hQuery, LocalConfig::QueryResultAvailableExt, &available);
      if (available == GL_FALSE)
      {
        break;
      }
      GLuint elapsedNanoseconds = 0;
      m_functions.GetQueryObjectuiv(hQuery, LocalConfig::QueryResultExt, &elapsedNanoseconds);
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
    m_functions.BeginQuery(LocalConfig::TimeElapsedExt, m_queries[m_nextQuery]);
    m_isFrameActive = true;
  }


  void GLGpuFrameTimer::EndFrame()
  {
    if (!m_isFrameActive)
    {
      return;
    }
    m_functions.EndQuery(LocalConfig::TimeElapsedExt);
    m_pending[m_nextQuery] = true;
    m_nextQuery = (m_nextQuery + 1) % QueryCount;
    m_isFrameActive = false;
  }
}
