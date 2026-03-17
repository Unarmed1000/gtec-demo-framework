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

import difflib

from FslBuildGen.DataTypes import AccessType, DependencyOutputType, PackageType
from FslBuildGen.Engine.ComplexExternalFlavorConstraints import ComplexExternalFlavorConstraints
from FslBuildGen.Engine.ExternalFlavorConstraints import ExternalFlavorConstraints
from FslBuildGen.Engine.Order.DependencyGraph import DependencyGraph, DependencyGraphNode
from FslBuildGen.Engine.Order.EvaluationPackage import EvaluationPackage, FlavorInfo
from FslBuildGen.Engine.Order.Exceptions import (
    ExtendingFlavorCanNotIntroduceNewOptionsException,
    FlavorExtendParentUndefinedException,
    FlavorNameCollisionException,
    PackageDependencyNotFoundException,
    PackageFlavorDependencyConstraintInvalidException,
    PackageFlavorExtensionOptionDependencyNotFoundException,
    PackageFlavorOptionDependencyNotFoundException,
)
from FslBuildGen.Engine.PackageFlavorName import PackageFlavorName
from FslBuildGen.Engine.PackageFlavorOptionName import PackageFlavorOptionName
from FslBuildGen.Engine.PackageFlavorSelection import PackageFlavorSelection
from FslBuildGen.Engine.PackageFlavorSelections import PackageFlavorSelections
from FslBuildGen.Engine.Unresolved.UnresolvedBasicPackage import UnresolvedBasicPackage
from FslBuildGen.Engine.Unresolved.UnresolvedPackageDependency import UnresolvedPackageDependency
from FslBuildGen.Engine.Unresolved.UnresolvedPackageFlavor import UnresolvedPackageFlavor
from FslBuildGen.Engine.Unresolved.UnresolvedPackageFlavorExtension import UnresolvedPackageFlavorExtension
from FslBuildGen.Engine.Unresolved.UnresolvedPackageFlavorOption import UnresolvedPackageFlavorOption
from FslBuildGen.Engine.Unresolved.UnresolvedPackageName import UnresolvedPackageName
from FslBuildGen.Exceptions import GroupedException
from FslBuildGen.Log import Log
from FslBuildGen.PackageConfig import PackageNameMagicString


class LocalVerbosityLevel:
    Info = 3
    Debug = 4
    Trace = 5


class LocalStrings:
    FlavorConstraintPackageName = "SYS_EXTERNAL_FLAVOR_CONSTRAINT_ON"


class ResolveDepRecord:
    def __init__(self, unresolved: UnresolvedBasicPackage, resolved: EvaluationPackage) -> None:
        super().__init__()
        self.Unresolved = unresolved
        self.Resolved = resolved


class ConstraintOptionRecord:
    def __init__(self, optionName: PackageFlavorOptionName, dependencyStack: list[UnresolvedPackageName]) -> None:
        super().__init__()
        self.OptionName = optionName
        self.DependencyStack = dependencyStack


class ConstraintRecord:
    def __init__(self) -> None:
        super().__init__()
        self.LookupDict: dict[PackageFlavorOptionName, list[ConstraintOptionRecord]] = {}


