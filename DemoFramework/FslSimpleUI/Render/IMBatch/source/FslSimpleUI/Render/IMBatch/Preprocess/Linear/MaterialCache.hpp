#ifndef FSLSIMPLEUI_RENDER_IMBATCH_PREPROCESS_LINEAR_MATERIALCACHE_HPP
#define FSLSIMPLEUI_RENDER_IMBATCH_PREPROCESS_LINEAR_MATERIALCACHE_HPP
/****************************************************************************************************************************************************
 * Copyright 2021-2022 NXP
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

#include <FslBase/Span/Span.hpp>
#include <FslBase/Span/SpanUtil.hpp>
#include <FslBase/UncheckedNumericCast.hpp>
#include <FslGraphics2D/Procedural/Batcher/BatchMaterialHandle.hpp>
#include <cassert>
#include <vector>
#include "MaterialCacheRecord.hpp"

namespace Fsl::UI::RenderIMBatch
{
  class MaterialCache
  {
    std::vector<MaterialCacheRecord> m_cache;
    std::size_t m_currentCapacity{0};

  public:
    void EnsureCapacity(const uint32_t minCapacity)
    {
      if (minCapacity > m_cache.size())
      {
        constexpr std::size_t GrowthChunkSize = 64;
        const std::size_t newCapacity = ((minCapacity / GrowthChunkSize) + ((minCapacity % GrowthChunkSize) != 0u ? 1 : 0)) * GrowthChunkSize;
        assert(newCapacity >= minCapacity);
        m_currentCapacity = newCapacity;
        // *2 because we need two buffers
        m_cache.resize(newCapacity * 2);
      }
    }

    Span<MaterialCacheRecord> GetOpaqueCacheSpan(const uint32_t count) noexcept
    {
      assert(count <= m_currentCapacity && m_currentCapacity <= m_cache.size());
      return SpanUtil::UncheckedFirstSpan(m_cache, count);
    }

    Span<MaterialCacheRecord> GetTransparentCacheSpan(const uint32_t count) noexcept
    {
      assert((m_currentCapacity + count) <= m_cache.size());
      return SpanUtil::UncheckedAsSpan(m_cache, m_currentCapacity, count);
    }
  };
}
#endif
