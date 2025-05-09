/****************************************************************************************************************************************************
 * Copyright 2021-2022, 2024 NXP
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

#include <FslBase/Span/SpanUtil_Vector.hpp>
#include <FslDemoService/NativeGraphics/OpenGLES2/NativeGraphicsMaterialAttribManager.hpp>
#include <FslGraphics/Vertices/VertexDeclarationSpan.hpp>


namespace Fsl::GLES2
{
  NativeGraphicsMaterialAttribManager::NativeGraphicsMaterialAttribManager()
    : m_records(16)
  {
  }

  NativeMaterialAttribHandle NativeGraphicsMaterialAttribManager::AcquireConfig(const GLint locVertexPosition, const GLint locVertexColor,
                                                                                const GLint locVertexTextureCoord,
                                                                                const VertexDeclarationSpan& vertexDeclaration)
  {
    if (locVertexTextureCoord == GLValues::InvalidLocation)
    {
      std::array<GLVertexAttribLink, 2> attribLink = {
        GLVertexAttribLink(locVertexPosition, vertexDeclaration.VertexElementGetIndexOf(VertexElementUsage::Position, 0)),
        GLVertexAttribLink(locVertexColor, vertexDeclaration.VertexElementGetIndexOf(VertexElementUsage::Color, 0))};

      const auto attribLinkSpan = SpanUtil::AsReadOnlySpan(attribLink);

      NativeMaterialAttribHandle handle = TryAcquireExisting(m_records, vertexDeclaration, attribLinkSpan);
      if (!handle.IsValid())
      {
        handle = NativeMaterialAttribHandle(m_records.Add(AttribConfigRecord(vertexDeclaration, attribLinkSpan)));
      }
      return handle;
    }

    {
      std::array<GLVertexAttribLink, 3> attribLink = {
        GLVertexAttribLink(locVertexPosition, vertexDeclaration.VertexElementGetIndexOf(VertexElementUsage::Position, 0)),
        GLVertexAttribLink(locVertexColor, vertexDeclaration.VertexElementGetIndexOf(VertexElementUsage::Color, 0)),
        GLVertexAttribLink(locVertexTextureCoord, vertexDeclaration.VertexElementGetIndexOf(VertexElementUsage::TextureCoordinate, 0))};

      const auto attribLinkSpan = SpanUtil::AsReadOnlySpan(attribLink);

      NativeMaterialAttribHandle handle = TryAcquireExisting(m_records, vertexDeclaration, attribLinkSpan);
      if (!handle.IsValid())
      {
        handle = NativeMaterialAttribHandle(m_records.Add(AttribConfigRecord(vertexDeclaration, attribLinkSpan)));
      }
      return handle;
    }
  }


  bool NativeGraphicsMaterialAttribManager::ReleaseConfig(const NativeMaterialAttribHandle handle) noexcept
  {
    AttribConfigRecord* pRecord = m_records.TryGet(handle.Value);
    const bool released = pRecord != nullptr;
    if (released)
    {
      assert(pRecord->RefCount > 0u);
      --pRecord->RefCount;
      if (pRecord->RefCount <= 0)
      {
        m_records.Remove(handle.Value);
      }
    }
    return released;
  }


  NativeMaterialAttribHandle NativeGraphicsMaterialAttribManager::TryAcquireExisting(HandleVector<AttribConfigRecord>& rRecords,
                                                                                     const VertexDeclarationSpan& vertexDeclaration,
                                                                                     const ReadOnlySpan<GLVertexAttribLink> attribLinks)
  {
    const VertexElementAttribLinks newLinks(vertexDeclaration, attribLinks);

    const uint32_t size = rRecords.Count();
    for (uint32_t i = 0; i < size; ++i)
    {
      if (rRecords[i].AttribLinks.IsCompatible(newLinks))
      {
        ++rRecords[i].RefCount;
        return NativeMaterialAttribHandle(rRecords.FastIndexToHandle(i));
      }
    }
    return NativeMaterialAttribHandle::Invalid();
  }
}
