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

# The command of a rule in a ninja build file ('command = ...').
#
# Ninja hands the command to the system as one line: on Windows to CreateProcess as it is, everywhere else to '/bin/sh -c'. The system splits
# the line into words at white space, so a word that holds a space (the path of a tool in 'C:/Program Files', a file in a directory with a
# space in its name) has to be quoted the way that system reads it:
#
# - Windows: the rules of the C runtime, the word is put in double quotes.
# - Everywhere else: the rules of the shell, the word is put in single quotes.
#
# A word that needs no quotes is written as it is, so a build file without such paths is the same as before and ninja runs nothing again.
#
# The '$' is the escape character of a ninja file: one that belongs to the word is written '$$'.
#
# This is for the words the tool writes into a command itself. Ninja quotes '$in' and '$out' on its own, and the paths of a build statement are
# escaped by ninja_syntax ('$ ', '$:' and '$$').

import shlex
import subprocess

from FslBuildGen.DataTypes import BuildPlatformType
from FslBuildGen.PlatformUtil import PlatformUtil


def _IsWindows() -> bool:
    return PlatformUtil.DetectBuildPlatformType() == BuildPlatformType.Windows


def EscapeText(text: str) -> str:
    """The text the way a ninja file holds it: with its '$' doubled"""
    return text.replace("$", "$$")


def QuoteArgumentForSystem(argument: str, isWindows: bool) -> str:
    """The word quoted for the system that runs the command, not yet escaped for the ninja file"""
    if isWindows:
        return subprocess.list2cmdline([argument])
    return shlex.quote(argument)


def QuoteArgument(argument: str) -> str:
    """One word of the command of a rule (the path of the tool, a file name), written so the system ninja runs on reads it as that one word"""
    return EscapeText(QuoteArgumentForSystem(argument, _IsWindows()))
