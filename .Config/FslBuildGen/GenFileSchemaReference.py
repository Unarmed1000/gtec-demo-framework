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

# The schema reference of a gen file: the 'xsi:noNamespaceSchemaLocation' attribute of its root element. This module finds the reference in the
# bytes of a gen file and replaces its value. It works on bytes and reads or writes no file.
#
# A gen file is edited by hand, so a replacement changes the bytes of the value and nothing else: the byte order mark, the newlines, the
# comments and the layout of the file stay as they are. An XML parser can not do that (it does not tell where a value is and it writes the
# document its own way), so the root element is scanned here:
# - a UTF-8 byte order mark is skipped, then the XML declaration, whitespace, comments and processing instructions
# - the root element has to be 'FslBuildGen'
# - its attributes are walked by their quotes, so a '>' in a value does not end the element
# - the reference is the attribute '<prefix>:noNamespaceSchemaLocation' where the prefix is bound to the XML schema instance namespace by a
#   'xmlns:<prefix>' attribute of the root element (the prefix is 'xsi' by custom, not by rule)
# The scan is not trusted on its own. The XML parser has to read the whole file and find the attribute too, and after a replacement it has to
# read the new value and find the two documents equal apart from it.
#
# What the scan can not be sure about is refused, never guessed: an encoding other than UTF-8, a DOCTYPE (it can define entities), another
# root element, a namespace declaration with a character or entity reference in it and a file that does not parse.

import re
import xml.etree.ElementTree as ET
from enum import Enum

_g_rootName = "FslBuildGen"
_g_instanceNamespace = b"http://www.w3.org/2001/XMLSchema-instance"
_g_attributeLocalName = b"noNamespaceSchemaLocation"
# The attribute as the XML parser names it
_g_attributeKey = "{http://www.w3.org/2001/XMLSchema-instance}noNamespaceSchemaLocation"

_g_utf8ByteOrderMark = b"\xef\xbb\xbf"
# UTF-16 and UTF-32, little and big endian
_g_otherByteOrderMarks = (b"\xff\xfe", b"\xfe\xff", b"\x00\x00\xfe\xff")
_g_whitespace = b" \t\r\n"
# What ends the name of the root element or of an attribute
_g_nameEnd = b" \t\r\n=>\"'"
_g_declaredEncoding = re.compile(rb"""\sencoding\s*=\s*(["'])(.*?)\1""")


class ReferenceStatus(Enum):
    # The root element has the schema reference
    Found = 1
    # The root element has no schema reference
    Missing = 2
    # The file is not one the reference can be located in for sure
    Unsupported = 3


class InspectResult:
    """What Inspect found.
    Value is the reference as the XML parser reads it (Found only).
    Reason tells why the file is not supported (Unsupported only).
    ValueStart and ValueEnd are the byte offsets of the value as it is written in the file, between its quotes, and Quote is the quote
    character (Found only).
    """

    def __init__(self, status: ReferenceStatus, value: str | None, reason: str | None, valueStart: int = -1, valueEnd: int = -1, quote: bytes = b"") -> None:
        super().__init__()
        self.Status = status
        self.Value = value
        self.Reason = reason
        self.ValueStart = valueStart
        self.ValueEnd = valueEnd
        self.Quote = quote


class GenFileSchemaReferenceError(Exception):
    """The schema reference of a gen file could not be replaced"""


class _Unsupported(Exception):
    """The scan gives up, the message is the reason"""


def _SkipWhitespace(content: bytes, position: int) -> int:
    while position < len(content) and content[position] in _g_whitespace:
        position += 1
    return position


def _SkipTo(content: bytes, position: int, endMark: bytes, what: str) -> int:
    """The position after the end mark"""
    end = content.find(endMark, position)
    if end < 0:
        raise _Unsupported(f"{what} is not closed")
    return end + len(endMark)


def _FindRootElement(content: bytes) -> int:
    """The position of the '<' of the root element"""
    for byteOrderMark in _g_otherByteOrderMarks:
        if content.startswith(byteOrderMark):
            raise _Unsupported("the file is UTF-16 or UTF-32, only UTF-8 is supported")
    if b"\x00" in content:
        raise _Unsupported("the file holds a zero byte, it is not UTF-8 text")
    try:
        content.decode("utf-8")
    except UnicodeDecodeError as ex:
        raise _Unsupported(f"the file is not valid UTF-8 (byte offset {ex.start})") from ex

    position = len(_g_utf8ByteOrderMark) if content.startswith(_g_utf8ByteOrderMark) else 0
    # The XML declaration, not a processing instruction with a name that starts with 'xml'. It is skipped below like the others.
    if content.startswith(b"<?xml", position) and content[position + 5 : position + 6] in (b" ", b"\t", b"\r", b"\n"):
        end = _SkipTo(content, position, b"?>", "the XML declaration")
        match = _g_declaredEncoding.search(content, position, end)
        if match is not None and match.group(2).lower() != b"utf-8":
            raise _Unsupported(f"the file declares the encoding '{match.group(2).decode('utf-8')}', only UTF-8 is supported")

    while True:
        position = _SkipWhitespace(content, position)
        if content.startswith(b"<!--", position):
            position = _SkipTo(content, position + 4, b"-->", "a comment")
        elif content.startswith(b"<?", position):
            position = _SkipTo(content, position + 2, b"?>", "a processing instruction")
        elif content.startswith(b"<!", position):
            # The only declaration XML has in front of the root element
            raise _Unsupported("the file has a DOCTYPE")
        elif content.startswith(b"<", position):
            return position
        else:
            raise _Unsupported("the file has no root element where one is expected")


