#!/usr/bin/env python3

# ****************************************************************************************************************************************************
# Copyright 2026 NXP
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

# Package dependency-graph queries for FslBuildInfo.
#
# The full package graph is already resolved and held in memory as forward edges on each Package
# (ResolvedDirectDependencies / ResolvedAllDependencies). This module provides a thin query layer on
# top of that resolved model:
# - forward:  what does package X depend on            (GetDependencies)
# - reverse:  who depends on package X                 (GetDependents)
# - path:     the dependency chain(s) from A to B      (FindDependencyPaths)
# - dump:     every package -> its direct dependencies (BuildAdjacency)
#
# Reverse edges are not stored on Package, so they are derived once when the index is built.

import json
from typing import Any

from FslBuildGen.DataTypes import AccessType, PackageType
from FslBuildGen.Log import Log
from FslBuildGen.Packages.Package import Package, PackageDependency

# Safety cap so a densely connected graph can not explode when enumerating every simple path.
# When the cap is hit we log it (no silent truncation).
MAX_DEPENDENCY_PATHS = 256


class QueryFormat:
    Text = "text"
    Json = "json"

    @staticmethod
    def AllStrings() -> list[str]:
        return [QueryFormat.Text, QueryFormat.Json]


class GraphEdge:
    """A resolved graph neighbour and the access type of the edge that reaches it (if meaningful)."""

    def __init__(self, package: Package, access: AccessType | None) -> None:
        self.Package = package
        # For transitive dependents there is no single meaningful access type, so it can be None.
        self.Access = access


class PackageGraphIndex:
    """A queryable view of the resolved package graph.

    The node universe is 'topLevelPackage.ResolvedBuildOrder' (every real package pulled in), matching
    what InfoSaver and the --stats output already operate on.
    """

    def __init__(self, topLevelPackage: Package) -> None:
        super().__init__()
        # The synthetic top level package is an internal graph-resolution artifact that depends on everything,
        # so we exclude it to keep the queries focused on real packages.
        self.Packages: list[Package] = [package for package in topLevelPackage.ResolvedBuildOrder if package.Type != PackageType.TopLevel]

        self.__byName: dict[str, Package] = {package.Name: package for package in self.Packages}

        # Derive the reverse of the direct-dependency edges once: name -> packages that directly depend on it.
        self.__directDependents: dict[str, list[Package]] = {package.Name: [] for package in self.Packages}
        for package in self.Packages:
            for dependency in package.ResolvedDirectDependencies:
                # A dependency can in theory point at a package outside our universe (it will not have an
                # entry here); we only track dependents that live in the node universe.
                dependentList = self.__directDependents.get(dependency.Name)
                if dependentList is not None:
                    dependentList.append(package)

    def TryResolve(self, name: str) -> Package | None:
        """Locate a package by its full name, falling back to a match on the source (template) name."""
        package = self.__byName.get(name)
        if package is not None:
            return package
        for candidate in self.Packages:
            if candidate.NameInfo.SourceName == name:
                return candidate
        return None

    def GetSuggestions(self, name: str, maxCount: int = 8) -> list[str]:
        """Return package names that contain the given (case-insensitive) substring, for 'did you mean' hints."""
        lowerName = name.lower()
        matches = sorted(package.Name for package in self.Packages if lowerName in package.Name.lower())
        return matches[:maxCount]

    def GetDependencies(self, package: Package, transitive: bool) -> list[GraphEdge]:
        """Forward edges: the packages 'package' depends on (direct, or the full closure when transitive)."""
        dependencyList = package.ResolvedAllDependencies if transitive else package.ResolvedDirectDependencies
        edges = [GraphEdge(dependency.Package, dependency.Access) for dependency in dependencyList]
        edges.sort(key=lambda edge: edge.Package.Name)
        return edges

    def GetDependents(self, package: Package, transitive: bool) -> list[GraphEdge]:
        """Reverse edges: the packages that depend on 'package' (direct, or the full closure when transitive)."""
        if not transitive:
            edges = [GraphEdge(dependent, self.__TryGetDirectAccess(dependent, package)) for dependent in self.__directDependents.get(package.Name, [])]
            edges.sort(key=lambda edge: edge.Package.Name)
            return edges

        # Transitive reverse closure via BFS over the direct-dependent edges. No single access type spans a
        # chain of edges, so Access is left as None here.
        visited: set[str] = set()
        pending = [package]
        result: list[Package] = []
        while len(pending) > 0:
            current = pending.pop()
            for dependent in self.__directDependents.get(current.Name, []):
                if dependent.Name not in visited:
                    visited.add(dependent.Name)
                    result.append(dependent)
                    pending.append(dependent)
        result.sort(key=lambda entry: entry.Name)
        return [GraphEdge(entry, None) for entry in result]

    def FindDependencyPaths(self, fromPackage: Package, toPackage: Package, log: Log | None = None) -> list[list[Package]]:
        """All simple forward dependency paths from 'fromPackage' to 'toPackage' (bounded by MAX_DEPENDENCY_PATHS)."""
        paths: list[list[Package]] = []
        capReached = [False]

        def visit(current: Package, trail: list[Package], onTrail: set[str]) -> None:
            if len(paths) >= MAX_DEPENDENCY_PATHS:
                capReached[0] = True
                return
            if current.Name == toPackage.Name:
                paths.append(list(trail))
                return
            for dependency in current.ResolvedDirectDependencies:
                nextPackage = dependency.Package
                if nextPackage.Name in onTrail:
                    continue
                onTrail.add(nextPackage.Name)
                trail.append(nextPackage)
                visit(nextPackage, trail, onTrail)
                trail.pop()
                onTrail.discard(nextPackage.Name)

        visit(fromPackage, [fromPackage], {fromPackage.Name})
        paths.sort(key=lambda path: [entry.Name for entry in path])
        if capReached[0] and log is not None:
            log.DoPrint(f"WARNING: More than {MAX_DEPENDENCY_PATHS} paths exist, only showing the first {MAX_DEPENDENCY_PATHS}")
        return paths

    def BuildAdjacency(self) -> list[tuple[Package, list[PackageDependency]]]:
        """The whole graph as an adjacency list: every package with its direct dependency edges (name sorted)."""
        adjacency = [(package, sorted(package.ResolvedDirectDependencies, key=lambda dependency: dependency.Name)) for package in self.Packages]
        adjacency.sort(key=lambda entry: entry[0].Name)
        return adjacency

    def __TryGetDirectAccess(self, dependent: Package, package: Package) -> AccessType | None:
        for dependency in dependent.ResolvedDirectDependencies:
            if dependency.Name == package.Name:
                return dependency.Access
        return None


