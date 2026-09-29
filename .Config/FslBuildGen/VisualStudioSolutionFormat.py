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

from enum import Enum


class VisualStudioSolutionFormat(Enum):
    """The file format of the Visual Studio solutions the non CMake Visual Studio generator writes"""

    # The classic text format
    Sln = 0
    # The xml format, requires Visual Studio 2026
    Slnx = 1

    @staticmethod
    def ToString(value: VisualStudioSolutionFormat) -> str:
        if value == VisualStudioSolutionFormat.Sln:
            return "sln"
        elif value == VisualStudioSolutionFormat.Slnx:
            return "slnx"
        raise Exception(f"Unknown VisualStudioSolutionFormat: {value}")

    @staticmethod
    def TryFromString(value: str) -> VisualStudioSolutionFormat | None:
        if value == "sln":
            return VisualStudioSolutionFormat.Sln
        elif value == "slnx":
            return VisualStudioSolutionFormat.Slnx
        return None

    @staticmethod
    def GetAllNames() -> list[str]:
        return [VisualStudioSolutionFormat.ToString(entry) for entry in VisualStudioSolutionFormat]

    @staticmethod
    def GetFileExtension(value: VisualStudioSolutionFormat) -> str:
        # The format names are the file extensions
        return VisualStudioSolutionFormat.ToString(value)
