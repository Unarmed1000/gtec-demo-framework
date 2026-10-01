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

# How the version of an external dependency is written for each consumer of it.
#
# A gen file holds one version concept (see SemanticVersionPattern): a plain minimum version, an exact version, a wildcard or a range. The consumers
# do not share a syntax for it, so each one has a function here that returns the text for that consumer or raises a VersionRenderException that tells
# why the consumer can not be given the version. A version is never approximated and never left out.
#
#   typed        NuGet package reference   CMake find_package   assembly reference   packages.config entry
#   1.2          1.2                       1.2                  1.2                  1.2
#   [1.2]        [1.2]                     1.2 EXACT            1.2                  error
#   4.*          4.*                       4...<5               error                error
#   4.1.*        4.1.*                     4.1...<4.2           error                error
#   [1,2)        [1,2)                     1...<2               error                error
#   [1,2]        [1,2]                     1...2                error                error
#   [1.0,)       [1.0,)                    1.0                  error                error
#   (1.0,)       (1.0,)                    error                error                error
#   (1.0,2.0)    (1.0,2.0)                 error                error                error
#   (,2.0]       (,2.0]                    0...2.0              error                error
#   (,2.0)       (,2.0)                    0...<2.0             error                error
#   1.0.0-beta   1.0.0-beta                error                error                1.0.0-beta
#
# The text of a version is always the text that was typed ("1.8" stays "1.8"), it is never built from the parsed bounds.
# A CMake range ("...") needs CMake 3.19, so it is an error when the project asks for an older CMake.

import re

from FslBuildGen.CMakeUtil import CMakeVersion
from FslBuildGen.Exceptions import UsageErrorException
from FslBuildGen.SemanticVersionPattern import SemanticVersionPattern, SemanticVersionPatternForm

# A version of numbers only, which is all CMake and an assembly reference know: no prerelease and no build label
_g_numericVersion = re.compile(r"[0-9]+(\.[0-9]+){0,3}")
# The first CMake that knows a version range
_g_firstCMakeWithVersionRange = CMakeVersion(3, 19, 0)

_g_consumerCMakeFindPackage = "CMake find_package"
_g_consumerAssembly = "an assembly reference"
_g_consumerPackagesConfig = "a packages.config entry"

_g_formDescription = {
    SemanticVersionPatternForm.Plain: "a plain version",
    SemanticVersionPatternForm.Exact: "an exact version",
    SemanticVersionPatternForm.Wildcard: "a wildcard",
    SemanticVersionPatternForm.Range: "a range",
}


class VersionRenderException(Exception):
    """A consumer can not be given a version. The message holds the version as typed, its form, the consumer and the reason."""


def DescribeForm(pattern: SemanticVersionPattern) -> str:
    return _g_formDescription[pattern.Form]


def CreatePackageError(packageName: str, dependencyName: str, error: VersionRenderException) -> UsageErrorException:
    """The error of a generator: it also names the package and its external dependency"""
    return UsageErrorException(f"External dependency '{dependencyName}' of package '{packageName}': {error}")


def _Error(pattern: SemanticVersionPattern, consumer: str, reason: str) -> VersionRenderException:
    return VersionRenderException(f"the version '{pattern.OriginalString}' ({DescribeForm(pattern)}) can not be written for {consumer}: {reason}")


def _IsNumeric(versionText: str) -> bool:
    return _g_numericVersion.fullmatch(versionText) is not None


def ToPackageReferenceVersion(pattern: SemanticVersionPattern) -> str:
    """The version of a NuGet package reference. A pattern is written the way NuGet writes one, so this is the text that was typed."""
    return pattern.OriginalString


def _NextPrefix(prefix: str) -> str:
    """The first version that does not start with the prefix of a wildcard: '4' -> '5', '4.1' -> '4.2'"""
    parts = prefix.split(".")
    parts[-1] = str(int(parts[-1]) + 1)
    return ".".join(parts)


