#!/usr/bin/env python3

# ****************************************************************************************************************************************************
# Copyright 2020 NXP
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


from FslBuildGen.DataTypes import PackageLanguage, PackageType
from FslBuildGen.Engine.PackageFlavorSelections import PackageFlavorSelections
from FslBuildGen.Engine.Resolver.ProcessedPackage import ProcessedPackage, ProcessedPackageFlags, ProcessedPackagePaths
from FslBuildGen.Engine.Resolver.ProcessedPackageDependency import ProcessedPackageDependency
from FslBuildGen.Engine.Resolver.ResolvedPackageTemplate import ResolvedPackageTemplate
from FslBuildGen.Generator.GeneratorInfo import GeneratorInfo
from FslBuildGen.Log import Log
from FslBuildGen.PackageFile import PackageFile
from FslBuildGen.Packages.CompanyName import CompanyName
from FslBuildGen.Packages.PackageCustomInfo import PackageCustomInfo
from FslBuildGen.Packages.PackageNameInfo import PackageNameInfo
from FslBuildGen.Packages.PackagePlatform import PackagePlatform
from FslBuildGen.Packages.PackageProjectContext import PackageProjectContext
from FslBuildGen.Packages.PackageTraceContext import PackageTraceContext
from FslBuildGen.Packages.Unresolved.UnresolvedExternalDependency import UnresolvedExternalDependency
from FslBuildGen.Packages.Unresolved.UnresolvedFilter import UnresolvedFilter
from FslBuildGen.Packages.Unresolved.UnresolvedPackageCopyFile import UnresolvedPackageCopyFile
from FslBuildGen.Packages.Unresolved.UnresolvedPackageDefine import UnresolvedPackageDefine
from FslBuildGen.Packages.Unresolved.UnresolvedPackageGenerate import UnresolvedPackageGenerate
from FslBuildGen.Packages.Unresolved.UnresolvedPackageGenerateGrpcProtoFile import UnresolvedPackageGenerateGrpcProtoFile
from FslBuildGen.Packages.Unresolved.UnresolvedPackageIgnore import UnresolvedPackageIgnore
from FslBuildGen.Packages.Unresolved.UnresolvedPackageRequirement import UnresolvedPackageRequirement
from FslBuildGen.Xml.XmlExperimentalRecipe import XmlExperimentalRecipe
from FslBuildGen.Xml.XmlStuff import XmlGenFileBuildCustomization


class ProcessedFactory:
    @staticmethod
    def CreatePackage(
        log: Log,
        generatorInfo: GeneratorInfo,
        packageProjectContext: PackageProjectContext,
        nameInfo: PackageNameInfo,
        companyName: CompanyName,
        creationYear: str | None,
        packageFile: PackageFile | None,
        sourceFileHash: str,
        packageType: PackageType,
        packageFlags: ProcessedPackageFlags,
        packageLanguage: PackageLanguage,
        generateList: list[UnresolvedPackageGenerate],
        generateGrpcProtoFileList: list[UnresolvedPackageGenerateGrpcProtoFile],
        copyFileList: list[UnresolvedPackageCopyFile],
        directDependencies: list[ProcessedPackageDependency],
        directRequirements: list[UnresolvedPackageRequirement],
        directDefines: list[UnresolvedPackageDefine],
        directIgnores: list[UnresolvedPackageIgnore],
        externalDependencies: list[UnresolvedExternalDependency],
        path: ProcessedPackagePaths,
        templateType: str,
        buildCustomization: dict[str, XmlGenFileBuildCustomization],
        directExperimentalRecipe: XmlExperimentalRecipe | None,
        resolvedFlavorSelections: PackageFlavorSelections,
        resolvedFlavorTemplate: ResolvedPackageTemplate,
        resolvedPlatform: PackagePlatform,
        directPlatformSupported: bool,
        customInfo: PackageCustomInfo,
        traceContext: PackageTraceContext,
    ) -> ProcessedPackage:
        # filter based on conditions
        externalDependencies = UnresolvedFilter.FilterOnConditions(log, generatorInfo, externalDependencies, "ExternalDependency")
        directDependencies = UnresolvedFilter.FilterOnConditions(log, generatorInfo, directDependencies, "Dependency")

        return ProcessedPackage(
            packageProjectContext,
            nameInfo,
            companyName,
            creationYear,
            packageFile,
            sourceFileHash,
            packageType,
            packageFlags,
            packageLanguage,
            generateList,
            generateGrpcProtoFileList,
            copyFileList,
            directDependencies,
            directRequirements,
            directDefines,
            directIgnores,
            externalDependencies,
            path,
            templateType,
            buildCustomization,
            directExperimentalRecipe,
            resolvedFlavorSelections,
            resolvedFlavorTemplate,
            resolvedPlatform,
            directPlatformSupported,
            customInfo,
            traceContext,
        )
