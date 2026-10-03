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


from dataclasses import dataclass, field
from typing import final


@final
@dataclass
class MakeDependencies:
    """The rules of a make style dependency file ('target ...: prerequisite ...')"""

    Targets: list[str] = field(default_factory=list)
    Prerequisites: list[str] = field(default_factory=list)


def _IsWhitespace(char: str) -> bool:
    return char in (" ", "\t")


def _IsLineEnd(content: str, index: int) -> bool:
    return index >= len(content) or content[index] in ("\r", "\n")


def ParseMakeDependencyFile(content: str) -> MakeDependencies:
    """Parse a make style dependency file, like the ones compilers write with '-MD -MF <file>' or '--depfile <file>'.
    - A backslash escapes a space, '#' or ':' and a backslash at the end of a line continues the rule on the next line.
    - '$$' is a '$'.
    - A ':' followed by whitespace or the end of the line ends the targets, so a drive letter like 'C:/' stays part of the path.
    The prerequisites of all rules are returned in the order they appear.
    """
    result = MakeDependencies()
    isTarget = True
    token: list[str] = []

    def EndToken() -> None:
        if len(token) > 0:
            (result.Targets if isTarget else result.Prerequisites).append("".join(token))
            token.clear()

    index = 0
    while index < len(content):
        char = content[index]
        if char == "\\" and index + 1 < len(content):
            nextChar = content[index + 1]
            if nextChar in ("\r", "\n"):
                # Line continuation
                EndToken()
                index += 3 if nextChar == "\r" and index + 2 < len(content) and content[index + 2] == "\n" else 2
                continue
            if nextChar in (" ", "#", ":"):
                token.append(nextChar)
                index += 2
                continue
            token.append(char)
            index += 1
        elif char == "$" and index + 1 < len(content) and content[index + 1] == "$":
            token.append("$")
            index += 2
        elif char == ":" and isTarget and (_IsLineEnd(content, index + 1) or _IsWhitespace(content[index + 1])):
            EndToken()
            isTarget = False
            index += 1
        elif char == "#" and len(token) == 0:
            # A comment runs to the end of the line
            while not _IsLineEnd(content, index):
                index += 1
        elif _IsWhitespace(char):
            EndToken()
            index += 1
        elif char in ("\r", "\n"):
            EndToken()
            isTarget = True
            index += 1
        else:
            token.append(char)
            index += 1
    EndToken()
    return result


def _Escape(path: str) -> str:
    if "\n" in path or "\r" in path:
        raise Exception(f"A path with a line break can not be written to a dependency file: '{path!r}'")
    return path.replace("$", "$$").replace("#", r"\#").replace(":", r"\:").replace(" ", r"\ ")


def BuildMakeDependencyFile(targets: list[str], prerequisites: list[str]) -> str:
    r"""A make style dependency file with one rule, escaped like the ones glslangValidator writes ('C\:/My\ Dir/File.glsl').
    Without targets the file is empty, as a rule needs a target.
    """
    if len(targets) <= 0:
        return ""
    lines = [" ".join(_Escape(target) for target in targets) + ":"]
    lines += [f"  {_Escape(entry)}" for entry in prerequisites]
    return " \\\n".join(lines) + "\n"
