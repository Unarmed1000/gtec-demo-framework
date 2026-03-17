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


# import xml.etree.ElementTree as ET

from FslBuildGen import IOUtil
from FslBuildGen.Log import Log
from FslBuildGen.ToolConfigExperimentalDefaultThirdPartyInstallDirectory import ToolConfigExperimentalDefaultThirdPartyInstallDirectory
from FslBuildGen.ToolConfigRootDirectory import ToolConfigRootDirectory
from FslBuildGen.Vars.VariableProcessor import VariableProcessor
from FslBuildGen.Xml.Project.XmlProjectRootConfigFile import XmlExperimental, XmlExperimentalDefaultThirdPartyInstallReadonlyCacheDirectory


class ToolConfigExperimental:
    def __init__(
        self, log: Log, rootDirs: list[ToolConfigRootDirectory], basedUponXML: XmlExperimental, configFileName: str, projectRootDirectory: str
    ) -> None:
        super().__init__()
        self.BasedOn = basedUponXML

        self.DefaultThirdPartyInstallReadonlyCacheDirectory: ToolConfigExperimentalDefaultThirdPartyInstallDirectory | None = None

        self.AllowDownloads = basedUponXML.AllowDownloads
        self.DisableDownloadEnv = basedUponXML.DisableDownloadEnv
        self.DefaultThirdPartyInstallDirectory = ToolConfigExperimentalDefaultThirdPartyInstallDirectory(
            log, basedUponXML.DefaultThirdPartyInstallDirectory, "DefaultThirdPartyInstallDirectory", False
        )
        self.DefaultThirdPartyInstallReadonlyCacheDirectory = self.__TryCreateReadonlyCache(log, basedUponXML.DefaultThirdPartyInstallReadonlyCacheDirectory)

        if log.Verbosity > 1:
            log.LogPrint(
                f"DefaultThirdPartyInstallDirectory: {self.DefaultThirdPartyInstallDirectory.Name} which resolves to '{self.DefaultThirdPartyInstallDirectory.ResolvedPath}'"
            )
            if self.DefaultThirdPartyInstallReadonlyCacheDirectory is not None:
                log.LogPrint(
                    f"DefaultThirdPartyInstallReadonlyCacheDirectory: {self.DefaultThirdPartyInstallReadonlyCacheDirectory.Name} which resolves to '{self.DefaultThirdPartyInstallReadonlyCacheDirectory.ResolvedPath}'"
                )

    def __TryCreateReadonlyCache(
        self, log: Log, basedUponXML: XmlExperimentalDefaultThirdPartyInstallReadonlyCacheDirectory | None
    ) -> ToolConfigExperimentalDefaultThirdPartyInstallDirectory | None:
        entryName = "DefaultThirdPartyInstallReadonlyCacheDirectory"
        if basedUponXML is None:
            raise Exception(f"No '{entryName}' was defined in the xml")

        variableProcessor = VariableProcessor(log)
        env = variableProcessor.TryExtractLeadingEnvironmentVariableName(basedUponXML.Name, False)
        if env is None:
            raise Exception(f"The {entryName} is expected to contain a environment variable '{basedUponXML.Name}'")

        resolvedPath = IOUtil.TryGetEnvironmentVariable(env)
        if resolvedPath is None:
            log.LogPrintVerbose(2, f"Read only cache environment variable {env} not set, disabling cache")
            return None

        return ToolConfigExperimentalDefaultThirdPartyInstallDirectory(log, basedUponXML, entryName, True)
