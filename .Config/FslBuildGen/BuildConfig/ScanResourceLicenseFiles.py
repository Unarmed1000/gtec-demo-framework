#!/usr/bin/env python3

# ****************************************************************************************************************************************************
# Copyright 2021 NXP
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
import os
import os.path
from typing import Any

from FslBuildGen import IOUtil
from FslBuildGen.BuildConfig.LicenseConfig import LicenseConfig
from FslBuildGen.DataTypes import PackageType
from FslBuildGen.Log import Log
from FslBuildGen.Packages.Package import Package
from FslBuildGen.ToolMinimalConfig import ToolMinimalConfig

# import argparse
# import os
# import subprocess

__g_Image = "Image"
__g_Model = "Model"
__g_Video = "Video"
__g_extensions = [
    (".bmp", __g_Image),
    (".dds", __g_Image),
    (".hdr", __g_Image),
    (".jpg", __g_Image),
    (".ktx", __g_Image),
    (".png", __g_Image),
    (".psd", __g_Image),
    (".tga", __g_Image),
    (".tiff", __g_Image),
    (".3ds", __g_Model),
    (".fbx", __g_Model),
    (".fsf", __g_Model),
    (".obj", __g_Model),
    (".nff", __g_Model),
    # video
    (".avi", __g_Video),
    (".fsf", __g_Video),
    (".mp4", __g_Video),
    (".mpg", __g_Video),
    (".mpeg", __g_Video),
    (".mkv", __g_Video),
]

__g_ignore: list[str] = []

__g_ignoreDir = [
    ".Config/Templates.gen/Android/Copy/res/drawable-hdpi",
    ".Config/Templates.gen/Android/Copy/res/drawable-ldpi",
    ".Config/Templates.gen/Android/Copy/res/drawable-mdpi",
    ".Config/Templates.gen/Android/Copy/res/drawable-xhdpi",
    ".Config/Templates.gen/Android/Copy/res/drawable-xxhdpi",
    ".Config/Templates.gen/Android/Copy/res/drawable-xxxhdpi",
    ".Config/Templates.gen/AndroidGradleCMake/Copy/app/src/main/res/mipmap-hdpi",
    ".Config/Templates.gen/AndroidGradleCMake/Copy/app/src/main/res/mipmap-ldpi",
    ".Config/Templates.gen/AndroidGradleCMake/Copy/app/src/main/res/mipmap-mdpi",
    ".Config/Templates.gen/AndroidGradleCMake/Copy/app/src/main/res/mipmap-xhdpi",
    ".Config/Templates.gen/AndroidGradleCMake/Copy/app/src/main/res/mipmap-xxhdpi",
    ".Config/Templates.gen/AndroidGradleCMake/Copy/app/src/main/res/mipmap-xxxhdpi",
]


class JsonBasicLicense:
    def __init__(self, sourceDict: dict[str, str] | None = None) -> None:
        if sourceDict is None:
            sourceDict = {}
        super().__init__()
        self.Origin = ""
        self.License = ""
        self.Comment = ""
        self.Url = ""
        self.Tags = ""
        self.TagsIdList: list[str] = []
        self.SourceDict = sourceDict

    def SetTags(self, tags: str) -> None:
        self.Tags = tags
        self.TagsIdList = [entry.lower() for entry in tags.split(";") if len(entry) > 0]

    def Compare(self, license: "JsonBasicLicense") -> bool:
        return (
            self.Origin == license.Origin
            and self.License == license.License
            and self.Url == license.Url
            and self.Tags == license.Tags
            and self.SourceDict == license.SourceDict
        )


