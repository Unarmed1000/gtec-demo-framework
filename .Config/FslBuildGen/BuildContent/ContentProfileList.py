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

# The files of a content directory that are not what the defaults of their content type say (ContentProfile).
#
# The helper file of 'Content' and the command file of 'Content.bld' ('Content.json', version 2) can hold:
#
#   { "ContentBuilder": { "Version": "2",
#       "Content.Profiles": [
#         { "Content": ["Textures"], "Profile": "UI" },
#         { "Content": ["Textures/MtsdfFont_*.png"], "Profile": "DistanceField" },
#         { "Content": ["Textures/Splash.png"], "Compression": "High" } ] } }
#
# - 'Content': the files of the entry, as in 'Platforms' (ContentPlatformList): files, directories and patterns with '*', with their case.
# - 'Profile': a profile of the tool. The other keys are properties, each overrides the one value. An entry has a profile, properties or
#   both, all for one content type.
# - An entry is for the files of its content type: a directory of textures can hold a 'License.json' that a texture profile is not about.
# - A file gets the default of its type, then each entry for it in order: a profile sets every property, a property sets itself. The last
#   entry wins, as in .gitattributes, so 'the directory is UI, but the font' is two entries.
# - A name the tool does not know, a value a property does not have and an entry that is for no file are errors: each of them is an entry
#   that does not say what its writer meant.
# - The paths are those below the directory of the file: the path a file has in its owner, before a content namespace is put in front.

import json
from collections.abc import Iterable, Sequence
from typing import Any

from FslBuildGen.BuildContent import ContentProfile
from FslBuildGen.BuildContent.ContentPlatformList import CheckPattern, IsPatternMatch
from FslBuildGen.BuildContent.ContentProfile import ContentType, ResolvedContentProfile
from FslBuildGen.Exceptions import UsageErrorException

ProfilesElement = "Content.Profiles"

_g_content = "Content"
_g_profile = "Profile"


class ContentProfileEntry:
    def __init__(self, description: str, patterns: Sequence[str], contentType: ContentType, profileName: str | None, properties: dict[str, str]) -> None:
        """description: the entry as its writer wrote it, for the messages
        profileName: the profile it gives its files, None if it only sets properties
        """
        super().__init__()
        self.Description = description
        self.Patterns = list(patterns)
        self.ContentType = contentType
        self.ProfileName = profileName
        self.Properties = properties

    def IsFor(self, relativePath: str, contentType: ContentType) -> bool:
        return contentType == self.ContentType and any(IsPatternMatch(pattern, relativePath) for pattern in self.Patterns)


