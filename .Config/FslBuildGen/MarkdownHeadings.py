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

# The headings of a markdown file, for the table of contents FslBuildDoc writes: which lines are headings on the sites that show the
# file (GitHub and the others follow CommonMark), with their level and their text.
#
# - A heading that starts with one to six '#' and a space ('## Title'). A closing sequence of '#' is no part of its text ('## Title ##').
# - A heading that is underlined: the lines of a paragraph with a line of '=' (level 1) or a line of '-' (level 2) under them.
# - A line inside a fenced code block is no heading, whatever it looks like: '# a comment of a shell script', a line of '---' in a
#   yaml file. A code block starts with a fence of three or more '`' or '~' and ends with a fence of the same character that is at least
#   as long, or with the file.
#
# What is deliberately not followed, so the tool keeps listing what it listed:
# - a '#' heading is one at the start of the line only (CommonMark allows up to three spaces in front of it), and it needs the space
#   ('#' on its own and '#' followed by a tab are no headings here).
# - the lines of a block quote ('> ...'), of a list item and of a html block are not looked into for underlined headings: an underline
#   needs a plain paragraph above it. A line of '---' after anything else is a rule and not a heading.
#
# The byte order mark of a file is the first character of its first line: the caller passes that line without it (MarkdownIO).

import re
from collections.abc import Sequence

MaxLevel = 6

# A fence: up to three spaces, then three or more '`' or '~'. What follows the fence that opens a block names the language.
_g_fence = re.compile(r"^ {0,3}(`{3,}|~{3,})(.*)$")
# The same for a line of a list item, which is indented by the list
_g_fenceInContainer = re.compile(r"^[ \t]*(`{3,}|~{3,})(.*)$")
_g_underline = re.compile(r"^ {0,3}(=+|-+)[ \t]*$")
# A rule: three or more '*', '-' or '_' with nothing but spaces between them
_g_thematicBreak = re.compile(r"^ {0,3}([*_-])[ \t]*(\1[ \t]*){2,}$")
_g_listItem = re.compile(r"^ {0,3}([*+-]|\d{1,9}[.)])([ \t]|$)")
_g_blockQuote = re.compile(r"^ {0,3}>")
_g_html = re.compile(r"^ {0,3}<")
_g_htmlCommentStart = re.compile(r"^ {0,3}<!--")
_g_tableRow = re.compile(r"^ {0,3}\|")
# The line under the header of a table: a cell of '-' for each column, with a '|' between the cells
_g_tableDelimiterRow = re.compile(r"^ {0,3}\|?[ \t]*:?-+:?[ \t]*(\|[ \t]*:?-+:?[ \t]*)*\|?[ \t]*$")
_g_indentedCode = re.compile(r"^( {4}|\t| {1,3}\t)")
_g_continuationIndent = re.compile(r"^( {2}|\t| \t)")


class MarkdownHeading:
    def __init__(self, lineIndex: int, level: int, text: str) -> None:
        super().__init__()
        # The index of the line the heading starts on
        self.LineIndex = lineIndex
        # 1 to 6
        self.Level = level
        # The text of the heading, without the white space around it
        self.Text = text


def TryGetAtxHeading(line: str) -> tuple[int, str] | None:
    """The level (1 to 6) and the text of a line that is a heading: one to six '#' and a space start it. A closing sequence of '#' that
    has a space in front of it ('## Title ##') is no part of the text.
    """
    level = len(line) - len(line.lstrip("#"))
    if level < 1 or level > MaxLevel or not line.startswith(" ", level):
        return None
    text = line[level + 1 :].strip(" \t")
    withoutClosing = text.rstrip("#")
    if len(withoutClosing) != len(text) and (len(withoutClosing) == 0 or withoutClosing[-1] in " \t"):
        text = withoutClosing.rstrip(" \t")
    return (level, text)


