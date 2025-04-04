/****************************************************************************************************************************************************
 * Copyright 2024 NXP
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
 *    * Neither the name of the NXP. nor the names of
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

#include <FslUtil/OpenGLES3/Exceptions.hpp>
#include <FslUtil/OpenGLES3/GLCheck.hpp>
#include <FslUtil/OpenGLES3/GLMultisampleFrameBufferTextureExt.hpp>
#include <FslUtil/OpenGLES3/GLUtil.hpp>
#include <EGL/egl.h>
#include <GLES2/gl2ext.h>
#include <algorithm>
#include <utility>

namespace Fsl::GLES3
{
  namespace
  {
    using PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC = void(GL_APIENTRYP)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
                                                                           GLint level, GLsizei samples);
    using PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC = void(GL_APIENTRYP)(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width,
                                                                          GLsizei height);


    extern "C" void GL_APIENTRY DebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message,
                                              const void* userParam)
    {
      FSLLOG3_INFO("Debug Message: {0}", message);
    }
  }


  // move assignment operator
  GLMultisampleFrameBufferTextureExt& GLMultisampleFrameBufferTextureExt::operator=(GLMultisampleFrameBufferTextureExt&& other) noexcept
  {
    if (this != &other)
    {
      // Free existing resources then transfer the content of other to this one and fill other with default values
      Reset();

      // Claim ownership here
      m_handle = other.m_handle;
      m_size = other.m_size;
      m_texture = std::move(other.m_texture);
      m_depthStencilRenderBuffer = std::move(other.m_depthStencilRenderBuffer);

      // Remove the data from other
      other.m_handle = GLValues::InvalidHandle;
      other.m_size = {};
    }
    return *this;
  }


  // Transfer ownership from other to this
  GLMultisampleFrameBufferTextureExt::GLMultisampleFrameBufferTextureExt(GLMultisampleFrameBufferTextureExt&& other) noexcept
    : m_handle(other.m_handle)
    , m_size(other.m_size)
    , m_texture(std::move(other.m_texture))
    , m_depthStencilRenderBuffer(std::move(other.m_depthStencilRenderBuffer))
  {
    // Remove the data from other
    other.m_handle = GLValues::InvalidHandle;
    other.m_size = {};
  }


  GLMultisampleFrameBufferTextureExt::GLMultisampleFrameBufferTextureExt()
    : m_handle(GLValues::InvalidHandle)
  {
  }

  GLMultisampleFrameBufferTextureExt::GLMultisampleFrameBufferTextureExt(const PxSize2D& size, const GLsizei maxSamples,
                                                                         const GLES3::GLTextureImageParameters texImageParams,
                                                                         const GLenum depthBufferFormat)
    : GLMultisampleFrameBufferTextureExt()
  {
    Reset(size, maxSamples, texImageParams, depthBufferFormat);
  }


  GLMultisampleFrameBufferTextureExt::~GLMultisampleFrameBufferTextureExt()
  {
    Reset();
  }


  void GLMultisampleFrameBufferTextureExt::Reset() noexcept
  {
    if (m_handle != GLValues::InvalidHandle)
    {
      glDeleteFramebuffers(1, &m_handle);
      m_handle = GLValues::InvalidHandle;
      m_size = {};
      m_texture.Reset();
      m_depthStencilRenderBuffer.Reset();
    }
  }


  void GLMultisampleFrameBufferTextureExt::Reset(const PxSize2D& size, const GLsizei maxSamples, const GLES3::GLTextureImageParameters texImageParams,
                                                 const GLenum depthBufferFormat)
  {
    if (m_handle != GLValues::InvalidHandle)
    {
      Reset();
    }

    auto glFramebufferTexture2DMultisampleEXT =
      reinterpret_cast<PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC>(eglGetProcAddress("glFramebufferTexture2DMultisampleEXT"));
    if (glFramebufferTexture2DMultisampleEXT == nullptr)
    {
      throw NotSupportedException("glFramebufferTexture2DMultisampleEXT not supported");
    }
    auto glRenderbufferStorageMultisampleEXT =
      reinterpret_cast<PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC>(eglGetProcAddress("glRenderbufferStorageMultisampleEXT"));
    if (glRenderbufferStorageMultisampleEXT == nullptr)
    {
      throw NotSupportedException("glRenderbufferStorageMultisampleEXT not supported");
    }

    m_size = size;

    // Prepare a texture of the right size and format
    {
      GLuint hColorTexture{0};
      GL_CHECK(glGenTextures(1, &hColorTexture));
      m_texture.Reset(hColorTexture, size);
    }
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, m_texture.Get()));
    GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, texImageParams.InternalFormat, size.RawWidth(), size.RawHeight(), 0, texImageParams.Format,
                          texImageParams.Type, nullptr));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));

    {    // Create the depth stencil depth buffer
      GLuint hDepthbuffer{0};
      GL_CHECK(glGenRenderbuffers(1, &hDepthbuffer));
      m_depthStencilRenderBuffer.Reset(hDepthbuffer, size, depthBufferFormat);
    }

    GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, m_depthStencilRenderBuffer.Get()));
    GL_CHECK(glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, maxSamples, depthBufferFormat, size.RawWidth(), size.RawHeight()));
    GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, 0));

    // Create the framebuffer and bind it.
    GL_CHECK(glGenFramebuffers(1, &m_handle));
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, m_handle));

    switch (depthBufferFormat)
    {
    case GL_DEPTH_COMPONENT16:
    case GL_DEPTH_COMPONENT24:
    case GL_DEPTH_COMPONENT32F:
      GL_CHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilRenderBuffer.Get()));
      break;
    case GL_DEPTH24_STENCIL8:
    case GL_DEPTH32F_STENCIL8:
      GL_CHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilRenderBuffer.Get()));
      break;
    case GL_STENCIL_INDEX8:
      GL_CHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilRenderBuffer.Get()));
      break;
    default:
      throw NotSupportedException("depthBufferFormat not supported");
    }

    GL_CHECK(glFramebufferTexture2DMultisampleEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture.Get(), 0, maxSamples));

    // Test the frame-buffer for completeness. This test only needs to be performed when the framebuffer's configuration changes.
    const auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
      throw GLESGraphicsException("glCheckFramebufferStatus", UncheckedNumericCast<int32_t>(status), __FILE__, __LINE__);
    }

    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
  }
}
