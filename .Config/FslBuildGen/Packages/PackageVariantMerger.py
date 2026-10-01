#!/usr/bin/env python3
# ****************************************************************************************************************************************************
# * BSD 3-Clause License
# *
# * Copyright (c) 2026, Mana Battery ApS
# * All rights reserved.
# *
# * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
# *
# * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
# * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the
# *    documentation and/or other materials provided with the distribution.
# * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this
# *    software without specific prior written permission.
# *
# * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
# * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
# * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# ****************************************************************************************************************************************************

from FslBuildGen.Generator.GeneratorInfo import GeneratorInfo
from FslBuildGen.Log import Log
from FslBuildGen.Packages.Package import Package, PackagePlatformVariant
from FslBuildGen.Packages.Unresolved.UnresolvedPackageVariant import UnresolvedPackageVariant


class FlavorInstanceVariantDeclarationsDifferException(Exception):
    def __init__(self, variantName: str, firstInstanceName: str, secondInstanceName: str) -> None:
        msg = (
            f"The variant '{variantName}' is declared differently by the flavor instances '{firstInstanceName}' and '{secondInstanceName}', "
            "the flavor instances of a package have to share its variant declarations"
        )
        super().__init__(msg)


class PackageVariantMerger:
    """Merges the variant declarations of the packages in the build order of one package into the variants that package sees.

    The first declaration of a variant is the variant and every later declaration extends it (PackagePlatformVariant.Extend).

    The flavor instances of a package share the variant declarations of that package, so they contribute a declaration once: the first instance
    that is added is merged, and a later instance of the same package is the same declaration, not an extension of it. Nothing is merged for the
    later instance, but when the declaration is an extension the merged variant and the options the extension lists name every instance that
    carries it in their ExtendedBy.
    """

    def __init__(self, log: Log, generatorInfo: GeneratorInfo, package: Package) -> None:
        """package is the package the variants are merged for, the private content of a declaration is only kept for the package that declares it"""
        super().__init__()
        self.__log = log
        self.__generatorInfo = generatorInfo
        self.__package = package
        # variant name -> the merged variant, in the order the variants were first added
        self.__variantDict: dict[str, PackagePlatformVariant] = {}
        # (variant name, source package name) -> the instance that was merged and whether it was merged as an extension
        self.__mergedDict: dict[tuple[str, str], tuple[Package, bool]] = {}

    def Add(self, declaringPackage: Package) -> None:
        """Merge the direct variants of a package of the build order, the packages have to be added in build order.
        Raises FlavorInstanceVariantDeclarationsDifferException for a flavor instance that does not share the declaration of the merged instance.
        """
        sourceName = declaringPackage.NameInfo.SourceName
        for variant in declaringPackage.ResolvedDirectVariants:
            key = (variant.Name, sourceName)
            merged = self.__mergedDict.get(key)
            if merged is None:
                clonedVariant = PackagePlatformVariant(self.__log, self.__generatorInfo, self.__package.Name, variant, declaringPackage is self.__package)
                existingVariant = self.__variantDict.get(variant.Name)
                if existingVariant is None:
                    self.__variantDict[variant.Name] = clonedVariant
                else:
                    self.__variantDict[variant.Name] = existingVariant.Extend(clonedVariant, declaringPackage.Name)
                self.__mergedDict[key] = (declaringPackage, existingVariant is not None)
            else:
                mergedInstance, isExtension = merged
                if _GetDeclaration(declaringPackage, variant.Name) is not _GetDeclaration(mergedInstance, variant.Name):
                    raise FlavorInstanceVariantDeclarationsDifferException(variant.Name, mergedInstance.Name, declaringPackage.Name)
                if isExtension:
                    _NameExtender(self.__variantDict[variant.Name], variant, declaringPackage.Name)

    def GetVariantDict(self) -> dict[str, PackagePlatformVariant]:
        """The merged variants by name, in the order the variants were first added"""
        return self.__variantDict

    @staticmethod
    def Merge(log: Log, generatorInfo: GeneratorInfo, package: Package) -> dict[str, PackagePlatformVariant]:
        """The variants of the package: the declarations of every package in its build order, which includes the package itself"""
        merger = PackageVariantMerger(log, generatorInfo, package)
        for entry in package.ResolvedBuildOrder:
            merger.Add(entry)
        return merger.GetVariantDict()


def _GetDeclaration(package: Package, variantName: str) -> UnresolvedPackageVariant | None:
    """The variant as the package file declares it, a package declares a variant name once"""
    for entry in package.GetVariants():
        if entry.Name == variantName:
            return entry
    return None


def _NameExtender(mergedVariant: PackagePlatformVariant, extension: PackagePlatformVariant, instanceName: str) -> None:
    """Add a flavor instance to the extenders of a variant its package extended already, and to those of the options the extension lists.
    mergedVariant is owned by the merger: it was created when the extension of the first instance was merged.
    """
    mergedVariant.ExtendedBy.append(instanceName)
    for option in extension.Options:
        mergedVariant.OptionDict[option.Name].ExtendedBy.append(instanceName)
