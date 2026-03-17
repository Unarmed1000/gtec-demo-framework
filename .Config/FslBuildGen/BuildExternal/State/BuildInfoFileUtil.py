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

import json
from typing import cast

from FslBuildGen import IOUtil
from FslBuildGen.BuildExternal.Pipeline import RecipeRecord
from FslBuildGen.BuildExternal.State.BuildInfoComplexJsonDecoder import BuildInfoComplexJsonDecoder
from FslBuildGen.BuildExternal.State.BuildInfoComplexJsonEncoder import BuildInfoComplexJsonEncoder
from FslBuildGen.BuildExternal.State.BuildInfoFile import BuildInfoFile, BuildInfoFileElements
from FslBuildGen.BuildExternal.State.JsonDictType import JsonDictType
from FslBuildGen.BuildExternal.State.JsonRecipeCMakeConfig import JsonRecipeCMakeConfig
from FslBuildGen.BuildExternal.State.PackageRecipeUtil import PackageRecipeUtil
from FslBuildGen.BuildExternal.State.RecipePackageStateCache import RecipePackageStateCache
from FslBuildGen.Generator.GeneratorCMakeConfig import GeneratorCMakeConfig
from FslBuildGen.Log import Log
from FslBuildGen.Packages.Package import Package


class BuildInfoFileHelper:
    @staticmethod
    def TryLoadBuildInformation(log: Log, sourcePackage: Package, path: str) -> JsonDictType | None:
        try:
            if not PackageRecipeUtil.HasBuildPipeline(sourcePackage):
                return None

            sourceRecipe = sourcePackage.ResolvedDirectExperimentalRecipe
            if sourceRecipe is None or sourceRecipe.ResolvedInstallLocation is None:
                raise Exception("Invalid recipe")

            # Generally this should not be called if there is no pipeline

            srcFilePath = IOUtil.Join(sourceRecipe.ResolvedInstallLocation.ResolvedPath, path)

            fileContent = IOUtil.TryReadFile(srcFilePath)
            if fileContent is None:
                log.LogPrint(f"Package build information for package {sourcePackage.Name} not found in the expected file '{srcFilePath}'")
                return None

            jsonBuildInfoDict = json.loads(fileContent)
            if not BuildInfoFile.IsDictValid(jsonBuildInfoDict):
                log.LogPrint(f"Package build information for package {sourcePackage.Name} found in file '{srcFilePath}' is invalid")
                return None

            # Decode the complex element to a object of the right type
            jsonBuildInfoDict[BuildInfoFileElements.ContentState] = BuildInfoComplexJsonDecoder.DecodeJson(
                jsonBuildInfoDict[BuildInfoFileElements.ContentState]
            )
            jsonBuildInfoDict[BuildInfoFileElements.CMakeConfig] = BuildInfoComplexJsonDecoder.DecodeJsonCMakeConfig(
                jsonBuildInfoDict[BuildInfoFileElements.CMakeConfig]
            )
            if BuildInfoFileElements.SourceState in jsonBuildInfoDict:
                jsonBuildInfoDict[BuildInfoFileElements.SourceState] = BuildInfoComplexJsonDecoder.DecodeJson(
                    jsonBuildInfoDict[BuildInfoFileElements.SourceState]
                )

            return cast(JsonDictType, jsonBuildInfoDict)
        except Exception as ex:
            log.LogPrintWarning(f"TryLoadBuildInformation failed for package '{sourcePackage.Name}' with {ex}")
            return None


