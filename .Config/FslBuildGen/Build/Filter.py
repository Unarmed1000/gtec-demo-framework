#!/usr/bin/env python3
# ****************************************************************************************************************************************************
# Copyright 2017 NXP
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

import fnmatch
from typing import TypeVar, cast, overload

from FslBuildGen import PackageListUtil, Util
from FslBuildGen.Build.RequirementTree import RequirementTree
from FslBuildGen.Build.RequirementTreeNode import RequirementTreeNode

# from FslBuildGen.Config import Config
from FslBuildGen.DataTypes import FilterMethod, PackageRequirementTypeString, PackageType
from FslBuildGen.Engine.Resolver.PreResolvePackageResult import PreResolvePackageResult
from FslBuildGen.Exceptions import UsageErrorException
from FslBuildGen.ExtensionListManager import ExtensionListManager
from FslBuildGen.ExtensionListManager2 import ExtensionListManager2
from FslBuildGen.Info.AppInfo import AppInfoPackage
from FslBuildGen.Info.AppInfoGlobalRequirementTreeNode import AppInfoGlobalRequirementTreeNode
from FslBuildGen.Info.AppInfoRequirementTree import AppInfoGlobalRequirementTree
from FslBuildGen.Info.RequirementInfo import RequirementInfo

# from FslBuildGen.Info.RequirementInfo import RequirementType
from FslBuildGen.Log import Log
from FslBuildGen.PackageFilters import PackageFilters
from FslBuildGen.Packages.Package import Package
from FslBuildGen.Packages.PackageRequirement import PackageRequirement
from FslBuildGen.QualifiedRequirementExtensionName import QualifiedRequirementExtensionName


class LocalUtil:
    @staticmethod
    def BuildListOfDirectlyNotSupported(package: Package) -> list[Package]:
        notSupported: list[Package] = []
        for dependency in package.ResolvedBuildOrder:
            if not dependency.ResolvedPlatformDirectSupported:
                notSupported.append(dependency)
        return notSupported


CommonPackage = TypeVar("CommonPackage", Package, PreResolvePackageResult)


class RequirementFilter:
    @staticmethod
    def FilterRequirementsByType(requirements: list[PackageRequirement], requirementType: str | None) -> list[PackageRequirement]:
        """Filter the requirements by the supplied type.
        If type is none this returns the requirements list without modification
        """
        return requirements if requirementType is None else [requirement for requirement in requirements if requirement.Type == requirementType]

    @staticmethod
    def GetRequirementList(topLevelPackage: Package, requestedPackages: list[Package] | None, requirementType: str | None = None) -> list[PackageRequirement]:
        """Generate a requirement list based on input, the requirement list can be optionally filtered by requirementType.
        If requestedPackages are None then then all packages used by the top level package will be filtered.
        If a top level package is supplied then we return the 'ResolvedAllRequirements' for it (filtered as requested).
        """
        if topLevelPackage is None:
            raise UsageErrorException("topLevelPackage can not be None")

        if requestedPackages is None or len(requestedPackages) <= 0:
            # Since no files were supplied we use the topLevelPackage
            return RequirementFilter.FilterRequirementsByType(topLevelPackage.ResolvedAllRequirements, requirementType)
        return RequirementFilter.GetRequirementListFromPackages(requestedPackages, requirementType)

    @staticmethod
    def GetRequirementListFromPackages(requestedPackages: list[CommonPackage], requirementType: str | None = None) -> list[PackageRequirement]:
        # extract the package requirements into a unique list while still respecting the filter
        requirementDict: dict[str, PackageRequirement] = {}
        for package in requestedPackages:
            requirementList = RequirementFilter.FilterRequirementsByType(package.ResolvedAllRequirements, requirementType)
            for requirement in requirementList:
                if requirement.FullId not in requirementDict:
                    requirementDict[requirement.FullId] = requirement
        return list(requirementDict.values())


T = TypeVar("T")


