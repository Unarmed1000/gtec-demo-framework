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

# The schema of a gen file (Fsl.gen): where it lives and which version of it is the current one.
#
# The master copy of the schema is in the 'Schema' directory next to the entry scripts of the tool, one directory per version:
# Schema/<version>/FslBuildGen.xsd. The tool is deployed into a project as its '.Config' directory, which puts the schema at
# '.Config/Schema/<version>/FslBuildGen.xsd'. That is the path the default url below ends in.
#
# The versioning rule:
# - A version is an integer, the first one is 1.
# - A published version only gets compatible additions: a gen file that is valid stays valid.
# - A change that would make a valid gen file invalid gets a new version directory, and CurrentVersion is raised to it.
#
# The reference a gen file holds ('xsi:noNamespaceSchemaLocation') depends on where the schema versions of its project are, a
# GenFileSchemaLocation:
# - a url base: the reference is '<base>/<version>/FslBuildGen.xsd'
# - a directory: the reference is the relative path from the directory of the gen file to '<directory>/<version>/FslBuildGen.xsd', written
#   as a URI (forward slashes, what a URI can not hold as it is percent-encoded)
# A project that says nothing uses DefaultLocation.
#
# A gen file holds a reference already, ClassifyReference tells if it is the one for the current version, the one for an older version (it
# is valid and left alone) or wrong. A reference that is spelled another way but names the same file everywhere is not wrong:
# - whitespace around it is ignored, a schema processor drops it
# - a '.' part and a 'name/..' pair are ignored, so './../x' is '../x'
# - percent-encoding is ignored: 'My%20Dir' is 'My Dir' and '%C3%A9' is the character it encodes. Not for '%2F': an encoded '/' is no
#   separator
# - the 'https' and the host name of a url are compared without case
# A reference that only some editors or machines resolve is wrong, so it gets repaired:
# - a backslash is not a separator in a URI: '..\x' only resolves on Windows
# - the names of a path are compared with case, a file system and a web server can be case-sensitive
# - an absolute path only fits one machine, and a doubled slash is not a path every program accepts

import os
from enum import Enum

from FslBuildGen import IOUtil
from FslBuildGen.Exceptions import UsageErrorException

# The schema version a gen file refers to
CurrentVersion = 1

SchemaFileName = "FslBuildGen.xsd"

# Where the schema versions are published. A gen file refers to '<DefaultUrl>/<version>/<SchemaFileName>'.
DefaultUrl = "https://raw.githubusercontent.com/Unarmed1000/gtec-demo-framework/master/.Config/Schema"

_g_schemaDirectoryName = "Schema"

# The path rules of the platform the tool runs on. A drive is a Windows thing, the tests of the drive rules put the Windows rules here.
_g_path = os.path

_g_urlPrefixes = ("http://", "https://")