class PackageBuildOrder:
    @staticmethod
    def ResolveBuildOrder(
        log: Log, allPackages: list[UnresolvedBasicPackage], flavorConstraints: ComplexExternalFlavorConstraints
    ) -> list[UnresolvedBasicPackage]:
        log.LogPrintVerbose(LocalVerbosityLevel.Info, "Resolve build order")
        log.PushIndent()
        try:
            finalBuildOrder: list[UnresolvedBasicPackage] = []
            if len(allPackages) <= 0:
                log.LogPrintVerbose(LocalVerbosityLevel.Info, "No packages supplied")
                finalBuildOrder.append(UnresolvedBasicPackage.Create2(UnresolvedPackageName(PackageNameMagicString.TopLevelName), PackageType.TopLevel))
            else:
                evaluationPackages: list[EvaluationPackage] = PackageBuildOrder.__ResolveEvaluationPackages(log, allPackages)

                PackageBuildOrder.__ValidateDependencies(log, evaluationPackages)

                log.LogPrintVerbose(LocalVerbosityLevel.Info, f"Determining build order for {len(allPackages)} packages")

                graph, topLevelNode = PackageBuildOrder.__CreateDependencyGraph(log, evaluationPackages, flavorConstraints)

                buildOrder = graph.DetermineBuildOrder(topLevelNode.Source)
                for entry in buildOrder:
                    finalBuildOrder.append(entry.SourcePackage)
                finalBuildOrder.append(topLevelNode.Source.SourcePackage)

            PackageBuildOrder.__CheckFlavors(log, finalBuildOrder)
            PackageBuildOrder.__ValidateConstraints(log, finalBuildOrder)
            return finalBuildOrder
        finally:
            log.PopIndent()

    @staticmethod
    def __ValidateDependencies(log: Log, evaluationPackages: list[EvaluationPackage]) -> None:
        log.LogPrintVerbose(LocalVerbosityLevel.Info, "Validating that the dependencies are acyclic")
        exceptionList: list[Exception] | None = None
        for package in evaluationPackages:
            try:
                PackageBuildOrder.__ValidateDependenciesFor(package)
            except Exception as ex:  # pylint: disable=broad-except
                exceptionList = PackageBuildOrder.__AddException(exceptionList, ex)
                break  # for now just break on the first exception

        PackageBuildOrder.__CheckForExceptions(exceptionList)

    @staticmethod
    def __ValidateDependenciesFor(package: EvaluationPackage) -> None:
        allPackages: list[EvaluationPackage] = PackageBuildOrder.__ExtractTouchedPackageList(package)
        graph = DependencyGraph(allPackages)
        graph.DetermineBuildOrder(package)

    @staticmethod
    def __ExtractTouchedPackageList(startPackage: EvaluationPackage) -> list[EvaluationPackage]:
        packageDict: dict[UnresolvedPackageName, EvaluationPackage] = {}
        packageDict[startPackage.Name] = startPackage

        packageQueue: list[EvaluationPackage] = []
        packageQueue.append(startPackage)

        while len(packageQueue) > 0:
            currentPackage = packageQueue.pop(0)
            for depRecord in currentPackage.DirectDependencies:
                if depRecord.Package.Name not in packageDict:
                    packageDict[depRecord.Package.Name] = depRecord.Package
                    packageQueue.append(depRecord.Package)
        return list(packageDict.values())

    @staticmethod
    def __ResolveEvaluationPackages(log: Log, allPackages: list[UnresolvedBasicPackage]) -> list[EvaluationPackage]:
        # Build lookup dict
        packageNameToNodeDict = PackageBuildOrder.__CreatePackageNameToPackageDict(log, allPackages)

        log.LogPrintVerbose(LocalVerbosityLevel.Info, "Building evaluation packages")
        # then resolve all direct dependencies
        res: list[EvaluationPackage] = []
        exceptionList: list[Exception] | None = None
        for record in packageNameToNodeDict.values():
            package = record.Unresolved

            # packageDependencies = [] # type: List[EvaluationPackage]
            for dep in package.DirectDependencies:
                if dep.Name not in packageNameToNodeDict:
                    exceptionList = PackageBuildOrder.__AddException(
                        exceptionList, PackageBuildOrder.__CreatePackageDependencyNotFoundException(package, dep, allPackages)
                    )
                else:
                    depRecord: ResolveDepRecord = packageNameToNodeDict[dep.Name]
                    record.Resolved.DirectDependencies.append(EvaluationPackage.DependencyRecord(depRecord.Resolved, None))

            for flavor in package.Flavors:
                for flavorOption in flavor.Options:
                    for dep in flavorOption.DirectDependencies:
                        if dep.Name not in packageNameToNodeDict:
                            exceptionList = PackageBuildOrder.__AddException(
                                exceptionList,
                                PackageBuildOrder.__CreatePackageFlavorOptionDependencyNotFoundException(package, flavor, flavorOption, dep, allPackages),
                            )
                        else:
                            depRecord = packageNameToNodeDict[dep.Name]
                            record.Resolved.DirectDependencies.append(
                                EvaluationPackage.DependencyRecord(depRecord.Resolved, FlavorInfo(flavor.Name, flavorOption.Name))
                            )

            for flavorExtension in package.FlavorExtensions:
                for flavorOption in flavorExtension.Options:
                    for dep in flavorOption.DirectDependencies:
                        if dep.Name not in packageNameToNodeDict:
                            exceptionList = PackageBuildOrder.__AddException(
                                exceptionList,
                                PackageBuildOrder.__CreatePackageFlavorExtensionOptionDependencyNotFoundException(
                                    package, flavorExtension, flavorOption, dep, allPackages
                                ),
                            )
                        else:
                            depRecord = packageNameToNodeDict[dep.Name]
                            record.Resolved.DirectDependencies.append(
                                EvaluationPackage.DependencyRecord(depRecord.Resolved, FlavorInfo(flavor.Name, flavorOption.Name))
                            )

            record.Resolved.Seal()
            res.append(record.Resolved)

        PackageBuildOrder.__CheckForExceptions(exceptionList)
        return res

    @staticmethod
    def __CreatePackageNameToPackageDict(log: Log, allPackages: list[UnresolvedBasicPackage]) -> dict[UnresolvedPackageName, ResolveDepRecord]:
        log.LogPrintVerbose(LocalVerbosityLevel.Info, "Building package lookup dict")

        exceptionList: list[Exception] | None = None

        # Build lookup dict
        packageNameToPackageDict: dict[UnresolvedPackageName, ResolveDepRecord] = {}
        for package in allPackages:
            if package.Name in packageNameToPackageDict:
                exceptionList = PackageBuildOrder.__AddException(exceptionList, Exception(f"PackageName '{package.Name}' defined multiple times"))

            packageNameToPackageDict[package.Name] = ResolveDepRecord(package, EvaluationPackage(package.Name, package, []))

        PackageBuildOrder.__CheckForExceptions(exceptionList)
        return packageNameToPackageDict

    @staticmethod
    def __CreateDependencyGraph(
        log: Log, allPackages: list[EvaluationPackage], flavorConstraints: ComplexExternalFlavorConstraints
    ) -> tuple[DependencyGraph, DependencyGraphNode]:
        graph = DependencyGraph(allPackages)

        # Add the final top level node so we can determine the global build order
        node: DependencyGraphNode = PackageBuildOrder.__CreateTopLevelNode(log, graph, flavorConstraints)

        return (graph, node)

    @staticmethod
    def __CreateTopLevelNode(log: Log, graph: DependencyGraph, flavorConstraints: ComplexExternalFlavorConstraints) -> DependencyGraphNode:
        log.LogPrintVerbose(LocalVerbosityLevel.Info, "Creating top level node to determine global build order")

        # Extract the top level nodes
        topLevelPackage: EvaluationPackage = PackageBuildOrder.__CreateTopLevelPackage(log, graph, flavorConstraints)

        # Add the top level node to the graph with all it dependency edges
        node = graph.AddNode(topLevelPackage)
        graph.AddPackageDirectDependencies(node)
        return node

    @staticmethod
    def __CreateTopLevelPackage(log: Log, graph: DependencyGraph, flavorConstraints: ComplexExternalFlavorConstraints) -> EvaluationPackage:
        # Extract the top level nodes and add them as direct dependencies to a 'top level node' (so we can determine the global build order)
        topLevelNodes: list[DependencyGraphNode] = graph.FindNodesWithNoIncomingDependencies()

        if len(topLevelNodes) < 1:
            raise Exception("Internal error, no top level nodes found, so either no packages are available or there is a circular dependency")

        # If there are no top level nodes, that means we are likely to have a circular dependency
        # so we just rely on the circular dependency detection to handle it
        log.LogPrintVerbose(LocalVerbosityLevel.Info, f"Main packages: {len(topLevelNodes)}")

        unresolvedDirectDependencies: list[UnresolvedPackageDependency] = []
        packageDirectDependencies: list[EvaluationPackage.DependencyRecord] = []
        if not flavorConstraints.HasConstraints():
            for srcNode in topLevelNodes:
                packageDirectDependencies.append(EvaluationPackage.DependencyRecord(srcNode.Source, None))
                unresolvedDirectDependencies.append(UnresolvedPackageDependency(srcNode.Source.Name, AccessType.Public, DependencyOutputType.Reference, True))
        else:
            for srcNode in topLevelNodes:
                flavorInfo = PackageBuildOrder.__ToFlavorInfo(flavorConstraints.GetFlavorConstraints(srcNode.Source.Name.Value))
                if log.Verbosity >= 4:
                    log.LogPrint(f"- '{srcNode.Source.Name}' applying constraints '{PackageBuildOrder.__ToString(flavorInfo)}'")
                externalConstraintNode = PackageBuildOrder.__CreateExternalFlavorConstraintNode(graph, srcNode, flavorInfo)
                packageDirectDependencies.append(EvaluationPackage.DependencyRecord(externalConstraintNode.Source, None))
                unresolvedDirectDependencies.append(
                    UnresolvedPackageDependency(externalConstraintNode.Source.Name, AccessType.Public, DependencyOutputType.Reference, True)
                )

        newTopLevel = UnresolvedBasicPackage(
            UnresolvedPackageName(PackageNameMagicString.TopLevelName), PackageType.TopLevel, unresolvedDirectDependencies, [], []
        )
        return EvaluationPackage(newTopLevel.Name, newTopLevel, packageDirectDependencies)

    @staticmethod
    def __ToString(flavorInfoList: list[FlavorInfo]) -> str:
        strFlavor = [f"{entry.FlavorName.Value}={entry.FlavorOption.Value}" for entry in flavorInfoList]
        return ", ".join(strFlavor)

    @staticmethod
    def __CreateExternalFlavorConstraintNode(graph: DependencyGraph, graphRootNode: DependencyGraphNode, flavorInfo: list[FlavorInfo]) -> DependencyGraphNode:
        # When we are using external flavor constraints we basically add some internal ExternalFlavorConstraint nodes in front of the root nodes that
        # contain the flavor dependency constraints. The only packages that satisfy the constraints will be the nodes that these internal constraint
        # nodes depend upon.
        flavorConstraints = PackageBuildOrder.__CreatePackageFlavorSelections(flavorInfo)
        constrainedDirectDependency = UnresolvedPackageDependency(
            graphRootNode.Source.Name, AccessType.Public, DependencyOutputType.Reference, True, flavorConstraints
        )
        constraintPackageName = UnresolvedPackageName(f"{LocalStrings.FlavorConstraintPackageName}_{graphRootNode.Source.Name.Value}", True)
        constraintSourcePackage = UnresolvedBasicPackage(constraintPackageName, PackageType.ExternalFlavorConstraint, [constrainedDirectDependency], [], [])
        directDependencies = [EvaluationPackage.DependencyRecord(graphRootNode.Source, None)]
        constraintPackage = EvaluationPackage(constraintPackageName, constraintSourcePackage, directDependencies)

        # Add the constract node to the graph with all it dependency edges
        constraintNode = graph.AddNode(constraintPackage)
        graph.AddPackageDirectDependencies(constraintNode)
        return constraintNode

    @staticmethod
    def __CreatePackageFlavorSelections(flavorInfo: list[FlavorInfo]) -> PackageFlavorSelections:
        selections = [PackageFlavorSelection(info.FlavorName, info.FlavorOption) for info in flavorInfo]
        return PackageFlavorSelections(selections)

    @staticmethod
    def __ToFlavorInfo(flavorConstraints: ExternalFlavorConstraints) -> list[FlavorInfo]:
        return [FlavorInfo(key, value) for key, value in flavorConstraints.Dict.items()]

    @staticmethod
    def __CreatePackageDependencyNotFoundException(
        package: UnresolvedBasicPackage, dep: UnresolvedPackageDependency, debugAllPackages: list[UnresolvedBasicPackage]
    ) -> Exception:
        topCandidates: str = PackageBuildOrder.__BuildPackageCandidateString(dep.Name, debugAllPackages)
        return PackageDependencyNotFoundException(package.Name.Value, dep.Name.Value, topCandidates)

    @staticmethod
    def __CreatePackageFlavorOptionDependencyNotFoundException(
        package: UnresolvedBasicPackage,
        flavor: UnresolvedPackageFlavor,
        flavorOption: UnresolvedPackageFlavorOption,
        dep: UnresolvedPackageDependency,
        debugAllPackages: list[UnresolvedBasicPackage],
    ) -> Exception:
        topCandidates: str = PackageBuildOrder.__BuildPackageCandidateString(dep.Name, debugAllPackages)
        return PackageFlavorOptionDependencyNotFoundException(
            f"Package '{package.Name}' flavor '{flavor.Name}' option '{flavorOption}' dependency to '{dep.Name}' not found, did you mean '{topCandidates}'"
        )

    @staticmethod
    def __CreatePackageFlavorExtensionOptionDependencyNotFoundException(
        package: UnresolvedBasicPackage,
        flavorExtension: UnresolvedPackageFlavorExtension,
        flavorOption: UnresolvedPackageFlavorOption,
        dep: UnresolvedPackageDependency,
        debugAllPackages: list[UnresolvedBasicPackage],
    ) -> Exception:
        topCandidates: str = PackageBuildOrder.__BuildPackageCandidateString(dep.Name, debugAllPackages)
        return PackageFlavorExtensionOptionDependencyNotFoundException(
            f"Package '{package.Name}' flavor '{flavorExtension.Name}' option '{flavorOption}' dependency to '{dep.Name}' not found, did you mean '{topCandidates}'"
        )

    @staticmethod
    def __BuildPackageCandidateString(unresolvedPackageName: UnresolvedPackageName, debugAllPackages: list[UnresolvedBasicPackage]) -> str:
        ratingList: list[tuple[UnresolvedBasicPackage, float]] = []

        # Build a possible candidate array
        for possiblePackage in debugAllPackages:
            ratio = difflib.SequenceMatcher(None, unresolvedPackageName.Value, possiblePackage.Name.Value).ratio()
            ratingList.append((possiblePackage, ratio))

        ratingList.sort(key=lambda s: -s[1])

        candidateList = [entry[0].Name.Value for entry in ratingList]
        candidateList = candidateList[0:5]

        topCandidates: str = ", ".join(candidateList)
        return topCandidates

    @staticmethod
    def __AddException(exceptionList: list[Exception] | None, exception: Exception) -> list[Exception]:
        if exceptionList is None:
            exceptionList = []
        exceptionList.append(exception)
        return exceptionList

    # -----------------------------------------------------------------------------------------------------------------------------------------------

    @staticmethod
    def __CheckFlavors(log: Log, finalBuildOrder: list[UnresolvedBasicPackage]) -> None:
        log.LogPrintVerbose(LocalVerbosityLevel.Info, "Checking flavors")

        # Create a dict of all the original flavor definitions for quick lookup when we do a basic verification of the flavor extensions
        flavorDefinitionDict: dict[str, UnresolvedPackageFlavor] = {}
        for package in finalBuildOrder:
            for flavor in package.Flavors:
                if flavor.Name.Id not in flavorDefinitionDict:
                    flavorDefinitionDict[flavor.Name.Id] = flavor
                else:
                    previousDefinition = flavorDefinitionDict[flavor.Name.Id]
                    raise FlavorNameCollisionException(previousDefinition, flavor)

        # Now validate all the flavor extensions by checking
        # - its a known flavor that can be extended
        # - the flavor options are valid
        for package in finalBuildOrder:
            for flavorExtension in package.FlavorExtensions:
                if flavorExtension.Name.Id not in flavorDefinitionDict:
                    # The parent we are trying to extend is unknown
                    raise FlavorExtendParentUndefinedException(flavorExtension)
                else:
                    # Validate each option is valid
                    flavorDefinition = flavorDefinitionDict[flavorExtension.Name.Id]
                    invalidOptions = PackageBuildOrder.__TryGetFlavorInvalidOptions(flavorDefinition, flavorExtension)
                    if invalidOptions is not None and len(invalidOptions) > 0:
                        raise ExtendingFlavorCanNotIntroduceNewOptionsException(flavorDefinition, flavorExtension, invalidOptions)

    @staticmethod
    def __TryGetFlavorInvalidOptions(
        baseFlavor: UnresolvedPackageFlavor, extendingVariant: UnresolvedPackageFlavorExtension
    ) -> list[PackageFlavorOptionName] | None:
        res: list[PackageFlavorOptionName] | None = None
        for entry in extendingVariant.Options:
            if baseFlavor.TryGetOptionByName(entry.Name) is None:
                if res is None:
                    res = []
                res.append(entry.Name)
        return res

    # -----------------------------------------------------------------------------------------------------------------------------------------------

    @staticmethod
    def __ValidateConstraints(log: Log, finalBuildOrder: list[UnresolvedBasicPackage]) -> None:
        log.LogPrintVerbose(LocalVerbosityLevel.Info, "Validating flavor constraints")

        lookupDict: dict[UnresolvedPackageName, UnresolvedBasicPackage] = {}
        for package in finalBuildOrder:
            lookupDict[package.Name] = package

        depStack: list[UnresolvedPackageName] = []
        for package in finalBuildOrder:
            PackageBuildOrder.__ValidatePackageConstraints(log, lookupDict, depStack, package)

    @staticmethod
    def __ValidatePackageConstraints(
        log: Log, lookupDict: dict[UnresolvedPackageName, UnresolvedBasicPackage], depStack: list[UnresolvedPackageName], package: UnresolvedBasicPackage
    ) -> None:
        if package.Type == PackageType.TopLevel:
            return
        PackageBuildOrder.__CheckConstraintExists(lookupDict, package)
        constraintDict: dict[PackageFlavorName, ConstraintRecord] = {}
        depStack.clear()
        PackageBuildOrder.__ProcessDependencyConstraints(constraintDict, depStack, package, lookupDict)

        errors: list[tuple[str, list[str]]] = []
        for entry in constraintDict.items():
            if len(entry[1].LookupDict) > 1:
                locationStrings: list[str] = []
                for dep in entry[1].LookupDict.items():
                    for option in dep[1]:
                        depOrderStr = "->".join([depStackEntry.Value for depStackEntry in option.DependencyStack])
                        locationStrings.append(f"'{entry[0]}'='{dep[0]}' at {depOrderStr}")
                optionStr = " && ".join([keyName.Value for keyName in entry[1].LookupDict])
                errors.append((f"{entry[0]}={optionStr}", locationStrings))

        if len(errors) > 0:
            ", ".join([error[0] for error in errors])
            constraintsLocDesc: str = ", ".join([", ".join(error[1]) for error in errors])
            raise Exception(f"Mutually exclusive constraints encountered while resolving '{package.Name}' constraints=({constraintsLocDesc})")

    @staticmethod
    def __CheckConstraintExists(lookupDict: dict[UnresolvedPackageName, UnresolvedBasicPackage], package: UnresolvedBasicPackage) -> None:
        for dep in package.DirectDependencies:
            for depConstraint in dep.FlavorConstraints.Selections:
                if depConstraint.Name.OwnerPackageName not in lookupDict:
                    raise Exception(f"Dependency to unknown package '{depConstraint.Name.OwnerPackageName}'")
                targetPackage = lookupDict[depConstraint.Name.OwnerPackageName]
                targetFlavor = targetPackage.TryGetFlavor(depConstraint.Name)
                if targetFlavor is None:
                    raise Exception(f"Dependency to unknown flavor '{depConstraint.Name}' in package '{depConstraint.Name.OwnerPackageName}'")
                if not targetFlavor.IsValidOptionName(depConstraint.Option):
                    raise PackageFlavorDependencyConstraintInvalidException(package.Name, depConstraint, targetFlavor)

    @staticmethod
    def __ProcessDependencyConstraints(
        constraintDict: dict[PackageFlavorName, ConstraintRecord],
        depStack: list[UnresolvedPackageName],
        unresolvedPackage: UnresolvedBasicPackage,
        packageDict: dict[UnresolvedPackageName, UnresolvedBasicPackage],
    ) -> None:
        depStack.append(unresolvedPackage.Name)
        for dep in unresolvedPackage.DirectDependencies:
            if len(dep.FlavorConstraints.Selections) > 0:
                for constraintOption in dep.FlavorConstraints.Selections:
                    if constraintOption.Name not in constraintDict:
                        record = ConstraintRecord()
                        constraintDict[constraintOption.Name] = record
                    else:
                        record = constraintDict[constraintOption.Name]

                    if constraintOption.Option not in record.LookupDict:
                        optionRecordList: list[ConstraintOptionRecord] = []
                        record.LookupDict[constraintOption.Option] = optionRecordList
                    else:
                        optionRecordList = record.LookupDict[constraintOption.Option]
                    depList: list[UnresolvedPackageName] = list(depStack)
                    depList.reverse()
                    optionRecordList.append(ConstraintOptionRecord(constraintOption.Option, depList))

            if dep.Name not in packageDict:
                raise Exception(f"Unknown dependency '{dep.Name}")
            depPackage = packageDict[dep.Name]
            PackageBuildOrder.__ProcessDependencyConstraints(constraintDict, depStack, depPackage, packageDict)
        depStack.pop()

    @staticmethod
    def __CheckForExceptions(exceptionList: list[Exception] | None) -> None:
        if exceptionList is not None and len(exceptionList) > 0:
            if len(exceptionList) > 1:
                raise GroupedException(exceptionList)
            else:
                raise exceptionList[0]