class PackageFilter:
    @staticmethod
    def __ToPackageName(package: CommonPackage | AppInfoPackage) -> str:
        if isinstance(package, AppInfoPackage):
            return package.Name
        if isinstance(package, Package):
            return package.NameInfo.FullName.Value
        return package.SourcePackage.NameInfo.FullName.Value

    @staticmethod
    def __HasRecipe(package: CommonPackage | AppInfoPackage) -> bool:
        if isinstance(package, PreResolvePackageResult):
            return package.SourcePackage.DirectExperimentalRecipe is not None
        return package.ResolvedDirectExperimentalRecipe is not None

    @staticmethod
    def __ContainsFeature(resolvedFeatureList: list[PackageRequirement] | list[RequirementInfo], featureName: str) -> bool:
        return any(feature.Name == featureName for feature in resolvedFeatureList)

    @staticmethod
    def __UsesFeatures(package: CommonPackage | AppInfoPackage, requiredFeatureNameList: list[str]) -> bool:
        return all(PackageFilter.__ContainsFeature(package.ResolvedAllUsedFeatures, featureName) for featureName in requiredFeatureNameList)

    @staticmethod
    def __FeaturesAvailable(package: CommonPackage | AppInfoPackage, featureNameList: list[str]) -> bool:
        return all(feature.Name in featureNameList for feature in package.ResolvedAllUsedFeatures)

    @staticmethod
    def __IsExtensionAvailable(requirementTree: RequirementTree | AppInfoGlobalRequirementTree, featureName: str, extensionName: str) -> bool:
        extensionNode = requirementTree.TryLocateExtensionNode(featureName, extensionName)
        return extensionNode is not None and extensionNode.Supported

    @staticmethod
    def __IsAllPackageExtensionAvailable(package: CommonPackage | AppInfoPackage, requirementTree: RequirementTree | AppInfoGlobalRequirementTree) -> bool:
        for requirement in package.ResolvedAllRequirements:
            if requirement.Type == PackageRequirementTypeString.Extension and not PackageFilter.__IsExtensionAvailable(
                requirementTree, requirement.Extends, requirement.Name
            ):
                return False
        return True

    @staticmethod
    def __GetCompleteMissingExtensionNames(
        package: CommonPackage | AppInfoPackage, requirementTree: RequirementTree | AppInfoGlobalRequirementTree
    ) -> list[str]:
        missing = []
        for requirement in package.ResolvedAllRequirements:
            if requirement.Type == PackageRequirementTypeString.Extension and not PackageFilter.__IsExtensionAvailable(
                requirementTree, requirement.Extends, requirement.Name
            ):
                missing.append(QualifiedRequirementExtensionName.ToString(requirement.Extends, requirement.Name))
        return missing

    @staticmethod
    def __GetCompleteMissingFeatureNames(package: CommonPackage | AppInfoPackage, featureNameList: list[str]) -> list[str]:
        missing = []
        for feature in package.ResolvedAllUsedFeatures:
            if feature.Name not in featureNameList:
                missing.append(feature.Name)
        return missing

    @overload
    @staticmethod
    def __FiltersPackagesByRequiredFeature(log: Log, packages: list[CommonPackage], requiredFeatureNameList: list[str]) -> list[T]:
        pass

    @overload
    @staticmethod
    def __FiltersPackagesByRequiredFeature(log: Log, packages: list[AppInfoPackage], requiredFeatureNameList: list[str]) -> list[T]:
        pass

    @staticmethod
    def __FiltersPackagesByRequiredFeature(log: Log, packages: list[CommonPackage] | list[AppInfoPackage], requiredFeatureNameList: list[str]) -> list[T]:
        """Filter packages to those that require the specified feature.
        - If '*' is contained in 'requiredFeatureNameList' then we return 'packages' and no filtering is done.
        - Will always return a new list
        """
        allowAllFeatures = "*" in requiredFeatureNameList
        if allowAllFeatures:
            return cast(list[T], list(packages))

        # Count and filter executables
        filteredPackageList = []
        for package in packages:
            if PackageFilter.__UsesFeatures(package, requiredFeatureNameList):
                filteredPackageList.append(package)
            else:
                log.LogPrint(
                    "Skipping '{}' since it did not use the features '{}'".format(PackageFilter.__ToPackageName(package), ", ".join(requiredFeatureNameList))
                )
        return cast(list[T], filteredPackageList)

    @staticmethod
    def __AddParentFeatures(
        log: Log, featureNameList: list[str], requirementTree: RequirementTree | AppInfoGlobalRequirementTree, useStrictFeatureWarning: bool
    ) -> list[str]:
        if "*" in featureNameList:
            return featureNameList
        featureNameList.sort()
        if log.Verbosity > 1:
            log.LogPrint(f"Automatically adding features to supplied feature list {featureNameList}")
        featureNameSet = set(featureNameList)
        for featureName in featureNameList:
            if featureName in requirementTree.FeatureToNodeDict:
                requirementNode = requirementTree.FeatureToNodeDict[featureName]
                currentNode: RequirementTreeNode | AppInfoGlobalRequirementTreeNode | None = requirementNode
                while currentNode is not None:
                    if currentNode.Content is not None and currentNode.Content.Name not in featureNameSet:
                        featureNameSet.add(currentNode.Content.Name)
                        if log.Verbosity > 1 and requirementNode.Content is not None:
                            log.LogPrint(f"- '{currentNode.Content.Name}' because '{requirementNode.Content.Name}' depends on it")
                    currentNode = currentNode.Parent
            else:
                featureNameSet.remove(featureName)
                if useStrictFeatureWarning:
                    log.DoPrintWarning(f"Unknown feature name '{featureName}' in filterNameList {featureNameList}")
                else:
                    # For now just log a warning
                    log.LogPrintVerbose(5, f"Unknown feature name '{featureName}' in filterNameList {featureNameList}")
        resultList = list(featureNameSet)
        resultList.sort()
        return resultList

    @overload
    @staticmethod
    def __FiltersPackagesByFeatures(log: Log, packages: list[CommonPackage], featureNameList: list[str]) -> list[T]:
        pass

    @overload
    @staticmethod
    def __FiltersPackagesByFeatures(log: Log, packages: list[AppInfoPackage], featureNameList: list[str]) -> list[T]:
        pass

    @staticmethod
    def __FiltersPackagesByFeatures(log: Log, packages: list[CommonPackage] | list[AppInfoPackage], featureNameList: list[str]) -> list[T]:
        """Filter packages by features.
        If '*' is in the featureNameList a clone of 'packages' will be returned
        Else we return a list containing only the packages that can be build with the available features
        """

        if "*" in featureNameList:
            log.LogPrint("Filtering by features: All")
            return cast(list[T], list(packages))
        else:
            log.LogPrint("Filtering by features: {}".format(", ".join(featureNameList)))

        filteredPackageList = []
        for package in packages:
            if PackageFilter.__FeaturesAvailable(package, featureNameList):
                filteredPackageList.append(package)
            elif package.Type == PackageType.Library or package.Type == PackageType.Executable or PackageFilter.__HasRecipe(package):
                missingFeatures = PackageFilter.__GetCompleteMissingFeatureNames(package, featureNameList)
                log.LogPrint(
                    "Could not build package '{}' due to missing features '{}'".format(PackageFilter.__ToPackageName(package), ", ".join(missingFeatures))
                )
        return cast(list[T], filteredPackageList)

    @overload
    @staticmethod
    def __FiltersPackagesByExtensions(
        log: Log, packages: list[CommonPackage], extensionNameList: ExtensionListManager, featureNameList: list[str], requirementTree: RequirementTree
    ) -> list[T]:
        pass

    @overload
    @staticmethod
    def __FiltersPackagesByExtensions(
        log: Log,
        packages: list[AppInfoPackage],
        extensionNameList: ExtensionListManager,
        featureNameList: list[str],
        requirementTree: AppInfoGlobalRequirementTree,
    ) -> list[T]:
        pass

    @staticmethod
    def __FiltersPackagesByExtensions(
        log: Log,
        packages: list[CommonPackage] | list[AppInfoPackage],
        extensionNameList: ExtensionListManager,
        featureNameList: list[str],
        requirementTree: RequirementTree | AppInfoGlobalRequirementTree,
    ) -> list[T]:
        """Filter packages by extensions.
        If '*' is in the extensionNameList a clone of 'packages' will be returned
        Else we return a list containing only the packages that can be build with the available extensions
        """
        if extensionNameList.AllowAllExtensions:
            log.LogPrint("Filtering by extensions: All")
            return cast(list[T], list(packages))
        else:
            log.LogPrint("Filtering by extensions: {}".format(", ".join([str(qualifiedName) for qualifiedName in extensionNameList.Content])))

        filteredPackageList = []
        for package in packages:
            if PackageFilter.__IsAllPackageExtensionAvailable(package, requirementTree):
                filteredPackageList.append(package)
            elif package.Type == PackageType.Library or package.Type == PackageType.Executable or PackageFilter.__HasRecipe(package):
                missingNames = PackageFilter.__GetCompleteMissingExtensionNames(package, requirementTree)
                log.LogPrint(
                    "Could not build package '{}' due to missing extension '{}'".format(PackageFilter.__ToPackageName(package), ", ".join(missingNames))
                )
        return cast(list[T], filteredPackageList)

    @overload
    @staticmethod
    def __FiltersPackagesBySupported(log: Log, packages: list[CommonPackage]) -> list[T]:
        pass

    @overload
    @staticmethod
    def __FiltersPackagesBySupported(log: Log, packages: list[AppInfoPackage]) -> list[T]:
        pass

    @staticmethod
    def __FiltersPackagesBySupported(log: Log, packages: list[CommonPackage] | list[AppInfoPackage]) -> list[T]:
        """Remove packages that are marked as not supported by the platform"""
        packageList = []
        for package in packages:
            if package.ResolvedPlatformSupported:
                packageList.append(package)
            elif package.Type != PackageType.TopLevel and log.IsVerbose and isinstance(package, Package):
                assert isinstance(package, Package)
                notSupported = LocalUtil.BuildListOfDirectlyNotSupported(package)
                notSupportedNames = Util.ExtractNames(notSupported)
                log.DoPrint(f"Skipping {package.Name} since its marked as not supported on this platform by package: {notSupportedNames}")
        return cast(list[T], packageList)

    @staticmethod
    def __ContainsExecutablePackage(packages: list[Package]) -> bool:
        if packages is None:
            return False
        return any(package.Type == PackageType.Executable for package in packages)

    @staticmethod
    def PrintExecutableSkipReason(log: Log, fullPackageList: list[Package], filteredPackageList: list[Package]) -> None:
        for package in fullPackageList:
            if package.Type == PackageType.Executable and not package.ResolvedPlatformSupported:
                notSupported = LocalUtil.BuildListOfDirectlyNotSupported(package)
                notSupportedNames = Util.ExtractNames(notSupported)
                log.DoPrint(f"{package.Name} was marked as not supported on this platform by package: {notSupportedNames}")

    @staticmethod
    def __FilterExtensionsByAvailableFeatures(log: Log, featureNameList: list[str], qualifiedExtensionNameList: ExtensionListManager) -> ExtensionListManager:
        if qualifiedExtensionNameList.AllowAllExtensions:
            return qualifiedExtensionNameList

        filteredList = []
        for qualifiedExtensionNameRecord in qualifiedExtensionNameList.Content:
            if qualifiedExtensionNameRecord.FeatureName in featureNameList:
                filteredList.append(qualifiedExtensionNameRecord)
            else:
                log.LogPrint(f"Removing extension '{qualifiedExtensionNameRecord}' as the feature '{qualifiedExtensionNameRecord.FeatureName}' is unavailable")
        return ExtensionListManager(False, filteredList)

    @staticmethod
    def __DetermineActualUserBuildRequest(
        allAvailablePackageListInResolvedBuildOrder: list[CommonPackage], requestedPackages: list[CommonPackage] | None
    ) -> list[CommonPackage]:
        if requestedPackages is not None and len(requestedPackages) > 0:
            return requestedPackages
        return allAvailablePackageListInResolvedBuildOrder

    @staticmethod
    def __FiltersRecipePackages(log: Log, resolvedPackageOrder: list[CommonPackage], requestedPackages: list[CommonPackage] | None) -> list[CommonPackage]:
        return [package for package in resolvedPackageOrder if not package.ContainsRecipe() or (requestedPackages is not None and package in requestedPackages)]

    @staticmethod
    def FilterNotSupported(log: Log, topLevelPackage: Package, requestedPackages: list[Package] | None) -> list[Package]:
        """Filter the package list based
        - if they are supported on the platform
        """
        resolvedBuildOrder = topLevelPackage.ResolvedBuildOrder

        # Try to determine what the user is interested in building
        requestedPackagesInOrder = PackageFilter.__DetermineActualUserBuildRequest(resolvedBuildOrder, requestedPackages)

        # remove all the unsupported packages
        requestedPackagesInOrder = PackageFilter.__FiltersPackagesBySupported(log, requestedPackagesInOrder)
        # Now that we have a filtered list of desired packages, extend it to include all required packages
        return PackageListUtil.GetRequiredPackagesInSourcePackageListOrder(requestedPackagesInOrder, resolvedBuildOrder)

    @staticmethod
    def Filter(log: Log, topLevelPackage: Package, requestedPackages: list[Package] | None, packageFilters: PackageFilters) -> list[Package]:
        """Filter the package list based
        - Required packages by the requested packages (if requestedPackages isnt None)
        - If there is executeables then chose those that implement the required features in requiredFeatureNameList
        - the available features from featureNameList
        - the available extensions from extensionNameList
        - if they are supported on the platform
        """
        resolvedBuildOrder = topLevelPackage.ResolvedBuildOrder
        requirements = RequirementFilter.GetRequirementList(topLevelPackage, None)
        return PackageFilter.Filter2(log, resolvedBuildOrder, requirements, requestedPackages, packageFilters)

    @staticmethod
    def Filter2(
        log: Log,
        resolvedBuildOrder: list[CommonPackage],
        requirements: list[PackageRequirement],
        requestedPackages: list[CommonPackage] | None,
        packageFilters: PackageFilters,
    ) -> list[CommonPackage]:
        """Filter the package list based
        - Required packages by the requested packages (if requestedPackages isnt None)
        - If there is executeables then chose those that implement the required features in requiredFeatureNameList
        - the available features from featureNameList
        - the available extensions from extensionNameList
        - if they are supported on the platform
        """
        requirementTree = RequirementTree(requirements)

        # Smart expand the input lists
        # - First we add all parent features automatically to ensure the feature list is complete
        useStrictFeatureWarning = requestedPackages is None or len(requestedPackages) <= 0
        featureNameList2 = PackageFilter.__AddParentFeatures(log, packageFilters.FeatureNameList, requirementTree, useStrictFeatureWarning)
        extensionNameList = PackageFilter.__CreateExtensionNameList(resolvedBuildOrder, packageFilters.ExtensionNameList)
        if not extensionNameList.AllowAllExtensions:
            # Filter extensions by available features
            extensionNameList = PackageFilter.__FilterExtensionsByAvailableFeatures(log, featureNameList2, extensionNameList)
            requirementTree.SetExtensionSupport(log, extensionNameList)

        # Try to determine what the user is interested in building
        requestedPackagesInOrder = PackageFilter.__DetermineActualUserBuildRequest(resolvedBuildOrder, requestedPackages)

        # Remove recipe packages
        requestedPackagesInOrder = PackageFilter.__FiltersRecipePackages(log, requestedPackagesInOrder, requestedPackages)

        # Remove packages based on the users required features request
        requestedPackagesInOrder = PackageFilter.__FiltersPackagesByRequiredFeature(log, requestedPackagesInOrder, packageFilters.RequiredFeatureNameList)
        # Remove packages based on the available features (remove all packages that can't be build due to missing features)
        requestedPackagesInOrder = PackageFilter.__FiltersPackagesByFeatures(log, requestedPackagesInOrder, featureNameList2)
        # Remove packages based on the available extensions (remove all packages that can't be build due missing extensions)
        requestedPackagesInOrder = PackageFilter.__FiltersPackagesByExtensions(
            log, requestedPackagesInOrder, extensionNameList, featureNameList2, requirementTree
        )
        # Remove packages that are not supported on this platform
        requestedPackagesInOrder = PackageFilter.__FiltersPackagesBySupported(log, requestedPackagesInOrder)
        # Now that we have a filtered list of desired packages, extend it to include all required packages
        return PackageListUtil.GetRequiredPackagesInSourcePackageListOrder(requestedPackagesInOrder, resolvedBuildOrder)

    @staticmethod
    def FilterAppInfo(
        log: Log, resolvedBuildOrder: list[AppInfoPackage], appInfoRequirementTree: AppInfoGlobalRequirementTree, packageFilters: PackageFilters
    ) -> list[AppInfoPackage]:
        """The goal of this filter is to follow the same rules as the 'Filter' method
        so we filter the AppInfoPackage in the same way we would the packages
        """
        useStrictFeatureWarning = True
        # Smart expand the input lists
        # - First we add all parent features automatically to ensure the feature list is complete
        featureNameList2 = PackageFilter.__AddParentFeatures(log, packageFilters.FeatureNameList, appInfoRequirementTree, useStrictFeatureWarning)
        extensionNameList = PackageFilter.__CreateExtensionNameList(resolvedBuildOrder, packageFilters.ExtensionNameList)
        if not extensionNameList.AllowAllExtensions:
            # Filter extensions by available features
            extensionNameList = PackageFilter.__FilterExtensionsByAvailableFeatures(log, featureNameList2, extensionNameList)
            appInfoRequirementTree.SetExtensionSupport(log, extensionNameList)

        # Remove recipe packages (we dont have recipe packages in the app info)
        # requestedPackagesInOrder = PackageFilter.__FiltersRecipePackages(log, requestedPackagesInOrder)

        # Remove packages based on the users required features request (app must have feature, if no executables in resolvedBuildOrder no filtering is done!)
        resolvedBuildOrder = PackageFilter.__FiltersPackagesByRequiredFeature(log, resolvedBuildOrder, packageFilters.RequiredFeatureNameList)
        # Remove packages based on the available features (remove all packages that can't be build due to missing features)
        resolvedBuildOrder = PackageFilter.__FiltersPackagesByFeatures(log, resolvedBuildOrder, featureNameList2)
        # Remove packages based on the available extensions (remove all packages that can't be build due missing extensions)
        resolvedBuildOrder = PackageFilter.__FiltersPackagesByExtensions(log, resolvedBuildOrder, extensionNameList, featureNameList2, appInfoRequirementTree)
        # Remove packages that are not supported on this platform
        resolvedBuildOrder = PackageFilter.__FiltersPackagesBySupported(log, resolvedBuildOrder)

        if packageFilters.ExePackageNameFilter is not None:
            resolvedBuildOrder = PackageFilter.ApplyExePackageNameFilterAppInfo(log, resolvedBuildOrder, packageFilters.ExePackageNameFilter)

        # NOTE: this is probably not necessary for app info
        # Now that we have a filtered list of desired packages, extend it to include all required packages
        # return PackageListUtil.GetRequiredPackagesInSourcePackageListOrder(requestedPackagesInOrder, resolvedBuildOrder)
        return resolvedBuildOrder

    @staticmethod
    def WasThisAExecutableBuildAndAreThereAnyLeft(sourcePackageList: list[Package], packageList: list[Package]) -> bool:
        # If we require executables and had executables available to begin with and if there is no executables left -> exit
        return not (PackageFilter.__ContainsExecutablePackage(sourcePackageList) and not PackageFilter.__ContainsExecutablePackage(packageList))

    @staticmethod
    def FilterBuildablePackages(resolvedBuildOrder: list[Package]) -> list[Package]:
        """Remove any package that aint buildable."""
        result = []
        for package in resolvedBuildOrder:
            if not package.IsVirtual and (package.Type == PackageType.Library or package.Type == PackageType.Executable):
                result.append(package)
        return result

    @staticmethod
    def __CreateExtensionNameList(resolvedBuildOrder: list[CommonPackage] | list[AppInfoPackage], extensionList: ExtensionListManager2) -> ExtensionListManager:
        if extensionList.FilterMethod == FilterMethod.AllowAll:
            return ExtensionListManager(True, [])
        elif extensionList.FilterMethod == FilterMethod.AllowList:
            return ExtensionListManager(False, extensionList.Content)

        uniqueExtensionDict: dict[str, QualifiedRequirementExtensionName] = {}
        for package in resolvedBuildOrder:
            for requirement in package.ResolvedAllRequirements:
                if requirement.Type == PackageRequirementTypeString.Extension:
                    ext = QualifiedRequirementExtensionName(requirement.Extends, requirement.Name)
                    uniqueExtensionDict[ext.ToId()] = ext

        # pop all banned
        for bannedEntry in extensionList.Content:
            bannedId = bannedEntry.ToId()
            uniqueExtensionDict.pop(bannedId, None)

        allowedList = list(uniqueExtensionDict.values())
        return ExtensionListManager(False, allowedList)

    @staticmethod
    def ApplyExePackageNameFilter(log: Log, candidatePackageList: list[CommonPackage], exePackageNameFilter: str) -> list[CommonPackage]:
        result: list[CommonPackage] = []
        for candidatePackage in candidatePackageList:
            if candidatePackage.Type == PackageType.Executable:
                candidateSourcePackageName = ""
                if isinstance(candidatePackage, Package):
                    candidateSourcePackageName = candidatePackage.NameInfo.SourceName
                else:
                    candidateSourcePackageName = candidatePackage.SourcePackage.NameInfo.SourceName
                if PackageFilter.IsMatchForExePackageNameFilter(candidateSourcePackageName, exePackageNameFilter):
                    result.append(candidatePackage)
        if len(result) <= 0:
            raise Exception(f"No executable match the exePackageNameFilter '{exePackageNameFilter}'")
        return result

    @staticmethod
    def ApplyExePackageNameFilterAppInfo(log: Log, candidatePackageList: list[AppInfoPackage], exePackageNameFilter: str) -> list[AppInfoPackage]:
        result: list[AppInfoPackage] = []
        for candidatePackage in candidatePackageList:
            if candidatePackage.Type == PackageType.Executable:
                if PackageFilter.IsMatchForExePackageNameFilter(candidatePackage.SourceName, exePackageNameFilter):
                    result.append(candidatePackage)
        if len(result) <= 0:
            raise Exception(f"No executable match the exePackageNameFilter '{exePackageNameFilter}'")
        return result

    @staticmethod
    def IsMatchForExePackageNameFilter(packageName: str, exePackageNameFilter: str) -> bool:
        res = fnmatch.fnmatch(packageName, exePackageNameFilter)
        return res
