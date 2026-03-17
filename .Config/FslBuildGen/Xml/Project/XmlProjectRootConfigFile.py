#!/usr/bin/env python3

# ****************************************************************************************************************************************************
# Copyright (c) 2016 Freescale Semiconductor, Inc.
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

# from typing import Union
import os
import os.path
import xml.etree.ElementTree as ET
from typing import Any, Optional

from FslBuildGen import IOUtil
from FslBuildGen.DataTypes import MagicStrings, PackageLanguage
from FslBuildGen.Exceptions import FileNotFoundException
from FslBuildGen.Log import Log
from FslBuildGen.ProjectId import ProjectId
from FslBuildGen.Vars.VariableEnvironment import VariableEnvironment
from FslBuildGen.Vars.VariableProcessor import VariableProcessor
from FslBuildGen.Xml import FakeXmlElementFactory
from FslBuildGen.Xml.Exceptions import XmlException, XmlException2, XmlInvalidRootElement
from FslBuildGen.Xml.Project.XmlBuildDocConfiguration import XmlBuildDocConfiguration
from FslBuildGen.Xml.Project.XmlClangTidyConfiguration import XmlClangTidyConfiguration
from FslBuildGen.Xml.Project.XmlCMakeConfiguration import XmlCMakeConfiguration
from FslBuildGen.Xml.Project.XmlExperimentalPlatform import XmlExperimentalPlatform
from FslBuildGen.Xml.ToolConfig import LoadUtil
from FslBuildGen.Xml.ToolConfig.XmlConfigFileAddNewProjectTemplatesRootDirectory import XmlConfigFileAddNewProjectTemplatesRootDirectory
from FslBuildGen.Xml.ToolConfig.XmlConfigPackageConfiguration import XmlConfigPackageConfiguration
from FslBuildGen.Xml.ToolConfig.XmlConfigPackageLocation import XmlConfigPackageLocation
from FslBuildGen.Xml.XmlBase import XmlBase


class LocalInvalidValues:
    INVALID_FILE_NAME = "**NotDefined**"
    INVALID_COMPANY_NAME = "**INVALID_COMPANY_NAME**"


class XmlConfigFileAddBasePackage(XmlBase):
    __AttribName = "Name"

    def __init__(self, log: Log, xmlElement: ET.Element) -> None:
        super().__init__(log, xmlElement)
        self._CheckAttributes({self.__AttribName})
        self.Name = self._ReadAttrib(xmlElement, self.__AttribName)


class XmlConfigFileAddRootDirectory(XmlBase):
    __AttribName = "Name"
    __AttribCreate = "Create"

    def __init__(self, log: Log, xmlElement: ET.Element, projectId: ProjectId) -> None:
        super().__init__(log, xmlElement)
        self._CheckAttributes({self.__AttribName, self.__AttribCreate})
        self.Name = self._ReadAttrib(xmlElement, self.__AttribName)
        self.Id = self.Name.lower()
        self.Create = self._ReadBoolAttrib(xmlElement, self.__AttribCreate, False)
        self.ProjectId = projectId


class XmlClangFormatConfiguration(XmlBase):
    __AttribFileExtensions = "FileExtensions"
    __AttribRecipe = "Recipe"

    def __init__(self, log: Log, xmlElement: ET.Element) -> None:
        super().__init__(log, xmlElement)
        self._CheckAttributes({self.__AttribFileExtensions, self.__AttribRecipe})
        fileExtensions = self._ReadAttrib(xmlElement, self.__AttribFileExtensions)
        self.FileExtensions = fileExtensions.split(";")
        self.Recipe = self._ReadAttrib(xmlElement, self.__AttribRecipe)


class XmlDotnetFormatConfiguration(XmlBase):
    __AttribFileExtensions = "FileExtensions"
    __AttribRecipe = "Recipe"

    def __init__(self, log: Log, xmlElement: ET.Element) -> None:
        super().__init__(log, xmlElement)
        self._CheckAttributes({self.__AttribFileExtensions, self.__AttribRecipe})
        fileExtensions = self._ReadAttrib(xmlElement, self.__AttribFileExtensions)
        self.FileExtensions = fileExtensions.split(";")
        self.Recipe = self._ReadAttrib(xmlElement, self.__AttribRecipe)


