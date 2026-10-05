#!/usr/bin/env python3
# ****************************************************************************************************************************************************
# * BSD 3-Clause License
# *
# * Copyright (c) 2025, Mana Battery
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

import re
from enum import Enum

# A regular expression to parse a single semantic version.
# Handles Major.Minor.Patch.Revision, pre-release tags, and build metadata.
SEMVER_VERSION_PATTERN = re.compile(
    r"^(?P<major>\d+)(\.(?P<minor>\d+))?(\.(?P<patch>\d+))?(\.(?P<revision>\d+))?(?P<prerelease>-[0-9a-zA-Z\-\.]+)?(?P<build>\+[0-9a-zA-Z\-\.]+)?$"
)

# A regular expression to parse a wildcard version pattern like "4.*" or "4.1.*"
WILDCARD_VERSION_PATTERN = re.compile(r"^(?P<prefix>\d+(\.\d+){0,3})\.\*$")

# The prerelease that sorts before every other prerelease of a version
_g_lowestPrerelease = "-0"


class SemanticVersion:
    """
    A class to represent a single NuGet-style semantic version.
    It can be compared to other versions for sorting and range checks.
    """

    def __init__(self, major: int, minor: int = 0, patch: int = 0, revision: int = 0, prerelease: str | None = None):
        self.Major = major
        self.Minor = minor
        self.Patch = patch
        self.Revision = revision
        self.Prerelease = prerelease

    @staticmethod
    def try_parse(versionString: str) -> SemanticVersion | None:
        """Parses a version string into a SemanticVersion object."""
        match = SEMVER_VERSION_PATTERN.match(versionString)
        if not match:
            return None

        try:
            major = int(match.group("major"))
            minor = int(match.group("minor")) if match.group("minor") else 0
            patch = int(match.group("patch")) if match.group("patch") else 0
            revision = int(match.group("revision")) if match.group("revision") else 0
            prerelease = match.group("prerelease")
            return SemanticVersion(major, minor, patch, revision, prerelease)
        except ValueError, TypeError:
            return None

    def _SortKey(self) -> tuple[int, int, int, int, int, tuple[tuple[int, int, str], ...]]:
        """The key every comparison uses, so the order is total and agrees with equality.
        Like SemVer a release sorts after its prereleases, prerelease identifiers are separated by '.' only (a '-' is part of an
        identifier), numeric identifiers compare numerically and sort before alphanumeric ones and a shorter prerelease sorts first.
        The identifier text breaks the tie between numbers that only differ by leading zeros.
        """
        if not self.Prerelease:
            return (self.Major, self.Minor, self.Patch, self.Revision, 1, ())
        identifiers = tuple((0, int(part), part) if part.isdigit() else (1, 0, part) for part in self.Prerelease[1:].split("."))
        return (self.Major, self.Minor, self.Patch, self.Revision, 0, identifiers)

    def __lt__(self, other: object) -> bool:
        if not isinstance(other, SemanticVersion):
            return NotImplemented
        return self._SortKey() < other._SortKey()

    def __le__(self, other: object) -> bool:
        if not isinstance(other, SemanticVersion):
            return NotImplemented
        return self._SortKey() <= other._SortKey()

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, SemanticVersion):
            return NotImplemented
        return self._SortKey() == other._SortKey()

    def __gt__(self, other: object) -> bool:
        if not isinstance(other, SemanticVersion):
            return NotImplemented
        return self._SortKey() > other._SortKey()

    def __ge__(self, other: object) -> bool:
        if not isinstance(other, SemanticVersion):
            return NotImplemented
        return self._SortKey() >= other._SortKey()

    def __hash__(self) -> int:
        return hash(self._SortKey())

    def get_numeric_parts(self) -> tuple[int, int, int, int]:
        """Returns the numeric parts of the version as a tuple."""
        return (self.Major, self.Minor, self.Patch, self.Revision)

    def __repr__(self) -> str:
        version_str = f"{self.Major}.{self.Minor}.{self.Patch}.{self.Revision}"
        if self.Prerelease:
            version_str += self.Prerelease
        return f"SemanticVersion('{version_str}')"

    def __str__(self) -> str:
        version_str = f"{self.Major}.{self.Minor}.{self.Patch}"
        if self.Revision > 0:
            version_str += f".{self.Revision}"
        if self.Prerelease:
            version_str += self.Prerelease
        return version_str


class SemanticVersionPatternForm(Enum):
    """The way a version pattern was written"""

    # "1.0": a minimum version
    Plain = 1
    # "[1.0]": exactly that version
    Exact = 2
    # "4.*", "4.1.*": every version that starts with the prefix
    Wildcard = 3
    # "[1.0,2.0)", "(,2.0]", "[1.0,)": a range, a bound can be left out
    Range = 4


