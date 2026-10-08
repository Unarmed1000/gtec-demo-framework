#!/usr/bin/env python3
# ****************************************************************************************************************************************************
# * BSD 3-Clause License
# *
# * Copyright (c) 2026, Mana Battery
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
"""Takes the model of the graphics device out of the files of a capture.

The log of an app names the graphics device it ran on (the model as the driver reports it and its PCI device id), and so does what the
app prints. A capture is made to be handed on, and the model of somebody's own hardware is not something to hand on by accident. So the
capture tool replaces it in every file of a run with the vendor ("NVIDIA GPU"), which together with the driver version is what a reader
of the log needs, unless it is told to keep the names.
"""

import os
import re
from pathlib import Path

# The vendors a log is likely to name (PCI vendor ids, and the ids Khronos gave to vendors without one)
_g_vendorNames = {
    0x1002: "AMD",
    0x1010: "Imagination",
    0x106b: "Apple",
    0x10de: "NVIDIA",
    0x13b5: "Arm",
    0x14e4: "Broadcom",
    0x5143: "Qualcomm",
    0x8086: "Intel",
    0x10005: "Mesa",
}

_g_sdkEnvironmentVariable = "FSL_GRAPHICS_SDK"
_g_deviceNameFact = "vulkan.deviceName"
_g_vendorIdFact = "vulkan.vendorId"
_g_anonymousDeviceId = "0x0"
# 'vulkan.deviceId=0x1234' in the events of a log and 'deviceID: 0x1234' in what an app prints
_g_deviceIdPattern = re.compile(rb"(device ?id\W{0,3})(0x[0-9a-f]+|[0-9]+)", re.IGNORECASE)
# The lines of what an app prints that name the graphics device: '- deviceName: <model>' of a Vulkan app, 'Renderer: <model>' of a
# OpenGL ES app and 'GL renderer: [<model>]' of a OpenGL ES emulator. The trace of an app is anonymised by the app itself, so the tool
# is not told the model and finds it by these lines.
_g_deviceNameLinePattern = re.compile(rb"^((?:- deviceName|Renderer|GL renderer): ?)([^\r\n]*)", re.IGNORECASE | re.MULTILINE)


def GetAnonymousDeviceName(vendorIdText: str | None) -> str:
    """The name a graphics device gets in place of its model: its vendor, if the vendor id is one that is known"""
    try:
        vendorId = int(vendorIdText, 0) if vendorIdText is not None else None
    except ValueError:
        vendorId = None
    vendorName = _g_vendorNames.get(vendorId) if vendorId is not None else None
    return f"{vendorName} GPU" if vendorName is not None else "GPU"


def GetLocalPaths(outputPath: Path) -> list[tuple[Path, str]]:
    """The directories of this machine the files of a run can name, each with what it is replaced by. The longest come first, so a
    directory inside another one is replaced as itself."""
    localPaths = [(outputPath.resolve(), "<output>"), (Path.home().resolve(), "<home>")]
    sdkPath = os.environ.get(_g_sdkEnvironmentVariable)
    if sdkPath:
        localPaths.append((Path(sdkPath).resolve(), "<sdk>"))
    return sorted(localPaths, key=lambda entry: len(str(entry[0])), reverse=True)


def AnonymisePaths(content: bytes, localPaths: list[tuple[Path, str]]) -> bytes:
    """Replace the directories in the content, however their separators and their case are written"""
    for localPath, replacement in localPaths:
        parts = [part for part in re.split(r"[\\/]+", str(localPath)) if len(part) > 0]
        if len(parts) < 2:
            # A drive or the root alone is not a directory worth hiding, and it would match far too much
            continue
        pattern = re.compile(rb"[\\/]+".join(re.escape(part.encode("utf-8")) for part in parts), re.IGNORECASE)
        content = pattern.sub(replacement.encode("ascii"), content)
    return content


def AnonymiseFiles(paths: list[Path], facts: dict[str, str], localPaths: list[tuple[Path, str]] | None = None) -> int:
    """Replace the model of the graphics device in the files, given the facts of the log of the run, and the directories of this
    machine (see GetLocalPaths).

    Returns the number of files that were changed.
    """
    deviceName = facts.get(_g_deviceNameFact, "")
    anonymousName = GetAnonymousDeviceName(facts.get(_g_vendorIdFact))
    replaceName = len(deviceName) > 0 and deviceName != anonymousName
    changedCount = 0
    for path in paths:
        if not path.is_file():
            continue
        content = path.read_bytes()
        newContent = content
        if replaceName:
            newContent = newContent.replace(deviceName.encode("utf-8"), anonymousName.encode("utf-8"))
        newContent = _g_deviceIdPattern.sub(lambda match: match.group(1) + _g_anonymousDeviceId.encode("ascii"), newContent)
        newContent = _g_deviceNameLinePattern.sub(lambda match: match.group(1) + anonymousName.encode("utf-8"), newContent)
        if localPaths:
            newContent = AnonymisePaths(newContent, localPaths)
        if newContent != content:
            path.write_bytes(newContent)
            changedCount += 1
    return changedCount
