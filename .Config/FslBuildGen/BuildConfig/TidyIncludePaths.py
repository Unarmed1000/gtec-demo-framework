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

# The include directories of the clang-tidy command of a package ('FslBuildCheck --tidy'): which ones are passed as '-I', which ones as
# '-isystem', and in which order.
#
# The headers of a third-party library are system headers: clang reports nothing in them and the checks leave them alone. The code of
# the project is not. So:
# - '-I' is for the directories of the project: the include and the source directory of a package of the build that is not third-party.
#   Third-party is the gen file concept: a package of type 'ExternalLibrary'.
# - '-isystem' is for every other directory: the include directory of an external dependency or of a found package (cmake), the install
#   directory of a recipe, the include directory of an 'ExternalLibrary' package, and a directory only the compile command knows (it
#   belongs to no package of the project).
# - a directory the compile command passes as '-isystem' is one, whatever it belongs to. clang treats a directory that is given both ways
#   as a system directory, so a directory is listed once.
# The generated cmake project passes a third-party directory as a plain '-I', so the compile command alone does not tell them apart.
#
# The order inside each list is the one of the compile command cmake wrote for the source files of the package: when two directories
# hold a header of the same name, the first one is used, and it has to be the one the build uses. The compiler searches the '-I'
# directories before the '-isystem' ones, so the system list has the '-I' directories of the command first, then its '-isystem' ones.
# The directories the package itself resolves (its own include directory first, then the ones of its dependencies) come after the ones of
# the command, as far as the command does not have them. A package without a source file has no compile command: its directories keep
# the order of the package, and a directory of the project is a system directory when the compile command of another package passes it
# as one.
#
# Directories are compared the way the file system compares them (FslBuildGen/PathCompare.py), by their path: a directory of the project
# that reaches the command by another path (a link) is not recognized and is a system directory. The first spelling of a directory is the
# one that is used.

from collections.abc import Collection, Iterable, Sequence

from FslBuildGen import PathCompare
from FslBuildGen.DataTypes import PackageType
from FslBuildGen.Packages.Package import Package


def ToKey(directory: str) -> str:
    """What two spellings of the same directory have in common"""
    return PathCompare.ToComparable(directory)


def ToKeys(directories: Collection[str]) -> frozenset[str]:
    return frozenset(ToKey(directory) for directory in directories)


def IsThirdParty(package: Package) -> bool:
    """A package whose headers are not code of the project"""
    return package.Type == PackageType.ExternalLibrary


def GetProjectDirectories(packages: Iterable[Package]) -> list[str]:
    """The directories that hold code of the project: the include and the source directory of each package that is not third-party.
    The directory an external dependency of such a package names is not one of them.
    """
    result: list[str] = []
    for package in packages:
        if not IsThirdParty(package):
            if package.AbsoluteIncludePath is not None:
                result.append(package.AbsoluteIncludePath.Name)
            if package.AbsoluteSourcePath is not None:
                result.append(package.AbsoluteSourcePath)
    return result


def GetProjectDirectoriesOf(package: Package) -> list[str]:
    """The directories of the project a source file of the package can include from: the ones of the package and of every package it
    depends on, directly or not
    """
    buildOrder = package.ResolvedBuildOrder
    if all(entry is not package for entry in buildOrder):
        buildOrder = [*buildOrder, package]
    return GetProjectDirectories(buildOrder)


class CommandIncludeDirs:
    """The include directories of the compile commands of one package: each directory once, in the order the commands name them"""

    def __init__(self) -> None:
        super().__init__()
        self.Includes: list[str] = []
        self.SystemIncludes: list[str] = []
        self.__IncludeKeys: set[str] = set()
        self.__SystemIncludeKeys: set[str] = set()

    def AddCommand(self, includes: Sequence[str], systemIncludes: Sequence[str]) -> None:
        """Add the '-I' and the '-isystem' directories of the compile command of one source file, in the order of the command"""
        for directory in includes:
            key = ToKey(directory)
            if key not in self.__IncludeKeys:
                self.__IncludeKeys.add(key)
                self.Includes.append(directory)
        for directory in systemIncludes:
            key = ToKey(directory)
            if key not in self.__SystemIncludeKeys:
                self.__SystemIncludeKeys.add(key)
                self.SystemIncludes.append(directory)


class ArrangedIncludeDirs:
    def __init__(self, includes: list[str], systemIncludes: list[str]) -> None:
        super().__init__()
        # The directories to pass as '-I', in that order
        self.Includes = includes
        # The directories to pass as '-isystem', in that order
        self.SystemIncludes = systemIncludes


def Arrange(
    packageIncludeDirs: Sequence[str],
    commandIncludes: Sequence[str],
    commandSystemIncludes: Sequence[str],
    knownSystemIncludeKeys: Collection[str],
    projectDirectoryKeys: Collection[str],
) -> ArrangedIncludeDirs:
    """
    packageIncludeDirs: the include directories the package resolves, in its order.
    commandIncludes, commandSystemIncludes: the '-I' and '-isystem' directories of the compile commands of the package, in their order.
    knownSystemIncludeKeys: the keys (ToKey) of every directory a compile command of any package passes as '-isystem'.
    projectDirectoryKeys: the keys (ToKey) of the directories that hold code of the project (GetProjectDirectoriesOf).
    """
    commandSystemKeys = ToKeys(commandSystemIncludes)
    includes: list[str] = []
    systemIncludes: list[str] = []
    usedKeys: set[str] = set()

    # The '-I' directories of the command. One the command also passes as '-isystem' has its place among those.
    for directory in commandIncludes:
        key = ToKey(directory)
        if key not in usedKeys and key not in commandSystemKeys:
            usedKeys.add(key)
            if key in projectDirectoryKeys:
                includes.append(directory)
            else:
                systemIncludes.append(directory)
    for directory in commandSystemIncludes:
        key = ToKey(directory)
        if key not in usedKeys:
            usedKeys.add(key)
            systemIncludes.append(directory)
    # What only the package resolves
    for directory in packageIncludeDirs:
        key = ToKey(directory)
        if key not in usedKeys:
            usedKeys.add(key)
            if key in projectDirectoryKeys and key not in knownSystemIncludeKeys:
                includes.append(directory)
            else:
                systemIncludes.append(directory)
    return ArrangedIncludeDirs(includes, systemIncludes)
