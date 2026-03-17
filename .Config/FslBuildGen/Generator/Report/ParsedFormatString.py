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

from collections.abc import Callable

# from typing import Dict
# from typing import Tuple
# from typing import Union
# import itertools
# import os
from FslBuildGen import IOUtil, Util

# from FslBuildGen.Log import Log
from FslBuildGen.Generator.Report.VariableDict import VariableDict
from FslBuildGen.Generator.Report.VariableReport import VariableReport


class FormatStringUndefinedVariableNameException(Exception):
    def __init__(self, name: str) -> None:
        super().__init__(f"The variable '{name}' was undefined")


class FormatStringUndefinedEnvironmentVariableNameException(Exception):
    def __init__(self, name: str) -> None:
        super().__init__(f"The environment variable '{name}' was undefined")


class FormatStringInvalidVariableNameException(Exception):
    def __init__(self, name: str) -> None:
        super().__init__(f"The name '{name}' is not a valid C variable style name")


class FormatStringVariableMissingTerminationException(Exception):
    def __init__(self, message: str) -> None:
        super().__init__(message)


class FormatStringEnvironmentVariableMissingTerminationException(Exception):
    def __init__(self, message: str) -> None:
        super().__init__(message)


class FormatStringInvalidException(Exception):
    def __init__(self, message: str) -> None:
        super().__init__(message)


class LookupCommand:
    def __init__(self, name: str, formattedName: str, splitIndex: int) -> None:
        if not Util.IsValidCStyleName(name):
            raise FormatStringInvalidVariableNameException(name)

        self.Name = name
        self.FormattedName = formattedName
        # if this is negative then its a virtual entry
        self.SplitIndex = splitIndex


class LookupVariableCommand(LookupCommand):
    def __init__(self, name: str, report: VariableReport, splitIndex: int) -> None:
        super().__init__(name, f"${{{name}}}", splitIndex)
        self.Report = report


class LookupEnvironmentVariableCommand(LookupCommand):
    def __init__(self, name: str, value: str, splitIndex: int) -> None:
        super().__init__(name, f"$({name})", splitIndex)
        self.Value = value


class ParseState:
    Scanning = 1
    CommandStart = 2
    VariableBlock = 3
    EnvBlock = 4


FormatStringEnvironmentVariableResolver = Callable[[str], str]


class ParsedFormatString:
    def __init__(
        self,
        source: str,
        variableDict: VariableDict | None,
        environmentVariableResolver: FormatStringEnvironmentVariableResolver | None = None,
        noVariableResolve: bool = False,
        noEnvVariableResolve: bool = False,
    ) -> None:
        super().__init__()
        self.SplitList: list[str] = []
        self.VarCommandList: list[LookupVariableCommand] = []
        self.EnvCommandList: list[LookupEnvironmentVariableCommand] = []

        if source == "":
            self.SplitList.append("")
            return

        state = ParseState.Scanning
        startSplitIndex = 0
        blockStartIndex = 0
        for index, ch in enumerate(source):
            if state == ParseState.Scanning:
                if ch == "$":
                    state = ParseState.CommandStart
            elif state == ParseState.CommandStart:
                if ch == "{":
                    state = ParseState.VariableBlock
                    if startSplitIndex < index - 1:
                        self.SplitList.append(source[startSplitIndex : index - 1])
                    startSplitIndex = index - 1
                    blockStartIndex = index + 1
                elif ch == "(":
                    state = ParseState.EnvBlock
                    if startSplitIndex < index - 1:
                        self.SplitList.append(source[startSplitIndex : index - 1])
                    startSplitIndex = index - 1
                    blockStartIndex = index + 1
                    # stringEndSplitIndex = blockStartIndex-2
            elif state == ParseState.VariableBlock:
                if ch == "}":
                    state = ParseState.Scanning
                    self.SplitList.append(source[startSplitIndex : index + 1])
                    startSplitIndex = index + 1
                    # Do the report lookup
                    variableName = source[blockStartIndex:index]
                    if not Util.IsValidCStyleName(variableName):
                        raise FormatStringInvalidVariableNameException(variableName)

                    if noVariableResolve:
                        variableValue: VariableReport | None = VariableReport("*NotDefined*", ["*NotDefined*"], None)
                    else:
                        variableValue = variableDict.TryGetVariableReport(variableName) if variableDict is not None else None
                    if variableValue is None:
                        raise FormatStringUndefinedVariableNameException(variableName)

                    self.VarCommandList.append(LookupVariableCommand(variableName, variableValue, len(self.SplitList) - 1))
                    # stringEndSplitIndex = blockStartIndex
            elif state == ParseState.EnvBlock and ch == ")":
                state = ParseState.Scanning
                self.SplitList.append(source[startSplitIndex : index + 1])
                startSplitIndex = index + 1

                envName = source[blockStartIndex:index]
                if not Util.IsValidCStyleName(envName):
                    raise FormatStringInvalidVariableNameException(envName)

                if noEnvVariableResolve:
                    envValue: str | None = "*NotDefined*"
                elif environmentVariableResolver is None:
                    envValue = IOUtil.TryGetEnvironmentVariable(envName)
                else:
                    envValue = environmentVariableResolver(envName)

                if envValue is None:
                    raise FormatStringUndefinedEnvironmentVariableNameException(envName)

                self.EnvCommandList.append(LookupEnvironmentVariableCommand(envName, envValue, len(self.SplitList) - 1))

        if startSplitIndex < len(source):
            self.SplitList.append(source[startSplitIndex:])

        if state != ParseState.Scanning and state != ParseState.CommandStart:
            if state == ParseState.VariableBlock:
                raise FormatStringVariableMissingTerminationException(f"The string '{source}' is missing a terminating '}}' ")
            elif state == ParseState.EnvBlock:
                raise FormatStringEnvironmentVariableMissingTerminationException(f"The string '{source}' is missing a terminating ')' ")
            raise FormatStringInvalidException(f"The string '{source}' contained invalid format codes")
