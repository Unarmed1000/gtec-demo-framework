#!/usr/bin/env python3
# ****************************************************************************************************************************************************
# Copyright (c) 2014 Freescale Semiconductor, Inc.
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
#    * Neither the name of the Freescale Semiconductor, Inc. nor the names of
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

from enum import Enum

from FslBuildGen.Exceptions import UnknownTypeException


class PackageType(Enum):
    ExternalFlavorConstraint = 0  # For internal graph resolution only.
    TopLevel = 1  # a top level package, this is a internal thing (used for total
    # verification)
    Library = 2  # a static library
    Executable = 3  # a executable
    ExternalLibrary = 4  # a unmanaged static library
    HeaderLibrary = 5  # a header only library
    ToolRecipe = 6  # a external tool recipe
    # ExeLibCombo =  6               # a executeable and static library combo (not supported yet)

    @staticmethod
    def ToString(value: PackageType) -> str:
        if value == PackageType.TopLevel:
            return "TopLevel"
        elif value == PackageType.Library:
            return "Library"
        elif value == PackageType.Executable:
            return "Executable"
        elif value == PackageType.ExternalLibrary:
            return "ExternalLibrary"
        elif value == PackageType.HeaderLibrary:
            return "HeaderLibrary"
        elif value == PackageType.ToolRecipe:
            return "ToolRecipe"
        raise UnknownTypeException(f"Unknown PackageType: {value}")

    @staticmethod
    def FromString(value: str) -> PackageType:
        if value == "TopLevel":
            return PackageType.TopLevel
        elif value == "Library":
            return PackageType.Library
        elif value == "Executable":
            return PackageType.Executable
        elif value == "ExternalLibrary":
            return PackageType.ExternalLibrary
        elif value == "HeaderLibrary":
            return PackageType.HeaderLibrary
        elif value == "ToolRecipe":
            return PackageType.ToolRecipe
        raise Exception(f"Unknown PackageType: {value}")

    @staticmethod
    def AllStrings() -> list[str]:
        return [
            PackageType.ToString(PackageType.TopLevel),
            PackageType.ToString(PackageType.Library),
            PackageType.ToString(PackageType.Executable),
            PackageType.ToString(PackageType.ExternalLibrary),
            PackageType.ToString(PackageType.HeaderLibrary),
            PackageType.ToString(PackageType.ToolRecipe),
        ]


class PackageInstanceType(Enum):
    Normal = 0
    Flavor = 1
    FlavorSingleton = 2

    @staticmethod
    def ToString(value: PackageInstanceType) -> str:
        if value == PackageInstanceType.Normal:
            return "Normal"
        elif value == PackageInstanceType.Flavor:
            return "Flavor"
        elif value == PackageInstanceType.FlavorSingleton:
            return "FlavorSingleton"
        raise UnknownTypeException(f"Unknown PackageInstanceType: {value}")


# Beware that code relies on the more accessible accesstype being smaller value (so public < private < link)
class DependencyOutputType(Enum):
    Reference = 0
    Analyzer = 1
    # Content = 2
    # EmbeddedResource = 3
    # Compile = 4
    # None = 5

    @staticmethod
    def ToString(value: DependencyOutputType) -> str:
        if value == DependencyOutputType.Reference:
            return "Reference"
        if value == DependencyOutputType.Analyzer:
            return "Analyzer"
        # if value == DependencyOutputType.Content:
        #     return "Content"
        # if value == DependencyOutputType.EmbeddedResource:
        #     return "EmbeddedResource"
        # if value == DependencyOutputType.Compile:
        #     return "Compile"
        # if value == DependencyOutputType.None:
        #     return "None"
        raise Exception(f"Unknown DependencyOutputType: {value}")

    @staticmethod
    def FromString(value: str) -> DependencyOutputType:
        if value == "Reference":
            return DependencyOutputType.Reference
        elif value == "Analyzer":
            return DependencyOutputType.Analyzer
        raise Exception(f"Unknown DependencyOutputType: {value}")


# Beware that code relies on the more accessible accesstype being smaller value (so public < private < link)
class AccessType(Enum):
    Public = 0
    Private = 1
    Link = 2  # (only valid for Dependency)

    # @staticmethod
    # def IsLessThan(lhs: 'AccessType', rhs: 'AccessType' )
    #    return lhs.value < rhs.value

    @staticmethod
    def ToString(value: AccessType) -> str:
        if value == AccessType.Public:
            return "Public"
        elif value == AccessType.Private:
            return "Private"
        elif value == AccessType.Link:
            return "Link"
        raise Exception(f"Unknown AccessType: {value}")


