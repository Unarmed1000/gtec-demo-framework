#ifndef FSLSIMPLEUI_DECLARATIVE_THEMEPROPERTIES_LAYOUTORIENTATIONTHEMEPROPERTY_HPP
#define FSLSIMPLEUI_DECLARATIVE_THEMEPROPERTIES_LAYOUTORIENTATIONTHEMEPROPERTY_HPP
/****************************************************************************************************************************************************
 * Copyright 2023 NXP
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

#include <FslBase/String/StringViewLite.hpp>
#include <FslBase/UncheckedNumericCast.hpp>
#include <FslSimpleUI/Base/Layout/LayoutOrientation.hpp>
#include <FslSimpleUI/Declarative/ThemeProperties/EnumTypedThemeProperty.hpp>

namespace Fsl::UI::Declarative
{
  class LayoutOrientationThemeProperty final : public EnumTypedThemeProperty<LayoutOrientation>
  {
  public:
    LayoutOrientationThemeProperty()
      : LayoutOrientationThemeProperty(PropertyName("theme_Orientation"))
    {
    }

    explicit LayoutOrientationThemeProperty(PropertyName themePropertyName)
      : EnumTypedThemeProperty(std::move(themePropertyName),
                               {Create("Vertical", LayoutOrientation::Vertical), Create("Horizontal", LayoutOrientation::Horizontal)})
    {
    }
  };
}

#endif