class JsonComplexLicense:
    def __init__(self, licenses: list[JsonBasicLicense], comment: str | None) -> None:
        super().__init__()
        self.Comment = comment if comment is not None else ""
        self.Licenses = list(licenses)

    def Compare(self, license: "JsonComplexLicense") -> bool:
        return self.Comment == license.Comment and self.__IsConsideredEqual(license.Licenses)

    def __IsConsideredEqual(self, otherLicenses: list[JsonBasicLicense]) -> bool:
        return all(self.__IsMember(entry) for entry in otherLicenses)

    def __IsMember(self, license: JsonBasicLicense) -> bool:
        return any(entry.Compare(license) for entry in self.Licenses)

    def Merge(self, license: "JsonComplexLicense") -> None:
        for entry in license.Licenses:
            if not self.__IsMember(entry):
                self.Licenses.append(entry)


def _GetExtensionList(extensions: list[tuple[str, str]]) -> list[str]:
    return [extension[0] for extension in extensions]


def _ReadJsonFile(filename: str) -> Any:  # nasty any return
    content = IOUtil.ReadFile(filename)
    return json.loads(content)


def _WriteJsonFileIfChanged(filename: str, dict: dict[str, str]) -> None:
    content = str(json.dumps(dict, ensure_ascii=False, indent=2))
    IOUtil.WriteFileUTF8IfChanged(filename, content)


class Resource:
    def __init__(self, sourcePath: str, relativeSkipChars: int) -> None:
        super().__init__()
        self.SourcePath = sourcePath
        self.SourceDirectory = IOUtil.GetDirectoryName(sourcePath)
        self.License: JsonComplexLicense | None = None
        self.RelativePath = sourcePath[relativeSkipChars:]


def _ScanForFiles(path: str, extensionList: list[str], ignoreFiles: list[str], ignoreDirs: list[str]) -> list[str]:
    foundFiles = []
    for root, dirs, files in os.walk(path):
        if ignoreDirs is not None:
            dirs[:] = [dir for dir in dirs if IOUtil.ToUnixStylePath(os.path.join(root, dir)) not in ignoreDirs]
        for file in files:
            fileId = file.lower()
            for extension in extensionList:
                if fileId.endswith(extension) and fileId not in ignoreFiles:
                    foundFiles.append(IOUtil.Join(root, file))
                    break
    return foundFiles


def _BuildFileLengthDict(files: list[str]) -> dict[int, list[str]]:
    result: dict[int, list[str]] = {}
    for file in files:
        fileLength = os.stat(file).st_size
        if fileLength in result:
            result[fileLength].append(file)
        else:
            result[fileLength] = [file]
    return result


def _BuildFileContentHashDict(files: list[str]) -> dict[str, list[str]]:
    result: dict[str, list[str]] = {}
    for file in files:
        hash = IOUtil.HashFile(file)
        if hash in result:
            result[hash].append(file)
        else:
            result[hash] = [file]
    return result


def _BuildDuplicatedList(fileName: str, files: list[str]) -> list[str]:
    srcFilename = files[0]
    srcContentSet = set(IOUtil.ReadBinaryFile(srcFilename))
    matchingFiles = [fileName]
    for file in files:
        content = IOUtil.ReadBinaryFile(file)
        if len(srcContentSet.intersection(content)) == len(srcContentSet):
            matchingFiles.append(file)
    return matchingFiles


def _BuildDuplicatedDict(log: Log, files: list[str], uniqueFiles: list[str]) -> dict[str, list[str]]:
    result: dict[str, list[str]] = {}
    while len(files) > 1:
        srcFile = files[0]
        remainingFiles = files[1:]
        matchingFiles = _BuildDuplicatedList(srcFile, remainingFiles)
        if len(matchingFiles) > 1:
            result[srcFile] = matchingFiles
        else:
            uniqueFiles.append(srcFile)

        # Remove all non duplicated files
        files.remove(srcFile)
        files = []
        for file in remainingFiles:
            if file not in matchingFiles:
                files.append(file)
    return result


def _BuildUniqueFileDictByContent(log: Log, files: list[str], uniqueFiles: list[str]) -> dict[str, list[str]]:
    # we start by sorting files by their hash
    # this should limit the amount of files that have to be byte compared quite a bit
    duplicationDict: dict[str, list[str]] = {}
    dictHash = _BuildFileContentHashDict(files)
    for fileList in list(dictHash.values()):
        if len(fileList) > 1:
            newDuplicationDict = _BuildDuplicatedDict(log, fileList, uniqueFiles)
            duplicationDict.update(newDuplicationDict)
        else:
            uniqueFiles.append(fileList[0])
    return duplicationDict


