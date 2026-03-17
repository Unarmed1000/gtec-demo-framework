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

# from typing import Callable
import itertools
import os

# from FslBuildGen import IOUtil
# from FslBuildGen import Util
# from FslBuildGen.Log import Log
from FslBuildGen.ExternalVariantConstraints import ExternalVariantConstraints
from FslBuildGen.Generator.Report.Datatypes import FormatStringEnvironmentVariableResolveMethod
from FslBuildGen.Generator.Report.GeneratorVariableReport import GeneratorVariableReport, InvalidVariableOptionNameException
from FslBuildGen.Generator.Report.ParsedFormatString import (
    FormatStringEnvironmentVariableResolver,
    FormatStringUndefinedVariableNameException,
    LookupEnvironmentVariableCommand,
    LookupVariableCommand,
    ParsedFormatString,
)


def GetLinkedCommandList(
    linkedVariables: list[LookupVariableCommand], sourceDict: dict[str, LookupVariableCommand]
) -> dict[LookupVariableCommand, list[LookupVariableCommand]]:
    linkedCommandDict: dict[LookupVariableCommand, list[LookupVariableCommand]] = {}
    for entry in linkedVariables:
        if entry.Report.LinkTargetName is None:
            raise Exception("entry.Report.LinkTargetName can not be None")
        # we assume the master is present in the sourceDict!
        master = sourceDict[entry.Report.LinkTargetName]
        if master in linkedCommandDict:
            linkedCommandDict[master].append(entry)
        else:
            linkedCommandDict[master] = [entry]
    return linkedCommandDict


def CreateLookupDict(
    varCommandList: list[LookupVariableCommand], generatorVariableReport: GeneratorVariableReport
) -> tuple[dict[str, LookupVariableCommand], list[LookupVariableCommand]]:
    """Returns
    - Dict -> which is a lookup dict that maps variable names to their lookup command
    - List -> a list of all LookupVariableCommand that are linked to something
    """
    resultDict: dict[str, LookupVariableCommand] = {}
    linkedCommands: list[LookupVariableCommand] = []
    for varCommand in varCommandList:
        resultDict[varCommand.Name] = varCommand
        if varCommand.Report.LinkTargetName is not None:
            linkedCommands.append(varCommand)

    # Check if all linked commands have a 'command' to chain to, if not create a virtual one
    for varCommand in linkedCommands:
        linkedName = varCommand.Report.LinkTargetName
        if linkedName is None:
            raise Exception("entry.Report.LinkTargetName can not be None")
        if linkedName not in resultDict:
            variableValue = generatorVariableReport.TryGetVariableReport(linkedName)
            if variableValue is None:
                raise FormatStringUndefinedVariableNameException(linkedName)
            resultDict[linkedName] = LookupVariableCommand(linkedName, variableValue, -1)
    return (resultDict, linkedCommands)


def RecursiveReplace(
    rFormatList: list[str], command: LookupVariableCommand, commandOptionIndex: int, linkedCommandDict: dict[LookupVariableCommand, list[LookupVariableCommand]]
) -> None:
    if command.SplitIndex >= 0:
        rFormatList[command.SplitIndex] = command.Report.Options[commandOptionIndex]
    if command in linkedCommandDict:
        for linkedCommand in linkedCommandDict[command]:
            RecursiveReplace(rFormatList, linkedCommand, commandOptionIndex, linkedCommandDict)


def GetFormattedString(
    rFormatList: list[str],
    variableList: list[LookupVariableCommand],
    variableOptionIndices: list[int] | tuple[int, ...],
    envCommandList: list[LookupEnvironmentVariableCommand],
    linkedCommandDict: dict[LookupVariableCommand, list[LookupVariableCommand]],
) -> str:
    for envCommand in envCommandList:
        rFormatList[envCommand.SplitIndex] = envCommand.Value

    for index, command in enumerate(variableList):
        RecursiveReplace(rFormatList, command, variableOptionIndices[index], linkedCommandDict)

    return "".join(rFormatList)


def TryGetEnvironmentVariableResolveMethod(
    environmentVariableResolveMethod: FormatStringEnvironmentVariableResolveMethod,
) -> FormatStringEnvironmentVariableResolver | None:
    if environmentVariableResolveMethod == FormatStringEnvironmentVariableResolveMethod.Lookup:
        return None
    elif environmentVariableResolveMethod == FormatStringEnvironmentVariableResolveMethod.OSShellEnvironmentVariable:
        if os.name == "posix":
            return lambda s: f"$({s})"
        elif os.name == "nt":
            return lambda s: f"%{s}%"
        else:
            raise Exception(f"Unsupported os: {os.name}")
    else:
        raise Exception(f"Unknown environment variable resolve method: {environmentVariableResolveMethod}")