class ContentProfileList:
    def __init__(self, sourceFilename: str, entries: Sequence[ContentProfileEntry]) -> None:
        super().__init__()
        self.SourceFilename = sourceFilename
        self.Entries = list(entries)

    def Resolve(self, relativePaths: Iterable[str]) -> dict[str, ResolvedContentProfile]:
        """What each file is.
        relativePaths: every file the file describes, relative to the directory and with '/', whatever platform it is for
        """
        usedEntries: set[int] = set()
        result: dict[str, ResolvedContentProfile] = {}
        for relativePath in relativePaths:
            contentType = ContentProfile.GetContentType(relativePath)
            profileName = ContentProfile.DefaultProfileName
            properties = ContentProfile.GetDefault(contentType).Properties
            for index, entry in enumerate(self.Entries):
                if entry.IsFor(relativePath, contentType):
                    usedEntries.add(index)
                    if entry.ProfileName is not None:
                        profile = ContentProfile.TryGetProfile(entry.ProfileName)
                        assert profile is not None
                        profileName = profile.Name
                        properties = dict(profile.Properties)
                    properties.update(entry.Properties)
            result[relativePath] = ResolvedContentProfile(contentType, profileName, properties)

        for index, entry in enumerate(self.Entries):
            if index not in usedEntries:
                raise UsageErrorException(
                    f"The entry {entry.Description} of '{ProfilesElement}' in '{self.SourceFilename}' is for no file: it is about the "
                    f"{entry.ContentType.value.lower()} files of its '{_g_content}', and none of the files, directories and patterns there is "
                    "one. A name has the case the file has"
                )
        return result

    @staticmethod
    def Parse(jsonProfiles: Any, sourceFilename: str) -> ContentProfileList:
        """jsonProfiles: the value of 'Content.Profiles' in the file"""
        if not isinstance(jsonProfiles, list):
            raise UsageErrorException(f"'{ProfilesElement}' in '{sourceFilename}' is not a list of entries")
        return ContentProfileList(sourceFilename, [ContentProfileList.__ParseEntry(jsonEntry, sourceFilename) for jsonEntry in jsonProfiles])

    @staticmethod
    def __ParseEntry(jsonEntry: Any, sourceFilename: str) -> ContentProfileEntry:
        where = f"'{ProfilesElement}' in '{sourceFilename}'"
        allProperties = sorted({name for contentType in ContentType for name in ContentProfile.GetPropertyValues(contentType)})
        validKeys = [_g_content, _g_profile, *allProperties]
        if not isinstance(jsonEntry, dict):
            raise UsageErrorException(f"An entry of {where} is not an object with '{_g_content}' and a '{_g_profile}' or properties")
        unknownKeys = sorted(str(key) for key in jsonEntry if key not in validKeys)
        if len(unknownKeys) > 0:
            raise UsageErrorException(f"An entry of {where} has the unknown key '{unknownKeys[0]}'. Valid keys are: {', '.join(validKeys)}")
        description = json.dumps(jsonEntry)
        if _g_content not in jsonEntry:
            raise UsageErrorException(f"The entry {description} of {where} has no '{_g_content}': the files it is about")
        patterns = jsonEntry[_g_content]
        if not isinstance(patterns, list) or len(patterns) <= 0 or not all(isinstance(entry, str) and len(entry) > 0 for entry in patterns):
            raise UsageErrorException(f"'{_g_content}' of the entry {description} of {where} is not a list of at least one name")
        for pattern in patterns:
            CheckPattern(pattern, f"an entry of {where}")

        # The content type the entry is for: the one of its profile and of each of its properties
        contentTypes: dict[str, ContentType] = {}
        profileName: str | None = None
        if _g_profile in jsonEntry:
            profileName = jsonEntry[_g_profile]
            profile = ContentProfile.TryGetProfile(profileName) if isinstance(profileName, str) else None
            if profile is None:
                raise UsageErrorException(
                    f"'{profileName}' in '{_g_profile}' of the entry {description} of {where} is not a profile. The profiles are: "
                    f"{', '.join(ContentProfile.GetProfileNames())}"
                )
            contentTypes[f"'{_g_profile}' {profile.Name}"] = profile.ContentType

        properties: dict[str, str] = {}
        for name in allProperties:
            if name in jsonEntry:
                contentType = ContentProfile.GetContentTypeOfProperty(name)
                assert contentType is not None
                contentTypes[f"'{name}'"] = contentType
                properties[name] = ContentProfileList.__ParseValue(
                    jsonEntry[name], name, ContentProfile.GetPropertyValues(contentType)[name], description, where
                )

        if len(contentTypes) <= 0:
            raise UsageErrorException(f"The entry {description} of {where} has no '{_g_profile}' and no property: it says nothing about its files")
        if len(set(contentTypes.values())) > 1:
            names = ", ".join(f"{name} is for {contentType.value.lower()} files" for name, contentType in contentTypes.items())
            raise UsageErrorException(f"The entry {description} of {where} is about more than one content type: {names}. Give each its own entry")
        return ContentProfileEntry(description, patterns, next(iter(contentTypes.values())), profileName, properties)

    @staticmethod
    def __ParseValue(jsonValue: Any, name: str, values: tuple[str, ...], description: str, where: str) -> str:
        # A switch can be a json bool
        if isinstance(jsonValue, bool) and ContentProfile.ValueTrue in values:
            return ContentProfile.ValueTrue if jsonValue else ContentProfile.ValueFalse
        if not isinstance(jsonValue, str) or jsonValue not in values:
            raise UsageErrorException(
                f"{json.dumps(jsonValue)} is not a value of '{name}' in the entry {description} of {where}. Its values are: {', '.join(values)}"
            )
        return jsonValue


def Resolve(profileList: ContentProfileList | None, relativePaths: Iterable[str]) -> dict[str, ResolvedContentProfile]:
    """What each file is, by its path relative to the directory. Without a list each file is the default of its type"""
    if profileList is not None:
        return profileList.Resolve(relativePaths)
    return {relativePath: ContentProfile.GetDefault(ContentProfile.GetContentType(relativePath)) for relativePath in relativePaths}
