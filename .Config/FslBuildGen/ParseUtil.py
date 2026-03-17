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

# from typing import Union
from FslBuildGen.DataTypes import FilterMethod, PackageType
from FslBuildGen.ExtensionListManager2 import ExtensionListManager2
from FslBuildGen.ExternalVariantConstraints import ExternalVariantConstraints
from FslBuildGen.QualifiedRequirementExtensionName import QualifiedRequirementExtensionName
from FslBuildGen.RecipeFilterManager import RecipeFilterManager
from FslBuildGen.RecipeFilterName import RecipeFilterName
from FslBuildGen.SharedGeneration import ToolAddedVariant, ToolAddedVariantConfigOption


def ParseList(strSrcList: str, message: str, allowWildcard: bool) -> list[str]:
    if not strSrcList:
        return []
    if allowWildcard and strSrcList == "*":
        return ["*"]
    if not strSrcList.startswith("[") and not strSrcList.endswith("["):
        raise Exception(f"Expected a {message} list in the format '[{message},{message}]' not '{strSrcList}'")

    strSrcList = strSrcList[1:-1]
    if len(strSrcList) == 0:
        return []
    parsedList = strSrcList.split(",")
    return parsedList if "*" not in parsedList else ["*"]


def ParseComplexList(strSrcList: str, message: str, allowWildcard: bool) -> list[str]:
    if not strSrcList:
        return []
    if allowWildcard and strSrcList == "*":
        return ["*"]
    if not strSrcList.startswith("[") and not strSrcList.endswith("["):
        raise Exception(f"Expected a {message} list in the format '[{message},{message}]' not '{strSrcList}'")

    strSrcList = strSrcList[1:-1]
    if len(strSrcList) == 0:
        return []
    parsedList = strSrcList.split(",")
    return parsedList


def ParseFilterList(strSrcList: str, message: str, allowWildcard: bool) -> list[str]:
    if not strSrcList:
        return []
    if allowWildcard and strSrcList == "*":
        return ["*"]
    if not strSrcList.startswith("[") and not strSrcList.endswith("["):
        raise Exception(f"Expected a {message} list in the format '[{message},{message}]' not '{strSrcList}'")

    strSrcList = strSrcList[1:-1]
    if len(strSrcList) == 0:
        return []
    parsedList = strSrcList.split(",")
    return parsedList


# Do some minimal basic validation of the requirement name input
def __ValidateRequirementList(requirementNameList: list[str], strHelpListName: str, strHelpEntryName: str) -> None:
    if len(requirementNameList) <= 0 or requirementNameList[0] == "*":
        return

    for entry in requirementNameList:
        if not Util.IsValidRequirementName(entry):
            raise Exception(
                f"The {strHelpListName} must be valid, the {strHelpEntryName} name '{entry}' is not a valid {strHelpEntryName} name in list {requirementNameList}"
            )


def ParseExtensionList(strExtensionList: str) -> ExtensionListManager2:
    parsedList = ParseComplexList(strExtensionList, "extension", True)
    if "*" not in parsedList:
        newParsedList = []
        for entry in parsedList:
            # Do some minimal basic validation of the input
            # All extensions has to be qualified with a feature "featureName:extensionName
            values = entry.split(":")
            if not len(values) == 2:
                raise Exception(f"The extension list must be valid, the extension '{entry}' did not follow the expected '<FeatureName>:<ExtensionName>' format")
            if not Util.IsValidRequirementName(values[0]):
                raise Exception(f"The extension list must be valid, the extension '{entry}' did not contain a valid feature name '{values[0]}'")
            if not Util.IsValidRequirementName(values[1]):
                raise Exception(f"The extension list must be valid, the extension '{entry}' did not contain a valid extension name '{values[1]}'")
            newParsedList.append(QualifiedRequirementExtensionName(values[0], values[1]))
        return ExtensionListManager2(FilterMethod.AllowList, newParsedList)
    newParsedList = []
    for entry in parsedList:
        if entry != "*":
            # Do some minimal basic validation of the input
            # All extensions has to be qualified with a feature "featureName:extensionName
            values = entry.split(":")
            if not len(values) == 2:
                raise Exception(f"The extension list must be valid, the extension '{entry}' did not follow the expected '<FeatureName>:<ExtensionName>' format")
            if len(values[0]) < 1 or values[0][0] != "-":
                raise Exception(
                    f"A extension as part of a list that contains a wildcard must be 'substracted' so it is expected to start with '-' which '{entry}' did not"
                )
            firstValue = values[0][1:]
            if not Util.IsValidRequirementName(firstValue):
                raise Exception(f"The extension list must be valid, the extension '{entry}' did not contain a valid feature name '{firstValue}'")
            if not Util.IsValidRequirementName(values[1]):
                raise Exception(f"The extension list must be valid, the extension '{entry}' did not contain a valid extension name '{values[1]}'")
            newParsedList.append(QualifiedRequirementExtensionName(firstValue, values[1]))
    return ExtensionListManager2(FilterMethod.AllowAll if len(newParsedList) <= 0 else FilterMethod.BannedList, newParsedList)