class ReportVariableFormatter:
    @staticmethod
    def Format2(
        strFormat: str,
        generatorVariableReport: GeneratorVariableReport,
        userVariantSettingDict: dict[str, str],
        environmentVariableResolveMethod: FormatStringEnvironmentVariableResolveMethod = FormatStringEnvironmentVariableResolveMethod.Lookup,
    ) -> str:
        environmentVariableResolver = TryGetEnvironmentVariableResolveMethod(environmentVariableResolveMethod)
        parsedFormatString = ParsedFormatString(strFormat, generatorVariableReport, environmentVariableResolver)
        sourceVariableDict, linkedVariables = CreateLookupDict(parsedFormatString.VarCommandList, generatorVariableReport)
        linkedCommandDict = GetLinkedCommandList(linkedVariables, sourceVariableDict)

        variableList: list[LookupVariableCommand] = []
        variableOptionIndices: list[int] = []
        # the ordering of the sourceVariableDict.values is pretty random, but it doesnt matter
        for command in sourceVariableDict.values():
            if command.Report.LinkTargetName is None:
                variableList.append(command)
                if command.Name in userVariantSettingDict:
                    userDefinedOptionName = userVariantSettingDict[command.Name]
                    if userDefinedOptionName not in command.Report.Options:
                        raise InvalidVariableOptionNameException(command.Name, userDefinedOptionName, str(command.Report.Options))
                    variableOptionIndex = command.Report.Options.index(userDefinedOptionName)
                else:
                    defaultOptionIndex = generatorVariableReport.TryGetDefaultOptionIndex(command.Name)
                    variableOptionIndex = 0 if defaultOptionIndex is None else defaultOptionIndex
                variableOptionIndices.append(variableOptionIndex)
            elif command.Name in userVariantSettingDict:
                raise Exception(f"A linked variable '{command.Name}' was found in the user feature list, this indicates a internal error")

        # Substitute the (env) variables with their values
        scratchpadFormatList: list[str] = parsedFormatString.SplitList
        return GetFormattedString(scratchpadFormatList, variableList, variableOptionIndices, parsedFormatString.EnvCommandList, linkedCommandDict)

    @staticmethod
    def Format(
        strFormat: str,
        generatorVariableReport: GeneratorVariableReport,
        externalVariantConstraints: ExternalVariantConstraints,
        environmentVariableResolveMethod: FormatStringEnvironmentVariableResolveMethod = FormatStringEnvironmentVariableResolveMethod.Lookup,
    ) -> str:
        environmentVariableResolver = TryGetEnvironmentVariableResolveMethod(environmentVariableResolveMethod)
        parsedFormatString = ParsedFormatString(strFormat, generatorVariableReport, environmentVariableResolver)
        sourceVariableDict, linkedVariables = CreateLookupDict(parsedFormatString.VarCommandList, generatorVariableReport)
        linkedCommandDict = GetLinkedCommandList(linkedVariables, sourceVariableDict)

        variableList: list[LookupVariableCommand] = []
        variableOptionIndices: list[int] = []
        # the ordering of the sourceVariableDict.values is pretty random, but it doesnt matter
        for command in sourceVariableDict.values():
            if command.Report.LinkTargetName is None:
                variableList.append(command)
                userDefinedOptionName = externalVariantConstraints.TryGetOptionStringByNameString(command.Name)
                if userDefinedOptionName is not None:
                    if userDefinedOptionName not in command.Report.Options:
                        raise InvalidVariableOptionNameException(command.Name, userDefinedOptionName, str(command.Report.Options))
                    variableOptionIndex = command.Report.Options.index(userDefinedOptionName)
                else:
                    defaultOptionIndex = generatorVariableReport.TryGetDefaultOptionIndex(command.Name)
                    variableOptionIndex = 0 if defaultOptionIndex is None else defaultOptionIndex
                variableOptionIndices.append(variableOptionIndex)
            elif externalVariantConstraints.TryGetOptionStringByNameString(command.Name) is not None:
                raise Exception(f"A linked variable '{command.Name}' was found in the user feature list, this indicates a internal error")

        # Substitute the (env) variables with their values
        scratchpadFormatList: list[str] = parsedFormatString.SplitList
        return GetFormattedString(scratchpadFormatList, variableList, variableOptionIndices, parsedFormatString.EnvCommandList, linkedCommandDict)

    @staticmethod
    def GetAllKnownCombinations(strFormat: str, generatorVariableReport: GeneratorVariableReport) -> list[str]:
        parsedFormatString = ParsedFormatString(strFormat, generatorVariableReport)

        sourceVariableDict, linkedVariables = CreateLookupDict(parsedFormatString.VarCommandList, generatorVariableReport)
        linkedCommandDict = GetLinkedCommandList(linkedVariables, sourceVariableDict)

        variableList: list[LookupVariableCommand] = []
        optionList: list[list[int]] = []
        # the ordering of the sourceVariableDict.values is pretty random, but it doesnt matter
        for command in sourceVariableDict.values():
            if command.Report.LinkTargetName is None:
                variableList.append(command)
                optionList.append(list(range(0, len(command.Report.Options))))

        # since we dont need the parsed format string object after this we can just reuse its list
        resultList: list[str] = []
        scratchpadFormatList: list[str] = parsedFormatString.SplitList
        cartesianProduct: list[tuple[int, ...]] = list(itertools.product(*optionList))
        # newStr = strFormat
        for entry in cartesianProduct:
            result = GetFormattedString(scratchpadFormatList, variableList, entry, parsedFormatString.EnvCommandList, linkedCommandDict)
            resultList.append(result)
        return resultList
