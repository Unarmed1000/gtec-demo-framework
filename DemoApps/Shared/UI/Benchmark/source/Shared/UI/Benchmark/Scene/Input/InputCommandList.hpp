#ifndef SHARED_UI_BENCHMARK_SCENE_INPUT_INPUTCOMMANDLIST_HPP
#define SHARED_UI_BENCHMARK_SCENE_INPUT_INPUTCOMMANDLIST_HPP
/****************************************************************************************************************************************************
 * Copyright 2021 NXP
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

#include <FslBase/Span/ReadOnlySpan.hpp>
#include <Shared/UI/Benchmark/Persistence/Input/InputCommandRecord.hpp>
#include <memory>
#include <vector>

namespace Fsl
{
  class InputCommandList
  {
    uint32_t m_frameIndex{0};
    std::vector<InputCommandRecord> m_input;
    std::size_t m_inputEntries{0};

  public:
    InputCommandList();

    void SetRecording(ReadOnlySpan<InputCommandRecord> span, const uint32_t frameCount);
    ReadOnlySpan<InputCommandRecord> GetCommandSpan() const;
    ReadOnlySpan<InputCommandRecord> GetCommandSpanForFrame(const uint32_t frameIndex) const;

    uint32_t FrameCount() const;

    void Clear();
    void NextFrameIndex();

    void AddMouseButtonUp(const MillisecondTickCount32 timestamp, const CustomWindowId windowId, const PxRectangle windowRectPx,
                          const PxPoint2 mousePosition, const bool isTouch);
    void AddMouseButtonDown(const MillisecondTickCount32 timestamp, const CustomWindowId windowId, const PxRectangle windowRectPx,
                            const PxPoint2 mousePosition, const bool isTouch);
    void AddMouseMoveWhileDown(const MillisecondTickCount32 timestamp, const CustomWindowId windowId, const PxRectangle windowRectPx,
                               const PxPoint2 mousePosition, const bool isTouch);
    void AddMouseMove(const MillisecondTickCount32 timestamp, const CustomWindowId windowId, const PxRectangle windowRectPx,
                      const PxPoint2 mousePosition, const bool isTouch);
    void AddMouseMoveDone(const MillisecondTickCount32 timestamp);

  private:
    void Enqueue(const InputCommandRecord record);
  };
}

#endif
