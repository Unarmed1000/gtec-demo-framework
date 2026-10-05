#!/usr/bin/env python3

# ****************************************************************************************************************************************************
# Copyright 2019 NXP
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

from FslBuildGen import IOUtil, TextFileReader
from FslBuildGen.Log import Log


def _EscapeLoneSurrogates(jsonText: str) -> str:
    """The JSON text with every lone surrogate written as its JSON escape (as json.dumps writes it with ensure_ascii), so the text can be
    encoded as UTF-8 and loads back as the same strings. A lone surrogate is what python makes of an environment value that is not valid
    text: a byte that is not UTF-8 on Linux, half a surrogate pair on Windows.
    Text that can be encoded is returned as it is, so a cache without such a value has the bytes it always had.
    """
    try:
        jsonText.encode("utf-8")
        return jsonText
    except UnicodeEncodeError:
        return "".join(f"\\u{ord(ch):04x}" if 0xD800 <= ord(ch) <= 0xDFFF else ch for ch in jsonText)


class BuildConfigureCache:
    CURRENT_VERSION = 5

    def __init__(
        self,
        environmentDict: dict[str, str],
        userSetVariablesDict: dict[str, str],
        fileHashDict: dict[str, str],
        commandList: list[str],
        platformName: str,
        fslBuildVersion: str,
        allowFindPackage: str,
        configuredValues: dict[str, str] | None = None,
    ) -> None:
        super().__init__()
        self.Version = BuildConfigureCache.CURRENT_VERSION
        self.EnvironmentDict = environmentDict
        self.UserSetVariablesDict = userSetVariablesDict
        self.FileHashDict = fileHashDict
        self.CommandList = commandList
        self.PlatformName = platformName
        self.FslBuildVersion = fslBuildVersion
        self.AllowFindPackage = allowFindPackage
        # What the cache of cmake held for the variables of the command right after the configure this record is of
        # (Build/CMakeCacheValues.py). Empty in the record of a build that is about to be configured and in a file an older version wrote.
        self.ConfiguredValues: dict[str, str] = {} if configuredValues is None else configuredValues

    @staticmethod
    def TryLoad(log: Log, cacheFilename: str) -> BuildConfigureCache | None:
        try:
            # The cache is written as UTF-8, a cache that an older version wrote in the locale encoding still loads
            strJson = TextFileReader.TryReadUTF8OrLocale(log, cacheFilename, "configure cache", skipBom=True, warn=False)
            if strJson is None:
                return None
            jsonDict = json.loads(strJson)
            if jsonDict["Version"] != BuildConfigureCache.CURRENT_VERSION:
                raise Exception("Unsupported version")

            jsonUserSetVariablesDict = jsonDict["UserSetVariablesDict"]
            finalUserSetVariablesDict: dict[str, str] = {}
            for key, value in jsonUserSetVariablesDict.items():
                if not isinstance(key, str) or not isinstance(value, str):
                    raise Exception("json decode failed")
                finalUserSetVariablesDict[key] = value

            jsonEnvironmentDict = jsonDict["EnvironmentDict"]
            finalEnvironmentDict: dict[str, str] = {}
            for key, value in jsonEnvironmentDict.items():
                if not isinstance(key, str) or not isinstance(value, str):
                    raise Exception("json decode failed")
                finalEnvironmentDict[key] = value

            jsonFileHashDict = jsonDict["FileHashDict"]
            finalDict: dict[str, str] = {}
            for key, value in jsonFileHashDict.items():
                if not isinstance(key, str) or not isinstance(value, str):
                    raise Exception("json decode failed")
                finalDict[key] = value

            finalCommandList: list[str] = []
            jsonCommandList = jsonDict["CommandList"]
            for value in jsonCommandList:
                if not isinstance(value, str):
                    raise Exception("json decode failed")
                finalCommandList.append(value)

            platformName: str = jsonDict["PlatformName"]
            fslBuildVersion: str = jsonDict["FslBuildVersion"]
            allowFindPackage: str = jsonDict["AllowFindPackage"]

            # A file of a version that did not keep these values has no such entry: that file is as good as it was
            finalConfiguredValues: dict[str, str] = {}
            for key, value in jsonDict.get("ConfiguredValues", {}).items():
                if not isinstance(key, str) or not isinstance(value, str):
                    raise Exception("json decode failed")
                finalConfiguredValues[key] = value
            return BuildConfigureCache(
                finalEnvironmentDict,
                finalUserSetVariablesDict,
                finalDict,
                finalCommandList,
                platformName,
                fslBuildVersion,
                allowFindPackage,
                finalConfiguredValues,
            )
        except Exception:
            log.DoPrintWarning(f"Failed to decode cache file '{cacheFilename}'")
            return None

    @staticmethod
    def Save(log: Log, cacheFilename: str, buildConfigureCache: BuildConfigureCache) -> None:
        log.LogPrintVerbose(4, f"- Saving generated file hash cache '{cacheFilename}'")
        jsonText = json.dumps(buildConfigureCache.__dict__, ensure_ascii=False, sort_keys=True, indent=2)
        IOUtil.WriteFileUTF8IfChanged(cacheFilename, _EscapeLoneSurrogates(jsonText))

    @staticmethod
    def TrySave(log: Log, cacheFilename: str, buildConfigureCache: BuildConfigureCache) -> bool:
        """Save the cache, returns false if that failed.
        The cache only saves work: one that can not be saved is a warning, the previous cache is left as it was and configure runs again
        the next time.
        """
        try:
            BuildConfigureCache.Save(log, cacheFilename, buildConfigureCache)
            return True
        except (OSError, UnicodeError) as ex:
            log.DoPrintWarning(f"Failed to save the configure cache '{cacheFilename}', the next build runs configure again: {ex}")
            return False

    @staticmethod
    def IsEqual(lhs: BuildConfigureCache, rhs: BuildConfigureCache) -> bool:
        """True if what decides the configure is the same: anything of it that differs means the build has to be configured again. That
        includes a user variable or an environment variable that is no longer set, and the find package setting.
        """
        # FslBuildVersion is not compared. It is stored to tell which tool wrote the cache. Whatever a new version of the tool generates
        # differently is seen through FileHashDict (the hashes of the generated files) and CommandList, and a build directory that had to
        # be configured again after every update of the tool would cost the user a configure of each of them on every deploy for nothing.
        #
        # ConfiguredValues is not compared either: it is what a configure left behind, not what decides one. The builder compares it with
        # the cache of cmake (Build/CMakeCacheValues.py).
        return (
            lhs.Version == rhs.Version
            and lhs.EnvironmentDict == rhs.EnvironmentDict
            and lhs.UserSetVariablesDict == rhs.UserSetVariablesDict
            and lhs.FileHashDict == rhs.FileHashDict
            and lhs.CommandList == rhs.CommandList
            and lhs.PlatformName == rhs.PlatformName
            and lhs.AllowFindPackage == rhs.AllowFindPackage
        )
