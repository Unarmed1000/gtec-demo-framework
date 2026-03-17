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

from enum import Enum

from FslBuildGen.Engine.PackageFlavorSelections import PackageFlavorSelections
from FslBuildGen.Engine.Resolver.PackageName import PackageName
from FslBuildGen.Engine.Resolver.ResolvedPackage import ResolvedPackage
from FslBuildGen.Engine.Resolver.ResolvedPackageInstance import ResolvedPackageInstance

# from FslBuildGen.Engine.Resolver.PackageDependency import PackageDependency


class EdgeType(Enum):
    # A edge between instances
    Normal = 0
    # A edge to the template used to create the instance
    InstanceTemplate = 1
    # A edge between templates
    Template = 2
    # A edge between templates originating from a specific flavor
    TemplateFlavor = 3


class EdgeRecord:
    def __init__(self, node: "ResolvedPackageGraphNode", edgeType: EdgeType, constraint: PackageFlavorSelections | None, desc: str | None) -> None:
        super().__init__()
        self.Node = node
        self.Constraint = constraint
        self.Type = edgeType
        self.Desc = desc


class ResolvedPackageGraphNode:
    def __init__(self, source: ResolvedPackage) -> None:
        self.Source = source
        self.From: list[ResolvedPackageGraphNode] = []
        self.To: list[EdgeRecord] = []

    def AddEdge(self, toNode: "ResolvedPackageGraphNode", edgeType: EdgeType, constraint: PackageFlavorSelections | None, desc: str | None) -> None:
        if toNode == self:
            raise Exception("Can't add edge to self")

        if ResolvedPackageGraphNode.__FindIndex(self.To, toNode, edgeType, constraint, desc) < 0:
            self.To.append(EdgeRecord(toNode, edgeType, constraint, desc))
            toNode.From.append(self)

    @staticmethod
    def __FindIndex(
        edgeRecords: list[EdgeRecord], toNode: "ResolvedPackageGraphNode", edgeType: EdgeType, constraint: PackageFlavorSelections | None, desc: str | None
    ) -> int:
        for index, x in enumerate(edgeRecords):
            if x.Node == toNode and x.Type == edgeType and x.Constraint == constraint and x.Desc == desc:
                return index
        return -1


class ResolvedPackageGraph:
    def __init__(self) -> None:
        super().__init__()
        self.__uniqueNodeDict: dict[ResolvedPackage, ResolvedPackageGraphNode] = {}
        self.__instanceNodeDict: dict[PackageName, ResolvedPackageGraphNode] = {}
        self.__nodes: list[ResolvedPackageGraphNode] = []
        self.HasExternalContraints = False

    def DebugNodes(self) -> list[ResolvedPackageGraphNode]:
        return self.__nodes

    def AddNode(self, package: ResolvedPackage) -> ResolvedPackageGraphNode:
        node = ResolvedPackageGraphNode(package)
        self.__uniqueNodeDict[package] = node
        self.__nodes.append(node)
        if isinstance(package, ResolvedPackageInstance):
            self.__instanceNodeDict[package.Name] = node
        return node

    def AddEdge(
        self,
        fromObj: ResolvedPackageGraphNode | ResolvedPackage,
        toObj: ResolvedPackageGraphNode | ResolvedPackage,
        edgeType: EdgeType,
        constraint: PackageFlavorSelections | None,
        desc: str | None,
    ) -> None:
        if isinstance(fromObj, ResolvedPackage):
            if fromObj not in self.__uniqueNodeDict:
                toName = toObj.Name if isinstance(toObj, ResolvedPackage) else toObj.Source.Name
                raise Exception(f"fromObj: {fromObj.Name} not found (To:{toName})")
            fromNode = self.__uniqueNodeDict[fromObj]
        else:
            fromNode = fromObj

        if isinstance(toObj, ResolvedPackage):
            if toObj not in self.__uniqueNodeDict:
                fromName = fromObj.Name if isinstance(fromObj, ResolvedPackage) else fromObj.Source.Name
                raise Exception(f"toObj: {toObj.Name} not found (From:{fromName})")
            toNode = self.__uniqueNodeDict[toObj]
        else:
            toNode = toObj

        fromNode.AddEdge(toNode, edgeType, constraint, desc)

    def FindNodesWithNoIncomingDependencies(self) -> list[ResolvedPackageGraphNode]:
        return [entry for entry in self.__nodes if len(entry.From) <= 0]

    def FindNodesWithNoOutgoingDependencies(self) -> list[ResolvedPackageGraphNode]:
        return [entry for entry in self.__nodes if len(entry.To) <= 0]
