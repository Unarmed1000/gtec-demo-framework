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

# What a generator adds to the build files of an app for the content it inherits from the libraries it uses.
#
# A generator already lists the app's own content: the files of its 'Content' directory that are copied, and what the content builder makes
# from its 'Content.bld' and its command file. The inherited content comes on top, in the same two groups, each file with the path it has in
# the content directory of the app (PackageAppContentResolver).

from FslBuildGen import IOUtil, ToolSharedValues
from FslBuildGen.Packages.Package import Package
from FslBuildGen.Packages.PackageAppContent import PackageAppContentEntry, PackageAppContentKind


def GetInheritedCopyFiles(package: Package) -> list[PackageAppContentEntry]:
    """The inherited files that are copied as they are, sorted by their path in the app"""
    return [entry for entry in package.ResolvedAppContent if entry.OwnerName != package.Name and entry.Kind == PackageAppContentKind.Content]


def GetInheritedBuilderFiles(package: Package) -> list[PackageAppContentEntry]:
    """The inherited files the content builder delivers: what it builds and what a command of the owner copies in, sorted by their path in the app"""
    return [entry for entry in package.ResolvedAppContent if entry.OwnerName != package.Name and entry.Kind != PackageAppContentKind.Content]


def ToContentRelativePath(entry: PackageAppContentEntry) -> str:
    """Where the file is, relative to the directory that holds the content directory of the app ('Content/Textures/Atlas.png')"""
    return IOUtil.Join(ToolSharedValues.CONTENT_FOLDER_NAME, entry.AppPath)


def HasContent(package: Package) -> bool:
    """True if the app has any content, its own or inherited"""
    return len(package.ResolvedAppContent) > 0
