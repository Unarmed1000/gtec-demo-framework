#ifndef SHARED_PIXELART_BASE_PIXELARTPARAMSET_HPP
#define SHARED_PIXELART_BASE_PIXELARTPARAMSET_HPP
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

#include <FslBase/Span/ReadOnlySpan.hpp>
#include <Shared/PixelArt/Base/PixelArtSceneDesc.hpp>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Fsl
{
  //! The values of the adjustable constants of a scene, packed the way the shaders read them (slot i is Params[i / 4][i % 4])
  class PixelArtParamSet
  {
    std::vector<PixelArtParamDesc> m_params;
    std::array<float, PixelArtConfig::MaxParams> m_values{};

  public:
    PixelArtParamSet() = default;
    explicit PixelArtParamSet(std::vector<PixelArtParamDesc> params);

    [[nodiscard]] uint32_t GetCount() const noexcept
    {
      return static_cast<uint32_t>(m_params.size());
    }

    [[nodiscard]] const PixelArtParamDesc& GetDesc(const uint32_t index) const
    {
      return m_params.at(index);
    }

    [[nodiscard]] float GetValue(const uint32_t index) const
    {
      return m_values.at(index);
    }

    //! Set a value, it is clamped to the range of the param and snapped to its step
    //! @return true if the value changed
    bool SetValue(const uint32_t index, const float value);

    //! Set the value of the param with the given name
    //! @return false if the scene has no param with that name
    bool TrySetValue(const std::string& name, const float value);

    //! Copy the values of the params the other set has a param with the same name for
    void CopyValuesByName(const PixelArtParamSet& other);

    void ResetToDefaults();

    //! All the values, as many as the shaders have room for (the unused ones are zero)
    [[nodiscard]] ReadOnlySpan<float> GetPackedValues() const noexcept
    {
      return ReadOnlySpan<float>(m_values.data(), m_values.size());
    }
  };
}

#endif
