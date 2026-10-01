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

# The encoding of a ninja build file ('build.ninja'). A build file names the files to build, and ninja has to find those files, so the file is
# written in the encoding the ninja that runs it reads a build file with:
#
# - Not on Windows: UTF-8.
# - On Windows it depends on the ninja. Ninja 1.11 and newer run with UTF-8 as their code page where Windows supports that (Windows 10 1903 and
#   newer) and tell which one it is: 'ninja -t wincodepage' prints 'Build file encoding: UTF-8' or 'Build file encoding: ANSI'. An older ninja
#   does not know that tool and reads the build file in the ANSI code page.
#
# A build file that only holds ASCII has the same bytes in both encodings.

import io
import os
import subprocess

from FslBuildGen import IOUtil
from FslBuildGen.BuildConfig.ClangExeInfo import ClangExeInfo
from FslBuildGen.DataTypes import BuildPlatformType
from FslBuildGen.Exceptions import UsageErrorException
from FslBuildGen.Log import Log
from FslBuildGen.PlatformUtil import PlatformUtil

_g_utf8 = "utf-8"
# The first ninja that can read a UTF-8 build file on Windows, and that knows the 'wincodepage' tool
_g_firstUtf8Version = (1, 11)
_g_queryTimeoutSeconds = 30
_g_queryAnswerPrefix = "Build file encoding:"

# The encoding each ninja executable was found to read, so a ninja is asked once for a run of the tool
g_encodingCache: dict[str, str] = {}


def GetAnsiEncoding() -> str:
    """The codec of the ANSI code page of this process, which is the code page a ninja that reads ANSI build files uses too"""
    return "mbcs"


def _IsWindows() -> bool:
    return PlatformUtil.DetectBuildPlatformType() == BuildPlatformType.Windows


def _TryAskNinja(command: str) -> str | None:
    """Ask ninja which encoding it reads a build file with: 'UTF-8' or 'ANSI', None if it gave no answer"""
    try:
        result = subprocess.run([command, "-t", "wincodepage"], stdin=subprocess.DEVNULL, capture_output=True, timeout=_g_queryTimeoutSeconds, check=False)
    except OSError, subprocess.SubprocessError:
        return None
    if result.returncode != 0:
        return None
    for line in result.stdout.decode("ascii", errors="replace").splitlines():
        if line.startswith(_g_queryAnswerPrefix):
            return line[len(_g_queryAnswerPrefix) :].strip()
    return None


def _DetermineEncoding(ninjaExeInfo: ClangExeInfo) -> tuple[str, str]:
    """The encoding and why it is that one"""
    if not _IsWindows():
        return (_g_utf8, "ninja reads UTF-8 build files on this platform")
    version = ninjaExeInfo.Version
    if (version.Major, version.Minor) < _g_firstUtf8Version:
        return (GetAnsiEncoding(), f"ninja {version} is older than 1.11, it reads build files in the ANSI code page")
    answer = _TryAskNinja(ninjaExeInfo.Command)
    if answer == "UTF-8":
        return (_g_utf8, "ninja reports that it reads UTF-8 build files")
    if answer is None:
        return (GetAnsiEncoding(), "ninja did not tell which encoding it reads, so it is taken to read build files in the ANSI code page")
    return (GetAnsiEncoding(), f"ninja reports that it reads build files in the '{answer}' code page")


def GetEncoding(log: Log, ninjaExeInfo: ClangExeInfo) -> str:
    """The name of the codec the ninja executable reads a build file with. A ninja is examined once for a run of the tool."""
    encoding = g_encodingCache.get(ninjaExeInfo.Command)
    if encoding is None:
        encoding, reason = _DetermineEncoding(ninjaExeInfo)
        g_encodingCache[ninjaExeInfo.Command] = encoding
        log.LogPrint(f"Ninja build files for '{ninjaExeInfo.Command}' are written as '{encoding}': {reason}")
    return encoding


def _Encode(content: str, encoding: str) -> bytes:
    """Encode the content to the bytes a text mode file of that encoding holds: with the newline of the platform"""
    buffer = io.BytesIO()
    with io.TextIOWrapper(buffer, encoding=encoding, errors="strict") as writer:
        writer.write(content)
        writer.flush()
        return buffer.getvalue()


def _CanEncode(text: str, encoding: str) -> bool:
    try:
        text.encode(encoding)
        return True
    except UnicodeEncodeError:
        return False


def _TryFindCharacterNotIn(content: str, encoding: str) -> int | None:
    """The index of the first character the encoding can not hold. The error of a code page codec does not tell where it is."""
    offset = 0
    for line in content.splitlines(keepends=True):
        if not _CanEncode(line, encoding):
            for index, character in enumerate(line):
                if not _CanEncode(character, encoding):
                    return offset + index
        offset += len(line)
    return None


def _WordAt(content: str, index: int) -> str:
    """The word of a build file (for example a path, a space in a path is written '$ ') that holds the character at the index"""

    def IsSeparator(position: int) -> bool:
        return content[position].isspace() and not (content[position] == " " and position > 0 and content[position - 1] == "$")

    start = index
    while start > 0 and not IsSeparator(start - 1):
        start -= 1
    end = index
    while end < len(content) and not IsSeparator(end):
        end += 1
    return content[start:end].replace("$ ", " ").replace("$:", ":")


def _Printable(text: str) -> str:
    """The text with what can not be printed (half a surrogate pair) written as an escape"""
    return text.encode("utf-8", errors="backslashreplace").decode("utf-8")


def _CreateNotEncodableError(ninjaExeInfo: ClangExeInfo, filename: str, content: str, encoding: str) -> UsageErrorException:
    index = _TryFindCharacterNotIn(content, encoding)
    what = "a character"
    if index is not None:
        character = content[index]
        shownCharacter = f"'{character}' " if _CanEncode(character, _g_utf8) else ""
        what = f"the character {shownCharacter}(U+{ord(character):04X}) of '{_Printable(_WordAt(content, index))}'"
    if encoding == _g_utf8:
        return UsageErrorException(f"The ninja build file '{filename}' can not be written: {what} can not be written as UTF-8")
    return UsageErrorException(
        f"The ninja build file '{filename}' can not be written: ninja {ninjaExeInfo.Version} reads build files in the ANSI code page, and "
        f"{what} does not exist in it. Ninja 1.11 or newer reads UTF-8 build files"
    )


def _NormalizeNewlines(content: bytes) -> bytes:
    return content.replace(b"\r\n", b"\n").replace(b"\r", b"\n")


def WriteBuildFileIfChanged(log: Log, ninjaExeInfo: ClangExeInfo, filename: str, content: str) -> bool:
    """Write a ninja build file in the encoding the ninja executable reads, unless the file already holds the content in that encoding.
    Returns true if the file was written. The content is encoded before the file is opened, so content that can not be written leaves an existing
    file as it was.
    """
    encoding = GetEncoding(log, ninjaExeInfo)
    try:
        encodedContent = _Encode(content, encoding)
    except UnicodeEncodeError as ex:
        raise _CreateNotEncodableError(ninjaExeInfo, filename, content, encoding) from ex

    if os.path.exists(filename):
        if not os.path.isfile(filename):
            raise OSError(f"'{filename}' exist but it's not a file")
        # As a text file is compared: the newline form of the existing file is not a change
        if _NormalizeNewlines(IOUtil.ReadBinaryFile(filename)) == _NormalizeNewlines(encodedContent):
            return False
    IOUtil.WriteBinaryFile(filename, encodedContent)
    return True
