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

# The text files the generators work with: the templates they fill in and the project and build files they generate from them.
#
# - A template is read with the pass-through policy of TextFileReader: as UTF-8, with a byte order mark and any bytes that are not valid UTF-8 kept.
# - A generated file is written as UTF-8. No byte order mark is added, the file has one when its template starts with one.
#
# So what a template holds reaches the generated file byte for byte, and what the generator fills in (text from the gen files and the configuration) is
# written as UTF-8, whatever the locale encoding of the machine is.

from FslBuildGen import IOUtil, TextFileReader
from FslBuildGen.Log import Log

# The templates that gave the 'not valid UTF-8' warning. The generators read the same template for every package and every platform they generate, this
# makes it one warning per template for a run of the tool.
g_warnedTemplates: set[str] = set()


def ReadTemplate(log: Log, filename: str) -> str:
    """Read a template with universal newlines. A template that is not valid UTF-8 keeps its bytes and gives one warning per run."""
    return TextFileReader.ReadUTF8PassThrough(log, filename, "template", warnedFiles=g_warnedTemplates)


def TryReadTemplate(log: Log, filename: str) -> str | None:
    """Like ReadTemplate, but a template that can not be opened or read (an optional template that is not there) gives None"""
    return TextFileReader.TryReadUTF8PassThrough(log, filename, "template", warnedFiles=g_warnedTemplates)


def WriteGeneratedFileIfChanged(filename: str, content: str) -> bool:
    """Write a file generated from templates as UTF-8 unless the file already holds the content, returns true if the file was written.
    Bytes a template kept because they are not valid UTF-8 are written back as they were.
    """
    return IOUtil.WriteFileUTF8IfChanged(filename, content, errors="surrogateescape")
