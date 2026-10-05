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

# A json file of Visual Studio Code that the tool updates: '.vscode/settings.json' and '.vscode/launch.json'.
#
# The file belongs to the user, the tool only sets the values it knows about:
# - A file the tool can not read as plain JSON is left as it is, with one warning that says why. Visual Studio Code accepts comments and
#   a comma after the last entry in these files, the tool can not write such a file back without losing them.
# - One kind of comment is read and kept: the '//' lines directly after the opening brace of the file. The 'launch.json' Visual Studio Code
#   creates starts with three of them. They are written back as they are, in their place (see SplitLeadingComments).
# - What the user has in the file stays: the keys keep their order (a key the tool adds comes last) and the file keeps its indentation.
# - Nothing is written when the tool changed no value.
# A file that does not exist is created the way it always was: the keys sorted, four spaces of indentation.
#
# Visual Studio Code reads and writes the files as UTF-8. A byte order mark is accepted and a file in the locale encoding (as older
# versions of the tool wrote it) is read with a warning; such a file is written back as UTF-8 also when no value changed.

import json
from typing import Any

from FslBuildGen import IOUtil, TextFileReader
from FslBuildGen.Log import Log

# The indentation of a file the tool creates, and of a file that shows none (all of it on one line)
DefaultIndent = 4

# The white space of a json text that can be on a line
_g_lineWhiteSpace = " \t\r"
_g_commentStart = "//"


def SplitLeadingComments(text: str) -> tuple[list[str], str]:
    """The comment lines at the top of the object of a json text, and the text without them.
    A comment line is a line whose first characters after its white space are '//'. Only the lines directly after the opening brace count:
    - The '{' is the first character of the text that is not white space and nothing else is on its line. So it opens the object of the
      file, and no member (and with it no string) has started when its line ends.
    - The lines after it are taken while they are comment lines or empty. The first other line ends the block: it is where the first member
      (or the closing brace) starts. Up to there the text holds white space and comment lines only, so no '//' that was taken is inside a
      string.
    Returns the lines of the block as they are in the text, from the first comment line to the last one (an empty line between two of them
    belongs to it), and the text with each comment line emptied. The lines keep their numbers that way, so what the json parser says about
    the rest of the text names the line of the file. Without such a block the result is no lines and the text as it is.
    """
    lines = text.split("\n")
    braceIndex = 0
    while braceIndex < len(lines) and len(lines[braceIndex].strip(_g_lineWhiteSpace)) <= 0:
        braceIndex += 1
    if braceIndex >= len(lines) or lines[braceIndex].strip(_g_lineWhiteSpace) != "{":
        return ([], text)
    lastCommentIndex = -1
    index = braceIndex + 1
    while index < len(lines):
        lineContent = lines[index].strip(_g_lineWhiteSpace)
        if lineContent.startswith(_g_commentStart):
            lastCommentIndex = index
        elif len(lineContent) > 0:
            break
        index += 1
    if lastCommentIndex < 0:
        return ([], text)
    commentLines = lines[braceIndex + 1 : lastCommentIndex + 1]
    emptiedLines = ["" if line.strip(_g_lineWhiteSpace).startswith(_g_commentStart) else line for line in commentLines]
    return (commentLines, "\n".join([*lines[: braceIndex + 1], *emptiedLines, *lines[lastCommentIndex + 1 :]]))


def _InsertAfterOpeningBrace(jsonText: str, commentLines: list[str]) -> str:
    """The text of a json object (as json.dumps writes it with an indentation) with the lines directly after its opening brace"""
    rest = jsonText[1:]
    if not rest.startswith("\n"):
        # An object without members is written '{}'
        rest = "\n" + rest
    return "{\n" + "\n".join(commentLines) + rest


def DetectIndent(text: str) -> str | int:
    """The indentation of a json text: the white space the first indented line starts with. DefaultIndent when no line is indented."""
    for line in text.split("\n")[1:]:
        content = line.lstrip(" \t")
        if len(content) > 0 and len(content) < len(line):
            return line[: len(line) - len(content)]
    return DefaultIndent


class VSCodeJsonFile:
    def __init__(self, filename: str, text: str | None, content: dict[str, Any]) -> None:
        """text is what the file holds, None for a file that does not exist. content is the object of the file: the tool changes it and
        calls Save.
        """
        super().__init__()
        self.Filename = filename
        self.Content = content
        self.__text = text
        # The comment lines after the opening brace of the file, and the json of the file
        self.__commentLines, self.__plainText = SplitLeadingComments("" if text is None else text)
        self.__originalJson = _ToExactJson(content)

    @property
    def IsNew(self) -> bool:
        return self.__text is None

    @staticmethod
    def TryLoad(log: Log, filename: str, what: str) -> VSCodeJsonFile | None:
        """Read the file. what says what the file is in the messages ('Visual Studio Code settings file').
        Returns None for a file that can not be updated: it is not plain JSON or it does not hold an object. A warning says so.
        The comment lines directly after the opening brace are not part of the JSON, they are kept (see SplitLeadingComments).
        A file that does not exist gives an empty object.
        """
        text = TextFileReader.TryReadUTF8OrLocale(log, filename, what, skipBom=True)
        if text is None:
            return VSCodeJsonFile(filename, None, {})
        _commentLines, plainText = SplitLeadingComments(text)
        try:
            content = json.loads(plainText)
        except ValueError as ex:
            WarnNotUpdated(log, filename, what, f"it is not plain JSON ({ex}), a file with a comment or with a comma after the last entry is left as it is")
            return None
        if not isinstance(content, dict):
            WarnNotUpdated(log, filename, what, "it does not hold a JSON object")
            return None
        return VSCodeJsonFile(filename, text, content)

    def Save(self) -> bool:
        """Write the file when the tool changed a value. Returns True when the file was written."""
        if self.__text is None:
            return IOUtil.WriteFileUTF8IfChanged(self.Filename, json.dumps(self.Content, ensure_ascii=False, sort_keys=True, indent=DefaultIndent))
        if _ToExactJson(self.Content) == self.__originalJson:
            # Nothing to update. A file that is not UTF-8 was read with the locale encoding: it is written back as UTF-8 so Visual Studio
            # Code reads what the tool read
            if _IsUtf8(IOUtil.ReadBinaryFile(self.Filename)):
                return False
            return IOUtil.WriteFileUTF8IfChanged(self.Filename, self.__text)
        text = json.dumps(self.Content, ensure_ascii=False, sort_keys=False, indent=DetectIndent(self.__plainText))
        if len(self.__commentLines) > 0:
            text = _InsertAfterOpeningBrace(text, self.__commentLines)
        if self.__plainText.endswith("\n"):
            text += "\n"
        return IOUtil.WriteFileUTF8IfChanged(self.Filename, text)


def WarnNotUpdated(log: Log, filename: str, what: str, reason: str) -> None:
    log.DoPrintWarning(f"The {what} '{filename}' was not updated: {reason}")


def _ToExactJson(content: dict[str, Any]) -> str:
    """A text that is the same for the same values in the same order only (1 and true are equal as python values, not as json)"""
    return json.dumps(content, ensure_ascii=False, sort_keys=False)


def _IsUtf8(content: bytes) -> bool:
    try:
        content.decode("utf-8")
    except UnicodeDecodeError:
        return False
    return True
