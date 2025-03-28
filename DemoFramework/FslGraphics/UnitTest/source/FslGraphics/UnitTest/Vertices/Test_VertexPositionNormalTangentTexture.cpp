/****************************************************************************************************************************************************
 * Copyright 2018, 2022 NXP
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
#include <FslBase/Log/Math/LogMatrix.hpp>
#include <FslBase/Log/Math/LogPoint2.hpp>
#include <FslBase/Log/Math/LogVector2.hpp>
#include <FslBase/Log/Math/LogVector3.hpp>
#include <FslBase/Log/Math/LogVector4.hpp>
#include <FslGraphics/Log/LogColor.hpp>
#include <FslGraphics/UnitTest/Helper/Common.hpp>
#include <FslGraphics/UnitTest/Helper/TestFixtureFslGraphics.hpp>
#include <FslGraphics/Vertices/VertexDeclaration.hpp>
#include <FslGraphics/Vertices/VertexPositionNormalTangentTexture.hpp>
#include <cstddef>

using namespace Fsl;

namespace
{
  using TestVertices_VertexPositionNormalTangentTexture = TestFixtureFslGraphics;
}


TEST(TestVertices_VertexPositionNormalTangentTexture, Construct_Default)
{
  VertexPositionNormalTangentTexture vertex;

  EXPECT_EQ(Vector3(), vertex.Position);
  EXPECT_EQ(Vector3(), vertex.Normal);
  EXPECT_EQ(Vector3(), vertex.Tangent);
  EXPECT_EQ(Vector2(), vertex.TextureCoordinate);
}


TEST(TestVertices_VertexPositionNormalTangentTexture, GetVertexDeclaration)
{
  const VertexElement expected0(offsetof(VertexPositionNormalTangentTexture, Position), VertexElementFormat::Vector3, VertexElementUsage::Position,
                                0u);
  const VertexElement expected1(offsetof(VertexPositionNormalTangentTexture, Normal), VertexElementFormat::Vector3, VertexElementUsage::Normal, 0u);
  const VertexElement expected2(offsetof(VertexPositionNormalTangentTexture, Tangent), VertexElementFormat::Vector3, VertexElementUsage::Tangent, 0u);
  const VertexElement expected3(offsetof(VertexPositionNormalTangentTexture, TextureCoordinate), VertexElementFormat::Vector2,
                                VertexElementUsage::TextureCoordinate, 0u);
  const auto vertexDecl = VertexDeclaration(VertexPositionNormalTangentTexture::AsVertexDeclarationSpan());

  EXPECT_EQ(sizeof(VertexPositionNormalTangentTexture), vertexDecl.VertexStride());
  ASSERT_EQ(4u, vertexDecl.Count());
  EXPECT_NE(nullptr, vertexDecl.DirectAccess());

  // Get by element usage
  EXPECT_EQ(vertexDecl.VertexElementGetIndexOf(VertexElementUsage::Position, 0u), 0);
  EXPECT_EQ(vertexDecl.VertexElementIndexOf(VertexElementUsage::Position, 0u), 0);
  EXPECT_EQ(expected0, vertexDecl.VertexElementGet(VertexElementUsage::Position, 0u));

  EXPECT_EQ(vertexDecl.VertexElementGetIndexOf(VertexElementUsage::Normal, 0u), 1);
  EXPECT_EQ(vertexDecl.VertexElementIndexOf(VertexElementUsage::Normal, 0u), 1);
  EXPECT_EQ(expected1, vertexDecl.VertexElementGet(VertexElementUsage::Normal, 0u));

  EXPECT_EQ(vertexDecl.VertexElementGetIndexOf(VertexElementUsage::Tangent, 0u), 2);
  EXPECT_EQ(vertexDecl.VertexElementIndexOf(VertexElementUsage::Tangent, 0u), 2);
  EXPECT_EQ(expected2, vertexDecl.VertexElementGet(VertexElementUsage::Tangent, 0u));

  EXPECT_EQ(vertexDecl.VertexElementGetIndexOf(VertexElementUsage::TextureCoordinate, 0u), 3);
  EXPECT_EQ(vertexDecl.VertexElementIndexOf(VertexElementUsage::TextureCoordinate, 0u), 3);
  EXPECT_EQ(expected3, vertexDecl.VertexElementGet(VertexElementUsage::TextureCoordinate, 0u));

  // By index
  EXPECT_EQ(expected0, vertexDecl.At(0u));
  EXPECT_EQ(expected1, vertexDecl.At(1u));
  EXPECT_EQ(expected2, vertexDecl.At(2u));
  EXPECT_EQ(expected3, vertexDecl.At(3u));

  // Direct access should produce the same as At
  for (uint32_t i = 0; i < vertexDecl.Count(); ++i)
  {
    EXPECT_EQ(vertexDecl.At(i), vertexDecl.DirectAccess()[i]);
  }
}


TEST(TestVertices_VertexPositionNormalTangentTexture, GetVertexDeclarationArray)
{
  const VertexElement expected0(offsetof(VertexPositionNormalTangentTexture, Position), VertexElementFormat::Vector3, VertexElementUsage::Position,
                                0u);
  const VertexElement expected1(offsetof(VertexPositionNormalTangentTexture, Normal), VertexElementFormat::Vector3, VertexElementUsage::Normal, 0u);
  const VertexElement expected2(offsetof(VertexPositionNormalTangentTexture, Tangent), VertexElementFormat::Vector3, VertexElementUsage::Tangent, 0u);
  const VertexElement expected3(offsetof(VertexPositionNormalTangentTexture, TextureCoordinate), VertexElementFormat::Vector2,
                                VertexElementUsage::TextureCoordinate, 0u);
  const auto vertexDecl = VertexPositionNormalTangentTexture::GetVertexDeclarationArray();

  EXPECT_EQ(sizeof(VertexPositionNormalTangentTexture), vertexDecl.VertexStride());
  ASSERT_EQ(4u, vertexDecl.Count());
  EXPECT_NE(nullptr, vertexDecl.DirectAccess());

  // Get by element usage
  EXPECT_EQ(vertexDecl.VertexElementGetIndexOf(VertexElementUsage::Position, 0u), 0);
  EXPECT_EQ(vertexDecl.VertexElementIndexOf(VertexElementUsage::Position, 0u), 0);
  EXPECT_EQ(expected0, vertexDecl.VertexElementGet(VertexElementUsage::Position, 0u));

  EXPECT_EQ(vertexDecl.VertexElementGetIndexOf(VertexElementUsage::Normal, 0u), 1);
  EXPECT_EQ(vertexDecl.VertexElementIndexOf(VertexElementUsage::Normal, 0u), 1);
  EXPECT_EQ(expected1, vertexDecl.VertexElementGet(VertexElementUsage::Normal, 0u));

  EXPECT_EQ(vertexDecl.VertexElementGetIndexOf(VertexElementUsage::Tangent, 0u), 2);
  EXPECT_EQ(vertexDecl.VertexElementIndexOf(VertexElementUsage::Tangent, 0u), 2);
  EXPECT_EQ(expected2, vertexDecl.VertexElementGet(VertexElementUsage::Tangent, 0u));

  EXPECT_EQ(vertexDecl.VertexElementGetIndexOf(VertexElementUsage::TextureCoordinate, 0u), 3);
  EXPECT_EQ(vertexDecl.VertexElementIndexOf(VertexElementUsage::TextureCoordinate, 0u), 3);
  EXPECT_EQ(expected3, vertexDecl.VertexElementGet(VertexElementUsage::TextureCoordinate, 0u));

  // By index
  EXPECT_EQ(expected0, vertexDecl.At(0u));
  EXPECT_EQ(expected1, vertexDecl.At(1u));
  EXPECT_EQ(expected2, vertexDecl.At(2u));
  EXPECT_EQ(expected3, vertexDecl.At(3u));

  // Direct access should produce the same as At
  for (uint32_t i = 0; i < vertexDecl.Count(); ++i)
  {
    EXPECT_EQ(vertexDecl.At(i), vertexDecl.DirectAccess()[i]);
  }
}


TEST(TestVertices_VertexPositionNormalTangentTexture, AsVertexDeclarationSpan)
{
  const VertexElement expected0(offsetof(VertexPositionNormalTangentTexture, Position), VertexElementFormat::Vector3, VertexElementUsage::Position,
                                0u);
  const VertexElement expected1(offsetof(VertexPositionNormalTangentTexture, Normal), VertexElementFormat::Vector3, VertexElementUsage::Normal, 0u);
  const VertexElement expected2(offsetof(VertexPositionNormalTangentTexture, Tangent), VertexElementFormat::Vector3, VertexElementUsage::Tangent, 0u);
  const VertexElement expected3(offsetof(VertexPositionNormalTangentTexture, TextureCoordinate), VertexElementFormat::Vector2,
                                VertexElementUsage::TextureCoordinate, 0u);
  const auto vertexDecl = VertexPositionNormalTangentTexture::AsVertexDeclarationSpan();

  EXPECT_EQ(sizeof(VertexPositionNormalTangentTexture), vertexDecl.VertexStride());
  ASSERT_EQ(4u, vertexDecl.Count());
  EXPECT_NE(nullptr, vertexDecl.DirectAccess());

  // Get by element usage
  EXPECT_EQ(vertexDecl.VertexElementGetIndexOf(VertexElementUsage::Position, 0u), 0);
  EXPECT_EQ(vertexDecl.VertexElementIndexOf(VertexElementUsage::Position, 0u), 0);
  EXPECT_EQ(expected0, vertexDecl.VertexElementGet(VertexElementUsage::Position, 0u));

  EXPECT_EQ(vertexDecl.VertexElementGetIndexOf(VertexElementUsage::Normal, 0u), 1);
  EXPECT_EQ(vertexDecl.VertexElementIndexOf(VertexElementUsage::Normal, 0u), 1);
  EXPECT_EQ(expected1, vertexDecl.VertexElementGet(VertexElementUsage::Normal, 0u));

  EXPECT_EQ(vertexDecl.VertexElementGetIndexOf(VertexElementUsage::Tangent, 0u), 2);
  EXPECT_EQ(vertexDecl.VertexElementIndexOf(VertexElementUsage::Tangent, 0u), 2);
  EXPECT_EQ(expected2, vertexDecl.VertexElementGet(VertexElementUsage::Tangent, 0u));

  EXPECT_EQ(vertexDecl.VertexElementGetIndexOf(VertexElementUsage::TextureCoordinate, 0u), 3);
  EXPECT_EQ(vertexDecl.VertexElementIndexOf(VertexElementUsage::TextureCoordinate, 0u), 3);
  EXPECT_EQ(expected3, vertexDecl.VertexElementGet(VertexElementUsage::TextureCoordinate, 0u));

  // By index
  EXPECT_EQ(expected0, vertexDecl.At(0u));
  EXPECT_EQ(expected1, vertexDecl.At(1u));
  EXPECT_EQ(expected2, vertexDecl.At(2u));
  EXPECT_EQ(expected3, vertexDecl.At(3u));

  // Direct access should produce the same as At
  for (uint32_t i = 0; i < vertexDecl.Count(); ++i)
  {
    EXPECT_EQ(vertexDecl.At(i), vertexDecl.DirectAccess()[i]);
  }
}
