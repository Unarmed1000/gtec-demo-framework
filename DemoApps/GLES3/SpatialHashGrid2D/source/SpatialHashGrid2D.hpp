#ifndef GLES3_SPATIALHASHGRID2D_SPATIALHASHGRID2D_HPP
#define GLES3_SPATIALHASHGRID2D_SPATIALHASHGRID2D_HPP
/****************************************************************************************************************************************************
 * Copyright 2022 NXP
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

#include <FslDemoApp/OpenGLES3/DemoAppGLES3.hpp>
#include <FslUtil/OpenGLES3/Draw/GLLineDraw.hpp>
#include <FslUtil/OpenGLES3/GLProgram.hpp>
#include <Shared/SpatialHashGrid2D/Shared.hpp>
#include <array>

namespace Fsl
{
  class SpatialHashGrid2D final : public DemoAppGLES3
  {
    using base_type = DemoAppGLES3;

    struct VertexUBOData
    {
      Matrix MatViewProjection;
    };

    struct ProgramInfo
    {
      GLES3::GLProgram Program;
      GLint LocWorldViewProjection{GLES3::GLValues::InvalidLocation};
      std::array<GLES3::GLVertexAttribLink, 2> Links;
    };

    struct Resources
    {
      ProgramInfo LineProgram;
      GLES3::GLLineDraw LineDraw;
      explicit Resources(const uint32_t initialLineCapacity)
        : LineDraw(initialLineCapacity)
      {
      }
    };
    Shared m_shared;
    Resources m_resources;
    VertexUBOData m_vertexUboData;

  public:
    explicit SpatialHashGrid2D(const DemoAppConfig& config);

  protected:
    void ConfigurationChanged(const DemoWindowMetrics& windowMetrics) final;
    void Update(const DemoTime& demoTime) final;
    void Draw(const FrameInfo& frameInfo) final;

  private:
    ProgramInfo CreateLineProgram(const std::shared_ptr<IContentManager>& contentManager);
  };
}

#endif
