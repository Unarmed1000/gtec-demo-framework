#ifndef SHARED_PIXELART_BASE_PIXELARTSCENELOADER_HPP
#define SHARED_PIXELART_BASE_PIXELARTSCENELOADER_HPP
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

#include <FslBase/IO/Path.hpp>
#include <Shared/PixelArt/Base/PixelArtSceneDesc.hpp>
#include <string>
#include <vector>

namespace Fsl
{
  class IContentManager;

  //! Reads the scenes from the content folder. Every function throws a std::runtime_error that says what is wrong.
  namespace PixelArtSceneLoader
  {
    //! The scene ids listed in PixelArt/Shaders/Scenes.json, in the order the UI shows them
    std::vector<std::string> LoadSceneList(const IContentManager& contentManager);

    //! Read PixelArt/Shaders/<sceneId>/Scene.json and check it against the Params.glsl of the scene
    PixelArtSceneDesc LoadScene(const IContentManager& contentManager, const std::string& sceneId);

    //! The path of a file in the folder of a scene
    IO::Path GetScenePath(const std::string& sceneId, const IO::PathView fileName);

    //! The path of a file in the Common folder of the shader tree
    IO::Path GetCommonPath(const IO::PathView fileName);
  }
}

#endif