class XmlConfigCompilerConfiguration(XmlBase):
    __AttribName = "Name"
    __AttribPlatform = "Platform"
    __AttribDefaultVersion = "DefaultVersion"

    def __init__(self, log: Log, xmlElement: ET.Element) -> None:
        super().__init__(log, xmlElement)
        self._CheckAttributes({self.__AttribName, self.__AttribPlatform, self.__AttribDefaultVersion})
        self.Name = self._ReadAttrib(xmlElement, self.__AttribName)
        self.Platform = self._ReadAttrib(xmlElement, self.__AttribPlatform)
        self.DefaultVersion = self._ReadAttrib(xmlElement, self.__AttribDefaultVersion)
        self.Id = self.Name.lower()


class XmlExperimentalDefaultThirdPartyInstallDirectory(XmlBase):
    __AttribName = "Name"

    def __init__(self, log: Log, xmlElement: ET.Element) -> None:
        super().__init__(log, xmlElement)
        self._CheckAttributes({self.__AttribName})
        self.Name = self._ReadAttrib(xmlElement, self.__AttribName)


class XmlExperimentalDefaultThirdPartyInstallReadonlyCacheDirectory(XmlBase):
    __AttribName = "Name"

    def __init__(self, log: Log, xmlElement: ET.Element) -> None:
        super().__init__(log, xmlElement)
        self._CheckAttributes({self.__AttribName})
        self.Name = self._ReadAttrib(xmlElement, "Name")


class XmlExperimental(XmlBase):
    __AttribAllowDownloads = "AllowDownloads"
    __AttribDisableDownloadEnv = "DisableDownloadEnv"

    def __init__(self, log: Log, xmlElement: ET.Element) -> None:
        super().__init__(log, xmlElement)
        self._CheckAttributes({self.__AttribAllowDownloads, self.__AttribDisableDownloadEnv})
        self.DefaultThirdPartyInstallDirectory = self.__TryLoadInstallDirectory(log, xmlElement)
        self.DefaultThirdPartyInstallReadonlyCacheDirectory: XmlExperimentalDefaultThirdPartyInstallReadonlyCacheDirectory | None = (
            self.__TryLoadReadonlyCacheDirectory(log, xmlElement)
        )

        self.AllowDownloads = self._ReadBoolAttrib(xmlElement, self.__AttribAllowDownloads)
        self.DisableDownloadEnv = self._ReadAttrib(xmlElement, self.__AttribDisableDownloadEnv)
        self.Platforms: dict[str, XmlExperimentalPlatform] = self.__TryLoadPlatforms(log, xmlElement)

    def TryGetRecipesDefaultValue(self, platformName: str) -> str | None:
        if platformName in self.Platforms:
            platform = self.Platforms[platformName]
            if platform.Recipes is not None:
                return platform.Recipes.Value
        return None

    def __TryLoadInstallDirectory(self, log: Log, xmlElement: ET.Element) -> XmlExperimentalDefaultThirdPartyInstallDirectory | None:
        extendedElement = xmlElement.find("DefaultThirdPartyInstallDirectory")
        if extendedElement is None:
            return None
        return XmlExperimentalDefaultThirdPartyInstallDirectory(log, extendedElement)

    def __TryLoadReadonlyCacheDirectory(self, log: Log, xmlElement: ET.Element) -> XmlExperimentalDefaultThirdPartyInstallReadonlyCacheDirectory | None:
        extendedElement = xmlElement.find("DefaultThirdPartyInstallReadonlyCacheDirectory")
        if extendedElement is None:
            return None
        return XmlExperimentalDefaultThirdPartyInstallReadonlyCacheDirectory(log, extendedElement)

    def __TryLoadPlatforms(self, log: Log, xmlElement: ET.Element) -> dict[str, XmlExperimentalPlatform]:
        platformDict: dict[str, XmlExperimentalPlatform] = {}
        platformElements = xmlElement.findall("Platform")
        if platformElements is not None and len(platformElements) > 0:
            for element in platformElements:
                platform = XmlExperimentalPlatform(log, element)
                if platform.Id in platformDict:
                    errorMsg = f"Multiple platforms called '{platform.Id}' found in Project.gen"
                    raise XmlException2(errorMsg)
                platformDict[platform.Id] = platform
        return platformDict

    def Merge(self, src: Optional["XmlExperimental"]) -> None:
        if src is None:
            return
        if src.DefaultThirdPartyInstallDirectory is not None:
            raise Exception("DefaultThirdPartyInstallDirectory can only be set by the root package")
        if src.DefaultThirdPartyInstallReadonlyCacheDirectory is not None:
            raise Exception("DefaultThirdPartyInstallReadonlyCacheDirectory can only be set by the root package")


