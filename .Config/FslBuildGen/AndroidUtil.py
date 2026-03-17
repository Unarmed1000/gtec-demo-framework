#!/usr/bin/env python3
# ****************************************************************************************************************************************************
# Copyright 2018 NXP
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


from FslBuildGen import IOUtil
from FslBuildGen.DataTypes import BuildPlatformType
from FslBuildGen.PlatformUtil import PlatformUtil
from FslBuildGen.SharedGeneration import AndroidABIOption

g_cachedNDKVersionString: str | None = None


class AndroidUtil:
    @staticmethod
    def GetTargetSDKVersion() -> int:
        return 35

    @staticmethod
    def GetMinimumSDKVersion() -> int:
        return 25

    @staticmethod
    def GetSDKPath() -> str:
        try:
            return IOUtil.GetEnvironmentVariableForDirectory("ANDROID_SDK_ROOT")
        except OSError:
            return IOUtil.GetEnvironmentVariableForDirectory("ANDROID_HOME")

    @staticmethod
    def GetNDKPath() -> str:
        return IOUtil.GetEnvironmentVariableForDirectory("ANDROID_NDK")

    @staticmethod
    def IsValidVersionString(version: str) -> bool:
        if len(version) <= 0:
            return False
        return all(ch >= "0" and ch <= "9" or ch == "." for ch in version)

    @staticmethod
    def GetVersionStringFromSourceProperties(sdkPath: str) -> str:
        filePath = IOUtil.Join(sdkPath, "source.properties")
        content = IOUtil.ReadFile(filePath)
        searchString = "Pkg.Revision"
        index = content.find(searchString)
        if index < 0:
            raise Exception(f"source.properties at '{filePath} did not contain the expected '{searchString}' entry")
        index += len(searchString)
        endIndex = content.find("\n", index)
        endIndex = endIndex if endIndex >= index else len(content)
        content = content[index:endIndex]
        content = content.strip()
        if len(content) <= 0:
            raise Exception(f"Failed to retrieve version from '{filePath}' entry")
        if content[0] != "=":
            raise Exception(f"source.properties at '{filePath} did not contain the expected '{searchString}=' entry")
        content = content[1 : len(content)].strip()
        if not AndroidUtil.IsValidVersionString(content):
            raise Exception(f"Failed to retrieve version from '{filePath}' entry as was in a unexpected format")
        return content

    @staticmethod
    def DetectSDKVersionString(sdkPath: str) -> str:
        sdkPath = IOUtil.Join(sdkPath, "platform-tools")
        return AndroidUtil.GetVersionStringFromSourceProperties(sdkPath)

    @staticmethod
    def DetectNDKVersionString(sdkPath: str) -> str:
        return AndroidUtil.GetVersionStringFromSourceProperties(sdkPath)

    @staticmethod
    def GetSDKVersion() -> str:
        sdkPath = AndroidUtil.GetSDKPath()
        return AndroidUtil.DetectSDKVersionString(sdkPath)

    @staticmethod
    def GetNDKVersion() -> str:
        global g_cachedNDKVersionString
        if g_cachedNDKVersionString is None:
            ndkPath = AndroidUtil.GetNDKPath()
            g_cachedNDKVersionString = AndroidUtil.DetectNDKVersionString(ndkPath)
        return g_cachedNDKVersionString

    @staticmethod
    def GetSDKNDKId() -> str:
        sdkVersion = AndroidUtil.GetSDKVersion().replace(".", "_")
        ndkVersion = AndroidUtil.GetNDKVersion().replace(".", "_")
        hostType = "Win" if PlatformUtil.DetectBuildPlatformType() == BuildPlatformType.Windows else "Unix"

        return f"S{sdkVersion}N{ndkVersion}{hostType}"

    @staticmethod
    def UseNDKCMakeToolchain() -> bool:
        """
        Check if the NDK cmake toolchain file should be utilized to build
        """
        strVersion = AndroidUtil.GetNDKVersion()
        index = strVersion.find(".")
        if index < 0:
            return False
        strVersion = strVersion[0:index]
        version = int(strVersion)
        return version >= 19

    @staticmethod
    def GetKnownABIList(allowAllId: bool) -> list[str]:
        result = [
            AndroidABIOption.ArmeAbiV7a,
            AndroidABIOption.Arm64V8a,
            AndroidABIOption.X86,
            AndroidABIOption.X86_64,
        ]
        if allowAllId:
            result.insert(0, AndroidABIOption.All)
        return result
