#ifdef __linux__
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

#include <FslBase/Exceptions.hpp>
#include <FslBase/Log/Log3Fmt.hpp>
#include <FslBase/System/Platform/Linux/ScopedSystemMemoryAccessLinux.hpp>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstring>


namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
      constexpr const uint64_t MapSize = 4096UL;
      constexpr const uint64_t MapMask = (MapSize - 1);

    }


    inline const char* safeStrerror()
    {
      auto errorNumber = errno;
      auto* psz = strerror(errorNumber);
      return psz != nullptr ? psz : "";
    }
  }

  ScopedSystemMemoryAccessLinux::ScopedSystemMemoryAccessLinux(const std::size_t targetAddress)
    : m_fd(0)
    , m_pMem(nullptr)
    , m_pVirtAddress(nullptr)
    , m_lastValue(0)
  {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    if ((m_fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1)
    {
      throw IOException(safeStrerror());
    }

    // Map one page
    m_pMem = mmap(nullptr, LocalConfig::MapSize, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, targetAddress & (~LocalConfig::MapMask));
    if (m_pMem == reinterpret_cast<void*>(-1))
    {
      throw IOException(safeStrerror());
    }

    m_pVirtAddress = (static_cast<uint8_t*>(m_pMem)) + (targetAddress & LocalConfig::MapMask);
  }


  ScopedSystemMemoryAccessLinux::~ScopedSystemMemoryAccessLinux()
  {
    if (munmap(m_pMem, LocalConfig::MapSize) == -1)
    {
      FSLLOG3_WARNING("munmap failed with '{}'", safeStrerror());
    }
    close(m_fd);
  }


  uint8_t ScopedSystemMemoryAccessLinux::GetUInt8() const
  {
    return *static_cast<uint8_t*>(m_pVirtAddress);
  }


  uint16_t ScopedSystemMemoryAccessLinux::GetUInt16() const
  {
    return *static_cast<uint16_t*>(m_pVirtAddress);
  }


  uint32_t ScopedSystemMemoryAccessLinux::GetUInt32() const
  {
    return *static_cast<uint32_t*>(m_pVirtAddress);
  }


  void ScopedSystemMemoryAccessLinux::SetUInt8(const uint8_t value)
  {
    *static_cast<uint8_t*>(m_pVirtAddress) = value;
    m_lastValue = *static_cast<uint8_t*>(m_pVirtAddress);
  }


  void ScopedSystemMemoryAccessLinux::SetUInt16(const uint16_t value)
  {
    *static_cast<uint16_t*>(m_pVirtAddress) = value;
    m_lastValue = *static_cast<uint16_t*>(m_pVirtAddress);
  }


  void ScopedSystemMemoryAccessLinux::SetUInt32(const uint32_t value)
  {
    *static_cast<uint32_t*>(m_pVirtAddress) = value;
    m_lastValue = *static_cast<uint32_t*>(m_pVirtAddress);
  }
}

#endif