class IncludePriority(Enum):
    After = 0  # the default where include directories are appended to the current list
    Before = 1  # include directory is inserted at the front of the current list allowing us to override system includes.

    @staticmethod
    def ToString(value: IncludePriority) -> str:
        if value == IncludePriority.After:
            return "After"
        elif value == IncludePriority.Before:
            return "Before"
        raise Exception(f"Unknown IncludePriority: '{value}'")

    @staticmethod
    def TryFromString(value: str) -> IncludePriority | None:
        if value == "After":
            return IncludePriority.After
        elif value == "Before":
            return IncludePriority.Before
        return None


class DependencyCondition:
    FindPackageAllowed = "AllowFindPackage"
    FindPackageNotAllowed = "!AllowFindPackage"


class ExternalDependencyType(Enum):
    StaticLib = 1
    DLL = 2
    Headers = 3
    # C# specific
    Assembly = 4
    PackageReference = 5
    # CMake specific
    CMakeFindLegacy = 6
    CMakeFindModern = 7

    @staticmethod
    def ToString(value: ExternalDependencyType) -> str:
        if value == ExternalDependencyType.StaticLib:
            return "StaticLib"
        elif value == ExternalDependencyType.DLL:
            return "DLL"
        elif value == ExternalDependencyType.Headers:
            return "Headers"
        elif value == ExternalDependencyType.Assembly:
            return "Assembly"
        elif value == ExternalDependencyType.PackageReference:
            return "PackageReference"
        elif value == ExternalDependencyType.CMakeFindLegacy:
            return "CMakeFindLegacy"
        elif value == ExternalDependencyType.CMakeFindModern:
            return "CMakeFindModern"
        raise Exception(f"Unknown ExternalDependencyType: {value}")

    @staticmethod
    def TryFromString(value: str) -> ExternalDependencyType | None:
        if value == "StaticLib":
            return ExternalDependencyType.StaticLib
        elif value == "DLL":
            return ExternalDependencyType.DLL
        elif value == "Headers":
            return ExternalDependencyType.Headers
        elif value == "Assembly":
            return ExternalDependencyType.Assembly
        elif value == "PackageReference":
            return ExternalDependencyType.PackageReference
        elif value == "CMakeFindLegacy":
            return ExternalDependencyType.CMakeFindLegacy
        elif value == "CMakeFindModern":
            return ExternalDependencyType.CMakeFindModern
        return None

    @staticmethod
    def FromString(value: str) -> ExternalDependencyType:
        result = ExternalDependencyType.TryFromString(value)
        if result is not None:
            return result
        raise Exception(f"Unknown external dependency type: '{value}' expected: StaticLib, DLL, Headers, Assembly, CMakeFindLegacy, CMakeFindModern")

    @staticmethod
    def AllStrings() -> list[str]:
        return [
            ExternalDependencyType.ToString(ExternalDependencyType.StaticLib),
            ExternalDependencyType.ToString(ExternalDependencyType.DLL),
            ExternalDependencyType.ToString(ExternalDependencyType.Headers),
            ExternalDependencyType.ToString(ExternalDependencyType.Assembly),
            ExternalDependencyType.ToString(ExternalDependencyType.PackageReference),
            ExternalDependencyType.ToString(ExternalDependencyType.CMakeFindLegacy),
            ExternalDependencyType.ToString(ExternalDependencyType.CMakeFindModern),
        ]


class VariantType(Enum):
    Normal = 1
    Virtual = 2


class CompilerNames:
    VisualStudio = "VisualStudio"


class VisualStudioVersion:
    VS2015 = 2015
    VS2017 = 2017
    VS2019 = 2019
    VS2022 = 2022
    VS2026 = 2026
    DEFAULT = VS2026

    AllEntries = [VS2015, VS2017, VS2019, VS2022, VS2026]

    @staticmethod
    def ToString(value: int) -> str:
        if value == VisualStudioVersion.VS2015:
            return "2015"
        elif value == VisualStudioVersion.VS2017:
            return "2017"
        elif value == VisualStudioVersion.VS2019:
            return "2019"
        elif value == VisualStudioVersion.VS2022:
            return "2022"
        elif value == VisualStudioVersion.VS2026:
            return "2026"
        return "Unknown"

    @staticmethod
    def FromString(strValue: str) -> int:
        result = VisualStudioVersion.TryParse(strValue)
        if result is not None:
            return result
        else:
            raise Exception(f"Unsupported visual studio version {strValue}")

    @staticmethod
    def TryParse(strValue: str) -> int | None:
        if strValue == "2015":
            return VisualStudioVersion.VS2015
        elif strValue == "2017":
            return VisualStudioVersion.VS2017
        elif strValue == "2019":
            return VisualStudioVersion.VS2019
        elif strValue == "2022":
            return VisualStudioVersion.VS2022
        elif strValue == "2026":
            return VisualStudioVersion.VS2026
        return None


