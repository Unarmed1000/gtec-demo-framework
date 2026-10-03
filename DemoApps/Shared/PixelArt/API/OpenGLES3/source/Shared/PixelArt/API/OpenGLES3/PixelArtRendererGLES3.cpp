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
#include <FslBase/Span/SpanUtil_Array.hpp>
#include <FslDemoApp/Base/Service/Content/IContentManager.hpp>
#include <FslGraphics/Bitmap/ReadOnlyRawBitmap.hpp>
#include <FslGraphics/Texture/Texture.hpp>
#include <FslUtil/OpenGLES3/GLCheck.hpp>
#include <FslUtil/OpenGLES3/GLTextureParameters.hpp>
#include <FslUtil/OpenGLES3/GLTextureParameters3.hpp>
#include <Shared/PixelArt/API/OpenGLES3/PixelArtRendererGLES3.hpp>
#include <Shared/PixelArt/Base/PixelArtSceneLoader.hpp>
#include <GLES3/gl3.h>
#include <fmt/format.h>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

namespace Fsl
{
  using namespace GLES3;

  namespace
  {
    namespace LocalConfig
    {
      constexpr IO::PathView VertexShader("PixelArtGLES3Vertex.glsl");
      constexpr IO::PathView Prologue("PixelArtGLES3.glsl");
      constexpr IO::PathView CommonTab("Common.glsl");
      constexpr IO::PathView ParamsFile("Params.glsl");
      constexpr uint32_t MaxIncludeDepth = 8;
    }

    uint32_t ToSamplerIndex(const PixelArtFilter filter, const PixelArtWrap wrap) noexcept
    {
      return (static_cast<uint32_t>(filter) * 2u) + static_cast<uint32_t>(wrap);
    }

    //! Remove "dir/.." and "." from a path in the content folder
    std::string NormalizePath(const std::string& path)
    {
      std::vector<std::string> parts;
      std::size_t start = 0;
      while (start <= path.size())
      {
        std::size_t end = path.find('/', start);
        if (end == std::string::npos)
        {
          end = path.size();
        }
        const std::string part = path.substr(start, end - start);
        if (part == "..")
        {
          if (!parts.empty())
          {
            parts.pop_back();
          }
        }
        else if (!part.empty() && part != ".")
        {
          parts.push_back(part);
        }
        start = end + 1;
      }
      std::string result;
      for (const std::string& part : parts)
      {
        if (!result.empty())
        {
          result += '/';
        }
        result += part;
      }
      return result;
    }

    //! Replace every line '#include "file"' with the file, the path is relative to the folder of the file that includes it (like glslang
    //! does for the Vulkan shaders)
    std::string ResolveIncludes(const IContentManager& contentManager, const std::string& text, const std::string& filePath, const uint32_t depth)
    {
      if (depth > LocalConfig::MaxIncludeDepth)
      {
        throw std::runtime_error(fmt::format("{}: the includes are nested too deep", filePath));
      }
      const std::size_t folderEnd = filePath.rfind('/');
      const std::string folder = folderEnd == std::string::npos ? std::string() : filePath.substr(0, folderEnd + 1);

      std::string result;
      std::size_t lineStart = 0;
      while (lineStart < text.size())
      {
        std::size_t lineEnd = text.find('\n', lineStart);
        if (lineEnd == std::string::npos)
        {
          lineEnd = text.size();
        }
        const std::string line = text.substr(lineStart, lineEnd - lineStart);
        const std::size_t firstChar = line.find_first_not_of(" \t");
        if (firstChar != std::string::npos && line.compare(firstChar, 8, "#include") == 0)
        {
          const std::size_t quoteStart = line.find('"', firstChar);
          const std::size_t quoteEnd = quoteStart == std::string::npos ? std::string::npos : line.find('"', quoteStart + 1);
          if (quoteEnd == std::string::npos)
          {
            throw std::runtime_error(fmt::format("{}: expected #include \"file\"", filePath));
          }
          const std::string includePath = NormalizePath(folder + line.substr(quoteStart + 1, quoteEnd - quoteStart - 1));
          std::string includeText;
          if (!contentManager.TryReadAllText(includeText, IO::Path(includePath)))
          {
            throw std::runtime_error(fmt::format("{}: the include '{}' was not found", filePath, includePath));
          }
          result += ResolveIncludes(contentManager, includeText, includePath, depth + 1);
        }
        else if (firstChar != std::string::npos && line.compare(firstChar, 10, "#extension") == 0 &&
                 line.find("GL_GOOGLE_include_directive") != std::string::npos)
        {
          // The Vulkan shaders need it for their includes, OpenGL ES does not know it
        }
        else
        {
          result += line;
        }
        result += '\n';
        lineStart = lineEnd + 1;
      }
      return result;
    }

