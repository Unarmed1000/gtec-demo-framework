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
#include <EGL/egl.h>

namespace Fsl::GLES3
{
  namespace
  {
    namespace LocalConfig
    {
      // The values of https://registry.khronos.org/OpenGL/extensions/EXT/EXT_disjoint_timer_query.txt
      constexpr GLenum QueryCounterBitsExt = 0x8864;
      constexpr GLenum TimeElapsedExt = 0x88BF;
      constexpr GLenum TimestampExt = 0x8E28;
      constexpr GLenum GpuDisjointExt = 0x8FBB;
      constexpr int64_t NanosecondsPerTick = 100;
      //! A read of the two clocks that took longer than the one in use replaces it once that one is this old: the clocks drift apart
      constexpr TimeSpan MaxCalibrationAge(TimeSpan::FromSeconds(2));
      //! The reads of one calibration, the one that took the shortest is used
      constexpr uint32_t CalibrationReads = 3;
    }

    //! Look up a function of the extension (nullptr if the driver does not have it)
    template <typename TFunction>
    void GetFunction(TFunction& rFunction, const char* const pszName)
    {
      rFunction = reinterpret_cast<TFunction>(eglGetProcAddress(pszName));
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

    {    // The timestamps of the extension: a driver says with the bits of the counter if it has them
      GetFunction(m_timestampFunctions.QueryCounter, "glQueryCounterEXT");
      GetFunction(m_timestampFunctions.GetQueryObjectui64v, "glGetQueryObjectui64vEXT");
      GLint timestampBits = 0;
      glGetQueryiv(LocalConfig::TimestampExt, LocalConfig::QueryCounterBitsExt, &timestampBits);
      m_isEndTimeSupported = m_timestampFunctions.QueryCounter != nullptr && m_timestampFunctions.GetQueryObjectui64v != nullptr && timestampBits > 0;
      if (m_isEndTimeSupported)
      {
        glGenQueries(static_cast<GLsizei>(m_endQueries.size()), m_endQueries.data());
      }
      else
      {
        FSLLOG3_INFO("The timestamps of GL_EXT_disjoint_timer_query are not available ({} bits), the time the GPU finished a frame is not measured",
                     timestampBits);
      }
    }

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
    if (m_isEndTimeSupported)
    {
      glDeleteQueries(static_cast<GLsizei>(m_endQueries.size()), m_endQueries.data());
    }
  }


  void GLGpuFrameTimer::BeginFrame(const uint64_t frameTag)
  {
    m_newMeasurementCount = 0;
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
      const GLuint hEndQuery = m_endQueries[m_oldestQuery];
      if (m_isEndTimeSupported)
      {
        // The timestamp is taken after the last command of the frame, so it is the last of the two to arrive
        glGetQueryObjectuiv(hEndQuery, GL_QUERY_RESULT_AVAILABLE, &available);
        if (available == GL_FALSE)
        {
          break;
        }
      }
      GLuint elapsedNanoseconds = 0;
      glGetQueryObjectuiv(hQuery, GL_QUERY_RESULT, &elapsedNanoseconds);
      GLuint64 endNanoseconds = 0;
      if (m_isEndTimeSupported)
      {
        m_timestampFunctions.GetQueryObjectui64v(hEndQuery, GL_QUERY_RESULT, &endNanoseconds);
      }
      if (disjoint == 0)
      {
        m_gpuTime = TimeSpan(static_cast<int64_t>(elapsedNanoseconds) / LocalConfig::NanosecondsPerTick);
        ++m_measurementId;
        Measurement measurement{m_frameTags[m_oldestQuery], m_gpuTime, {}};
        if (m_isEndTimeSupported && m_isCalibrated)
        {
          measurement.EndTime = TickCount((static_cast<int64_t>(endNanoseconds) / LocalConfig::NanosecondsPerTick) + m_hostMinusGlTicks);
        }
        m_newMeasurements[m_newMeasurementCount] = measurement;
        ++m_newMeasurementCount;
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
    m_frameTags[m_nextQuery] = frameTag;
    m_isFrameActive = true;
  }


  void GLGpuFrameTimer::EndFrame()
  {
    if (!m_isFrameActive)
    {
      return;
    }
    glEndQuery(LocalConfig::TimeElapsedExt);
    if (m_isEndTimeSupported)
    {
      // The time is recorded once every command before it is done: when the GPU finished the frame
      m_timestampFunctions.QueryCounter(m_endQueries[m_nextQuery], LocalConfig::TimestampExt);
    }
    m_pending[m_nextQuery] = true;
    m_nextQuery = (m_nextQuery + 1) % QueryCount;
    m_isFrameActive = false;
  }


  void GLGpuFrameTimer::Calibrate()
  {
    if (!m_isEndTimeSupported)
    {
      return;
    }
    // The time of the GL is read between two reads of the clock of the framework and taken to be in the middle of them, so it can
    // be off by half the time the read took. The read that took the shortest of a few is the one that is used.
    TickCount bestBeforeTime;
    TimeSpan bestReadTime;
    int64_t bestGlNanoseconds = 0;
    for (uint32_t i = 0; i < LocalConfig::CalibrationReads; ++i)
    {
      const TickCount beforeTime = m_timer.GetTimestamp();
      GLint64 glNanoseconds = 0;
      glGetInteger64v(LocalConfig::TimestampExt, &glNanoseconds);
      const TimeSpan readTime = m_timer.GetTimestamp() - beforeTime;
      if (glNanoseconds > 0 && (bestGlNanoseconds == 0 || readTime < bestReadTime))
      {
        bestBeforeTime = beforeTime;
        bestReadTime = readTime;
        bestGlNanoseconds = glNanoseconds;
      }
    }
    if (bestGlNanoseconds == 0)
    {
      return;
    }
    // A read that took longer than the one in use only replaces it once that one got old
    const TickCount afterTime = bestBeforeTime + bestReadTime;
    if (!m_isCalibrated || bestReadTime <= m_calibrationReadTime || (afterTime - m_calibrationTime) >= LocalConfig::MaxCalibrationAge)
    {
      m_hostMinusGlTicks = (bestBeforeTime.Ticks() + (bestReadTime.Ticks() / 2)) - (bestGlNanoseconds / LocalConfig::NanosecondsPerTick);
      m_calibrationReadTime = bestReadTime;
      m_calibrationTime = afterTime;
      m_isCalibrated = true;
      ++m_calibrationId;
    }
  }
}