class _Scanner:
    """Goes through the lines of a file and keeps what decides if a line can be a heading"""

    def __init__(self) -> None:
        super().__init__()
        self.Headings: list[MarkdownHeading] = []
        # The fence that opened the code block the lines are in: its character and its length
        self.__FenceCharacter = ""
        self.__FenceLength = 0
        # The lines of the paragraph the lines are in: (line index, text)
        self.__Paragraph: list[tuple[int, str]] = []
        # True while the lines belong to a list item or a block quote
        self.__InContainer = False
        self.__BlankLineInContainer = False
        # What ends the html block the lines are in: the text that ends a comment, an empty text for a block a blank line ends
        self.__InHtmlBlock = False
        self.__HtmlBlockEnd = ""
        # True while the lines are the rows of a table
        self.__InTable = False

    def __TryHandleFence(self, line: str) -> bool:
        """True if the line belongs to a fenced code block: the fence that opens it, a line of it, or the fence that closes it"""
        match = (_g_fenceInContainer if self.__InContainer else _g_fence).match(line)
        if self.__FenceLength > 0:
            if match is not None and match.group(1)[0] == self.__FenceCharacter and len(match.group(1)) >= self.__FenceLength:
                # A closing fence is followed by nothing
                if len(match.group(2).strip(" \t")) == 0:
                    self.__FenceLength = 0
            return True
        if match is None:
            return False
        fence = match.group(1)
        # The text after a fence of '`' can not hold a '`': that is a code span of a paragraph ('```code```')
        if fence[0] == "`" and "`" in match.group(2):
            return False
        self.__FenceCharacter = fence[0]
        self.__FenceLength = len(fence)
        self.__Paragraph = []
        self.__InHtmlBlock = False
        self.__InTable = False
        if _g_continuationIndent.match(line) is None:
            # A code block at the start of the line is no part of a list item
            self.__InContainer = False
        return True

    def __StartHtmlBlock(self, line: str) -> None:
        self.__Paragraph = []
        if _g_htmlCommentStart.match(line) is not None:
            # A comment ends on the line that closes it, which can be the line that opens it
            self.__HtmlBlockEnd = "-->"
            self.__InHtmlBlock = "-->" not in line[line.index("<!--") + 4 :]
        else:
            self.__HtmlBlockEnd = ""
            self.__InHtmlBlock = True

    def __AddAtxHeading(self, lineIndex: int, heading: tuple[int, str]) -> None:
        self.Headings.append(MarkdownHeading(lineIndex, heading[0], heading[1]))
        self.__Paragraph = []
        self.__InContainer = False
        self.__InHtmlBlock = False
        self.__InTable = False

    def Add(self, lineIndex: int, line: str) -> None:
        if self.__TryHandleFence(line):
            return

        # A '#' heading is one wherever it is outside of a code block, as it always was for the tool
        atxHeading = TryGetAtxHeading(line)
        if atxHeading is not None:
            self.__AddAtxHeading(lineIndex, atxHeading)
            return

        if len(line.strip(" \t")) == 0:
            self.__Paragraph = []
            self.__InTable = False
            if self.__InHtmlBlock and len(self.__HtmlBlockEnd) == 0:
                self.__InHtmlBlock = False
            self.__BlankLineInContainer = self.__InContainer
            return

        if self.__InHtmlBlock:
            if len(self.__HtmlBlockEnd) > 0 and self.__HtmlBlockEnd in line:
                self.__InHtmlBlock = False
            return
        if self.__InTable:
            return

        isThematicBreak = _g_thematicBreak.match(line) is not None
        startsContainer = not isThematicBreak and (_g_listItem.match(line) is not None or _g_blockQuote.match(line) is not None)
        if self.__InContainer and not startsContainer:
            isIndented = _g_continuationIndent.match(line) is not None
            # A line of the list item or of the block quote, unless it ends it: a line that is not indented after a blank line, and a
            # rule or a line of html that is not indented
            endsContainer = not isIndented and (self.__BlankLineInContainer or isThematicBreak or _g_html.match(line) is not None)
            if not endsContainer:
                return
            self.__InContainer = False
        self.__BlankLineInContainer = False

        if len(self.__Paragraph) > 0:
            underline = _g_underline.match(line)
            if underline is not None:
                text = " ".join(text for _, text in self.__Paragraph)
                self.Headings.append(MarkdownHeading(self.__Paragraph[0][0], 1 if underline.group(1)[0] == "=" else 2, text))
                self.__Paragraph = []
                return
        if isThematicBreak:
            self.__Paragraph = []
            return
        if len(self.__Paragraph) > 0 and "|" in line and _g_tableDelimiterRow.match(line) is not None:
            # The paragraph was the header of a table, what follows are its rows
            self.__Paragraph = []
            self.__InTable = True
            return
        if startsContainer:
            # An ordered list interrupts a paragraph only when it starts with 1, and an empty list item does not: both are rare enough
            # to end the paragraph here anyway, as no heading is lost by it but one that an underline would make of such a line
            self.__Paragraph = []
            self.__InContainer = True
            return
        if _g_html.match(line) is not None:
            self.__StartHtmlBlock(line)
            return
        if _g_tableRow.match(line) is not None:
            self.__Paragraph = []
            return
        if len(self.__Paragraph) == 0 and _g_indentedCode.match(line) is not None:
            # An indented code block. Inside a paragraph an indented line is one more line of it.
            return
        self.__Paragraph.append((lineIndex, line.strip(" \t")))


def FindHeadings(lines: Sequence[str]) -> list[MarkdownHeading]:
    """The headings of a file in the order of the file. lines: the lines of the file without their newline, the first one without the
    byte order mark of the file.
    """
    scanner = _Scanner()
    for lineIndex, line in enumerate(lines):
        scanner.Add(lineIndex, line)
    return scanner.Headings
