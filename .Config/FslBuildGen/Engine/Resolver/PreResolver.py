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


from FslBuildGen.DataTypes import PackageRequirementTypeString
from FslBuildGen.Engine.Resolver.PreResolvePackageResult import PreResolvePackageResult
from FslBuildGen.Engine.Resolver.ProcessedPackage import ProcessedPackage
from FslBuildGen.Exceptions import GroupedException, PackageRequirementExtendsUnusedFeatureException
from FslBuildGen.Log import Log
from FslBuildGen.Packages.ExceptionsXml import RequirementNameCollisionException
from FslBuildGen.Packages.PackageInstanceName import PackageInstanceName
from FslBuildGen.Packages.PackageNameInfo import PackageNameInfo
from FslBuildGen.Packages.PackageRequirement import PackageRequirement
from FslBuildGen.Packages.Unresolved.Exceptions import RequirementUseDuplicatedException
from FslBuildGen.Packages.Unresolved.UnresolvedPackageRequirement import UnresolvedPackageRequirement


class LocalVerbosityLevel:
    Info = 3
    Debug = 4
    Trace = 5


class PreResolver:
    @staticmethod
    def PreResolve(log: Log, resolvedBuildOrder: list[ProcessedPackage]) -> list[PreResolvePackageResult]:
        log.LogPrintVerbose(LocalVerbosityLevel.Info, "Preresolve")
        log.PushIndent()
        try:
            packageLookupDict: dict[str, PreResolvePackageResult] = {}

            preResolveBuildOrder: list[PreResolvePackageResult] = []

            for buildIndex, package in enumerate(resolvedBuildOrder):
                packageResult = PreResolver.PreResolvePackage(log, packageLookupDict, package, buildIndex)
                preResolveBuildOrder.append(packageResult)
                packageLookupDict[package.NameInfo.FullName.Value] = packageResult

            PreResolver.__ValidateRequirementExtends(log, preResolveBuildOrder)
            return preResolveBuildOrder
        finally:
            log.PopIndent()

    @staticmethod
    def CreatePackageLookupDict(packages: list[PreResolvePackageResult]) -> dict[str, PreResolvePackageResult]:
        packageLookupDict: dict[str, PreResolvePackageResult] = {}
        for packageResult in packages:
            packageLookupDict[packageResult.SourcePackage.NameInfo.FullName.Value] = packageResult
        return packageLookupDict

    @staticmethod
    def PreResolvePackage(
        log: Log, packageLookupDict: dict[str, PreResolvePackageResult], package: ProcessedPackage, resolvedBuildIndex: int
    ) -> PreResolvePackageResult:
        # determine if this package is supported on this platform based on if all its dependencies are supported or not
        isSupported = PreResolver.__PreResolveIsPlatformSupported(log, packageLookupDict, package)

        # Resolve the direct used requirements
        resolvedDirectRequirements = PreResolver.__ResolvePackageDirectRequirements(log, packageLookupDict, package)

        # Resolve all used requirements
        resolvedAllRequirements = PreResolver.__ResolvePackageAllRequirements(log, packageLookupDict, package, resolvedDirectRequirements)

        # extract feature requirements (this will be a sorted list since resolvedDirectRequirements is already sorted)
        resolvedDirectUsedFeatures = [requirement for requirement in resolvedDirectRequirements if requirement.Type == PackageRequirementTypeString.Feature]

        # extract all used features (this will be a sorted list since resolvedAllRequirements is already sorted)
        resolvedAllUsedFeatures = [requirement for requirement in resolvedAllRequirements if requirement.Type == PackageRequirementTypeString.Feature]

        # Figure out the build order of all dependent packages
        resolvedBuildOrder: list[PreResolvePackageResult] = PreResolver.__ResolvePackageBuildOrder(log, packageLookupDict, package)

        packageResult = PreResolvePackageResult(
            package,
            isSupported,
            resolvedDirectRequirements,
            resolvedAllRequirements,
            resolvedDirectUsedFeatures,
            resolvedAllUsedFeatures,
            resolvedBuildIndex,
            resolvedBuildOrder,
        )
        return packageResult

    @staticmethod
    def __PreResolveIsPlatformSupported(log: Log, packageLookupDict: dict[str, PreResolvePackageResult], package: ProcessedPackage) -> bool:
        # Determine if the package is supported or not
        isSupported = package.DirectPlatformSupported
        if isSupported:
            # Ensure that if one package dependency is marked as NotSupported this package gets marked as not supported too
            for directDependency in package.DirectDependencies:
                if directDependency.Name.Value not in packageLookupDict:
                    raise Exception(
                        f"Internal error during pre-resolve, the package '{package.NameInfo.FullName}' has a dependency to '{directDependency.Name}' has not been resolved yet"
                    )
                depPackage = packageLookupDict[directDependency.Name.Value]
                if not depPackage.ResolvedPlatformSupported:
                    isSupported = False
        return isSupported

    @staticmethod
    def __ResolvePackageBuildOrder(log: Log, packageLookupDict: dict[str, PreResolvePackageResult], package: ProcessedPackage) -> list[PreResolvePackageResult]:
        if len(package.DirectDependencies) <= 0:
            return []

        # Discover all unique packages that we depend on directly or indirectly
        foundPackageDict: dict[str, PreResolvePackageResult] = {}
        # Since we know this is being called in the final resolved build order we just need to look at all the direct dependencies and
        # take the sum of their ResolvedBuildOrder lists
        for dependency in package.DirectDependencies:
            dependencyPackage = packageLookupDict[dependency.Name.Value]
            foundPackageDict[dependency.Name.Value] = dependencyPackage
            for resolvedPackage in dependencyPackage.ResolvedBuildOrder:
                foundPackageDict[resolvedPackage.SourcePackage.NameInfo.FullName.Value] = resolvedPackage

        # Create a sorted list based on the ResolvedBuildIndex
        resolvedBuildOrder: list[PreResolvePackageResult] = list(foundPackageDict.values())
        resolvedBuildOrder.sort(key=lambda s: s.ResolvedBuildIndex)
        return resolvedBuildOrder

    @staticmethod
    def __ExtractRequirements(
        resResolvedDirectRequirements: list[PackageRequirement],
        uniqueDict: dict[str, set[str]],
        nameInfo: PackageNameInfo,
        directRequirements: list[UnresolvedPackageRequirement],
    ) -> None:
        for requirement1 in directRequirements:
            packageRequirement = PackageRequirement(requirement1, nameInfo.FullName.Value)
            uniqueSet = uniqueDict.get(packageRequirement.GroupId, set())
            if packageRequirement.Id in uniqueSet:
                raise RequirementUseDuplicatedException(nameInfo.FullName, requirement1)
            resResolvedDirectRequirements.append(packageRequirement)

            uniqueSet.add(packageRequirement.Id)
            if len(uniqueSet) == 1:
                uniqueDict[packageRequirement.GroupId] = uniqueSet

    @staticmethod
    def __ResolvePackageDirectRequirements(
        log: Log, packageLookupDict: dict[str, PreResolvePackageResult], package: ProcessedPackage
    ) -> list[PackageRequirement]:
        uniqueDict: dict[str, set[str]] = {}
        # Resolve the direct used requirements
        resResolvedDirectRequirements: list[PackageRequirement] = []

        PreResolver.__ExtractRequirements(resResolvedDirectRequirements, uniqueDict, package.NameInfo, package.DirectRequirements)
        PreResolver.__ExtractRequirements(resResolvedDirectRequirements, uniqueDict, package.NameInfo, package.ResolvedPlatform.DirectRequirements)

        resResolvedDirectRequirements.sort(key=lambda s: s.Id)
        return resResolvedDirectRequirements

    @staticmethod
    def __ResolvePackageAllRequirements(
        log: Log, packageLookupDict: dict[str, PreResolvePackageResult], package: ProcessedPackage, packageResolvedDirectRequirements: list[PackageRequirement]
    ) -> list[PackageRequirement]:
        # Resolve all used requirements
        resResolvedAllRequirements: list[PackageRequirement] = []
        uniqueRequirementFullIds: dict[str, PackageRequirement] = {}
        fullIdDict: dict[str, tuple[PackageInstanceName, str]] = {}
        for directDependency in package.DirectDependencies:
            depPackage = packageLookupDict[directDependency.Name.Value]
            for requirement2 in depPackage.ResolvedAllRequirements:
                if requirement2.FullId not in uniqueRequirementFullIds:
                    # ensure that we dont have two requirement names with different casing
                    if requirement2.FullId in fullIdDict:
                        raise RequirementNameCollisionException(
                            depPackage.SourcePackage.NameInfo.FullName,
                            requirement2.Name,
                            fullIdDict[requirement2.FullId][0],
                            fullIdDict[requirement2.FullId][1],
                        )
                    fullIdDict[requirement2.FullId] = (depPackage.SourcePackage.NameInfo.FullName, requirement2.FullId)
                    resResolvedAllRequirements.append(requirement2)
                    uniqueRequirementFullIds[requirement2.FullId] = requirement2
                else:
                    for introPackageName in requirement2.IntroducedByPackages:
                        uniqueIntroPackage = uniqueRequirementFullIds[requirement2.FullId]
                        if introPackageName not in uniqueIntroPackage.IntroducedByPackages:
                            uniqueIntroPackage.IntroducedByPackages.add(introPackageName)

        # Add this package's direct used requirements to
        for requirement3 in packageResolvedDirectRequirements:
            if requirement3.FullId not in uniqueRequirementFullIds:
                # ensure that we dont have two requirement names with different casing
                if requirement3.FullId in fullIdDict:
                    raise RequirementNameCollisionException(
                        package.NameInfo.FullName, requirement3.Name, fullIdDict[requirement3.FullId][0], fullIdDict[requirement3.FullId][1]
                    )
                fullIdDict[requirement3.FullId] = (package.NameInfo.FullName, requirement3.FullId)
                resResolvedAllRequirements.append(requirement3)
                uniqueRequirementFullIds[requirement3.FullId] = requirement3
            else:
                for introPackageName in requirement3.IntroducedByPackages:
                    uniqueIntroPackage = uniqueRequirementFullIds[requirement3.FullId]
                    if introPackageName not in uniqueIntroPackage.IntroducedByPackages:
                        uniqueIntroPackage.IntroducedByPackages.add(introPackageName)

        resResolvedAllRequirements.sort(key=lambda s: s.Id)
        return resResolvedAllRequirements

    @staticmethod
    def __ValidateRequirementExtends(log: Log, finalResolveOrder: list[PreResolvePackageResult]) -> None:
        errors = []
        for package in finalResolveOrder:
            for requirement in package.ResolvedDirectRequirements:
                if requirement.Type != PackageRequirementTypeString.Feature or len(requirement.Extends) != 0:
                    parent = PreResolver.__TryLocateRequiredFeature(package.ResolvedAllRequirements, requirement.Extends)
                    if parent is None:
                        errors.append((package, requirement))
        if len(errors) > 0:
            exceptionList: list[Exception] = []
            for error in errors:
                exceptionList.append(
                    PackageRequirementExtendsUnusedFeatureException(error[1].Name, error[1].Extends, error[0].SourcePackage.NameInfo.FullName.Value)
                )
            raise GroupedException(exceptionList)

    @staticmethod
    def __TryLocateRequiredFeature(allUsedRequirements: list[PackageRequirement], name: str) -> PackageRequirement | None:
        for requirement in allUsedRequirements:
            if requirement.Type == PackageRequirementTypeString.Feature and requirement.Name == name:
                return requirement
        return None
