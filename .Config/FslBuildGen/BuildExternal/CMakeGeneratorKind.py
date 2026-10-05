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

# What the tool knows about the generators of CMake: does a generator build one configuration, which the configure command chooses
# ('-DCMAKE_BUILD_TYPE=Debug'), or does its build directory hold every configuration and the build chooses ('--config Debug').
#
# The names are the ones 'cmake --help' lists (checked with CMake 4.4.3), the kinds the ones the CMake documentation gives
# (CMAKE_CONFIGURATION_TYPES names Visual Studio, Xcode and Ninja Multi-Config as the multi-config generators).
#
# A generator that is not listed here is 'Unknown'. It is given the configuration both ways, at configure and at build: a single-config
# generator ignores '--config' and a multi-config generator ignores CMAKE_BUILD_TYPE, so that is right for either kind.

import re

from FslBuildGen.BuildExternal.CMakeTypes import CMakeGeneratorMultiConfigCapability, CMakeGeneratorName

_g_multiConfigGenerators = frozenset({"Xcode", "Ninja Multi-Config"})

# Ninja and the generators that write makefiles
_g_singleConfigGenerators = frozenset(
    {
        "Ninja",
        "Unix Makefiles",
        "NMake Makefiles",
        "NMake Makefiles JOM",
        "MinGW Makefiles",
        "MSYS Makefiles",
        "Borland Makefiles",
        "Watcom WMake",
    }
)

# 'Visual Studio 17 2022', and up to Visual Studio 2017 with the platform in the name: 'Visual Studio 15 2017 Win64'
_g_visualStudioGenerator = re.compile(r"Visual Studio \d+ \d{4}( \S+)?")

# An extra generator writes the project files of an editor on top of another generator: 'CodeBlocks - Ninja', 'Kate - Ninja Multi-Config'
_g_extraGeneratorSeparator = " - "

# The generators that have had a build directory for each configuration, see HasBuildDirectoryPerConfiguration
_g_buildDirectoryPerConfigurationGenerators = frozenset({CMakeGeneratorName.UnixMakeFile, CMakeGeneratorName.Ninja})


def GetMultiConfigCapability(cmakeGeneratorName: str) -> CMakeGeneratorMultiConfigCapability:
    """Yes: the build chooses the configuration. No: the configure command chooses it. Unknown: the tool does not know the generator."""
    extraGenerator, separator, wrappedGenerator = cmakeGeneratorName.partition(_g_extraGeneratorSeparator)
    if len(separator) > 0 and len(extraGenerator) > 0:
        # The build is the one of the generator it wraps
        cmakeGeneratorName = wrappedGenerator
    if cmakeGeneratorName in _g_multiConfigGenerators or _g_visualStudioGenerator.fullmatch(cmakeGeneratorName) is not None:
        return CMakeGeneratorMultiConfigCapability.Yes
    if cmakeGeneratorName in _g_singleConfigGenerators:
        return CMakeGeneratorMultiConfigCapability.No
    return CMakeGeneratorMultiConfigCapability.Unknown


def IsConfigurationGivenAtConfigure(cmakeGeneratorName: str) -> bool:
    """True if the configure command is to say which configuration is built (CMAKE_BUILD_TYPE): for every generator that is not known to
    have every configuration in one build directory. Such a build directory is configured again when another configuration is asked for.
    """
    return GetMultiConfigCapability(cmakeGeneratorName) != CMakeGeneratorMultiConfigCapability.Yes


def IsConfigurationGivenAtBuild(cmakeGeneratorName: str) -> bool:
    """True if the build command is to say which configuration is built ('--config'): for every generator that is not known to build the
    one configuration its configure command chose.
    """
    return GetMultiConfigCapability(cmakeGeneratorName) != CMakeGeneratorMultiConfigCapability.No


def HasBuildDirectoryPerConfiguration(cmakeGeneratorName: str) -> bool:
    """True if the default build directory of the generator has a directory for each configuration below it.
    That is so for 'Unix Makefiles' and 'Ninja' only, not for every single-config generator: a generator that got its kind later (or
    has none) keeps the one build directory it always had, as another answer here would move the build directories that exist.
    """
    return cmakeGeneratorName in _g_buildDirectoryPerConfigurationGenerators
