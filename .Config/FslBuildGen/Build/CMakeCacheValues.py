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

# The values the CMake cache of a build directory ('CMakeCache.txt') holds for the variables the configure command of the tool sets.
#
# The builder skips the configure when its own record says nothing changed (Build/BuildConfigureCache.py). That record does not see a
# configure that was run without the tool: a preset of the generated project ('cmake --preset coverage', Generator/CMakePresetsFile.py),
# an IDE, or a cmake command typed by hand. The build directory can then be configured for something else than the tool thinks, and the
# tool would build that.
#
# So after a configure of its own the builder keeps what cmake stored for the variables its command set ('-D'), and before it skips a
# configure it checks that the cache still holds those values. What cmake stored is compared, not what the command said: cmake may store
# a value in another form (a relative path made absolute), and a comparison with the command would then configure on every build.
#
# Only the variables of the command are looked at. A cache that was configured with the same values by something else is fine as it is.

from collections.abc import Mapping, Sequence

from FslBuildGen import IOUtil

FileName = "CMakeCache.txt"


def GetDefinedNames(command: Sequence[str]) -> list[str]:
    """The names of the cache variables a configure command sets: '-D<name>=<value>', '-D<name>:<type>=<value>' and the form with two
    arguments ('-D', '<name>=<value>'). In the order of the command, each name once.
    """
    names: list[str] = []
    index = 0
    while index < len(command):
        argument = command[index]
        index += 1
        if not argument.startswith("-D"):
            continue
        definition = argument[2:]
        if len(definition) == 0:
            if index >= len(command):
                break
            definition = command[index]
            index += 1
        name = definition.partition("=")[0].partition(":")[0]
        if len(name) > 0 and name not in names:
            names.append(name)
    return names


def _ParseValues(content: str, names: Sequence[str]) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in content.splitlines():
        # An entry is '<name>:<type>=<value>', a comment starts with '#' or '//'
        if len(line) == 0 or line.startswith(("#", "//")):
            continue
        key, separator, value = line.partition("=")
        if len(separator) == 0:
            continue
        name = key.rpartition(":")[0]
        if len(name) >= 2 and name.startswith('"') and name.endswith('"'):
            # cmake puts a name that holds a ':' in quotes
            name = name[1:-1]
        if len(name) > 0 and name in names:
            values[name] = value
    return values


def TryReadValues(cacheFilename: str, names: Sequence[str]) -> dict[str, str] | None:
    """The value the cache file holds for each of the names, a name it does not hold is left out. None when the file can not be read."""
    if len(names) == 0:
        # Nothing to look for, so the file does not have to be read
        return {}
    content = IOUtil.TryReadBinaryFile(cacheFilename)
    if content is None:
        return None
    # A byte that is no UTF-8 is replaced the same way each time, so a value with one still compares equal to itself
    return _ParseValues(content.decode("utf-8", errors="replace"), names)


def TryDescribeChange(configuredValues: Mapping[str, str], cacheFilename: str) -> str | None:
    """What is different in the cache file from the values that were kept after the last configure of the tool, None when nothing is.
    Without kept values there is nothing to compare with: that is no change (the cache of a tool version that kept none).
    """
    if len(configuredValues) == 0:
        return None
    currentValues = TryReadValues(cacheFilename, list(configuredValues))
    if currentValues is None:
        return f"'{cacheFilename}' is missing"
    for name, configuredValue in configuredValues.items():
        currentValue = currentValues.get(name)
        if currentValue is None:
            return f"'{name}' is no longer in '{cacheFilename}'"
        if currentValue != configuredValue:
            return f"'{name}' is '{currentValue}' in '{cacheFilename}', the last configure of the tool set it to '{configuredValue}'"
    return None
