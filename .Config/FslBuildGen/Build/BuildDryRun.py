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

# What a dry run of a build says ('FslBuild --DryRun'): the commands it would run. A dry run starts no process and writes, creates and
# deletes nothing, so each step that would do so prints one of these lines instead. They are printed at every verbosity.

# The summary of a dry run with '--KeepGoing': nothing was built, so no package is counted as built or as failed
KeepGoingSummary = "Build summary: dry run, nothing was built"
# '-c open2' writes the Visual Studio Code files of the project and starts Visual Studio Code
OpenProject = "Dry run: would configure and launch Visual Studio Code"


def FormatConfigCommand(command: str, currentWorkingDirectory: str, isForced: bool) -> str:
    """The configure step of the build. Unless it is forced a build only runs it when the build configuration changed, which a dry run can
    not tell: the generated files it would compare were not written.
    """
    text = f"Dry run: would run build config command '{command}' in '{currentWorkingDirectory}'"
    return text if isForced else f"{text} if the build configuration changed"


def FormatBuildCommand(command: str, currentWorkingDirectory: str) -> str:
    return f"Dry run: would run build command '{command}' in '{currentWorkingDirectory}'"


def FormatRunCommand(command: str, currentWorkingDirectory: str) -> str:
    """A '--ForAll' or '--ForAllExe' command"""
    return f"Dry run: would run command '{command}' in '{currentWorkingDirectory}'"


def FormatUnknownRunCommand(packageName: str) -> str:
    """A '--ForAll' or '--ForAllExe' command of a package whose paths the configure step of the build writes down: without a configured
    build they are not known
    """
    return f"Dry run: would run the command for package '{packageName}', its paths are not known before the build is configured"


def FormatRecipeBuild(packageName: str) -> str:
    """The recipe of an external package that is not installed"""
    return f"Dry run: would build the recipe of package '{packageName}'"
