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


from FslBuildGen.Exceptions import UsageErrorException
from FslBuildGen.Packages.Package import Package


class DependencyGraphNode:
    def __init__(self, package: Package) -> None:
        self.Name: str = package.Name
        self.From: list[DependencyGraphNode] = []
        self.To: list[DependencyGraphNode] = []
        self.Package: Package = package

    def AddEdge(self, toNode: "DependencyGraphNode") -> None:
        if toNode not in self.To:
            self.To.append(toNode)
            toNode.From.append(self)

    def RemoveToEdges(self) -> None:
        for node in self.To:
            node.From.remove(self)

    def RemoveFromEdges(self) -> None:
        for node in self.From:
            node.To.remove(self)


class DependencyGraph:
    def __init__(self, package: Package | None) -> None:  # , exploreVariants):
        # self.ExploreVariants = exploreVariants

        self.UniqueNodeDict: dict[Package, DependencyGraphNode] = {}
        self.Nodes: list[DependencyGraphNode] = []

        if package is not None:
            self.AddNode(package)
            self.Finalize()

    def Get(self, package: Package) -> DependencyGraphNode | None:
        for node in self.Nodes:
            if node.Package == package:
                return node
        return None

    def RemoveNodesWithNoDependencies(self) -> list[DependencyGraphNode]:
        """This is useful for finding the build order"""
        removeList: list[DependencyGraphNode] = []
        for node in self.Nodes:
            if len(node.To) <= 0:
                removeList.append(node)

        for node in removeList:
            node.RemoveFromEdges()
            self.Nodes.remove(node)
        return removeList

    def GetNodesWithNoIncomingDependencies(self) -> list[DependencyGraphNode]:
        resultList: list[DependencyGraphNode] = []
        for node in self.Nodes:
            if len(node.From) <= 0:
                resultList.append(node)
        return resultList

    def RemoveNodesWithNoIncomingDependencies(self) -> list[DependencyGraphNode]:
        """This is useful for finding the dependency order"""
        removeList = self.GetNodesWithNoIncomingDependencies()
        for node in removeList:
            node.RemoveToEdges()
            self.Nodes.remove(node)
        return removeList

    def AddNode(self, package: Package) -> None:
        if package not in self.UniqueNodeDict:
            self.UniqueNodeDict[package] = DependencyGraphNode(package)
            for dep in package.ResolvedDirectDependencies:
                self.AddNode(dep.Package)
            # if self.ExploreVariants:
            #    for dep in package.ResolvedDirectVariantDependencies:
            #        self.AddNode(dep.Package)

    def AddNodeAndEdges(self, package: Package) -> None:
        if package not in self.UniqueNodeDict:
            newNode = DependencyGraphNode(package)
            for dep in package.ResolvedDirectDependencies:
                if dep.Package not in self.UniqueNodeDict:
                    raise UsageErrorException(f"Unknown dependency to: '{dep.Name}'")
                newNode.AddEdge(self.UniqueNodeDict[dep.Package])
            self.UniqueNodeDict[package] = newNode
            self.Nodes.append(newNode)

    def Finalize(self) -> None:
        self.Nodes = list(self.UniqueNodeDict.values())
        for key, value in list(self.UniqueNodeDict.items()):
            for dep in key.ResolvedDirectDependencies:
                value.AddEdge(self.UniqueNodeDict[dep.Package])
