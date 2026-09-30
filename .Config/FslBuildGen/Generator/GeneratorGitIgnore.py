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


from FslBuildGen import IOUtil
from FslBuildGen.DataTypes import PackageType
from FslBuildGen.Generator import GitIgnoreMerge
from FslBuildGen.Generator.GeneratorBase import GeneratorBase
from FslBuildGen.Generator.GitIgnoreText import GitIgnoreText, TryReadGitIgnoreContent, TryReadGitIgnoreText, WriteGitIgnoreTextIfChanged
from FslBuildGen.Packages.Package import Package


class GeneratorGitIgnore(GeneratorBase):
    def __init__(
        self, configSDKConfigTemplatePath: str, configDisableWrite: bool, packages: list[Package], platformName: str, activeGenerator: GeneratorBase
    ) -> None:
        super().__init__()

        virtualTemplate = TryReadGitIgnoreContent(IOUtil.Join(configSDKConfigTemplatePath, "Template_gitignore_virtual.txt"))
        headerLibTemplate = TryReadGitIgnoreContent(IOUtil.Join(configSDKConfigTemplatePath, "Template_gitignore_headerlib.txt"))
        libTemplate = TryReadGitIgnoreContent(IOUtil.Join(configSDKConfigTemplatePath, "Template_gitignore_lib.txt"))
        exeTemplate = TryReadGitIgnoreContent(IOUtil.Join(configSDKConfigTemplatePath, "Template_gitignore_exe.txt"))

        generatorIgnoreDict = activeGenerator.GetPackageGitIgnoreDict()

        for package in packages:
            if package.Type == PackageType.Library:
                self.__GenerateLibraryBuildFile(configDisableWrite, package, platformName, libTemplate, generatorIgnoreDict)
            elif package.Type == PackageType.Executable:
                self.__GenerateLibraryBuildFile(configDisableWrite, package, platformName, exeTemplate, generatorIgnoreDict)
            elif package.Type == PackageType.HeaderLibrary:
                self.__GenerateLibraryBuildFile(configDisableWrite, package, platformName, headerLibTemplate, generatorIgnoreDict)
            else:
                self.__GenerateLibraryBuildFile(configDisableWrite, package, platformName, virtualTemplate, generatorIgnoreDict)

    def __GenerateLibraryBuildFile(
        self, configDisableWrite: bool, package: Package, platformName: str, template: str | None, generatorIgnoreDict: dict[str, set[str]]
    ) -> None:
        if template is None or package.AbsolutePath is None:
            return
        template = template.replace("##PROJECT_NAME##", package.Name)
        targetFilePath = IOUtil.Join(package.AbsolutePath, ".gitignore")

        # Read like git reads it, a BOM and bytes that are not valid UTF-8 are written back as they were
        existingText = TryReadGitIgnoreText(targetFilePath)
        existingLines = GitIgnoreMerge.SplitLines(existingText.Content if existingText is not None else None)
        templateLines = GitIgnoreMerge.SplitLines(template) or []
        # Allow each generator to add things that should be ignored
        generatorEntries = generatorIgnoreDict.get(package.Name, set())

        # A hand arranged file keeps its lines and their order (git reads them top to bottom), a sorted file stays sorted
        mergedLines = GitIgnoreMerge.MergeGitIgnoreLines(existingLines, templateLines, generatorEntries)
        if mergedLines is None or configDisableWrite:
            return
        hasBom = existingText is not None and existingText.HasBom
        WriteGitIgnoreTextIfChanged(targetFilePath, GitIgnoreText(GitIgnoreMerge.JoinLines(mergedLines), hasBom))