def _BuildUniqueFileDict(log: Log, files: list[str], uniqueFiles: list[str]) -> dict[str, list[str]]:
    # we start by sorting files by their size
    # this should limit the amount of files that have to be byte compared quite a bit
    dictFileLength = _BuildFileLengthDict(files)

    # log.LogPrint("Initial bins {0}".format(len(dictFileLength)))

    duplicationDict = {}
    for fileList in list(dictFileLength.values()):
        if len(fileList) > 1:
            newDuplicationDict = _BuildUniqueFileDictByContent(log, fileList, uniqueFiles)
            duplicationDict.update(newDuplicationDict)
        else:
            uniqueFiles.append(fileList[0])
    return duplicationDict


def _BuildExtensionDict(extensions: list[tuple[str, str]]) -> dict[str, str]:
    result: dict[str, str] = {}
    for extension in extensions:
        result[extension[0]] = extension[1]
    return result


def _GetContentTypeByExtension(extensionDict: dict[str, str], filename: str) -> str:
    filenameExtension = IOUtil.GetFileNameExtension(filename).lower()
    return extensionDict.get(filenameExtension, "")


def _BuildResourceDirectorySet(uniqueFiles: list[str], duplicatedFilesDict: dict[str, list[str]]) -> set[str]:
    # build unique dir list
    resourceDirSet: set[str] = set()
    for entry in uniqueFiles:
        dirName = IOUtil.GetDirectoryName(entry)
        if dirName not in resourceDirSet:
            resourceDirSet.add(dirName)

    for fileList in list(duplicatedFilesDict.values()):
        for entry in fileList:
            dirName = IOUtil.GetDirectoryName(entry)
            if dirName not in resourceDirSet:
                resourceDirSet.add(dirName)
    return resourceDirSet


