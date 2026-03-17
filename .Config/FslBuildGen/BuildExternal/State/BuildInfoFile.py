#!/usr/bin/env python3
# ****************************************************************************************************************************************************
# Copyright 2018 NXP
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

from typing import cast

from FslBuildGen import PackageListUtil
from FslBuildGen.BuildExternal.PackageExperimentalRecipe import PackageExperimentalRecipe
from FslBuildGen.BuildExternal.State.BuildInfoFilePackageDependency import BuildInfoFilePackageDependency
from FslBuildGen.BuildExternal.State.JsonDictType import JsonDictType
from FslBuildGen.BuildExternal.State.JsonRecipeCMakeConfig import JsonRecipeCMakeConfig, JsonRecipeCMakeVersion
from FslBuildGen.BuildExternal.State.JsonRecipePackageContentState import JsonRecipePackageContentState
from FslBuildGen.BuildExternal.State.RecipePackageState import RecipePackageState
from FslBuildGen.BuildExternal.State.RecipePackageStateCache import RecipePackageStateCache
from FslBuildGen.CMakeUtil import CMakeVersion
from FslBuildGen.Generator.GeneratorCMakeConfig import GeneratorCMakeConfig
from FslBuildGen.Log import Log
from FslBuildGen.Packages.Package import Package


class BuildInfoFileElements:
    PackageName = "PackageName"
    PackageDependencies = "PackageDependencies"
    FileFormatVersion = "FileFormatVersion"
    RecipeHash = "RecipeHash"
    ContentState = "ContentState"
    ContentStateHash = "ContentStateHash"
    SourceState = "SourceState"
    SourceStateHash = "SourceStateHash"

    CMakeConfig = "CMakeConfig"

    CURRENT_VERSION = "3"


