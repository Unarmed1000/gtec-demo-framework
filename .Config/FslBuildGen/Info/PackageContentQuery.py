#!/usr/bin/env python3
# ****************************************************************************************************************************************************
# * BSD 3-Clause License
# *
# * Copyright (c) 2026, Mana Battery ApS
# * All rights reserved.
# *
# * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
# *
# * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
# * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the
# *    documentation and/or other materials provided with the distribution.
# * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this
# *    software without specific prior written permission.
# *
# * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
# * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
# * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# ****************************************************************************************************************************************************

# The 'FslBuildInfo --Content <package>' query: the content of an app, each file with the package it comes from.
# The answer has the shape of the dependency graph queries (Info/PackageGraphQuery.py), so one json document holds them all.

from typing import Any

from FslBuildGen.DataTypes import PackageType
from FslBuildGen.Log import Log
from FslBuildGen.Packages.Package import Package

QueryName = "Content"
_g_notAnApp = "Only an executable has content of its own at run time: the content of a library is content of the apps that use it"


def IsApp(package: Package) -> bool:
    return package.Type == PackageType.Executable


def PrintContent(log: Log, package: Package) -> None:
    if not IsApp(package):
        log.DoPrint(f"'{package.Name}' is not an executable. {_g_notAnApp}")
        return
    log.DoPrint(f"Content of '{package.Name}' ({len(package.ResolvedAppContent)} files):")
    for entry in package.ResolvedAppContent:
        log.DoPrint(f"  {entry.AppPath} ({entry.Kind.value}, {entry.OwnerName}) <- {entry.SourcePath}")


def BuildContentResult(package: Package) -> dict[str, Any]:
    if not IsApp(package):
        return {"Query": "Error", "Package": package.Name, "Error": f"Package is not an executable. {_g_notAnApp}"}
    return {
        "Query": QueryName,
        "Package": package.Name,
        "Results": [
            {"Path": entry.AppPath, "Kind": entry.Kind.value, "Owner": entry.OwnerName, "Source": entry.SourcePath} for entry in package.ResolvedAppContent
        ],
    }
