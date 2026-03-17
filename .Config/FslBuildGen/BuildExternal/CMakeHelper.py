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


from FslBuildGen import IOUtil, PackageConfig
from FslBuildGen.AndroidUtil import AndroidUtil
from FslBuildGen.BuildConfig.BuildVariables import BuildVariables
from FslBuildGen.BuildConfig.UserSetVariables import UserSetVariables
from FslBuildGen.BuildExternal.CMakeTypes import CMakeGeneratorMultiConfigCapability, CMakeGeneratorName
from FslBuildGen.DataTypes import BuildPlatformType
from FslBuildGen.Log import Log
from FslBuildGen.PackageConfig import PlatformNameString
from FslBuildGen.PlatformUtil import PlatformUtil


class CMakeEmscriptenConfig:
    def __init__(self, configureCommand: str, buildCommand: str, runCommand: str) -> None:
        super().__init__()
        self.ConfigureCommand = configureCommand
        self.BuildCommand = buildCommand
        self.RunCommand = runCommand


def DetermineCMakeCommand(platformName: str) -> str:
    return PlatformUtil.GetExecutableName("cmake", platformName)


def DetermineEmscriptenCommands() -> CMakeEmscriptenConfig:
    scriptName = "emcmake"
    buildScriptName = "emmake"
    runName = "emrun"
    if PlatformUtil.DetectBuildPlatformType() == BuildPlatformType.Windows:
        return CMakeEmscriptenConfig(scriptName + ".bat", buildScriptName + ".bat", runName + ".bat")
    return CMakeEmscriptenConfig(scriptName, buildScriptName, runName)


def TryGetPlatformDefaultCMakeGenerator(platformName: str, compilerVersion: int) -> str | None:
    if (
        platformName == PackageConfig.PlatformNameString.UBUNTU
        or platformName == PackageConfig.PlatformNameString.APPLE
        or platformName == PackageConfig.PlatformNameString.YOCTO
        or platformName == PackageConfig.PlatformNameString.RDK_YOCTO
        or platformName == PackageConfig.PlatformNameString.FREERTOS
        or platformName == PackageConfig.PlatformNameString.QNX
    ):
        return CMakeGeneratorName.Ninja
    elif platformName == PackageConfig.PlatformNameString.WINDOWS:
        return CMakeGeneratorName.FromVisualStudioVersion(compilerVersion)
    elif platformName == PackageConfig.PlatformNameString.ANDROID:
        return CMakeGeneratorName.Android
    elif platformName == PackageConfig.PlatformNameString.EMSCRIPTEN:
        return CMakeGeneratorName.Ninja
    return None

    # if generator.Name == PackageConfig.PlatformNameString.ANDROID:
    #    return "NotSupported"
    # elif generator.Name == PackageConfig.PlatformNameString.YOCTO:
    #    return "NotSupported"
    # elif generator.Name == PackageConfig.PlatformNameString.QNX:
    #    return "NotSupported"


def DetermineFinalCMakeGenerator(generatorName: str) -> str:
    if generatorName != CMakeGeneratorName.Android:
        return generatorName
    if PlatformUtil.DetectBuildPlatformType() == BuildPlatformType.Windows:
        return CMakeGeneratorName.Ninja
    return CMakeGeneratorName.UnixMakeFile


def GetPlatformDefaultCMakeGenerator(platformName: str, compilerVersion: int) -> str:
    result = TryGetPlatformDefaultCMakeGenerator(platformName, compilerVersion)
    if result is not None:
        return result
    raise Exception(f"CMake generator name could not be determined for this platform '{platformName}")


def GetCompilerShortIdFromGeneratorName(generatorName: str) -> str:
    if generatorName == CMakeGeneratorName.UnixMakeFile:
        return "Make"
    elif generatorName == CMakeGeneratorName.VisualStudio2015_X64:
        return "VS2015_X64"
    elif generatorName == CMakeGeneratorName.VisualStudio2017_X64:
        return "VS2017_X64"
    elif generatorName == CMakeGeneratorName.VisualStudio2019_X64:
        return "VS2019_X64"
    elif generatorName == CMakeGeneratorName.VisualStudio2022_X64:
        return "VS2022_X64"
    elif generatorName == CMakeGeneratorName.VisualStudio2026_X64:
        return "VS2026_X64"
    elif generatorName == CMakeGeneratorName.Android:
        # For android we utilize a combination of the SDK and NDK version for the unique 'toolchain' name
        theId = AndroidUtil.GetSDKNDKId()
        sdkVersion = AndroidUtil.GetSDKVersion()
        return f"V{sdkVersion}{theId}"

    generatorName = generatorName.strip()
    generatorName = generatorName.replace(" ", "_")
    return generatorName


