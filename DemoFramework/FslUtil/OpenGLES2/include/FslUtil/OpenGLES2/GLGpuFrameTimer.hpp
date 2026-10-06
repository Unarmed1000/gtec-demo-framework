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
#include <FslBase/System/HighResolutionTimer.hpp>
#include <FslBase/Time/TickCount.hpp>
#include <FslBase/Time/TimeSpan.hpp>
#include <FslUtil/OpenGLES2/Common.hpp>
#include <GLES2/gl2.h>
#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace Fsl::GLES2
{
  //! @brief Measures the time the GPU needs for a frame with a GL_EXT_disjoint_timer_query time elapsed query around the commands of the
  //!        frame, and when the GPU finished the frame with a timestamp query of the same extension after its last command.
  //! @note  OpenGL ES 2 has no query objects of its own, so the functions of the extension are used (they are looked up when the timer
  //!        is created). The queries are a ring, a result is read once the GPU has it, so reading never waits (a frame is not measured if
  //!        all the queries of the ring are still in use). A result the driver flags as disjoint (GL_GPU_DISJOINT_EXT) is thrown away.
  //!        Without the extension the timer is not supported and the GPU time stays zero. It needs the GL context it is created in.
  //! @note  A driver can have the extension and no timestamps (it says so with zero bits for the counter): the time the GPU finished
  //!        a frame is then not measured (IsEndTimeSupported). The timestamp is on the clock of the GL, which is related to the clock
  //!        of the framework by reading both (Calibrate).
  class GLGpuFrameTimer final
  {
    static constexpr uint32_t QueryCount = 4;

  public:
    //! The GPU time of one measured frame
    struct Measurement
    {
      //! The tag the frame was given when it began (BeginFrame)
      uint64_t FrameTag{0};
      TimeSpan GpuTime;
      //! When the GPU finished the frame: all its commands were done, on a tile based GPU also the copy out of the tile memory. A
      //! HighResolutionTimer timestamp. Empty if the driver has no timestamps or the clocks were not related yet (Calibrate).
      std::optional<TickCount> EndTime;
    };

  private:
    //! The functions of GL_EXT_disjoint_timer_query the timer uses
    struct QueryFunctions
    {
      void(GL_APIENTRYP GenQueries)(GLsizei n, GLuint* ids){nullptr};
      void(GL_APIENTRYP DeleteQueries)(GLsizei n, const GLuint* ids){nullptr};
      void(GL_APIENTRYP BeginQuery)(GLenum target, GLuint id){nullptr};
      void(GL_APIENTRYP EndQuery)(GLenum target){nullptr};
      void(GL_APIENTRYP GetQueryObjectuiv)(GLuint id, GLenum pname, GLuint* params){nullptr};
    };

    //! The functions of GL_EXT_disjoint_timer_query the timestamps need
    struct TimestampFunctions
    {
      void(GL_APIENTRYP GetQueryiv)(GLenum target, GLenum pname, GLint* params){nullptr};
      void(GL_APIENTRYP QueryCounter)(GLuint id, GLenum target){nullptr};
      void(GL_APIENTRYP GetQueryObjectui64v)(GLuint id, GLenum pname, uint64_t* params){nullptr};
      void(GL_APIENTRYP GetInteger64v)(GLenum pname, int64_t* data){nullptr};
    };

    QueryFunctions m_functions;
    TimestampFunctions m_timestampFunctions;
    HighResolutionTimer m_timer;
    std::array<GLuint, QueryCount> m_queries{};
    //! The timestamp query of each frame, issued after its last command
    std::array<GLuint, QueryCount> m_endQueries{};
    bool m_isEndTimeSupported{false};
    //! The time of the framework minus the time of the GL in ticks (valid if m_isCalibrated), how long the read took that it is from
    //! and when that was
    int64_t m_hostMinusGlTicks{0};
    TimeSpan m_calibrationReadTime;
    TickCount m_calibrationTime;
    bool m_isCalibrated{false};
    uint64_t m_calibrationId{0};
    std::array<bool, QueryCount> m_pending{};
    //! The tag of the frame each query measures
    std::array<uint64_t, QueryCount> m_frameTags{};
    //! The results the last BeginFrame read, oldest first
    std::array<Measurement, QueryCount> m_newMeasurements{};
    uint32_t m_newMeasurementCount{0};
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
    //! @param frameTag a number of the caller that says which frame this is. A result arrives one or more frames later and is
    //!        reported with the tag of the frame it was measured on (GetNewMeasurements).
    void BeginFrame(const uint64_t frameTag = 0);

    //! @return true if the driver has timestamps, so the time the GPU finished a frame is measured (Measurement::EndTime)
    [[nodiscard]] bool IsEndTimeSupported() const noexcept
    {
      return m_isEndTimeSupported;
    }

    //! @brief Call it after the last GL command of the frame.
    void EndFrame();

    //! @brief Relate the clock of the GL to the clock of the framework by reading both, so a timestamp of the GL can be given as a
    //!        HighResolutionTimer timestamp. Call it now and then, the clocks drift apart. Reading the time of the GL sends the
    //!        commands that were issued so far to the GL server and returns after that, so the place for the call is the start of
    //!        a frame, before its first command: there is nothing to send, the read is short and it changes nothing about when the
    //!        GPU is given the frame. It does nothing without timestamps.
    void Calibrate();

    //! @return a number that grows by one every time Calibrate related the two clocks anew
    [[nodiscard]] uint64_t GetCalibrationId() const noexcept
    {
      return m_calibrationId;
    }

    //! @return how long the read of the time of the GL took that the clocks are related by: the time of the GL can be off by up
    //!         to half of it (zero if the clocks were not related yet)
    [[nodiscard]] TimeSpan GetCalibrationReadTime() const noexcept
    {
      return m_calibrationReadTime;
    }

    //! @return the GPU time of the last frame that was measured (zero if no frame was measured yet)
    [[nodiscard]] TimeSpan GetGpuTime() const noexcept
    {
      return m_gpuTime;
    }

    //! @return the results the last BeginFrame read, oldest first, each with the tag of the frame it was measured on (empty if none
    //!         arrived). The last one is the one GetGpuTime returns.
    [[nodiscard]] std::span<const Measurement> GetNewMeasurements() const noexcept
    {
      return {m_newMeasurements.data(), m_newMeasurementCount};
    }

    //! @return a number that grows by one for every measured frame, so a caller can tell if GetGpuTime has a new value
    [[nodiscard]] uint64_t GetMeasurementId() const noexcept
    {
      return m_measurementId;
    }
  };
}

#endif