def _LoadPackageConfigurations(log: Log, projectElem: ET.Element, filename: str) -> list[XmlConfigPackageConfiguration]:
    xmlPackageConfigurations = LoadUtil.XMLLoadPackageConfiguration(log, projectElem, filename)
    for entry in xmlPackageConfigurations:
        # if no locations has been supplied then we assume the root folder of the project file
        # if entry.Name == 'default' and len(entry.Locations) <= 0:
        if len(entry.Locations) <= 0:
            xmlConfigPackageLocation = XmlConfigPackageLocation(log, FakeXmlElementFactory.CreateWithName("PackageLocation", MagicStrings.ProjectRoot))
            entry.Locations = [xmlConfigPackageLocation]
    return xmlPackageConfigurations


def _LoadAddBasePackage(log: Log, xmlElement: ET.Element, filename: str) -> list[XmlConfigFileAddBasePackage]:
    res = []
    foundElements = xmlElement.findall("AddBasePackage")
    for foundElement in foundElements:
        res.append(XmlConfigFileAddBasePackage(log, foundElement))
    return res


def _LoadAddRootDirectory(log: Log, xmlElement: ET.Element, filename: str, projectId: ProjectId) -> list[XmlConfigFileAddRootDirectory]:
    res = []
    foundElements = xmlElement.findall("AddRootDirectory")
    for foundElement in foundElements:
        res.append(XmlConfigFileAddRootDirectory(log, foundElement, projectId))

    if len(res) < 1:
        raise XmlException(f"The file '{filename}' did not contain at least one AddRootDirectory element")

    return res


def _LoadBuildDocConfiguration(log: Log, xmlElement: ET.Element, filename: str) -> list[XmlBuildDocConfiguration]:
    res = []
    foundElements = xmlElement.findall("BuildDocConfiguration")
    for foundElement in foundElements:
        res.append(XmlBuildDocConfiguration(log, foundElement))

    if len(res) > 1:
        raise XmlException(f"The file '{filename}' contained more than one BuildDocConfiguration")

    return res


def _LoadCMakeConfiguration(log: Log, xmlElement: ET.Element, filename: str) -> list[XmlCMakeConfiguration]:
    res = []
    foundElements = xmlElement.findall("CMakeConfiguration")
    for foundElement in foundElements:
        res.append(XmlCMakeConfiguration(log, foundElement))
    return res


def _LoadClangFormatConfiguration(log: Log, xmlElement: ET.Element, filename: str) -> list[XmlClangFormatConfiguration]:
    res = []
    foundElements = xmlElement.findall("ClangFormatConfiguration")
    for foundElement in foundElements:
        res.append(XmlClangFormatConfiguration(log, foundElement))
    return res


def _LoadDotnetFormatConfiguration(log: Log, xmlElement: ET.Element, filename: str) -> list[XmlDotnetFormatConfiguration]:
    res = []
    foundElements = xmlElement.findall("DotnetFormatConfiguration")
    for foundElement in foundElements:
        res.append(XmlDotnetFormatConfiguration(log, foundElement))
    return res


def _LoadClangTidyConfiguration(log: Log, xmlElement: ET.Element, filename: str) -> list[XmlClangTidyConfiguration]:
    res = []
    foundElements = xmlElement.findall("ClangTidyConfiguration")
    for foundElement in foundElements:
        res.append(XmlClangTidyConfiguration(log, foundElement))
    return res


def _LoadCompilerConfiguration(log: Log, xmlElement: ET.Element, filename: str) -> list[XmlConfigCompilerConfiguration]:
    res = []
    foundElements = xmlElement.findall("CompilerConfiguration")
    for foundElement in foundElements:
        res.append(XmlConfigCompilerConfiguration(log, foundElement))
    return res