class LicenseManager:
    def __init__(self) -> None:
        super().__init__()
        self.KeyOrigin = "Origin"
        self.KeyLicense = "License"
        self.KeyComment = "Comment"
        self.KeyTags = "Tags"
        self.KeyURL = "URL"

        self.KeyComplexLicense = "ComplexLicense"
        self.KeyComplexLicenses = "Licenses"
        self.KeyComplexComment = "Comment"

    def CreateDefaultLicense(self, origin: str, license: str) -> JsonBasicLicense:
        jsonDict: dict[str, str] = {}
        jsonDict[self.KeyOrigin] = origin
        jsonDict[self.KeyLicense] = license

        jsonLicense = JsonBasicLicense(jsonDict)
        jsonLicense.Origin = jsonDict[self.KeyOrigin]
        jsonLicense.License = jsonDict[self.KeyLicense]
        return jsonLicense

    def WriteLicenseIfChanged(self, log: Log, dstFilename: str, srcComplexLicense: JsonComplexLicense) -> None:
        licenses: list[dict[str, str]] = []
        for license in srcComplexLicense.Licenses:
            licenseDict = {}
            if len(license.Origin) > 0:
                licenseDict[self.KeyOrigin] = license.Origin
            if len(license.License) > 0:
                licenseDict[self.KeyLicense] = license.License
            if len(license.Comment) > 0:
                licenseDict[self.KeyComment] = license.Comment
            if len(license.Tags) > 0:
                licenseDict[self.KeyTags] = license.Tags
            if len(license.Url) > 0:
                licenseDict[self.KeyURL] = license.Url
            if len(licenseDict) > 0:
                licenses.append(licenseDict)

        jsonLicenseDict: dict[str, Any] = {}
        jsonLicenseDict[self.KeyComplexLicenses] = licenses
        if len(srcComplexLicense.Comment) > 0:
            jsonLicenseDict[self.KeyComplexComment] = srcComplexLicense.Comment

        jsonDict: dict[str, Any] = {}
        jsonDict[self.KeyComplexLicense] = jsonLicenseDict

        _WriteJsonFileIfChanged(dstFilename, jsonDict)

    def TryReadLicense(self, log: Log, filename: str) -> JsonComplexLicense | None:
        if not os.path.isfile(filename):
            return None

        content = None
        try:
            content = _ReadJsonFile(filename)
        except Exception:
            print(f"ERROR: Exception while parsing {filename}")
            raise

        if self.KeyComplexLicense in content:
            return self.__TryParseComplexLicense(log, content, filename)

        if self.KeyOrigin not in content:
            log.LogPrint(f"ERROR: '{self.KeyOrigin}' not present in file '{filename}'")
            return None
        if self.KeyLicense not in content:
            log.LogPrint(f"ERROR: '{self.KeyLicense}' not present in file '{filename}'")
            return None
        basicLicense = self.__TryParseBasicLicense(log, content, filename)
        return JsonComplexLicense([basicLicense], None) if basicLicense is not None else None

    def __TryParseBasicLicense(self, log: Log, jsonDict: Any, debugFilename: str) -> JsonBasicLicense | None:
        license = JsonBasicLicense(jsonDict)
        license.Origin = jsonDict[self.KeyOrigin]
        license.License = jsonDict[self.KeyLicense]
        license.Comment = jsonDict.get(self.KeyComment, "")
        license.Url = jsonDict.get(self.KeyURL, "")
        license.SetTags(jsonDict.get(self.KeyTags, ""))
        return license

    def __TryParseComplexLicense(self, log: Log, jsonDict: Any, debugFilename: str) -> JsonComplexLicense | None:
        jsonDict = jsonDict[self.KeyComplexLicense]
        if self.KeyComplexLicenses not in jsonDict:
            log.LogPrint(f"ERROR: '{self.KeyComplexLicenses}' not present in file '{debugFilename}'")
            return None

        licenses: list[JsonBasicLicense] = []
        for entry in jsonDict[self.KeyComplexLicenses]:
            basicLicense = self.__TryParseBasicLicense(log, entry, debugFilename)
            if basicLicense is None:
                log.LogPrint(f"ERROR: Failed to parse complex license in file '{debugFilename}'")
                return None
            licenses.append(basicLicense)
        comment = jsonDict.get(self.KeyComplexComment, "")
        return JsonComplexLicense(licenses, comment)

    # def SaveLicense(self, filename: str, license: JsonComplexLicense) -> None:
    #    if len(license.Comment) > 0 or len(license.Licenses) > 1:
    #        # save complex license
    #        pass
    #    else:
    #        #contentDict = {}
    #        #self.__AddKeyIfNeeded(contentDict, self.KeyOrigin, license.Origin)
    #        #self.__AddKeyIfNeeded(contentDict, self.KeyLicense, license.License)
    #        #self.__AddKeyIfNeeded(contentDict, self.KeyURL, license.URL)

    #        _WriteJsonFile(filename, license.Licenses[0].SourceDict)

    # def __AddKeyIfNeeded(self, dict, key, value):
    #    if len(value) <= 0:
    #        return
    #    dict[key] = value


def _HasScreenshot(package: Package, licenseConfig: LicenseConfig) -> bool:
    if package.AbsolutePath is None:
        return False
    screenshotName = IOUtil.Join(package.AbsolutePath, licenseConfig.ScreenshotName)
    return IOUtil.IsFile(screenshotName)


