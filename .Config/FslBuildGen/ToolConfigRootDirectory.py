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
from FslBuildGen.ProjectId import ProjectId
from FslBuildGen.Vars.VariableProcessor import VariableProcessor
from FslBuildGen.Xml.Project.XmlProjectRootConfigFile import XmlConfigFileAddRootDirectory


class ToolConfigRootDirectory:
    def __init__(
        self,
        log: Log,
        basedUponXML: XmlConfigFileAddRootDirectory | None,
        projectId: ProjectId,
        dynamicSourceRootDir: XmlConfigFileAddRootDirectory | None | ToolConfigRootDirectory | None = None,
        dynamicRootName: str | None = None,
        dynamicPath: str | None = None,
    ) -> None:
        super().__init__()
        dirMustExist = True
        self.ProjectId = projectId
        if basedUponXML is not None:
            self.BasedOn: XmlConfigFileAddRootDirectory | ToolConfigRootDirectory = basedUponXML
            self.Name: str = basedUponXML.Name
            self.DynamicName: str = basedUponXML.Name
            dirMustExist = not basedUponXML.Create
        else:
            if dynamicSourceRootDir is None:
                raise Exception("dynamicSourceRootDir can not be none")
            if dynamicRootName is None:
                raise Exception("dynamicRootName can not be none")
            if dynamicPath is None:
                raise Exception("dynamicPath can not be none")
            self.BasedOn = dynamicSourceRootDir
            self.Name = dynamicRootName
            self.DynamicName = dynamicPath
        variableProcessor = VariableProcessor(log)
        # NOTE: workaround Union of tuples not being iterable bug in mypy https://github.com/python/mypy/issues/1575
        tupleResult = variableProcessor.TryExtractLeadingEnvironmentVariableNameAndPath(self.DynamicName, dynamicSourceRootDir is not None)
        env = tupleResult[0]
        remainingPath = tupleResult[1]
        if env is None:
            raise Exception(f"Root dirs are expected to contain environment variables '{self.DynamicName}'")
        remainingPath = remainingPath if remainingPath is not None else ""

        resolvedPath = IOUtil.GetEnvironmentVariableForDirectory(env, dirMustExist)
        if not IOUtil.Exists(resolvedPath):
            IOUtil.SafeMakeDirs(resolvedPath)
        if not IOUtil.IsDirectory(resolvedPath):
            raise OSError(f"The {env} environment variable content '{resolvedPath}' does not point to a valid directory")

        resolvedPath = resolvedPath + remainingPath
        self.BashName: str = f"${env}{remainingPath}"
        self.DosName: str = f"%{env}%{remainingPath}"
        self.ResolvedPath: str = IOUtil.ToUnixStylePath(resolvedPath)
        self.ResolvedPathEx: str = f"{self.ResolvedPath}/" if len(self.ResolvedPath) > 0 else ""
        self.__EnvironmentVariableName: str = env

    def TryGetEnvironmentVariableName(self) -> str:
        return self.__EnvironmentVariableName

    def GetEnvironmentVariableName(self) -> str:
        return self.__EnvironmentVariableName