class BuildPlatformType(Enum):
    Unix = 0
    Windows = 1
    Unknown = -1


class LegacyGeneratorType:
    Default = 0
    Experimental = 1
    Deprecated = 2


class BuildVariantType:
    Static = 0
    Dynamic = 1


class PackageLanguage(Enum):
    CPP = 0
    CSharp = 1

    @staticmethod
    def ToString(value: PackageLanguage) -> str:
        if value == PackageLanguage.CPP:
            return "C++"
        elif value == PackageLanguage.CSharp:
            return "C#"
        return "Unknown"

    @staticmethod
    def FromString(strPackageLanguage: str) -> PackageLanguage:
        if strPackageLanguage == "C++":
            return PackageLanguage.CPP
        elif strPackageLanguage == "C#":
            return PackageLanguage.CSharp
        else:
            raise Exception(f"Unsupported package language {strPackageLanguage}")


class OptimizationType:
    Disabled = 0
    Default = 1
    Full = 2

    @staticmethod
    def ToString(value: int) -> str:
        if value == OptimizationType.Disabled:
            return "Disabled"
        elif value == OptimizationType.Default:
            return "Default"
        elif value == OptimizationType.Full:
            return "Full"
        else:
            raise Exception(f"Unsupported OptimizationType {value}")


class MagicStrings:
    ProjectRoot = "${PROJECT_ROOT}"
    VSDefaultCPPTemplate = "DF"
    VSDefaultCPPLinuxTemplate = "DF-Linux"
    DefaultSDKConfiguration = "sdk"


class CheckType(Enum):
    Normal = 1
    Forced = 2


class FilterMethod(Enum):
    AllowAll = 1
    AllowList = 2
    BannedList = 3


class ScanMethod:
    Directory = 0
    OneSubDirectory = 1
    AllSubDirectories = 2

    @staticmethod
    def TryToString(value: int, returnValueStringIfUnknown: bool = False) -> str | None:
        if value == ScanMethod.Directory:
            return "Directory"
        elif value == ScanMethod.OneSubDirectory:
            return "OneSubDirectory"
        elif value == ScanMethod.AllSubDirectories:
            return "AllSubDirectories"
        else:
            return None if not returnValueStringIfUnknown else f"{value}"

    @staticmethod
    def ToString(value: int) -> str:
        if value == ScanMethod.Directory:
            return "Directory"
        elif value == ScanMethod.OneSubDirectory:
            return "OneSubDirectory"
        elif value == ScanMethod.AllSubDirectories:
            return "AllSubDirectories"
        else:
            raise Exception(f"Unsupported ScanMethod {value}")

    @staticmethod
    def FromString(value: str) -> int:
        if value == "Directory":
            return ScanMethod.Directory
        elif value == "OneSubDirectory":
            return ScanMethod.OneSubDirectory
        elif value == "AllSubDirectories":
            return ScanMethod.AllSubDirectories
        else:
            raise Exception(f"Unsupported ScanMethod {value}")


class PackageRequirementTypeString:
    Invalid = "invalid"
    Feature = "feature"
    Extension = "extension"


class PackageRequirementType:
    Invalid = 0
    Feature = 1
    Extension = 2

    @staticmethod
    def TryFromString(value: str) -> int:
        if value == PackageRequirementTypeString.Feature:
            return PackageRequirementType.Feature
        elif value == PackageRequirementTypeString.Extension:
            return PackageRequirementType.Extension
        return PackageRequirementType.Invalid

    @staticmethod
    def FromString(value: str) -> int:
        result = PackageRequirementType.TryFromString(value)
        if result != PackageRequirementType.Invalid:
            return result
        raise Exception(f"Unsupported PackageRequirementType {value}")


class PackageCreationYearString:
    NotDefined = "NotDefined"


