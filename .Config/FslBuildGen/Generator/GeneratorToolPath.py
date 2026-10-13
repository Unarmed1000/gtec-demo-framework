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

# Where the tool is, for a project that runs one of its scripts as a build step (FslBuildContent before a build that is started in the IDE).
#
# A generated project is not to depend on which copy of the tool is first on the PATH of whoever builds it: several copies can be installed,
# and the one that generated the project is the one that knows what it generated. So the generator gives the project the directory of the
# tool that runs it ('##FSLBUILD_TOOL_PATH##').
#
# A project file can be committed and is then the same file for everyone. So the directory is written below a root directory of the
# project when it is in one, by the variable of that root ('$(MB_BUILD)\.build\FslBuild'), which is a path of the same form on every
# machine that has the variable. A tool outside every root directory is written with its full path.

import os

import FslBuildGen
from FslBuildGen import IOUtil
from FslBuildGen.ToolConfig import ToolConfig

Token = "##FSLBUILD_TOOL_PATH##"


def GetToolDirectory() -> str:
    """The directory of the tool that is running: the one its scripts are in ('FslBuildContent.py')"""
    return IOUtil.NormalizePath(os.path.dirname(os.path.dirname(os.path.abspath(FslBuildGen.__file__))))


def GetVisualStudioToolPath(toolConfig: ToolConfig, toolDirectory: str | None = None) -> str:
    """The directory of the tool as a Visual Studio project writes it.
    toolDirectory: the directory of the tool, None for the one that is running
    """
    toolDirectory = IOUtil.NormalizePath(toolDirectory) if toolDirectory is not None else GetToolDirectory()
    rootedPath = toolConfig.TryToPath(toolDirectory)
    path = rootedPath if rootedPath is not None else toolDirectory
    return path.rstrip("/").replace("/", "\\")
