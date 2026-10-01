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

# from typing import Set
# from FslBuildGen import PackageListUtil
# from FslBuildGen.Config import Config

from FslBuildGen.DataTypes import AccessType, FilterMode, PackageType
from FslBuildGen.DependencyGraph import DependencyGraph
from FslBuildGen.Engine.BasicBuildConfig import BasicBuildConfig
from FslBuildGen.Engine.EngineResolveConfig import EngineResolveConfig
from FslBuildGen.Exceptions import InternalErrorException
from FslBuildGen.ExternalVariantConstraints import ExternalVariantConstraints
from FslBuildGen.Generator.GeneratorInfo import GeneratorInfo
from FslBuildGen.Log import Log
from FslBuildGen.PackageConfig import PackageNameMagicString
from FslBuildGen.PackageManager import PackageManager, PackageManagerFilter
from FslBuildGen.Packages.Package import Package, PackageDependency
from FslBuildGen.ToolConfig import ToolConfig
from FslBuildGen.Xml.XmlBase2 import FakeXmlGenFileDependency
from FslBuildGen.Xml.XmlGenFile import XmlGenFile


class PackageBuilder:
    def __init__(
        self,
        log: Log,
        configBuildDir: str,
        configIgnoreNotSupported: bool,
        toolConfig: ToolConfig,
        platformName: str,
        hostPlatformName: str,
        basicBuildConfig: BasicBuildConfig,
        generatorInfo: GeneratorInfo,
        genFiles: list[XmlGenFile],
        packageManagerFilter: PackageManagerFilter,
        externalVariantConstraints: ExternalVariantConstraints,
        engineResolveConfig: EngineResolveConfig,
        filterMode: FilterMode,
        allowExeDependency: bool,
        logVerbosity: int = 1,
        writeGraph: bool = False,
    ) -> None:
        super().__init__()

        # create top level package and resolve build order
        log.LogPrintVerbose(logVerbosity, "Validating dependencies")

        packageManager = PackageManager(
            log,
            configBuildDir,
            configIgnoreNotSupported,
            toolConfig,
            platformName,
            hostPlatformName,
            basicBuildConfig,
            generatorInfo,
            genFiles,
            packageManagerFilter,
            externalVariantConstraints,
            engineResolveConfig,
            writeGraph,
            filterMode,
            allowExeDependency,
        )
        packages = packageManager.Packages

        # Build a graph containing all packages
        graph = DependencyGraph(None)
        for package in packages:
            graph.AddNode(package)
        graph.Finalize()

        # Extract the top level nodes, the dependencies were validated to be acyclic when the packages were resolved,
        # so there is always at least one top level node unless there are no packages at all
        nodes = graph.GetNodesWithNoIncomingDependencies()
        topLevelGenFile = XmlGenFile(log, toolConfig, toolConfig.DefaultPackageLanguage)
        topLevelGenFile.Name = PackageNameMagicString.TopLevelName
        topLevelGenFile.SetType(PackageType.TopLevel)
        for entry in nodes:
            topLevelGenFile.DirectDependencies.append(FakeXmlGenFileDependency(log, entry.Name, AccessType.Public))
        topLevelGenFile.DirectDependencies.sort(key=lambda s: s.Name.lower())

        topLevelPackage = packageManager.CreatePackage(
            log, configBuildDir, configIgnoreNotSupported, toolConfig, platformName, hostPlatformName, topLevelGenFile, True
        )
        graph.AddNodeAndEdges(topLevelPackage)

        self.__ResolveBuildOrders(packages)
        self.AllPackages: list[Package] = packages
        self.TopLevelPackage = topLevelPackage
        self.__ResolveAllPackageDependencies(log, topLevelPackage)

    #        self.TopLevelPackage = PackageListUtil.GetTopLevelPackage(packages)
    #        self.__ResolveAllPackageDependencies(config, self.TopLevelPackage)

    def __ResolveBuildOrders(self, packages: list[Package]) -> None:
        for package in packages:
            graph = DependencyGraph(package)
            orderedDependencyList: list[Package] = []
            self.__ResolveBuildOrderFor(orderedDependencyList, package, graph)
            orderedDependencyList.reverse()
            package.ResolvedBuildOrder = orderedDependencyList
            # NOTE: we can not filter the dependency list based on 'ExperimentalRecipes' here since we need to resolve it first
            # package.ResolvedExperimentalRecipeBuildOrder = [entry for entry in orderedDependencyList if not entry.DirectExperimentalRecipe is None]

    def __ResolveBuildOrderFor(self, rOrderedDependencyList: list[Package], package: Package, graph: DependencyGraph) -> None:
        packageNode = graph.Get(package)
        # TODO: make the get method never return none
        if packageNode is None:
            raise Exception("the package should always have a node in the graph")
        while len(graph.Nodes) > 0:
            nodes = graph.RemoveNodesWithNoIncomingDependencies()
            if len(nodes) <= 0:
                # Circular dependencies are rejected when the packages are resolved (Engine.Order), so this can not happen
                raise InternalErrorException(f"Package '{package.Name}' has a circular dependency that was not detected when the packages were resolved")
            nodes.sort(key=lambda node: node.Name.lower())
            for node in nodes:
                # if node != packageNode:
                rOrderedDependencyList.append(node.Package)

    def __ResolveAllPackageDependencies(self, log: Log, topLevel: Package) -> None:
        for package in topLevel.ResolvedBuildOrder:
            self.__DoResolveAllPackageDependencies(log, package)

    def __DoResolveAllPackageDependencies(self, log: Log, package: Package) -> None:
        # FIX: we are doing some of the same checks twice here
        addedDict: dict[str, PackageDependency] = {}
        # First we resolve all direct dependencies
        for dep in package.ResolvedDirectDependencies:
            if dep.Name not in addedDict:
                package.ResolvedAllDependencies.append(dep)
                addedDict[dep.Name] = dep
            else:
                # The package was already added so we need to check if this dependency is less restrictive than the old one
                oldDep = addedDict[dep.Name]
                if dep.Access.value < oldDep.Access.value:
                    package.ResolvedAllDependencies.remove(oldDep)
                    addedDict[dep.Name] = dep

        # Then we pull in the children's dependencies
        resolvedDirectDependencies = list(package.ResolvedDirectDependencies)
        for directDep in resolvedDirectDependencies:
            for dep in directDep.Package.ResolvedAllDependencies:
                # What the direct dependency reaches through Public edges only is reached with the access of the edge to the direct dependency,
                # what it reaches through a Private or Link edge can only be linked
                access = directDep.Access if dep.Access == AccessType.Public else AccessType.Link
                if access != dep.Access:
                    dep = PackageDependency(dep.Package, access, dep.OutputType, dep.ReferenceOutputAssembly)
                if dep.Name not in addedDict:
                    package.ResolvedAllDependencies.append(dep)
                    addedDict[dep.Name] = dep
                else:
                    # The package was already added so we need to check if this dependency is less restrictive than the old one
                    oldDep = addedDict[dep.Name]
                    if dep.Access.value < oldDep.Access.value:
                        package.ResolvedAllDependencies.remove(oldDep)
                        package.ResolvedAllDependencies.append(dep)
                        addedDict[dep.Name] = dep
                        foundDep = self.__TryFindDep(package.ResolvedDirectDependencies, dep)
                        if foundDep is not None:
                            log.DoPrintWarning(
                                f"Package '{package.Name}' requested dependency access to '{dep.Name}', overwritten by dependency from '{directDep.Name}'"
                            )
                            package.ResolvedDirectDependencies.remove(foundDep)
                            package.ResolvedDirectDependencies.append(dep)

        package.ResolvedDirectDependencies.sort(key=lambda s: s.Name.lower())
        package.ResolvedAllDependencies.sort(key=lambda s: s.Name.lower())

        # tmp = []
        # for dep in package.ResolvedAllDependencies:
        #    tmp.append(dep.Name)
        # print ("%s -> %s" % (package.Name, ", ".join(tmp)))

    def __TryFindDep(self, deps: list[PackageDependency], findDep: PackageDependency) -> PackageDependency | None:
        for dep in deps:
            if dep.Name == findDep.Name:
                return dep
        return None
