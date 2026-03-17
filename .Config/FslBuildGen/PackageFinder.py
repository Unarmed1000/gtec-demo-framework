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

from typing import cast

# from FslBuildGen import IOUtil
from FslBuildGen.Exceptions import DependencyNotFoundException, PackageHasMultipleDefinitionsException, PackageLoaderFailedToLocatePackageException
from FslBuildGen.Generator.GeneratorPluginBase import GeneratorPluginBase
from FslBuildGen.Log import Log
from FslBuildGen.PackageFile import PackageFile
from FslBuildGen.PackageLocationCache import PackageLocationCache, PackageLocationCachePath
from FslBuildGen.ToolConfig import ToolConfigPackageConfiguration, ToolConfigPackageLocation
from FslBuildGen.Xml.XmlGenFile import XmlGenFile


class PackageFinder:
    def __init__(
        self,
        log: Log,
        platform: GeneratorPluginBase,
        configTypeName: str,
        packageConfigDict: dict[str, ToolConfigPackageConfiguration],
        genFilename: str,
        testModeEnabled: bool,
    ) -> None:
        self.Log = log
        self.ConfigTypeName = configTypeName
        # The locations available in this configuration
        self.PackageLocations: list[ToolConfigPackageLocation] = [] if configTypeName not in packageConfigDict else packageConfigDict[configTypeName].Locations
        self.GenFilename = genFilename
        self.PackageLocationCache = PackageLocationCache(log, self.PackageLocations, self.GenFilename)
        self.__TestModeEnabled = testModeEnabled
        self.__InitialSearchLocations: list[ToolConfigPackageLocation] = self.__BuildInitialSearchLocations(self.PackageLocations)

        if configTypeName not in packageConfigDict:
            log.DoPrintWarning(f"The configuration name '{configTypeName}' is unknown, expected one of {list(packageConfigDict.keys())}")

    def __BuildInitialSearchLocations(self, selectedConfigurationLocationList: list[ToolConfigPackageLocation]) -> list[ToolConfigPackageLocation]:
        """Scan the package configurations and build a list of unique locations for the initial scan"""
        uniqueLocationDict: dict[str, ToolConfigPackageLocation] = {}
        for location in selectedConfigurationLocationList:
            resolvedPathId = location.ResolvedPath
            if resolvedPathId in uniqueLocationDict:
                raise PackageHasMultipleDefinitionsException(
                    f"The package location '{resolvedPathId}' was already added by {location.Name} and {uniqueLocationDict[resolvedPathId].Name} tried to add it again."
                )
            uniqueLocationDict[resolvedPathId] = location

        sortedList = list(uniqueLocationDict.values())
        sortedList.sort(key=lambda s: -len(s.ResolvedPath))
        return sortedList

    def LocateInputFiles(self, files: list[str]) -> list[PackageFile]:
        oldFiles = files
        resultFileList: list[PackageFile] = []
        for file in oldFiles:
            location = self.__LocateInputFileLocation(file)
            resultFileList.append(PackageFile(file, None, location))
        return resultFileList

    def __LocateInputFileLocation(self, file: str) -> ToolConfigPackageLocation:
        # print(file)
        for location in self.__InitialSearchLocations:
            if file.startswith(location.ResolvedPathEx):
                return location
        scannedLocations = []
        for location in self.__InitialSearchLocations:
            scannedLocations.append(location.ResolvedPathEx)
        raise Exception(f"Could not find package location for file {file} scanned locations: {scannedLocations}")

    def TryLocateMissingPackagesByName(self, packageName: str) -> PackageFile | None:
        # Check to see if the package can be found
        foundLocation = self.PackageLocationCache.TryLocatePackage(packageName)
        if foundLocation is not None and foundLocation.FoundPackageFilePath is not None:
            return PackageFile(foundLocation.FoundPackageFilePath, packageName, foundLocation.SourceLocation)
        return None

    def LocateMissingPackages(self, missingDict: dict[str, XmlGenFile]) -> list[PackageFile]:
        """Given a dict of missing package requests, try to locate them"""
        files = []
        for packageName in sorted(missingDict.keys()):
            foundLocation = self.__LocateMissingPackage(packageName, missingDict)
            files.append(PackageFile(cast(str, foundLocation.FoundPackageFilePath), packageName, foundLocation.SourceLocation))
        return files

    def __LocateMissingPackage(self, packageName: str, missingDict: dict[str, XmlGenFile], allowRetry: bool = True) -> PackageLocationCachePath:
        """We only provide the missing dict here so that we can use it in case of a exception"""
        # Check to see if the package can be found
        foundLocation = self.PackageLocationCache.TryLocatePackage(packageName)
        if foundLocation is not None and foundLocation.FoundPackageFilePath is not None:
            return foundLocation

        # So we did not locate a package file, it's time to fire and exception,
        # but lets try to provide some useful feedback.
        candidateList = self.PackageLocationCache.FindCandidates(packageName, True)
        if len(candidateList) != 1 or packageName not in candidateList:
            raise DependencyNotFoundException(missingDict[packageName].Name, packageName, candidateList)
        else:
            # So the candidate list was able to find the package,
            # that indicates that our finder/cache combo has a bug.
            if allowRetry and not self.__TestModeEnabled:
                self.Log.LogPrintWarning("Optimized package locator failed to locate package, but we have reason to assume that it might exist so trying again")
                return self.__LocateMissingPackage(packageName, missingDict, False)
            raise PackageLoaderFailedToLocatePackageException(missingDict[packageName].Name, packageName)

    def GetKnownPackageFiles(self, theFiles: list[PackageFile]) -> list[PackageFile]:
        """Get all the files associated with the typeId then merge it with the supplied file list."""

        knownPackageLocationList: list[PackageLocationCachePath] = self.PackageLocationCache.GetKnownPackageLocations()
        result = list(theFiles)
        for packageLocation in knownPackageLocationList:
            result.append(PackageFile(cast(str, packageLocation.FoundPackageFilePath), packageLocation.PackageName, packageLocation.SourceLocation))

        return result

    def TryLocatePackageFileByName(self, packageName: str) -> PackageFile | None:
        foundLocation = self.PackageLocationCache.TryLocatePackage(packageName)
        if foundLocation is None:
            return None
        return PackageFile(cast(str, foundLocation.FoundPackageFilePath), packageName, foundLocation.SourceLocation)

    def LocatePackageFileByName(self, packageName: str) -> PackageFile:
        found = self.TryLocatePackageFileByName(packageName)
        if found is None:
            raise Exception(f"Could not locate package {packageName}")
        return found
