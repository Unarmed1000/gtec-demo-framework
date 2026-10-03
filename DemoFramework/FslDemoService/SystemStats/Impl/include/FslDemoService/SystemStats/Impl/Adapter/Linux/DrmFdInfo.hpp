#ifndef FSLDEMOSERVICE_SYSTEMSTATS_IMPL_ADAPTER_LINUX_DRMFDINFO_HPP
#define FSLDEMOSERVICE_SYSTEMSTATS_IMPL_ADAPTER_LINUX_DRMFDINFO_HPP
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


#include <array>
#include <cstdint>
#include <string_view>

namespace Fsl
{
  //! What one file of /proc/<pid>/fdinfo says about a DRM client (a open GPU device), see the "DRM client usage stats" of the Linux kernel.
  struct DrmFdInfo
  {
    static constexpr uint32_t MaxEngines = 16;
    static constexpr uint32_t MaxEngineNameLength = 31;

    struct Engine
    {
      std::array<char, MaxEngineNameLength + 1> Name{};
      uint32_t NameLength{0};
      //! How long the engine has worked for the client in nanoseconds (it only grows)
      uint64_t BusyNanoseconds{0};

      [[nodiscard]] std::string_view GetName() const noexcept
      {
        return {Name.data(), NameLength};
      }
    };

    //! True if the file has a drm-client-id: the file descriptor is a DRM client
    bool IsDrmClient{false};
    //! Several file descriptors can be the same client, they then have the same id
    uint64_t ClientId{0};
    std::array<Engine, MaxEngines> Engines{};
    uint32_t EngineCount{0};
    //! True if the driver reports the memory of the client
    bool HasMemory{false};
    //! The memory of the client in bytes: what is resident if the driver says so, else what is allocated
    uint64_t MemoryBytes{0};
  };
}

#endif