class BuildInfoFileUtil:
    @staticmethod
    def TryValidateBuildInformation(
        log: Log,
        sourcePackage: Package,
        packagesToBuild: list[Package],
        recipePackageStateCache: RecipePackageStateCache,
        cmakeConfig: GeneratorCMakeConfig,
        path: str,
    ) -> bool:
        if not PackageRecipeUtil.HasBuildPipeline(sourcePackage):
            return False

        loadedBuildInfoDict = BuildInfoFileHelper.TryLoadBuildInformation(log, sourcePackage, path)
        if loadedBuildInfoDict is None:
            return False

        if sourcePackage.ResolvedDirectExperimentalRecipe is None:
            raise Exception("Invalid package")

        # Loaded information
        loadedInfo = BuildInfoFile(loadedBuildInfoDict)
        cachedContentState = loadedInfo.ContentState

        # Load current information
        currentInfoDict = BuildInfoFile.TryCreateJsonBuildInfoRootDict(
            log,
            path,
            sourcePackage,
            sourcePackage.ResolvedDirectExperimentalRecipe,
            recipePackageStateCache,
            cmakeConfig,
            cachedContentState,
            loadedInfo.SourceState,
        )
        if currentInfoDict is None:
            log.LogPrint(f"Failed to create Package build information for package {sourcePackage.Name}")
            return False

        currentInfo = BuildInfoFile(currentInfoDict)

        if loadedInfo.PackageName != currentInfo.PackageName:
            log.LogPrint(f"The current package name {currentInfo.PackageName} did not match the stored package {loadedInfo.PackageName}")
            return False

        if loadedInfo.RecipeHash != currentInfo.RecipeHash:
            log.LogPrint(f"The current package recipe hash {currentInfo.RecipeHash} did not match the stored package hash {loadedInfo.RecipeHash}")
            return False

        if len(currentInfo.PackageDependencies) != len(loadedInfo.PackageDependencies):
            log.LogPrint(
                f"The current package dependencies {currentInfo.PackageDependencies} did not match the stored package dependencies {loadedInfo.PackageDependencies}"
            )
            return False

        if loadedInfo.CMakeConfig != currentInfo.CMakeConfig:
            log.LogPrint(
                f"The current cmake config did not match the stored package config: {JsonRecipeCMakeConfig.GetDiff(loadedInfo.CMakeConfig, currentInfo.CMakeConfig)}"
            )
            return False

        if currentInfo.ContentStateHash != loadedInfo.ContentStateHash:
            log.LogPrint(
                f"The current package contentStateHash {currentInfo.ContentStateHash} did not match the stored package contentStateHash {loadedInfo.ContentStateHash}"
            )
            return False

        # As long as we trust the hash is unique this check wont be necessary
        # if not BuildInfoFile.Compare(loadedInfo.ContentState, currentInfo.ContentState):
        #    log.LogPrint("The current package build content {0} did not match the stored package content {1}".format(currentInfo.ContentState, loadedInfo.ContentState))
        #    return False

        if currentInfo.SourceStateHash != loadedInfo.SourceStateHash:
            log.LogPrint(
                f"The current package sourceStateHash {currentInfo.SourceStateHash} did not match the stored package sourceStateHash {loadedInfo.SourceStateHash}"
            )
            return False

        loadedDependencyDict = {dep.Name: dep for dep in loadedInfo.DecodedPackageDependencies}

        rebuildPackages = {package.Name for package in packagesToBuild}
        for dependency in currentInfo.DecodedPackageDependencies:
            # Future improvement:
            # If the user just deleted a package the 'newly build package' might not actually have changed
            # that can be checked using the ContentStateHash,
            # however the problem is that we would not have that available before we have build the package
            # meaning the validation check would have to occur while building instead of as a prebuild step
            if dependency.Name in rebuildPackages:
                log.LogPrint(f"The dependency {dependency.Name} is being rebuild so we also rebuild {loadedInfo.PackageName}")
                return False
            if dependency.Name not in loadedDependencyDict:
                log.LogPrint(f"The dependency {dependency.Name} did not exist in the stored package {loadedInfo.PackageName}")
                return False
            loadedDep = loadedDependencyDict[dependency.Name]
            currentState = recipePackageStateCache.TryGet(dependency.Name)
            if currentState is not None and loadedDep.Revision != currentState.ContentStateHash:
                log.LogPrint(
                    f"The dependency {dependency.Name} content revision has changed from {currentState.ContentStateHash} to {loadedDep.Revision}, rebuilding"
                )
                return False
        return True

    @staticmethod
    def SaveBuildInformation(
        log: Log, recipeRecord: RecipeRecord | None, recipePackageStateCache: RecipePackageStateCache, cmakeConfig: GeneratorCMakeConfig, path: str
    ) -> None:
        if recipeRecord is None or not PackageRecipeUtil.HasBuildPipeline(recipeRecord.SourcePackage):
            return
        if recipeRecord.SourceRecipe is None or recipeRecord.SourceRecipe.ResolvedInstallLocation is None:
            return

        installPath = recipeRecord.SourceRecipe.ResolvedInstallLocation.ResolvedPath

        jsonRootDict = BuildInfoFile.TryCreateJsonBuildInfoRootDict(
            log, path, recipeRecord.SourcePackage, recipeRecord.SourceRecipe, recipePackageStateCache, cmakeConfig
        )
        if jsonRootDict is None:
            return

        jsonText = json.dumps(jsonRootDict, ensure_ascii=False, sort_keys=True, indent=2, cls=BuildInfoComplexJsonEncoder)

        dstFilePath = IOUtil.Join(installPath, path)
        IOUtil.WriteFileIfChanged(dstFilePath, jsonText)
