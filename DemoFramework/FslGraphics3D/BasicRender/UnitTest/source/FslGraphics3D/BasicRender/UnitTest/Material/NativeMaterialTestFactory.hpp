#ifndef FSLGRAPHICS3D_BASICRENDER_UNITTEST_MATERIAL_FSLGRAPHICS3D_BASICRENDER_UNITTEST_NATIVEMATERIALTESTFACTORY_HPP
#define FSLGRAPHICS3D_BASICRENDER_UNITTEST_MATERIAL_FSLGRAPHICS3D_BASICRENDER_UNITTEST_NATIVEMATERIALTESTFACTORY_HPP
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

#include <FslBase/Collections/HandleVector.hpp>
#include <FslBase/Exceptions.hpp>
#include <FslGraphics3D/BasicRender/Adapter/INativeMaterialFactory.hpp>

namespace Fsl
{
  class NativeMaterialTestFactory final : public Graphics3D::INativeMaterialFactory
  {
    struct Record
    {
      BasicNativeMaterialCreateInfo CreateInfo;
    };

    HandleVector<Record> m_materials;

  public:
    uint32_t MaterialCount() noexcept
    {
      return m_materials.Count();
    }

    void CreateMaterials(Span<BasicNativeMaterialHandle> dstMaterialHandles, ReadOnlySpan<BasicNativeMaterialCreateInfo> createInfoSpan) final
    {
      if (createInfoSpan.size() > dstMaterialHandles.size())
      {
        throw std::invalid_argument("dstMaterialHandles.size() must be >= createInfoSpan.size()");
      }

      for (std::size_t i = 0; i < createInfoSpan.size(); ++i)
      {
        auto handle = m_materials.Add(Record{createInfoSpan[i]});
        dstMaterialHandles[i] = BasicNativeMaterialHandle(handle);
      }
    }

    bool DestroyMaterial(const BasicNativeMaterialHandle hMaterial) noexcept final
    {
      return m_materials.Remove(hMaterial.Value);
    }
  };
}

#endif
