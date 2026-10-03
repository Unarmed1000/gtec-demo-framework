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
#include <FslDemoApp/Base/Service/Content/IContentManager.hpp>
#include <Shared/PixelArt/Base/PixelArtSceneLoader.hpp>
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <algorithm>
#include <map>
#include <regex>
#include <set>
#include <stdexcept>
#include <utility>

namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
      constexpr IO::PathView SceneFile("Scene.json");
      constexpr IO::PathView ParamsFile("Params.glsl");
      constexpr IO::PathView CommonTabFile("Common.glsl");
      constexpr IO::PathView CommonFolder("Common");
    }

    [[noreturn]] void ThrowError(const IO::Path& file, const std::string& message)
    {
      throw std::runtime_error(fmt::format("{}: {}", file.ToUTF8String(), message));
    }

    nlohmann::json ParseJson(const IContentManager& contentManager, const IO::Path& path)
    {
      std::string text;
      if (!contentManager.TryReadAllText(text, path))
      {
        ThrowError(path, "the file was not found");
      }
      try
      {
        // Comments are allowed, they make a scene easier to document
        return nlohmann::json::parse(text, nullptr, true, true);
      }
      catch (const nlohmann::json::exception& ex)
      {
        ThrowError(path, ex.what());
      }
    }

    std::string ReadString(const nlohmann::json& json, const char* const pszKey, const std::string& defaultValue)
    {
      const auto itr = json.find(pszKey);
      if (itr == json.end())
      {
        return defaultValue;
      }
      if (!itr->is_string())
      {
        throw std::runtime_error(fmt::format("'{}' must be a string", pszKey));
      }
      return itr->get<std::string>();
    }

    float ReadFloat(const nlohmann::json& json, const char* const pszKey, const float defaultValue)
    {
      const auto itr = json.find(pszKey);
      if (itr == json.end())
      {
        return defaultValue;
      }
      if (!itr->is_number())
      {
        throw std::runtime_error(fmt::format("'{}' must be a number", pszKey));
      }
      return itr->get<float>();
    }

    bool ReadBool(const nlohmann::json& json, const char* const pszKey, const bool defaultValue)
    {
      const auto itr = json.find(pszKey);
      if (itr == json.end())
      {
        return defaultValue;
      }
      if (!itr->is_boolean())
      {
        throw std::runtime_error(fmt::format("'{}' must be true or false", pszKey));
      }
      return itr->get<bool>();
    }

    PixelArtPass ParseBufferName(const std::string& name)
    {
      if (name == "A")
      {
        return PixelArtPass::BufferA;
      }
      if (name == "B")
      {
        return PixelArtPass::BufferB;
      }
      if (name == "C")
      {
        return PixelArtPass::BufferC;
      }
      if (name == "D")
      {
        return PixelArtPass::BufferD;
      }
      throw std::runtime_error(fmt::format("unknown buffer '{}', use A, B, C or D", name));
    }

    PixelArtFilter ParseFilter(const std::string& name)
    {
      if (name == "Nearest")
      {
        return PixelArtFilter::Nearest;
      }
      if (name == "Linear")
      {
        return PixelArtFilter::Linear;
      }
      if (name == "Mipmap")
      {
        return PixelArtFilter::Mipmap;
      }
      throw std::runtime_error(fmt::format("unknown filter '{}', use Nearest, Linear or Mipmap", name));
    }

    PixelArtWrap ParseWrap(const std::string& name)
    {
      if (name == "Clamp")
      {
        return PixelArtWrap::Clamp;
      }
      if (name == "Repeat")
      {
        return PixelArtWrap::Repeat;
      }
      throw std::runtime_error(fmt::format("unknown wrap '{}', use Clamp or Repeat", name));
    }

    const char* ToString(const PixelArtFilter filter) noexcept
    {
      switch (filter)
      {
      case PixelArtFilter::Nearest:
        return "Nearest";
      case PixelArtFilter::Linear:
        return "Linear";
      case PixelArtFilter::Mipmap:
        return "Mipmap";
      default:
        return "Unknown";
      }
    }

    const char* ToString(const PixelArtWrap wrap) noexcept
    {
      switch (wrap)
      {
      case PixelArtWrap::Clamp:
        return "Clamp";
      case PixelArtWrap::Repeat:
        return "Repeat";
      default:
        return "Unknown";
      }
    }

    //! A channel, with the defaults shadertoy uses for its kind: a buffer is linear and clamped, a texture or cubemap is mipmapped
    PixelArtChannelDesc ParseChannel(const nlohmann::json& json)
    {
      if (!json.is_object())
      {
        throw std::runtime_error("a channel must be a object");
      }
      PixelArtChannelDesc channel;
      int sourceCount = 0;
      if (json.contains("Buffer"))
      {
        channel.Source = PixelArtChannelSource::Buffer;
        channel.Buffer = ParseBufferName(ReadString(json, "Buffer", ""));
        channel.Filter = PixelArtFilter::Linear;
        channel.Wrap = PixelArtWrap::Clamp;
        ++sourceCount;
      }
      if (json.contains("Texture"))
      {
        channel.Source = PixelArtChannelSource::Texture;
        channel.Path = IO::Path(ReadString(json, "Texture", ""));
        channel.Filter = PixelArtFilter::Mipmap;
        channel.Wrap = PixelArtWrap::Repeat;
        ++sourceCount;
      }
      if (json.contains("Cubemap"))
      {
        channel.Source = PixelArtChannelSource::Cubemap;
        channel.Path = IO::Path(ReadString(json, "Cubemap", ""));
        channel.Filter = PixelArtFilter::Mipmap;
        channel.Wrap = PixelArtWrap::Clamp;
        ++sourceCount;
      }
      if (sourceCount != 1)
      {
        throw std::runtime_error("a channel must have exactly one of 'Buffer', 'Texture' or 'Cubemap'");
      }
      channel.Filter = ParseFilter(ReadString(json, "Filter", ToString(channel.Filter)));
      channel.Wrap = ParseWrap(ReadString(json, "Wrap", ToString(channel.Wrap)));
      channel.VFlip = ReadBool(json, "VFlip", channel.VFlip);
      channel.Srgb = ReadBool(json, "sRGB", channel.Srgb);
      if (channel.Source == PixelArtChannelSource::Buffer && channel.Filter == PixelArtFilter::Mipmap)
      {
        FSLLOG3_WARNING("A buffer channel with the filter 'Mipmap' is read with the filter 'Linear' (buffers have no mipmaps)");
        channel.Filter = PixelArtFilter::Linear;
      }
      return channel;
    }

    PixelArtPassDesc ParsePass(const nlohmann::json& json)
    {
      if (!json.is_object())
      {
        throw std::runtime_error("a pass must be a object");
      }
      PixelArtPassDesc pass;
      pass.Enabled = true;
      for (const auto& item : json.items())
      {
        const std::string& key = item.key();
        uint32_t channelIndex = PixelArtConfig::ChannelCount;
        for (uint32_t i = 0; i < PixelArtConfig::ChannelCount; ++i)
        {
          if (key == fmt::format("iChannel{}", i))
          {
            channelIndex = i;
          }
        }
        if (channelIndex >= PixelArtConfig::ChannelCount)
        {
          throw std::runtime_error(fmt::format("unknown key '{}', a pass has the channels iChannel0 to iChannel3", key));
        }
        try
        {
          pass.Channels[channelIndex] = ParseChannel(item.value());
        }
        catch (const std::exception& ex)
        {
          throw std::runtime_error(fmt::format("{}: {}", key, ex.what()));
        }
      }
      return pass;
    }

    PixelArtParamDesc ParseParam(const nlohmann::json& json)
    {
      if (!json.is_object())
      {
        throw std::runtime_error("a param must be a object");
      }
      PixelArtParamDesc param;
      param.Name = ReadString(json, "Name", "");
      if (param.Name.empty())
      {
        throw std::runtime_error("a param needs a 'Name'");
      }
      try
      {
        param.Label = ReadString(json, "Label", param.Name);
        param.Group = ReadString(json, "Group", "Settings");
        const std::string type = ReadString(json, "Type", "float");
        if (type == "float")
        {
          param.Type = PixelArtParamType::Float;
        }
        else if (type == "int")
        {
          param.Type = PixelArtParamType::Int;
        }
        else
        {
          throw std::runtime_error(fmt::format("unknown type '{}', use float or int", type));
        }
        param.Default = ReadFloat(json, "Default", 0.0f);
        param.Min = ReadFloat(json, "Min", 0.0f);
        param.Max = ReadFloat(json, "Max", 1.0f);
        param.Step = ReadFloat(json, "Step", param.Type == PixelArtParamType::Int ? 1.0f : 0.0f);
        param.Format = ReadString(json, "Format", param.Type == PixelArtParamType::Int ? "{:.0f}" : "{:.2f}");
        if (param.Min > param.Default || param.Default > param.Max)
        {
          throw std::runtime_error(fmt::format("Min <= Default <= Max is not true ({} <= {} <= {})", param.Min, param.Default, param.Max));
        }
        if (param.Step < 0.0f)
        {
          throw std::runtime_error("'Step' can not be negative");
        }
      }
      catch (const std::exception& ex)
      {
        throw std::runtime_error(fmt::format("param '{}': {}", param.Name, ex.what()));
      }
      return param;
    }

    //! The slots Params.glsl gives the defines that read a param: "#define NAME ... PARAM(n) ..."
    std::map<std::string, uint32_t> ParseParamsGlsl(const std::string& text)
    {
      static const std::regex g_defineRegex(R"(^\s*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)\s+.*\bPARAM\s*\(\s*([0-9]+)\s*\))");
      std::map<std::string, uint32_t> slots;
      std::size_t lineStart = 0;
      while (lineStart < text.size())
      {
        std::size_t lineEnd = text.find('\n', lineStart);
        if (lineEnd == std::string::npos)
        {
          lineEnd = text.size();
        }
        const std::string line = text.substr(lineStart, lineEnd - lineStart);
        std::smatch match;
        if (std::regex_search(line, match, g_defineRegex))
        {
          slots[match[1].str()] = static_cast<uint32_t>(std::stoul(match[2].str()));
        }
        lineStart = lineEnd + 1;
      }
      return slots;
    }

    //! The shaders read the params through Params.glsl, so its slots have to be the order of the params in Scene.json
    void CheckParamsGlsl(const IContentManager& contentManager, const std::string& sceneId, const std::vector<PixelArtParamDesc>& params)
    {
      const IO::Path path = PixelArtSceneLoader::GetScenePath(sceneId, LocalConfig::ParamsFile);
      std::string text;
      if (!contentManager.TryReadAllText(text, path))
      {
        if (!params.empty())
        {
          ThrowError(path, "the file was not found, a scene with params needs it");
        }
        return;
      }
      const std::map<std::string, uint32_t> slots = ParseParamsGlsl(text);
      for (std::size_t i = 0; i < params.size(); ++i)
      {
        const auto itr = slots.find(params[i].Name);
        if (itr == slots.end())
        {
          ThrowError(path, fmt::format("'{}' is not defined, add '#define {} PARAM({})'", params[i].Name, params[i].Name, i));
        }
        if (itr->second != i)
        {
          ThrowError(path, fmt::format("'{}' reads PARAM({}), but it is param {} in Scene.json", params[i].Name, itr->second, i));
        }
      }
      for (const auto& entry : slots)
      {
        const bool found = std::any_of(params.begin(), params.end(), [&entry](const PixelArtParamDesc& param) { return param.Name == entry.first; });
        if (!found)
        {
          ThrowError(path, fmt::format("'{}' reads PARAM({}), but Scene.json has no param named '{}'", entry.first, entry.second, entry.first));
        }
      }
    }
  }


  namespace PixelArtPassUtil
  {
    const char* GetTabName(const PixelArtPass pass) noexcept
    {
      switch (pass)
      {
      case PixelArtPass::BufferA:
        return "BufferA";
      case PixelArtPass::BufferB:
        return "BufferB";
      case PixelArtPass::BufferC:
        return "BufferC";
      case PixelArtPass::BufferD:
        return "BufferD";
      case PixelArtPass::Image:
        return "Image";
      default:
        return "Unknown";
      }
    }
  }


  namespace PixelArtSceneLoader
  {
    std::vector<std::string> LoadSceneList(const IContentManager& contentManager)
    {
      const IO::Path path(PixelArtConfig::SceneList);
      const nlohmann::json json = ParseJson(contentManager, path);
      const auto itr = json.find("Scenes");
      if (itr == json.end() || !itr->is_array())
      {
        ThrowError(path, "expected a 'Scenes' array");
      }
      std::vector<std::string> scenes;
      for (const auto& entry : *itr)
      {
        if (!entry.is_string() || entry.get<std::string>().empty())
        {
          ThrowError(path, "every scene must be the name of a scene folder");
        }
        scenes.push_back(entry.get<std::string>());
      }
      if (scenes.empty())
      {
        ThrowError(path, "the list of scenes is empty");
      }
      return scenes;
    }


    PixelArtSceneDesc LoadScene(const IContentManager& contentManager, const std::string& sceneId)
    {
      const IO::Path path = GetScenePath(sceneId, LocalConfig::SceneFile);
      const nlohmann::json json = ParseJson(contentManager, path);

      PixelArtSceneDesc scene;
      scene.Id = sceneId;
      try
      {
        if (!json.is_object())
        {
          throw std::runtime_error("expected a object");
        }
        scene.Name = ReadString(json, "Name", sceneId);
        scene.Description = ReadString(json, "Description", "");
        const std::string output = ReadString(json, "Output", "Gamma");
        if (output == "Linear")
        {
          scene.Output = PixelArtOutput::Linear;
        }
        else if (output == "Gamma")
        {
          scene.Output = PixelArtOutput::Gamma;
        }
        else
        {
          throw std::runtime_error(fmt::format("unknown output '{}', use Linear or Gamma", output));
        }

        const auto itrPasses = json.find("Passes");
        if (itrPasses == json.end() || !itrPasses->is_object())
        {
          throw std::runtime_error("expected a 'Passes' object");
        }
        for (const auto& item : itrPasses->items())
        {
          uint32_t passIndex = PixelArtConfig::PassCount;
          for (uint32_t i = 0; i < PixelArtConfig::PassCount; ++i)
          {
            if (item.key() == PixelArtPassUtil::GetTabName(PixelArtPassUtil::FromIndex(i)))
            {
              passIndex = i;
            }
          }
          if (passIndex >= PixelArtConfig::PassCount)
          {
            throw std::runtime_error(fmt::format("unknown pass '{}', use BufferA, BufferB, BufferC, BufferD or Image", item.key()));
          }
          try
          {
            scene.Passes[passIndex] = ParsePass(item.value());
          }
          catch (const std::exception& ex)
          {
            throw std::runtime_error(fmt::format("{}: {}", item.key(), ex.what()));
          }
        }
        if (!scene.GetPass(PixelArtPass::Image).Enabled)
        {
          throw std::runtime_error("the 'Image' pass is missing");
        }
        // A channel can only read a buffer the scene draws
        for (uint32_t passIndex = 0; passIndex < PixelArtConfig::PassCount; ++passIndex)
        {
          for (const PixelArtChannelDesc& channel : scene.Passes[passIndex].Channels)
          {
            if (channel.Source == PixelArtChannelSource::Buffer && !scene.GetPass(channel.Buffer).Enabled)
            {
              throw std::runtime_error(fmt::format("{} reads {}, which is not a pass of the scene",
                                                   PixelArtPassUtil::GetTabName(PixelArtPassUtil::FromIndex(passIndex)),
                                                   PixelArtPassUtil::GetTabName(channel.Buffer)));
            }
          }
        }

        const auto itrParams = json.find("Params");
        if (itrParams != json.end())
        {
          if (!itrParams->is_array())
          {
            throw std::runtime_error("'Params' must be a array");
          }
          std::set<std::string> names;
          for (const auto& entry : *itrParams)
          {
            PixelArtParamDesc param = ParseParam(entry);
            if (!names.insert(param.Name).second)
            {
              throw std::runtime_error(fmt::format("the param '{}' is listed twice", param.Name));
            }
            scene.Params.push_back(std::move(param));
          }
          if (scene.Params.size() > PixelArtConfig::MaxParams)
          {
            throw std::runtime_error(fmt::format("a scene can have at most {} params", PixelArtConfig::MaxParams));
          }
        }
      }
      catch (const std::exception& ex)
      {
        ThrowError(path, ex.what());
      }

      CheckParamsGlsl(contentManager, sceneId, scene.Params);
      scene.HasCommonTab = contentManager.Exists(GetScenePath(sceneId, LocalConfig::CommonTabFile));
      return scene;
    }


    IO::Path GetScenePath(const std::string& sceneId, const IO::PathView fileName)
    {
      return IO::Path::Combine(IO::Path::Combine(PixelArtConfig::ShaderRoot, IO::Path(sceneId)), fileName);
    }


    IO::Path GetCommonPath(const IO::PathView fileName)
    {
      return IO::Path::Combine(IO::Path::Combine(PixelArtConfig::ShaderRoot, LocalConfig::CommonFolder), fileName);
    }
  }
}
