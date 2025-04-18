/****************************************************************************************************************************************************
 * Copyright (c) 2016 Freescale Semiconductor, Inc.
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

#include "BloomRender.hpp"
#include <FslBase/IO/Path.hpp>
#include <FslBase/Math/MathHelper.hpp>
#include <FslBase/Math/MatrixConverter.hpp>
#include <FslBase/Span/SpanUtil_Array.hpp>
#include <FslDemoService/Graphics/IGraphicsService.hpp>
#include <FslGraphics/Bitmap/Bitmap.hpp>
#include <FslGraphics/Colors.hpp>
#include <FslGraphics/Vertices/VertexPositionNormalTexture.hpp>
#include <FslUtil/OpenGLES3/Exceptions.hpp>
#include <FslUtil/OpenGLES3/GLCheck.hpp>
#include <GLES3/gl3.h>
#include <iostream>
#include "../IScene.hpp"
#include "GaussianShaderBuilder.hpp"
#include "VBHelper.hpp"


namespace Fsl
{
  using namespace GLES3;

  namespace
  {
    namespace LocalCfg
    {
      constexpr GLTextureParameters DefaultTextureParams(GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

      constexpr PxSize1D SizeMod = PxSize1D::Create(2);
      constexpr PxSize1D Size16 = PxSize1D::Create(16) * SizeMod;
      constexpr PxSize1D Size32 = PxSize1D::Create(32) * SizeMod;
      constexpr PxSize1D Size64 = PxSize1D::Create(64) * SizeMod;
      constexpr PxSize1D Size128 = PxSize1D::Create(128) * SizeMod;
      constexpr PxSize1D Size256 = PxSize1D::Create(256) * SizeMod;
    }

    constexpr std::array<GLES3::GLBindAttribLocation, 2> ShaderAttributeArray = {GLES3::GLBindAttribLocation(0, "VertexPosition"),
                                                                                 GLES3::GLBindAttribLocation(1, "VertexTexCoord")};

    constexpr GLTextureImageParameters DefaultFbImageParams(GL_RGB, GL_RGB, GL_UNSIGNED_BYTE);
  }

  // Bloom as described here
  // The idea is not to create the most accurate bloom, but something that is fairly fast.
  // http://prideout.net/archive/bloom/
  // http://kalogirou.net/2006/05/20/how-to-do-good-bloom-for-hdr-rendering/

  BloomRender::BloomRender(const DemoAppConfig& config)
    : m_screenResolution(config.ScreenResolution)
    , m_batch(std::dynamic_pointer_cast<NativeBatch2D>(config.DemoServiceProvider.Get<IGraphicsService>()->GetNativeBatch2D()))
    , m_rotationSpeed(0, -0.6f, 0)
    , m_fbBlur16A(PxSize2D(LocalCfg::Size16, LocalCfg::Size16), LocalCfg::DefaultTextureParams, DefaultFbImageParams)
    , m_fbBlur16B(PxSize2D(LocalCfg::Size16, LocalCfg::Size16), LocalCfg::DefaultTextureParams, DefaultFbImageParams)
    , m_fbBlur32A(PxSize2D(LocalCfg::Size32, LocalCfg::Size32), LocalCfg::DefaultTextureParams, DefaultFbImageParams)
    , m_fbBlur32B(PxSize2D(LocalCfg::Size32, LocalCfg::Size32), LocalCfg::DefaultTextureParams, DefaultFbImageParams)
    , m_fbBlur64A(PxSize2D(LocalCfg::Size64, LocalCfg::Size64), LocalCfg::DefaultTextureParams, DefaultFbImageParams)
    , m_fbBlur64B(PxSize2D(LocalCfg::Size64, LocalCfg::Size64), LocalCfg::DefaultTextureParams, DefaultFbImageParams)
    , m_fbBlur128A(PxSize2D(LocalCfg::Size128, LocalCfg::Size128), LocalCfg::DefaultTextureParams, DefaultFbImageParams)
    , m_fbBlur128B(PxSize2D(LocalCfg::Size128, LocalCfg::Size128), LocalCfg::DefaultTextureParams, DefaultFbImageParams)
    , m_fbBlur256A(PxSize2D(LocalCfg::Size256, LocalCfg::Size256), LocalCfg::DefaultTextureParams, DefaultFbImageParams)
    , m_fbBlur256B(PxSize2D(LocalCfg::Size256, LocalCfg::Size256), LocalCfg::DefaultTextureParams, DefaultFbImageParams)
    , m_fbRender256(PxSize2D(LocalCfg::Size256, LocalCfg::Size256), LocalCfg::DefaultTextureParams, DefaultFbImageParams, GL_DEPTH_COMPONENT16)
    , m_locBlurHTexSize(GLValues::InvalidLocation)
    , m_locBlurVTexSize(GLValues::InvalidLocation)
    , m_locBloomTexture256(GLValues::InvalidLocation)
    , m_locBloomTexture128(GLValues::InvalidLocation)
    , m_locBloomTexture64(GLValues::InvalidLocation)
    , m_locBloomTexture32(GLValues::InvalidLocation)
    , m_locBloomTexture16(GLValues::InvalidLocation)
    , m_locBloomTextureLevel(GLValues::InvalidLocation)
  //, m_renderUI(true)
  {
    m_storedStartRotation = m_rotation;

    VBHelper::BuildVB(m_vbFullScreen, BoxF(-1, -1, 1, 1), BoxF(0.0f, 0.0f, 1.0f, 1.0f));

    const auto contentManager = config.DemoServiceProvider.Get<IContentManager>();

    m_strShaderVertPass = contentManager->ReadAllText("Shaders/Bloom/Pass.vert");

    constexpr auto ShaderAttributeSpan = SpanUtil::AsReadOnlySpan(ShaderAttributeArray);
    m_programBrightPass.Reset(m_strShaderVertPass, contentManager->ReadAllText("Shaders/Bloom/BrightPass.frag"), ShaderAttributeSpan);
    m_programCopy.Reset(m_strShaderVertPass, contentManager->ReadAllText("Shaders/Bloom/CopyPass.frag"), ShaderAttributeSpan);
    m_programBloomPass.Reset(m_strShaderVertPass, contentManager->ReadAllText("Shaders/Bloom/BloomPass.frag"), ShaderAttributeSpan);

    // Prepare the blur shader
    {
      m_programBlurHPass.Reset(m_strShaderVertPass,
                               GaussianShaderBuilder::Build5x5(contentManager->ReadAllText("Shaders/Bloom/GaussianTemplate5HPass.frag"), 1.0f),
                               ShaderAttributeSpan);
      m_programBlurVPass.Reset(m_strShaderVertPass,
                               GaussianShaderBuilder::Build5x5(contentManager->ReadAllText("Shaders/Bloom/GaussianTemplate5VPass.frag"), 1.0f),
                               ShaderAttributeSpan);
      m_locBlurHTexSize = glGetUniformLocation(m_programBlurHPass.Get(), "TexSize");
      m_locBlurVTexSize = glGetUniformLocation(m_programBlurVPass.Get(), "TexSize");
    }

    m_locBloomTexture256 = glGetUniformLocation(m_programBloomPass.Get(), "Texture256");
    m_locBloomTexture128 = glGetUniformLocation(m_programBloomPass.Get(), "Texture128");
    m_locBloomTexture64 = glGetUniformLocation(m_programBloomPass.Get(), "Texture64");
    m_locBloomTexture32 = glGetUniformLocation(m_programBloomPass.Get(), "Texture32");
    m_locBloomTexture16 = glGetUniformLocation(m_programBloomPass.Get(), "Texture16");
    m_locBloomTextureLevel = glGetUniformLocation(m_programBloomPass.Get(), "Level");

    GL_CHECK_FOR_ERROR();
  }


  BloomRender::~BloomRender() = default;


  void BloomRender::Update(const DemoTime& /*demoTime*/)
  {
  }


  void BloomRender::Draw(IScene& scene)
  {
    glEnable(GL_DEPTH_TEST);

    // 1. Render the scene to a low res frame buffer
    {
      auto& fb = m_fbRender256;
      glBindFramebuffer(GL_FRAMEBUFFER, fb.Get());
      glViewport(0, 0, fb.GetSize().RawWidth(), fb.GetSize().RawHeight());

      scene.Draw();
    }

    // Since we are only doing opaque 2d-composition type operations we can disable blend and depth testing
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    // 2. Apply bright pass
    if (m_config.IsBrightPassEnabled)
    {
      PostProcess(m_fbBlur256A, m_fbRender256, m_programBrightPass);
    }
    else
    {
      PostProcess(m_fbBlur256A, m_fbRender256, m_programCopy);
    }

    // 3. copy to the smaller blur render targets
    if (m_config.IsScaleInputSequentiallyEnabled)
    {
      PostProcess(m_fbBlur128A, m_fbBlur256A, m_programCopy);
      PostProcess(m_fbBlur64A, m_fbBlur128A, m_programCopy);
      PostProcess(m_fbBlur32A, m_fbBlur64A, m_programCopy);
      PostProcess(m_fbBlur16A, m_fbBlur32A, m_programCopy);
    }
    else
    {
      PostProcess(m_fbBlur128A, m_fbBlur256A, m_programCopy);
      PostProcess(m_fbBlur64A, m_fbBlur256A, m_programCopy);
      PostProcess(m_fbBlur32A, m_fbBlur256A, m_programCopy);
      PostProcess(m_fbBlur16A, m_fbBlur256A, m_programCopy);
    }

    if (m_config.IsBlurEnabled)
    {
      // 4A. Blur the content X
      PostProcessBlurH(m_fbBlur256B, m_fbBlur256A);
      PostProcessBlurH(m_fbBlur128B, m_fbBlur128A);
      PostProcessBlurH(m_fbBlur64B, m_fbBlur64A);
      PostProcessBlurH(m_fbBlur32B, m_fbBlur32A);
      PostProcessBlurH(m_fbBlur16B, m_fbBlur16A);

      // 4B. Blur the content Y
      PostProcessBlurV(m_fbBlur256A, m_fbBlur256B);
      PostProcessBlurV(m_fbBlur128A, m_fbBlur128B);
      PostProcessBlurV(m_fbBlur64A, m_fbBlur64B);
      PostProcessBlurV(m_fbBlur32A, m_fbBlur32B);
      PostProcessBlurV(m_fbBlur16A, m_fbBlur16B);
    }

    DrawFinalComposite(scene);
  }

  void BloomRender::DrawFinalComposite(IScene& scene)
  {
    // Composite everything
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_screenResolution.X, m_screenResolution.Y);

    if (m_config.IsFinalSceneEnabled)
    {
      scene.Draw();
    }
    else
    {
      glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    // Draw bloom with a fullscreen additive pass
    if (m_config.IsFinalBloomEnabled)
    {
      glDisable(GL_DEPTH_TEST);
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE);

      auto& vb = m_vbFullScreen;

      glUseProgram(m_programBloomPass.Get());
      glUniform1i(m_locBloomTexture256, 0);
      glUniform1i(m_locBloomTexture128, 1);
      glUniform1i(m_locBloomTexture64, 2);
      glUniform1i(m_locBloomTexture32, 3);
      glUniform1i(m_locBloomTexture16, 4);
      glUniform1f(m_locBloomTextureLevel, m_config.BlendLevel);


      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, m_fbBlur256A.GetTextureInfo().Handle);
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, m_fbBlur128A.GetTextureInfo().Handle);
      glActiveTexture(GL_TEXTURE2);
      glBindTexture(GL_TEXTURE_2D, m_fbBlur64A.GetTextureInfo().Handle);
      glActiveTexture(GL_TEXTURE3);
      glBindTexture(GL_TEXTURE_2D, m_fbBlur32A.GetTextureInfo().Handle);
      glActiveTexture(GL_TEXTURE4);
      glBindTexture(GL_TEXTURE_2D, m_fbBlur16A.GetTextureInfo().Handle);

      glBindBuffer(vb.GetTarget(), vb.Get());
      vb.EnableAttribArrays();
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      vb.DisableAttribArrays();
    }

    // Draw some debug overlays
    if (m_config.IsShowBuffersEnabled)
    {
      int32_t dstX = 0;
      m_batch->Begin(BlendState::Opaque);
      m_batch->Draw(m_fbRender256, Vector2(dstX, 0), Colors::White());
      dstX += m_fbRender256.GetSize().RawWidth();
      m_batch->Draw(m_fbBlur256A, Vector2(dstX, 0), Colors::White());
      dstX += m_fbBlur256A.GetSize().RawWidth();
      m_batch->Draw(m_fbBlur128A, Vector2(dstX, 0), Colors::White());
      dstX += m_fbBlur128A.GetSize().RawWidth();
      m_batch->Draw(m_fbBlur64A, Vector2(dstX, 0), Colors::White());
      dstX += m_fbBlur64A.GetSize().RawWidth();
      m_batch->Draw(m_fbBlur32A, Vector2(dstX, 0), Colors::White());
      dstX += m_fbBlur32A.GetSize().RawWidth();
      m_batch->Draw(m_fbBlur16A, Vector2(dstX, 0), Colors::White());
      // dstX += m_fbBlur16A.GetSize().X;
      m_batch->End();
    }
  }


  void BloomRender::PostProcessBlurH(const GLFrameBuffer& dst, const GLFrameBuffer& src)
  {
    glUseProgram(m_programBlurHPass.Get());
    // glUseProgram(m_programCopy.Get());
    if (m_locBlurHTexSize >= 0)
    {
      glUniform1f(m_locBlurHTexSize, 1.0f / static_cast<float>(src.GetSize().RawWidth()));
    }
    PostProcess(dst, src);
  }


  void BloomRender::PostProcessBlurV(const GLFrameBuffer& dst, const GLFrameBuffer& src)
  {
    glUseProgram(m_programBlurVPass.Get());
    // glUseProgram(m_programCopy.Get());
    if (m_locBlurVTexSize >= 0)
    {
      glUniform1f(m_locBlurVTexSize, 1.0f / static_cast<float>(src.GetSize().RawHeight()));
    }
    PostProcess(dst, src);
  }


  void BloomRender::PostProcess(const GLFrameBuffer& dst, const GLFrameBuffer& src, const GLProgram& program)
  {
    glUseProgram(program.Get());
    PostProcess(dst, src);
  }


  void BloomRender::PostProcess(const GLFrameBuffer& dst, const GLFrameBuffer& src)
  {
    const auto& fb = dst;
    auto& vb = m_vbFullScreen;
    glBindFramebuffer(GL_FRAMEBUFFER, fb.Get());
    glViewport(0, 0, fb.GetSize().RawWidth(), fb.GetSize().RawHeight());
    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src.GetTextureInfo().Handle);

    glBindBuffer(vb.GetTarget(), vb.Get());
    vb.EnableAttribArrays();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    vb.DisableAttribArrays();
  }
}
