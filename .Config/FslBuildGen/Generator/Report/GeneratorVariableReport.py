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

# from typing import Iterable


from FslBuildGen.Generator.Report.VariableDict import VariableDict
from FslBuildGen.Generator.Report.VariableReport import VariableReport

# from typing import Set
# from FslBuildGen.DataTypes import BuildVariantConfig
from FslBuildGen.Log import Log
from FslBuildGen.SharedGeneration import ToolAddedVariant, ToolAddedVariantOptions


class VariableOptionListCanNotBeEmptyException(Exception):
    def __init__(self, name: str) -> None:
        super().__init__(f"The variable '{name}' can have a empty option list")


class VariableAlreadyDefinedException(Exception):
    def __init__(self, name: str) -> None:
        super().__init__(f"The variable '{name}' has already been defined")


class LinkToSelfNotAllowedException(Exception):
    def __init__(self, name: str) -> None:
        super().__init__(f"The variable '{name}' can not be linked to itself")


class LinkedVariableNotFoundException(Exception):
    def __init__(self, name: str) -> None:
        super().__init__(f"The linked variable '{name}' could not be found")


class VariableDoesNotContainTheSameAmountOfOptionsAsTheLinkedTargetException(Exception):
    def __init__(self, name: str, options: str, linkTargetName: str, linkOptions: str) -> None:
        super().__init__(
            f"Variable '{name}' options {options} does not contain the same amount of entries as the link target '{linkTargetName}' options: {linkOptions}"
        )


class VariableNotFoundException(Exception):
    def __init__(self, name: str) -> None:
        super().__init__(f"The variable '{name}' could not be found")


class LinkedVariableCanNotHaveDefaultValueException(Exception):
    def __init__(self, name: str, nameLinkTarget: str) -> None:
        super().__init__(f"The variable '{name}' is linked to '{nameLinkTarget}' and can therefore not have a default value")


class InvalidVariableOptionException(Exception):
    def __init__(self, message: str) -> None:
        super().__init__(message)


class InvalidVariableOptionNameException(InvalidVariableOptionException):
    def __init__(self, variableName: str, variableOption: str, variableReportOptions: str) -> None:
        message = f"The variable option '{variableOption}' is not valid for option '{variableName}' expected {variableReportOptions}"
        super().__init__(message)


class InvalidVariableOptionIndexException(InvalidVariableOptionException):
    def __init__(self, variableName: str, variableOptionIndex: int, variableReportOptions: str, lenVariableReportOptions: int) -> None:
        message = "The variable option index '{0}' is not valid for option '{1}' expected a option from {2} in the range 0 to {3}"
        super().__init__(message.format(variableOptionIndex, variableName, variableReportOptions, lenVariableReportOptions))


# class CircularLinkNotAllowedException(Exception):
#    def __init__(self, nameFrom: str, nameTo: str, linkDesc: str) -> None:
#        super().__init__("Linking variable '{0}' to '{1}' would create a circular dependency which is not allowed: {2}".format(nameFrom, nameTo, linkDesc))


class GeneratorVariableReport(VariableDict):
    def __init__(self, log: Log | None = None, allowAutoVariablesOverride: bool = False, configVariantOptions: list[str] | None = None) -> None:
        """
        allowAutoVariablesOverride if true then the automatic defined variables can be overridden with a add call,
        however if the options are different a warning is logged. However it is still not possible to change the 'LinkedTargetName'
        """
        super().__init__()
        self.__Log = log
        self.__Order: list[str] = []
        self.__DefaultOption: dict[str, int] = {}
        self.__AllowAutoVariablesOverride = allowAutoVariablesOverride

        configVariantOptions = configVariantOptions if configVariantOptions is not None else ToolAddedVariantOptions.CONFIG
        self.Add(ToolAddedVariant.CONFIG, configVariantOptions)

    def GetVariableReportList(self) -> list[VariableReport]:
        """Get the variable report for all registered variables in the same order as they where registered"""
        return [self.GetVariableReport(name) for name in self.__Order]

    def SYS_GetDefaultOptions(self) -> dict[str, int]:
        return self.__DefaultOption

    def __HasSameOptions(self, variableOptionList1: list[str], variableOptionList2: list[str]) -> bool:
        if len(variableOptionList1) != len(variableOptionList2):
            return False
        return all(entry in variableOptionList2 for entry in variableOptionList1)

    def Add(self, variableName: str, variableOptionList: list[str], linkedVariableName: str | None = None) -> None:
        if len(variableOptionList) <= 0:
            raise VariableOptionListCanNotBeEmptyException(variableName)

        theDict = self._GetDict()
        if variableName in theDict:
            if self.__AllowAutoVariablesOverride and variableName == ToolAddedVariant.CONFIG:
                currentDef = theDict[variableName]
                if not self.__HasSameOptions(variableOptionList, currentDef.Options):
                    if self.__Log is not None:
                        self.__Log.LogPrintVerbose(
                            2,
                            f"Overriding the auto variable '{variableName}' and the option list is different. New: {variableOptionList}, old: {currentDef.Options}",
                        )
                    theDict[variableName] = VariableReport(variableName, variableOptionList, currentDef.LinkTargetName)
                return
            raise VariableAlreadyDefinedException(f"The variable '{variableName}' has already been added")

        if linkedVariableName is not None:
            if linkedVariableName == variableName:
                raise LinkToSelfNotAllowedException(variableName)
            if linkedVariableName not in theDict:
                raise LinkedVariableNotFoundException(linkedVariableName)
            linkedOptionList = theDict[linkedVariableName].Options
            if len(linkedOptionList) != len(variableOptionList):
                raise VariableDoesNotContainTheSameAmountOfOptionsAsTheLinkedTargetException(
                    variableName, str(variableOptionList), linkedVariableName, str(linkedOptionList)
                )

        self.__Order.append(variableName)
        theDict[variableName] = VariableReport(variableName, variableOptionList, linkedVariableName)

    def TryGetDefaultOptionIndex(self, variableName: str) -> int | None:
        return self.__DefaultOption.get(variableName, None)

    def SetDefaultOption(self, variableName: str, variableOption: str) -> None:
        theDict = self._GetDict()
        if variableName not in theDict:
            raise VariableNotFoundException(variableName)

        variableReport = theDict[variableName]
        if variableReport.LinkTargetName is not None:
            raise LinkedVariableCanNotHaveDefaultValueException(variableName, variableReport.LinkTargetName)

        if variableOption not in variableReport.Options:
            raise InvalidVariableOptionNameException(variableName, variableOption, str(variableReport.Options))

        self.__DefaultOption[variableName] = variableReport.Options.index(variableOption)

    def SetDefaultOptionIndex(self, variableName: str, variableOptionIndex: int) -> None:
        theDict = self._GetDict()
        if variableName not in theDict:
            raise VariableNotFoundException(variableName)

        variableReport = theDict[variableName]
        if variableReport.LinkTargetName is not None:
            raise LinkedVariableCanNotHaveDefaultValueException(variableName, variableReport.LinkTargetName)

        if variableOptionIndex < 0 or variableOptionIndex >= len(variableReport.Options):
            raise InvalidVariableOptionIndexException(variableName, variableOptionIndex, str(variableReport.Options), len(variableReport.Options))

        self.__DefaultOption[variableName] = variableOptionIndex