def _ToCMakeFindPackageVersion(pattern: SemanticVersionPattern) -> tuple[str, bool]:
    """The version arguments of find_package, and if they hold a version range"""
    for boundText in (pattern.LowerBoundText, pattern.UpperBoundText):
        if boundText is not None and not _IsNumeric(boundText):
            raise _Error(pattern, _g_consumerCMakeFindPackage, f"'{boundText}' is not a CMake version, that is numbers only (major[.minor[.patch[.tweak]]])")

    lowerText = pattern.LowerBoundText
    upperText = pattern.UpperBoundText
    if lowerText is not None:
        if pattern.Form == SemanticVersionPatternForm.Plain:
            return (lowerText, False)
        if pattern.Form == SemanticVersionPatternForm.Exact:
            return (f"{lowerText} EXACT", False)
        if pattern.Form == SemanticVersionPatternForm.Wildcard:
            return (f"{lowerText}...<{_NextPrefix(lowerText)}", True)
        if not pattern.LowerInclusive:
            raise _Error(
                pattern, _g_consumerCMakeFindPackage, f"the range starts after '{lowerText}' and a CMake version range always includes its lower bound"
            )
        if upperText is None:
            # No upper bound is what a plain version means to find_package
            return (lowerText, False)
    elif upperText is None:
        raise _Error(pattern, _g_consumerCMakeFindPackage, "a range without a bound holds no version to ask for")
    # CMake has no range without a lower bound, and every version is 0 or higher
    return (f"{lowerText if lowerText is not None else '0'}{'...' if pattern.UpperInclusive else '...<'}{upperText}", True)


def CheckCMakeFindPackageVersion(pattern: SemanticVersionPattern) -> None:
    """Raise if CMake find_package can not be given the version, whatever the CMake version of the project is"""
    _ToCMakeFindPackageVersion(pattern)


def ToCMakeFindPackageVersion(pattern: SemanticVersionPattern, cmakeMinimumVersion: CMakeVersion) -> str:
    """The version arguments of CMake find_package: '1.2', '1.2 EXACT' or a range like '1...<2'.
    cmakeMinimumVersion is the CMake version the generated files ask for, a range needs 3.19.
    """
    result, isRange = _ToCMakeFindPackageVersion(pattern)
    if isRange and cmakeMinimumVersion < _g_firstCMakeWithVersionRange:
        firstVersion = f"{_g_firstCMakeWithVersionRange.Major}.{_g_firstCMakeWithVersionRange.Minor}"
        raise _Error(
            pattern,
            _g_consumerCMakeFindPackage,
            f"it becomes the version range '{result}', which needs CMake {firstVersion} and the project asks for CMake {cmakeMinimumVersion}. "
            f"Raise the MinVersion of the CMakeConfiguration of the project to {firstVersion} or newer",
        )
    return result


def ToAssemblyVersion(pattern: SemanticVersionPattern) -> str:
    """The 'Version=' of an assembly reference: one version of numbers only. The reference is to exactly that version, so '[1.2]' is '1.2'."""
    versionText = pattern.LowerBoundText
    if pattern.Form not in (SemanticVersionPatternForm.Plain, SemanticVersionPatternForm.Exact) or versionText is None or not _IsNumeric(versionText):
        raise _Error(pattern, _g_consumerAssembly, "an assembly reference needs one version of numbers only (major[.minor[.build[.revision]]])")
    return versionText


def ToPackagesConfigVersion(versionText: str) -> str:
    """The version of an entry in packages.config: one plain version (a prerelease is fine). versionText is the version of the package manager
    element of an external dependency, it is not a pattern.
    """
    pattern = SemanticVersionPattern.TryFromString(versionText)
    if pattern is None:
        raise VersionRenderException(
            f"the version '{versionText}' of its package manager can not be written for {_g_consumerPackagesConfig}: it is not a version"
        )
    if pattern.Form != SemanticVersionPatternForm.Plain:
        raise VersionRenderException(
            f"the version '{pattern.OriginalString}' ({DescribeForm(pattern)}) of its package manager can not be written for {_g_consumerPackagesConfig}: "
            "a packages.config entry needs one plain version"
        )
    return pattern.OriginalString
