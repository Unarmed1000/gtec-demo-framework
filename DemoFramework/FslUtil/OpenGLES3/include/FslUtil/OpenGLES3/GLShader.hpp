#ifndef FSLUTIL_OPENGLES3_GLSHADER_HPP
#define FSLUTIL_OPENGLES3_GLSHADER_HPP
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

#include <FslBase/Attributes.hpp>
#include <FslUtil/OpenGLES3/Common.hpp>
#include <FslUtil/OpenGLES3/GLValues.hpp>
#include <GLES3/gl3.h>
#include <string>

namespace Fsl::GLES3
{
  class GLShader
  {
    GLint m_shaderType{0};
    GLuint m_handle;

  public:
    GLShader(const GLShader&) = delete;
    GLShader& operator=(const GLShader&) = delete;

    //! @brief Move assignment operator
    GLShader& operator=(GLShader&& other) noexcept
    {
      if (this != &other)
      {
        // Free existing resources then transfer the content of other to this one and fill other with default values
        if (IsValid())
        {
          Reset();
        }

        // Claim ownership here
        m_shaderType = other.m_shaderType;
        m_handle = other.m_handle;

        // Remove the data from other
        other.m_shaderType = 0;
        other.m_handle = GLValues::InvalidHandle;
      }
      return *this;
    }

    //! @brief Move constructor
    //! Transfer ownership from other to this
    GLShader(GLShader&& other) noexcept
      : m_shaderType(other.m_shaderType)
      , m_handle(other.m_handle)
    {
      // Remove the data from other
      other.m_shaderType = 0;
      other.m_handle = GLValues::InvalidHandle;
    }

    GLShader();
    GLShader(const GLint shaderType, const std::string& strShaderCode);
    GLShader(const GLint shaderType, const char* const pszShaderCode);
    ~GLShader() noexcept;

    //! @brief Check if this contains a valid gl handle.
    bool IsValid() const noexcept
    {
      return m_handle != GLValues::InvalidHandle;
    }

    void Reset() noexcept;
    void Reset(const GLint shaderType, const std::string& strShaderCode)
    {
      Reset(shaderType, strShaderCode.c_str());
    }

    void Reset(const GLint shaderType, const char* const pszShaderCode);

    GLint GetType() const noexcept
    {
      return m_shaderType;
    }

    //! @brief Get the gl handle associated with the shader.
    //! @return the handle or GLValues::InvalidHandle if the buffer is unallocated.
    GLuint Get() const noexcept
    {
      return m_handle;
    }

    //! @brief Get the gl handle associated with the shader.
    //! @return the handle or GLValues::InvalidHandle if the buffer is unallocated.
    [[deprecated("use one of the other overloads instead")]] GLuint GetHandle() const
    {
      return Get();
    }
  };
}

#endif
