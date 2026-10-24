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

# The MonoGame content project of an app ('Content.mgcb' in its resolved content directory), made from the content of the app
# (PackageAppContentResolver).
#
# MonoGame imports content with its own builder, MGCB, which the C# build runs on a '.mgcb' file (MonoGame.Content.Builder.Task). The app does
# not keep one by hand: an app whose template asks for it gets one in its resolved content directory, next to the files it lists, so every
# path in it is the path of the file in the app. MGCB is the importer of the platform, as the editor is for Unity: what each file is comes
# from its content type and its profile (ContentProfile), how MGCB builds that is decided here.
#
# - Texture: TextureImporter and TextureProcessor. Mipmaps and compression from the profile. No colour key and no premultiplied alpha:
#   neither is a property of the content, a texture keeps the alpha it has. Encoding has no parameter, the runtime picks the format.
# - Effect: EffectImporter and EffectProcessor. Sound: the importer of the extension and SoundEffectProcessor with the quality.
# - Data and a licence file: copied as they are. Include: not in the project, an effect reads it from beside itself. Documentation is not
#   content of the app, so it never gets here.
# - The project never premultiplies alpha and never keys a colour (ContentProfile): a texture that must be premultiplied is baked so.
# - The file is written only when its text changes, so MGCB keeps its own state of what is up to date.
# - MGCB does not remove what it made for a file that is no longer in the project, and the C# build copies all of its output into the app.
#   So when the project is written again, the output and the intermediate of every file that left it are removed, unless a file that is
#   still in it makes the same output. The project that was there is what was built: no other record is needed.

import glob
import os
from collections.abc import Sequence

from FslBuildGen import IOUtil
from FslBuildGen.BuildContent import ContentProfile
from FslBuildGen.BuildContent.ContentProfile import ContentType
from FslBuildGen.Exceptions import UsageErrorException
from FslBuildGen.Log import Log
from FslBuildGen.Packages.PackageAppContent import PackageAppContentEntry

_g_soundImporters = {".wav": "WavImporter", ".ogg": "OggImporter", ".mp3": "Mp3Importer"}
_g_newLine = "\n"


# The name of the content project of an app, in its resolved content directory. The name MonoGame gives it, so the project of the template
# can name it, and its output is in 'Content' of the app as MonoGame has it
FileName = "Content.mgcb"
# Where MGCB puts what it makes, below the directory of the project (MonoGame.Content.Builder.Task): the output in
# 'bin/<platform>/<project name>', the intermediate in 'obj/<platform>/<framework>/<project name>'
_g_outputPattern = f"bin/*/{IOUtil.GetFileNameWithoutExtension(FileName)}"
_g_intermediatePattern = f"obj/*/*/{IOUtil.GetFileNameWithoutExtension(FileName)}"
_g_build = "/build:"
_g_copy = "/copy:"


def _Bool(value: bool) -> str:
    return "True" if value else "False"


def _Entry(entry: PackageAppContentEntry) -> list[str]:
    """The lines of one file, none for a file that is not in the project"""
    path = entry.AppPath
    profile = entry.Profile
    # A built file of another content type than its source has the default of its own type: the profile is about the source
    properties = profile.Properties if profile.ContentType == entry.ContentType else ContentProfile.GetDefault(entry.ContentType).Properties
    if entry.ContentType in (ContentType.Include, ContentType.Documentation):
        return []
    if entry.ContentType == ContentType.Texture:
        return [
            f"#begin {path}",
            "/importer:TextureImporter",
            "/processor:TextureProcessor",
            "/processorParam:ColorKeyEnabled=False",
            f"/processorParam:GenerateMipmaps={_Bool(properties[ContentProfile.PropertyMipmaps] == ContentProfile.ValueTrue)}",
            "/processorParam:PremultiplyAlpha=False",
            "/processorParam:ResizeToPowerOfTwo=False",
            "/processorParam:MakeSquare=False",
            f"/processorParam:TextureFormat={'Color' if properties[ContentProfile.PropertyCompression] == 'None' else 'Compressed'}",
            f"/build:{path}",
        ]
    if entry.ContentType == ContentType.Effect:
        return [f"#begin {path}", "/importer:EffectImporter", "/processor:EffectProcessor", "/processorParam:DebugMode=Auto", f"/build:{path}"]
    if entry.ContentType == ContentType.Sound:
        return [
            f"#begin {path}",
            f"/importer:{_g_soundImporters[IOUtil.GetFileNameExtension(path).lower()]}",
            "/processor:SoundEffectProcessor",
            f"/processorParam:Quality={properties[ContentProfile.PropertyQuality]}",
            f"/build:{path}",
        ]
    return [f"#begin {path}", f"/copy:{path}"]


