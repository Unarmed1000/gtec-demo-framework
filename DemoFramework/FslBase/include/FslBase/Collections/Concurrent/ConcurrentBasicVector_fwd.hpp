#ifndef FSLBASE_COLLECTIONS_CONCURRENT_CONCURRENTBASICVECTOR_FWD_HPP
#define FSLBASE_COLLECTIONS_CONCURRENT_CONCURRENTBASICVECTOR_FWD_HPP
/****************************************************************************************************************************************************
 * Copyright 2023 NXP
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

#include <mutex>
#include <vector>

namespace Fsl
{
  template <typename T>
  class ConcurrentBasicVector
  {
    mutable std::mutex m_mutex;
    std::vector<T> m_content;

  public:
    ConcurrentBasicVector(const ConcurrentBasicVector<T>&) = delete;
    ConcurrentBasicVector& operator=(const ConcurrentBasicVector<T>&) = delete;
    ConcurrentBasicVector() = default;

    using value_type = T;

    //! @brief Clear all elements from the queue
    void Clear();

    //! @brief Add element to queue
    void Add(const T& value);

    //! @brief Extract all content of the queue into the supplied vector
    void ExtractTo(std::vector<T>& rDst) const;

    std::vector<T> ToVector() const;
  };
}

#endif
