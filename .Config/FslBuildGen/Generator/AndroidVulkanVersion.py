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

import re

from FslBuildGen.DataTypes import PackageRequirementTypeString
from FslBuildGen.Packages.Package import Package

_VULKAN_FEATURE_ID = "vulkan"

# 'major', 'major.minor' or 'major.minor.patch'
_VERSION_PATTERN = re.compile(r"^(\d+)(?:\.(\d+))?(?:\.(\d+))?$")

# The bits VK_MAKE_API_VERSION has for each part
_MAX_MAJOR = (1 << 7) - 1
_MAX_MINOR = (1 << 10) - 1
_MAX_PATCH = (1 << 12) - 1


def ParseVulkanVersion(version: str, packageName: str) -> tuple[int, int, int]:
    """The (major, minor, patch) of the Version of a 'Vulkan' feature requirement, like '1.3'"""
    match = _VERSION_PATTERN.match(version)
    if match is not None:
        major, minor, patch = int(match.group(1)), int(match.group(2) or 0), int(match.group(3) or 0)
        if major <= _MAX_MAJOR and minor <= _MAX_MINOR and patch <= _MAX_PATCH:
            return (major, minor, patch)
    raise Exception(f"Package '{packageName}' requires the feature 'Vulkan' with Version=\"{version}\", the version must be like '1.3' or '1.3.0'")


def TryGetRequiredVulkanVersion(package: Package) -> tuple[int, int, int] | None:
    """The highest Vulkan version the package or one of its dependencies requires with the Version of its 'Vulkan' feature requirement.
    None if the package does not use Vulkan or no requirement has a version.
    """
    if not any(feature.Id == _VULKAN_FEATURE_ID for feature in package.ResolvedAllUsedFeatures):
        return None
    result: tuple[int, int, int] | None = None
    for entry in [package, *package.ResolvedBuildOrder]:
        for requirement in entry.ResolvedDirectRequirements:
            if requirement.Type == PackageRequirementTypeString.Feature and requirement.Id == _VULKAN_FEATURE_ID and len(requirement.Version) > 0:
                version = ParseVulkanVersion(requirement.Version, entry.Name)
                if result is None or version > result:
                    result = version
    return result


def ToVulkanApiVersion(version: tuple[int, int, int]) -> int:
    """The VK_MAKE_API_VERSION value of the version (variant 0), (1, 3, 0) is 0x403000"""
    return (version[0] << 22) | (version[1] << 12) | version[2]


def GetVulkanUsesFeature(package: Package) -> str:
    """The Android manifest element that makes the app require the Vulkan version its packages require, empty if there is none.
    It starts with a line break and the indentation of the elements in the manifest, so it can follow another element.
    """
    version = TryGetRequiredVulkanVersion(package)
    if version is None:
        return ""
    return f'\n    <uses-feature android:name="android.hardware.vulkan.version" android:version="0x{ToVulkanApiVersion(version):x}" android:required="true"/>'
