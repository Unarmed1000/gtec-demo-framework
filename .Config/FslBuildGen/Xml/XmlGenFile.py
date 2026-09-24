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

import copy
import hashlib
import os
import xml.etree.ElementTree as ET
from typing import Any, cast

from FslBuildGen import IOUtil, PackageConfig, ToolSharedValues, Util
from FslBuildGen.BuildExternal import ConanRecipeUtil
from FslBuildGen.Config import Config
from FslBuildGen.DataTypes import DependencyOutputType, IncludePriority, PackageCreationYearString, PackageLanguage, PackageString, PackageType
from FslBuildGen.Exceptions import (
    FileNotFoundException,
    PackageMissingRequiredIncludeDirectoryException,
    PackageMissingRequiredSourceDirectoryException,
    UnsupportedException,
    UsageErrorException,
)

# from FslBuildGen.Location.ResolvedPath import ResolvedPath
from FslBuildGen.Log import Log
from FslBuildGen.PackageConfig import APPROVED_PLATFORM_NAMES
from FslBuildGen.PackageFile import PackageFile
from FslBuildGen.PackageIncludeDir import PackageIncludeDir
from FslBuildGen.PackageIncludePath import PackageIncludePath
from FslBuildGen.PackagePath import PackagePath
from FslBuildGen.PackageTemplateLoader import PackageTemplateLoader
from FslBuildGen.ToolConfig import ToolConfig, ToolConfigPackageLocation
from FslBuildGen.Xml import FakeXmlElementFactory
from FslBuildGen.Xml.Exceptions import (
    BuildCustomizationAlreadyDefinedException,
    DefaultValueAlreadyDefinedException,
    PlatformAlreadyDefinedException,
    UnknownBuildCustomizationException,
    UnknownDefaultValueException,
    XmlException2,
    XmlInvalidRootElement,
    XmlUnsupportedPackageType,
    XmlUnsupportedPlatformException,
)
from FslBuildGen.Xml.Flavor.XmlGenFileFlavor import XmlGenFileFlavor
from FslBuildGen.Xml.Flavor.XmlGenFileFlavorExtension import XmlGenFileFlavorExtension
from FslBuildGen.Xml.XmlCommonFslBuild import XmlCommonFslBuild
from FslBuildGen.Xml.XmlExperimentalRecipe import XmlExperimentalRecipe, XmlRecipePipeline
from FslBuildGen.Xml.XmlGenFileCopyFile import XmlGenFileCopyFile
from FslBuildGen.Xml.XmlGenFileDefine import XmlGenFileDefine
from FslBuildGen.Xml.XmlGenFileDependency import XmlGenFileDependency
from FslBuildGen.Xml.XmlGenFileExternalDependency import XmlGenFileExternalDependency
from FslBuildGen.Xml.XmlGenFileFindPackage import FakeXmlGenFileFindPackage
from FslBuildGen.Xml.XmlGenFileGenerate import XmlGenFileGenerate
from FslBuildGen.Xml.XmlGenFileGenerateGrpcProtoFile import XmlGenFileGenerateGrpcProtoFile
from FslBuildGen.Xml.XmlGenFileIgnore import XmlGenFileIgnore
from FslBuildGen.Xml.XmlGenFileRequirement import XmlGenFileRequirement
from FslBuildGen.Xml.XmlGenFileSourceGeneration import XmlGenFileSourceGeneration
from FslBuildGen.Xml.XmlStuff import (
    DefaultValueName,
    LocalPackageDefaultValues,
    XmlGenFileBuildCustomization,
    XmlGenFileBuildCustomization_Optimization,
    XmlGenFileImportTemplate,
    XmlGenFilePlatform,
    XmlGenFileVariant,
)


