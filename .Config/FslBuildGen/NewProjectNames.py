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

# The names FslBuildNew works with.
#
# - What the name of a new package may be: what a gen file can name a package.
# - The name of the package the template sanity check creates for a template. It comes from the name of the template, which is the name of a
#   directory and can hold what a package name can not ('GLES2-UI', 'My Template', 'Lib.Old').
# - The package names a new package can not have: the names of the packages that are there, but not the one of the package in the directory
#   the new package is written to. That package is the one '--AllowOverwrite' writes again.

import os
import re
from collections.abc import Iterable, Sequence

from FslBuildGen import Util
from FslBuildGen.Packages.Package import Package

# What a package name does not hold between two of its letters and digits, apart from one '_' ('__' is reserved for the tool)
_g_notLettersOrDigits = re.compile(r"[^A-Za-z0-9]+")


# What a package name may be, for the message about a name that is none
PackageNameRules = (
    "a name is made of letters (a-z, A-Z), digits and '_', it starts with a letter, ends with a letter or a digit and has no '__'. "
    "A '.' separates the parts of a name, each part follows these rules"
)


def IsValidPackageName(name: str) -> bool:
    """True if a gen file can name a package like this: it is what the loader accepts as the name of a package (XmlBase2._ValidateName)"""
    return Util.IsValidUnresolvedPackageName(name)


def ToPackageNamePart(text: str) -> str:
    """The letters and digits of the text, with one '_' in the place of everything that was between them. A package name ends with a letter
    or a digit and does not hold '__', so the result can follow a '_' in a name. It is empty for a text without a letter or a digit.
    """
    return _g_notLettersOrDigits.sub("_", text).strip("_")


def CreateSanityCheckProjectNames(prefix: str, templateNames: Sequence[str]) -> dict[str, str]:
    """The name of the package the sanity check creates for each template: '<prefix>_<template name>' made a valid package name.
    Each template gets a name of its own: a name that is taken (compared without case, the name is a directory too) gets '_2', '_3' and so on.
    A template whose name is fine as it is keeps it, the others are named in the order they are given.
    """
    result: dict[str, str] = {}
    used: set[str] = set()
    # The names that are fine as they are come first so no other template can take them
    for templateName in templateNames:
        name = f"{prefix}_{templateName}"
        if len(templateName) > 0 and ToPackageNamePart(templateName) == templateName and name.lower() not in used:
            result[templateName] = name
            used.add(name.lower())
    for templateName in templateNames:
        if templateName not in result:
            namePart = ToPackageNamePart(templateName)
            baseName = prefix if len(namePart) <= 0 else f"{prefix}_{namePart}"
            name = baseName
            count = 2
            while name.lower() in used:
                name = f"{baseName}_{count}"
                count += 1
            result[templateName] = name
            used.add(name.lower())
    return result


def IsSameDirectory(path1: str, path2: str) -> bool:
    """True if the two paths name the same directory the way the file system compares names (on Windows 'e:/Work' is 'E:\\work')"""
    return os.path.normcase(os.path.normpath(path1)) == os.path.normcase(os.path.normpath(path2))


def GetReservedPackageNames(packages: Iterable[Package], newPackagePath: str) -> set[str]:
    """The names a new package in the directory newPackagePath can not have: the names of the packages that are somewhere else"""
    return {package.Name for package in packages if package.AbsolutePath is None or not IsSameDirectory(package.AbsolutePath, newPackagePath)}