    //! Compile the fragment shader on its own first, so the error says what is wrong (GLProgram only logs it)
    void CheckFragmentShader(const std::string& source)
    {
      const GLuint hShader = glCreateShader(GL_FRAGMENT_SHADER);
      if (hShader == 0)
      {
        throw std::runtime_error("glCreateShader failed");
      }
      const char* pSource = source.c_str();
      glShaderSource(hShader, 1, &pSource, nullptr);
      glCompileShader(hShader);
      GLint status = GL_FALSE;
      glGetShaderiv(hShader, GL_COMPILE_STATUS, &status);
      if (status == GL_TRUE)
      {
        glDeleteShader(hShader);
        return;
      }
      GLint logLength = 0;
      glGetShaderiv(hShader, GL_INFO_LOG_LENGTH, &logLength);
      std::string log(static_cast<std::size_t>(std::max(logLength, 1)), '\0');
      glGetShaderInfoLog(hShader, static_cast<GLsizei>(log.size()), nullptr, log.data());
      glDeleteShader(hShader);
      while (!log.empty() && (log.back() == '\0' || log.back() == '\n' || log.back() == '\r'))
      {
        log.pop_back();
      }
      throw std::runtime_error(log);
    }

    GLenum ToMinFilter(const PixelArtFilter filter) noexcept
    {
      switch (filter)
      {
      case PixelArtFilter::Nearest:
        return GL_NEAREST;
      case PixelArtFilter::Mipmap:
        return GL_LINEAR_MIPMAP_LINEAR;
      case PixelArtFilter::Linear:
      default:
        return GL_LINEAR;
      }
    }
  }


  PixelArtRendererGLES3::SamplerCache::SamplerCache()
  {
    glGenSamplers(static_cast<GLsizei>(m_samplers.size()), m_samplers.data());
    for (uint32_t filterIndex = 0; filterIndex < 3; ++filterIndex)
    {
      for (uint32_t wrapIndex = 0; wrapIndex < 2; ++wrapIndex)
      {
        const auto filter = static_cast<PixelArtFilter>(filterIndex);
        const GLuint hSampler = m_samplers[ToSamplerIndex(filter, static_cast<PixelArtWrap>(wrapIndex))];
        const GLint wrap = wrapIndex == 0 ? GL_CLAMP_TO_EDGE : GL_REPEAT;
        glSamplerParameteri(hSampler, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(ToMinFilter(filter)));
        glSamplerParameteri(hSampler, GL_TEXTURE_MAG_FILTER, filter == PixelArtFilter::Nearest ? GL_NEAREST : GL_LINEAR);
        glSamplerParameteri(hSampler, GL_TEXTURE_WRAP_S, wrap);
        glSamplerParameteri(hSampler, GL_TEXTURE_WRAP_T, wrap);
        glSamplerParameteri(hSampler, GL_TEXTURE_WRAP_R, wrap);
      }
    }
  }


  PixelArtRendererGLES3::SamplerCache::~SamplerCache() noexcept
  {
    glDeleteSamplers(static_cast<GLsizei>(m_samplers.size()), m_samplers.data());
  }


  GLuint PixelArtRendererGLES3::SamplerCache::Get(const PixelArtFilter filter, const PixelArtWrap wrap) const noexcept
  {
    return m_samplers[ToSamplerIndex(filter, wrap)];
  }


