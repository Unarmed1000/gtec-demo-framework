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

# The markdown files FslBuildDoc edits in place (the README.md files and the other .md files of a repository).
#
# - A markdown file is read with the pass-through policy of TextFileReader: as UTF-8, with universal newlines, and with a byte order mark and any bytes
#   that are not valid UTF-8 kept.
# - It is written back as UTF-8 with the newline of the platform, and only when its content changed. No byte order mark is added, the file keeps the one
#   it had, and bytes that were kept because they are not valid UTF-8 are written back as they were.
#
# So the parts of a file the tool does not generate keep their bytes, and what it generates is written as UTF-8, whatever the locale encoding of the
# machine is.

from FslBuildGen import IOUtil, TextFileReader
from FslBuildGen.Log import Log

# The markdown files that gave the 'not valid UTF-8' warning. FslBuildDoc reads a package README.md more than once, this makes it one warning per file
# for a run of the tool.
g_warnedFiles: set[str] = set()


def TryReadMarkdownFile(log: Log, filename: str) -> str | None:
    """Read a markdown file with universal newlines, a file that can not be opened or read (one that is not there) gives None.
    A file that is not valid UTF-8 keeps its bytes and gives one warning per run.
    """
    return TextFileReader.TryReadUTF8PassThrough(log, filename, "markdown file", warnedFiles=g_warnedFiles)


def WriteMarkdownFileIfChanged(filename: str, content: str) -> bool:
    """Write a markdown file as UTF-8 unless the file already holds the content, returns true if the file was written.
    Bytes the read kept because they are not valid UTF-8 are written back as they were.
    """
    return IOUtil.WriteFileUTF8IfChanged(filename, content, errors="surrogateescape")
