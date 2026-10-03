#ifndef FSLDEMOSERVICE_SYSTEMSTATS_IMPL_ADAPTER_LINUX_DRMFDINFOPARSER_HPP
#define FSLDEMOSERVICE_SYSTEMSTATS_IMPL_ADAPTER_LINUX_DRMFDINFOPARSER_HPP
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


#include <FslDemoService/SystemStats/Impl/Adapter/Linux/DrmFdInfo.hpp>
#include <string_view>

//! Parses the text of a /proc/<pid>/fdinfo/<fd> file of a DRM client. It has no platform dependency, so it can be tested everywhere.
namespace Fsl::DrmFdInfoParser
{
  //! @brief Parse the content of a fdinfo file.
  //!        Used keys: drm-client-id, drm-engine-<name> (a busy time in ns), drm-resident-<region> and drm-total-<region> (a size with a
  //!        optional KiB or MiB). Everything else is ignored, including the deprecated drm-memory-<region>.
  //! @return the parsed info, its IsDrmClient is false if the file is not the one of a DRM client.
  [[nodiscard]] DrmFdInfo Parse(const std::string_view content) noexcept;
}

#endif
