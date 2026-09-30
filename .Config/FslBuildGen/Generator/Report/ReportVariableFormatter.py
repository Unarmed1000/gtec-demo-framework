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

import itertools
import os
from collections.abc import Callable, Sequence

from FslBuildGen.ExternalVariantConstraints import ExternalVariantConstraints
from FslBuildGen.Generator.Report.Datatypes import FormatStringEnvironmentVariableResolveMethod
from FslBuildGen.Generator.Report.GeneratorVariableReport import GeneratorVariableReport, InvalidVariableOptionNameException
from FslBuildGen.Generator.Report.ParsedFormatString import (
    FormatStringEnvironmentVariableResolver,
    FormatStringUndefinedVariableNameException,
    LookupVariableCommand,
    ParsedFormatString,
)
from FslBuildGen.Generator.Report.VariableDict import VariableDict
from FslBuildGen.Generator.Report.VariableReport import VariableReport

# Returns the option name the user selected for a variable or None if the user did not select one
_UserOptionLookup = Callable[[str], str | None]


def _FindMaster(variable: VariableReport, variableDict: VariableDict, rChainVariables: dict[str, VariableReport]) -> VariableReport:
    """Follow the link chain of the variable to its master (the first variable in the chain that is not linked).
    Every variable on the chain (the variable, the linked variables passed on the way and the master) is recorded in rChainVariables.
    """
    visitedNames: set[str] = set()
    current = variable
    while current.LinkTargetName is not None:
        # GeneratorVariableReport.Add prevents link cycles, this is just a guard against a endless loop
        if current.Name in visitedNames:
            raise Exception(f"The variable '{variable.Name}' has a circular link chain")
        visitedNames.add(current.Name)
        rChainVariables.setdefault(current.Name, current)
        linkTarget = variableDict.TryGetVariableReport(current.LinkTargetName)
        if linkTarget is None:
            raise FormatStringUndefinedVariableNameException(current.LinkTargetName)
        current = linkTarget
    rChainVariables.setdefault(current.Name, current)
    return current


class _FormatPlan:
    """The variable references of a parsed format string resolved to the masters that select their option.
    A linked variable has a option list that is paired by index with the option list of its master,
    so every reference uses the option of its own variable at the index selected for its master.
    """

    def __init__(self, parsedFormatString: ParsedFormatString, variableDict: VariableDict) -> None:
        self.__SplitList = parsedFormatString.SplitList
        self.__EnvCommandList = parsedFormatString.EnvCommandList
        self.__MasterIndexByName: dict[str, int] = {}
        self.Masters: list[VariableReport] = []
        self.References: list[tuple[LookupVariableCommand, int]] = []

        # The referenced variables in first reference order
        referencedVariables: dict[str, VariableReport] = {}
        for command in parsedFormatString.VarCommandList:
            referencedVariables.setdefault(command.Name, command.Report)

        # The master order decides the order of GetAllKnownCombinations
        chainVariables: dict[str, VariableReport] = {}
        # Pass 1: the referenced variables that are not linked
        for variable in referencedVariables.values():
            if variable.LinkTargetName is None:
                self.__AddMaster(variable)
        # Pass 2: the masters of the linked variables whose link target is not referenced
        for variable in referencedVariables.values():
            if variable.LinkTargetName is not None and variable.LinkTargetName not in referencedVariables:
                self.__AddMaster(_FindMaster(variable, variableDict, chainVariables))
        for command in parsedFormatString.VarCommandList:
            masterIndex = self.__AddMaster(_FindMaster(command.Report, variableDict, chainVariables))
            self.References.append((command, masterIndex))

        # Every variable involved: the referenced ones in first reference order followed by the ones only reached through a link
        self.Variables: list[VariableReport] = list(referencedVariables.values())
        self.Variables += [entry for entry in chainVariables.values() if entry.Name not in referencedVariables]

    def __AddMaster(self, master: VariableReport) -> int:
        masterIndex = self.__MasterIndexByName.get(master.Name)
        if masterIndex is None:
            masterIndex = len(self.Masters)
            self.__MasterIndexByName[master.Name] = masterIndex
            self.Masters.append(master)
        return masterIndex

    def Substitute(self, masterOptionIndices: Sequence[int]) -> str:
        """Substitute the (env) variables with their values, masterOptionIndices contains the selected option index for each entry in Masters"""
        formatList = list(self.__SplitList)
        for envCommand in self.__EnvCommandList:
            formatList[envCommand.SplitIndex] = envCommand.Value
        for command, masterIndex in self.References:
            formatList[command.SplitIndex] = command.Report.Options[masterOptionIndices[masterIndex]]
        return "".join(formatList)


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


def _Format(
    strFormat: str,
    generatorVariableReport: GeneratorVariableReport,
    tryGetUserOption: _UserOptionLookup,
    environmentVariableResolveMethod: FormatStringEnvironmentVariableResolveMethod,
) -> str:
    environmentVariableResolver = TryGetEnvironmentVariableResolveMethod(environmentVariableResolveMethod)
    parsedFormatString = ParsedFormatString(strFormat, generatorVariableReport, environmentVariableResolver)
    plan = _FormatPlan(parsedFormatString, generatorVariableReport)

    # The variables are checked in first reference order, so the first problem in the string is the one reported
    optionIndexByName: dict[str, int] = {}
    for variable in plan.Variables:
        userDefinedOptionName = tryGetUserOption(variable.Name)
        if variable.LinkTargetName is not None:
            if userDefinedOptionName is not None:
                raise Exception(f"A linked variable '{variable.Name}' was found in the user feature list, this indicates a internal error")
        elif userDefinedOptionName is not None:
            if userDefinedOptionName not in variable.Options:
                raise InvalidVariableOptionNameException(variable.Name, userDefinedOptionName, str(variable.Options))
            optionIndexByName[variable.Name] = variable.Options.index(userDefinedOptionName)
        else:
            defaultOptionIndex = generatorVariableReport.TryGetDefaultOptionIndex(variable.Name)
            optionIndexByName[variable.Name] = 0 if defaultOptionIndex is None else defaultOptionIndex
    return plan.Substitute([optionIndexByName[master.Name] for master in plan.Masters])


class ReportVariableFormatter:
    @staticmethod
    def Format2(
        strFormat: str,
        generatorVariableReport: GeneratorVariableReport,
        userVariantSettingDict: dict[str, str],
        environmentVariableResolveMethod: FormatStringEnvironmentVariableResolveMethod = FormatStringEnvironmentVariableResolveMethod.Lookup,
    ) -> str:
        return _Format(strFormat, generatorVariableReport, userVariantSettingDict.get, environmentVariableResolveMethod)

    @staticmethod
    def Format(
        strFormat: str,
        generatorVariableReport: GeneratorVariableReport,
        externalVariantConstraints: ExternalVariantConstraints,
        environmentVariableResolveMethod: FormatStringEnvironmentVariableResolveMethod = FormatStringEnvironmentVariableResolveMethod.Lookup,
    ) -> str:
        return _Format(strFormat, generatorVariableReport, externalVariantConstraints.TryGetOptionStringByNameString, environmentVariableResolveMethod)

    @staticmethod
    def GetAllKnownCombinations(strFormat: str, generatorVariableReport: GeneratorVariableReport) -> list[str]:
        parsedFormatString = ParsedFormatString(strFormat, generatorVariableReport)
        plan = _FormatPlan(parsedFormatString, generatorVariableReport)
        optionIndexRanges = [range(len(master.Options)) for master in plan.Masters]
        return [plan.Substitute(masterOptionIndices) for masterOptionIndices in itertools.product(*optionIndexRanges)]
