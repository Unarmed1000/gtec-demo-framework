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

# Takes the files that are not content on the platform of the run out of what was found in the content directories of a package.
# The resolver and the content builder both do this, with the same lists, so what a generator expects is what the content build makes.

from FslBuildGen import IOUtil, ToolSharedValues
from FslBuildGen.BuildContent import ContentDirectoryHelperFile, ContentProfileList
from FslBuildGen.BuildContent.ContentPlatformList import ContentPlatformList, PlatformsElement
from FslBuildGen.BuildContent.ContentProfile import ResolvedContentProfile
from FslBuildGen.BuildContent.PathRecord import PathRecord
from FslBuildGen.BuildContent.Processor.SourceContent import SourceContent
from FslBuildGen.Exceptions import UsageErrorException
from FslBuildGen.Log import Log


def RemoveBuildFilesNotForPlatform(
    sourceContent: SourceContent, platformList: ContentPlatformList | None, contentBuildPath: str, platformName: str | None
) -> None:
    """Take the files of 'Content.bld' that are not for the platform out of the build inputs.
    platformList: the lists of the command file of the directory, None if it has none
    platformName: None if the caller does not know the platform, which is an error when there are lists to apply
    """
    if platformList is None:
        return
    if platformName is None:
        raise UsageErrorException(
            f"'{platformList.SourceFilename}' has '{PlatformsElement}', so the content can not be built without the platform it is built for"
        )
    # The lists are about the files of the directory, what a command of the file brings in from somewhere else is not in them
    ownFiles = [record for record in sourceContent.ContentBuildSource.Files if record.SourceRoot.ResolvedPath == contentBuildPath]
    notForPlatform = platformList.GetFilesNotForPlatform([record.RelativePath for record in ownFiles], platformName)
    for record in ownFiles:
        if record.RelativePath in notForPlatform:
            sourceContent.ContentBuildSource.RemoveFileByResolvedSourcePath(record.ResolvedPath)
            sourceContent.AllContentSource.RemoveFileByResolvedSourcePath(record.ResolvedPath)


def ResolveContentFiles(log: Log, contentPath: str, files: list[PathRecord], platformName: str) -> tuple[list[PathRecord], dict[str, ResolvedContentProfile]]:
    """The files of a 'Content' directory that are content on the platform, and what each of them is by its resolved path. The helper file
    of the directory is never content.
    files: what was found in the directory
    """
    helperFilename = IOUtil.Join(contentPath, ToolSharedValues.CONTENT_BUILD_FILE_NAME)
    helper: ContentDirectoryHelperFile.ContentDirectoryHelper | None = None
    if any(record.ResolvedPath == helperFilename for record in files):
        files = [record for record in files if record.ResolvedPath != helperFilename]
        helper = ContentDirectoryHelperFile.TryRead(log, contentPath)
    # The profiles are checked against every file of the directory, so a helper file that is wrong is wrong on every platform
    profiles = ContentProfileList.Resolve(helper.ProfileList if helper is not None else None, [record.RelativePath for record in files])
    if helper is not None:
        notForPlatform = helper.PlatformList.GetFilesNotForPlatform([record.RelativePath for record in files], platformName)
        files = [record for record in files if record.RelativePath not in notForPlatform]
    return (files, {record.ResolvedPath: profiles[record.RelativePath] for record in files})