class BuildVariantConfig(Enum):
    Debug = 0
    Release = 1
    Coverage = 2

    @staticmethod
    def ToString(value: BuildVariantConfig) -> str:
        if value == BuildVariantConfig.Release:
            return "release"
        elif value == BuildVariantConfig.Debug:
            return "debug"
        elif value == BuildVariantConfig.Coverage:
            return "coverage"
        else:
            raise Exception(f"Unsupported BuildVariantConfig '{value}'")

    @staticmethod
    def FromString(value: str) -> BuildVariantConfig:
        if value == "release":
            return BuildVariantConfig.Release
        elif value == "debug":
            return BuildVariantConfig.Debug
        elif value == "coverage":
            return BuildVariantConfig.Coverage
        else:
            raise Exception(f"Unsupported BuildVariantConfig '{value}'")


class BuildVariantConfigDefaults:
    DefaultSetting = BuildVariantConfig.Release


class CMakeTargetType(Enum):
    Project = 0
    Install = 1

    @staticmethod
    def FromString(value: str) -> CMakeTargetType:
        if value == "project":
            return CMakeTargetType.Project
        elif value == "install":
            return CMakeTargetType.Install
        else:
            raise Exception(f"Unsupported CMakeTargetType '{value}'")


class BuildRecipePipelineCommand(Enum):
    Invalid = 0
    Download = 1
    GitClone = 2
    Unpack = 3
    CMakeBuild = 4
    Source = 5
    Combine = 6
    Copy = 7
    JoinCopy = 8
    JoinUnpack = 9
    JoinGitApply = 10
    JoinDelete = 11


class BuildRecipeValidateCommand(Enum):
    Invalid = 0
    EnvironmentVariable = 1
    Path = 2
    FindFileInPath = 3
    FindExecutableFileInPath = 4
    AddHeaders = 5
    AddLib = 6
    AddDLL = 7
    AddTool = 8


class BuildRecipeValidateMethod:
    IsDirectory = 0
    IsFile = 1
    Exists = 2

    @staticmethod
    def FromString(value: str) -> int:
        if value == "IsDirectory":
            return BuildRecipeValidateMethod.IsDirectory
        elif value == "IsFile":
            return BuildRecipeValidateMethod.IsFile
        elif value == "Exists":
            return BuildRecipeValidateMethod.Exists
        raise Exception(f"Unsupported BuildRecipeValidateMethod '{value}'")

    @staticmethod
    def ToString(value: int) -> str:
        result = BuildRecipeValidateMethod.TryToString(value)
        if result is None:
            raise Exception(f"Unsupported BuildRecipeValidateMethod '{value}'")
        return result

    @staticmethod
    def TryToString(value: int, returnValueStringIfUnknown: bool = False) -> str | None:
        if value == BuildRecipeValidateMethod.IsDirectory:
            return "IsDirectory"
        elif value == BuildRecipeValidateMethod.IsFile:
            return "IsFile"
        elif value == BuildRecipeValidateMethod.Exists:
            return "Exists"
        return None if not returnValueStringIfUnknown else f"{value}"


class BuildThreads:
    NotDefined = 0
    Auto = -1

    @staticmethod
    def FromString(value: str) -> int:
        if value == "auto" or value == "Auto":
            return BuildThreads.Auto
        try:
            return int(value)
        except ValueError as ex:
            raise Exception(f"Unsupported BuildThreads '{value}'") from ex


class BoolStringHelper:
    @staticmethod
    def FromString(value: str) -> bool:
        result = BoolStringHelper.TryFromString(value)
        if result is None:
            raise Exception(f"Failed to parse boolean string '{value}' to bool value")
        return result

    @staticmethod
    def TryFromString(value: str) -> bool | None:
        if value == "true":
            return True
        elif value == "false":
            return False
        return None


class PackageString:
    PLATFORM_WILDCARD = "*"
    PLATFORM_SEPARATOR = ";"


class GeneratorNameString:
    Default = "default"
    CMake = "cmake"
    Legacy = "legacy"

    @staticmethod
    def AllStrings() -> list[str]:
        return [GeneratorNameString.Default, GeneratorNameString.CMake, GeneratorNameString.Legacy]


class GeneratorType(Enum):
    Default = 0
    CMake = 1
    Legacy = 2

    @staticmethod
    def FromString(value: str) -> GeneratorType:
        if value == GeneratorNameString.Default:
            return GeneratorType.Default
        elif value == GeneratorNameString.CMake:
            return GeneratorType.CMake
        elif value == GeneratorNameString.Legacy:
            return GeneratorType.Legacy
        raise Exception(f"Unsupported GeneratorName '{value}'")

    @staticmethod
    def ToString(value: GeneratorType) -> str:
        result = GeneratorType.TryToString(value)
        if result is None:
            raise Exception(f"Unsupported GeneratorName '{value}'")
        return result

    @staticmethod
    def TryToString(value: GeneratorType, returnValueStringIfUnknown: bool = False) -> str | None:
        if value == GeneratorType.Default:
            return GeneratorNameString.Default
        elif value == GeneratorType.CMake:
            return GeneratorNameString.CMake
        elif value == GeneratorType.Legacy:
            return GeneratorNameString.Legacy
        return None if not returnValueStringIfUnknown else f"{value}"


