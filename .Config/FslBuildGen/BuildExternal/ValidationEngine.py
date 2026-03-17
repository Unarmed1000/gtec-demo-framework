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
import re
import subprocess
from enum import Enum

from FslBuildGen import IOUtil, Util
from FslBuildGen.BuildExternal.Commands.PackageRecipeValidateCommandAddDLL import PackageRecipeValidateCommandAddDLL
from FslBuildGen.BuildExternal.Commands.PackageRecipeValidateCommandAddHeaders import PackageRecipeValidateCommandAddHeaders
from FslBuildGen.BuildExternal.Commands.PackageRecipeValidateCommandAddLib import PackageRecipeValidateCommandAddLib
from FslBuildGen.BuildExternal.Commands.PackageRecipeValidateCommandAddTool import PackageRecipeValidateCommandAddTool
from FslBuildGen.BuildExternal.Commands.PackageRecipeValidateCommandEnvironmentVariable import PackageRecipeValidateCommandEnvironmentVariable
from FslBuildGen.BuildExternal.Commands.PackageRecipeValidateCommandFindExecutableFileInPath import PackageRecipeValidateCommandFindExecutableFileInPath
from FslBuildGen.BuildExternal.Commands.PackageRecipeValidateCommandFindExecutableFileInPathAddOnErrorWarning import (
    PackageRecipeValidateCommandFindExecutableFileInPathAddOnErrorWarning,
)
from FslBuildGen.BuildExternal.Commands.PackageRecipeValidateCommandFindFileInPath import PackageRecipeValidateCommandFindFileInPath
from FslBuildGen.BuildExternal.Commands.PackageRecipeValidateCommandPath import PackageRecipeValidateCommandPath
from FslBuildGen.BuildExternal.PackageExperimentalRecipe import PackageExperimentalRecipe
from FslBuildGen.BuildExternal.PackageRecipeInstallation import PackageRecipeInstallation
from FslBuildGen.BuildExternal.PackageRecipeResult import PackageRecipeResult
from FslBuildGen.BuildExternal.PackageRecipeResultFoundExecutable import PackageRecipeResultFoundExecutable
from FslBuildGen.BuildExternal.PackageRecipeResultManager import PackageRecipeResultManager
from FslBuildGen.DataTypes import BuildRecipeValidateCommand, BuildRecipeValidateMethod
from FslBuildGen.ErrorHelpManager import ErrorHelpManager
from FslBuildGen.Exceptions import GroupedException
from FslBuildGen.Log import Log
from FslBuildGen.Packages.Package import Package
from FslBuildGen.PlatformUtil import PlatformUtil
from FslBuildGen.Vars.VariableProcessor import VariableProcessor
from FslBuildGen.Version import Version


class InstallationStatus(Enum):
    NotInstalled = 0
    Installed = 1
    # There was a user environment error that caused the validation to fail.
    EnvironmentError = 2
    # No installation validation was available
    Undefined = 3


class ErrorClassification:
    # Currently valued according to severity
    NoError = 0
    Help = 1
    Environment = 2
    Critical = 3


class ErrorRecord:
    def __init__(self, errorClassification: int, message: str) -> None:
        self.Classification = errorClassification
        self.Message = message

    def ToString(self) -> str:
        return self.Message


