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

// OpenCL 1.2 project

#include "MorphoErode.hpp"
#include <FslBase/Exceptions.hpp>
#include <FslBase/Log/Log3Fmt.hpp>
#include <FslBase/Math/TypeConverter_Double.hpp>
#include <FslBase/Span/SpanUtil_Vector.hpp>
#include <FslBase/System/HighResolutionTimer.hpp>
#include <FslBase/System/HighResolutionTimerUtil.hpp>
#include <FslBase/Time/TimeSpanUtil.hpp>
#include <FslGraphics/Bitmap/Bitmap.hpp>
#include <FslUtil/OpenCL1_2/OpenCLHelper.hpp>
#include <FslUtil/OpenCL1_2/ProgramEx.hpp>
#include <Shared/OpenCL/Helper/BitmapHelper.hpp>
#include <Shared/OpenCL/Helper/TightPlanarBitmapR8G8B8.hpp>
#include <CL/cl.h>
#include <array>
#include <cstring>
#include "OptionParser.hpp"

namespace Fsl
{
  namespace
  {
    namespace BufferConfig
    {
      constexpr std::size_t SrcBufferIndex = 0;
      constexpr std::size_t DstBufferIndex = 1;
      // The maximum amount of buffers used
      constexpr std::size_t MaxBufferCount = 2;
    }

    std::vector<RapidOpenCL1::Buffer> AllocateMemory(const cl_context context, const std::size_t numPixels)
    {
      std::vector<RapidOpenCL1::Buffer> deviceImg(BufferConfig::MaxBufferCount);

      deviceImg[BufferConfig::SrcBufferIndex].Reset(context, CL_MEM_READ_ONLY, numPixels, nullptr);
      deviceImg[BufferConfig::DstBufferIndex].Reset(context, CL_MEM_WRITE_ONLY, numPixels, nullptr);
      return deviceImg;
    }
  }


  MorphoErode::MorphoErode(const DemoAppConfig& config)
    : DemoAppOpenCL(config)
    , m_context(CL_DEVICE_TYPE_GPU)
  {
    auto optionParser = config.GetOptions<OptionParser>();
    m_bmpPath = IO::Path(optionParser->GetBmpFileName());
    m_morphoOutputPath = IO::Path(optionParser->GetOutputFileName());

    m_srcBitmap = TightBitmap(GetContentManager()->ReadBitmap(m_bmpPath, PixelFormat::R8G8B8_UNORM).Release());
  }


  MorphoErode::~MorphoErode() = default;


  void MorphoErode::Run()
  {
    const auto imgWidth = UncheckedNumericCast<cl_int>(m_srcBitmap.RawWidth());
    const auto imgHeight = UncheckedNumericCast<cl_int>(m_srcBitmap.RawHeight());

    const std::size_t srcPixelCount = m_srcBitmap.GetPixelCount().RawUnsignedValue();
    std::vector<RapidOpenCL1::Buffer> deviceImg = AllocateMemory(m_context.Get(), srcPixelCount);
    const std::string strProgram = GetContentManager()->ReadAllText("morpho_math.cl");

    OpenCL::ProgramEx program(m_context.Get(), m_context.GetDeviceId(), strProgram);

    FSLLOG3_INFO("Creating kernels...");
    FSLLOG3_INFO("Please wait for compiling and building kernels, about one minute...");
    RapidOpenCL1::Kernel kernels(program.Get(), "erode_op");
    clSetKernelArg(kernels.Get(), 0, sizeof(cl_mem), deviceImg[BufferConfig::SrcBufferIndex].GetPointer());
    clSetKernelArg(kernels.Get(), 1, sizeof(cl_mem), deviceImg[BufferConfig::DstBufferIndex].GetPointer());
    clSetKernelArg(kernels.Get(), 2, sizeof(cl_int), &imgWidth);
    clSetKernelArg(kernels.Get(), 3, sizeof(cl_int), &imgHeight);

    Bitmap finalImage;
    TightBitmap imageGray;
    {
      constexpr const int Dimension = 2;
      const std::array<std::size_t, Dimension> global = {m_srcBitmap.RawUnsignedWidth(), m_srcBitmap.RawUnsignedHeight()};
      constexpr std::array<std::size_t, Dimension> Local = {4, 16};

      FSLLOG3_INFO("Creating Command Queue...");
      RapidOpenCL1::CommandQueue commandQueue(m_context.Get(), m_context.GetDeviceId(), 0);

      FSLLOG3_INFO("Convert RGB to Gray...");
      imageGray = BitmapHelper::R8G8B8ToGrayscaleLuminanceNTSC(m_srcBitmap);

      Span<uint8_t> imageGraySpan = imageGray.AsSpan();
      HighResolutionTimer timer;
      auto startTime = timer.GetNativeTicks();
      RAPIDOPENCL_CHECK(clEnqueueWriteBuffer(commandQueue.Get(), deviceImg[BufferConfig::SrcBufferIndex].Get(), CL_TRUE, 0, imageGraySpan.size(),
                                             imageGraySpan.data(), 0, nullptr, nullptr));
      RAPIDOPENCL_CHECK(
        clEnqueueNDRangeKernel(commandQueue.Get(), kernels.Get(), Dimension, nullptr, global.data(), Local.data(), 0, nullptr, nullptr));
      RAPIDOPENCL_CHECK(clEnqueueReadBuffer(commandQueue.Get(), deviceImg[BufferConfig::DstBufferIndex].Get(), CL_TRUE, 0, imageGraySpan.size(),
                                            imageGraySpan.data(), 0, nullptr, nullptr));
      clFinish(commandQueue.Get());
      auto endTime = timer.GetNativeTicks();
      TimeSpan timeSpan = HighResolutionTimerUtil::ToTimeSpan(endTime - startTime, timer.GetNativeTickFrequency());
      FSLLOG3_INFO("Process time: {} ms", TimeSpanUtil::ToMillisecondsUInt64(timeSpan));
    }
    FSLLOG3_INFO("Convert Gray to RGB...");
    finalImage = BitmapHelper::Gray8ToR8G8B8AsBitmap(imageGray);

    FSLLOG3_INFO("Saving images...");
    GetPersistentDataManager()->Write(m_morphoOutputPath, finalImage, ImageFormat::Bmp, PixelFormat::B8G8R8_UNORM);
  }
}