  PixelArtRendererGLES3::PixelArtRendererGLES3(std::shared_ptr<IContentManager> contentManager, const bool srgbFramebuffer)
    : m_contentManager(std::move(contentManager))
    , m_srgbFramebuffer(srgbFramebuffer)
    , m_vertexShader(m_contentManager->ReadAllText(PixelArtSceneLoader::GetCommonPath(LocalConfig::VertexShader)))
    , m_prologue(m_contentManager->ReadAllText(PixelArtSceneLoader::GetCommonPath(LocalConfig::Prologue)))
    , m_vertexArray(true)
  {
    constexpr std::array<uint8_t, 4> BlackPixel = {0x00, 0x00, 0x00, 0xFF};
    const auto rawBitmap =
      ReadOnlyRawBitmap::Create(SpanUtil::AsReadOnlySpan(BlackPixel), PxSize2D::Create(1, 1), PixelFormat::R8G8B8A8_UNORM, BitmapOrigin::LowerLeft);
    m_blackTexture = std::make_shared<GLTexture>(rawBitmap, GLTextureParameters(GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE));
  }


  PixelArtRendererGLES3::~PixelArtRendererGLES3() = default;


  void PixelArtRendererGLES3::LoadScene(const PixelArtSceneDesc& scene)
  {
    // Everything is created before the current scene is replaced, so a scene that fails leaves the current one as it was
    auto newScene = std::make_unique<SceneRecord>();
    newScene->Output = scene.Output;
    for (uint32_t passIndex = 0; passIndex < PixelArtConfig::PassCount; ++passIndex)
    {
      const PixelArtPassDesc& passDesc = scene.Passes[passIndex];
      if (!passDesc.Enabled)
      {
        continue;
      }
      const auto pass = PixelArtPassUtil::FromIndex(passIndex);
      const char* const pszTab = PixelArtPassUtil::GetTabName(pass);
      PassRecord& rPass = newScene->Passes[passIndex];
      rPass.Enabled = true;

      for (uint32_t channelIndex = 0; channelIndex < PixelArtConfig::ChannelCount; ++channelIndex)
      {
        const PixelArtChannelDesc& channelDesc = passDesc.Channels[channelIndex];
        ChannelRecord& rChannel = rPass.Channels[channelIndex];
        rChannel.Source = channelDesc.Source;
        rChannel.Sampler = m_samplers.Get(channelDesc.Filter, channelDesc.Wrap);
        switch (channelDesc.Source)
        {
        case PixelArtChannelSource::Buffer:
          rChannel.BufferIndex = PixelArtPassUtil::ToIndex(channelDesc.Buffer);
          break;
        case PixelArtChannelSource::Texture:
        case PixelArtChannelSource::Cubemap:
          try
          {
            rChannel.Texture = GetTexture(channelDesc);
          }
          catch (const std::exception& ex)
          {
            throw std::runtime_error(fmt::format("{} iChannel{}: {}", pszTab, channelIndex, ex.what()));
          }
          break;
        case PixelArtChannelSource::Unused:
        default:
          rChannel.Texture = m_blackTexture;
          rChannel.Sampler = m_samplers.Get(PixelArtFilter::Nearest, PixelArtWrap::Clamp);
          break;
        }
      }

      const std::string fragmentShader = BuildFragmentShader(scene, pass);
      try
      {
        CheckFragmentShader(fragmentShader);
      }
      catch (const std::exception& ex)
      {
        throw std::runtime_error(
          fmt::format("{}.glsl did not compile (source 4 is the tab, 3 Params.glsl, 2 Common.glsl, 1 the prologue): {}", pszTab, ex.what()));
      }
      rPass.Program.Reset(m_vertexShader, fragmentShader);

      const GLProgram& program = rPass.Program;
      rPass.LocResolution = program.TryGetUniformLocation("iResolution");
      rPass.LocTime = program.TryGetUniformLocation("iTime");
      rPass.LocTimeDelta = program.TryGetUniformLocation("iTimeDelta");
      rPass.LocFrameRate = program.TryGetUniformLocation("iFrameRate");
      rPass.LocFrame = program.TryGetUniformLocation("iFrame");
      rPass.LocMouse = program.TryGetUniformLocation("iMouse");
      rPass.LocDate = program.TryGetUniformLocation("iDate");
      rPass.LocSampleRate = program.TryGetUniformLocation("iSampleRate");
      rPass.LocChannelTime = program.TryGetUniformLocation("iChannelTime");
      rPass.LocChannelResolution = program.TryGetUniformLocation("iChannelResolution");
      rPass.LocParams = program.TryGetUniformLocation("PixelArt_Params");
      rPass.LocPass = program.TryGetUniformLocation("PixelArt_Pass");
      for (uint32_t channelIndex = 0; channelIndex < PixelArtConfig::ChannelCount; ++channelIndex)
      {
        const std::string name = fmt::format("iChannel{}", channelIndex);
        rPass.LocChannels[channelIndex] = program.TryGetUniformLocation(name.c_str());
      }
    }
    m_scene = std::move(newScene);
    // The new scene starts with empty buffers
    ClearTargets();
  }


