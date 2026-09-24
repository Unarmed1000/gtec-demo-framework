#!/usr/bin/env python3
# ****************************************************************************************************************************************************
# * BSD 3-Clause License
# *
# * Copyright (c) 2026, Mana Battery ApS
# * All rights reserved.
# *
# * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
# *
# * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
# * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the
# *    documentation and/or other materials provided with the distribution.
# * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this
# *    software without specific prior written permission.
# *
# * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
# * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
# * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# ****************************************************************************************************************************************************

import xml.etree.ElementTree as ET
from typing import final

from FslBuildGen.DataTypes import BuildRecipePipelineCommand, BuildVariantConfig
from FslBuildGen.Log import Log
from FslBuildGen.Xml.Exceptions import XmlException2
from FslBuildGen.Xml.XmlExperimentalRecipe import XmlRecipePipelineFetchCommand

CONAN_PACKAGE_NAME = "Recipe.BuildTool.Conan"


@final
class XmlRecipePipelineFetchCommandConanInstall(XmlRecipePipelineFetchCommand):
    """
    Acquires a prebuilt (or Conan built) package and its transitive dependencies from Conan.
    The result is a relocatable folder with CMake find_package config files, so it can only be consumed through the recipe 'Find' support.
    """

    __AttribReference = "Reference"
    __AttribConfiguration = "Configuration"
    __AttribOptions = "Options"

    # CMakeDeps guards every imported property with $<CONFIG:...>, so a configuration that was not installed links nothing
    DEFAULT_CONFIGURATION = "debug;release"

    def __init__(self, log: Log, xmlElement: ET.Element) -> None:
        super().__init__(
            log,
            xmlElement,
            "ConanInstall",
            BuildRecipePipelineCommand.ConanInstall,
            outputPathAllowed=False,
            buildToolPackageName=CONAN_PACKAGE_NAME,
            allowJoinCommandList=False,
        )
        self._CheckAttributes({self.__AttribReference, self.__AttribConfiguration, self.__AttribOptions})
        self.Reference: str = self._ReadAttrib(xmlElement, self.__AttribReference)
        configuration = self._ReadAttrib(xmlElement, self.__AttribConfiguration, self.DEFAULT_CONFIGURATION)
        self.Options: str = self._ReadAttrib(xmlElement, self.__AttribOptions, "")
        if "/" not in self.Reference:
            raise XmlException2(f"ConanInstall Reference '{self.Reference}' is expected to be a Conan reference like 'name/version'")
        self.ConfigurationList = self.__ParseConfiguration(configuration)

    @staticmethod
    def __ParseConfiguration(configuration: str) -> list[BuildVariantConfig]:
        configurationList: list[BuildVariantConfig] = []
        for entry in configuration.split(";"):
            config = BuildVariantConfig.FromString(entry)
            if config not in (BuildVariantConfig.Debug, BuildVariantConfig.Release):
                raise XmlException2(f"ConanInstall only supports the configurations 'debug' and 'release' not '{entry}'")
            if config not in configurationList:
                configurationList.append(config)
        return configurationList