def ParseRecipeList(strRecipeFilterList: str) -> RecipeFilterManager:
    parsedList = ParseFilterList(strRecipeFilterList, "recipe", True)
    containsWildCard = "*" in parsedList
    newParsedList = []
    for entry in parsedList:
        # Do some minimal basic validation of the input
        if entry != "*":
            newParsedList.append(RecipeFilterName(entry))
    return RecipeFilterManager(containsWildCard, newParsedList)


def ParsePackageTypeList(strPackageTypeList: str) -> list[str]:
    parsedList = ParseList(strPackageTypeList, "packageType", True)
    if "*" not in parsedList:
        validPackageTypes = PackageType.AllStrings()
        for entry in parsedList:
            if entry not in validPackageTypes:
                raise Exception(f"The package type list must be valid, the package type '{entry}' is not, valid types {validPackageTypes}")
    return parsedList


def ParseFeatureList(features: str) -> list[str]:
    parsedList = ParseList(features, "feature", True)
    __ValidateRequirementList(parsedList, "feature list", "feature")
    return parsedList


def ParseBool(value: str) -> bool:
    if value is None:
        return False
    value = value.lower()
    if value == "1" or value == "true" or value == "on":
        return True
    elif value == "0" or value == "false" or value == "off":
        return False
    else:
        raise Exception(f"Unsupported bool value '{value}'")


def __ParseExternalVariantConstraints(variants: str | None) -> dict[str, str]:
    if not variants:
        return {}
    if not variants.startswith("[") and not variants.endswith("["):
        raise Exception(f"Expected a variant list in the format '[variant=value,variant=value]' not '{variants}'")

    variants = variants[1:-1]
    if len(variants) == 0:
        return {}
    entries = variants.split(",")
    variantDict: dict[str, str] = {}
    configId = ToolAddedVariant.CONFIG.upper()
    for entry in entries:
        pair = entry.split("=")
        if len(pair) != 2:
            raise Exception(f"Expected the variant config to be in the format 'key=value' not '{entry}'")
        if len(pair[0]) <= 0:
            raise Exception(f"The variant name must be valid not empty '{entry}'")
        if len(pair[1]) <= 0:
            raise Exception(f"The variant value must be valid not empty '{entry}'")
        if pair[0] in variantDict:
            raise Exception(f"The variant '{pair[0]}' has already been configured ('{entry}')")
        if pair[0] != ToolAddedVariant.CONFIG and pair[0].upper() == configId:
            raise Exception(f"The variant name '{pair[0]}' collides with '{ToolAddedVariant.CONFIG}'")
        variantDict[pair[0]] = pair[1]
    return variantDict


def ParseExternalVariantConstraints(variants: str | None, setDebugMode: bool = False) -> ExternalVariantConstraints:
    variantDict = __ParseExternalVariantConstraints(variants)
    if setDebugMode:
        if ToolAddedVariant.CONFIG in variantDict:
            raise Exception(f"Can not force debug mode when variants '{ToolAddedVariant.CONFIG}' is already set")
        variantDict[ToolAddedVariant.CONFIG] = ToolAddedVariantConfigOption.Debug

    return ExternalVariantConstraints.ToExternalVariantConstraints(variantDict)
