#ifndef FSLBASE_BITS_BYTESPANUTIL_READLE_HPP
#define FSLBASE_BITS_BYTESPANUTIL_READLE_HPP
/****************************************************************************************************************************************************
 * Copyright 2020, 2022, 2024-2025 NXP
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

#include <FslBase/BasicTypes.hpp>
#include <FslBase/Span/Span.hpp>
#include <bit>
#include <cassert>

namespace Fsl::ByteSpanUtil
{
  //! @brief Read a uint8_t from the given index in little endian format
  //! @note Yes this method does not make much sense, but its here to follow the style of the other methods and
  //!       it does some basic validation on the input params like the rest of the methods
  constexpr inline uint8_t ReadUInt8LE(const ReadOnlySpan<uint8_t> src)
  {
    assert(src.size() >= 1);
    return src[0];
  }

  //! @brief Read a int8_t from the given index in little endian format
  //! @note Yes this method does not make much sense, but its here to follow the style of the other methods and
  //!       it does some basic validation on the input params like the rest of the methods
  constexpr inline int8_t ReadInt8LE(const ReadOnlySpan<uint8_t> src)
  {
    assert(src.size() >= 1);
    return static_cast<int8_t>(src[0]);
  }

  //! @brief Read a uint16_t from the given index in little endian format
  constexpr inline uint16_t ReadUInt16LE(const ReadOnlySpan<uint8_t> src)
  {
    assert(src.size() >= 2);
    return static_cast<uint16_t>(static_cast<uint16_t>(src[0]) | (static_cast<uint16_t>(src[1]) << 8));
  }

  //! @brief Read a int16_t from the given index in little endian format
  constexpr inline int16_t ReadInt16LE(const ReadOnlySpan<uint8_t> src)
  {
    assert(src.size() >= 2);
    return static_cast<int16_t>(static_cast<uint16_t>(src[0]) | (static_cast<uint16_t>(src[1]) << 8));
  }

  //! @brief Read a uint32_t from the given index in little endian format
  constexpr inline uint32_t ReadUInt32LE(const ReadOnlySpan<uint8_t> src)
  {
    assert(src.size() >= 4);
    return static_cast<uint32_t>(static_cast<uint32_t>(src[0]) | (static_cast<uint32_t>(src[1]) << 8) | (static_cast<uint32_t>(src[2]) << 16) |
                                 (static_cast<uint32_t>(src[3]) << 24));
  }

  //! @brief Read a int32_t from the given index in little endian format
  constexpr inline int32_t ReadInt32LE(const ReadOnlySpan<uint8_t> src)
  {
    assert(src.size() >= 4);
    return static_cast<int32_t>(static_cast<uint32_t>(src[0]) | (static_cast<uint32_t>(src[1]) << 8) | (static_cast<uint32_t>(src[2]) << 16) |
                                (static_cast<uint32_t>(src[3]) << 24));
  }

  //! @brief Read a uint64_t from the given index in little endian format
  constexpr inline uint64_t ReadUInt64LE(const ReadOnlySpan<uint8_t> src)
  {
    assert(src.size() >= 8);
    return static_cast<uint64_t>(static_cast<uint64_t>(src[0]) | (static_cast<uint64_t>(src[1]) << 8) | (static_cast<uint64_t>(src[2]) << 16) |
                                 (static_cast<uint64_t>(src[3]) << 24) | (static_cast<uint64_t>(src[4]) << 32) |
                                 (static_cast<uint64_t>(src[5]) << 40) | (static_cast<uint64_t>(src[6]) << 48) |
                                 (static_cast<uint64_t>(src[7]) << 56));
  }

  //! @brief Read a int64_t from the given index in little endian format
  constexpr inline int64_t ReadInt64LE(const ReadOnlySpan<uint8_t> src)
  {
    assert(src.size() >= 8);
    return static_cast<int64_t>(static_cast<int64_t>(src[0]) | (static_cast<int64_t>(src[1]) << 8) | (static_cast<int64_t>(src[2]) << 16) |
                                (static_cast<int64_t>(src[3]) << 24) | (static_cast<int64_t>(src[4]) << 32) | (static_cast<int64_t>(src[5]) << 40) |
                                (static_cast<int64_t>(src[6]) << 48) | (static_cast<int64_t>(src[7]) << 56));
  }


  //! @brief Read a float from the given index in little endian format
  constexpr inline float ReadFloatLE(const ReadOnlySpan<uint8_t> src)
  {
    assert(src.size() >= 4);
    // Read the LE encoded float bit-pattern into a uint32 then bit_cast it to a float
    return std::bit_cast<float>(ReadUInt32LE(src));
  }


  //! @brief Read a double from the given index in little endian format
  constexpr inline double ReadDoubleLE(const ReadOnlySpan<uint8_t> src)
  {
    assert(src.size() >= 8);
    // Read the LE encoded double bit-pattern into a uint64 then bit_cast it to a double
    return std::bit_cast<double>(ReadUInt64LE(src));
  }

  // -----------------------------------------------------------------------------------------------------------------------------------------------
  // Source span with index
  // -----------------------------------------------------------------------------------------------------------------------------------------------

  //! @brief Read a uint8_t from the given index in little endian format
  //! @note Yes this method does not make much sense, but its here to follow the style of the other methods and
  //!       it does some basic validation on the input params like the rest of the methods
  constexpr inline uint8_t ReadUInt8LE(const ReadOnlySpan<uint8_t> src, const Span<uint8_t>::size_type index)
  {
    assert(index < src.size());
    return ReadUInt8LE(src.subspan(index));
  }

  //! @brief Read a int8_t from the given index in little endian format
  //! @note Yes this method does not make much sense, but its here to follow the style of the other methods and
  //!       it does some basic validation on the input params like the rest of the methods
  constexpr inline int8_t ReadInt8LE(const ReadOnlySpan<uint8_t> src, const Span<uint8_t>::size_type index)
  {
    assert(index < src.size());
    return ReadInt8LE(src.subspan(index));
  }

  //! @brief Read a uint16_t from the given index in little endian format
  constexpr inline uint16_t ReadUInt16LE(const ReadOnlySpan<uint8_t> src, const Span<uint8_t>::size_type index)
  {
    assert(index < src.size());
    return ReadUInt16LE(src.subspan(index));
  }

  //! @brief Read a int16_t from the given index in little endian format
  constexpr inline int16_t ReadInt16LE(const ReadOnlySpan<uint8_t> src, const Span<uint8_t>::size_type index)
  {
    assert(index < src.size());
    return ReadInt16LE(src.subspan(index));
  }

  //! @brief Read a uint32_t from the given index in little endian format
  constexpr inline uint32_t ReadUInt32LE(const ReadOnlySpan<uint8_t> src, const Span<uint8_t>::size_type index)
  {
    assert(index < src.size());
    return ReadUInt32LE(src.subspan(index));
  }

  //! @brief Read a int32_t from the given index in little endian format
  constexpr inline int32_t ReadInt32LE(const ReadOnlySpan<uint8_t> src, const Span<uint8_t>::size_type index)
  {
    assert(index < src.size());
    return ReadInt32LE(src.subspan(index));
  }

  constexpr inline uint64_t ReadUInt64LE(const ReadOnlySpan<uint8_t> src, const Span<uint8_t>::size_type index)
  {
    assert(index < src.size());
    return ReadUInt64LE(src.subspan(index));
  }


  constexpr inline int64_t ReadInt64LE(const ReadOnlySpan<uint8_t> src, const Span<uint8_t>::size_type index)
  {
    assert(index < src.size());
    return ReadInt64LE(src.subspan(index));
  }


  constexpr inline float ReadFloatLE(const ReadOnlySpan<uint8_t> src, const Span<uint8_t>::size_type index)
  {
    assert(index < src.size());
    return ReadFloatLE(src.subspan(index));
  }


  constexpr inline double ReadDoubleLE(const ReadOnlySpan<uint8_t> src, const Span<uint8_t>::size_type index)
  {
    assert(index < src.size());
    return ReadDoubleLE(src.subspan(index));
  }
}

#endif