# The characters a path in a URI holds as they are, the others are percent-encoded
_g_uriPathCharacters = frozenset(b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-~/")
_g_hexDigits = frozenset(b"0123456789abcdefABCDEF")
_g_slash = 0x2F
_g_percent = 0x25


def GetToolSchemaDirectory() -> str:
    """The absolute path of the directory that holds the schema versions of this tool, it is placed next to the FslBuildGen package"""
    toolDirectory = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    return IOUtil.Join(IOUtil.NormalizePath(toolDirectory), _g_schemaDirectoryName)


def IsUrl(value: str) -> bool:
    """True if the value is a url base ('http://' or 'https://', a scheme has no case), anything else is taken to be a directory"""
    return value[: len(_g_urlPrefixes[1])].lower().startswith(_g_urlPrefixes)


class GenFileSchemaLocation:
    """Where the schema versions of a project are: a url base or an absolute directory. Both hold one directory per version."""

    def __init__(self, value: str) -> None:
        super().__init__()
        self.IsUrl = IsUrl(value)
        if self.IsUrl:
            # The reference is made with one slash, whatever the base ends with
            value = value.rstrip("/")
        elif not _g_path.isabs(value):
            raise UsageErrorException(f"The gen file schema location '{value}' is not a url (http:// or https://) and not an absolute directory")
        self.Value = value


# The location of a project that says nothing about its schema
DefaultLocation = GenFileSchemaLocation(DefaultUrl)


class ReferenceClass(Enum):
    """What the schema reference of a gen file is, compared to the one it is expected to have"""

    # The reference for the current schema version
    Current = 1
    # The reference for an older schema version: valid, the gen file was not moved to the current version
    Older = 2
    # Anything else
    Wrong = 3


def _PercentEncode(path: str) -> str:
    """The path as it is written in a URI. This is urllib.parse.quote(path, safe='/') without importing urllib, and it also takes the bytes
    of a file name that could not be decoded.
    """
    return "".join(chr(value) if value in _g_uriPathCharacters else f"%{value:02X}" for value in path.encode("utf-8", "surrogateescape"))


def _PercentDecode(text: str) -> str:
    """The text with its percent-encoded bytes decoded, but for '%2F': an encoded '/' is a character of a name, not a separator"""
    if "%" not in text:
        return text
    data = text.encode("utf-8", "surrogateescape")
    result = bytearray()
    index = 0
    while index < len(data):
        if data[index] == _g_percent and index + 2 < len(data) and data[index + 1] in _g_hexDigits and data[index + 2] in _g_hexDigits:
            value = int(data[index + 1 : index + 3], 16)
            if value != _g_slash:
                result.append(value)
                index += 3
                continue
        result.append(data[index])
        index += 1
    return result.decode("utf-8", "surrogateescape")


def _NormalizeReference(reference: str) -> str:
    """What the spellings of a reference that name the same file everywhere have in common, see the rules at the top of this file"""
    text = reference.strip(" \t\r\n")
    prefix = ""
    if IsUrl(text):
        # 'https://host' is compared without case
        scheme, separator, rest = text.partition("//")
        host, slash, path = rest.partition("/")
        prefix = f"{scheme}{separator}{host}".lower()
        text = f"{slash}{path}"
    parts: list[str] = []
    for part in _PercentDecode(text).split("/"):
        if part == ".":
            continue
        if part == ".." and len(parts) > 0 and parts[-1] not in ("..", ""):
            parts.pop()
            continue
        parts.append(part)
    return prefix + "/".join(parts)


def GetExpectedReference(location: GenFileSchemaLocation, genFileDirectory: str, version: int) -> str:
    """The schema reference a gen file in the directory is expected to hold for the schema version.
    genFileDirectory is the absolute path of the directory the gen file is in.
    """
    if location.IsUrl:
        return f"{location.Value}/{version}/{SchemaFileName}"
    schemaFile = _g_path.join(location.Value, str(version), SchemaFileName)
    try:
        relativePath = _g_path.relpath(schemaFile, genFileDirectory)
    except ValueError as ex:
        # Windows: there is no relative path from one drive to another
        raise UsageErrorException(
            f"The gen file schema directory '{location.Value}' is on another drive than the gen file directory '{genFileDirectory}', "
            "so a gen file can not refer to the schema with a relative path"
        ) from ex
    if _g_path.sep != "/":
        relativePath = relativePath.replace(_g_path.sep, "/")
    return _PercentEncode(relativePath)


def ClassifyReference(reference: str, location: GenFileSchemaLocation, genFileDirectory: str, currentVersion: int = CurrentVersion) -> ReferenceClass:
    """Tells if the schema reference a gen file holds is the expected one for the current schema version, the expected one for an older
    version or wrong. Another spelling of an expected reference is that reference, see the rules at the top of this file.
    """
    normalizedReference = _NormalizeReference(reference)
    for version in range(currentVersion, 0, -1):
        if normalizedReference == _NormalizeReference(GetExpectedReference(location, genFileDirectory, version)):
            return ReferenceClass.Current if version == currentVersion else ReferenceClass.Older
    return ReferenceClass.Wrong
