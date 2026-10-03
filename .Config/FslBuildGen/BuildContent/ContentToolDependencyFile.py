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


import os
import subprocess
from collections.abc import Callable
from typing import final

from FslBuildGen.Log import Log

# The content tools that can write a make style dependency file listing the files a content file includes, by tool name (lower case, no '.exe').
# A build system only knows the files that are listed in Content.bld, so the tool itself has to say which other files (like shader includes) the
# output depends on. Older glslang releases lack '--depfile'.
_DEPENDENCY_FILE_OPTION_BY_TOOL_NAME = {
    "glslangvalidator": "--depfile",
    "glslang": "--depfile",
}


def GetToolName(toolCommand: str) -> str:
    """The tool name used to look up how a content tool lists its dependencies ('C:/Bin/glslangValidator.exe' -> 'glslangvalidator')"""
    name = os.path.basename(toolCommand.replace("\\", "/")).lower()
    return name[: -len(".exe")] if name.endswith(".exe") else name


def _RunHelp(toolCommand: str) -> str:
    """The '--help' output of a tool, empty if it could not be run (a missing tool is reported when the content is built)"""
    try:
        result = subprocess.run([toolCommand, "--help"], capture_output=True, text=True, errors="replace", timeout=60, check=False)
    except OSError, subprocess.SubprocessError:
        return ""
    return result.stdout + result.stderr


@final
class ContentToolDependencyFile:
    """Asks the content tools that support it to write a make style dependency file while they build a content file.
    Whether a tool supports it is checked once per tool, by looking for the option in its '--help' output.
    """

    def __init__(self, runHelp: Callable[[str], str] = _RunHelp) -> None:
        super().__init__()
        self.__RunHelp = runHelp
        self.__IsSupportedByToolCommand: dict[str, bool] = {}

    def TryGetArguments(self, log: Log, toolCommand: str, dependencyFileName: str) -> list[str] | None:
        """The extra tool arguments that write the dependency file, None if the tool can not write one"""
        option = _DEPENDENCY_FILE_OPTION_BY_TOOL_NAME.get(GetToolName(toolCommand))
        if option is None or not self.__IsSupported(log, toolCommand, option):
            return None
        return [option, dependencyFileName]

    def __IsSupported(self, log: Log, toolCommand: str, option: str) -> bool:
        isSupported = self.__IsSupportedByToolCommand.get(toolCommand)
        if isSupported is None:
            isSupported = option in self.__RunHelp(toolCommand)
            self.__IsSupportedByToolCommand[toolCommand] = isSupported
            if not isSupported:
                log.DoPrintWarning(
                    f"'{toolCommand}' can not list the files a content file includes (it has no '{option}' option), so content files that "
                    "include other files are built every time. A newer version of the tool fixes this."
                )
        return isSupported


# Shared by all content processors, so each tool is checked once
g_contentToolDependencyFile = ContentToolDependencyFile()
