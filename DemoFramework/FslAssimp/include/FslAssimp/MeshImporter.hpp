#ifndef FSLASSIMP_MESHIMPORTER_HPP
#define FSLASSIMP_MESHIMPORTER_HPP
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

#include <FslBase/BasicTypes.hpp>
#include <FslBase/Span/Span.hpp>
#include <FslBase/Span/TypedFlexSpan.hpp>
#include <FslGraphics/Color.hpp>
#include <FslGraphics/PrimitiveType.hpp>
#include <FslGraphics3D/BasicScene/MeshAllocator.hpp>
#include <FslGraphics3D/BasicScene/MeshAllocatorFunc.hpp>
#include <assimp/mesh.h>
#include <memory>

namespace Fsl
{
  struct Vector3;

  namespace Graphics3D
  {
    class Mesh;
  }

  //! @note the 'Fast' methods only verify the input with asserts, so be careful if you use them
  class MeshImporter
  {
  public:
    MeshImporter();
    explicit MeshImporter(Graphics3D::MeshAllocatorFunc meshAllocator);

    Graphics3D::MeshAllocatorFunc GetDefaultAllocator() const;

    //! @brief Extract the mesh
    //! @param pSrcMesh the mesh to extract
    std::shared_ptr<Graphics3D::Mesh> ExtractMesh(const aiMesh* const pSrcMesh);

    //! @brief Extract the mesh and scale it according to 'scale'
    //! @param pSrcMesh the mesh to extract
    std::shared_ptr<Graphics3D::Mesh> ExtractMesh(const aiMesh* const pSrcMesh, const Vector3& positionMod, const Vector3& scale);

    //! @brief Extract the mesh
    //! @param pSrcMesh the mesh to extract
    static std::shared_ptr<Graphics3D::Mesh> ExtractMesh(const Graphics3D::MeshAllocatorFunc& meshAllocator, const aiMesh* const pSrcMesh);

    //! @brief Extract the mesh and scale it according to 'scale'
    //! @param pSrcMesh the mesh to extract
    //! @param meshAllocator the mesh allocator to use.
    static std::shared_ptr<Graphics3D::Mesh> ExtractMesh(const Graphics3D::MeshAllocatorFunc& meshAllocator, const aiMesh* const pSrcMesh,
                                                         const Vector3& positionMod, const Vector3& scale);

    template <typename TMesh>
    std::shared_ptr<TMesh> ExtractMesh(const aiMesh* const pSrcMesh)
    {
      Graphics3D::MeshAllocatorFunc meshAllocator(Graphics3D::MeshAllocator::Allocate<TMesh>);
      auto res = std::dynamic_pointer_cast<TMesh>(ExtractMesh(meshAllocator, pSrcMesh));
      if (!res)
      {
        throw std::runtime_error("Failed to allocate mesh of the desired type");
      }
      return res;
    }

    template <typename TMesh>
    std::shared_ptr<TMesh> ExtractMesh(const aiMesh* const pSrcMesh, const Vector3& positionMod, const Vector3& scale)
    {
      Graphics3D::MeshAllocatorFunc meshAllocator(Graphics3D::MeshAllocator::Allocate<TMesh>);
      auto res = std::dynamic_pointer_cast<TMesh>(ExtractMesh(meshAllocator, pSrcMesh, positionMod, scale));
      if (!res)
      {
        throw std::runtime_error("Failed to allocate mesh of the desired type");
      }
      return res;
    }


    //! @brief Extract the vertex position from the mesh as Vector3 entries
    //! @param dst is the dst span
    //! @param pSrcMesh the mesh to extract vertex positions from (!= nullptr)
    //! @return the number of vertices that were written
    static std::size_t FastExtractVertexPosition(TypedFlexSpan<Vector3> dst, const aiMesh* const pSrcMesh);

    //! @brief Extract the vertex position from the mesh as Vector3 entries
    //! @param dst is the dst span
    //! @param pSrcMesh the mesh to extract vertex positions from (!= nullptr)
    //! @return the number of vertices that were written
    static std::size_t FastExtractVertexPosition(TypedFlexSpan<Vector3> dst, const aiMesh* const pSrcMesh, const Vector3& positionMod,
                                                 const Vector3& scale);

    //! @brief Extract the vertex normal from the mesh as Vector3 entries
    //! @param dst is the dst span
    //! @param pSrcMesh the mesh to extract vertex normals from (!= nullptr)
    //! @return the number of vertices that were written
    static std::size_t FastExtractVertexNormals(TypedFlexSpan<Vector3> dst, const aiMesh* const pSrcMesh);