def _BuildPackageLicenseList(log: Log, package: Package, licenseConfig: LicenseConfig) -> list[JsonComplexLicense]:
    licenseManager = LicenseManager()
    licenseList: list[JsonComplexLicense] = []

    if package.ResolvedContentBuilderAllInputFiles is not None:
        for fileEntry in package.ResolvedContentBuilderAllInputFiles:
            if fileEntry.ResolvedPath.endswith(licenseConfig.LicenseFilename):
                license = licenseManager.TryReadLicense(log, fileEntry.ResolvedPath)
                if license is None:
                    raise Exception(f"Failed to read license file: {fileEntry.ResolvedPath}")
                licenseList.append(license)

    if package.ResolvedContentFiles is not None:
        for fileEntry in package.ResolvedContentFiles:
            if fileEntry.ResolvedPath.endswith(licenseConfig.LicenseFilename):
                license = licenseManager.TryReadLicense(log, fileEntry.ResolvedPath)
                if license is None:
                    raise Exception(f"Failed to read license file: {fileEntry.ResolvedPath}")
                licenseList.append(license)

    # Add the default license for executables unless we dont have any other licenses and no screenshot was found
    if package.Type == PackageType.Executable and (_HasScreenshot(package, licenseConfig) or len(licenseList) > 0):
        licenseManager = LicenseManager()
        defaultLicense = licenseManager.CreateDefaultLicense(licenseConfig.DefaultOrigin, licenseConfig.DefaultLicense)
        defaultLicenses = [defaultLicense]
        complexDefaultLicense = JsonComplexLicense(defaultLicenses, None)
        licenseList.append(complexDefaultLicense)

    return licenseList


def _BuildDirectoryLicenseDict(log: Log, resourceDirectories: set[str], licenseFilename: str) -> dict[str, JsonComplexLicense]:
    licenseManager = LicenseManager()
    licenseDict: dict[str, JsonComplexLicense] = {}
    for dir in resourceDirectories:
        license = licenseManager.TryReadLicense(log, IOUtil.Join(dir, licenseFilename))
        if license is not None:
            licenseDict[dir] = license
    return licenseDict


def _TagListWithLicenses(inputDirectory: str, files: list[str], directoryLicenseDict: dict[str, JsonComplexLicense]) -> list[Resource]:
    inputDirectory = IOUtil.NormalizePath(inputDirectory)
    skipChars = len(inputDirectory) if inputDirectory.endswith("/") else len(inputDirectory) + 1

    res = []
    for entry in files:
        resource = Resource(entry, skipChars)
        if resource.SourceDirectory in directoryLicenseDict:
            resource.License = directoryLicenseDict[resource.SourceDirectory]
        res.append(resource)
    return res


def _TagDictWithLicenses(inputDirectory: str, fileDict: dict[str, list[str]], directoryLicenseDict: dict[str, JsonComplexLicense]) -> dict[str, list[Resource]]:
    inputDirectory = IOUtil.NormalizePath(inputDirectory)
    skipChars = len(inputDirectory) if inputDirectory.endswith("/") else len(inputDirectory) + 1

    res: dict[str, list[Resource]] = {}
    for key, value in fileDict.items():
        keyFilename = key[skipChars:]
        res[keyFilename] = _TagListWithLicenses(inputDirectory, value, directoryLicenseDict)
    return res


def _Flatten(entry: JsonComplexLicense) -> tuple[str, str, str, str]:
    originList: list[str] = []
    licenseList: list[str] = []
    commentList: list[str] = []
    urlList: list[str] = []
    for licenseEntry in entry.Licenses:
        originList.append(licenseEntry.Origin)
        licenseList.append(licenseEntry.License)
        commentList.append(licenseEntry.Comment)
        urlList.append(licenseEntry.Url)
    strOrigin = "\\".join(originList)
    strLicense = "\\".join(licenseList)
    strComment = "\\".join(commentList)
    strUrl = "\\".join(urlList)
    return (strOrigin, strLicense, strComment, strUrl)


