#!/usr/bin/env python3
# ****************************************************************************************************************************************************
# Copyright 2023 NXP
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

from enum import Enum

from FslBuildGen import Util
from FslBuildGen.Engine.ExternalFlavorConstraints import ExternalFlavorConstraints
from FslBuildGen.Engine.Order.Exceptions import PackageExternalFlavorConstraintInvalidException, PackageExternalFlavorConstraintOptionInvalidException
from FslBuildGen.Engine.PackageFlavorName import PackageFlavorName
from FslBuildGen.Engine.PackageFlavorOptionName import PackageFlavorOptionName
from FslBuildGen.Engine.Unresolved.UnresolvedBasicPackage import UnresolvedBasicPackage
from FslBuildGen.Engine.Unresolved.UnresolvedPackageFlavor import UnresolvedPackageFlavor
from FslBuildGen.Engine.Unresolved.UnresolvedPackageFlavorName import UnresolvedPackageFlavorName
from FslBuildGen.ExternalVariantConstraints import ExternalVariantConstraints
from FslBuildGen.Log import Log


class NameType(Enum):
    FullyQualified = 1
    Quick = 2
    QuickCollision = 3


class QuickLookupRecord:
    def __init__(self, flavor: UnresolvedPackageFlavor, nameType: NameType, collisions: list[UnresolvedPackageFlavor] | None = None) -> None:
        super().__init__()
        self.Flavor = flavor
        self.NameType = nameType
        self.Collisions = collisions


class ExternalConstraintResolver:
    @staticmethod
    def Resolve(
        log: Log, allPackageDict: dict[str, UnresolvedBasicPackage], externalVariantConstraints: ExternalVariantConstraints
    ) -> ExternalFlavorConstraints:
        flavorConstraintsDict: dict[str, PackageFlavorOptionName] = {}
        if len(externalVariantConstraints.Dict) > 0:
            buildFlavorLookupDict = ExternalConstraintResolver.__BuildFlavorLookupDict(allPackageDict)
            for key, value in externalVariantConstraints.Dict.items():
                if key.Value in buildFlavorLookupDict:
                    lookupRecord = buildFlavorLookupDict[key.Value]
                    if key.Value in flavorConstraintsDict:
                        log.LogPrintWarning(
                            f"Flavor constraint '{key.Value}' already specified as {flavorConstraintsDict[key.Value].Value}, overriding it with: {value.Value}"
                        )
                    finalFlavorName = ExternalConstraintResolver.__ValidateConstraint(key, value, lookupRecord)
                    flavorConstraintsDict[finalFlavorName.Value] = value
                else:
                    ExternalConstraintResolver.__HandleFlavorNameNotFound(log, allPackageDict, key, value)

        finalFlavorConstraintsDict: dict[PackageFlavorName, PackageFlavorOptionName] = {}
        for flavorName, flavorValue in flavorConstraintsDict.items():
            finalFlavorConstraintsDict[PackageFlavorName.FromString(flavorName)] = flavorValue
        return ExternalFlavorConstraints(finalFlavorConstraintsDict)

    @staticmethod
    def __HandleFlavorNameNotFound(
        log: Log, allPackageDict: dict[str, UnresolvedBasicPackage], flavorName: UnresolvedPackageFlavorName, flavorOptionName: PackageFlavorOptionName
    ) -> None:
        actualFlavorName = PackageFlavorName.TryFromString(flavorName.Value)
        if actualFlavorName is not None and actualFlavorName.OwnerPackageName.Value in allPackageDict:
            targetPackage = allPackageDict[actualFlavorName.OwnerPackageName.Value]
            targetFlavor = targetPackage.TryGetFlavor(actualFlavorName)
            if targetFlavor is None:
                raise PackageExternalFlavorConstraintInvalidException(actualFlavorName, targetPackage.Flavors)
            if not targetFlavor.IsValidOptionName(flavorOptionName):
                raise PackageExternalFlavorConstraintOptionInvalidException(actualFlavorName, flavorOptionName, targetFlavor)
        else:
            log.LogPrintWarning(f"Unknown flavor constraint: '{flavorName.Value}={flavorOptionName.Value}'")

    @staticmethod
    def __ValidateConstraint(
        flavorName: UnresolvedPackageFlavorName, flavorOptionName: PackageFlavorOptionName, lookupRecord: QuickLookupRecord
    ) -> PackageFlavorName:
        if lookupRecord.NameType == NameType.FullyQualified or lookupRecord.NameType == NameType.Quick:
            if not lookupRecord.Flavor.IsValidOptionName(flavorOptionName):
                raise PackageExternalFlavorConstraintOptionInvalidException(lookupRecord.Flavor.Name, flavorOptionName, lookupRecord.Flavor)
            return lookupRecord.Flavor.Name
        if lookupRecord.NameType == NameType.QuickCollision:
            if lookupRecord.Collisions is not None:
                raise Exception(
                    f"Flavor quick name '{flavorName.Value}' collision, provide the fully qualified name. Choices: {Util.ExtractNames(lookupRecord.Collisions)}"
                )
            else:
                raise Exception(f"Flavor quick name '{flavorName.Value}' collision, provide the fully qualified name")
        raise Exception(f"Internal error, unsupported NameType: {lookupRecord.NameType}")

    @staticmethod
    def __BuildFlavorLookupDict(allPackageDict: dict[str, UnresolvedBasicPackage]) -> dict[str, QuickLookupRecord]:
        lookupDict: dict[str, QuickLookupRecord] = {}
        quickNameDict: dict[str, list[UnresolvedPackageFlavor]] = {}
        for package in allPackageDict.values():
            for flavor in package.Flavors:
                lookupDict[flavor.Name.Value] = QuickLookupRecord(flavor, NameType.FullyQualified)
                if flavor.QuickName is not None:
                    if flavor.QuickName.Value not in quickNameDict:
                        quickNameDict[flavor.QuickName.Value] = [flavor]
                    else:
                        quickNameDict[flavor.QuickName.Value].append(flavor)
        for quickName, flavors in quickNameDict.items():
            if quickName in lookupDict:
                raise Exception(f"internal error quick name collides with fully qualified name this should not be possible: {quickName}")
            if len(flavors) == 1:
                lookupDict[quickName] = QuickLookupRecord(flavors[0], NameType.Quick)
            elif len(flavors) > 1:
                lookupDict[quickName] = QuickLookupRecord(flavors[0], NameType.QuickCollision, flavors)
        return lookupDict
