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

"""
Read and write a .gitignore file the way git reads it: as UTF-8, and a BOM at the start is not part of the first line.
Bytes that are not valid UTF-8 (a file saved as ANSI) are kept undecoded ('surrogateescape'), so a line the tool does not change
is written back with exactly the bytes it had.
"""

from typing import NamedTuple

_BOM = "\ufeff"


class GitIgnoreText(NamedTuple):
    # The content with '\n' line endings, without the BOM
    Content: str
    # True when the file starts with a UTF-8 BOM, it is kept when the file is written
    HasBom: bool


def TryReadGitIgnoreText(filename: str) -> GitIgnoreText | None:
    """Returns None if the file does not exist or can not be read"""
    try:
        with open(filename, encoding="utf-8", errors="surrogateescape") as file:
            content = file.read()
    except OSError:
        return None
    if content.startswith(_BOM):
        return GitIgnoreText(content[len(_BOM) :], True)
    return GitIgnoreText(content, False)


def TryReadGitIgnoreContent(filename: str) -> str | None:
    """The content of the file without a BOM, or None if the file does not exist or can not be read"""
    text = TryReadGitIgnoreText(filename)
    return text.Content if text is not None else None


def WriteGitIgnoreTextIfChanged(filename: str, text: GitIgnoreText) -> bool:
    """
    Write the text with the line endings of the platform, unless the file already has this content (line endings are not compared).
    Returns True if the file was written.
    """
    if TryReadGitIgnoreText(filename) == text:
        return False
    with open(filename, "w", encoding="utf-8", errors="surrogateescape") as file:
        _ = file.write((_BOM if text.HasBom else "") + text.Content)
    return True
