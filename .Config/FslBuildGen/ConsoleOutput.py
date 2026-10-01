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

# The tools print with 'print'. When the output is not a console (it is redirected to a file or a pipe) the text is encoded with the locale
# encoding, on Windows that is the ANSI code page (cp1252). A character that encoding does not have (a path with a Polish or a Chinese name,
# a value of a gen file, the text of an error) then stops the tool with a UnicodeEncodeError instead of the message.
#
# So the standard streams are told to write such a character as an escape ('\u0141'). Nothing else about a stream changes: not its encoding
# (what reads the output expects the encoding it gets today), not its newline handling and not its buffering. Text the encoding has is written
# as before.
#
# An escape like that is not json. Json that is printed (for a program that reads the output of a tool) is given json escapes for those
# characters before it is printed, see EscapeJsonForStream.

import codecs
import io
import re
import sys

_g_escapeHandler = "backslashreplace"
# The error handlers that write something for every character, a stream that uses one of them is left as it is
_g_handlersThatNeverFail = frozenset({"backslashreplace", "replace", "ignore", "xmlcharrefreplace", "namereplace"})
# The name of an error handler that is made here: the handler of the stream followed by this
_g_combinedHandlerPostfix = f"_or_{_g_escapeHandler}"

# Every character that is not ASCII. The pattern is compiled when it is first used, a tool that prints no json does not pay for it.
_g_notAscii = r"[^\x00-\x7f]"


def _NeverFails(errors: str) -> bool:
    return errors in _g_handlersThatNeverFail or errors.endswith(_g_combinedHandlerPostfix)


def _GetHandlerThatEscapes(errors: str) -> str:
    """The name of the error handler that does what the handler 'errors' does and writes an escape for the characters that handler fails on.
    'surrogateescape' (the handler of stdout in the C locale and in the UTF-8 mode of python) writes the bytes of a file name that could not be
    decoded, that is kept.
    """
    if errors == "strict":
        # It fails on every character it is asked about
        return _g_escapeHandler
    currentHandler = codecs.lookup_error(errors)
    escapeHandler = codecs.lookup_error(_g_escapeHandler)

    def Handle(error: UnicodeError) -> tuple[str | bytes, int]:
        try:
            return currentHandler(error)
        except UnicodeError:
            return escapeHandler(error)

    name = f"{errors}{_g_combinedHandlerPostfix}"
    codecs.register_error(name, Handle)
    return name


def UseEscapesForUnencodableText(stream: object) -> bool:
    """Make the text stream write a character its encoding does not have as an escape instead of failing.
    Only a text stream of python itself (what sys.stdout and sys.stderr normally are) is changed. Anything else, like the StringIO of a test or
    the stream of a host the tool is embedded in, is left alone, and so is a stream that can not fail.
    Returns true if the stream was changed.
    """
    if not isinstance(stream, io.TextIOWrapper):
        return False
    try:
        errors = stream.errors
        if errors is None or _NeverFails(errors):
            return False
        # Only the error handler is given, so the encoding, the newline handling and the buffering stay as they are
        stream.reconfigure(errors=_GetHandlerThatEscapes(errors))
    except ValueError, OSError, LookupError:
        # A closed or detached stream, or an error handler that is not known: the stream stays as it is
        return False
    return True


def _IsWrittenAsItIs(character: str, encoding: str, errors: str) -> bool:
    """True if what the stream writes for the character is read back as that character"""
    try:
        return character.encode(encoding, errors).decode(encoding, errors) == character
    except UnicodeError, LookupError:
        return False


def _ToJsonEscape(character: str) -> str:
    """The escape json has for a character: four hex digits, and a surrogate pair for a character above U+FFFF"""
    code = ord(character)
    if code > 0xFFFF:
        code -= 0x10000
        return f"\\u{0xD800 + (code >> 10):04x}\\u{0xDC00 + (code & 0x3FF):04x}"
    return f"\\u{code:04x}"


def EscapeJsonForStream(jsonText: str, stream: object) -> str:
    """Returns the json text with every character the text stream can not write written as its json escape, so what is printed is valid json
    that holds exactly the values of jsonText: reading the printed bytes with the encoding of the stream and parsing them gives those values.
    jsonText is json made without escaping what is not ASCII (json.dumps with ensure_ascii=False).

    In json every structural character is ASCII and every other character sits inside a string, so replacing such a character with its
    escape is safe wherever it is in the text.

    A character the stream writes as it is today is not touched, so the bytes of everything that printed before stay the same. That is
    decided with the encoding of the stream and its error handler: a character is written as it is when what the stream writes for it is
    read back as that character. So a stream that writes the bytes of an undecodable file name ('surrogateescape', also after
    UseEscapesForUnencodableText changed the stream) keeps doing that where those bytes are read back as they were, and what an error handler
    replaces ('?', a python escape) is given its json escape.

    Anything that is not a text stream of python (the StringIO of a test, the stream of a host, no stream) has no encoding to go by,
    the text is returned unchanged.
    """
    if jsonText.isascii() or not isinstance(stream, io.TextIOWrapper):
        return jsonText
    encoding = stream.encoding
    errors = stream.errors or "strict"

    def Replace(match: re.Match[str]) -> str:
        character = match.group()
        return character if _IsWrittenAsItIs(character, encoding, errors) else _ToJsonEscape(character)

    return re.sub(_g_notAscii, Replace, jsonText)


def UseEscapesOnStandardStreams() -> None:
    """Called first thing by the entry of the tools, before anything is printed"""
    UseEscapesForUnencodableText(sys.stdout)
    UseEscapesForUnencodableText(sys.stderr)
