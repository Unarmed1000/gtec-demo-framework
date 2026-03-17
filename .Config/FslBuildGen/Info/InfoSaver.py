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

import json
from typing import Any

from FslBuildGen import IOUtil

# from FslBuildGen import PackageUtil
# from FslBuildGen.Build.Filter import PackageFilter
from FslBuildGen.Context.GeneratorContext import GeneratorContext
from FslBuildGen.DataTypes import PackageCreationYearString, PackageType
from FslBuildGen.Generator.GeneratorConfig import GeneratorConfig
from FslBuildGen.Generator.Report.Datatypes import FormatStringEnvironmentVariableResolveMethod
from FslBuildGen.Generator.Report.GeneratorExecutableReport import GeneratorExecutableReport
from FslBuildGen.Generator.Report.GeneratorVariableReport import GeneratorVariableReport
from FslBuildGen.Generator.Report.PackageGeneratorReport import PackageGeneratorReport
from FslBuildGen.Generator.Report.VariableReport import VariableReport
from FslBuildGen.Info.AppInfoJson import JsonRootKey
from FslBuildGen.Log import Log
from FslBuildGen.Packages.Package import Package
from FslBuildGen.Packages.PackageRequirement import PackageRequirement

# TODO: this should really generate and save the Info objects instead, but the code was originally written as simple export only code

## Info save code


class JsonRequirement:
    def __init__(self, requirement: PackageRequirement) -> None:
        super().__init__()
        self.Name = requirement.Name
        self.Type = requirement.Type
        if len(requirement.Version) > 0:
            self.Version = requirement.Version
        if len(requirement.Extends) > 0:
            self.Extends = requirement.Extends


class JsonPackageVariableReport:
    def __init__(self, variableReport: VariableReport) -> None:
        super().__init__()
        self.Name: str = variableReport.Name
        self.Options: list[str] = variableReport.Options
        if variableReport.LinkTargetName is not None:
            self.LinkTargetName: str | None = variableReport.LinkTargetName


class JsonPackageGeneratorVariableReport:
    def __init__(self, variableReport: GeneratorVariableReport) -> None:
        super().__init__()
        self.VariableReportList = [JsonPackageVariableReport(report) for report in variableReport.GetVariableReportList()]
        self.DefaultOptions: dict[str, int] = variableReport.SYS_GetDefaultOptions()


class JsonPackageGeneratorExecutableReport:
    def __init__(self, executableReport: GeneratorExecutableReport) -> None:
        super().__init__()
        self.UseAsRelative = executableReport.UseAsRelative
        self.EnvironmentVariableResolveMethod: int = FormatStringEnvironmentVariableResolveMethod.ToInt(executableReport.EnvironmentVariableResolveMethod)
        self.ExeFormatString: str = executableReport.ExeFormatString
        if executableReport.RunScript is not None:
            self.RunScript: str = executableReport.RunScript


class JsonPackageGeneratorReport:
    def __init__(self, package: Package, generatorReport: PackageGeneratorReport) -> None:
        super().__init__()

        if generatorReport.ExecutableReport is not None:
            # We only save the variable report if we have a executable report
            self.VariableReport = JsonPackageGeneratorVariableReport(generatorReport.VariableReport)
            self.ExecutableReport = JsonPackageGeneratorExecutableReport(generatorReport.ExecutableReport)


class JsonPackage:
    def __init__(self, package: Package, packageGeneratorReport: PackageGeneratorReport | None) -> None:
        super().__init__()
        self.SourcePackageName = package.NameInfo.SourceName
        self.Type = PackageType.ToString(package.Type)
        # self.IsVirtual = package.IsVirtual

        # self.PackageLanguage = PackageLanguage.ToString(package.PackageLanguage)
        if package.CreationYear != PackageCreationYearString.NotDefined and package.CreationYear is not None:
            self.CreationYear: str = package.CreationYear
        # self.CompanyName = package.CompanyName

        # resolved
        # self.PlatformName = package.ResolvedPlatformName

        self.AllRequirements = [JsonRequirement(requirement) for requirement in package.ResolvedAllRequirements]
        if not package.ResolvedPlatformSupported:
            self.Supported: bool = package.ResolvedPlatformSupported

        if packageGeneratorReport is not None:
            self.GeneratorReport = JsonPackageGeneratorReport(package, packageGeneratorReport)


class ComplexEncoder(json.JSONEncoder):
    def default(self, o: Any) -> Any:
        if isinstance(
            o,
            (
                JsonPackage,
                JsonRequirement,
                JsonPackageGeneratorReport,
                JsonPackageGeneratorVariableReport,
                JsonPackageVariableReport,
                JsonPackageGeneratorExecutableReport,
            ),
        ):
            return o.__dict__
        # Let the base class default method raise the TypeError
        return json.JSONEncoder.default(self, o)


def ToJsonPackage(package: Package, generatorReportDict: dict[Package, PackageGeneratorReport] | None) -> JsonPackage:
    packageGeneratorReport = None if generatorReportDict is None or package not in generatorReportDict else generatorReportDict[package]
    return JsonPackage(package, packageGeneratorReport)


def SavePackageMetaDataToJson(
    generatorContext: GeneratorContext,
    generatorConfig: GeneratorConfig,
    dstFilePath: str,
    log: Log,
    topLevelPackage: Package,
    packageTypeFilters: list[str],
    includeGeneratorReport: bool,
) -> None:
    resolvedBuildOrder = topLevelPackage.ResolvedBuildOrder

    # Raw package type filtering
    if "*" not in packageTypeFilters:
        log.LogPrint(f"Filtering by PackageType: {packageTypeFilters}")
        resolvedBuildOrder = [package for package in resolvedBuildOrder if PackageType.ToString(package.Type) in packageTypeFilters]

    if resolvedBuildOrder is None:
        log.LogPrint(f"WARNING: No packages left to write to json file '{dstFilePath}'")
        resolvedBuildOrder = []

    generatorReportDict = (
        None if not includeGeneratorReport else generatorContext.Generator.GenerateReport(log, generatorConfig, resolvedBuildOrder).PackageReportDict
    )

    jsonRootDict: dict[str, Any] = {}
    jsonRootDict[JsonRootKey.PlatformName] = topLevelPackage.ResolvedPlatformName
    jsonRootDict[JsonRootKey.ResolvedPackageList] = {package.Name: ToJsonPackage(package, generatorReportDict) for package in resolvedBuildOrder}

    jsonText = str(json.dumps(jsonRootDict, ensure_ascii=False, sort_keys=True, indent=2, cls=ComplexEncoder))
    IOUtil.WriteFileIfChanged(dstFilePath, jsonText)