class SpecialFiles:
    Natvis = "Package.natvis"


class ClangTidyProfileString:
    Fast = "fast"
    Strict = "strict"

    @staticmethod
    def AllStrings() -> list[str]:
        return [ClangTidyProfileString.Fast, ClangTidyProfileString.Strict]


class ClangTidyProfile(Enum):
    Fast = 0
    Strict = 1

    @staticmethod
    def FromString(value: str) -> ClangTidyProfile:
        if value == ClangTidyProfileString.Fast:
            return ClangTidyProfile.Fast
        elif value == ClangTidyProfileString.Strict:
            return ClangTidyProfile.Strict
        raise Exception(f"Unsupported ClangTidyProfile '{value}'")

    @staticmethod
    def ToString(value: ClangTidyProfile) -> str:
        result = ClangTidyProfile.TryToString(value)
        if result is None:
            raise Exception(f"Unsupported ClangTidyProfile '{value}'")
        return result

    @staticmethod
    def TryToString(value: ClangTidyProfile, returnValueStringIfUnknown: bool = False) -> str | None:
        if value == ClangTidyProfile.Fast:
            return ClangTidyProfileString.Fast
        elif value == ClangTidyProfile.Strict:
            return ClangTidyProfileString.Strict
        return None if not returnValueStringIfUnknown else f"{value}"


class FilterMode(Enum):
    Disabled = 0  # Do not use a filter
    TrimUnrequestedPackages = 1  # Trim packages that where not imported because of a user request


class SourceGenerationVisiblePropertyName(Enum):
    """The concept level build properties that can be made visible to a source generator.
    Each backend is responsible for mapping these to its own property name.
    """

    BuildConfiguration = 0
    TargetFramework = 1
    RootNamespace = 2
    AssemblyName = 3

    @staticmethod
    def ToString(value: SourceGenerationVisiblePropertyName) -> str:
        if value == SourceGenerationVisiblePropertyName.BuildConfiguration:
            return "BuildConfiguration"
        elif value == SourceGenerationVisiblePropertyName.TargetFramework:
            return "TargetFramework"
        elif value == SourceGenerationVisiblePropertyName.RootNamespace:
            return "RootNamespace"
        elif value == SourceGenerationVisiblePropertyName.AssemblyName:
            return "AssemblyName"
        raise Exception(f"Unknown SourceGenerationVisiblePropertyName: {value}")

    @staticmethod
    def TryFromString(value: str) -> SourceGenerationVisiblePropertyName | None:
        if value == "BuildConfiguration":
            return SourceGenerationVisiblePropertyName.BuildConfiguration
        elif value == "TargetFramework":
            return SourceGenerationVisiblePropertyName.TargetFramework
        elif value == "RootNamespace":
            return SourceGenerationVisiblePropertyName.RootNamespace
        elif value == "AssemblyName":
            return SourceGenerationVisiblePropertyName.AssemblyName
        return None

    @staticmethod
    def GetAllNames() -> list[str]:
        return [SourceGenerationVisiblePropertyName.ToString(entry) for entry in SourceGenerationVisiblePropertyName]


class GrpcServices(Enum):
    Both = 0
    Server = 1
    Client = 2
    NoneGen = 3

    @staticmethod
    def ToString(value: GrpcServices) -> str:
        if value == GrpcServices.Both:
            return "Both"
        elif value == GrpcServices.Server:
            return "Server"
        elif value == GrpcServices.Client:
            return "Client"
        elif value == GrpcServices.NoneGen:
            return "None"
        raise Exception(f"Unknown GrpcServices: {value}")

    @staticmethod
    def TryFromString(value: str) -> GrpcServices | None:
        if value == "Both":
            return GrpcServices.Both
        elif value == "Server":
            return GrpcServices.Server
        elif value == "Client":
            return GrpcServices.Client
        elif value == "None":
            return GrpcServices.NoneGen
        return None

    @staticmethod
    def FromString(value: str) -> GrpcServices:
        result = GrpcServices.TryFromString(value)
        if result is not None:
            return result
        raise Exception(f"Unknown external dependency type: '{value}' expected: Both, Server, Client, None")