    //! @brief Extract the vertex tangent from the mesh as Vector3 entries
    //! @param dst is the dst span
    //! @param pSrcMesh the mesh to extract vertex normals from (!= nullptr)
    //! @return the number of vertices that were written
    static std::size_t FastExtractVertexTangents(TypedFlexSpan<Vector3> dst, const aiMesh* const pSrcMesh);

    //! @brief Extract the vertex bitangent from the mesh as Vector3 entries
    //! @param dst is the dst span
    //! @param pSrcMesh the mesh to extract vertex normals from (!= nullptr)
    //! @return the number of vertices that were written
    static std::size_t FastExtractVertexBitangents(TypedFlexSpan<Vector3> dst, const aiMesh* const pSrcMesh);

    //! @brief Extract the vertex texture coordinate from the mesh as Vector2 entries
    //! @param dst is the dst span
    //! @param pSrcMesh the mesh to extract vertex normals from (!= nullptr)
    //! @param srcSetIndex the index of the set to extract
    //! @return the number of vertices that were written
    static std::size_t FastExtractTextureCoordinates2(TypedFlexSpan<Vector2> dst, const aiMesh* const pSrcMesh, const int32_t srcSetIndex);

    //! @brief Extract the vertex texture coordinate from the mesh as Vector3 entries
    //! @param dst is the dst span
    //! @param pSrcMesh the mesh to extract vertex normals from (!= nullptr)
    //! @param srcSetIndex the index of the set to extract
    //! @return the number of vertices that were written
    static std::size_t FastExtractTextureCoordinates3(TypedFlexSpan<Vector3> dst, const aiMesh* const pSrcMesh, const int32_t srcSetIndex);

    //! @brief Extract the vertex color from the mesh as Vector4 entries
    //! @param dst is the dst span
    //! @param pSrcMesh the mesh to extract vertex normals from (!= nullptr)
    //! @param srcSetIndex the index of the set to extract
    //! @return the number of vertices that were written
    static std::size_t FastExtractColors(TypedFlexSpan<Vector4> dst, const aiMesh* const pSrcMesh, const int32_t srcSetIndex);
    static std::size_t FastExtractColors(TypedFlexSpan<Color> dst, const aiMesh* const pSrcMesh, const int32_t srcSetIndex);

    //! @brief Extract the indices from the mesh
    //! @param pDst the destination buffer
    //! @param dstCapacity the number of indices of size cbDstIndexEntry that fit inside the pDst buffer.
    //! @param cbDstIndexEntry the size of one index entry.
    //! @param pSrcMesh the mesh to extract indices from (!= nullptr)
    //! @return the number of indices that were written
    static std::size_t FastExtractIndices(void* pDst, const std::size_t dstCapacity, const std::size_t cbDstIndexEntry, const aiMesh* const pSrcMesh);

    //! @brief Extract the indices from the mesh as UInt8
    //! @param pDst the destination buffer
    //! @param dstCapacity the number of indices that inside the pDst buffer.
    //! @param pSrcMesh the mesh to extract indices from (!= nullptr)
    //! @return the number of indices that were written
    //! @note Throws a exception if no indices exist in the mesh
    static std::size_t FastExtractIndicesUInt8(Span<uint8_t> dstSpan, const aiMesh* const pSrcMesh);

    //! @brief Extract the indices from the mesh as UInt16
    //! @param pDst the destination buffer
    //! @param dstCapacity the number of indices that inside the pDst buffer.
    //! @param pSrcMesh the mesh to extract indices from (!= nullptr)
    //! @return the number of indices that were written
    //! @note Throws a exception if no indices exist in the mesh
    static std::size_t FastExtractIndicesUInt16(Span<uint16_t> dstSpan, const aiMesh* const pSrcMesh);

    /// <summary>
    /// Extract the colors from the mesh, if no colors are present the dst span will be filled with the fillColor instead.
    /// </summary>
    /// <typeparam name="T"></typeparam>
    /// <param name="dst"></param>
    /// <param name="pSrcMesh"></param>
    /// <param name="srcSetIndex"></param>
    /// <param name="fillColor"></param>
    /// <returns></returns>
    template <typename T>
    static std::size_t FastExtractColors(TypedFlexSpan<T> dst, const aiMesh* const pSrcMesh, const int32_t srcSetIndex, const T fillColor)
    {
      if (pSrcMesh->mColors[srcSetIndex] != nullptr)
      {
        return FastExtractColors(dst, pSrcMesh, srcSetIndex);
      }
      dst.Fill(fillColor);
      return dst.size();
    }

  private:
    Graphics3D::MeshAllocatorFunc m_meshAllocator;
  };
}

#endif
