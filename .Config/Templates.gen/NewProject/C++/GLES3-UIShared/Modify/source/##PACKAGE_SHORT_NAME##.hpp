#ifndef ##{$(PACKAGE_NAME).upper().replace('.','_')}##_HPP
#define ##{$(PACKAGE_NAME).upper().replace('.','_')}##_HPP
/****************************************************************************************************************************************************
* Copyright ##PACKAGE_CREATION_YEAR## ##PACKAGE_COMPANY_NAME##
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
*    * Neither the name of the ##PACKAGE_COMPANY_NAME##. nor the names of
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
// FIX: correct this to the right path by removing the Shared/GLES3/ with Shared/
#include <Shared/##DIRS_PACKAGE_NAME##/##PACKAGE_SHORT_NAME##Shared.hpp>

namespace Fsl
{
  class ##PACKAGE_SHORT_NAME## final : public DemoAppGLES3
  {
    using base_type = DemoAppGLES3;

    //! All the actual UI example code can be found in the Shared class since its reused for all PixelPerfect samples.
    ##PACKAGE_SHORT_NAME##Shared m_shared;

  public:
    explicit ##PACKAGE_SHORT_NAME##(const DemoAppConfig& config);
    ~##PACKAGE_SHORT_NAME##() final;

  protected:
    void OnKeyEvent(const KeyEvent& event) final;
    void ConfigurationChanged(const DemoWindowMetrics& windowMetrics) final;
    void Update(const DemoTime& demoTime) final;
    void Draw(const FrameInfo& frameInfo) final;
  };
}

#endif
