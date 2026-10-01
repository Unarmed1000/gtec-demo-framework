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

# Reading text files that should be UTF-8 but might not be. There are three policies:
#
# - Pass-through, for text the tool copies or edits in place (templates, markdown files, source files):
#   ReadUTF8PassThrough / TryReadUTF8PassThrough. The file is read as UTF-8 with the 'surrogateescape' error handler, so no content can fail the read
#   and bytes that are not valid UTF-8 are kept. Writing the text with IOUtil.WriteFileUTF8(..., errors="surrogateescape") gives the same bytes back.
#   A byte order mark is kept as the first character. A file that is not valid UTF-8 gives one warning that names it.
# - Parsed input, for files the tool interprets (json):
#   ReadUTF8OrLocale(..., skipBom=True). A file that is not valid UTF-8 is read with the locale encoding and a warning names it.
# - Tool state, for caches and other files the tool wrote itself:
#   ReadUTF8OrLocale / TryReadUTF8OrLocale(..., skipBom=True, warn=False). As parsed input, but the locale fallback is only logged at verbose level
#   as the user did nothing wrong, and the tool writes the file as UTF-8 the next time it saves it.

import codecs
import io
import locale

from FslBuildGen import IOUtil
from FslBuildGen.Exceptions import UsageErrorException
from FslBuildGen.Log import Log


def _Decode(content: bytes, encoding: str, newline: str | None, errors: str) -> str:
    """Decode the content to the text a text mode open(filename, newline=newline, encoding=encoding, errors=errors) reads: the newlines are translated the
    same way, as it is the same io.TextIOWrapper doing it. A UnicodeDecodeError holds the byte offset in 'content'.
    """
    with io.TextIOWrapper(io.BytesIO(content), encoding=encoding, errors=errors, newline=newline) as reader:
        return reader.read()


def ReadUTF8OrLocale(log: Log, filename: str, what: str, skipBom: bool = False, warn: bool = True) -> str:
    """Read a text file that is expected to be UTF-8, with universal newlines.
    A file that is not valid UTF-8 (for example one saved as ANSI) is read with the locale encoding, like older versions of the tool did on Windows,
    and a message names the file. A file that can not be read with the locale encoding either is rejected with a UsageErrorException.
    what: what the file is, a singular noun that the messages use as 'The <what>' and '<What>s', for example 'gen file'.
    skipBom: a UTF-8 byte order mark at the start of the file is not part of the returned text (parsers like json.loads reject it).
    warn: true: the locale fallback is a warning that asks for the file to be saved as UTF-8.
          false: it is only logged at verbose level, for files the tool writes itself.
    """
    content = IOUtil.ReadBinaryFile(filename)
    bomLength = len(codecs.BOM_UTF8) if skipBom and content.startswith(codecs.BOM_UTF8) else 0
    if bomLength > 0:
        content = content[bomLength:]
    try:
        return _Decode(content, "utf-8", None, "strict")
    except UnicodeDecodeError as utf8Error:
        byteOffset = bomLength + utf8Error.start
        # locale.getencoding ignores the Python UTF-8 mode, so this stays the locale encoding once UTF-8 mode is the default
        localeEncoding = locale.getencoding()
        try:
            text = _Decode(content, localeEncoding, None, "strict")
        except UnicodeDecodeError as localeError:
            message = f"The {what} '{filename}' is not valid UTF-8 (byte offset {byteOffset}) and it can not be read with the locale encoding '{localeEncoding}' either"
            if warn:
                message += f". {what[:1].upper()}{what[1:]}s must be saved as UTF-8"
            raise UsageErrorException(message) from localeError
        message = f"The {what} '{filename}' is not valid UTF-8 (byte offset {byteOffset}), it was read with the locale encoding '{localeEncoding}'"
        if warn:
            log.DoPrintWarning(f"{message}. Please save it as UTF-8")
        else:
            log.LogPrint(message)
        return text


def TryReadUTF8OrLocale(log: Log, filename: str, what: str, skipBom: bool = False, warn: bool = True) -> str | None:
    """Like ReadUTF8OrLocale, but a file that can not be opened or read (for example one that is not there) gives None.
    A file that can be read but not decoded is not hidden, it still raises the UsageErrorException.
    """
    try:
        return ReadUTF8OrLocale(log, filename, what, skipBom, warn)
    except OSError:
        return None


def ReadUTF8PassThrough(log: Log, filename: str, what: str, newline: str | None = None, warnedFiles: set[str] | None = None) -> str:
    """Read a text file the tool copies or edits in place as UTF-8. A byte order mark is kept as the first character.
    Bytes that are not valid UTF-8 never fail the read: they are kept as lone surrogates ('surrogateescape'), so writing the text with
    IOUtil.WriteFileUTF8(..., errors="surrogateescape") gives the same bytes back, and one warning names the file.
    what: what the file is, the warning uses it as 'The <what>', for example 'template'.
    newline: as for open, None is universal newlines.
    warnedFiles: for a caller that reads the same file many times: the file names that have given the warning. A file name in the set does not give
                 it again and a file name that gives it is added.
    """
    content = IOUtil.ReadBinaryFile(filename)
    try:
        return _Decode(content, "utf-8", newline, "strict")
    except UnicodeDecodeError as utf8Error:
        if warnedFiles is None or filename not in warnedFiles:
            if warnedFiles is not None:
                warnedFiles.add(filename)
            log.DoPrintWarning(
                f"The {what} '{filename}' is not valid UTF-8 (byte offset {utf8Error.start}), the bytes that are not UTF-8 are kept as they are. "
                "Please save it as UTF-8"
            )
        return _Decode(content, "utf-8", newline, "surrogateescape")


def TryReadUTF8PassThrough(log: Log, filename: str, what: str, newline: str | None = None, warnedFiles: set[str] | None = None) -> str | None:
    """Like ReadUTF8PassThrough, but a file that can not be opened or read gives None"""
    try:
        return ReadUTF8PassThrough(log, filename, what, newline, warnedFiles)
    except OSError:
        return None
