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

# What the content directories of a package are called.
#
# A package keeps its content in 'Content' and the content that has to be built in 'Content.bld'. The tool config of a set of projects can
# give the first another name, as it gives the gen file its name:
#
#   <ContentBuilderConfiguration ContentDirectory="content"/>
#
# The directory of the content to build is that name with '.bld' ('content.bld'). The name is the one of the directory in a package, of a
# library and of an app alike. What an app is given to run with, its own content and what it inherits, is in the 'Content' directory of
# its build area whatever the name is: that is the name an app loads from.

from FslBuildGen import ToolSharedValues
from FslBuildGen.Exceptions import UsageErrorException

BuildPostfix = ".bld"
AttributeName = "ContentDirectory"

# The directories of a package the tool or its build systems have another use for
_g_reservedNames = frozenset({"source", "include", "build", "bin", "obj"})
_g_invalidCharacters = '/\\:*?"<>|'


class ContentDirectoryConfig:
    def __init__(self, name: str = ToolSharedValues.CONTENT_FOLDER_NAME) -> None:
        """name: the name of the content directory of a package, the default when the tool config names none"""
        super().__init__()
        self.Name = name
        self.BuildName = f"{name}{BuildPostfix}"

    @property
    def IsDefault(self) -> bool:
        return self.Name == ToolSharedValues.CONTENT_FOLDER_NAME

    @staticmethod
    def Parse(name: str | None, configFileName: str) -> ContentDirectoryConfig:
        """name: the value of 'ContentDirectory' in the tool config, None if it is not there"""
        if name is None:
            return ContentDirectoryConfig()
        where = f"'{AttributeName}' of the ContentBuilderConfiguration in '{configFileName}'"
        if len(name) <= 0 or name != name.strip() or name in (".", "..") or name.endswith(".") or any(ch in name for ch in _g_invalidCharacters):
            raise UsageErrorException(f"{where} is '{name}', which is not the name of a directory. It names the content directory of a package: 'Content'")
        if name.lower() in _g_reservedNames:
            raise UsageErrorException(f"{where} is '{name}', which is a directory of a package with another use")
        if name.lower().endswith(BuildPostfix):
            raise UsageErrorException(
                f"{where} is '{name}'. It names the content directory; the directory of the content to build is that name with '{BuildPostfix}'"
            )
        return ContentDirectoryConfig(name)
