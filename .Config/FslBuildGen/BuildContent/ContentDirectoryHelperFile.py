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

# The helper file of a 'Content' directory ('Content.json', version 2): the files that are not for every platform (ContentPlatformList) and
# the files that are not what the defaults of their content type say (ContentProfileList). Nothing else: the commands of a content build
# belong in the command file of 'Content.bld'.

import json

from FslBuildGen import ContentDirectoryConfig, IOUtil, TextFileReader, ToolSharedValues
from FslBuildGen.BuildContent.ContentPlatformList import ContentPlatformList, PlatformsElement, RootElement, VersionElement, VersionWithPlatforms
from FslBuildGen.BuildContent.ContentProfileList import ContentProfileList, ProfilesElement
from FslBuildGen.Exceptions import UsageErrorException
from FslBuildGen.Log import Log


class ContentDirectoryHelper:
    def __init__(self, sourceFilename: str, platformList: ContentPlatformList, profileList: ContentProfileList | None) -> None:
        """profileList: None when the file has no profiles"""
        super().__init__()
        self.SourceFilename = sourceFilename
        self.PlatformList = platformList
        self.ProfileList = profileList


def TryRead(log: Log, contentPath: str) -> ContentDirectoryHelper | None:
    """The helper file of a 'Content' directory. None if the directory has none"""
    filename = IOUtil.Join(contentPath, ToolSharedValues.CONTENT_BUILD_FILE_NAME)
    fileContent = TextFileReader.TryReadUTF8OrLocaleIfExists(log, filename, "content helper file", skipBom=True)
    if fileContent is None:
        return None
    try:
        jsonContent = json.loads(fileContent)
    except ValueError as ex:
        raise UsageErrorException(f"The file '{filename}' is not valid json: {ex}") from ex

    if not isinstance(jsonContent, dict) or len(jsonContent) != 1 or not isinstance(jsonContent.get(RootElement), dict):
        raise UsageErrorException(f"The file '{filename}' is not of the expected format: one '{RootElement}' object")
    jsonRoot = jsonContent[RootElement]
    # The name the directory has: a tool config can give the content directory another name than 'Content'
    directoryName = IOUtil.GetFileName(contentPath)
    holds = f"'{PlatformsElement}' and '{ProfilesElement}'"
    if jsonRoot.get(VersionElement) != VersionWithPlatforms:
        raise UsageErrorException(
            f"The file '{filename}' is version {jsonRoot.get(VersionElement)!r} and we expected {VersionWithPlatforms!r}: it is the helper "
            f"file of a '{directoryName}' directory, which holds {holds}"
        )
    unknownKeys = sorted(str(key) for key in jsonRoot if key not in (VersionElement, PlatformsElement, ProfilesElement))
    if len(unknownKeys) > 0:
        raise UsageErrorException(
            f"Unsupported element '{unknownKeys[0]}' in file '{filename}'. The helper file of a '{directoryName}' directory "
            f"holds {holds}; the commands belong in the one of '{directoryName}{ContentDirectoryConfig.BuildPostfix}'"
        )
    platformList = ContentPlatformList.Parse(jsonRoot.get(PlatformsElement, []), filename)
    profileList = ContentProfileList.Parse(jsonRoot[ProfilesElement], filename) if ProfilesElement in jsonRoot else None
    return ContentDirectoryHelper(filename, platformList, profileList)
