#!/usr/bin/env python3
# ****************************************************************************************************************************************************
# Copyright (c) 2014 Freescale Semiconductor, Inc.
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
#    * Neither the name of the Freescale Semiconductor, Inc. nor the names of
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


class ExitException(Exception):
    def __init__(self, exitCode: int) -> None:
        super().__init__()
        self.ExitCode = exitCode


class AggregateException(Exception):
    def __init__(self, exceptionList: list[Exception]) -> None:
        super().__init__("AggregateException")
        self.ExceptionList: list[Exception] = exceptionList


class GroupedException(Exception):
    def __init__(self, exceptionList: list[Exception]) -> None:
        super().__init__("GroupedException")
        self.ExceptionList: list[Exception] = exceptionList


class PackageHasMultipleDefinitionsException(Exception):
    def __init__(self, message: str) -> None:
        # pylint: disable=useless-super-delegation
        super().__init__(message)


class PackageMissingRequiredIncludeDirectoryException(Exception):
    def __init__(self, directory: str) -> None:
        msg = f"Required include directory '{directory}' not found"
        super().__init__(msg)


class PackageMissingRequiredSourceDirectoryException(Exception):
    def __init__(self, directory: str) -> None:
        msg = f"Required source directory '{directory}' not found"
        super().__init__(msg)


class PackageRequirementExtendsUnusedFeatureException(Exception):
    def __init__(self, requirementName: str, requirementExtends: str, packageName: str) -> None:
        msg = f"Package requirement '{requirementName}' in package '{packageName}' extends unknown feature '{requirementExtends}'. Is it a feature spelling error or missing package dependency?"
        super().__init__(msg)


class InternalErrorException(Exception):
    """Error"""


class UsageErrorException(Exception):
    """Error"""


class UnknownTypeException(Exception):
    """Error"""


class FileNotFoundException(Exception):
    """Error"""

    def __init__(self, oldSchoolFormat: str, filename: str) -> None:
        super().__init__(oldSchoolFormat % filename)


class PackageIncludeFilePathInvalidException(Exception):
    """Error"""

    def __init__(self, packageName: str, filename: str, expectedIncludePathStart: str) -> None:
        msg = f"Package '{packageName}' include file '{filename}' did not start with '{expectedIncludePathStart} as expected"
        super().__init__(msg)


class CircularDependencyException(Exception):
    """E"""


class CircularDependencyInDependentModuleException(Exception):
    """E"""


class NotImplementedException(Exception):
    """E"""


class UnsupportedException(Exception):
    """E"""


class DuplicatedNewProjectTemplatesRootPath(Exception):
    """E"""

    def __init__(self, name: str, configFileName1: str, configFileName2: str) -> None:
        msg = f"Root path '{name}' listed multiple times in {configFileName1} and {configFileName2}"
        super().__init__(msg)


class DuplicatedConfigRootPath(Exception):
    """E"""

    def __init__(self, name: str, configFileName: str) -> None:
        msg = f"Root path '{name}' listed multiple times in {configFileName}"
        super().__init__(msg)


class DuplicatedConfigBasePackage(Exception):
    """E"""

    def __init__(self, name: str, configFileName: str) -> None:
        msg = f"Base package '{name}' listed multiple times in {configFileName}"
        super().__init__(msg)


class DuplicatedConfigPackageLocation(Exception):
    """E"""

    def __init__(self, name: str, configFileName: str) -> None:
        msg = f"Package location '{name}' listed multiple times in {configFileName}"
        super().__init__(msg)


class DuplicatedConfigContentBuilder(Exception):
    """E"""

    def __init__(self, name: str, configFileName: str) -> None:
        msg = f"ContentBuilder '{name}' listed multiple times in {configFileName}"
        super().__init__(msg)


class IncompleteVariableFoundException(Exception):
    """E"""

    def __init__(self, message: str, tag: object | None) -> None:
        msg = f"String '{message}' contains '${{' but is missing a terminating '}}'."
        super().__init__(msg)
        self.Tag = tag


class IncompleteEnvironmentVariableFoundException(Exception):
    """E"""

    def __init__(self, message: str, tag: object | None) -> None:
        msg = f"String '{message}' contains '$(' but is missing a terminating ')'."
        super().__init__(msg)
        self.Tag = tag


class EnvironmentVariableInMiddleOfStringException(Exception):
    """E"""

    def __init__(self, message: str, tag: object | None) -> None:
        msg = f"Environment variables can not be located in the middle of a string '{message}'"
        super().__init__(msg)
        self.Tag = tag