class BuildInfoFile:
    def __init__(self, jsonDict: JsonDictType) -> None:
        super().__init__()
        self.PackageName: str = jsonDict[BuildInfoFileElements.PackageName]
        self.PackageDependencies: list[str] = jsonDict[BuildInfoFileElements.PackageDependencies]
        self.FileFormatVersion: str = jsonDict[BuildInfoFileElements.FileFormatVersion]
        self.RecipeHash: str = jsonDict[BuildInfoFileElements.RecipeHash]
        self.ContentState: JsonRecipePackageContentState = jsonDict[BuildInfoFileElements.ContentState]
        self.ContentStateHash: str = jsonDict[BuildInfoFileElements.ContentStateHash]
        self.CMakeConfig: JsonRecipeCMakeConfig = jsonDict[BuildInfoFileElements.CMakeConfig]
        self.DecodedPackageDependencies = [BuildInfoFilePackageDependency(entry) for entry in self.PackageDependencies]
        # Optional entries
        self.SourceState: JsonRecipePackageContentState = (
            jsonDict[BuildInfoFileElements.SourceState] if BuildInfoFileElements.SourceState in jsonDict else JsonRecipePackageContentState()
        )
        self.SourceStateHash: str = jsonDict.get(BuildInfoFileElements.SourceStateHash, "")

    @staticmethod
    def IsDictValid(srcDict: JsonDictType) -> bool:
        if (
            BuildInfoFileElements.PackageName not in srcDict
            or BuildInfoFileElements.PackageDependencies not in srcDict
            or BuildInfoFileElements.FileFormatVersion not in srcDict
            or BuildInfoFileElements.RecipeHash not in srcDict
            or BuildInfoFileElements.ContentState not in srcDict
            or BuildInfoFileElements.CMakeConfig not in srcDict
            or BuildInfoFileElements.ContentStateHash not in srcDict
        ):
            return False
        if not isinstance(srcDict[BuildInfoFileElements.PackageName], str):
            return False
        if not isinstance(srcDict[BuildInfoFileElements.PackageDependencies], list):
            return False
        if not isinstance(srcDict[BuildInfoFileElements.FileFormatVersion], str):
            return False
        if not isinstance(srcDict[BuildInfoFileElements.RecipeHash], str):
            return False
        if not isinstance(srcDict[BuildInfoFileElements.ContentState], dict):
            return False
        if not isinstance(srcDict[BuildInfoFileElements.ContentStateHash], str):
            return False
        if not isinstance(srcDict[BuildInfoFileElements.CMakeConfig], dict):
            return False
        if BuildInfoFileElements.SourceState in srcDict and not isinstance(srcDict[BuildInfoFileElements.SourceState], dict):
            return False
        if BuildInfoFileElements.SourceStateHash in srcDict and not isinstance(srcDict[BuildInfoFileElements.SourceStateHash], str):
            return False
        return cast(bool, srcDict[BuildInfoFileElements.FileFormatVersion] == BuildInfoFileElements.CURRENT_VERSION)

    @staticmethod
    def TryCreateJsonBuildInfoRootDict(
        log: Log,
        cacheFilename: str,
        sourcePackage: Package,
        sourceRecipe: PackageExperimentalRecipe,
        recipePackageStateCache: RecipePackageStateCache,
        cmakeConfig: GeneratorCMakeConfig,
        cachedContentState: JsonRecipePackageContentState | None = None,
        cachedSourceState: JsonRecipePackageContentState | None = None,
    ) -> JsonDictType | None:
        try:
            if sourcePackage is None or sourceRecipe is None or sourceRecipe.ResolvedInstallLocation is None:
                return None

            localSourceState = None
            if sourceRecipe.IsLocalSourceBuild and sourcePackage.ResolvedPath is not None:
                localSourceState = RecipePackageState(
                    log, sourcePackage.Name, sourcePackage.ResolvedPath, "fsl-cached-state", sourcePackage.SourceFileHash, cachedSourceState
                )

            # Generate the package state
            recipePackageState = RecipePackageState(
                log, sourcePackage.Name, sourceRecipe.ResolvedInstallLocation, cacheFilename, sourcePackage.SourceFileHash, cachedContentState
            )
            recipePackageStateCache.Set(recipePackageState)

            referencedPackageSet = PackageListUtil.BuildReferencedPackageSet([sourcePackage])
            referencedPackageSet.remove(sourcePackage)
            referencedPackageNameList = BuildInfoFile.CreateReferencedPackageNameList(referencedPackageSet, recipePackageStateCache)
            referencedPackageNameList.sort()

            recipeHash = sourcePackage.SourceFileHash

            jsonRootDict: JsonDictType = {}
            jsonRootDict[BuildInfoFileElements.PackageName] = sourcePackage.Name
            jsonRootDict[BuildInfoFileElements.PackageDependencies] = referencedPackageNameList
            jsonRootDict[BuildInfoFileElements.FileFormatVersion] = BuildInfoFileElements.CURRENT_VERSION
            jsonRootDict[BuildInfoFileElements.RecipeHash] = recipeHash
            jsonRootDict[BuildInfoFileElements.ContentState] = recipePackageState.ContentState
            jsonRootDict[BuildInfoFileElements.ContentStateHash] = recipePackageState.ContentStateHash
            jsonRootDict[BuildInfoFileElements.CMakeConfig] = BuildInfoFile.ToJsonRecipeCMakeConfig(cmakeConfig)
            if localSourceState is not None:
                jsonRootDict[BuildInfoFileElements.SourceState] = localSourceState.ContentState
                jsonRootDict[BuildInfoFileElements.SourceStateHash] = localSourceState.ContentStateHash
            return jsonRootDict
        except Exception as ex:
            log.LogPrintWarning(f"TryCreateJsonBuildInfoRootDict failed {ex}")
            return None

    @staticmethod
    def ToJsonRecipeCMakeConfig(cmakeConfig: GeneratorCMakeConfig) -> JsonRecipeCMakeConfig:
        result = JsonRecipeCMakeConfig()
        result.Set(
            cmakeConfig.CMakeFinalGeneratorName,
            BuildInfoFile.ToJsonRecipeCMakeVersion(cmakeConfig.CMakeVersion),
            cmakeConfig.CMakeInternalArguments,
            cmakeConfig.CMakeConfigUserGlobalArguments,
        )
        return result

    @staticmethod
    def ToJsonRecipeCMakeVersion(cmakeConfig: CMakeVersion) -> JsonRecipeCMakeVersion:
        result = JsonRecipeCMakeVersion()
        result.Set(cmakeConfig.Major, cmakeConfig.Minor)
        return result

    @staticmethod
    def CreateReferencedPackageNameList(referencedPackageSet: set[Package], recipePackageStateCache: RecipePackageStateCache) -> list[str]:
        referencedPackageNameList: list[str] = []
        for package in referencedPackageSet:
            cachedState = recipePackageStateCache.TryGet(package.Name)
            dependencyHash = "0" if cachedState is None else cachedState.ContentStateHash
            referencedPackageNameList.append(BuildInfoFilePackageDependency.EncodeDependency(package.Name, dependencyHash))
        referencedPackageNameList.sort()
        return referencedPackageNameList