def _WriteCSV(dstFilename: str, extensions: list[tuple[str, str]], uniqueEntries: list[Resource], duplicatedEntryDict: dict[str, list[Resource]]) -> None:
    # count = len(uniqueFiles)
    # for list in duplicatedFilesDict.values():
    #  count += len(list)
    # log.LogPrint("Found {0} resource files".format(count))

    uniqueEntries.sort(key=lambda s: s.SourcePath.lower())
    sortedDuplicatedFiles = list(duplicatedEntryDict.keys())
    sortedDuplicatedFiles.sort()
    for fileList in list(duplicatedEntryDict.values()):
        fileList.sort(key=lambda s: s.SourcePath.lower())

    extensionDict = _BuildExtensionDict(extensions)

    lines = []
    lines.append(f"Unique files ({len(uniqueEntries)});;Origin;License;Type;Comment;URL")
    for entry in uniqueEntries:
        contentType = _GetContentTypeByExtension(extensionDict, entry.RelativePath)
        if entry.License is None:
            lines.append(f"{entry.RelativePath};;;;{contentType};;")
        else:
            strOrigin, strLicense, strComment, strUrl = _Flatten(entry.License)
            lines.append(f"{entry.RelativePath};;{strOrigin};{strLicense};{contentType};{strComment};{strUrl}")

    lines.append("\n")
    lines.append(f"Duplicated files ({len(duplicatedEntryDict)})")
    for key in sortedDuplicatedFiles:
        lines.append(f"{key};;;;{_GetContentTypeByExtension(extensionDict, key)};;")
        for entry in duplicatedEntryDict[key]:
            contentType = _GetContentTypeByExtension(extensionDict, entry.RelativePath)
            if entry.License is None:
                lines.append(f";{entry.RelativePath};;;{contentType};;")
            else:
                strOrigin, strLicense, strComment, strUrl = _Flatten(entry.License)
                lines.append(f";{entry.RelativePath};{strOrigin};{strLicense};{contentType};{strComment};{strUrl}")

    IOUtil.WriteFile(dstFilename, "\n".join(lines))


def _PrintIssueDirectories(fileList: list[Resource], dict: dict[str, list[Resource]]) -> None:
    uniqueDirs: set[str] = set()
    for entry in fileList:
        if entry.SourceDirectory not in uniqueDirs:
            uniqueDirs.add(entry.SourceDirectory)

    for value in list(dict.values()):
        for entry in value:
            if entry.SourceDirectory not in uniqueDirs:
                uniqueDirs.add(entry.SourceDirectory)

    if len(uniqueDirs) > 0:
        print("Investigate license for the following directories:")
        uniqueDirSet = list(uniqueDirs)
        uniqueDirSet.sort()
        for dirEntry in uniqueDirSet:
            print(f"  {dirEntry}")


def _ProcessDictLicenses(log: Log, licenseFilename: str, dict: dict[str, list[Resource]]) -> None:
    LicenseManager()
    newLicenseDirs: set[str] = set()
    for _key, entryList in dict.items():
        firstLicenseEntry = None
        noLicenseEntries = []
        for entry in entryList:
            if entry.License is None:
                noLicenseEntries.append(entry)
            elif firstLicenseEntry is None:
                firstLicenseEntry = entry
            elif firstLicenseEntry.License is None or not entry.License.Compare(firstLicenseEntry.License):
                raise Exception(f"The license of the duplicated resource at {firstLicenseEntry.SourceDirectory} and {entry.SourceDirectory} is different")

        if len(noLicenseEntries) > 0 and firstLicenseEntry is not None:
            log.LogPrint(f"Info: Found duplicated resource missing a license, cloning source license from {firstLicenseEntry.SourcePath}")
            for entry in noLicenseEntries:
                entry.License = firstLicenseEntry.License
                if entry.SourceDirectory not in newLicenseDirs:
                    newLicenseDirs.add(entry.SourceDirectory)
                    newLicenseFile = IOUtil.Join(entry.SourceDirectory, licenseFilename)
                    if os.path.isfile(newLicenseFile):
                        raise Exception(f"Could not create a new license at {newLicenseFile} since one already exist")
                    if firstLicenseEntry.License is None:
                        raise Exception("internal error")

                    oldLicenseFile = IOUtil.Join(firstLicenseEntry.SourceDirectory, licenseFilename)

                    IOUtil.CopySmallFile(oldLicenseFile, newLicenseFile)
                    # licenseManager.SaveLicense(newLicenseFile, firstLicenseEntry.License)