def _TryLoadExperimental(log: Log, xmlElement: ET.Element, filename: str) -> XmlExperimental | None:
    extendedElement = xmlElement.find("Experimental")
    if extendedElement is None:
        return None
    return XmlExperimental(log, extendedElement)


class XmlExtendedProject(XmlBase):
    __AttribName = "Name"
    __AttribShortName = "ShortName"
    __AttribVersion = "Version"
    __AttribParent = "Parent"
    __AttribParentRoot = "ParentRoot"

    def __init__(self, log: Log, xmlElement: ET.Element, filename: str) -> None:
        super().__init__(log, xmlElement)
        self._CheckAttributes({self.__AttribName, self.__AttribShortName, self.__AttribVersion, self.__AttribParent, self.__AttribParentRoot})
        # raise Exception("ExtendedProject not implemented");
        self.ProjectName: str = self._ReadAttrib(xmlElement, self.__AttribName)
        self.ShortProjectName: str | None = self._TryReadAttrib(xmlElement, self.__AttribShortName)
        self.ProjectVersion: str = self._ReadAttrib(xmlElement, self.__AttribVersion, "1.0.0.0")
        self.RootDirectory = IOUtil.GetDirectoryName(filename)
        self.Parent: str = self._ReadAttrib(xmlElement, self.__AttribParent)
        self.ParentRoot: str = self._ReadAttrib(xmlElement, self.__AttribParentRoot)
        configFilename: str = IOUtil.GetFileName(filename)
        self.ParentConfigFilename: str = IOUtil.Join(self.ParentRoot, configFilename)
        self.SourceFileName: str = filename

        self.ProjectId = ProjectId(self.ProjectName, self.ShortProjectName)

        variableProcessor = VariableProcessor(log)
        self.AbsoluteParentConfigFilename = variableProcessor.ResolveAbsolutePathWithLeadingEnvironmentVariablePath(self.ParentConfigFilename)
        self.XmlPackageConfiguration: list[XmlConfigPackageConfiguration] = _LoadPackageConfigurations(log, xmlElement, filename)
        self.XmlBasePackages: list[XmlConfigFileAddBasePackage] = _LoadAddBasePackage(log, xmlElement, filename)
        self.XmlRootDirectories: list[XmlConfigFileAddRootDirectory] = _LoadAddRootDirectory(log, xmlElement, filename, self.ProjectId)
        self.XmlNewProjectTemplatesRootDirectories = LoadUtil.LoadAddNewProjectTemplatesRootDirectory(log, xmlElement, filename)
        self.XmlBuildDocConfiguration: list[XmlBuildDocConfiguration] = _LoadBuildDocConfiguration(log, xmlElement, filename)
        self.XmlClangFormatConfiguration: list[XmlClangFormatConfiguration] = _LoadClangFormatConfiguration(log, xmlElement, filename)
        self.XmlClangTidyConfiguration: list[XmlClangTidyConfiguration] = _LoadClangTidyConfiguration(log, xmlElement, filename)
        self.XmlCMakeConfiguration: list[XmlCMakeConfiguration] = _LoadCMakeConfiguration(log, xmlElement, filename)
        self.XmlCompilerConfiguration: list[XmlConfigCompilerConfiguration] = _LoadCompilerConfiguration(log, xmlElement, filename)
        self.XmlExperimental: XmlExperimental | None = _TryLoadExperimental(log, xmlElement, filename)


