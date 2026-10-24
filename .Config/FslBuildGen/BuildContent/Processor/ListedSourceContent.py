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

# What the content builder builds and copies, given as lists.
#
# SourceContent finds the files of one package: it reads the command file of its 'Content.bld' and looks into the directory. The content of
# an app also holds the files of the libraries it uses, which the resolver has found and filtered. This is the same three collections for
# the content builder, made from such lists without reading a directory again.

from FslBuildGen.BuildContent.ContentProfile import ResolvedContentProfile
from FslBuildGen.BuildContent.PathRecord import PathRecord
from FslBuildGen.BuildContent.Sync.Content import Content
from FslBuildGen.Log import Log


class ListedBuildStep:
    def __init__(
        self, outputRelativePath: str, builderName: str | None, toolArguments: list[str] | None, profile: ResolvedContentProfile | None = None
    ) -> None:
        """One output of a file that is built.
        outputRelativePath: the output, below the content directory
        builderName: the content builder a build rule names for it, None for the builder of the extension of the file
        toolArguments: the parameters the rule gives the builder, None for its own
        profile: what the file is, as its owner says (ContentProfileList)
        """
        super().__init__()
        self.OutputRelativePath = outputRelativePath
        self.BuilderName = builderName
        self.ToolArguments = toolArguments
        self.Profile = profile


class ListedSourceContent:
    def __init__(
        self,
        log: Log,
        contentPath: str,
        contentSourcePath: str,
        syncFiles: list[PathRecord],
        buildFiles: list[PathRecord],
        buildSteps: dict[str, list[ListedBuildStep]] | None = None,
    ) -> None:
        """syncFiles: the files that are copied as they are
        buildFiles: the files a content builder makes an output of, each once
        buildSteps: the outputs of each of the buildFiles, by the path of the file. A file that is not in it is built by the builder of
                    its extension into the output that builder names
        """
        super().__init__()
        self.BuildSteps = buildSteps if buildSteps is not None else {}
        self.ContentSource = Content(log, contentPath, False, syncFiles)
        self.ContentBuildSource = Content(log, contentSourcePath, False, buildFiles)
        self.AllContentSource = Content(log, contentSourcePath, False, buildFiles + syncFiles)
        self.IsEmpty = len(syncFiles) <= 0 and len(buildFiles) <= 0
