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

# The content directories of a package are 'Content' and 'Content.bld', spelled like that.
#
# A file system that ignores case finds a directory named 'content' when it is asked for 'Content', one that tells them apart does not: the
# same package would have content on one machine and none on the other. So a directory whose name is one of the two in another case is
# refused, on every platform. Whoever made it meant content, leaving it out without a word would be the surprise.

import os

from FslBuildGen import IOUtil
from FslBuildGen.ContentDirectoryConfig import AttributeName, ContentDirectoryConfig
from FslBuildGen.Exceptions import UsageErrorException


def FindMisspelledDirectories(packagePath: str, contentDirectory: ContentDirectoryConfig | None = None) -> list[tuple[str, str]]:
    """The directories of the package whose name is a content directory name in another case, as (the name it has, the name it is to have).
    contentDirectory: what the content directories are called, None for the default names
    """
    contentDirectory = contentDirectory if contentDirectory is not None else ContentDirectoryConfig()
    namesById = {contentDirectory.Name.lower(): contentDirectory.Name, contentDirectory.BuildName.lower(): contentDirectory.BuildName}
    try:
        names = os.listdir(packagePath)
    except FileNotFoundError:
        return []
    result: list[tuple[str, str]] = []
    for name in sorted(names):
        expectedName = namesById.get(name.lower())
        if expectedName is not None and name != expectedName and os.path.isdir(os.path.join(packagePath, name)):
            result.append((name, expectedName))
    return result


def CheckSpelling(packageName: str, packagePath: str, contentDirectory: ContentDirectoryConfig | None = None) -> None:
    for name, expectedName in FindMisspelledDirectories(packagePath, contentDirectory):
        # The default names are the ones of the tool. Another name is the choice of the tool config, which is where to change it
        isDefault = contentDirectory is None or contentDirectory.IsDefault
        how = (
            "rename it"
            if isDefault
            else f"rename it, or change the name the tool config gives the content directory ('{AttributeName}' of its ContentBuilderConfiguration)"
        )
        raise UsageErrorException(
            f"The package '{packageName}' has a directory named '{name}' ('{IOUtil.Join(packagePath, name)}'). The content directory of a package "
            f"is spelled '{expectedName}': {how}. A name that only differs in case is refused, because it is found on a file system that "
            "ignores case and not on one that does not"
        )
