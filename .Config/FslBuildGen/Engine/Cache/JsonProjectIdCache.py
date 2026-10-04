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

from FslBuildGen import IOUtil, TextFileReader, Util
from FslBuildGen.Exceptions import InvalidPackageNameException, UsageErrorException
from FslBuildGen.Log import Log


class LocalVerbosityLevel:
    Info = 3
    Debug = 4
    Trace = 5


class JsonProjectIdCache:
    CURRENT_VERSION = 1

    def __init__(self, projectIdDict: dict[str, str]) -> None:
        super().__init__()
        self.Version = JsonProjectIdCache.CURRENT_VERSION
        self.ProjectIdDict = projectIdDict

        projectIdToNameDict: dict[str, str] = {}
        for packageName, packageProjectId in projectIdDict.items():
            if not Util.IsValidPackageName(packageName):
                raise InvalidPackageNameException(packageName)
            if packageProjectId in projectIdToNameDict:
                raise Exception(
                    f"The package project id '{packageProjectId}' is registered for multiple package names. First '{projectIdToNameDict[packageProjectId]}' Second '{packageName}'"
                )
            projectIdToNameDict[packageProjectId] = packageName

    def Add(self, packageName: str, packageProjectId: str) -> None:
        if not Util.IsValidPackageName(packageName):
            raise InvalidPackageNameException(packageName)
        self.ProjectIdDict[packageName] = packageProjectId

    def Remove(self, packageName: str) -> None:
        self.ProjectIdDict.pop(packageName)

    @staticmethod
    def TryLoad(log: Log, cacheFilename: str) -> JsonProjectIdCache | None:
        try:
            # The cache is written as UTF-8, a cache that an older version wrote in the locale encoding still loads
            strJson = TextFileReader.TryReadUTF8OrLocale(log, cacheFilename, "project id cache", skipBom=True, warn=False)
            if strJson is None:
                return None
            jsonDict = json.loads(strJson)
            if jsonDict["Version"] != JsonProjectIdCache.CURRENT_VERSION:
                raise Exception("Unsupported version")

            jsonProjectIdDict = jsonDict["ProjectIdDict"]
            finalDict: dict[str, str] = {}

            for key, value in jsonProjectIdDict.items():
                if not isinstance(key, str) or not isinstance(value, str):
                    raise Exception("json decode failed")
                finalDict[key] = value
        except Exception:
            log.DoPrintWarning(f"Failed to decode cache file '{cacheFilename}'")
            return None

        # A cache that decodes but holds an entry the tool would not write is not thrown away: the project ids are random, a new cache
        # would give every package another one. It stops the tool, as it did before the cache was checked here
        try:
            return JsonProjectIdCache(finalDict)
        except Exception as ex:
            raise UsageErrorException(
                f"The project id cache '{cacheFilename}' is not valid: {ex}. Remove the entry, or delete the file to give every package a new project id"
            ) from ex

    @staticmethod
    def Save(log: Log, cacheFilename: str, JsonProjectIdCache: JsonProjectIdCache) -> None:
        log.LogPrintVerbose(LocalVerbosityLevel.Trace, f"- Saving cache '{cacheFilename}'")
        jsonText = json.dumps(JsonProjectIdCache.__dict__, ensure_ascii=False, sort_keys=True, indent=2)
        IOUtil.WriteFileUTF8IfChanged(cacheFilename, jsonText)

    @staticmethod
    def IsEqual(lhs: JsonProjectIdCache, rhs: JsonProjectIdCache) -> bool:
        if lhs.Version != rhs.Version or len(lhs.ProjectIdDict) != len(rhs.ProjectIdDict):
            return False
        return all(not (key not in rhs.ProjectIdDict or value != rhs.ProjectIdDict[key]) for key, value in lhs.ProjectIdDict.items())
