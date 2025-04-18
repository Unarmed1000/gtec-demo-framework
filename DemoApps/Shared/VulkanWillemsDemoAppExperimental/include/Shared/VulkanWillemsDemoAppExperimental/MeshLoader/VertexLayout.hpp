#ifndef SHARED_VULKANWILLEMSDEMOAPPEXPERIMENTAL_MESHLOADER_VERTEXLAYOUT_HPP
#define SHARED_VULKANWILLEMSDEMOAPPEXPERIMENTAL_MESHLOADER_VERTEXLAYOUT_HPP
/****************************************************************************************************************************************************
 * Copyright (c) 2016 Freescale Semiconductor, Inc.
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

namespace Fsl::Willems::MeshLoader
{
  enum class VertexLayout
  {
    // NOLINTNEXTLINE(readability-identifier-naming)
    VERTEX_LAYOUT_POSITION = 0x0,
    // NOLINTNEXTLINE(readability-identifier-naming)
    VERTEX_LAYOUT_NORMAL = 0x1,
    // NOLINTNEXTLINE(readability-identifier-naming)
    VERTEX_LAYOUT_COLOR = 0x2,
    // NOLINTNEXTLINE(readability-identifier-naming)
    VERTEX_LAYOUT_UV = 0x3,
    // NOLINTNEXTLINE(readability-identifier-naming)
    VERTEX_LAYOUT_TANGENT = 0x4,
    // NOLINTNEXTLINE(readability-identifier-naming)
    VERTEX_LAYOUT_BITANGENT = 0x5,
    // NOLINTNEXTLINE(readability-identifier-naming)
    VERTEX_LAYOUT_DUMMY_FLOAT = 0x6,
    // NOLINTNEXTLINE(readability-identifier-naming)
    VERTEX_LAYOUT_DUMMY_VEC4 = 0x7
  };
}

#endif
