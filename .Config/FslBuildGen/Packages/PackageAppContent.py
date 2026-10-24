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

# One file of the content of an app: its own, or one it inherits from a package it uses.

from enum import Enum

from FslBuildGen.BuildContent import ContentProfile
from FslBuildGen.BuildContent.ContentProfile import ContentType, ResolvedContentProfile


class PackageAppContentKind(Enum):
    # A file of a 'Content' directory, used as it is
    Content = "Content"
    # The output a content builder makes from a file of a 'Content.bld' directory
    Built = "Built"
    # A file a command of the command file copies in from somewhere else
    Synced = "Synced"


class PackageAppContentEntry:
    def __init__(
        self,
        ownerName: str,
        sourceRootPath: str,
        sourcePath: str,
        appPath: str,
        kind: PackageAppContentKind,
        builderName: str | None = None,
        builderArguments: list[str] | None = None,
        profile: ResolvedContentProfile | None = None,
    ) -> None:
        """ownerName: the package the file belongs to
        sourceRootPath: the directory the file was found from: a content directory of its owner, or where a command copies from
        sourcePath: the absolute path of the file it comes from (for a built file: of what it is built from)
        appPath: where it is in the content directory of the app, with '/' and in the case its owner gave it
        builderName: for a built file a build rule is for: the content builder the rule names (None: the builder of its extension)
        builderArguments: for such a file: the parameters the rule gives the builder (None: its own)
        profile: what the file it comes from is (ContentProfileList), None for the default of its content type. A built file can be of another
                 content type than its source ('.png' into '.ktx'): the profile is the one of the source, for the builder
        """
        super().__init__()
        self.OwnerName = ownerName
        self.SourceRootPath = sourceRootPath
        self.SourcePath = sourcePath
        self.AppPath = appPath
        self.Kind = kind
        self.BuilderName = builderName
        self.BuilderArguments = builderArguments
        # What the file is in the app, by the extension of its path there
        self.ContentType: ContentType = ContentProfile.GetContentType(appPath)
        self.Profile = profile if profile is not None else ContentProfile.GetDefault(ContentProfile.GetContentType(sourcePath))
