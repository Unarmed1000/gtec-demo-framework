#!/usr/bin/env python3

# ****************************************************************************************************************************************************
# Copyright 2020 NXP
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#    * Redistributions of source code must retain the above copyright notice,
#      this list of conditions and the following disclaimer.
#
#    * Redistributions in binary form must reproduce the above copyright notice,
#      this list of conditions and the following disclaimer in the documentation
#      and/or other materials provided with the distribution.
#
#    * Neither the name of the NXP. nor the names of
#      its contributors may be used to endorse or promote products derived from
#      this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# ****************************************************************************************************************************************************


from FslBuildGen.DataTypes import VariantType
from FslBuildGen.Exceptions import VariantOptionNameCollisionException
from FslBuildGen.Packages.Unresolved.UnresolvedPackageVariantOption import UnresolvedPackageVariantOption


class UnresolvedPackageVariant:
    def __init__(
        self, name: str, introducedByPackageName: str, allowExtend: bool, variantType: VariantType, options: list[UnresolvedPackageVariantOption]
    ) -> None:
        super().__init__()
        self.Name = name
        self.IntroducedByPackageName = introducedByPackageName
        self.AllowExtend = allowExtend
        self.Options = options
        self.Type = variantType
        self.OptionDict: dict[str, UnresolvedPackageVariantOption] = UnresolvedPackageVariant.__BuildOptionDict(options)
        # TODO: verify
        # self.__ValidateVariantName()
        # self.__ValidateOptionNames()

    @staticmethod
    def __BuildOptionDict(options: list[UnresolvedPackageVariantOption]) -> dict[str, UnresolvedPackageVariantOption]:
        optionDict: dict[str, UnresolvedPackageVariantOption] = {}
        optionNameSet: dict[str, str] = {}
        for option in options:
            optionDict[option.Name] = option
            key = option.Name.upper()
            if key not in optionNameSet:
                optionNameSet[key] = option.Name
            else:
                raise VariantOptionNameCollisionException(optionNameSet[key], option.Name)
        return optionDict
