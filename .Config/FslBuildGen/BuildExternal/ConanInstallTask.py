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

import os
import subprocess
from typing import final

from FslBuildGen import IOUtil
from FslBuildGen.BuildExternal.ConanProfileBuilder import DETECTED_PROFILE_NAME, ConanProfileBuilder
from FslBuildGen.BuildExternal.Tasks import BasicTask
from FslBuildGen.Context.GeneratorContext import GeneratorContext
from FslBuildGen.DataTypes import BuildVariantConfig
from FslBuildGen.PackageToolFinder import PackageToolFinder
from FslBuildGen.PlatformUtil import PlatformUtil


@final
class ConanInstallTask(BasicTask):
    """
    Runs 'conan install' for a single reference and deploys the package and all its dependencies into a folder together with the
    CMakeDeps find_package config files. The CMakeDeps files reference the deployed files relative to their own location,
    so the folder can be moved into the recipe install location afterwards.
    """

    def __init__(self, generatorContext: GeneratorContext) -> None:
        super().__init__(generatorContext)
        self.CMakeConfig = generatorContext.CMakeConfig
        if not ConanProfileBuilder.IsPlatformSupported(self.CMakeConfig.PlatformName):
            raise Exception(f"Conan recipes are not supported on platform '{self.CMakeConfig.PlatformName}'")
        self.ConanCommand = PlatformUtil.GetExecutableName("conan", self.CMakeConfig.PlatformName)
        self.__HasDetectedProfile = False

    def RunConanInstall(
        self,
        toolFinder: PackageToolFinder,
        reference: str,
        configurationList: list[BuildVariantConfig],
        userOptionList: list[str],
        dstPath: str,
        allowDownloads: bool,
    ) -> None:
        self.LogPrint(f"Running conan install of '{reference}' to '{dstPath}'")
        try:
            buildEnv: dict[str, str] = os.environ.copy()
            if len(toolFinder.ToolPaths) > 0:
                buildEnv["PATH"] = os.pathsep.join([buildEnv.get("PATH", "")] + toolFinder.ToolPaths)

            if ConanProfileBuilder.RequiresDetectedProfile(self.CMakeConfig.PlatformName):
                self.__DetectProfile(buildEnv)

            profilePaths = self.__WriteProfiles(configurationList, dstPath)
            # Tools needed while building from source run on the host, so the release profile is used for them regardless of the configuration
            buildProfilePath = profilePaths.get(BuildVariantConfig.Release, next(iter(profilePaths.values())))
            for config in configurationList:
                self.__Install(reference, profilePaths[config], buildProfilePath, userOptionList, dstPath, allowDownloads, buildEnv)
        except Exception:
            self.LogPrint(f"* A error occurred removing '{dstPath}' to be safe.")
            IOUtil.SafeRemoveDirectoryTree(dstPath, True)
            raise

    def __WriteProfiles(self, configurationList: list[BuildVariantConfig], dstPath: str) -> dict[BuildVariantConfig, str]:
        # The profiles are kept next to the install so its easy to see what a install was build with
        profilePaths: dict[BuildVariantConfig, str] = {}
        for config in configurationList:
            profileText = ConanProfileBuilder.BuildFromCMakeConfig(self.CMakeConfig, config)
            profilePath = IOUtil.Join(dstPath, f"fslbuild_conan_host_{BuildVariantConfig.ToString(config)}.profile")
            IOUtil.WriteFile(profilePath, profileText)
            profilePaths[config] = profilePath
        return profilePaths

    def __Install(
        self,
        reference: str,
        hostProfilePath: str,
        buildProfilePath: str,
        userOptionList: list[str],
        dstPath: str,
        allowDownloads: bool,
        buildEnv: dict[str, str],
    ) -> None:
        command = [
            self.ConanCommand,
            "install",
            f"--requires={reference}",
            "-g",
            "CMakeDeps",
            "--deployer=full_deploy",
            f"--deployer-folder={dstPath}",
            f"--output-folder={dstPath}",
            f"-pr:h={hostProfilePath}",
            f"-pr:b={buildProfilePath}",
            "--build=missing",
        ]
        if not allowDownloads:
            # Only use what is already in the local conan cache
            command.append("--no-remote")
        command += userOptionList
        self.Log.LogPrintVerbose(4, f"Conan command '{command}'")
        result = subprocess.call(command, cwd=dstPath, env=buildEnv)
        if result != 0:
            raise Exception(f"Conan install failed {command}")

    def __DetectProfile(self, buildEnv: dict[str, str]) -> None:
        if self.__HasDetectedProfile:
            return
        command = [self.ConanCommand, "profile", "detect", f"--name={DETECTED_PROFILE_NAME}", "--force"]
        self.Log.LogPrintVerbose(4, f"Conan command '{command}'")
        result = subprocess.call(command, env=buildEnv)
        if result != 0:
            raise Exception(f"Conan profile detect failed {command}")
        self.__HasDetectedProfile = True