class ValidationEngine:
    def __init__(
        self, log: Log, variableProcessor: VariableProcessor, packageRecipeResultManager: PackageRecipeResultManager, errorHelpManager: ErrorHelpManager
    ) -> None:
        self.__Log = log
        self.__VariableProcessor = variableProcessor
        self.__PackageRecipeResultManager = packageRecipeResultManager
        self.__ErrorHelpManager = errorHelpManager

    def Process(self, sourcePackage: Package) -> None:
        packageRecipeResult = self.__PackageRecipeResultManager.AddIfMissing(sourcePackage.Name)

        sourceRecipe = sourcePackage.ResolvedDirectExperimentalRecipe
        if sourceRecipe is None:
            raise Exception(f"Can't validate a package '{sourcePackage.Name}' that doesnt contain a source recipe")

        errorRecordList: list[ErrorRecord] = []
        status = self.__GetInstallationStatus(errorRecordList, sourceRecipe, sourcePackage.ResolvedRecipeVariants, packageRecipeResult)
        if status == InstallationStatus.Installed or status == InstallationStatus.Undefined:
            return

        self.RaiseExceptionFromErrorRecords(sourcePackage, errorRecordList)

    def RaiseExceptionFromErrorRecords(self, sourcePackage: Package, errorRecordList: list[ErrorRecord]) -> None:
        self.__PackageRecipeResultManager.AddIfMissing(sourcePackage.Name)

        sourceRecipe = sourcePackage.ResolvedDirectExperimentalRecipe
        if sourceRecipe is None:
            raise Exception("Invalid recipe")

        exceptionList = [Exception(f"Installation validation failed for package '{sourcePackage.Name}' recipe '{sourceRecipe.FullName}'")]
        for errorRecord in errorRecordList:
            exceptionList.append(Exception(errorRecord.ToString()))
        raise GroupedException(exceptionList)

    def GetInstallationStatus(self, sourcePackage: Package, rErrorRecordList: list[ErrorRecord] | None = None) -> InstallationStatus:
        packageRecipeResult = self.__PackageRecipeResultManager.AddIfMissing(sourcePackage.Name)

        sourceRecipe = sourcePackage.ResolvedDirectExperimentalRecipe
        if sourceRecipe is None:
            raise Exception(f"Can't validate a package '{sourcePackage.Name}' that doesnt contain a source recipe")

        errorRecordList = [] if rErrorRecordList is None else rErrorRecordList

        status = self.__GetInstallationStatus(errorRecordList, sourceRecipe, sourcePackage.ResolvedRecipeVariants, packageRecipeResult)
        if status == InstallationStatus.Installed or status == InstallationStatus.Undefined:
            return status

        if rErrorRecordList is None:
            self.__Log.LogPrintWarning(f"Installation validation failed for package '{sourcePackage.Name}' recipe '{sourceRecipe.FullName}'")
            for errorRecord in errorRecordList:
                self.__Log.LogPrintWarning(errorRecord.ToString())
        return status

    def __ChoseMostSevereClasification(self, errorClassification1: int, errorClassification2: int) -> int:
        return errorClassification1 if errorClassification1 > errorClassification2 else errorClassification2

    def __GetStatusFromErrorRecords(self, errorRecordList: list[ErrorRecord]) -> InstallationStatus:
        errorClassification = ErrorClassification.NoError
        for record in errorRecordList:
            errorClassification = self.__ChoseMostSevereClasification(errorClassification, record.Classification)

        if errorClassification == ErrorClassification.NoError:
            return InstallationStatus.Installed
        elif errorClassification == ErrorClassification.Help:
            return InstallationStatus.NotInstalled
        elif errorClassification == ErrorClassification.Environment:
            return InstallationStatus.EnvironmentError
        elif errorClassification == ErrorClassification.Critical:
            return InstallationStatus.NotInstalled
        raise Exception(f"Unsupported error classification: {errorClassification}")

    def __GetInstallationStatus(
        self, rErrorRecordList: list[ErrorRecord], sourceRecipe: PackageExperimentalRecipe, recipeVariants: list[str], packageRecipeResult: PackageRecipeResult
    ) -> InstallationStatus:
        if sourceRecipe.ValidateInstallation is None:
            self.__Log.LogPrintVerbose(3, f"WARNING: no intallation validation available for recipe '{sourceRecipe.FullName}'")
            return InstallationStatus.Undefined

        if self.__Log.Verbosity >= 1:
            if len(sourceRecipe.ValidateInstallation.CommandList) > 0:
                self.__Log.LogPrint(f"Validating installation for '{sourceRecipe.FullName}'")
            else:
                self.__Log.LogPrint(f"WARNING: No installation validation commands available for '{sourceRecipe.FullName}'")

        self.__DoValidateInstallation(rErrorRecordList, sourceRecipe, recipeVariants, packageRecipeResult)

        return self.__GetStatusFromErrorRecords(rErrorRecordList)

    def __DoValidateInstallation(
        self, rErrorRecordList: list[ErrorRecord], sourceRecipe: PackageExperimentalRecipe, recipeVariants: list[str], packageRecipeResult: PackageRecipeResult
    ) -> None:
        self.__Log.PushIndent()
        try:
            installationPath = sourceRecipe.ResolvedInstallLocation.ResolvedPath if sourceRecipe.ResolvedInstallLocation is not None else None
            validateInstallation = sourceRecipe.ValidateInstallation
            if validateInstallation is None:
                raise Exception("Invalid recipe")

            if len(recipeVariants) <= 0:
                self.__DoValidate(rErrorRecordList, installationPath, validateInstallation, packageRecipeResult)
            else:
                for entry in recipeVariants:
                    installationPathCopy = IOUtil.Join(installationPath, entry) if installationPath is not None else None
                    self.__Log.LogPrint(f"Recipe variant: {entry}")
                    self.__Log.PushIndent()
                    try:
                        self.__DoValidate(rErrorRecordList, installationPathCopy, validateInstallation, packageRecipeResult)
                    finally:
                        self.__Log.PopIndent()
        finally:
            self.__Log.PopIndent()

    def __DoValidate(
        self,
        rErrorRecordList: list[ErrorRecord],
        installationPath: str | None,
        validateInstallation: PackageRecipeInstallation,
        packageRecipeResult: PackageRecipeResult,
    ) -> None:
        for command in validateInstallation.CommandList:
            result = False
            if command.CommandType == BuildRecipeValidateCommand.EnvironmentVariable:
                if not isinstance(command, PackageRecipeValidateCommandEnvironmentVariable):
                    raise Exception("Invalid command")
                result = self.__ValidateEnvironmentVariable(rErrorRecordList, command)
            elif command.CommandType == BuildRecipeValidateCommand.Path:
                if not isinstance(command, PackageRecipeValidateCommandPath):
                    raise Exception("Invalid command")
                result = self.__ValidatePath(rErrorRecordList, installationPath, command)
            elif command.CommandType == BuildRecipeValidateCommand.FindFileInPath:
                if not isinstance(command, PackageRecipeValidateCommandFindFileInPath):
                    raise Exception("Invalid command")
                result = self.__ValidateFindFileInPath(rErrorRecordList, installationPath, command)
            elif command.CommandType == BuildRecipeValidateCommand.FindExecutableFileInPath:
                if not isinstance(command, PackageRecipeValidateCommandFindExecutableFileInPath):
                    raise Exception("Invalid command")
                result = self.__ValidateFindExecutableFileInPath(rErrorRecordList, installationPath, command, packageRecipeResult)
            elif command.CommandType == BuildRecipeValidateCommand.AddHeaders:
                if not isinstance(command, PackageRecipeValidateCommandAddHeaders):
                    raise Exception("Invalid command")
                result = self.__ValidateAddHeaders(rErrorRecordList, installationPath, command)
            elif command.CommandType == BuildRecipeValidateCommand.AddLib:
                if not isinstance(command, PackageRecipeValidateCommandAddLib):
                    raise Exception("Invalid command")
                result = self.__ValidateAddLib(rErrorRecordList, installationPath, command)
            elif command.CommandType == BuildRecipeValidateCommand.AddDLL:
                if not isinstance(command, PackageRecipeValidateCommandAddDLL):
                    raise Exception("Invalid command")
                result = self.__ValidateAddDLL(rErrorRecordList, installationPath, command)
            elif command.CommandType == BuildRecipeValidateCommand.AddTool:
                if not isinstance(command, PackageRecipeValidateCommandAddTool):
                    raise Exception("Invalid command")
                result = self.__ValidateAddTool(rErrorRecordList, installationPath, command, packageRecipeResult)
            else:
                raise Exception(f"Unknown validation command '{command.CommandName}'")
            if not result and command.Help is not None:
                rErrorRecordList.append(ErrorRecord(ErrorClassification.Help, f"  {command.Help}"))

    def __ValidateEnvironmentVariable(self, rErrorRecordList: list[ErrorRecord], command: PackageRecipeValidateCommandEnvironmentVariable) -> bool:
        result, value = self.__DoValidateEnvironmentVariable(rErrorRecordList, command)
        if self.__Log.Verbosity >= 1:
            self.__Log.LogPrint(f"Validating environment variable '{command.Name}' '{BuildRecipeValidateMethod.TryToString(command.Method, True)}': {result}")
            if self.__Log.Verbosity >= 2:
                self.__Log.LogPrint(f"  '{value}'")
        return result

    def __ValidatePath(self, rErrorRecordList: list[ErrorRecord], installationPath: str | None, command: PackageRecipeValidateCommandPath) -> bool:
        result, value = self.__DoValidatePath(rErrorRecordList, installationPath, command)
        if self.__Log.Verbosity >= 1:
            self.__Log.LogPrint(f"Validating path '{command.Name}' '{BuildRecipeValidateMethod.TryToString(command.Method, True)}': {result}")
            if self.__Log.Verbosity >= 2:
                self.__Log.LogPrint(f"  '{value}'")
        return result

    def __ValidateFindFileInPath(
        self, rErrorRecordList: list[ErrorRecord], installationPath: str | None, command: PackageRecipeValidateCommandFindFileInPath
    ) -> bool:
        result, value = self.__TryFindFileInPath(rErrorRecordList, installationPath, command.Name, command.ExpectedPath)
        if self.__Log.Verbosity >= 1:
            if command.ExpectedPath is None:
                self.__Log.LogPrint(f"Validating file '{command.Name}' is in path: {result}")
            else:
                self.__Log.LogPrint(f"Validating file '{command.Name}' is in path at '{command.ExpectedPath}': {result}")
            if value is not None and self.__Log.Verbosity >= 2:
                self.__Log.LogPrint(f"  '{value}'")
        return result

    def __CheckVersion(self, minVersionList: list[int], foundVersionList: list[int]) -> bool:
        for idx, val in enumerate(minVersionList):
            if idx > len(foundVersionList):
                return idx > 0
            if val < foundVersionList[idx]:
                return True
            if val > foundVersionList[idx]:
                return False
        return True

    def __CheckOnErrorWarning(
        self, foundVersionList: list[int], versionRegEx: str, warning: PackageRecipeValidateCommandFindExecutableFileInPathAddOnErrorWarning
    ) -> None:
        startVersionList = Util.ParseVersionString(warning.StartVersion, maxValues=4)
        if len(startVersionList) > len(foundVersionList):
            self.__Log.DoPrintWarning(
                f"The regex '{versionRegEx}' did not capture the enough version number elements to compare against the start version '{warning.StartVersion}'"
            )
            return  # Its just a warning hint we can't display, so we log it and skip it for now
        if self.__CheckVersion(startVersionList, foundVersionList):
            # ok we passed the start version check for this warning hint
            if warning.EndVersion is not None:
                endVersionList = Util.ParseVersionString(warning.EndVersion, maxValues=4)
                if len(endVersionList) > len(foundVersionList):
                    self.__Log.DoPrintWarning(
                        f"The regex '{versionRegEx}' did not capture the enough version number elements to compare against the end version '{warning.EndVersion}'"
                    )
                    return  # Its just a warning hint we can't display, so we log it and skip it for now
                if self.__CheckVersion(endVersionList, foundVersionList):
                    return
            self.__ErrorHelpManager.AddOnErrorWarningHint(warning.Help)

    def __CheckOnErrorWarnings(
        self, foundVersionList: list[int], versionRegEx: str, addOnErrorWarning: list[PackageRecipeValidateCommandFindExecutableFileInPathAddOnErrorWarning]
    ) -> None:
        for warning in addOnErrorWarning:
            self.__CheckOnErrorWarning(foundVersionList, versionRegEx, warning)

    def __TryValidateCommandVersion(
        self,
        cmd: str,
        versionCommand: str,
        versionRegEx: str,
        minVersion: str | None,
        addOnErrorWarning: list[PackageRecipeValidateCommandFindExecutableFileInPathAddOnErrorWarning],
        versionSplitChar: str,
    ) -> list[int]:
        output = ""
        try:
            runCmd = [cmd, versionCommand]
            with subprocess.Popen(runCmd, stderr=subprocess.STDOUT, stdout=subprocess.PIPE, universal_newlines=True) as proc:
                output = proc.stdout.read().strip() if proc.stdout is not None else ""
                if proc.stdout is not None:
                    proc.stdout.close()
                result = proc.wait()
                if result != 0:
                    self.__Log.LogPrintWarning("The command '{}' failed with '{}'".format(" ".join(runCmd), result))
                    return []
        except FileNotFoundError:
            self.__Log.DoPrintWarning("The command '{}' failed with 'file not found'.".format(" ".join(runCmd)))
            return []

        match = re.search(versionRegEx, output)
        if match is None:
            self.__Log.DoPrintWarning(f"The regex '{versionRegEx}' did not capture the version")
            return []
        if len(match.groups()) > 1:
            self.__Log.DoPrintWarning(f"The regex '{versionRegEx}' captured more than one group: {match.groups}")
        if len(match.groups()) != 1:
            self.__Log.DoPrintWarning(f"The regex '{versionRegEx}' did not capture a group with version information")
        matchResult = match.group(1)
        return self.__TryParseVersion(versionRegEx, minVersion, addOnErrorWarning, matchResult, versionSplitChar)

    def __TryParseVersion(
        self,
        versionRegEx: str,
        minVersion: str | None,
        addOnErrorWarning: list[PackageRecipeValidateCommandFindExecutableFileInPathAddOnErrorWarning],
        matchResult: str,
        splitChar: str,
    ) -> list[int]:
        try:
            # time to parse the version string
            foundVersionList = Util.ParseVersionString(matchResult, splitChar=splitChar, maxValues=4)
            if minVersion is not None:
                minVersionList = Util.ParseVersionString(minVersion, splitChar=splitChar, maxValues=4)
                if len(minVersionList) > len(foundVersionList):
                    self.__Log.DoPrintWarning(
                        f"The regex '{versionRegEx}' did not capture the enough version number elements to compare against the min version '{minVersion}'"
                    )
                    return []
                self.__Log.LogPrint(f"  Found version {matchResult} expected minVersion {minVersion}")
                if not self.__CheckVersion(minVersionList, foundVersionList):
                    return []
            self.__CheckOnErrorWarnings(foundVersionList, versionRegEx, addOnErrorWarning)
            return foundVersionList
        except Exception as ex:
            self.__Log.DoPrintWarning(f"Failed to parse version string: {str(ex)}")
            return []

    def __ValidateFindExecutableFileInPath(
        self,
        rErrorRecordList: list[ErrorRecord],
        installationPath: str | None,
        command: PackageRecipeValidateCommandFindExecutableFileInPath,
        packageRecipeResult: PackageRecipeResult,
    ) -> bool:
        # Patch filename with the platform dependent name
        alternatives = [command.Name]
        if len(command.Alternatives) > 0:
            alternatives += command.Alternatives
        retry = True
        currentCommandName = ""
        newErrors: list[ErrorRecord] = []
        while retry and len(alternatives) > 0:
            currentCommandName = alternatives[0]
            retry = False
            platformFilename = PlatformUtil.GetPlatformDependentExecuteableName(currentCommandName, PlatformUtil.DetectBuildPlatformType())

            result, value = self.__TryFindFileInPath(newErrors, installationPath, platformFilename, command.ExpectedPath)

            foundVersion = [0]
            if result and value is not None and command.VersionCommand is not None:
                if command.VersionCommand is None or command.VersionRegEx is None:
                    raise Exception("Internal error")
                foundVersion = self.__TryValidateCommandVersion(
                    value, command.VersionCommand, command.VersionRegEx, command.MinVersion, command.AddOnErrorWarning, command.VersionSplitChar
                )
                result = len(foundVersion) > 0

            # Cache information about what we discovered
            if result and value is not None:
                exeRecord = PackageRecipeResultFoundExecutable(command.Name, currentCommandName, value, self.__ToVersion(foundVersion))
                packageRecipeResult.AddFoundExecutable(exeRecord)
            elif len(alternatives) > 0:
                alternatives = alternatives[1:]
                retry = True

        if self.__Log.Verbosity >= 1:
            # On failure we show the 'most' common name
            if result or (len(command.Alternatives) < 0):
                if command.ExpectedPath is None:
                    self.__Log.LogPrint(f"Validating executable file '{currentCommandName}' is in path: {result}")
                else:
                    self.__Log.LogPrint(f"Validating executable file '{currentCommandName}' is in path at '{command.ExpectedPath}': {result}")
            else:
                allAlternatives = [command.Name]
                if command.Alternatives is not None:
                    allAlternatives += command.Alternatives
                if command.ExpectedPath is None:
                    self.__Log.LogPrint("Validating one of these executable files '{}' is in path: {}".format(", ".join(allAlternatives), result))
                else:
                    self.__Log.LogPrint(
                        "Validating one of these executable files '{}' is in path at '{}': {}".format(", ".join(allAlternatives), command.ExpectedPath, result)
                    )
            if value is not None and self.__Log.Verbosity >= 2:
                self.__Log.LogPrint(f"  '{value}'")

        if not result:
            rErrorRecordList += newErrors

        return result

    def __ToVersion(self, foundVersion: list[int]) -> Version:
        if len(foundVersion) >= 4:
            return Version(foundVersion[0], foundVersion[1], foundVersion[2], foundVersion[3])
        if len(foundVersion) >= 3:
            return Version(foundVersion[0], foundVersion[1], foundVersion[2])
        if len(foundVersion) >= 2:
            return Version(foundVersion[0], foundVersion[1])
        return Version(foundVersion[0])

    def __ValidateAddHeaders(self, rErrorRecordList: list[ErrorRecord], installationPath: str | None, command: PackageRecipeValidateCommandAddHeaders) -> bool:
        result, value = self.__DoValidateAddHeaders(rErrorRecordList, installationPath, command)
        if self.__Log.Verbosity >= 1:
            self.__Log.LogPrint(f"Validating AddHeaders '{command.Name}': {result}")
            if self.__Log.Verbosity >= 2:
                self.__Log.LogPrint(f"  '{value}'")
        return result

    def __ValidateAddLib(self, rErrorRecordList: list[ErrorRecord], installationPath: str | None, command: PackageRecipeValidateCommandAddLib) -> bool:
        result, value = self.__DoValidateFile(rErrorRecordList, installationPath, command.Name, False)
        if self.__Log.Verbosity >= 1:
            self.__Log.LogPrint(f"Validating AddLib '{command.Name}': {result}")
            if self.__Log.Verbosity >= 2:
                self.__Log.LogPrint(f"  '{value}'")
        result2 = True
        if command.Name != command.DebugName:
            result2, value2 = self.__DoValidateFile(rErrorRecordList, installationPath, command.DebugName, False)
            if self.__Log.Verbosity >= 1:
                self.__Log.LogPrint(f"Validating AddLib (Debug) '{command.DebugName}': {result2}")
                if self.__Log.Verbosity >= 2:
                    self.__Log.LogPrint(f"  '{value2}'")
        return result and result2

    def __ValidateAddDLL(self, rErrorRecordList: list[ErrorRecord], installationPath: str | None, command: PackageRecipeValidateCommandAddDLL) -> bool:
        result, value = self.__DoValidateFile(rErrorRecordList, installationPath, command.Name, False)
        if self.__Log.Verbosity >= 1:
            self.__Log.LogPrint(f"Validating AddDLL '{command.Name}': {result}")
            if self.__Log.Verbosity >= 2:
                self.__Log.LogPrint(f"  '{value}'")
        result2 = True
        if command.Name != command.DebugName:
            result2, value2 = self.__DoValidateFile(rErrorRecordList, installationPath, command.DebugName, False)
            if self.__Log.Verbosity >= 1:
                self.__Log.LogPrint(f"Validating AddDLL (Debug) '{command.DebugName}': {result2}")
                if self.__Log.Verbosity >= 2:
                    self.__Log.LogPrint(f"  '{value2}'")
        return result and result2

    def __ValidateAddTool(
        self,
        rErrorRecordList: list[ErrorRecord],
        installationPath: str | None,
        command: PackageRecipeValidateCommandAddTool,
        packageRecipeResult: PackageRecipeResult,
    ) -> bool:
        toolName = PlatformUtil.GetPlatformDependentExecuteableName(command.Name, PlatformUtil.DetectBuildPlatformType())
        result, value = self.__DoValidateFile(rErrorRecordList, installationPath, toolName, False)
        if self.__Log.Verbosity >= 1:
            self.__Log.LogPrint(f"Validating AddTool '{command.Name}': {result}")
            if self.__Log.Verbosity >= 2:
                self.__Log.LogPrint(f"  '{value}'")
        if result and value is not None:
            foundVersion = [0]
            if result and value is not None and command.VersionCommand is not None:
                if command.VersionCommand is None or command.VersionRegEx is None:
                    raise Exception("Internal error")
                foundVersion = self.__TryValidateCommandVersion(
                    value, command.VersionCommand, command.VersionRegEx, command.MinVersion, [], command.VersionSplitChar
                )
                result = len(foundVersion) > 0

            exeRecord = PackageRecipeResultFoundExecutable(command.Name, command.Name, value, self.__ToVersion(foundVersion))
            packageRecipeResult.AddFoundExecutable(exeRecord)
        return result

    def __DoValidateEnvironmentVariable(
        self, rErrorRecordList: list[ErrorRecord], command: PackageRecipeValidateCommandEnvironmentVariable
    ) -> tuple[bool, str | None]:
        value = os.environ.get(command.Name)
        if not value:
            rErrorRecordList.append(
                ErrorRecord(ErrorClassification.Environment, f"Environment variable '{command.Name}' is not defined, please define it as required.")
            )
            return False, value

        if self.__Log.Verbosity >= 4:
            self.__Log.LogPrint(f"ValidateEnvironmentVariable: '{command.Name}'='{value}'")

        if command.Method == BuildRecipeValidateMethod.IsDirectory:
            if not IOUtil.IsDirectory(value):
                rErrorRecordList.append(
                    ErrorRecord(ErrorClassification.Environment, f"Environment variable '{command.Name}' contained '{value}' which is not a directory.")
                )
                return False, value
        elif command.Method == BuildRecipeValidateMethod.IsFile:
            if not IOUtil.IsFile(value):
                fileHelp = self.__GetFailedFileCheckExtraHelpString(value)
                rErrorRecordList.append(
                    ErrorRecord(ErrorClassification.Environment, f"Environment variable '{command.Name}' contained '{value}' which is not a file.{fileHelp}")
                )
                return False, value
        elif command.Method == BuildRecipeValidateMethod.Exists:
            if not IOUtil.Exists(value):
                rErrorRecordList.append(
                    ErrorRecord(ErrorClassification.Environment, f"Environment variable '{command.Name}' contained '{value}' which is a path that dont exist.")
                )
                return False, value
        else:
            raise Exception(f"Unsupported BuildRecipeValidateMethod '{command.Method}'")

        if not command.AllowEndSlash and (value.endswith("/") or value.endswith("\\")):
            rErrorRecordList.append(
                ErrorRecord(
                    ErrorClassification.Environment, f"Environment variable '{command.Name}' content '{value}' can not end with a slash '/' or backslash '\\'."
                )
            )
            return False, value

        return True, IOUtil.NormalizePath(value)

    def __DoValidatePath(
        self, rErrorRecordList: list[ErrorRecord], installationPath: str | None, command: PackageRecipeValidateCommandPath
    ) -> tuple[bool, str | None]:
        if self.__Log.Verbosity >= 4:
            self.__Log.LogPrint(f"ValidatePath '{command.Name}'")

        result, path = self.__TryResolvePath(rErrorRecordList, installationPath, command.Name)
        if not result or path is None:
            return False, path

        if self.__Log.Verbosity >= 4:
            self.__Log.LogPrint(f"Resolving to '{path}'")

        if command.Method == BuildRecipeValidateMethod.IsDirectory:
            if not IOUtil.IsDirectory(path):
                rErrorRecordList.append(ErrorRecord(ErrorClassification.Critical, f"Path '{command.Name}' resolved to '{path}' which is not a directory."))
                return False, path
        elif command.Method == BuildRecipeValidateMethod.IsFile:
            if not IOUtil.IsFile(path):
                fileHelp = self.__GetFailedFileCheckExtraHelpString(path)
                rErrorRecordList.append(ErrorRecord(ErrorClassification.Critical, f"Path '{command.Name}' resolved to '{path}' which is not a file.{fileHelp}"))
                return False, path
        elif command.Method == BuildRecipeValidateMethod.Exists:
            if not IOUtil.Exists(path):
                rErrorRecordList.append(
                    ErrorRecord(ErrorClassification.Critical, f"Path '{command.Name}' resolved to '{path}' which is a path that dont exist.")
                )
                return False, path
        else:
            raise Exception(f"Unsupported BuildRecipeValidateMethod '{command.Method}'")
        return True, path

    def __TryFindFileInPath(
        self, rErrorRecordList: list[ErrorRecord], installationPath: str | None, commandFileName: str, commandExpectedPath: str | None
    ) -> tuple[bool, str | None]:
        if self.__Log.Verbosity >= 4:
            self.__Log.LogPrint(f"FindFileInPath: '{commandFileName}'")

        path = IOUtil.TryFindFileInPath(commandFileName)
        if path is None:
            rErrorRecordList.append(ErrorRecord(ErrorClassification.Environment, f"File '{commandFileName}' could not be found in path."))
            return False, path

        if self.__Log.Verbosity >= 4:
            self.__Log.LogPrint(f"Found at '{path}'")

        path = IOUtil.NormalizePath(path)

        if commandExpectedPath is not None:
            result, expectedPath = self.__TryResolvePath(rErrorRecordList, installationPath, commandExpectedPath)
            if not result or expectedPath is None:
                return False, path
            expectedPath = IOUtil.Join(expectedPath, commandFileName)
            if path != expectedPath:
                rErrorRecordList.append(
                    ErrorRecord(
                        ErrorClassification.Environment,
                        f"File '{commandFileName}' was not found at the expected path '{commandExpectedPath}' which resolves to '{expectedPath}' but at '{path}' instead",
                    )
                )
                if expectedPath.lower() == path.lower():
                    rErrorRecordList.append(
                        ErrorRecord(ErrorClassification.Help, f"Please beware that '{expectedPath}'=='{path}' if you ignore character case")
                    )
                return False, path

        return True, path

    def __DoValidateAddHeaders(
        self, rErrorRecordList: list[ErrorRecord], installationPath: str | None, command: PackageRecipeValidateCommandAddHeaders
    ) -> tuple[bool, str | None]:
        if self.__Log.Verbosity >= 4:
            self.__Log.LogPrint(f"ValidateAddHeaders '{command.Name}'")

        result, path = self.__TryResolvePath(rErrorRecordList, installationPath, command.Name, False)
        if not result or path is None:
            return False, path

        if self.__Log.Verbosity >= 4:
            self.__Log.LogPrint(f"Resolving to '{path}'")

        if not IOUtil.IsDirectory(path):
            rErrorRecordList.append(ErrorRecord(ErrorClassification.Critical, f"Path '{command.Name}' resolved to '{path}' which is not a directory."))
            return False, path
        return True, path

    def __DoValidateFile(
        self, rErrorRecordList: list[ErrorRecord], installationPath: str | None, filename: str, allowEnvironmentVariable: bool
    ) -> tuple[bool, str | None]:
        result, path = self.__TryResolvePath(rErrorRecordList, installationPath, filename, allowEnvironmentVariable)
        if not result or path is None:
            return False, path

        if self.__Log.Verbosity >= 4:
            self.__Log.LogPrint(f"Resolving to '{path}'")

        if not IOUtil.IsFile(path):
            fileHelp = self.__GetFailedFileCheckExtraHelpString(path)
            rErrorRecordList.append(ErrorRecord(ErrorClassification.Critical, f"Path '{filename}' resolved to '{path}' which is not a file.{fileHelp}"))
            return False, path
        return True, path

    def __TryResolvePath(
        self, rErrorRecordList: list[ErrorRecord], installationPath: str | None, sourcePath: str, allowEnvironmentVariable: bool = True
    ) -> tuple[bool, str | None]:
        if not allowEnvironmentVariable:
            if installationPath is None:
                rErrorRecordList.append(
                    ErrorRecord(
                        ErrorClassification.Environment,
                        f"Path '{sourcePath}' tried to use a undefined install path. Is the recipe missing a Pipeline section, ExternalInstallDirectory or is the supplied path missing a environment variable to qualify it?",
                    )
                )
                return False, None
            return True, IOUtil.Join(installationPath, sourcePath)

        # NOTE: workaround Union of tuples not being iterable bug in mypy https://github.com/python/mypy/issues/1575
        tupleResult = self.__VariableProcessor.TrySplitLeadingEnvironmentVariablesNameAndPath(sourcePath)
        environmentVariableName = tupleResult[0]
        restPath = tupleResult[1]
        if environmentVariableName is None:
            if installationPath is None:
                rErrorRecordList.append(
                    ErrorRecord(
                        ErrorClassification.Environment,
                        f"Path '{sourcePath}' tried to use a undefined install path. Is the recipe missing a Pipeline section, ExternalInstallDirectory or is the supplied path missing a environment variable to qualify it?",
                    )
                )
                return False, None
            return True, IOUtil.Join(installationPath, sourcePath)
        restPath = restPath if restPath is not None else ""

        value = os.environ.get(environmentVariableName)
        if value is None:
            rErrorRecordList.append(
                ErrorRecord(
                    ErrorClassification.Environment, f"Path '{sourcePath}' contained environment variable '{environmentVariableName}' which is not defined."
                )
            )
            return False, None
        path = IOUtil.Join(value, restPath)
        if self.__Log.Verbosity >= 4:
            self.__Log.LogPrint(f"Path: '{sourcePath}' using environment variable '{environmentVariableName}'='{value}' resolving to '{path}'")
        return True, path

    def __GetFailedFileCheckExtraHelpString(self, sourcePath: str) -> str:
        directoryName = IOUtil.GetDirectoryName(sourcePath)
        if not directoryName or IOUtil.IsDirectory(directoryName):
            return ""
        # filename = IOUtil.GetDirectoryName(sourcePath)
        return f" The parent directory '{directoryName}' did not exist either."