class SemanticVersionPattern:
    """
    A class to represent and validate NuGet semantic version dependency patterns.
    Supports a plain minimum version ("1.0"), an exact version ("[1.0]"), ranges ("[1.0,2.0)", "(,2.0]") and wildcards ("4.*").

    OriginalString is the pattern as it was typed (without surrounding whitespace). Form tells which way it was written, LowerBoundText and
    UpperBoundText are the versions of the bounds as they were typed ("1.8" stays "1.8", the parsed bound reads "1.8.0"):
    - Plain: the version is LowerBoundText.
    - Exact: the version is LowerBoundText and UpperBoundText.
    - Wildcard: LowerBoundText is the prefix ("4.1" of "4.1.*"), there is no UpperBoundText.
    - Range: a bound that was left out is None.
    """

    def __init__(
        self,
        lowerBound: SemanticVersion | None,
        lowerInclusive: bool,
        upperBound: SemanticVersion | None,
        upperInclusive: bool,
        originalString: str,
        form: SemanticVersionPatternForm,
        lowerBoundText: str | None,
        upperBoundText: str | None,
    ):
        self.LowerBound = lowerBound
        self.LowerInclusive = lowerInclusive
        self.UpperBound = upperBound
        self.UpperInclusive = upperInclusive
        self.OriginalString = originalString
        self.Form = form
        self.LowerBoundText = lowerBoundText
        self.UpperBoundText = upperBoundText

    @staticmethod
    def TryFromString(versionString: str) -> SemanticVersionPattern | None:
        """
        Parses a NuGet version constraint string and returns a boolean
        and a SemanticVersionPattern instance if successful.
        """
        versionString = versionString.strip()

        # Handle wildcard pattern like "4.*"
        wildcard_match = WILDCARD_VERSION_PATTERN.match(versionString)
        if wildcard_match:
            prefix = wildcard_match.group("prefix")
            parts = [int(p) for p in prefix.split(".")]

            # The lower bound is the version prefix itself, e.g., "4.1" becomes 4.1.0.0
            lowerBound: SemanticVersion | None = SemanticVersion(
                parts[0], parts[1] if len(parts) > 1 else 0, parts[2] if len(parts) > 2 else 0, parts[3] if len(parts) > 3 else 0
            )

            upperBound: SemanticVersion | None = None
            # The exclusive upper bound is the lowest prerelease ('-0') of the next major/minor/patch version,
            # so a prerelease of the next version like "5.0.0-beta" is not matched by "4.*"
            if len(parts) == 1:  # "4.*" -> upper is 5.0.0.0-0
                upperBound = SemanticVersion(parts[0] + 1, prerelease=_g_lowestPrerelease)
            elif len(parts) == 2:  # "4.1.*" -> upper is 4.2.0.0-0
                upperBound = SemanticVersion(parts[0], parts[1] + 1, prerelease=_g_lowestPrerelease)
            elif len(parts) == 3:  # "4.1.2.*" -> upper is 4.1.3.0-0
                upperBound = SemanticVersion(parts[0], parts[1], parts[2] + 1, prerelease=_g_lowestPrerelease)
            else:  # "4.1.2.3.*" -> upper is 4.1.2.4-0
                upperBound = SemanticVersion(parts[0], parts[1], parts[2], parts[3] + 1, prerelease=_g_lowestPrerelease)

            # This is an inclusive lower bound and exclusive upper bound
            return SemanticVersionPattern(lowerBound, True, upperBound, False, versionString, SemanticVersionPatternForm.Wildcard, prefix, None)

        # Exact version "[1.0]" or a plain minimum version "1.0".
        # A bracketed string that is not a single version (like "[1,2]") is left untouched for the range parsing below.
        isBracketed = versionString.startswith("[") and versionString.endswith("]")
        if isBracketed or (not versionString.startswith(("(", "[", ",")) and not versionString.endswith((")", "]"))):
            exactString = versionString[1:-1].strip() if isBracketed else versionString
            version = SemanticVersion.try_parse(exactString)
            if version:
                if isBracketed:
                    # An exact match is an inclusive range with identical lower and upper bounds.
                    # The brackets are kept in the original string, without them it reads as a minimum version
                    return SemanticVersionPattern(version, True, version, True, versionString, SemanticVersionPatternForm.Exact, exactString, exactString)
                # A plain version is a minimum version, which is how NuGet and CMake find_package read it
                return SemanticVersionPattern(version, True, None, False, exactString, SemanticVersionPatternForm.Plain, exactString, None)

        # Range patterns: "(1.0, 2.0]", "[1.0,)", etc.
        if versionString.startswith(("(", "[")) and versionString.endswith((")", "]")):
            lowerInclusive = versionString.startswith("[")
            upperInclusive = versionString.endswith("]")

            inner = versionString[1:-1].strip()
            range_parts = inner.split(",")

            if len(range_parts) == 2:
                lowerStr = range_parts[0].strip()
                upperStr = range_parts[1].strip()

                lowerBound = None
                if lowerStr:
                    lowerBound = SemanticVersion.try_parse(lowerStr)
                    if not lowerBound:
                        return None

                upperBound = None
                if upperStr:
                    upperBound = SemanticVersion.try_parse(upperStr)
                    if not upperBound:
                        return None

                # A range requires the lower bound to be less than or equal to the upper bound
                if lowerBound and upperBound and lowerBound > upperBound:
                    return None

                # A range between a version and itself holds that version when both ends are inclusive ("[1.0,1.0]", the exact version).
                # With an exclusive end ("[1.0,1.0)", "(1.0,1.0]", "(1.0,1.0)") it holds no version at all
                if lowerBound and upperBound and lowerBound == upperBound and not (lowerInclusive and upperInclusive):
                    return None

                return SemanticVersionPattern(
                    lowerBound,
                    lowerInclusive,
                    upperBound,
                    upperInclusive,
                    versionString,
                    SemanticVersionPatternForm.Range,
                    lowerStr if lowerStr else None,
                    upperStr if upperStr else None,
                )

        return None

    def Satisfies(self, version: SemanticVersion) -> bool:
        """Checks if a given version satisfies this pattern."""
        lowerMatch = False
        if self.LowerBound:
            lowerMatch = version >= self.LowerBound if self.LowerInclusive else version > self.LowerBound
        else:
            lowerMatch = True  # No lower bound means it always satisfies this condition

        upperMatch = False
        if self.UpperBound:
            upperMatch = version <= self.UpperBound if self.UpperInclusive else version < self.UpperBound
        else:
            upperMatch = True  # No upper bound means it always satisfies this condition

        return lowerMatch and upperMatch

    def IsCompatible(self, other: SemanticVersionPattern) -> bool:
        """
        Checks if this version pattern is compatible with another.
        Compatibility means that the intersection of the two patterns is non-empty.
        """
        # Determine the effective lower bound of the intersection
        effectiveLowerBound = None
        effectiveLowerInclusive = False
        if self.LowerBound and other.LowerBound:
            if self.LowerBound > other.LowerBound:
                effectiveLowerBound = self.LowerBound
                effectiveLowerInclusive = self.LowerInclusive
            elif other.LowerBound > self.LowerBound:
                effectiveLowerBound = other.LowerBound
                effectiveLowerInclusive = other.LowerInclusive
            else:  # They are equal
                effectiveLowerBound = self.LowerBound
                effectiveLowerInclusive = self.LowerInclusive and other.LowerInclusive
        elif self.LowerBound:
            effectiveLowerBound = self.LowerBound
            effectiveLowerInclusive = self.LowerInclusive
        elif other.LowerBound:
            effectiveLowerBound = other.LowerBound
            effectiveLowerInclusive = other.LowerInclusive

        # Determine the effective upper bound of the intersection
        effectiveUpperBound = None
        effectiveUpperInclusive = False
        if self.UpperBound and other.UpperBound:
            if self.UpperBound < other.UpperBound:
                effectiveUpperBound = self.UpperBound
                effectiveUpperInclusive = self.UpperInclusive
            elif other.UpperBound < self.UpperBound:
                effectiveUpperBound = other.UpperBound
                effectiveUpperInclusive = other.UpperInclusive
            else:  # They are equal
                effectiveUpperBound = self.UpperBound
                effectiveUpperInclusive = self.UpperInclusive and other.UpperInclusive
        elif self.UpperBound:
            effectiveUpperBound = self.UpperBound
            effectiveUpperInclusive = self.UpperInclusive
        elif other.UpperBound:
            effectiveUpperBound = other.UpperBound
            effectiveUpperInclusive = other.UpperInclusive

        # Check for non-empty intersection
        if not effectiveLowerBound or not effectiveUpperBound:
            return True  # At least one bound is open, so they must overlap

        if effectiveLowerBound < effectiveUpperBound:
            return True

        if effectiveLowerBound == effectiveUpperBound:
            return effectiveLowerInclusive and effectiveUpperInclusive

        return False

    def __repr__(self) -> str:
        lowerChar = "[" if self.LowerInclusive else "("
        upperChar = "]" if self.UpperInclusive else ")"
        lowerVer = str(self.LowerBound) if self.LowerBound else ""
        upperVer = str(self.UpperBound) if self.UpperBound else ""
        return f"SemanticVersionPattern(Original: '{self.OriginalString}', Range: '{lowerChar}{lowerVer},{upperVer}{upperChar}')"

    def __str__(self) -> str:
        return self.OriginalString
