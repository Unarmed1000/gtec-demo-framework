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


from FslBuildGen import Util
from FslBuildGen.Engine.PackageFlavorName import PackageFlavorName
from FslBuildGen.Engine.PackageFlavorOptionName import PackageFlavorOptionName
from FslBuildGen.Engine.PackageFlavorSelection import PackageFlavorSelection
from FslBuildGen.Engine.Resolver.PackageName import PackageName
from FslBuildGen.Engine.Unresolved.UnresolvedPackageFlavor import UnresolvedPackageFlavor
from FslBuildGen.Engine.Unresolved.UnresolvedPackageFlavorExtension import UnresolvedPackageFlavorExtension
from FslBuildGen.Engine.Unresolved.UnresolvedPackageName import UnresolvedPackageName

# from FslBuildGen.Engine.Unresolved.UnresolvedPackageFlavorOption import UnresolvedPackageFlavorOption


class ExtendingFlavorCanNotIntroduceNewOptionsException(Exception):
    def __init__(
        self, flavorDefinition: UnresolvedPackageFlavor, extendingFlavor: UnresolvedPackageFlavorExtension, newOptions: list[PackageFlavorOptionName]
    ) -> None:
        msg = "Flavor: '{}' can not extend the options defined by '{}' with '{}'".format(
            extendingFlavor.Name, flavorDefinition.Name, ", ".join(Util.ExtractValues(newOptions))
        )
        super().__init__(msg)


class FlavorExtendParentUndefinedException(Exception):
    def __init__(self, flavor: UnresolvedPackageFlavorExtension) -> None:
        msg = f"Can not extend flavor: '{flavor}' as its undefined"
        super().__init__(msg)


class FlavorNameCollisionException(Exception):
    def __init__(self, flavor0: UnresolvedPackageFlavor, flavor1: UnresolvedPackageFlavor) -> None:
        msg = f"Flavor: '{flavor0.Name}' names collides with flavor '{flavor1.Name}'"
        super().__init__(msg)


class PackageDependencyNotFoundException(Exception):
    def __init__(self, sourcePackageName: str, dependencyPackageName: str, candidateList: str) -> None:
        super().__init__(f"'{sourcePackageName}' dependency '{dependencyPackageName}' not found, did you mean: '{candidateList}'")


class CircularDependencyInDependentPackageException(Exception):
    def __init__(self, message: str) -> None:
        # pylint: disable=useless-super-delegation
        super().__init__(message)


class PackageFlavorOptionDependencyNotFoundException(Exception):
    def __init__(self, message: str) -> None:
        # pylint: disable=useless-super-delegation
        super().__init__(message)


class PackageFlavorExtensionOptionDependencyNotFoundException(Exception):
    def __init__(self, message: str) -> None:
        # pylint: disable=useless-super-delegation
        super().__init__(message)


class PackageFlavorDependencyConstraintInvalidException(Exception):
    def __init__(self, packageName: UnresolvedPackageName, depConstraint: PackageFlavorSelection, targetFlavor: UnresolvedPackageFlavor) -> None:
        # pylint: disable=useless-super-delegation
        super().__init__(
            f"Package '{packageName}' has dependency to unknown flavor option '{depConstraint.Option}' flavor '{depConstraint.Name}' in package '{depConstraint.Name.OwnerPackageName}', valid options are '{targetFlavor.Description}': "
        )


class FlavorExtensionCanNotBeAddedToFlavorOriginException(Exception):
    def __init__(self, flavor: UnresolvedPackageFlavorExtension) -> None:
        super().__init__(f"Package '{flavor.Name.OwnerPackageName}' flavor: '{flavor.Name}' can not extend its own flavor.")


class MustBeFlavorExtensionException(Exception):
    def __init__(self, extendingPackageName: UnresolvedPackageName, extendingFlavor: UnresolvedPackageFlavor) -> None:
        super().__init__(f"Package '{extendingPackageName}' flavor: '{extendingFlavor.Name}' must be marked as a flavor extension")


class FlavorCanNotExtendPackageItsNotDependentUponException(Exception):
    def __init__(self, message: str) -> None:
        super().__init__(message)

    @staticmethod
    def CreateSimple(packageName: PackageName, flavor: UnresolvedPackageFlavor) -> "FlavorCanNotExtendPackageItsNotDependentUponException":
        return FlavorCanNotExtendPackageItsNotDependentUponException(
            f"Package '{packageName}' can not extend flavor: '{flavor}' as no dependency to the source package '{flavor.Name.OwnerPackageName}' exist"
        )

    @staticmethod
    def CreateComplex(packageToUnusedFlavorExtensionDict: dict[str, set[str]]) -> "FlavorCanNotExtendPackageItsNotDependentUponException":
        res = ""
        for key, valueSet in packageToUnusedFlavorExtensionDict.items():
            for flavor in valueSet:
                if len(res) > 0:
                    res += "\n"
                res += f"Package '{key}' can not extend flavor: '{flavor}' as no dependency to the source package exist"
        return FlavorCanNotExtendPackageItsNotDependentUponException(res)


class PackageExternalFlavorConstraintInvalidException(Exception):
    def __init__(self, flavorName: PackageFlavorName, validFlavors: list[UnresolvedPackageFlavor]) -> None:
        super().__init__(
            f"Invalid external flavor constraint for flavor '{flavorName}', valid flavors are '{PackageExternalFlavorConstraintInvalidException.ToString(validFlavors)}'"
        )

    @staticmethod
    def ToString(validFlavors: list[UnresolvedPackageFlavor]) -> str:
        res = ""
        for flavor in validFlavors:
            if len(res) > 0:
                res += ", "
            res += flavor.Name.Value
        return res


class PackageExternalFlavorConstraintOptionInvalidException(Exception):
    def __init__(self, flavorName: PackageFlavorName, optionName: PackageFlavorOptionName, targetFlavor: UnresolvedPackageFlavor) -> None:
        super().__init__(f"Invalid external flavor constraint option '{optionName}' for flavor '{flavorName}', valid options are '{targetFlavor.Description}'")


class PackageExternalFlavorConstraintMustBeSetException(Exception):
    def __init__(self, flavorsThatMustBeConstrained: str) -> None:
        super().__init__(f"Please specify exactly which flavor you desire to build: {flavorsThatMustBeConstrained}")
