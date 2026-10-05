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

# The anchor of a markdown heading: what a link to the heading has after the '#'.
#
# The sites that show a markdown file give every heading an anchor that is made from its text. The rule here is the one of GitHub:
# - the text in lower case,
# - every character that is not a letter, a digit, a space, a hyphen or an underscore removed (a letter or digit of any script counts,
#   and so do combining marks),
# - each space replaced by a hyphen. Nothing is merged: two spaces are two hyphens, and so is a space on each side of a character that
#   was removed.
# A heading whose anchor an earlier heading of the same file has gets '-1' appended, the next one '-2' and so on.
# A byte of the file that is not valid UTF-8 is kept (MarkdownIO reads it as a lone surrogate): nothing can be said about what it is.
#
# 'Getting started (Windows)' -> 'getting-started-windows', 'C++ / CMake' -> 'c--cmake', 'FslBuild.py: usage' -> 'fslbuildpy-usage'

import unicodedata

# What a byte that is not valid UTF-8 is read as (the error handler 'surrogateescape'): the bytes 0x80 to 0xFF
_g_firstKeptByte = 0xDC80
_g_lastKeptByte = 0xDCFF


def _IsKept(character: str) -> bool:
    if character.isalnum() or character in " -_":
        return True
    if _g_firstKeptByte <= ord(character) <= _g_lastKeptByte:
        return True
    # Combining marks and the other characters that connect words like '_' does
    category = unicodedata.category(character)
    return category.startswith("M") or category == "Pc"


def CreateAnchor(headingText: str) -> str:
    """The anchor of a heading with this text that is the first one with it in its file"""
    text = headingText.strip().lower()
    return "".join(character for character in text if _IsKept(character)).replace(" ", "-")


class AnchorNames:
    """The anchors of the headings of one file: Add is called for every heading of the file, in the order of the file"""

    def __init__(self) -> None:
        super().__init__()
        # anchor -> the number of headings that got it from their text after the first one
        self.__occurrences: dict[str, int] = {}

    def Add(self, headingText: str) -> str:
        """The anchor of the next heading of the file"""
        firstAnchor = CreateAnchor(headingText)
        anchor = firstAnchor
        # An anchor is never given twice, also not when a heading is named like a numbered one ('Setup' twice and 'Setup-1')
        while anchor in self.__occurrences:
            self.__occurrences[firstAnchor] += 1
            anchor = f"{firstAnchor}-{self.__occurrences[firstAnchor]}"
        self.__occurrences[anchor] = 0
        return anchor
