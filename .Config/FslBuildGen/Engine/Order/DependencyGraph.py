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


from FslBuildGen.Engine.Order.EvaluationPackage import EvaluationPackage
from FslBuildGen.Engine.Order.Exceptions import CircularDependencyInDependentPackageException
from FslBuildGen.Exceptions import CircularDependencyException


class DependencyGraphNode:
    def __init__(self, source: EvaluationPackage) -> None:
        super().__init__()
        self.Source = source
        self.From: list[DependencyGraphNode] = []
        self.To: list[DependencyGraphNode] = []

    def AddEdge(self, toNode: "DependencyGraphNode") -> None:
        if toNode == self:
            raise Exception("Can't add edge to self")

        if toNode not in self.To:
            self.To.append(toNode)
            toNode.From.append(self)

    def RemoveEdges(self) -> None:
        for edge in self.From:
            edge.To.remove(self)

        for edge in self.To:
            edge.From.remove(self)

        self.From.clear()
        self.To.clear()

    def __str__(self) -> str:
        return str(self.Source)


class DependencyGraph:
    def __init__(self, allPackages: list[EvaluationPackage] | None = None) -> None:
        super().__init__()
        self.__uniqueNodeDict: dict[EvaluationPackage, DependencyGraphNode] = {}
        self.__nodes: list[DependencyGraphNode] = []
        if allPackages is not None:
            self.AddAllNodes(allPackages)
            self.AddAllDependencies(allPackages)

    def Empty(self) -> bool:
        return len(self.__nodes) <= 0

    def DebugNodes(self) -> list[DependencyGraphNode]:
        return self.__nodes

    def Contains(self, package: EvaluationPackage) -> bool:
        return package in self.__uniqueNodeDict

    def GetNode(self, package: EvaluationPackage) -> DependencyGraphNode:
        return self.__uniqueNodeDict[package]

    def Remove(self, node: DependencyGraphNode) -> None:
        node.RemoveEdges()
        self.__uniqueNodeDict.pop(node.Source, None)
        self.__nodes.remove(node)

    def AddNode(self, package: EvaluationPackage) -> DependencyGraphNode:
        node = DependencyGraphNode(package)
        self.__uniqueNodeDict[package] = node
        self.__nodes.append(node)
        return node

    def AddEdge(self, fromObj: DependencyGraphNode | EvaluationPackage, toObj: DependencyGraphNode | EvaluationPackage) -> None:
        if isinstance(fromObj, EvaluationPackage):
            if fromObj not in self.__uniqueNodeDict:
                raise Exception(f"Unknown node: '{fromObj.Name}'")
            fromNode = self.__uniqueNodeDict[fromObj]
        else:
            fromNode = fromObj

        if isinstance(toObj, EvaluationPackage):
            if toObj not in self.__uniqueNodeDict:
                raise Exception(f"Unknown node: '{toObj.Name}'")
            toNode = self.__uniqueNodeDict[toObj]
        else:
            toNode = toObj

        fromNode.AddEdge(toNode)

    def FindNodesWithNoIncomingDependencies(self) -> list[DependencyGraphNode]:
        return [entry for entry in self.__nodes if len(entry.From) <= 0]

    def FindNodesWithNoOutgoingDependencies(self) -> list[DependencyGraphNode]:
        return [entry for entry in self.__nodes if len(entry.To) <= 0]

    def AddAllNodes(self, allPackages: list[EvaluationPackage]) -> None:
        for package in allPackages:
            self.AddNode(package)

    def AddPackageDirectDependencies(self, node: DependencyGraphNode) -> None:
        for depPackage in node.Source.DirectDependencies:
            self.AddEdge(node, depPackage.Package)

    def AddAllDependencies(self, allPackages: list[EvaluationPackage]) -> None:
        for package in allPackages:
            self.AddPackageDirectDependencies(self.GetNode(package))

    def DetermineBuildOrder(self, rootPackage: EvaluationPackage) -> list[EvaluationPackage]:
        """
        This extract the correct build order, but it also clears the graph!
        """
        orderedDependencyList: list[EvaluationPackage] = []

        rootNode = self.GetNode(rootPackage)
        while not self.Empty():
            removedNodes: list[DependencyGraphNode] = self.RemoveNodesWithNoIncomingDependencies()
            if len(removedNodes) <= 0:
                self.__HandleCircularDependencies(rootNode)

            removedNodes.sort(key=lambda s: s.Source.Name.Value.upper())

            for node in removedNodes:
                if node != rootNode:
                    orderedDependencyList.append(node.Source)

        orderedDependencyList.reverse()
        return orderedDependencyList

    def __RemoveNodesWithNoOutgoingDependencies(self) -> list[DependencyGraphNode]:
        removeList: list[DependencyGraphNode] = self.FindNodesWithNoOutgoingDependencies()
        for node in removeList:
            self.Remove(node)
        return removeList

    def RemoveNodesWithNoIncomingDependencies(self) -> list[DependencyGraphNode]:
        """
        This is useful for finding the dependency order
        """
        removeList: list[DependencyGraphNode] = self.FindNodesWithNoIncomingDependencies()
        for node in removeList:
            self.Remove(node)
        return removeList

    def __HandleCircularDependencies(self, node: DependencyGraphNode) -> None:
        # Nodes without outgoing dependencies can not be part of the cycle -> so we remove them to simplify the graph
        while len(self.__RemoveNodesWithNoOutgoingDependencies()) > 0:
            pass

        # check if this package is actually part of the circular dependency or not
        if not self.Contains(node.Source):
            raise CircularDependencyInDependentPackageException(f"'{node.Source.Name}' uses a package that has a circular dependency")

        # We are only interested in the dependencies that start at packageNode
        circularDependencies: list[list[DependencyGraphNode]] = []
        dependencies: list[DependencyGraphNode] = [node]
        DependencyGraph.__BuildDependencyList(circularDependencies, dependencies, node)

        circularDepStringsSet: set[str] = set()
        for circularDep in circularDependencies:
            circularDepStringsSet.add(DependencyGraph.__GetCircularDependencyString(circularDep))

        circularDepStrings: list[str] = list(circularDepStringsSet)
        circularDepStrings.sort(key=lambda s: (DependencyGraph.__CountArrows(s), s.upper()))

        strDependencies = "\n  ".join(circularDepStrings)
        raise CircularDependencyException(f"Circular dependency detected while validating {node.Source.Name.Value}:\n  {strDependencies}")

    @staticmethod
    def __CountArrows(strContent: str) -> int:
        count = 0
        index = strContent.find("->")
        lenStr = len(strContent)
        while index >= 0 and ((index + 2) < lenStr):
            count = count + 1
            index = strContent.find("->", index + 2)
        return count

    @staticmethod
    def __GetCircularDependencyString(srcList: list[DependencyGraphNode]) -> str:
        if srcList is None or len(srcList) < 2:
            raise Exception("No circular dependency exist in the supplied list")

        # Since the last element is where the circular dependency is detected
        # we need to find the first occurrence of the last element
        endIndex = len(srcList) - 1
        lastElement = srcList[endIndex]
        foundIndex = srcList.index(lastElement) if lastElement in srcList else -1
        if foundIndex < 0 or foundIndex == endIndex:
            raise Exception("No circular dependency exist in the supplied list")
        del srcList[0:foundIndex]
        return "->".join([entry.Source.Name.Value for entry in srcList])

    @staticmethod
    def __BuildDependencyList(
        circularDependencies: list[list[DependencyGraphNode]], dependencies: list[DependencyGraphNode], node: DependencyGraphNode
    ) -> None:
        for depNode in node.To:
            if depNode not in dependencies:
                dependencies.append(depNode)
                DependencyGraph.__BuildDependencyList(circularDependencies, dependencies, depNode)
                dependencies.pop()
            else:
                circularDependencyList: list[DependencyGraphNode] = list(dependencies)
                circularDependencyList.append(depNode)
                circularDependencies.append(circularDependencyList)