def _ReadRootAttributes(content: bytes, position: int) -> list[tuple[bytes, int, int, bytes]]:
    """The attributes of the root element as they are written: name, start and end of the value between its quotes, and the quote.
    position is the '<' of the root element. Which element it is, is checked with the XML parser.
    """
    while position < len(content) and content[position] not in _g_nameEnd:
        position += 1

    attributes: list[tuple[bytes, int, int, bytes]] = []
    while True:
        position = _SkipWhitespace(content, position)
        if content.startswith(b">", position) or content.startswith(b"/>", position):
            return attributes
        nameStart = position
        while position < len(content) and content[position] not in _g_nameEnd:
            position += 1
        name = content[nameStart:position]
        position = _SkipWhitespace(content, position)
        if len(name) <= 0 or not content.startswith(b"=", position):
            raise _Unsupported("the attributes of the root element can not be read")
        position = _SkipWhitespace(content, position + 1)
        quote = content[position : position + 1]
        if quote not in (b'"', b"'"):
            raise _Unsupported("the attributes of the root element can not be read")
        valueStart = position + 1
        valueEnd = content.find(quote, valueStart)
        if valueEnd < 0:
            raise _Unsupported("the attributes of the root element can not be read")
        attributes.append((name, valueStart, valueEnd, quote))
        position = valueEnd + 1


def _FindCandidates(content: bytes, attributes: list[tuple[bytes, int, int, bytes]]) -> list[tuple[bytes, int, int, bytes]]:
    """The attributes that are the schema reference going by the namespace prefixes the root element declares"""
    names: set[bytes] = set()
    for name, valueStart, valueEnd, _quote in attributes:
        if name == b"xmlns" or name.startswith(b"xmlns:"):
            value = content[valueStart:valueEnd]
            if b"&" in value:
                # It could spell the namespace we look for, or one that only looks like it
                raise _Unsupported(f"the namespace declaration '{name.decode('utf-8')}' of the root element holds a '&'")
            if value == _g_instanceNamespace:
                # For 'xmlns' itself that is a name no attribute has
                names.add(name[len(b"xmlns:") :] + b":" + _g_attributeLocalName)
    return [entry for entry in attributes if entry[0] in names]


def Inspect(content: bytes) -> InspectResult:
    """Find the schema reference in the bytes of a gen file"""
    try:
        candidates = _FindCandidates(content, _ReadRootAttributes(content, _FindRootElement(content)))
    except _Unsupported as ex:
        return InspectResult(ReferenceStatus.Unsupported, None, str(ex))

    # The scan found what it found, the XML parser has the last word
    try:
        root = ET.fromstring(content)
    except ET.ParseError as ex:
        return InspectResult(ReferenceStatus.Unsupported, None, f"the file does not parse: {ex}")
    if root.tag != _g_rootName:
        return InspectResult(ReferenceStatus.Unsupported, None, f"the root element is '{root.tag}', not '{_g_rootName}'")
    value = root.attrib.get(_g_attributeKey)
    if len(candidates) == 0 and value is None:
        return InspectResult(ReferenceStatus.Missing, None, None)
    if len(candidates) != 1 or value is None:
        return InspectResult(ReferenceStatus.Unsupported, None, "the schema reference of the root element could not be located")
    _name, valueStart, valueEnd, quote = candidates[0]
    return InspectResult(ReferenceStatus.Found, value, None, valueStart, valueEnd, quote)


def _CheckNewValue(newValue: str) -> None:
    for character in newValue:
        code = ord(character)
        if code < 0x20 or 0x7F <= code <= 0x9F:
            raise GenFileSchemaReferenceError(f"The schema reference can not hold the control character U+{code:04X}")
    try:
        newValue.encode("utf-8")
    except UnicodeEncodeError as ex:
        raise GenFileSchemaReferenceError("The schema reference can not be written as UTF-8") from ex


def _Escape(value: str, quote: bytes) -> bytes:
    """The value as it is written between the quotes"""
    escaped = value.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
    escaped = escaped.replace('"', "&quot;") if quote == b'"' else escaped.replace("'", "&apos;")
    return escaped.encode("utf-8")


def _WithoutReference(content: bytes) -> bytes:
    """The document as the XML parser writes it, without the schema reference"""
    root = ET.fromstring(content)
    del root.attrib[_g_attributeKey]
    return ET.tostring(root)


def Replace(content: bytes, newValue: str) -> bytes:
    """Returns the bytes of the gen file with newValue as its schema reference. Only the bytes of the value differ.
    Content that already holds the value is returned as it is.
    Raises a GenFileSchemaReferenceError when the content has no schema reference, is not supported or when the result does not check out.
    """
    found = Inspect(content)
    if found.Status == ReferenceStatus.Missing:
        raise GenFileSchemaReferenceError("The root element has no schema reference to replace")
    if found.Status != ReferenceStatus.Found:
        raise GenFileSchemaReferenceError(f"The schema reference can not be replaced: {found.Reason}")
    _CheckNewValue(newValue)
    if found.Value == newValue:
        return content

    newContent = content[: found.ValueStart] + _Escape(newValue, found.Quote) + content[found.ValueEnd :]

    # The bytes around the value are the same by construction. What is checked is that the value was the reference and nothing else.
    replaced = Inspect(newContent)
    if replaced.Value != newValue:
        raise GenFileSchemaReferenceError("The schema reference could not be replaced: the new reference is not read back")
    if _WithoutReference(content) != _WithoutReference(newContent):
        raise GenFileSchemaReferenceError("The schema reference could not be replaced: more than the reference would change")
    return newContent
