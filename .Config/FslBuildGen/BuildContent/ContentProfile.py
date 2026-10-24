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

# What a content file is, said without the vocabulary of any platform, so the importer of each platform can build it the way the package
# means: MGCB for MonoGame, the editor for Unity, a content builder of the tool.
#
# - The content type of a file comes from its extension: a texture, an effect, a sound, a file an effect includes, or data that is used as it
#   is. Two are about the content and not content themselves: the licence file of a resource ('License.json', by its name), which ships
#   with the app unless its project says otherwise, and documentation ('*.md', like the 'SOURCE.md' of an atlas), which never does.
# - The content build never premultiplies alpha and never keys a colour: a texture that must be premultiplied is baked so by the tool
#   that makes it (TexturePacker), the content build uses the image as it is.
# - A texture and a sound have properties. A property is only here when every platform can express it: Unity can not key a colour or
#   premultiply in its importer, so neither is a property.
# - The profiles are a standard set of the tool: every project means the same by a name and every backend is written against one list.
#   A profile is a value for each property of its content type. A new profile is a change of the tool.
# - A file with no entry in a helper file gets the default of its type. The defaults are those of a texture in a scene: sRGB, mipmaps and
#   compressed; a file that can not take that (UI, a distance field) says so with a profile.

from collections.abc import Mapping
from enum import Enum

from FslBuildGen import IOUtil, ToolSharedValues


class ContentType(Enum):
    Texture = "Texture"
    Effect = "Effect"
    Sound = "Sound"
    # A source file an effect includes ('#include "Common.fxh"'): it is next to the effect for the build, it is not content by itself
    Include = "Include"
    # The licence file of a resource ('License.json'): it ships with the app unless its project says otherwise (ContentConfiguration)
    License = "License"
    # Documentation of the content ('SOURCE.md'): it is not content of an app
    Documentation = "Documentation"
    # Used as it is, also an image that is already in a GPU format (dds, ktx)
    Data = "Data"


_g_typeOfExtension: dict[str, ContentType] = {
    ".png": ContentType.Texture,
    ".jpg": ContentType.Texture,
    ".jpeg": ContentType.Texture,
    ".bmp": ContentType.Texture,
    ".tga": ContentType.Texture,
    ".fx": ContentType.Effect,
    ".fxh": ContentType.Include,
    ".hlsl": ContentType.Include,
    ".hlsli": ContentType.Include,
    ".md": ContentType.Documentation,
    ".wav": ContentType.Sound,
    ".ogg": ContentType.Sound,
    ".mp3": ContentType.Sound,
}

# The properties of a type of content -> the values each can have
PropertyEncoding = "Encoding"
PropertyAlpha = "Alpha"
PropertyMipmaps = "Mipmaps"
PropertyCompression = "Compression"
PropertyQuality = "Quality"

ValueTrue = "true"
ValueFalse = "false"

_g_properties: dict[ContentType, dict[str, tuple[str, ...]]] = {
    ContentType.Texture: {
        # How the values of the texture are encoded: colour in sRGB, linear data, or a normal map
        PropertyEncoding: ("sRGB", "Linear", "Normal"),
        # The alpha of the image is used as it is, or there is none
        PropertyAlpha: ("Straight", "None"),
        PropertyMipmaps: (ValueTrue, ValueFalse),
        # None keeps every pixel, the others are lossy with that quality
        PropertyCompression: ("None", "Low", "Normal", "High"),
    },
    ContentType.Sound: {PropertyQuality: ("Best", "Medium", "Low")},
    ContentType.Effect: {},
    ContentType.Include: {},
    ContentType.License: {},
    ContentType.Documentation: {},
    ContentType.Data: {},
}

DefaultProfileName = "Default"


class ContentProfile:
    def __init__(self, name: str, contentType: ContentType, properties: Mapping[str, str]) -> None:
        super().__init__()
        self.Name = name
        self.ContentType = contentType
        self.Properties = dict(properties)


def _Texture(name: str, encoding: str, mipmaps: str, compression: str) -> ContentProfile:
    properties = {PropertyEncoding: encoding, PropertyAlpha: "Straight", PropertyMipmaps: mipmaps, PropertyCompression: compression}
    return ContentProfile(name, ContentType.Texture, properties)


# The profiles of the tool, by name. 'Default' is the one of a file no entry names a profile for
_g_profiles: dict[str, ContentProfile] = {
    profile.Name: profile
    for profile in [
        # A colour texture in a scene
        _Texture(DefaultProfileName, "sRGB", ValueTrue, "Normal"),
        # UI, sprites and atlases are drawn 1:1, with crisp edges and text: no mipmaps, no lossy compression
        _Texture("UI", "sRGB", ValueFalse, "None"),
        # A signed distance field (SDF/MTSDF fonts and shapes) is data, and lossy compression breaks it
        _Texture("DistanceField", "Linear", ValueFalse, "None"),
        # Masks, ramps, roughness and other data
        _Texture("Linear", "Linear", ValueTrue, "Normal"),
        # A normal map: the backend picks a format for normals
        _Texture("NormalMap", "Normal", ValueTrue, "Normal"),
    ]
}
# The default of each type that has properties
_g_defaults: dict[ContentType, dict[str, str]] = {
    ContentType.Texture: _g_profiles[DefaultProfileName].Properties,
    ContentType.Sound: {PropertyQuality: "Best"},
    ContentType.Effect: {},
    ContentType.Include: {},
    ContentType.License: {},
    ContentType.Documentation: {},
    ContentType.Data: {},
}


class ResolvedContentProfile:
    """What a file is: its content type, the name of its profile and the value of each property of its type"""

    def __init__(self, contentType: ContentType, profileName: str, properties: Mapping[str, str]) -> None:
        super().__init__()
        self.ContentType = contentType
        self.ProfileName = profileName
        self.Properties = dict(properties)

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, ResolvedContentProfile):
            return NotImplemented
        return (self.ContentType, self.ProfileName, self.Properties) == (other.ContentType, other.ProfileName, other.Properties)

    def __hash__(self) -> int:
        return hash((self.ContentType, self.ProfileName, tuple(sorted(self.Properties.items()))))

    def __repr__(self) -> str:
        return f"ResolvedContentProfile({self.ContentType.value}, {self.ProfileName}, {self.Properties})"


def GetContentType(path: str) -> ContentType:
    """The content type of a file, by its extension (in any case). The licence file is known by its name, with the case it has"""
    if IOUtil.GetFileName(path) == ToolSharedValues.LICENSE_FILE_NAME:
        return ContentType.License
    return _g_typeOfExtension.get(IOUtil.GetFileNameExtension(path).lower(), ContentType.Data)


def GetDefault(contentType: ContentType) -> ResolvedContentProfile:
    return ResolvedContentProfile(contentType, DefaultProfileName, _g_defaults[contentType])


def TryGetProfile(name: str) -> ContentProfile | None:
    return _g_profiles.get(name)


def GetProfileNames() -> list[str]:
    return list(_g_profiles)


def GetPropertyValues(contentType: ContentType) -> dict[str, tuple[str, ...]]:
    """The properties of the type, with the values each can have"""
    return dict(_g_properties[contentType])


def GetContentTypeOfProperty(name: str) -> ContentType | None:
    """The type of content that has the property, None if no type has it"""
    for contentType, properties in _g_properties.items():
        if name in properties:
            return contentType
    return None
