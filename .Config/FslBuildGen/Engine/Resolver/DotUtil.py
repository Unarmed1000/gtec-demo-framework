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

import os
import subprocess
from enum import Enum

from FslBuildGen import IOUtil
from FslBuildGen.Engine.GraphImageSaveInfo import GraphImageSaveInfo
from FslBuildGen.Engine.PackageFlavorName import PackageFlavorName
from FslBuildGen.Engine.PackageFlavorOptionName import PackageFlavorOptionName
from FslBuildGen.Engine.PackageFlavorSelections import PackageFlavorSelections
from FslBuildGen.Engine.Resolver.ResolvedPackage import ResolvedPackage
from FslBuildGen.Engine.Resolver.ResolvedPackageGraph import EdgeRecord, EdgeType, ResolvedPackageGraph, ResolvedPackageGraphNode
from FslBuildGen.Engine.Resolver.ResolvedPackageInstance import ResolvedPackageInstance
from FslBuildGen.Engine.Resolver.ResolvedPackageTemplate import ResolvedPackageTemplate
from FslBuildGen.Log import Log


class LocalVerbosityLevel:
    Info = 3
    Debug = 4
    Trace = 5


class RenderMode(Enum):
    Templates = 0
    Instances = 1
    Full = 2
    Raw = 3


