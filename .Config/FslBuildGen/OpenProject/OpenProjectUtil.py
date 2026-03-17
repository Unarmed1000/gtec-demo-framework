#!/usr/bin/env python3
# ****************************************************************************************************************************************************
# Copyright 2020, 2024 NXP
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#    * Redistributions of source code must retain the above copyright notice,
#      this list of conditions and the following disclaimer.
#
#    * Redistributions in binary form must reproduce the above copyright notice,
#      this list of conditions and the following disclaimer in the documentation
#      and/or other materials provided with the distribution.
#
#    * Neither the name of the NXP. nor the names of
#      its contributors may be used to endorse or promote products derived from
#      this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# ****************************************************************************************************************************************************

import subprocess

from FslBuildGen import IOUtil
from FslBuildGen.DataTypes import BuildPlatformType
from FslBuildGen.Exceptions import ExitException
from FslBuildGen.Location.ResolvedPath import ResolvedPath
from FslBuildGen.Log import Log
from FslBuildGen.OpenProject.NatvisCombiner import NatvisCombiner
from FslBuildGen.OpenProject.OpenProjectCreateInfo import OpenProjectCreateInfo
from FslBuildGen.OpenProject.VSCodeLaunchJsonUtil import VSCodeLaunchJsonUtil
from FslBuildGen.OpenProject.VSCodeSettingsJsonUtil import VSCodeSettingsJsonUtil
from FslBuildGen.PlatformUtil import PlatformUtil


class OpenProjectUtil:
    @staticmethod
    def Run(log: Log, createInfo: OpenProjectCreateInfo, allNatvisFiles: list[ResolvedPath]) -> None:
        log.LogPrintVerbose(1, f"Configuring and launching Visual Studio Code for path '{createInfo.SourcePath}'")

        buildPlatformType = PlatformUtil.DetectBuildPlatformType()

        vsCodeConfigPath = IOUtil.Join(createInfo.SourcePath, ".vscode")
        launchFilePath = IOUtil.Join(vsCodeConfigPath, "launch.json")
        settingsFilePath = IOUtil.Join(vsCodeConfigPath, "settings.json")
        combinedNatvisFile = IOUtil.Join(vsCodeConfigPath, "CustomCombined.natvis")

        IOUtil.SafeMakeDirs(vsCodeConfigPath)

        # Combine all natvis files into one
        NatvisCombiner.Combine(log, allNatvisFiles, combinedNatvisFile)

        log.LogPrintVerbose(1, f"- Patching settings at '{settingsFilePath}'")
        log.PushIndent()
        try:
            VSCodeSettingsJsonUtil.Patch(log, settingsFilePath, createInfo.CMakeInfo)
        finally:
            log.PopIndent()

        exeInfo = createInfo.ExeInfo
        if exeInfo is not None:
            if log.Verbosity >= 1:
                log.LogPrint(f"- Patching launch settings at '{launchFilePath}'")
                log.LogPrint(f"  - Exe: '{exeInfo.Executable}'")
                log.LogPrint(f"  - Cwd: '{exeInfo.CurrentWorkingDirectory}'")
            if not VSCodeLaunchJsonUtil.TryPatch(launchFilePath, buildPlatformType, exeInfo.Executable, exeInfo.CurrentWorkingDirectory, combinedNatvisFile):
                log.LogPrintVerbose(1, f"WARNING Failed to patch launch file '{launchFilePath}'")
        else:
            log.LogPrintVerbose(1, "- Launch: No executable information found")

        log.PushIndent()
        try:
            OpenProjectUtil.__RunVSCode(log, buildPlatformType, createInfo.SourcePath, createInfo.OpenCommandArgs)
        finally:
            log.PopIndent()

    @staticmethod
    def __RunVSCode(log: Log, buildPlatformType: BuildPlatformType, sourcePath: str, openCommandArgs: list[str]) -> None:
        try:
            if log.Verbosity >= 1:
                log.LogPrint(f"Opening visual studio code in '{sourcePath}'")

            codeCmd = OpenProjectUtil.__GetCodeCmd(buildPlatformType)
            vsCodeCommand = [codeCmd, "."]
            if len(openCommandArgs) > 0:
                vsCodeCommand += openCommandArgs
            if log.Verbosity >= 4:
                log.LogPrint(f"Running vs code with the arguments {vsCodeCommand}")
            result = subprocess.call(vsCodeCommand, cwd=sourcePath)
            if result != 0:
                log.LogPrintWarning(
                    f"The open vscode command '{OpenProjectUtil.__SafeJoinCommandArguments(vsCodeCommand)}' failed with '{result}'. It was run with CWD: '{sourcePath}'"
                )
                raise ExitException(result)
        except FileNotFoundError:
            log.DoPrintWarning(
                f"The open vscode command '{OpenProjectUtil.__SafeJoinCommandArguments(vsCodeCommand)}' failed with 'file not found'. It was run with CWD: '{sourcePath}'"
            )
            raise

    @staticmethod
    def __GetCodeCmd(buildPlatformType: BuildPlatformType) -> str:
        if buildPlatformType == BuildPlatformType.Windows:
            return "code.cmd"
        return "code"

    @staticmethod
    def __SafeJoinCommandArguments(strings: list[str]) -> str:
        res = []
        for entry in strings:
            if " " in entry:
                entry = f'"{entry}"'
            res.append(entry)
        return " ".join(res)
