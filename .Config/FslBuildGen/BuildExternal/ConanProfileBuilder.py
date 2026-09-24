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

import platform
from typing import final

from FslBuildGen.DataTypes import BuildVariantConfig
from FslBuildGen.Generator.GeneratorCMakeConfig import GeneratorCMakeConfig
from FslBuildGen.PackageConfig import PlatformNameString

# The name of the profile 'conan profile detect' is asked to create, it is used as the base for the non windows profiles
DETECTED_PROFILE_NAME = "fslbuildgen_detect"

# Matches the 'cxx_std_20' used by Templates.gen/CMake/Snippet_TargetCompileFeatures_*.txt.
# Leaving cppstd out makes Conan miss the prebuilt binaries, as they are all built with a cppstd set.
CPP_STANDARD = "20"

# Visual studio platform toolset version -> Conan 'msvc' compiler.version
g_msvcToolsetToCompilerVersion = {
    "140": "190",
    "141": "191",
    "142": "192",
    "143": "194",
    "145": "195",
}

# python platform.machine() -> Conan arch
g_machineToArch = {
    "x86_64": "x86_64",
    "amd64": "x86_64",
    "aarch64": "armv8",
    "arm64": "armv8",
}


@final
class ConanProfileBuilder:
    @staticmethod
    def IsPlatformSupported(platformName: str) -> bool:
        return platformName in (PlatformNameString.WINDOWS, PlatformNameString.UBUNTU)

    @staticmethod
    def RequiresDetectedProfile(platformName: str) -> bool:
        return platformName != PlatformNameString.WINDOWS

    @staticmethod
    def BuildFromCMakeConfig(cmakeConfig: GeneratorCMakeConfig, config: BuildVariantConfig) -> str:
        return ConanProfileBuilder.Build(
            cmakeConfig.PlatformName, cmakeConfig.CMakeFinalGeneratorName, cmakeConfig.VsToolsetVersionStr, platform.machine(), config
        )

    @staticmethod
    def Build(platformName: str, cmakeGeneratorName: str, vsToolsetVersion: str, machine: str, config: BuildVariantConfig) -> str:
        """
        Build the text of a Conan host profile that matches what FslBuildGen uses to build everything else
        - platformName: the FslBuildGen platform name
        - cmakeGeneratorName: the final cmake generator name
        - vsToolsetVersion: the visual studio platform toolset version (143, 145, ...). Only used on windows
        - machine: the python platform.machine() string. Not used on windows as we always build x64 there
        """
        buildType = ConanProfileBuilder.__ToBuildType(config)
        if platformName == PlatformNameString.WINDOWS:
            return ConanProfileBuilder.__BuildWindows(cmakeGeneratorName, vsToolsetVersion, buildType)
        if platformName == PlatformNameString.UBUNTU:
            return ConanProfileBuilder.__BuildLinux(cmakeGeneratorName, machine, buildType)
        raise Exception(f"Conan recipes are not supported on platform '{platformName}'")

    @staticmethod
    def __BuildWindows(cmakeGeneratorName: str, vsToolsetVersion: str, buildType: str) -> str:
        compilerVersion = g_msvcToolsetToCompilerVersion.get(vsToolsetVersion)
        if compilerVersion is None:
            raise Exception(
                f"Conan recipes do not know the msvc compiler version for the visual studio toolset '{vsToolsetVersion}', known toolsets: {list(g_msvcToolsetToCompilerVersion)}"
            )
        lines = [
            "[settings]",
            "os=Windows",
            # For now we always build x64 on windows, see CMakeHelper.DetermineGeneratorArguments
            "arch=x86_64",
            "compiler=msvc",
            f"compiler.version={compilerVersion}",
            f"compiler.cppstd={CPP_STANDARD}",
            "compiler.runtime=dynamic",
            f"compiler.runtime_type={buildType}",
            f"build_type={buildType}",
        ]
        lines += ConanProfileBuilder.__BuildConf(cmakeGeneratorName)
        return ConanProfileBuilder.__Join(lines)

    @staticmethod
    def __BuildLinux(cmakeGeneratorName: str, machine: str, buildType: str) -> str:
        arch = g_machineToArch.get(machine.lower())
        if arch is None:
            raise Exception(f"Conan recipes do not know the Conan arch for the machine '{machine}', known machines: {list(g_machineToArch)}")
        # The compiler is taken from the detected profile, 'conan profile detect' honors CC/CXX the same way cmake does
        lines = [
            f"include({DETECTED_PROFILE_NAME})",
            "",
            "[settings]",
            "os=Linux",
            f"arch={arch}",
            f"compiler.cppstd={CPP_STANDARD}",
            f"build_type={buildType}",
        ]
        lines += ConanProfileBuilder.__BuildConf(cmakeGeneratorName)
        return ConanProfileBuilder.__Join(lines)

    @staticmethod
    def __BuildConf(cmakeGeneratorName: str) -> list[str]:
        # Make packages that Conan has to build from source use the same generator as we do
        return ["", "[conf]", f"tools.cmake.cmaketoolchain:generator={cmakeGeneratorName}"]

    @staticmethod
    def __ToBuildType(config: BuildVariantConfig) -> str:
        if config == BuildVariantConfig.Debug:
            return "Debug"
        if config == BuildVariantConfig.Release:
            return "Release"
        raise Exception(f"Conan recipes do not support the build variant '{BuildVariantConfig.ToString(config)}'")

    @staticmethod
    def __Join(lines: list[str]) -> str:
        return "\n".join(lines) + "\n"
