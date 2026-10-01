#!/usr/bin/env python3

# ****************************************************************************************************************************************************
# Copyright (c) 2014 Freescale Semiconductor, Inc.
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
#    * Neither the name of the Freescale Semiconductor, Inc. nor the names of
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


from FslBuildGen import Util
from FslBuildGen.Packages.Package import PackagePlatformVariant, PackagePlatformVariantOption
from FslBuildGen.Packages.PackageInstanceName import PackageInstanceName
from FslBuildGen.Packages.PackageRequirement import PackageRequirement
from FslBuildGen.Xml.Exceptions import XmlException2

# from FslBuildGen.Xml.XmlStuff import XmlGenFileVariant
# from FslBuildGen.Xml.XmlStuff import XmlGenFileVariantOption

# class FeatureUseDuplicatedException(XmlException2):
#    def __init__(self, package, name):
#        msg = "Feature name '{0}' in package '{1}' already defined".format(name, package.Name)
#        super().__init__(package.XMLElement, msg)


# class FeatureNameCollisionException(XmlException2):
#    def __init__(self, package1, name1, package2, name2) -> None:
#        msg = "Feature name '{0}' in package '{1}' collides with feature name '{2}' from package '{3}'".format(name1, package1.Name, name2, package2.Name)
#        super().__init__(package1.XMLElement, msg)


class RequirementNameCollisionException(XmlException2):
    def __init__(self, spellings: list[PackageRequirement]) -> None:
        """spellings holds one requirement per spelling of the same requirement, its IntroducedByPackages are the packages that use that spelling"""
        descriptions = [RequirementNameCollisionException.__Describe(entry) for entry in sorted(spellings, key=lambda s: (s.Name, s.Extends))]
        strSpellings = f"{', '.join(descriptions[:-1])} and {descriptions[-1]}"
        msg = f"The requirement names {strSpellings} differ only by case, every package has to use the same spelling"
        super().__init__(msg)

    @staticmethod
    def __Describe(requirement: PackageRequirement) -> str:
        strExtends = f" extending '{requirement.Extends}'" if len(requirement.Extends) > 0 else ""
        packageNames = sorted(requirement.IntroducedByPackages, key=lambda s: (s.lower(), s))
        strPackages = "package" if len(packageNames) == 1 else "packages"
        return f"{requirement.Type} '{requirement.Name}'{strExtends} in {strPackages} {', '.join(f"'{name}'" for name in packageNames)}"


class VariantExtensionNotSupportedException(XmlException2):
    def __init__(self, extendingPackageName: PackageInstanceName, extendingVariant: PackagePlatformVariant, basePackageName: PackageInstanceName) -> None:
        msg = f"Package '{extendingPackageName}' variant: '{extendingVariant.Name}' can not extend the variant defined in '{basePackageName}'"
        super().__init__(msg)


class VariantDeclaredByUnrelatedPackagesException(XmlException2):
    def __init__(self, packageName: PackageInstanceName, variant: PackagePlatformVariant, declaredByPackageName: PackageInstanceName) -> None:
        """packageName declares the variant again, declaredByPackageName declared it first and packageName does not depend on it"""
        msg = (
            f"Package '{packageName}' variant: '{variant.Name}' is already declared by '{declaredByPackageName}', which '{packageName}' does not depend on. "
            "Only a package that depends on the declaring package can extend a variant"
        )
        super().__init__(msg)


class ExtendingVariantCanNotIntroduceNewOptionsException(XmlException2):
    def __init__(
        self,
        extendingPackageName: PackageInstanceName,
        extendingVariant: PackagePlatformVariant,
        basePackageName: PackageInstanceName,
        baseVariant: PackagePlatformVariant,
        newOptions: list[PackagePlatformVariantOption],
    ) -> None:
        msg = "Package '{}' variant: '{}' can not extend the options defined by '{}' with '{}'".format(
            extendingPackageName, extendingVariant.Name, basePackageName, ", ".join(Util.ExtractNames(newOptions))
        )
        super().__init__(msg)


class VariantNameCollisionException(XmlException2):
    def __init__(
        self,
        extendingPackageName: PackageInstanceName,
        extendingVariant: PackagePlatformVariant,
        basePackageName: PackageInstanceName,
        baseVariant: PackagePlatformVariant,
    ) -> None:
        msg = f"Package '{extendingPackageName}' variant: '{extendingVariant.Name}' names collides with '{basePackageName}' variant '{baseVariant.Name}'"
        super().__init__(msg)
