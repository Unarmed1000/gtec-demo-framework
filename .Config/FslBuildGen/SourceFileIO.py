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

# The source files (C and C++ headers and sources) FslBuildCheck scans and repairs in place.
#
# - A source file is read with the pass-through policy of TextFileReader: as UTF-8, with universal newlines, and with a byte order mark and any bytes
#   that are not valid UTF-8 kept.
# - A repaired file is written back as UTF-8 with the newline of the platform. No byte order mark is added, the file keeps the one it had, and bytes
#   that were kept because they are not valid UTF-8 are written back as they were.
#
# So a repair changes the lines it repairs and nothing else, whatever the locale encoding of the machine is.

from FslBuildGen import IOUtil, TextFileReader
from FslBuildGen.Log import Log

# The source files that gave the 'not valid UTF-8' warning. A header can be listed by a package both as an include file and as a source file, this
# makes it one warning per file for a run of the tool.
g_warnedFiles: set[str] = set()


def ReadSourceFile(log: Log, filename: str) -> str:
    """Read a source file with universal newlines. A file that is not valid UTF-8 keeps its bytes and gives one warning per run."""
    return TextFileReader.ReadUTF8PassThrough(log, filename, "source file", warnedFiles=g_warnedFiles)


def WriteSourceFile(filename: str, content: str) -> None:
    """Write a source file as UTF-8. Bytes the read kept because they are not valid UTF-8 are written back as they were."""
    IOUtil.WriteFileUTF8(filename, content, errors="surrogateescape")
