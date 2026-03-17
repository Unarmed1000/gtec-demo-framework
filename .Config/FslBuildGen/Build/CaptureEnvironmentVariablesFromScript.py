#!/usr/bin/env python3

# ****************************************************************************************************************************************************
# Copyright 2018 NXP
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

import json
import subprocess

from FslBuildGen import IOUtil
from FslBuildGen.Build.CaptureEnvironmentBlock import CaptureEnvironmentBlock
from FslBuildGen.DataTypes import BuildPlatformType
from FslBuildGen.Log import Log
from FslBuildGen.PlatformUtil import PlatformUtil


class LocalUtil:
    @staticmethod
    def TryRun(log: Log, cmdList: list[str]) -> str | None:
        """
        Run the command and capture the output
        :return: the captured output on sucess, None if it failed
        """
        try:
            if cmdList[0].endswith(".py") and PlatformUtil.DetectBuildPlatformType() == BuildPlatformType.Windows:
                cmdList[0] = cmdList[0][:-3] + ".bat"

            with subprocess.Popen(cmdList, stderr=subprocess.STDOUT, stdout=subprocess.PIPE, universal_newlines=True) as proc:
                output = proc.stdout.read().strip() if proc.stdout is not None else ""
                if proc.stdout is not None:
                    proc.stdout.close()
                result = proc.wait()
                if result != 0:
                    LocalUtil.DumpCapture(log, 4, output)
                    log.LogPrintWarning("The command '{}' failed with '{}'".format(" ".join(cmdList), result))
                    return None
                if isinstance(output, str):
                    return output
                return None
        except FileNotFoundError:
            log.DoPrintWarning("The command '{}' failed with 'file not found'.".format(" ".join(cmdList)))
            return None

    @staticmethod
    def ExtractJson(content: str) -> str:
        startPos = content.find(CaptureEnvironmentBlock.Begin)
        endPos = content.find(CaptureEnvironmentBlock.End)
        if startPos < 0 or endPos < 0 or endPos < startPos:
            raise Exception("Captured content is not of the expecte format")

        return content[(startPos + len(CaptureEnvironmentBlock.Begin)) : endPos].strip()

    @staticmethod
    def DumpCapture(log: Log, verbosity: int, content: str) -> None:
        if log.Verbosity < verbosity:
            return
        log.LogPrint("***Dump capture begin***")
        log.LogPrint(content)
        log.LogPrint("***Dump capture end***")


class CaptureEnvironmentVariablesFromScript:
    @staticmethod
    def Capture(log: Log, runCommand: list[str], pythonScriptRoot: str, envNameList: list[str]) -> dict[str, str]:
        """
        param: runCommand: the command we should run.
        param: pythonScriptRoot: The root location of the FslBuild.py tools (this must be a absolute path)
                                 This will be joined with the FslBuildDump.py name to get the absolute path to the python script.
        """
        if not IOUtil.IsAbsolutePath(pythonScriptRoot):
            raise Exception(f"pythonScriptRoot '{pythonScriptRoot}' is not absolute")

        # Validate that the input is in the correct format and create a initial dict
        entryDict: dict[str, str] = {}
        for envEntry in envNameList:
            if not (envEntry.startswith("$(") and envEntry.endswith(")")):
                raise Exception(f"Environment variable not in the correct $(NAME) format {envEntry}")
            strippedEntry = envEntry[2:-1]
            entryDict[envEntry] = strippedEntry

        log.LogPrintVerbose(4, f"Trying to capture environment variables: {envNameList}")

        # call a custom python script with the stripped argument list using the 'generator' run script
        # the scripts main role is to dump the listed environment variables to screen in a 'json dictionary' format that can be easily captured and
        # decoded.

        # Call generator run script (captured)
        # 'use the "FslBuildDumpEnv --Env [ENV1,ENV2]" script to generate a json dump for the env variables
        strEnv = "[{}]".format(",".join(entryDict.values()))
        commandName = "FslBuildDumpEnv.py"
        absCommandName = IOUtil.Join(pythonScriptRoot, commandName)
        cmdList = [absCommandName, "--AllowNotFound", "--Enclose", "--Env", strEnv]
        runCommand += cmdList
        content = LocalUtil.TryRun(log, runCommand)
        if content is None:
            raise Exception(f"Failed to capture virtual variant environment variables {envNameList}")

        # Parse captured json output
        strJson = LocalUtil.ExtractJson(content)

        # Decode json
        jsonDict = json.loads(strJson)

        finalDict: dict[str, str] = {}
        for key, value in jsonDict.items():
            if not isinstance(key, str) or not isinstance(value, str):
                LocalUtil.DumpCapture(log, 4, content)
                raise Exception("captured json decode failed")
            if key not in jsonDict:
                LocalUtil.DumpCapture(log, 4, content)
                raise Exception(f"Capture contained wrong key: '{key}")
            finalDict[key] = value

        # Ensure that all the requested entries are present
        if len(finalDict) != len(entryDict):
            missingKeys = []
            for key in entryDict:
                if key not in finalDict:
                    missingKeys.append(key)
            LocalUtil.DumpCapture(log, 4, content)
            raise Exception(f"The Captured environment variable output is missing for: {missingKeys}")

        log.LogPrintVerbose(4, f"Captured environment variables: {finalDict}")

        return finalDict

        # Disable
        #   BuildVariantUtil.CreateCompleteVariantSettings
        #   BuildVariantUtil.CreateCompleteVirtualVariantSettings
        #   As they would require us to have a 'run' for the top level package which we simply dont have (and none of the generators can handle it atm).
        #   To get that one we would to figure out were to save the file, the next problem is that it would change depending on whats being build.
        #   So instead we either
        #   A. use a 'environment variable cache' that is updated while we process the packages
        #   B. score each package depending on how many of the needed environment varibles it gives us
        #   C. Tweak the generators to be able to generate a 'run' for the top level file
        #
