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

# Which project template the Visual Studio generator makes the projects of a platform from, and what the files of those projects are named.
#
# - The template is the one the project file names for the platform (Project.gen, PlatformTemplate), else its 'DefaultTemplate'.
# - The project and the solution of a package are named after the package: 'Lib.csproj', 'Lib.sln'. A platform whose projects are made
#   next to the ones of another platform has a postfix for the name: the platform 'Unity' writes 'Lib.Unity.csproj' and 'Lib.Unity.sln'
#   and leaves 'Lib.csproj' alone.
# - Such a platform needs a template of its own. The default one would give a second project that keeps its intermediate files where the
#   first project keeps them, and the two would break the build of each other. So a project that names none is told, nothing is generated.

from FslBuildGen.Exceptions import UsageErrorException
from FslBuildGen.Generator.GeneratorVSTemplateInfo import GeneratorVSTemplateInfo
from FslBuildGen.ToolConfig import ToolConfig
from FslBuildGen.Xml.Project.XmlProjectPlatformTemplate import ElementPlatformTemplate


def GetProjectName(packageName: str, projectNamePostfix: str) -> str:
    """The name of the project and the solution of a package, without the extension"""
    return f"{packageName}{projectNamePostfix}"


def HasProject(isPackageSupported: bool, projectNamePostfix: str) -> bool:
    """A platform with projects of its own has them for the packages that are for it. A package that is not for the platform
    (Platform, Supported="false"), or that uses one that is not, gets no project there: the template of the platform need not have a
    project of its kind. The other platforms keep a project for every package.
    """
    return isPackageSupported or len(projectNamePostfix) <= 0


def GetTemplateName(toolConfig: ToolConfig, platformName: str, projectNamePostfix: str) -> str:
    """projectNamePostfix: the postfix of the platform for its project names, empty for a platform that has none"""
    projectRootConfig = toolConfig.ProjectRootConfig
    templateName = projectRootConfig.PlatformTemplates.get(platformName)
    if templateName is not None:
        return templateName
    if len(projectNamePostfix) > 0:
        raise UsageErrorException(
            f"The project names no project template for the platform '{platformName}'. The projects of this platform are made next to the "
            f"ones of the other platforms ('<Package>{projectNamePostfix}.<extension>'), so it needs a template of its own that keeps what "
            f'it builds apart from theirs. Add \'<{ElementPlatformTemplate} Platform="{platformName}" Name="..."/>\' to the Project of '
            f"'{projectRootConfig.SourceFileName}'"
        )
    return projectRootConfig.DefaultTemplate


def CreateTemplateInfo(toolConfig: ToolConfig, sdkConfigTemplatePath: str, platformName: str, projectNamePostfix: str) -> GeneratorVSTemplateInfo:
    return GeneratorVSTemplateInfo(GetTemplateName(toolConfig, platformName, projectNamePostfix), toolConfig.TemplateFolder, sdkConfigTemplatePath)
