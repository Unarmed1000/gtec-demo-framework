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
import xml.etree.ElementTree as ET

from FslBuildGen import IOUtil
from FslBuildGen.Generator import VSSolutionSlnx
from FslBuildGen.Log import Log
from FslBuildGen.VisualStudioSolutionFormat import VisualStudioSolutionFormat

# A generated solution is small, a bigger file is not one of ours
_MAX_SOLUTION_FILE_SIZE = 16 * 1024 * 1024
_SLN_HEADER = "Microsoft Visual Studio Solution File"


def TryRemoveStaleSolution(
    log: Log,
    packageDirectory: str,
    packageName: str,
    projectFileName: str,
    staleFormat: VisualStudioSolutionFormat,
    activeFormat: VisualStudioSolutionFormat,
    isDryRun: bool,
) -> bool:
    """Remove the package solution of the format that is no longer used ('<packageName>.<stale extension>' in the package directory).
    It is only removed if it is a regular file in the package directory that is a solution we generated for the package project
    ('projectFileName'), anything else is left alone with a warning. Returns true if the file was removed.
    """
    if not _IsPlainFileName(packageName):
        log.DoPrintWarning(f"Stale solution check skipped, the package name '{packageName}' is not a plain file name")
        return False

    filename = IOUtil.Join(packageDirectory, f"{packageName}.{VisualStudioSolutionFormat.GetFileExtension(staleFormat)}")
    if not os.path.lexists(filename):
        return False
    if not IOUtil.IsRegularFileDirectlyIn(filename, packageDirectory):
        log.DoPrintWarning(f"Stale solution '{filename}' was not removed, it is not a regular file in the package directory")
        return False
    if not _IsGeneratedSolution(filename, staleFormat, projectFileName):
        log.DoPrintWarning(f"Stale solution '{filename}' was not removed, it is not a solution generated for '{projectFileName}'")
        return False
    if isDryRun:
        log.LogPrint(f"Dry run, the stale solution '{filename}' would be removed")
        return False
    if not IOUtil.TrySafeRemoveFile(filename, packageDirectory):
        log.DoPrintWarning(f"Stale solution '{filename}' could not be removed")
        return False
    log.DoPrint(f"Removed stale solution '{filename}' (SolutionFormat is now '{VisualStudioSolutionFormat.ToString(activeFormat)}')")
    return True


def _IsPlainFileName(name: str) -> bool:
    return len(name) > 0 and name != "." and name != ".." and not any(character in name for character in "/\\:")


def _IsGeneratedSolution(filename: str, solutionFormat: VisualStudioSolutionFormat, projectFileName: str) -> bool:
    try:
        if os.path.getsize(filename) > _MAX_SOLUTION_FILE_SIZE:
            return False
        with open(filename, encoding="utf-8-sig", errors="replace") as file:
            content = file.read()
        if solutionFormat == VisualStudioSolutionFormat.Sln:
            # In .sln mode the generator owns and overwrites this file, so a .sln for the package project is ours
            return content.lstrip().startswith(_SLN_HEADER) and f'"{projectFileName}"' in content
        # A .slnx next to a generated .sln could be saved by Visual Studio, so it also has to carry our marker
        if VSSolutionSlnx.GENERATED_MARKER not in content:
            return False
        root = ET.fromstring(content)
        return root.tag == "Solution" and any(entry.get("Path") == projectFileName for entry in root.iter("Project"))
    except OSError, ET.ParseError:
        return False