def _FilterDictBasedOnLicense(sourceDict: dict[str, list[Resource]]) -> dict[str, list[Resource]]:
    newDict: dict[str, list[Resource]] = {}
    for key, entryList in sourceDict.items():
        newList = []
        for entry in entryList:
            if entry.License is None:
                newList.append(entry)
        if len(newList) > 0:
            if len(entryList) != len(newList):
                raise Exception("Internal error, license for duplicated files have not been fixed prior to this filtering")
            newDict[key] = newList
    return newDict


def _PrintListFixTags(entries: list[Resource]) -> None:
    for entry in entries:
        if entry.License is not None:
            for licenseEntry in entry.License.Licenses:
                if licenseEntry is not None and "fix" in licenseEntry.TagsIdList:
                    print(f"Fix: {entry.SourcePath}")


def _PrintFixTags(uniqueEntries: list[Resource], duplicatedEntriesDict: dict[str, list[Resource]]) -> None:
    _PrintListFixTags(uniqueEntries)
    for entries in list(duplicatedEntriesDict.values()):
        _PrintListFixTags(entries)


def _AddLicenses(dict: dict[str, list[Resource]], entries: list[Resource]) -> None:
    for entry in entries:
        if entry.License is not None:
            for licenseEntry in entry.License.Licenses:
                if licenseEntry is not None:
                    if licenseEntry.License not in dict:
                        dict[licenseEntry.License] = [entry]
                    else:
                        dict[licenseEntry.License].append(entry)


def _ExpandLicense(key: str) -> bool:
    key = key.lower()
    return (
        key != "bsd-3-clause"
        and key != "mit"
        and key != "mixed"
        and key != "cc0-1.0"
        and key != "cc-by-3.0"
        and key != "cc-by-sa-4.0"
        and key != "modified 3-clause bsd-license"
    )


def _PrintLicenses(uniqueEntries: list[Resource], duplicatedEntriesDict: dict[str, list[Resource]]) -> None:
    licenseDict: dict[str, list[Resource]] = {}
    _AddLicenses(licenseDict, uniqueEntries)
    for entries in list(duplicatedEntriesDict.values()):
        _AddLicenses(licenseDict, entries)

    sortedKeys = list(licenseDict.keys())
    sortedKeys.sort()

    print("License")
    for key in sortedKeys:
        value = licenseDict[key]
        print(f"- '{key}' entries: {len(value)}")
        if _ExpandLicense(key):
            for entry in value:
                print(f"  * {entry.SourcePath}")


def _Process(
    log: Log,
    ignoreDirList: list[str],
    inputDirectory: str,
    extensions: list[tuple[str, str]],
    ignoreFiles: list[str],
    licenseFilename: str,
    listLicenses: bool,
    saveCSVs: bool,
) -> None:
    if not os.path.isdir(inputDirectory):
        raise Exception(f"'{inputDirectory}' is not a directory")

    print("Please run this on a clean checkout as compiler obj files could be found otherwise.")
    extensionList = _GetExtensionList(extensions)
    files = _ScanForFiles(inputDirectory, extensionList, ignoreFiles, ignoreDirList)
    log.LogPrint(f"Found {len(files)} resource files")

    uniqueFiles: list[str] = []
    duplicatedFilesDict = _BuildUniqueFileDict(log, files, uniqueFiles)

    log.LogPrint(f"Found {len(uniqueFiles)} unique resource files")
    log.LogPrint(f"Found {len(duplicatedFilesDict)} duplicated resource files")

    # Check license information
    resourceDirectories = _BuildResourceDirectorySet(uniqueFiles, duplicatedFilesDict)
    directoryLicenseDict = _BuildDirectoryLicenseDict(log, resourceDirectories, licenseFilename)

    log.LogPrint(f"Found {len(directoryLicenseDict)} license files")

    uniqueEntries = _TagListWithLicenses(inputDirectory, uniqueFiles, directoryLicenseDict)
    duplicatedEntriesDict = _TagDictWithLicenses(inputDirectory, duplicatedFilesDict, directoryLicenseDict)

    _ProcessDictLicenses(log, licenseFilename, duplicatedEntriesDict)

    # All resources
    if saveCSVs:
        _WriteCSV("resources.csv", extensions, uniqueEntries, duplicatedEntriesDict)

    # Remove all entries that have a license associated
    noLicenseUniqueEntries = [entry for entry in uniqueEntries if entry.License is None]
    noLicenseDuplicatedEntriesDict = _FilterDictBasedOnLicense(duplicatedEntriesDict)

    if len(noLicenseUniqueEntries) > 0:
        print(f"WARNING: Found {len(noLicenseUniqueEntries)} unique resource files with no license attached")
    if len(noLicenseDuplicatedEntriesDict) > 0:
        print(f"WARNING: Found {len(noLicenseDuplicatedEntriesDict)} duplicated resource files with no license attached")

    if saveCSVs:
        _WriteCSV("resourcesIssues.csv", extensions, noLicenseUniqueEntries, noLicenseDuplicatedEntriesDict)

    _PrintIssueDirectories(noLicenseUniqueEntries, noLicenseDuplicatedEntriesDict)

    _PrintFixTags(uniqueEntries, duplicatedEntriesDict)

    if listLicenses:
        _PrintLicenses(uniqueEntries, duplicatedEntriesDict)


