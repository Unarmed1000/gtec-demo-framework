#ifndef OPENCL_RGB2HSV_RGB2HSV_HPP
#define OPENCL_RGB2HSV_RGB2HSV_HPP
/****************************************************************************************************************************************************
 * Copyright 2024 NXP
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

#include <FslDemoApp/OpenCL/DemoAppOpenCL.hpp>
#include <FslGraphics/Bitmap/TightBitmap.hpp>
#include <FslUtil/OpenCL1_2/ContextEx.hpp>
#include <RapidOpenCL1/Buffer.hpp>
#include <RapidOpenCL1/CommandQueue.hpp>
#include <RapidOpenCL1/Kernel.hpp>
#include <RapidOpenCL1/UserEvent.hpp>
#include <vector>

namespace Fsl
{
  class Rgb2Hsv final : public DemoAppOpenCL
  {
    OpenCL::ContextEx m_context;
    IO::Path m_hsvOutputPath;
    IO::Path m_rgbOutputPath;
    IO::Path m_bmpPath;
    TightBitmap m_srcBitmap;

  public:
    explicit Rgb2Hsv(const DemoAppConfig& config);
    ~Rgb2Hsv() final;

  protected:
    void Run() final;
    void CopyToBMP(Bitmap& bitmap, const IO::Path& fileName, const void* ptr);
  };
}

#endif
