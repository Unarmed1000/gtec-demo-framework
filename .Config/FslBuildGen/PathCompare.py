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

# Comparing paths and file names the way the file system of the machine does.
#
# On Windows 'E:\Work\App' and 'e:/work/app' are the same directory and 'fsl.gen' is the file 'Fsl.gen': a path the tool is given on the
# command line (the current directory) does not have to be spelled like the path the tool made itself. Comparing the texts treats them as
# two things. The comparison here is the one of the platform: on Windows without case and with either separator ('os.path.normcase'),
# everywhere else the names as they are.
#
# This compares names only, it does not look at the file system: a link, a short name ('PROGRA~1') or a case-insensitive file system on a
# platform that is case-sensitive by default is not seen.

import os

# The path rules of the platform the tool runs on. The tests of the Windows and the posix rules put ntpath and posixpath here.
_g_path = os.path


def ToComparable(path: str) -> str:
    """The text of a path or a file name to compare with: two paths that differ only by what the platform ignores give the same text"""
    return _g_path.normcase(path)


def IsSameName(name1: str, name2: str) -> bool:
    """True if the two file names (or paths that are written the same way) name the same thing on this platform"""
    return ToComparable(name1) == ToComparable(name2)


def IsSamePath(path1: str, path2: str) -> bool:
    """True if the two paths name the same file or directory: '.', '..', doubled and trailing separators are resolved first"""
    return ToComparable(_g_path.normpath(path1)) == ToComparable(_g_path.normpath(path2))


def IsBelow(path: str, directory: str) -> bool:
    """True if the path is inside the directory, at any depth. The directory itself is not below itself."""
    comparablePath = ToComparable(_g_path.normpath(path))
    comparableDirectory = ToComparable(_g_path.normpath(directory))
    if not comparableDirectory.endswith(_g_path.sep):
        # The root of a drive ('C:\\') and '/' end with the separator, any other directory does not
        comparableDirectory += _g_path.sep
    return len(comparablePath) > len(comparableDirectory) and comparablePath.startswith(comparableDirectory)
