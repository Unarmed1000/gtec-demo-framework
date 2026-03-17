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


from FslBuildGen import IOUtil
from FslBuildGen.Log import Log
from FslBuildGen.Vars.VariableProcessor import VariableProcessor
from FslBuildGen.Xml.Project.XmlProjectRootConfigFile import (
    XmlExperimentalDefaultThirdPartyInstallDirectory,
    XmlExperimentalDefaultThirdPartyInstallReadonlyCacheDirectory,
)


class ToolConfigExperimentalDefaultThirdPartyInstallDirectory:
    def __init__(
        self,
        log: Log,
        basedUponXML: XmlExperimentalDefaultThirdPartyInstallDirectory | XmlExperimentalDefaultThirdPartyInstallReadonlyCacheDirectory | None,
        entryName: str,
        isReadonlyCache: bool,
    ) -> None:
        super().__init__()

        if basedUponXML is None:
            raise Exception(f"No '{entryName}' was defined in the xml")

        self.__Log: Log = log
        self.BasedOn = basedUponXML
        self.Name: str = basedUponXML.Name
        self.DynamicName: str = basedUponXML.Name
        self.IsReadonlyCache: bool = isReadonlyCache

        variableProcessor = VariableProcessor(log)

        # NOTE: workaround Union of tuples not being iterable bug in mypy https://github.com/python/mypy/issues/1575
        tupleResult = variableProcessor.TryExtractLeadingEnvironmentVariableNameAndPath(self.DynamicName, False)
        env = tupleResult[0]
        remainingPath = tupleResult[1]
        if env is None:
            raise Exception(f"The {entryName} is expected to contain a environment variable '{self.DynamicName}'")

        resolvedPath = self.__GetEnvironmentVariable(env)

        self.__EnvironmentVariableName: str = env
        self.BashName: str = f"${env}{remainingPath}"
        self.DosName: str = f"%{env}%{remainingPath}"
        self.ResolvedPath: str = IOUtil.ToUnixStylePath(resolvedPath)
        self.ResolvedPathEx: str = f"{self.ResolvedPath}/" if len(self.ResolvedPath) > 0 else ""

    def __GetEnvironmentVariable(self, name: str) -> str:
        # For cache entries we allow the variable to not be defined, but if it is defned we retrieve is as normal
        value = IOUtil.TryGetEnvironmentVariable(name)
        if value is None:
            raise OSError(f"{name} environment variable not set")

        value = IOUtil.NormalizePath(value)
        if value is None:
            raise OSError(f"{name} environment variable not set")

        if not IOUtil.IsAbsolutePath(value):
            raise OSError(f"{name} environment path '{value}' is not absolute")

        if value.endswith("/"):
            raise OSError(f"{name} environment path '{value}' not allowed to end with '/' or ''")

        # Create the directory if it didnt exist
        if not IOUtil.IsDirectory(value) and not IOUtil.Exists(value):
            self.__Log.LogPrint(f"The directory '{value}' did not exist, creating it")
            IOUtil.SafeMakeDirs(value)

        if not IOUtil.IsDirectory(value):
            raise OSError(f"The {name} environment variable content '{value}' does not point to a valid directory")
        return value

    def TryGetEnvironmentVariableName(self) -> str:
        return self.__EnvironmentVariableName
