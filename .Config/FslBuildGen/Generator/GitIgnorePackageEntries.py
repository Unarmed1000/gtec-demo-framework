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

# The entries of the .gitignore of a package: what the template of the package type and the active generator ask to be ignored.
#
# The generators name what they write into a package directory after the package: 'Demo.App.vcxproj'. A package that has a flavor is
# built as an instance of it, and the name of an instance is '<package name>___<the selected options>': 'Demo.App___X11.vcxproj'. The
# .gitignore of a package is a tracked file, so it must not depend on which flavor was built. For a package that is built as a flavor
# instance the entries are therefore made with the package name, and with one pattern that stands for every instance of the package:
# 'Demo.App___*' in place of the name. A package without flavors gets the entries it always got.
#
# A package name only holds letters, digits, '_' and '.', none of which has a meaning in a .gitignore pattern, so a name is used as it is.
#
# The versions before 3.14.62 wrote the entries of the instance that was built, so the file of a package with flavors holds lines like
# '/Demo.App___X11.vcxproj' for every flavor that was ever built. Such a line is dropped when a pattern the tool asks for covers it
# (TryRemoveCoveredInstanceLines): it ignores nothing the pattern does not.

import re
from collections.abc import Iterable, Mapping

from FslBuildGen import Util
from FslBuildGen.Generator import GitIgnoreMerge

# What the template holds in place of the name of the project of the package
ProjectNamePlaceholder = "##PROJECT_NAME##"
# What separates the name of a package from the flavor options in the name of a flavor instance
FlavorSeparator = "___"
# Stands for every flavor instance of a package when it is put after the package name
AnyFlavorPattern = FlavorSeparator + "*"

# A line that names one file or directory of the package directory and nothing else: anchored, one path segment, no white space and
# none of the characters that mean something in a pattern
_g_plainPathLine = re.compile(r"/[A-Za-z0-9_.+-]+")
# A pattern for the flavor instances of a package as the tool makes it: like a plain path, with the pattern for every instance in it
_g_instancePatternLine = re.compile(r"/[A-Za-z0-9_.+-]*___\*[A-Za-z0-9_.+*-]*")


# Every function takes the name a package is built with (Package.Name): the name of the package, or the name of a flavor instance of it


def IsFlavorInstance(packageName: str) -> bool:
    return len(Util.GetPackageSourceAndFlavorNames(packageName)[1]) > 0


def GetProjectNames(packageName: str) -> list[str]:
    """The names the template is filled with: the name of the package, and for a package that is built as a flavor instance the pattern
    that stands for every instance of it as well. Never the name of the one instance.
    """
    sourceName, flavorName = Util.GetPackageSourceAndFlavorNames(packageName)
    if len(flavorName) <= 0:
        return [packageName]
    return [sourceName, sourceName + AnyFlavorPattern]


def GetTemplateLines(template: str, packageName: str) -> list[str]:
    """The lines of the template of the package type for the package: the template filled with each of its project names in turn"""
    lines: list[str] = []
    for projectName in GetProjectNames(packageName):
        lines += GitIgnoreMerge.SplitLines(template.replace(ProjectNamePlaceholder, projectName)) or []
    return lines


def GetGeneratorEntries(packageName: str, generatorIgnoreDict: Mapping[str, set[str]]) -> set[str]:
    """What the active generator writes into the directory of the package (GeneratorBase.GetPackageGitIgnoreDict holds it by the name the
    package is built with). For a flavor instance the options in each entry are replaced by the pattern for every instance.
    """
    entries = generatorIgnoreDict.get(packageName, set())
    flavorName = Util.GetPackageSourceAndFlavorNames(packageName)[1]
    if len(flavorName) <= 0:
        return entries
    return {entry.replace(FlavorSeparator + flavorName, AnyFlavorPattern) for entry in entries}


def _ToRegex(patternLine: str) -> re.Pattern[str]:
    """What the pattern matches the way git matches it: every character stands for itself, '*' for any characters but a '/'"""
    return re.compile("".join("[^/]*" if character == "*" else re.escape(character) for character in patternLine))


def TryRemoveCoveredInstanceLines(lines: list[str], requiredEntries: Iterable[str]) -> list[str] | None:
    """The lines of a .gitignore without the lines that name a flavor instance and that a pattern of the file covers.
    - lines: the lines of the file as it is about to be written (GitIgnoreMerge.MergeGitIgnoreLines), or as it is.
    - requiredEntries: the entries the tool asks for in this run (GitIgnoreMerge.GetRequiredEntries).
    A line is removed when all of this holds:
    - a pattern for the instances of the package ('/Demo.App___*.vcxproj') is one of the required entries and a line of the file,
    - the line is a plain path of the package directory ('/Demo.App___X11.vcxproj': anchored, one segment, no white space and no
      character with a meaning in a pattern) and the pattern matches all of it, so the line ignores nothing the pattern does not,
    - the line is not a required entry itself (the next run would add it again),
    - no line of the file is a negation: with one the order of the lines decides what is ignored.
    Every other line stays where it is. Returns None when nothing is removed.
    """
    required = set(requiredEntries)
    present = set(lines)
    patterns = [_ToRegex(entry) for entry in sorted(required) if entry in present and _g_instancePatternLine.fullmatch(entry) is not None]
    if len(patterns) <= 0 or any(line.lstrip().startswith("!") for line in lines):
        return None

    def IsCovered(line: str) -> bool:
        if line in required or _g_plainPathLine.fullmatch(line) is None:
            return False
        return any(pattern.fullmatch(line) is not None for pattern in patterns)

    result = [line for line in lines if not IsCovered(line)]
    return result if len(result) != len(lines) else None
