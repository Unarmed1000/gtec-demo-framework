/****************************************************************************************************************************************************
 * Copyright 2018 NXP
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

#include <FslBase/Exceptions.hpp>
#include <FslBase/Log/Math/LogPoint2.hpp>
#include <FslBase/Log/Math/Pixel/LogPxExtent2D.hpp>
#include <FslBase/Log/Math/Pixel/LogPxSize2D.hpp>
#include <FslGraphics/Bitmap/Bitmap.hpp>
#include <FslGraphics/Log/LogPixelFormat.hpp>
#include <FslGraphics/Log/LogStrideRequirement.hpp>
#include <FslGraphics/UnitTest/Helper/Common.hpp>
#include <FslGraphics/UnitTest/Helper/TestFixtureFslGraphics.hpp>

using namespace Fsl;

namespace
{
  using TestBitmap_Bitmap = TestFixtureFslGraphics;
}


TEST(TestBitmap_Bitmap, Construct_Default)
{
  Bitmap bitmap;
  EXPECT_FALSE(bitmap.IsValid());
  EXPECT_EQ(PxSize1D(), bitmap.Width());
  EXPECT_EQ(PxSize1D(), bitmap.Height());
  EXPECT_EQ(PxValueU(), bitmap.UnsignedWidth());
  EXPECT_EQ(PxValueU(), bitmap.UnsignedHeight());
  EXPECT_EQ(0, bitmap.RawWidth());
  EXPECT_EQ(0, bitmap.RawHeight());
  EXPECT_EQ(0u, bitmap.RawUnsignedWidth());
  EXPECT_EQ(0u, bitmap.RawUnsignedHeight());
  EXPECT_EQ(PxSize2D(), bitmap.GetSize());
  EXPECT_EQ(0u, bitmap.Stride());
  EXPECT_EQ(PxExtent2D(), bitmap.GetExtent());
  EXPECT_EQ(BitmapOrigin::UpperLeft, bitmap.GetOrigin());
  EXPECT_EQ(StrideRequirement::Any, bitmap.GetStrideRequirement());
  EXPECT_EQ(PixelFormat::Undefined, bitmap.GetPixelFormat());
  EXPECT_EQ(PixelFormatLayout::Undefined, bitmap.GetPixelFormatLayout());
  EXPECT_EQ(0u, bitmap.GetPreferredStride(PixelFormat::R8G8B8A8_UNORM));
  EXPECT_EQ(0u, bitmap.GetByteSize());
}