class CombinedEnvironmentVariableAndPathException(Exception):
    """E"""

    def __init__(self, message: str, tag: object | None) -> None:
        msg = f"String '{message}' contains a environment variable mixed with a path."
        super().__init__(msg)
        self.Tag = tag


class CombinedVariableAndPathException(Exception):
    """E"""

    def __init__(self, message: str, tag: object | None) -> None:
        msg = f"String '{message}' contains a variable mixed with a path."
        super().__init__(msg)
        self.Tag = tag


class VariableInMiddleOfStringException(Exception):
    """E"""

    def __init__(self, message: str, tag: object | None) -> None:
        msg = f"Variables can not be located in the middle of a string '{message}'"
        super().__init__(msg)
        self.Tag = tag


class VariableNotDefinedException(Exception):
    """E"""

    def __init__(self, variableName: str, variableDict: dict[str, object | None]) -> None:
        msg = "Variable '{}' is not defined, possible variable names [{}]".format(variableName, ", ".join(list(variableDict.keys())))
        super().__init__(msg)


class ToolDependencyNotFoundException(Exception):
    def __init__(self, depName: str, additionalMessage: str = "") -> None:
        message = f"Package not found '{depName}', its required by the tool chain."
        if len(additionalMessage) > 0:
            message += f" {additionalMessage}"
        super().__init__(message)


class BasePackageNotFoundException(Exception):
    def __init__(self, depName: str, additionalMessage: str = "") -> None:
        message = f"Base package not found '{depName}' (added by project)"
        if len(additionalMessage) > 0:
            message += f" {additionalMessage}"
        super().__init__(message)


class DependencyNotFoundException(Exception):
    def __init__(self, packageName: str, depName: str, candidateList: list[str] | None = None, additionalMessage: str = "") -> None:
        if candidateList is None or len(candidateList) <= 0:
            message = f"'{packageName}' has a dependency to a unknown package '{depName}'."
        else:
            message = f"'{packageName}' has a dependency to a unknown package '{depName}' did you mean {candidateList}."
        if len(additionalMessage) > 0:
            message += f" {additionalMessage}"
        super().__init__(message)


# If this exception fires it means the package loader has a bug
class PackageLoaderFailedToLocatePackageException(DependencyNotFoundException):
    def __init__(self, packageName: str, depName: str) -> None:
        msg = "*INTERNAL_ERROR* the package exist, but the package loader failed to locate it."
        super().__init__(packageName, depName, additionalMessage=msg)


class InvalidDependencyException(Exception):
    def __init__(self, packageName: str, depName: str) -> None:
        super().__init__(f"'{packageName}' has a invalid dependency to package '{depName}'")


class VariantOptionNameCollisionException(Exception):
    def __init__(self, firstName: str, secondName: str) -> None:
        msg = f"The option name: '{secondName}' collides with the previously defined '{firstName}'"
        super().__init__(msg)


class InvalidPackageFlavorNameException(Exception):
    def __init__(self, name: str) -> None:
        super().__init__(f"'{name}' is not a valid package flavor name")


class InvalidPackageFlavorOptionNameException(Exception):
    def __init__(self, name: str) -> None:
        super().__init__(f"'{name}' is not a valid package flavor option name")


class InvalidUnresolvedBasicPackageNameException(Exception):
    def __init__(self, name: str) -> None:
        super().__init__(f"'{name}' is not a valid unresolved basic package name")


class InvalidUnresolvedPackageNameException(Exception):
    def __init__(self, name: str) -> None:
        super().__init__(f"'{name}' is not a valid unresolved package name")


class InvalidUnresolvedPackageFlavorNameException(Exception):
    def __init__(self, name: str) -> None:
        super().__init__(f"'{name}' is not a valid unresolved flavor name")


class InvalidPackageNameException(Exception):
    def __init__(self, name: str) -> None:
        super().__init__(f"'{name}' is not a valid package name")


class InvalidPackageInstanceNameException(Exception):
    def __init__(self, name: str) -> None:
        super().__init__(f"'{name}' is not a valid package instance name")


class InvalidCompanyNameException(Exception):
    def __init__(self, name: str) -> None:
        super().__init__(f"'{name}' is not a valid company name")


class InvalidPackageShortNameException(Exception):
    def __init__(self, name: str) -> None:
        super().__init__(f"'{name}' is not a valid package short name")


class InvalidPackageNamespaceNameException(Exception):
    def __init__(self, name: str) -> None:
        super().__init__(f"'{name}' is not a valid package namespace name")


class InvalidDefineValueException(Exception):
    def __init__(self, defineName: str, defineValue: str | None) -> None:
        super().__init__(f"'{defineValue}' is not a valid define value for {defineName}")
