/****************************************************************************************************************************************************
 * Copyright 2018, 2023-2024 NXP
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

#include <FslBase/Log/Math/Pixel/LogPxExtent2D.hpp>
#include <FslBase/Log/Math/Pixel/LogPxExtent3D.hpp>
#include <FslBase/Log/Math/Pixel/LogPxPoint2.hpp>
#include <FslBase/Log/Math/Pixel/LogPxSize2D.hpp>
#include <FslGraphics/Exceptions.hpp>
#include <FslGraphics/Log/LogPixelFormat.hpp>
#include <FslGraphics/Render/Texture2D.hpp>
#include <FslGraphics/UnitTest/Helper/Common.hpp>
#include <FslGraphics/UnitTest/Helper/Render/NativeGraphicsTestImpl.hpp>
#include <FslGraphics/UnitTest/Helper/TestFixtureFslGraphics.hpp>

using namespace Fsl;

namespace
{
  using TestRender_Texture2D = TestFixtureFslGraphics;
}


TEST(TestRender_Texture2D, Construct_Empty)
{
  Texture2D texture;

  EXPECT_FALSE(texture.IsValid());
  EXPECT_EQ(PxExtent2D(), texture.GetExtent());
  EXPECT_EQ(PxSize2D(), texture.GetSize());
  EXPECT_EQ(PixelFormat::Undefined, texture.GetPixelFormat());
  EXPECT_EQ(std::shared_ptr<INativeTexture2D>(), texture.TryGetNative());
  EXPECT_THROW(texture.GetNative(), GraphicsException);
}


TEST(TestRender_Texture2D, Construct_Bitmap)
{
  const auto nativeGraphics = std::make_shared<NativeGraphicsTestImpl>();
  const Bitmap bitmap(PxSize2D::Create(128, 128), PixelFormat::R8G8B8A8_UNORM);
  Texture2D texture(nativeGraphics, bitmap, Texture2DFilterHint::Smooth);

  EXPECT_TRUE(texture.IsValid());
  EXPECT_EQ(bitmap.GetExtent(), texture.GetExtent());
  EXPECT_EQ(bitmap.GetSize(), texture.GetSize());
  EXPECT_EQ(bitmap.GetPixelFormat(), texture.GetPixelFormat());
  EXPECT_NE(std::shared_ptr<INativeTexture2D>(), texture.TryGetNative());
  EXPECT_EQ(texture.TryGetNative(), texture.GetNative());
}

TEST(TestRender_Texture2D, Construct_RawBitmap)
{
  const auto nativeGraphics = std::make_shared<NativeGraphicsTestImpl>();
  const Bitmap bitmap(PxSize2D::Create(128, 128), PixelFormat::R8G8B8A8_UNORM);
  const Bitmap::ScopedDirectReadAccess directAccess(bitmap);
  Texture2D texture(nativeGraphics, directAccess.AsRawBitmap(), Texture2DFilterHint::Smooth);

  EXPECT_TRUE(texture.IsValid());
  EXPECT_EQ(bitmap.GetExtent(), texture.GetExtent());
  EXPECT_EQ(bitmap.GetSize(), texture.GetSize());
  EXPECT_EQ(bitmap.GetPixelFormat(), texture.GetPixelFormat());
  EXPECT_NE(std::shared_ptr<INativeTexture2D>(), texture.TryGetNative());
  EXPECT_EQ(texture.TryGetNative(), texture.GetNative());
}


TEST(TestRender_Texture2D, Construct_Texture)
{
  const auto nativeGraphics = std::make_shared<NativeGraphicsTestImpl>();
  const Texture srcTexture(PxExtent2D::Create(128, 128), PixelFormat::R8G8B8A8_UNORM, BitmapOrigin::UpperLeft);
  Texture2D texture(nativeGraphics, srcTexture, Texture2DFilterHint::Smooth);

  auto extent = srcTexture.GetExtent();

  EXPECT_TRUE(texture.IsValid());
  EXPECT_EQ(PxExtent2D(extent.Width, extent.Height), texture.GetExtent());
  EXPECT_EQ(PxSize2D::Create(static_cast<int32_t>(srcTexture.GetExtent().Width.Value), static_cast<int32_t>(srcTexture.GetExtent().Height.Value)),
            texture.GetSize());
  EXPECT_EQ(srcTexture.GetPixelFormat(), texture.GetPixelFormat());
  EXPECT_NE(std::shared_ptr<INativeTexture2D>(), texture.TryGetNative());
  EXPECT_EQ(texture.TryGetNative(), texture.GetNative());
}


TEST(TestRender_Texture2D, Construct_ReadOnlyRawTexture)
{
  const auto nativeGraphics = std::make_shared<NativeGraphicsTestImpl>();
  const Texture srcTexture(PxExtent2D::Create(128, 128), PixelFormat::R8G8B8A8_UNORM, BitmapOrigin::UpperLeft);
  Texture::ScopedDirectReadAccess directAccess(srcTexture);
  Texture2D texture(nativeGraphics, directAccess.AsRawTexture(), Texture2DFilterHint::Smooth);

  auto extent = srcTexture.GetExtent();

  EXPECT_TRUE(texture.IsValid());
  EXPECT_EQ(PxExtent2D(extent.Width, extent.Height), texture.GetExtent());
  EXPECT_EQ(PxSize2D::Create(static_cast<int32_t>(srcTexture.GetExtent().Width.Value), static_cast<int32_t>(srcTexture.GetExtent().Height.Value)),
            texture.GetSize());
  EXPECT_EQ(srcTexture.GetPixelFormat(), texture.GetPixelFormat());
  EXPECT_NE(std::shared_ptr<INativeTexture2D>(), texture.TryGetNative());
  EXPECT_EQ(texture.TryGetNative(), texture.GetNative());
}


TEST(TestRender_Texture2D, Reset)
{
  const auto nativeGraphics = std::make_shared<NativeGraphicsTestImpl>();
  const Bitmap bitmap(PxSize2D::Create(128, 128), PixelFormat::R8G8B8A8_UNORM);
  Texture2D texture(nativeGraphics, bitmap, Texture2DFilterHint::Smooth);

  EXPECT_TRUE(texture.IsValid());

  texture.Reset();

  EXPECT_FALSE(texture.IsValid());
  EXPECT_EQ(PxExtent2D(), texture.GetExtent());
  EXPECT_EQ(PxSize2D(), texture.GetSize());
  EXPECT_EQ(PixelFormat::Undefined, texture.GetPixelFormat());
  EXPECT_EQ(std::shared_ptr<INativeTexture2D>(), texture.TryGetNative());
  EXPECT_THROW(texture.GetNative(), GraphicsException);
}


TEST(TestRender_Texture2D, Reset_EmptyWithBitmap)
{
  auto nativeGraphics = std::make_shared<NativeGraphicsTestImpl>();
  Texture2D texture;
  ASSERT_FALSE(texture.IsValid());

  const Bitmap bitmap2(PxSize2D::Create(64, 64), PixelFormat::R8G8B8_UNORM);
  texture.Reset(nativeGraphics, bitmap2, Texture2DFilterHint::Nearest);

  EXPECT_TRUE(texture.IsValid());
  EXPECT_EQ(bitmap2.GetExtent(), texture.GetExtent());
  EXPECT_EQ(bitmap2.GetSize(), texture.GetSize());
  EXPECT_EQ(bitmap2.GetPixelFormat(), texture.GetPixelFormat());
  EXPECT_NE(std::shared_ptr<INativeTexture2D>(), texture.TryGetNative());
  EXPECT_EQ(texture.TryGetNative(), texture.GetNative());
}


TEST(TestRender_Texture2D, Reset_NotEmptyWithBitmap)
{
  auto nativeGraphics = std::make_shared<NativeGraphicsTestImpl>();
  const Bitmap bitmap1(PxSize2D::Create(128, 128), PixelFormat::R8G8B8A8_UNORM);
  Texture2D texture(nativeGraphics, bitmap1, Texture2DFilterHint::Smooth);
  ASSERT_TRUE(texture.IsValid());

  const Bitmap bitmap2(PxSize2D::Create(64, 64), PixelFormat::R8G8B8_UNORM);
  texture.Reset(nativeGraphics, bitmap2, Texture2DFilterHint::Nearest);

  EXPECT_TRUE(texture.IsValid());
  EXPECT_EQ(bitmap2.GetExtent(), texture.GetExtent());
  EXPECT_EQ(bitmap2.GetSize(), texture.GetSize());
  EXPECT_EQ(bitmap2.GetPixelFormat(), texture.GetPixelFormat());
  EXPECT_NE(std::shared_ptr<INativeTexture2D>(), texture.TryGetNative());
  EXPECT_EQ(texture.TryGetNative(), texture.GetNative());
}


TEST(TestRender_Texture2D, Reset_EmptyWithTexture)
{
  auto nativeGraphics = std::make_shared<NativeGraphicsTestImpl>();
  Texture2D texture;
  ASSERT_FALSE(texture.IsValid());

  const Texture srcTexture(PxExtent2D::Create(64, 64), PixelFormat::R8G8B8_UNORM, BitmapOrigin::UpperLeft);
  texture.Reset(nativeGraphics, srcTexture, Texture2DFilterHint::Nearest);

  auto extent = srcTexture.GetExtent();

  EXPECT_TRUE(texture.IsValid());
  EXPECT_EQ(PxExtent2D(extent.Width, extent.Height), texture.GetExtent());
  EXPECT_EQ(PxSize2D::Create(static_cast<int32_t>(srcTexture.GetExtent().Width.Value), static_cast<int32_t>(srcTexture.GetExtent().Height.Value)),
            texture.GetSize());
  EXPECT_EQ(srcTexture.GetPixelFormat(), texture.GetPixelFormat());
  EXPECT_NE(std::shared_ptr<INativeTexture2D>(), texture.TryGetNative());
  EXPECT_EQ(texture.TryGetNative(), texture.GetNative());
}


TEST(TestRender_Texture2D, Reset_NotEmptyWithTexture)
{
  auto nativeGraphics = std::make_shared<NativeGraphicsTestImpl>();
  const Bitmap bitmap1(PxSize2D::Create(128, 128), PixelFormat::R8G8B8A8_UNORM);
  Texture2D texture(nativeGraphics, bitmap1, Texture2DFilterHint::Smooth);
  ASSERT_TRUE(texture.IsValid());

  const Texture srcTexture(PxExtent2D::Create(64, 64), PixelFormat::R8G8B8_UNORM, BitmapOrigin::UpperLeft);
  texture.Reset(nativeGraphics, srcTexture, Texture2DFilterHint::Nearest);

  auto extent = srcTexture.GetExtent();

  EXPECT_TRUE(texture.IsValid());
  EXPECT_EQ(PxExtent2D(extent.Width, extent.Height), texture.GetExtent());
  EXPECT_EQ(PxSize2D::Create(static_cast<int32_t>(srcTexture.GetExtent().Width.Value), static_cast<int32_t>(srcTexture.GetExtent().Height.Value)),
            texture.GetSize());
  EXPECT_EQ(srcTexture.GetPixelFormat(), texture.GetPixelFormat());
  EXPECT_NE(std::shared_ptr<INativeTexture2D>(), texture.TryGetNative());
  EXPECT_EQ(texture.TryGetNative(), texture.GetNative());
}