# ---------------------------------------------------------------------------------------------------------------------------------------------------
# Console rendering (mirrors the style of Builder.Show* using log.DoPrint)
# ---------------------------------------------------------------------------------------------------------------------------------------------------


def __FormatEdge(edge: GraphEdge) -> str:
    typeString = PackageType.ToString(edge.Package.Type)
    if edge.Access is None:
        return f"  {edge.Package.Name} ({typeString})"
    return f"  {edge.Package.Name} ({typeString}, {AccessType.ToString(edge.Access)})"


def PrintDependencies(log: Log, package: Package, edges: list[GraphEdge], transitive: bool) -> None:
    scope = "all" if transitive else "direct"
    if len(edges) <= 0:
        log.DoPrint(f"{package.Name} has no {scope} dependencies")
        return
    log.DoPrint(f"{package.Name} depends on ({scope}, {len(edges)}):")
    for edge in edges:
        log.DoPrint(__FormatEdge(edge))


def PrintDependents(log: Log, package: Package, edges: list[GraphEdge], transitive: bool) -> None:
    scope = "all" if transitive else "direct"
    if len(edges) <= 0:
        log.DoPrint(f"Nothing depends on {package.Name} ({scope})")
        return
    log.DoPrint(f"Used by {package.Name} ({scope}, {len(edges)}):")
    for edge in edges:
        log.DoPrint(__FormatEdge(edge))


def PrintDependencyPaths(log: Log, fromPackage: Package, toPackage: Package, paths: list[list[Package]]) -> None:
    if len(paths) <= 0:
        log.DoPrint(f"No dependency path from {fromPackage.Name} to {toPackage.Name}")
        return
    log.DoPrint(f"Dependency paths from {fromPackage.Name} to {toPackage.Name} ({len(paths)}):")
    for path in paths:
        log.DoPrint("  " + " -> ".join(entry.Name for entry in path))


def PrintGraphDump(log: Log, adjacency: list[tuple[Package, list[PackageDependency]]]) -> None:
    log.DoPrint(f"Package graph ({len(adjacency)} packages):")
    for package, dependencies in adjacency:
        log.DoPrint(f"  {package.Name} ({PackageType.ToString(package.Type)})")
        for dependency in dependencies:
            log.DoPrint(f"    -> {dependency.Name} ({AccessType.ToString(dependency.Access)})")


# ---------------------------------------------------------------------------------------------------------------------------------------------------
# Machine-readable rendering (for --Format json; prints one JSON document to the log)
# ---------------------------------------------------------------------------------------------------------------------------------------------------


def __EdgeToDict(edge: GraphEdge) -> dict[str, Any]:
    entry: dict[str, Any] = {"Name": edge.Package.Name, "Type": PackageType.ToString(edge.Package.Type)}
    if edge.Access is not None:
        entry["Access"] = AccessType.ToString(edge.Access)
    return entry


def BuildDependenciesResult(package: Package, edges: list[GraphEdge], transitive: bool) -> dict[str, Any]:
    return {"Query": "DependsOn", "Package": package.Name, "Transitive": transitive, "Results": [__EdgeToDict(edge) for edge in edges]}


def BuildDependentsResult(package: Package, edges: list[GraphEdge], transitive: bool) -> dict[str, Any]:
    return {"Query": "UsedBy", "Package": package.Name, "Transitive": transitive, "Results": [__EdgeToDict(edge) for edge in edges]}


def BuildDependencyPathsResult(fromPackage: Package, toPackage: Package, paths: list[list[Package]]) -> dict[str, Any]:
    return {
        "Query": "DependencyPath",
        "From": fromPackage.Name,
        "To": toPackage.Name,
        "PathCount": len(paths),
        "Paths": [[entry.Name for entry in path] for path in paths],
    }


def BuildGraphDumpResult(adjacency: list[tuple[Package, list[PackageDependency]]]) -> dict[str, Any]:
    packages = {
        package.Name: {
            "Type": PackageType.ToString(package.Type),
            "DirectDependencies": [{"Name": dependency.Name, "Access": AccessType.ToString(dependency.Access)} for dependency in dependencies],
        }
        for package, dependencies in adjacency
    }
    return {"Query": "DumpGraph", "PackageCount": len(adjacency), "Packages": packages}


def BuildNotFoundResult(name: str, suggestions: list[str]) -> dict[str, Any]:
    return {"Query": "Error", "Package": name, "Error": "Package not found", "Suggestions": suggestions}


def ResultsToJsonText(results: list[dict[str, Any]]) -> str:
    """Render all query results as a single json document, so a consumer (e.g. an MCP) can parse it directly."""
    return json.dumps({"Results": results}, ensure_ascii=False, sort_keys=True, indent=2)
