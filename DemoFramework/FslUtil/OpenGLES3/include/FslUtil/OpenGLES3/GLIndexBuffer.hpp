#ifndef FSLUTIL_OPENGLES3_GLINDEXBUFFER_HPP
#define FSLUTIL_OPENGLES3_GLINDEXBUFFER_HPP
/****************************************************************************************************************************************************
 * Copyright (c) 2014 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *
 *    * Neither the name of the Freescale Semiconductor, Inc. nor the names of
 *      its contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************************************************************************************/

#include <FslBase/BasicTypes.hpp>
#include <FslBase/Span/ReadOnlySpan.hpp>
#include <FslUtil/OpenGLES3/Common.hpp>
#include <FslUtil/OpenGLES3/GLBuffer.hpp>
#include <GLES3/gl3.h>
#include <vector>

namespace Fsl::GLES3
{
  class GLIndexBuffer : public GLBuffer
  {
    GLenum m_type{0};

  public:
    //! @brief Move assignment operator
    GLIndexBuffer& operator=(GLIndexBuffer&& other) noexcept
    {
      if (this != &other)
      {
        // Claim ownership here
        m_type = other.m_type;
        // Remove the data from other
        other.m_type = 0;
        // Move the base class part
        GLBuffer::operator=(std::move(other));
      }
      return *this;
    }

    //! @brief Move constructor
    //! Transfer ownership from other to this
    GLIndexBuffer(GLIndexBuffer&& other) noexcept
      : GLBuffer(std::move(other))    // NOLINT(bugprone-use-after-move)
      , m_type(other.m_type)          // NOLINT(bugprone-use-after-move)
    {
      // Remove the data from other
      // NOLINTNEXTLINE(bugprone-use-after-move)
      other.m_type = 0;
    }


    //! @brief Create a uninitialized index buffer
    GLIndexBuffer();

    //! @brief Create a initialized index buffer
    GLIndexBuffer(const void* const pIndices, const std::size_t elementCount, const uint32_t elementStride, const GLenum usage, const GLenum type);

    //! @brief Create a initialized index buffer
    GLIndexBuffer(const uint8_t* const pIndices, const std::size_t elementCount, const GLenum usage);

    //! @brief Create a initialized index buffer
    GLIndexBuffer(const uint16_t* const pIndices, const std::size_t elementCount, const GLenum usage);

    //! @brief Create a initialized index buffer
    GLIndexBuffer(const ReadOnlySpan<uint8_t> indices, const GLenum usage);

    //! @brief Create a initialized index buffer
    GLIndexBuffer(const ReadOnlySpan<uint16_t> indices, const GLenum usage);

    //! @brief Create a initialized index buffer
    GLIndexBuffer(const std::vector<uint8_t>& indices, const GLenum usage);

    //! @brief Create a initialized index buffer
    GLIndexBuffer(const std::vector<uint16_t>& indices, const GLenum usage);

    using GLBuffer::Reset;

    //! @brief Reset the buffer to contain the supplied elements
    //! @note  This is a very slow operation and its not recommended for updating the content of the buffer (since it creates a new buffer
    //! internally)
    void Reset(const void* const pIndices, const std::size_t elementCount, const uint32_t elementStride, const GLenum usage, const GLenum type);

    //! @brief Reset the buffer to contain the supplied elements
    //! @note  This is a very slow operation and its not recommended for updating the content of the buffer (since it creates a new buffer
    //! internally)
    void Reset(const uint8_t* const pIndices, const std::size_t elementCount, const GLenum usage);

    //! @brief Reset the buffer to contain the supplied elements
    //! @note  This is a very slow operation and its not recommended for updating the content of the buffer (since it creates a new buffer
    //! internally)
    void Reset(const uint16_t* const pIndices, const std::size_t elementCount, const GLenum usage);

    //! @brief Reset the buffer to contain the supplied elements
    //! @note  This is a very slow operation and its not recommended for updating the content of the buffer (since it creates a new buffer
    //! internally)
    void Reset(const ReadOnlySpan<uint8_t>& indices, const GLenum usage);

    //! @brief Reset the buffer to contain the supplied elements
    //! @note  This is a very slow operation and its not recommended for updating the content of the buffer (since it creates a new buffer
    //! internally)
    void Reset(const ReadOnlySpan<uint16_t>& indices, const GLenum usage);

    //! @brief Reset the buffer to contain the supplied elements
    //! @note  This is a very slow operation and its not recommended for updating the content of the buffer (since it creates a new buffer
    //! internally)
    void Reset(const std::vector<uint8_t>& indices, const GLenum usage);

    //! @brief Reset the buffer to contain the supplied elements
    //! @note  This is a very slow operation and its not recommended for updating the content of the buffer (since it creates a new buffer
    //! internally)
    void Reset(const std::vector<uint16_t>& indices, const GLenum usage);

    //! @brief Get the type of the buffer content
    GLenum GetType() const
    {
      return m_type;
    }
  };
}

#endif
