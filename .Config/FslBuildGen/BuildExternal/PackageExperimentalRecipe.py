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


from FslBuildGen.BuildExternal.DataTypes import RecipeType
from FslBuildGen.BuildExternal.PackageRecipeInstallationUtil import PackageRecipeInstallationUtil
from FslBuildGen.DataTypes import BuildRecipePipelineCommand
from FslBuildGen.Location.PathBuilder import PathBuilder
from FslBuildGen.Location.ResolvedPath import ResolvedPath
from FslBuildGen.Log import Log
from FslBuildGen.Version import Version
from FslBuildGen.Xml.XmlExperimentalRecipe import XmlExperimentalRecipe


class PackageExperimentalRecipe:
    def __init__(self, log: Log, packageName: str, pathBuilder: PathBuilder, xmlExperimentalRecipe: XmlExperimentalRecipe, forceDisable: bool) -> None:
        """
        forceDisable will not disable 'the external' recipe type used for build tools
        """
        self._Log = log
        self.SysXmlSource = xmlExperimentalRecipe
        self.Version: Version | None = xmlExperimentalRecipe.Version
        self.ShortName = xmlExperimentalRecipe.ShortName
        self.FullName = xmlExperimentalRecipe.FullName
        self.AllowFind = xmlExperimentalRecipe.Find

        determinedType = self.__DetermineRecipeType(xmlExperimentalRecipe)
        self.Type = determinedType if not forceDisable or determinedType == RecipeType.External else RecipeType.Disabled
        self.Pipeline = xmlExperimentalRecipe.Pipeline if self.Type == RecipeType.Build else None

        self.ValidateInstallation = (
            PackageRecipeInstallationUtil.ToPackageRecipeInstallation(pathBuilder, xmlExperimentalRecipe.ValidateInstallation)
            if self.Type != RecipeType.Disabled and xmlExperimentalRecipe.ValidateInstallation is not None
            else None
        )
        self.IsLocalSourceBuild = False
        if self.Pipeline is not None and len(self.Pipeline.CommandList) > 0 and self.Pipeline.CommandList[0].CommandType == BuildRecipePipelineCommand.Source:
            self.IsLocalSourceBuild = True

        # The installation path of the package
        self.ResolvedInstallLocation: ResolvedPath | None = None

        if self.Type == RecipeType.Undefined:
            log.DoPrintWarning(f"No installation or validation available for package '{packageName}' recipe '{self.FullName}'")
        if self.ValidateInstallation is None and self.Type != RecipeType.Disabled:
            log.DoPrintWarning(f"No installation validation available for package '{packageName}' recipe '{self.FullName}'")
        if forceDisable and log.Verbosity >= 4:
            if self.Type == RecipeType.Disabled:
                log.LogPrint(f"  - Force disabling recipe for package {packageName}")
            else:
                log.LogPrint(f"  - Force disabling recipe for package {packageName} ignored due to type")

    def __DetermineRecipeType(self, xmlExperimentalRecipe: XmlExperimentalRecipe) -> int:
        if xmlExperimentalRecipe.ExternalInstallDirectory is not None:
            return RecipeType.ExternalInstallation
        elif xmlExperimentalRecipe.Pipeline is not None:
            return RecipeType.Build
        elif xmlExperimentalRecipe.ValidateInstallation is not None:
            return RecipeType.External
        return RecipeType.Undefined
        # raise Exception("Could not determine recipe type of recipe '{0}'".format(xmlExperimentalRecipe.Name))