def DeterminePlatformArguments(platformName: str) -> list[str]:
    res: list[str] = []
    if platformName != PackageConfig.PlatformNameString.ANDROID:
        return res

    androidToolchain = "clang"
    androidStlType = "c++_shared"
    if not AndroidUtil.UseNDKCMakeToolchain():
        # NDK before 19
        res.append("-DCMAKE_SYSTEM_NAME=Android")
        res.append(f"-DCMAKE_ANDROID_NDK_TOOLCHAIN_VERSION={androidToolchain}")
        res.append(f"-DCMAKE_ANDROID_STL_TYPE={androidStlType}")
    else:
        # NDK from 19
        res.append("-DCMAKE_TOOLCHAIN_FILE={}".format(IOUtil.Join(AndroidUtil.GetNDKPath(), "build/cmake/android.toolchain.cmake")))
        res.append(f"-DANDROID_STL={androidStlType}")
        res.append(f"-DANDROID_TOOLCHAIN={androidToolchain}")

    return res


def DetermineGeneratorArguments(cmakeGeneratorName: str, platformName: str) -> list[str]:
    res = DeterminePlatformArguments(platformName)
    if (
        cmakeGeneratorName != CMakeGeneratorName.VisualStudio2019_X64
        and cmakeGeneratorName != CMakeGeneratorName.VisualStudio2022_X64
        and cmakeGeneratorName != CMakeGeneratorName.VisualStudio2026_X64
    ):
        return res
    # for now we always use x64
    res.append("-A")
    res.append("x64")
    return res


def DetermineVSToolsetVersion(log: Log, cmakeGeneratorName: str, platformName: str, userSetVariables: UserSetVariables) -> str:
    buildVariableName = BuildVariables.VS_TOOLSET_VERSION
    userToolsetVersion = userSetVariables.TryGet(buildVariableName)
    if userToolsetVersion is not None:
        log.LogPrint(f"DetermineVSToolsetVersion using user set '{buildVariableName}={userToolsetVersion}'")
        return userToolsetVersion

    if not CMakeGeneratorName.IsVisualStudio(cmakeGeneratorName):
        if platformName == PlatformNameString.WINDOWS:
            log.LogPrintVerbose(
                2,
                f"Not a visual studio build so do not know the VSToolsetVersion, a best guess will be made but if necessary set it manually with '--set {buildVariableName}=version'",
            )
            envVersion = IOUtil.TryGetEnvironmentVariable("VCToolsVersion")
            if envVersion is not None:
                envVersion = envVersion.strip()
                res = envVersion.split(".")
                if len(res) >= 2 and len(res[0]) >= 2 and len(res[1]) > 0:
                    bestGuess = res[0] + res[1][0]
                    log.LogPrintVerbose(
                        2, f"The VSToolsetVersion is likely to be '{bestGuess}' based on the environment variable VCToolsVersion being '{envVersion}'"
                    )
                    return bestGuess
        return "ErrorUnknownToolsetVersion"
    return CMakeGeneratorName.GetToolsetVersionString(cmakeGeneratorName)


def GetNativeBuildThreadArguments(cmakeGeneratorName: str, numBuildThreads: int) -> list[str]:
    if cmakeGeneratorName == CMakeGeneratorName.UnixMakeFile or cmakeGeneratorName == CMakeGeneratorName.Ninja:
        return ["-j", str(numBuildThreads)]
    elif (
        cmakeGeneratorName == CMakeGeneratorName.VisualStudio2015_X64
        or cmakeGeneratorName == CMakeGeneratorName.VisualStudio2017_X64
        or cmakeGeneratorName == CMakeGeneratorName.VisualStudio2019_X64
        or cmakeGeneratorName == CMakeGeneratorName.VisualStudio2022_X64
        or cmakeGeneratorName == CMakeGeneratorName.VisualStudio2026_X64
    ):
        return [f"/maxcpucount:{numBuildThreads}"]
    return []


def GetGeneratorMultiConfigCapabilities(cmakeGeneratorName: str) -> CMakeGeneratorMultiConfigCapability:
    """
    Hardcode some knowledge about certain generators to avoid cmake warnings
    """
    if cmakeGeneratorName == CMakeGeneratorName.UnixMakeFile or cmakeGeneratorName == CMakeGeneratorName.Ninja:
        return CMakeGeneratorMultiConfigCapability.No
    elif (
        cmakeGeneratorName == CMakeGeneratorName.VisualStudio2015_X64
        or cmakeGeneratorName == CMakeGeneratorName.VisualStudio2017_X64
        or cmakeGeneratorName == CMakeGeneratorName.VisualStudio2019_X64
        or cmakeGeneratorName == CMakeGeneratorName.VisualStudio2022_X64
        or cmakeGeneratorName == CMakeGeneratorName.VisualStudio2026_X64
    ):
        return CMakeGeneratorMultiConfigCapability.Yes
    # Since we dont know, we just return false
    return CMakeGeneratorMultiConfigCapability.Unknown