def _ScanPackageContent(log: Log, licenseConfig: LicenseConfig, scanPackageList: list[Package], disableWrite: bool) -> None:
    for package in scanPackageList:
        packageLicenseList = _BuildPackageLicenseList(log, package, licenseConfig)

        if len(packageLicenseList) > 0:
            # Create initial empty license
            packageLicense = JsonComplexLicense([], "Merged complex license for everything that could be used in a screenshot")
            for license in packageLicenseList:
                packageLicense.Merge(license)

            licenseManager = LicenseManager()
            if not disableWrite and package.AbsolutePath is not None:
                dstFilename = IOUtil.Join(package.AbsolutePath, licenseConfig.LicenseFilename)
                licenseManager.WriteLicenseIfChanged(log, dstFilename, packageLicense)
        elif package.AllowCheck and package.Type == PackageType.Executable and package.AbsolutePath is not None and not disableWrite:
            # no licenses, so remove the license if it exist
            dstFilename = IOUtil.Join(package.AbsolutePath, licenseConfig.LicenseFilename)
            if IOUtil.IsFile(dstFilename):
                IOUtil.RemoveFile(dstFilename)


def GetExtensionList() -> list[str]:
    return _GetExtensionList(__g_extensions)


def Scan(
    log: Log,
    miniToolConfig: ToolMinimalConfig,
    directory: str,
    scanPackageList: list[Package],
    repairEnabled: bool,
    disableWrite: bool,
    listLicenses: bool,
    saveCSVs: bool,
    licenseConfig: LicenseConfig,
) -> None:
    """
    Run through all resource files and check if the licenses are specified.
    """

    # log: Log, ignoreDirList: List[str], inputDirectory: str, extensions: List[Tuple[str,str]],
    #        ignoreFiles: List[str], licenseFilename: str, listLicenses: List[str],
    #        saveCSVs: bool

    log.LogPrint("Building package content licenses")
    _ScanPackageContent(log, licenseConfig, scanPackageList, disableWrite)

    log.LogPrint("Running resource license scan")

    ignoreDirs = list(miniToolConfig.IgnoreDirectories)

    for rootDirectory in miniToolConfig.RootDirectories:
        if rootDirectory.BashName == "$FSL_GRAPHICS_SDK":
            for ignoreDir in __g_ignoreDir:
                ignoreDirs.append(IOUtil.Join(rootDirectory.ResolvedPath, ignoreDir))

    _Process(log, ignoreDirs, directory, __g_extensions, __g_ignore, licenseConfig.LicenseFilename, listLicenses, saveCSVs)