class DotUtil:
    @staticmethod
    def __ToDotFile(graph: ResolvedPackageGraph, graphImageSaveInfo: GraphImageSaveInfo) -> list[str]:
        renderMode = RenderMode.Full
        showDependenciesToBasePackages = False

        instanceNodes: list[ResolvedPackageGraphNode] = []
        templateNodes: list[ResolvedPackageGraphNode] = []

        allNodes = graph.DebugNodes()
        allowedTemplateFilterSet: set[str] = set()
        DotUtil.__Extract(instanceNodes, templateNodes, allNodes)
        DotUtil.__ExtractUsedTemplates(allowedTemplateFilterSet, instanceNodes)

        nodeNameDict: dict[ResolvedPackage, str] = {}
        for node in allNodes:
            nodeNameDict[node.Source] = DotUtil.__ToNodeName(node.Source, renderMode != RenderMode.Raw)

        # Find all base packages
        basePackages = DotUtil.__FindBasePackages(graphImageSaveInfo, allNodes)

        res: list[str] = []
        res.append("digraph xmlTest")
        res.append("{")
        res.append("  overlap=scale;")
        res.append("  splines=true;")
        res.append("  edge [len=1];")

        if renderMode == RenderMode.Templates:
            DotUtil.__AddTemplateNodes(nodeNameDict, res, templateNodes)
        elif renderMode == RenderMode.Instances:
            DotUtil.__AddInstanceNodes(nodeNameDict, res, instanceNodes)
        elif renderMode == RenderMode.Full or renderMode == RenderMode.Raw:
            DotUtil.__AddTemplateNodes(nodeNameDict, res, templateNodes)
            DotUtil.__AddInstanceNodes(nodeNameDict, res, instanceNodes)
        else:
            raise Exception("Unsupported renderMode")

        if renderMode == RenderMode.Raw:
            for node in allNodes:
                for toRecord in node.To:
                    if isinstance(toRecord.Node.Source, ResolvedPackageInstance) and (
                        showDependenciesToBasePackages or toRecord.Node.Source.Name.Value not in basePackages
                    ):
                        res.append(f"  {nodeNameDict[node.Source]} -> {nodeNameDict[toRecord.Node.Source]}")
        elif renderMode == RenderMode.Full or renderMode == RenderMode.Instances:
            for node in instanceNodes:
                for toRecord in node.To:
                    if (
                        isinstance(toRecord.Node.Source, ResolvedPackageInstance)
                        and DotUtil.__IsFirstDependencyReference(node.To, toRecord.Node)
                        and (showDependenciesToBasePackages or toRecord.Node.Source.Name.Value not in basePackages)
                    ):
                        res.append(f"  {nodeNameDict[node.Source]} -> {nodeNameDict[toRecord.Node.Source]}")

        res.append("  edge [color=Blue, style=dashed]")

        edgeTypeFilter = EdgeType.Template
        if renderMode == RenderMode.Full or renderMode == RenderMode.Raw:
            DotUtil.__AddEdgesFull(nodeNameDict, allNodes, res, edgeTypeFilter, False, showDependenciesToBasePackages, basePackages)
        elif renderMode == RenderMode.Templates:
            DotUtil.__AddEdgesTemplates(nodeNameDict, allNodes, res, edgeTypeFilter, True, showDependenciesToBasePackages, basePackages)

        res.append("  edge [color=Orange, style=dotted]")
        edgeTypeFilter = EdgeType.TemplateFlavor
        if renderMode == RenderMode.Full or renderMode == RenderMode.Raw:
            DotUtil.__AddEdgesFull(nodeNameDict, allNodes, res, edgeTypeFilter, False, showDependenciesToBasePackages, basePackages)
        elif renderMode == RenderMode.Templates:
            DotUtil.__AddEdgesTemplates(nodeNameDict, allNodes, res, edgeTypeFilter, True, showDependenciesToBasePackages, basePackages)

        res.append("}")
        return res

    @staticmethod
    def __FindBasePackages(graphImageSaveInfo: GraphImageSaveInfo, allNodes: list[ResolvedPackageGraphNode]) -> set[str]:
        packageDict: dict[str, ResolvedPackage] = {}
        for srcNode in allNodes:
            packageDict[srcNode.Source.Name.Value] = srcNode.Source

        basePackages: set[str] = set()
        for projectContext in graphImageSaveInfo.ProjectContexts:
            for basePackage in projectContext.BasePackages:
                basePackages.add(packageDict[basePackage.Name].Name.Value)
                basePackages.add(packageDict[basePackage.Name].Name.Value + "<>")
        return basePackages

    @staticmethod
    def __AddEdgesTemplates(
        nodeNameDict: dict[ResolvedPackage, str],
        allNodes: list[ResolvedPackageGraphNode],
        res: list[str],
        edgeTypeFilter: EdgeType,
        removeInherited: bool,
        showDependenciesToBasePackages: bool,
        basePackages: set[str],
    ) -> None:
        for node in allNodes:
            for toRecord in node.To:
                if (
                    toRecord.Type == edgeTypeFilter
                    and isinstance(node.Source, ResolvedPackageTemplate)
                    and isinstance(toRecord.Node.Source, ResolvedPackageTemplate)
                    and (not removeInherited or DotUtil.__IsFirstDependencyReference(node.To, toRecord.Node))
                    and (showDependenciesToBasePackages or toRecord.Node.Source.Name.Value not in basePackages)
                ):
                    res.append(f"  {nodeNameDict[node.Source]} -> {nodeNameDict[toRecord.Node.Source]}")

    @staticmethod
    def __AddEdgesFull(
        nodeNameDict: dict[ResolvedPackage, str],
        allNodes: list[ResolvedPackageGraphNode],
        res: list[str],
        edgeTypeFilter: EdgeType,
        removeInherited: bool,
        showDependenciesToBasePackages: bool,
        basePackages: set[str],
    ) -> None:
        for node in allNodes:
            for toRecord in node.To:
                if toRecord.Type == edgeTypeFilter:
                    toPackage = toRecord.Node.Source
                    if (
                        isinstance(toPackage, ResolvedPackageTemplate)
                        and (not removeInherited or DotUtil.__IsFirstDependencyReference(node.To, toRecord.Node))
                        and (showDependenciesToBasePackages or toRecord.Node.Source.Name.Value not in basePackages)
                    ):
                        strEdge = f"  {nodeNameDict[node.Source]} -> {nodeNameDict[toPackage]}"

                        label: str | None = None
                        if toRecord.Type == EdgeType.TemplateFlavor:
                            label = toRecord.Desc
                        if toRecord.Constraint is not None:
                            strConstraint = DotUtil.__TryExtractLabel(toRecord.Constraint)
                            if strConstraint is not None:
                                label = f"{label}<{strConstraint}>" if label is not None else f"<{strConstraint}>"
                        if label is not None:
                            strEdge += f'[taillabel = "{label}"]'
                        res.append(strEdge)

    @staticmethod
    def __IsFirstDependencyReference(edges: list[EdgeRecord], srcNode: ResolvedPackageGraphNode) -> bool:
        visitedSet: set[ResolvedPackageGraphNode] = set()
        return all(not (edge.Node != srcNode and DotUtil.__FindNodeInDependencies(edge.Node.To, srcNode, visitedSet)) for edge in edges)

    @staticmethod
    def __FindNodeInDependencies(edges: list[EdgeRecord], srcNode: ResolvedPackageGraphNode, visitedSet: set[ResolvedPackageGraphNode]) -> bool:
        for edge in edges:
            if edge.Node not in visitedSet:
                visitedSet.add(edge.Node)
                if edge.Node == srcNode:
                    return True
                if DotUtil.__FindNodeInDependencies(edge.Node.To, srcNode, visitedSet):
                    return True
        return False

    #     private static ResolvedPackageTemplateDependency TryLocateDependency(ResolvedPackageTemplate fromPackage, ResolvedPackageTemplate toPackage)
    #     {
    #       if (fromPackage == null)
    #         return null;

    #       foreach (var dep in fromPackage.DirectDependencies)
    #       {
    #         if (dep.Template.Name == toPackage.Name)
    #         {
    #           return dep;
    #         }
    #       }
    #       return null;
    #     }

    @staticmethod
    def __AddInstanceNodes(nodeNameDict: dict[ResolvedPackage, str], res: list[str], instanceNodes: list[ResolvedPackageGraphNode]) -> None:
        res.append("  node [color=Black];")
        for node in instanceNodes:
            res.append(f"  {nodeNameDict[node.Source]}")

    @staticmethod
    def __AddTemplateNodes(
        nodeNameDict: dict[ResolvedPackage, str], res: list[str], templateNodes: list[ResolvedPackageGraphNode], templateNameFilterSet: set[str] | None = None
    ) -> None:
        indent = "  "
        for node in templateNodes:
            sourceEx = node.Source
            if isinstance(sourceEx, ResolvedPackageTemplate) and (templateNameFilterSet is None or sourceEx.Name.Value in templateNameFilterSet):
                res.append(f"{indent}node [color=blue];")
                res.append(f'{indent}{nodeNameDict[sourceEx]} [label="{DotUtil.__ToNodeLabel(sourceEx)}"]')

    @staticmethod
    def __ExtractUsedTemplates(allowedTemplateFilterSet: set[str], instanceNodes: list[ResolvedPackageGraphNode]) -> None:
        foundSet: set[ResolvedPackageTemplate] = set()
        for node in instanceNodes:
            if isinstance(node.Source, ResolvedPackageInstance) and node.Source.FlavorTemplate is not None:
                foundSet.add(node.Source.FlavorTemplate)
                allowedTemplateFilterSet.add(node.Source.FlavorTemplate.Name.Value)

        resolveSet: set[ResolvedPackageTemplate] = set()

        while len(foundSet) > 0:
            tmp = foundSet
            foundSet = resolveSet
            resolveSet = tmp
            foundSet.clear()

            for entry in resolveSet:
                for dependency in entry.DirectDependencies:
                    if dependency.Template.Name.Value not in allowedTemplateFilterSet:
                        foundSet.add(dependency.Template)
                        allowedTemplateFilterSet.add(dependency.Template.Name.Value)

    @staticmethod
    def __Extract(
        instanceNodes: list[ResolvedPackageGraphNode], templateNodes: list[ResolvedPackageGraphNode], allNodes: list[ResolvedPackageGraphNode]
    ) -> None:
        for node in allNodes:
            if isinstance(node.Source, ResolvedPackageInstance):
                instanceNodes.append(node)
            else:
                templateNodes.append(node)

    #     /// <summary>
    #     /// All possible combinations
    #     /// </summary>
    #     /// <param name="package"></param>
    #     /// <returns></returns>
    #     //private static string ToCombinationsNodeName(ResolvedPackageTemplate package)
    #     //{
    #     //  if(package.InstanceConfigs.Count == 0)
    #     //    return $"\"{package.Name.Value}<>\"";
    #     //  var res = new string[package.InstanceConfigs.Count];
    #     //  for (int i = 0; i < res.Length; ++i)
    #     //  {
    #     //    var instanceConfig = package.InstanceConfigs[i];
    #     //    res[i] = instanceConfig.Description;
    #     //  }
    #     //  return $"\"{package.Name.Value}<{string.Join(" * ", res)}>\"";
    #     //}

    #     private static string ToNodeLabel2(ResolvedPackageTemplate package)
    #     {
    #       if (package.PackageFlavors.Count == 0)
    #         return $"\"{package.Name.Value}<>\"";
    #       var res = new string[package.PackageFlavors.Count];
    #       for (int i = 0; i < res.Length; ++i)
    #       {
    #         res[i] = ExtractFlavorConfigurationString(package.PackageFlavors[i]); ;
    #       }
    #       return $"{package.Name.Value}<{string.Join(" * ", res)}>";
    #     }

    @staticmethod
    def __ToNodeLabel(package: ResolvedPackageTemplate) -> str:
        if len(package.InstanceConfigs) == 0:
            return f'"{package.Name.Value}<>"'

        fullFlavorHash = DotUtil.__BuildPartialFlavorDict(package)
        flavorDict = DotUtil.__BuildUsageDict(package)
        res = DotUtil.__ExtractFlavorCombination(flavorDict, fullFlavorHash)
        return "{}<{}>".format(package.Name.Value, ", ".join(res))

    @staticmethod
    def __ExtractFlavorCombination(flavorDict: dict[PackageFlavorName, set[PackageFlavorOptionName]], fullFlavorHash: set[PackageFlavorName]) -> list[str]:
        flavorNames: list[PackageFlavorName] = []
        for flavorName in flavorDict:
            flavorNames.append(flavorName)
        flavorNames.sort(key=lambda s: s.Value.upper())

        res: list[str] = []
        for flavorName in flavorNames:
            entry = flavorDict[flavorName]
            options: list[str] = []

            for optionEntry in entry:
                options.append(optionEntry.Value)
            options.sort(key=lambda s: s.upper())

            if flavorName in fullFlavorHash:
                res.append("{}={}".format(flavorName, "|".join(options)))
            else:
                res.append("[{}={}]".format(flavorName, "|".join(options)))
        return res

    @staticmethod
    def __BuildUsageDict(package: ResolvedPackageTemplate) -> dict[PackageFlavorName, set[PackageFlavorOptionName]]:
        flavorDict: dict[PackageFlavorName, set[PackageFlavorOptionName]] = {}
        for instanceConfig in package.InstanceConfigs:
            for flavor in instanceConfig.FlavorSelections.Selections:
                if flavor.Name not in flavorDict:
                    optionSet: set[PackageFlavorOptionName] = set()
                    flavorDict[flavor.Name] = optionSet
                else:
                    optionSet = flavorDict[flavor.Name]
                optionSet.add(flavor.Option)
        return flavorDict

    @staticmethod
    def __BuildPartialFlavorDict(package: ResolvedPackageTemplate) -> set[PackageFlavorName]:
        flavorDict: dict[PackageFlavorName, int] = {}
        for instanceConfig in package.InstanceConfigs:
            for flavor in instanceConfig.FlavorSelections.Selections:
                if flavor.Name in flavorDict:
                    flavorDict[flavor.Name] += 1
                else:
                    flavorDict[flavor.Name] = 1

        targetHitCount = len(package.InstanceConfigs)
        fullFlavorHash: set[PackageFlavorName] = set()
        for pairKey, pairValue in flavorDict.items():
            if pairValue == targetHitCount:
                fullFlavorHash.add(pairKey)
        return fullFlavorHash

    #     private static string ExtractFlavorConfigurationString(ResolvedPackageFlavor flavor)
    #     {
    #       if (flavor.Options.Length == 1)
    #         return $"{flavor.Name.Value}={flavor.Options[0].Name}";
    #       var res = new string[flavor.Options.Length];
    #       for (int i = 0; i < res.Length; ++i)
    #       {
    #         res[i] = flavor.Options[i].Name.Value;
    #       }
    #       return $"{flavor.Name.Value}={string.Join("|", res)}";
    #     }

    @staticmethod
    def __ToResolvedPackageTemplateNodeName(package: ResolvedPackageTemplate, useSmartName: bool) -> str:
        return f'"{package.Name.SmartValue if useSmartName else package.Name.Value}<>"'

    @staticmethod
    def __ToResolvedPackageInstanceNodeName(package: ResolvedPackageInstance, useSmartName: bool) -> str:
        return f'"{package.Name.SmartValue if useSmartName else package.Name.Value}"'

    @staticmethod
    def __ToNodeName(package: ResolvedPackage, useSmartName: bool = True) -> str:
        if isinstance(package, ResolvedPackageTemplate):
            return DotUtil.__ToResolvedPackageTemplateNodeName(package, useSmartName)
        if isinstance(package, ResolvedPackageInstance):
            return DotUtil.__ToResolvedPackageInstanceNodeName(package, useSmartName)
        raise Exception("ToNodeName failed")

    #     //private static string TryExtractLabel(ResolvedPackageTemplate from, ResolvedPackageTemplate to)
    #     //{
    #     //  foreach(var dep in from.DirectDependencies)
    #     //  {
    #     //    if(dep.Template == to)
    #     //    {
    #     //      return TryExtractLabel(dep.FlavorConstraint);
    #     //    }
    #     //  }
    #     //  return null;
    #     //}

    @staticmethod
    def __TryExtractLabel(depFlavor: PackageFlavorSelections) -> str | None:
        if depFlavor is not None and len(depFlavor.Selections) > 0:
            flavors: list[str] = []
            for entry in depFlavor.Selections:
                flavors.append(f"{entry.Name.Value}={entry.Option.Value}")
            return ", ".join(flavors)
        return None

    @staticmethod
    def ToFile(log: Log, filename: str, graph: ResolvedPackageGraph, graphImageSaveInfo: GraphImageSaveInfo) -> None:
        lines = DotUtil.__ToDotFile(graph, graphImageSaveInfo)
        content = "\n".join(lines)

        dotFilename = f"{filename}.dot"

        log.LogPrintVerbose(LocalVerbosityLevel.Debug, f"Writing dot file to '{dotFilename}'")
        IOUtil.WriteFileIfChanged(dotFilename, content)

        outputFile = f"{filename}.png"

        try:
            log.LogPrintVerbose(LocalVerbosityLevel.Debug, f"Writing png file to '{outputFile}'")
            subprocess.call(["dot", "-Tpng", f"-o{outputFile}", dotFilename])
            os.remove(dotFilename)
        except Exception:
            print("WARNING: Failed to execute dot, is it part of the path?")
            os.remove(dotFilename)
            raise
