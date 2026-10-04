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

# 'FslBuild --KeepGoing': the arguments that make a native build tool carry on with the targets that do not depend on one that failed.
# The generators add them to their build command when GeneratorConfig.KeepGoing is set.

from FslBuildGen.BuildExternal.CMakeTypes import CMakeGeneratorName


def GetCMakeNativeArguments(cmakeGeneratorName: str) -> list[str]:
    """The arguments for the native build tool of a cmake generator, they are placed after the '--' of 'cmake --build'.
    - Ninja stops after the first failed job unless it is told how many failures to accept, 0 is 'as many as there are'.
    - make stops at the first target that fails unless it is told to keep going.
    - MSBuild (the Visual Studio generators) builds every project that does not depend on a failed one without being asked.
    Nothing is added for a generator that is not known.
    """
    if cmakeGeneratorName == CMakeGeneratorName.Ninja:
        return ["-k", "0"]
    if cmakeGeneratorName == CMakeGeneratorName.UnixMakeFile:
        return ["-k"]
    return []


def GetMakeArguments() -> list[str]:
    """The arguments for the make of the legacy makefile generators"""
    return ["-k"]


def GetGradleArguments() -> list[str]:
    """The arguments for the gradle build of an Android package: gradle carries on with the tasks that do not depend on a failed one"""
    return ["--continue"]