def BuildText(entries: Sequence[PackageAppContentEntry]) -> str:
    """The content project for the content of an app, in the order of the entries"""
    lines = [
        "# Made by FslBuildContent from the content of the app, do not edit: it is written again when the content changes.",
        "# The output, intermediate and platform directories are given by MonoGame.Content.Builder.Task.",
        "",
        "#----------------------------- Global Properties ----------------------------#",
        "",
        "/compress:False",
        "",
        "#---------------------------------- Content ---------------------------------#",
    ]
    for entry in entries:
        entryLines = _Entry(entry)
        if len(entryLines) > 0:
            lines += ["", *entryLines]
    return _g_newLine.join(lines) + _g_newLine


def Write(log: Log, configDisableWrite: bool, contentPath: str, appName: str, entries: Sequence[PackageAppContentEntry]) -> str:
    """Write the content project of the app into its resolved content directory, when its text changed. Returns its path"""
    # The project is next to the files it lists: a file of the content on its path would be overwritten by it
    clash = next((entry for entry in entries if entry.AppPath.lower() == FileName.lower()), None)
    if clash is not None:
        raise UsageErrorException(
            f"The content of '{appName}' has a file on the path of its MonoGame content project '{FileName}': '{clash.SourcePath}' (package "
            f"'{clash.OwnerName}'). The project is made from the content of the app, a '.mgcb' kept by hand is not needed: remove the file"
        )
    filePath = IOUtil.Join(contentPath, FileName)
    if configDisableWrite:
        log.LogPrint(f"Would write the MonoGame content project '{filePath}'")
        return filePath
    IOUtil.SafeMakeDirs(contentPath)
    text = BuildText(entries)
    lastText = IOUtil.ReadFileUTF8(filePath) if IOUtil.IsFile(filePath) else None
    if IOUtil.WriteFileUTF8IfChanged(filePath, text):
        log.LogPrint(f"Wrote the MonoGame content project '{filePath}'")
        if lastText is not None:
            RemoveOutputsOfFilesThatLeft(log, contentPath, lastText, text)
    return filePath


def GetOutputs(text: str) -> dict[str, str | None]:
    """What MGCB makes for the files of a project: the output below its output directory -> the intermediate below its intermediate directory
    (None for a file that is copied)
    """
    outputs: dict[str, str | None] = {}
    for line in text.splitlines():
        if line.startswith(_g_build):
            stem = os.path.splitext(line[len(_g_build) :])[0]
            outputs[f"{stem}.xnb"] = f"{stem}.mgcontent"
        elif line.startswith(_g_copy):
            outputs[line[len(_g_copy) :]] = None
    return outputs


def RemoveOutputsOfFilesThatLeft(log: Log, contentPath: str, lastText: str, text: str) -> int:
    """Remove what MGCB made for the files of the last project that are not in this one. Returns the number of files that were removed"""
    outputs = GetOutputs(text)
    left = {output: intermediate for output, intermediate in GetOutputs(lastText).items() if output not in outputs}
    removed = 0
    for pattern, isOutput in [(_g_outputPattern, True), (_g_intermediatePattern, False)]:
        for directory in glob.glob(IOUtil.Join(glob.escape(contentPath), pattern)):
            for output, intermediate in left.items():
                relativePath = output if isOutput else intermediate
                if relativePath is None:
                    continue
                filename = IOUtil.Join(IOUtil.NormalizePath(directory), relativePath)
                if IOUtil.IsFile(filename):
                    log.LogPrintVerbose(2, f"Removing '{filename}', it is no longer in the MonoGame content project")
                    os.remove(filename)
                    removed += 1
    if removed > 0:
        log.LogPrint(f"Removed {removed} files MGCB made for content that is no longer in '{FileName}'")
    return removed
