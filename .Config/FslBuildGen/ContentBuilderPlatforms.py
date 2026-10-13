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

# The platforms a content builder of the tool config is for ('<ContentBuilder Platforms="Android;Yocto" ...>').
#
# The features of an app pick its content builders (a Vulkan app compiles its shaders to SPIR-V). The platform it is built for can pick them
# too: a texture is compressed to another format for a phone than for a desktop. Two builders can then take the same extension, each for
# its platforms, and '$(PlatformName)' in the parameters of a builder is the platform it is run for.

from FslBuildGen import PackageConfig
from FslBuildGen.Exceptions import UsageErrorException

Separator = ";"


def Parse(builderName: str, platforms: str) -> frozenset[str] | None:
    """The platforms of the attribute, None for a builder that is for every platform (no attribute, or an empty one)"""
    names = [entry.strip() for entry in platforms.split(Separator)]
    names = [entry for entry in names if len(entry) > 0]
    if len(names) <= 0:
        return None
    for name in names:
        if name not in PackageConfig.APPROVED_PLATFORM_NAMES:
            raise UsageErrorException(
                f"'{name}' in 'Platforms' of the ContentBuilder '{builderName}' is not a platform. Valid platforms are: "
                f"{', '.join(PackageConfig.APPROVED_PLATFORM_NAMES)}, separated by '{Separator}'"
            )
    return frozenset(names)


def IsFor(platformNames: frozenset[str] | None, platformName: str | None) -> bool:
    """True if a builder with these platforms is for the platform. A caller that does not know the platform gets the builders of every platform"""
    return platformNames is None or platformName is None or platformName in platformNames