  void PixelArtRendererGLES3::Draw(const PixelArtFrameState& frameState)
  {
    if (!m_scene)
    {
      return;
    }
    if (frameState.RenderSizePx != m_targetSizePx)
    {
      ResizeTargets(frameState.RenderSizePx);
    }
    else if (frameState.Frame == 0)
    {
      ClearTargets();
    }

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    m_vertexArray.Bind();

    for (uint32_t bufferIndex = 0; bufferIndex < PixelArtConfig::BufferCount; ++bufferIndex)
    {
      const PassRecord& pass = m_scene->Passes[bufferIndex];
      if (!pass.Enabled)
      {
        continue;
      }
      // Draw to the target that does not have the last output, so the pass can read that
      const uint32_t writeTarget = 1u - m_currentTarget[bufferIndex];
      glBindFramebuffer(GL_FRAMEBUFFER, m_targets[bufferIndex][writeTarget].Get());
      glViewport(0, 0, m_targetSizePx.RawWidth(), m_targetSizePx.RawHeight());
      DrawPass(pass, frameState, m_targetSizePx, false);
      // The passes after this one read what it drew this frame
      m_currentTarget[bufferIndex] = writeTarget;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // The scene area starts in the bottom left corner (OpenGL ES) as well as the top left corner of the window, the UI panel is to its right
    glViewport(0, 0, frameState.SceneSizePx.RawWidth(), frameState.SceneSizePx.RawHeight());
    DrawPass(m_scene->Passes[PixelArtPassUtil::ToIndex(PixelArtPass::Image)], frameState, frameState.SceneSizePx, true);

    // Leave the state the way the UI expects it
    for (uint32_t channelIndex = 0; channelIndex < PixelArtConfig::ChannelCount; ++channelIndex)
    {
      glActiveTexture(GL_TEXTURE0 + channelIndex);
      glBindSampler(channelIndex, 0);
      glBindTexture(GL_TEXTURE_2D, 0);
      glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    }
    glActiveTexture(GL_TEXTURE0);
    m_vertexArray.Unbind();
    glUseProgram(0);
  }


  std::shared_ptr<GLTexture> PixelArtRendererGLES3::GetTexture(const PixelArtChannelDesc& channel)
  {
    const bool isCubemap = channel.Source == PixelArtChannelSource::Cubemap;
    const std::string key = fmt::format("{}|{}|{}|{}", channel.Path.ToUTF8String(), isCubemap, channel.Srgb, channel.VFlip);
    const auto itr = m_textures.find(key);
    if (itr != m_textures.end())
    {
      return itr->second;
    }

    // shadertoy's VFlip means the first row of the texture is the bottom of the image, which is how OpenGL ES stores it
    const BitmapOrigin origin = (isCubemap || !channel.VFlip) ? BitmapOrigin::UpperLeft : BitmapOrigin::LowerLeft;
    Texture texture = m_contentManager->ReadTexture(channel.Path, PixelFormat::R8G8B8A8_UNORM, origin);
    if (isCubemap != (texture.GetTextureType() == TextureType::TexCube))
    {
      throw std::runtime_error(fmt::format("'{}' is not a {}", channel.Path.ToUTF8String(), isCubemap ? "cubemap" : "2D texture"));
    }
    if (channel.Srgb)
    {
      texture.ChangeCompatiblePixelFormatFlags(PixelFormatFlags::NF_Srgb);
    }
    const GLenum target = isCubemap ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
    auto glTexture = isCubemap
                       ? std::make_shared<GLTexture>(
                           texture, GLTextureParameters3(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE),
                           TextureFlags::AllowAnyBitmapOrigin)
                       : std::make_shared<GLTexture>(texture, GLTextureParameters(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_REPEAT, GL_REPEAT),
                                                     TextureFlags::AllowAnyBitmapOrigin);
    if (texture.GetLevels() == 1)
    {
      // The mipmap filter needs the levels
      glBindTexture(target, glTexture->Get());
      glGenerateMipmap(target);
      glBindTexture(target, 0);
    }
    m_textures.emplace(key, glTexture);
    return glTexture;
  }


  std::string PixelArtRendererGLES3::BuildFragmentShader(const PixelArtSceneDesc& scene, const PixelArtPass pass) const
  {
    const PixelArtPassDesc& passDesc = scene.GetPass(pass);
    // '#line 1 n' makes a compile error name the file it is in: 1 = the prologue, 2 = Common.glsl, 3 = Params.glsl, 4 = the tab
    std::string source = "#version 300 es\n#line 1 1\n";
    source += m_prologue;
    source += '\n';
    for (uint32_t channelIndex = 0; channelIndex < PixelArtConfig::ChannelCount; ++channelIndex)
    {
      const bool isCubemap = passDesc.Channels[channelIndex].Source == PixelArtChannelSource::Cubemap;
      source += fmt::format("uniform {} iChannel{};\n", isCubemap ? "samplerCube" : "sampler2D", channelIndex);
    }

    const IContentManager& contentManager = *m_contentManager;
    if (scene.HasCommonTab)
    {
      const IO::Path path = PixelArtSceneLoader::GetScenePath(scene.Id, LocalConfig::CommonTab);
      source += "#line 1 2\n";
      source += ResolveIncludes(contentManager, contentManager.ReadAllText(path), path.ToUTF8String(), 0);
    }
    {
      const IO::Path path = PixelArtSceneLoader::GetScenePath(scene.Id, LocalConfig::ParamsFile);
      std::string text;
      if (contentManager.TryReadAllText(text, path))
      {
        source += "#line 1 3\n";
        source += ResolveIncludes(contentManager, text, path.ToUTF8String(), 0);
      }
    }
    {
      const IO::Path path = PixelArtSceneLoader::GetScenePath(scene.Id, IO::PathView(PixelArtPassUtil::GetTabName(pass)));
      const IO::Path tabPath(path.ToUTF8String() + ".glsl");
      std::string text;
      if (!contentManager.TryReadAllText(text, tabPath))
      {
        throw std::runtime_error(fmt::format("{}: the tab was not found", tabPath.ToUTF8String()));
      }
      source += "#line 1 4\n";
      source += ResolveIncludes(contentManager, text, tabPath.ToUTF8String(), 0);
    }
    return source;
  }


  void PixelArtRendererGLES3::ResizeTargets(const PxSize2D sizePx)
  {
    const GLTextureParameters textureParameters(GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
    const GLTextureImageParameters imageParameters(GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    for (auto& rTargets : m_targets)
    {
      for (auto& rTarget : rTargets)
      {
        rTarget.Reset(sizePx, textureParameters, imageParameters);
      }
    }
    m_targetSizePx = sizePx;
    ClearTargets();
  }


  void PixelArtRendererGLES3::ClearTargets()
  {
    m_currentTarget.fill(0);
    if (m_targetSizePx.RawWidth() <= 0 || m_targetSizePx.RawHeight() <= 0)
    {
      return;
    }
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    for (const auto& targets : m_targets)
    {
      for (const auto& target : targets)
      {
        glBindFramebuffer(GL_FRAMEBUFFER, target.Get());
        glClear(GL_COLOR_BUFFER_BIT);
      }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }


  void PixelArtRendererGLES3::DrawPass(const PassRecord& pass, const PixelArtFrameState& frameState, const PxSize2D passSizePx,
                                       const bool isImagePass)
  {
    glUseProgram(pass.Program.Get());

    const auto passWidth = static_cast<float>(passSizePx.RawWidth());
    const auto passHeight = static_cast<float>(passSizePx.RawHeight());
    glUniform3f(pass.LocResolution, passWidth, passHeight, 1.0f);
    glUniform1f(pass.LocTime, frameState.Time);
    glUniform1f(pass.LocTimeDelta, frameState.TimeDelta);
    glUniform1f(pass.LocFrameRate, frameState.FrameRate);
    glUniform1i(pass.LocFrame, frameState.Frame);
    {
      // iMouse is given in the pixels of the pass
      const float scaleX = frameState.SceneSizePx.RawWidth() > 0 ? passWidth / static_cast<float>(frameState.SceneSizePx.RawWidth()) : 1.0f;
      const float scaleY = frameState.SceneSizePx.RawHeight() > 0 ? passHeight / static_cast<float>(frameState.SceneSizePx.RawHeight()) : 1.0f;
      const auto& mouse = frameState.MouseWindowPx;
      glUniform4f(pass.LocMouse, mouse[0] * scaleX, mouse[1] * scaleY, mouse[2] * scaleX, mouse[3] * scaleY);
    }
    glUniform4f(pass.LocDate, frameState.Date[0], frameState.Date[1], frameState.Date[2], frameState.Date[3]);
    glUniform1f(pass.LocSampleRate, 44100.0f);
    {
      constexpr std::array<float, PixelArtConfig::ChannelCount> ChannelTime{};
      glUniform1fv(pass.LocChannelTime, static_cast<GLsizei>(ChannelTime.size()), ChannelTime.data());
    }
    if (frameState.Params.size() >= PixelArtConfig::MaxParams)
    {
      glUniform4fv(pass.LocParams, static_cast<GLsizei>(PixelArtConfig::MaxParams / 4), frameState.Params.data());
    }
    {
      // What happens to the color of the image pass: the window framebuffer encodes linear colors to sRGB if it is a sRGB framebuffer
      float outputMode = 0.0f;
      if (isImagePass)
      {
        if (m_scene->Output == PixelArtOutput::Linear && !m_srgbFramebuffer)
        {
          outputMode = 1.0f;
        }
        else if (m_scene->Output == PixelArtOutput::Gamma && m_srgbFramebuffer)
        {
          outputMode = 2.0f;
        }
      }
      glUniform4f(pass.LocPass, isImagePass ? 1.0f : 0.0f, outputMode, 0.0f, 0.0f);
    }

    std::array<float, std::size_t{PixelArtConfig::ChannelCount} * 3u> channelResolution{};
    for (uint32_t channelIndex = 0; channelIndex < PixelArtConfig::ChannelCount; ++channelIndex)
    {
      const ChannelRecord& channel = pass.Channels[channelIndex];
      const std::size_t resolutionIndex = std::size_t{channelIndex} * 3u;
      glActiveTexture(GL_TEXTURE0 + channelIndex);
      glBindSampler(channelIndex, channel.Sampler);
      if (channel.Source == PixelArtChannelSource::Buffer)
      {
        // A buffer drawn earlier this frame has its new output, the pass itself or a later buffer has the output of the last frame
        const GLFrameBuffer& target = m_targets[channel.BufferIndex][m_currentTarget[channel.BufferIndex]];
        glBindTexture(GL_TEXTURE_2D, target.GetTextureInfo().Handle);
        channelResolution[resolutionIndex + 0] = static_cast<float>(m_targetSizePx.RawWidth());
        channelResolution[resolutionIndex + 1] = static_cast<float>(m_targetSizePx.RawHeight());
      }
      else
      {
        glBindTexture(channel.Texture->GetTarget(), channel.Texture->Get());
        const auto extent = channel.Texture->GetExtent();
        channelResolution[resolutionIndex + 0] = static_cast<float>(extent.Width.Value);
        channelResolution[resolutionIndex + 1] = static_cast<float>(extent.Height.Value);
      }
      channelResolution[resolutionIndex + 2] = 1.0f;
      glUniform1i(pass.LocChannels[channelIndex], static_cast<GLint>(channelIndex));
    }
    glUniform3fv(pass.LocChannelResolution, static_cast<GLsizei>(PixelArtConfig::ChannelCount), channelResolution.data());

    glDrawArrays(GL_TRIANGLES, 0, 3);
  }
}
