#!/usr/bin/env python3

# ****************************************************************************************************************************************************
# Copyright 2017 NXP
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

import os

# from typing import Any
from typing import cast

from FslBuildGen import IOUtil
from FslBuildGen.DataTypes import MagicStrings
from FslBuildGen.Exceptions import (
    CombinedEnvironmentVariableAndPathException,
    CombinedVariableAndPathException,
    EnvironmentVariableInMiddleOfStringException,
    IncompleteEnvironmentVariableFoundException,
    IncompleteVariableFoundException,
    VariableInMiddleOfStringException,
    VariableNotDefinedException,
)
from FslBuildGen.Generator.Report.ParsedFormatString import ParsedFormatString
from FslBuildGen.Generator.Report.StringVariableDict import StringVariableDict
from FslBuildGen.Log import Log
from FslBuildGen.Vars.VariableEnvironment import VariableEnvironment


class VariableProcessor:
    def __init__(self, log: Log, variableEnvironment: VariableEnvironment | None = None) -> None:
        super().__init__()
        self.__Variables = VariableEnvironment(log) if variableEnvironment is None else variableEnvironment
        self.__StringVariableDict = StringVariableDict()
        for key, value in self.__Variables.Dict.items():
            self.__StringVariableDict.Add(key, value)

    def TrySplitLeadingEnvironmentVariablesNameAndPath(self, entry: str, tag: object | None = None) -> tuple[str, str] | tuple[None, None]:
        if entry.startswith("$("):
            endIndex = entry.find(")")
            if endIndex < 0:
                raise IncompleteEnvironmentVariableFoundException(entry, tag)
            env = entry[2:endIndex]
            rest = entry[endIndex + 1 :] if (endIndex + 1) <= len(entry) else ""
            if rest.startswith("/") or rest.startswith("\\"):
                rest = rest[1:]
            return env, rest
        elif entry.find("$(") >= 0:
            raise EnvironmentVariableInMiddleOfStringException(entry, tag)
        return None, None

    def TryExtractLeadingEnvironmentVariableNameAndPath(
        self, entry: str, allowCombinedPath: bool, tag: object | None = None
    ) -> tuple[str, str] | tuple[None, None]:
        if entry.startswith("$("):
            endIndex = entry.find(")")
            if endIndex < 0:
                raise IncompleteEnvironmentVariableFoundException(entry, tag)
            elif not allowCombinedPath and endIndex != (len(entry) - 1):
                raise CombinedEnvironmentVariableAndPathException(entry, tag)
            return entry[2:endIndex], entry[endIndex + 1 :]
        elif entry.find("$(") >= 0:
            raise EnvironmentVariableInMiddleOfStringException(entry, tag)
        return None, None

    def TryExtractLeadingEnvironmentVariableName(self, entry: str, allowCombinedPath: bool, tag: object | None = None) -> str | None:
        if entry.startswith("$("):
            endIndex = entry.find(")")
            if endIndex < 0:
                raise IncompleteEnvironmentVariableFoundException(entry, tag)
            elif not allowCombinedPath and endIndex != (len(entry) - 1):
                raise CombinedEnvironmentVariableAndPathException(entry, tag)
            return entry[2:endIndex]
        elif entry.find("$(") >= 0:
            raise EnvironmentVariableInMiddleOfStringException(entry, tag)
        return None

    def __TryExtractLeadingVariableName(self, entry: str, allowCombinedPath: bool, tag: object | None = None) -> str | None:
        if entry.startswith("${"):
            endIndex = entry.find("}")
            if endIndex < 0:
                raise IncompleteVariableFoundException(entry, tag)
            elif not allowCombinedPath and endIndex != (len(entry) - 1):
                raise CombinedVariableAndPathException(entry, tag)
            return entry[2:endIndex]
        elif entry.find("$(") >= 0:
            raise VariableInMiddleOfStringException(entry, tag)
        return None

    def ResolveAbsolutePathWithLeadingEnvironmentVariablePathAsDir(self, pathName: str, tag: object | None = None, checkExists: bool = True) -> str:
        pathName = self.ResolveAbsolutePathWithLeadingEnvironmentVariablePath(pathName, tag)
        if checkExists and not os.path.isdir(pathName):
            raise OSError(f"'{pathName}' path is not a valid directory")
        return pathName

    def ResolveAbsolutePathWithLeadingEnvironmentVariablePathAsFile(self, pathName: str, tag: object | None = None) -> str:
        pathName = self.ResolveAbsolutePathWithLeadingEnvironmentVariablePath(pathName, tag)
        if not os.path.isfile(pathName):
            raise OSError(f"'{pathName}' path is not a valid file")
        return pathName

    def ResolveAbsolutePathWithLeadingEnvironmentVariablePath(self, pathName: str, tag: object | None = None) -> str:
        """Resolve a absolute path
        It can start with a environment variable $()
        It can start with a variable ${}
        """
        pathName = self.__DoBasicPathResolve(pathName, pathName, tag)
        if not os.path.isabs(pathName):
            raise OSError(f"'{pathName}' path is not absolute")
        return pathName

    def ResolvePathToAbsolute(self, pathName: str, tag: object | None = None) -> str:
        """Resolve a path to a absolute one
        The path can be relative to the ${PROJECT_ROOT} or
        It can start with a environment variable $()
        It can start with a variable ${}
        """

        resultPath: str = self.__DoBasicPathResolve(pathName, pathName, tag)
        if not os.path.isabs(resultPath):
            absPath = IOUtil.Join(MagicStrings.ProjectRoot, pathName)
            resultPath = self.__DoBasicPathResolve(absPath, absPath, tag)

        if not os.path.isabs(resultPath):
            raise OSError(f"'{pathName}' could not be made absolute '{resultPath}")
        return resultPath

    def ResolveFilenameWithVariables(self, pathName: str) -> str:
        parsedStr = ParsedFormatString(pathName, self.__StringVariableDict)
        for var1 in parsedStr.VarCommandList:
            parsedStr.SplitList[var1.SplitIndex] = var1.Report.Options[0]
        for var2 in parsedStr.EnvCommandList:
            parsedStr.SplitList[var2.SplitIndex] = var2.Value
        return "".join(parsedStr.SplitList)

    def __DoBasicPathResolve(self, pathName: str, defaultResult: str, tag: object | None = None) -> str:
        """Resolve a path
        It can start with a environment variable $() or
        It can start with a variable ${}
        """
        result = None
        environmentName = self.TryExtractLeadingEnvironmentVariableName(pathName, True, tag)
        if environmentName is not None:
            path = IOUtil.GetEnvironmentVariableForDirectory(environmentName)
            result = pathName.replace(f"$({environmentName})", path)
        else:
            variableName = self.__TryExtractLeadingVariableName(pathName, True, tag)
            if variableName is not None:
                if variableName not in self.__Variables.Dict:
                    raise VariableNotDefinedException(variableName, cast(dict[str, object | None], self.__Variables.Dict))
                strReplace = f"${{{variableName}}}"
                result = pathName.replace(strReplace, self.__Variables.Dict[variableName])

        return IOUtil.NormalizePath(result if result is not None else defaultResult)