class XmlGenFile(XmlCommonFslBuild):
    __AttribAllowCheck = "AllowCheck"
    __AttribAllowCombinedDirectory = "AllowCombinedDirectory"
    __AttribCompany = "Company"
    __AttribCreationYear = "CreationYear"
    __AttribEnableExtendedSourceExtensions = "EnableExtendedSourceExtensions"
    __AttribName = "Name"
    __AttribNoInclude = "NoInclude"
    __AttribOverrideInclude = "OverrideInclude"
    __AttribOverrideIncludePriority = "OverrideIncludePriority"
    __AttribOverrideSource = "OverrideSource"
    __AttribPackageNameBasedIncludePath = "PackageNameBasedIncludePath"
    __AttribShowInMainReadme = "ShowInMainReadme"
    __AttribTemplateType = "TemplateType"
    __AttribUnitTest = "UnitTest"

    __ValidAttribs = {
        __AttribAllowCheck,
        __AttribAllowCombinedDirectory,
        __AttribCompany,
        __AttribCreationYear,
        __AttribEnableExtendedSourceExtensions,
        __AttribName,
        __AttribNoInclude,
        __AttribOverrideInclude,
        __AttribOverrideIncludePriority,
        __AttribOverrideSource,
        __AttribPackageNameBasedIncludePath,
        __AttribShowInMainReadme,
        __AttribTemplateType,
        __AttribUnitTest,
    }

    def __init__(self, log: Log, toolConfig: ToolConfig, defaultPackageLanguage: PackageLanguage) -> None:
        super().__init__(log, toolConfig.RequirementTypes, FakeXmlElementFactory.CreateWithName("FakeGenFile", "FSLBUILD_INVALID_INITIAL_VALUE"))
        self.SourceFilename: str | None = None
        self.SourceFileHash: str = ""
        self.Name = ""
        self.ShortName: str | None = None
        self.Namespace: str | None = None
        self.PackageFile: PackageFile | None = None
        self.PackageLocation: ToolConfigPackageLocation | None = None
        self.Type = PackageType.Library
        self.IsVirtual = False
        self.GenerateList: list[XmlGenFileGenerate] = []
        self.GenerateGrpcProtoFileList: list[XmlGenFileGenerateGrpcProtoFile] = []
        self.SourceGeneration: XmlGenFileSourceGeneration | None = None
        self.CopyFileList: list[XmlGenFileCopyFile] = []
        self.DirectDependencies: list[XmlGenFileDependency] = []
        self.DirectRequirements: list[XmlGenFileRequirement] = []
        self.DirectIgnores: list[XmlGenFileIgnore] = []
        self.DirectDefines = []
        self.DirectExperimentalRecipe: XmlExperimentalRecipe | None = None
        self.Platforms: dict[str, XmlGenFilePlatform] = {}
        self.IncludePath: PackageIncludePath | None = None
        self.SourcePath: PackagePath | None = None
        self.ContentPath: PackagePath | None = None
        self.ContentSourcePath: PackagePath | None = None
        self.PackageLanguage = defaultPackageLanguage
        self.BaseIncludePath = PackageIncludeDir("include", IncludePriority.After)
        self.BaseSourcePath = "source"
        self.BuildCustomization: dict[str, XmlGenFileBuildCustomization] = {}
        self.CompanyName = "NotDefined"
        self.CreationYear: str | None = None
        self.TemplateType = ""
        self.AllowCheck = True
        self.EnableExtendedSourceExtensions = False
        self.AllowCombinedDirectory = False
        self.PackageNameBasedIncludePath = True
        self.PlatformDefaultSupportedValue = True
        self.SystemDefaultValues = LocalPackageDefaultValues()
        self.UnitTest = False
        self.ShowInMainReadme = True

    def Load(self, config: Config, packageTemplateLoader: PackageTemplateLoader, packageFile: PackageFile) -> None:
        log: Log = config
        filename = packageFile.AbsoluteFilePath
        if not os.path.isfile(filename):
            raise FileNotFoundException("Could not locate gen file %s", filename)

        configDisableIncludeDirCheck = config.DisableIncludeDirCheck
        configDisableSourceDirCheck = config.DisableSourceDirCheck
        toolConfig = config.ToolConfig

        self.SourceFilename = filename
        self.PackageFile = packageFile
        self.PackageLocation = packageFile.PackageRootLocation

        fileContent = IOUtil.ReadFile(filename)
        self.SourceFileHash = self.__CalcContentHash(fileContent)
        elem = ET.fromstring(fileContent)
        if elem.tag != "FslBuildGen":
            raise XmlInvalidRootElement("The file did not contain the expected root tag 'FslBuildGen'")

        elem, theType = self.__FindPackageElementAndType(elem)

        packageName = self._ReadAttrib(elem, self.__AttribName)
        defaultValues = self.__GetDefaultValues(elem, packageName)
        allowNoInclude = self._ReadBoolAttrib(elem, self.__AttribNoInclude, False)
        companyName = self._ReadAttrib(elem, self.__AttribCompany, toolConfig.DefaultCompany)

        # Used by FslBuildDoc to determine if it should be visible in the main readme
        self.ShowInMainReadme = self._ReadBoolAttrib(elem, self.__AttribShowInMainReadme, True)

        if toolConfig.RequirePackageCreationYear:
            creationYear = self._ReadAttrib(elem, self.__AttribCreationYear)
        else:
            creationYear = self._ReadAttrib(elem, self.__AttribCreationYear, PackageCreationYearString.NotDefined)

        templateType = self._ReadAttrib(elem, self.__AttribTemplateType, "")
        self.AllowCheck = self._ReadBoolAttrib(elem, self.__AttribAllowCheck, True)
        self.UnitTest = self._ReadBoolAttrib(elem, self.__AttribUnitTest, False)
        # if this is set we allow '.cc' files for C++ code.
        self.EnableExtendedSourceExtensions = self._ReadBoolAttrib(elem, self.__AttribEnableExtendedSourceExtensions, False)

        strBaseInclude = self._ReadAttrib(elem, self.__AttribOverrideInclude, "include")
        includePriority = self._ReadIncludePriorityAttrib(elem, self.__AttribOverrideIncludePriority, IncludePriority.After)

        self.BaseIncludePath = PackageIncludeDir(strBaseInclude, includePriority)

        self.BaseSourcePath = self._ReadAttrib(elem, self.__AttribOverrideSource, "source")
        self.AllowCombinedDirectory = self._ReadBoolAttrib(elem, self.__AttribAllowCombinedDirectory, False)
        self.PackageNameBasedIncludePath = self._ReadBoolAttrib(elem, self.__AttribPackageNameBasedIncludePath, True)

        self.BaseLoad(elem)
        self._CheckAttributes(self.__ValidAttribs)

        self.GenerateList = self.__GetGenerateList(log, elem)
        self.GenerateGrpcProtoFileList = self.__GetGenerateGrpcProtoFileList(log, elem)
        self.CopyFileList = self.__GetCopyFileList(log, elem)

        self.SourceGeneration = self.__TryGetSourceGeneration(log, elem, packageName)
        if self.SourceGeneration is not None:
            self.DirectDependencies += self.__CreateSourceGenerationDependencies(self.SourceGeneration, packageName, self.DirectDependencies)

        requirements = self._GetXMLRequirements(elem)
        allowRecipes = self.__DoesTypeAllowRecipes(theType)

        # Add recipe and dependencies
        self.DirectExperimentalRecipe = self._TryGetExperimentalRecipe(elem, packageName, allowRecipes)
        if self.DirectExperimentalRecipe is not None:
            resDD, resED = self.__ProcessExperimentalRecipeDependencies(self.DirectDependencies, [], self.DirectExperimentalRecipe, self.ExternalDependencies)
            self.DirectDependencies += resDD
            self.ExternalDependencies += resED

        platforms = self.__GetXMLPlatforms(toolConfig.RequirementTypes, elem, packageName, self.DirectDependencies, allowRecipes, defaultValues)
        self.BuildCustomization = self.__GetBuildCustomizations(elem, packageName)

        templates = self.__GetXMLImportTemplates(elem)

        # self.__ImportTemplates(packageTemplateLoader, templates, requirements, self.DirectDependencies, self.DirectIgnores, self.ExternalDependencies, self.DirectDefines)
        self.__ImportTemplates(packageTemplateLoader, templates, requirements, self.DirectDependencies, self.ExternalDependencies, self.DirectDefines)

        if self.BaseIncludePath.Name == self.BaseSourcePath and not self.AllowCombinedDirectory:
            raise XmlException2(f"Package '{packageName}' uses the same directory for include and source '{self.BaseIncludePath}'")

        self.XMLElement = elem
        self.Name = packageName
        self.ShortName = None
        self.Namespace = None
        self.SetType(theType)
        self.Platforms = platforms
        self.DirectRequirements = requirements
        self.IncludePath = None
        self.SourcePath = None
        self.CompanyName = companyName
        self.CreationYear = creationYear
        self.TemplateType = templateType
        self.PlatformDefaultSupportedValue = defaultValues.Platform_Supported
        self.SystemDefaultValues = defaultValues

        self._ValidateName(elem, self.Name)
        # This check was moved to the package loader where it belongs
        # self.__ValidateFilename(config, filename)
        self.__ResolveNames(self.Name)
        self.__ValidateBasicDependencyCorrectness()
        self.__ValidateDefines()

        self.__ResolvePaths(configDisableIncludeDirCheck, configDisableSourceDirCheck, packageFile, allowNoInclude, includePriority)
        # FIX: check for clashes with platform addition
        #      check for platform variant name clashes

    def __CalcContentHash(self, content: str) -> str:
        encodedContent = content.encode()
        hashObject = hashlib.sha1(encodedContent)
        return hashObject.hexdigest()

    def __DoesTypeAllowRecipes(self, packageType: PackageType) -> bool:
        return packageType == PackageType.ExternalLibrary or packageType == PackageType.ToolRecipe

    def __FindPackageElementAndType(self, elem: ET.Element) -> tuple[ET.Element, PackageType]:
        currentElem = elem.find("Library")
        if currentElem is not None:
            return (currentElem, PackageType.Library)

        currentElem = elem.find("Executable")
        if currentElem is not None:
            return (currentElem, PackageType.Executable)

        currentElem = elem.find("ExternalLibrary")
        if currentElem is not None:
            return (currentElem, PackageType.ExternalLibrary)

        currentElem = elem.find("HeaderLibrary")
        if currentElem is not None:
            return (currentElem, PackageType.HeaderLibrary)

        currentElem = elem.find("ToolRecipe")
        if currentElem is not None:
            return (currentElem, PackageType.ToolRecipe)

        raise XmlUnsupportedPackageType("Could not locate a Executable, Library, ExternalLibrary or HeaderLibrary element")

    # def SYS_SetName(self, name: str) -> None:
    #     self.Name = name
    #     self.__ResolveNames(name)

    def __ResolveNames(self, name: str) -> None:
        self.ShortName, self.Namespace = Util.GetPackageNames(name)

    def SetType(self, theType: PackageType) -> None:
        self.Type = theType
        self.IsVirtual = (
            theType == PackageType.TopLevel
            or theType == PackageType.ExternalLibrary
            or theType == PackageType.HeaderLibrary
            or theType == PackageType.ToolRecipe
        )

    def _AddPlatform(self, platforms: dict[str, XmlGenFilePlatform], xmlPlatform: XmlGenFilePlatform, resED: list[XmlGenFileExternalDependency]) -> None:
        if xmlPlatform.Name in platforms:
            raise PlatformAlreadyDefinedException(xmlPlatform.XMLElement, xmlPlatform.Name)
        xmlPlatform.ExternalDependencies += resED
        platforms[xmlPlatform.Name] = xmlPlatform

    def __GenerateClones(
        self,
        platformNamesStr: str,
        child: ET.Element,
        defaultValues: LocalPackageDefaultValues,
        requirements: list[XmlGenFileRequirement],
        dependencies: list[XmlGenFileDependency],
        flavors: list[XmlGenFileFlavor],
        flavorExtensions: list[XmlGenFileFlavorExtension],
        variants: list[XmlGenFileVariant],
        experimentalRecipe: XmlExperimentalRecipe | None,
    ) -> list[XmlGenFilePlatform]:
        platformNames: list[str] = platformNamesStr.split(PackageString.PLATFORM_SEPARATOR)

        expandedPlatformList: list[XmlGenFilePlatform] = []
        for name in platformNames:
            if self.Log.Verbosity >= 2:
                self.Log.LogPrint(f"Adding entry for platform '{name}' from entry marked with '{platformNamesStr}'")
            if name not in PackageConfig.APPROVED_PLATFORM_NAMES:
                raise XmlUnsupportedPlatformException(child, f"{name}' from '{platformNamesStr}")

            xmlPlatform = XmlGenFilePlatform(
                self.Log, child, defaultValues, requirements, dependencies, flavors, flavorExtensions, variants, experimentalRecipe
            )
            xmlPlatform.SYS_SetName(name)
            expandedPlatformList.append(xmlPlatform)
        return expandedPlatformList

    def __GetGenerateList(self, log: Log, xmlElement: ET.Element) -> list[XmlGenFileGenerate]:
        res: list[XmlGenFileGenerate] = []
        foundElements = xmlElement.findall("Generate")
        for element in foundElements:
            res.append(XmlGenFileGenerate(log, element))
        return res

    def __GetGenerateGrpcProtoFileList(self, log: Log, xmlElement: ET.Element) -> list[XmlGenFileGenerateGrpcProtoFile]:
        res: list[XmlGenFileGenerateGrpcProtoFile] = []
        foundElements = xmlElement.findall("GenerateGrpcProtoFile")
        for element in foundElements:
            res.append(XmlGenFileGenerateGrpcProtoFile(log, element))
        return res

    def __TryGetSourceGeneration(self, log: Log, xmlElement: ET.Element, packageName: str) -> XmlGenFileSourceGeneration | None:
        foundElements = xmlElement.findall("SourceGeneration")
        if len(foundElements) <= 0:
            return None
        if len(foundElements) > 1:
            raise XmlException2(f"Package '{packageName}' contains more than one SourceGeneration element")

        result = XmlGenFileSourceGeneration(log, foundElements[0])

        # The generated output directory used to be hidden with a <Ignore Path="..."/>, that is now described by the OutputPath attribute
        if result.OutputPath is not None:
            for ignoreEntry in self.DirectIgnores:
                if ignoreEntry.Path == result.OutputPath:
                    raise XmlException2(
                        f"Package '{packageName}' contains a <Ignore Path=\"{ignoreEntry.Path}\"/> that is already described by "
                        f'<SourceGeneration OutputPath="{result.OutputPath}"/>, please remove the Ignore element'
                    )

        if self.PackageLanguage != PackageLanguage.CSharp:
            log.LogPrintWarning(
                f"Package '{packageName}' uses SourceGeneration which is not supported for the package language "
                f"'{PackageLanguage.ToString(self.PackageLanguage)}', it will be ignored"
            )
        return result

    def __CreateSourceGenerationDependencies(
        self, sourceGeneration: XmlGenFileSourceGeneration, packageName: str, existingDependencies: list[XmlGenFileDependency]
    ) -> list[XmlGenFileDependency]:
        """A source generator is just a dependency with a specific output type, so we translate it into one here"""
        existingNames = {entry.Name for entry in existingDependencies}
        res: list[XmlGenFileDependency] = []
        for generator in sourceGeneration.Generators:
            if generator.Name in existingNames:
                raise XmlException2(
                    f"Package '{packageName}' has both a <Generator Name='{generator.Name}'/> and a <Dependency Name='{generator.Name}'/>, "
                    "a package can only be depended upon once"
                )
            self._ValidateName(sourceGeneration.XMLElement, generator.Name)
            res.append(self._CreateFakeXMLDependencies(generator.Name, generator.Access, DependencyOutputType.Analyzer, generator.Reference))
        return res

    def __GetCopyFileList(self, log: Log, xmlElement: ET.Element) -> list[XmlGenFileCopyFile]:
        res: list[XmlGenFileCopyFile] = []
        foundElements = xmlElement.findall("CopyFile")
        for element in foundElements:
            res.append(XmlGenFileCopyFile(log, element))
        return res

    def __GetXMLPlatforms(
        self,
        requirementTypes: list[str],
        elem: ET.Element,
        ownerPackageName: str,
        directDependencies: list[XmlGenFileDependency],
        allowRecipes: bool,
        defaultValues: LocalPackageDefaultValues,
    ) -> dict[str, XmlGenFilePlatform]:
        platforms: dict[str, XmlGenFilePlatform] = {}
        for child in elem:
            if child.tag == "Platform":
                dependencies = self._GetXMLDependencies(child)
                requirements = self._GetXMLRequirements(child)
                flavors = self.__GetXMLFlavors(requirementTypes, child, ownerPackageName)
                flavorExtensions = self.__GetXMLFlavorExtensions(requirementTypes, child, ownerPackageName)
                variants = self.__GetXMLVariants(child, ownerPackageName)
                experimentalRecipe = self._TryGetExperimentalRecipe(child, ownerPackageName, allowRecipes)
                dependencies, resED = self.__ProcessExperimentalRecipeDependencies(directDependencies, dependencies, experimentalRecipe, [])
                xmlPlatform = XmlGenFilePlatform(
                    self.Log, child, defaultValues, requirements, dependencies, flavors, flavorExtensions, variants, experimentalRecipe
                )

                if PackageString.PLATFORM_SEPARATOR in xmlPlatform.Name:
                    xmlPlatforms = self.__GenerateClones(
                        xmlPlatform.Name, child, defaultValues, requirements, dependencies, flavors, flavorExtensions, variants, experimentalRecipe
                    )
                    for clonePlatform in xmlPlatforms:
                        self._AddPlatform(platforms, clonePlatform, resED)
                else:
                    self._AddPlatform(platforms, xmlPlatform, resED)

        # Handle wildcard platforms
        for platformName in APPROVED_PLATFORM_NAMES:
            if platformName not in platforms and PackageString.PLATFORM_WILDCARD in platforms:
                clone = copy.deepcopy(platforms[PackageString.PLATFORM_WILDCARD])
                clone.SYS_SetName(platformName)
                platforms[platformName] = clone

        return platforms

    def _TryGetExperimentalRecipe(self, xmlElement: ET.Element, defaultName: str, allowRecipe: bool) -> XmlExperimentalRecipe | None:
        recipeElementName = "ExperimentalRecipe"
        child = self._TryGetElement(xmlElement, recipeElementName)
        if child is None:
            return None
        if not allowRecipe:
            raise Exception(f"This package type does not allow '{recipeElementName}' elements")
        recipe = XmlExperimentalRecipe(self.Log, child, defaultName)
        ConanRecipeUtil.ValidateRecipe(recipe)
        return recipe

    def __GetPipelineToolDependencyNames(self, pipeline: XmlRecipePipeline | None) -> set[str]:
        dependencies: set[str] = set()
        if pipeline is not None and pipeline.CommandList is not None:
            for command in pipeline.CommandList:
                if command.BuildToolPackageNames is not None:
                    for buildToolPackageName in command.BuildToolPackageNames:
                        dependencies.add(buildToolPackageName)
        return dependencies

    def __ProcessExperimentalRecipeDependencies(
        self,
        directDependencies: list[XmlGenFileDependency],
        dependencies: list[XmlGenFileDependency],
        experimentalRecipe: XmlExperimentalRecipe | None,
        externalDependencies: list[XmlGenFileExternalDependency],
    ) -> tuple[list[XmlGenFileDependency], list[XmlGenFileExternalDependency]]:
        resDirectDependencies = self.__AddExperimentalRecipeDependencies(directDependencies, dependencies, experimentalRecipe)
        resExternalDependencies = externalDependencies
        if experimentalRecipe is not None and experimentalRecipe.Find:
            findVersion = experimentalRecipe.Version if experimentalRecipe.FindVersion is None else experimentalRecipe.FindVersion
            fakeFindPackage = FakeXmlGenFileFindPackage(
                self.Log, experimentalRecipe.ShortName, findVersion, experimentalRecipe.FindTargetName, experimentalRecipe.ExternalInstallDirectory, None
            )
            self.Log.LogPrintVerbose(3, f"* Adding FindPackage(Name='{fakeFindPackage.Name}' If='{fakeFindPackage.IfCondition}')")
            resExternalDependencies = list(externalDependencies)
            resExternalDependencies.append(self.ConvertToXmlGenFileExternalDependency(fakeFindPackage))
        return (resDirectDependencies, resExternalDependencies)

    def __AddExperimentalRecipeDependencies(
        self, directDependencies: list[XmlGenFileDependency], dependencies: list[XmlGenFileDependency], experimentalRecipe: XmlExperimentalRecipe | None
    ) -> list[XmlGenFileDependency]:
        if experimentalRecipe is None:
            return dependencies
        pipelineDependencies = self.__GetPipelineToolDependencyNames(experimentalRecipe.Pipeline)
        if len(pipelineDependencies) <= 0:
            return dependencies

        predefinedUniqueDeps: set[str] = set()
        for entry in directDependencies:
            predefinedUniqueDeps.add(entry.Name)
        for entry in dependencies:
            predefinedUniqueDeps.add(entry.Name)

        combinedDependencies = list(dependencies)
        for dependencyPackageName in pipelineDependencies:
            if dependencyPackageName not in predefinedUniqueDeps:
                combinedDependencies.append(self._CreateFakeXMLDependencies(dependencyPackageName))
        return combinedDependencies

    def __GetDefaultValues(self, elem: ET.Element, ownerPackageName: str) -> LocalPackageDefaultValues:
        defaultValues = LocalPackageDefaultValues()
        customizations: set[str] = set()
        for child in elem:
            key = DefaultValueName.DEFAULT_PLATFORM_Supported
            if child.tag == key:
                defaultValues.Platform_Supported = self._ReadBoolAttrib(child, "Value")
                if key in customizations:
                    raise DefaultValueAlreadyDefinedException(child, key)
                customizations.add(key)
            elif child.tag.startswith("Default."):
                raise UnknownDefaultValueException(child)

        return defaultValues

    def __GetBuildCustomizations(self, elem: ET.Element, ownerPackageName: str) -> dict[str, XmlGenFileBuildCustomization]:
        customizations: dict[str, XmlGenFileBuildCustomization] = {}
        for child in elem:
            if child.tag == "BuildCustomization.Debug.Optimization":
                xmlBuildCustomization = XmlGenFileBuildCustomization_Optimization(self.Log, child)
                if xmlBuildCustomization.Name in customizations:
                    raise BuildCustomizationAlreadyDefinedException(
                        xmlBuildCustomization.XMLElement, xmlBuildCustomization.Name
                    )  # , customizations[xmlBuildCustomization.Name])
                customizations[xmlBuildCustomization.Name] = xmlBuildCustomization
            elif child.tag.startswith("BuildCustomization."):
                raise UnknownBuildCustomizationException(child)

        return customizations

    def __GetXMLImportTemplates(self, elem: ET.Element) -> list[XmlGenFileImportTemplate]:
        elements: list[XmlGenFileImportTemplate] = []
        for child in elem:
            if child.tag == "ImportTemplate":
                elements.append(XmlGenFileImportTemplate(self.Log, child))
        return elements

    def __GetXMLFlavors(self, requirementTypes: list[str], elem: ET.Element, ownerPackageName: str) -> list[XmlGenFileFlavor]:
        elements = []
        for child in elem:
            if child.tag == "Flavor":
                elements.append(XmlGenFileFlavor(self.Log, requirementTypes, child, ownerPackageName))
        return elements

    def __GetXMLFlavorExtensions(self, requirementTypes: list[str], elem: ET.Element, ownerPackageName: str) -> list[XmlGenFileFlavorExtension]:
        elements = []
        for child in elem:
            if child.tag == "FlavorExtension":
                elements.append(XmlGenFileFlavorExtension(self.Log, requirementTypes, child, ownerPackageName))
        return elements

    def __GetXMLVariants(self, elem: ET.Element, ownerPackageName: str) -> list[XmlGenFileVariant]:
        elements = []
        for child in elem:
            if child.tag == "Variant":
                elements.append(XmlGenFileVariant(self.Log, child, ownerPackageName))
        return elements

    def __ImportTemplates(
        self,
        packageTemplateLoader: PackageTemplateLoader,
        templates: list[XmlGenFileImportTemplate],
        requirements: list[XmlGenFileRequirement],
        dependencies: list[XmlGenFileDependency],
        # ignore: List[XmlGenFileIgnore],
        externalDependencies: list[XmlGenFileExternalDependency],
        directDefines: list[XmlGenFileDefine],
    ) -> None:
        for template in templates:
            imported = packageTemplateLoader.Import(template, template.Name)
            for reqEntry in imported.DirectRequirements:
                requirements.append(reqEntry)
            for directEntry in imported.DirectDependencies:
                dependencies.append(directEntry)
            # for ignoreEntries in imported.DirectIgnores:
            #     ignore.append(ignoreEntries)
            for extDepEntry in imported.ExternalDependencies:
                externalDependencies.append(extDepEntry)
            for directDefEntry in imported.DirectDefines:
                directDefines.append(directDefEntry)

    def __ValidateBasicDependencyCorrectness(self) -> None:
        errorMsg = "Dependency defined multiple times '{0}'"
        nameDict: dict[str, XmlGenFileDependency | XmlGenFileExternalDependency | XmlGenFileDefine] = {}
        self.__ValidateNames(nameDict, self.DirectDependencies, errorMsg)
        for platform in list(self.Platforms.values()):
            platformNameDict = copy.copy(nameDict)
            self.__ValidateNames(platformNameDict, platform.DirectDependencies, errorMsg)
            for flavor in platform.Flavors:
                for flavorOption in flavor.Options:
                    self.__ValidateNames(copy.copy(platformNameDict), flavorOption.DirectDependencies, errorMsg)
            for flavorExtension in platform.FlavorExtensions:
                for flavorOption in flavorExtension.Options:
                    self.__ValidateNames(copy.copy(platformNameDict), flavorOption.DirectDependencies, errorMsg)

        errorMsg = "ExternalDependency defined multiple times '{0}'"
        nameDict.clear()
        self.__ValidateNames(nameDict, self.ExternalDependencies, errorMsg)
        for platform in list(self.Platforms.values()):
            platformNameDict = copy.copy(nameDict)
            self.__ValidateNames(platformNameDict, platform.ExternalDependencies, errorMsg)
            for flavor in platform.Flavors:
                for flavorOption in flavor.Options:
                    self.__ValidateNames(copy.copy(platformNameDict), flavorOption.ExternalDependencies, errorMsg)
            for flavorExtension in platform.FlavorExtensions:
                for flavorOption in flavorExtension.Options:
                    self.__ValidateNames(copy.copy(platformNameDict), flavorOption.ExternalDependencies, errorMsg)

    def __ValidateDefines(self) -> None:
        errorMsg = "Define defined multiple times '{0}'"
        nameDict: dict[str, XmlGenFileDependency | XmlGenFileExternalDependency | XmlGenFileDefine] = {}
        self.__ValidateNames(nameDict, self.DirectDefines, errorMsg)
        for platform in list(self.Platforms.values()):
            platformNameDict = copy.copy(nameDict)
            self.__ValidateNames(platformNameDict, platform.DirectDefines, errorMsg)
            for flavor in platform.Flavors:
                for flavorOption in flavor.Options:
                    self.__ValidateNames(copy.copy(platformNameDict), flavorOption.DirectDefines, errorMsg)
            for flavorExtension in platform.FlavorExtensions:
                for flavorOption in flavorExtension.Options:
                    self.__ValidateNames(copy.copy(platformNameDict), flavorOption.DirectDefines, errorMsg)

    def __ValidateNames(
        self,
        rNameDict: dict[str, XmlGenFileDependency | XmlGenFileExternalDependency | XmlGenFileDefine],
        dependencyList: list[XmlGenFileDependency] | list[XmlGenFileExternalDependency] | list[XmlGenFileDefine],
        errorStr: str,
    ) -> None:
        for entry in dependencyList:
            entryId = entry.Name.lower()
            if hasattr(entry, "IfCondition") and cast(Any, entry).IfCondition is not None:
                entryId = f"{entryId}|'{cast(Any, entry).IfCondition}'"
            if entryId not in rNameDict:
                rNameDict[entryId] = entry
            else:
                raise XmlException2(errorStr.format(entry.Name))

    def __ResolvePathIncludeDir(self, configDisableIncludeDirCheck: bool, allowNoInclude: bool, includePriority: IncludePriority) -> None:
        packagePath = self.PackageFile
        if packagePath is None:
            raise Exception("PackageFile can not be None")

        includeDir = PackageIncludeDir(IOUtil.Join(packagePath.AbsoluteDirPath, self.BaseIncludePath.Name), includePriority)
        self.IncludePath = PackageIncludePath(includeDir, packagePath.PackageRootLocation)
        includeDirExist = os.path.isdir(self.IncludePath.AbsoluteDirPath.Name)
        if not includeDirExist and (os.path.exists(self.IncludePath.AbsoluteDirPath.Name) or not (allowNoInclude or configDisableIncludeDirCheck)):
            raise PackageMissingRequiredIncludeDirectoryException(self.IncludePath.AbsoluteDirPath.Name)
        if not includeDirExist and allowNoInclude:
            self.IncludePath = None

    def __ResolvePaths(
        self,
        configDisableIncludeDirCheck: bool,
        configDisableSourceDirCheck: bool,
        packagePath: PackagePath,
        allowNoInclude: bool,
        includePriority: IncludePriority,
    ) -> None:
        rootRelativeDirPath = packagePath.RootRelativeDirPath
        if not self.IsVirtual:
            sourcePath = self.BaseSourcePath
            if self.PackageLanguage == PackageLanguage.CPP:
                self.__ResolvePathIncludeDir(configDisableIncludeDirCheck, allowNoInclude, includePriority)
            elif self.PackageLanguage == PackageLanguage.CSharp:
                # sourcePath = self.Name
                pass
            else:
                raise UnsupportedException(f"Unsupported package language: {self.PackageLanguage}")
            self.SourcePath = PackagePath(IOUtil.Join(rootRelativeDirPath, sourcePath), packagePath.PackageRootLocation)

            self.ContentPath = PackagePath(IOUtil.Join(rootRelativeDirPath, ToolSharedValues.CONTENT_FOLDER_NAME), packagePath.PackageRootLocation)
            self.ContentSourcePath = PackagePath(IOUtil.Join(rootRelativeDirPath, ToolSharedValues.CONTENT_BUILD_FOLDER_NAME), packagePath.PackageRootLocation)

            if not os.path.isdir(self.SourcePath.AbsoluteDirPath) and not configDisableSourceDirCheck:
                raise PackageMissingRequiredSourceDirectoryException(self.SourcePath.AbsoluteDirPath)
        elif self.Type == PackageType.HeaderLibrary:
            if self.PackageLanguage == PackageLanguage.CPP:
                self.__ResolvePathIncludeDir(configDisableIncludeDirCheck, allowNoInclude, includePriority)
            else:
                raise UsageErrorException("HeaderLibrary is only supported for C++")
