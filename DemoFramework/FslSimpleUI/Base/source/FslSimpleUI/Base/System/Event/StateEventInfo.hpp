#ifndef FSLSIMPLEUI_BASE_SYSTEM_EVENT_STATEEVENTINFO_HPP
#define FSLSIMPLEUI_BASE_SYSTEM_EVENT_STATEEVENTINFO_HPP
/****************************************************************************************************************************************************
 * Copyright (c) 2015 Freescale Semiconductor, Inc.
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

#include <FslBase/Math/Pixel/PxPoint2.hpp>
#include <FslBase/Time/MillisecondTickCount32.hpp>
#include <FslSimpleUI/Base/Event/EventTransactionState.hpp>

namespace Fsl::UI
{
  struct StateEventInfo
  {
  private:
    MillisecondTickCount32 m_timestamp;
    int32_t m_sourceId{0};
    int32_t m_sourceSubId{0};
    EventTransactionState m_transactionState{EventTransactionState::End};
    bool m_isRepeat{false};
    PxPoint2 m_param1;
    int32_t m_tag{0};

  public:
    StateEventInfo() = default;

    StateEventInfo(const MillisecondTickCount32 timestamp, const int32_t sourceId, const int32_t sourceSubId,
                   const EventTransactionState transactionState, const bool isRepeat)
      : m_timestamp(timestamp)
      , m_sourceId(sourceId)
      , m_sourceSubId(sourceSubId)
      , m_transactionState(transactionState)
      , m_isRepeat(isRepeat)
    {
    }


    StateEventInfo(const MillisecondTickCount32 timestamp, const int32_t sourceId, const int32_t sourceSubId,
                   const EventTransactionState transactionState, const bool isRepeat, const int32_t tag)
      : m_timestamp(timestamp)
      , m_sourceId(sourceId)
      , m_sourceSubId(sourceSubId)
      , m_transactionState(transactionState)
      , m_isRepeat(isRepeat)
      , m_tag(tag)
    {
    }

    MillisecondTickCount32 Timestamp() const
    {
      return m_timestamp;
    }

    int32_t SourceId() const noexcept
    {
      return m_sourceId;
    }

    int32_t SourceSubId() const noexcept
    {
      return m_sourceSubId;
    }

    EventTransactionState TransactionState() const noexcept
    {
      return m_transactionState;
    }

    bool IsBegin() const noexcept
    {
      return m_transactionState == EventTransactionState::Begin;
    }
    bool IsCancel() const noexcept
    {
      return m_transactionState == EventTransactionState::Canceled;
    }
    bool IsEnd() const noexcept
    {
      return m_transactionState == EventTransactionState::End;
    }
    bool IsRepeat() const noexcept
    {
      return m_isRepeat;
    }

    int32_t Tag() const noexcept
    {
      return m_tag;
    }

    PxPoint2 Param1() const noexcept
    {
      return m_param1;
    }
    void SetParam1(const PxPoint2& value)
    {
      m_param1 = value;
    }

    void Clear() noexcept
    {
      m_transactionState = EventTransactionState::Canceled;
      m_tag = 0;
    }
  };
}

#endif