class XmlProjectRootConfigFile(XmlBase):
    __AttribVersion = "Version"

    def __init__(self, log: Log, filename: str) -> None:
        xmlElement = self.__LoadXml(log, filename)
        super().__init__(log, xmlElement)
        self._CheckAttributes({self.__AttribVersion})

        self.__LoadFromXml(log, xmlElement, filename)
        if self.XmlExperimental is not None and self.XmlExperimental.DefaultThirdPartyInstallDirectory is None:
            raise Exception("DefaultThirdPartyInstallDirectory was not defined")

    def __LoadXml(self, log: Log, filename: str) -> ET.Element:
        """Careful this code must be self contained as it can be used before the parent class is initialized"""
        if filename is None:
            raise Exception("filename can not be None")

        if not os.path.isfile(filename):
            raise FileNotFoundException("Could not locate config file %s", filename)
        tree = ET.parse(filename)
        xmlElement = tree.getroot()
        if xmlElement.tag != "FslBuildGenProjectRoot":
            raise XmlInvalidRootElement("The file did not contain the expected root tag 'FslBuildGenProjectRoot'")
        return xmlElement

    def __LoadFromXml(self, log: Log, xmlElement: ET.Element, filename: str, canExtend: bool = True) -> None:
        self.Version: str = "1"
        self.ProjectName = "not set"
        self.ProjectVersion = "0.0.0.0"
        self.RootDirectory: str = LocalInvalidValues.INVALID_FILE_NAME
        self.DefaultPackageLanguage: PackageLanguage = PackageLanguage.CPP
        self.DefaultCompany: str = LocalInvalidValues.INVALID_COMPANY_NAME
        self.ToolConfigFile: str = LocalInvalidValues.INVALID_FILE_NAME
        self.AllowExeDependency: bool = False
        self.RequirePackageCreationYear = False
        self.XmlExperimental: XmlExperimental | None = None
        self.XmlPackageConfiguration: list[XmlConfigPackageConfiguration] = []
        self.XmlBasePackages: list[XmlConfigFileAddBasePackage] = []
        self.XmlRootDirectories: list[XmlConfigFileAddRootDirectory] = []
        self.XmlNewProjectTemplatesRootDirectories: list[XmlConfigFileAddNewProjectTemplatesRootDirectory] = []
        self.XmlCompilerConfiguration: list[XmlConfigCompilerConfiguration] = []
        self.SourceFileName: str = LocalInvalidValues.INVALID_FILE_NAME
        self.DefaultTemplate: str = MagicStrings.VSDefaultCPPTemplate
        self.ExtendedProject: list[XmlExtendedProject] = []
        if xmlElement is not None:
            extendedElement = xmlElement.find("ExtendedProject") if canExtend else None
            if extendedElement is None:
                rootDirectory = IOUtil.GetDirectoryName(filename)
                variableEnvironment = VariableEnvironment(self.Log)
                variableEnvironment.Set("PROJECT_ROOT", rootDirectory)
                variableProcessor = VariableProcessor(self.Log, variableEnvironment)
                self.Version = self._ReadAttrib(xmlElement, "Version")
                self.RootDirectory = rootDirectory
                projectElem: ET.Element = XmlBase._GetElement(self, xmlElement, "Project")
                self.ProjectName = self._ReadAttrib(projectElem, "Name")
                self.ShortProjectName = self._TryReadAttrib(projectElem, "ShortName")
                self.ProjectId = ProjectId(self.ProjectName, self.ShortProjectName)
                self.ProjectVersion = self._ReadAttrib(projectElem, "Version", "1.0.0.0")
                toolConfigFilePath: str = self._ReadAttrib(projectElem, "ToolConfigFile")
                self.DefaultPackageLanguage = self.__GetDefaultPackageLanguage(projectElem)
                self.DefaultCompany = self._ReadAttrib(projectElem, "DefaultCompany")
                # if this is set to true each package is required to contain a 'CreationYear=""' attribute
                self.RequirePackageCreationYear = self._ReadBoolAttrib(projectElem, "RequirePackageCreationYear", False)
                self.AllowExeDependency = self._ReadBoolAttrib(projectElem, "AllowExeDependency", False)
                self.ToolConfigFile = variableProcessor.ResolvePathToAbsolute(toolConfigFilePath, self.XMLElement)
                self.XmlPackageConfiguration = _LoadPackageConfigurations(log, projectElem, filename)
                self.XmlBasePackages = _LoadAddBasePackage(log, projectElem, filename)
                self.XmlRootDirectories = _LoadAddRootDirectory(log, projectElem, filename, self.ProjectId)
                self.XmlNewProjectTemplatesRootDirectories = LoadUtil.LoadAddNewProjectTemplatesRootDirectory(log, projectElem, filename)
                self.XmlBuildDocConfiguration = _LoadBuildDocConfiguration(log, projectElem, filename)
                self.XmlClangFormatConfiguration = _LoadClangFormatConfiguration(log, projectElem, filename)
                self.XmlDotnetFormatConfiguration = _LoadDotnetFormatConfiguration(log, projectElem, filename)
                self.XmlClangTidyConfiguration = _LoadClangTidyConfiguration(log, projectElem, filename)
                self.XmlCMakeConfiguration = _LoadCMakeConfiguration(log, projectElem, filename)
                self.XmlCompilerConfiguration = _LoadCompilerConfiguration(log, projectElem, filename)
                self.XmlExperimental = _TryLoadExperimental(log, projectElem, filename)
                self.SourceFileName = filename
                self.DefaultTemplate = self._ReadAttrib(projectElem, "DefaultTemplate", MagicStrings.VSDefaultCPPTemplate)
            else:
                # Do something with the extended element
                extendedProject = XmlExtendedProject(log, extendedElement, filename)
                parentFileName = extendedProject.AbsoluteParentConfigFilename
                parentElem = self.__LoadXml(log, parentFileName)
                self.__LoadFromXml(log, parentElem, parentFileName, True)  # True to allow multiple extensions
                self.ExtendedProject.append(extendedProject)
                self.__ApplyExtended(self.XmlPackageConfiguration, extendedProject.XmlPackageConfiguration, True)
                self.__ApplyExtended(self.XmlRootDirectories, extendedProject.XmlRootDirectories, False)
                self.__ApplyExtended(self.XmlNewProjectTemplatesRootDirectories, extendedProject.XmlNewProjectTemplatesRootDirectories, False)
                self.__ApplyExtended(self.XmlBuildDocConfiguration, extendedProject.XmlBuildDocConfiguration, False)
                self.__ApplyExtended(self.XmlClangFormatConfiguration, extendedProject.XmlClangFormatConfiguration, False)
                self.__ApplyExtended(self.XmlCompilerConfiguration, extendedProject.XmlCompilerConfiguration, False)
                self.__ApplyExtendedExperimental(self.XmlExperimental, extendedProject.XmlExperimental)

        if self.RootDirectory == LocalInvalidValues.INVALID_FILE_NAME:
            raise Exception("RootDirectory not configured")
        if self.DefaultCompany == LocalInvalidValues.INVALID_COMPANY_NAME:
            raise Exception("Default company not configured")
        if self.ToolConfigFile == LocalInvalidValues.INVALID_FILE_NAME:
            raise Exception("ToolConfigFile not configured")
        if self.SourceFileName == LocalInvalidValues.INVALID_FILE_NAME:
            raise Exception("SourceFileName not configured")
        if len(self.XmlBuildDocConfiguration) > 1:
            raise Exception("There can only be one BuildDocConfiguration entry")
        if len(self.XmlClangFormatConfiguration) > 1:
            raise Exception("There can only be one ClangFormatConfiguration entry")
        if len(self.XmlClangTidyConfiguration) > 1:
            raise Exception("There can only be one ClangTidyConfiguration entry")
        if len(self.XmlCMakeConfiguration) > 1:
            raise Exception("There can only be one CMakeConfiguration entry")

    def __ApplyExtendedExperimental(self, dst: XmlExperimental | None, src: XmlExperimental | None) -> None:
        if dst is not None:
            dst.Merge(src)

    def __GetDefaultPackageLanguage(self, xmlElement: ET.Element) -> PackageLanguage:
        defaultPackageLanguage = self._ReadAttrib(xmlElement, "DefaultPackageLanguage", "C++")
        return PackageLanguage.FromString(defaultPackageLanguage)

    # TODO: deal with the dynamic types
    def __ApplyExtended(self, dst: Any, src: Any, canMerge: bool) -> None:
        for entry in src:
            parentItem = self.__TryFindById(dst, entry.Id)
            if parentItem is None:
                dst.append(entry)
            elif canMerge:
                parentItem.Merge(entry)
            else:
                raise Exception(f"The extended value can not be merged '{entry.Name}'.")

    # TODO: deal with the dynamic types
    def __TryFindById(self, sourceList: Any, findId: str) -> Any | None:
        for entry in sourceList:
            if entry.Id == findId:
                return entry
        return None
